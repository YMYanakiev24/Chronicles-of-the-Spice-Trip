// ============================================================================
//  Look.cpp — the pixel canvas and the dark-fantasy lighting (see Look.h).
//
//  The light map is kept at half strength and multiplied in at double ("2x
//  modulate"), so a light can push the world brighter than its own colours,
//  the way a torch makes the stone round it glow warm.
// ============================================================================
#include "Look.h"
#include "rlgl.h"
#include <cmath>
#include <vector>

namespace look {
namespace {
    struct Light { Vector2 at; float radius; Color c; float strength; };

    int  g_pixels = PIXELS_SOFT;
    bool g_lighting = true;

    bool g_drawing = false, g_direct = false, g_lit = false;
    RenderTexture2D g_world{}, g_light{};
    int  g_w = 0, g_h = 0;
    Camera2D g_canvasCam{};
    Rectangle g_dest{};
    Color g_ambient = WHITE;
    std::vector<Light> g_lights, g_glows;
    Texture2D g_vignette{};
    int g_lastW = 0, g_lastH = 0, g_lastLights = 0;

    void fit(int w, int h) {
        if (w == g_w && h == g_h && g_world.id) return;
        if (g_world.id) UnloadRenderTexture(g_world);
        if (g_light.id) UnloadRenderTexture(g_light);
        g_world = LoadRenderTexture(w, h);
        g_light = LoadRenderTexture(w, h);
        SetTextureFilter(g_world.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(g_light.texture, TEXTURE_FILTER_POINT);
        g_w = w; g_h = h;
    }

    // Darker towards the edges of the screen.
    Texture2D vignette() {
        if (g_vignette.id) return g_vignette;
        const int N = 128;
        Image img = GenImageColor(N, N, WHITE);
        Color* px = (Color*)img.data;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                float dx = (x + 0.5f) / N * 2 - 1, dy = (y + 0.5f) / N * 2 - 1;
                float r = sqrtf(dx * dx + dy * dy), k = fminf(1, fmaxf(0, (r - 0.55f) / 0.75f));
                unsigned char v = (unsigned char)(255 * (1 - 0.6f * k * k * (3 - 2 * k)));
                px[y * N + x] = { v, v, v, 255 };
            }
        g_vignette = LoadTextureFromImage(img);
        SetTextureFilter(g_vignette, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
        return g_vignette;
    }

    unsigned char half(float v) { return (unsigned char)fminf(255, fmaxf(0, v * 0.5f)); }

    // Bright in the middle, falling away smoothly (the canvas gives it its pixels).
    void drawLight(const Light& l) {
        float s = l.strength;
        Color in = { half(l.c.r * s), half(l.c.g * s), half(l.c.b * s), 255 };
        // a wide, gentle pool, and a brighter heart (falling off quadratically) in the middle of it
        DrawCircleGradient((int)l.at.x, (int)l.at.y, l.radius, { (unsigned char)(in.r * 0.45f), (unsigned char)(in.g * 0.45f), (unsigned char)(in.b * 0.45f), 255 }, { 0, 0, 0, 255 });
        DrawCircleGradient((int)l.at.x, (int)l.at.y, l.radius * 0.8f, { (unsigned char)(in.r * 0.75f), (unsigned char)(in.g * 0.75f), (unsigned char)(in.b * 0.75f), 255 }, { 0, 0, 0, 0 });
    }
}

void configure(int pixels, bool lighting) { g_pixels = pixels; g_lighting = lighting; }
int  pixels() { return g_pixels; }
bool lighting() { return g_lighting; }
bool drawing() { return g_drawing; }

void begin(const Camera2D& cam, float pixel, Color ambient, bool lit) {
    g_drawing = true;
    g_lit = lit && g_lighting;
    g_ambient = ambient;
    g_lights.clear(); g_glows.clear();
    if (g_pixels == PIXELS_OFF && !g_lit) {             // nothing to do: straight to the screen
        g_direct = true;
        BeginMode2D(cam);
        return;
    }
    g_direct = false;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (g_pixels == PIXELS_OFF || pixel < 1) pixel = 1;
    float P = pixel / cam.zoom;                         // world units per canvas pixel
    fit((int)ceilf(sw / pixel) + 2, (int)ceilf(sh / pixel) + 2);
    // the canvas follows the camera a whole canvas pixel at a time (the rest is made up
    // when it goes on screen), so pixels never crawl as the view moves
    Vector2 snap = { floorf(cam.target.x / P) * P, floorf(cam.target.y / P) * P };
    g_canvasCam = {};
    g_canvasCam.offset = { (float)(g_w / 2), (float)(g_h / 2) };
    g_canvasCam.target = snap;
    g_canvasCam.zoom = 1 / P;
    g_dest = { cam.offset.x - (g_w / 2) * pixel + (snap.x - cam.target.x) * cam.zoom,
               cam.offset.y - (g_h / 2) * pixel + (snap.y - cam.target.y) * cam.zoom,
               g_w * pixel, g_h * pixel };
    BeginTextureMode(g_world);
    ClearBackground(BLACK);
    BeginMode2D(g_canvasCam);
}

void end() {
    if (!g_drawing) return;
    g_drawing = false;
    EndMode2D();
    g_lastW = g_direct ? 0 : g_w; g_lastH = g_direct ? 0 : g_h;
    g_lastLights = g_lit ? (int)g_lights.size() : 0;
    if (g_direct) return;
    EndTextureMode();

    Rectangle flip = { 0, 0, (float)g_w, -(float)g_h };
    if (g_lit) {
        BeginTextureMode(g_light);                       // the light map: dark, then every light added in
        ClearBackground({ half(g_ambient.r), half(g_ambient.g), half(g_ambient.b), 255 });
        BeginMode2D(g_canvasCam);
        BeginBlendMode(BLEND_ADDITIVE);
        for (auto& l : g_lights) drawLight(l);
        EndBlendMode();
        EndMode2D();
        BeginBlendMode(BLEND_MULTIPLIED);                // the edges of the view sink into shadow
        DrawTexturePro(vignette(), { 0, 0, (float)vignette().width, (float)vignette().height },
            { 0, 0, (float)g_w, (float)g_h }, { 0, 0 }, 0, WHITE);
        EndBlendMode();
        EndTextureMode();

        BeginTextureMode(g_world);                       // the world times the light, doubled
        rlSetBlendFactors(RL_DST_COLOR, RL_SRC_COLOR, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        DrawTextureRec(g_light.texture, flip, { 0, 0 }, WHITE);
        EndBlendMode();
        BeginMode2D(g_canvasCam);                        // and the fire's bloom over the top
        BeginBlendMode(BLEND_ADDITIVE);
        for (auto& gl : g_glows)
            DrawCircleGradient((int)gl.at.x, (int)gl.at.y, gl.radius, Fade(gl.c, fminf(1, gl.strength)), { gl.c.r, gl.c.g, gl.c.b, 0 });
        EndBlendMode();
        EndMode2D();
        EndTextureMode();
    }

    // onto the screen, every canvas pixel a hard-edged block (copied, not blended)
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    DrawTexturePro(g_world.texture, flip, g_dest, { 0, 0 }, 0, WHITE);
    EndBlendMode();
}

void light(Vector2 at, float radius, Color c, float strength) {
    if (g_drawing && g_lit && strength > 0.01f && radius > 1) g_lights.push_back({ at, radius, c, strength });
}

void glow(Vector2 at, float radius, Color c, float strength) {
    if (g_drawing && g_lit && strength > 0.01f && radius > 1) g_glows.push_back({ at, radius, c, strength });
}

void lightLine(Vector2 a, Vector2 b, float step, float radius, Color c, float strength) {
    float dx = b.x - a.x, dy = b.y - a.y, len = sqrtf(dx * dx + dy * dy);
    int n = (int)(len / fmaxf(step, 1)) + 1;
    for (int i = 0; i <= n; ++i) {
        float k = n ? (float)i / n : 0;
        light({ a.x + dx * k, a.y + dy * k }, radius, c, strength);
    }
}

int canvasWidth() { return g_lastW; }
int canvasHeight() { return g_lastH; }
int lightCount() { return g_lastLights; }

float flicker(float t, float seed) {
    return 0.97f + 0.06f * sinf(t * 7.3f + seed * 3.1f) * sinf(t * 2.9f + seed) + 0.04f * sinf(t * 17.0f + seed * 5.7f);
}

void unload() {
    if (g_world.id) UnloadRenderTexture(g_world);
    if (g_light.id) UnloadRenderTexture(g_light);
    if (g_vignette.id) UnloadTexture(g_vignette);
    g_world = {}; g_light = {}; g_vignette = {};
    g_w = g_h = 0;
}
}
