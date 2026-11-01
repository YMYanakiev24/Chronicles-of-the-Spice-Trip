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
    float charge;            // seconds of wind-up before it leaves the hand
};

inline constexpr int SPELL_COUNT = 3;
enum SpellId { SP_FIRE = 0, SP_ICE, SP_BOLT };
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
    Boot,       // Triceps' magnetic boots, curving in their field
    Ball,       // the iron balls her staff bats at you
    Arrow,      // a Soldier's arrow
    Rock,       // the boulders the Bad Plasmodium throws
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
    float    curve = 0;           // magnetic boots: the velocity turns this fast (rad/s), F = q(v x B)
    int      spell = -1;          // the player's god-powers: which SpellId
    float    travelled = 0;       // distance flown...
    float    nextSpike = 0;       // ...where Tidelash raises its next spikes, or Stormbolt jags...
    int      trail = 0;           // ...which trail those spikes belong to...
    Vector2  lastJag{};           // ...and where Stormbolt's trail has reached
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
    Vector2 knock{};                   // knocked back: this velocity, dying away
    float   burnT = 0;                 // seconds left on fire (looks only: burning does no damage)
    float   lunge = 0, lungeCd = 0;    // hounds and demons: short burst of speed at the player
    bool    swinging = false;          // mid-attack, the blow or shot still to come
    float   flankSide = 1;             // which side this enemy circles toward (+1/-1)
    int     kind = 0;                  // EnemyKind (Sprites.h)
    float   animT = 0;                 // running clock for idle/move loops
    float   atkAnim = 0;               // >0 while the attack animation plays
    float   deathT = 0;                // time since death (death animation)
    bool    dying = false, flip = false, moving = false;
    int     lastRollHit = -1;          // Pupesh's roll hits each enemy once per roll
    int     stolen = 0;                // Toshko: gold he snatched and is running off with
    float   fleeT = 0;                 // Toshko: seconds spent running away

    // Boss bodies (bossForm >= 0) are moved and fired by the BossFight.
    int     bossForm = -1;             // 0 = the boss, 1 = a copy, 2 = a tiny copy
    float   scale = 1;                 // drawn size relative to the boss
    Vector2 roamTo{};
    float   moveCd = 0, shootCd = 1.2f, fireInterval = 1.1f;
    int     spread = 3, volleys = 0;
    float   diveT = 0;                 // Mordekai's dive
    Vector2 diveDir{};
    bool    chaser = false;            // tiny copies just chase
    Vector2 look{ 0, 1 };              // Triceps: the way she faces as she walks
    int     pose = -1;                 // Triceps: the attack pose she strikes (-1: walking)
};

struct Particle {
    Vector2 pos, vel; float life, maxLife, size; Color color;
    float gravity = 0;
    bool  shrink = false, star = false;
};

// A bright flash where something landed: a four-pointed glint, or an expanding ring.
struct Flash { Vector2 pos; float t, life, size, ang; Color color; bool ring; };

// One stroke of Stormbolt's lightning. `seed` picks how it flickers.
struct Arc { Vector2 a, b; float t, life, width, seed; };

// Floor cracked by Tidelash. `seed` picks its pattern.
struct FloorCrack { Vector2 pos; float t, size, seed; Color glow; };
inline constexpr float CRACK_TIME = 1.8f;

// A piece of a beaten boss. It holds its place until `delay`, then falls to `ground`.
struct Rubble { Texture2D tex; Rectangle src; Vector2 pos, size, vel; float delay, t, rot, spin, ground; };

// An ice spike of Tidelash's trail: from `pos` on the floor up to pos + rise. A trail stands
// until its spell ends, then breaks spike by spike (breakAt, on the spike's own clock `t`).
struct IceSpike { Vector2 pos, rise; float width, t, breakAt; int trail, seq; };

// Vapes Marin sells: permanent, one of each (bits in Player::vapes). Also the frame in SPR.vapes.
enum Vape {
    VP_MANGO = 0,   // move 10% faster
    VP_RAZZ,        // powers cost 20% less mana
    VP_MELON,       // +4 blade damage
    VP_MINT,        // take 15% less damage
    VP_COUNT
};

// Weapons Galin sells (the table is in Game.h). Each one keeps its own upgrades.
inline constexpr int WEAPON_COUNT = 5;

// A weapon's upgrades at Galin's stall. Each level takes a cigarette pack.
enum Upgrade {
    UP_SHARPEN = 0, // +3 blade damage
    UP_BALANCE,     // swings 8% faster
    UP_EXTEND,      // +4 reach
    UPGRADE_COUNT
};
inline constexpr int UPGRADE_MAX = 3;

struct Player {
    Vector2 pos{};
    Vector2 facing{ 0, 1 };
    int   level = 1, xp = 0, xpNext = 60;
    float maxHp = 100, hp = 100, maxMana = 100, mana = 100;
    int   selected = 0;
    int   gold = 0;
    int   packs = 0;                  // cigarette packs: Gakev drops them, Galin takes them
    unsigned vapes = 0;               // a bit per Vape smoked
    bool  has(Vape v) const { return (vapes >> v) & 1u; }
    int   weapon = 0;                 // index into WEAPONS, in hand
    unsigned weaponsOwned = 1;        // a bit per weapon: everyone starts with the Rusty Sword
    bool  owns(int w) const { return (weaponsOwned >> w) & 1u; }
    int   upgrades[WEAPON_COUNT][UPGRADE_COUNT] = {};   // levels, per weapon
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
