// ============================================================================
//  World.h — the tile map and movement collision.
//  0 = ground, 1 = tree/prop (blocks movement).
// ============================================================================
#pragma once

#include "raylib.h"
#include "Common.h"

extern int g_tiles[MAP_H][MAP_W];

bool    isSolidTile(int tx, int ty);
void    generateMap();
Vector2 moveWithCollision(Vector2 pos, Vector2 delta, float r);
