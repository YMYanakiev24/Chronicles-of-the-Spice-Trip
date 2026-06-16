// ============================================================================
//  Rooms.h — a Binding-of-Isaac-style room dungeon.
//  The world is a grid of single-screen rooms connected by doors. Doors stay
//  shut until the room's enemies are cleared, then open to the neighbours.
// ============================================================================
#pragma once

#include "raylib.h"
#include "Common.h"

constexpr int DUNGEON_G = 9;     // dungeon is DUNGEON_G x DUNGEON_G rooms
constexpr int ROOM_TW = 21;     // room width  in tiles (incl. 1-tile walls)
constexpr int ROOM_TH = 11;     // room height in tiles

// Door direction bits.
enum Dir { DIR_N = 1, DIR_E = 2, DIR_S = 4, DIR_W = 8 };

struct Room {
    bool exists = false;
    bool visited = false;
    bool cleared = false;     // enemies defeated -> doors open
    bool isStart = false;
    bool isBoss = false;
    int  doors = 0;           // bitmask of neighbours that exist
};

struct Dungeon {
    Room grid[DUNGEON_G][DUNGEON_G];
    int  cx = DUNGEON_G / 2;     // current room
    int  cy = DUNGEON_G / 2;

    void  generate();
    Room& cur() { return grid[cy][cx]; }
};

// Pixel size of a room (world units).
constexpr float ROOM_PX_W = ROOM_TW * TILE;
constexpr float ROOM_PX_H = ROOM_TH * TILE;

// Is tile (tx,ty) a solid wall for this room? Open door cells are passable
// only when doorsOpen is true.
bool roomWallSolid(const Room& r, int tx, int ty, bool doorsOpen);

// Move a circle within a room, sliding along walls (axis-separated).
Vector2 moveInRoom(Vector2 pos, Vector2 delta, float radius, const Room& r, bool doorsOpen);
