// ============================================================================
//  World.cpp — map generation and circle-vs-tile collision.
// ============================================================================
#include "World.h"
#include <cmath>
#include <cstdlib>

int g_tiles[MAP_H][MAP_W];

bool isSolidTile(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return true;  // border = wall
    return g_tiles[ty][tx] == 1;
}

void generateMap() {
    SetRandomSeed(1337);
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            bool border = (x < 2 || y < 2 || x >= MAP_W - 2 || y >= MAP_H - 2);
            bool nearCenter = (abs(x - MAP_W / 2) < 6 && abs(y - MAP_H / 2) < 6);
            bool tree = !nearCenter && GetRandomValue(0, 99) < 16;
            g_tiles[y][x] = (border || tree) ? 1 : 0;
        }
    }
}

// Axis-separated resolution so the player slides along walls.
Vector2 moveWithCollision(Vector2 pos, Vector2 delta, float r) {
    Vector2 np = pos;

    // --- X axis ---
    np.x += delta.x;
    {
        int minTx = (int)floorf((np.x - r) / TILE);
        int maxTx = (int)floorf((np.x + r) / TILE);
        int minTy = (int)floorf((pos.y - r) / TILE);
        int maxTy = (int)floorf((pos.y + r) / TILE);
        for (int ty = minTy; ty <= maxTy; ++ty) {
            for (int tx = minTx; tx <= maxTx; ++tx) {
                if (!isSolidTile(tx, ty)) continue;
                if (delta.x > 0)      np.x = tx * TILE - r - 0.01f;        // hit left face
                else if (delta.x < 0) np.x = (tx + 1) * TILE + r + 0.01f;  // hit right face
            }
        }
    }

    // --- Y axis ---
    np.y += delta.y;
    {
        int minTx = (int)floorf((np.x - r) / TILE);
        int maxTx = (int)floorf((np.x + r) / TILE);
        int minTy = (int)floorf((np.y - r) / TILE);
        int maxTy = (int)floorf((np.y + r) / TILE);
        for (int ty = minTy; ty <= maxTy; ++ty) {
            for (int tx = minTx; tx <= maxTx; ++tx) {
                if (!isSolidTile(tx, ty)) continue;
                if (delta.y > 0)      np.y = ty * TILE - r - 0.01f;        // hit top face
                else if (delta.y < 0) np.y = (ty + 1) * TILE + r + 0.01f;  // hit bottom face
            }
        }
    }

    return np;
}
