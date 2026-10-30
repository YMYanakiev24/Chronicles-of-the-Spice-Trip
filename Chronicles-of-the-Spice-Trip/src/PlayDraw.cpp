// ============================================================================
//  PlayDraw.cpp — the dungeon on screen: room, actors, HUD and overlays.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Render.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>

// --------------------------------------------------------------- the screen
void Game::playScreen() {
    float dt = fminf(GetFrameTime(), 0.05f);
    if (in::pressed(KEY_ESCAPE)) {
        if (overlay == Overlay::Shop || overlay == Overlay::Pause) overlay = Overlay::None;
        else if (overlay == Overlay::None) overlay = Overlay::Pause;
        play("ui");
    }
    if (overlay == Overlay::None) updatePlay(dt);
    if (screen != Screen::Play) return;            // left for the menu this frame

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
    BeginMode2D(cam);
    drawRoom();
    drawActors();
    EndMode2D();

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (gloom > 0.001f) DrawRectangle(0, 0, sw, sh, Fade({ 4, 2, 10, 255 }, gloom));
    DrawRectangle(0, 0, sw, sh, Fade(FOG_TINT, 0.04f));
    if (player.auraT > 0) DrawRectangle(0, 0, sw, sh, Fade({ 120, 180, 255, 255 }, 0.06f));
    drawHud();

    switch (overlay) {
    case Overlay::Shop:        drawShop(); break;
    case Overlay::Pause:       drawPause(); break;
    case Overlay::RunComplete: drawRunComplete(); break;
    case Overlay::RushOver:    drawRushOver(); break;
    default: break;
    }
}

// ----------------------------------------------------------------- the room
// The boss key: the pack's spinning gold key, or a drawn one if the sprite is missing.
static void drawKey(Vector2 c, float s) {
    if (SPR.key.ok()) { drawAnim(SPR.key, animFrame(SPR.key, (float)GetTime(), 6, true), c, 2.0f * s); return; }
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
    int spikeFrame = trapPh < 1.2f ? 0 : trapPh < 1.35f ? 1 : trapPh < 2.25f ? 2 : 3;
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
            bool innerWall = !border && r.tiles[ty][tx] == RT_WALL;
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
                if (isDoor && (!r.cleared || dungeon.bossDoorLocked(dir))) drawTile(8, 3, px, py);   // barred door
                else if (!border && r.tiles[ty][tx] == RT_SPIKES)
                    drawAnim(SPR.peaks, spikeFrame, { px + TILE / 2.0f, py + TILE / 2.0f }, 2.0f);
                if (ty == 1 && tx == 1 && !r.isBoss) drawTile(4, 6, px, py);              // cobwebs
                if (ty == 1 && tx == ROOM_TW - 2 && !r.isBoss) drawTile(5, 6, px, py);
            }
            else if (innerWall) drawTile(1 + (h >> 4) % 4, 0, px, py);   // brick block
            else if (ty == 0) drawTile(tx == 0 ? 0 : tx == ROOM_TW - 1 ? 5 : 1 + tx % 4, 0, px, py);
            else if (ty == ROOM_TH - 1) drawTile(tx == 0 ? 0 : tx == ROOM_TW - 1 ? 5 : 1 + tx % 4, 4, px, py);
            else drawTile(tx == 0 ? 0 : 5, 1 + ty % 3, px, py);
        }

    if (arena.id) {             // the boss's painted arena, framed by the walls
        Rectangle inner = { (float)TILE, (float)TILE, ROOM_PX_W - 2 * TILE, ROOM_PX_H - 2 * TILE };
        ui::drawCover(arena, inner, r.cleared ? WHITE : Color{ 214, 208, 226, 255 });
        DrawRectangleLinesEx(inner, 3, Color{ 0, 0, 0, 150 });
    }

    // wall torches (unlit in the boss lair), banners, candles
    if (SPR.torch.ok())
        for (float x : { 4 * (float)TILE + 16, (ROOM_TW - 5) * (float)TILE + 16 }) {
            if (bossDark) drawTile(2, 9, x - 16, 14 - 16);
            else drawAnim(SPR.torch, animFrame(SPR.torch, clock + x * 0.01f, 8, true), { x, 14 }, 2.0f);
        }
    if (SPR.sideTorch.ok())
        for (int ty : { 2, ROOM_TH - 3 })
            for (float x : { 18.0f, ROOM_PX_W - 18 }) {
                Vector2 at = { x, ty * (float)TILE + 16 };
                if (bossDark) drawTile(2, 9, at.x - 16, at.y - 16);
                else drawAnim(SPR.sideTorch, animFrame(SPR.sideTorch, clock + x * 0.01f, 8, true), at, 2.0f, x > ROOM_PX_W / 2);
            }
    if (r.isBoss && !arena.id)
        for (int i = 0; i < 4; ++i)
            drawAnim(SPR.flag, animFrame(SPR.flag, clock + i * 0.3f, 6, true), { (2 + i * 5 + (i > 1 ? 2 : 0)) * (float)TILE + 16, 14 }, 2.0f);
    if (r.isStart && mode == Mode::Run) {
        drawAnim(SPR.candle, animFrame(SPR.candle, clock, 7, true), { ORACLE_POS.x - 34, ORACLE_POS.y - 4 }, 2.0f);
        drawAnim(SPR.candle, animFrame(SPR.candle, clock + 0.3f, 7, true), { ORACLE_POS.x + 34, ORACLE_POS.y - 4 }, 2.0f);
        drawAnim(SPR.candle2, animFrame(SPR.candle2, clock + 0.5f, 7, true), { ROOM_PX_W - 3 * TILE, 2.5f * TILE }, 2.0f);
        drawAnim(SPR.candle2, animFrame(SPR.candle2, clock + 0.1f, 7, true), { ROOM_PX_W - 3 * TILE, ROOM_PX_H - 2.5f * TILE }, 2.0f);
    }
}

// --------------------------------------------------------------- the actors
void Game::drawActors() {
    Room& r = dungeon.cur();

    // the staircase down, once the boss falls
    if (mode == Mode::Run && r.isBoss && r.cleared) {
        float sx = ROOM_CENTER.x, sy = ROOM_CENTER.y;
        DrawRectangle((int)sx - 30, (int)sy - 24, 60, 48, { 8, 8, 12, 255 });
        for (int i = 0; i < 4; ++i)
            DrawRectangle((int)sx - 26 + i * 3, (int)sy - 18 + i * 11, 52 - i * 6, 9,
                { (unsigned char)(78 - i * 14), (unsigned char)(74 - i * 14), (unsigned char)(96 - i * 16), 255 });
        DrawRectangleLinesEx({ sx - 30, sy - 24, 60, 48 }, 2, { 150, 140, 90, 255 });
    }

    if (r.hasKey && r.cleared && !dungeon.keyFound) {      // the boss key, bobbing and glinting
        Vector2 k = { ROOM_CENTER.x, ROOM_CENTER.y - 6 + sinf(clock * 3) * 3 };
        drawShadow({ ROOM_CENTER.x, ROOM_CENTER.y + 8 }, 8);
        drawKey(k, 1.0f);
    }

    if (r.hasChest) {
        Vector2 c = { r.chestTx * (float)TILE + 16, r.chestTy * (float)TILE + 16 };
        drawShadow({ c.x, c.y + 8 }, 12);
        if (SPR.chest.ok()) {
            if (r.chestOpened) drawAnim(SPR.chestOpen, animFrame(SPR.chestOpen, chestOpenT, 10, false), c, 2.0f);
            else drawAnim(SPR.chest, fmodf(clock, 2.5f) < 0.5f ? animFrame(SPR.chest, clock, 8, true) : 0, c, 2.0f);  // rattles now and then
        }
        else {
            DrawRectangle((int)c.x - 11, (int)c.y - 6, 22, 14, { 120, 78, 40, 255 });
            DrawRectangle((int)c.x - 2, (int)c.y - 6, 4, 5, { 230, 200, 90, 255 });
        }
    }

    if (r.isShop) {             // rug, candles and Marin himself, gently bobbing
        Vector2 m = MERCHANT_POS;
        Rectangle rug = { m.x - 70, m.y - 6, 140, 60 };
        DrawRectangleRec(rug, { 70, 34, 70, 255 });
        DrawRectangleLinesEx(rug, 3, { 200, 160, 80, 255 });
        drawAnim(SPR.candle, animFrame(SPR.candle, clock, 7, true), { m.x - 54, m.y + 6 }, 2.0f);
        drawAnim(SPR.candle, animFrame(SPR.candle, clock + 0.4f, 7, true), { m.x + 54, m.y + 6 }, 2.0f);
        drawShadow({ m.x, m.y + 14 }, 16);
        if (marinTex.id) {
            float hgt = 70, sc = hgt / marinTex.height, bob = sinf(clock * 2.2f) * 2;
            DrawTextureEx(marinTex, { m.x - marinTex.width * sc / 2, m.y - hgt + 26 + bob }, 0, sc, WHITE);
        }
        else DrawCircleV(m, 14, { 90, 60, 160, 255 });
        for (int i = 0; i < 3; ++i)
            drawAnim(SPR.coin, animFrame(SPR.coin, clock + i * 0.25f, 10, true), { m.x - 30 + i * 30.0f, m.y + 40 }, 1.2f);
    }

    if (r.isStart && mode == Mode::Run) {
        drawShadow(ORACLE_POS, 12);
        if (SPR.oracle.ok())
            drawAnim(SPR.oracle, animFrame(SPR.oracle, clock, 5, true), { ORACLE_POS.x, ORACLE_POS.y - 6 }, 2.2f, player.pos.x < ORACLE_POS.x);
        else {
            DrawRectangle((int)ORACLE_POS.x - 7, (int)ORACLE_POS.y - 12, 14, 22, { 202, 168, 232, 255 });
            DrawCircle((int)ORACLE_POS.x, (int)ORACLE_POS.y - 14, 6, { 244, 226, 250, 255 });
        }
    }

    auto drawBody = [&](const Enemy& e) {
        if (e.bossForm >= 0) drawBossBody(e, currentBossIndex(), bossArt);
        else drawEnemy(e);
    };
    drawLabFloor(fight, clock);             // Triceps' wells, wires and the staff's reach lie on the floor
    for (auto& c : corpses) drawBody(c);
    for (auto& e : enemies) drawBody(e);
    for (auto& e : enemies) {               // a fearful face bobs over everyone Momiche scared
        if (e.dying) continue;
        if (e.bossForm >= 0) {
            float top = bossTop(e, currentBossIndex(), bossArt);         // over the head of the token or figure
            drawFear(e.pos.x, top - 16, e.pos.y + e.r * 0.9f + 16, 26);
        }
        else drawFear(e.pos.x, e.pos.y - e.r * 2 - 14, e.pos.y + e.r + 10, 16);
    }
    for (auto& c : coins) drawAnim(SPR.coin, animFrame(SPR.coin, c.t, 12, true), c.pos, 1.4f);

    // the hero, with their champion's effects
    if (scareT > SCARE_TIME - 0.6f)         // the scream: two rings racing outward
        for (int i = 0; i < 2; ++i) {
            float k = Clamp((SCARE_TIME - scareT - i * 0.12f) / 0.6f, 0, 1);
            float r = (30 + 300 * k) / 2;
            if (k > 0 && k < 1) DrawRing(player.pos, r - 2, r, 0, 360, 48, Fade({ 166, 191, 255, 255 }, 0.7f * (1 - k)));
        }
    float lift = player.jumpT > 0 ? sinf((1 - player.jumpT / 0.6f) * PI) * 22 : 0;
    if (player.auraT > 0) {
        float pulse = 34 + sinf(clock * 6) * 3;
        DrawCircleV(player.pos, pulse, Fade({ 130, 200, 255, 255 }, 0.12f));
        DrawCircleLinesV(player.pos, pulse, Fade({ 170, 220, 255, 255 }, 0.7f));
    }
    if (player.dashT > 0 || player.rollAtkT > 0)
        DrawCircleV(player.pos, PLAYER_R + 4, Fade(player.rollAtkT > 0 ? Color{ 110, 200, 80, 255 } : Color{ 245, 210, 110, 255 }, 0.35f));
    // walking as their own figure, facing the way they walk (or aim, standing still)
    drawChampion(walk[player.champion], player.pos, heroMoving ? heroMoveDir : player.facing, heroMoving, clock,
        player.facing, player.meleeAnim, player.hitFlash > 0, champ().cloak, lift);
    if (player.shieldT > 0) {
        DrawCircleV({ player.pos.x, player.pos.y - 6 }, 22, Fade({ 230, 190, 120, 255 }, 0.18f));
        DrawCircleLinesV({ player.pos.x, player.pos.y - 6 }, 22, Fade({ 250, 220, 150, 255 }, 0.9f));
    }

    for (auto& p : shots) drawProjectile(p, clock);
    for (auto& p : enemyShots) drawProjectile(p, clock);
    drawBus(fight.bus, clock);
    drawLabAir(fight, clock);

    for (auto& pt : parts) {
        Color c = pt.color; c.a = (unsigned char)(255 * (pt.life / pt.maxLife));
        DrawCircleV(pt.pos, pt.size, c);
    }
    for (auto& pu : popups)
        drawAnim(*pu.anim, animFrame(*pu.anim, pu.t, 8, true), pu.pos, 2.0f, false, Fade(WHITE, fminf(1.0f, 1.4f - pu.t)));
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
    for (auto& f : floaters) {
        Vector2 s = GetWorldToScreen2D(f.pos, cam);
        ui::textCentered(TextFormat("%d", f.value), s.x, s.y, 20, Fade(f.color, fminf(1, 2.5f * (0.8f - f.t))));
    }

    // --- interaction prompts ---
    const char* E = keyName(settings.keys.interact);
    Vector2 chestPos = { r.chestTx * (float)TILE + 16, r.chestTy * (float)TILE + 16 };
    if (r.hasChest && !r.chestOpened && r.cleared && Vector2Distance(player.pos, chestPos) < 40)
        prompt(cam, { chestPos.x, chestPos.y - 34 }, E, "open");
    if (r.isShop && overlay != Overlay::Shop && Vector2Distance(player.pos, MERCHANT_POS) < 56)
        prompt(cam, { MERCHANT_POS.x, MERCHANT_POS.y - 66 }, E, "trade with Marin");
    if (r.isStart && mode == Mode::Run && dialogue.empty() && Vector2Distance(player.pos, ORACLE_POS) < 48)
        prompt(cam, { ORACLE_POS.x, ORACLE_POS.y - 40 }, E, "talk");
    if (mode == Mode::Run && r.isBoss && r.cleared && Vector2Distance(player.pos, ROOM_CENTER) < 70)
        ui::textCentered("Descend", GetWorldToScreen2D({ ROOM_CENTER.x, ROOM_CENTER.y - 46 }, cam).x,
            GetWorldToScreen2D({ ROOM_CENTER.x, ROOM_CENTER.y - 46 }, cam).y, 20, ui::ACCENT);

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
    const char* where = mode == Mode::Run ? TextFormat("Floor %d   %s", floor, ui::clock(runTime))
                                          : TextFormat("Boss Rush %d/%d   %s", rushStage + 1, BOSS_COUNT, ui::clock(rushTime));
    ui::text(where, box.x + 100, box.y + box.height + 16, 10, ui::MUTED);

    // active vapes and champion effects
    float iy = box.y + box.height + 54, ix = box.x + 14;
    for (int v = 0; v < VP_COUNT; ++v) {
        if (!player.buzzing((Vape)v)) continue;
        if (SPR.vapes.ok()) drawAnim(SPR.vapes, v, { ix + 10, iy + 8 }, 0.9f);
        ui::text(TextFormat("%ds", (int)ceilf(player.vapeT[v])), ix + 26, iy + 2, 10, VAPE_COLOR[v]);
        ix += 64;
    }
    auto effect = [&](float left, const char* name, Color col) {
        if (left <= 0) return;
        ui::text(TextFormat("%s %.0fs", name, ceilf(left)), ix, iy + 2, 10, col);
        ix += ui::measure(TextFormat("%s %.0fs", name, ceilf(left)), 10) + 14;
    };
    effect(player.auraT, "Calm", { 150, 210, 255, 255 });
    effect(player.shieldT, "Mug-Shield", { 240, 200, 130, 255 });
    effect(player.wobbleT, "Tipsy", { 230, 150, 150, 255 });

    // --- hotbar (bottom centre): three powers and the special ---
    float slot = 60, gap = 10, total = 4 * slot + 3 * gap + 14;
    float hx = (sw - total) / 2, hy = sh - slot - 34;
    for (int i = 0; i < SPELL_COUNT; ++i) {
        Rectangle s = { hx + i * (slot + gap), hy, slot, slot };
        bool sel = i == player.selected;
        int cost = player.buzzing(VP_RAZZ) ? 0 : (int)ceilf(SPELLS[i].manaCost * c.manaCostMul);
        bool castable = player.mana >= cost;
        DrawRectangleRec(s, { 16, 14, 24, 230 });
        if (SPR.spellIcons.ok()) drawAnim(SPR.spellIcons, i, { s.x + slot / 2, s.y + slot / 2 - 4 }, 1.1f, false, castable ? WHITE : Color{ 90, 90, 100, 255 });
        else DrawRectangle((int)s.x + 8, (int)s.y + 8, (int)slot - 16, 24, SPELLS[i].color);
        DrawRectangleLinesEx(s, sel ? 3.0f : 1.0f, sel ? ui::ACCENT : ui::EDGE);
        ui::text(TextFormat("%d", i + 1), s.x + 5, s.y + 4, 10, ui::TEXT);
        ui::textRight(TextFormat("%d", cost), s.x + slot - 5, s.y + slot - 14, 10, castable ? ui::MANA : ui::BAD);
        ui::textCentered(SPELLS[i].name, s.x + slot / 2, s.y + slot + 6, 10, sel ? ui::ACCENT : ui::MUTED);
    }
    {
        Rectangle s = { hx + 3 * (slot + gap) + 14, hy, slot, slot };
        DrawRectangleRec(s, { 30, 20, 40, 235 });
        if (champTex[player.champion].id) {
            Texture2D tx = champTex[player.champion];
            ui::drawContain(tx, { c.crop.x * tx.width, c.crop.y * tx.height, c.crop.width * tx.width, c.crop.height * tx.height },
                { s.x + 4, s.y + 4, slot - 8, slot - 8 });
        }
        else DrawCircleV({ s.x + slot / 2, s.y + slot / 2 }, 16, Fade({ 150, 110, 230, 255 }, 0.6f));
        bool metered = c.kind == Special::CoinBarrage || c.kind == Special::Blink;
        float meter = c.kind == Special::CoinBarrage ? player.charge / COIN_POWER_MAX : player.charge / DUST_MAX;
        bool ready = player.specialCd <= 0 && (c.kind != Special::CoinBarrage || player.charge >= COIN_POWER_MAX)
            && (c.kind != Special::Blink || player.charge >= DUST_PER_BLINK);
        if (player.specialCd > 0)           // cooldown curtain
            DrawRectangle((int)s.x, (int)s.y, (int)slot, (int)(slot * player.specialCd / c.cooldown), { 0, 0, 0, 170 });
        else if (!ready) DrawRectangleRec(s, { 0, 0, 0, 110 });
        if (metered) ui::bar({ s.x + 4, s.y + slot - 8, slot - 8, 4 }, meter, c.kind == Special::Blink ? Color{ 230, 160, 255, 255 } : ui::ACCENT);
        DrawRectangleLinesEx(s, 2, ready ? ui::ACCENT : ui::EDGE);
        ui::keycap(keyName(settings.keys.special), s.x + 3, s.y + 3);
        ui::textCentered(c.special, s.x + slot / 2, s.y + slot + 6, 10, ready ? ui::ACCENT : ui::MUTED);
    }

    drawMinimap();
    if (fight.active()) drawBossBar();

    // --- the oracle's words ---
    if (!dialogue.empty()) {
        float w = fminf(760.0f, sw - 80.0f);
        Rectangle d = { (sw - w) / 2, hy - 140, w, 112 };
        ui::panel(d);
        ui::text("Theira, the Last Oracle", d.x + 24, d.y + 16, 20, { 202, 168, 232, 255 });
        ui::textWrapped(dialogue.c_str(), d.x + 24, d.y + 46, w - 48, 20, ui::TEXT);
        ui::keycap(E, d.x + w - 110, d.y + d.height - 26);
        ui::text("continue", d.x + w - 92, d.y + d.height - 24, 10, ui::MUTED);
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
    if (r.isStart && mode == Mode::Run && floor == 1)
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
            else if (rm.isShop && rm.visited) c = { 220, 180, 70, 255 };
            else if (rm.visited) c = { 120, 124, 150, 255 };
            else c = { 56, 58, 78, 255 };
            int rx = ox + (x - minX) * (cell + gap), ry = oy + (y - minY) * (cell + gap);
            DrawRectangle(rx, ry, cell, cell, c);
            if (rm.isBoss) DrawRectangle(rx + 5, ry + 5, cell - 10, cell - 10, { 30, 10, 10, 255 });
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
    ui::bar({ x, y + 28, w, 14 }, fight.health(enemies), b.color);
    const char* note = fight.strength > 1.01f ? TextFormat("weak to %s   -   x%.1f strength", b.weakness, fight.strength)
                                              : TextFormat("weak to %s", b.weakness);
    ui::textCentered(note, sw / 2.0f, y + 50, 10, ui::MUTED);
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
    ui::text("\"Potions, powers, a little spice... all for honest gold.\"", panel.x + pw * 0.28f, panel.y + artH - 12, 20, ui::MUTED);
    drawAnim(SPR.coin, animFrame(SPR.coin, clock, 8, true), { panel.x + pw - 110, panel.y + 34 }, 2.0f);
    ui::text(TextFormat("%d", player.gold), panel.x + pw - 90, panel.y + 24, 20, ui::ACCENT);
    if (ui::button({ panel.x + pw - 46, panel.y + 12, 32, 32 }, "X", ui::Style::Normal, 20)) { play("ui"); overlay = Overlay::None; }

    float gx = panel.x + 24, gy = panel.y + artH + 26, colW = (pw - 72) / 2, rowH = 52;
    for (int i = 0; i < SHOP_COUNT; ++i) {
        const ShopItem& it = SHOP_ITEMS[i];
        Rectangle row = { gx + (i % 2) * (colW + 24), gy + (i / 2) * (rowH + 8), colW, rowH };
        bool owned = it.skill && player.has((Skill)it.skill);
        DrawRectangleRec(row, owned ? Color{ 30, 44, 34, 255 } : ui::PANEL_LIGHT);
        DrawRectangleLinesEx(row, 1, Fade(ui::EDGE, 0.8f));
        Vector2 ic = { row.x + 28, row.y + rowH / 2 };
        if (it.vape >= 0 && SPR.vapes.ok()) drawAnim(SPR.vapes, it.vape, ic, 1.4f);
        else if (!it.skill) drawAnim(i == 0 ? SPR.flaskRed : SPR.flaskBlue, animFrame(SPR.flaskRed, clock, 6, true), ic, 2.4f);
        else {
            DrawCircleV(ic, 16, Fade({ 150, 110, 230, 255 }, 0.35f + 0.15f * sinf(clock * 3 + i)));
            DrawCircleLinesV(ic, 16, { 200, 170, 255, 255 });
            ui::textCentered(TextFormat("%c", it.name[0]), ic.x, ic.y - 10, 20, ui::TEXT);
        }
        ui::text(it.name, row.x + 54, row.y + 8, 20, ui::TEXT);
        ui::text(it.desc, row.x + 54, row.y + 32, 10, ui::MUTED);
        Rectangle buyBtn = { row.x + colW - 96, row.y + 11, 84, rowH - 22 };
        if (owned) ui::text("Learned", buyBtn.x + 6, buyBtn.y + 6, 20, ui::GOOD);
        else if (ui::button(buyBtn, TextFormat("%d g", it.price), player.gold >= it.price ? ui::Style::Primary : ui::Style::Normal, 20))
            buy(i);
    }
    if (shopMsgT > 0) ui::textCentered(shopMsg.c_str(), panel.x + pw / 2, panel.y + ph - 36, 20, Fade(ui::ACCENT, fminf(1.0f, shopMsgT)));
    ui::text(TextFormat("HP %d/%d   Mana %d/%d", (int)player.hp, (int)player.maxHp, (int)player.mana, (int)player.maxMana),
        panel.x + 24, panel.y + ph - 32, 10, ui::MUTED);
}

void Game::drawPause() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (saveMsgT > 0) saveMsgT -= GetFrameTime();
    ui::veil(0.72f);
    ui::textCentered("Paused", sw / 2.0f, sh / 2.0f - 200, 40, ui::ACCENT);

    float bw = 260, bh = 52, gap = 14, bx = sw / 2.0f - bw - 30, by = sh / 2.0f - 120;
    if (ui::button({ bx, by, bw, bh }, "Resume", ui::Style::Primary)) { play("ui"); overlay = Overlay::None; }
    if (ui::button({ bx, by + (bh + gap), bw, bh }, "Save", ui::Style::Normal, 20, mode == Mode::Run)) {
        bool ok = saveRun();
        saveMsgT = 2.0f; saveMsg = ok ? "Game saved" : "Save failed";
        play(ok ? "quest" : "hurt");
    }
    if (ui::button({ bx, by + 2 * (bh + gap), bw, bh }, "Main Menu", ui::Style::Danger)) { play("ui"); toMenu(); return; }
    if (saveMsgT > 0) ui::textCentered(saveMsg.c_str(), bx + bw / 2, by + 3 * (bh + gap) + 6, 20, Fade(ui::GOOD, fminf(1.0f, saveMsgT)));

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
    float bw = 240, by = p.y + ph - 76;
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
