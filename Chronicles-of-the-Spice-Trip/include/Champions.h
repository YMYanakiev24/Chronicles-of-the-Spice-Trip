// ============================================================================
//  Champions.h — who you play as. Every champion has a passive perk and a
//  special ability on Q, taken from their character sheet.
// ============================================================================
#pragma once

#include "raylib.h"

enum class Special {
    ArcaneSurge,   // Spellwoven: refill mana
    Aura,          // Atanas: heal, and slow every enemy and shot
    CoinDash,      // Gakev: a fast, untouchable dash
    MugShield,     // Nikolay: shots shatter on you for a moment
    MelonRoll,     // Pupesh: roll through enemies, hurting each one
    Drink,         // Ristyo: heal, but aim wobbles for a while
    BearJump,      // Iliqn Puh: leap and slam down
    CoinBarrage,   // Daniil & Vasil: charged by hits, a ring of coins
    Koi,           // the Storyteller: three homing koi
    Blink,         // the Fairy: teleport to the cursor, paid in fairy dust
};

struct ChampionDef {
    const char* name;
    const char* art;          // character sheet in assets/ (nullptr = the procedural hero)
    Rectangle   crop;         // the portrait inside the sheet, as fractions of the image
    Color       cloak;        // colour of the hero's cloak in the dungeon
    const char* perk;         // the passive, in one line
    const char* special;      // name of the Q ability
    const char* specialDesc;
    Special     kind;
    float       cooldown;     // seconds between uses of the special

    // Passive numbers.
    float hpBonus = 0;        // extra max health
    float manaCostMul = 1;    // spell costs
    float castCdMul = 1;      // time between casts
    float speedMul = 1;       // walking and running
    int   bladeBonus = 0;     // extra blade damage
    int   extraCoins = 0;     // extra coins per kill
    float stillRegen = 0;     // health per second while standing still
    float spellHoming = 0;    // spells curve toward enemies (rad/s)
};

inline constexpr int CHAMPION_COUNT = 10;
extern const ChampionDef CHAMPIONS[CHAMPION_COUNT];

// Coin Barrage and Blink run on a meter instead of only a cooldown.
inline constexpr float COIN_POWER_MAX = 100;     // filled by hits
inline constexpr float COIN_POWER_PER_HIT = 6;
inline constexpr float DUST_MAX = 50;            // refills over time
inline constexpr float DUST_REGEN = 4;
inline constexpr float DUST_PER_BLINK = 10;
