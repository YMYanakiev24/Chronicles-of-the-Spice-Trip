// ============================================================================
//  Settings.h — audio, screen and keybinds. Saved to settings.dat.
// ============================================================================
#pragma once

#include "raylib.h"

// Rebindable controls.
struct Keybinds {
    int up = KEY_W;
    int down = KEY_S;
    int left = KEY_A;
    int right = KEY_D;
    int run = KEY_LEFT_SHIFT;
    int roll = KEY_SPACE;
    int interact = KEY_E;
    int special = KEY_Q;
};

struct Settings {
    float musicVol = 0.6f;
    float sfxVol = 0.8f;
    bool  fullscreen = true;
    Keybinds keys;
};

// The phone controls. Positions are fractions of the screen, so a layout
// survives a different screen shape; sizes are radii on the 720-line screen.
enum TouchControl { TC_BLADE, TC_CAST, TC_ROLL, TC_INTERACT, TC_PAUSE, TC_STICK, TC_COUNT };

struct TouchLayout {
    bool  custom = false;       // false: the default layout for this screen
    float x[TC_COUNT] = {}, y[TC_COUNT] = {}, r[TC_COUNT] = {};
};

// Human-readable name for a key code (covers the keys players can bind).
inline const char* keyName(int k) {
    switch (k) {
    case KEY_A: return "A"; case KEY_B: return "B"; case KEY_C: return "C";
    case KEY_D: return "D"; case KEY_E: return "E"; case KEY_F: return "F";
    case KEY_G: return "G"; case KEY_H: return "H"; case KEY_I: return "I";
    case KEY_J: return "J"; case KEY_K: return "K"; case KEY_L: return "L";
    case KEY_M: return "M"; case KEY_N: return "N"; case KEY_O: return "O";
    case KEY_P: return "P"; case KEY_Q: return "Q"; case KEY_R: return "R";
    case KEY_S: return "S"; case KEY_T: return "T"; case KEY_U: return "U";
    case KEY_V: return "V"; case KEY_W: return "W"; case KEY_X: return "X";
    case KEY_Y: return "Y"; case KEY_Z: return "Z";
    case KEY_SPACE: return "Space";
    case KEY_TAB: return "Tab";
    case KEY_LEFT_SHIFT: return "L-Shift";
    case KEY_RIGHT_SHIFT: return "R-Shift";
    case KEY_LEFT_CONTROL: return "L-Ctrl";
    case KEY_LEFT_ALT: return "L-Alt";
    case KEY_UP: return "Up"; case KEY_DOWN: return "Down";
    case KEY_LEFT: return "Left"; case KEY_RIGHT: return "Right";
    default: return "?";
    }
}
