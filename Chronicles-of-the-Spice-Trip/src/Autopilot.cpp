// ============================================================================
//  Autopilot.cpp — a scripted play-through. Menus are clicked by label with
//  the same input path a player uses; fights are sped up by setting health
//  directly once the attack being checked has been seen.
// ============================================================================
#include "Autopilot.h"
#include "Game.h"
#include "Input.h"
#include "Ui.h"
#include <cmath>

namespace {
    // Click the button or card drawn last frame with this label.
    bool clickLabel(const char* label) {
        for (auto& h : ui::hits())
            if (h.label == label) {
                in::moveMouse({ h.rect.x + h.rect.width / 2, h.rect.y + h.rect.height / 2 });
                in::click(MOUSE_BUTTON_LEFT);
                return true;
            }
        return false;
    }

    void aimAt(Game& g, Vector2 world) { in::moveMouse(GetWorldToScreen2D(world, g.camera())); }

    // Walk into the first room that matches, as if through its door.
    template <typename Pred>
    bool goToRoom(Game& g, Pred pred, bool cleared = false) {
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
    }

    bool isCombatRoom(const Room& r) { return !r.isStart && !r.isShop && !r.isBoss; }

    int countShots(const Game& g, ShotKind k) {
        int n = 0;
        for (auto& p : g.enemyShots) n += p.kind == k;
        return n;
    }

    int countForm(const Game& g, int form) {
        int n = 0;
        for (auto& e : g.enemies) n += e.alive && e.bossForm == form;
        return n;
    }

    Enemy* boss(Game& g) {
        for (auto& e : g.enemies) if (e.alive && e.bossForm == 0) return &e;
        return nullptr;
    }

    // Finish every boss body (the Plasmodia multiply on the way).
    void slayBosses(Game& g) {
        for (auto& e : g.enemies) if (e.alive && e.bossForm >= 0) e.hp = 0;
    }

    Enemy dummy(Vector2 at) {
        Enemy e;
        e.name = "Dummy"; e.hp = e.maxHp = 200; e.speed = 0; e.dmg = 0; e.r = 12; e.pos = at;
        e.color = { 200, 200, 200, 255 };
        return e;
    }
}

void Autopilot::start(Game& g, const std::string& d) {
    dir = d;
    log = fopen((dir + "/autopilot.log").c_str(), "w");
    in::setScripted(true);
    build();
}

void Autopilot::check(const char* what, bool ok) {
    (ok ? passes : fails)++;
    if (log) { fprintf(log, "%s  %s\n", ok ? "PASS" : "FAIL", what); fflush(log); }
}

void Autopilot::shot(Game& g, const char* name) {
    std::string file = TextFormat("%s/%02d-%s.png", dir.c_str(), ++shotCount, name);
    g.pendingShot = file;
    if (log) { fprintf(log, "shot  %s\n", GetFileName(file.c_str())); fflush(log); }
}

void Autopilot::finish(Game& g) {
    if (log) {
        if (fails == 0) fprintf(log, "RESULT  all %d checks passed\n", passes);
        else fprintf(log, "RESULT  %d check(s) failed, %d passed\n", fails, passes);
        fclose(log); log = nullptr;
    }
    g.quit = true;
}

void Autopilot::step(Game& g) {
    if (current >= steps.size()) { finish(g); return; }
    if (keepAlive && g.screen == Screen::Play && g.player.hp < g.player.maxHp * 0.5f) g.player.hp = g.player.maxHp;
    Step& s = steps[current];
    bool done = s.run(g, stepT);
    stepT += fminf(GetFrameTime(), 0.05f);
    if (!done && stepT > s.timeout) { check(("timed out: " + s.name).c_str(), false); done = true; }
    if (done) { current++; stepT = 0; }
}

// --------------------------------------------------------------------- script
void Autopilot::build() {
    auto add = [&](const char* name, float timeout, std::function<bool(Game&, float)> run) {
        steps.push_back({ name, timeout, std::move(run) });
    };

    // ===== menus =====
    add("menu", 6, [this](Game& g, float t) {
        if (t < 1.5f) return false;
        check("boots to the main menu", g.screen == Screen::Menu);
        shot(g, "menu");
        return true;
    });
    add("open champions", 3, [](Game&, float t) { return t > 0.1f && clickLabel("Champions"); });
    add("pick a champion", 4, [this](Game& g, float t) {
        if (t < 0.4f || g.screen != Screen::Champions) return false;
        return clickLabel("Atanas");
    });
    add("champion picked", 4, [this](Game& g, float t) {
        if (t < 0.3f) return false;
        check("clicking a card picks that champion", g.profile.champion == 1);
        int cards = 0;
        for (int i = 0; i < CHAMPION_COUNT; ++i)
            for (auto& h : ui::hits()) if (h.label == CHAMPIONS[i].name) { cards++; break; }
        check(TextFormat("all %d champions have a card, Momiche included", CHAMPION_COUNT), cards == CHAMPION_COUNT);
        bool sides = true;
        for (auto& w : g.walk) sides &= w.ok() && w.frames == 5 && w.fw * 4 == w.fh * 3;
        check("every champion walks from all four sides (a walk sheet each)", sides);
        shot(g, "champions");
        for (auto& h : ui::hits())          // point at Momiche's card to show her details
            if (h.label == "Momiche") in::moveMouse({ h.rect.x + h.rect.width / 2, h.rect.y + h.rect.height / 2 });
        return true;
    });
    add("momiche's card", 3, [this](Game& g, float t) {
        if (t < 0.3f) return false;
        shot(g, "champions-momiche");
        return true;
    });
    add("leave champions", 3, [](Game&, float t) {
        if (t < 0.1f) return false;
        in::tap(KEY_ESCAPE);
        return true;
    });
    add("settings", 4, [this](Game& g, float t) {
        if (t < 0.3f) return false;
        check("Esc goes back to the menu", g.screen == Screen::Menu);
        return clickLabel("Settings");
    });
    add("settings shown", 4, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("the settings screen opens", g.screen == Screen::Settings);
        shot(g, "settings");
        return clickLabel("Back");
    });
    add("new run", 4, [this](Game& g, float t) {
        if (t < 0.3f) return false;
        return clickLabel("New Run");
    });
    add("intro", 5, [this](Game& g, float t) {
        if (t < 2.0f) return false;
        check("the intro plays", g.screen == Screen::Intro);
        shot(g, "intro");
        return true;
    });
    add("skip intro", 3, [](Game&, float t) {
        if (t < 0.1f) return false;
        in::click(MOUSE_BUTTON_LEFT);
        return true;
    });
    add("start room", 4, [this](Game& g, float t) {
        if (t < 1.2f) return false;
        check("a run starts in the first room of floor 1", g.screen == Screen::Play && g.mode == Mode::Run && g.floor == 1 && g.dungeon.cur().isStart);
        check("the champion's perk applies (Atanas: 120 max health)", g.player.champion == 1 && g.player.maxHp == 120);
        check("the run starts with the Rusty Sword, art and all", g.player.weapon == 0 && TextIsEqual(WEAPONS[0].name, "Rusty Sword") && g.weaponTex[0].id != 0);
        shot(g, "start-room");
        return true;
    });

    // ===== moving and talking =====
    add("walk", 3, [this](Game& g, float t) {
        if (t == 0) { markX = g.player.pos.x; in::hold(g.settings.keys.right, true); return false; }
        if (t < 0.5f) return false;
        in::hold(g.settings.keys.right, false);
        check("holding D walks right", g.player.pos.x > markX + 40);
        return true;
    });
    add("oracle", 4, [this](Game& g, float t) {
        if (t == 0) { g.player.pos = { ORACLE_POS.x + 24, ORACLE_POS.y + 16 }; return false; }
        if (t < 0.2f) return false;
        in::tap(g.settings.keys.interact);
        return true;
    });
    add("oracle speaks", 4, [this](Game& g, float t) {
        if (t < 0.3f) return false;
        check("E talks to the oracle", !g.dialogue.empty());
        shot(g, "oracle");
        return true;
    });
    add("oracle done", 4, [](Game& g, float) {
        if (g.dialogue.empty()) return true;
        in::tap(g.settings.keys.interact);      // each press shows the next line
        return false;
    });

    // ===== a combat room =====
    add("combat room", 3, [this](Game& g, float) {
        check("there is a combat room", goToRoom(g, isCombatRoom));
        check("its monsters wake", !g.enemies.empty() && g.inFight);
        keepAlive = true;
        return true;
    });
    add("blade", 3, [this](Game& g, float t) {
        if (t == 0) {
            Enemy& e = g.enemies.front();
            e.pos = { g.player.pos.x + 26, g.player.pos.y };
            markF = e.hp;
            aimAt(g, e.pos);
            in::click(MOUSE_BUTTON_LEFT);
            return false;
        }
        if (t < 0.1f) return false;
        check("the blade hits what's in front", !g.enemies.empty() && g.enemies.front().hp < markF);
        shot(g, "sword-swing");
        return true;
    });
    add("enter casts", 3, [this](Game& g, float t) {
        if (t == 0) {
            g.player.castCd = 0; g.player.mana = g.player.maxMana;
            if (!g.enemies.empty()) aimAt(g, g.enemies.front().pos);
            in::hold(KEY_ENTER, true);
            return false;
        }
        if (g.player.castCd <= 0 && t < 1) return false;
        in::hold(KEY_ENTER, false);
        check("Enter casts the selected power", g.player.castCd > 0);
        return true;
    });
    add("clear the room", 30, [this](Game& g, float t) {
        Room& r = g.dungeon.cur();
        if (r.cleared) { check("spells clear the room and open the doors", true); return true; }
        if (t > 1.0f && !markB) { shot(g, "combat"); markB = true; }
        Enemy* target = nullptr; float best = 1e9f;
        for (auto& e : g.enemies) {
            float d = Vector2Distance(e.pos, g.player.pos);
            if (e.alive && d < best) { best = d; target = &e; }
        }
        if (!target) return false;
        if (!lineClear(r, g.player.pos, target->pos, 6)) {     // step to where the target can be seen
            for (int k = 0; k < 8; ++k) {
                Vector2 spot = Vector2Add(target->pos, Vector2Scale(fromAngle(k * PI / 4), 90));
                int tx = (int)(spot.x / TILE), ty = (int)(spot.y / TILE);
                if (r.isFloor(tx, ty) && lineClear(r, spot, target->pos, 6)) { g.player.pos = spot; break; }
            }
        }
        g.player.mana = g.player.maxMana;
        g.player.selected = (int)(t * 2) % SPELL_COUNT;
        aimAt(g, target->pos);
        in::click(MOUSE_BUTTON_RIGHT);
        return false;
    });
    add("room cleared", 3, [this](Game& g, float t) {
        if (t < 1.4f) return false;
        markB = false;
        shot(g, "room-cleared");
        return true;
    });

    // ===== how a hit feels =====
    auto stageDummy = [](Game& g, float x) {    // an open room, with a target to the right
        Room& r = g.dungeon.cur();
        for (int y = 1; y < ROOM_TH - 1; ++y) for (int x2 = 1; x2 < ROOM_TW - 1; ++x2) r.tiles[y][x2] = RT_FLOOR;
        g.player.pos = { 120, 176 };
        g.player.mana = g.player.maxMana; g.player.castCd = 0; g.player.meleeCd = 0;
        g.enemies.clear();
        g.enemies.push_back(dummy({ x, 176 }));
    };
    add("bat hit", 3, [this, stageDummy](Game& g, float t) {
        if (t == 0) {
            stageDummy(g, 146);
            markWeapon = g.player.weapon;
            g.player.weapon = 3;                // the Baseball Bat: the hardest shove
            markX = 146; markF = g.enemies.front().hp;
            aimAt(g, g.enemies.front().pos);
            in::click(MOUSE_BUTTON_LEFT);
            return false;
        }
        if (g.enemies.front().hp >= markF) return false;
        check("a blade hit flashes, shakes and freezes the world", !g.flashes.empty() && g.shake > 0 && g.hitStop > 0);
        check("the enemy turns white", g.enemies.front().hitFlash > 0);
        shot(g, "blade-hit");
        return true;
    });
    add("bat knockback", 3, [this](Game& g, float t) {
        if (t < 0.7f) return false;
        float pushed = g.enemies.front().pos.x - markX;
        check(TextFormat("the Baseball Bat knocks it back %.0f (its stat: %.0f)", pushed, WEAPONS[3].knockback), fabsf(pushed - WEAPONS[3].knockback) < 6);
        g.player.weapon = markWeapon;
        check("only the Pink Sword deals Anal damage", WEAPONS[4].anal && !WEAPONS[0].anal && !WEAPONS[3].anal);
        return true;
    });
    add("fire charge", 3, [this, stageDummy](Game& g, float t) {
        if (t == 0) {
            stageDummy(g, 420);
            g.player.selected = SP_FIRE;
            markF = g.enemies.front().hp;
            aimAt(g, g.enemies.front().pos);
            in::click(MOUSE_BUTTON_RIGHT);
            return false;
        }
        if (g.castT <= 0 || g.castT > SPELLS[SP_FIRE].charge * 0.3f) return false;
        check("Emberhex charges before it flies", g.shots.empty());
        shot(g, "fire-charge");
        return true;
    });
    add("fire flight", 3, [this](Game& g, float) {
        if (g.shots.empty() || g.shots.front().pos.x < 300) return false;
        shot(g, "fire-flight");
        return true;
    });
    add("fire burst", 3, [this](Game& g, float) {
        if (g.enemies.front().hp >= markF) return false;
        check("Emberhex bursts and sets its target alight", !g.flashes.empty() && g.enemies.front().burnT > 0);
        shot(g, "fire-burst");
        markF = g.enemies.front().hp;
        return true;
    });
    add("burning", 3, [this](Game& g, float t) {
        if (t < 0.6f) return false;
        check("burning does no damage", g.enemies.front().burnT > 0 && g.enemies.front().hp == markF);
        shot(g, "burning");
        return true;
    });
    add("ice charge", 3, [this, stageDummy](Game& g, float t) {
        if (t == 0) {
            stageDummy(g, 500);
            g.player.selected = SP_ICE;
            markF = g.enemies.front().hp;
            aimAt(g, g.enemies.front().pos);
            in::click(MOUSE_BUTTON_RIGHT);
            return false;
        }
        if (g.castT <= 0 || g.castT > SPELLS[SP_ICE].charge * 0.4f) return false;
        shot(g, "ice-charge");
        return true;
    });
    add("ice trail", 3, [this](Game& g, float) {
        if (g.spikes.size() < 40) return false;
        shot(g, "ice-trail");
        const IceSpike* first = nullptr, *last = nullptr;
        int above = 0, below = 0;
        bool leaning = true;
        for (auto& s : g.spikes) {
            if (s.pos.y < 173) { above++; leaning &= s.rise.y < -s.width * 4; }    // on this line the lean shows in y:
            else if (s.pos.y > 179) { below++; leaning &= s.rise.y > -s.width * 2; }           // those above reach up, those below lie down
            else {
                if (!first || s.seq < first->seq) first = &s;
                if (!last || s.seq > last->seq) last = &s;
            }
        }
        check("the spikes grow the further the trail runs", first && last && -last->rise.y > -first->rise.y + 8);
        check("small spikes line both sides, leaning outward", above > 5 && below > 5 && leaning);
        return true;
    });
    add("ice hit", 3, [this](Game& g, float) {
        if (g.enemies.front().hp >= markF) return false;
        check("Tidelash cracks the floor around its target", !g.cracks.empty());
        return true;
    });
    add("ice cracks", 3, [this](Game& g, float t) {
        if (t < 0.25f) return false;
        shot(g, "ice-cracks");
        return true;
    });
    add("ice shatter", 3, [this](Game& g, float t) {
        if (g.spikes.size() > 30) return false;
        if (!markB) { shot(g, "ice-shatter"); markB = true; }
        if (!g.spikes.empty()) return false;
        check("then the whole trail shatters", g.spikes.empty());
        markB = false;
        g.enemies.clear();
        return true;
    });

    add("bolt flight", 3, [this, stageDummy](Game& g, float t) {
        if (t == 0) {
            stageDummy(g, 520);
            g.player.selected = SP_BOLT;
            markF = g.enemies.front().hp;
            aimAt(g, g.enemies.front().pos);
            in::click(MOUSE_BUTTON_RIGHT);
            return false;
        }
        if (g.shots.empty() || g.shots.front().pos.x < 330) return false;
        check("Stormbolt leaves a lightning trail", g.arcs.size() >= 6);
        shot(g, "bolt-flight");
        return true;
    });
    add("bolt strikes", 3, [this](Game& g, float) {
        if (g.enemies.front().hp >= markF) return false;
        check("the strike flashes and lights the enemy white", !g.flashes.empty() && g.skyFlash > 0 && g.enemies.front().hitFlash > 0.1f);
        shot(g, "bolt-strike");
        return true;
    });
    add("bolt fades", 3, [this](Game& g, float t) {
        if (t < 0.5f) return false;
        check("the lightning is gone a moment later", g.arcs.empty() && g.skyFlash == 0);
        g.enemies.clear();
        return true;
    });

    // ===== a chest and Marin =====
    add("chest", 4, [this](Game& g, float t) {
        if (t == 0) {
            if (!goToRoom(g, [](const Room& r) { return r.hasChest && !r.isBoss; }, true)) {
                check("(no chest on this floor)", true);
                markB = true;
                return true;
            }
            Room& r = g.dungeon.cur();
            g.player.pos = { r.chestTx * (float)TILE + 16, r.chestTy * (float)TILE + 40 };
            return false;
        }
        if (t < 0.2f) return false;
        in::tap(g.settings.keys.interact);
        return true;
    });
    add("chest shakes", 3, [this](Game& g, float t) {
        if (markB) return true;
        if (t < 0.3f) return false;
        check("a chest shakes first, the loot still inside", g.dungeon.cur().chestOpened && g.chestOpenT < CHEST_SLAM && g.coins.empty());
        g.chestPack = true;                     // this one holds a pack
        markF = (float)g.player.packs;
        shot(g, "chest-shake");
        return true;
    });
    add("chest opened", 3, [this](Game& g, float t) {
        if (markB) { markB = false; return true; }
        if (t < 0.3f) return false;
        check("E opens a chest", g.dungeon.cur().chestOpened);
        check("the lid slams up with light, stars and loot", g.chestLight > 0 && !g.coins.empty() && !g.parts.empty());
        shot(g, "chest");
        return true;
    });
    add("chest looted", 5, [this](Game& g, float t) {
        if (g.chestOpenT > 90 || t < 0.5f) return g.chestOpenT > 90;
        if (t < 0.8f && !markC) { shot(g, "chest-light"); markC = true; }
        if (g.chestLight > 0) return false;
        markC = false;
        check("the light goes out once the chest is looted", g.coins.empty() && g.popups.empty());
        check("a chest can hold a cigarette pack", g.player.packs == (int)markF + 1);
        return true;
    });
    add("shop", 4, [this](Game& g, float t) {
        if (t == 0) {
            check("the floor has its shop", goToRoom(g, [](const Room& r) { return r.isShop; }));
            g.dungeon.cur().isGalin = false;    // whichever it rolled, see Marin's first
            g.player.gold = 200;
            g.player.pos = { MERCHANT_POS.x, MERCHANT_POS.y + 40 };
            return false;
        }
        if (t < 0.2f) return false;
        in::tap(g.settings.keys.interact);
        return true;
    });
    add("shop open", 4, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("E opens Marin's shop", g.overlay == Overlay::Shop);
        shot(g, "shop");
        g.player.hp = g.player.maxHp - 40;
        return clickLabel("8 g");
    });
    add("bought", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("buying a Health Potion takes 8 gold and heals", g.player.gold == 192 && g.player.hp > g.player.maxHp - 40);
        check("Marin sells potions and vapes, nothing else", SHOP_COUNT == 2 + VP_COUNT);
        return clickLabel("30 g");              // Mango Haze
    });
    add("vape bought", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("Mango Haze is bought once and kept", g.player.has(VP_MANGO) && g.player.gold == 162);
        shot(g, "shop-vape");
        in::tap(KEY_ESCAPE);
        return true;
    });
    add("vape speed", 3, [this](Game& g, float t) {
        if (t == 0) {
            g.player.pos = { ROOM_CENTER.x - 200, ROOM_CENTER.y + 60 };
            markX = g.player.pos.x;
            in::hold(g.settings.keys.right, true);
            return false;
        }
        if (t < 0.5f) return false;
        in::hold(g.settings.keys.right, false);
        float speed = (g.player.pos.x - markX) / t;
        check(TextFormat("Mango Haze walks 10%% faster (%.0f)", speed), speed > WALK * 1.03f && speed < WALK * 1.17f);
        return true;
    });

    // ===== Galin's stall =====
    add("galin", 4, [this](Game& g, float t) {
        if (t == 0) {
            g.dungeon.cur().isGalin = true;
            g.player.gold = 200; g.player.packs = 0;
            g.player.pos = { MERCHANT_POS.x, MERCHANT_POS.y + 40 };
            return false;
        }
        if (t < 0.2f) return false;
        shot(g, "galin-room");
        return true;
    });
    add("trade with galin", 3, [this](Game& g, float t) {
        if (t < 0.1f) return false;
        in::tap(g.settings.keys.interact);
        return true;
    });
    add("galin open", 4, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("E opens Galin's stall", g.overlay == Overlay::Galin);
        check("Galin, the stall and the pack have their art", g.galinTex.id && g.galinShopTex.id && g.packTex.id);
        bool armed = true;
        for (auto& tx : g.weaponTex) armed = armed && tx.id;
        check("every weapon has its own art", armed);
        shot(g, "galin");
        return clickLabel("50 g");              // the Iron Sword
    });
    add("weapon bought", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("buying the Iron Sword puts it in hand", g.player.weapon == 1 && g.player.owns(1) && g.player.gold == 150);
        markF = (float)g.bladeDamage();
        return clickLabel("Sharpen: 1 pack, 25 g");
    });
    add("no pack", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("no cigarette pack, no upgrade", g.player.upgrades[1][UP_SHARPEN] == 0 && g.player.gold == 150);
        g.player.packs = 2;
        return clickLabel("Sharpen: 1 pack, 25 g");
    });
    add("upgraded", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("an upgrade burns a pack and gold, and the weapon hits harder",
            g.player.upgrades[1][UP_SHARPEN] == 1 && g.player.packs == 1 && g.player.gold == 125 && g.bladeDamage() == (int)markF + 3);
        shot(g, "galin-upgraded");
        return clickLabel("Equip");             // back to the Rusty Sword
    });
    add("swap back", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("owned weapons swap for free, each with its own upgrades",
            g.player.weapon == 0 && g.player.gold == 125 && g.player.upgrades[0][UP_SHARPEN] == 0 && g.player.upgrades[1][UP_SHARPEN] == 1);
        return clickLabel("Equip");             // and the Iron Sword again
    });
    add("leave galin", 3, [this](Game& g, float t) {
        if (t < 0.2f) return false;
        check("the upgrades come back with the weapon", g.player.weapon == 1 && g.bladeDamage() == (int)markF + 3);
        in::tap(KEY_ESCAPE);
        return true;
    });
    for (int i = 0; i < WEAPON_COUNT; ++i)
        add(TextFormat("swing weapon %d", i), 3, [this, i](Game& g, float t) {
            if (t == 0) { g.player.weapon = i; g.player.facing = { 1, 0 }; g.player.meleeCd = 0; return false; }
            if (t < 0.1f) { g.swingBlade(); return false; }
            if (g.player.meleeAnim > 0.6f) return false;
            shot(g, TextFormat("swing-%d", i));
            return true;
        });
    add("iron sword again", 1, [](Game& g, float) { g.player.weapon = 1; return true; });

    // ===== the six floor bosses =====
    auto enterBoss = [this, add](int index) {
        add(TextFormat("boss %d", index + 1), 4, [this, index](Game& g, float t) {
            if (t < 0.3f) return false;
            check(TextFormat("floor %d's boss room holds %s", index + 1, BOSSES[index].name),
                goToRoom(g, [](const Room& r) { return r.isBoss; }) && g.fight.active() && g.fight.def == &BOSSES[index]);
            g.player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 2.0f };
            keepAlive = true;
            return true;
        });
    };
    auto slayAndDescend = [this, add](int index, const char* shotName) {
        add(TextFormat("slay boss %d", index + 1), 15, [this, index, shotName](Game& g, float t) {
            if (t < 0.2f) return false;
            slayBosses(g);
            if (!g.dungeon.cur().cleared) return false;
            check(TextFormat("%s falls and the room opens", BOSSES[index].name), (g.profile.bossesBeaten >> index) & 1u);
            if (shotName) shot(g, shotName);
            return true;
        });
        if (index + 1 < BOSS_COUNT)
            add(TextFormat("descend from %d", index + 1), 8, [this, index](Game& g, float t) {
                g.player.pos = ROOM_CENTER;           // stand on the stairs once the loot is in
                if (g.floor != index + 2) return false;
                check(TextFormat("the stairs lead down to floor %d", index + 2), g.dungeon.cur().isStart);
                return true;
            });
    };

    // floor 1: the Good Plazmodium
    enterBoss(0);
    add("good plaz fires", 5, [this](Game& g, float t) {
        if (countShots(g, ShotKind::Orb) < 3) return false;
        check("its painted arena is loaded", g.bossArt.arena[0].id != 0);
        check("the Good Plazmodium fires a 3-shot spread", countShots(g, ShotKind::Orb) >= 3);
        shot(g, "boss-good-plazmodium");
        return true;
    });
    add("spell hits boss", 4, [this](Game& g, float t) {
        Enemy* b = boss(g);
        if (!b) return false;
        if (t == 0) markF = b->hp;
        if (b->hp < markF) { check("spells hurt the boss", true); return true; }
        g.player.mana = g.player.maxMana;
        aimAt(g, b->pos);
        in::click(MOUSE_BUTTON_RIGHT);
        return false;
    });
    add("aura", 3, [this](Game& g, float t) {
        if (t == 0) {
            g.player.hp = 50; g.player.specialCd = 0; g.player.invuln = 1;   // no stray hits while measuring
            in::tap(g.settings.keys.special);
            return false;
        }
        check("Q: Atanas' Aura of Calmness heals 25 and slows the fight", fabsf(g.player.hp - 75) < 0.5f && g.player.auraT > 0);
        shot(g, "aura-of-calmness");
        return true;
    });
    add("split", 16, [this](Game& g, float t) {     // on the way down it breaks the arena and charges its core
        if (countForm(g, 1) == 3) {
            check("the Plazmodium multiplies into three copies", true);
            shot(g, "plazmodium-split");
            return true;
        }
        Enemy* b = boss(g);
        if (!b) return false;
        b->hp = fminf(b->hp, 1);
        g.player.mana = g.player.maxMana;
        aimAt(g, b->pos);
        in::click(MOUSE_BUTTON_RIGHT);
        return false;
    });
    slayAndDescend(0, "boss-down");

    // floor 2: the Bad Plasmodium
    enterBoss(1);
    add("bad plaz balls", 6, [this](Game& g, float) {
        Enemy* b = boss(g);
        if (!b || countShots(g, ShotKind::Orb) < 16) return false;
        int sides = 0;
        for (auto& p : g.enemyShots)
            if (p.kind == ShotKind::Orb) sides |= 1 << ((p.pos.x > b->pos.x ? 1 : 0) + (p.pos.y > b->pos.y ? 2 : 0));
        check("the Bad Plasmodium's head is loaded", g.bossArt.poses[1].id != 0 && g.bossArt.walk[1].ok() && SPR.rock.ok());
        check("the Bad Plasmodium shoots its balls in every direction", sides == 15);
        markX = b->pos.x; markY = b->pos.y;
        shot(g, "boss-bad-plasmodium");
        return true;
    });
    add("bad plaz grabs", 8, [this](Game& g, float) {
        Enemy* b = boss(g);
        if (!b || b->pose != 2) return false;
        check("it grabs a rock", true);
        shot(g, "head-grab");
        return true;
    });
    add("bad plaz throws", 4, [this](Game& g, float) {
        if (countShots(g, ShotKind::Rock) < 1) return false;
        check("and throws it", true);
        shot(g, "head-rock");
        return true;
    });
    add("bad plaz minions", 10, [this](Game& g, float) {
        Enemy* b = boss(g);
        if (!b || countForm(g, 1) < 1) return false;
        check("its old body walks out as minions", b->moving == false);
        shot(g, "head-minions");
        return true;
    });
    add("bad plaz slam", 8, [this](Game& g, float) {
        Enemy* b = boss(g);
        if (!b) return false;
        g.player.pos = { b->pos.x, b->pos.y + b->r + 50 };          // too close
        if (g.fight.meleeWind <= 0 || g.fight.meleeWind > 0.2f) return false;
        check("it slams whoever stands close", b->pose == 1);
        check("and through all of it the head has not moved", b->pos.x == markX && b->pos.y == markY);
        shot(g, "head-slam");
        return true;
    });
    slayAndDescend(1, nullptr);

    // floor 3: Mordekai
    enterBoss(2);
    add("mordekai fan", 5, [this](Game& g, float) {
        if (countShots(g, ShotKind::Feather) < 7) return false;
        check("Mordekai fires a fan of 7 feathers", true);
        shot(g, "boss-mordekai");
        return true;
    });
    add("mordekai dive", 9, [this](Game& g, float) {
        Enemy* b = boss(g);
        if (!b || b->diveT <= 0) return false;
        check("Mordekai dives every third volley", true);
        return true;
    });
    slayAndDescend(2, nullptr);

    // floor 4: Vlado
    enterBoss(3);
    add("vlado tickets", 5, [this](Game& g, float) {
        if (countShots(g, ShotKind::Ticket) < 12) return false;
        check("Vlado throws a spiral of 12 tickets", true);
        return true;
    });
    add("whistle", 8, [this](Game& g, float) {
        if (!g.fight.bus.active) return false;
        check("the whistle calls bus No.9 with a warning lane", !g.fight.bus.driving);
        shot(g, "bus-warning");
        return true;
    });
    add("bus", 4, [this](Game& g, float t) {
        Bus& bus = g.fight.bus;
        if (!bus.active) return false;
        if (!bus.driving) { markB = false; return false; }
        if (!markB) {                       // step into the lane, just ahead of the bus
            markB = true;
            g.enemyShots.clear();
            g.player.hp = g.player.maxHp; g.player.invuln = 0;
            g.player.pos = { fminf(bus.x + bus.w / 2 + 50, ROOM_PX_W - TILE * 2.0f), bus.y };
            markF = g.player.hp;
            return false;
        }
        if (!bus.hit) return false;
        check("bus No.9 hits for 20", markF - g.player.hp >= 19.5f);
        shot(g, "bus");
        markB = false;
        return true;
    });
    slayAndDescend(3, nullptr);

    // floor 5: Triceps in her cave. Her four attacks take turns, so they're checked in order.
    enterBoss(4);
    add("triceps walks", 5, [this](Game& g, float t) {
        Enemy* b = boss(g);
        if (!b) return false;
        if (t == 0) { markX = b->pos.x; markY = b->pos.y; }
        if (!b->moving || Vector2Distance(b->pos, { markX, markY }) < 20) return false;
        const WalkSheet& w = g.bossArt.walk[4];
        check("her cave arena is loaded", g.bossArt.arena[4].id != 0);
        check("Triceps walks as her own figure (five frames a side) with her attack poses",
            w.ok() && w.frames == 5 && g.bossArt.poses[4].id != 0);
        shot(g, "boss-triceps");
        return true;
    });
    add("gravity grenade", 8, [this](Game& g, float) {
        Grenade& gr = g.fight.grenades[0];
        if (!gr.active || gr.well <= 0) return false;
        Vector2 pull = g.fight.gravityPull({ gr.to.x + 60, gr.to.y });   // a step to the right of the middle
        check("a gravity grenade lands on you and its well pulls you in", pull.x < -30 && fabsf(pull.y) < 1);
        shot(g, "triceps-well");
        return true;
    });
    add("blast", 4, [this](Game& g, float) {
        Grenade& gr = g.fight.grenades[0];
        g.player.pos = gr.to; g.player.invuln = 0;      // stay in the middle for the blast
        if (!gr.hit) return false;
        check("the well goes off under you", true);
        return true;
    });
    add("magnetic boots", 8, [this](Game& g, float) {
        int boots = 0, curving = 0;
        for (auto& p : g.enemyShots) if (p.kind == ShotKind::Boot) { boots++; curving += p.curve != 0; }
        if (boots < 3) return false;
        check("she kicks three magnetic boots, two curving in from the sides", boots == 3 && curving == 2);
        shot(g, "triceps-boots");
        return true;
    });
    add("fulcrum staff", 9, [this](Game& g, float) {
        if (!g.fight.staff.swinging) return false;
        check("she comes at you and swings the fulcrum staff", true);
        shot(g, "triceps-staff");
        return true;
    });
    add("iron balls", 3, [this](Game& g, float) {
        if (countShots(g, ShotKind::Ball) < 3) return false;
        check("the staff's far end bats three iron balls", true);
        return true;
    });
    add("circuit chain", 9, [this](Game& g, float) {
        const Circuit& c = g.fight.circuit;
        if (!c.active || c.charge > 0) return false;
        check("her circuit chain charges, then goes live across four nodes", c.count == 4);
        shot(g, "triceps-circuit");
        return true;
    });
    add("shock", 3, [this](Game& g, float) {
        const Circuit& c = g.fight.circuit;
        if (!c.active) return false;
        g.player.pos = Vector2Lerp(c.node[0], c.node[1], 0.5f); g.player.invuln = 0;   // onto the first wire
        if (c.shockCd <= 0) return false;
        check("a live wire shocks you", true);
        return true;
    });
    slayAndDescend(4, nullptr);

    // floor 6: the Cinder-Maw
    enterBoss(5);
    add("embers", 6, [this](Game& g, float) {
        if (countShots(g, ShotKind::Ember) < 10) return false;
        check("the Cinder-Maw breathes a ring of embers", true);
        shot(g, "boss-cinder-maw");
        return true;
    });
    slayAndDescend(5, nullptr);
    add("run complete", 7, [this](Game& g, float t) {
        if (g.overlay != Overlay::RunComplete && t < 5) return false;    // after BOSS DEFEATED has had its moment
        check("beating floor 6 completes the Spice Trip", g.overlay == Overlay::RunComplete && g.profile.runsCompleted == 1);
        check("every boss is marked as beaten", g.profile.bossesBeaten == (1u << BOSS_COUNT) - 1);
        shot(g, "run-complete");
        return true;
    });
    add("descend deeper", 3, [](Game&, float t) { return t > 0.1f && clickLabel("Descend deeper"); });
    add("deeper", 4, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("floor 7 starts the cycle again, stronger", g.floor == 7 && g.overlay == Overlay::None
            && g.currentBossIndex() == 0 && g.currentStrength() > 1.4f);
        check("the vape and the upgraded weapon are still there seven floors on", g.player.has(VP_MANGO) && g.player.weapon == 1 && g.player.upgrades[1][UP_SHARPEN] == 1);
        keepAlive = false;
        return true;
    });

    // ===== pause, save, continue =====
    add("pause", 3, [this](Game& g, float t) {
        if (t == 0) { in::tap(KEY_ESCAPE); return false; }
        if (t < 0.3f) return false;
        check("Esc pauses", g.overlay == Overlay::Pause);
        shot(g, "pause");
        return clickLabel("Save");
    });
    add("saved", 3, [this](Game& g, float t) {
        if (t < 0.3f) return false;
        check("Save writes the run", save::hasRun());
        return clickLabel("Main Menu");
    });
    add("continue", 4, [this](Game& g, float t) {
        if (t < 0.5f) return false;
        check("Main Menu leaves the run", g.screen == Screen::Menu && g.hasSave);
        return clickLabel("Continue");
    });
    add("continued", 3, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("Continue resumes on floor 7", g.screen == Screen::Play && g.floor == 7);
        check("the save keeps the vape, the weapon and the pack", g.player.has(VP_MANGO) && g.player.weapon == 1 && g.player.upgrades[1][UP_SHARPEN] == 1 && g.player.packs == 1);
        g.toMenu();
        return true;
    });

    // ===== Boss Rush =====
    add("boss rush", 4, [this](Game& g, float t) {
        if (t < 0.5f) return false;
        return clickLabel("Boss Rush");
    });
    add("rush begins", 3, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("Boss Rush starts with the Good Plazmodium at level 5", g.mode == Mode::BossRush && g.fight.def == &BOSSES[0] && g.player.level == 5);
        keepAlive = true; markB = false;
        return true;
    });
    add("rush", 60, [this](Game& g, float t) {
        if (g.overlay == Overlay::RushOver) return true;
        if (g.rushStage == 1 && g.fight.active() && !markB && g.fight.time > 1.0f) { shot(g, "rush-stage-2"); markB = true; }
        if (g.fight.active() && g.fight.time > 1.2f) slayBosses(g);
        return false;
    });
    add("rush won", 3, [this](Game& g, float t) {
        if (t < 0.4f) return false;
        check("all six bosses back to back wins the Boss Rush", g.rushWon && g.profile.bestRush > 0);
        shot(g, "rush-won");
        keepAlive = false;
        return true;
    });
    add("leave rush", 3, [](Game&, float t) { return t > 0.1f && clickLabel("Main menu"); });

    // ===== every champion's special =====
    for (int i = 0; i < CHAMPION_COUNT; ++i) {
        add(TextFormat("special %d", i), 3, [this, i](Game& g, float t) {
            const ChampionDef& c = CHAMPIONS[i];
            if (t == 0) {
                g.profile.champion = i;
                g.startRun();
                goToRoom(g, isCombatRoom);
                g.enemies.clear();
                g.enemies.push_back(dummy({ ROOM_CENTER.x + (c.kind == Special::BearJump ? 30.0f : c.kind == Special::Scare ? 20.0f : 60.0f), ROOM_CENTER.y }));
                g.player.pos = ROOM_CENTER;
                g.player.facing = { 1, 0 };
                aimAt(g, { ROOM_CENTER.x + 120, ROOM_CENTER.y });
                switch (c.kind) {
                case Special::ArcaneSurge: g.player.mana = 10; break;
                case Special::Drink:       g.player.hp = 50; break;
                case Special::CoinBarrage: g.player.charge = COIN_POWER_MAX; break;
                case Special::Scare:       g.enemies.front().speed = 70; g.enemies.front().dmg = 10; break;   // close enough to bite
                case Special::MugShield: {
                    Projectile p;
                    p.pos = { ROOM_CENTER.x - 60, ROOM_CENTER.y }; p.vel = { 200, 0 };
                    p.damage = 10; p.r = 6; p.life = 3; p.kind = ShotKind::Orb; p.color = RED; p.glow = RED;
                    g.enemyShots.push_back(p);
                    break;
                }
                default: break;
                }
                markX = g.player.pos.x; markY = g.player.pos.y;
                in::tap(g.settings.keys.special);
                return false;
            }
            float checkAt = c.kind == Special::CoinBarrage || c.kind == Special::Koi ? 0.05f : 0.8f;
            if (t < checkAt) return false;
            const Enemy* d = g.enemies.empty() ? nullptr : &g.enemies.front();
            int coinShots = 0, koi = 0;
            for (auto& p : g.shots) { coinShots += p.kind == ShotKind::Coin; koi += p.kind == ShotKind::Koi; }
            bool ok = false;
            switch (c.kind) {
            case Special::ArcaneSurge: ok = g.player.mana >= 49; break;
            case Special::Aura:        ok = g.player.auraT > 0 || g.player.specialCd > 0; break;
            case Special::CoinDash:    ok = Vector2Distance(g.player.pos, { markX, markY }) > 80; break;
            case Special::MugShield:   ok = g.enemyShots.empty() && g.player.hp == g.player.maxHp; break;
            case Special::MelonRoll:   ok = d && d->hp <= d->maxHp - 20; break;
            case Special::Drink:       ok = g.player.hp >= 69 && g.player.wobbleT > 0; break;
            case Special::BearJump:    ok = d && d->hp <= d->maxHp - 18; break;
            case Special::CoinBarrage: ok = coinShots == 16; break;
            case Special::Koi:         ok = koi == 3; break;
            case Special::Blink:       ok = g.player.pos.x > markX + 100 && g.player.charge < DUST_MAX; break;
            case Special::Scare:       ok = g.scareT > 0 && d && d->pos.x > ROOM_CENTER.x + 40 && g.player.hp == g.player.maxHp; break;
            }
            check(TextFormat("%s's special (%s) works", c.name, c.special), ok);
            if (c.kind == Special::Koi) shot(g, "koi");
            return true;
        });
    }

    // ===== shops, Gakev the enemy and his cigarette packs =====
    add("nobody gets stuck", 30, [this](Game& g, float) {
        struct Walker { const char* name; float r, speed; int kind; };
        static const Walker walkers[] = {
            { "Mistling", 12, 60, EK_SKELETON }, { "Hollow Hound", 13, 110, EK_SKELETON2 },
            { "Gakev", 12, 120, EK_GAKEV }, { "Melon Warrior", 14, 50, EK_MELON },
            { "Wisp", 10, 85, EK_SKULL }, { "Toshko", 12, 175, EK_TOSHKO },
        };
        const float dt = 1 / 30.0f, limit = 40;
        const int rooms = 60;
        auto freshRoom = [&]() -> Room& {       // a combat room's layout, with nobody in it
            g.scareT = 0;                       // the last special may still have them running
            g.dungeon.generate();
            goToRoom(g, isCombatRoom);
            g.dungeon.cur().cleared = false;
            g.enemies.clear();
            return g.dungeon.cur();
        };
        auto spot = [](const Room& r) {
            for (;;) {
                int tx = GetRandomValue(1, ROOM_TW - 2), ty = GetRandomValue(1, ROOM_TH - 2);
                if (r.isFloor(tx, ty)) return Vector2{ tx * (float)TILE + 16, ty * (float)TILE + 16 };
            }
        };
        for (const Walker& w : walkers) {
            int stuck = 0;
            for (int i = 0; i < rooms; ++i) {
                Room& r = freshRoom();
                g.player.pos = spot(r);
                Enemy e = dummy(spot(r));
                e.name = w.name; e.kind = w.kind; e.r = w.r; e.speed = w.speed;
                g.enemies.push_back(e);
                bool reached = false;
                for (float t = 0; t < limit && !reached; t += dt) {
                    g.player.invuln = 1;
                    g.updateEnemies(dt, 1);
                    reached = Vector2Distance(g.enemies.front().pos, g.player.pos) < w.r + PLAYER_R + 6;
                }
                stuck += !reached;
            }
            check(TextFormat("%s reaches you in every layout (%d stuck of %d)", w.name, stuck, rooms), stuck == 0);
        }
        // and a crowd of the widest ones, shoving each other through the same gaps
        int jammed = 0;
        for (int i = 0; i < rooms; ++i) {
            Room& r = freshRoom();
            g.player.pos = spot(r);
            for (int k = 0; k < 6; ++k) {
                Enemy e = dummy(spot(r));
                e.kind = k % 2 ? EK_MELON : EK_SKELETON2; e.r = k % 2 ? 14.0f : 13.0f; e.speed = k % 2 ? 50.0f : 110.0f;
                g.enemies.push_back(e);
            }
            for (float t = 0; t < limit; t += dt) { g.player.invuln = 1; g.updateEnemies(dt, 1); }
            for (auto& e : g.enemies)
                if (Vector2Distance(e.pos, g.player.pos) > 200) { jammed++; break; }
        }
        check(TextFormat("a crowd of hounds and melons gets to you too (%d jammed of %d)", jammed, rooms), jammed == 0);
        g.enemies.clear();
        return true;
    });
    add("one shop a floor", 3, [this](Game&, float) {
        static Dungeon d;                   // a Dungeon is large; keep it off the stack
        int floors = 200, oneShop = 0, turns = 0;
        bool wasGalin = false;
        for (int i = 0; i < floors; ++i) {
            d.generate();
            int shops = 0;
            bool galin = false;
            for (auto& row : d.grid) for (auto& r : row)
                if (r.exists && r.isShop) { shops++; galin = r.isGalin; }
            oneShop += shops == 1;
            turns += i > 0 && galin != wasGalin;
            wasGalin = galin;
        }
        check(TextFormat("every floor has one shop (%d of %d)", oneShop, floors), oneShop == floors);
        check(TextFormat("Marin and Galin take turns, so Galin is there by floor 2 (%d of %d)", turns, floors - 1), turns == floors - 1);
        return true;
    });
    add("gakev the enemy", 3, [this](Game& g, float t) {
        if (t == 0) {
            g.profile.champion = 0;
            g.startRun();
            goToRoom(g, isCombatRoom);
            g.enemies.clear();
            Enemy e = dummy({ ROOM_CENTER.x + 90, ROOM_CENTER.y });   // the door lane is never walled
            e.name = "Gakev"; e.kind = EK_GAKEV; e.speed = 40;        // slowed down to be photographed
            g.enemies.push_back(e);
            markF = e.pos.x;
            return false;
        }
        if (t < 0.8f) return false;
        check("Gakev hunts you, with his own sprites", SPR.enemy[EK_GAKEV].idle.ok() && !g.enemies.empty() && g.enemies.front().pos.x < markF - 10);
        shot(g, "gakev-enemy");
        return true;
    });
    add("gakev's packs", 3, [this](Game& g, float) {
        auto packsFrom = [&](int kind, int kills) {
            g.coins.clear();
            for (int i = 0; i < kills; ++i) {
                Enemy e = dummy(ROOM_CENTER);
                e.kind = kind;
                g.killEnemy(e);
            }
            int packs = 0;
            for (auto& c : g.coins) packs += c.pack;
            g.coins.clear(); g.corpses.clear(); g.parts.clear();
            return packs;
        };
        int fromGakev = packsFrom(EK_GAKEV, 1000), fromOthers = packsFrom(EK_SKELETON, 1000);
        check(TextFormat("every Gakev drops a pack (%d of 1000)", fromGakev), fromGakev == 1000);
        check("nobody else drops one", fromOthers == 0);
        g.enemies.clear();
        return true;
    });
    add("pack pickup", 3, [this](Game& g, float t) {
        if (t == 0) {
            g.coins.push_back({ { g.player.pos.x + 40, g.player.pos.y }, { 0, -120 }, 0, true });
            markF = (float)g.player.packs;
            markB = false;
            return false;
        }
        if (!markB) { shot(g, "pack-drop"); markB = true; }
        if (!g.coins.empty()) return false;
        markB = false;
        check("the pack flies to you and goes in your pocket", g.player.packs == (int)markF + 1);
        shot(g, "pack-found");
        return true;
    });

    // ===== Gakev's coins and Momiche's scare on a boss =====
    add("gakev's coins", 3, [this](Game& g, float t) {
        if (t == 0) {
            g.profile.champion = 2;             // Gakev
            g.startRun();
            goToRoom(g, isCombatRoom);
            g.enemies.clear(); g.coins.clear();
            Enemy e = dummy({ ROOM_CENTER.x + 60, ROOM_CENTER.y });
            e.hp = 0;                           // falls this frame
            g.enemies.push_back(e);
            return false;
        }
        if (t < 0.05f) return false;
        int n = (int)g.coins.size();            // a monster drops 1-3, Gakev adds three
        check(TextFormat("Gakev: a kill drops three extra coins (%d)", n), n >= 4 && n <= 6);
        return true;
    });
    add("scared boss", 4, [this](Game& g, float t) {
        if (t == 0) {
            for (int i = 0; i < CHAMPION_COUNT; ++i) if (CHAMPIONS[i].kind == Special::Scare) g.profile.champion = i;
            g.startRun();
            goToRoom(g, [](const Room& r) { return r.isBoss; });
            g.player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 3.0f };
            Enemy* b = boss(g);
            markF = b ? Vector2Distance(b->pos, g.player.pos) : 0;
            markB = false;
            in::tap(g.settings.keys.special);
            return false;
        }
        if (!markB) { g.enemyShots.clear(); markB = true; }   // nothing is in the air before the scream
        if (t < 1.2f) return false;
        Enemy* b = boss(g);
        check("Q: Momiche's scare sends the boss running", g.scareT > 0 && b && Vector2Distance(b->pos, g.player.pos) > markF + 20);
        check("a scared boss holds its fire", g.enemyShots.empty());
        shot(g, "momiche-scare");
        return true;
    });
    add("hud", 3, [this](Game& g, float t) {
        if (t < 0.6f) return false;
        shot(g, "hud");
        return true;
    });

    // ===== how a boss fight builds (Mordekai, step by step) =====
    add("boss stage one", 4, [this](Game& g, float t) {
        if (t == 0) {
            g.profile.champion = 0;
            g.startRun();
            g.floor = 3;
            goToRoom(g, [](const Room& r) { return r.isBoss; });
            g.player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 3.0f };
            keepAlive = true;
            return false;
        }
        g.player.invuln = 1;
        Enemy* b = boss(g);
        if (!b) return false;
        check("the boss bar has a notch at half", g.fight.notch(0.5f) == 0.5f);
        b->hp = b->maxHp * 0.49f;
        return true;
    });
    add("boss roars", 4, [this](Game& g, float) {
        g.player.invuln = 1;
        Enemy* b = boss(g);
        if (!b || g.fight.stage != BossFight::Stage::Breaking) return false;
        markF = b->hp;
        g.hitEnemy(*b, 50, WHITE);
        check("at half it stops, roars and can't be hurt", b->hp == markF);
        shot(g, "boss-roar");
        return true;
    });
    add("boss arena", 5, [this](Game& g, float) {
        g.player.invuln = 1;
        if (g.fight.stage != BossFight::Stage::Two) return false;
        check("the arena breaks: cracks, and rocks to hide behind", g.fight.rockCount == BossFight::ROCKS && g.fight.scarCount > 0);
        shot(g, "boss-arena");
        return true;
    });
    add("boss minions", 6, [this](Game& g, float) {
        g.player.invuln = 1;
        if (countForm(g, 2) < 1) return false;
        check("minions join the fight", true);
        shot(g, "boss-minions");
        return true;
    });
    add("boss beam aims", 12, [this](Game& g, float) {
        g.player.invuln = 1;
        const Beam& b = g.fight.beam;
        if (!b.active || b.charge > 0.2f) return false;
        const Rock& r = g.fight.rocks[0];
        check("a rock is cover", g.fight.behindRock({ r.pos.x - 60, r.pos.y }, { r.pos.x + 60, r.pos.y }));
        shot(g, "boss-beam-aim");
        g.fight.rockCount = 0;                  // nowhere to hide from this one
        return true;
    });
    add("boss beam fires", 3, [this](Game& g, float) {
        const Beam& b = g.fight.beam;
        if (!b.active) return false;
        float reach = fminf(90, Vector2Distance(b.from, g.fight.beamEnd()) - 10);
        g.player.pos = Vector2Add(b.from, Vector2Scale(fromAngle(b.ang), reach));
        g.player.invuln = 0;
        g.enemyShots.clear();
        if (!b.hit) return false;
        check("the new beam burns you where you stand", true);
        shot(g, "boss-beam");
        return true;
    });
    add("boss threes", 9, [this](Game& g, float) {
        g.player.invuln = 1;
        if (countShots(g, ShotKind::Feather) < 18) return false;
        check("its old moves come in threes", true);
        return true;
    });
    add("boss slam", 10, [this](Game& g, float) {
        Slam* s = nullptr;
        for (auto& o : g.fight.slams) if (o.active) s = &o;
        g.enemyShots.clear();
        if (!s) { g.player.invuln = 1; return false; }
        g.player.pos = s->at;
        g.player.invuln = s->wait > 0.03f ? 1.0f : 0.0f;
        if (s->wait > 0.03f) {
            if (!markC && s->wait < 0.6f) { shot(g, "boss-slam"); markC = true; }
            return false;
        }
        if (!s->hit) return false;
        markC = false;
        check("the slam waits, then lands", true);
        return true;
    });
    add("boss fire", 3, [this](Game& g, float t) {
        g.player.invuln = 1;
        g.player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 3.0f };
        if (t < 0.4f) return false;
        bool burning = false;
        for (auto& o : g.fight.slams) burning |= o.active && o.wait <= 0 && o.burn > 0;
        check("and leaves fire behind", burning);
        shot(g, "boss-fire");
        return true;
    });
    add("boss mercy", 3, [this](Game& g, float) {
        g.player.hp = g.player.maxHp; markF = g.player.hp;
        g.hurtPlayer(20, 0);
        float healthy = markF - g.player.hp;
        g.player.hp = g.player.maxHp * 0.25f; markF = g.player.hp;
        g.hurtPlayer(20, 0);
        check("your last 30% of health counts double", fabsf((markF - g.player.hp) * 2 - healthy) < 0.01f);
        g.player.hp = g.player.maxHp; g.player.invuln = 1;
        return true;
    });
    add("boss desperate", 5, [this](Game& g, float t) {
        g.player.invuln = 1;
        Enemy* b = boss(g);
        if (!b) return false;
        if (t == 0) { b->hp = b->maxHp * 0.19f; return false; }
        if (g.fight.stage != BossFight::Stage::Desperate) return false;
        markF = b->hp;
        g.hitEnemy(*b, 1, WHITE);
        check("at low health it charges the big one, and hits go to the core", b->hp == markF && g.fight.coreHp < g.fight.coreMax);
        shot(g, "boss-core");
        return true;
    });
    add("boss core breaks", 3, [this](Game& g, float t) {
        g.player.invuln = 1;
        Enemy* b = boss(g);
        if (!b || t < 0.5f) return false;
        g.hitEnemy(*b, 100000, WHITE);
        check("hit the core in time and the boss reels", g.fight.stage == BossFight::Stage::Last && g.fight.stunT > 0);
        return true;
    });
    add("boss final hit", 4, [this](Game& g, float t) {
        if (t == 0) { slayBosses(g); return false; }
        if (g.finaleT <= 0) return false;
        check("the last hit freezes, slows and breaks the boss up", g.hitStop > 0 && g.slowMo > 0 && !g.rubble.empty());
        shot(g, "boss-final-hit");
        return true;
    });
    add("boss crumbles", 3, [this](Game& g, float t) {
        if (t < 1.2f) return false;
        shot(g, "boss-crumbles");
        return true;
    });
    add("boss defeated", 5, [this](Game& g, float) {
        if (!g.dungeon.cur().cleared) return false;
        check("BOSS DEFEATED", g.bannerText == "BOSS DEFEATED" && g.rubble.empty());
        shot(g, "boss-defeated");
        return true;
    });

    // ===== and every boss goes through it all =====
    for (int i = 0; i < BOSS_COUNT; ++i)
        add(TextFormat("stages of boss %d", i + 1), 25, [this, i](Game& g, float t) {
            using Stage = BossFight::Stage;
            if (t == 0) {
                g.startRun();
                g.floor = i + 1;
                goToRoom(g, [](const Room& r) { return r.isBoss; });
                g.player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 3.0f };   // off the stairs, or the win walks straight down them
                markB = true;
                return false;
            }
            g.player.invuln = 1;
            g.enemyShots.clear();
            BossFight& f = g.fight;
            Enemy* b = boss(g);
            if (!b) slayBosses(g);
            else if (f.stage == Stage::One) b->hp = b->maxHp * 0.49f;
            else if (f.stage == Stage::Two) { markB &= f.rockCount == BossFight::ROCKS; b->hp = b->maxHp * 0.19f; }
            else if (f.stage == Stage::Desperate) { if (i % 2) f.stageT = 0.01f; else g.hitEnemy(*b, 100000, WHITE); }
            else if (f.stage == Stage::Last) { if (i % 2) markB &= f.blast > 0; slayBosses(g); }
            if (!g.dungeon.cur().cleared) return false;
            check(TextFormat("%s goes through every stage", BOSSES[i].name), markB && g.bannerText == "BOSS DEFEATED");
            markB = false;
            return true;
        });
    add("bosses done", 1, [this](Game&, float) { keepAlive = false; return true; });
}
