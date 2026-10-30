// ============================================================================
//  Bosses.h — the six floor bosses and the rules of a boss fight.
//
//  Floors 1-4 end with the four bosses from the Path (the Good Plazmodium,
//  the Bad Plasmodium, Mordekai and Vlado), each in their own painted arena.
//  Floor 5 ends with Triceps in her cave, floor 6 with Pyraketh, the
//  Cinder-Maw. Deeper floors repeat the cycle with stronger bosses.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Entities.h"
#include "Rooms.h"
#include "Sprites.h"
#include <functional>
#include <vector>

enum class BossKind {
    Split,       // the Plasmodia: multiply into three copies on death
    Feathers,    // Mordekai: fans of feathers, and a dive every third volley
    Conductor,   // Vlado: ticket spirals, and a whistle that calls bus No.9
    Inferno,     // the Cinder-Maw: chases you, rings of embers, fireballs when wounded
    Physics,     // Triceps: walks her cave; gravity grenades, magnetic boots, the fulcrum staff, the circuit chain
};

struct BossDef {
    const char* name;
    const char* epithet;      // one line under the name
    const char* sheet;        // character sheet in assets/ (nullptr: the dungeon vampire sprite)
    Rectangle   crop;         // the boss inside its sheet, as fractions of the image
    const char* arena;        // painted arena in assets/arenas/ (nullptr: the stone lair)
    BossKind    kind;
    float hp, radius, speed;
    int   contact;            // touch damage
    Color color;              // shots, health bar
    const char* weakness;     // spell school that deals x1.6
    int   spread;             // shots per volley (Plasmodia: the first form)
    float fireInterval;
    const char* walk = nullptr;    // walks as a figure from every side, like the champions (assets/walk/)
    const char* poses = nullptr;   // her attack poses beside the walk sheet: a strip of four, facing right
};

inline constexpr int BOSS_COUNT = 6;
extern const BossDef BOSSES[BOSS_COUNT];

int   bossIndexForFloor(int floor);     // 1-6 meet each boss in turn, then the cycle repeats
float bossStrength(int floor);          // x1 on floors 1-6, x1.5 on 7-12, ...

// Vlado's bus: a warning lane, then the bus crosses the whole room.
struct Bus {
    float x = 0, y = 0, w = 0, h = 0, warn = 0;
    bool  active = false, driving = false, hit = false;
};

// Triceps' gravity grenade: flies a parabola to where you stood, opens a well
// that pulls you in (F = ma), then goes off.
struct Grenade {
    Vector2 from{}, to{};
    float t = 0, flight = 1;       // seconds in the air / the whole flight
    float well = 0, boom = 0;      // seconds the well stays open / the blast lasts
    bool  active = false, hit = false;
    float peak = 0;                // highest point of the arc
    Vector2 at() const;            // over the ground
    float height() const;          // above the ground
};

// Her circuit chain: capacitor nodes charge, then current arcs along every wire.
struct Circuit {
    static constexpr int MAX_NODES = 6;
    Vector2 node[MAX_NODES]{};
    int   count = 0;
    float charge = 0, live = 0, shockCd = 0;   // seconds until live / live left / until the next shock
    bool  active = false;
};

// The fulcrum staff: wound up toward you, then swung round her like a lever.
struct Staff {
    Vector2 at{};
    float from = 0, sweep = 0;     // start angle and signed sweep (radians)
    float t = 0;                   // seconds into the swing
    bool  active = false, swinging = false, hit = false;
    float angle() const;
};

class BossFight {
public:
    const BossDef* def = nullptr;
    float strength = 1;       // health and damage multiplier
    float time = 0;           // seconds since the fight began
    Bus   bus;
    Grenade grenades[2];
    Circuit circuit;
    Staff   staff;
    const char* formula = nullptr;   // the physics behind Triceps' last attack, shown over it
    Vector2 formulaAt{};
    float   formulaT = 0;
    std::function<void(const char*)> sfx;   // sound cues
    bool  scared = false;     // Momiche's scare: the bodies run and every attack waits (a bus already called still drives)

    bool  active() const { return def != nullptr; }
    void  begin(const BossDef& d, float strength, Vector2 at, std::vector<Enemy>& enemies);
    void  end();

    // Moves and fires every boss body. `slow` scales time for the bosses (Atanas' aura).
    void  update(float dt, Vector2 player, std::vector<Enemy>& enemies, std::vector<Projectile>& shots,
                 const Room& room, float slow);
    // A boss body died. The Plasmodia multiply: new bodies go into `spawned`.
    void  onDeath(const Enemy& dead, std::vector<Enemy>& spawned);

    bool  busHits(Vector2 p, float r) const;
    int   busDamage() const;
    // Triceps: how fast her open wells drag a body at `p` (px/s), and the damage a blast,
    // a live wire or the staff deals to a body touching it this frame (0: none; it counts as landed).
    Vector2 gravityPull(Vector2 p) const;
    int   hazardHit(Vector2 p, float r);
    float health(const std::vector<Enemy>& enemies) const;    // 0..1 across every body

private:
    float spiralT = 0, spiralBase = 0, whistleT = 0, emberT = 0, aimT = 0;
    float poolMax = 1;
    // Triceps walks, winds up, lets go, recovers; her four attacks take turns
    enum class Lab { Walk, Chase, WindUp, Recover };
    Lab   lab = Lab::Walk;
    float labT = 0;
    int   attack = 0;

    Enemy body(int form, Vector2 at) const;
    void  roam(Enemy& e, float dt, const Room& room);
    void  shoot(const Enemy& from, float angle, float speed, float r, int damage, ShotKind kind,
                std::vector<Projectile>& shots) const;
    void  shootAt(Vector2 at, float angle, float speed, float r, int damage, ShotKind kind,
                  std::vector<Projectile>& shots) const;
    void  updateBus(float dt, Vector2 player);
    void  updateLab(Enemy& e, float dt, Vector2 player, std::vector<Projectile>& shots, const Room& room);
    void  windUp(Enemy& e, Vector2 player);
    void  release(Enemy& e, Vector2 player, std::vector<Projectile>& shots);
    void  updateHazards(float dt, Vector2 player, std::vector<Projectile>& shots);
    void  showFormula(const char* f, Vector2 at) { formula = f; formulaAt = at; formulaT = 0; }
};

// Round portrait tokens (cut from the character sheets) and arena paintings, loaded once.
// A boss with a walk sheet (Triceps) walks as her own figure instead of a token.
struct BossArt {
    Texture2D token[BOSS_COUNT]{};
    Texture2D arena[BOSS_COUNT]{};
    WalkSheet walk[BOSS_COUNT];
    Texture2D poses[BOSS_COUNT]{};
    void load();
    void unload();
};

// Drawing helpers for the fight.
void drawBossToken(int index, const BossArt& art, Vector2 centre, float radius, Color tint = WHITE);
void drawBossBody(const Enemy& e, int index, const BossArt& art);
float bossTop(const Enemy& e, int index, const BossArt& art);   // the top of the drawn body
void drawBus(const Bus& bus, float t);
// Triceps' hazards: wells, wires and the staff's reach on the floor; then grenades in
// the air, the swinging staff and the formula over her attack.
void drawLabFloor(const BossFight& f, float t);
void drawLabAir(const BossFight& f, float t);
