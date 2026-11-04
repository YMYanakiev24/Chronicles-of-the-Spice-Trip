// ============================================================================
//  Castle.cpp — the corridor and the shopkeepers' rooms (see Castle.h), and
//  Beray's outfits.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Touch.h"
#include "Render.h"
#include "Ui.h"
#include "rlgl.h"
#include <cmath>

using namespace castle;

namespace castle {

    // In front of each shop's door in the corridor, where you come out of it.
    static constexpr Vector2 OUT_MARIN = { 268, 432 }, OUT_GALIN = { 535, 432 }, OUT_CASINO = { 790, 432 }, OUT_DRIP = { 1155, 450 };
    static constexpr Vector2 UP = { 0, -1 }, DOWN = { 0, 1 }, LEFT = { -1, 0 };

    // A shop room: its floor, the way back out (the Hallway door on the left, or the
    // carpet at the bottom), and what stands in the way.
    static PlaceDef shopRoom(const char* name, const char* caption, const char* art, Vector2 out, std::vector<Rectangle> blocks,
                             Vector2 keeper, float keeperR, const char* label, Rectangle counter) {
        PlaceDef p{ name, caption, art, 2.0f, 2.0f,
            { { 95, 362, 1210, 338 }, { 50, 440, 50, 96 }, { 765, 690, 80, 40 } },
            std::move(blocks),
            { { { 78, 488 }, 46, LEFT, CORRIDOR, out, DOWN, "Hallway" },
              { { 805, 712 }, 50, DOWN, CORRIDOR, out, DOWN, "Hallway" } } };
        p.blocks.push_back({ 322, 352, 90, 30 });           // the firewood by the fireplace
        p.keeper = keeper; p.keeperR = keeperR; p.keeperLabel = label; p.counter = counter;
        return p;
    }

    static const std::vector<PlaceDef>& places() {
        static const std::vector<PlaceDef> P = [] {
            std::vector<PlaceDef> v(PLACE_COUNT);
            v[CORRIDOR] = { "Castle Corridor",
                "CASTLE CORRIDOR - MARIN'S VAPE SHOP, GALIN'S WEAPON SHOP, TODOR'S CASINO AND BERAY'S DRIP LOCKER",
                "castle/corridor.png", 2.0f, 2.0f,
                { { 95, 398, 1212, 302 }, { 50, 440, 50, 96 } },
                { { 1085, 380, 145, 40 }, { 1268, 600, 42, 110 } },
                { { { 268, 404 }, 50, UP, MARIN, { 805, 680 }, UP, "Marin's Vape Shop" },
                  { { 535, 404 }, 50, UP, GALIN, { 805, 680 }, UP, "Galin's Weapon Shop" },
                  { { 790, 404 }, 50, UP, CASINO, { 805, 680 }, UP, "Todor's Casino" },
                  { { 1155, 440 }, 58, UP, DRIP, { 560, 728 }, UP, "Beray's Drip Locker" },
                  { { 78, 488 }, 46, LEFT, -1, {}, DOWN, "Back to the dungeon" } } };
            v[MARIN] = shopRoom("Marin's Vape Shop", "MARIN'S ARCANE VAPES - WIZARD SUPPLIES - ELIXIRS ACCEPTED", "castle/marin.png", OUT_MARIN,
                { { 1088, 352, 214, 98 }, { 1195, 552, 112, 148 } }, { 1192, 480 }, 90, "trade with Marin", { 1088, 340, 214, 106 });
            v[GALIN] = shopRoom("Galin's Weapon Shop", "GALIN'S WEAPON SHOP - DOBRICH STOCK - PACKS ACCEPTED", "castle/galin.png", OUT_GALIN,
                { { 932, 352, 370, 98 }, { 1150, 452, 165, 245 }, { 968, 640, 48, 60 } }, { 1060, 480 }, 100, "trade with Galin", { 932, 346, 370, 104 });
            v[CASINO] = shopRoom("Todor's Casino", "TODOR'S CASINO - ROULETTE & CARDS - LEV & COINS ACCEPTED", "castle/casino.png", OUT_CASINO,
                { { 1088, 352, 214, 98 }, { 1195, 552, 112, 148 } }, { 1192, 480 }, 90, "gamble with Todor", { 1088, 340, 214, 106 });
            v[DRIP] = { "Beray's Drip Locker", "BERAY'S DRIP LOCKER - FRESH GEAR FOR EVERY CHAMPION - BEST PRICE",
                "castle/drip.png", 4.4f, 2.8f,
                { { 130, 612, 610, 150 } },
                { { 120, 600, 252, 42 }, { 340, 596, 190, 44 }, { 688, 560, 210, 150 } },
                { { { 440, 760 }, 330, DOWN, CORRIDOR, OUT_DRIP, DOWN, "Corridor" } } };
            v[DRIP].keeper = { 585, 690 }; v[DRIP].keeperR = 125; v[DRIP].keeperLabel = "shop with Beray";
            return v;
        }();
        return P;
    }

    const PlaceDef& place(int p) { return places()[p]; }

    bool walkable(int p, Vector2 feet) {
        const PlaceDef& P = place(p);
        for (const Rectangle& b : P.blocks) if (CheckCollisionPointRec(feet, b)) return false;
        for (const Rectangle& f : P.floors) if (CheckCollisionPointRec(feet, f)) return true;
        return false;
    }

} // namespace castle

// ------------------------------------------------------------------ coming and going
void Game::enterCastle() {
    hall = Hall{};
    hall.active = true; hall.place = CORRIDOR; hall.pos = ENTRY; hall.face = { 1, 0 };
    dialogue.clear(); vasilGreetT = 0;
    overlay = Overlay::None;
    showBanner("Castle Corridor", "Marin, Galin, Todor and Beray", 2.0f);
    play("lid");
}

void Game::leaveCastle() {
    hall.active = false;
    overlay = Overlay::None;
    castleDoorCd = 0.5f;                            // the E that brought you out doesn't take you straight back in
    player.pos = CASTLE_DOOR_FRONT; player.facing = { 0, 1 }; heroMoveDir = { 0, 1 };
    play("lid");
    autosave();                                     // what you bought is kept
}

Camera2D Game::castleCamera() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Texture2D art = castleArt[hall.place];
    float w = art.id ? (float)art.width : 1408, h = art.id ? (float)art.height : 730;
    Camera2D cam{};
    cam.offset = { sw / 2.0f, sh / 2.0f };
    cam.target = { w / 2, h / 2 };
    cam.zoom = fminf(sw / w, sh / h);
    return cam;
}

void Game::openKeeper(int p) {
    play("ui");
    switch (p) {
    case MARIN: overlay = Overlay::Shop; break;
    case GALIN: overlay = Overlay::Galin; break;
    case CASINO: {
        int bet = wheel.bet, pick = wheel.pick;     // remember the last stake and colour
        wheel = Wheel{}; wheel.casino = true; wheel.bet = bet; wheel.pick = pick;
        overlay = Overlay::Casino;
        break;
    }
    case DRIP: dripChampion = player.champion; dripHover = -1; overlay = Overlay::Drip; break;
    default: break;
    }
}

void Game::updateCastle(float dt) {
    Hall& H = hall;
    const PlaceDef& P = place(H.place);
    Keybinds& K = settings.keys;
    if (banner > 0) banner -= dt;
    H.shown += dt;

    Vector2 mv{ 0, 0 };
    if (in::down(K.up)) mv.y -= 1;
    if (in::down(K.down)) mv.y += 1;
    if (in::down(K.left)) mv.x -= 1;
    if (in::down(K.right)) mv.x += 1;
    H.moving = false;
    if (mv.x || mv.y) {
        mv = Vector2Normalize(mv);
        float speed = (in::down(K.run) ? RUN : WALK) * P.speed * champ().speedMul;
        Vector2 d = Vector2Scale(mv, speed * dt);
        if (walkable(H.place, { H.pos.x + d.x, H.pos.y })) H.pos.x += d.x;
        if (walkable(H.place, { H.pos.x, H.pos.y + d.y })) H.pos.y += d.y;
        H.face = mv; H.moving = true;
    }

    bool use = in::pressed(K.interact);
    if (P.keeperLabel && Vector2Distance(H.pos, P.keeper) < P.keeperR && use) { openKeeper(H.place); return; }
    for (const Exit& e : P.exits) {
        if (Vector2Distance(H.pos, e.at) > e.r) continue;
        bool pushing = Vector2DotProduct(mv, e.push) > 0.7f && !walkable(H.place, Vector2Add(H.pos, Vector2Scale(e.push, 8)));
        if (!use && !pushing) continue;
        if (e.to < 0) { leaveCastle(); return; }
        H.place = e.to; H.pos = e.arrive; H.face = e.face; H.shown = 0; H.moving = false;
        showBanner(place(e.to).name, e.to == CORRIDOR ? "" : TextFormat("Press %s to %s", keyName(K.interact), place(e.to).keeperLabel), 1.8f);
        play("lid");
        return;
    }
}

// ------------------------------------------------------------------ drawing
// Whoever keeps the shop, behind their counter (the counter is drawn again over them).
void Game::drawKeeper(int p) {
    switch (p) {
    case MARIN:
        if (marinTex.id) {
            float w = 150, h = w * marinTex.height / marinTex.width, bob = sinf(clock * 2.2f) * 2;
            DrawTexturePro(marinTex, { 0, 0, (float)marinTex.width, (float)marinTex.height }, { 1150 - w / 2, 372 - h + bob, w, h }, { 0, 0 }, 0, WHITE);
        }
        break;
    case GALIN:
        if (galinTex.id) {
            float s = 196, bob = sinf(clock * 1.7f) * 1.5f;
            ui::drawContain(galinTex, { 0, 0, (float)galinTex.width, (float)galinTex.height }, { 1074 - s / 2, 232 + bob, s, s });
        }
        break;
    case CASINO: {
        const Anim& ta = SPR.enemy[EK_TOSHKO].idle;
        if (ta.ok()) drawAnim(ta, animFrame(ta, clock, 6, true), { 1150, 312 }, 3.4f, hall.pos.x < 1150);
        break;
    }
    default: break;
    }
    const Rectangle& c = place(p).counter;
    if (c.width > 0 && castleArt[p].id) DrawTexturePro(castleArt[p], c, c, { 0, 0 }, 0, WHITE);
}

void Game::drawCastle() {
    const Hall& H = hall;
    const PlaceDef& P = place(H.place);
    ClearBackground(BLACK);
    BeginMode2D(castleCamera());
    Texture2D art = castleArt[H.place];
    if (art.id) DrawTexture(art, 0, 0, WHITE);
    else for (const Rectangle& f : P.floors) DrawRectangleRec(f, { 70, 64, 60, 255 });
    drawKeeper(H.place);

    // you, in whatever Beray sold you
    const WalkSheet& w = figure(player.champion);
    float fw = 24 * 1.4f * P.figure, fh = 32 * 1.4f * P.figure;
    drawShadow({ H.pos.x, H.pos.y - 6 * P.figure / 2 }, 11 * P.figure);
    if (w.ok()) w.draw(facingOf(H.face), w.frameAt(H.moving, clock), { H.pos.x - fw / 2, H.pos.y - fh, fw, fh });
    else drawHero({ H.pos.x, H.pos.y - fh / 2 }, H.face, 0, false, champ().cloak);

    if (H.place == DRIP) {                                  // a glint off Beray's chain now and then
        float k = fmodf(clock, 2.6f);
        if (k < 0.5f) drawStar({ 432, 448 }, 6 * sinf(k / 0.5f * PI), clock * 3, { 255, 250, 210, 255 });
    }
    EndMode2D();
}

// A "[E] talk" prompt over a spot in the place.
static void castlePrompt(const Camera2D& cam, Vector2 world, const char* key, const char* label) {
    Vector2 s = GetWorldToScreen2D(world, cam);
    float keyW = (float)ui::measure(key, 20) + 16, w = keyW + ui::measure(label, 20);
    DrawRectangleRounded({ s.x - w / 2 - 8, s.y - 4, w + 16, 30 }, 0.3f, 6, Fade(BLACK, 0.6f));
    ui::keycap(key, s.x - w / 2, s.y + 2);
    ui::text(label, s.x - w / 2 + keyW, s.y + 1, 20, ui::TEXT);
}

void Game::drawCastleHud() {
    const Hall& H = hall;
    const PlaceDef& P = place(H.place);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Camera2D cam = castleCamera();
    const char* E = keyName(settings.keys.interact);

    // what's near: the shopkeeper first, then a door
    if (overlay == Overlay::None) {
        if (P.keeperLabel && Vector2Distance(H.pos, P.keeper) < P.keeperR)
            castlePrompt(cam, { H.pos.x, H.pos.y + 10 }, E, P.keeperLabel);
        else
            for (const Exit& e : P.exits)
                if (Vector2Distance(H.pos, e.at) < e.r && (H.place != DRIP || H.pos.y > 700)) {
                    castlePrompt(cam, { H.pos.x, H.pos.y + 10 }, E, e.label);
                    break;
                }
    }

    // you: champion, health, gold and packs
    Rectangle box = { 16, 16, 300, 76 };
    ui::panel(box);
    Rectangle pic = { box.x + 10, box.y + 10, 56, 56 };
    DrawRectangleRec(pic, { 12, 12, 20, 255 });
    drawFigureIn(figure(player.champion), FACE_DOWN, 0, { pic.x + 6, pic.y + 2, pic.width - 12, pic.height - 4 });
    DrawRectangleLinesEx(pic, 2, ui::ACCENT);
    drawName(player.champion, box.x + 76, box.y + 10, 10, ui::ACCENT);
    ui::bar({ box.x + 76, box.y + 26, box.width - 90, 14 }, player.hp / player.maxHp, ui::HP,
        TextFormat("%d / %d", (int)ceilf(player.hp), (int)player.maxHp));
    drawAnim(SPR.coin, animFrame(SPR.coin, clock, 8, true), { box.x + 86, box.y + 58 }, 1.2f);
    ui::text(TextFormat("%d", player.gold), box.x + 98, box.y + 50, 20, ui::ACCENT);
    if (packTex.id) ui::drawContain(packTex, { 0, 0, (float)packTex.width, (float)packTex.height }, { box.x + 180, box.y + 46, 22, 22 });
    ui::text(TextFormat("x%d", player.packs), box.x + 206, box.y + 50, 20, ui::ACCENT);

    // where you are
    float a = fminf(1, H.shown * 2);
    ui::textRight(P.name, sw - 24.0f, 24, 30, Fade(ui::ACCENT, a));
    // the caption along the bottom, as on a painted sign
    int cw = ui::measure(P.caption, 20);
    float cs = cw > sw - 40 ? (sw - 40.0f) / cw : 1.0f;
    DrawRectangle(0, sh - 38, sw, 38, Fade(BLACK, 0.75f));
    if (cs < 1) ui::textCentered(P.caption, sw / 2.0f, sh - 28.0f, 10, ui::TEXT);
    else ui::textCentered(P.caption, sw / 2.0f, sh - 32.0f, 20, ui::TEXT);

    // the banner
    if (banner > 0 && overlay == Overlay::None) {
        float b = fminf(1.0f, banner), by = sh * 0.22f;
        DrawRectangleGradientH(0, (int)by - 16, sw / 2, bannerSub.empty() ? 72 : 96, Fade(BLACK, 0), Fade(BLACK, 0.5f * b));
        DrawRectangleGradientH(sw / 2, (int)by - 16, sw / 2, bannerSub.empty() ? 72 : 96, Fade(BLACK, 0.5f * b), Fade(BLACK, 0));
        ui::textCentered(bannerText.c_str(), sw / 2.0f, by, 40, Fade(ui::ACCENT, b));
        if (!bannerSub.empty()) ui::textCentered(bannerSub.c_str(), sw / 2.0f, by + 50, 20, Fade(ui::TEXT, b));
    }
    if (!touch::enabled() && overlay == Overlay::None)
        ui::textCentered(TextFormat("WASD walk   %s doors and shopkeepers   Esc pause", E), sw / 2.0f, sh - 52.0f, 10, Fade(ui::MUTED, 0.9f));
}

// ------------------------------------------------------------------ Beray's outfits
const WalkSheet& Game::figure(int champion) const {
    int k = profile.outfit[champion];
    if (k > 0 && k < OUTFIT_COUNT && dressed[champion].ok()) return dressed[champion];
    return walk[champion];
}

const WalkSheet& Game::outfitLook(int champion, int outfit) {
    if (outfit <= 0 || outfit >= OUTFIT_COUNT) return walk[champion];
    WalkSheet& w = outfitLooks[champion * OUTFIT_COUNT + outfit];
    if (!w.ok()) w.load(CHAMPIONS[champion].walk, TextFormat("outfits/%s/", OUTFITS[outfit].key));
    return w.ok() ? w : walk[champion];
}

void Game::wearOutfit(int champion, int outfit) {
    profile.outfit[champion] = (signed char)outfit;
    rlDrawRenderBatchActive();                      // what's already drawn this frame with the old one goes out first
    dressed[champion].unload();
    if (outfit > 0) dressed[champion].load(CHAMPIONS[champion].walk, TextFormat("outfits/%s/", OUTFITS[outfit].key));
    saveProfile();
}

bool Game::ownsOutfit(int champion, int outfit) const {
    return outfit == 0 || ((profile.outfitsOwned[champion] >> outfit) & 1u);
}

void Game::buyOutfit(int champion, int outfit) {
    const OutfitDef& o = OUTFITS[outfit];
    if (ownsOutfit(champion, outfit)) { wearOutfit(champion, outfit); play("ui"); return; }
    if (player.gold < o.price) { shopMsg = "Not enough gold. Beray doesn't do credit."; shopMsgT = 2.0f; play("hurt"); return; }
    player.gold -= o.price;
    profile.outfitsOwned[champion] |= 1u << outfit;
    wearOutfit(champion, outfit);                   // straight on
    shopMsg = TextFormat("%s, fresh in the %s.", CHAMPIONS[champion].name, o.name); shopMsgT = 2.4f;
    play("coin");
}

void Game::drawDrip() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (shopMsgT > 0) shopMsgT -= GetFrameTime();
    ui::veil(0.65f);
    float pw = fminf(1100.0f, sw - 40.0f), ph = fminf(690.0f, sh - 30.0f);
    Rectangle panel = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::panel(panel);
    if (ui::button({ panel.x + pw - 46, panel.y + 12, 32, 32 }, "X", ui::Style::Normal, 20)) { play("ui"); overlay = Overlay::None; return; }

    // Beray on the left
    float lw = pw * 0.29f;
    Rectangle art = { panel.x + 14, panel.y + 14, lw - 14, (lw - 14) * 0.85f };
    if (berayTex.id) ui::drawContain(berayTex, { 0, 0, (float)berayTex.width, (float)berayTex.height }, art);
    DrawRectangleLinesEx(art, 2, { 34, 194, 180, 255 });
    float ly = art.y + art.height + 14;
    ui::text("Beray", panel.x + 18, ly, 30, { 160, 110, 240, 255 });
    ui::textWrapped("Clothes and outfits for every champion. Buy one and they wear it at once; it's theirs for good.",
        panel.x + 18, ly + 40, lw - 24, 10, ui::MUTED);
    drawAnim(SPR.coin, animFrame(SPR.coin, clock, 8, true), { panel.x + 30, ly + 104 }, 2.0f);
    ui::text(TextFormat("%d", player.gold), panel.x + 50, ly + 94, 20, ui::ACCENT);

    // the champions: dress anyone
    float rx = panel.x + lw + 10, rw = pw - lw - 30;
    float tabW = (rw - (CHAMPION_COUNT - 1) * 4) / CHAMPION_COUNT, tabH = tabW * 1.25f, ty = panel.y + 56;
    ui::text("Dressing", rx, panel.y + 22, 20, ui::MUTED);
    for (int i = 0; i < CHAMPION_COUNT; ++i) {
        Rectangle t = { rx + i * (tabW + 4), ty, tabW, tabH };
        bool sel = i == dripChampion, over = ui::hovered(t);
        DrawRectangleRec(t, sel ? Color{ 60, 40, 90, 255 } : over ? ColorBrightness(ui::PANEL_LIGHT, 0.12f) : ui::PANEL_LIGHT);
        drawFigureIn(figure(i), FACE_DOWN, 0, { t.x + 3, t.y + 3, t.width - 6, t.height - 6 });
        DrawRectangleLinesEx(t, sel ? 2.0f : 1.0f, sel ? Color{ 34, 194, 180, 255 } : ui::EDGE);
        ui::track(TextFormat("dress %s", CHAMPIONS[i].name), t);
        if (over) { ui::handCursor(); if (in::mousePressed(MOUSE_BUTTON_LEFT)) { dripChampion = i; dripHover = -1; play("ui"); } }
    }
    int c = dripChampion;
    drawName(c, rx + 98, panel.y + 22, 20, ui::ACCENT);

    // the champion in the outfit you point at (or the one they're wearing), from every side
    int shown = dripHover >= 0 ? dripHover : profile.outfit[c];
    float py = ty + tabH + 14, prevH = fminf(150.0f, ph * 0.22f);
    Rectangle prev = { rx, py, rw, prevH };
    DrawRectangleRec(prev, Fade(ui::INK, 0.6f));
    DrawRectangleLinesEx(prev, 1, ui::EDGE);
    float turnW = (prevH - 16) * 3 / 4 * 4 + 12;
    drawTurnaround(outfitLook(c, shown), { prev.x + 14, prev.y + 8, turnW, prevH - 16 }, clock);
    ui::text(OUTFITS[shown].name, prev.x + turnW + 34, prev.y + 18, 30, ui::TEXT);
    ui::textWrapped(OUTFITS[shown].desc, prev.x + turnW + 34, prev.y + 58, rw - turnW - 50, 20, ui::MUTED);

    // the rail: every outfit, bought per champion
    dripHover = -1;
    float gy = py + prevH + 14, colW = (rw - 12) / 2, rowH = fminf(64.0f, (panel.y + ph - 50 - gy) / 4 - 8);
    for (int k = 0; k < OUTFIT_COUNT; ++k) {
        Rectangle row = { rx + (k % 2) * (colW + 12), gy + (k / 2) * (rowH + 8), colW, rowH };
        bool owned = ownsOutfit(c, k), worn = profile.outfit[c] == k;
        if (ui::hovered(row)) dripHover = k;
        DrawRectangleRec(row, worn ? Color{ 30, 52, 50, 255 } : ui::PANEL_LIGHT);
        DrawRectangleLinesEx(row, 1, Fade(ui::EDGE, 0.8f));
        DrawRectangle((int)row.x + 10, (int)(row.y + rowH / 2 - 14), 14, 28, OUTFITS[k].colors[0]);
        DrawRectangle((int)row.x + 24, (int)(row.y + rowH / 2 - 14), 10, 28, OUTFITS[k].colors[1]);
        DrawRectangleLines((int)row.x + 10, (int)(row.y + rowH / 2 - 14), 24, 28, Fade(BLACK, 0.6f));
        ui::text(OUTFITS[k].name, row.x + 44, row.y + 8, 20, ui::TEXT);
        ui::text(worn ? "Wearing it" : owned ? "In the wardrobe" : "On the rail", row.x + 44, row.y + rowH - 20, 10, worn ? ui::GOOD : ui::MUTED);
        Rectangle btn = { row.x + colW - 104, row.y + 10, 92, rowH - 20 };
        if (worn) continue;
        ui::track(TextFormat("outfit %d", k), btn);
        if (owned) { if (ui::button(btn, "Wear", ui::Style::Good, 20)) buyOutfit(c, k); }
        else if (ui::button(btn, TextFormat("%d g", OUTFITS[k].price), player.gold >= OUTFITS[k].price ? ui::Style::Primary : ui::Style::Normal, 20))
            buyOutfit(c, k);
    }
    if (shopMsgT > 0) ui::textCentered(shopMsg.c_str(), rx + rw / 2, panel.y + ph - 34, 20, Fade(ui::ACCENT, fminf(1.0f, shopMsgT)));
}
