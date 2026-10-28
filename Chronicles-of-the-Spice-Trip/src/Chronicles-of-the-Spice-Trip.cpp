// ============================================================================
//  The Mythic Chronicles: The Hidden World  —  raylib Edition
// ----------------------------------------------------------------------------
//  Entry point + game loop. Split across:
//    Common.h            shared constants, palette, helpers
//    Audio.h/.cpp        runtime sound synthesis (music + 13 SFX)
//    Entities.h/.cpp     spells, monsters, projectiles, the player
//    World.h/.cpp        tile map + movement collision
//    Render.h/.cpp       procedural pixel sprites + asset lookup
//    Settings.h          audio/keybind settings + champion roster
//    Sprites.h/.cpp      dungeon-pack tiles + strip animations
//    this file           MENU / CHARSELECT / SETTINGS / INTRO / PLAY
//
//  The window is resizable and can go borderless-fullscreen (F11), which hides
//  the taskbar. Menu layout follows the live window size.
// ============================================================================

#include "Common.h"
#include "Audio.h"
#include "Entities.h"
#include "World.h"
#include "Render.h"
#include "Settings.h"
#include "Rooms.h"
#include "Sprites.h"

#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <cstring>

enum class State { MENU, CHARSELECT, SETTINGS, INTRO, PLAY };

static const char* SAVE_FILE = "savegame.dat";
static const unsigned SAVE_MAGIC = 0x53504332;   // "SPC2" (gold + skills)

// Everything needed to resume a run. All members are trivially copyable.
struct SaveData {
    unsigned magic = SAVE_MAGIC;
    Player   player;
    Dungeon  dungeon;
    int      floor = 1, kills = 0, champion = 0;
};

static bool writeSave(const SaveData& d) {
    return SaveFileData(SAVE_FILE, (void*)&d, (int)sizeof d);
}

static bool readSave(SaveData& d) {
    int size = 0;
    unsigned char* data = LoadFileData(SAVE_FILE, &size);
    if (!data) return false;
    bool ok = size == (int)sizeof d;
    if (ok) { memcpy(&d, data, sizeof d); ok = d.magic == SAVE_MAGIC; }
    UnloadFileData(data);
    return ok;
}

// What Marin sells. skill == 0 marks a consumable that can be bought repeatedly;
// vape >= 0 makes it a timed vape buff instead of a potion.
struct ShopItem { const char* name; const char* desc; int price; unsigned skill; int vape; };
static const ShopItem SHOP_ITEMS[] = {
    { "Health Potion",   "Heals 35 HP (never above max)",     8, 0, -1 },
    { "Mana Potion",     "Restores 50 mana",                  6, 0, -1 },
    { "Mango Haze",      "Vape: move 35% faster for 20s",    12, 0, VP_MANGO },
    { "Blue Razz Ice",   "Vape: powers cost no mana for 12s", 18, 0, VP_RAZZ },
    { "Watermelon Rage", "Vape: +12 blade damage for 20s",   15, 0, VP_MELON },
    { "Cool Mint Fog",   "Vape: take 50% less damage for 15s", 15, 0, VP_MINT },
    { "Iron Skin",       "Take 25% less damage",             30, SK_IRON_SKIN, -1 },
    { "Swift Feet",      "Move 15% faster",                  20, SK_SWIFT, -1 },
    { "Sharpened Blade", "+8 blade damage",                  25, SK_BLADE, -1 },
    { "Arcane Flow",     "Mana regenerates 60% faster",      25, SK_ARCANE, -1 },
    { "Shadow Step",     "Dodge-roll recharges twice as fast", 15, SK_DASH, -1 },
    { "Leeching Edge",   "Blade hits heal 2 HP",             35, SK_LEECH, -1 },
};
static const int SHOP_COUNT = (int)(sizeof SHOP_ITEMS / sizeof SHOP_ITEMS[0]);

// How long each vape lasts, and the colour of its vapour puffs.
static const float VAPE_TIME[VP_COUNT] = { 20, 12, 20, 15 };
static const Color VAPE_COLOR[VP_COUNT] = {
    { 255, 176, 64, 255 }, { 96, 160, 255, 255 }, { 240, 80, 96, 255 }, { 170, 250, 220, 255 } };

static const char* PROPHECY[4] = {
    "When the Veil grows thin and the old powers stir,",
    "an Unmarked child shall take the godless blade.",
    "Through cursed wood and drowned and hollow hall,",
    "they will wake what sleeps -- and be claimed, or unmade.",
};

// --- small immediate-mode UI helpers ---------------------------------------
static bool button(Rectangle r, const char* label, int fontSize, Color fill,
    Color border, Color text, Vector2 mouse) {
    bool over = CheckCollisionPointRec(mouse, r);
    DrawRectangleRec(r, over ? ColorBrightness(fill, 0.15f) : fill);
    DrawRectangleLinesEx(r, 2.0f, over ? ColorBrightness(border, 0.3f) : border);
    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - fontSize) / 2), fontSize, text);
    return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "The Mythic Chronicles: The Hidden World");
    SetWindowMinSize(960, 540);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);                 // we handle Esc ourselves
    InitAudioDevice();

    // Start in borderless fullscreen (covers the taskbar). F11 toggles it.
    ToggleBorderlessWindowed();

    Settings settings;

    // Audio
    std::map<std::string, Sound> SFX = au::buildSfx();
    auto play = [&](const std::string& n) {
        auto it = SFX.find(n);
        if (it == SFX.end()) return;
        SetSoundVolume(it->second, settings.sfxVol);
        PlaySound(it->second);
    };
    au::Music music; music.init(); music.setMood("menu");

    // Helper: load a PNG asset into a texture (id 0 if missing).
    auto loadTex = [&](const char* file) -> Texture2D {
        std::string p = findAsset(file);
        if (p.empty()) return Texture2D{};
        Image im = LoadImage(p.c_str());
        if (!im.data) return Texture2D{};
        Texture2D tx = LoadTextureFromImage(im);
        UnloadImage(im);
        return tx;
    };

    // Menu art + castle button + champion portraits.
    Texture2D menuBg = loadTex("menu.png");
    if (menuBg.id == 0) menuBg = loadTex("mainscreen.png");
    Texture2D castleTex = loadTex("castle.png");           // optional overlay
    SPR.load();                                             // dungeon tiles + animations
    Texture2D marinTex = loadTex("marin.png");               // the merchant
    Texture2D shopTex = loadTex("shop.png");                 // shop screen backdrop

    std::vector<Champion> roster = championRoster();
    std::vector<Texture2D> champTex(roster.size(), Texture2D{});
    for (size_t i = 0; i < roster.size(); ++i)
        if (roster[i].file) champTex[i] = loadTex(roster[i].file);
    int selectedChampion = 0;

    State state = State::MENU;
    float t = 0;
    int rebinding = -1;                   // index into a keybind array, or -1

    // World (created fresh when entering PLAY)
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Projectile> shots;
    std::vector<Particle> parts;
    std::vector<Enemy> corpses;               // enemies playing their death animation
    struct Popup { const Anim* anim; Vector2 pos; float t; };       // flask rising from a chest
    struct Coin { Vector2 pos, vel; float t; };                      // loot sparkle that flies to you
    std::vector<Popup> popups;
    std::vector<Coin> coins;
    float chestOpenT = 99;                     // time since this room's chest opened
    float shake = 0;                           // screen-shake strength (seconds left)
    bool shopOpen = false;                     // Marin's trade window
    float shopMsg = 0; std::string shopText;   // "Bought X" / "Not enough gold"
    static FlowField flow;                     // path distances to the player, rebuilt each frame
    Vector2 merchantPos{ ROOM_PX_W / 2, ROOM_PX_H / 2 - 10 };
    float gloom = 0;                           // extra darkness, eased toward its target
    Vector2 oraclePos{ ROOM_PX_W / 2 - 90, ROOM_PX_H / 2 - 40 };
    std::vector<std::string> oracleLines = {
        "So. The Veil spat you out at last.",
        "Press 1 and right-click to loose an Emberhex on the mistlings.",
        "Clear a room to open its doors. The boss waits at the far end." };
    std::string dialogue; int dialogueIdx = 0;
    int kills = 0; float banner = 0; int floor = 1;
    Dungeon dungeon; bool inFight = false;
    bool paused = false; float trapClock = 0; float saveMsg = 0; const char* saveText = "";
    bool hasSave = FileExists(SAVE_FILE); std::string bannerText = "The Mistwood Depths";

    auto spawnEnemy = [&](const char* name, float hp, float spd, int dmg, int xp,
        float r, Color col, const char* weak, bool boss, Vector2 at, int kind) {
        Enemy e;
        e.name = name; e.hp = e.maxHp = hp; e.speed = spd; e.dmg = dmg; e.xp = xp;
        e.r = r; e.color = col; e.weakness = weak; e.boss = boss; e.pos = at;
        e.wobble = frand(0, 6.28f); e.kind = kind; e.animT = frand(0, 1);
        e.flankSide = GetRandomValue(0, 1) ? 1.0f : -1.0f; e.lungeCd = frand(0.5f, 2.0f);
        enemies.push_back(e);
    };
    // A random open floor spot inside the current room (away from the walls).
    auto interiorPoint = [&]() -> Vector2 {
        int tx = 0, ty = 0;
        for (int tries = 0; tries < 100; ++tries) {
            tx = GetRandomValue(3, ROOM_TW - 4);
            ty = GetRandomValue(3, ROOM_TH - 4);
            if (dungeon.cur().isFloor(tx, ty)) break;
        }
        return { tx * (float)TILE + 16, ty * (float)TILE + 16 };
    };
    // Load the current room: clear actors, then spawn its enemies if uncleared.
    auto enterRoom = [&]() {
        enemies.clear(); shots.clear(); parts.clear();
        corpses.clear(); popups.clear(); coins.clear();
        Room& r = dungeon.cur();
        chestOpenT = 99; shopOpen = false;
        if (r.isShop && !r.visited) { banner = 2.0f; bannerText = "Marin's Market"; }
        r.visited = true;
        inFight = false;
        if (r.cleared) return;
        if (r.isBoss) {
            spawnEnemy("Pyraketh, the Cinder-Maw", 340, 55, 30, 150, 24,
                { 106,42,31,255 }, "Water", true, { ROOM_PX_W / 2, ROOM_PX_H / 2 }, EK_VAMPIRE);
            play("ultimate"); banner = 2.2f; bannerText = "Pyraketh, the Cinder-Maw, wakes";
            shake = 0.9f;
        }
        else {
            int m = 3 + GetRandomValue(0, 2);
            for (int i = 0; i < m; ++i)
                spawnEnemy("Mistling", 30, 60, 6, 12, 12, { 138,168,154,255 }, "Fire", false, interiorPoint(), EK_SKELETON);
            for (int i = 0, h = GetRandomValue(0, 1); i < h; ++i)
                spawnEnemy("Hollow Hound", 70, 110, 14, 26, 13, { 42,31,46,255 }, "Fire", false, interiorPoint(), EK_SKELETON2);
            for (int i = 0, w = GetRandomValue(0, 2); i < w; ++i)
                spawnEnemy("Wisp", 20, 85, 5, 10, 10, { 120,180,230,255 }, "Lightning", false, interiorPoint(), EK_SKULL);
            if (GetRandomValue(0, 2) == 0)
                spawnEnemy("Coin Thief", 45, 120, 9, 30, 12, { 40,60,140,255 }, "Lightning", false, interiorPoint(), EK_THIEF);
            if (GetRandomValue(0, 2) == 0)
                spawnEnemy("Melon Warrior", 110, 50, 16, 24, 14, { 230,150,60,255 }, "Fire", false, interiorPoint(), EK_MELON);
        }
        inFight = true;
    };
    auto startGame = [&]() {
        player = Player{};
        player.pos = { ROOM_PX_W / 2, ROOM_PX_H / 2 };
        dungeon.generate();
        kills = 0; floor = 1; banner = 3.0f; bannerText = "The Mistwood Depths"; dialogue.clear();
        enterRoom();
        music.setMood("mist");
        paused = false;
    };
    auto loadGame = [&]() -> bool {
        static SaveData d;      // Dungeon is large; keep it off the stack
        if (!readSave(d)) return false;
        player = d.player; dungeon = d.dungeon; floor = d.floor; kills = d.kills;
        selectedChampion = d.champion;
        dialogue.clear(); banner = 2.0f; bannerText = TextFormat("Floor %d", floor);
        enterRoom();
        music.setMood("mist");
        paused = false;
        return true;
    };

    // ----------------------------------------------------------------- LOOP
    while (!WindowShouldClose()) {
        float dt = GetFrameTime(); if (dt > 0.05f) dt = 0.05f;
        t += dt;
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        Vector2 mouse = GetMousePosition();

        // F11 toggles borderless fullscreen (covers the taskbar).
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        // keep music volume in sync with settings
        music.vol = 0.5f * settings.musicVol;
        music.update();

        // ===================================================== MENU + screens
        // Draw the shared menu background (anchored to the TOP so the title is
        // never clipped by the window edge), used by MENU/CHARSELECT/SETTINGS.
        auto drawMenuBackground = [&]() {
            if (menuBg.id) {
                float sc = fmaxf((float)sw / menuBg.width, (float)sh / menuBg.height);
                float dx = (sw - menuBg.width * sc) / 2;
                DrawTextureEx(menuBg, { dx, 0 }, 0, sc, WHITE);   // dy = 0 -> top visible
            }
            else {
                ClearBackground({ 30, 24, 40, 255 });
                const char* tt = "CHRONICLES OF THE SPICE TRIP";
                int tw = MeasureText(tt, 40); DrawText(tt, (sw - tw) / 2, 60, 40, { 235, 200, 120, 255 });
            }
        };

        // ------------------------------------------------------------- MENU
        if (state == State::MENU) {
            // Castle hotspot, expressed as fractions of the displayed art.
            // The image is cover-scaled and anchored to the top (dy = 0).
            float sc = menuBg.id ? fmaxf((float)sw / menuBg.width, (float)sh / menuBg.height) : 1.0f;
            float imgW = menuBg.id ? menuBg.width * sc : (float)sw;
            float imgH = menuBg.id ? menuBg.height * sc : (float)sh;
            float imgX = (sw - imgW) / 2.0f;
            Rectangle castle = {
                imgX + imgW * 0.625f,   // left edge of the castle hill
                imgH * 0.010f,          // top of the tallest tower
                imgW * 0.200f,          // width
                imgH * 0.360f           // down to the base
            };
            bool overCastle = CheckCollisionPointRec(mouse, castle);

            // Bottom-left buttons
            float bw = 220, bh = 50, gap = 14, bx = 44;
            Rectangle contBtn = { bx, sh - 40.0f - 4 * bh - 3 * gap, bw, bh };
            Rectangle charBtn = { bx, sh - 40.0f - 3 * bh - 2 * gap, bw, bh };
            Rectangle setBtn = { bx, sh - 40.0f - 2 * bh - 1 * gap, bw, bh };
            Rectangle exitBtn = { bx, sh - 40.0f - 1 * bh,           bw, bh };

            bool overUi = (hasSave && CheckCollisionPointRec(mouse, contBtn)) ||
                CheckCollisionPointRec(mouse, charBtn) ||
                CheckCollisionPointRec(mouse, setBtn) ||
                CheckCollisionPointRec(mouse, exitBtn);
            SetMouseCursor((overCastle || overUi) ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);

            // Enter the game by clicking the castle (or Enter/Space).
            if ((overCastle && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
                IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                play("ui"); SetMouseCursor(MOUSE_CURSOR_DEFAULT); state = State::INTRO; t = 0;
            }

            BeginDrawing();
            ClearBackground({ 8, 10, 16, 255 });
            drawMenuBackground();

            // Castle "button": overlay castle.png on the castle, brighten on hover.
            if (castleTex.id) {
                float csc = fminf(castle.width / castleTex.width, castle.height / castleTex.height);
                float cw = castleTex.width * csc, ch = castleTex.height * csc;
                Vector2 cpos = { castle.x + (castle.width - cw) / 2, castle.y + (castle.height - ch) / 2 };
                DrawTextureEx(castleTex, cpos, 0, csc, overCastle ? Color{ 255,255,255,255 } : Color{ 225,225,235,255 });
            }
            if (overCastle) {
                DrawRectangleLinesEx(castle, 2.0f, Fade({ 245, 215, 140, 255 }, 0.9f));
                const char* hint = "Enter the castle";
                int hw = MeasureText(hint, 20);
                DrawText(hint, (int)(castle.x + (castle.width - hw) / 2), (int)(castle.y + castle.height + 8), 20, { 245, 215, 140, 255 });
            }

            // Bottom-left menu buttons
            Color pf{ 74, 42, 120, 235 }, pb{ 180, 150, 230, 255 };
            if (hasSave && button(contBtn, "> Continue", 24, Color{ 40, 90, 60, 235 }, Color{ 140, 220, 160, 255 }, RAYWHITE, mouse)) {
                play("ui"); SetMouseCursor(MOUSE_CURSOR_DEFAULT);
                if (loadGame()) { state = State::PLAY; t = 0; }
                else hasSave = false;
            }
            if (button(charBtn, "> Character", 24, pf, pb, RAYWHITE, mouse)) { play("ui"); state = State::CHARSELECT; }
            if (button(setBtn, "> Settings", 24, pf, pb, RAYWHITE, mouse)) { play("ui"); state = State::SETTINGS; rebinding = -1; }
            if (button(exitBtn, "> Exit", 24, Color{ 96, 36, 44, 235 }, Color{ 230, 140, 150, 255 }, RAYWHITE, mouse)) { play("ui"); break; }

            DrawText("F11 fullscreen", 44, sh - 30, 14, Fade(RAYWHITE, 0.5f));
            EndDrawing();
            continue;
        }

        // -------------------------------------------------------- CHARSELECT
        if (state == State::CHARSELECT) {
            if (IsKeyPressed(KEY_ESCAPE)) { play("ui"); state = State::MENU; }

            BeginDrawing();
            ClearBackground({ 8, 10, 16, 255 });
            drawMenuBackground();
            DrawRectangle(0, 0, sw, sh, { 10, 8, 18, 210 });

            // Panel
            float pw = fminf(980.0f, sw - 80.0f), ph = fminf(640.0f, sh - 60.0f);
            Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
            DrawRectangleRec(panel, { 24, 18, 36, 245 });
            DrawRectangleLinesEx(panel, 2.0f, { 150, 120, 190, 255 });
            DrawText("Choose Your Champion", (int)panel.x + 30, (int)panel.y + 24, 34, { 240, 205, 120, 255 });
            DrawText("Pick who you fight as. Drop more art into /assets and add them later.",
                (int)panel.x + 32, (int)panel.y + 66, 18, { 200, 195, 210, 255 });

            // Close (X)
            Rectangle xBtn = { panel.x + pw - 50, panel.y + 16, 34, 34 };
            if (button(xBtn, "X", 22, Color{ 60, 40, 80, 255 }, Color{ 180, 150, 220, 255 }, RAYWHITE, mouse)) { play("ui"); state = State::MENU; }

            // Grid of champion cards (5 columns)
            int cols = 5;
            float pad = 18, gx = panel.x + 30, gy = panel.y + 110;
            float cw = (pw - 60 - (cols - 1) * pad) / cols;
            float chh = cw * 1.0f;
            for (size_t i = 0; i < roster.size(); ++i) {
                int col = (int)i % cols, row = (int)i / cols;
                Rectangle card = { gx + col * (cw + pad), gy + row * (chh + 36 + pad), cw, chh + 36 };
                bool over = CheckCollisionPointRec(mouse, card);
                bool sel = ((int)i == selectedChampion);
                Champion& ch = roster[i];

                DrawRectangleRec(card, ch.empty ? Color{ 18,16,24,255 } : Color{ 30, 26, 42, 255 });
                Color bd = sel ? Color{ 110, 230, 130, 255 } : (over ? Color{ 210, 180, 120, 255 } : Color{ 90, 80, 110, 255 });
                DrawRectangleLinesEx(card, sel ? 3.0f : 2.0f, bd);

                // portrait area
                Rectangle pic = { card.x + 8, card.y + 8, card.width - 16, chh - 16 };
                if (champTex[i].id) {
                    float isc = fminf(pic.width / champTex[i].width, pic.height / champTex[i].height);
                    float iw = champTex[i].width * isc, ih = champTex[i].height * isc;
                    DrawTextureEx(champTex[i], { pic.x + (pic.width - iw) / 2, pic.y + (pic.height - ih) / 2 }, 0, isc, WHITE);
                }
                else if (ch.empty) {
                    DrawText("?", (int)(pic.x + pic.width / 2 - 8), (int)(pic.y + pic.height / 2 - 16), 32, { 90, 84, 100, 255 });
                }
                else {
                    // procedural hero portrait (Spellwoven)
                    DrawRectangle((int)pic.x, (int)pic.y, (int)pic.width, (int)pic.height, { 14, 16, 26, 255 });
                    drawHero({ pic.x + pic.width / 2, pic.y + pic.height / 2 + 14 }, { 0,1 }, 0, false);
                }

                const char* nm = ch.name;
                int nw = MeasureText(nm, 16);
                DrawText(nm, (int)(card.x + (card.width - nw) / 2), (int)(card.y + chh + 4), 16,
                    ch.empty ? Color{ 110,104,120,255 } : RAYWHITE);

                if (over && !ch.empty && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    selectedChampion = (int)i; play("pickup");
                }
            }
            EndDrawing();
            continue;
        }

        // ---------------------------------------------------------- SETTINGS
        if (state == State::SETTINGS) {
            if (IsKeyPressed(KEY_ESCAPE)) { play("ui"); state = State::MENU; rebinding = -1; }

            // capture a key when rebinding
            int* binds[7] = { &settings.keys.up, &settings.keys.down, &settings.keys.left,
                              &settings.keys.right, &settings.keys.run, &settings.keys.roll,
                              &settings.keys.interact };
            const char* bindNames[7] = { "Move Up", "Move Down", "Move Left", "Move Right",
                                         "Run", "Dodge-roll", "Interact" };
            if (rebinding >= 0) {
                int k = GetKeyPressed();
                if (k == KEY_ESCAPE) rebinding = -1;
                else if (k != 0) { *binds[rebinding] = k; rebinding = -1; play("ui"); }
            }

            BeginDrawing();
            ClearBackground({ 8, 10, 16, 255 });
            drawMenuBackground();
            DrawRectangle(0, 0, sw, sh, { 10, 8, 18, 215 });

            float pw = fminf(820.0f, sw - 80.0f), ph = fminf(710.0f, sh - 40.0f);
            Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
            DrawRectangleRec(panel, { 24, 18, 36, 245 });
            DrawRectangleLinesEx(panel, 2.0f, { 150, 120, 190, 255 });
            DrawText("Settings", (int)panel.x + 30, (int)panel.y + 22, 34, { 240, 205, 120, 255 });

            // --- Audio ---
            float ax = panel.x + 34, ay = panel.y + 78;
            DrawText("Audio", (int)ax, (int)ay, 24, { 200, 220, 255, 255 });
            auto slider = [&](float y, const char* label, float& val) {
                DrawText(label, (int)ax, (int)y, 18, RAYWHITE);
                Rectangle track = { ax + 160, y + 2, 360, 14 };
                DrawRectangleRec(track, { 40, 44, 60, 255 });
                DrawRectangleRec({ track.x, track.y, track.width * val, track.height }, { 90, 150, 240, 255 });
                Rectangle knob = { track.x + track.width * val - 7, track.y - 5, 14, 24 };
                DrawRectangleRec(knob, { 220, 225, 240, 255 });
                if (CheckCollisionPointRec(mouse, { track.x - 7, track.y - 6, track.width + 14, 28 }) &&
                    IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    val = (mouse.x - track.x) / track.width;
                    if (val < 0) val = 0; if (val > 1) val = 1;
                }
                DrawText(TextFormat("%d%%", (int)(val * 100)), (int)(track.x + track.width + 14), (int)y, 18, RAYWHITE);
            };
            slider(ay + 34, "Music", settings.musicVol);
            slider(ay + 68, "SFX", settings.sfxVol);

            // --- Screen ---
            float scx = panel.x + 34, scy = ay + 108;
            bool isFull = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            DrawText("Screen", (int)scx, (int)scy, 24, { 200, 220, 255, 255 });
            Rectangle fsBtn = { scx, scy + 34, 220, 38 };
            Rectangle wnBtn = { scx + 238, scy + 34, 220, 38 };
            Color onF{ 40, 90, 60, 255 }, offF{ 36, 40, 56, 255 }, brd{ 120, 150, 120, 255 };
            if (button(fsBtn, isFull ? "Fullscreen: ON" : "Fullscreen: OFF", 18, isFull ? onF : offF, brd, RAYWHITE, mouse)) {
                if (!isFull) ToggleBorderlessWindowed();
                play("ui");
            }
            if (button(wnBtn, "Windowed 1280x720", 18, !isFull ? onF : offF, brd, RAYWHITE, mouse)) {
                if (isFull) ToggleBorderlessWindowed();
                SetWindowSize(1280, 720);
                int mon = GetCurrentMonitor();
                SetWindowPosition((GetMonitorWidth(mon) - 1280) / 2, (GetMonitorHeight(mon) - 720) / 2);
                play("ui");
            }

            // --- Keybinds ---
            float kx = panel.x + 34, ky = scy + 96;
            DrawText("Keybinds", (int)kx, (int)ky, 24, { 200, 220, 255, 255 });
            DrawText("(click a key, then press a new key; Esc cancels)", (int)kx + 150, (int)ky + 6, 14, { 170,170,190,255 });
            for (int i = 0; i < 7; ++i) {
                float ry = ky + 38 + i * 34;
                DrawText(bindNames[i], (int)kx, (int)ry, 18, RAYWHITE);
                Rectangle keyBox = { kx + 200, ry - 4, 130, 28 };
                bool active = (rebinding == i);
                bool over = CheckCollisionPointRec(mouse, keyBox);
                DrawRectangleRec(keyBox, active ? Color{ 90, 70, 40, 255 } : Color{ 36, 40, 56, 255 });
                DrawRectangleLinesEx(keyBox, 2.0f, over ? Color{ 220,190,120,255 } : Color{ 90,90,120,255 });
                const char* kn = active ? "press a key..." : keyName(*binds[i]);
                DrawText(kn, (int)keyBox.x + 10, (int)ry, 16, active ? Color{ 255,220,140,255 } : RAYWHITE);
                if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { rebinding = i; play("ui"); }
            }

            // Back
            Rectangle backBtn = { panel.x + pw - 180, panel.y + ph - 64, 150, 46 };
            if (button(backBtn, "> Back", 22, Color{ 74, 42, 120, 235 }, Color{ 180, 150, 230, 255 }, RAYWHITE, mouse)) {
                play("ui"); state = State::MENU; rebinding = -1;
            }
            EndDrawing();
            continue;
        }

        // =========================================================== INTRO
        if (state == State::INTRO) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || t > 12.0f) {
                play("ui"); startGame(); state = State::PLAY; t = 0;
            }
            if (IsKeyPressed(KEY_ESCAPE)) { state = State::MENU; music.setMood("menu"); t = 0; }

            BeginDrawing();
            ClearBackground({ 6, 8, 14, 255 });
            DrawCircle(sw - 220, 150, 70, Fade({ 220, 226, 240, 255 }, 0.9f));
            for (int i = 0; i < 200; ++i) { int x = (i * 137) % sw; int y = (i * 89) % sh; DrawPixel(x, y, Fade(WHITE, 0.15f)); }
            DrawRectangle(0, sh - 160, sw, 160, { 8, 12, 14, 255 });
            for (int i = 0; i < 4; ++i) {
                float a = (t - i * 1.6f - 0.5f); float alpha = a <= 0 ? 0 : (a > 1 ? 1 : a);
                int w = MeasureText(PROPHECY[i], 30);
                DrawText(PROPHECY[i], (sw - w) / 2, 230 + i * 56, 30, Fade({ 230, 224, 210, 255 }, alpha));
            }
            if (t > 7.0f) { const char* go = "click to begin"; int gw = MeasureText(go, 20); DrawText(go, (sw - gw) / 2, sh - 90, 20, Fade(RAYWHITE, 0.4f + 0.4f * sinf(t * 3))); }
            EndDrawing();
            continue;
        }

        // ============================================================ PLAY
        // Camera frames the whole room, centered (Isaac-style fixed view).
        float zoom = fminf(sw / (ROOM_PX_W * 1.12f), sh / (ROOM_PX_H * 1.18f));
        Camera2D cam{}; cam.target = { ROOM_PX_W / 2, ROOM_PX_H / 2 }; cam.offset = { sw / 2.0f, sh / 2.0f }; cam.zoom = zoom;
        if (shake > 0 && !paused && !shopOpen) {
            shake = fmaxf(0, shake - dt);
            float amp = 14.0f * shake;
            cam.offset.x += frand(-amp, amp); cam.offset.y += frand(-amp, amp);
        }
        bool bossDark = dungeon.cur().isBoss && !dungeon.cur().cleared;
        gloom += ((bossDark ? 0.38f : 0.0f) - gloom) * fminf(1.0f, dt * 2.5f);
        Vector2 mouseWorld = GetScreenToWorld2D(mouse, cam);
        bool dialogueOpen = !dialogue.empty();
        Keybinds& K = settings.keys;
        bool doorsOpen = dungeon.cur().cleared;

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (shopOpen) shopOpen = false;
            else paused = !paused;
            play("ui");
        }
        bool nearMerchant = dungeon.cur().isShop && Vector2Distance(player.pos, merchantPos) < 56;
        bool nearOracle = dungeon.cur().isStart && Vector2Distance(player.pos, oraclePos) < 48;
        Room& room = dungeon.cur();
        Vector2 chestPos = { room.chestTx * (float)TILE + 16, room.chestTy * (float)TILE + 16 };
        bool nearChest = room.hasChest && !room.chestOpened && room.cleared &&
            Vector2Distance(player.pos, chestPos) < 40;
        bool spikesUp = fmodf(trapClock, 2.4f) < 1.2f;

        if (!paused && !shopOpen) {

            // --- tick timers, regen ---
            if (banner > 0) banner -= dt;
            player.castCd = fmaxf(0, player.castCd - dt);
            player.meleeCd = fmaxf(0, player.meleeCd - dt);
            player.meleeAnim = fmaxf(0, player.meleeAnim - dt * 4);
            player.rollCd = fmaxf(0, player.rollCd - dt);
            player.invuln = fmaxf(0, player.invuln - dt);
            player.hitFlash = fmaxf(0, player.hitFlash - dt);
            player.mana = fminf(player.maxMana, player.mana + (player.has(SK_ARCANE) ? 14.4f : 9.0f) * dt);
            for (int v = 0; v < VP_COUNT; ++v) {
                if (!player.buzzing((Vape)v)) continue;
                player.vapeT[v] = fmaxf(0, player.vapeT[v] - dt);
                if (frand(0, 1) < dt * 5)   // a lazy cloud drifting up off the player
                    parts.push_back({ { player.pos.x + frand(-6, 6), player.pos.y - 18 }, { frand(-12, 12), frand(-40, -20) },
                        1.0f, 1.0f, frand(2, 4), VAPE_COLOR[v] });
            }
            trapClock += dt;

            // --- hotbar ---
            if (IsKeyPressed(KEY_ONE)) { player.selected = 0; play("ui"); }
            if (IsKeyPressed(KEY_TWO)) { player.selected = 1; play("ui"); }
            if (IsKeyPressed(KEY_THREE)) { player.selected = 2; play("ui"); }

            // --- movement input (rebindable) ---
            Vector2 mv{ 0, 0 };
            if (!dialogueOpen) {
                if (IsKeyDown(K.up))    mv.y -= 1;
                if (IsKeyDown(K.down))  mv.y += 1;
                if (IsKeyDown(K.left))  mv.x -= 1;
                if (IsKeyDown(K.right)) mv.x += 1;
            }
            if (mv.x || mv.y) { mv = Vector2Normalize(mv); player.facing = mv; }
            Vector2 aim = Vector2Subtract(mouseWorld, player.pos);
            if (Vector2Length(aim) > 1) player.facing = Vector2Normalize(aim);

            // --- dodge-roll ---
            if (player.roll > 0) player.roll -= dt;
            if (!dialogueOpen && IsKeyPressed(K.roll) && player.rollCd <= 0 && (mv.x || mv.y)) {
                player.roll = 0.28f; player.rollCd = player.has(SK_DASH) ? 0.35f : 0.7f; player.invuln = 0.3f; player.rollDir = mv;
            }

            float speed = (IsKeyDown(K.run) ? RUN : WALK) * (player.has(SK_SWIFT) ? 1.15f : 1.0f)
                * (player.buzzing(VP_MANGO) ? 1.35f : 1.0f);
            Vector2 delta = (player.roll > 0) ? Vector2Scale(player.rollDir, ROLL_SPEED * dt) : Vector2Scale(mv, speed * dt);
            player.pos = moveInRoom(player.pos, delta, PLAYER_R, dungeon.cur(), doorsOpen);

            // damage the player takes is scaled by Iron Skin and the mint vape
            float hurtMul = (player.has(SK_IRON_SKIN) ? 0.75f : 1.0f) * (player.buzzing(VP_MINT) ? 0.5f : 1.0f);

            // --- spike traps hurt while raised ---
            if (spikesUp && player.roll <= 0 && player.invuln <= 0 && onSpikes(room, player.pos)) {
                player.hp -= 8 * hurtMul; player.hitFlash = 0.15f; player.invuln = 0.6f; play("hurt");
            }

            // --- open a chest (only once the room is safe) ---
            if (nearChest && !dialogueOpen && IsKeyPressed(K.interact)) {
                room.chestOpened = true; play("pickup"); chestOpenT = 0;
                bool mana = GetRandomValue(0, 1) == 0;
                popups.push_back({ mana ? &SPR.flaskBlue : &SPR.flaskRed, { chestPos.x, chestPos.y - 14 }, 0 });
                if (mana) {
                    player.maxMana += 10; player.mana = fminf(player.maxMana, player.mana + 10);
                    banner = 1.6f; bannerText = "Chest: +10 max mana";
                }
                else {
                    player.hp = fminf(player.maxHp, player.hp + 10);
                    banner = 1.6f; bannerText = "Chest: +10 health";
                }
                for (int k = 0; k < 12; ++k)
                    parts.push_back({ chestPos, {frand(-80,80),frand(-80,80)}, 0.6f, 0.6f, frand(2,4), {245,215,120,255} });
            }

            // --- talk to the oracle (she only stands in the start room) ---
            if (nearOracle && IsKeyPressed(K.interact)) {
                play("ui");
                if (!dialogueOpen) { dialogueIdx = 0; dialogue = oracleLines[0]; }
                else { dialogueIdx++; if (dialogueIdx >= (int)oracleLines.size()) dialogue.clear(); else dialogue = oracleLines[dialogueIdx]; }
            }

            // --- cast the selected power toward the cursor ---
            if (!dialogueOpen && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && player.castCd <= 0) {
                SpellDef& s = SPELLS[player.selected];
                int cost = player.buzzing(VP_RAZZ) ? 0 : s.manaCost;
                if (player.mana >= cost) {
                    player.mana -= cost; player.castCd = 0.28f; play(s.sfx);
                    Vector2 dir = Vector2Normalize(Vector2Subtract(mouseWorld, player.pos));
                    if (Vector2Length(dir) < 0.01f) dir = player.facing;
                    Projectile pr;
                    pr.pos = Vector2Add(player.pos, Vector2Scale(dir, 14));
                    pr.vel = Vector2Scale(dir, s.speed);
                    pr.damage = s.damage; pr.color = s.color; pr.glow = s.glow; pr.school = s.school;
                    pr.life = 1.4f; pr.r = (player.selected == 2) ? 4.0f : 6.0f;
                    shots.push_back(pr);
                }
            }

            // --- melee swing ---
            if (!dialogueOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && player.meleeCd <= 0) {
                player.meleeCd = 0.4f; player.meleeAnim = 1.0f; bool hitAny = false;
                for (auto& e : enemies) {
                    if (!e.alive) continue;
                    Vector2 to = Vector2Subtract(e.pos, player.pos);
                    float d = Vector2Length(to);
                    bool inArc = d < 40 + e.r && Vector2DotProduct(Vector2Normalize(to), player.facing) > 0.3f;
                    if (!inArc) continue;
                    e.hp -= (10 + player.level * 2 + (player.has(SK_BLADE) ? 8 : 0) + (player.buzzing(VP_MELON) ? 12 : 0)); e.hitFlash = 0.12f; hitAny = true;
                    if (player.has(SK_LEECH)) player.hp = fminf(player.maxHp, player.hp + 2);
                    for (int k = 0; k < 5; ++k)
                        parts.push_back({ e.pos, {frand(-60,60),frand(-60,60)}, 0.4f, 0.4f, frand(2,4), {234,255,255,255} });
                }
                if (hitAny) play("hit");
            }

            // --- projectiles ---
            for (auto& pr : shots) {
                if (!pr.alive) continue;
                pr.pos = Vector2Add(pr.pos, Vector2Scale(pr.vel, dt));
                pr.life -= dt;
                if (pr.life <= 0 || roomWallSolid(dungeon.cur(), (int)(pr.pos.x / TILE), (int)(pr.pos.y / TILE), doorsOpen)) { pr.alive = false; continue; }
                for (auto& e : enemies) {
                    if (!e.alive) continue;
                    if (Vector2Distance(pr.pos, e.pos) >= e.r + pr.r) continue;
                    int dmg = pr.damage;
                    if (e.weakness && pr.school && std::string(e.weakness) == pr.school) dmg = (int)(dmg * 1.6f);
                    e.hp -= dmg; e.hitFlash = 0.12f; pr.alive = false; play("hit");
                    for (int k = 0; k < 8; ++k)
                        parts.push_back({ pr.pos, {frand(-90,90),frand(-90,90)}, 0.5f, 0.5f, frand(2,5), pr.glow });
                    break;
                }
            }

            // --- enemy AI ---
            // One flow field per frame: every enemy reads its next step from it, so
            // they path around pillars and (while it's cheaper) around spike traps.
            buildFlowField(dungeon.cur(), player.pos, true, flow);
            for (auto& e : enemies) {
                if (!e.alive) continue;
                e.hitFlash = fmaxf(0, e.hitFlash - dt);
                e.atkCd = fmaxf(0, e.atkCd - dt);
                e.wobble += dt * 4;
                e.animT += dt;
                e.atkAnim = fmaxf(0, e.atkAnim - dt);
                Vector2 to = Vector2Subtract(player.pos, e.pos);
                float d = Vector2Length(to);
                if (fabsf(to.x) > 2) e.flip = to.x < 0;
                e.moving = d > 1 && e.atkAnim <= 0;        // plant feet while swinging
                e.lunge = fmaxf(0, e.lunge - dt);
                e.lungeCd = fmaxf(0, e.lungeCd - dt);
                if (e.moving) {
                    bool sees = lineClear(dungeon.cur(), e.pos, player.pos, e.r);
                    Vector2 goal;
                    if (!sees) goal = nextWaypoint(dungeon.cur(), flow, e.pos);   // route around walls
                    else if (e.kind == EK_SKELETON && d > 70) {
                        // foot soldiers fan out to the sides so they surround you
                        Vector2 n = Vector2Normalize(to);
                        Vector2 flank = Vector2Add(player.pos, Vector2Scale({ -n.y, n.x }, 40 * e.flankSide));
                        goal = lineClear(dungeon.cur(), e.pos, flank, e.r) ? flank : player.pos;
                    }
                    else goal = player.pos;
                    Vector2 dir = Vector2Normalize(Vector2Subtract(goal, e.pos));
                    if (e.kind == EK_SKULL) {
                        // wisps weave side to side, harder to hit with spells
                        Vector2 side = { -dir.y, dir.x };
                        dir = Vector2Normalize(Vector2Add(dir, Vector2Scale(side, sinf(e.wobble * 1.3f) * 0.8f)));
                    }
                    // hounds that see you crouch and lunge
                    if (e.kind == EK_SKELETON2 && sees && d < 130 && d > 40 && e.lungeCd <= 0) {
                        e.lunge = 0.35f; e.lungeCd = 2.5f;
                    }
                    float spd = e.speed * (e.lunge > 0 ? 2.6f : 1.0f);
                    // a wounded boss gets angry
                    if (e.boss && e.hp < e.maxHp * 0.4f) spd *= 1.5f;
                    e.pos = moveInRoom(e.pos, Vector2Scale(dir, spd * dt), e.r, dungeon.cur(), doorsOpen);
                }
                // solid bodies: push out of the player and out of other enemies
                {
                    Vector2 off = Vector2Subtract(e.pos, player.pos);
                    float dist = Vector2Length(off), minD = e.r + PLAYER_R;
                    if (dist < minD) {
                        Vector2 n = dist > 0.01f ? Vector2Scale(off, 1.0f / dist) : Vector2{ 1, 0 };
                        e.pos = moveInRoom(e.pos, Vector2Scale(n, minD - dist), e.r, dungeon.cur(), doorsOpen);
                    }
                    for (auto& o : enemies) {
                        if (&o == &e || !o.alive) continue;
                        Vector2 sep = Vector2Subtract(e.pos, o.pos);
                        float sd = Vector2Length(sep), need = e.r + o.r;
                        if (sd >= need) continue;
                        Vector2 n = sd > 0.01f ? Vector2Scale(sep, 1.0f / sd) : Vector2{ 0, 1 };
                        e.pos = moveInRoom(e.pos, Vector2Scale(n, (need - sd) * 0.5f), e.r, dungeon.cur(), doorsOpen);
                    }
                }
                if (d < e.r + PLAYER_R + 4 && e.atkCd <= 0 && player.invuln <= 0) {
                    player.hp -= e.dmg * hurtMul;
                    player.hitFlash = 0.15f; player.invuln = 0.4f; e.atkCd = 1.0f; play("hurt");
                    if (e.kind != EK_SKULL) e.atkAnim = SPR.enemy[e.kind].attack.length(16);
                }
                if (e.hp <= 0) {
                    e.alive = false; kills++; player.xp += e.xp; play("pickup");
                    Enemy c = e; c.dying = true; c.deathT = 0; c.hitFlash = 0; c.atkAnim = 0;
                    corpses.push_back(c);
                    for (int k = 0, n = e.boss ? 12 : 1 + GetRandomValue(0, 2); k < n; ++k)
                        coins.push_back({ e.pos, { frand(-90, 90), frand(-140, -40) }, 0 });
                    for (int k = 0; k < 14; ++k)
                        parts.push_back({ e.pos, {frand(-120,120),frand(-120,120)}, 0.7f, 0.7f, frand(2,5), e.color });
                    while (player.xp >= player.xpNext) {
                        player.xp -= player.xpNext; player.level++;
                        player.xpNext = (int)(60 * powf((float)player.level, 1.5f));
                        player.maxHp += 18; player.hp = player.maxHp;
                        player.maxMana += 10; player.mana = player.maxMana;
                        banner = 2.5f; bannerText = "The Unmarked grows stronger"; play("levelup");
                    }
                }
            }
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                [](const Enemy& e) { return !e.alive; }), enemies.end());
            shots.erase(std::remove_if(shots.begin(), shots.end(),
                [](const Projectile& p) { return !p.alive; }), shots.end());

            for (auto& pt : parts) { pt.pos = Vector2Add(pt.pos, Vector2Scale(pt.vel, dt)); pt.life -= dt; }
            for (auto& c : corpses) c.deathT += dt;
            corpses.erase(std::remove_if(corpses.begin(), corpses.end(), [](const Enemy& c) {
                float len = c.kind == EK_SKULL ? 0.5f : SPR.enemy[c.kind].death.length(14) + 0.6f;  // linger a moment
                return c.deathT > len; }), corpses.end());
            chestOpenT += dt;
            for (auto& pu : popups) { pu.t += dt; pu.pos.y -= 26 * dt; }
            popups.erase(std::remove_if(popups.begin(), popups.end(),
                [](const Popup& pu) { return pu.t > 1.4f; }), popups.end());
            for (auto& c : coins) {
                c.t += dt;
                if (c.t < 0.45f) {                          // pop out with a little gravity
                    c.vel.y += 360 * dt;
                    c.pos = Vector2Add(c.pos, Vector2Scale(c.vel, dt));
                }
                else {                                      // then get pulled to the player
                    Vector2 to = Vector2Subtract(player.pos, c.pos);
                    float spd = 160 + 600 * (c.t - 0.45f);
                    c.pos = Vector2Add(c.pos, Vector2Scale(Vector2Normalize(to), fminf(spd * dt, Vector2Length(to))));
                }
            }
            coins.erase(std::remove_if(coins.begin(), coins.end(), [&](const Coin& c) {
                if (c.t <= 0.45f || Vector2Distance(c.pos, player.pos) >= 8) return false;
                player.gold++; play("ui");
                return true; }), coins.end());
            parts.erase(std::remove_if(parts.begin(), parts.end(),
                [](const Particle& p) { return p.life <= 0; }), parts.end());

            // --- room cleared? open the doors ---
            if (inFight && enemies.empty()) {
                Room& r = dungeon.cur();
                r.cleared = true; inFight = false;
                banner = 1.6f; bannerText = r.isBoss ? "The Cinder-Maw falls!" : "Room cleared";
                play("quest");
            }

            // --- walk through an open door into the neighbouring room ---
            if (dungeon.cur().cleared) {
                Room& r = dungeon.cur();
                float dcx = (ROOM_TW / 2) * TILE + TILE * 0.5f;
                float dcy = (ROOM_TH / 2) * TILE + TILE * 0.5f;
                bool moved = false;
                if ((r.doors & DIR_N) && player.pos.y < TILE + 8 && fabsf(player.pos.x - dcx) < TILE) { dungeon.cy--; player.pos = { dcx, (ROOM_TH - 1) * TILE - 28.0f }; moved = true; }
                else if ((r.doors & DIR_S) && player.pos.y >(ROOM_TH - 1) * TILE - 8 && fabsf(player.pos.x - dcx) < TILE) { dungeon.cy++; player.pos = { dcx, TILE + 28.0f }; moved = true; }
                else if ((r.doors & DIR_W) && player.pos.x < TILE + 8 && fabsf(player.pos.y - dcy) < TILE) { dungeon.cx--; player.pos = { (ROOM_TW - 1) * TILE - 28.0f, dcy }; moved = true; }
                else if ((r.doors & DIR_E) && player.pos.x >(ROOM_TW - 1) * TILE - 8 && fabsf(player.pos.y - dcy) < TILE) { dungeon.cx++; player.pos = { TILE + 28.0f, dcy }; moved = true; }
                if (moved) { enterRoom(); continue; }
            }

            // --- step onto the staircase in a cleared boss room to descend ---
            if (dungeon.cur().isBoss && dungeon.cur().cleared) {
                Vector2 stairs = { ROOM_PX_W / 2, ROOM_PX_H / 2 };
                if (Vector2Distance(player.pos, stairs) < 18) {
                    floor++;
                    dungeon.generate();
                    player.hp = fminf(player.maxHp, player.hp + 30);   // a breather between floors
                    player.pos = { ROOM_PX_W / 2, ROOM_PX_H / 2 };
                    enterRoom();
                    banner = 2.5f; bannerText = TextFormat("Descend - Floor %d", floor);
                    play("ultimate");
                    continue;
                }
            }

            if (player.hp <= 0) {
                play("death");
                player.hp = player.maxHp; player.mana = player.maxMana;
                dungeon.cx = DUNGEON_G / 2; dungeon.cy = DUNGEON_G / 2;
                player.pos = { ROOM_PX_W / 2, ROOM_PX_H / 2 };
                player.invuln = 1.5f; banner = 2.2f; bannerText = "You fall... back to the entrance";
                enterRoom();
            }
            // --- talk to Marin to trade ---
            if (nearMerchant && !dialogueOpen && IsKeyPressed(K.interact)) { shopOpen = true; play("ui"); }
        } // !paused

        // ---- draw room ----
        // Wall torches sit at fixed spots (clear of the doors); they also light the room.
        struct TorchSpot { Vector2 pos; bool side; };
        std::vector<TorchSpot> torches;
        if (SPR.torch.ok()) {
            torches.push_back({ { 4 * (float)TILE + 16, 14 }, false });
            torches.push_back({ { (ROOM_TW - 5) * (float)TILE + 16, 14 }, false });
        }
        if (SPR.sideTorch.ok())
            for (int ty : { 2, ROOM_TH - 3 }) {
                torches.push_back({ { 18, ty * (float)TILE + 16 }, true });
                torches.push_back({ { ROOM_PX_W - 18, ty * (float)TILE + 16 }, true });
            }
        float spikePh = fmodf(trapClock, 2.4f);
        int spikeFrame = spikePh < 1.2f ? 0 : spikePh < 1.35f ? 1 : spikePh < 2.25f ? 2 : 3;

        BeginDrawing();
        ClearBackground({ 8, 8, 12, 255 });
        BeginMode2D(cam);
        {
            Room& r = dungeon.cur();
            int midX = ROOM_TW / 2, midY = ROOM_TH / 2;
            for (int ty = 0; ty < ROOM_TH; ++ty)
                for (int tx = 0; tx < ROOM_TW; ++tx) {
                    float px = tx * (float)TILE, py = ty * (float)TILE;
                    bool border = (tx == 0 || ty == 0 || tx == ROOM_TW - 1 || ty == ROOM_TH - 1);
                    int dir = 0;
                    if (ty == 0 && tx == midX) dir = DIR_N;
                    else if (ty == ROOM_TH - 1 && tx == midX) dir = DIR_S;
                    else if (tx == 0 && ty == midY) dir = DIR_W;
                    else if (tx == ROOM_TW - 1 && ty == midY) dir = DIR_E;
                    bool isDoor = dir && (r.doors & dir);

                    bool innerWall = !border && r.tiles[ty][tx] == RT_WALL;
                    unsigned h = (unsigned)(tx * 73856093) ^ (unsigned)(ty * 19349663) ^
                        (unsigned)(dungeon.cx * 83492791 + dungeon.cy * 2654435761u);
                    if (!SPR.tileset.id) {
                        // procedural fallback when the tileset is missing
                        if ((!border || isDoor) && !innerWall) {
                            Color f = ((tx + ty) & 1) ? Color{ 46, 44, 60, 255 } : Color{ 40, 38, 52, 255 };
                            DrawRectangle((int)px, (int)py, TILE, TILE, f);
                            if (isDoor && !r.cleared) DrawRectangle((int)px + 3, (int)py + 3, TILE - 6, TILE - 6, { 120, 86, 54, 255 });
                        }
                        else {
                            DrawRectangle((int)px, (int)py, TILE, TILE, { 24, 22, 32, 255 });
                            DrawRectangle((int)px, (int)py, TILE, 6, { 58, 54, 74, 255 });
                        }
                        if (!border && r.tiles[ty][tx] == RT_SPIKES && spikesUp)
                            DrawRectangle((int)px + 6, (int)py + 6, TILE - 12, TILE - 12, { 200, 200, 210, 255 });
                        continue;
                    }
                    if ((!border || isDoor) && !innerWall) {
                        drawTile(6 + h % 4, (h >> 4) % 3, px, py);          // stone floor variants
                        if (isDoor && !r.cleared) drawTile(8, 3, px, py);    // barred door
                        else if (!border && r.tiles[ty][tx] == RT_SPIKES)
                            drawAnim(SPR.peaks, spikeFrame, { px + TILE / 2.0f, py + TILE / 2.0f }, 2.0f);
                        // cobwebs in the top corners
                        if (ty == 1 && tx == 1 && !r.isBoss) drawTile(4, 6, px, py);
                        if (ty == 1 && tx == ROOM_TW - 2 && !r.isBoss) drawTile(5, 6, px, py);
                    }
                    else if (innerWall) drawTile(1 + (h >> 4) % 4, 0, px, py);   // brick block
                    else if (ty == 0) drawTile(tx == 0 ? 0 : tx == ROOM_TW - 1 ? 5 : 1 + tx % 4, 0, px, py);
                    else if (ty == ROOM_TH - 1) drawTile(tx == 0 ? 0 : tx == ROOM_TW - 1 ? 5 : 1 + tx % 4, 4, px, py);
                    else drawTile(tx == 0 ? 0 : 5, 1 + ty % 3, px, py);
                }

            // wall decorations: flickering torches (and banners in the boss lair)
            for (auto& tp : torches) {
                if (bossDark) drawTile(2, 9, tp.pos.x - 16, tp.pos.y - 16);     // empty sconce: unlit
                else if (tp.side) drawAnim(SPR.sideTorch, animFrame(SPR.sideTorch, t + tp.pos.x * 0.01f, 8, true), tp.pos, 2.0f, tp.pos.x > ROOM_PX_W / 2);
                else drawAnim(SPR.torch, animFrame(SPR.torch, t + tp.pos.x * 0.01f, 8, true), tp.pos, 2.0f);
            }
            if (r.isBoss)
                for (int i = 0; i < 4; ++i)
                    drawAnim(SPR.flag, animFrame(SPR.flag, t + i * 0.3f, 6, true),
                        { (2 + i * 5 + (i > 1 ? 2 : 0)) * (float)TILE + 16, 14 }, 2.0f);
            if (r.isStart) {
                drawAnim(SPR.candle, animFrame(SPR.candle, t, 7, true), { oraclePos.x - 34, oraclePos.y - 4 }, 2.0f);
                drawAnim(SPR.candle, animFrame(SPR.candle, t + 0.3f, 7, true), { oraclePos.x + 34, oraclePos.y - 4 }, 2.0f);
                drawAnim(SPR.candle2, animFrame(SPR.candle2, t + 0.5f, 7, true), { ROOM_PX_W - 3 * TILE, 2.5f * TILE }, 2.0f);
                drawAnim(SPR.candle2, animFrame(SPR.candle2, t + 0.1f, 7, true), { ROOM_PX_W - 3 * TILE, ROOM_PX_H - 2.5f * TILE }, 2.0f);
            }
        }
        if (room.hasChest) {
            float cx = chestPos.x, cy = chestPos.y;
            drawShadow({ cx, cy + 8 }, 12);
            if (SPR.chest.ok()) {
                if (room.chestOpened)
                    drawAnim(SPR.chestOpen, animFrame(SPR.chestOpen, chestOpenT, 10, false), { cx, cy }, 2.0f);
                else   // closed chests rattle now and then to draw the eye
                    drawAnim(SPR.chest, fmodf(t, 2.5f) < 0.5f ? animFrame(SPR.chest, t, 8, true) : 0, { cx, cy }, 2.0f);
            }
            else {
                DrawRectangle((int)cx - 11, (int)cy - 6, 22, 14, { 120, 78, 40, 255 });
                DrawRectangle((int)cx - 2, (int)cy - 6, 4, 5, { 230, 200, 90, 255 });
            }
            if (nearChest) DrawText("[E] open", (int)cx - 22, (int)cy - 28, 10, RAYWHITE);
        }
        for (auto& pt : parts) {
            Color c = pt.color; c.a = (unsigned char)(255 * (pt.life / pt.maxLife));
            DrawCircleV(pt.pos, pt.size, c);
        }
        if (dungeon.cur().isShop) {
            // rug, counter candles and Marin himself, gently bobbing
            Rectangle rug = { merchantPos.x - 70, merchantPos.y - 6, 140, 60 };
            DrawRectangleRec(rug, { 70, 34, 70, 255 });
            DrawRectangleLinesEx(rug, 3, { 200, 160, 80, 255 });
            drawAnim(SPR.candle, animFrame(SPR.candle, t, 7, true), { merchantPos.x - 54, merchantPos.y + 6 }, 2.0f);
            drawAnim(SPR.candle, animFrame(SPR.candle, t + 0.4f, 7, true), { merchantPos.x + 54, merchantPos.y + 6 }, 2.0f);
            drawShadow({ merchantPos.x, merchantPos.y + 14 }, 16);
            if (marinTex.id) {
                float hgt = 70, sc = hgt / marinTex.height, bob = sinf(t * 2.2f) * 2;
                DrawTextureEx(marinTex, { merchantPos.x - marinTex.width * sc / 2, merchantPos.y - hgt + 26 + bob }, 0, sc, WHITE);
            }
            else DrawCircleV(merchantPos, 14, { 90, 60, 160, 255 });
            if (nearMerchant && !shopOpen) DrawText("[E] trade", (int)merchantPos.x - 26, (int)merchantPos.y - 60, 10, RAYWHITE);
            for (int i = 0; i < 3; ++i)   // a few coins glinting on the rug
                drawAnim(SPR.coin, animFrame(SPR.coin, t + i * 0.25f, 10, true),
                    { merchantPos.x - 30 + i * 30.0f, merchantPos.y + 40 }, 1.2f);
        }
        if (dungeon.cur().isStart) {
            drawShadow(oraclePos, 12);
            if (SPR.oracle.ok())
                drawAnim(SPR.oracle, animFrame(SPR.oracle, t, 5, true), { oraclePos.x, oraclePos.y - 6 }, 2.2f,
                    player.pos.x < oraclePos.x);
            else {
                DrawRectangle((int)oraclePos.x - 7, (int)oraclePos.y - 12, 14, 22, { 202, 168, 232, 255 });
                DrawCircle((int)oraclePos.x, (int)oraclePos.y - 14, 6, { 244, 226, 250, 255 });
            }
            if (nearOracle && dialogue.empty()) DrawText("[E] talk", (int)oraclePos.x - 22, (int)oraclePos.y - 34, 10, RAYWHITE);
        }
        // staircase down, revealed once the boss room is cleared
        if (dungeon.cur().isBoss && dungeon.cur().cleared) {
            float sx = ROOM_PX_W / 2, sy = ROOM_PX_H / 2;
            DrawRectangle((int)sx - 30, (int)sy - 24, 60, 48, { 8, 8, 12, 255 });   // dark pit
            for (int i = 0; i < 4; ++i)                                              // descending steps
                DrawRectangle((int)sx - 26 + i * 3, (int)sy - 18 + i * 11, 52 - i * 6, 9,
                    { (unsigned char)(78 - i * 14), (unsigned char)(74 - i * 14), (unsigned char)(96 - i * 16), 255 });
            DrawRectangleLinesEx({ sx - 30, sy - 24, 60, 48 }, 2, { 150, 140, 90, 255 });
            if (Vector2Distance(player.pos, { sx, sy }) < 60)
                DrawText("Descend", (int)sx - 28, (int)sy - 40, 14, { 245, 220, 140, 255 });
        }
        for (auto& c : corpses) drawEnemy(c);
        for (auto& e : enemies) drawEnemy(e);
        for (auto& c : coins)
            drawAnim(SPR.coin, animFrame(SPR.coin, c.t, 12, true), c.pos, 1.4f);
        for (auto& pr : shots) {
            if (SPR.flame.ok() && pr.school && std::string(pr.school) == "Fire") {
                // Emberhex flies as an animated fireball, rotated along its path
                float ang = atan2f(pr.vel.y, pr.vel.x) * RAD2DEG + 180.0f;
                DrawCircleV(pr.pos, pr.r + 5, Fade(pr.glow, 0.25f));
                drawAnim(SPR.flame, animFrame(SPR.flame, 1.4f - pr.life, 14, true), pr.pos, 1.3f, false, WHITE, ang);
            }
            else { DrawCircleV(pr.pos, pr.r + 3, Fade(pr.glow, 0.4f)); DrawCircleV(pr.pos, pr.r, pr.color); }
        }
        for (auto& pu : popups)
            drawAnim(*pu.anim, animFrame(*pu.anim, pu.t, 8, true), pu.pos, 2.0f, false, Fade(WHITE, fminf(1.0f, 1.4f - pu.t)));
        drawHero(player.pos, player.facing, player.meleeAnim, player.hitFlash > 0);
        EndMode2D();

        // ---- boss lair gloom: the room darkens while the boss lives ----
        if (gloom > 0.001f) DrawRectangle(0, 0, sw, sh, Fade({ 4, 2, 10, 255 }, gloom));
        DrawRectangle(0, 0, sw, sh, Fade(FOG_TINT, 0.04f));

        // ---- HUD ----
        DrawRectangle(20, 20, 244, 22, { 20,10,10,200 });
        DrawRectangle(22, 22, (int)(240 * (player.hp / player.maxHp)), 18, { 200, 60, 60, 255 });
        DrawText(TextFormat("HP %d/%d", (int)player.hp, (int)player.maxHp), 28, 25, 12, RAYWHITE);
        DrawRectangle(20, 48, 244, 18, { 10,14,28,200 });
        DrawRectangle(22, 50, (int)(240 * (player.mana / player.maxMana)), 14, { 80, 140, 240, 255 });
        DrawText(TextFormat("Mana %d/%d", (int)player.mana, (int)player.maxMana), 28, 50, 11, RAYWHITE);
        DrawText(TextFormat("Lv %d   XP %d/%d   Floor %d   Kills %d", player.level, player.xp, player.xpNext, floor, kills), 20, 72, 14, { 220,210,180,255 });
        drawAnim(SPR.coin, animFrame(SPR.coin, t, 8, true), { 30, 102 }, 1.3f);
        DrawText(TextFormat("%d", player.gold), 44, 95, 18, { 245, 215, 120, 255 });
        // active vapes: icon + seconds left
        for (int v = 0, n = 0; v < VP_COUNT; ++v) {
            if (!player.buzzing((Vape)v)) continue;
            float vx = 30.0f + n++ * 56;
            if (SPR.vapes.ok()) drawAnim(SPR.vapes, v, { vx, 140 }, 1.0f);
            DrawText(TextFormat("%ds", (int)ceilf(player.vapeT[v])), (int)vx + 10, 132, 16, VAPE_COLOR[v]);
        }
        for (int i = 0; i < 3; ++i) {
            int x = 20 + i * 70, y = sh - 80;
            bool sel = (i == player.selected);
            Color bd = sel ? Color{ 255, 220, 140, 255 } : Color{ 90, 90, 110, 255 };
            DrawRectangle(x, y, 62, 62, { 16, 18, 24, 220 });
            DrawRectangleLinesEx({ (float)x, (float)y, 62, 62 }, sel ? 3.0f : 1.0f, bd);
            if (SPR.spellIcons.ok()) {   // greyed out while you can't afford the cast
                bool castable = player.buzzing(VP_RAZZ) || player.mana >= SPELLS[i].manaCost;
                drawAnim(SPR.spellIcons, i, { x + 31.0f, y + 24.0f }, 1.1f, false, castable ? WHITE : Color{ 90, 90, 100, 255 });
            }
            else DrawRectangle(x + 8, y + 8, 46, 24, SPELLS[i].color);
            DrawText(TextFormat("%d", i + 1), x + 4, y + 2, 12, RAYWHITE);
            DrawText(SPELLS[i].name, x - 2, y + 44, 10, RAYWHITE);
        }
        DrawText("LMB blade  RMB cast  1-3 power  E open/talk/trade  Esc pause", 20, sh - 16, 12, { 180,190,200,220 });

        // ---- minimap (top-right) ----
        {
            int cell = 16, gap = 3;
            int mw = DUNGEON_G * (cell + gap) - gap;
            int ox = sw - mw - 20, oy = 20;
            DrawRectangle(ox - 8, oy - 8, mw + 16, mw + 16, { 10, 10, 16, 180 });
            for (int y = 0; y < DUNGEON_G; ++y)
                for (int x = 0; x < DUNGEON_G; ++x) {
                    Room& rm = dungeon.grid[y][x];
                    if (!rm.exists) continue;
                    bool isCur = (x == dungeon.cx && y == dungeon.cy);
                    Color c;
                    if (rm.isBoss) c = rm.visited ? Color{ 210, 90, 80, 255 } : Color{ 110, 54, 50, 255 };
                    else if (rm.isShop && rm.visited && !isCur) c = { 220, 180, 70, 255 };
                    else if (isCur) c = { 255, 220, 120, 255 };
                    else if (rm.visited) c = { 120, 124, 150, 255 };
                    else c = { 56, 58, 78, 255 };
                    int rx = ox + x * (cell + gap), ry = oy + y * (cell + gap);
                    DrawRectangle(rx, ry, cell, cell, c);
                    if (isCur) DrawRectangleLinesEx({ (float)rx - 1, (float)ry - 1, cell + 2.0f, cell + 2.0f }, 2, RAYWHITE);
                }
        }

        if (!dialogue.empty()) {
            DrawRectangle(0, sh - 160, sw, 100, { 10, 12, 18, 230 });
            DrawRectangleLines(0, sh - 160, sw, 100, { 120, 100, 150, 255 });
            DrawText("Theira, the Last Oracle", 30, sh - 150, 18, { 202, 168, 232, 255 });
            DrawText(dialogue.c_str(), 30, sh - 120, 18, RAYWHITE);
            DrawText("[E] continue", sw - 140, sh - 80, 14, { 180,180,200,255 });
        }
        if (banner > 0) {
            int w = MeasureText(bannerText.c_str(), 40);
            DrawText(bannerText.c_str(), (sw - w) / 2, 120, 40, Fade(RAYWHITE, fminf(1.0f, banner)));
        }

        // ---- Marin's shop window ----
        if (shopOpen) {
            if (shopMsg > 0) shopMsg -= GetFrameTime();
            DrawRectangle(0, 0, sw, sh, { 6, 6, 12, 170 });
            float pw = fminf(1000.0f, sw - 60.0f), ph = fminf(660.0f, sh - 40.0f);
            Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
            DrawRectangleRec(panel, { 24, 18, 36, 250 });
            // shop art across the top, cover-cropped
            float artH = ph * 0.32f;
            if (shopTex.id) {
                float sc = fmaxf(pw / shopTex.width, artH / shopTex.height);
                float cw = pw / sc, ch = artH / sc;
                DrawTexturePro(shopTex, { (shopTex.width - cw) / 2, (shopTex.height - ch) / 2, cw, ch },
                    { panel.x, panel.y, pw, artH }, { 0, 0 }, 0, WHITE);
            }
            DrawRectangleGradientV((int)panel.x, (int)(panel.y + artH - 60), (int)pw, 60, BLANK, { 24, 18, 36, 250 });
            if (marinTex.id) {
                float hgt = artH * 0.95f, sc = hgt / marinTex.height;
                DrawTextureEx(marinTex, { panel.x + 16, panel.y + artH - hgt + 30 }, 0, sc, WHITE);
            }
            DrawText("Marin the Wizard", (int)(panel.x + pw * 0.30f), (int)(panel.y + artH - 44), 30, { 240, 205, 120, 255 });
            DrawText("\"Potions, powers, a little spice... all for honest gold.\"",
                (int)(panel.x + pw * 0.30f), (int)(panel.y + artH - 10), 16, { 210, 200, 225, 255 });
            drawAnim(SPR.coin, animFrame(SPR.coin, t, 8, true), { panel.x + pw - 110, panel.y + artH - 30 }, 2.0f);
            DrawText(TextFormat("%d", player.gold), (int)(panel.x + pw - 90), (int)(panel.y + artH - 42), 26, { 245, 215, 120, 255 });

            Rectangle xBtn = { panel.x + pw - 46, panel.y + 10, 34, 34 };
            if (button(xBtn, "X", 22, Color{ 60, 40, 80, 255 }, Color{ 180, 150, 220, 255 }, RAYWHITE, mouse)) { play("ui"); shopOpen = false; }

            // two columns of wares
            float gx = panel.x + 24, gy = panel.y + artH + 26, colW = (pw - 72) / 2, rowH = 52;
            for (int i = 0; i < SHOP_COUNT; ++i) {
                const ShopItem& it = SHOP_ITEMS[i];
                Rectangle row = { gx + (i % 2) * (colW + 24), gy + (i / 2) * (rowH + 8), colW, rowH };
                bool owned = it.skill && (player.skills & it.skill);
                DrawRectangleRec(row, owned ? Color{ 30, 44, 34, 255 } : Color{ 34, 28, 48, 255 });
                DrawRectangleLinesEx(row, 1, { 90, 80, 120, 255 });
                // icon: vapes their own art, potions the flask animations, skills a glowing rune
                Vector2 ic = { row.x + 28, row.y + rowH / 2 };
                if (it.vape >= 0 && SPR.vapes.ok()) drawAnim(SPR.vapes, it.vape, ic, 1.4f);
                else if (!it.skill) drawAnim(i == 0 ? SPR.flaskRed : SPR.flaskBlue, animFrame(SPR.flaskRed, t, 6, true), ic, 2.4f);
                else {
                    DrawCircleV(ic, 16, Fade({ 150, 110, 230, 255 }, 0.35f + 0.15f * sinf(t * 3 + i)));
                    DrawCircleLinesV(ic, 16, { 200, 170, 255, 255 });
                    DrawText(TextFormat("%c", it.name[0]), (int)ic.x - 6, (int)ic.y - 10, 20, RAYWHITE);
                }
                DrawText(it.name, (int)row.x + 54, (int)row.y + 7, 20, RAYWHITE);
                DrawText(it.desc, (int)row.x + 54, (int)row.y + 31, 14, { 180, 175, 200, 255 });
                Rectangle buy = { row.x + colW - 100, row.y + 12, 88, rowH - 24 };
                if (owned) DrawText("Learned", (int)buy.x + 8, (int)buy.y + 8, 18, { 140, 220, 160, 255 });
                else {
                    bool afford = player.gold >= it.price;
                    if (button(buy, TextFormat("%d g", it.price), 18, afford ? Color{ 90, 70, 30, 255 } : Color{ 50, 44, 50, 255 },
                        afford ? Color{ 230, 190, 100, 255 } : Color{ 100, 90, 100, 255 }, afford ? RAYWHITE : Color{ 140,130,140,255 }, mouse)) {
                        if (!afford) { shopMsg = 1.5f; shopText = "Not enough gold"; play("hurt"); }
                        else {
                            player.gold -= it.price;
                            if (it.skill) { player.skills |= it.skill; play("levelup"); }
                            else if (it.vape >= 0) { player.vapeT[it.vape] = VAPE_TIME[it.vape]; play("pickup"); }
                            else {
                                if (i == 0) player.hp = fminf(player.maxHp, player.hp + 35);
                                else player.mana = fminf(player.maxMana, player.mana + 50);
                                play("pickup");
                            }
                            shopMsg = 1.5f; shopText = std::string("Bought ") + it.name;
                        }
                    }
                }
            }
            if (shopMsg > 0) {
                int mw = MeasureText(shopText.c_str(), 22);
                DrawText(shopText.c_str(), (int)(panel.x + (pw - mw) / 2), (int)(panel.y + ph - 34), 22, Fade({ 245, 215, 120, 255 }, fminf(1.0f, shopMsg)));
            }
            DrawText(TextFormat("HP %d/%d   Mana %d/%d", (int)player.hp, (int)player.maxHp, (int)player.mana, (int)player.maxMana),
                (int)panel.x + 24, (int)(panel.y + ph - 30), 16, { 200, 195, 210, 255 });
        }

        // ---- pause menu ----
        bool toMenu = false;
        if (paused) {
            if (saveMsg > 0) saveMsg -= GetFrameTime();
            DrawRectangle(0, 0, sw, sh, { 6, 6, 12, 190 });
            const char* ttl = "Paused";
            int tw = MeasureText(ttl, 48);
            DrawText(ttl, (sw - tw) / 2, sh / 2 - 170, 48, { 240, 205, 120, 255 });
            float bw = 260, bh = 54, gap = 16, bx = (sw - bw) / 2, by = sh / 2 - 80.0f;
            Color pf{ 74, 42, 120, 235 }, pb{ 180, 150, 230, 255 };
            if (button({ bx, by, bw, bh }, "Resume", 26, pf, pb, RAYWHITE, mouse)) { play("ui"); paused = false; }
            if (button({ bx, by + (bh + gap), bw, bh }, "Save", 26, pf, pb, RAYWHITE, mouse)) {
                static SaveData d;
                d = SaveData{}; d.player = player; d.dungeon = dungeon;
                d.floor = floor; d.kills = kills; d.champion = selectedChampion;
                bool ok = writeSave(d);
                hasSave = hasSave || ok;
                saveMsg = 2.0f; saveText = ok ? "Game saved" : "Save failed";
                play(ok ? "quest" : "hurt");
            }
            if (button({ bx, by + 2 * (bh + gap), bw, bh }, "Main Menu", 26, Color{ 96, 36, 44, 235 }, Color{ 230, 140, 150, 255 }, RAYWHITE, mouse)) {
                play("ui"); toMenu = true;
            }
            if (saveMsg > 0) {
                int mw = MeasureText(saveText, 22);
                DrawText(saveText, (sw - mw) / 2, (int)(by + 3 * (bh + gap) + 6), 22, Fade({ 160, 230, 170, 255 }, fminf(1.0f, saveMsg)));
            }
        }
        EndDrawing();
        if (toMenu) { paused = false; state = State::MENU; music.setMood("menu"); t = 0; }
    }

    SPR.unload();
    if (marinTex.id) UnloadTexture(marinTex);
    if (shopTex.id) UnloadTexture(shopTex);
    if (menuBg.id) UnloadTexture(menuBg);
    if (castleTex.id) UnloadTexture(castleTex);
    for (auto& tx : champTex) if (tx.id) UnloadTexture(tx);
    for (auto& [k, s] : SFX) UnloadSound(s);
    music.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
