// ============================================================================
//  Toilet.h — the hide-and-seek level after Triceps: you hide in the school
//  toilets for a minute while The Sergeant hunts you down by your smoke.
//
//  The map is assets/arenas/toilets.png, and the level's world is that picture,
//  pixel for pixel (1408 x 768). You smoke the whole time and the smoke gives you
//  away; Q (your special) is a 3 s pee instead: no smoke while it lasts, and it
//  leaves a puddle she slips on. Duck into a stall (walk up into its door, or E)
//  and you're out of sight, but your smoke still rises over the door.
// ============================================================================
#pragma once

#include "raylib.h"
#include <vector>

namespace toilet {

    inline constexpr float MAP_W = 1408, MAP_H = 768;
    inline constexpr float HIDE_TIME = 60;          // seconds to survive
    inline constexpr float PEE_TIME = 3;            // seconds a pee lasts
    inline constexpr float PEE_COOLDOWN = 6;        // before you can go again
    inline constexpr float SMOKE_EVERY = 0.16f;     // a puff of smoke this often
    inline constexpr float SMOKE_LIFE = 3.2f;
    inline constexpr float SMELL_R = 190;           // she notices smoke this close to her
    inline constexpr float SIGHT_R = 280;           // and sees you this far, if nothing's in the way
    inline constexpr float CATCH_R = 26;
    inline constexpr float PUDDLE_R = 24;           // a full puddle's radius
    inline constexpr float SLIP_TIME = 1.8f;        // she's down this long
    inline constexpr float SPEED_PATROL = 95, SPEED_SNIFF = 125, SPEED_CHASE = 165;
    inline constexpr float FIGURE_SCALE = 1.5f;     // figures are drawn this much bigger than in the dungeon
    inline constexpr float FIGURE_H = 32 * 1.4f * FIGURE_SCALE;   // so this tall, feet to the top of the head
    inline constexpr float MOUTH_Y = FIGURE_H * 0.69f;            // the cigarette, this far above the feet
    inline constexpr int   CELL = 8;                // the path grid
    inline constexpr int   GW = (int)MAP_W / CELL, GH = (int)MAP_H / CELL;

    // A stall you can hide in: its door (in map pixels) and where you stand in front of it.
    struct Stall { Rectangle door; };
    Vector2 stallFront(const Stall& s);             // on the floor below the door
    Vector2 stallInside(const Stall& s);            // behind the door

    const std::vector<Rectangle>& floors();         // where you can walk (feet), in map pixels
    const std::vector<Stall>& stalls();
    bool walkable(Vector2 p);

    struct Puff { Vector2 pos, vel, from; float t; int stall; };   // `from`: where you were; `stall`: the one you were in, or -1
    struct Puddle { Vector2 pos; float r; float slipCd; };

    enum class Hunt { Patrol, Sniff, Check, Chase, Slip };

    struct Level {
        bool  active = false;
        float timeLeft = HIDE_TIME;
        float endT = 0;                 // after being caught or winning: seconds until what's next
        bool  caught = false, won = false;
        int   tries = 0;

        // you
        Vector2 pos{}, face{ 0, 1 };
        bool  moving = false;
        int   hiddenIn = -1;            // the stall you're in, or -1
        float peeT = 0, peeCd = 0, smokeT = 0;
        std::vector<Puff> smoke;
        std::vector<Puddle> puddles;

        // The Sergeant
        Vector2 sPos{}, sFace{ 0, 1 };
        bool  sMoving = false;
        Hunt  state = Hunt::Patrol;
        Vector2 target{};
        int   waypoint = 0, checkStall = -1;
        float stateT = 0, slipT = 0, sinceSeen = 99, checkCd = 0;
        Hunt  before = Hunt::Patrol;     // what she was doing when she slipped
        Vector2 lastSeen{};
        int   pathTo = -1;              // the cell the path field leads to
        std::vector<int> dist;          // the path field: steps to `pathTo` per cell
    };

} // namespace toilet
