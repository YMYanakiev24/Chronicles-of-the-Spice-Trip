// ============================================================================
//  Rooms.cpp — dungeon generation + room collision.
// ============================================================================
#include "Rooms.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include "raymath.h"

void Dungeon::generate() {
    for (int y = 0; y < DUNGEON_G; ++y)
        for (int x = 0; x < DUNGEON_G; ++x)
            grid[y][x] = Room{};

    cx = cy = DUNGEON_G / 2;
    Room& start = grid[cy][cx];
    start.exists = true;
    start.isStart = true;
    start.cleared = true;       // the start room is safe
    start.visited = true;

    // Scatter rooms by repeatedly budding off an existing room.
    int target = 9 + GetRandomValue(0, 3);
    int count = 1, guard = 0;
    while (count < target && guard++ < 4000) {
        int rx = GetRandomValue(0, DUNGEON_G - 1);
        int ry = GetRandomValue(0, DUNGEON_G - 1);
        if (!grid[ry][rx].exists) continue;
        static const int stepX[4] = { 0, 1, 0, -1 }, stepY[4] = { -1, 0, 1, 0 };   // N, E, S, W
        int dir = GetRandomValue(0, 3);
        int nx = rx + stepX[dir], ny = ry + stepY[dir];
        if (nx < 0 || ny < 0 || nx >= DUNGEON_G || ny >= DUNGEON_G) continue;
        if (grid[ny][nx].exists) continue;
        if (GetRandomValue(0, 100) < 55) { grid[ny][nx].exists = true; count++; }
    }

    // Connect doors to every existing orthogonal neighbour (keeps it linked).
    for (int y = 0; y < DUNGEON_G; ++y)
        for (int x = 0; x < DUNGEON_G; ++x) {
            if (!grid[y][x].exists) continue;
            int d = 0;
            if (y > 0 && grid[y - 1][x].exists)               d |= DIR_N;
            if (x < DUNGEON_G - 1 && grid[y][x + 1].exists)   d |= DIR_E;
            if (y < DUNGEON_G - 1 && grid[y + 1][x].exists)   d |= DIR_S;
            if (x > 0 && grid[y][x - 1].exists)               d |= DIR_W;
            grid[y][x].doors = d;
        }

    // Boss room = the existing room farthest from start (BFS over doors).
    int dist[DUNGEON_G][DUNGEON_G];
    for (int y = 0; y < DUNGEON_G; ++y) for (int x = 0; x < DUNGEON_G; ++x) dist[y][x] = -1;
    int qx[DUNGEON_G * DUNGEON_G], qy[DUNGEON_G * DUNGEON_G], head = 0, tail = 0;
    dist[cy][cx] = 0; qx[tail] = cx; qy[tail] = cy; tail++;
    int bx = cx, by = cy, best = 0;
    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        int d = grid[y][x].doors;
        const int ddx[4] = { 0, 1, 0, -1 }, ddy[4] = { -1, 0, 1, 0 };
        const int dbit[4] = { DIR_N, DIR_E, DIR_S, DIR_W };
        for (int k = 0; k < 4; ++k) {
            if (!(d & dbit[k])) continue;
            int nx = x + ddx[k], ny = y + ddy[k];
            if (nx < 0 || ny < 0 || nx >= DUNGEON_G || ny >= DUNGEON_G) continue;
            if (!grid[ny][nx].exists || dist[ny][nx] != -1) continue;
            dist[ny][nx] = dist[y][x] + 1;
            if (dist[ny][nx] > best && !grid[ny][nx].isStart) { best = dist[ny][nx]; bx = nx; by = ny; }
            qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
    grid[by][bx].isBoss = true;

    // Marin's market goes in a dead-end room if there is one (any room otherwise).
    {
        int sx = -1, sy = -1, fallbackX = -1, fallbackY = -1;
        for (int y = 0; y < DUNGEON_G; ++y)
            for (int x = 0; x < DUNGEON_G; ++x) {
                Room& rm = grid[y][x];
                if (!rm.exists || rm.isStart || rm.isBoss) continue;
                int n = 0;
                for (int b : { DIR_N, DIR_E, DIR_S, DIR_W }) if (rm.doors & b) n++;
                if (n == 1 && (sx < 0 || GetRandomValue(0, 1))) { sx = x; sy = y; }
                if (fallbackX < 0 || GetRandomValue(0, 2) == 0) { fallbackX = x; fallbackY = y; }
            }
        if (sx < 0) { sx = fallbackX; sy = fallbackY; }
        if (sx >= 0) { grid[sy][sx].isShop = true; grid[sy][sx].cleared = true; }
    }

    // The boss key waits in one random combat room.
    {
        int kx = -1, ky = -1, n = 0;
        for (int y = 0; y < DUNGEON_G; ++y)
            for (int x = 0; x < DUNGEON_G; ++x) {
                const Room& rm = grid[y][x];
                if (!rm.exists || rm.isStart || rm.isBoss || rm.isShop) continue;
                if (GetRandomValue(0, n++) == 0) { kx = x; ky = y; }
            }
        if (kx >= 0) grid[ky][kx].hasKey = true;
        keyFound = kx < 0;
    }

    for (int y = 0; y < DUNGEON_G; ++y)
        for (int x = 0; x < DUNGEON_G; ++x)
            if (grid[y][x].exists) grid[y][x].buildLayout();
}

bool Dungeon::bossDoorLocked(int dir) const {
    if (keyFound || !(cur().doors & dir)) return false;
    int nx = cx + (dir == DIR_E) - (dir == DIR_W), ny = cy + (dir == DIR_S) - (dir == DIR_N);
    if (nx < 0 || ny < 0 || nx >= DUNGEON_G || ny >= DUNGEON_G) return false;
    return grid[ny][nx].isBoss;
}

void Dungeon::generateArena() {
    for (auto& row : grid) for (auto& r : row) r = Room{};
    cx = cy = DUNGEON_G / 2;
    Room& arena = grid[cy][cx];
    keyFound = true;                                      // nothing to unlock in the arena
    arena.exists = arena.isBoss = arena.visited = true;   // no doors: the only way out is through
    arena.buildLayout();
}

bool Room::isFloor(int tx, int ty) const {
    if (tx <= 0 || ty <= 0 || tx >= ROOM_TW - 1 || ty >= ROOM_TH - 1) return false;
    return tiles[ty][tx] == RT_FLOOR;
}

// Carve the room's outline: tiles outside the chosen shape become solid void.
// The centre row and column are never carved, so every door stays connected.
static void carveShape(Room& r) {
    int midX = ROOM_TW / 2, midY = ROOM_TH / 2;
    int shape = GetRandomValue(0, 6), quad = GetRandomValue(0, 3);
    for (int y = 1; y < ROOM_TH - 1; ++y)
        for (int x = 1; x < ROOM_TW - 1; ++x) {
            if (x == midX || y == midY) continue;
            int ax = abs(x - midX), ay = abs(y - midY);           // distance from the centre lanes
            int ex = std::min(x - 1, ROOM_TW - 2 - x), ey = std::min(y - 1, ROOM_TH - 2 - y);   // from the edges
            bool right = x > midX, down = y > midY;
            bool cut = false;
            switch (shape) {
            case 0: cut = ax > 3 && ay > 1; break;                                   // plus / cross
            case 1: cut = ex + ey < 3; break;                                        // octagon (clipped corners)
            case 2: cut = ax > 1 && ay > 1 && right == (quad & 1) && down == (quad >> 1); break;   // L (one quadrant gone)
            case 3: cut = ax > 1 && ay > 1 && (right == down) == (quad & 1); break;  // Z (two opposite quadrants gone)
            case 4: cut = ax * 4 + ay * 9 > 46; break;                               // diamond
            case 5: cut = ay > 1 && ax > 2 && (down == (quad & 1)); break;            // T (a bar with a stem)
            default: cut = (ax > 6 && ay > 0 && ay < 4) || (ex + ey < 2); break;     // hourglass-ish pinched sides
            }
            if (cut) r.tiles[y][x] = RT_VOID;
        }
}

// Mount up to MAX_FLAMERS fire-throwers on walls, aimed into open floor.
static void placeFlamers(Room& r) {
    int midX = ROOM_TW / 2, midY = ROOM_TH / 2;
    static const int ddx[4] = { 0, 1, 0, -1 }, ddy[4] = { -1, 0, 1, 0 };
    static const int dbit[4] = { DIR_N, DIR_E, DIR_S, DIR_W };
    int want = GetRandomValue(0, 99) < 35 ? 0 : GetRandomValue(1, MAX_FLAMERS);
    for (int tries = 0; tries < 300 && r.flamerCount < want; ++tries) {
        int x = GetRandomValue(1, ROOM_TW - 2), y = GetRandomValue(1, ROOM_TH - 2);
        if (x == midX || y == midY || r.tiles[y][x] != RT_FLOOR) continue;
        int k = GetRandomValue(0, 3);
        int bx = x - ddx[k], by = y - ddy[k];                     // it must be backed by a wall
        bool backed = bx <= 0 || by <= 0 || bx >= ROOM_TW - 1 || by >= ROOM_TH - 1 || tileSolid(r.tiles[by][bx]);
        if (!backed) continue;
        bool open = true;
        for (int s = 1; s <= 2 && open; ++s) {
            int fx = x + ddx[k] * s, fy = y + ddy[k] * s;
            open = fx > 0 && fy > 0 && fx < ROOM_TW - 1 && fy < ROOM_TH - 1 && !tileSolid(r.tiles[fy][fx]);
        }
        if (!open) continue;
        r.tiles[y][x] = RT_FLAMER;
        r.flamers[r.flamerCount++] = { x, y, dbit[k], frand(0, FLAMER_CYCLE) };
    }
}

int flamerState(const Flamer& f, float t) {
    float ph = fmodf(t + f.phase, FLAMER_CYCLE);
    float idle = FLAMER_CYCLE - FLAMER_WARN - FLAMER_BURN;
    return ph < idle ? 0 : ph < idle + FLAMER_WARN ? 1 : 2;
}

int flameTiles(const Room& r, const Flamer& f, int outX[], int outY[]) {
    int dx = (f.dir == DIR_E) - (f.dir == DIR_W), dy = (f.dir == DIR_S) - (f.dir == DIR_N);
    int n = 0;
    for (int s = 1; s <= FLAMER_RANGE; ++s) {
        int x = f.tx + dx * s, y = f.ty + dy * s;
        if (x <= 0 || y <= 0 || x >= ROOM_TW - 1 || y >= ROOM_TH - 1 || tileSolid(r.tiles[y][x])) break;
        outX[n] = x; outY[n] = y; n++;
    }
    return n;
}

bool inFlames(const Room& r, Vector2 p, float t) {
    int tx = (int)floorf(p.x / TILE), ty = (int)floorf(p.y / TILE);
    int fx[FLAMER_RANGE], fy[FLAMER_RANGE];
    for (int i = 0; i < r.flamerCount; ++i) {
        if (flamerState(r.flamers[i], t) != 2) continue;
        for (int k = 0, n = flameTiles(r, r.flamers[i], fx, fy); k < n; ++k)
            if (fx[k] == tx && fy[k] == ty) return true;
    }
    return false;
}

// Fill a rectangle of interior tiles (clipped to the interior).
static void fillRect(Room& r, int x0, int y0, int w, int h, unsigned char v) {
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            if (x > 0 && y > 0 && x < ROOM_TW - 1 && y < ROOM_TH - 1) r.tiles[y][x] = v;
}

void Room::buildLayout() {
    for (auto& row : tiles) for (auto& c : row) c = RT_FLOOR;
    hasChest = chestOpened = false;
    flamerCount = 0;
    if (isStart || isBoss || isShop) return;    // start, boss and shop rooms stay plain rectangles

    int midX = ROOM_TW / 2, midY = ROOM_TH / 2;
    switch (GetRandomValue(0, 5)) {
    case 0:     // four corner blocks
        fillRect(*this, 3, 2, 3, 2, RT_WALL); fillRect(*this, ROOM_TW - 6, 2, 3, 2, RT_WALL);
        fillRect(*this, 3, ROOM_TH - 4, 3, 2, RT_WALL); fillRect(*this, ROOM_TW - 6, ROOM_TH - 4, 3, 2, RT_WALL);
        break;
    case 1:     // pillar grid
        for (int y = 2; y < ROOM_TH - 2; y += 3)
            for (int x = 3; x < ROOM_TW - 3; x += 4) tiles[y][x] = RT_WALL;
        break;
    case 2:     // two long horizontal walls
        fillRect(*this, 3, 3, midX - 4, 1, RT_WALL); fillRect(*this, midX + 2, ROOM_TH - 4, midX - 4, 1, RT_WALL);
        break;
    case 3:     // central block ringed by spikes
        fillRect(*this, midX - 3, midY - 2, 7, 5, RT_SPIKES);
        fillRect(*this, midX - 2, midY - 1, 5, 3, RT_WALL);
        break;
    case 4:     // vertical walls with spike lanes
        fillRect(*this, 5, 2, 1, ROOM_TH - 4, RT_WALL); fillRect(*this, ROOM_TW - 6, 2, 1, ROOM_TH - 4, RT_WALL);
        fillRect(*this, 7, 2, 2, ROOM_TH - 4, RT_SPIKES); fillRect(*this, ROOM_TW - 9, 2, 2, ROOM_TH - 4, RT_SPIKES);
        break;
    default:    // scattered rubble + traps
        for (int i = 0; i < 10; ++i)
            tiles[GetRandomValue(2, ROOM_TH - 3)][GetRandomValue(2, ROOM_TW - 3)] = RT_WALL;
        for (int i = 0; i < 6; ++i)
            tiles[GetRandomValue(2, ROOM_TH - 3)][GetRandomValue(2, ROOM_TW - 3)] = RT_SPIKES;
        break;
    }
    // A few extra traps in any layout.
    for (int i = 0, n = GetRandomValue(0, 3); i < n; ++i) {
        int x = GetRandomValue(2, ROOM_TW - 3), y = GetRandomValue(2, ROOM_TH - 3);
        if (tiles[y][x] == RT_FLOOR) tiles[y][x] = RT_SPIKES;
    }

    carveShape(*this);
    placeFlamers(*this);

    // Keep the door lanes (centre row/column) clear so every door is reachable.
    for (int x = 1; x < ROOM_TW - 1; ++x) if (tiles[midY][x] == RT_WALL) tiles[midY][x] = RT_FLOOR;
    for (int y = 1; y < ROOM_TH - 1; ++y) if (tiles[y][midX] == RT_WALL) tiles[y][midX] = RT_FLOOR;

    // Wall off any pocket the player could never reach (flood fill from a door lane).
    bool seen[ROOM_TH][ROOM_TW] = {};
    int qx[ROOM_TW * ROOM_TH], qy[ROOM_TW * ROOM_TH], head = 0, tail = 0;
    seen[midY][1] = true; qx[tail] = 1; qy[tail] = midY; tail++;
    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx <= 0 || ny <= 0 || nx >= ROOM_TW - 1 || ny >= ROOM_TH - 1) continue;
            if (seen[ny][nx] || tileSolid(tiles[ny][nx])) continue;
            seen[ny][nx] = true; qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
    for (int y = 1; y < ROOM_TH - 1; ++y)
        for (int x = 1; x < ROOM_TW - 1; ++x)
            if (!seen[y][x] && !tileSolid(tiles[y][x])) tiles[y][x] = RT_WALL;

    // Roughly half the rooms hide a chest on a reachable floor tile.
    if (GetRandomValue(0, 99) < 50) {
        for (int tries = 0; tries < 200; ++tries) {
            int x = GetRandomValue(2, ROOM_TW - 3), y = GetRandomValue(2, ROOM_TH - 3);
            if (x == midX || y == midY || !isFloor(x, y)) continue;
            hasChest = true; chestTx = x; chestTy = y;
            break;
        }
    }
}

bool onSpikes(const Room& r, Vector2 p) {
    int tx = (int)floorf(p.x / TILE), ty = (int)floorf(p.y / TILE);
    if (tx <= 0 || ty <= 0 || tx >= ROOM_TW - 1 || ty >= ROOM_TH - 1) return false;
    return r.tiles[ty][tx] == RT_SPIKES;
}

// Which door cell (if any) is tile (tx,ty), and in which direction?
static int doorCellDir(int tx, int ty) {
    int midX = ROOM_TW / 2, midY = ROOM_TH / 2;
    if (ty == 0 && tx == midX)             return DIR_N;
    if (ty == ROOM_TH - 1 && tx == midX)   return DIR_S;
    if (tx == 0 && ty == midY)             return DIR_W;
    if (tx == ROOM_TW - 1 && ty == midY)   return DIR_E;
    return 0;
}

bool roomWallSolid(const Room& r, int tx, int ty, bool doorsOpen) {
    bool border = (tx <= 0 || ty <= 0 || tx >= ROOM_TW - 1 || ty >= ROOM_TH - 1);
    if (!border) return tileSolid(r.tiles[ty][tx]);
    int dir = doorCellDir(tx, ty);
    if (dir && (r.doors & dir) && doorsOpen) return false;  // open passage
    return true;
}

Vector2 moveInRoom(Vector2 pos, Vector2 delta, float radius, const Room& r, bool doorsOpen) {
    Vector2 np = pos;

    np.x += delta.x;
    {
        int minTx = (int)floorf((np.x - radius) / TILE), maxTx = (int)floorf((np.x + radius) / TILE);
        int minTy = (int)floorf((pos.y - radius) / TILE), maxTy = (int)floorf((pos.y + radius) / TILE);
        for (int ty = minTy; ty <= maxTy; ++ty)
            for (int tx = minTx; tx <= maxTx; ++tx)
                if (roomWallSolid(r, tx, ty, doorsOpen)) {
                    if (delta.x > 0)      np.x = tx * TILE - radius - 0.01f;
                    else if (delta.x < 0) np.x = (tx + 1) * TILE + radius + 0.01f;
                }
    }
    np.y += delta.y;
    {
        int minTx = (int)floorf((np.x - radius) / TILE), maxTx = (int)floorf((np.x + radius) / TILE);
        int minTy = (int)floorf((np.y - radius) / TILE), maxTy = (int)floorf((np.y + radius) / TILE);
        for (int ty = minTy; ty <= maxTy; ++ty)
            for (int tx = minTx; tx <= maxTx; ++tx)
                if (roomWallSolid(r, tx, ty, doorsOpen)) {
                    if (delta.y > 0)      np.y = ty * TILE - radius - 0.01f;
                    else if (delta.y < 0) np.y = (ty + 1) * TILE + radius + 0.01f;
                }
    }
    return np;
}

// ---------------------------------------------------------------- pathfinding
static bool walkable(const Room& r, int tx, int ty) {
    if (tx <= 0 || ty <= 0 || tx >= ROOM_TW - 1 || ty >= ROOM_TH - 1) return false;
    return !tileSolid(r.tiles[ty][tx]);
}

void buildFlowField(const Room& r, Vector2 target, bool avoidSpikes, FlowField& out) {
    for (auto& row : out) for (auto& c : row) c = -1;
    int gx = (int)floorf(target.x / TILE), gy = (int)floorf(target.y / TILE);
    if (!walkable(r, gx, gy)) return;

    // Dijkstra over a tiny grid (<= 231 cells): a linear scan for the minimum is fine.
    bool done[ROOM_TH][ROOM_TW] = {};
    out[gy][gx] = 0;
    const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    for (;;) {
        int bx = -1, by = -1, best = 1 << 30;
        for (int y = 1; y < ROOM_TH - 1; ++y)
            for (int x = 1; x < ROOM_TW - 1; ++x)
                if (!done[y][x] && out[y][x] >= 0 && out[y][x] < best) { best = out[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        done[by][bx] = true;
        for (int k = 0; k < 8; ++k) {
            int nx = bx + dx[k], ny = by + dy[k];
            if (!walkable(r, nx, ny)) continue;
            // diagonals may not cut a wall corner
            if (k >= 4 && (!walkable(r, bx + dx[k], by) || !walkable(r, bx, by + dy[k]))) continue;
            int cost = (k >= 4 ? 14 : 10) + (avoidSpikes && r.tiles[ny][nx] == RT_SPIKES ? 40 : 0);
            int nd = best + cost;
            if (out[ny][nx] < 0 || nd < out[ny][nx]) out[ny][nx] = nd;
        }
    }
}

bool lineClear(const Room& r, Vector2 a, Vector2 b, float radius) {
    Vector2 d = Vector2Subtract(b, a);
    float len = Vector2Length(d);
    if (len < 1) return true;
    Vector2 dir = Vector2Scale(d, 1.0f / len), side = { -dir.y * radius, dir.x * radius };
    for (float s = 0; s <= len; s += 6) {
        Vector2 p = Vector2Add(a, Vector2Scale(dir, s));
        for (Vector2 q : { p, Vector2Add(p, side), Vector2Subtract(p, side) })
            if (!walkable(r, (int)floorf(q.x / TILE), (int)floorf(q.y / TILE))) return false;
    }
    return true;
}

Vector2 nextWaypoint(const Room& r, const FlowField& f, Vector2 from, float radius) {
    int tx = (int)floorf(from.x / TILE), ty = (int)floorf(from.y / TILE);
    int bestX = tx, bestY = ty;
    int best = (tx >= 0 && ty >= 0 && tx < ROOM_TW && ty < ROOM_TH && f[ty][tx] >= 0) ? f[ty][tx] : (1 << 30);
    const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    for (int k = 0; k < 8; ++k) {
        int nx = tx + dx[k], ny = ty + dy[k];
        if (nx < 0 || ny < 0 || nx >= ROOM_TW || ny >= ROOM_TH || f[ny][nx] < 0) continue;
        if (k >= 4 && (!walkable(r, tx + dx[k], ty) || !walkable(r, tx, ty + dy[k]))) continue;
        if (f[ny][nx] < best) { best = f[ny][nx]; bestX = nx; bestY = ny; }
    }
    Vector2 step = { bestX * (float)TILE + TILE / 2.0f, bestY * (float)TILE + TILE / 2.0f };
    if (radius > 0 && !lineClear(r, from, step, radius))
        return { tx * (float)TILE + TILE / 2.0f, ty * (float)TILE + TILE / 2.0f };
    return step;
}
