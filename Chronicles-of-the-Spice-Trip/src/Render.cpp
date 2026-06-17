// ============================================================================
//  Render.cpp — procedural pixel sprites (rectangles/circles, no image files).
// ============================================================================
#include "Render.h"
#include "Common.h"
#include <cmath>

void drawShadow(Vector2 p, float r) {
    DrawEllipse((int)p.x, (int)(p.y + r * 0.7f), r * 0.9f, r * 0.45f, { 0,0,0,90 });
}

void drawHero(Vector2 p, Vector2 facing, float meleeAnim, bool flash) {
    drawShadow(p, PLAYER_R);
    Color cloak = flash ? Color{ 255,255,255,255 } : Color{ 70, 96, 120, 255 };
    DrawRectangle((int)p.x - 8, (int)p.y - 10, 16, 20, cloak);
    DrawRectangle((int)p.x - 8, (int)p.y - 10, 16, 4, { 159, 208, 255, 255 });
    DrawRectangle((int)p.x - 5, (int)p.y - 20, 10, 10, { 224, 196, 168, 255 });
    DrawRectangle((int)p.x - 6, (int)p.y - 22, 12, 4, { 40,30,46,255 });
    float ang = atan2f(facing.y, facing.x);
    float swing = meleeAnim > 0 ? sinf((1.0f - meleeAnim) * PI) * 0.9f : 0.0f;
    Vector2 tip = { p.x + cosf(ang - swing) * 26, p.y + sinf(ang - swing) * 26 };
    Vector2 hilt = { p.x + cosf(ang - swing) * 8,  p.y + sinf(ang - swing) * 8 };
    DrawLineEx(hilt, tip, 3.0f, meleeAnim > 0 ? Color{ 234, 255, 255, 255 } : Color{ 180,190,205,255 });
}

void drawEnemy(const Enemy& e) {
    drawShadow(e.pos, e.r);
    Color c = e.hitFlash > 0 ? Color{ 255,255,255,255 } : e.color;
    float w = sinf(e.wobble) * 2.0f;
    if (e.boss) {
        DrawCircle((int)e.pos.x, (int)e.pos.y, e.r, c);
        DrawCircle((int)e.pos.x, (int)(e.pos.y - e.r * 0.3f), e.r * 0.5f, { 255, 90, 30, 255 });
        DrawCircle((int)e.pos.x, (int)(e.pos.y - e.r * 0.3f), e.r * 0.25f, { 255, 220, 120, 255 });
    }
    else {
        DrawRectangle((int)(e.pos.x - e.r), (int)(e.pos.y - e.r + w), (int)(e.r * 2), (int)(e.r * 2), c);
        DrawCircle((int)e.pos.x, (int)(e.pos.y - e.r + w), e.r * 0.7f, c);
        DrawCircle((int)(e.pos.x - e.r * 0.4f), (int)(e.pos.y - e.r * 0.8f + w), 2, { 200,255,220,255 });
        DrawCircle((int)(e.pos.x + e.r * 0.4f), (int)(e.pos.y - e.r * 0.8f + w), 2, { 200,255,220,255 });
    }
    if (e.hp < e.maxHp) {
        float bw = e.r * 2.4f;
        DrawRectangle((int)(e.pos.x - bw / 2), (int)(e.pos.y - e.r - 12), (int)bw, 4, { 20,10,10,200 });
        DrawRectangle((int)(e.pos.x - bw / 2), (int)(e.pos.y - e.r - 12), (int)(bw * (e.hp / e.maxHp)), 4, { 200,60,60,255 });
    }
}

void drawTree(int tx, int ty) {
    float x = tx * (float)TILE, y = ty * (float)TILE;
    DrawEllipse((int)(x + TILE / 2), (int)(y + TILE - 4), TILE * 0.4f, TILE * 0.2f, { 0,0,0,80 });
    DrawRectangle((int)(x + TILE / 2 - 3), (int)(y + 8), 6, TILE - 10, { 30, 22, 18, 255 });
    DrawCircle((int)(x + TILE / 2), (int)(y + 8), TILE * 0.42f, { 18, 34, 24, 255 });
    DrawCircle((int)(x + TILE / 2 - 6), (int)(y + 6), TILE * 0.28f, { 26, 46, 32, 255 });
}

std::string findAsset(const char* file) {
    // Images live in the project's assets/ folder. Try several relative depths
    // so it works whether the cwd is the project dir (VS) or x64/Debug (exe).
    const char* roots[] = { "assets/", "../assets/", "../../assets/",
                            "../../../assets/", "../../../../assets/" };
    for (auto r : roots) { std::string p = std::string(r) + file; if (FileExists(p.c_str())) return p; }
    return "";
}
