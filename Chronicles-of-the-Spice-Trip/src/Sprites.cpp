// ============================================================================
//  Sprites.cpp — loading + drawing of the dungeon pack animations.
// ============================================================================
#include "Sprites.h"
#include "Render.h"
#include "Common.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <string>

Sprites SPR;

static Texture2D loadTexture(const char* file) {
    std::string p = findAsset((std::string("dungeon/") + file).c_str());
    if (p.empty()) return Texture2D{};
    Texture2D t = LoadTexture(p.c_str());
    SetTextureFilter(t, TEXTURE_FILTER_POINT);   // keep pixel art crisp
    return t;
}

// Load a strip whose frames are `frameW` wide (frame height = image height).
Anim loadStrip(const char* file, int frameW) {
    Anim a;
    a.tex = loadTexture(file);
    if (!a.ok()) return a;
    a.fw = frameW; a.fh = a.tex.height;
    a.frames = a.tex.width / frameW;
    if (a.frames < 1) a.frames = 1;
    return a;
}

static EnemyAnims loadEnemy(const char* prefix, int frameW = 32) {
    EnemyAnims e;
    std::string p = prefix;
    e.idle = loadStrip((p + "_idle.png").c_str(), frameW);
    e.move = loadStrip((p + "_move.png").c_str(), frameW);
    e.attack = loadStrip((p + "_attack.png").c_str(), frameW);
    e.hurt = loadStrip((p + "_hurt.png").c_str(), frameW);
    e.death = loadStrip((p + "_death.png").c_str(), frameW);
    return e;
}

void Sprites::load() {
    tileset = loadTexture("tileset.png");
    peaks = loadStrip("peaks.png", 16);
    chest = loadStrip("chest.png", 16);
    chestOpen = loadStrip("chest_open.png", 16);
    torch = loadStrip("torch.png", 16);
    sideTorch = loadStrip("side_torch.png", 16);
    candle = loadStrip("candlestick.png", 16);
    candle2 = loadStrip("candlestick2.png", 16);
    flaskRed = loadStrip("flask_red.png", 16);
    flaskBlue = loadStrip("flask_blue.png", 16);
    flag = loadStrip("flag.png", 16);
    coin = loadStrip("coin.png", 16);
    skull = loadStrip("skull.png", 16);
    flame = loadStrip("flame.png", 32);
    key = loadStrip("key.png", 16);
    keySilver = loadStrip("key_silver.png", 16);
    rock = loadStrip("rock.png", 64);
    galinSmoke = loadStrip("galin_smoke.png", 60);
    daniilSling = loadStrip("daniil_sling.png", 100);
    filThrow = loadStrip("filqcheto_throw.png", 100);
    krisaPunch = loadStrip("krisa_punch.png", 100);
    grinder = loadStrip("grinder.png", 65);
    grinderBig = loadStrip("grinder_big.png", 125);
    castleLadder = loadStrip("castle_ladder.png", 36);
    bossStairs = loadStrip("boss_stairs.png", 60);
    vapes = loadStrip("vapes.png", 32);
    spellIcons = loadStrip("spell_icons.png", 32);
    enemy[EK_SKELETON] = loadEnemy("skel1");
    enemy[EK_SKELETON2] = loadEnemy("skel2");
    enemy[EK_VAMPIRE] = loadEnemy("vamp");
    enemy[EK_GAKEV] = loadEnemy("thief");
    enemy[EK_MELON] = loadEnemy("melon");
    enemy[EK_TOSHKO] = loadEnemy("toshko", 48);
    enemy[EK_SOLDIER] = loadEnemy("soldier", 100);
    enemy[EK_ORC] = loadEnemy("orc", 100);
    enemy[EK_DEMON] = loadEnemy("demon", 100);
    enemy[EK_BLOOD] = loadEnemy("blood", 100);
    arrow = loadStrip("arrow.png", 32);
}

void Sprites::unload() {
    auto un = [](Texture2D& t) { if (t.id) UnloadTexture(t); t = Texture2D{}; };
    un(tileset);
    for (Anim* a : { &peaks, &chest, &chestOpen, &torch, &sideTorch, &candle, &candle2,
                     &flaskRed, &flaskBlue, &flag, &coin, &skull, &flame, &key, &keySilver, &rock, &arrow, &vapes, &spellIcons, &galinSmoke, &daniilSling, &filThrow, &krisaPunch, &grinder, &grinderBig, &castleLadder, &bossStairs })
        un(a->tex);
    for (auto& e : enemy)
        for (Anim* a : { &e.idle, &e.move, &e.attack, &e.hurt, &e.death }) un(a->tex);
}

int animFrame(const Anim& a, float time, float fps, bool loop) {
    int f = (int)(time * fps);
    if (f < 0) f = 0;
    if (loop) return f % a.frames;
    return f >= a.frames ? a.frames - 1 : f;
}

Rectangle snapToPixels(Rectangle r) {
    // the world-to-screen scale and shift in effect (the camera's, or none for the HUD)
    Matrix m = rlGetMatrixModelview();
    float sx = m.m0, sy = m.m5;
    if (fabsf(sx) < 1e-4f || fabsf(sy) < 1e-4f || fabsf(m.m1) > 1e-4f || fabsf(m.m4) > 1e-4f) return r;   // turned: leave it
    float x0 = roundf(r.x * sx + m.m12), y0 = roundf(r.y * sy + m.m13);
    float w = roundf(r.width * sx), h = roundf(r.height * sy);
    return { (x0 - m.m12) / sx, (y0 - m.m13) / sy, w / sx, h / sy };
}

void drawAnim(const Anim& a, int frame, Vector2 center, float scale, bool flipX, Color tint, float rotation) {
    if (!a.ok()) return;
    Rectangle src = { (float)(frame * a.fw), 0, flipX ? -(float)a.fw : (float)a.fw, (float)a.fh };
    float w = a.fw * scale, h = a.fh * scale;
    if (rotation == 0) {                            // on whole pixels, so it doesn't shimmer as it moves
        Rectangle d = snapToPixels({ center.x - w / 2, center.y - h / 2, w, h });
        DrawTexturePro(a.tex, src, d, { 0, 0 }, 0, tint);
        return;
    }
    DrawTexturePro(a.tex, src, { center.x, center.y, w, h }, { w / 2, h / 2 }, rotation, tint);
}

void drawAnimOnFoot(const Anim& a, int frame, Vector2 foot, float w, float h, float rotation) {
    if (!a.ok()) return;
    DrawTexturePro(a.tex, { (float)(frame * a.fw), 0, (float)a.fw, (float)a.fh }, { foot.x, foot.y, w, h }, { w / 2, h }, rotation, WHITE);
}

void drawTile(int col, int row, float x, float y, Color tint) {
    if (!SPR.tileset.id) return;
    DrawTexturePro(SPR.tileset, { col * 16.0f, row * 16.0f, 16, 16 }, { x, y, (float)TILE, (float)TILE },
        { 0, 0 }, 0, tint);
}

Facing facingOf(Vector2 dir) {
    if (fabsf(dir.x) > fabsf(dir.y)) return dir.x > 0 ? FACE_RIGHT : FACE_LEFT;
    return dir.y < 0 ? FACE_UP : FACE_DOWN;
}

void WalkSheet::load(const char* file, const char* dir) {
    std::string p = findAsset((std::string(dir) + file).c_str());
    if (p.empty()) return;
    Image img = LoadImage(p.c_str());
    if (!img.data) return;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    fh = img.height / 3;
    fw = fh * 3 / 4;
    frames = fw > 0 ? img.width / fw : 0;
    if (frames < 2) {
        TraceLog(LOG_WARNING, "%s is not a walk sheet (three rows of 3:4 frames)", p.c_str());
        UnloadImage(img);
        *this = WalkSheet{};
        return;
    }
    // Measure every frame's figure: where its feet are and how tall it stands.
    const Color* px = (const Color*)img.data;
    struct Box { int top = -1, bottom = -1; };
    std::vector<Box> box(3 * frames);
    for (int row = 0; row < 3; ++row)
        for (int f = 0; f < frames; ++f) {
            Box& b = box[row * frames + f];
            for (int y = 0; y < fh; ++y)
                for (int x = 0; x < fw; ++x)
                    if (px[(row * fh + y) * img.width + f * fw + x].a > 40) {
                        if (b.top < 0) b.top = y;
                        b.bottom = y + 1;
                        break;
                    }
        }
    // Feet on one line (the front view's, standing), every side as tall as the front, and no
    // step dipping more than a touch below standing height.
    const Box& ref = box[0];
    footLine = ref.bottom > 0 ? (float)ref.bottom : (float)fh;
    float refH = ref.bottom > ref.top && ref.top >= 0 ? (float)(ref.bottom - ref.top) : (float)fh;
    fit.assign(3 * frames, Fit{ 1, footLine });
    for (int row = 0; row < 3; ++row) {
        const Box& stand = box[row * frames];
        float standH = stand.top >= 0 ? (float)(stand.bottom - stand.top) : refH;
        float rowScale = std::clamp(refH / standH, 0.9f, 1.12f);
        for (int f = 0; f < frames; ++f) {
            const Box& b = box[row * frames + f];
            if (b.top < 0) continue;
            float h = (float)(b.bottom - b.top);
            fit[row * frames + f] = { rowScale * std::max(1.0f, 0.975f * standH / h), (float)b.bottom };
        }
    }
    tex = LoadTextureFromImage(img);
    UnloadImage(img);
    // The drawn figures are tiny pixel art: keep them crisp. A sheet cut from a big
    // painting (Momiche's) is drawn far smaller than it is, so give it mipmaps.
    if (fh > 64) { GenTextureMipmaps(&tex); SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR); }
    else SetTextureFilter(tex, TEXTURE_FILTER_POINT);
}

void WalkSheet::unload() {
    if (tex.id) UnloadTexture(tex);
    *this = WalkSheet{};
}

int WalkSheet::frameAt(bool walking, float time) const {
    if (!walking || frames < 2) return 0;
    return 1 + (int)(fmaxf(0, time) * 8) % (frames - 1);
}

void WalkSheet::draw(Facing facing, int frame, Rectangle dst, Color tint, bool flash, float rotation) const {
    if (!ok()) return;
    int row = facing == FACE_DOWN ? 0 : facing == FACE_UP ? 1 : 2;
    frame = std::clamp(frame, 0, frames - 1);
    Rectangle src = { (float)(frame * fw), (float)(row * fh), facing == FACE_LEFT ? -(float)fw : (float)fw, (float)fh };
    if (rotation == 0) {                            // standing up: feet planted, steady height, whole pixels
        Fit k = (size_t)(row * frames + frame) < fit.size() ? fit[row * frames + frame] : Fit{ 1, footLine };
        float sy = dst.height / fh, footY = dst.y + footLine * sy;
        Rectangle d = { dst.x + dst.width * (1 - k.scale) / 2, footY - k.foot * sy * k.scale, dst.width * k.scale, dst.height * k.scale };
        d = snapToPixels(d);
        DrawTexturePro(tex, src, d, { 0, 0 }, 0, tint);
        if (flash) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(tex, src, d, { 0, 0 }, 0, WHITE);
            EndBlendMode();
        }
        return;
    }
    Vector2 origin = { dst.width / 2, dst.height / 2 };
    dst.x += origin.x; dst.y += origin.y;          // DrawTexturePro places the origin at dst.x, y
    DrawTexturePro(tex, src, dst, origin, rotation, tint);
    if (flash) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(tex, src, dst, origin, rotation, WHITE);
        EndBlendMode();
    }
}
