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

// Interior tile kinds for a room's layout.
// RT_VOID is solid rock outside a shaped room's outline; RT_FLAMER is a solid
// fire-thrower nozzle that periodically breathes flame down a lane.
enum RoomTile : unsigned char { RT_FLOOR = 0, RT_WALL = 1, RT_SPIKES = 2, RT_VOID = 3, RT_FLAMER = 4 };
inline bool tileSolid(unsigned char t) { return t == RT_WALL || t == RT_VOID || t == RT_FLAMER; }

// A fire-thrower trap: nozzle tile, the direction it fires and its timing offset.
struct Flamer { int tx = 0, ty = 0, dir = 0; float phase = 0; };
constexpr int MAX_FLAMERS = 3;
constexpr float FLAMER_CYCLE = 3.2f;   // seconds per cycle
constexpr float FLAMER_WARN = 0.6f;    // sputtering warning before the blast
constexpr float FLAMER_BURN = 1.1f;    // how long the flame jet lasts
constexpr int   FLAMER_RANGE = 4;      // flame reach in tiles

// Door direction bits.
enum Dir { DIR_N = 1, DIR_E = 2, DIR_S = 4, DIR_W = 8 };

struct Room {
    bool exists = false;
    bool visited = false;
    bool cleared = false;     // enemies defeated -> doors open
    bool isStart = false;
    bool isBoss = false;
    bool isShop = false;      // Marin's market: no enemies, no clutter
    bool hasKey = false;      // the boss key lies here once the room is cleared
    int  doors = 0;           // bitmask of neighbours that exist
    unsigned char tiles[ROOM_TH][ROOM_TW] = {};   // interior layout (RoomTile)
    bool hasChest = false, chestOpened = false;
    int  chestTx = 0, chestTy = 0;
    Flamer flamers[MAX_FLAMERS];
    int  flamerCount = 0;

    void buildLayout();       // walls, traps and maybe a chest
    bool isFloor(int tx, int ty) const;   // walkable, trap-free interior tile
};

struct Dungeon {
    Room grid[DUNGEON_G][DUNGEON_G];
    int  cx = DUNGEON_G / 2;     // current room
    int  cy = DUNGEON_G / 2;
    bool keyFound = false;       // the boss lair stays locked until this floor's key is found

    void  generate();          // a fresh floor: start room, combat rooms, Marin, the boss
    void  generateArena();     // a single sealed boss room (Boss Rush)
    Room& cur() { return grid[cy][cx]; }
    const Room& cur() const { return grid[cy][cx]; }
    // Does the door on side `dir` of the current room lead into the still-locked lair?
    bool bossDoorLocked(int dir) const;
};

// Pixel size of a room (world units).
constexpr float ROOM_PX_W = ROOM_TW * TILE;
constexpr float ROOM_PX_H = ROOM_TH * TILE;

// Is tile (tx,ty) a solid wall for this room? Open door cells are passable
// only when doorsOpen is true.
bool roomWallSolid(const Room& r, int tx, int ty, bool doorsOpen);

// Is world point p standing on a spike trap tile?
bool onSpikes(const Room& r, Vector2 p);

// Fire-thrower state at time t: 0 idle, 1 warning, 2 burning.
int flamerState(const Flamer& f, float t);
// The tiles a flamer's jet covers (stops at the first solid tile); returns the count.
int flameTiles(const Room& r, const Flamer& f, int outX[], int outY[]);
// Is world point p inside a burning flame jet?
bool inFlames(const Room& r, Vector2 p, float t);

// Path distances (in tile steps) from every tile to the target tile. Walls
// are -1; raised-spike tiles cost extra so chasers route around traps.
using FlowField = int[ROOM_TH][ROOM_TW];
void buildFlowField(const Room& r, Vector2 target, bool avoidSpikes, FlowField& out);

// Can a circle of radius r slide straight from a to b without touching a wall?
bool lineClear(const Room& r, Vector2 a, Vector2 b, float radius);

// Where a chaser at `from` should head next to reach the flow field's target.
// A body of `radius` that can't reach that step in a straight line (it would clip
// a wall corner) is sent back to the centre of its own tile first.
Vector2 nextWaypoint(const Room& r, const FlowField& f, Vector2 from, float radius = 0);

// Move a circle within a room, sliding along walls (axis-separated).
Vector2 moveInRoom(Vector2 pos, Vector2 delta, float radius, const Room& r, bool doorsOpen);
