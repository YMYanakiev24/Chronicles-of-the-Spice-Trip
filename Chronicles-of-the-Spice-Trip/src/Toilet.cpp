// ============================================================================
//  Toilet.cpp — hiding from The Sergeant in the toilets (see Toilet.h).
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Touch.h"
#include "Render.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>

using namespace toilet;

namespace toilet {

    // Where feet can go, read off the map: three rooms of floor, the passage between the top
    // and middle rooms, two sets of double doors and the corridor outside.
    static const std::vector<Rectangle> FLOORS = {
        { 40, 196, 375, 60 },       // top room, in front of the urinals
        { 40, 238, 1322, 18 },      // top room, along the stalls
        { 810, 196, 552, 60 },      // top room, by the sinks
        { 1286, 250, 50, 225 },     // the passage down on the right
        { 775, 470, 587, 78 },      // middle room, by the sinks
        { 40, 537, 750, 19 },       // in front of the middle row of stalls
        { 655, 548, 100, 160 },     // through the left doors
        { 1210, 540, 102, 168 },    // through the right doors
        { 10, 700, 1388, 60 },      // the corridor outside
    };

    // The stalls with doors that shut (the two standing open are no use for hiding).
    static const std::vector<Stall> STALLS = {
        { { 509, 139, 54, 91 } }, { { 584, 139, 55, 91 } }, { { 659, 139, 55, 91 } }, { { 736, 139, 57, 91 } },
        { { 44, 443, 55, 91 } }, { { 121, 443, 54, 91 } }, { { 196, 443, 50, 91 } }, { { 275, 443, 50, 91 } },
        { { 354, 443, 50, 91 } }, { { 509, 443, 53, 91 } }, { { 594, 443, 52, 91 } }, { { 679, 443, 56, 91 } },
    };

    // The Sergeant's rounds.
    static const Vector2 WAYPOINTS[] = {
        { 120, 225 }, { 600, 247 }, { 1100, 225 }, { 1310, 360 }, { 1100, 510 }, { 850, 510 }, { 150, 546 },
        { 450, 546 }, { 705, 640 }, { 200, 730 }, { 700, 730 }, { 1250, 730 }, { 1260, 620 },
    };
    static constexpr int WAYPOINT_COUNT = (int)(sizeof WAYPOINTS / sizeof WAYPOINTS[0]);

    static const Vector2 YOU_START = { 120, 735 }, HER_START = { 1150, 225 };

    const std::vector<Rectangle>& floors() { return FLOORS; }
    const std::vector<Stall>& stalls() { return STALLS; }
    Vector2 stallFront(const Stall& s) { return { s.door.x + s.door.width / 2, s.door.y + s.door.height + 14 }; }
    Vector2 stallInside(const Stall& s) { return { s.door.x + s.door.width / 2, s.door.y + s.door.height - 10 }; }

    bool walkable(Vector2 p) {
        for (const Rectangle& r : FLOORS) if (CheckCollisionPointRec(p, r)) return true;
        return false;
    }

    static int cellOf(Vector2 p) {
        int x = (int)Clamp(p.x / CELL, 0, GW - 1), y = (int)Clamp(p.y / CELL, 0, GH - 1);
        return y * GW + x;
    }
    static Vector2 cellCentre(int c) { return { (c % GW) * (float)CELL + CELL / 2.0f, (c / GW) * (float)CELL + CELL / 2.0f }; }
    static bool cellOpen(int c) { return walkable(cellCentre(c)); }

    // Nothing but floor between the two points (walls block her view).
    static bool clearLine(Vector2 a, Vector2 b) {
        float d = Vector2Distance(a, b);
        for (float s = 0; s <= d; s += 6)
            if (!walkable(Vector2Lerp(a, b, d > 0 ? s / d : 0))) return false;
        return true;
    }

    // Steps from every cell to `to` (breadth first over the floor cells), for her to walk down.
    static void pathField(Level& L, int to) {
        if (L.pathTo == to && !L.dist.empty()) return;
        L.pathTo = to;
        L.dist.assign(GW * GH, -1);
        if (!cellOpen(to)) return;
        std::vector<int> q; q.reserve(GW * GH);
        q.push_back(to); L.dist[to] = 0;
        for (size_t i = 0; i < q.size(); ++i) {
            int c = q[i], cx = c % GW, cy = c / GW;
            for (int k = 0; k < 8; ++k) {
                static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
                int nx = cx + DX[k], ny = cy + DY[k];
                if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) continue;
                int n = ny * GW + nx;
                if (L.dist[n] >= 0 || !cellOpen(n)) continue;
                if (k >= 4 && (!cellOpen(cy * GW + nx) || !cellOpen(ny * GW + cx))) continue;   // no cutting corners
                L.dist[n] = L.dist[c] + 1;
                q.push_back(n);
            }
        }
    }

    // One step of hers toward `target`; true once she's there.
    static bool walkTo(Level& L, Vector2 target, float speed, float dt) {
        Vector2 to = Vector2Subtract(target, L.sPos);
        if (Vector2Length(to) < 6) { L.sMoving = false; return true; }
        pathField(L, cellOf(target));
        int here = cellOf(L.sPos);
        Vector2 aim = target;
        if (!clearLine(L.sPos, target) && here != L.pathTo && L.dist[here] > 0) {
            // down the path field; of the equally good steps, the one that heads most toward the target
            int best = here, cx = here % GW, cy = here / GW;
            float bestD = 1e9f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) continue;
                    int n = ny * GW + nx;
                    if (L.dist[n] < 0) continue;
                    float d = Vector2Distance(cellCentre(n), target);
                    if (L.dist[n] < L.dist[best] || (L.dist[n] == L.dist[best] && best != here && d < bestD)) { best = n; bestD = d; }
                }
            aim = cellCentre(best);
        }
        Vector2 d = Vector2Subtract(aim, L.sPos);
        float len = Vector2Length(d);
        if (len > 0.01f) {
            Vector2 step = Vector2Scale(d, fminf(speed * dt, len) / len);
            L.sPos = Vector2Add(L.sPos, step);
            L.sFace = Vector2Normalize(d);
        }
        L.sMoving = true;
        return false;
    }

    static int nearestStall(Vector2 p, float within) {
        int best = -1; float bd = within;
        for (int i = 0; i < (int)STALLS.size(); ++i) {
            float d = Vector2Distance(p, stallFront(STALLS[i]));
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }

} // namespace toilet

// ---------------------------------------------------------------------- the level
void Game::startToilet() {
    hunt = Level{};
    hunt.active = true;
    hunt.pos = YOU_START; hunt.sPos = HER_START;
    hunt.waypoint = 2;
    hunt.target = WAYPOINTS[hunt.waypoint];
    hunt.lastSeen = hunt.pos;
    enemies.clear(); enemyShots.clear(); shots.clear(); coins.clear(); parts.clear();
    music.setMood("boss4");
    showBanner("The Toilets", "Hide from The Sergeant for a minute. She can smell your smoke.", 3.2f);
    play("whistle");
}

static void resetHunt(Level& h) {
    int tries = h.tries + 1;
    h = Level{};
    h.active = true; h.tries = tries;
    h.pos = YOU_START; h.sPos = HER_START;
    h.waypoint = 2; h.target = WAYPOINTS[2]; h.lastSeen = h.pos;
}

Camera2D Game::toiletCamera() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Camera2D cam{};
    cam.offset = { sw / 2.0f, sh / 2.0f };
    cam.target = { MAP_W / 2, MAP_H / 2 };
    cam.zoom = fminf(sw / (MAP_W * 1.02f), sh / (MAP_H * 1.04f));
    return cam;
}

void Game::updateToilet(float dt) {
    Level& L = hunt;
    Keybinds& K = settings.keys;
    if (banner > 0) banner -= dt;

    // caught, or made it: a moment, then go again or go on
    if (L.caught || L.won) {
        L.endT -= dt;
        for (auto& p : L.smoke) { p.t += dt; p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt)); }
        if (L.endT > 0) return;
        if (L.caught) { resetHunt(L); showBanner("Hide again", "A whole minute this time", 1.8f); return; }
        L.active = false;
        descend();
        return;
    }

    L.timeLeft -= dt;
    L.peeT = fmaxf(0, L.peeT - dt);
    L.peeCd = fmaxf(0, L.peeCd - dt);
    L.checkCd = fmaxf(0, L.checkCd - dt);

    // --- you: sneak about, duck into stalls, pee ---
    Vector2 mv{ 0, 0 };
    if (in::down(K.up)) mv.y -= 1;
    if (in::down(K.down)) mv.y += 1;
    if (in::down(K.left)) mv.x -= 1;
    if (in::down(K.right)) mv.x += 1;
    if (mv.x || mv.y) mv = Vector2Normalize(mv);
    bool use = in::pressed(K.interact);
    L.moving = false;
    if (L.hiddenIn >= 0) {                                   // in a stall: down (or E) to come out
        if ((mv.y > 0.5f || use) && L.peeT <= 0) {
            L.pos = stallFront(STALLS[L.hiddenIn]); L.hiddenIn = -1; L.face = { 0, 1 };
            play("lid");
        }
    }
    else {
        int at = -1;                                         // standing at a stall's door?
        for (int i = 0; i < (int)STALLS.size(); ++i) {
            Vector2 f = stallFront(STALLS[i]);
            if (fabsf(L.pos.x - f.x) < STALLS[i].door.width / 2 && fabsf(L.pos.y - f.y) < 14) at = i;
        }
        if (at >= 0 && (mv.y < -0.5f || use) && L.peeT <= 0) {
            L.hiddenIn = at; L.pos = stallInside(STALLS[at]); L.face = { 0, 1 };
            play("lid");
        }
        else if (L.peeT <= 0 && (mv.x || mv.y)) {            // no walking while you pee
            float speed = (in::down(K.run) ? RUN : WALK) * champ().speedMul;
            Vector2 d = Vector2Scale(mv, speed * dt);
            if (walkable({ L.pos.x + d.x, L.pos.y })) L.pos.x += d.x;
            if (walkable({ L.pos.x, L.pos.y + d.y })) L.pos.y += d.y;
            L.face = mv; L.moving = true;
        }
    }
    if (in::pressed(K.special) && L.peeT <= 0 && L.peeCd <= 0) {
        L.peeT = PEE_TIME; L.peeCd = PEE_TIME + PEE_COOLDOWN;
        Vector2 at = L.hiddenIn >= 0 ? stallFront(STALLS[L.hiddenIn]) : Vector2Add(L.pos, Vector2Scale(L.face, 12));
        L.puddles.push_back({ at, 2, 0 });
        play("chill");
    }
    if (L.peeT > 0 && !L.puddles.empty())                     // the puddle spreads while you go
        L.puddles.back().r = fminf(PUDDLE_R, L.puddles.back().r + PUDDLE_R / PEE_TIME * dt);

    // --- your smoke: always, except while you pee ---
    if (L.peeT <= 0 && (L.smokeT -= dt) <= 0) {
        L.smokeT = SMOKE_EVERY;
        Puff p;
        if (L.hiddenIn >= 0) {
            const Rectangle& d = STALLS[L.hiddenIn].door;
            p.pos = { d.x + d.width / 2 + frand(-8, 8), d.y - 2 };
            p.from = L.pos; p.stall = L.hiddenIn;
        }
        else {
            p.pos = { L.pos.x + L.face.x * 7, L.pos.y - MOUTH_Y };
            p.from = L.pos; p.stall = -1;
        }
        p.vel = { frand(-10, 10) + 6, frand(-30, -18) };
        p.t = 0;
        L.smoke.push_back(p);
    }
    for (auto& p : L.smoke) { p.t += dt; p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt)); p.vel.x += sinf(p.t * 3 + p.from.x) * 4 * dt; }
    std::erase_if(L.smoke, [](const Puff& p) { return p.t > SMOKE_LIFE; });
    for (auto& d : L.puddles) d.slipCd = fmaxf(0, d.slipCd - dt);

    // --- The Sergeant ---
    L.stateT += dt;
    bool sees = L.hiddenIn < 0 && Vector2Distance(L.sPos, L.pos) < SIGHT_R && clearLine(L.sPos, L.pos);
    if (L.state == Hunt::Slip) {
        L.sMoving = false;
        if ((L.slipT -= dt) <= 0) { L.state = L.before == Hunt::Slip ? Hunt::Patrol : L.before; L.stateT = 0; }
    }
    else {
        if (sees) {
            if (L.state != Hunt::Chase) { play("whistle"); L.stateT = 0; }
            L.state = Hunt::Chase; L.lastSeen = L.pos; L.sinceSeen = 0;
        }
        else if (L.state != Hunt::Chase && L.state != Hunt::Check) {   // she follows her nose to the newest smoke she can smell
            const Puff* best = nullptr;
            for (auto& p : L.smoke)
                if (Vector2Distance(p.pos, L.sPos) < SMELL_R && (!best || p.t < best->t)) best = &p;
            if (best) {
                if (L.state != Hunt::Sniff) L.stateT = 0;
                L.state = Hunt::Sniff;
                if (best->stall >= 0) { L.checkStall = best->stall; L.target = stallFront(STALLS[best->stall]); }
                else { L.checkStall = -1; L.target = best->from; }
            }
        }
        // every slip has its puddle
        for (auto& d : L.puddles)
            if (d.slipCd <= 0 && d.r > 6 && Vector2Distance(d.pos, L.sPos) < d.r && L.sMoving) {
                d.slipCd = 5;
                L.before = L.state; L.state = Hunt::Slip; L.slipT = SLIP_TIME; L.stateT = 0;
                play("bonk"); shake = fmaxf(shake, 0.2f);
                showBanner("SLIP!", "The Sergeant went down in your puddle", 1.4f);
                break;
            }
    }
    switch (L.state) {
    case Hunt::Chase:
        if (!sees) L.sinceSeen += dt;
        if (walkTo(L, sees ? L.pos : L.lastSeen, SPEED_CHASE, dt) && !sees) {
            // lost you where she last saw you: you must be in one of these stalls
            int s = nearestStall(L.lastSeen, 120);
            if (s >= 0) { L.state = Hunt::Check; L.checkStall = s; L.stateT = 0; }
            else { L.state = Hunt::Patrol; L.stateT = 0; }
        }
        break;
    case Hunt::Sniff:
        if (walkTo(L, L.target, SPEED_SNIFF, dt)) {
            if (L.checkStall >= 0) { L.state = Hunt::Check; L.stateT = 0; }
            else if (L.stateT > 1.0f) { L.state = Hunt::Patrol; L.stateT = 0; }   // sniffs about, then moves on
        }
        break;
    case Hunt::Check:
        if (L.checkStall < 0) { L.state = Hunt::Patrol; break; }
        if (!walkTo(L, stallFront(STALLS[L.checkStall]), SPEED_SNIFF, dt)) { L.stateT = 0; break; }
        L.sFace = { 0, -1 };
        if (L.stateT > 0.9f) {                              // she kicks the door in
            play("kick");
            if (L.hiddenIn == L.checkStall) {
                L.caught = true; L.endT = 2.4f;
                showBanner("The Sergeant found you!", "Try again", 2.2f);
                play("hurt"); hapticBuzz(200);
            }
            L.checkCd = 4; L.state = Hunt::Patrol; L.stateT = 0;
            L.checkStall = -1;
        }
        break;
    case Hunt::Patrol:
        if (walkTo(L, WAYPOINTS[L.waypoint], SPEED_PATROL, dt)) {
            int next = GetRandomValue(0, WAYPOINT_COUNT - 2);
            L.waypoint = next >= L.waypoint ? next + 1 : next;
        }
        if (L.checkCd <= 0) {                               // now and then she tries a door on her way past
            int s = nearestStall(L.sPos, 30);
            if (s >= 0) {
                L.checkCd = 6;
                if (GetRandomValue(1, 100) <= 30) { L.state = Hunt::Check; L.checkStall = s; L.stateT = 0; }
            }
        }
        break;
    case Hunt::Slip: break;
    }

    // --- caught in the open? ---
    if (!L.caught && L.state != Hunt::Slip && L.hiddenIn < 0 && Vector2Distance(L.sPos, L.pos) < CATCH_R) {
        L.caught = true; L.endT = 2.4f;
        showBanner("The Sergeant found you!", "Try again", 2.2f);
        play("hurt"); hapticBuzz(200);
    }

    // --- a whole minute: you made it ---
    if (!L.caught && L.timeLeft <= 0) {
        L.timeLeft = 0; L.won = true; L.endT = 2.8f;
        player.gold += 60; stats.goldEarned += 60; player.packs++;
        showBanner("You made it!", "The Sergeant never found you  +60 gold and a cigarette pack", 2.6f);
        play("victory");
    }
}

// ---------------------------------------------------------------------- drawing
static void drawFigure(const WalkSheet& w, Vector2 feet, Vector2 look, bool walking, float time, Color tint = WHITE) {
    float fw = 24 * 1.4f * FIGURE_SCALE, fh = FIGURE_H;
    drawShadow({ feet.x, feet.y - 6 }, 15);
    if (w.ok()) w.draw(facingOf(look), w.frameAt(walking, time), { feet.x - fw / 2, feet.y - fh, fw, fh }, tint);
    else drawHero({ feet.x, feet.y - 14 }, look, 0, false, { 90, 110, 70, 255 });
}

void Game::drawToilet() {
    Level& L = hunt;
    ClearBackground(BLACK);
    BeginMode2D(toiletCamera());
    if (toiletMap.id) DrawTexturePro(toiletMap, { 0, 0, (float)toiletMap.width, (float)toiletMap.height }, { 0, 0, MAP_W, MAP_H }, { 0, 0 }, 0, WHITE);
    else for (const Rectangle& r : FLOORS) DrawRectangleRec(r, { 190, 220, 230, 255 });

    for (auto& d : L.puddles) {                             // puddles
        DrawEllipse((int)d.pos.x, (int)d.pos.y, d.r * 1.3f, d.r * 0.55f, Fade({ 236, 214, 70, 255 }, 0.55f));
        DrawEllipse((int)(d.pos.x - d.r * 0.3f), (int)(d.pos.y - d.r * 0.12f), d.r * 0.4f, d.r * 0.14f, Fade(WHITE, 0.5f));
    }

    // you and her, nearer the bottom in front
    auto drawYou = [&]() {
        drawFigure(walk[player.champion], L.pos, L.face, L.moving, clock);
        if (L.hiddenIn < 0 && L.peeT > 0) {                  // the stream
            Vector2 a = { L.pos.x + L.face.x * 6, L.pos.y - FIGURE_H * 0.36f }, b = Vector2Add(L.pos, Vector2Scale(L.face, 14));
            for (int k = 0; k < 10; ++k) {
                float u = k / 9.0f;
                Vector2 p = Vector2Lerp(a, b, u);
                p.y -= sinf(u * PI) * 6 + sinf(clock * 40 + k) * 0.8f;
                DrawCircleV(p, 1.6f, { 240, 214, 60, 255 });
            }
        }
        else if (L.hiddenIn < 0) DrawCircleV({ L.pos.x + L.face.x * 10, L.pos.y - MOUTH_Y + 1 }, 2.0f, { 255, 120, 40, 255 });   // the cigarette's tip
    };
    auto drawHer = [&]() {
        if (L.state == Hunt::Slip) {                         // flat on her back
            if (sergeantWalk.ok()) {
                float fw = 24 * 1.4f * FIGURE_SCALE, fh = 32 * 1.4f * FIGURE_SCALE;
                Rectangle src = { 0, 0, (float)sergeantWalk.fw, (float)sergeantWalk.fh };
                drawShadow({ L.sPos.x, L.sPos.y - 4 }, 24);
                DrawTexturePro(sergeantWalk.tex, src, { L.sPos.x, L.sPos.y - 6, fw, fh }, { fw / 2, fh * 0.55f }, 82, WHITE);
            }
            for (int k = 0; k < 3; ++k)
                drawStar({ L.sPos.x + cosf(clock * 5 + k * 2.1f) * 16 - 18, L.sPos.y - 24 + sinf(clock * 5 + k * 2.1f) * 5 }, 4, clock * 4, { 255, 230, 90, 255 });
            return;
        }
        drawFigure(sergeantWalk, L.sPos, L.sFace, L.sMoving, clock * (L.state == Hunt::Chase ? 1.4f : 1.0f));
    };
    if (L.hiddenIn >= 0) {                                  // behind the door of your stall
        drawYou();
        const Rectangle& d = STALLS[L.hiddenIn].door;
        if (toiletMap.id) DrawTexturePro(toiletMap, d, d, { 0, 0 }, 0, WHITE);
    }
    if (L.state == Hunt::Check && L.checkStall >= 0 && Vector2Distance(L.sPos, stallFront(STALLS[L.checkStall])) < 8 && toiletMap.id) {
        Rectangle d = STALLS[L.checkStall].door, to = d;     // the door rattling as she tries it
        to.x += sinf(clock * 60) * 2;
        DrawTexturePro(toiletMap, d, to, { 0, 0 }, 0, WHITE);
    }
    if (L.hiddenIn >= 0) drawHer();
    else if (L.pos.y < L.sPos.y) { drawYou(); drawHer(); }
    else { drawHer(); drawYou(); }

    for (auto& p : L.smoke) {                               // your smoke
        float k = p.t / SMOKE_LIFE;
        DrawCircleV(p.pos, 4 + 12 * k, Fade({ 150, 150, 162, 255 }, 0.5f * (1 - k)));
        DrawCircleV({ p.pos.x - 1, p.pos.y - 1 }, 3 + 10 * k, Fade({ 226, 226, 234, 255 }, 0.6f * (1 - k)));
    }
    EndMode2D();
}

void Game::drawToiletHud() {
    Level& L = hunt;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Camera2D cam = toiletCamera();

    // what she's up to, over her head
    const char* mark = L.state == Hunt::Chase ? "!" : L.state == Hunt::Sniff ? "?" : L.state == Hunt::Check ? "..." : nullptr;
    if (mark && !L.caught && !L.won) {
        Vector2 s = GetWorldToScreen2D({ L.sPos.x, L.sPos.y - FIGURE_H - 26 }, cam);
        ui::textCentered(mark, s.x, s.y + sinf(clock * 8) * 2, 20, L.state == Hunt::Chase ? ui::BAD : ui::ACCENT);
    }

    // the clock
    float k = L.timeLeft / HIDE_TIME;
    ui::textCentered(TextFormat("HIDE  %d:%02d", (int)ceilf(L.timeLeft) / 60, (int)ceilf(L.timeLeft) % 60), sw / 2.0f, 16, 40,
        L.timeLeft < 10 ? ui::GOOD : ui::ACCENT);
    ui::bar({ sw / 2.0f - 180, 64, 360, 8 }, k, ui::ACCENT);
    const char* what = L.won ? "She never found you" : L.caught ? "Caught!" :
        L.state == Hunt::Chase ? "SHE SEES YOU - RUN!" : L.state == Hunt::Sniff ? "She smells your smoke..." :
        L.state == Hunt::Check ? "She's trying a stall door..." : L.state == Hunt::Slip ? "She slipped in your pee!" :
        "The Sergeant is on patrol";
    ui::textCentered(what, sw / 2.0f, 80, 20, L.state == Hunt::Chase ? ui::BAD : ui::TEXT);

    // who's after you
    Rectangle box = { 16, 16, 230, 74 };
    ui::panel(box);
    Rectangle pic = { box.x + 10, box.y + 10, 54, 54 };
    DrawRectangleRec(pic, { 12, 12, 20, 255 });
    if (sergeantTex.id)
        ui::drawContain(sergeantTex, { 95, 50, 170, 170 }, pic);
    DrawRectangleLinesEx(pic, 2, ui::BAD);
    ui::text("The Sergeant", box.x + 74, box.y + 14, 20, ui::BAD);
    ui::text(L.tries ? TextFormat("Tries  %d", L.tries + 1) : "Don't let her find you", box.x + 74, box.y + 44, 10, ui::MUTED);

    // your pee and your stall, up in the top right over the wall (the floor stays clear)
    float slot = 56, x = sw - 2 * slot - 40.0f, y = 18.0f;
    if (touch::enabled()) { Vector2 at = touch::controlPos(TC_BAR); slot = touch::controlSize(TC_BAR); x = at.x - slot - 8; y = at.y - slot / 2; }
    Rectangle q = { x, y, slot, slot };
    DrawRectangleRec(q, { 30, 26, 14, 235 });
    touch::zone(q, settings.keys.special);
    DrawCircleV({ q.x + slot / 2, q.y + slot / 2 + 6 }, slot * 0.28f, { 236, 214, 70, 255 });
    DrawCircleV({ q.x + slot / 2 - 5, q.y + slot / 2 + 2 }, slot * 0.08f, Fade(WHITE, 0.7f));
    if (L.peeCd > 0 && L.peeT <= 0) DrawRectangle((int)q.x, (int)q.y, (int)slot, (int)(slot * L.peeCd / PEE_COOLDOWN), { 0, 0, 0, 170 });
    DrawRectangleLinesEx(q, 2, L.peeT > 0 ? ui::GOOD : L.peeCd <= 0 ? ui::ACCENT : ui::EDGE);
    ui::keycap(keyName(settings.keys.special), q.x + 3, q.y + 3);
    ui::textCentered(L.peeT > 0 ? TextFormat("Peeing %.1f", L.peeT) : "Pee", q.x + slot / 2, q.y + slot + 6, 10, L.peeT > 0 ? ui::GOOD : ui::MUTED);

    Rectangle e = { x + slot + 16, y, slot, slot };
    DrawRectangleRec(e, { 16, 14, 24, 230 });
    touch::zone(e, settings.keys.interact);
    DrawRectangle((int)(e.x + slot * 0.3f), (int)(e.y + slot * 0.18f), (int)(slot * 0.4f), (int)(slot * 0.64f), { 170, 150, 110, 255 });
    DrawRectangle((int)(e.x + slot * 0.6f), (int)(e.y + slot * 0.5f), 3, 3, { 60, 50, 40, 255 });
    DrawRectangleLinesEx(e, 2, L.hiddenIn >= 0 ? ui::GOOD : ui::EDGE);
    ui::keycap(keyName(settings.keys.interact), e.x + 3, e.y + 3);
    ui::textCentered(L.hiddenIn >= 0 ? "Leave stall" : "Hide in stall", e.x + slot / 2, e.y + slot + 6, 10, L.hiddenIn >= 0 ? ui::GOOD : ui::MUTED);

    if (!touch::enabled())
        ui::textCentered(TextFormat("WASD sneak   Shift run   %s or W at a stall door: hide   %s pee: no smoke for 3 s, and a puddle she slips on",
            keyName(settings.keys.interact), keyName(settings.keys.special)), sw / 2.0f, 106, 10, Fade(ui::TEXT, 0.85f));

    // the banner
    if (banner > 0) {
        float a = fminf(1.0f, banner), by = sh * 0.3f;
        DrawRectangleGradientH(0, (int)by - 16, sw / 2, bannerSub.empty() ? 72 : 96, Fade(BLACK, 0), Fade(BLACK, 0.55f * a));
        DrawRectangleGradientH(sw / 2, (int)by - 16, sw / 2, bannerSub.empty() ? 72 : 96, Fade(BLACK, 0.55f * a), Fade(BLACK, 0));
        ui::textCentered(bannerText.c_str(), sw / 2.0f, by, 40, Fade(ui::ACCENT, a));
        if (!bannerSub.empty()) ui::textCentered(bannerSub.c_str(), sw / 2.0f, by + 50, 20, Fade(ui::TEXT, a));
    }
}
