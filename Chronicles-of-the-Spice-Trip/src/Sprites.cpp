// ============================================================================
//  Sprites.cpp — loading + drawing of the dungeon pack animations.
// ============================================================================
#include "Sprites.h"
#include "Render.h"
#include "Common.h"
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
static Anim loadStrip(const char* file, int frameW) {
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
    oracle = loadStrip("oracle.png", 16);
    flame = loadStrip("flame.png", 32);
    key = loadStrip("key.png", 16);
    rock = loadStrip("rock.png", 64);
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
                     &flaskRed, &flaskBlue, &flag, &coin, &skull, &oracle, &flame, &key, &rock, &arrow, &vapes, &spellIcons })
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

void drawAnim(const Anim& a, int frame, Vector2 center, float scale, bool flipX, Color tint, float rotation) {
    if (!a.ok()) return;
    Rectangle src = { (float)(frame * a.fw), 0, flipX ? -(float)a.fw : (float)a.fw, (float)a.fh };
    float w = a.fw * scale, h = a.fh * scale;
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

void WalkSheet::load(const char* file) {
    std::string p = findAsset((std::string("walk/") + file).c_str());
    if (p.empty()) return;
    tex = LoadTexture(p.c_str());
    fh = tex.height / 3;
    fw = fh * 3 / 4;
    frames = fw > 0 ? tex.width / fw : 0;
    if (frames < 2) {
        TraceLog(LOG_WARNING, "%s is not a walk sheet (three rows of 3:4 frames)", p.c_str());
        unload();
        return;
    }
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
    return 1 + (int)(time * 8) % (frames - 1);
}

void WalkSheet::draw(Facing facing, int frame, Rectangle dst, Color tint, bool flash) const {
    if (!ok()) return;
    int row = facing == FACE_DOWN ? 0 : facing == FACE_UP ? 1 : 2;
    Rectangle src = { (float)(frame * fw), (float)(row * fh), facing == FACE_LEFT ? -(float)fw : (float)fw, (float)fh };
    DrawTexturePro(tex, src, dst, { 0, 0 }, 0, tint);
    if (flash) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(tex, src, dst, { 0, 0 }, 0, WHITE);
        EndBlendMode();
    }
}
