// ============================================================================
//  Game.cpp — lifetime, the frame loop, runs, rooms, floors and records.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Touch.h"
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

// Art that arrives without alpha, on a flat backdrop, is cut out as it loads. Magenta goes
// everywhere; white only where it reaches in from the edges, as the art has white in it too.
// Backdrop that shows through gaps in the art goes as well: with a `minHole`, so does any
// white patch of at least that many pixels.
static void cutOut(Color* px, int w, int h, int minHole) {
    for (int i = 0; i < w * h; ++i)
        if (std::min(px[i].r, px[i].b) - px[i].g > 50) px[i] = BLANK;

    auto patchAt = [&](int start) {     // the white touching a pixel; alpha 254 marks it as seen
        std::vector<int> patch;
        auto take = [&](int at) {
            Color& c = px[at];
            if (c.a != 255 || c.r < 225 || c.g < 225 || c.b < 225) return;
            c.a = 254;
            patch.push_back(at);
        };
        take(start);
        for (size_t i = 0; i < patch.size(); ++i) {
            int at = patch[i], x = at % w, y = at / w;
            if (x > 0) take(at - 1);
            if (x < w - 1) take(at + 1);
            if (y > 0) take(at - w);
            if (y < h - 1) take(at + w);
        }
        return patch;
    };
    auto clear = [&](const std::vector<int>& patch) { for (int at : patch) px[at] = BLANK; };
    for (int x = 0; x < w; ++x) { clear(patchAt(x)); clear(patchAt((h - 1) * w + x)); }
    for (int y = 0; y < h; ++y) { clear(patchAt(y * w)); clear(patchAt(y * w + w - 1)); }
    if (minHole > 0)
        for (int i = 0; i < w * h; ++i) {
            auto patch = patchAt(i);
            if ((int)patch.size() >= minHole) clear(patch);
        }
    for (int i = 0; i < w * h; ++i) if (px[i].a == 254) px[i].a = 255;
}

static Texture2D loadCutout(const char* file, int minHole = 0) {
    std::string p = findAsset(file);
    if (p.empty()) return Texture2D{};
    Image im = LoadImage(p.c_str());
    if (!im.data) return Texture2D{};
    ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px = (Color*)im.data;
    if (px[0].a == 255) cutOut(px, im.width, im.height, minHole);   // transparent art is ready (and may well be pink)

    Texture2D t = LoadTextureFromImage(im);
    UnloadImage(im);
    GenTextureMipmaps(&t);                          // huge pictures, drawn tiny
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

// Fullscreen is a borderless window one pixel taller than the monitor. A window that
// exactly covers the monitor gets Windows' fullscreen fast path, and screen shares
// (Discord, OBS window capture) then freeze on the last frame they saw.
static bool g_fullscreen = false;
static Vector2 g_winPos{};
static int g_winW = SCREEN_W, g_winH = SCREEN_H;

bool isFullscreen() { return g_fullscreen; }

void setFullscreen(bool on) {
    if (on == g_fullscreen || touch::enabled()) return;     // phones are always fullscreen
    if (on) {
        g_winPos = GetWindowPosition(); g_winW = GetScreenWidth(); g_winH = GetScreenHeight();
        int mon = GetCurrentMonitor();
        Vector2 mp = GetMonitorPosition(mon);
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon) + 1);
        SetWindowPosition((int)mp.x, (int)mp.y);
    }
    else {
        ClearWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(g_winW, g_winH);
        SetWindowPosition((int)g_winPos.x, (int)g_winPos.y);
    }
    g_fullscreen = on;
}

void Game::init(bool windowed) {
    settings = save::readSettings();
    touch::setLayout(save::readTouchLayout());
    profile = save::readProfile();
    if (profile.champion < 0 || profile.champion >= CHAMPION_COUNT) profile.champion = 0;
    player.champion = profile.champion;
    hasSave = save::hasRun();
    if (settings.fullscreen && !windowed) setFullscreen(true);

    sounds = au::buildSfx();
    music.init();
    music.setMood("menu");

    menuBg = loadArt("menu.png");
    castleTex = loadArt("castle.png");
    marinTex = loadArt("marin.png");
    shopTex = loadArt("shop.png");
    galinTex = loadCutout("Galin.png");
    galinShopTex = loadCutout("GalinShop.png", 50);    // the sky between its poles; the cigarette is smaller
    packTex = loadCutout("Marlboro.png");
    for (int i = 0; i < WEAPON_COUNT; ++i)
        weaponTex[i] = loadCutout(WEAPONS[i].art);
    for (int i = 0; i < CHAMPION_COUNT; ++i)
        if (CHAMPIONS[i].art) champTex[i] = loadArt(CHAMPIONS[i].art);
    for (int i = 0; i < CHAMPION_COUNT; ++i) walk[i].load(CHAMPIONS[i].walk);
    if (std::string p = findAsset("walk/fear.png"); !p.empty()) {
        fearTex = LoadTexture(p.c_str());
        SetTextureFilter(fearTex, TEXTURE_FILTER_POINT);
    }
    SPR.load();
    bossArt.load();
    fight.sfx = [this](const char* s) { play(s); };
}

void Game::shutdown() {
    SPR.unload();
    bossArt.unload();
    for (Texture2D* tx : { &menuBg, &castleTex, &marinTex, &shopTex, &galinTex, &galinShopTex, &packTex })
        if (tx->id) UnloadTexture(*tx);
    for (auto& tx : weaponTex) if (tx.id) UnloadTexture(tx);
    for (auto& tx : champTex) if (tx.id) UnloadTexture(tx);
    for (auto& w : walk) w.unload();
    if (fearTex.id) UnloadTexture(fearTex);
    for (auto& [name, s] : sounds) UnloadSound(s);
    music.unload();
}

void Game::play(const char* sfx) {
    auto it = sounds.find(sfx);
    if (it == sounds.end()) return;
    SetSoundVolume(it->second, settings.sfxVol);
    PlaySound(it->second);
}

// The game draws its own cursor so it is always visible, including in screen shares
// and captures that leave out the system pointer.
static void drawCursor() {
    if (!IsCursorOnScreen()) return;
    if (!IsCursorHidden()) HideCursor();
    Vector2 m = in::mouse();
    if (ui::wantsHand()) {
        DrawCircleV(m, 7, Fade(BLACK, 0.6f));
        DrawCircleV(m, 5, { 240, 200, 90, 255 });
        return;
    }
    Vector2 tip = m, l = { m.x, m.y + 18 }, r = { m.x + 12, m.y + 13 };
    DrawTriangle({ tip.x - 1, tip.y - 2 }, { l.x - 2, l.y + 2 }, { r.x + 3, r.y + 1 }, BLACK);
    DrawTriangle(tip, l, r, RAYWHITE);
}

void Game::frame() {
    float dt = fminf(GetFrameTime(), 0.05f);
    t += dt; clock += dt;

    if (in::pressed(KEY_F11)) {
        setFullscreen(!isFullscreen());
        settings.fullscreen = isFullscreen();
        save::writeSettings(settings);
    }
    music.vol = 0.5f * settings.musicVol;
    music.update();

    touch::update(screen == Screen::Play && overlay == Overlay::None, settings.keys);
    ui::beginFrame();
    BeginDrawing();
    ClearBackground(ui::INK);
    switch (screen) {
    case Screen::Menu:      menuScreen(); break;
    case Screen::Champions: championsScreen(); break;
    case Screen::Settings:  settingsScreen(); break;
    case Screen::Controls:  controlsScreen(); break;
    case Screen::Intro:     introScreen(); break;
    case Screen::Play:      playScreen(); break;
    }
    if (touch::enabled()) touch::draw(settings.keys);
    else drawCursor();
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
    dungeon.galinNext = GetRandomValue(0, 1);
    dungeon.generate();
    floor = 1; kills = 0; runTime = 0;
    dialogue.clear(); overlay = Overlay::None; scareT = 0;
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
    dialogue.clear(); overlay = Overlay::None; scareT = 0;
    screen = Screen::Play; t = 0;
    enterRoom();
}

bool Game::continueRun() {
    static RunSave run;                 // a Dungeon is large; keep it off the stack
    if (!save::readRun(run)) { hasSave = false; return false; }
    mode = Mode::Run;
    player = run.player; dungeon = run.dungeon;
    floor = run.floor; kills = run.kills; runTime = run.runTime;
    dialogue.clear(); overlay = Overlay::None; scareT = 0;
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
    flashes.clear(); arcs.clear(); cracks.clear(); spikes.clear(); rubble.clear();
    skyFlash = 0; finaleT = 0; slowMo = 0; rewardT = 0;
    chestOpenT = 99; chestLight = 0; chestLooting = false;
    castT = 0; hitStop = 0;
    if (overlay == Overlay::Shop || overlay == Overlay::Galin) overlay = Overlay::None;

    Room& r = dungeon.cur();
    if (r.isShop && !r.visited) showBanner(r.isGalin ? "Galin's Dobrich Stock" : "Marin's Market", "Press E to trade", 2.0f);
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
        e.shootCd = frand(0.8f, 2.2f);
        // a random open floor spot away from the walls
        int tx = 0, ty = 0;
        for (int tries = 0; tries < 100; ++tries) {
            tx = GetRandomValue(3, ROOM_TW - 4);
            ty = GetRandomValue(3, ROOM_TH - 4);
            if (!r.isFloor(tx, ty)) continue;
            // Toshko starts well away from the player, so there's time to react
            Vector2 at = { tx * (float)TILE + 16, ty * (float)TILE + 16 };
            if (kind == EK_TOSHKO && tries < 90 && Vector2Distance(at, player.pos) < 340) continue;
            break;
        }
        e.pos = { tx * (float)TILE + 16, ty * (float)TILE + 16 };
        enemies.push_back(e);
    };

    if (r.isBoss) {
        const BossDef& b = currentBoss();
        fight.begin(b, currentStrength(), { ROOM_CENTER.x, ROOM_CENTER.y - (b.kind == BossKind::Rooted ? 0 : 30) }, enemies);
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
        for (int i = 0, n = 2 + GetRandomValue(0, 2) + extra; i < n; ++i)
            spawn("Mistling", 30 * tough, 60, 6, 12, 12, { 138,168,154,255 }, "Fire", EK_SKELETON);
        for (int i = 0, n = GetRandomValue(0, 1); i < n; ++i)
            spawn("Hollow Hound", 70 * tough, 110, 14, 26, 13, { 42,31,46,255 }, "Fire", EK_SKELETON2);
        for (int i = 0, n = GetRandomValue(0, 1); i < n; ++i)
            spawn("Wisp", 20 * tough, 85, 5, 10, 10, { 120,180,230,255 }, "Lightning", EK_SKULL);
        if (GetRandomValue(0, 2) == 0)         // Gakev: the only one who carries cigarette packs
            spawn("Gakev", 45 * tough, 120, 9, 30, 12, { 40,60,140,255 }, "Lightning", EK_GAKEV);
        if (GetRandomValue(1, 100) <= 30)      // Toshko: snatches 50 gold and runs
            spawn("Toshko", 40 * tough, 175, 0, 40, 12, { 30,30,60,255 }, "Lightning", EK_TOSHKO);
        if (GetRandomValue(0, 2) == 0)
            spawn("Melon Warrior", 110 * tough, 50, 16, 24, 14, { 230,150,60,255 }, "Fire", EK_MELON);
        for (int i = 0, n = 1 + GetRandomValue(0, 1) + extra / 2; i < n; ++i) {  // something that shoots, in every combat room
            int shooter = GetRandomValue(0, 2);
            if (shooter == 0) spawn("Soldier", 35 * tough, 70, 8, 20, 12, { 150,160,180,255 }, "Lightning", EK_SOLDIER);
            else if (shooter == 1) spawn("Wisp", 20 * tough, 85, 5, 10, 10, { 120,180,230,255 }, "Lightning", EK_SKULL);
            else spawn("Blood Monster", 70 * tough, 60, 10, 24, 14, { 170,30,60,255 }, "Fire", EK_BLOOD);
        }
        if (GetRandomValue(1, 100) <= 33)      // Orc: a slow axe you can step away from
            spawn("Orc", 90 * tough, 75, 16, 28, 13, { 110,150,70,255 }, "Fire", EK_ORC);
        if (floor >= 2 && GetRandomValue(1, 100) <= 25)  // Demon: leaps at you from across the room
            spawn("Demon", 55 * tough, 105, 12, 32, 12, { 120,30,40,255 }, "Water", EK_DEMON);
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
    play("victory");
    rubble.clear();
    for (int k = 0; k < 36; ++k) {             // a shower of stars over the reward
        float life = frand(0.9f, 1.5f);
        parts.push_back({ { ROOM_CENTER.x + frand(-50, 50), ROOM_CENTER.y }, Vector2Scale(fromAngle(-PI / 2 + frand(-1.1f, 1.1f)), frand(120, 320)), life, life,
            frand(3, 6), k % 3 ? Color{ 255, 222, 110, 255 } : Color{ 255, 255, 235, 255 }, 300, false, true });
    }
    std::string fell = std::string(b.name) + " falls!";
    showBanner("BOSS DEFEATED", fell, 3.0f);
    auto thenShow = [this](Overlay o) { rewardOverlay = o; rewardT = 2.4f; };

    if (mode == Mode::Run) {
        profile.deepestFloor = std::max(profile.deepestFloor, floor);
        if (floor % BOSS_COUNT == 0) { profile.runsCompleted++; thenShow(Overlay::RunComplete); }
        else showBanner("BOSS DEFEATED", fell + " The stairs lead down", 3.0f);
    }
    else if (rushStage + 1 >= BOSS_COUNT) {
        rushWon = true;
        rushRecord = profile.bestRush <= 0 || rushTime < profile.bestRush;
        if (rushRecord) profile.bestRush = rushTime;
        thenShow(Overlay::RushOver);
    }
    else {
        showBanner("BOSS DEFEATED", fell + " The next challenger approaches...", 2.5f);
        rushNextT = 2.5f;
    }
    saveProfile();
}
