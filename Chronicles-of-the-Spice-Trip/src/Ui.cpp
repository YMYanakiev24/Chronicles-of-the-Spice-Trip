// ============================================================================
//  Ui.cpp — text, panels, buttons and bars in the game's style.
// ============================================================================
#include "Ui.h"
#include "Input.h"
#include <cmath>
#include <string>

namespace ui {

    namespace {
        bool g_hand = false, g_handShown = false;
        std::vector<Hit> g_hits;      // between frames: everything the last frame drew
    }

    void beginFrame() { g_hand = false; g_hits.clear(); }
    void handCursor() { g_hand = true; }
    void track(const char* label, Rectangle r) { g_hits.push_back({ label, r }); }
    const std::vector<Hit>& hits() { return g_hits; }

    void endFrame() {
        if (g_hand != g_handShown) {
            SetMouseCursor(g_hand ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
            g_handShown = g_hand;
        }
    }

    // ------------------------------------------------------------------ text
    int measure(const char* s, int size) { return MeasureText(s, size); }

    void text(const char* s, float x, float y, int size, Color c) {
        int off = size >= 20 ? 2 : 1;
        DrawText(s, (int)x + off, (int)y + off, size, Color{ 0, 0, 0, (unsigned char)(c.a * 0.55f) });
        DrawText(s, (int)x, (int)y, size, c);
    }

    void textCentered(const char* s, float cx, float y, int size, Color c) {
        text(s, cx - measure(s, size) / 2.0f, y, size, c);
    }

    void textRight(const char* s, float right, float y, int size, Color c) {
        text(s, right - measure(s, size), y, size, c);
    }

    float textWrapped(const char* s, float x, float y, float width, int size, Color c) {
        std::string line, word, all = s;
        float yy = y, lineH = size * 1.3f;
        auto flush = [&]() { if (!line.empty()) { text(line.c_str(), x, yy, size, c); yy += lineH; line.clear(); } };
        for (size_t i = 0; i <= all.size(); ++i) {
            char ch = i < all.size() ? all[i] : ' ';
            if (ch != ' ' && ch != '\n') { word += ch; continue; }
            std::string trial = line.empty() ? word : line + " " + word;
            if (!line.empty() && measure(trial.c_str(), size) > width) { flush(); line = word; }
            else line = trial;
            word.clear();
            if (ch == '\n') flush();
        }
        flush();
        return yy - y;
    }

    // --------------------------------------------------------------- widgets
    bool hovered(Rectangle r) { return CheckCollisionPointRec(in::mouse(), r); }

    void veil(float alpha) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(INK, alpha));
    }

    void panel(Rectangle r) {
        DrawRectangleRec({ r.x + 6, r.y + 8, r.width, r.height }, Color{ 0, 0, 0, 90 });   // drop shadow
        DrawRectangleRec(r, PANEL);
        DrawRectangleLinesEx(r, 2, EDGE);
        DrawRectangleLinesEx({ r.x + 4, r.y + 4, r.width - 8, r.height - 8 }, 1, Fade(EDGE, 0.35f));
    }

    void titledPanel(Rectangle r, const char* title) {
        panel(r);
        text(title, r.x + 28, r.y + 22, 30, ACCENT);
        DrawRectangle((int)r.x + 28, (int)r.y + 62, (int)r.width - 56, 1, Fade(EDGE, 0.8f));
    }

    bool button(Rectangle r, const char* label, Style style, int size, bool enabled) {
        Color fill, edge;
        switch (style) {
        case Style::Primary: fill = { 92, 68, 28, 240 }; edge = { 236, 196, 110, 255 }; break;
        case Style::Danger:  fill = { 92, 34, 44, 240 }; edge = { 230, 140, 150, 255 }; break;
        case Style::Good:    fill = { 34, 84, 56, 240 }; edge = { 140, 220, 160, 255 }; break;
        default:             fill = { 64, 40, 104, 240 }; edge = { 180, 150, 230, 255 }; break;
        }
        track(label, r);
        bool over = enabled && hovered(r);
        if (!enabled) { fill = { 40, 38, 48, 230 }; edge = { 90, 86, 104, 255 }; }
        if (over) { fill = ColorBrightness(fill, 0.18f); edge = ColorBrightness(edge, 0.25f); g_hand = true; }
        DrawRectangleRec({ r.x + 3, r.y + 4, r.width, r.height }, Color{ 0, 0, 0, 80 });
        DrawRectangleRec(r, fill);
        DrawRectangleLinesEx(r, 2, edge);
        textCentered(label, r.x + r.width / 2, r.y + (r.height - size) / 2, size, enabled ? TEXT : MUTED);
        return over && in::mousePressed(MOUSE_BUTTON_LEFT);
    }

    void bar(Rectangle r, float ratio, Color fill, const char* label, int size) {
        ratio = ratio < 0 ? 0 : (ratio > 1 ? 1 : ratio);
        DrawRectangleRec({ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, Color{ 6, 4, 10, 220 });
        DrawRectangleRec(r, Color{ 30, 20, 28, 255 });
        DrawRectangleRec({ r.x, r.y, r.width * ratio, r.height }, fill);
        DrawRectangleRec({ r.x, r.y, r.width * ratio, r.height * 0.35f }, Fade(WHITE, 0.16f));   // sheen
        if (label) textCentered(label, r.x + r.width / 2, r.y + (r.height - size) / 2, size, TEXT);
    }

    void keycap(const char* key, float x, float y) {
        int w = measure(key, 10) + 8;
        DrawRectangle((int)x, (int)y, w, 14, Color{ 14, 12, 20, 230 });
        DrawRectangleLines((int)x, (int)y, w, 14, Fade(EDGE_LIGHT, 0.7f));
        DrawText(key, (int)x + 4, (int)y + 2, 10, TEXT);
    }

    void drawCover(Texture2D tex, Rectangle dst, Color tint) {
        if (!tex.id) return;
        float sc = fmaxf(dst.width / tex.width, dst.height / tex.height);
        float sw = dst.width / sc, sh = dst.height / sc;
        DrawTexturePro(tex, { (tex.width - sw) / 2, (tex.height - sh) / 2, sw, sh }, dst, { 0, 0 }, 0, tint);
    }

    void drawContain(Texture2D tex, Rectangle src, Rectangle dst, Color tint) {
        if (!tex.id) return;
        float sc = fminf(dst.width / src.width, dst.height / src.height);
        float w = src.width * sc, h = src.height * sc;
        DrawTexturePro(tex, src, { dst.x + (dst.width - w) / 2, dst.y + (dst.height - h) / 2, w, h }, { 0, 0 }, 0, tint);
    }

    const char* clock(float seconds) {
        int s = (int)(seconds + 0.5f);
        return TextFormat("%d:%02d", s / 60, s % 60);
    }

} // namespace ui
