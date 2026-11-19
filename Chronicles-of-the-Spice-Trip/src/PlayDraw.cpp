// ============================================================================
//  PlayDraw.cpp — the dungeon on screen: room, actors, HUD and overlays.
// ============================================================================
#include "Game.h"
#include "Look.h"
#include "Input.h"
#include "Touch.h"
#include "Render.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
#include <string>

// --------------------------------------------------------------- the screen
void Game::playScreen() {
    float dt = fminf(GetFrameTime(), 0.05f);
    if (coop.joined()) {                            // co-op, joined: the world comes from the host
        coopJoinerFrame();
        if (screen != Screen::Play || !coop.joined()) return;
        if (!coop.haveWorld) {
            ui::textCentered("Waiting for the host...", GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f - 10, 20, ui::MUTED);
            if (in::pressed(KEY_ESCAPE)) coopLeave();
            return;
        }
        Overlay before = overlay;
        playFrame(dt);
        if (screen == Screen::Play && coop.joined()) coopOverlayChanged(before);
        return;
    }
    if (coop.hosting() && coop.started) {           // co-op, hosting: every hero and the world, whatever's open here
        coopStep(dt);
        if (screen != Screen::Play) return;
    }
    playFrame(dt);
}

void Game::playFrame(float dt) {
    bool alone = !coop.active();                    // (in co-op the world moves on above, whatever window is open)
    if (in::pressed(KEY_ESCAPE)) {
        if (overlay == Overlay::Controls) { save::writeTouchLayout(touch::layout()); overlay = Overlay::Pause; }
        else if (overlay == Overlay::Shop || overlay == Overlay::Galin || overlay == Overlay::Drip || overlay == Overlay::Pause) overlay = Overlay::None;
        else if (overlay == Overlay::Casino && !wheel.spinning) overlay = Overlay::None;
        else if (overlay == Overlay::None) overlay = Overlay::Pause;
        play("ui");
    }
    if (hall.active) {                              // the castle: its own rooms, shopkeepers and HUD
        if (alone && overlay == Overlay::None) updateCastle(dt);
        if (screen != Screen::Play) return;
        if (hall.active) {
            drawCastle();
            drawCastleHud();
            switch (overlay) {
            case Overlay::Shop:     drawShop(); break;
            case Overlay::Galin:    drawGalin(); break;
            case Overlay::Casino:   drawCasino(); break;
            case Overlay::Drip:     drawDrip(); break;
            case Overlay::Pause:    drawPause(); break;
            case Overlay::Controls: drawControlsEditor(); break;
            default: break;
            }
            return;
        }
    }
    if (hunt.active) {                              // the toilets: their own map, rules and HUD
        if (alone && overlay == Overlay::None) updateToilet(dt);
        if (screen != Screen::Play) return;
        drawToilet();
        drawToiletHud();
        if (overlay == Overlay::Pause) drawPause();
        else if (overlay == Overlay::Controls) drawControlsEditor();
        return;
    }
    if (alone && overlay == Overlay::None) updatePlay(dt);
    if (screen != Screen::Play) return;            // left for the menu this frame
    if (hall.active) { drawCastle(); drawCastleHud(); return; }     // just went through the castle door
    if (hunt.active) { drawToilet(); drawToiletHud(); return; }     // or down Triceps' stairs

    // the lair darkens while its boss lives (painted arenas less, so the art shows)
    Room& r = dungeon.cur();
    bool painted = r.isBoss && bossArt.arena[currentBossIndex()].id;
    float target = r.isBoss && !r.cleared ? (painted ? 0.16f : 0.38f) : 0.0f;
    gloom += (target - gloom) * fminf(1.0f, dt * 2.5f);

    Camera2D cam = camera();
    if (shake > 0 && overlay == Overlay::None) {
        shake = fmaxf(0, shake - dt);
        float amp = 14.0f * shake;
        cam.offset.x += frand(-amp, amp); cam.offset.y += frand(-amp, amp);
    }
    look::begin(cam, pixelSize(), darkness());
    drawRoom();
    drawActors();
    look::end();

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (gloom > 0.001f && !look::lighting()) DrawRectangle(0, 0, sw, sh, Fade({ 4, 2, 10, 255 }, gloom));
    DrawRectangle(0, 0, sw, sh, Fade(THEMES[theme()].fog, theme() ? 0.07f : 0.04f));
    if (player.auraT > 0) DrawRectangle(0, 0, sw, sh, Fade({ 120, 180, 255, 255 }, 0.06f));
    if (skyFlash > 0) DrawRectangle(0, 0, sw, sh, Fade({ 205, 225, 255, 255 }, skyFlash * 0.2f));
    if (floor != shownFloor) { arriveFade = shownFloor > 0 && floor > shownFloor ? 1.0f : 0.6f; shownFloor = floor; }
    if (arriveFade > 0) {                           // down the stairs: the new floor comes up out of the dark
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, fminf(1, arriveFade * 1.4f)));
        arriveFade = fmaxf(0, arriveFade - GetFrameTime());
    }
    drawHud();

    switch (overlay) {
    case Overlay::Shop:        drawShop(); break;
    case Overlay::Galin:       drawGalin(); break;
    case Overlay::Pause:       drawPause(); break;
    case Overlay::Controls:    drawControlsEditor(); break;
    case Overlay::RunComplete: drawRunComplete(); break;
    case Overlay::RushOver:    drawRushOver(); break;
    case Overlay::Casino:      drawCasino(); break;
    case Overlay::Summary:     drawSummary(); break;
    case Overlay::Cutscene:    drawCutscene(); break;
    default: break;
    }
}

// ----------------------------------------------------------------- the room
// The boss key: the pack's spinning gold key, or a drawn one if the sprite is missing.
static void drawKey(Vector2 c, float s, bool silver = false) {
    const Anim& k = silver ? SPR.keySilver : SPR.key;
    if (k.ok()) { drawAnim(k, animFrame(k, (float)GetTime(), 6, true), c, 2.0f * s); return; }
    Color gold = { 240, 200, 80, 255 }, dark = { 120, 80, 20, 255 };
    DrawRing(c, 4 * s, 7.5f * s, 0, 360, 16, dark);
    DrawRing(c, 3 * s, 6 * s, 0, 360, 16, gold);
    DrawRectangleRec({ c.x + 5 * s, c.y - 1.5f * s, 12 * s, 3 * s }, gold);
    DrawRectangleRec({ c.x + 12 * s, c.y + 1.5f * s, 2.5f * s, 4 * s }, gold);
    DrawRectangleRec({ c.x + 16 * s, c.y + 1.5f * s, 2.5f * s, 3 * s }, gold);
}

void Game::drawRoom() {
    Room& r = dungeon.cur();
    Texture2D arena = r.isBoss ? bossArt.arena[currentBossIndex()] : Texture2D{};
    bool bossDark = r.isBoss && !r.cleared;
    float trapPh = fmodf(trapClock, 2.4f);
    bool spikesUp = trapPh < 1.2f;
    int spikeFrame = trapPh < 1.2f ? 0 : trapPh < 1.35f ? 1 : trapPh < 2.25f ? 2 : 3;     // the old pack's spikes
    // the spike trap: up (and hurting) for 1.2 s, then down; the tips rattle in their holes
    // for half a second before they shoot up again
    int trapFrame = trapPh < 0.05f ? 2 : trapPh < 1.2f ? (trapPh > 0.55f && trapPh < 0.65f ? 4 : 3)
                  : trapPh < 1.27f ? 5 : trapPh < 1.34f ? 6 : trapPh < 1.9f ? 0
                  : trapPh < 2.25f ? ((int)(trapPh * 18) & 1) : 1;
    int midX = ROOM_TW / 2, midY = ROOM_TH / 2;

    for (int ty = 0; ty < ROOM_TH; ++ty)
        for (int tx = 0; tx < ROOM_TW; ++tx) {
            float px = tx * (float)TILE, py = ty * (float)TILE;
            bool border = tx == 0 || ty == 0 || tx == ROOM_TW - 1 || ty == ROOM_TH - 1;
            if (arena.id && !border) continue;          // the painting covers the floor
            int dir = 0;
            if (ty == 0 && tx == midX) dir = DIR_N;
            else if (ty == ROOM_TH - 1 && tx == midX) dir = DIR_S;
            else if (tx == 0 && ty == midY) dir = DIR_W;
            else if (tx == ROOM_TW - 1 && ty == midY) dir = DIR_E;
            bool isDoor = dir && (r.doors & dir);
            bool innerWall = !border && (r.tiles[ty][tx] == RT_WALL || r.tiles[ty][tx] == RT_FLAMER);
            if (!border && r.tiles[ty][tx] == RT_VOID) {    // outside the room's shape
                bool edge = false;                         // void touching floor reads as a wall face
                for (int k = 0; k < 4; ++k) {
                    int nx = tx + (k == 0) - (k == 1), ny = ty + (k == 2) - (k == 3);
                    if (nx > 0 && ny > 0 && nx < ROOM_TW - 1 && ny < ROOM_TH - 1 && !tileSolid(r.tiles[ny][nx])) edge = true;
                }
                DrawRectangle((int)px, (int)py, TILE, TILE, { 10, 9, 14, 255 });
                if (edge) {
                    if (SPR.tileset.id) drawTile(1 + ((tx * 7 + ty * 3) % 4), 0, px, py);
                    else { DrawRectangle((int)px, (int)py, TILE, TILE, { 24, 22, 32, 255 }); DrawRectangle((int)px, (int)py, TILE, 6, { 58, 54, 74, 255 }); }
                }
                continue;
            }
            unsigned h = (unsigned)(tx * 73856093) ^ (unsigned)(ty * 19349663) ^
                (unsigned)(dungeon.cx * 83492791 + dungeon.cy * 2654435761u);

            if (!SPR.tileset.id) {      // procedural fallback when the tileset is missing
                if ((!border || isDoor) && !innerWall) {
                    DrawRectangle((int)px, (int)py, TILE, TILE, ((tx + ty) & 1) ? Color{ 46, 44, 60, 255 } : Color{ 40, 38, 52, 255 });
                    if (isDoor && !r.cleared) DrawRectangle((int)px + 3, (int)py + 3, TILE - 6, TILE - 6, { 120, 86, 54, 255 });
                }
                else {
                    DrawRectangle((int)px, (int)py, TILE, TILE, { 24, 22, 32, 255 });
                    DrawRectangle((int)px, (int)py, TILE, 6, { 58, 54, 74, 255 });
                }
                if (!border && r.tiles[ty][tx] == RT_SPIKES && spikesUp)
                    DrawRectangle((int)px + 6, (int)py + 6, TILE - 12, TILE - 12, { 200, 200, 210, 255 });
                continue;
            }
            if ((!border || isDoor) && !innerWall) {
                drawTile(6 + h % 4, (h >> 4) % 3, px, py);                 // stone floor variants
                if (isDoor && (!r.cleared || dungeon.doorLock(dir))) drawTile(8, 3, px, py);   // barred door
                else if (!border && r.tiles[ty][tx] == RT_SPIKES) {
                    if (SPR.spikeTrap.ok()) drawAnim(SPR.spikeTrap, trapFrame, { px + TILE / 2.0f, py + TILE / 2.0f - 8 }, 2.0f);
                    else drawAnim(SPR.peaks, spikeFrame, { px + TILE / 2.0f, py + TILE / 2.0f }, 2.0f);
                }
                if (ty == 1 && tx == 1 && !r.isBoss) drawTile(4, 6, px, py);              // cobwebs
                if (ty == 1 && tx == ROOM_TW - 2 && !r.isBoss) drawTile(5, 6, px, py);
            }
            else if (innerWall) drawTile(1 + (h >> 4) % 4, 0, px, py);   // brick block
            else if (ty == 0) drawTile(tx == 0 ? 0 : tx == ROOM_TW - 1 ? 5 : 1 + tx % 4, 0, px, py);
            else if (ty == ROOM_TH - 1) drawTile(tx == 0 ? 0 : tx == ROOM_TW - 1 ? 5 : 1 + tx % 4, 4, px, py);
            else drawTile(tx == 0 ? 0 : 5, 1 + ty % 3, px, py);
        }

    // the floor's theme washes over the stone (painted arenas keep their own colours)
    Color tint = THEMES[theme()].tint;
    if (!arena.id && (tint.r < 255 || tint.g < 255 || tint.b < 255)) {
        BeginBlendMode(BLEND_MULTIPLIED);
        DrawRectangle(0, 0, (int)ROOM_PX_W, (int)ROOM_PX_H, tint);
        EndBlendMode();
    }

    // fire-throwers: an iron spout on the brick, coughing flame, then a roaring jet
    for (int i = 0; i < r.flamerCount; ++i) {
        const Flamer& f = r.flamers[i];
        int state = flamerState(f, trapClock);
        Vector2 c = { f.tx * (float)TILE + TILE / 2.0f, f.ty * (float)TILE + TILE / 2.0f };
        Vector2 d = { (float)((f.dir == DIR_E) - (f.dir == DIR_W)), (float)((f.dir == DIR_S) - (f.dir == DIR_N)) };
        float rot = atan2f(d.y, d.x) * RAD2DEG;
        Vector2 mouth = Vector2Add(c, Vector2Scale(d, 10));
        float flick = look::flicker(clock, (float)i * 2.3f);
        if (!SPR.flamer.ok() || !SPR.flameJet.id) {         // the art is missing: the old blobs of fire
            DrawRectangle((int)c.x - 11, (int)c.y - 11, 22, 22, { 70, 66, 74, 255 });
            DrawCircleV(mouth, 3.5f, state == 0 ? Color{ 90, 40, 30, 255 } : Color{ 255, 150, 50, 255 });
            if (state == 2) {
                int fx[FLAMER_RANGE], fy[FLAMER_RANGE];
                float len = flameTiles(r, f, fx, fy) * (float)TILE;
                for (float s2 = 0; s2 < len; s2 += 7)
                    DrawCircleV(Vector2Add(mouth, Vector2Scale(d, s2)), 7 + 7 * s2 / fmaxf(len, 1), Fade({ 255, 130, 40, 255 }, 0.7f));
            }
            continue;
        }
        const float JW = 76, JH = 24;                       // a frame of the jet (drawn under the barrel it comes out of)
        int frame = (int)(clock * 16 + i * 3) % 8;
        if (state == 1) {                                   // coughing: flame and smoke out of the mouth
            DrawTexturePro(SPR.flameJet, { frame * JW, 4 * JH, JW, JH }, { mouth.x, mouth.y, JW * 2, JH * 2 }, { 0, JH }, rot, WHITE);
            look::light(Vector2Add(mouth, Vector2Scale(d, 10)), 54, { 255, 120, 40, 255 }, 0.7f * flick);
            look::glow(Vector2Add(mouth, Vector2Scale(d, 6)), 12, { 255, 160, 70, 255 }, 0.35f);
        }
        int fx[FLAMER_RANGE], fy[FLAMER_RANGE];
        int n = state == 2 ? flameTiles(r, f, fx, fy) : 0;
        if (n > 0) {                                        // the jet: it races out to its full reach, then roars
            float ph = fmodf(trapClock + f.phase, FLAMER_CYCLE) - (FLAMER_CYCLE - FLAMER_BURN);
            int reach = std::clamp((int)ceilf(n * ph / 0.14f), 1, n);
            float fade = fminf(1, (FLAMER_BURN - ph) / 0.12f);
            DrawTexturePro(SPR.flameJet, { frame * JW, (reach - 1) * JH, JW, JH }, { mouth.x, mouth.y, JW * 2, JH * 2 }, { 0, JH }, rot,
                Fade(WHITE, 0.35f + 0.65f * fade));
            Vector2 end = Vector2Add(mouth, Vector2Scale(d, reach * (float)TILE));
            look::lightLine(Vector2Add(mouth, Vector2Scale(d, 8)), end, 30, 92, { 255, 128, 44, 255 }, 1.05f * flick * fade);
            for (int k = 0; k < reach; ++k)
                look::glow(Vector2Add(mouth, Vector2Scale(d, 14 + k * (float)TILE)), 26, { 255, 150, 60, 255 }, 0.22f * fade);
        }
        drawAnim(SPR.flamer, state, c, 2.0f, false, WHITE, rot);
    }

    if (arena.id) {             // the boss's painted arena, framed by the walls
        Rectangle inner = { (float)TILE, (float)TILE, ROOM_PX_W - 2 * TILE, ROOM_PX_H - 2 * TILE };
        ui::drawCover(arena, inner, r.cleared ? WHITE : Color{ 214, 208, 226, 255 });
        DrawRectangleLinesEx(inner, 3, Color{ 0, 0, 0, 150 });
    }

    // wall torches (unlit in the boss lair), banners, candles; each torch throws a warm pool of light
    const Color TORCH = { 255, 150, 72, 255 }, FLAME = { 255, 176, 84, 255 };
    if (SPR.torch.ok())
        for (float x : { 4 * (float)TILE + 16, (ROOM_TW - 5) * (float)TILE + 16 }) {
            if (bossDark) { drawTile(2, 9, x - 16, 14 - 16); continue; }
            drawAnim(SPR.torch, animFrame(SPR.torch, clock + x * 0.01f, 8, true), { x, 14 }, 2.0f);
            look::light({ x, 34 }, 150, TORCH, 1.1f * look::flicker(clock, x));
            look::glow({ x, 10 }, 15, FLAME, 0.4f);
        }
    if (SPR.sideTorch.ok())
        for (int ty : { 2, ROOM_TH - 3 })
            for (float x : { 18.0f, ROOM_PX_W - 18 }) {
                Vector2 at = { x, ty * (float)TILE + 16 };
                if (bossDark) { drawTile(2, 9, at.x - 16, at.y - 16); continue; }
                drawAnim(SPR.sideTorch, animFrame(SPR.sideTorch, clock + x * 0.01f, 8, true), at, 2.0f, x > ROOM_PX_W / 2);
                look::light({ x < ROOM_PX_W / 2 ? x + 18 : x - 18, at.y }, 130, TORCH, 1.0f * look::flicker(clock, x + ty * 7));
                look::glow({ x, at.y - 6 }, 13, FLAME, 0.35f);
            }
    if (r.isBoss && !arena.id)
        for (int i = 0; i < 4; ++i)
            drawAnim(SPR.flag, animFrame(SPR.flag, clock + i * 0.3f, 6, true), { (2 + i * 5 + (i > 1 ? 2 : 0)) * (float)TILE + 16, 14 }, 2.0f);
    if (r.isStart && mode == Mode::Run) {
        // the castle: a ladder up the top wall to a little door, a torch beside it, light under the door
        bool open = ladderDoorOpen();
        float glow = 0.5f + 0.5f * sinf(clock * 2.3f) * sinf(clock * 3.7f + 1);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient((int)CASTLE_DOOR_X, 30, open ? 64.0f : 40.0f, Fade({ 255, 170, 90, 255 }, (open ? 0.45f : 0.16f) + 0.08f * glow), BLANK);
        EndBlendMode();
        if (SPR.castleLadder.ok()) drawAnimOnFoot(SPR.castleLadder, open ? 1 : 0, { CASTLE_DOOR_X, 108 }, 72, 124);
        else {
            DrawRectangle((int)CASTLE_DOOR_X - 18, -8, 36, 40, { 96, 58, 32, 255 });
            for (float y = 36; y < 106; y += 10) DrawRectangle((int)CASTLE_DOOR_X - 12, (int)y, 24, 3, { 137, 90, 69, 255 });
            DrawRectangle((int)CASTLE_DOOR_X - 14, 32, 4, 74, { 137, 90, 69, 255 });
            DrawRectangle((int)CASTLE_DOOR_X + 10, 32, 4, 74, { 137, 90, 69, 255 });
        }
        if (!open) {                                       // light under the shut door
            BeginBlendMode(BLEND_ADDITIVE);
            DrawRectangle((int)CASTLE_DOOR_X - 14, 29, 28, 2, Fade({ 255, 200, 110, 255 }, 0.35f + 0.3f * glow));
            EndBlendMode();
        }
        if (SPR.torch.ok())
            drawAnim(SPR.torch, animFrame(SPR.torch, clock + 0.37f, 8, true), { CASTLE_DOOR_X - 52, 14 }, 2.0f);
        look::light({ CASTLE_DOOR_X - 40, 40 }, 140, TORCH, 1.1f * look::flicker(clock, 3));
        look::glow({ CASTLE_DOOR_X - 52, 10 }, 15, FLAME, 0.4f);
        look::light({ CASTLE_DOOR_X, 34 }, open ? 110.0f : 60.0f, { 255, 190, 110, 255 }, open ? 1.0f : 0.6f + 0.2f * glow);
        drawAnim(SPR.candle2, animFrame(SPR.candle2, clock + 0.5f, 7, true), { ROOM_PX_W - 3 * TILE, 2.5f * TILE }, 2.0f);
        drawAnim(SPR.candle2, animFrame(SPR.candle2, clock + 0.1f, 7, true), { ROOM_PX_W - 3 * TILE, ROOM_PX_H - 2.5f * TILE }, 2.0f);
        for (float y : { 2.5f * TILE, ROOM_PX_H - 2.5f * TILE }) {
            look::light({ ROOM_PX_W - 3 * TILE, y - 8 }, 80, { 255, 196, 120, 255 }, 0.8f * look::flicker(clock, y));
            look::glow({ ROOM_PX_W - 3 * TILE, y - 14 }, 9, FLAME, 0.4f);
        }
    }
}

// An iron brazier on three legs, burning.
static void drawBrazier(Vector2 foot, float t, float seed) {
    const Color out = { 14, 10, 18, 255 }, iron = { 46, 42, 54, 255 }, ironL = { 96, 90, 108, 255 };
    drawShadow({ foot.x, foot.y + 2 }, 11);
    for (float lx : { -9.0f, 0.0f, 9.0f }) {
        DrawLineEx({ foot.x + lx, foot.y }, { foot.x + lx * 0.4f, foot.y - 16 }, 4, out);
        DrawLineEx({ foot.x + lx, foot.y }, { foot.x + lx * 0.4f, foot.y - 16 }, 2, iron);
    }
    DrawRectangle((int)foot.x - 13, (int)foot.y - 24, 26, 9, out);          // the bowl
    DrawRectangle((int)foot.x - 12, (int)foot.y - 23, 24, 7, iron);
    DrawRectangle((int)foot.x - 12, (int)foot.y - 23, 24, 2, ironL);
    for (int k = -1; k <= 1; ++k) DrawRectangle((int)foot.x + k * 8 - 1, (int)foot.y - 20, 2, 2, ironL);   // rivets
    DrawRectangle((int)foot.x - 10, (int)foot.y - 26, 20, 3, { 40, 18, 16, 255 });                       // coals
    for (int k = 0; k < 5; ++k) DrawRectangle((int)foot.x - 9 + k * 4, (int)foot.y - 26, 2, 2, { 255, 120, 40, 255 });
    // the fire: tongues of flame licking up, flickering, sparks flying off
    auto tongue = [](Vector2 base, float r, float h, float sway, Color c) {
        Vector2 tip = { base.x + sway, base.y - h }, l = { base.x - r, base.y }, rr = { base.x + r, base.y };
        DrawCircleV(base, r, c);
        if ((rr.x - l.x) * (tip.y - l.y) - (rr.y - l.y) * (tip.x - l.x) > 0) DrawTriangle(l, tip, rr, c);
        else DrawTriangle(l, rr, tip, c);
    };
    look::light({ foot.x, foot.y - 24 }, 130, { 255, 140, 60, 255 }, 1.15f * look::flicker(t, seed));
    look::glow({ foot.x, foot.y - 34 }, 22, { 255, 150, 60, 255 }, 0.35f);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient((int)foot.x, (int)foot.y - 32, 36, Fade({ 255, 110, 40, 255 }, 0.20f + 0.06f * sinf(t * 9 + seed)), BLANK);
    for (int k = 0; k < 3; ++k) {
        float off = (k - 1) * 6.0f, tall = k == 1 ? 1.0f : 0.62f;
        float flick = 0.8f + 0.25f * sinf(t * 11 + seed + k * 2.1f) + 0.1f * sinf(t * 23 + k);
        float sway = sinf(t * 5 + seed + k) * 2.5f;
        Vector2 base = { foot.x + off, foot.y - 28 };
        tongue(base, 5.5f * tall + 1.5f, 24 * tall * flick, sway, Fade({ 220, 50, 20, 255 }, 0.75f));
        tongue({ base.x, base.y + 1 }, 4.0f * tall + 1, 17 * tall * flick, sway * 0.8f, Fade({ 255, 150, 50, 255 }, 0.85f));
        tongue({ base.x, base.y + 2 }, 2.2f * tall + 0.6f, 9 * tall * flick, sway * 0.5f, Fade({ 255, 236, 170, 255 }, 0.9f));
    }
    for (int k = 0; k < 4; ++k) {                       // sparks
        float ph = fmodf(t * (0.9f + 0.2f * k) + seed * 0.3f + k * 0.31f, 1.0f);
        Vector2 sp = { foot.x + sinf(k * 3.1f + seed) * 8 + sinf(t * 3 + k) * 4 * ph, foot.y - 34 - ph * 30 };
        DrawRectangleV(sp, { 2, 2 }, Fade({ 255, 190, 90, 255 }, 0.9f * (1 - ph)));
    }
    EndBlendMode();
}

// The way down from a beaten boss: a well of black stone cut in the floor, runes round its rim
// glowing like coals, steps falling away into a red light, embers and smoke rising out of it,
// and a brazier burning either side.
void Game::drawBossStairs() {
    Vector2 c = ROOM_CENTER;
    float pulse = 0.5f + 0.5f * sinf(clock * 2.1f) * (0.7f + 0.3f * sinf(clock * 5.3f));
    look::light({ c.x, c.y - 10 }, 150, { 220, 40, 40, 255 }, 1.0f + 0.35f * pulse);
    look::glow({ c.x, c.y - 4 }, 40, { 200, 30, 36, 255 }, 0.18f + 0.12f * pulse);
    BeginBlendMode(BLEND_ADDITIVE);                 // the light coming up out of it, on the floor round about
    DrawCircleGradient((int)c.x, (int)c.y - 18, 92, Fade({ 170, 24, 30, 255 }, 0.20f + 0.12f * pulse), BLANK);
    EndBlendMode();
    // smoke curling over the rim, slow
    for (int i = 0; i < 4; ++i) {
        float ph = fmodf(clock * 0.06f + i * 0.25f, 1.0f);
        float x = c.x - 70 + ph * 140, y = c.y + 26 - i * 14 + sinf(clock * 0.7f + i) * 4;
        DrawEllipse((int)x, (int)y, 34, 9, Fade({ 26, 12, 30, 255 }, 0.22f * sinf(ph * PI)));
    }
    if (SPR.bossStairs.ok()) {
        drawAnim(SPR.bossStairs, 0, c, 2.0f);
        BeginBlendMode(BLEND_ADDITIVE);             // the runes and the light below, breathing
        drawAnim(SPR.bossStairs, 1, c, 2.0f, false, Fade(WHITE, 0.55f + 0.45f * pulse));
        EndBlendMode();
    }
    else {
        DrawRectangle((int)c.x - 40, (int)c.y - 34, 80, 68, { 14, 10, 18, 255 });
        for (int i = 0; i < 5; ++i)
            DrawRectangle((int)c.x - 34 + i * 3, (int)c.y + 22 - i * 10, 68 - i * 6, 8,
                { (unsigned char)(60 - i * 10), (unsigned char)(50 - i * 9), (unsigned char)(70 - i * 12), 255 });
        DrawRectangle((int)c.x - 20, (int)c.y - 30, 40, 10, { 150, 26, 32, 255 });
    }
    // embers drifting up out of the dark
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 22; ++i) {
        float speed = 0.28f + 0.07f * (i % 5);
        float ph = fmodf(clock * speed + i * 0.137f, 1.0f);
        float x0 = c.x - 22 + fmodf(i * 23.7f, 44.0f);
        Vector2 p = { x0 + sinf(clock * 1.8f + i * 1.7f) * 7 * ph, c.y - 22 - ph * 86 };
        float a = sinf(ph * PI);
        Color col = i % 3 == 0 ? Color{ 255, 210, 120, 255 } : Color{ 255, 90, 50, 255 };
        DrawRectangleV({ p.x - 1, p.y - 1 }, { 2.0f + (i % 2), 2.0f + (i % 2) }, Fade(col, 0.85f * a));
    }
    EndBlendMode();
    drawBrazier({ c.x - 84, c.y + 30 }, clock, 0.0f);
    drawBrazier({ c.x + 84, c.y + 30 }, clock, 1.7f);
}

// --------------------------------------------------------------- the actors
void Game::drawActors() {
    Room& r = dungeon.cur();

    // the stairwell down, once the boss falls
    if (mode == Mode::Run && r.isBoss && r.cleared) drawBossStairs();

    if (r.hasSilverKey && r.cleared && !dungeon.silverFound) {      // the treasure room's key
        Vector2 k = { SILVER_POS.x, SILVER_POS.y - 6 + sinf(clock * 3 + 1) * 3 };
        look::light(k, 60, { 200, 220, 255, 255 }, 0.7f);
        drawShadow({ SILVER_POS.x, SILVER_POS.y + 8 }, 8);
        drawKey(k, 1.0f, true);
    }
    if (r.isTreasure) {             // the hoard: a big chest on a heap of gold
        if (!r.chestOpened) look::light({ ROOM_CENTER.x, ROOM_CENTER.y + 8 }, 120, { 255, 210, 120, 255 }, 0.7f);
        if (!r.chestOpened)
            for (int i = 0; i < 16; ++i) {
                float a = i * 2.4f, d = 26 + (i % 4) * 9;
                drawAnim(SPR.coin, (i + (int)(clock * 8)) % std::max(1, SPR.coin.frames),
                    { ROOM_CENTER.x + cosf(a) * d, ROOM_CENTER.y + 12 + sinf(a) * d * 0.45f }, 1.3f);
            }
        drawShadow({ ROOM_CENTER.x, ROOM_CENTER.y + 14 }, 22);
        const Anim& a = r.chestOpened ? SPR.chestOpen : SPR.chest;
        if (a.ok()) drawAnim(a, r.chestOpened ? a.frames - 1 : 0, { ROOM_CENTER.x, ROOM_CENTER.y - 8 }, 3.4f);
    }
    if (r.hasKey && r.cleared && !dungeon.keyFound) {      // the boss key, bobbing and glinting
        Vector2 k = { ROOM_CENTER.x, ROOM_CENTER.y - 6 + sinf(clock * 3) * 3 };
        look::light(k, 64, { 255, 214, 120, 255 }, 0.8f);
        drawShadow({ ROOM_CENTER.x, ROOM_CENTER.y + 8 }, 8);
        drawKey(k, 1.0f);
    }

    if (r.hasChest && !r.mimic) {
        Vector2 c = { r.chestTx * (float)TILE + 16, r.chestTy * (float)TILE + 16 };
        drawShadow({ c.x, c.y + 8 }, 12);
        if (SPR.chest.ok()) {
            Vector2 foot = { c.x, c.y + SPR.chest.fh };
            float size = SPR.chest.fw * 2.0f;
            if (r.chestOpened && chestOpenT < CHEST_SLAM) {          // shaking harder and harder, squashing down
                float k = chestOpenT / CHEST_SLAM, beat = sinf(chestOpenT * 40);
                float tall = 1 - 0.22f * k + beat * 0.07f * k;
                drawAnimOnFoot(SPR.chestOpen, 0, foot, size * (2 - tall), size * tall, beat * (2 + 10 * k));
            }
            else if (r.chestOpened) {                                // the lid slams up, and the chest bounces
                float since = chestOpenT - CHEST_SLAM, tall = 1 + 0.4f * expf(-since * 9) * cosf(since * 26);
                drawAnimOnFoot(SPR.chestOpen, animFrame(SPR.chestOpen, since, 40, false), foot, size * (2 - tall), size * tall);
            }
            else drawAnim(SPR.chest, fmodf(clock, 2.5f) < 0.5f ? animFrame(SPR.chest, clock, 8, true) : 0, c, 2.0f);  // rattles now and then
        }
        else {
            DrawRectangle((int)c.x - 11, (int)c.y - 6, 22, 14, { 120, 78, 40, 255 });
            DrawRectangle((int)c.x - 2, (int)c.y - 6, 4, 5, { 230, 200, 90, 255 });
        }
    }

    if (chestLight > 0) drawChestLight({ r.chestTx * (float)TILE + 16, r.chestTy * (float)TILE + 12 }, chestLight, clock);

    if (r.isStart && mode == Mode::Run) drawVasil();

    auto drawBody = [&](const Enemy& e) {
        if (e.bossForm >= 0) drawBossBody(e, currentBossIndex(), bossArt);
        else drawEnemy(e);
    };
    drawLabFloor(fight, clock);             // Triceps' wells, wires and the staff's reach lie on the floor
    drawArenaFloor(fight, clock);
    for (auto& c : cracks) drawCrack(c);
    for (auto& s : spikes) drawIceSpike(s);
    drawRocks(fight, player.pos.y, true);
    for (auto& c : corpses) drawBody(c);
    for (auto& r : rubble)
        DrawTexturePro(r.tex, r.src, { r.pos.x, r.pos.y, r.size.x, r.size.y }, { r.size.x / 2, r.size.y / 2 }, r.rot * RAD2DEG, Fade(WHITE, fminf(1, finaleT * 2)));
    for (auto& e : enemies) drawBody(e);
    for (auto& e : enemies) if (e.burnT > 0) drawBurn(e, clock);
    for (auto& e : enemies) {               // a fearful face bobs over everyone Momiche scared
        if (e.dying) continue;
        if (e.bossForm >= 0) {
            float top = bossTop(e, currentBossIndex(), bossArt);         // over the head of the token or figure
            drawFear(e.pos.x, top - 16, e.pos.y + e.r * 0.9f + 16, 26);
        }
        else drawFear(e.pos.x, e.pos.y - e.r * 2 - 14, e.pos.y + e.r + 10, 16);
    }
    for (auto& e : enemies) {               // stars circle every monster Krisa knocked silly
        if (e.dying || e.dazeT <= 0 || e.bossForm >= 0) continue;
        float fade = fminf(1, e.dazeT * 3), y = e.pos.y - e.r * 2 - 6;
        for (int i = 0; i < 3; ++i) {
            float a = clock * 6 + i * 2 * PI / 3;
            Vector2 at = { e.pos.x + cosf(a) * 11, y + sinf(a) * 4 };
            DrawPoly(at, 4, 4.2f, 45 + a * 30, Fade({ 60, 40, 10, 255 }, fade));
            DrawPoly(at, 4, 3.0f, 45 + a * 30, Fade({ 255, 225, 90, 255 }, fade));
        }
    }
    for (auto& e : enemies) {               // hearts float over every monster Vanessa's kiss left lovestruck
        if (e.dying || e.charmT <= 0 || e.bossForm >= 0) continue;
        float fade = fminf(1, e.charmT * 2), y = e.pos.y - e.r * 2 - 10;
        for (int i = 0; i < 3; ++i) {
            float ph = clock * 1.6f + i * 2.1f, rise = fmodf(ph, 2.0f) / 2.0f;     // each drifts up and fades, then again
            Vector2 at = { e.pos.x + sinf(ph * 3 + i) * 7 + (i - 1) * 7, y - rise * 14 };
            float a = fade * (1 - rise * 0.8f);
            drawHeart(at, 9, Fade({ 70, 10, 40, 255 }, a));
            drawHeart(at, 7, Fade({ 255, 90, 160, 255 }, a));
        }
    }
    for (auto& c : coins) {
        if (!c.pack) look::light(c.pos, 26, { 255, 214, 120, 255 }, 0.45f);
        if (c.pack) ui::drawContain(packTex, { 0, 0, (float)packTex.width, (float)packTex.height }, { c.pos.x - 11, c.pos.y - 11, 22, 22 });
        else drawAnim(SPR.coin, animFrame(SPR.coin, c.t, 12, true), c.pos, 1.4f);
    }

    // the hero, with their champion's effects
    if (scareT > SCARE_TIME - 0.6f)         // the scream: two rings racing outward
        for (int i = 0; i < 2; ++i) {
            float k = Clamp((SCARE_TIME - scareT - i * 0.12f) / 0.6f, 0, 1);
            float r = (30 + 300 * k) / 2;
            if (k > 0 && k < 1) DrawRing(player.pos, r - 2, r, 0, 360, 48, Fade({ 166, 191, 255, 255 }, 0.7f * (1 - k)));
        }
    drawHeroes();                           // the others (co-op)
    drawHero();
    drawRocks(fight, player.pos.y, false);

    for (auto& p : shots) drawProjectile(p, clock);
    for (auto& p : enemyShots) drawProjectile(p, clock);
    for (auto& l : lasers) {                // the Gym Rat's C++: green lasers, bits racing along them
        float k = 1 - l.t / GYM_POSE_TIME, len = Vector2Distance(l.a, l.b);
        look::lightLine(l.a, l.b, 40, 50, { 60, 255, 110, 255 }, 0.7f * k);
        Vector2 d = Vector2Scale(Vector2Subtract(l.b, l.a), 1 / fmaxf(len, 1));
        BeginBlendMode(BLEND_ADDITIVE);
        DrawLineEx(l.a, l.b, 8 * k, Fade({ 40, 255, 90, 255 }, 0.22f * k));
        DrawLineEx(l.a, l.b, 3, Fade({ 80, 255, 120, 255 }, 0.75f * k));
        DrawLineEx(l.a, l.b, 1.2f, Fade({ 225, 255, 225, 255 }, k));
        for (float s2 = fmodf(l.t * 420, 22); s2 < len; s2 += 22) {
            Vector2 p = Vector2Add(l.a, Vector2Scale(d, s2));
            DrawRectangleV({ p.x - 1.5f, p.y - 1.5f }, { 3, 3 }, Fade({ 190, 255, 200, 255 }, k));
        }
        EndBlendMode();
    }
    drawBus(fight.bus, clock);
    drawLabAir(fight, clock);
    drawArenaAir(fight, clock);

    for (auto& pt : parts) {
        float left = pt.life / pt.maxLife;
        Color c = pt.color; c.a = (unsigned char)(255 * left);
        if (pt.star) drawStar(pt.pos, pt.size, pt.life * 5, c);
        else DrawCircleV(pt.pos, pt.shrink ? pt.size * left : pt.size, c);
    }
    for (auto& a : arcs) drawArc(a, clock);
    for (auto& f : flashes) drawFlash(f);
    for (auto& pu : popups)
        drawAnim(*pu.anim, animFrame(*pu.anim, pu.t, 8, true), pu.pos, 2.0f, false, Fade(WHITE, fminf(1.0f, 1.4f - pu.t)));
}

// The hero in the fields: their champion's figure (or pose), and what they have going.
void Game::drawHero() {
    // every hero carries a little light into the dark
    if (climbT == 0) look::light({ player.pos.x, player.pos.y - 10 }, player.hp > 0 ? 175.0f : 110.0f, { 255, 228, 190, 255 }, player.hp > 0 ? 0.8f : 0.45f);
    if (player.auraT > 0) look::light(player.pos, 90, { 130, 200, 255, 255 }, 0.6f);
    if (coop.active() && player.hp <= 0) {          // co-op: knocked out, lying where they fell
        const WalkSheet& w = figure(player.champion);
        drawShadow({ player.pos.x, player.pos.y + 6 }, 15);
        Rectangle dst = { player.pos.x - 17, player.pos.y - 26, 34, 45 };
        if (w.ok()) w.draw(FACE_DOWN, 0, dst, { 150, 150, 170, 235 }, false, 90);
        else ::drawHero(player.pos, { 1, 0 }, 0, false, Fade(champ().cloak, 0.5f));
        for (int i = 0; i < 3; ++i) {                // stars round their head
            float a = clock * 4 + i * 2 * PI / 3;
            DrawPoly({ player.pos.x + 14 + cosf(a) * 9, player.pos.y - 14 + sinf(a) * 3 }, 4, 2.6f, a * 40, { 255, 225, 90, 255 });
        }
        return;
    }
    if (climbT != 0) {                              // on the castle ladder: their back to you, stepping up the rungs
        const WalkSheet& w = figure(player.champion);
        float into = climbT > 0 ? climbT / (CLIMB_UP * 0.22f) : (CLIMB_DOWN + climbT) / (CLIMB_DOWN * 0.22f);
        Color tint = Fade(WHITE, fminf(1, into));      // through the door at the top, they fade
        const float fw = 24 * 1.4f, fh = 32 * 1.4f;
        if (w.ok()) w.draw(FACE_UP, w.frameAt(true, heroStride * STEP_PACE), { player.pos.x - fw / 2, player.pos.y - 10 - fh / 2, fw, fh }, tint);
        else ::drawHero(player.pos, { 0, -1 }, 0, false, Fade(champ().cloak, fminf(1, into)));
        return;
    }
    float lift = player.jumpT > 0 ? sinf((1 - player.jumpT / 0.6f) * PI) * 22 : 0;
    if (player.auraT > 0) {
        float pulse = 34 + sinf(clock * 6) * 3;
        DrawCircleV(player.pos, pulse, Fade({ 130, 200, 255, 255 }, 0.12f));
        DrawCircleLinesV(player.pos, pulse, Fade({ 170, 220, 255, 255 }, 0.7f));
    }
    if (player.dashT > 0 || player.rollAtkT > 0)
        DrawCircleV(player.pos, PLAYER_R + 4, Fade(player.rollAtkT > 0 ? Color{ 110, 200, 80, 255 } : Color{ 245, 210, 110, 255 }, 0.35f));
    // Galin and Daniil strike their own poses (his smoke, the slingshot), and everyone their dodge-roll
    const Anim* pose = nullptr;
    int poseFrame = 0;
    bool poseFlip = player.facing.x < 0;
    if (champ().kind == Special::SmokeBreak) {
        const Anim& sm = SPR.galinSmoke;
        if (sm.ok() && stubT > 0) { pose = &sm; poseFrame = sm.frames - 1; }
        else if (sm.ok() && cigT > 0) { pose = &sm; poseFrame = std::min(sm.frames - 2, (int)((1 - cigT / CIG_TIME) * (sm.frames - 1))); }
    }
    if (champ().kind == Special::GrinderThrow && throwT > 0 && SPR.filThrow.ok()) {   // Filqcheto: wind up and throw (arms wide for the storm)
        pose = &SPR.filThrow;
        poseFrame = throwKind == 2 ? 2 : (THROW_POSE_TIME - throwT < THROW_AT ? 0 : 1);
    }
    if (champ().kind == Special::Haymaker && punchT > 0 && SPR.krisaPunch.ok()) {     // Krisa: the jab as she dashes, then the punch
        pose = &SPR.krisaPunch;
        poseFrame = PUNCH_POSE_TIME - punchT < HAYMAKER_DASH ? 0 : 1;
        poseFlip = punchDir.x < 0;
    }
    if (champ().kind == Special::Slingshot && slingT > 0 && SPR.daniilSling.ok()) {   // Daniil: draw, aim, let go
        float into = SLING_TIME - slingT;
        pose = &SPR.daniilSling;
        poseFrame = into < SLING_DRAW * 0.45f ? 0 : into < SLING_DRAW ? 1 : 2;
    }
    if (!pose && player.roll > 0 && roll[player.champion].ok()) {   // their drawn dodge-roll
        pose = &roll[player.champion];
        poseFrame = std::min(pose->frames - 1, (int)((1 - player.roll / ROLL_TIME) * pose->frames));
        poseFlip = fabsf(player.rollDir.x) > 0.01f ? player.rollDir.x < 0 : player.facing.x < 0;
    }
    if (pose) drawPose(*pose, poseFrame, player.pos, poseFlip, player.hitFlash > 0);
    // no roll drawn for them: their own figure, side on, tumbling
    else if (player.roll > 0 && figure(player.champion).ok())
        drawRolling(figure(player.champion), player.pos, player.rollDir, player.facing, 1 - player.roll / ROLL_TIME, player.hitFlash > 0);
    // walking as their own figure, facing the way they walk (or aim, standing still)
    else drawChampion(figure(player.champion), player.pos, heroMoving ? heroMoveDir : player.facing, heroStepping, heroStride * STEP_PACE,
        player.facing, player.meleeAnim, player.hitFlash > 0, champ().cloak,
        { weaponTex[player.weapon], WEAPONS[player.weapon].slant, bladeReach() * 0.95f }, lift);
    if (player.shieldT > 0) {
        DrawCircleV({ player.pos.x, player.pos.y - 6 }, 22, Fade({ 230, 190, 120, 255 }, 0.18f));
        DrawCircleLinesV({ player.pos.x, player.pos.y - 6 }, 22, Fade({ 250, 220, 150, 255 }, 0.9f));
    }
    if (castT > 0) drawSpellCharge(castingSpell, castHand(), 1 - castT / SPELLS[castingSpell].charge, clock);
}

// Vasil on his stool by his sign, a few listeners round him (a different few every floor).
// Telling the tale he stands up beside the stool, glowing; at the end he takes a bow.
void Game::drawVasil() {
    const Vector2 v = VASIL_POS;
    const float S = 46.0f / 211;               // his art to the world: standing, a champion's height
    auto draw = [](Texture2D tx, Vector2 foot, float scale, bool flip = false, float rot = 0, float squash = 1) {
        if (!tx.id) return;
        float w = tx.width * scale, h = tx.height * scale * squash;
        DrawTexturePro(tx, { 0, 0, flip ? -(float)tx.width : (float)tx.width, (float)tx.height }, { foot.x, foot.y, w, h }, { w / 2, h }, rot, WHITE);
    };
    bool telling = !dialogue.empty(), bowing = !telling && vasilBowT > 0;
    if (telling) look::light({ v.x, v.y - 20 }, 110, { 255, 200, 120, 255 }, 0.9f);
    auto shadowAt = [](Vector2 foot, float r) { drawShadow({ foot.x, foot.y - r * 0.7f - 1 }, r); };   // under the feet

    // the sign behind him to the left
    shadowAt({ v.x - 52, v.y - 2 }, 6);
    draw(vasil.sign, { v.x - 52, v.y - 2 }, 0.36f);

    if (telling || bowing) {
        if (telling) {                          // the warm light of a story being told
            BeginBlendMode(BLEND_ADDITIVE);
            float pulse = 1 + 0.06f * sinf(clock * 3);
            DrawCircleGradient((int)v.x, (int)(v.y - 26), 40 * pulse, Fade({ 255, 200, 110, 255 }, 0.30f), Fade({ 255, 200, 110, 255 }, 0));
            EndBlendMode();
        }
        shadowAt({ v.x + 24, v.y + 2 }, 8);
        draw(vasil.stool, { v.x + 24, v.y + 2 }, 0.26f);
        shadowAt({ v.x, v.y + 2 }, 11);
        float bow = bowing ? sinf((1 - vasilBowT / 1.2f) * PI) : 0;
        bool flip = player.pos.x < v.x - 6;     // he tells it to you
        float bob = telling ? fabsf(sinf(clock * 2.6f)) * 1.5f : 0;
        draw(vasil.story[telling ? dialogueIdx % 2 : 1], { v.x, v.y + 2 - bob }, S, flip, bow * (flip ? -10.0f : 10.0f), 1 - 0.1f * bow);
    }
    else {                                      // sitting, breathing, waiting for a listener
        shadowAt({ v.x, v.y + 2 }, 12);
        draw(vasil.seated, { v.x, v.y + 2 }, S, false, 0, 1 + 0.012f * sinf(clock * 2.2f));
    }

    // the listeners, in front of him
    if (vasil.crowd.id) {
        static const Vector2 spots[3] = { { -54, 30 }, { 50, 28 }, { 88, 12 } };
        int fw = vasil.crowd.width / VasilArt::CROWD, fh = vasil.crowd.height;
        for (int i = 0; i < 3; ++i) {
            int who = (floor * 7 + i * 3) % VasilArt::CROWD;
            Vector2 foot = Vector2Add(v, spots[i]);
            float sc = 0.40f, lean = telling ? sinf(clock * 2 + i * 1.7f) * 1.2f : 0;
            shadowAt(foot, 8);
            DrawTexturePro(vasil.crowd, { (float)(who * fw), 0, (float)(spots[i].x > 0 ? -fw : fw), (float)fh },
                { foot.x, foot.y, fw * sc, fh * sc }, { fw * sc / 2, fh * sc }, lean, WHITE);
        }
    }
}

// ---------------------------------------------------------------------- HUD
// A "[E] talk" style prompt above a spot in the room, drawn crisp in screen space.
static void prompt(const Camera2D& cam, Vector2 world, const char* key, const char* label) {
    Vector2 s = GetWorldToScreen2D(world, cam);
    float keyW = (float)ui::measure(key, 10) + 14;
    float w = keyW + ui::measure(label, 10);
    ui::keycap(key, s.x - w / 2, s.y);
    ui::text(label, s.x - w / 2 + keyW, s.y + 2, 10, ui::TEXT);
}

void Game::drawHud() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    const ChampionDef& c = champ();
    Camera2D cam = camera();
    Room& r = dungeon.cur();

    // --- damage numbers ---
    // bigger on a phone and for big hits, with a dark outline, popping in as they appear
    for (auto& f : floaters) {
        Vector2 s = GetWorldToScreen2D(f.pos, cam);
        float a = fminf(1, 2.5f * (0.8f - f.t)), pop = f.t < 0.1f ? 1 + (0.1f - f.t) * 3 : 1;
        int size = (int)(((touch::enabled() ? 30 : 20) + (f.value >= 30 ? 10 : 0)) * pop);
        const char* txt = TextFormat("%d", f.value);
        int w = ui::measure(txt, size);
        for (int k = 0; k < 4; ++k)
            DrawText(txt, (int)(s.x - w / 2.0f) + (k % 2 ? 2 : -2), (int)s.y + (k / 2 ? 2 : -2), size, Fade(BLACK, 0.8f * a));
        DrawText(txt, (int)(s.x - w / 2.0f), (int)s.y, size, Fade(f.color, a));
    }

    // --- interaction prompts ---
    const char* E = keyName(settings.keys.interact);
    Vector2 chestPos = { r.chestTx * (float)TILE + 16, r.chestTy * (float)TILE + 16 };
    if (r.hasChest && !r.chestOpened && r.cleared && Vector2Distance(player.pos, chestPos) < 40)
        prompt(cam, { chestPos.x, chestPos.y - 34 }, E, "open");
    if (r.isTreasure && !r.chestOpened && Vector2Distance(player.pos, ROOM_CENTER) < 48)
        prompt(cam, { ROOM_CENTER.x, ROOM_CENTER.y - 54 }, E, "open the hoard");
    if (r.isStart && mode == Mode::Run && overlay == Overlay::None && dialogue.empty() && climbT == 0
        && fabsf(player.pos.x - CASTLE_DOOR_X) < 28 && player.pos.y < CASTLE_DOOR_FRONT.y + 16)
        prompt(cam, { CASTLE_DOOR_X, 34 }, E, "Climb to the castle");
    if (r.isStart && mode == Mode::Run && dialogue.empty() && Vector2Distance(player.pos, VASIL_POS) < 56)
        prompt(cam, { VASIL_POS.x, VASIL_POS.y - 62 }, E, "hear the tale");
    if (mode == Mode::Run && r.isBoss && r.cleared && Vector2Distance(player.pos, ROOM_CENTER) < 70)
        ui::textCentered("Descend", GetWorldToScreen2D({ ROOM_CENTER.x, ROOM_CENTER.y - 70 }, cam).x,
            GetWorldToScreen2D({ ROOM_CENTER.x, ROOM_CENTER.y - 70 }, cam).y, 20, { 255, 120, 90, 255 });

    // --- vitals (top-left) ---
    Rectangle box = { 16, 16, 340, 100 };
    ui::panel(box);
    Rectangle pic = { box.x + 14, box.y + 14, 72, 72 };
    DrawRectangleRec(pic, { 12, 12, 20, 255 });
    if (champTex[player.champion].id) {
        Texture2D tx = champTex[player.champion];
        ui::drawContain(tx, { c.crop.x * tx.width, c.crop.y * tx.height, c.crop.width * tx.width, c.crop.height * tx.height }, pic);
    }
    else drawFigureIn(walk[player.champion], FACE_DOWN, 0, { pic.x + 8, pic.y + 4, pic.width - 16, pic.height - 8 });
    DrawRectangleLinesEx(pic, 2, ui::ACCENT);
    ui::text(TextFormat("Lv %d", player.level), pic.x + 4, pic.y + pic.height - 14, 10, ui::ACCENT);

    float bx = box.x + 100, bw = box.width - 116;
    drawName(player.champion, bx, box.y + 12, 10, ui::ACCENT);
    ui::bar({ bx, box.y + 28, bw, 18 }, player.hp / player.maxHp, ui::HP, TextFormat("%d / %d", (int)ceilf(fmaxf(0, player.hp)), (int)player.maxHp));
    ui::bar({ bx, box.y + 54, bw, 14 }, player.mana / player.maxMana, ui::MANA, TextFormat("%d / %d", (int)player.mana, (int)player.maxMana));
    ui::bar({ bx, box.y + 76, bw, 6 }, (float)player.xp / player.xpNext, ui::XP);

    // gold, floor and run clock under the panel
    drawAnim(SPR.coin, 0, { box.x + 14, box.y + box.height + 22 }, 1.3f);
    ui::text(TextFormat("%d", player.gold), box.x + 28, box.y + box.height + 12, 20, ui::ACCENT);
    if (mode == Mode::Run && dungeon.keyFound) drawKey({ box.x + 72, box.y + box.height + 22 }, 0.8f);
    bool hoardWaits = false;
    for (auto& row : dungeon.grid) for (auto& rm : row) hoardWaits |= rm.isTreasure && !rm.chestOpened;
    if (mode == Mode::Run && dungeon.silverFound && hoardWaits) drawKey({ box.x + 90, box.y + box.height + 22 }, 0.8f, true);
    const char* where = mode == Mode::Run ? TextFormat("Floor %d   %s", floor, ui::clock(runTime))
                                          : TextFormat("Boss Rush %d/%d   %s", rushStage + 1, BOSS_COUNT, ui::clock(rushTime));
    float whereW = (float)ui::measure(where, 10);       // (on a dark backing: it sits over the room's corner)
    DrawRectangleRounded({ box.x + 94, box.y + box.height + 12, whereW + 12, 18 }, 0.4f, 4, Fade(BLACK, 0.45f));
    ui::text(where, box.x + 100, box.y + box.height + 16, 10, ui::TEXT);

    // the weapon in hand and the packs in your pocket
    float wy = box.y + box.height + 44;
    drawWeapon(player.weapon, { box.x + 14, wy, 44, 22 });
    int upgrades = 0;
    for (int level : player.upgrades[player.weapon]) upgrades += level;
    const char* held = upgrades ? TextFormat("%s +%d", WEAPONS[player.weapon].name, upgrades) : WEAPONS[player.weapon].name;
    ui::text(held, box.x + 64, wy + 6, 10, ui::TEXT);
    if (player.packs > 0) {
        float px = box.x + 76 + ui::measure(held, 10);
        ui::drawContain(packTex, { 0, 0, (float)packTex.width, (float)packTex.height }, { px, wy, 22, 22 });
        ui::text(TextFormat("x%d", player.packs), px + 24, wy + 6, 10, ui::ACCENT);
    }

    // vapes for good, and the champion's effects for now
    float iy = box.y + box.height + 74, ix = box.x + 14;
    for (int v = 0; v < VP_COUNT; ++v) {
        if (!player.has((Vape)v)) continue;
        if (SPR.vapes.ok()) drawAnim(SPR.vapes, v, { ix + 10, iy + 8 }, 0.9f);
        ix += 34;
    }
    auto effect = [&](float left, const char* name, Color col) {
        if (left <= 0) return;
        ui::text(TextFormat("%s %.0fs", name, ceilf(left)), ix, iy + 2, 10, col);
        ix += ui::measure(TextFormat("%s %.0fs", name, ceilf(left)), 10) + 14;
    };
    effect(player.auraT, "Calm", { 150, 210, 255, 255 });
    effect(player.shieldT, "Mug-Shield", { 240, 200, 130, 255 });
    effect(player.wobbleT, "Tipsy", { 230, 150, 150, 255 });

    drawTeam();                             // co-op: the others' health, under yours

    // --- hotbar: the potion belt, three powers and the special (on a phone, wherever it was put) ---
    float slot = 60, barX = sw / 2.0f, barY = sh - 64.0f;
    if (touch::enabled()) { Vector2 at = touch::controlPos(TC_BAR); slot = touch::controlSize(TC_BAR); barX = at.x; barY = at.y; }
    float gap = slot / 6, hx = barX - hotbarWidth(slot) / 2, hy = barY - slot / 2;
    for (int i = 0; i < 2; ++i) {
        Rectangle s = { hx + i * (slot + gap), hy, slot, slot };
        int key = i == 0 ? settings.keys.potionHp : settings.keys.potionMana;
        DrawRectangleRec(s, { 16, 14, 24, 230 });
        touch::zone(s, key);
        const Anim& a = i == 0 ? SPR.flaskRed : SPR.flaskBlue;
        drawAnim(a, animFrame(a, clock, 6, true), { s.x + slot / 2, s.y + slot / 2 }, slot / 26, false,
            player.potions[i] ? WHITE : Color{ 80, 80, 90, 255 });
        DrawRectangleLinesEx(s, 1, ui::EDGE);
        ui::keycap(keyName(key), s.x + 3, s.y + 3);
        ui::textRight(TextFormat("x%d", player.potions[i]), s.x + slot - 5, s.y + slot - 14, 10, player.potions[i] ? ui::TEXT : ui::MUTED);
        ui::textCentered(i == 0 ? "Health" : "Mana", s.x + slot / 2, s.y + slot + 6, 10, ui::MUTED);
    }
    float sx0 = hx + 2 * slot + gap + 14;
    for (int i = 0; i < SPELL_COUNT; ++i) {
        Rectangle s = { sx0 + i * (slot + gap), hy, slot, slot };
        bool sel = i == player.selected;
        int cost = spellCost(i);
        bool castable = player.mana >= cost;
        DrawRectangleRec(s, { 16, 14, 24, 230 });
        touch::zone(s, KEY_ONE + i);
        if (SPR.spellIcons.ok()) drawAnim(SPR.spellIcons, i, { s.x + slot / 2, s.y + slot / 2 - 4 }, 1.1f * slot / 60, false, castable ? WHITE : Color{ 90, 90, 100, 255 });
        else DrawRectangle((int)s.x + 8, (int)s.y + 8, (int)slot - 16, 24, SPELLS[i].color);
        DrawRectangleLinesEx(s, sel ? 3.0f : 1.0f, sel ? ui::ACCENT : ui::EDGE);
        ui::text(TextFormat("%d", i + 1), s.x + 5, s.y + 4, 10, ui::TEXT);
        ui::textRight(TextFormat("%d", cost), s.x + slot - 5, s.y + slot - 14, 10, castable ? ui::MANA : ui::BAD);
        ui::textCentered(SPELLS[i].name, s.x + slot / 2, s.y + slot + 6, 10, sel ? ui::ACCENT : ui::MUTED);
    }
    {
        Rectangle s = { sx0 + 3 * slot + 2 * gap + 14, hy, slot, slot };
        DrawRectangleRec(s, { 30, 20, 40, 235 });
        touch::zone(s, settings.keys.special);
        if (champTex[player.champion].id) {
            Texture2D tx = champTex[player.champion];
            ui::drawContain(tx, { c.crop.x * tx.width, c.crop.y * tx.height, c.crop.width * tx.width, c.crop.height * tx.height },
                { s.x + 4, s.y + 4, slot - 8, slot - 8 });
        }
        else DrawCircleV({ s.x + slot / 2, s.y + slot / 2 }, 16, Fade({ 150, 110, 230, 255 }, 0.6f));
        bool metered = c.kind == Special::Slingshot || c.kind == Special::Blink;
        float meter = c.kind == Special::Slingshot ? player.charge / COIN_POWER_MAX : player.charge / DUST_MAX;
        bool ready = player.specialCd <= 0 && (c.kind != Special::Slingshot || player.charge >= COIN_POWER_MAX)
            && (c.kind != Special::Blink || player.charge >= DUST_PER_BLINK);
        if (player.specialCd > 0)           // cooldown curtain
            DrawRectangle((int)s.x, (int)s.y, (int)slot, (int)(slot * player.specialCd / c.cooldown), { 0, 0, 0, 170 });
        else if (!ready) DrawRectangleRec(s, { 0, 0, 0, 110 });
        if (metered) ui::bar({ s.x + 4, s.y + slot - 8, slot - 8, 4 }, meter, c.kind == Special::Blink ? Color{ 230, 160, 255, 255 } : ui::ACCENT);
        DrawRectangleLinesEx(s, 2, ready ? ui::ACCENT : ui::EDGE);
        ui::keycap(keyName(settings.keys.special), s.x + 3, s.y + 3);
        ui::textCentered(c.kind == Special::GrinderThrow ? GRINDER_NAMES[grindStep] : c.special, s.x + slot / 2, s.y + slot + 6, 10, ready ? ui::ACCENT : ui::MUTED);
    }

    // a challenge room's clock, or a mini-boss's health
    if (challengeT > 0) {
        ui::textCentered(TextFormat("SURVIVE  %d", (int)ceilf(challengeT)), sw / 2.0f, 22, 40, { 255, 170, 80, 255 });
        ui::bar({ sw / 2.0f - 160, 70, 320, 8 }, challengeT / CHALLENGE_TIME, { 255, 170, 80, 255 });
    }
    else if (!fight.active())
        for (auto& e : enemies) {
            if (!e.alive || !e.elite) continue;
            ui::textCentered(e.name, sw / 2.0f, 22, 20, ui::ACCENT);
            ui::bar({ sw / 2.0f - 200, 50, 400, 12 }, e.hp / e.maxHp, { 230, 180, 70, 255 });
            break;
        }

    drawMinimap();
    if (fight.active()) drawBossBar();

    // --- Vasil: his call as a floor begins, then the tale itself ---
    if (r.isStart && mode == Mode::Run && dialogue.empty() && vasilGreetT > 0 && banner <= 0) {
        std::string say = vasilGreeting();
        float a = fminf(1, vasilGreetT);
        float bw = 250, th = ui::wrappedHeight(say.c_str(), bw - 24, 10);
        Vector2 tip = GetWorldToScreen2D({ VASIL_POS.x + 4, VASIL_POS.y - 52 }, cam);
        Rectangle b = { tip.x - 30, tip.y - th - 30, bw, th + 20 };
        // the outline is the bubble drawn 2px bigger underneath (the Windows raylib.lib has no DrawRectangleRoundedLinesEx)
        Color paper = { 250, 242, 222, (unsigned char)(242 * a) }, ink = { 120, 84, 50, (unsigned char)(255 * a) };
        DrawRectangleRounded({ b.x - 2, b.y - 2, b.width + 4, b.height + 4 }, 0.28f, 6, ink);
        DrawTriangle({ tip.x - 16, b.y + b.height }, { tip.x, tip.y }, { tip.x + 4, b.y + b.height }, ink);
        DrawRectangleRounded(b, 0.25f, 6, paper);
        DrawTriangle({ tip.x - 14, b.y + b.height - 1 }, { tip.x, tip.y - 2.5f }, { tip.x + 2, b.y + b.height - 1 }, paper);
        ui::textWrapped(say.c_str(), b.x + 12, b.y + 10, bw - 24, 10, Fade({ 60, 40, 30, 255 }, a));
    }
    if (!dialogue.empty()) {
        float w = fminf(800.0f, sw - 80.0f);
        Rectangle d = { (sw - w) / 2, hy - 150, w, 122 };
        ui::panel(d);
        Rectangle face = { d.x + 16, d.y + 16, 90, 90 };
        DrawRectangleRec(face, { 12, 12, 20, 255 });
        if (vasil.portrait.id) ui::drawContain(vasil.portrait, { 0, 0, (float)vasil.portrait.width, (float)vasil.portrait.height }, face);
        DrawRectangleLinesEx(face, 2, ui::ACCENT);
        float tx = face.x + face.width + 18;
        ui::text("Vasil, the Storyteller", tx, d.y + 16, 20, { 120, 150, 240, 255 });
        ui::textWrapped(dialogue.c_str(), tx, d.y + 46, d.x + w - 24 - tx, 20, ui::TEXT);
        int lines = (int)vasilTale().size();
        ui::text(TextFormat("%d / %d", dialogueIdx + 1, lines), d.x + w - 160, d.y + d.height - 24, 10, ui::MUTED);
        ui::keycap(E, d.x + w - 110, d.y + d.height - 26);
        ui::text(dialogueIdx + 1 < lines ? "continue" : "thank him", d.x + w - 92, d.y + d.height - 24, 10, ui::MUTED);
    }

    // --- the Gym Rat shouts what he's throwing ---
    if (gymT > 0) {
        float k = gymT / GYM_POSE_TIME, rise = (1 - k) * 16, a = fminf(1, k * 2.5f);
        Vector2 ahead = GetWorldToScreen2D({ player.pos.x + gymDir.x * 50, player.pos.y - 34 + gymDir.y * 26 - rise }, cam);
        Vector2 behind = GetWorldToScreen2D({ player.pos.x - gymDir.x * 50, player.pos.y - 34 - gymDir.y * 26 - rise }, cam);
        ui::textCentered("C++", ahead.x, ahead.y, 20, Fade({ 90, 255, 120, 255 }, a));
        ui::textCentered("CREATINE", behind.x, behind.y, 20, Fade({ 245, 245, 255, 255 }, a));
    }

    // --- banner ---
    if (banner > 0 && overlay == Overlay::None) {
        float a = fminf(1.0f, banner);
        float y = sh * 0.22f;
        DrawRectangleGradientH(0, (int)y - 16, sw / 2, bannerSub.empty() ? 72 : 96, Fade(BLACK, 0), Fade(BLACK, 0.45f * a));
        DrawRectangleGradientH(sw / 2, (int)y - 16, sw / 2, bannerSub.empty() ? 72 : 96, Fade(BLACK, 0.45f * a), Fade(BLACK, 0));
        ui::textCentered(bannerText.c_str(), sw / 2.0f, y, 40, Fade(ui::ACCENT, a));
        if (!bannerSub.empty()) ui::textCentered(bannerSub.c_str(), sw / 2.0f, y + 50, 20, Fade(ui::TEXT, a));
    }

    // --- a reminder of the controls, in the first room only ---
    if (r.isStart && mode == Mode::Run && floor == 1 && !touch::enabled())
        ui::textCentered(TextFormat("WASD move   LMB blade   RMB/Enter cast   1-3 power   %s special   %s roll   %s interact   Esc pause",
            keyName(settings.keys.special), keyName(settings.keys.roll), E), sw / 2.0f, sh - 18.0f, 10, Fade(ui::MUTED, 0.9f));
}

void Game::drawMinimap() {
    if (mode != Mode::Run) return;
    int minX = DUNGEON_G, minY = DUNGEON_G, maxX = -1, maxY = -1;
    for (int y = 0; y < DUNGEON_G; ++y)
        for (int x = 0; x < DUNGEON_G; ++x)
            if (dungeon.grid[y][x].exists) { minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y); }
    if (maxX < 0) return;
    int cell = 16, gap = 4;
    int w = (maxX - minX + 1) * (cell + gap) - gap, h = (maxY - minY + 1) * (cell + gap) - gap;
    int ox = GetScreenWidth() - w - 28, oy = 28;
    ui::panel({ (float)ox - 12, (float)oy - 12, (float)w + 24, (float)h + 24 });
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x) {
            const Room& rm = dungeon.grid[y][x];
            if (!rm.exists) continue;
            bool cur = x == dungeon.cx && y == dungeon.cy;
            Color c;
            if (cur) c = { 255, 220, 120, 255 };
            else if (rm.isBoss) c = rm.visited ? Color{ 210, 90, 80, 255 } : Color{ 120, 54, 50, 255 };
            else if (rm.isTreasure) c = rm.chestOpened ? Color{ 110, 80, 140, 255 } : Color{ 190, 120, 255, 255 };
            else if (rm.isChallenge && rm.visited) c = rm.cleared ? Color{ 150, 110, 70, 255 } : Color{ 230, 140, 60, 255 };
            else if (rm.visited) c = { 120, 124, 150, 255 };
            else c = { 56, 58, 78, 255 };
            int rx = ox + (x - minX) * (cell + gap), ry = oy + (y - minY) * (cell + gap);
            DrawRectangle(rx, ry, cell, cell, c);
            if (rm.isBoss) DrawRectangle(rx + 5, ry + 5, cell - 10, cell - 10, { 30, 10, 10, 255 });
            if (rm.isStart) {                          // the castle ladder
                DrawRectangle(rx + 5, ry + 2, 2, 9, { 176, 112, 80, 255 }); DrawRectangle(rx + 10, ry + 2, 2, 9, { 176, 112, 80, 255 });
                for (int k = 0; k < 3; ++k) DrawRectangle(rx + 7, ry + 3 + k * 3, 3, 1, { 236, 196, 110, 255 });
            }
            if (cur) DrawRectangleLinesEx({ (float)rx - 2, (float)ry - 2, cell + 4.0f, cell + 4.0f }, 2, RAYWHITE);
        }
}

void Game::drawBossBar() {
    const BossDef& b = *fight.def;
    int sw = GetScreenWidth();
    float w = fminf(560.0f, sw - 760.0f);
    if (w < 300) w = 300;
    float x = (sw - w) / 2, y = 22;
    drawBossToken(currentBossIndex(), bossArt, { sw / 2.0f - ui::measure(b.name, 20) / 2.0f - 22, y + 9 }, 14);
    ui::textCentered(b.name, sw / 2.0f, y, 20, ui::ACCENT);
    float health = fight.health(enemies);
    ui::bar({ x, y + 28, w, 14 }, health, b.color);
    for (float mark : { 0.5f, 0.2f }) {          // the notches: something's coming
        float at = fight.notch(mark);
        DrawRectangleRec({ x + w * at - 1.5f, y + 24, 3, 22 }, health > at ? WHITE : Fade(WHITE, 0.3f));
    }
    const char* note = fight.strength > 1.01f ? TextFormat("weak to %s   -   x%.1f strength", b.weakness, fight.strength)
                                              : TextFormat("weak to %s", b.weakness);
    ui::textCentered(note, sw / 2.0f, y + 50, 10, ui::MUTED);
    float below = y + 66;
    if (fight.stage == BossFight::Stage::Desperate) {
        ui::textCentered("HIT THE CORE", sw / 2.0f, below, 20, { 255, 214, 130, 255 });
        ui::bar({ x + w * 0.2f, below + 24, w * 0.6f, 8 }, fight.coreHp / fight.coreMax, { 255, 190, 90, 255 });
    }
    else if (player.hp <= player.maxHp * MERCY_BELOW)
        ui::textCentered("Mercy: your last 30% counts double", sw / 2.0f, below, 10, ui::GOOD);
}

// ----------------------------------------------------------------- overlays
void Game::drawShop() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (shopMsgT > 0) shopMsgT -= GetFrameTime();
    ui::veil(0.65f);
    float pw = fminf(1000.0f, sw - 60.0f), ph = fminf(660.0f, sh - 40.0f);
    Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::panel(panel);

    // shop art across the top, Marin in front
    float artH = ph * 0.30f;
    ui::drawCover(shopTex, { panel.x + 2, panel.y + 2, pw - 4, artH });
    DrawRectangleGradientV((int)panel.x + 2, (int)(panel.y + artH - 70), (int)pw - 4, 70, Fade(ui::PANEL, 0), ui::PANEL);
    if (marinTex.id) {
        float hgt = artH * 0.95f, sc = hgt / marinTex.height;
        DrawTextureEx(marinTex, { panel.x + 16, panel.y + artH - hgt + 30 }, 0, sc, WHITE);
    }
    ui::text("Marin the Wizard", panel.x + pw * 0.28f, panel.y + artH - 50, 30, ui::ACCENT);
    ui::text("\"Potions, vapes, a little spice... all for honest gold.\"", panel.x + pw * 0.28f, panel.y + artH - 12, 20, ui::MUTED);
    drawAnim(SPR.coin, animFrame(SPR.coin, clock, 8, true), { panel.x + pw - 110, panel.y + 34 }, 2.0f);
    ui::text(TextFormat("%d", player.gold), panel.x + pw - 90, panel.y + 24, 20, ui::ACCENT);
    if (ui::button({ panel.x + pw - 46, panel.y + 12, 32, 32 }, "X", ui::Style::Normal, 20)) { play("ui"); overlay = Overlay::None; }

    float gx = panel.x + 24, gy = panel.y + artH + 26, colW = (pw - 72) / 2, rowH = 52;
    for (int i = 0; i < SHOP_COUNT; ++i) {
        const ShopItem& it = SHOP_ITEMS[i];
        Rectangle row = { gx + (i % 2) * (colW + 24), gy + (i / 2) * (rowH + 8), colW, rowH };
        bool owned = it.vape >= 0 && player.has((Vape)it.vape);
        DrawRectangleRec(row, owned ? Color{ 30, 44, 34, 255 } : ui::PANEL_LIGHT);
        DrawRectangleLinesEx(row, 1, Fade(ui::EDGE, 0.8f));
        Vector2 ic = { row.x + 28, row.y + rowH / 2 };
        if (it.vape >= 0) drawAnim(SPR.vapes, it.vape, ic, 1.4f);
        else drawAnim(i == 0 ? SPR.flaskRed : SPR.flaskBlue, animFrame(SPR.flaskRed, clock, 6, true), ic, 2.4f);
        ui::text(it.name, row.x + 54, row.y + 8, 20, ui::TEXT);
        ui::text(it.desc, row.x + 54, row.y + 32, 10, ui::MUTED);
        Rectangle buyBtn = { row.x + colW - 96, row.y + 11, 84, rowH - 22 };
        if (owned) ui::text("Owned", buyBtn.x + 6, buyBtn.y + 6, 20, ui::GOOD);
        else if (ui::button(buyBtn, TextFormat("%d g", it.price), player.gold >= it.price ? ui::Style::Primary : ui::Style::Normal, 20))
            buy(i);
    }
    if (shopMsgT > 0) ui::textCentered(shopMsg.c_str(), panel.x + pw / 2, panel.y + ph - 36, 20, Fade(ui::ACCENT, fminf(1.0f, shopMsgT)));
    ui::text(TextFormat("HP %d/%d   Mana %d/%d", (int)player.hp, (int)player.maxHp, (int)player.mana, (int)player.maxMana),
        panel.x + 24, panel.y + ph - 32, 10, ui::MUTED);
}

// Over the live game, so the buttons can be placed around the real HUD.
void Game::drawControlsEditor() {
    ui::veil(0.35f);
    if (touch::editFrame()) {
        save::writeTouchLayout(touch::layout());
        play("ui");
        overlay = Overlay::Pause;
    }
}

void Game::drawWeapon(int weapon, Rectangle box) const {
    Texture2D tx = weaponTex[weapon];
    ui::drawContain(tx, { 0, 0, (float)tx.width, (float)tx.height }, box);
}

void Game::drawGalin() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (shopMsgT > 0) shopMsgT -= GetFrameTime();
    ui::veil(0.65f);
    float pw = fminf(1000.0f, sw - 60.0f), ph = fminf(660.0f, sh - 40.0f);
    Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::panel(panel);

    // Galin, and the purse on the right
    float hdr = Clamp(ph * 0.2f, 100, 130);
    ui::drawContain(galinTex, { 0, 0, (float)galinTex.width, (float)galinTex.height }, { panel.x + 14, panel.y + 8, hdr * 1.1f, hdr - 12 });
    ui::text("Galin", panel.x + 30 + hdr * 1.1f, panel.y + hdr / 2 - 34, 30, ui::ACCENT);
    ui::text("\"Bring me a pack and I'll see what I can do.\"", panel.x + 30 + hdr * 1.1f, panel.y + hdr / 2 + 4, 20, ui::MUTED);
    drawAnim(SPR.coin, animFrame(SPR.coin, clock, 8, true), { panel.x + pw - 110, panel.y + 34 }, 2.0f);
    ui::text(TextFormat("%d", player.gold), panel.x + pw - 90, panel.y + 24, 20, ui::ACCENT);
    ui::drawContain(packTex, { 0, 0, (float)packTex.width, (float)packTex.height }, { panel.x + pw - 212, panel.y + 18, 32, 32 });
    ui::text(TextFormat("%d", player.packs), panel.x + pw - 176, panel.y + 24, 20, ui::ACCENT);
    if (ui::button({ panel.x + pw - 46, panel.y + 12, 32, 32 }, "X", ui::Style::Normal, 20)) { play("ui"); overlay = Overlay::None; }

    float top = panel.y + hdr + 6, bottom = panel.y + ph - 44;
    float leftX = panel.x + 24, leftW = (pw - 72) * 0.54f, rightX = leftX + leftW + 24, rightW = pw - 72 - leftW;
    auto rowBack = [&](Rectangle row, bool lit) {
        DrawRectangleRec(row, lit ? Color{ 52, 40, 78, 255 } : ui::PANEL_LIGHT);
        DrawRectangleLinesEx(row, lit ? 2.0f : 1.0f, lit ? ui::ACCENT : Fade(ui::EDGE, 0.8f));
    };

    // --- weapons: a tile each, cheapest first ---
    int order[WEAPON_COUNT];
    for (int i = 0; i < WEAPON_COUNT; ++i) order[i] = i;
    std::stable_sort(order, order + WEAPON_COUNT, [](int a, int b) { return WEAPONS[a].price < WEAPONS[b].price; });
    if (galinPick < 0 || galinPick >= WEAPON_COUNT) galinPick = player.weapon;
    ui::text("Weapons", leftX, top, 20, ui::ACCENT);
    constexpr int COLS = 4;
    int rows = (WEAPON_COUNT + COLS - 1) / COLS;
    float detailH = 108, gridTop = top + 28;
    float tileW = (leftW - (COLS - 1) * 8) / COLS, tileH = fminf(84.0f, (bottom - gridTop - detailH - 10) / rows - 8);
    for (int n = 0; n < WEAPON_COUNT; ++n) {
        int i = order[n];
        const WeaponDef& w = WEAPONS[i];
        Rectangle t = { leftX + (n % COLS) * (tileW + 8), gridTop + (n / COLS) * (tileH + 8), tileW, tileH };
        bool over = ui::hovered(t);
        rowBack(t, i == galinPick);
        if (over && i != galinPick) DrawRectangleLinesEx(t, 1, ui::EDGE_LIGHT);
        drawWeapon(i, { t.x + 6, t.y + 4, t.width - 12, tileH - 26 });
        ui::text(w.name, t.x + 6, t.y + tileH - 18, 10, ui::TEXT);
        const char* tag = i == player.weapon ? "In hand" : player.owns(i) ? "Yours" : TextFormat("%d g", w.price);
        Color tagC = i == player.weapon ? ui::GOOD : player.owns(i) ? ui::MUTED : player.gold >= w.price ? ui::ACCENT : ui::BAD;
        ui::textRight(tag, t.x + t.width - 6, t.y + 4, 10, tagC);
        ui::track(TextFormat("weapon %s", w.name), t);
        if (over) { ui::handCursor(); if (in::mousePressed(MOUSE_BUTTON_LEFT) && galinPick != i) { galinPick = i; play("ui"); } }
    }

    // --- the weapon looked at: what it does, and buy, take in hand or sell ---
    {
        int i = galinPick;
        const WeaponDef& w = WEAPONS[i];
        Rectangle d = { leftX, bottom - detailH, leftW, detailH };
        DrawRectangleRec(d, Fade(ui::INK, 0.6f));
        DrawRectangleLinesEx(d, 1, ui::EDGE);
        drawWeapon(i, { d.x + 8, d.y + 8, 96, d.height - 16 });
        float tx = d.x + 114;
        ui::text(w.name, tx, d.y + 10, 20, ui::TEXT);
        ui::text(TextFormat("+%d %s   %.2fs swing   %d reach   %d knockback", w.damage, w.anal ? "Anal damage" : "damage", w.swing, (int)w.reach, (int)w.knockback),
            tx, d.y + 36, 10, ui::MUTED);
        if (w.note) ui::textWrapped(w.note, tx, d.y + 52, d.width - 114 - 150, 10, ui::ACCENT);
        Rectangle b1 = { d.x + d.width - 150, d.y + 10, 140, 40 }, b2 = { d.x + d.width - 150, d.y + 58, 140, 40 };
        if (i == player.weapon) ui::textCentered("In hand", b1.x + b1.width / 2, b1.y + 14, 10, ui::GOOD);
        else if (player.owns(i)) { if (ui::button(b1, "Equip", ui::Style::Good, 20)) buyWeapon(i); }
        else if (ui::button(b1, TextFormat("Buy: %d g", w.price), player.gold >= w.price ? ui::Style::Primary : ui::Style::Normal, 20)) buyWeapon(i);
        if (i > 0 && player.owns(i)) { if (ui::button(b2, TextFormat("Sell: +%d g", sellPrice(i)), ui::Style::Danger, 10)) sellWeapon(i); }
        else if (i == 0) ui::textCentered("Galin won't take it", b2.x + b2.width / 2, b2.y + 14, 10, ui::MUTED);
        else ui::textCentered("Sells back at full price", b2.x + b2.width / 2, b2.y + 14, 10, ui::MUTED);
    }

    // --- upgrades of the weapon in hand ---
    ui::text(TextFormat("Upgrades: %s", WEAPONS[player.weapon].name), rightX, top, 20, ui::ACCENT);
    float upTop = top + 28, rowH = fminf(46.0f, (bottom - upTop - 22) / UPGRADE_COUNT - 6);
    for (int u = 0; u < UPGRADE_COUNT; ++u) {
        const UpgradeDef& up = UPGRADES[u];
        int level = player.upgrades[player.weapon][u], price = up.gold * (level + 1);
        Rectangle row = { rightX, upTop + u * (rowH + 6), rightW, rowH };
        rowBack(row, false);
        ui::text(up.name, row.x + 10, row.y + (rowH - 30) / 2 - 1, 20, ui::TEXT);
        ui::text(u == UP_SHARPEN && WEAPONS[player.weapon].anal ? "+3 Anal damage" : up.desc, row.x + 10, row.y + rowH - 14, 10, ui::MUTED);
        for (int k = 0; k < UPGRADE_MAX; ++k)     // a pip per level
            DrawRectangleRec({ row.x + 104 + k * 14, row.y + (rowH - 30) / 2 + 3, 10, 10 }, k < level ? ui::ACCENT : Color{ 16, 12, 24, 255 });
        Rectangle btn = { row.x + rightW - 160, row.y + 6, 152, rowH - 12 };
        if (level >= UPGRADE_MAX) ui::textCentered("Maxed", btn.x + btn.width / 2, btn.y + (btn.height - 10) / 2, 10, ui::GOOD);
        else if (ui::button(btn, TextFormat("%s: 1 pack, %d g", up.name, price),
            player.packs > 0 && player.gold >= price ? ui::Style::Primary : ui::Style::Normal, 10)) buyUpgrade(u);
    }
    ui::textWrapped("Every upgrade burns a cigarette pack: every Gakev carries one, and chests sometimes do.",
        rightX, bottom - 14, rightW, 10, ui::MUTED);

    if (shopMsgT > 0) ui::textCentered(shopMsg.c_str(), panel.x + pw / 2, panel.y + ph - 34, 20, Fade(ui::ACCENT, fminf(1.0f, shopMsgT)));
    else {
        std::string extra;
        if (critChance() > 0) extra += TextFormat("   Crit %d%%", (int)critChance());
        if (bladeLeech() > 0) extra += TextFormat("   +%d health a hit", bladeLeech());
        if (killCoins() > 0) extra += TextFormat("   +%d coins a kill", killCoins());
        if (WEAPONS[player.weapon].fire > 0) extra += TextFormat("   +%d fire", WEAPONS[player.weapon].fire);
        ui::text(TextFormat("In hand: %s %d   Swing %.2fs   Reach %d   Knockback %d%s", WEAPONS[player.weapon].anal ? "Anal damage" : "Damage",
            bladeDamage(), swingTime(), (int)bladeReach(), (int)bladeKnockback(), extra.c_str()), panel.x + 24, panel.y + ph - 30, 10, ui::MUTED);
    }
}

void Game::drawPause() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (saveMsgT > 0) saveMsgT -= GetFrameTime();
    ui::veil(0.72f);
    ui::textCentered("Paused", sw / 2.0f, sh / 2.0f - 200, 40, ui::ACCENT);

    float bw = 260, bh = 52, gap = 14, bx = sw / 2.0f - bw - 30, by = sh / 2.0f - 120;
    if (ui::button({ bx, by, bw, bh }, "Resume", ui::Style::Primary)) { play("ui"); overlay = Overlay::None; }
    if (ui::button({ bx, by + (bh + gap), bw, bh }, "Save", ui::Style::Normal, 20, mode == Mode::Run && !coop.active())) {
        bool ok = saveRun();
        saveMsgT = 2.0f; saveMsg = ok ? "Game saved" : "Save failed";
        play(ok ? "quest" : "hurt");
    }
    if (ui::button({ bx, by + 2 * (bh + gap), bw, bh }, coop.active() ? "Leave the game" : "Main Menu", ui::Style::Danger)) { play("ui"); toMenu(); return; }
    if (coop.active())
        ui::textCentered(coop.hosting() ? "Co-op: the game goes on while you're paused. Leaving ends it for everyone."
                                        : "Co-op: the game goes on while you're paused.", sw / 2.0f, sh / 2.0f - 150, 10, ui::MUTED);
    float msgY = by + 3 * (bh + gap);
    if (touch::enabled()) {
        if (ui::button({ bx, msgY, bw, bh }, "Touch Controls")) { play("ui"); overlay = Overlay::Controls; }
        msgY += bh + gap;
    }
    if (saveMsgT > 0) ui::textCentered(saveMsg.c_str(), bx + bw / 2, msgY + 6, 20, Fade(ui::GOOD, fminf(1.0f, saveMsgT)));

    // controls and the champion's kit
    Rectangle card = { sw / 2.0f + 10, by - 10, 380, 300 };
    ui::panel(card);
    const ChampionDef& c = champ();
    const Keybinds& k = settings.keys;
    ui::text("Controls", card.x + 20, card.y + 16, 20, ui::ACCENT);
    const char* rows[][2] = {
        { TextFormat("%s %s %s %s", keyName(k.up), keyName(k.left), keyName(k.down), keyName(k.right)), "move" },
        { keyName(k.run), "run" }, { keyName(k.roll), "dodge-roll" }, { keyName(k.interact), "talk, trade, open" },
        { "LMB", "swing the blade" }, { "RMB / Enter", "cast the selected power" }, { "1 2 3", "choose a power" },
        { keyName(k.special), c.special },
    };
    float ry = card.y + 48;
    for (auto& row : rows) {
        ui::keycap(row[0], card.x + 20, ry);
        ui::text(row[1], card.x + 130, ry + 2, 10, ui::TEXT);
        ry += 22;
    }
    ui::textWrapped(TextFormat("%s: %s", c.special, c.specialDesc), card.x + 20, ry + 8, card.width - 40, 10, ui::MUTED);
    ui::textWrapped(TextFormat("Perk: %s", c.perk), card.x + 20, ry + 40, card.width - 40, 10, ui::MUTED);
}

void Game::drawRunComplete() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ui::veil(0.7f);
    float pw = fminf(720.0f, sw - 60.0f), ph = 420;
    Rectangle p = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(p, "The Spice Trip is complete");
    ui::textWrapped("Pyraketh, the Cinder-Maw, lies still. Every boss of the Path has fallen to you.",
        p.x + 28, p.y + 80, pw - 56, 20, ui::TEXT);
    float y = p.y + 150;
    for (int i = 0; i < BOSS_COUNT; ++i) {      // the six fallen bosses
        float slot = (pw - 56) / BOSS_COUNT;
        drawBossToken(i, bossArt, { p.x + 28 + i * slot + slot / 2, y + 50 }, 44);
    }
    ui::text(TextFormat("Floor %d   Time %s   Kills %d   Gold %d   Level %d", floor, ui::clock(runTime), kills, player.gold, player.level),
        p.x + 28, y + 120, 20, ui::MUTED);
    ui::text(TextFormat("Damage dealt %d   taken %d   Deaths %d   %s%s", stats.damageDealt, stats.damageTaken,
        stats.deaths, DIFFICULTIES[difficulty].name, hardcore ? ", hardcore" : ""), p.x + 28, y + 150, 10, ui::MUTED);
    float bw = 240, by = p.y + ph - 76;
    if (coop.active()) {                       // the stairs are there for everyone: anyone on them takes the party down
        if (ui::button({ p.x + pw / 2 - bw - 10, by, bw, 50 }, "Keep going", ui::Style::Primary)) { play("ui"); overlay = Overlay::None; }
        if (ui::button({ p.x + pw / 2 + 10, by, bw, 50 }, "Leave the game", ui::Style::Normal)) { play("ui"); toMenu(); }
        return;
    }
    if (ui::button({ p.x + pw / 2 - bw - 10, by, bw, 50 }, "Descend deeper", ui::Style::Primary)) { play("ui"); descend(); }
    if (ui::button({ p.x + pw / 2 + 10, by, bw, 50 }, "Main menu", ui::Style::Normal)) { play("ui"); toMenu(); }
}

void Game::drawRushOver() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ui::veil(0.7f);
    float pw = fminf(620.0f, sw - 60.0f), ph = 330;
    Rectangle p = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(p, rushWon ? "Boss Rush conquered" : "Defeated");
    if (rushWon) {
        ui::text(TextFormat("All six bosses in %s", ui::clock(rushTime)), p.x + 28, p.y + 90, 30, ui::TEXT);
        ui::text(rushRecord ? "A new record!" : TextFormat("Best: %s", ui::clock(profile.bestRush)), p.x + 28, p.y + 140, 20,
            rushRecord ? ui::ACCENT : ui::MUTED);
    }
    else {
        ui::text(TextFormat("%s bested you at stage %d of %d.", currentBoss().name, rushStage + 1, BOSS_COUNT), p.x + 28, p.y + 90, 20, ui::TEXT);
        if (profile.bestRush > 0) ui::text(TextFormat("Best: %s", ui::clock(profile.bestRush)), p.x + 28, p.y + 130, 20, ui::MUTED);
    }
    float bw = 220, by = p.y + ph - 76;
    if (ui::button({ p.x + pw / 2 - bw - 10, by, bw, 50 }, "Try again", ui::Style::Primary)) { play("ui"); startBossRush(); }
    if (ui::button({ p.x + pw / 2 + 10, by, bw, 50 }, "Main menu", ui::Style::Normal)) { play("ui"); toMenu(); }
}

// ------------------------------------------------------------- Toshko's wheel
void Game::drawWheel(Vector2 c, float R) {
    int n = CASINO_SLOTS;
    float step = 360.0f / n;
    DrawCircleV(c, R + 10, { 60, 40, 20, 255 });
    DrawCircleV(c, R + 4, { 200, 160, 60, 255 });
    for (int i = 0; i < n; ++i) {
        float a0 = wheel.angle + i * step;
        Color col = i == 0 ? Color{ 40, 140, 60, 255 } : i % 2 ? Color{ 170, 30, 40, 255 } : Color{ 25, 22, 30, 255 };
        if (wheel.done && i == wheel.result && fmodf(clock * 4, 1) < 0.5f) col = ColorBrightness(col, 0.45f);
        DrawCircleSector(c, R, a0, a0 + step, 12, col);
        DrawLineEx(c, { c.x + cosf(a0 * DEG2RAD) * R, c.y + sinf(a0 * DEG2RAD) * R }, 2, { 230, 200, 120, 255 });
        float mid = (a0 + step / 2) * DEG2RAD;
        const char* label = i == 0 ? "T" : TextFormat("%d", i);
        ui::textCentered(label, c.x + cosf(mid) * R * 0.66f, c.y + sinf(mid) * R * 0.66f - 5, 10, RAYWHITE);
    }
    DrawCircleV(c, R * 0.14f, { 200, 160, 60, 255 });
    DrawCircleV(c, R * 0.08f, { 90, 60, 20, 255 });
    DrawTriangle({ c.x, c.y - R + 16 }, { c.x + 13, c.y - R - 16 }, { c.x - 13, c.y - R - 16 }, RAYWHITE);   // the pointer
}

void Game::drawCasino() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (!coop.joined()) updateWheel(GetFrameTime());    // (a joiner's wheel turns on the host)
    ui::veil(0.72f);
    float pw = fminf(980.0f, sw - 40.0f), ph = fminf(600.0f, sh - 40.0f);
    Rectangle p = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(p, "Toshko's Roulette");
    drawAnim(SPR.coin, animFrame(SPR.coin, clock, 8, true), { p.x + pw - 170, p.y + 34 }, 2.0f);
    ui::text(TextFormat("%d", player.gold), p.x + pw - 150, p.y + 24, 20, ui::ACCENT);
    if (!wheel.spinning && ui::button({ p.x + pw - 50, p.y + 16, 32, 32 }, "X")) { play("ui"); overlay = Overlay::None; return; }

    float R = fminf(200.0f, ph * 0.36f);
    Vector2 c = { p.x + 40 + R, p.y + 90 + R };
    drawWheel(c, R);
    const Anim& ta = SPR.enemy[EK_TOSHKO].idle;
    if (ta.ok()) drawAnim(ta, animFrame(ta, clock, 6, true), { c.x + R + 30, c.y + R - 20 }, 1.6f);

    float x = c.x + R + 80, y = p.y + 84, colW = p.x + pw - 30 - x;
    ui::text("Your bet", x, y, 20, ui::ACCENT);
    float bw = (colW - 3 * 10) / 4;
    for (int i = 0; i < 4; ++i)
        if (ui::button({ x + i * (bw + 10), y + 30, bw, 44 }, TextFormat("%d g", CASINO_BETS[i]),
            wheel.bet == CASINO_BETS[i] ? ui::Style::Good : ui::Style::Normal, 20, !wheel.spinning)) { wheel.bet = CASINO_BETS[i]; play("ui"); }
    y += 100;
    ui::text("On", x, y, 20, ui::ACCENT);
    const char* picks[3] = { "Red  x2", "Black  x2", "Toshko  x14" };
    float pbw = (colW - 2 * 10) / 3;
    for (int i = 0; i < 3; ++i)
        if (ui::button({ x + i * (pbw + 10), y + 30, pbw, 44 }, picks[i], wheel.pick == i ? ui::Style::Good : ui::Style::Normal, 20, !wheel.spinning)) {
            wheel.pick = i; play("ui");
        }
    y += 100;
    if (ui::button({ x, y, colW, 56 }, wheel.spinning ? "..." : TextFormat("Spin for %d gold", wheel.bet), ui::Style::Primary, 20,
        !wheel.spinning && player.gold >= wheel.bet)) spinWheel();
    if (!wheel.message.empty()) ui::textWrapped(wheel.message.c_str(), x, y + 74, colW, 20, wheel.done && wheel.message.find("win") != std::string::npos ? ui::GOOD : ui::MUTED);
    ui::textWrapped("\"The wheel is fair. The croupier is not.\"  - Toshko", x, p.y + ph - 50, colW, 10, ui::MUTED);
}

// A cutscene: the whole screen, its own sound, and a click, a tap or a key skips it.
void Game::drawCutscene() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    bool skip = cutscene.time > 0.3f && (in::mousePressed(MOUSE_BUTTON_LEFT) || in::pressed(KEY_ENTER) || in::pressed(KEY_SPACE)
        || in::pressed(KEY_ESCAPE) || in::pressed(settings.keys.interact));
    cutscene.update(fminf(GetFrameTime(), 0.25f));     // real time, so the pictures keep up with the sound
    if (skip || cutscene.finished()) {
        cutscene.close();
        overlay = Overlay::None;
        return;
    }
    DrawRectangle(0, 0, sw, sh, BLACK);
    cutscene.draw({ 0, 0, (float)sw, (float)sh });
    if (cutscene.time > 1.0f) {
        float a = fminf(1, (cutscene.time - 1.0f) * 2) * (0.45f + 0.15f * sinf(clock * 3));
        ui::textRight(touch::enabled() ? "tap to skip" : "click to skip", sw - 24.0f, sh - 34.0f, 20, Fade(ui::TEXT, a));
    }
}

void Game::drawSummary() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ui::veil(0.78f);
    float pw = fminf(720.0f, sw - 60.0f), ph = fminf(500.0f, sh - 40.0f);
    Rectangle p = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(p, summaryWon ? "Run complete" : "Run over");
    float x = p.x + 30, y = p.y + 80;
    drawName(player.champion, x, y, 30, ui::ACCENT);
    ui::text(TextFormat("%s%s", DIFFICULTIES[difficulty].name, hardcore ? "  -  hardcore" : ""), x, y + 40, 10, ui::MUTED);
    if (!summaryWon && stats.killer[0]) ui::text(TextFormat("Killed by %s on floor %d", stats.killer, floor), x, y + 58, 20, ui::BAD);
    // (kept as strings: TextFormat has only a few buffers, and nine of them at once overwrote each other)
    const std::pair<const char*, std::string> rows[] = {
        { "Floor reached", std::to_string(floor) }, { "Time", ui::clock(runTime) }, { "Level", std::to_string(player.level) },
        { "Kills", std::to_string(kills) }, { "Gold earned", std::to_string(stats.goldEarned) }, { "Gold lost", std::to_string(stats.goldLost) },
        { "Damage dealt", std::to_string(stats.damageDealt) }, { "Damage taken", std::to_string(stats.damageTaken) },
        { "Deaths", std::to_string(stats.deaths) },
    };
    float ry = y + 100;
    for (int i = 0; i < (int)(sizeof rows / sizeof rows[0]); ++i) {
        float cx = x + (i % 2) * (pw / 2 - 10), cy = ry + (i / 2) * 30;
        ui::text(rows[i].first, cx, cy, 20, ui::MUTED);
        ui::textRight(rows[i].second.c_str(), cx + pw / 2 - 60, cy, 20, ui::TEXT);
    }
    float bw = 220, by = p.y + ph - 74;
    if (ui::button({ p.x + pw / 2 - bw - 10, by, bw, 50 }, "New Run", ui::Style::Primary)) { play("ui"); toMenu(); screen = Screen::Setup; return; }
    if (ui::button({ p.x + pw / 2 + 10, by, bw, 50 }, "Main menu")) { play("ui"); toMenu(); }
}

// ------------------------------------------------------------ Momiche's scare
// The fearful face over a scared enemy: above it, or below when that would run into the top wall.
void Game::drawFear(float x, float above, float below, float size) const {
    if (scareT <= 0 || !fearTex.id) return;
    float y = (above < TILE * 2 ? below : above) + sinf(clock * 8 + x) * 2;
    DrawTexturePro(fearTex, { 0, 0, (float)fearTex.width, (float)fearTex.height },
        { x - size / 2, y - size / 2, size, size }, { 0, 0 }, 0, WHITE);
}

// A champion's name, with the fearful face after it for Momiche😨 (the pixel font has no emoji).
void Game::drawName(int champion, float x, float y, int size, Color color, bool centred) const {
    const ChampionDef& c = CHAMPIONS[champion];
    float w = (float)ui::measure(c.name, size);
    float face = c.fearFace && fearTex.id ? size * 1.2f : 0;
    float total = w + (face > 0 ? face + 3 : 0);
    float x0 = centred ? x - total / 2 : x;
    ui::text(c.name, x0, y, size, color);
    if (face > 0)
        DrawTexturePro(fearTex, { 0, 0, (float)fearTex.width, (float)fearTex.height },
            { x0 + w + 3, y + (size - face) / 2, face, face }, { 0, 0 }, 0, WHITE);
}
