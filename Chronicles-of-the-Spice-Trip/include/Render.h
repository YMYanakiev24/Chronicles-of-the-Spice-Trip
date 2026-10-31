// ============================================================================
//  Render.h — the hero, monsters and projectiles, plus asset lookup.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Entities.h"
#include "Sprites.h"
#include <string>

void drawShadow(Vector2 p, float r);
void drawHero(Vector2 p, Vector2 facing, float meleeAnim, bool flash, Color cloak, float lift = 0);
// The weapon in hand: a picture rising `slant` degrees from hilt to tip, drawn `length` long.
struct Blade { Texture2D tex; float slant; float length; };
// The champion walking as their own figure, facing `look` (y down), with the blade
// while it swings. Falls back to the procedural hero when the walk sheet is missing.
void drawChampion(const WalkSheet& w, Vector2 p, Vector2 look, bool walking, float time,
                  Vector2 facing, float meleeAnim, bool flash, Color cloak, const Blade& blade, float lift = 0);
// One frame of a walking figure, as large as fits in `box` (3:4), feet at the bottom.
void drawFigureIn(const WalkSheet& w, Facing facing, int frame, Rectangle box, Color tint = WHITE);
// A row of four figures (front, left, back, right) walking in place inside `box`.
void drawTurnaround(const WalkSheet& w, Rectangle box, float time);
void drawEnemy(const Enemy& e);
void drawProjectile(const Projectile& p, float t);

// Combat and loot effects.
void drawBurn(const Enemy& e, float t);                             // flames licking at a burning enemy
void drawSpellCharge(int spell, Vector2 hand, float k, float t);    // k runs 0 to 1 as it winds up
void drawIceSpike(const IceSpike& s);
void drawCrack(const FloorCrack& c);
void drawArc(const Arc& a, float t);
void drawFlash(const Flash& f);
void drawStar(Vector2 c, float r, float turn, Color col);
void drawChestLight(Vector2 mouth, float k, float t);

// First existing path among candidates (handles VS vs. exe working dirs).
std::string findAsset(const char* file);
