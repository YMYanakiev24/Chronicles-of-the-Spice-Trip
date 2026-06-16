// ============================================================================
//  Render.h — procedural pixel-sprite drawing + asset path lookup.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Entities.h"
#include <string>

void drawShadow(Vector2 p, float r);
void drawHero(Vector2 p, Vector2 facing, float meleeAnim, bool flash);
void drawEnemy(const Enemy& e);
void drawTree(int tx, int ty);

// First existing path among candidates (handles VS vs. exe working dirs).
std::string findAsset(const char* file);
