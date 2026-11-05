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
    Slingshot,     // Daniil: charged by hits, a volley of coins from his slingshot at the aim
    Koi,           // the Storyteller: three homing koi
    Blink,         // the Fairy: teleport to the cursor, paid in fairy dust
    Scare,         // Momiche: every enemy runs from you and can't attack
    CreatineCpp,   // the Gym Rat: C++ lasers ahead, a creatine cloud behind
    BlownKiss,     // Vanessa: a fan of hearts that leaves monsters lovestruck
    SmokeBreak,    // Galin: smoke a cig to heal, then stub it out on the nearest monster
    GrinderThrow,  // Filqcheto: grinders, three kinds in turn (shot, meta-grinder, ultra-grinder storm)
    Haymaker,      // Krisa: a dash and a punch that sends monsters flying, dazed
};

struct ChampionDef {
    const char* name;
    const char* art;          // character sheet in assets/ (nullptr = no sheet: the walking figure stands in)
    const char* walk;         // walking figure from every side, in assets/walk/ (see README)
    Rectangle   crop;         // the portrait inside the sheet, as fractions of the image
    Color       cloak;        // colour of the hero's cloak in the dungeon
    const char* perk;         // the passive, in one line
    const char* special;      // name of the Q ability
    const char* specialDesc;
    Special     kind;
    float       cooldown;     // seconds between uses of the special
    bool        fearFace = false;   // the name ends in a fearful face (Momiche😨)

    // Passive numbers.
    float hpBonus = 0;        // extra max health
    float manaCostMul = 1;    // spell costs
    float castCdMul = 1;      // time between casts
    float speedMul = 1;       // walking and running
    int   bladeBonus = 0;     // extra blade damage
    int   extraCoins = 0;     // extra coins per kill
    float stillRegen = 0;     // health per second while standing still
    float spellHoming = 0;    // spells curve toward enemies (rad/s)
    int   killHeal = 0;       // health back for every kill
    float swingMul = 1;       // time between swings of the weapon
    float damageTakenMul = 1; // damage he takes

    const char* quote = "";   // their line on the champion select
    const char* winLine = ""; // what they say when a boss falls
};

inline constexpr int CHAMPION_COUNT = 16;

// Beray's outfits. Every champion can wear each one; their walking figure in it is
// assets/outfits/<key>/<their walk sheet> (made by tools/outfits.py). 0 is their own clothes.
struct OutfitDef { const char* key; const char* name; const char* desc; int price; Color colors[2]; };
inline constexpr int OUTFIT_COUNT = 7;
inline constexpr OutfitDef OUTFITS[OUTFIT_COUNT] = {
    { nullptr,    "Own Clothes",        "What they walked in with.",                        0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
    { "drip",     "Drip Puffer",        "Purple puffer, teal drips, a cap and a chain.",  150, { { 123, 63, 196, 255 }, { 34, 194, 180, 255 } } },
    { "midnight", "Midnight Tracksuit", "Black tracksuit, white stripes, gold chain.",    100, { { 28, 28, 38, 255 }, { 240, 240, 246, 255 } } },
    { "gold",     "Gold Rush",          "Gold jacket, cream trousers, gold chain.",       180, { { 217, 165, 32, 255 }, { 236, 229, 210, 255 } } },
    { "camo",     "Camo Fatigues",      "Borrowed from The Sergeant. Cap included.",      120, { { 94, 106, 60, 255 }, { 110, 90, 58, 255 } } },
    { "pink",     "Pink Velour",        "Soft, pink, and not up for discussion.",          90, { { 224, 69, 154, 255 }, { 255, 159, 208, 255 } } },
    { "ice",      "Ice Cold",           "White and ice blue, a cap and a chain.",         140, { { 210, 236, 250, 255 }, { 95, 183, 239, 255 } } },
};
extern const ChampionDef CHAMPIONS[CHAMPION_COUNT];

// Coin Barrage and Blink run on a meter instead of only a cooldown.
inline constexpr float COIN_POWER_MAX = 100;     // filled by hits
inline constexpr float COIN_POWER_PER_HIT = 6;

// Daniil's slingshot: he draws it, aims, and lets fly a volley of coins at the aim, then lowers it.
inline constexpr int   SLING_COINS = 8;            // coins in the volley
inline constexpr float SLING_SPREAD = 0.05f;       // radians between neighbouring coins
inline constexpr float SLING_SPEED = 430;
inline constexpr int   SLING_DAMAGE = 12;          // each
inline constexpr float SLING_TIME = 0.55f;         // the whole pose...
inline constexpr float SLING_DRAW = 0.25f;         // ...and when in it the coins fly
inline constexpr float DUST_MAX = 50;            // refills over time
inline constexpr float DUST_REGEN = 4;
inline constexpr float DUST_PER_BLINK = 10;

// Momiche's scare: how long everyone runs, and how much faster than they walk.
inline constexpr float SCARE_TIME = 3;
inline constexpr float FLEE_SPEED = 1.3f;

// The Gym Rat's special: arms out, C++ lasers one way and a cloud of creatine the other.
inline constexpr int   CPP_BEAMS = 5;              // lasers in the fan
inline constexpr float CPP_SPREAD = 0.13f;         // radians between neighbouring lasers
inline constexpr float CPP_RANGE = 420;            // how far a laser reaches (walls stop it sooner)
inline constexpr int   CPP_DAMAGE = 22;            // to everything a laser touches (once per cast)
inline constexpr float CREATINE_RANGE = 120;       // the cloud behind you...
inline constexpr float CREATINE_CONE = 0.8f;       // ...this many radians either side
inline constexpr int   CREATINE_DAMAGE = 8;        // and it shoves what it catches away
inline constexpr float CREATINE_SHOVE = 40;        // pixels
inline constexpr float GYM_POSE_TIME = 0.45f;      // the lasers and the pose last this long

// Vanessa's Blown Kiss: a fan of hearts toward the aim, curving after monsters. A heart that
// lands hurts, and a monster it lands on is lovestruck: it stands gazing at her and can't attack.
// Bosses have no heart to lose: they only take the damage.
inline constexpr int   KISS_HEARTS = 5;            // hearts in the fan
inline constexpr float KISS_SPREAD = 0.22f;        // radians between neighbouring hearts
inline constexpr float KISS_SPEED = 260;
inline constexpr int   KISS_DAMAGE = 12;
inline constexpr float CHARM_TIME = 3.5f;          // seconds a monster stays lovestruck

// Galin's Smoke Break: he stands and smokes, healing as he does, then steps to the nearest
// monster in reach and stubs the cig out on its skin. It burns.
inline constexpr float CIG_TIME = 0.9f;            // seconds of smoking (he can't walk meanwhile)
inline constexpr float CIG_HEAL = 30;              // health back over the smoke
inline constexpr float CIG_REACH = 160;            // how far he'll step to stub it out
inline constexpr int   CIG_DAMAGE = 24;
inline constexpr float CIG_STUB_TIME = 0.3f;       // he holds the stub pose this long

// Filqcheto's grinders. Each Q throws the next of three, round and round:
//   0 Weed Grinder Shot: three grinders in a fan
//   1 Meta-Grinder: a cluster that bursts into grinders flying every way when it lands (or soon after)
//   2 Ultra-Grinder Storm: a big grinder in a whirlwind that drifts ahead, drags monsters in and grinds them
inline constexpr const char* GRINDER_NAMES[3] = { "Grinder Shot", "Meta-Grinder", "Grinder Storm" };
inline constexpr float GRINDER_SPEED = 380;
inline constexpr float GRINDER_SPREAD = 0.22f;      // radians between the shot's grinders
inline constexpr int   GRINDER_DAMAGE = 16;
inline constexpr float META_SPEED = 300;
inline constexpr int   META_DAMAGE = 20;            // the cluster itself
inline constexpr int   META_SPLIT = 6;              // grinders it bursts into...
inline constexpr int   META_SPLIT_DAMAGE = 12;      // ...each
inline constexpr float META_FUSE = 0.8f;            // it bursts by itself after this long
inline constexpr float STORM_SPEED = 110;
inline constexpr float STORM_TIME = 3.0f;
inline constexpr float STORM_RADIUS = 40;           // what it grinds...
inline constexpr float STORM_PULL_RADIUS = 140;     // ...and how far off it drags monsters in
inline constexpr float STORM_PULL = 75;             // px per second
inline constexpr int   STORM_DAMAGE = 7;            // every tick
inline constexpr float STORM_TICK = 0.3f;
inline constexpr float THROW_POSE_TIME = 0.4f;      // his wind-up and throw (the storm: arms wide)
inline constexpr float THROW_AT = 0.14f;            // when in it the grinders leave his hands

// Krisa's Haymaker: a quick dash the way you aim, then a punch at the end of it.
inline constexpr float HAYMAKER_DASH = 0.16f;       // seconds of dash (520 px/s)
inline constexpr int   HAYMAKER_DAMAGE = 28;
inline constexpr float HAYMAKER_REACH = 58;
inline constexpr float HAYMAKER_KNOCK = 90;         // px the punch sends them
inline constexpr float DAZE_TIME = 1.5f;            // seconds they stand seeing stars
inline constexpr float PUNCH_POSE_TIME = 0.42f;
