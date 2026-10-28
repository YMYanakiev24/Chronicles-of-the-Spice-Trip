// ============================================================================
//  Entities.h — gameplay data: spells, monsters, projectiles, the player.
//  Everything here is plain data (trivially copyable) so the player can be
//  written straight into the save file.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Common.h"

// A god-power on the hotbar.
struct SpellDef {
    const char* name;
    const char* school;      // "Fire" / "Water" / "Lightning": hits a weakness for x1.6
    int   manaCost, damage;
    float speed;
    Color color, glow;
    const char* sfx;
};

inline constexpr int SPELL_COUNT = 3;
extern const SpellDef SPELLS[SPELL_COUNT];

// What a projectile looks like (and, for the player's, where it came from).
enum class ShotKind : unsigned char {
    Spell,      // the player's god-powers
    Coin,       // Daniil & Vasil's coin barrage
    Koi,        // the Storyteller's homing koi
    Orb,        // a plasmodium's glob
    Feather,    // Mordekai's fan
    Ticket,     // Vlado's spiralling tickets
    Ember,      // the Cinder-Maw's fire
};

struct Projectile {
    Vector2  pos{}, vel{};
    int      damage = 0;
    Color    color{}, glow{};
    const char* school = nullptr;
    float    life = 1.4f, r = 6.0f;
    bool     alive = true;
    ShotKind kind = ShotKind::Spell;
    float    homing = 0;          // turn rate toward the nearest target (rad/s)
};

struct Enemy {
    Vector2 pos{};
    float   hp = 1, maxHp = 1, speed = 0;
    int     dmg = 0, xp = 0;
    float   r = 12;
    Color   color{};
    const char* name = "";
    const char* weakness = nullptr;
    bool    boss = false, alive = true;
    float   hitFlash = 0, atkCd = 0, wobble = 0;
    float   lunge = 0, lungeCd = 0;    // hounds: short burst of speed at the player
    float   flankSide = 1;             // which side this enemy circles toward (+1/-1)
    int     kind = 0;                  // EnemyKind (Sprites.h)
    float   animT = 0;                 // running clock for idle/move loops
    float   atkAnim = 0;               // >0 while the attack animation plays
    float   deathT = 0;                // time since death (death animation)
    bool    dying = false, flip = false, moving = false;
    int     lastRollHit = -1;          // Pupesh's roll hits each enemy once per roll

    // Boss bodies (bossForm >= 0) are moved and fired by the BossFight.
    int     bossForm = -1;             // 0 = the boss, 1 = a copy, 2 = a tiny copy
    float   scale = 1;                 // drawn size relative to the boss
    Vector2 roamTo{};
    float   moveCd = 0, shootCd = 1.2f, fireInterval = 1.1f;
    int     spread = 3, volleys = 0;
    float   diveT = 0;                 // Mordekai's dive
    Vector2 diveDir{};
    bool    chaser = false;            // tiny copies just chase
};

struct Particle { Vector2 pos, vel; float life, maxLife, size; Color color; };

// Skills Marin sells (bit flags in Player::skills).
enum Skill {
    SK_IRON_SKIN = 1 << 0,   // take 25% less damage
    SK_SWIFT = 1 << 1,       // +15% move speed
    SK_BLADE = 1 << 2,       // +8 melee damage
    SK_ARCANE = 1 << 3,      // +60% mana regen
    SK_DASH = 1 << 4,        // half dodge-roll cooldown
    SK_LEECH = 1 << 5,       // melee hits heal 2 HP
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
    Vector2 pos{};
    Vector2 facing{ 0, 1 };
    int   level = 1, xp = 0, xpNext = 60;
    float maxHp = 100, hp = 100, maxMana = 100, mana = 100;
    int   selected = 0;
    int   gold = 0;
    unsigned skills = 0;              // Skill bit flags
    bool  has(Skill s) const { return (skills & s) != 0; }
    float vapeT[VP_COUNT] = {};       // seconds each vape buff has left
    bool  buzzing(Vape v) const { return vapeT[v] > 0; }
    float castCd = 0, meleeCd = 0, meleeAnim = 0, roll = 0, rollCd = 0, invuln = 0, hitFlash = 0;
    Vector2 rollDir{ 0, 1 };

    // The chosen champion and their special (Q).
    int   champion = 0;
    float specialCd = 0;
    float auraT = 0, dashT = 0, shieldT = 0, rollAtkT = 0, wobbleT = 0, jumpT = 0;
    float charge = 0;                 // coin power (Daniil & Vasil) or fairy dust (the Fairy)
    int   rollId = 0;
    Vector2 dashDir{ 0, 1 };
};
