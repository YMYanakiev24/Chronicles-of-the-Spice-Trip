// ============================================================================
//  Touch.cpp вЂ” on-screen stick and buttons for phones.
// ============================================================================
#include "Touch.h"
#include "Input.h"
#include "Ui.h"
#include "raymath.h"
#include <vector>

namespace touch {

    namespace {
        constexpr float DEAD_ZONE = 0.3f, RUN_ZONE = 0.85f;
        constexpr int ROLE_NONE = -2, ROLE_STICK = -1;        // otherwise a button index

        struct Button { Vector2 pos; float r; const char* label; };
        struct Finger { int id; int role; };
        struct Zone { Rectangle r; int key; };

        bool g_enabled = false, g_playing = false;
        std::vector<Finger> g_fingers;
        std::vector<Zone> g_zones, g_nextZones;
        Vector2 g_base{}, g_thumb{}, g_dir{};
        bool g_stickActive = false;
        TouchLayout g_layout;
        int g_selected = TC_BLADE;
        bool g_dragging = false, g_sizing = false;
        Vector2 g_grab{};

        // Right-hand cluster, anchored to the corner so it fits every screen shape.
        void place(Button (&b)[TC_COUNT]) {
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            b[TC_BLADE] = { { sw - 104, sh - 104 }, 64, "Blade" };
            b[TC_CAST] = { { sw - 240, sh - 76 }, 50, "Cast" };
            b[TC_ROLL] = { { sw - 84, sh - 240 }, 50, "Roll" };
            b[TC_INTERACT] = { { sw - 206, sh - 192 }, 44, "" };      // labelled with its key, like the prompts
            b[TC_PAUSE] = { { 44, 190 }, 28, "II" };
            b[TC_STICK] = { { 140, sh - 130 }, 70, "Move" };
            if (!g_layout.custom) return;
            for (int i = 0; i < TC_COUNT; ++i)
                { b[i].pos = { g_layout.x[i] * sw, g_layout.y[i] * sh }; b[i].r = g_layout.r[i]; }
        }

        // The layout on screen becomes the player's own, ready to change.
        void makeCustom(const Button (&b)[TC_COUNT]) {
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            for (int i = 0; i < TC_COUNT; ++i)
                { g_layout.x[i] = b[i].pos.x / sw; g_layout.y[i] = b[i].pos.y / sh; g_layout.r[i] = b[i].r; }
            g_layout.custom = true;
        }

        void readPoints(std::vector<Vector2>& pos, std::vector<int>& ids) {
            int n = GetTouchPointCount();
            for (int i = 0; i < n; ++i) { pos.push_back(GetTouchPosition(i)); ids.push_back(GetTouchPointId(i)); }
#ifndef PLATFORM_ANDROID
            if (n == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { pos.push_back(GetMousePosition()); ids.push_back(0); }
#endif
        }

        int findFinger(int id) {
            for (size_t i = 0; i < g_fingers.size(); ++i) if (g_fingers[i].id == id) return (int)i;
            return -1;
        }

        // What did a new finger land on? Taps fire here; holds are read every frame.
        int landFinger(Vector2 p, const Button (&buttons)[TC_COUNT], const Keybinds& keys) {
            for (auto& z : g_zones)
                if (CheckCollisionPointRec(p, z.r)) { in::virtualTap(z.key); return ROLE_NONE; }
            for (int b = 0; b < TC_STICK; ++b) {
                if (Vector2Distance(p, buttons[b].pos) > buttons[b].r + 10) continue;
                if (b == TC_ROLL) in::virtualTap(keys.roll);
                else if (b == TC_INTERACT) in::virtualTap(keys.interact);
                else if (b == TC_PAUSE) in::virtualTap(KEY_ESCAPE);
                return b;
            }
            // the stick wakes under the thumb, anywhere on its side of the screen
            float middle = GetScreenWidth() * 0.5f;
            if ((p.x < middle) == (buttons[TC_STICK].pos.x < middle)) { g_base = p; return ROLE_STICK; }
            return ROLE_NONE;
        }

        void moveStick(Vector2 p, float radius, const Keybinds& keys) {
            g_stickActive = true;
            Vector2 off = Vector2Subtract(p, g_base);
            float len = Vector2Length(off);
            if (len > radius) {                             // the base follows a thumb that slides away
                g_base = Vector2Subtract(p, Vector2Scale(off, radius / len));
                off = Vector2Subtract(p, g_base);
                len = radius;
            }
            g_thumb = p;
            float amount = len / radius;
            if (amount < DEAD_ZONE) return;
            g_dir = Vector2Scale(off, 1.0f / len);
            in::virtualHold(keys.left, g_dir.x < -0.38f);
            in::virtualHold(keys.right, g_dir.x > 0.38f);
            in::virtualHold(keys.up, g_dir.y < -0.38f);
            in::virtualHold(keys.down, g_dir.y > 0.38f);
            in::virtualHold(keys.run, amount > RUN_ZONE);
        }
    }

    void setEnabled(bool on) { g_enabled = on; }
    bool enabled() { return g_enabled; }
    Vector2 stickDir() { return g_dir; }
    void zone(Rectangle r, int key) { g_nextZones.push_back({ r, key }); }
    void setLayout(const TouchLayout& t) { g_layout = t; }
    const TouchLayout& layout() { return g_layout; }

    Vector2 controlPos(int control) {
        Button b[TC_COUNT];
        place(b);
        return b[control].pos;
    }

    void update(bool playing, const Keybinds& keys) {
        if (!g_enabled) return;
        g_zones.swap(g_nextZones);
        g_nextZones.clear();
        g_playing = playing;
        in::setCaptured(playing);
        in::clearVirtual();
        g_stickActive = false;
        g_dir = { 0, 0 };
        if (!playing) { g_fingers.clear(); return; }

        Button buttons[TC_COUNT];
        place(buttons);
        std::vector<Vector2> pos;
        std::vector<int> ids;
        readPoints(pos, ids);

        // forget lifted fingers
        std::vector<Finger> kept;
        for (auto& f : g_fingers)
            for (int id : ids) if (id == f.id) { kept.push_back(f); break; }
        g_fingers.swap(kept);

        bool blade = false, cast = false;
        for (size_t i = 0; i < pos.size(); ++i) {
            int at = findFinger(ids[i]);
            if (at < 0) {
                g_fingers.push_back({ ids[i], landFinger(pos[i], buttons, keys) });
                at = (int)g_fingers.size() - 1;
            }
            int role = g_fingers[at].role;
            if (role == TC_BLADE) blade = true;
            else if (role == TC_CAST) cast = true;
            else if (role == ROLE_STICK) moveStick(pos[i], buttons[TC_STICK].r, keys);
        }
        in::virtualMouse(MOUSE_BUTTON_LEFT, blade);         // hold to keep swinging
        in::virtualHold(KEY_ENTER, cast);                   // hold to keep casting
    }

    void draw(const Keybinds& keys) {
        if (!g_enabled || !g_playing) return;
        Button buttons[TC_COUNT];
        place(buttons);
        float stick = buttons[TC_STICK].r;
        if (g_stickActive) {
            DrawCircleV(g_base, stick, Fade(BLACK, 0.3f));
            DrawCircleLinesV(g_base, stick, Fade(RAYWHITE, 0.45f));
            DrawCircleV(g_thumb, stick * 0.43f, Fade(RAYWHITE, 0.5f));
        }
        else {
            Vector2 hint = buttons[TC_STICK].pos;
            DrawCircleLinesV(hint, stick, Fade(RAYWHITE, 0.2f));
            ui::textCentered("move", hint.x, hint.y - 5, 10, Fade(RAYWHITE, 0.35f));
        }
        for (int b = 0; b < TC_STICK; ++b) {
            bool held = false;
            for (auto& f : g_fingers) held = held || f.role == b;
            DrawCircleV(buttons[b].pos, buttons[b].r, Fade(BLACK, held ? 0.55f : 0.32f));
            DrawCircleLinesV(buttons[b].pos, buttons[b].r, Fade(ui::ACCENT, held ? 1.0f : 0.6f));
            const char* label = b == TC_INTERACT ? keyName(keys.interact) : buttons[b].label;
            ui::textCentered(label, buttons[b].pos.x, buttons[b].pos.y - 10, 20, Fade(RAYWHITE, 0.9f));
        }
    }

    bool editFrame() {
        float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
        Button b[TC_COUNT];
        place(b);
        Vector2 m = in::mouse();
        bool pressed = in::mousePressed(MOUSE_BUTTON_LEFT), held = in::mouseDown(MOUSE_BUTTON_LEFT);

        // the panel keeps out of the way of the control being edited
        float pw = fminf(600.0f, sw - 40), ph = 96;
        Rectangle bar = { (sw - pw) / 2, b[g_selected].pos.y < sh / 2 ? sh - ph - 12 : 12, pw, ph };
        Rectangle track = { bar.x + 90, bar.y + 58, pw - 340, 14 };
        Rectangle grabTrack = { track.x - 12, track.y - 16, track.width + 24, 46 };
        Rectangle reset = { bar.x + pw - 226, bar.y + 40, 104, 44 }, done = { bar.x + pw - 112, bar.y + 40, 100, 44 };
        ui::track("Size", track);

        float minR = g_selected == TC_STICK ? 44.0f : 26.0f, maxR = g_selected == TC_STICK ? 130.0f : 100.0f;
        bool onPanelControl = CheckCollisionPointRec(m, grabTrack) || CheckCollisionPointRec(m, reset) || CheckCollisionPointRec(m, done);
        if (!held) g_dragging = g_sizing = false;
        if (pressed && CheckCollisionPointRec(m, grabTrack)) g_sizing = true;
        else if (pressed && !onPanelControl) {
            // the nearest control under the finger, so overlapping ones stay reachable
            float best = 1e9f;
            for (int i = 0; i < TC_COUNT; ++i) {
                float d = Vector2Distance(m, b[i].pos);
                if (d > b[i].r + 10 || d >= best) continue;
                best = d; g_selected = i; g_dragging = true;
                g_grab = Vector2Subtract(b[i].pos, m);
            }
        }
        if (g_dragging || g_sizing) makeCustom(b);
        Button& s = b[g_selected];
        if (g_sizing) s.r = Lerp(minR, maxR, Clamp((m.x - track.x) / track.width, 0, 1));
        if (g_dragging) s.pos = Vector2Add(m, g_grab);
        if (g_dragging || g_sizing) {
            s.pos = { Clamp(s.pos.x, s.r, sw - s.r), Clamp(s.pos.y, s.r, sh - s.r) };
            makeCustom(b);
        }

        for (int i = 0; i < TC_COUNT; ++i) {
            bool sel = i == g_selected;
            DrawCircleV(b[i].pos, b[i].r, Fade(BLACK, sel ? 0.6f : 0.4f));
            DrawCircleLinesV(b[i].pos, b[i].r, sel ? ui::ACCENT : Fade(RAYWHITE, 0.6f));
            if (sel) DrawCircleLinesV(b[i].pos, b[i].r + 2, ui::ACCENT);
            ui::textCentered(i == TC_INTERACT ? "E" : b[i].label, b[i].pos.x, b[i].pos.y - 10, 20, RAYWHITE);
        }

        ui::panel(bar);
        ui::text("Drag a control to move it", bar.x + 16, bar.y + 12, 20, ui::ACCENT);
        ui::text("Size", bar.x + 16, bar.y + 54, 20, ui::TEXT);
        float value = (s.r - minR) / (maxR - minR);
        ui::bar(track, value, ui::MANA);
        DrawRectangleRec({ track.x + track.width * value - 6, track.y - 6, 12, 26 }, ui::TEXT);
        if (ui::button(reset, "Reset")) g_layout = TouchLayout{};
        return ui::button(done, "Done", ui::Style::Primary);
    }

} // namespace touch
