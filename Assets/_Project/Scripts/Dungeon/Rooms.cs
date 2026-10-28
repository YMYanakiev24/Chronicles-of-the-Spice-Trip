/*
 * Rooms.cs
 * ----------------------------------------------------------------------------
 * A floor is a grid of single-screen rooms joined by doors (Rooms.cpp). Doors
 * stay barred until a room's monsters are dead. Also here: circle-vs-wall
 * movement, spike traps, and the flow field monsters use to path around walls.
 * ----------------------------------------------------------------------------
 */

using System;
using UnityEngine;
using static SpiceTrip.Dungeon.Geo;

namespace SpiceTrip.Dungeon
{
    public enum RoomTile { Floor = 0, Wall = 1, Spikes = 2 }

    [Flags]
    public enum Dir { None = 0, N = 1, E = 2, S = 4, W = 8 }

    [Serializable]
    public class Room
    {
        public bool exists, visited, cleared, isStart, isBoss, isShop;
        public Dir doors;
        public int[] tiles = new int[RoomTW * RoomTH];     // RoomTile, row-major
        public bool hasChest, chestOpened;
        public int chestTx, chestTy;

        public RoomTile At(int tx, int ty) => (RoomTile)tiles[ty * RoomTW + tx];
        public void Set(int tx, int ty, RoomTile t) => tiles[ty * RoomTW + tx] = (int)t;

        /// <summary>A walkable, trap-free interior tile.</summary>
        public bool IsFloor(int tx, int ty) =>
            tx > 0 && ty > 0 && tx < RoomTW - 1 && ty < RoomTH - 1 && At(tx, ty) == RoomTile.Floor;

        public Vector2 ChestPos => new Vector2(chestTx * Tile + 16, chestTy * Tile + 16);

        void Fill(int x0, int y0, int w, int h, RoomTile v)
        {
            for (int y = y0; y < y0 + h; y++)
                for (int x = x0; x < x0 + w; x++)
                    if (x > 0 && y > 0 && x < RoomTW - 1 && y < RoomTH - 1) Set(x, y, v);
        }

        /// <summary>Walls, traps and maybe a chest. Start, boss and shop rooms stay open.</summary>
        public void BuildLayout(DRandom rng)
        {
            Array.Clear(tiles, 0, tiles.Length);
            hasChest = chestOpened = false;
            if (isStart || isBoss || isShop) return;

            int midX = RoomTW / 2, midY = RoomTH / 2;
            switch (rng.Range(0, 5))
            {
                case 0:     // four corner blocks
                    Fill(3, 2, 3, 2, RoomTile.Wall); Fill(RoomTW - 6, 2, 3, 2, RoomTile.Wall);
                    Fill(3, RoomTH - 4, 3, 2, RoomTile.Wall); Fill(RoomTW - 6, RoomTH - 4, 3, 2, RoomTile.Wall);
                    break;
                case 1:     // pillar grid
                    for (int y = 2; y < RoomTH - 2; y += 3)
                        for (int x = 3; x < RoomTW - 3; x += 4) Set(x, y, RoomTile.Wall);
                    break;
                case 2:     // two long horizontal walls
                    Fill(3, 3, midX - 4, 1, RoomTile.Wall); Fill(midX + 2, RoomTH - 4, midX - 4, 1, RoomTile.Wall);
                    break;
                case 3:     // central block ringed by spikes
                    Fill(midX - 3, midY - 2, 7, 5, RoomTile.Spikes);
                    Fill(midX - 2, midY - 1, 5, 3, RoomTile.Wall);
                    break;
                case 4:     // vertical walls with spike lanes
                    Fill(5, 2, 1, RoomTH - 4, RoomTile.Wall); Fill(RoomTW - 6, 2, 1, RoomTH - 4, RoomTile.Wall);
                    Fill(7, 2, 2, RoomTH - 4, RoomTile.Spikes); Fill(RoomTW - 9, 2, 2, RoomTH - 4, RoomTile.Spikes);
                    break;
                default:    // scattered rubble and traps
                    for (int i = 0; i < 10; i++) Set(rng.Range(2, RoomTW - 3), rng.Range(2, RoomTH - 3), RoomTile.Wall);
                    for (int i = 0; i < 6; i++) Set(rng.Range(2, RoomTW - 3), rng.Range(2, RoomTH - 3), RoomTile.Spikes);
                    break;
            }
            for (int i = 0, n = rng.Range(0, 3); i < n; i++)      // a few extra traps in any layout
            {
                int x = rng.Range(2, RoomTW - 3), y = rng.Range(2, RoomTH - 3);
                if (At(x, y) == RoomTile.Floor) Set(x, y, RoomTile.Spikes);
            }

            // Keep the door lanes (centre row and column) clear so every door is reachable.
            for (int x = 1; x < RoomTW - 1; x++) if (At(x, midY) == RoomTile.Wall) Set(x, midY, RoomTile.Floor);
            for (int y = 1; y < RoomTH - 1; y++) if (At(midX, y) == RoomTile.Wall) Set(midX, y, RoomTile.Floor);

            // Wall off any pocket the player could never reach (flood fill from a door lane).
            var seen = new bool[RoomTW * RoomTH];
            var queue = new System.Collections.Generic.Queue<(int x, int y)>();
            seen[midY * RoomTW + 1] = true;
            queue.Enqueue((1, midY));
            int[] dx = { 1, -1, 0, 0 }, dy = { 0, 0, 1, -1 };
            while (queue.Count > 0)
            {
                var (x, y) = queue.Dequeue();
                for (int k = 0; k < 4; k++)
                {
                    int nx = x + dx[k], ny = y + dy[k];
                    if (nx <= 0 || ny <= 0 || nx >= RoomTW - 1 || ny >= RoomTH - 1) continue;
                    if (seen[ny * RoomTW + nx] || At(nx, ny) == RoomTile.Wall) continue;
                    seen[ny * RoomTW + nx] = true;
                    queue.Enqueue((nx, ny));
                }
            }
            for (int y = 1; y < RoomTH - 1; y++)
                for (int x = 1; x < RoomTW - 1; x++)
                    if (!seen[y * RoomTW + x]) Set(x, y, RoomTile.Wall);

            // Roughly half the rooms hide a chest on a reachable floor tile.
            if (rng.Range(0, 99) < 50)
                for (int tries = 0; tries < 200; tries++)
                {
                    int x = rng.Range(2, RoomTW - 3), y = rng.Range(2, RoomTH - 3);
                    if (x == midX || y == midY || !IsFloor(x, y)) continue;
                    hasChest = true; chestTx = x; chestTy = y;
                    break;
                }
        }
    }

    [Serializable]
    public class Floor
    {
        public Room[] rooms = new Room[FloorSize * FloorSize];
        public int cx = FloorSize / 2, cy = FloorSize / 2;    // the current room

        public Room this[int x, int y] => rooms[y * FloorSize + x];
        public Room Cur => this[cx, cy];

        static readonly int[] StepX = { 0, 1, 0, -1 }, StepY = { -1, 0, 1, 0 };
        static readonly Dir[] Bits = { Dir.N, Dir.E, Dir.S, Dir.W };

        void Reset()
        {
            for (int i = 0; i < rooms.Length; i++) rooms[i] = new Room();
            cx = cy = FloorSize / 2;
        }

        /// <summary>A fresh floor: a safe start room, combat rooms, Marin's market and the boss.</summary>
        public void Generate(DRandom rng)
        {
            Reset();
            var start = Cur;
            start.exists = start.isStart = start.cleared = start.visited = true;

            // Scatter rooms by repeatedly budding off an existing one.
            int target = 9 + rng.Range(0, 3), count = 1, guard = 0;
            while (count < target && guard++ < 4000)
            {
                int rx = rng.Range(0, FloorSize - 1), ry = rng.Range(0, FloorSize - 1);
                if (!this[rx, ry].exists) continue;
                int dir = rng.Range(0, 3);
                int nx = rx + StepX[dir], ny = ry + StepY[dir];
                if (nx < 0 || ny < 0 || nx >= FloorSize || ny >= FloorSize || this[nx, ny].exists) continue;
                if (rng.Range(0, 100) < 55) { this[nx, ny].exists = true; count++; }
            }

            // Doors to every existing neighbour.
            for (int y = 0; y < FloorSize; y++)
                for (int x = 0; x < FloorSize; x++)
                {
                    if (!this[x, y].exists) continue;
                    Dir d = Dir.None;
                    for (int k = 0; k < 4; k++)
                    {
                        int nx = x + StepX[k], ny = y + StepY[k];
                        if (nx >= 0 && ny >= 0 && nx < FloorSize && ny < FloorSize && this[nx, ny].exists) d |= Bits[k];
                    }
                    this[x, y].doors = d;
                }

            // The boss waits in the room farthest from the start.
            var dist = new int[FloorSize * FloorSize];
            for (int i = 0; i < dist.Length; i++) dist[i] = -1;
            var queue = new System.Collections.Generic.Queue<(int x, int y)>();
            dist[cy * FloorSize + cx] = 0;
            queue.Enqueue((cx, cy));
            int bx = cx, by = cy, best = 0;
            while (queue.Count > 0)
            {
                var (x, y) = queue.Dequeue();
                for (int k = 0; k < 4; k++)
                {
                    if ((this[x, y].doors & Bits[k]) == 0) continue;
                    int nx = x + StepX[k], ny = y + StepY[k];
                    if (dist[ny * FloorSize + nx] != -1) continue;
                    dist[ny * FloorSize + nx] = dist[y * FloorSize + x] + 1;
                    if (dist[ny * FloorSize + nx] > best) { best = dist[ny * FloorSize + nx]; bx = nx; by = ny; }
                    queue.Enqueue((nx, ny));
                }
            }
            this[bx, by].isBoss = true;

            // Marin's market goes in a dead end if there is one.
            int sx = -1, sy = -1, fx = -1, fy = -1;
            for (int y = 0; y < FloorSize; y++)
                for (int x = 0; x < FloorSize; x++)
                {
                    var r = this[x, y];
                    if (!r.exists || r.isStart || r.isBoss) continue;
                    int doors = 0;
                    foreach (var b in Bits) if ((r.doors & b) != 0) doors++;
                    if (doors == 1 && (sx < 0 || rng.Range(0, 1) == 1)) { sx = x; sy = y; }
                    if (fx < 0 || rng.Range(0, 2) == 0) { fx = x; fy = y; }
                }
            if (sx < 0) { sx = fx; sy = fy; }
            if (sx >= 0) { this[sx, sy].isShop = true; this[sx, sy].cleared = true; }

            foreach (var r in rooms) if (r.exists) r.BuildLayout(rng);
        }

        /// <summary>A single sealed boss room (Boss Rush).</summary>
        public void GenerateArena(DRandom rng)
        {
            Reset();
            var arena = Cur;
            arena.exists = arena.isBoss = arena.visited = true;
            arena.BuildLayout(rng);
        }
    }

    public static class RoomPhysics
    {
        static Dir DoorCell(int tx, int ty)
        {
            int midX = RoomTW / 2, midY = RoomTH / 2;
            if (ty == 0 && tx == midX) return Dir.N;
            if (ty == RoomTH - 1 && tx == midX) return Dir.S;
            if (tx == 0 && ty == midY) return Dir.W;
            if (tx == RoomTW - 1 && ty == midY) return Dir.E;
            return Dir.None;
        }

        /// <summary>Is tile (tx, ty) solid? Door cells open only when doorsOpen.</summary>
        public static bool WallSolid(Room r, int tx, int ty, bool doorsOpen)
        {
            bool border = tx <= 0 || ty <= 0 || tx >= RoomTW - 1 || ty >= RoomTH - 1;
            if (!border) return r.At(tx, ty) == RoomTile.Wall;
            var dir = DoorCell(tx, ty);
            return !(dir != Dir.None && (r.doors & dir) != 0 && doorsOpen);
        }

        public static bool WallAt(Room r, Vector2 p, bool doorsOpen) =>
            WallSolid(r, Mathf.FloorToInt(p.x / Tile), Mathf.FloorToInt(p.y / Tile), doorsOpen);

        public static bool OnSpikes(Room r, Vector2 p)
        {
            int tx = Mathf.FloorToInt(p.x / Tile), ty = Mathf.FloorToInt(p.y / Tile);
            if (tx <= 0 || ty <= 0 || tx >= RoomTW - 1 || ty >= RoomTH - 1) return false;
            return r.At(tx, ty) == RoomTile.Spikes;
        }

        /// <summary>Moves a circle, sliding along walls (axis-separated).</summary>
        public static Vector2 Move(Vector2 pos, Vector2 delta, float radius, Room r, bool doorsOpen)
        {
            var np = pos;
            np.x += delta.x;
            for (int ty = Mathf.FloorToInt((pos.y - radius) / Tile); ty <= Mathf.FloorToInt((pos.y + radius) / Tile); ty++)
                for (int tx = Mathf.FloorToInt((np.x - radius) / Tile); tx <= Mathf.FloorToInt((np.x + radius) / Tile); tx++)
                    if (WallSolid(r, tx, ty, doorsOpen))
                    {
                        if (delta.x > 0) np.x = tx * Tile - radius - 0.01f;
                        else if (delta.x < 0) np.x = (tx + 1) * Tile + radius + 0.01f;
                    }
            np.y += delta.y;
            for (int ty = Mathf.FloorToInt((np.y - radius) / Tile); ty <= Mathf.FloorToInt((np.y + radius) / Tile); ty++)
                for (int tx = Mathf.FloorToInt((np.x - radius) / Tile); tx <= Mathf.FloorToInt((np.x + radius) / Tile); tx++)
                    if (WallSolid(r, tx, ty, doorsOpen))
                    {
                        if (delta.y > 0) np.y = ty * Tile - radius - 0.01f;
                        else if (delta.y < 0) np.y = (ty + 1) * Tile + radius + 0.01f;
                    }
            return np;
        }

        static bool Walkable(Room r, int tx, int ty) =>
            tx > 0 && ty > 0 && tx < RoomTW - 1 && ty < RoomTH - 1 && r.At(tx, ty) != RoomTile.Wall;

        static readonly int[] DX = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY = { 0, 0, 1, -1, 1, -1, 1, -1 };

        /// <summary>Path cost from every tile to the target (-1 = wall). Spikes cost extra so chasers avoid them.</summary>
        public static void FlowField(Room r, Vector2 target, bool avoidSpikes, int[] field)
        {
            for (int i = 0; i < field.Length; i++) field[i] = -1;
            int gx = Mathf.FloorToInt(target.x / Tile), gy = Mathf.FloorToInt(target.y / Tile);
            if (!Walkable(r, gx, gy)) return;
            var done = new bool[field.Length];
            field[gy * RoomTW + gx] = 0;
            while (true)        // Dijkstra over at most 171 cells: a linear scan for the minimum is fine
            {
                int bx = -1, by = -1, best = int.MaxValue;
                for (int y = 1; y < RoomTH - 1; y++)
                    for (int x = 1; x < RoomTW - 1; x++)
                    {
                        int v = field[y * RoomTW + x];
                        if (!done[y * RoomTW + x] && v >= 0 && v < best) { best = v; bx = x; by = y; }
                    }
                if (bx < 0) break;
                done[by * RoomTW + bx] = true;
                for (int k = 0; k < 8; k++)
                {
                    int nx = bx + DX[k], ny = by + DY[k];
                    if (!Walkable(r, nx, ny)) continue;
                    if (k >= 4 && (!Walkable(r, bx + DX[k], by) || !Walkable(r, bx, by + DY[k]))) continue;   // no corner cutting
                    int nd = best + (k >= 4 ? 14 : 10) + (avoidSpikes && r.At(nx, ny) == RoomTile.Spikes ? 40 : 0);
                    int cur = field[ny * RoomTW + nx];
                    if (cur < 0 || nd < cur) field[ny * RoomTW + nx] = nd;
                }
            }
        }

        /// <summary>Can a circle of this radius slide straight from a to b?</summary>
        public static bool LineClear(Room r, Vector2 a, Vector2 b, float radius)
        {
            var d = b - a;
            float len = d.magnitude;
            if (len < 1) return true;
            var dir = d / len;
            var side = new Vector2(-dir.y, dir.x) * radius;
            bool Open(Vector2 q) => Walkable(r, Mathf.FloorToInt(q.x / Tile), Mathf.FloorToInt(q.y / Tile));
            for (float s = 0; s <= len; s += 6)
            {
                var p = a + dir * s;
                if (!Open(p) || !Open(p + side) || !Open(p - side)) return false;
            }
            return true;
        }

        /// <summary>Where a chaser standing at `from` should head next.</summary>
        public static Vector2 NextWaypoint(Room r, int[] field, Vector2 from)
        {
            int tx = Mathf.FloorToInt(from.x / Tile), ty = Mathf.FloorToInt(from.y / Tile);
            int bestX = tx, bestY = ty;
            bool inside = tx >= 0 && ty >= 0 && tx < RoomTW && ty < RoomTH;
            int best = inside && field[ty * RoomTW + tx] >= 0 ? field[ty * RoomTW + tx] : int.MaxValue;
            for (int k = 0; k < 8; k++)
            {
                int nx = tx + DX[k], ny = ty + DY[k];
                if (nx < 0 || ny < 0 || nx >= RoomTW || ny >= RoomTH || field[ny * RoomTW + nx] < 0) continue;
                if (k >= 4 && (!Walkable(r, tx + DX[k], ty) || !Walkable(r, tx, ty + DY[k]))) continue;
                if (field[ny * RoomTW + nx] < best) { best = field[ny * RoomTW + nx]; bestX = nx; bestY = ny; }
            }
            return new Vector2(bestX * Tile + Tile / 2f, bestY * Tile + Tile / 2f);
        }
    }
}
