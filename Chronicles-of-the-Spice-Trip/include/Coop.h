// ============================================================================
//  Coop.h — co-op: up to four players in one run, over the local network.
//
//  One device hosts: it runs the whole game, every hero in it, and sends the
//  others what to draw (CoopSync.cpp) about 60 times a second. A joiner sends
//  what it presses and plays from what it's sent; its shops and menus run on
//  it, and what they buy is done on the host (a Command).
//
//  Everything that belongs to one player is a Hero. The game keeps the hero
//  it's working on in its usual fields (player, overlay, hall, ...), so all
//  the single-player code works for any of them: Game::useHero(i) swaps hero
//  i in. The rest of the Game is the world they share.
// ============================================================================
#pragma once

#include "Castle.h"
#include "Entities.h"
#include "Input.h"
#include "Net.h"
#include <string>

// The screens and windows over the game (Game.h).
enum class Overlay { None, Pause, Controls, Shop, Galin, RunComplete, RushOver, Casino, Summary, Drip, Cutscene };

// Toshko's roulette wheel, in Todor's casino.
struct Wheel {
    bool  casino = false;
    bool  spinning = false, done = false;
    float angle = 0, speed = 0;         // degrees, and degrees a second
    int   result = -1;                  // the wedge under the pointer when it stopped
    int   bet = 10, pick = 0;           // the casino: the stake, and 0 red / 1 black / 2 Toshko's zero
    std::string message;
};

inline constexpr int MAX_HEROES = 4;
inline constexpr int COOP_VERSION = 1;  // joiners with another version are turned away

// One player's own part of the game.
struct Hero {
    Player player;
    Overlay overlay = Overlay::None;    // the shop or window they have open
    castle::Hall hall;                  // in the castle (their own: the others may be fighting)
    Wheel  wheel;                       // at Todor's
    float  castleDoorCd = 0;
    std::string dialogue;               // the line of Vasil's tale they're on
    int    dialogueIdx = 0;
    // their champion's poses and specials in progress
    float  gymT = 0;  Vector2 gymDir{ 1, 0 };
    float  cigT = 0, stubT = 0;
    int    grindStep = 0;  float throwT = 0;  int throwKind = 0;  bool thrown = false;
    float  punchT = 0;  bool punchLanded = false;  Vector2 punchDir{ 1, 0 };
    float  slingT = 0;  bool slingFired = false;
    int    castingSpell = 0;  float castT = 0;
    bool   heroMoving = false;  Vector2 heroMoveDir{ 0, 1 };
    float  shopMsgT = 0;  std::string shopMsg;
    // who they are
    int    outfit = -1;                 // the outfit they wear (-1: as this device's profile says)
    std::string name;                   // shown over their head and in the lobby
    bool   present = false;             // a player is in this seat
    float  downT = 0;                   // knocked out: seconds they've been down
};

// What a joiner presses, sent to the host every frame. Bits of NetInput::held/tapped.
enum NetAct : uint32_t {
    NA_UP = 1u << 0, NA_DOWN = 1u << 1, NA_LEFT = 1u << 2, NA_RIGHT = 1u << 3,
    NA_ROLL = 1u << 4, NA_SPECIAL = 1u << 5, NA_INTERACT = 1u << 6,
    NA_POTION_HP = 1u << 7, NA_POTION_MANA = 1u << 8,
    NA_SPELL1 = 1u << 9, NA_SPELL2 = 1u << 10, NA_SPELL3 = 1u << 11,
    NA_BLADE = 1u << 12, NA_CAST = 1u << 13, NA_CAST_HOLD = 1u << 14, NA_RUN = 1u << 15,
};
struct NetInput {
    uint32_t held = 0, tapped = 0;
    Vector2  aim{};                     // the world point they aim at
};

// What a joiner's shops and windows ask the host to do for their hero.
enum class Cmd : uint8_t {
    CloseOverlay, Buy, BuyWeapon, SellWeapon, BuyUpgrade, Spin, BuyOutfit, Wear,
};

// Messages (net::Message::type).
enum MsgType : uint8_t {
    MSG_HELLO = 1,      // joiner -> host: version, layout, champion, outfit
    MSG_WELCOME,        // host -> joiner: your seat
    MSG_REFUSE,         // host -> joiner: why not
    MSG_LOBBY,          // host -> all: who's in, on which champion, and the difficulty
    MSG_PICK,           // joiner -> host: their champion and outfit
    MSG_START,          // host -> all: the run begins (the world follows in snapshots)
    MSG_INPUT,          // joiner -> host: a NetInput
    MSG_CMD,            // joiner -> host: a Cmd and its arguments
    MSG_SNAPSHOT,       // host -> joiner: the world and every hero
    MSG_DUNGEON,        // host -> joiner: the floor (sent again whenever it changes)
    MSG_BYE,            // either way: leaving
};

enum class CoopRole { Off, Host, Joiner };

// The network side of a co-op game (Coop.cpp).
struct CoopSession {
    CoopRole role = CoopRole::Off;
    bool     started = false;           // in the run (not the lobby)
    int      difficulty = 1;            // the host's choice, for everyone
    std::string status;                 // what's happening, for the lobby
    // hosting
    net::Server server;
    net::Link  links[MAX_HEROES];       // a joiner in each seat (0 is the host)
    bool       welcomed[MAX_HEROES] = {};
    NetInput   inputs[MAX_HEROES];      // the latest of each joiner's input...
    uint32_t   tappedSince[MAX_HEROES] = {};   // ...what they've tapped since the host last used it...
    double     inputAt[MAX_HEROES] = {};       // ...and when it came (a stalled joiner stops walking)
    uint64_t   dungeonSent[MAX_HEROES] = {};   // the floor each joiner has (its hash)
    in::Remote remote;                  // a joiner's input, as keys the host's code reads
    std::vector<std::string> sounds;    // played since the last snapshot: the joiners play them too
    unsigned   cutscenes = 0;           // the Bad Plasmodium's cutscene: a count, so joiners see it begin
    double     lastSent = 0;
    // joining
    net::Finder finder;
    net::Link  host;
    std::string address;                // typed in, to join by
    int        mySeat = 0;
    bool       helloSent = false;
    bool       seated = false;          // the host took us in
    unsigned   cutscenesSeen = 0;
    bool       haveWorld = false;       // a snapshot has come since the run began
    std::vector<uint8_t> snapshot;      // the newest snapshot, waiting to be applied
    bool       snapshotWaiting = false;
    Overlay    closed = Overlay::None;  // a shop we closed: the host may not know yet
    double     closedAt = 0;
    Overlay    lastHostOverlay = Overlay::None;
    std::string mood;                   // the music the world calls for
    bool hosting() const { return role == CoopRole::Host; }
    bool joined() const { return role == CoopRole::Joiner; }
    bool active() const { return role != CoopRole::Off; }
};

// Identifies how this build lays out the plain structs it sends whole; a joiner whose
// build differs is turned away rather than misread (CoopSync.cpp).
uint32_t coopLayout();
