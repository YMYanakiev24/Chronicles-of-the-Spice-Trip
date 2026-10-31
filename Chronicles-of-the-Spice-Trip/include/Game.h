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
#include <map>
#include <string>
#include <vector>

enum class Screen { Menu, Champions, Settings, Intro, Play };
enum class Mode { Run, BossRush };
enum class Overlay { None, Pause, Shop, Galin, RunComplete, RushOver };

struct Popup { const Anim* anim; Vector2 pos; float t; };        // a flask rising from a chest
struct Coin { Vector2 pos, vel; float t; bool pack = false; };    // loot that flies to you (gold, or a cigarette pack)
struct FloatText { Vector2 pos; float t; int value; Color color; };  // damage numbers

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
    void menuScreen();
    void championsScreen();
    void settingsScreen();
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
    void hurtPlayer(float damage, float invuln);
    void killEnemy(Enemy& e);
    void finalBlow(const Enemy& e);     // the last of the boss: freeze, slow down, fall apart
    void checkRoomExits();
    void buy(int item);
    void buyWeapon(int weapon);         // buys it, or just equips it if it's owned
    void buyUpgrade(int upgrade);       // for the weapon in hand
    Camera2D camera() const;
    Vector2 mouseWorld() const;
    Vector2 aimDir() const;
    Vector2 castHand() const;           // where a spell gathers as it winds up
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
    void drawRunComplete();
    void drawRushOver();
    void drawMenuBackground();
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
    float banner = 0;
    std::string bannerText, bannerSub;
    std::string dialogue;
    int   dialogueIdx = 0;
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
    Texture2D galinTex{}, galinShopTex{}, packTex{};
    Texture2D weaponTex[WEAPON_COUNT]{};
    Texture2D champTex[CHAMPION_COUNT]{};
    WalkSheet walk[CHAMPION_COUNT];     // each champion from every side (assets/walk/)
    Texture2D fearTex{};                // the fearful face (assets/walk/fear.png)
    BossArt bossArt;
};

// Fixed spots in the start and shop rooms.
inline constexpr Vector2 MERCHANT_POS = { ROOM_PX_W / 2, ROOM_PX_H / 2 - 10 };
inline constexpr Vector2 ORACLE_POS = { ROOM_PX_W / 2 - 90, ROOM_PX_H / 2 - 40 };
inline constexpr Vector2 ROOM_CENTER = { ROOM_PX_W / 2, ROOM_PX_H / 2 };

// What Marin sells. Potions (vape == -1) can be bought again and again; a vape
// is a permanent buff, one of each.
struct ShopItem { const char* name; const char* desc; int price; int vape; };
inline constexpr ShopItem SHOP_ITEMS[] = {
    { "Health Potion",   "Heals 35 HP (never above max)",         8, -1 },
    { "Mana Potion",     "Restores 50 mana",                      6, -1 },
    { "Mango Haze",      "Vape: move 10% faster, for good",      30, VP_MANGO },
    { "Blue Razz Ice",   "Vape: powers cost 20% less, for good", 35, VP_RAZZ },
    { "Watermelon Rage", "Vape: +4 blade damage, for good",      30, VP_MELON },
    { "Cool Mint Fog",   "Vape: take 15% less damage, for good", 40, VP_MINT },
};
inline constexpr int SHOP_COUNT = (int)(sizeof SHOP_ITEMS / sizeof SHOP_ITEMS[0]);

// The colour of each vape's vapour puffs.
inline constexpr Color VAPE_COLOR[VP_COUNT] = {
    { 255, 176, 64, 255 }, { 96, 160, 255, 255 }, { 240, 80, 96, 255 }, { 170, 250, 220, 255 } };

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

// The feel of combat.
inline constexpr float HIT_FREEZE = 0.05f;      // seconds the world stops on a blade hit
inline constexpr float KNOCK_DECAY = 8;         // how fast a shove dies away (per second)
inline constexpr float BURN_TIME = 3;           // seconds an Emberhex leaves its target alight
inline constexpr float SPIKE_GAP = 14;          // Tidelash raises spikes this far apart
inline constexpr float BOLT_JAG = 20;           // Stormbolt's trail changes direction this often
inline constexpr float CHEST_SLAM = 0.45f;      // seconds a chest shakes before the lid flies up
inline constexpr float FINAL_FREEZE = 0.4f, FINAL_SLOW = 1.0f, FINALE_TIME = 2.6f;   // the last hit on a boss
inline constexpr float MERCY_BELOW = 0.3f;      // in a boss fight your last 30% of health counts double
