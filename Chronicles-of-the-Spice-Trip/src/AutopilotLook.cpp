// ============================================================================
//  AutopilotLook.cpp — a gallery of screenshots for checking how the game looks:
//  the start room, traps firing and resting, a fight, a boss, its stairwell, the
//  castle and the toilets. SPICE_LOOK=1 with --autopilot <folder>; optionally
//  SPICE_LOOK_PIXELS=0|1|2, SPICE_LOOK_PLAIN=1 (no lighting), SPICE_LOOK_CHAMP=<n>.
// ============================================================================
#include "Autopilot.h"
#include "Game.h"
#include "Input.h"
#include "Look.h"
#include <cstdlib>
#include <cmath>

void Autopilot::buildLook() {
    auto add = [&](const char* name, float timeout, std::function<bool(Game&, float)> run) {
        steps.push_back({ name, timeout, std::move(run) });
    };
    auto go = [](Game& g, auto pred, bool cleared) {
        for (int y = 0; y < DUNGEON_G; ++y)
            for (int x = 0; x < DUNGEON_G; ++x) {
                Room& r = g.dungeon.grid[y][x];
                if (!r.exists || !pred(r)) continue;
                g.dungeon.cx = x; g.dungeon.cy = y;
                if (cleared) r.cleared = true;
                g.player.pos = ROOM_CENTER;
                g.enterRoom();
                return true;
            }
        return false;
    };
    add("look begin", 5, [](Game& g, float t) {
        if (t < 0.3f) return false;
        if (const char* px = getenv("SPICE_LOOK_PIXELS")) g.settings.pixels = atoi(px);
        if (getenv("SPICE_LOOK_PLAIN")) g.settings.darkLight = false;
        if (const char* ch = getenv("SPICE_LOOK_CHAMP")) g.profile.champion = atoi(ch);
        look::configure(g.settings.pixels, g.settings.darkLight);
        g.startRun(); g.screen = Screen::Play;
        return true;
    });
    add("look start", 4, [this](Game& g, float t) {
        g.player.pos = { CASTLE_DOOR_X + 60, 150 };
        if (t < 1.5f) return false;
        shot(g, "start-room");
        return true;
    });
    add("look traps", 4, [this, go](Game& g, float t) {
        if (t == 0) {
            keepAlive = true;
            go(g, [](const Room& r) { return !r.special(); }, true);
            g.enemies.clear();
            Room& r = g.dungeon.cur();
            r.cleared = true;
            for (int y = 1; y < ROOM_TH - 1; ++y) for (int x = 1; x < ROOM_TW - 1; ++x) r.tiles[y][x] = RT_FLOOR;
            for (int y = 5; y <= 7; ++y) for (int x = 12; x <= 15; ++x) r.tiles[y][x] = RT_SPIKES;
            r.tiles[3][2] = RT_FLAMER; r.tiles[9][9] = RT_FLAMER; r.tiles[2][17] = RT_FLAMER;
            r.flamers[0] = { 2, 3, DIR_E, 0 }; r.flamers[1] = { 9, 9, DIR_N, 0 }; r.flamers[2] = { 17, 2, DIR_S, 0 };
            r.flamerCount = 3;
            g.player.pos = { 6.5f * TILE, 6.5f * TILE };
            return false;
        }
        Room& r = g.dungeon.cur();
        g.player.hp = g.player.maxHp; g.player.invuln = 1;
        if (t < 1.0f) {                                // spikes up, two jets roaring, one sputtering
            g.trapClock = 0.5f;
            r.flamers[0].phase = 2.0f; r.flamers[1].phase = 2.3f; r.flamers[2].phase = 1.3f;
            return false;
        }
        if (!markB) { shot(g, "traps-hot"); markB = true; return false; }
        if (t < 1.6f) {                                // spikes about to rise, jets cold
            g.trapClock = 2.05f;
            r.flamers[0].phase = 0; r.flamers[1].phase = 0; r.flamers[2].phase = 0;
            return false;
        }
        markB = false;
        shot(g, "traps-cold");
        return true;
    });
    add("look combat", 6, [this, go](Game& g, float t) {
        if (t == 0) { go(g, [](const Room& r) { return !r.special() && !r.cleared; }, false); return false; }
        g.player.hp = g.player.maxHp; g.player.invuln = 1;
        if (t < 1.4f) return false;
        shot(g, "combat");
        return true;
    });
    add("look boss", 8, [this, go](Game& g, float t) {
        if (t == 0) { go(g, [](const Room& r) { return r.isBoss; }, false); return false; }
        g.player.hp = g.player.maxHp; g.player.invuln = 1;
        if (t < 3.0f) return false;
        shot(g, "boss-fight");
        return true;
    });
    add("look stairs", 4, [this, go](Game& g, float t) {
        if (t == 0) { go(g, [](const Room& r) { return r.isBoss; }, true); g.enemies.clear(); g.player.pos = { ROOM_CENTER.x - 150, ROOM_CENTER.y + 60 }; return false; }
        g.player.pos = { ROOM_CENTER.x - 150, ROOM_CENTER.y + 60 };
        if (t < 1.5f) return false;
        shot(g, "boss-stairs");
        return true;
    });
    add("look castle", 5, [this, go](Game& g, float t) {
        if (t == 0) { go(g, [](const Room& r) { return r.isStart; }, true); g.enterCastle(); return false; }
        if (t < 1.5f) return false;
        shot(g, "castle");
        return true;
    });
    add("look toilets", 5, [this](Game& g, float t) {
        if (t == 0) { g.leaveCastle(); g.startToilet(); return false; }
        if (t < 1.5f) return false;
        shot(g, "toilets");
        return true;
    });
    add("look end", 2, [](Game&, float) { return true; });
}
