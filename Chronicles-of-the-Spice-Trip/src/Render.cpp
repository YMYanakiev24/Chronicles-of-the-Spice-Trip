// ============================================================================
//  Render.cpp — procedural pixel sprites + sprite-sheet enemies.
// ============================================================================
#include "Render.h"
#include "Common.h"
#include "Sprites.h"
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

// Sprite scale per enemy kind (frames are 32px; the creature fills ~16px).
static float enemyScale(const Enemy& e) {
    if (e.boss) return 3.2f;
    if (e.kind == EK_SKULL) return 2.0f;
    if (e.kind == EK_THIEF || e.kind == EK_MELON) return 1.05f;   // these fill ~28px of the frame
    return e.kind == EK_SKELETON2 ? 1.9f : 1.7f;
}

void drawEnemy(const Enemy& e) {
    bool hasSprite = e.kind == EK_SKULL ? SPR.skull.ok() : SPR.enemy[e.kind].idle.ok();
    if (hasSprite) {
        float sc = enemyScale(e);
        Vector2 at = { e.pos.x, e.pos.y - e.r * 0.6f };
        if (!e.dying) drawShadow(e.pos, e.r);
        if (e.kind == EK_SKULL) {
            // floating wisp: bob up and down, flash white when struck
            at.y += sinf(e.wobble) * 3.0f - 6.0f;
            Color tint = e.dying ? Fade(WHITE, 1.0f - e.deathT / 0.5f) : (e.hitFlash > 0 ? Color{ 255,160,160,255 } : WHITE);
            drawAnim(SPR.skull, animFrame(SPR.skull, e.animT, 8, true), at, sc, e.flip, tint);
        }
        else {
            const EnemyAnims& A = SPR.enemy[e.kind];
            const Anim* a = &A.idle; float t = e.animT; float fps = 8; bool loop = true;
            if (e.dying) { a = &A.death; t = e.deathT; fps = 14; loop = false; }
            else if (e.hitFlash > 0) { a = &A.hurt; t = 0.12f - e.hitFlash; fps = 40; loop = false; }
            else if (e.atkAnim > 0) { a = &A.attack; t = A.attack.length(16) - e.atkAnim; fps = 16; loop = false; }
            else if (e.moving) { a = &A.move; fps = 12; }
            drawAnim(*a, animFrame(*a, t, fps, loop), at, sc, e.flip);
        }
    }
    else {
        drawShadow(e.pos, e.r);
        Color c = e.hitFlash > 0 ? Color{ 255,255,255,255 } : e.color;
        float w = sinf(e.wobble) * 2.0f;
        DrawRectangle((int)(e.pos.x - e.r), (int)(e.pos.y - e.r + w), (int)(e.r * 2), (int)(e.r * 2), c);
        DrawCircle((int)e.pos.x, (int)(e.pos.y - e.r + w), e.r * 0.7f, c);
    }
    if (!e.dying && e.hp < e.maxHp) {
        float bw = e.r * 2.4f, top = e.pos.y - e.r * (e.boss ? 2.6f : 2.0f) - 6;
        DrawRectangle((int)(e.pos.x - bw / 2), (int)top, (int)bw, 4, { 20,10,10,200 });
        DrawRectangle((int)(e.pos.x - bw / 2), (int)top, (int)(bw * (e.hp / e.maxHp)), 4, { 200,60,60,255 });
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
