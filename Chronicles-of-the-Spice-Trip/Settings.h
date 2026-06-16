// ============================================================================
//  Settings.h — user settings (audio + keybinds) and the champion roster.
// ============================================================================
#pragma once

#include "raylib.h"
#include <vector>
#include <string>

// Rebindable controls. Defaults match the original WASD scheme.
struct Keybinds {
    int up = KEY_W;
    int down = KEY_S;
    int left = KEY_A;
    int right = KEY_D;
    int run = KEY_LEFT_SHIFT;
    int roll = KEY_SPACE;
    int interact = KEY_E;
};

struct Settings {
    float musicVol = 0.6f;
    float sfxVol = 0.8f;
    Keybinds keys;
};

// A selectable champion (mirrors CHARACTERS in Scenes.js). file == nullptr
// means "draw the procedural hero"; empty == true is a "Coming soon" slot.
struct Champion {
    const char* name;
    const char* file;     // image filename in site/assets, or nullptr
    bool        empty;
};

inline std::vector<Champion> championRoster() {
    return {
        { "Spellwoven",     nullptr,                 false },
        { "Atanas",         "atanas.png",            false },
        { "Gakev",          "gakev.png",             false },
        { "Nikolay",        "nikolay.png",           false },
        { "Pupesh",         "pupesh.png",            false },
        { "Ristyo",         "ristyo.png",            false },
        { "Iliqn Puh",      "iliqnpuh.png",          false },
        { "Daniil & Vasil", "daniilandvasil.png",    false },
        { "The Storyteller","thestoryteller.png",    false },
        { "The Fairy",      "thefairy.png",          false },
    };
}

// Human-readable name for a key code (covers the keys we let players bind).
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
    case KEY_LEFT_SHIFT: return "L-Shift";
    case KEY_RIGHT_SHIFT: return "R-Shift";
    case KEY_LEFT_CONTROL: return "L-Ctrl";
    case KEY_UP: return "Up"; case KEY_DOWN: return "Down";
    case KEY_LEFT: return "Left"; case KEY_RIGHT: return "Right";
    default: return "?";
    }
}
