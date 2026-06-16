// ============================================================================
//  Entities.h — gameplay data: spells, monsters, projectiles, the player.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Common.h"

// A god-power (mirrors SPELLS in GameData.js).
struct SpellDef {
    const char* name;
    const char* school;
    int   manaCost, damage;
    float speed;
    Color color, glow;
    const char* sfx;
};

// The three hotbar powers. Defined in Entities.cpp.
extern SpellDef SPELLS[3];

struct Projectile {
    Vector2 pos, vel;
    int     damage;
    Color   color, glow;
    const char* school;
    float   life;
    bool    alive = true;
    float   r = 6.0f;
};

struct Enemy {
    Vector2 pos;
    float   hp, maxHp, speed;
    int     dmg, xp;
    float   r;
    Color   color;
    const char* name;
    const char* weakness;
    bool    boss = false, alive = true;
    float   hitFlash = 0, atkCd = 0, wobble = 0;
};

struct Particle { Vector2 pos, vel; float life, maxLife, size; Color color; };

struct Player {
    Vector2 pos{ MAP_W * TILE / 2.0f, MAP_H * TILE / 2.0f };
    Vector2 facing{ 0, 1 };
    int   level = 1, xp = 0, xpNext = 60;
    float maxHp = 100, hp = 100, maxMana = 100, mana = 100;
    int   selected = 0;
    float castCd = 0, meleeCd = 0, meleeAnim = 0, roll = 0, rollCd = 0, invuln = 0, hitFlash = 0;
    Vector2 rollDir{ 0, 1 };
};
