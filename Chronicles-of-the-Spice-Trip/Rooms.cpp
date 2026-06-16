// ============================================================================
//  Rooms.cpp — dungeon generation + room collision.
// ============================================================================
#include "Rooms.h"
#include <cmath>

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
        int dir = GetRandomValue(0, 3);
        int dx = (dir == DIR_E ? 1 : dir == DIR_W ? -1 : 0); // note: dir is 0..3 below
        // map 0..3 -> N/E/S/W deltas
        dx = 0; int dy = 0;
        if (dir == 0) dy = -1; else if (dir == 1) dx = 1; else if (dir == 2) dy = 1; else dx = -1;
        int nx = rx + dx, ny = ry + dy;
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
    if (!border) return false;
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
