// ============================================================================
//  Input.cpp — raylib input, or the autopilot's scripted input.
// ============================================================================
#include "Input.h"
#include <deque>

namespace in {

    namespace {
        constexpr int KEYS = 512, BUTTONS = 8;
        bool g_scripted = false;
        bool g_held[KEYS] = {}, g_tapped[KEYS] = {};
        bool g_mouseHeld[BUTTONS] = {}, g_mouseTapped[BUTTONS] = {};
        Vector2 g_mouse{};
        bool g_vHeld[KEYS] = {}, g_vTapped[KEYS] = {}, g_vMouse[BUTTONS] = {};
        bool g_captured = false;
        std::deque<int> g_queue;

        bool validKey(int k) { return k > 0 && k < KEYS; }
        bool validButton(int b) { return b >= 0 && b < BUTTONS; }
    }

    bool down(int key) {
        if (!g_scripted) return IsKeyDown(key) || (validKey(key) && g_vHeld[key]);
        return validKey(key) && (g_held[key] || g_tapped[key]);
    }

    bool pressed(int key) {
        if (!g_scripted) {
            if (key == KEY_ESCAPE && IsKeyPressed(KEY_BACK)) return true;      // Android's back gesture
            return IsKeyPressed(key) || (validKey(key) && g_vTapped[key]);
        }
        return validKey(key) && g_tapped[key];
    }

    int nextKey() {
        if (!g_scripted) return GetKeyPressed();
        if (g_queue.empty()) return 0;
        int k = g_queue.front(); g_queue.pop_front();
        return k;
    }

    bool mouseDown(int button) {
        if (!g_scripted) {
            if (g_captured) return validButton(button) && g_vMouse[button];
            return IsMouseButtonDown(button);
        }
        return validButton(button) && (g_mouseHeld[button] || g_mouseTapped[button]);
    }

    bool mousePressed(int button) {
        if (!g_scripted) {
            if (g_captured) return validButton(button) && g_vMouse[button];    // holding keeps swinging
            return IsMouseButtonPressed(button);
        }
        return validButton(button) && g_mouseTapped[button];
    }

    Vector2 mouse() { return g_scripted ? g_mouse : GetMousePosition(); }

    void clearVirtual() {
        for (auto& k : g_vHeld) k = false;
        for (auto& b : g_vMouse) b = false;
    }
    void virtualHold(int key, bool on) { if (validKey(key)) g_vHeld[key] = on; }
    void virtualTap(int key) { if (validKey(key)) g_vTapped[key] = true; }
    void virtualMouse(int button, bool on) { if (validButton(button)) g_vMouse[button] = on; }
    void setCaptured(bool on) { g_captured = on; }

    void setScripted(bool on) { g_scripted = on; releaseAll(); }
    bool scripted() { return g_scripted; }

    void hold(int key, bool on) { if (validKey(key)) g_held[key] = on; }
    void tap(int key) { if (validKey(key)) { g_tapped[key] = true; g_queue.push_back(key); } }
    void holdMouse(int button, bool on) { if (validButton(button)) g_mouseHeld[button] = on; }
    void click(int button) { if (validButton(button)) g_mouseTapped[button] = true; }
    void moveMouse(Vector2 p) { g_mouse = p; }

    void releaseAll() {
        for (auto& k : g_held) k = false;
        for (auto& k : g_tapped) k = false;
        for (auto& b : g_mouseHeld) b = false;
        for (auto& b : g_mouseTapped) b = false;
        g_queue.clear();
    }

    void endFrame() {
        for (auto& k : g_tapped) k = false;
        for (auto& b : g_mouseTapped) b = false;
        for (auto& k : g_vTapped) k = false;
        g_queue.clear();
    }

} // namespace in
