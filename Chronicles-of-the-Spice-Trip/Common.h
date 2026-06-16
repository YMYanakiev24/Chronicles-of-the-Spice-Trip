// ============================================================================
//  Common.h — shared constants, palette and helpers used across the game.
// ============================================================================
#pragma once

#include "raylib.h"
#include "raymath.h"

// --- Screen / world geometry ------------------------------------------------
inline constexpr int   SCREEN_W = 1280;
inline constexpr int   SCREEN_H = 720;
inline constexpr int   TILE = 32;
inline constexpr int   MAP_W = 56;
inline constexpr int   MAP_H = 56;

// --- Player kinematics ------------------------------------------------------
inline constexpr float WALK = 130.0f;
inline constexpr float RUN = 210.0f;
inline constexpr float ROLL_SPEED = 360.0f;
inline constexpr float PLAYER_R = 11.0f;

// --- Audio ------------------------------------------------------------------
inline constexpr int   SR = 44100;          // sample rate

// --- Mistwood palette (mirrors GameData.js "mistwood" region) ---------------
inline const Color GROUND_BASE = { 26, 42, 30, 255 };
inline const Color GROUND_ACCENT = { 36, 58, 40, 255 };
inline const Color FOG_TINT = { 122, 154, 138, 255 };
inline constexpr float DARKNESS = 0.66f;

// Uniform random float in [a, b) (uses raylib's RNG; valid after InitWindow).
inline float frand(float a, float b) {
    return a + (b - a) * (float)GetRandomValue(0, 10000) / 10000.0f;
}
