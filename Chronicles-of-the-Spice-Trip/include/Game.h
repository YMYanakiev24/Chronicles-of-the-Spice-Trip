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
enum class Overlay { None, Pause, Shop, RunComplete, RushOver };

struct Popup { const Anim* anim; Vector2 pos; float t; };        // a flask rising from a chest
struct Coin { Vector2 pos, vel; float t; };                       // loot that flies to you
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
    void swingBlade();
    void updateShots(float dt);
    void updateEnemies(float dt, float slow);
    void updateEnemyShots(float dt, float slow);
    void updateLoot(float dt);
    void hitEnemy(Enemy& e, int damage, Color fx);
    void hurtPlayer(float damage, float invuln);
    void killEnemy(Enemy& e);
    void checkRoomExits();
    void buy(int item);
    Camera2D camera() const;
    Vector2 mouseWorld() const;
    float hurtMul() const;

    // ---------------------------------------------------------------- drawing
    void drawRoom();
    void drawActors();
    void drawHud();
    void drawMinimap();
    void drawBossBar();
    void drawShop();
    void drawPause();
    void drawRunComplete();
    void drawRushOver();
    void drawMenuBackground();
    void drawFear(float x, float above, float below, float size) const;   // over a scared enemy
    void drawName(int champion, float x, float y, int size, Color color, bool centred = false) const;
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
    Texture2D champTex[CHAMPION_COUNT]{};
    WalkSheet walk[CHAMPION_COUNT];     // each champion from every side (assets/walk/)
    Texture2D fearTex{};                // the fearful face (assets/walk/fear.png)
    BossArt bossArt;
};

// Fixed spots in the start and shop rooms.
inline constexpr Vector2 MERCHANT_POS = { ROOM_PX_W / 2, ROOM_PX_H / 2 - 10 };
inline constexpr Vector2 ORACLE_POS = { ROOM_PX_W / 2 - 90, ROOM_PX_H / 2 - 40 };
inline constexpr Vector2 ROOM_CENTER = { ROOM_PX_W / 2, ROOM_PX_H / 2 };

// What Marin sells. skill == 0 marks a consumable that can be bought repeatedly;
// vape >= 0 makes it a timed vape buff instead of a potion.
struct ShopItem { const char* name; const char* desc; int price; unsigned skill; int vape; };
inline constexpr ShopItem SHOP_ITEMS[] = {
    { "Health Potion",   "Heals 35 HP (never above max)",       8, 0, -1 },
    { "Mana Potion",     "Restores 50 mana",                    6, 0, -1 },
    { "Mango Haze",      "Vape: move 35% faster for 20s",      12, 0, VP_MANGO },
    { "Blue Razz Ice",   "Vape: powers cost no mana for 12s",  18, 0, VP_RAZZ },
    { "Watermelon Rage", "Vape: +12 blade damage for 20s",     15, 0, VP_MELON },
    { "Cool Mint Fog",   "Vape: take 50% less damage for 15s", 15, 0, VP_MINT },
    { "Iron Skin",       "Take 25% less damage",               30, SK_IRON_SKIN, -1 },
    { "Swift Feet",      "Move 15% faster",                    20, SK_SWIFT, -1 },
    { "Sharpened Blade", "+8 blade damage",                    25, SK_BLADE, -1 },
    { "Arcane Flow",     "Mana regenerates 60% faster",        25, SK_ARCANE, -1 },
    { "Shadow Step",     "Dodge-roll recharges twice as fast", 15, SK_DASH, -1 },
    { "Leeching Edge",   "Blade hits heal 2 HP",               35, SK_LEECH, -1 },
};
inline constexpr int SHOP_COUNT = (int)(sizeof SHOP_ITEMS / sizeof SHOP_ITEMS[0]);

// How long each vape lasts, and the colour of its vapour puffs.
inline constexpr float VAPE_TIME[VP_COUNT] = { 20, 12, 20, 15 };
inline constexpr Color VAPE_COLOR[VP_COUNT] = {
    { 255, 176, 64, 255 }, { 96, 160, 255, 255 }, { 240, 80, 96, 255 }, { 170, 250, 220, 255 } };
