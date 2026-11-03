// ============================================================================
//  Castle.h — the castle behind the door in every floor's first room: a
//  corridor with four doors, each into a shopkeeper's room.
//
//    Marin's Vape Shop      Marin's potions and vapes (Overlay::Shop)
//    Galin's Weapon Shop    Galin's weapons and upgrades (Overlay::Galin)
//    Todor's Casino         Toshko's roulette (Overlay::Casino)
//    Beray's Drip Locker    outfits for every champion (Overlay::Drip)
//
//  Each place is a painted room (assets/castle/), and its world is that picture,
//  pixel for pixel. The tables below say where feet can go, where the doors are
//  and where to stand to talk to whoever keeps the shop.
// ============================================================================
#pragma once

#include "raylib.h"
#include <vector>

namespace castle {

    enum Place { CORRIDOR, MARIN, GALIN, CASINO, DRIP, PLACE_COUNT };

    // A way out of a place: stand within `r` of `at` and press E, or walk on toward `push`.
    struct Exit {
        Vector2 at; float r; Vector2 push;
        int to;                     // a Place, or -1 for back down to the dungeon
        Vector2 arrive, face;       // where you come out, and which way you look
        const char* label;
    };

    struct PlaceDef {
        const char* name;           // shown as you come in
        const char* caption;        // along the bottom of the screen
        const char* art;            // under assets/
        float figure;               // how big figures are drawn (1 = in the dungeon)
        float speed;                // walking speed, likewise
        std::vector<Rectangle> floors, blocks;   // where feet can and can't go
        std::vector<Exit> exits;
        Vector2 keeper{ -1, -1 };   // where to stand to talk to the shopkeeper
        float keeperR = 0;
        const char* keeperLabel = nullptr;
        Rectangle counter{};        // drawn again over the shopkeeper, who stands behind it
    };

    const PlaceDef& place(int p);
    bool walkable(int p, Vector2 feet);

    // From the dungeon you come out here, facing into the corridor.
    inline constexpr Vector2 ENTRY = { 118, 492 };

    struct Hall {
        bool  active = false;
        int   place = CORRIDOR;
        Vector2 pos{}, face{ 1, 0 };
        bool  moving = false;
        float shown = 0;            // seconds in this place (the name fades in)
    };

} // namespace castle
