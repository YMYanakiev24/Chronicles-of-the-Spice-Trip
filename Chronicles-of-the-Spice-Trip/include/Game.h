// ============================================================================
//  Game.h — the whole game state and the frame loop.
//
//  Game.cpp       lifetime, runs, rooms, floors, records
//  Play.cpp       the dungeon: movement, combat, champions, bosses, loot
//  PlayDraw.cpp   the dungeon on screen: room, actors, HUD, overlays
//  Screens.cpp    menu, champion select, settings, intro
//  Autopilot.cpp  a scripted play-through that checks every system
// ============================================================================
#pragma once

#include "Common.h"
#include "Audio.h"
#include "Bosses.h"
#include "Champions.h"
#include "Entities.h"
#include "Rooms.h"
#include "Save.h"
#include "Settings.h"
#include "Sprites.h"
#include "Video.h"
#include "Toilet.h"
#include "Castle.h"
#include <map>
#include <string>
#include <vector>

enum class Screen { Video, Menu, Setup, Champions, Settings, Controls, Intro, Play };
enum class Mode { Run, BossRush };
enum class Overlay { None, Pause, Controls, Shop, Galin, RunComplete, RushOver, Casino, Summary, Drip, Cutscene };

struct Popup { const Anim* anim; Vector2 pos; float t; };        // a flask rising from a chest
struct Coin { Vector2 pos, vel; float t; bool pack = false; };    // loot that flies to you (gold, or a cigarette pack)
struct FloatText { Vector2 pos; float t; int value; Color color; };  // damage numbers
struct Laser { Vector2 a, b; float t; };                         // one of the Gym Rat's C++ lasers

// Vasil the Storyteller (assets/vasil/). He sits on his stool in the first room of every
// floor, with his sign and a few listeners, and stands up to tell that floor's tale.
struct VasilArt {
    static constexpr int CROWD = 10;    // listeners in crowd.png, side by side
    Texture2D seated{}, story[2]{}, sign{}, stool{}, portrait{}, crowd{};
};

// Toshko's roulette wheel, in Todor's casino.
struct Wheel {
    bool  casino = false;
    bool  spinning = false, done = false;
    float angle = 0, speed = 0;         // degrees, and degrees a second
    int   result = -1;                  // the wedge under the pointer when it stopped
    int   bet = 10, pick = 0;           // the casino: the stake, and 0 red / 1 black / 2 Toshko's zero
    std::string message;
};

// Share-friendly borderless fullscreen (see Game.cpp).
bool isFullscreen();
void setFullscreen(bool on);

struct Game {
    // ---------------------------------------------------------------- lifetime
    void init(bool windowed);
    void shutdown();
    void frame();                       // update + draw one frame
    bool quit = false;

    // -------------------------------------------------------------------- runs
    void startRun();
    void startBossRush();
    bool continueRun();
    bool saveRun();
    void toMenu();
    void enterRoom();
    void descend();
    void bossDefeated();
    const BossDef& currentBoss() const;
    int   currentBossIndex() const;
    float currentStrength() const;
    const ChampionDef& champ() const { return CHAMPIONS[player.champion]; }
    void  showBanner(const std::string& text, const std::string& sub = "", float seconds = 2.2f);
    void  saveProfile();

    // ------------------------------------------------------------------ screens
    void videoScreen();                 // Marin's video, once as the game starts
    void menuScreen();
    void championsScreen();
    void settingsScreen();
    void setupScreen();                 // a new run: difficulty and hardcore
    void controlsScreen();
    void introScreen();
    void playScreen();

    // --------------------------------------------------------------- play logic
    void updatePlay(float dt);
    void movePlayer(float dt, Vector2 mv);
    void useSpecial(Vector2 mv);
    void castSpell();
    void releaseSpell(int spell);       // castSpell winds up; this lets it fly
    void swingBlade();
    void updateShots(float dt);
    void plantSpikes(const Projectile& p, Vector2 at);
    void zapFork(Vector2 from, float ang, float len);   // a short crooked branch of lightning
    void spellEnded(const Projectile& p, const Enemy* hit);
    void slamChest();
    void updateEffects(float dt);
    void updateEnemies(float dt, float slow);
    void updateEnemyShots(float dt, float slow);
    void updateLoot(float dt);
    void hitEnemy(Enemy& e, int damage, Color fx);
    void stubOut();                     // Galin's cig, on the nearest monster's skin
    void hurtPlayer(float damage, float invuln);
    void killEnemy(Enemy& e);
    void finalBlow(const Enemy& e);     // the last of the boss: freeze, slow down, fall apart
    void enemyShoot(const Enemy& e);    // an arrow, a magic orb or a gob of blood at the player
    void checkRoomExits();
    Enemy& spawnEnemy(const char* name, float hp, float spd, int dmg, int xp, float rad, Color col,
        const char* weak, int kind, float minFromPlayer = 0);
    void spawnMonster(int type, float tough, float minFromPlayer = 0);   // a MonsterType
    void spawnElite();
    void updateChallenge(float dt);
    void usePotion(int which);          // 0 health, 1 mana
    void openMimic();
    void playerDied();
    void autosave();
    void hapticBuzz(int ms);
    void spinWheel();
    void updateWheel(float dt);
    void wheelLanded();
    int   theme() const;                // THEMES index for this floor
    float diffHp() const;               // the difficulty's monster health...
    float diffDmg() const;              // ...and damage
    void buy(int item);
    void buyWeapon(int weapon);         // buys it, or just equips it if it's owned
    void buyUpgrade(int upgrade);       // for the weapon in hand
    Camera2D camera() const;
    // the toilets after Triceps (Toilet.cpp)
    void startToilet();
    void updateToilet(float dt);
    void drawToilet();
    void drawToiletHud();
    Camera2D toiletCamera() const;
    // the castle behind the door in every first room (Castle.cpp)
    void enterCastle();
    void leaveCastle();
    void updateCastle(float dt);
    void openKeeper(int place);
    void drawCastle();
    void drawKeeper(int place);
    void drawCastleHud();
    Camera2D castleCamera() const;
    // Beray's outfits
    void drawDrip();
    const WalkSheet& figure(int champion) const;          // their walking figure, in the outfit they wear
    const WalkSheet& outfitLook(int champion, int outfit); // ...or in any outfit (loaded when first asked for)
    void wearOutfit(int champion, int outfit);
    bool ownsOutfit(int champion, int outfit) const;
    void buyOutfit(int champion, int outfit);              // or put it on, if it's already theirs
    Vector2 mouseWorld() const;
    Vector2 aimDir() const;
    Vector2 castHand() const;           // where a spell gathers as it winds up
    void  creatineAndCpp(Vector2 dir);  // the Gym Rat's special
    void  slingCoins();                 // Daniil's volley, as the slingshot lets go
    std::vector<std::string> vasilTale() const;   // this floor's story, line by line
    std::string vasilGreeting() const;            // what he calls out as you arrive
    float hurtMul() const;
    int   spellCost(int spell) const;
    int   bladeDamage() const;          // the weapon, its upgrades, level, vape and perk
    float swingTime() const;
    float bladeReach() const;

    // ---------------------------------------------------------------- drawing
    void drawRoom();
    void drawActors();
    void drawHud();
    void drawMinimap();
    void drawBossBar();
    void drawShop();
    void drawGalin();
    void drawPause();
    void drawControlsEditor();
    void drawRunComplete();
    void drawRushOver();
    void drawWheel(Vector2 c, float radius);
    void drawCasino();
    void drawSummary();
    void drawCutscene();                // a video over the game, e.g. after the Bad Plasmodium
    void drawMenuBackground();
    void drawVasil();
    void drawFear(float x, float above, float below, float size) const;   // over a scared enemy
    void drawName(int champion, float x, float y, int size, Color color, bool centred = false) const;
    void drawWeapon(int weapon, Rectangle box) const;
    Rectangle castleRect() const;

    void play(const char* sfx);

    // ===================================================================== state
    Screen  screen = Screen::Menu;
    Mode    mode = Mode::Run;
    Overlay overlay = Overlay::None;
    Settings settings;
    Profile profile;
    bool  hasSave = false;
    float t = 0;                        // seconds on the current screen
    float clock = 0;                    // seconds since start (animations)
    int   rebinding = -1;               // settings: key being rebound

    // the dungeon
    Player player;
    Dungeon dungeon;
    std::vector<Enemy> enemies, corpses, spawned;
    std::vector<Projectile> shots, enemyShots;
    std::vector<Particle> parts;
    std::vector<Flash> flashes;
    std::vector<Arc> arcs;
    std::vector<FloorCrack> cracks;
    std::vector<IceSpike> spikes;       // kept sorted by y, so nearer ones stand in front
    std::vector<Rubble> rubble;
    std::vector<Laser> lasers;          // the Gym Rat's C++, fading
    toilet::Level hunt;                 // the toilets after Triceps: hiding from The Sergeant
    castle::Hall hall;                  // the castle corridor and its shops
    float castleDoorCd = 0;             // just came out: the door waits a moment before it opens again
    int   dripChampion = 0, dripHover = -1;   // Beray's: who you're dressing, the outfit pointed at
    float gymT = 0;                     // the Gym Rat holds his arms-out pose this long...
    Vector2 gymDir{ 1, 0 };             // ...lasers this way, creatine the other
    float cigT = 0, stubT = 0;          // Galin: seconds of smoke left, then of the stub pose
    float slingT = 0;                   // Daniil: seconds left of his slingshot pose...
    bool  slingFired = false;           // ...and whether the coins have flown yet
    float finaleT = 0, slowMo = 0;      // a boss is falling: seconds left of it, and of the slow motion
    float rewardT = 0;                  // BOSS DEFEATED gets its moment before this overlay
    Overlay rewardOverlay = Overlay::None;
    int   iceTrails = 0;
    int   castingSpell = 0;             // the spell winding up...
    float castT = 0;                    // ...and the seconds it has left to go
    float hitStop = 0;                  // the world freezes a moment on a solid hit
    float skyFlash = 0;                 // the room lights up when lightning strikes
    std::vector<Popup> popups;
    std::vector<Coin> coins;
    std::vector<FloatText> floaters;
    BossFight fight;
    float scareT = 0;                   // Momiche's scare: seconds everyone keeps running
    bool  heroMoving = false;           // walking this frame (the figure faces heroMoveDir)
    Vector2 heroMoveDir{ 0, 1 };
    int   floor = 1, kills = 0;
    float runTime = 0;
    bool  inFight = false;
    float chestOpenT = 99, shake = 0, gloom = 0, trapClock = 0;
    float chestLight = 0;               // the glow out of an open chest, 1 at full
    bool  chestLooting = false;         // its loot is still in the air: the glow stays
    bool  chestMana = false;            // what the chest held, for when the lid goes up
    bool  chestPack = false;            // and whether a cigarette pack was in there too
    int   difficulty = 1;               // DIFFICULTIES
    bool  hardcore = false;             // one life: dying ends the run
    RunStats stats;
    float challengeT = 0, challengeWaveT = 0;   // a challenge room: seconds left, and to the next wave
    float autosaveT = 0;
    bool  testMode = false;             // the autopilot: no mimics
    Wheel wheel;
    bool  summaryWon = false;           // the summary: a finished run, or a hardcore death
    float banner = 0;
    std::string bannerText, bannerSub;
    std::string dialogue;               // Vasil's tale, the line he's on
    int   dialogueIdx = 0;
    float vasilGreetT = 0;              // seconds left of his call as a floor begins
    float vasilBowT = 0;                // the bow at the end of a tale
    float shopMsgT = 0, saveMsgT = 0;
    std::string shopMsg, saveMsg;

    // Boss Rush
    int   rushStage = 0;
    float rushTime = 0, rushNextT = -1;
    bool  rushWon = false, rushRecord = false;

    // the autopilot asks for screenshots by name
    std::string pendingShot;

    // ------------------------------------------------------------------ assets
    std::map<std::string, Sound> sounds;
    au::Music music;
    Texture2D menuBg{}, castleTex{}, marinTex{}, shopTex{};
    Texture2D galinTex{}, packTex{};
    Texture2D weaponTex[WEAPON_COUNT]{};
    Texture2D champTex[CHAMPION_COUNT]{};
    WalkSheet walk[CHAMPION_COUNT];     // each champion from every side (assets/walk/)
    Anim roll[CHAMPION_COUNT];          // their dodge-roll, 60 x 80 frames facing right (assets/dungeon/roll-<champion>.png)
    Texture2D fearTex{};                // the fearful face (assets/walk/fear.png)
    VasilArt vasil;
    VideoPlayer introVideo;             // assets/intro/marin-trip.mpg
    VideoPlayer cutscene;               // the one playing in Overlay::Cutscene
    Texture2D toiletMap{}, sergeantTex{};   // assets/arenas/toilets.png, assets/sergeant.png
    WalkSheet sergeantWalk;             // assets/walk/walk-sergeant.png
    Texture2D castleArt[castle::PLACE_COUNT]{}, berayTex{};   // assets/castle/
    WalkSheet dressed[CHAMPION_COUNT];  // each champion in the outfit they wear
    std::map<int, WalkSheet> outfitLooks;   // any champion in any outfit, for Beray's mirror
    BossArt bossArt;
};

inline constexpr const char* INTRO_VIDEO = "intro/marin-trip.mpg";   // plays once as the game starts
inline constexpr const char* BAD_PLASMODIUM_CUTSCENE = "cutscenes/red-eyes.mpg";   // plays when the Bad Plasmodium falls (in a run)

// Fixed spots in the start and shop rooms.
// The door to the castle, in the top wall of every floor's first room, and where you come back out.
inline constexpr float   CASTLE_DOOR_X = 13 * TILE;
inline constexpr Vector2 CASTLE_DOOR_FRONT = { CASTLE_DOOR_X, TILE * 1.0f + 26 };
inline constexpr Vector2 VASIL_POS = { ROOM_PX_W / 2 - 110, ROOM_PX_H / 2 - 50 };   // the foot of his stool
inline constexpr Vector2 ROOM_CENTER = { ROOM_PX_W / 2, ROOM_PX_H / 2 };
inline constexpr Vector2 SILVER_POS = { ROOM_PX_W / 2 + 64, ROOM_PX_H / 2 };   // the silver key, beside the gold one

// Difficulty: how much health monsters (and bosses) have, and how hard they hit.
struct DifficultyDef { const char* name; const char* desc; float hp, dmg; };
inline constexpr DifficultyDef DIFFICULTIES[3] = {
    { "Easy",   "Monsters have 30% less health and hit 40% softer.", 0.7f, 0.6f },
    { "Normal", "The Spice Trip as it was meant to be.",             1.0f, 1.0f },
    { "Hard",   "Monsters have 35% more health and hit 40% harder.", 1.35f, 1.4f },
};

// Each floor of a cycle has its own look: a tint over the stone and a fog colour.
struct FloorTheme { const char* name; Color tint, fog; };
inline constexpr FloorTheme THEMES[6] = {
    { "The Mistwood Depths", { 255, 255, 255, 255 }, { 122, 154, 138, 255 } },
    { "The Drowned Crypt",   { 175, 205, 255, 255 }, {  80, 120, 190, 255 } },
    { "The Ember Halls",     { 255, 190, 155, 255 }, { 200,  90,  50, 255 } },
    { "The Bone Gardens",    { 240, 230, 205, 255 }, { 200, 190, 150, 255 } },
    { "The Toxic Sewers",    { 190, 255, 175, 255 }, {  90, 170,  70, 255 } },
    { "The Gilded Vault",    { 255, 228, 160, 255 }, { 220, 180,  80, 255 } },
};

// The monsters a room can hold (spawnMonster).
enum MonsterType { MT_MISTLING, MT_HOUND, MT_WISP, MT_GAKEV, MT_TOSHKO, MT_MELON, MT_SOLDIER, MT_BLOOD, MT_ORC, MT_DEMON };

// One combat room a floor holds a mini-boss: a named, bigger, tougher monster.
struct EliteDef { const char* name; int type; };
inline constexpr EliteDef ELITES[5] = {
    { "Orc Warlord", MT_ORC }, { "Gakev the Elder", MT_GAKEV }, { "Captain of the Guard", MT_SOLDIER },
    { "Mega Melon", MT_MELON }, { "Alpha Hound", MT_HOUND },
};

// Toshko's casino: his green zero, then red and black. Colours pay double, the zero fourteen times.
inline constexpr int CASINO_SLOTS = 15;
inline constexpr int CASINO_BETS[4] = { 10, 25, 50, 100 };

inline constexpr float CHALLENGE_TIME = 30, CHALLENGE_WAVE = 7;   // a challenge room: how long, and how often a wave comes
inline constexpr int   MIMIC_PERCENT = 18;    // the chance a chest is a Mimic
inline constexpr int   POTION_MAX = 3;        // potions of each kind the belt holds

// What Marin sells. Potions (vape == -1) can be bought again and again; a vape
// is a permanent buff, one of each.
struct ShopItem { const char* name; const char* desc; int price; int vape; };
inline constexpr ShopItem SHOP_ITEMS[] = {
    { "Health Potion",   "Into your belt: heals 35 HP",           8, -1 },
    { "Mana Potion",     "Into your belt: restores 50 mana",      6, -1 },
    { "Mango Haze",      "Vape: move 10% faster, for good",      30, VP_MANGO },
    { "Blue Razz Ice",   "Vape: powers cost 20% less, for good", 35, VP_RAZZ },
    { "Watermelon Rage", "Vape: +4 blade damage, for good",      30, VP_MELON },
    { "Cool Mint Fog",   "Vape: take 15% less damage, for good", 40, VP_MINT },
    { "Grape Ice",       "Vape: mana refills 50% faster, for good", 30, VP_GRAPE },
    { "Peach Rings",     "Vape: 1 coin in 4 counts double, for good", 35, VP_PEACH },
};
inline constexpr int SHOP_COUNT = (int)(sizeof SHOP_ITEMS / sizeof SHOP_ITEMS[0]);

// The colour of each vape's vapour puffs.
inline constexpr Color VAPE_COLOR[VP_COUNT] = {
    { 255, 176, 64, 255 }, { 96, 160, 255, 255 }, { 240, 80, 96, 255 }, { 170, 250, 220, 255 },
    { 190, 120, 255, 255 }, { 255, 150, 110, 255 } };

// What Galin sells.
struct WeaponDef {
    const char* name;
    const char* art;        // in assets/: transparent or on a flat magenta backdrop, hilt to the lower left
    float slant;            // degrees the art rises from hilt to tip
    int   price;            // gold
    int   damage;           // added to every swing
    float swing;            // seconds between swings
    float reach;            // how far a swing lands
    float knockback;        // how far a hit shoves an enemy back
    const char* sfx;        // the sound of a hit landing
    bool  anal = false;     // its damage goes by another name
};
inline constexpr WeaponDef WEAPONS[WEAPON_COUNT] = {
    { "Rusty Sword",  "rusty-iron-sword/sprite-90x22.png",  0,   0,  0, 0.40f, 40,  8, "clang" },
    { "Iron Sword",   "iron-sword/sprite-128.png",         45,  50,  5, 0.40f, 40, 10, "clang" },
    { "Police Baton", "police-baton/sprite-128.png",       45,  90,  3, 0.30f, 36,  6, "bonk" },
    { "Baseball Bat", "baseball-bat/sprite-128.png",       45, 130, 12, 0.55f, 48, 28, "bonk" },
    { "Pink Sword",   "pink-sword/sprite-128.png",         45, 190, 13, 0.40f, 50, 14, "clang", true },
    { "Frying Pan",   "frying-pan/sprite-128.png",         45, 110,  8, 0.45f, 38, 34, "bonk" },
    { "Toshko's Katana", "katana/sprite-128.png",          45, 240, 16, 0.32f, 54, 10, "clang" },
};

// An upgrade's price: one cigarette pack, and `gold` times the level it's going to.
struct UpgradeDef { const char* name; const char* desc; int gold; };
inline constexpr UpgradeDef UPGRADES[UPGRADE_COUNT] = {
    { "Sharpen", "+3 blade damage",  25 },
    { "Balance", "Swings 8% faster", 25 },
    { "Extend",  "+4 reach",         20 },
};

// Every Gakev drops a cigarette pack. This is the chance (in percent) that a chest holds one too.
inline constexpr int CHEST_PACK_PERCENT = 25;

// Soldiers hold their ground this far from you, and back off when you get closer than this.
inline constexpr float SOLDIER_RANGE = 220, SOLDIER_CLOSE = 130;
inline constexpr float ARROW_SPEED = 300, ORB_SHOT_SPEED = 190;
inline constexpr float ORC_REACH = 24;          // an orc's axe lands this far beyond touching you

// The feel of combat.
inline constexpr float HIT_FREEZE = 0.05f;      // seconds the world stops on a blade hit
inline constexpr float KNOCK_DECAY = 8;         // how fast a shove dies away (per second)
inline constexpr float BURN_TIME = 3;           // seconds an Emberhex leaves its target alight
inline constexpr float SPIKE_GAP = 14;          // Tidelash raises spikes this far apart
inline constexpr float BOLT_JAG = 20;           // Stormbolt's trail changes direction this often
inline constexpr float CHEST_SLAM = 0.45f;      // seconds a chest shakes before the lid flies up
inline constexpr float FINAL_FREEZE = 0.4f, FINAL_SLOW = 1.0f, FINALE_TIME = 2.6f;   // the last hit on a boss
inline constexpr float MERCY_BELOW = 0.3f;      // in a boss fight your last 30% of health counts double
