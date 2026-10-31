// ============================================================================
//  Render.cpp — the champions' walking figures (and the procedural hero they fall
//  back to), sprite-sheet enemies and projectiles.
// ============================================================================
#include "Render.h"
#include "Common.h"
#include "Sprites.h"
#include <cmath>

void drawShadow(Vector2 p, float r) {
    DrawEllipse((int)p.x, (int)(p.y + r * 0.7f), r * 0.9f, r * 0.45f, { 0,0,0,90 });
}

void drawHero(Vector2 p, Vector2 facing, float meleeAnim, bool flash, Color cloak, float lift) {
    drawShadow(p, PLAYER_R * (1 - lift * 0.02f));
    p.y -= lift;                                   // Bear Jump lifts the hero off the ground
    Color body = flash ? Color{ 255,255,255,255 } : cloak;
    DrawRectangle((int)p.x - 8, (int)p.y - 10, 16, 20, body);
    DrawRectangle((int)p.x - 8, (int)p.y - 10, 16, 4, ColorBrightness(cloak, 0.55f));
    DrawRectangle((int)p.x - 5, (int)p.y - 20, 10, 10, { 224, 196, 168, 255 });
    DrawRectangle((int)p.x - 6, (int)p.y - 22, 12, 4, { 40,30,46,255 });
    float ang = atan2f(facing.y, facing.x);
    float swing = meleeAnim > 0 ? sinf((1.0f - meleeAnim) * PI) * 0.9f : 0.0f;
    Vector2 tip = { p.x + cosf(ang - swing) * 26, p.y + sinf(ang - swing) * 26 };
    Vector2 hilt = { p.x + cosf(ang - swing) * 8,  p.y + sinf(ang - swing) * 8 };
    DrawLineEx(hilt, tip, 3.0f, meleeAnim > 0 ? Color{ 234, 255, 255, 255 } : Color{ 180,190,205,255 });
}

void drawChampion(const WalkSheet& w, Vector2 p, Vector2 look, bool walking, float time,
                  Vector2 facing, float meleeAnim, bool flash, Color cloak, const Blade& blade, float lift) {
    if (!w.ok()) { drawHero(p, facing, meleeAnim, flash, cloak, lift); return; }
    drawShadow(p, PLAYER_R * (1 - lift * 0.02f));
    float x = p.x, y = p.y - lift;                 // Bear Jump lifts the champion off the ground
    const float fw = 24 * 1.4f, fh = 32 * 1.4f;    // the drawn figures' 24 x 32, scaled up
    w.draw(facingOf(look), w.frameAt(walking, time), { x - fw / 2, y - 10 - fh / 2, fw, fh }, WHITE, flash);
    if (meleeAnim > 0) {                           // the godless blade's slash
        float ang = atan2f(facing.y, facing.x);
        float a = ang - sinf((1.0f - meleeAnim) * PI) * 0.9f;
        if (blade.tex.id) {
            BeginBlendMode(BLEND_ADDITIVE);                // the flash of the blade's path
            DrawRing({ x, y - 4 }, blade.length * 0.45f, blade.length * 1.05f, a * RAD2DEG - 6, ang * RAD2DEG + 6, 14, Fade(WHITE, 0.45f * meleeAnim));
            EndBlendMode();
            float len = blade.length, sc = len * cosf(blade.slant * DEG2RAD) / blade.tex.width;
            float w = blade.tex.width * sc, h = blade.tex.height * sc, mid = 6 + len * 0.4f;
            DrawTexturePro(blade.tex, { 0, 0, (float)blade.tex.width, (float)blade.tex.height },
                { x + cosf(a) * mid, y - 4 + sinf(a) * mid, w, h }, { w / 2, h / 2 }, a * RAD2DEG + blade.slant, WHITE);
        }
        else DrawRectanglePro({ x + cosf(a) * 17, y - 4 + sinf(a) * 17, 22, 3 }, { 11, 1.5f }, a * RAD2DEG, { 234, 255, 255, 255 });
    }
}

void drawFigureIn(const WalkSheet& w, Facing facing, int frame, Rectangle box, Color tint) {
    float h = fminf(box.height, box.width * 4 / 3), fw = h * 3 / 4;
    w.draw(facing, frame, { box.x + (box.width - fw) / 2, box.y + box.height - h, fw, h }, tint);
}

void drawTurnaround(const WalkSheet& w, Rectangle box, float time) {
    static const Facing order[4] = { FACE_DOWN, FACE_LEFT, FACE_UP, FACE_RIGHT };
    float cw = box.width / 4;
    for (int i = 0; i < 4; ++i)
        drawFigureIn(w, order[i], w.frameAt(true, time), { box.x + i * cw + 1, box.y, cw - 2, box.height });
}

// Sprite scale per enemy kind (frames are 32px; the creature fills ~16px).
static float enemyScale(const Enemy& e) {
    if (e.boss) return 3.2f;
    if (e.kind == EK_SKULL) return 2.0f;
    if (e.kind == EK_GAKEV || e.kind == EK_MELON) return 1.05f;   // these fill ~28px of the frame
    if (e.kind == EK_TOSHKO) return 0.7f;                          // 48px frames he fills ~44px of
    return e.kind == EK_SKELETON2 ? 1.9f : 1.7f;
}

void drawEnemy(const Enemy& e) {
    bool hasSprite = e.kind == EK_SKULL ? SPR.skull.ok() : SPR.enemy[e.kind].idle.ok();
    if (hasSprite) {
        float sc = enemyScale(e);
        Vector2 at = { e.pos.x, e.pos.y - e.r * 0.6f };
        if (!e.dying) drawShadow(e.pos, e.r);
        if (e.kind == EK_SKULL) {
            // floating wisp: bob up and down, flash when struck
            at.y += sinf(e.wobble) * 3.0f - 6.0f;
            Color tint = e.dying ? Fade(WHITE, 1.0f - e.deathT / 0.5f) : WHITE;
            int frame = animFrame(SPR.skull, e.animT, 8, true);
            drawAnim(SPR.skull, frame, at, sc, e.flip, tint);
            if (e.hitFlash > 0) { BeginBlendMode(BLEND_ADDITIVE); drawAnim(SPR.skull, frame, at, sc, e.flip); EndBlendMode(); }
        }
        else {
            const EnemyAnims& A = SPR.enemy[e.kind];
            const Anim* a = &A.idle; float t = e.animT; float fps = 8; bool loop = true;
            if (e.dying) { a = &A.death; t = e.deathT; fps = 14; loop = false; }
            else if (e.hitFlash > 0) { a = &A.hurt; t = 0.12f - e.hitFlash; fps = 40; loop = false; }
            else if (e.atkAnim > 0) { a = &A.attack; t = A.attack.length(16) - e.atkAnim; fps = 16; loop = false; }
            else if (e.moving) { a = &A.move; fps = 12; }
            int frame = animFrame(*a, t, fps, loop);
            drawAnim(*a, frame, at, sc, e.flip);
            if (e.hitFlash > 0) { BeginBlendMode(BLEND_ADDITIVE); drawAnim(*a, frame, at, sc, e.flip); EndBlendMode(); }   // struck: pure white
        }
    }
    else {
        drawShadow(e.pos, e.r);
        Color c = e.hitFlash > 0 ? Color{ 255,255,255,255 } : e.color;
        float w = sinf(e.wobble) * 2.0f;
        DrawRectangle((int)(e.pos.x - e.r), (int)(e.pos.y - e.r + w), (int)(e.r * 2), (int)(e.r * 2), c);
        DrawCircle((int)e.pos.x, (int)(e.pos.y - e.r + w), e.r * 0.7f, c);
    }
    // Bosses show their health in the big bar at the top of the screen instead.
    if (!e.dying && !e.boss && e.hp < e.maxHp) {
        float bw = e.r * 2.4f, top = e.pos.y - e.r * 2.0f - 6;
        DrawRectangle((int)(e.pos.x - bw / 2), (int)top, (int)bw, 4, { 20,10,10,200 });
        DrawRectangle((int)(e.pos.x - bw / 2), (int)top, (int)(bw * (e.hp / e.maxHp)), 4, { 200,60,60,255 });
    }
}

// raylib fills only counter-clockwise triangles; accept either winding.
static void fillTriangle(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) DrawTriangle(a, c, b, col); else DrawTriangle(a, b, c, col);
}

void drawProjectile(const Projectile& p, float t) {
    float ang = atan2f(p.vel.y, p.vel.x);
    switch (p.kind) {
    case ShotKind::Spell:
        if (p.spell == SP_FIRE) {
            // Emberhex flies as an animated fireball, rotated along its path, a tail of
            // flame behind it and a white-hot core
            Vector2 d = fromAngle(ang), side = { -d.y, d.x };
            Vector2 back = Vector2Subtract(p.pos, Vector2Scale(d, 34 + sinf(t * 30) * 5));
            BeginBlendMode(BLEND_ADDITIVE);
            fillTriangle(Vector2Add(p.pos, Vector2Scale(side, p.r + 3)), Vector2Subtract(p.pos, Vector2Scale(side, p.r + 3)), back, Fade({ 255, 110, 40, 255 }, 0.55f));
            DrawCircleV(p.pos, p.r + 10, Fade(p.glow, 0.16f));
            DrawCircleV(p.pos, p.r + 5, Fade(p.color, 0.3f));
            EndBlendMode();
            drawAnim(SPR.flame, animFrame(SPR.flame, 1.4f - p.life, 14, true), p.pos, 1.3f, false, WHITE, ang * RAD2DEG + 180.0f);
            DrawCircleV(p.pos, p.r * 0.8f, { 255, 226, 150, 255 });
            DrawCircleV(p.pos, p.r * 0.45f, WHITE);
        }
        else if (p.spell == SP_ICE) {       // the frost racing along the floor, raising spikes behind it
            DrawCircleV(p.pos, p.r + 4, Fade({ 170, 230, 255, 255 }, 0.3f));
            DrawCircleV(p.pos, p.r * 0.6f, { 225, 248, 255, 255 });
        }
        else if (p.spell == SP_BOLT) {      // a white-hot point, flickering at the head of its trail
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleV(p.pos, p.r + 6 + sinf(t * 70) * 3, Fade(p.color, 0.45f));
            EndBlendMode();
            DrawCircleV(p.pos, p.r, WHITE);
        }
        else { DrawCircleV(p.pos, p.r + 3, Fade(p.glow, 0.4f)); DrawCircleV(p.pos, p.r, p.color); }
        break;
    case ShotKind::Coin:
        if (SPR.coin.ok()) drawAnim(SPR.coin, animFrame(SPR.coin, t + p.pos.x * 0.01f, 14, true), p.pos, 1.2f);
        else DrawCircleV(p.pos, p.r, { 245, 215, 120, 255 });
        break;
    case ShotKind::Koi: {
        Vector2 d = fromAngle(ang), side = { -d.y, d.x };
        float wag = sinf(t * 14 + p.pos.x * 0.05f) * 3;
        Vector2 tail = Vector2Add(Vector2Subtract(p.pos, Vector2Scale(d, 10)), Vector2Scale(side, wag));
        fillTriangle(tail, Vector2Add(tail, Vector2Add(Vector2Scale(d, -6), Vector2Scale(side, 5))),
            Vector2Add(tail, Vector2Add(Vector2Scale(d, -6), Vector2Scale(side, -5))), { 255, 150, 60, 255 });
        DrawEllipse((int)p.pos.x, (int)p.pos.y, 8, 6, { 255, 130, 50, 255 });
        DrawCircleV(Vector2Add(p.pos, Vector2Scale(d, 3)), 3, { 255, 240, 230, 255 });
        break;
    }
    case ShotKind::Orb:
        DrawCircleV(p.pos, p.r * 2.0f, Fade(p.color, 0.25f));
        DrawCircleV(p.pos, p.r, p.color);
        DrawCircleV({ p.pos.x - p.r * 0.3f, p.pos.y - p.r * 0.3f }, p.r * 0.35f, Fade(WHITE, 0.7f));
        break;
    case ShotKind::Feather: {
        Vector2 d = Vector2Scale(fromAngle(ang), 9);
        DrawLineEx(Vector2Subtract(p.pos, d), Vector2Add(p.pos, d), 4, Fade({ 30, 40, 70, 255 }, 0.9f));
        DrawLineEx(Vector2Subtract(p.pos, d), Vector2Add(p.pos, d), 2, { 170, 200, 255, 255 });
        break;
    }
    case ShotKind::Ticket: {
        float spin = t * 6 * RAD2DEG + p.pos.x;
        DrawRectanglePro({ p.pos.x, p.pos.y, 14, 9 }, { 7, 4.5f }, spin, { 236, 226, 196, 255 });
        DrawRectanglePro({ p.pos.x, p.pos.y, 14, 2 }, { 7, 1 }, spin, { 200, 60, 60, 255 });
        break;
    }
    case ShotKind::Ember:
        DrawCircleV(p.pos, p.r * 2.2f, Fade({ 255, 110, 40, 255 }, 0.25f));
        DrawCircleV(p.pos, p.r, { 255, 150, 60, 255 });
        DrawCircleV(p.pos, p.r * 0.5f, { 255, 236, 180, 255 });
        break;
    case ShotKind::Boot: {      // a steel boot, its field lines streaming behind
        Vector2 d = fromAngle(ang), side = { -d.y, d.x };
        for (int i = -1; i <= 1; ++i)
            DrawLineEx(Vector2Add(Vector2Subtract(p.pos, Vector2Scale(d, 8)), Vector2Scale(side, i * 4.0f)),
                Vector2Add(Vector2Subtract(p.pos, Vector2Scale(d, 20)), Vector2Scale(side, i * 8.0f)), 1.5f, Fade({ 120, 200, 255, 255 }, 0.6f));
        DrawRectanglePro({ p.pos.x, p.pos.y, 14, 9 }, { 7, 4.5f }, ang * RAD2DEG, { 176, 186, 202, 255 });
        DrawCircleV(Vector2Add(p.pos, Vector2Add(Vector2Scale(d, 6), Vector2Scale(side, 2))), 4.5f, { 150, 160, 178, 255 });
        DrawLineEx(Vector2Add(Vector2Subtract(p.pos, Vector2Scale(d, 7)), Vector2Scale(side, 5)),
            Vector2Add(Vector2Add(p.pos, Vector2Scale(d, 9)), Vector2Scale(side, 5)), 2.5f, { 54, 58, 70, 255 });   // the sole
        break;
    }
    case ShotKind::Ball: {      // an iron ball off the end of her staff
        Vector2 d = fromAngle(ang);
        DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(d, 18)), p.pos, 3, Fade(WHITE, 0.3f));
        DrawCircleV(p.pos, p.r, { 70, 74, 88, 255 });
        DrawCircleV({ p.pos.x - p.r * 0.35f, p.pos.y - p.r * 0.35f }, p.r * 0.35f, { 196, 202, 216, 255 });
        break;
    }
    case ShotKind::Rock:        // a boulder, tumbling through the air
        drawAnim(SPR.rock, 0, p.pos, p.r * 2.6f / SPR.rock.fw, false, WHITE, t * 300 + p.pos.x);
        break;
    }
}

void drawBurn(const Enemy& e, float t) {
    float fade = fminf(1, e.burnT * 2), sc = e.r / 12;
    for (int i = -1; i <= 1; ++i)           // flames on the body, the sprite's head pointing up
        drawAnim(SPR.flame, animFrame(SPR.flame, t + i * 0.13f, 14, true), { e.pos.x + i * e.r * 0.6f, e.pos.y - e.r * (i ? 0.4f : 1.0f) },
            sc * (i ? 0.75f : 1.05f), false, Fade(WHITE, fade), 90);
}

void drawSpellCharge(int spell, Vector2 hand, float k, float t) {
    if (spell == SP_FIRE) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleV(hand, 8 + 14 * k, Fade({ 255, 110, 40, 255 }, 0.3f));
        DrawCircleV(hand, 4 + 8 * k, Fade({ 255, 190, 90, 255 }, 0.55f));
        EndBlendMode();
        drawAnim(SPR.flame, animFrame(SPR.flame, t, 14, true), hand, 0.4f + 0.9f * k, false, WHITE, 90);
        DrawCircleV(hand, 1 + 4 * k, WHITE);
        return;
    }
    float r = 34 * (1 - k) + 7;             // a ring of frost closing in, shards spinning with it
    DrawRing(hand, r - 1.5f, r, 0, 360, 32, Fade({ 190, 235, 255, 255 }, 0.3f + 0.6f * k));
    for (int i = 0; i < 6; ++i) {
        Vector2 d = fromAngle(t * 7 + i * PI / 3), side = { -d.y, d.x };
        Vector2 at = Vector2Add(hand, Vector2Scale(d, r)), tip = Vector2Add(at, Vector2Scale(d, -7 * k - 3));
        fillTriangle(Vector2Add(at, Vector2Scale(side, 2.5f)), Vector2Subtract(at, Vector2Scale(side, 2.5f)), tip, { 214, 244, 255, 255 });
    }
    DrawCircleV(hand, 1 + 3.5f * k, { 235, 250, 255, 255 });
}

void drawIceSpike(const IceSpike& s) {
    float g = fminf(1, s.t / 0.1f);
    g = 1 - (1 - g) * (1 - g);              // bursts up, then settles
    Vector2 rise = Vector2Scale(s.rise, g), tip = Vector2Add(s.pos, rise);
    Vector2 up = Vector2Normalize(s.rise);
    Vector2 n = Vector2Scale({ -up.y, up.x }, s.width * g);
    Vector2 a = Vector2Subtract(s.pos, n), b = Vector2Add(s.pos, n);
    DrawEllipse((int)s.pos.x, (int)s.pos.y, s.width * 1.6f, s.width * 0.7f, { 20, 50, 90, 110 });
    fillTriangle(Vector2Subtract(a, Vector2Scale(n, 0.35f)), Vector2Add(b, Vector2Scale(n, 0.35f)), Vector2Add(tip, Vector2Scale(up, 2)), { 24, 60, 110, 255 });
    fillTriangle(a, s.pos, tip, { 214, 244, 255, 245 });
    fillTriangle(s.pos, b, tip, { 118, 196, 236, 245 });
}

// A repeatable 0-1 value for part `i` of the pattern `seed`.
static float patternAt(float seed, int i) {
    float v = sinf(seed * 12.9898f + i * 78.233f) * 43758.547f;
    return v - floorf(v);
}

void drawCrack(const FloorCrack& c) {
    float g = fminf(1, c.t / 0.1f), fade = fminf(1, (CRACK_TIME - c.t) / 0.6f);
    DrawEllipse((int)c.pos.x, (int)c.pos.y, c.size * 0.9f * g, c.size * 0.55f * g, Fade(c.glow, 0.16f * fade));
    const int arms = 9;
    for (int i = 0; i < arms; ++i) {        // jagged lines running out from under the target
        float ang = i * 2 * PI / arms + patternAt(c.seed, i) * 0.5f;
        float len = c.size * g * (0.6f + 0.5f * patternAt(c.seed, i + 20));
        Vector2 at = c.pos;
        for (int seg = 1; seg <= 4; ++seg) {
            Vector2 d = fromAngle(ang + (patternAt(c.seed, i * 5 + seg) - 0.5f) * 0.8f);
            Vector2 next = { c.pos.x + d.x * len * seg / 4, c.pos.y + d.y * len * seg * 0.62f / 4 };   // lying flat on the floor
            DrawLineEx(at, next, 3.4f - seg * 0.5f, Fade({ 12, 16, 30, 255 }, 0.85f * fade));
            DrawLineEx(at, next, 1, Fade(c.glow, 0.7f * fade));
            at = next;
        }
    }
}

void drawArc(const Arc& a, float t) {
    float flicker = 0.45f + 0.55f * patternAt(a.seed, (int)(t * 45));
    float fade = (1 - a.t / a.life) * flicker;
    BeginBlendMode(BLEND_ADDITIVE);
    DrawLineEx(a.a, a.b, a.width * 3.2f, Fade({ 110, 160, 255, 255 }, 0.4f * fade));
    DrawLineEx(a.a, a.b, a.width, Fade({ 235, 245, 255, 255 }, fade));
    EndBlendMode();
}

void drawFlash(const Flash& f) {
    float k = f.t / f.life, left = 1 - k;
    BeginBlendMode(BLEND_ADDITIVE);
    if (f.ring) {
        float r = f.size * sqrtf(k);
        DrawCircleV(f.pos, r * 0.8f, Fade(f.color, 0.45f * left));
        DrawCircleV(f.pos, r * 0.45f, Fade(WHITE, 0.8f * left));
        DrawRing(f.pos, r * 0.85f, r, 0, 360, 32, Fade(f.color, 0.8f * left));
    }
    else {                                  // a four-pointed glint, one arm longer
        for (int q = 0; q < 2; ++q) {
            Vector2 d = fromAngle(f.ang + q * PI / 2), side = { -d.y, d.x };
            Vector2 a = Vector2Scale(d, f.size * (q ? 0.55f : 1.0f) * (0.6f + 0.4f * left));
            Vector2 w = Vector2Scale(side, 1 + 3.5f * left);
            Vector2 tip = Vector2Add(f.pos, a), tail = Vector2Subtract(f.pos, a);
            fillTriangle(Vector2Add(f.pos, w), Vector2Subtract(f.pos, w), tip, Fade(f.color, left));
            fillTriangle(Vector2Add(f.pos, w), Vector2Subtract(f.pos, w), tail, Fade(f.color, left));
        }
        DrawCircleV(f.pos, 3 + 6 * left, Fade(f.color, 0.8f * left));
    }
    EndBlendMode();
}

void drawStar(Vector2 c, float r, float turn, Color col) {
    for (int i = 0; i < 5; ++i) {
        float a = turn + i * 2 * PI / 5, b = a + PI / 5, n = a + 2 * PI / 5;
        Vector2 tip = Vector2Add(c, Vector2Scale(fromAngle(a), r));
        Vector2 dip = Vector2Add(c, Vector2Scale(fromAngle(b), r * 0.42f));
        Vector2 next = Vector2Add(c, Vector2Scale(fromAngle(n), r));
        fillTriangle(c, tip, dip, col);
        fillTriangle(c, dip, next, col);
    }
}

void drawChestLight(Vector2 mouth, float k, float t) {
    Color gold = { 255, 214, 120, 255 };
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient((int)mouth.x, (int)mouth.y, 58, Fade(gold, 0.55f * k), Fade(gold, 0));
    for (int i = 0; i < 7; ++i) {           // rays fanning up out of the chest
        float a = -PI / 2 + (i - 3) * 0.33f + sinf(t * 2 + i) * 0.05f, len = (62 + sinf(t * 3 + i * 2) * 12) * k;
        fillTriangle(mouth, Vector2Add(mouth, Vector2Scale(fromAngle(a - 0.075f), len)),
            Vector2Add(mouth, Vector2Scale(fromAngle(a + 0.075f), len)), Fade(gold, 0.26f * k));
    }
    EndBlendMode();
}

std::string findAsset(const char* file) {
    // Images live in the project's assets/ folder. Try several relative depths
    // so it works whether the cwd is the project dir (VS) or the build folder (exe).
    const char* roots[] = { "assets/", "../assets/", "../../assets/",
                            "../../../assets/", "../../../../assets/" };
    for (auto r : roots) { std::string p = std::string(r) + file; if (FileExists(p.c_str())) return p; }
    return "";
}
