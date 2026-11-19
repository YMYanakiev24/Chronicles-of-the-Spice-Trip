// ============================================================================
//  Look.h — how the world reaches the screen. It is drawn into a small canvas
//  and blown up with hard edges, so everything (painted heroes, spells, fire)
//  shares the chunky pixels of the tiles; then it is lit: a dark ambient, and
//  pools of light from torches, fire, spells and the heroes' own lanterns.
//
//    look::begin(cam, ...);   draw the world as usual, queueing lights with
//    ...                      look::light / look::glow as things are drawn
//    look::end();             light it and put it on the screen
// ============================================================================
#pragma once

#include "raylib.h"

namespace look {
    enum Pixels { PIXELS_OFF = 0, PIXELS_SOFT = 1, PIXELS_RETRO = 2 };

    // From the settings: how chunky, and whether the dark lighting is on.
    void configure(int pixels, bool lighting);
    int  pixels();
    bool lighting();

    // Start drawing the world through `cam`. `pixel` is the size of one canvas pixel in
    // screen pixels (1 or less draws at full resolution). `ambient` is the light where no
    // light falls (white: fully lit); `lit` false draws this place without the lighting.
    void begin(const Camera2D& cam, float pixel, Color ambient, bool lit = true);
    void end();
    bool drawing();          // between begin and end

    // A pool of light on the world (only while drawing, and lighting is on). Strength 1 at
    // the middle brings the world up to its own colours; more makes it glow.
    void light(Vector2 at, float radius, Color c, float strength = 1.0f);
    // A soft bloom drawn over the lit world, so fire stays bright in the dark.
    void glow(Vector2 at, float radius, Color c, float strength = 1.0f);
    // Lights every `step` along a line (a jet of fire, a laser).
    void lightLine(Vector2 a, Vector2 b, float step, float radius, Color c, float strength = 1.0f);

    // What the last frame drew (for the autopilot): its canvas (0 x 0 when it went straight
    // to the screen) and how many lights it had.
    int canvasWidth();
    int canvasHeight();
    int lightCount();

    // A slow, uneven 0.85-1.1 flicker for flames, different for each seed.
    float flicker(float t, float seed);

    void unload();
}
