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

// The full animation set for one enemy type (Enemy_Animations_Set).
struct EnemyAnims { Anim idle, move, attack, hurt, death; };

enum EnemyKind { EK_SKELETON = 0, EK_SKELETON2 = 1, EK_VAMPIRE = 2, EK_SKULL = 3, EK_THIEF = 4, EK_MELON = 5 };

struct Sprites {
    Texture2D tileset{};                       // 16px tiles, 10x10
    Anim peaks, chest, chestOpen, torch, sideTorch, candle, candle2;
    Anim flaskRed, flaskBlue, flag, coin, skull, oracle, flame;
    Anim vapes, spellIcons;                    // 32px icons: one frame per Vape / SPELLS entry
    EnemyAnims enemy[6];                      // indexed by EnemyKind (skull slot unused)

    void load();
    void unload();
};

extern Sprites SPR;

// Draw a 16px tileset cell (col,row) scaled to fill a TILE at (x,y).
void drawTile(int col, int row, float x, float y, Color tint = WHITE);
