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
        constexpr float STICK_RADIUS = 70, DEAD_ZONE = 0.3f, RUN_ZONE = 0.85f;
        constexpr int ROLE_NONE = -2, ROLE_STICK = -1;        // otherwise a button index

        struct Button { Vector2 pos; float r; const char* label; };
        struct Finger { int id; int role; };
        struct Zone { Rectangle r; int key; };
        enum { BTN_BLADE, BTN_CAST, BTN_ROLL, BTN_INTERACT, BTN_PAUSE, BTN_COUNT };

        bool g_enabled = false, g_playing = false;
        std::vector<Finger> g_fingers;
        std::vector<Zone> g_zones, g_nextZones;
        Vector2 g_base{}, g_thumb{}, g_dir{};
        bool g_stickActive = false;

        // Right-hand cluster, anchored to the corner so it fits every screen shape.
        void layout(Button (&b)[BTN_COUNT]) {
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            b[BTN_BLADE] = { { sw - 104, sh - 104 }, 64, "Blade" };
            b[BTN_CAST] = { { sw - 240, sh - 76 }, 50, "Cast" };
            b[BTN_ROLL] = { { sw - 84, sh - 240 }, 50, "Roll" };
            b[BTN_INTERACT] = { { sw - 206, sh - 192 }, 44, "" };     // labelled with its key, like the prompts
            b[BTN_PAUSE] = { { 44, 190 }, 28, "II" };
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
        int landFinger(Vector2 p, const Button (&buttons)[BTN_COUNT], const Keybinds& keys) {
            for (auto& z : g_zones)
                if (CheckCollisionPointRec(p, z.r)) { in::virtualTap(z.key); return ROLE_NONE; }
            for (int b = 0; b < BTN_COUNT; ++b) {
                if (Vector2Distance(p, buttons[b].pos) > buttons[b].r + 10) continue;
                if (b == BTN_ROLL) in::virtualTap(keys.roll);
                else if (b == BTN_INTERACT) in::virtualTap(keys.interact);
                else if (b == BTN_PAUSE) in::virtualTap(KEY_ESCAPE);
                return b;
            }
            if (p.x < GetScreenWidth() * 0.5f) { g_base = p; return ROLE_STICK; }
            return ROLE_NONE;
        }

        void moveStick(Vector2 p, const Keybinds& keys) {
            g_stickActive = true;
            Vector2 off = Vector2Subtract(p, g_base);
            float len = Vector2Length(off);
            if (len > STICK_RADIUS) {                       // the base follows a thumb that slides away
                g_base = Vector2Subtract(p, Vector2Scale(off, STICK_RADIUS / len));
                off = Vector2Subtract(p, g_base);
                len = STICK_RADIUS;
            }
            g_thumb = p;
            float amount = len / STICK_RADIUS;
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

        Button buttons[BTN_COUNT];
        layout(buttons);
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
            if (role == BTN_BLADE) blade = true;
            else if (role == BTN_CAST) cast = true;
            else if (role == ROLE_STICK) moveStick(pos[i], keys);
        }
        in::virtualMouse(MOUSE_BUTTON_LEFT, blade);         // hold to keep swinging
        in::virtualHold(KEY_ENTER, cast);                   // hold to keep casting
    }

    void draw(const Keybinds& keys) {
        if (!g_enabled || !g_playing) return;
        if (g_stickActive) {
            DrawCircleV(g_base, STICK_RADIUS, Fade(BLACK, 0.3f));
            DrawCircleLinesV(g_base, STICK_RADIUS, Fade(RAYWHITE, 0.45f));
            DrawCircleV(g_thumb, 30, Fade(RAYWHITE, 0.5f));
        }
        else {
            Vector2 hint = { 140, GetScreenHeight() - 130.0f };
            DrawCircleLinesV(hint, STICK_RADIUS, Fade(RAYWHITE, 0.2f));
            ui::textCentered("move", hint.x, hint.y - 5, 10, Fade(RAYWHITE, 0.35f));
        }
        Button buttons[BTN_COUNT];
        layout(buttons);
        for (int b = 0; b < BTN_COUNT; ++b) {
            bool held = false;
            for (auto& f : g_fingers) held = held || f.role == b;
            DrawCircleV(buttons[b].pos, buttons[b].r, Fade(BLACK, held ? 0.55f : 0.32f));
            DrawCircleLinesV(buttons[b].pos, buttons[b].r, Fade(ui::ACCENT, held ? 1.0f : 0.6f));
            const char* label = b == BTN_INTERACT ? keyName(keys.interact) : buttons[b].label;
            ui::textCentered(label, buttons[b].pos.x, buttons[b].pos.y - 10, 20, Fade(RAYWHITE, 0.9f));
        }
    }

} // namespace touch
