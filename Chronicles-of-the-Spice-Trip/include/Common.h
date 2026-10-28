// ============================================================================
//  Common.h — shared constants and tiny helpers used across the game.
// ============================================================================
#pragma once

#include "raylib.h"
#include "raymath.h"

// --- Screen / world geometry ------------------------------------------------
inline constexpr int   SCREEN_W = 1280;
inline constexpr int   SCREEN_H = 720;
inline constexpr int   TILE = 32;

// --- Player kinematics ------------------------------------------------------
inline constexpr float WALK = 130.0f;
inline constexpr float RUN = 210.0f;
inline constexpr float ROLL_SPEED = 360.0f;
inline constexpr float PLAYER_R = 11.0f;

// --- Audio ------------------------------------------------------------------
inline constexpr int   SR = 44100;          // sample rate

// A faint green fog laid over every room.
inline const Color FOG_TINT = { 122, 154, 138, 255 };

// Uniform random float in [a, b) (uses raylib's RNG).
inline float frand(float a, float b) {
    return a + (b - a) * (float)GetRandomValue(0, 10000) / 10000.0f;
}

// Unit vector pointing at angle a (radians).
inline Vector2 fromAngle(float a) { return { cosf(a), sinf(a) }; }
