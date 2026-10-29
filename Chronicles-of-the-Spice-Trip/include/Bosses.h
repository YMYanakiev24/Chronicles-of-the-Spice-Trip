// ============================================================================
//  Bosses.h — the five floor bosses and the rules of a boss fight.
//
//  Floors 1-4 end with the four bosses from the Path (the Good Plazmodium,
//  the Bad Plasmodium, Mordekai and Vlado), each in their own painted arena.
//  Floor 5 ends with Pyraketh, the Cinder-Maw. Deeper floors repeat the cycle
//  with stronger bosses.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Entities.h"
#include "Rooms.h"
#include <functional>
#include <vector>

enum class BossKind {
    Split,       // the Plasmodia: multiply into three copies on death
    Feathers,    // Mordekai: fans of feathers, and a dive every third volley
    Conductor,   // Vlado: ticket spirals, and a whistle that calls bus No.9
    Inferno,     // the Cinder-Maw: chases you, rings of embers, fireballs when wounded
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
};

inline constexpr int BOSS_COUNT = 5;
extern const BossDef BOSSES[BOSS_COUNT];

int   bossIndexForFloor(int floor);     // 1-5 meet each boss in turn, then the cycle repeats
float bossStrength(int floor);          // x1 on floors 1-5, x1.5 on 6-10, ...

// Vlado's bus: a warning lane, then the bus crosses the whole room.
struct Bus {
    float x = 0, y = 0, w = 0, h = 0, warn = 0;
    bool  active = false, driving = false, hit = false;
};

class BossFight {
public:
    const BossDef* def = nullptr;
    float strength = 1;       // health and damage multiplier
    float time = 0;           // seconds since the fight began
    Bus   bus;
    std::function<void(const char*)> sfx;   // sound cues
    bool  scared = false;     // Momiche's scare: the bodies run and every attack waits (a bus already called still drives)

    bool  active() const { return def != nullptr; }
    void  begin(const BossDef& d, float strength, Vector2 at, std::vector<Enemy>& enemies);
    void  end() { def = nullptr; bus = Bus{}; }

    // Moves and fires every boss body. `slow` scales time for the bosses (Atanas' aura).
    void  update(float dt, Vector2 player, std::vector<Enemy>& enemies, std::vector<Projectile>& shots,
                 const Room& room, float slow);
    // A boss body died. The Plasmodia multiply: new bodies go into `spawned`.
    void  onDeath(const Enemy& dead, std::vector<Enemy>& spawned);

    bool  busHits(Vector2 p, float r) const;
    int   busDamage() const;
    float health(const std::vector<Enemy>& enemies) const;    // 0..1 across every body

private:
    float spiralT = 0, spiralBase = 0, whistleT = 0, emberT = 0, aimT = 0;
    float poolMax = 1;

    Enemy body(int form, Vector2 at) const;
    void  roam(Enemy& e, float dt, const Room& room);
    void  shoot(const Enemy& from, float angle, float speed, float r, int damage, ShotKind kind,
                std::vector<Projectile>& shots) const;
    void  updateBus(float dt, Vector2 player);
};

// Round portrait tokens (cut from the character sheets) and arena paintings, loaded once.
struct BossArt {
    Texture2D token[BOSS_COUNT]{};
    Texture2D arena[BOSS_COUNT]{};
    void load();
    void unload();
};

// Drawing helpers for the fight.
void drawBossToken(int index, const BossArt& art, Vector2 centre, float radius, Color tint = WHITE);
void drawBossBody(const Enemy& e, int index, const BossArt& art);
void drawBus(const Bus& bus, float t);
