// ============================================================================
//  Game.cpp — lifetime, the frame loop, runs, rooms, floors and records.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Render.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------- lifetime
static Texture2D loadArt(const char* file) {
    std::string p = findAsset(file);
    if (p.empty()) return Texture2D{};
    Texture2D t = LoadTexture(p.c_str());
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);   // painted art, often drawn scaled down
    return t;
}

void Game::init(bool windowed) {
    settings = save::readSettings();
    profile = save::readProfile();
    if (profile.champion < 0 || profile.champion >= CHAMPION_COUNT) profile.champion = 0;
    player.champion = profile.champion;
    hasSave = save::hasRun();
    if (settings.fullscreen && !windowed) ToggleBorderlessWindowed();

    sounds = au::buildSfx();
    music.init();
    music.setMood("menu");

    menuBg = loadArt("menu.png");
    castleTex = loadArt("castle.png");
    marinTex = loadArt("marin.png");
    shopTex = loadArt("shop.png");
    for (int i = 0; i < CHAMPION_COUNT; ++i)
        if (CHAMPIONS[i].art) champTex[i] = loadArt(CHAMPIONS[i].art);
    SPR.load();
    bossArt.load();
    fight.sfx = [this](const char* s) { play(s); };
}

void Game::shutdown() {
    SPR.unload();
    bossArt.unload();
    for (Texture2D* tx : { &menuBg, &castleTex, &marinTex, &shopTex })
        if (tx->id) UnloadTexture(*tx);
    for (auto& tx : champTex) if (tx.id) UnloadTexture(tx);
    for (auto& [name, s] : sounds) UnloadSound(s);
    music.unload();
}

void Game::play(const char* sfx) {
    auto it = sounds.find(sfx);
    if (it == sounds.end()) return;
    SetSoundVolume(it->second, settings.sfxVol);
    PlaySound(it->second);
}

void Game::frame() {
    float dt = fminf(GetFrameTime(), 0.05f);
    t += dt; clock += dt;

    if (in::pressed(KEY_F11)) {
        ToggleBorderlessWindowed();
        settings.fullscreen = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        save::writeSettings(settings);
    }
    music.vol = 0.5f * settings.musicVol;
    music.update();

    ui::beginFrame();
    BeginDrawing();
    ClearBackground(ui::INK);
    switch (screen) {
    case Screen::Menu:      menuScreen(); break;
    case Screen::Champions: championsScreen(); break;
    case Screen::Settings:  settingsScreen(); break;
    case Screen::Intro:     introScreen(); break;
    case Screen::Play:      playScreen(); break;
    }
    if (!pendingShot.empty()) {            // the autopilot's screenshot, taken before the buffers swap
        rlDrawRenderBatchActive();
        Image shot = LoadImageFromScreen();
        ExportImage(shot, pendingShot.c_str());
        UnloadImage(shot);
        pendingShot.clear();
    }
    EndDrawing();
    ui::endFrame();
    in::endFrame();
}

// -------------------------------------------------------------------- runs
static Player freshPlayer(int champion, int level) {
    Player p;
    p.champion = champion;
    for (int l = 1; l < level; ++l) { p.maxHp += 18; p.maxMana += 10; }
    p.level = level;
    p.xpNext = (int)(60 * powf((float)level, 1.5f));
    const ChampionDef& c = CHAMPIONS[champion];
    p.maxHp += c.hpBonus;
    p.hp = p.maxHp; p.mana = p.maxMana;
    if (c.kind == Special::Blink) p.charge = DUST_MAX;
    return p;
}

void Game::startRun() {
    mode = Mode::Run;
    player = freshPlayer(profile.champion, 1);
    player.pos = ROOM_CENTER;
    dungeon.generate();
    floor = 1; kills = 0; runTime = 0;
    dialogue.clear(); overlay = Overlay::None;
    screen = Screen::Play; t = 0;
    enterRoom();
    showBanner("The Mistwood Depths", std::string(currentBoss().name) + " waits at the bottom of floor 1", 3.0f);
}

void Game::startBossRush() {
    mode = Mode::BossRush;
    player = freshPlayer(profile.champion, 5);
    player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 2.0f };
    dungeon.generateArena();
    floor = 1; kills = 0;
    rushStage = 0; rushTime = 0; rushNextT = -1; rushWon = rushRecord = false;
    dialogue.clear(); overlay = Overlay::None;
    screen = Screen::Play; t = 0;
    enterRoom();
}

bool Game::continueRun() {
    static RunSave run;                 // a Dungeon is large; keep it off the stack
    if (!save::readRun(run)) { hasSave = false; return false; }
    mode = Mode::Run;
    player = run.player; dungeon = run.dungeon;
    floor = run.floor; kills = run.kills; runTime = run.runTime;
    dialogue.clear(); overlay = Overlay::None;
    screen = Screen::Play; t = 0;
    enterRoom();
    showBanner(TextFormat("Floor %d", floor), "Welcome back", 2.0f);
    return true;
}

bool Game::saveRun() {
    if (mode != Mode::Run) return false;
    static RunSave run;
    run = RunSave{};
    run.player = player; run.dungeon = dungeon;
    run.floor = floor; run.kills = kills; run.runTime = runTime;
    bool ok = save::writeRun(run);
    hasSave = hasSave || ok;
    return ok;
}

void Game::toMenu() {
    fight.end();
    overlay = Overlay::None;
    screen = Screen::Menu; t = 0;
    music.setMood("menu");
    hasSave = save::hasRun();
}

void Game::saveProfile() { save::writeProfile(profile); }

void Game::showBanner(const std::string& text, const std::string& sub, float seconds) {
    bannerText = text; bannerSub = sub; banner = seconds;
}

int Game::currentBossIndex() const {
    return mode == Mode::BossRush ? rushStage : bossIndexForFloor(floor);
}

const BossDef& Game::currentBoss() const { return BOSSES[currentBossIndex()]; }

float Game::currentStrength() const { return mode == Mode::BossRush ? 1.0f : bossStrength(floor); }

// ------------------------------------------------------------------- rooms
void Game::enterRoom() {
    enemies.clear(); corpses.clear(); spawned.clear();
    shots.clear(); enemyShots.clear();
    parts.clear(); popups.clear(); coins.clear(); floaters.clear();
    fight.end();
    chestOpenT = 99;
    if (overlay == Overlay::Shop) overlay = Overlay::None;

    Room& r = dungeon.cur();
    if (r.isShop && !r.visited) showBanner("Marin's Market", "Press E to trade", 2.0f);
    r.visited = true;
    inFight = false;
    music.setMood(r.isBoss && !r.cleared ? "boss" : "mist");
    if (r.cleared) return;

    auto spawn = [&](const char* name, float hp, float spd, int dmg, int xp, float rad, Color col,
        const char* weak, int kind) {
        Enemy e;
        e.name = name; e.hp = e.maxHp = hp; e.speed = spd; e.dmg = dmg; e.xp = xp;
        e.r = rad; e.color = col; e.weakness = weak; e.kind = kind;
        e.wobble = frand(0, 6.28f); e.animT = frand(0, 1);
        e.flankSide = GetRandomValue(0, 1) ? 1.0f : -1.0f; e.lungeCd = frand(0.5f, 2.0f);
        // a random open floor spot away from the walls
        int tx = 0, ty = 0;
        for (int tries = 0; tries < 100; ++tries) {
            tx = GetRandomValue(3, ROOM_TW - 4);
            ty = GetRandomValue(3, ROOM_TH - 4);
            if (r.isFloor(tx, ty)) break;
        }
        e.pos = { tx * (float)TILE + 16, ty * (float)TILE + 16 };
        enemies.push_back(e);
    };

    if (r.isBoss) {
        const BossDef& b = currentBoss();
        fight.begin(b, currentStrength(), { ROOM_CENTER.x, ROOM_CENTER.y - 30 }, enemies);
        std::string sub = std::string(b.epithet) + "  -  weak to " + b.weakness;
        if (mode == Mode::BossRush) sub = TextFormat("Stage %d / %d  -  weak to %s", rushStage + 1, BOSS_COUNT, b.weakness);
        showBanner(b.name, sub, 3.0f);
        play("ultimate");
        shake = 0.6f;
    }
    else {
        // deeper floors send more and tougher monsters
        float tough = 1.0f + 0.15f * (floor - 1);
        int extra = (floor - 1) / 2;
        for (int i = 0, n = 3 + GetRandomValue(0, 2) + extra; i < n; ++i)
            spawn("Mistling", 30 * tough, 60, 6, 12, 12, { 138,168,154,255 }, "Fire", EK_SKELETON);
        for (int i = 0, n = GetRandomValue(0, 1); i < n; ++i)
            spawn("Hollow Hound", 70 * tough, 110, 14, 26, 13, { 42,31,46,255 }, "Fire", EK_SKELETON2);
        for (int i = 0, n = GetRandomValue(0, 2); i < n; ++i)
            spawn("Wisp", 20 * tough, 85, 5, 10, 10, { 120,180,230,255 }, "Lightning", EK_SKULL);
        if (GetRandomValue(0, 2) == 0)
            spawn("Coin Thief", 45 * tough, 120, 9, 30, 12, { 40,60,140,255 }, "Lightning", EK_THIEF);
        if (GetRandomValue(0, 2) == 0)
            spawn("Melon Warrior", 110 * tough, 50, 16, 24, 14, { 230,150,60,255 }, "Fire", EK_MELON);
    }
    inFight = true;
}

void Game::descend() {
    floor++;
    profile.deepestFloor = std::max(profile.deepestFloor, floor);
    saveProfile();
    dungeon.generate();
    player.hp = fminf(player.maxHp, player.hp + 30);    // a breather between floors
    player.pos = ROOM_CENTER;
    overlay = Overlay::None;
    enterRoom();
    showBanner(TextFormat("Floor %d", floor), std::string(currentBoss().name) + " waits at the bottom", 2.5f);
    play("ultimate");
}

void Game::bossDefeated() {
    const BossDef& b = currentBoss();
    profile.bossesBeaten |= 1u << currentBossIndex();
    enemyShots.clear();
    fight.end();
    music.setMood("mist");
    play("quest");

    if (mode == Mode::Run) {
        profile.deepestFloor = std::max(profile.deepestFloor, floor);
        if (floor % BOSS_COUNT == 0) { profile.runsCompleted++; overlay = Overlay::RunComplete; }
        else showBanner(std::string(b.name) + " falls!", "The stairs lead down", 2.5f);
    }
    else if (rushStage + 1 >= BOSS_COUNT) {
        rushWon = true;
        rushRecord = profile.bestRush <= 0 || rushTime < profile.bestRush;
        if (rushRecord) profile.bestRush = rushTime;
        overlay = Overlay::RushOver;
    }
    else {
        showBanner(std::string(b.name) + " falls!", "The next challenger approaches...", 2.5f);
        rushNextT = 2.5f;
    }
    saveProfile();
}
