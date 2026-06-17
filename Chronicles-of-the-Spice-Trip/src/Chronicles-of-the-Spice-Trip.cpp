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

#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>

enum class State { MENU, CHARSELECT, SETTINGS, INTRO, PLAY };

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
    int lightW = GetScreenWidth(), lightH = GetScreenHeight();
    RenderTexture2D lightTex = LoadRenderTexture(lightW, lightH);
    Vector2 oraclePos{ ROOM_PX_W / 2 - 90, ROOM_PX_H / 2 - 40 };
    std::vector<std::string> oracleLines = {
        "So. The Veil spat you out at last.",
        "Press 1 and right-click to loose an Emberhex on the mistlings.",
        "Clear a room to open its doors. The boss waits at the far end." };
    std::string dialogue; int dialogueIdx = 0;
    int kills = 0; float banner = 0; int floor = 1;
    Dungeon dungeon; bool inFight = false; std::string bannerText = "The Mistwood Depths";

    auto spawnEnemy = [&](const char* name, float hp, float spd, int dmg, int xp,
        float r, Color col, const char* weak, bool boss, Vector2 at) {
        Enemy e;
        e.name = name; e.hp = e.maxHp = hp; e.speed = spd; e.dmg = dmg; e.xp = xp;
        e.r = r; e.color = col; e.weakness = weak; e.boss = boss; e.pos = at;
        e.wobble = frand(0, 6.28f);
        enemies.push_back(e);
    };
    // A random spot inside the current room (away from the walls).
    auto interiorPoint = [&]() -> Vector2 {
        int tx = GetRandomValue(3, ROOM_TW - 4);
        int ty = GetRandomValue(3, ROOM_TH - 4);
        return { tx * (float)TILE + 16, ty * (float)TILE + 16 };
    };
    // Load the current room: clear actors, then spawn its enemies if uncleared.
    auto enterRoom = [&]() {
        enemies.clear(); shots.clear(); parts.clear();
        Room& r = dungeon.cur();
        r.visited = true;
        inFight = false;
        if (r.cleared) return;
        if (r.isBoss) {
            spawnEnemy("Pyraketh, the Cinder-Maw", 340, 55, 30, 150, 24,
                { 106,42,31,255 }, "Water", true, { ROOM_PX_W / 2, ROOM_PX_H / 2 });
            play("ultimate"); banner = 2.2f; bannerText = "Pyraketh, the Cinder-Maw, wakes";
        }
        else {
            int m = 3 + GetRandomValue(0, 2);
            for (int i = 0; i < m; ++i)
                spawnEnemy("Mistling", 30, 60, 6, 12, 12, { 138,168,154,255 }, "Fire", false, interiorPoint());
            for (int i = 0, h = GetRandomValue(0, 1); i < h; ++i)
                spawnEnemy("Hollow Hound", 70, 110, 14, 26, 13, { 42,31,46,255 }, "Fire", false, interiorPoint());
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
            Rectangle charBtn = { bx, sh - 40.0f - 3 * bh - 2 * gap, bw, bh };
            Rectangle setBtn = { bx, sh - 40.0f - 2 * bh - 1 * gap, bw, bh };
            Rectangle exitBtn = { bx, sh - 40.0f - 1 * bh,           bw, bh };

            bool overUi = CheckCollisionPointRec(mouse, charBtn) ||
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
        // recreate the light mask if the window was resized
        if (sw != lightW || sh != lightH) {
            UnloadRenderTexture(lightTex);
            lightW = sw; lightH = sh;
            lightTex = LoadRenderTexture(lightW, lightH);
        }

        // Camera frames the whole room, centered (Isaac-style fixed view).
        float zoom = fminf(sw / (ROOM_PX_W * 1.12f), sh / (ROOM_PX_H * 1.18f));
        Camera2D cam{}; cam.target = { ROOM_PX_W / 2, ROOM_PX_H / 2 }; cam.offset = { sw / 2.0f, sh / 2.0f }; cam.zoom = zoom;
        Vector2 mouseWorld = GetScreenToWorld2D(mouse, cam);
        bool dialogueOpen = !dialogue.empty();
        Keybinds& K = settings.keys;
        bool doorsOpen = dungeon.cur().cleared;

        if (IsKeyPressed(KEY_ESCAPE)) { state = State::MENU; music.setMood("menu"); t = 0; play("ui"); BeginDrawing(); EndDrawing(); continue; }

        // --- tick timers, regen ---
        if (banner > 0) banner -= dt;
        player.castCd = fmaxf(0, player.castCd - dt);
        player.meleeCd = fmaxf(0, player.meleeCd - dt);
        player.meleeAnim = fmaxf(0, player.meleeAnim - dt * 4);
        player.rollCd = fmaxf(0, player.rollCd - dt);
        player.invuln = fmaxf(0, player.invuln - dt);
        player.hitFlash = fmaxf(0, player.hitFlash - dt);
        player.mana = fminf(player.maxMana, player.mana + 9 * dt);
        player.hp = fminf(player.maxHp, player.hp + 1.2f * dt);

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
            player.roll = 0.28f; player.rollCd = 0.7f; player.invuln = 0.3f; player.rollDir = mv;
        }

        float speed = (IsKeyDown(K.run) ? RUN : WALK);
        Vector2 delta = (player.roll > 0) ? Vector2Scale(player.rollDir, ROLL_SPEED * dt) : Vector2Scale(mv, speed * dt);
        player.pos = moveInRoom(player.pos, delta, PLAYER_R, dungeon.cur(), doorsOpen);

        // --- talk to the oracle (she only stands in the start room) ---
        bool nearOracle = dungeon.cur().isStart && Vector2Distance(player.pos, oraclePos) < 48;
        if (nearOracle && IsKeyPressed(K.interact)) {
            play("ui");
            if (!dialogueOpen) { dialogueIdx = 0; dialogue = oracleLines[0]; }
            else { dialogueIdx++; if (dialogueIdx >= (int)oracleLines.size()) dialogue.clear(); else dialogue = oracleLines[dialogueIdx]; }
        }

        // --- cast the selected power toward the cursor ---
        if (!dialogueOpen && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && player.castCd <= 0) {
            SpellDef& s = SPELLS[player.selected];
            if (player.mana >= s.manaCost) {
                player.mana -= s.manaCost; player.castCd = 0.28f; play(s.sfx);
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
                e.hp -= (10 + player.level * 2); e.hitFlash = 0.12f; hitAny = true;
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
        for (auto& e : enemies) {
            if (!e.alive) continue;
            e.hitFlash = fmaxf(0, e.hitFlash - dt);
            e.atkCd = fmaxf(0, e.atkCd - dt);
            e.wobble += dt * 4;
            Vector2 to = Vector2Subtract(player.pos, e.pos);
            float d = Vector2Length(to);
            if (d > 1) {
                Vector2 step = Vector2Scale(Vector2Normalize(to), e.speed * dt);
                e.pos = moveInRoom(e.pos, step, e.r, dungeon.cur(), doorsOpen);
            }
            if (d < e.r + PLAYER_R + 4 && e.atkCd <= 0 && player.invuln <= 0) {
                player.hp -= e.dmg; player.hitFlash = 0.15f; player.invuln = 0.4f; e.atkCd = 1.0f; play("hurt");
            }
            if (e.hp <= 0) {
                e.alive = false; kills++; player.xp += e.xp; play("pickup");
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

        // ---- draw room ----
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

                    if (!border || isDoor) {
                        // floor (checker stone)
                        Color f = ((tx + ty) & 1) ? Color{ 46, 44, 60, 255 } : Color{ 40, 38, 52, 255 };
                        DrawRectangle((int)px, (int)py, TILE, TILE, f);
                        if (isDoor && !r.cleared) {
                            // closed door panel
                            DrawRectangle((int)px + 3, (int)py + 3, TILE - 6, TILE - 6, { 120, 86, 54, 255 });
                            DrawRectangleLinesEx({ px + 3, py + 3, TILE - 6.0f, TILE - 6.0f }, 2, { 70, 50, 32, 255 });
                        }
                    }
                    else {
                        // wall block with a lighter top edge
                        DrawRectangle((int)px, (int)py, TILE, TILE, { 24, 22, 32, 255 });
                        DrawRectangle((int)px, (int)py, TILE, 6, { 58, 54, 74, 255 });
                    }
                }
        }
        for (auto& pt : parts) {
            Color c = pt.color; c.a = (unsigned char)(255 * (pt.life / pt.maxLife));
            DrawCircleV(pt.pos, pt.size, c);
        }
        if (dungeon.cur().isStart) {
            drawShadow(oraclePos, 12);
            DrawRectangle((int)oraclePos.x - 7, (int)oraclePos.y - 12, 14, 22, { 202, 168, 232, 255 });
            DrawCircle((int)oraclePos.x, (int)oraclePos.y - 14, 6, { 244, 226, 250, 255 });
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
        for (auto& e : enemies) drawEnemy(e);
        for (auto& pr : shots) { DrawCircleV(pr.pos, pr.r + 3, Fade(pr.glow, 0.4f)); DrawCircleV(pr.pos, pr.r, pr.color); }
        drawHero(player.pos, player.facing, player.meleeAnim, player.hitFlash > 0);
        EndMode2D();

        // ---- lighting: gentle gloom with a torch pool (rooms stay readable) ----
        BeginTextureMode(lightTex);
        ClearBackground({ 6, 8, 12, (unsigned char)(0.40f * 255) });
        BeginBlendMode(BLEND_SUBTRACT_COLORS);
        {
            Vector2 pS = GetWorldToScreen2D(player.pos, cam);
            DrawCircleGradient((int)pS.x, (int)pS.y, (int)(fmaxf(sw, sh) * 0.55f), WHITE, BLANK);
            for (auto& pr : shots) {
                Vector2 s = GetWorldToScreen2D(pr.pos, cam);
                DrawCircleGradient((int)s.x, (int)s.y, 90, WHITE, BLANK);
            }
        }
        EndBlendMode();
        EndTextureMode();
        DrawTextureRec(lightTex.texture, { 0, 0, (float)lightW, -(float)lightH }, { 0, 0 }, WHITE);
        DrawRectangle(0, 0, sw, sh, Fade(FOG_TINT, 0.04f));

        // ---- HUD ----
        DrawRectangle(20, 20, 244, 22, { 20,10,10,200 });
        DrawRectangle(22, 22, (int)(240 * (player.hp / player.maxHp)), 18, { 200, 60, 60, 255 });
        DrawText(TextFormat("HP %d/%d", (int)player.hp, (int)player.maxHp), 28, 25, 12, RAYWHITE);
        DrawRectangle(20, 48, 244, 18, { 10,14,28,200 });
        DrawRectangle(22, 50, (int)(240 * (player.mana / player.maxMana)), 14, { 80, 140, 240, 255 });
        DrawText(TextFormat("Mana %d/%d", (int)player.mana, (int)player.maxMana), 28, 50, 11, RAYWHITE);
        DrawText(TextFormat("Lv %d   XP %d/%d   Floor %d   Kills %d", player.level, player.xp, player.xpNext, floor, kills), 20, 72, 14, { 220,210,180,255 });
        for (int i = 0; i < 3; ++i) {
            int x = 20 + i * 70, y = sh - 80;
            bool sel = (i == player.selected);
            Color bd = sel ? Color{ 255, 220, 140, 255 } : Color{ 90, 90, 110, 255 };
            DrawRectangle(x, y, 62, 62, { 16, 18, 24, 220 });
            DrawRectangleLinesEx({ (float)x, (float)y, 62, 62 }, sel ? 3.0f : 1.0f, bd);
            DrawRectangle(x + 8, y + 8, 46, 24, SPELLS[i].color);
            DrawText(TextFormat("%d", i + 1), x + 4, y + 2, 12, RAYWHITE);
            DrawText(SPELLS[i].name, x - 2, y + 44, 10, RAYWHITE);
        }
        DrawText("LMB blade  RMB cast  1-3 power  Esc menu", 20, sh - 16, 12, { 180,190,200,220 });

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
        EndDrawing();
    }

    UnloadRenderTexture(lightTex);
    if (menuBg.id) UnloadTexture(menuBg);
    if (castleTex.id) UnloadTexture(castleTex);
    for (auto& tx : champTex) if (tx.id) UnloadTexture(tx);
    for (auto& [k, s] : SFX) UnloadSound(s);
    music.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
