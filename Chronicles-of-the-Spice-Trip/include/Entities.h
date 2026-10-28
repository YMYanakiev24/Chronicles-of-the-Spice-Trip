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
    float   lunge = 0, lungeCd = 0;    // hounds: short burst of speed at the player
    float   flankSide = 1;             // which side this enemy circles toward (+1/-1)
    int     kind = 0;                 // EnemyKind (Sprites.h)
    float   animT = 0;                // running clock for idle/move loops
    float   atkAnim = 0;              // >0 while the attack animation plays
    float   deathT = 0;               // time since death (death animation)
    bool    dying = false, flip = false, moving = false;
};

struct Particle { Vector2 pos, vel; float life, maxLife, size; Color color; };

// Skills Marin sells (bit flags in Player::skills).
enum Skill {
    SK_IRON_SKIN = 1 << 0,   // take 25% less damage
    SK_SWIFT = 1 << 1,   // +15% move speed
    SK_BLADE = 1 << 2,   // +8 melee damage
    SK_ARCANE = 1 << 3,   // +60% mana regen
    SK_DASH = 1 << 4,   // half dodge-roll cooldown
    SK_LEECH = 1 << 5,   // melee hits heal 2 HP
};

// Vapes Marin sells: timed buffs (seconds left in Player::vapeT). Also the frame in SPR.vapes.
enum Vape {
    VP_MANGO = 0,   // move 35% faster
    VP_RAZZ,        // powers cost no mana
    VP_MELON,       // +12 blade damage
    VP_MINT,        // take 50% less damage
    VP_COUNT
};

struct Player {
    Vector2 pos{ MAP_W * TILE / 2.0f, MAP_H * TILE / 2.0f };
    Vector2 facing{ 0, 1 };
    int   level = 1, xp = 0, xpNext = 60;
    float maxHp = 100, hp = 100, maxMana = 100, mana = 100;
    int   selected = 0;
    int   gold = 0;
    unsigned skills = 0;      // Skill bit flags
    bool  has(Skill s) const { return (skills & s) != 0; }
    float vapeT[VP_COUNT] = {};   // seconds each vape buff has left
    bool  buzzing(Vape v) const { return vapeT[v] > 0; }
    float castCd = 0, meleeCd = 0, meleeAnim = 0, roll = 0, rollCd = 0, invuln = 0, hitFlash = 0;
    Vector2 rollDir{ 0, 1 };
};
