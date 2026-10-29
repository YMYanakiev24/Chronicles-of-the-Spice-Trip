// ============================================================================
//  Ui.h — the game's look in one place: palette, text, panels, buttons, bars.
//  Text uses raylib's pixel font at multiples of 10 so it stays crisp.
// ============================================================================
#pragma once

#include "raylib.h"
#include <string>
#include <vector>

namespace ui {

    // --- Palette --------------------------------------------------------------
    inline const Color INK = { 8, 8, 14, 255 };          // backgrounds
    inline const Color PANEL = { 22, 18, 34, 246 };      // panel fill
    inline const Color PANEL_LIGHT = { 34, 28, 50, 255 };// cards and rows
    inline const Color EDGE = { 112, 94, 150, 255 };     // panel borders
    inline const Color EDGE_LIGHT = { 206, 178, 240, 255 };
    inline const Color ACCENT = { 240, 205, 120, 255 };    // titles, gold, highlights
    inline const Color TEXT = { 232, 228, 240, 255 };
    inline const Color MUTED = { 160, 154, 180, 255 };
    inline const Color GOOD = { 128, 222, 150, 255 };
    inline const Color BAD = { 236, 112, 122, 255 };
    inline const Color HP = { 204, 62, 70, 255 };
    inline const Color MANA = { 82, 140, 240, 255 };
    inline const Color XP = { 226, 186, 92, 255 };

    enum class Style { Normal, Primary, Danger, Good };

    // Call once per frame around all drawing (handles the hand cursor).
    void beginFrame();
    void endFrame();
    void handCursor();                   // show the pointing hand this frame
    bool wantsHand();                    // a button asked for the hand this frame

    // Every button and card drawn this frame, by label, so the autopilot can click them.
    struct Hit { std::string label; Rectangle rect; };
    void track(const char* label, Rectangle r);
    const std::vector<Hit>& hits();

    // --- Text ---------------------------------------------------------------
    int  measure(const char* s, int size);
    void text(const char* s, float x, float y, int size, Color c);              // with a soft shadow
    void textCentered(const char* s, float cx, float y, int size, Color c);
    void textRight(const char* s, float right, float y, int size, Color c);
    // Word-wrapped text; returns the height used.
    float textWrapped(const char* s, float x, float y, float width, int size, Color c);

    // --- Widgets ------------------------------------------------------------
    bool hovered(Rectangle r);
    void veil(float alpha);                                   // darken the whole screen
    void panel(Rectangle r);
    void titledPanel(Rectangle r, const char* title);         // panel + gold title + divider
    bool button(Rectangle r, const char* label, Style style = Style::Normal, int size = 20, bool enabled = true);
    void bar(Rectangle r, float ratio, Color fill, const char* label = nullptr, int size = 10);
    void keycap(const char* key, float x, float y);           // a small [Q] style key label

    // Cover-fit a texture into dst (cropping the overflow), optionally from a source sub-rect.
    void drawCover(Texture2D tex, Rectangle dst, Color tint = WHITE);
    // Fit a texture region inside dst, keeping its aspect ratio (letterboxed).
    void drawContain(Texture2D tex, Rectangle src, Rectangle dst, Color tint = WHITE);

    // "m:ss" for a time in seconds.
    const char* clock(float seconds);

} // namespace ui
