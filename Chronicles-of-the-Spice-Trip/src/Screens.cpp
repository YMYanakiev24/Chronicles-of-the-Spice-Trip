// ============================================================================
//  Screens.cpp — the main menu, champion select, settings and the intro.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Touch.h"
#include "Render.h"
#include "Ui.h"
#include <bit>
#include <cmath>

static const char* PROPHECY[4] = {
    "When the Veil grows thin and the old powers stir,",
    "an Unmarked child shall take the godless blade.",
    "Through cursed wood and drowned and hollow hall,",
    "they will wake what sleeps -- and be claimed, or unmade.",
};

// The menu art is cover-scaled and anchored to the top, so the painted title is never cut off.
void Game::drawMenuBackground() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (menuBg.id) {
        float sc = fmaxf((float)sw / menuBg.width, (float)sh / menuBg.height);
        DrawTextureEx(menuBg, { (sw - menuBg.width * sc) / 2, 0 }, 0, sc, WHITE);
    }
    else ui::textCentered("CHRONICLES OF THE SPICE TRIP", sw / 2.0f, 60, 40, ui::ACCENT);
}

// The castle hotspot, as fractions of the displayed menu art.
Rectangle Game::castleRect() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float sc = menuBg.id ? fmaxf((float)sw / menuBg.width, (float)sh / menuBg.height) : 1.0f;
    float w = menuBg.id ? menuBg.width * sc : (float)sw, h = menuBg.id ? menuBg.height * sc : (float)sh;
    float x = (sw - w) / 2.0f;
    return { x + w * 0.625f, h * 0.010f, w * 0.200f, h * 0.360f };
}

// A champion's portrait, cropped from their sheet (or their walking figure, or the procedural hero).
static void drawPortrait(const Game& g, int i, Rectangle dst) {
    const ChampionDef& c = CHAMPIONS[i];
    Texture2D tx = g.champTex[i];
    if (tx.id) ui::drawContain(tx, { c.crop.x * tx.width, c.crop.y * tx.height, c.crop.width * tx.width, c.crop.height * tx.height }, dst);
    else if (g.walk[i].ok()) {   // Spellwoven has no sheet: their figure, standing, facing you
        DrawRectangleRec(dst, { 14, 16, 26, 255 });
        drawFigureIn(g.walk[i], FACE_DOWN, 0, { dst.x + dst.width * 0.15f, dst.y + dst.height * 0.08f, dst.width * 0.7f, dst.height * 0.84f });
    }
    else {                  // no figure either: the hero drawn in code, clipped to the frame
        DrawRectangleRec(dst, { 14, 16, 26, 255 });
        float s = fminf(dst.width, dst.height) / 44.0f;
        Camera2D cam{}; cam.zoom = s; cam.offset = { dst.x + dst.width / 2, dst.y + dst.height / 2 + 4 * s };
        BeginScissorMode((int)dst.x, (int)dst.y, (int)dst.width, (int)dst.height);
        BeginMode2D(cam);
        drawHero({ 0, 0 }, { 0.7f, 0.7f }, 0, false, c.cloak);
        EndMode2D();
        EndScissorMode();
    }
}

// ------------------------------------------------------------------ the menu
void Game::menuScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle castle = castleRect();
    bool overCastle = ui::hovered(castle);

    drawMenuBackground();
    if (castleTex.id) {
        float csc = fminf(castle.width / castleTex.width, castle.height / castleTex.height);
        float cw = castleTex.width * csc, ch = castleTex.height * csc;
        DrawTextureEx(castleTex, { castle.x + (castle.width - cw) / 2, castle.y + (castle.height - ch) / 2 }, 0, csc,
            overCastle ? WHITE : Color{ 225, 225, 235, 255 });
    }
    ui::track("castle", castle);
    if (overCastle) {
        DrawRectangleLinesEx(castle, 2.0f, Fade(ui::ACCENT, 0.9f));
        ui::textCentered("Enter the castle", castle.x + castle.width / 2, castle.y + castle.height + 8, 20, ui::ACCENT);
        ui::handCursor();
    }
    if ((overCastle && in::mousePressed(MOUSE_BUTTON_LEFT)) || in::pressed(KEY_ENTER) || in::pressed(KEY_SPACE)) {
        play("ui"); screen = Screen::Intro; t = 0;
        return;
    }

    // --- the left column: champion, records, buttons ---
    float bw = 260, bh = 46, gap = 10, x = 44;
    int count = hasSave ? 6 : 5;
    float y = sh - 36 - count * (bh + gap);

    Rectangle chip = { x, y - 132, bw, 112 };
    ui::panel(chip);
    drawPortrait(*this, profile.champion, { chip.x + 12, chip.y + 12, 64, 88 });
    ui::text("Champion", chip.x + 88, chip.y + 14, 10, ui::MUTED);
    drawName(profile.champion, chip.x + 88, chip.y + 28, 20, ui::ACCENT);
    ui::text(TextFormat("Deepest floor  %d", profile.deepestFloor), chip.x + 88, chip.y + 58, 10, ui::TEXT);
    ui::text(TextFormat("Bosses beaten  %d / %d", std::popcount(profile.bossesBeaten), BOSS_COUNT), chip.x + 88, chip.y + 72, 10, ui::TEXT);
    ui::text(profile.bestRush > 0 ? TextFormat("Boss Rush best  %s", ui::clock(profile.bestRush)) : "Boss Rush  not yet won",
        chip.x + 88, chip.y + 86, 10, ui::TEXT);

    auto next = [&]() { Rectangle r = { x, y, bw, bh }; y += bh + gap; return r; };
    if (hasSave && ui::button(next(), "Continue", ui::Style::Good)) {
        play("ui");
        if (!continueRun()) hasSave = false;
        return;
    }
    if (ui::button(next(), "New Run", ui::Style::Primary)) { play("ui"); screen = Screen::Intro; t = 0; return; }
    if (ui::button(next(), "Boss Rush")) { play("ui"); startBossRush(); return; }
    if (ui::button(next(), "Champions")) { play("ui"); screen = Screen::Champions; t = 0; return; }
    if (ui::button(next(), "Settings")) { play("ui"); screen = Screen::Settings; rebinding = -1; t = 0; return; }
    if (ui::button(next(), "Exit", ui::Style::Danger)) { play("ui"); quit = true; return; }

    if (!touch::enabled()) ui::textRight("F11 fullscreen", sw - 20.0f, sh - 24.0f, 10, Fade(ui::TEXT, 0.5f));
}

// ------------------------------------------------------------ champion select
void Game::championsScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (in::pressed(KEY_ESCAPE)) { play("ui"); screen = Screen::Menu; return; }

    drawMenuBackground();
    ui::veil(0.82f);
    float pw = fminf(1060.0f, sw - 60.0f), ph = fminf(690.0f, sh - 30.0f);
    Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(panel, "Choose Your Champion");
    if (ui::button({ panel.x + pw - 50, panel.y + 18, 32, 32 }, "X")) { play("ui"); screen = Screen::Menu; return; }

    int cols = 6;
    float pad = 14, gx = panel.x + 28, gy = panel.y + 80;
    float cw = (pw - 56 - (cols - 1) * pad) / cols;
    float detailH = 132;
    float chh = (ph - 80 - detailH - 24 - pad) / 2 - 26;
    float turnH = fminf(48.0f, (cw - 12) / 3);          // the four sides under the portrait (3:4 figures)
    int shown = profile.champion;
    for (int i = 0; i < CHAMPION_COUNT; ++i) {
        int col = i % cols, row = i / cols;
        Rectangle card = { gx + col * (cw + pad), gy + row * (chh + 26 + pad), cw, chh + 26 };
        bool over = ui::hovered(card), sel = i == profile.champion;
        ui::track(CHAMPIONS[i].name, card);
        if (over) { shown = i; ui::handCursor(); }
        DrawRectangleRec(card, over ? ColorBrightness(ui::PANEL_LIGHT, 0.12f) : ui::PANEL_LIGHT);
        drawPortrait(*this, i, { card.x + 6, card.y + 6, card.width - 12, chh - 12 - turnH });
        // ...and the figure they walk as, from the front, the left, the back and the right
        drawTurnaround(walk[i], { card.x + 6, card.y + chh - 2 - turnH, card.width - 12, turnH }, clock);
        DrawRectangleLinesEx(card, sel ? 3.0f : 2.0f, sel ? ui::GOOD : (over ? ui::ACCENT : ui::EDGE));
        drawName(i, card.x + card.width / 2, card.y + chh + 4, 10, sel ? ui::GOOD : ui::TEXT, true);
        if (over && in::mousePressed(MOUSE_BUTTON_LEFT) && !sel) {
            profile.champion = i; saveProfile(); play("pickup");
        }
    }

    // details of the hovered (or chosen) champion, walking from every side on the right
    const ChampionDef& c = CHAMPIONS[shown];
    Rectangle d = { panel.x + 28, panel.y + ph - detailH - 20, pw - 56, detailH };
    DrawRectangleRec(d, Fade(ui::INK, 0.6f));
    DrawRectangleLinesEx(d, 1, ui::EDGE);
    float turnW = (detailH - 24) * 3 / 4 * 4 + 12;
    drawTurnaround(walk[shown], { d.x + d.width - turnW - 14, d.y + 12, turnW, detailH - 24 }, clock);
    drawName(shown, d.x + 18, d.y + 14, 30, ui::ACCENT);
    float nameW = ui::measure(c.name, 30) + (c.fearFace ? 30 * 1.2f + 3 : 0);
    if (shown == profile.champion) ui::text("chosen", d.x + 26 + nameW, d.y + 26, 10, ui::GOOD);
    ui::text("Perk", d.x + 18, d.y + 56, 10, ui::MUTED);
    ui::text(c.perk, d.x + 70, d.y + 54, 20, ui::TEXT);
    ui::text("Special", d.x + 18, d.y + 90, 10, ui::MUTED);
    ui::keycap(keyName(settings.keys.special), d.x + 70, d.y + 88);
    ui::textWrapped(TextFormat("%s - %s", c.special, c.specialDesc), d.x + 96, d.y + 86, d.width - 130 - turnW, 20, ui::TEXT);
}

// ------------------------------------------------------------------ settings
void Game::settingsScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int* binds[] = { &settings.keys.up, &settings.keys.down, &settings.keys.left, &settings.keys.right,
                     &settings.keys.run, &settings.keys.roll, &settings.keys.interact, &settings.keys.special };
    const char* names[] = { "Move up", "Move down", "Move left", "Move right", "Run", "Dodge-roll", "Interact", "Special" };
    constexpr int BINDS = 8;
    auto leave = [&]() { save::writeSettings(settings); screen = Screen::Menu; rebinding = -1; t = 0; };

    if (rebinding >= 0) {
        int k = in::nextKey();
        if (k == KEY_ESCAPE) rebinding = -1;
        else if (k != 0) { *binds[rebinding] = k; rebinding = -1; play("ui"); }
    }
    else if (in::pressed(KEY_ESCAPE)) { play("ui"); leave(); return; }

    drawMenuBackground();
    ui::veil(0.82f);
    float pw = fminf(840.0f, sw - 60.0f), ph = fminf(640.0f, sh - 30.0f);
    Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(panel, "Settings");
    float x = panel.x + 34, y = panel.y + 84;

    // --- audio ---
    ui::text("Audio", x, y, 20, ui::ACCENT);
    auto slider = [&](float sy, const char* label, float& val) {
        ui::text(label, x, sy, 20, ui::TEXT);
        Rectangle track = { x + 140, sy + 4, 380, 12 };
        if (CheckCollisionPointRec(in::mouse(), { track.x - 8, track.y - 8, track.width + 16, 28 }) && in::mouseDown(MOUSE_BUTTON_LEFT))
            val = Clamp((in::mouse().x - track.x) / track.width, 0, 1);
        ui::bar(track, val, ui::MANA);
        DrawRectangleRec({ track.x + track.width * val - 6, track.y - 6, 12, 24 }, ui::TEXT);
        ui::text(TextFormat("%d%%", (int)(val * 100)), track.x + track.width + 18, sy, 20, ui::TEXT);
    };
    slider(y + 34, "Music", settings.musicVol);
    slider(y + 66, "Effects", settings.sfxVol);

    // --- screen and keys: the desktop's business ---
    if (!touch::enabled()) {
    y += 112;
    ui::text("Screen", x, y, 20, ui::ACCENT);
    bool full = isFullscreen();
    if (ui::button({ x, y + 32, 220, 40 }, "Fullscreen", full ? ui::Style::Good : ui::Style::Normal)) {
        setFullscreen(true);
        settings.fullscreen = true; play("ui");
    }
    if (ui::button({ x + 236, y + 32, 220, 40 }, "Window 1280x720", !full ? ui::Style::Good : ui::Style::Normal)) {
        setFullscreen(false);
        SetWindowSize(1280, 720);
        int mon = GetCurrentMonitor();
        SetWindowPosition((GetMonitorWidth(mon) - 1280) / 2, (GetMonitorHeight(mon) - 720) / 2);
        settings.fullscreen = false; play("ui");
    }

    // --- keybinds, in two columns ---
    y += 96;
    ui::text("Keys", x, y, 20, ui::ACCENT);
    ui::text("click a key, then press the new one (Esc cancels)", x + 70, y + 6, 10, ui::MUTED);
    for (int i = 0; i < BINDS; ++i) {
        float rx = x + (i / 4) * (pw / 2 - 20), ry = y + 36 + (i % 4) * 40;
        ui::text(names[i], rx, ry + 6, 20, ui::TEXT);
        Rectangle box = { rx + 170, ry, 130, 32 };
        bool active = rebinding == i;
        if (ui::button(box, active ? "press a key" : keyName(*binds[i]), active ? ui::Style::Primary : ui::Style::Normal, active ? 10 : 20)) {
            rebinding = i; play("ui");
        }
    }
    }
    else {
        y += 112;
        ui::text("Touch controls", x, y, 20, ui::ACCENT);
        if (ui::button({ x, y + 32, 320, 44 }, "Move and Resize Buttons")) { play("ui"); screen = Screen::Controls; }
    }

    if (ui::button({ panel.x + pw - 190, panel.y + ph - 70, 160, 46 }, "Back", ui::Style::Primary)) { play("ui"); leave(); }
}

// ------------------------------------------------------------ touch controls
void Game::controlsScreen() {
    drawMenuBackground();
    ui::veil(0.7f);
    bool done = touch::editFrame();
    if (done || in::pressed(KEY_ESCAPE)) {
        save::writeTouchLayout(touch::layout());
        play("ui");
        screen = Screen::Settings;
    }
}

// --------------------------------------------------------------------- intro
void Game::introScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (in::pressed(KEY_ESCAPE)) { screen = Screen::Menu; t = 0; return; }
    if (in::pressed(KEY_ENTER) || in::pressed(KEY_SPACE) || in::mousePressed(MOUSE_BUTTON_LEFT) || t > 12.0f) {
        play("ui"); startRun();
        return;
    }
    ClearBackground({ 6, 8, 14, 255 });
    DrawCircleGradient(sw - 220, 150, 140, Fade({ 200, 206, 240, 255 }, 0.18f), Fade({ 200, 206, 240, 255 }, 0));
    DrawCircle(sw - 220, 150, 70, Fade({ 220, 226, 240, 255 }, 0.9f));
    for (int i = 0; i < 200; ++i) DrawPixel((i * 137) % sw, (i * 89) % sh, Fade(WHITE, 0.15f + 0.1f * sinf(clock + i)));
    DrawRectangle(0, sh - 160, sw, 160, { 8, 12, 14, 255 });
    for (int i = 0; i < 4; ++i) {
        float a = Clamp(t - i * 1.6f - 0.5f, 0, 1);
        ui::textCentered(PROPHECY[i], sw / 2.0f, 230 + i * 56.0f, 30, Fade({ 230, 224, 210, 255 }, a));
    }
    if (t > 7.0f) ui::textCentered("click to begin", sw / 2.0f, sh - 90.0f, 20, Fade(ui::TEXT, 0.4f + 0.4f * sinf(t * 3)));
}
