// ============================================================================
//  Sprites.h — textures + strip animations from the dungeon asset packs.
//  Every animation is a horizontal strip of equal-width frames in
//  assets/dungeon/. Missing files leave tex.id == 0 and callers fall back to
//  the procedural drawing.
// ============================================================================
#pragma once

#include "raylib.h"

struct Anim {
    Texture2D tex{};
    int frames = 1, fw = 0, fh = 0;
    bool ok() const { return tex.id != 0; }
    float length(float fps) const { return frames / fps; }
};

// Frame index for an animation that started `time` seconds ago.
int animFrame(const Anim& a, float time, float fps, bool loop);

// Draw one frame centred at `center` (scaled), optionally mirrored.
void drawAnim(const Anim& a, int frame, Vector2 center, float scale, bool flipX = false,
    Color tint = WHITE, float rotation = 0.0f);

// Draw one frame squashed or stretched to w x h, standing on `foot` (it tilts about that point too).
void drawAnimOnFoot(const Anim& a, int frame, Vector2 foot, float w, float h, float rotation = 0.0f);

// The full animation set for one enemy type (Enemy_Animations_Set).
struct EnemyAnims { Anim idle, move, attack, hurt, death; };

enum EnemyKind { EK_SKELETON = 0, EK_SKELETON2 = 1, EK_VAMPIRE = 2, EK_SKULL = 3, EK_GAKEV = 4, EK_MELON = 5, EK_TOSHKO = 6,
                 EK_SOLDIER = 7, EK_ORC = 8, EK_DEMON = 9, EK_BLOOD = 10 };

// How fast a kind's attack animation plays: the bow and the axe take their time.
inline float attackFps(int kind) { return kind == EK_SOLDIER ? 12.0f : kind == EK_ORC ? 9.0f : 16.0f; }

struct Sprites {
    Texture2D tileset{};                       // 16px tiles, 10x10
    Anim peaks, chest, chestOpen, torch, sideTorch, candle, candle2;
    Anim flaskRed, flaskBlue, flag, coin, skull, oracle, flame, key, rock, arrow;
    Anim vapes, spellIcons;                    // 32px icons: one frame per Vape / SPELLS entry
    EnemyAnims enemy[11];                     // indexed by EnemyKind (skull slot unused)

    void load();
    void unload();
};

extern Sprites SPR;

// Draw a 16px tileset cell (col,row) scaled to fill a TILE at (x,y).
void drawTile(int col, int row, float x, float y, Color tint = WHITE);

// Which way a figure faces, in screen terms (y pointing down).
enum Facing { FACE_DOWN = 0, FACE_UP = 1, FACE_LEFT = 2, FACE_RIGHT = 3 };
Facing facingOf(Vector2 dir);

// A champion's walking figure from every side: assets/walk/walk-<id>.png, exported
// from the Unity project (Spice Trip > Export Walk Sheets). Rows are down, up and
// right; each row is the standing frame, then the walk cycle. Left is the right
// row mirrored. Frames are 3:4.
struct WalkSheet {
    Texture2D tex{};
    int frames = 0, fw = 0, fh = 0;
    bool ok() const { return tex.id != 0; }
    void load(const char* file);
    void unload();
    // The frame index for standing (0), or stepping through the walk cycle at 8 frames a second.
    int  frameAt(bool walking, float time) const;
    // Draws one frame stretched over `dst`; `flash` adds a white flash (struck).
    void draw(Facing facing, int frame, Rectangle dst, Color tint = WHITE, bool flash = false) const;
};
