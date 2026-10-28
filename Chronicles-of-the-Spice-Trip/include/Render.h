// ============================================================================
//  Render.h — the hero, monsters and projectiles, plus asset lookup.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Entities.h"
#include <string>

void drawShadow(Vector2 p, float r);
void drawHero(Vector2 p, Vector2 facing, float meleeAnim, bool flash, Color cloak, float lift = 0);
void drawEnemy(const Enemy& e);
void drawProjectile(const Projectile& p, float t);

// First existing path among candidates (handles VS vs. exe working dirs).
std::string findAsset(const char* file);
