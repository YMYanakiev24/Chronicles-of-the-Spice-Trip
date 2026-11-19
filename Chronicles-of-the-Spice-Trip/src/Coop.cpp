// ============================================================================
//  Coop.cpp — co-op (see Coop.h): several heroes in one game, the session
//  between the devices, the host's frame, and the lobby.
//
//  CoopSync.cpp has what goes over the wire every frame: the host's snapshots
//  and how a joiner plays from them.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Render.h"
#include "Touch.h"
#include "Ui.h"
#include "Wire.h"
#include <algorithm>
#include <cmath>

// ------------------------------------------------------------------ heroes
void Game::saveHeroTo(Hero& h) const {
    h.player = player; h.overlay = overlay; h.hall = hall; h.wheel = wheel; h.castleDoorCd = castleDoorCd;
    h.dialogue = dialogue; h.dialogueIdx = dialogueIdx;
    h.gymT = gymT; h.gymDir = gymDir; h.cigT = cigT; h.stubT = stubT;
    h.grindStep = grindStep; h.throwT = throwT; h.throwKind = throwKind; h.thrown = thrown;
    h.punchT = punchT; h.punchLanded = punchLanded; h.punchDir = punchDir;
    h.slingT = slingT; h.slingFired = slingFired; h.castingSpell = castingSpell; h.castT = castT;
    h.heroMoving = heroMoving; h.heroMoveDir = heroMoveDir; h.shopMsgT = shopMsgT; h.shopMsg = shopMsg;
    h.heroStepping = heroStepping; h.heroStride = heroStride; h.climbT = climbT; h.climbFrom = climbFrom;
    h.outfit = heroOutfit; h.name = heroName; h.downT = heroDownT;
}

void Game::loadHeroFrom(const Hero& h) {
    player = h.player; overlay = h.overlay; hall = h.hall; wheel = h.wheel; castleDoorCd = h.castleDoorCd;
    dialogue = h.dialogue; dialogueIdx = h.dialogueIdx;
    gymT = h.gymT; gymDir = h.gymDir; cigT = h.cigT; stubT = h.stubT;
    grindStep = h.grindStep; throwT = h.throwT; throwKind = h.throwKind; thrown = h.thrown;
    punchT = h.punchT; punchLanded = h.punchLanded; punchDir = h.punchDir;
    slingT = h.slingT; slingFired = h.slingFired; castingSpell = h.castingSpell; castT = h.castT;
    heroMoving = h.heroMoving; heroMoveDir = h.heroMoveDir; shopMsgT = h.shopMsgT; shopMsg = h.shopMsg;
    heroStepping = h.heroStepping; heroStride = h.heroStride; climbT = h.climbT; climbFrom = h.climbFrom;
    heroOutfit = h.outfit; heroName = h.name; heroDownT = h.downT;
}

void Game::useHero(int i) {
    if (i == curHero || i < 0 || i >= heroCount) return;
    saveHeroTo(heroes[curHero]);
    loadHeroFrom(heroes[i]);
    curHero = i;
}

void Game::syncHeroes() { saveHeroTo(heroes[curHero]); }

bool Game::heroIn(int i) const {
    if (i < 0 || i >= heroCount) return false;
    if (!coop.active()) return i == curHero;
    if (i == curHero) return heroes[i].present && !hall.active && player.hp > 0;
    const Hero& h = heroes[i];
    return h.present && !h.hall.active && h.player.hp > 0;
}

int Game::nearestHero(Vector2 p) const {
    int best = -1;
    float bestD = 1e30f;
    for (int h = 0; h < heroCount; ++h) {
        if (!heroIn(h)) continue;
        float d = Vector2DistanceSqr(p, h == curHero ? player.pos : heroes[h].player.pos);
        if (d < bestD) { bestD = d; best = h; }
    }
    return best;
}

// Where each seat stands when the party arrives somewhere together.
static Vector2 seatSpot(int seat) {
    static const Vector2 OFF[MAX_HEROES] = { { 0, 0 }, { -30, 0 }, { 30, 0 }, { 0, 30 } };
    return OFF[std::clamp(seat, 0, MAX_HEROES - 1)];
}

// The hero in the fields gets back up.
static void getUp(Game& g, float hpShare) {
    g.player.hp = fmaxf(1, g.player.maxHp * hpShare);
    g.player.invuln = 1.5f;
    g.heroDownT = 0;
    for (int k = 0; k < 14; ++k) {
        float life = frand(0.5f, 0.9f);
        g.parts.push_back({ { g.player.pos.x + frand(-10, 10), g.player.pos.y - frand(0, 20) }, { frand(-30, 30), frand(-90, -40) },
            life, life, frand(2, 3.5f), { 255, 236, 150, 255 }, 0, true, true });
    }
}

// ------------------------------------------------------------------ the session
static void resetSession(CoopSession& c) {
    c.role = CoopRole::Off;
    c.started = false;
    c.status.clear();
    for (int s = 0; s < MAX_HEROES; ++s) {
        c.links[s].close();
        c.welcomed[s] = false; c.inputs[s] = {}; c.tappedSince[s] = 0; c.inputAt[s] = 0; c.dungeonSent[s] = 0;
    }
    c.server.stop();
    c.finder.stop();
    c.host.close();
    c.sounds.clear();
    c.cutscenes = c.cutscenesSeen = 0;
    c.lastSent = 0;
    c.mySeat = 0; c.seated = false; c.helloSent = false; c.haveWorld = false;
    c.snapshot.clear(); c.snapshotWaiting = false;
    c.closed = c.lastHostOverlay = Overlay::None; c.closedAt = 0;
    c.mood.clear();
}

void Game::coopHost() {
    if (coop.active()) return;
    resetSession(coop);
    if (!net::available()) { coop.status = "Co-op needs the Windows or Android app."; return; }
    if (!coop.server.start()) { coop.status = "Couldn't open the game port. Is this device already hosting?"; return; }
    coop.role = CoopRole::Host;
    coop.difficulty = difficulty;
    heroCount = MAX_HEROES; curHero = localHero = 0;
    for (Hero& h : heroes) h = Hero{};
    heroes[0].present = true; heroes[0].name = heroName = "P1";
    heroes[0].player.champion = profile.champion;
    heroOutfit = -1;
}

void Game::coopJoin(const std::string& address) {
    if (coop.active() || address.empty()) return;
    resetSession(coop);
    if (!net::available()) { coop.status = "Co-op needs the Windows or Android app."; return; }
    coop.role = CoopRole::Joiner;
    coop.address = address;
    heroCount = MAX_HEROES; curHero = localHero = 0;
    for (Hero& h : heroes) h = Hero{};
    if (!coop.host.connect(address)) { coopLeave(TextFormat("Couldn't reach %s", address.c_str())); return; }
    coop.status = TextFormat("Joining %s...", address.c_str());
}

void Game::coopLeave(const char* why) {
    if (!coop.active()) return;
    if (coop.hosting()) {
        for (int s = 1; s < MAX_HEROES; ++s)
            if (coop.links[s].connected()) { coop.links[s].send(MSG_BYE, nullptr, 0); coop.links[s].update(); }
    }
    else if (coop.host.connected()) { coop.host.send(MSG_BYE, nullptr, 0); coop.host.update(); }
    std::string address = coop.address;
    resetSession(coop);
    coop.address = address;
    // back to one hero, this device's
    heroCount = 1; curHero = localHero = 0;
    heroOutfit = -1; heroName.clear(); heroDownT = 0;
    for (Hero& h : heroes) h = Hero{};
    hostOverlay = localOverlay = Overlay::None;
    bool playing = screen == Screen::Play;
    if (playing) toMenu();
    coop.status = why ? why : "";
    if (why || !playing) { screen = Screen::Coop; t = 0; }
}

void Game::coopSendLobby() {
    if (!coop.hosting()) return;
    heroes[0].player.champion = profile.champion;
    std::vector<uint8_t> b;
    wire::Writer w(b);
    w.u8(coop.difficulty);
    for (int s = 0; s < MAX_HEROES; ++s) {
        const Hero& h = heroes[s];
        int outfit = s == 0 ? profile.outfit[profile.champion] : h.outfit;
        std::string name = h.name;
        w.u8(h.present); w.u8(h.player.champion); w.u8((uint8_t)(int8_t)outfit); w.text(name);
    }
    for (int s = 1; s < MAX_HEROES; ++s)
        if (coop.welcomed[s] && coop.links[s].connected()) coop.links[s].send(MSG_LOBBY, b);
}

void Game::coopDropSeat(int seat, const char* why) {
    if (seat <= 0 || seat >= MAX_HEROES) return;
    bool was = heroes[seat].present;
    coop.links[seat].close();
    coop.welcomed[seat] = false; coop.inputs[seat] = {}; coop.tappedSince[seat] = 0; coop.dungeonSent[seat] = 0;
    if (curHero == seat) useHero(localHero);
    std::string name = heroes[seat].name;
    heroes[seat].present = false;
    heroes[seat].hall.active = false;
    if (!was) return;
    if (coop.started) showBanner(TextFormat("%s %s", name.c_str(), why), "", 2.0f);
    coopSendLobby();
}

// --------------------------------------------------------------- once a frame
void Game::coopNetwork() {
    net::Message m;
    if (coop.hosting()) {
        coop.server.update();
        int players = 0;
        for (const Hero& h : heroes) players += h.present;
        coop.server.advertise(TextFormat("%s's game", CHAMPIONS[profile.champion].name), players, MAX_HEROES,
            !coop.started && players < MAX_HEROES);
        for (int s = 1; s < MAX_HEROES; ++s)       // gone: their seat is free again
            if (!coop.links[s].connected() && !coop.links[s].connecting() && (coop.welcomed[s] || heroes[s].present))
                coopDropSeat(s, "left the game");
        // someone knocking: the first free seat, or turned away
        for (;;) {
            int free = -1;
            for (int s = 1; s < MAX_HEROES && free < 0; ++s)
                if (!coop.links[s].connected() && !coop.links[s].connecting()) free = s;
            static net::Link spare;
            net::Link& into = free >= 0 ? coop.links[free] : spare;
            if (free >= 0) { into.close(); coop.welcomed[free] = false; }
            if (!coop.server.accept(into)) break;
            if (free < 0) {
                std::string why = "That game is full.";
                std::vector<uint8_t> b; wire::Writer w(b); w.text(why);
                spare.send(MSG_REFUSE, b); spare.update(); spare.close();
            }
        }
        for (int s = 1; s < MAX_HEROES; ++s) {
            net::Link& L = coop.links[s];
            if (!L.connected()) continue;
            L.update();
            while (L.receive(m)) {
                wire::Reader r(m.data);
                switch (m.type) {
                case MSG_HELLO: {
                    int version = r.u16();
                    uint32_t layout = 0; r(layout);
                    int champion = r.u8(), outfit = (int8_t)r.u8();
                    const char* refuse = nullptr;
                    if (!r.ok || version != COOP_VERSION || layout != coopLayout())
                        refuse = "That game is on another version of Spice Trip. Update both to the same one.";
                    else if (coop.started) refuse = "That game's run has already begun.";
                    if (refuse) {
                        std::string why = refuse;
                        std::vector<uint8_t> b; wire::Writer w(b); w.text(why);
                        L.send(MSG_REFUSE, b);
                        break;
                    }
                    Hero fresh;
                    fresh.present = true;
                    fresh.name = TextFormat("P%d", s + 1);
                    fresh.player.champion = std::clamp(champion, 0, CHAMPION_COUNT - 1);
                    fresh.outfit = std::clamp(outfit, 0, OUTFIT_COUNT - 1);
                    heroes[s] = fresh;
                    coop.welcomed[s] = true;
                    uint8_t seat = (uint8_t)s;
                    L.send(MSG_WELCOME, &seat, 1);
                    coopSendLobby();
                    play("quest");
                    break;
                }
                case MSG_PICK: {
                    int champion = r.u8(), outfit = (int8_t)r.u8();
                    if (!r.ok || !coop.welcomed[s] || coop.started) break;
                    heroes[s].player.champion = std::clamp(champion, 0, CHAMPION_COUNT - 1);
                    heroes[s].outfit = std::clamp(outfit, 0, OUTFIT_COUNT - 1);
                    coopSendLobby();
                    break;
                }
                case MSG_INPUT: {
                    NetInput in;
                    r(in.held, in.tapped, in.aim);
                    if (!r.ok) break;
                    coop.inputs[s] = in;
                    coop.tappedSince[s] |= in.tapped;
                    coop.inputAt[s] = net::now();
                    break;
                }
                case MSG_CMD:
                    if (coop.started && heroes[s].present) coopCommand(s, m.data);
                    break;
                case MSG_BYE:
                    coopDropSeat(s, "left the game");
                    break;
                default: break;
                }
                if (!L.connected()) break;
            }
        }
        return;
    }

    if (coop.joined()) {
        coop.host.update();
        if (coop.host.connected() && !coop.helloSent) {
            std::vector<uint8_t> b; wire::Writer w(b);
            uint32_t layout = coopLayout();
            w.u16(COOP_VERSION); w(layout);
            w.u8(profile.champion); w.u8((uint8_t)(int8_t)profile.outfit[profile.champion]);
            coop.host.send(MSG_HELLO, b);
            coop.helloSent = true;
            coop.status = "Asking to join...";
        }
        while (coop.host.receive(m)) {
            wire::Reader r(m.data);
            switch (m.type) {
            case MSG_WELCOME:
                coop.mySeat = std::clamp(r.u8(), 1, MAX_HEROES - 1);
                coop.seated = true;
                curHero = localHero = coop.mySeat;
                heroName = TextFormat("P%d", coop.mySeat + 1);
                coop.status.clear();
                play("quest");
                break;
            case MSG_REFUSE: {
                std::string why; r.text(why);
                coopLeave(why.empty() ? "The host turned us away." : why.c_str());
                return;
            }
            case MSG_LOBBY: {
                if (coop.started) break;            // in a run, the snapshots say who's there
                coop.difficulty = std::clamp(r.u8(), 0, 2);
                for (int s = 0; s < MAX_HEROES && r.ok; ++s) {
                    Hero& h = heroes[s];
                    h.present = r.u8() != 0;
                    h.player.champion = std::clamp(r.u8(), 0, CHAMPION_COUNT - 1);
                    h.outfit = std::clamp((int)(int8_t)r.u8(), 0, OUTFIT_COUNT - 1);
                    r.text(h.name);
                }
                break;
            }
            case MSG_START:
                coop.started = true;
                coop.haveWorld = false; coop.snapshotWaiting = false;
                coop.cutscenesSeen = 0;
                coop.closed = coop.lastHostOverlay = Overlay::None;
                hostOverlay = localOverlay = overlay = Overlay::None;
                hall = castle::Hall{}; hunt = toilet::Level{};
                mode = Mode::Run; hardcore = false;
                screen = Screen::Play; t = 0;
                break;
            case MSG_DUNGEON:
                applyDungeon(m.data);
                break;
            case MSG_SNAPSHOT:
                coop.snapshot.swap(m.data);
                coop.snapshotWaiting = true;
                break;
            case MSG_BYE:
                coopLeave("The host left the game.");
                return;
            default: break;
            }
        }
        if (coop.host.failed()) coopLeave(coop.seated ? "Lost the host." : TextFormat("Couldn't reach %s", coop.address.c_str()));
    }
}

// --------------------------------------------------------------------- a run
void Game::coopStartRun() {
    if (!coop.hosting() || coop.started) return;
    difficulty = coop.difficulty; hardcore = false; mode = Mode::Run;
    heroCount = MAX_HEROES; curHero = localHero = 0;
    for (int s = 1; s < MAX_HEROES; ++s) {
        Hero& h = heroes[s];
        if (!h.present) { h = Hero{}; continue; }
        Hero fresh;
        fresh.present = true; fresh.name = h.name; fresh.outfit = h.outfit;
        fresh.player = freshPlayer(h.player.champion, 1);
        fresh.player.pos = Vector2Add(ROOM_CENTER, seatSpot(s));
        h = fresh;
    }
    heroes[0].present = true; heroName = "P1"; heroOutfit = -1; heroDownT = 0;
    coop.started = true;
    coop.cutscenes = 0; coop.sounds.clear(); coop.lastSent = 0;
    for (int s = 0; s < MAX_HEROES; ++s) { coop.dungeonSent[s] = 0; coop.inputs[s] = {}; coop.tappedSince[s] = 0; }
    startRun();                                     // this device's hero, the floor, the first room
    for (int s = 1; s < MAX_HEROES; ++s)
        if (coop.welcomed[s] && coop.links[s].connected()) {
            uint8_t seat = (uint8_t)s;
            coop.links[s].send(MSG_START, &seat, 1);
        }
    showBanner("The Mistwood Depths", TextFormat("%d heroes, one run. %s waits at the bottom of floor 1",
        [&] { int n = 0; for (const Hero& h : heroes) n += h.present; return n; }(), currentBoss().name), 3.0f);
}

// The last boss of the cycle has fallen: everyone sees it, then the stairs lead on.
void Game::coopRunOver() {
    int me = curHero;
    for (int h = 0; h < heroCount; ++h) {
        if (!heroes[h].present && h != curHero) continue;
        useHero(h);
        if (overlay == Overlay::None) overlay = Overlay::RunComplete;
    }
    useHero(me);
}

void Game::coopReviveDowned(float hpShare) {
    int me = curHero;
    for (int h = 0; h < heroCount; ++h) {
        if (!heroes[h].present && h != curHero) continue;
        useHero(h);
        if (!hall.active && player.hp <= 0) getUp(*this, hpShare);
    }
    useHero(me);
}

void Game::coopPartyMoved(int leader) {
    int me = curHero;
    useHero(leader);
    Vector2 at = player.pos;
    Room& room = dungeon.cur();
    for (int h = 0; h < heroCount; ++h) {
        if (h == leader || (!heroes[h].present && h != curHero)) continue;
        useHero(h);
        if (hall.active) continue;                  // in the castle: they'll come out beside someone
        Vector2 off = Vector2Scale(seatSpot(h), 0.5f);
        player.pos = moveInRoom(at, off, PLAYER_R, room, true);
        player.roll = 0; player.dashT = 0; player.rollAtkT = 0; player.jumpT = 0;
        dialogue.clear();
    }
    useHero(me);
}

void Game::coopPartyFell() {
    int me = curHero;
    int lostAll = 0;
    for (int h = 0; h < heroCount; ++h) {
        if (!heroes[h].present && h != curHero) continue;
        useHero(h);
        if (hall.active) continue;
        int lost = player.gold < 10 ? player.gold : std::max(10, player.gold * 3 / 10);   // Toshko goes through every pocket
        player.gold -= lost; stats.goldLost += lost; lostAll += lost;
        player.hp = player.maxHp; player.mana = player.maxMana;
        player.pos = Vector2Add(ROOM_CENTER, seatSpot(h));
        player.invuln = 1.5f;
        heroDownT = 0;
        castT = 0; cigT = 0; dialogue.clear();
    }
    useHero(me);
    stats.deaths++;
    dungeon.cx = DUNGEON_G / 2; dungeon.cy = DUNGEON_G / 2;
    enterRoom();
    play("death");
    showBanner("The party falls...", lostAll > 0 ? TextFormat("Toshko went through everyone's pockets: -%d gold", lostAll) : "back to the entrance", 2.8f);
}

void Game::coopRemoteKeys(int seat) {
    in::Remote& r = coop.remote;
    r = in::Remote{};
    const NetInput& ni = coop.inputs[seat];
    uint32_t held = net::now() - coop.inputAt[seat] < 0.5 ? ni.held : 0;     // a joiner gone quiet stops walking
    uint32_t tap = coop.tappedSince[seat];
    const Keybinds& K = settings.keys;
    auto key = [&](int k, uint32_t bit) {
        if (k <= 0 || k >= in::Remote::KEYS) return;
        r.held[k] = r.held[k] || (held & bit);
        r.tapped[k] = r.tapped[k] || (tap & bit);
    };
    key(K.up, NA_UP); key(K.down, NA_DOWN); key(K.left, NA_LEFT); key(K.right, NA_RIGHT); key(K.run, NA_RUN);
    key(K.roll, NA_ROLL); key(K.special, NA_SPECIAL); key(K.interact, NA_INTERACT);
    key(K.potionHp, NA_POTION_HP); key(K.potionMana, NA_POTION_MANA);
    for (int i = 0; i < SPELL_COUNT; ++i) key(KEY_ONE + i, NA_SPELL1 << i);
    key(KEY_ENTER, NA_CAST_HOLD);
    r.mouseHeld[MOUSE_BUTTON_LEFT] = held & NA_BLADE;   r.mouseTapped[MOUSE_BUTTON_LEFT] = tap & NA_BLADE;
    r.mouseHeld[MOUSE_BUTTON_RIGHT] = held & NA_CAST;   r.mouseTapped[MOUSE_BUTTON_RIGHT] = tap & NA_CAST;
    r.mouse = ni.aim;
}

// The host's frame: every hero with their own keys, then the world they share.
void Game::coopStep(float dt) {
    dt = updateWorldTimers(dt);
    for (int h = 0; h < heroCount; ++h) {
        if (!heroes[h].present && h != curHero) continue;
        useHero(h);
        bool remote = h != localHero;
        if (remote) {
            coopRemoteKeys(h);
            in::setRemote(&coop.remote);
            shopMsgT = fmaxf(0, shopMsgT - dt);     // (this device's own shop counts its message down as it draws)
        }
        if (!hall.active && player.hp <= 0) {      // knocked out
            if (heroDownT <= 0) {
                player.hp = 0;
                castT = 0; cigT = 0; gymT = 0; throwT = 0; punchT = 0; slingT = 0;
                player.roll = 0; player.dashT = 0; player.rollAtkT = 0; player.jumpT = 0;
                dialogue.clear();
                stats.deaths++;
                showBanner(TextFormat("%s is down", heroName.c_str()), "Clear the room and they get back up", 2.2f);
                play("death");
                heroDownT = 0.001f;
            }
            heroDownT += dt;
            if (!inFight && heroDownT > 1.5f) getUp(*this, 0.4f);   // nothing left to fight: up again
        }
        else if (hall.active) {
            if (overlay == Overlay::None) updateCastle(dt);
            else if (remote && overlay == Overlay::Casino) updateWheel(dt);   // (this device's own wheel turns as it's drawn)
        }
        else if (overlay == Overlay::None) {
            size_t fired = shots.size();
            updateHero(dt);
            for (size_t i = fired; i < shots.size(); ++i) if (shots[i].owner < 0) shots[i].owner = h;   // their hits are theirs
        }
        in::setRemote(nullptr);
        if (remote) coop.tappedSince[h] = 0;
        if (screen != Screen::Play) { useHero(localHero); return; }
    }
    useHero(localHero);
    updateWorld(dt);
    if (screen != Screen::Play) return;

    // everyone in the dungeon is down: the party falls back to the entrance
    syncHeroes();
    bool anyUp = false, anyDown = false;
    for (int h = 0; h < heroCount; ++h) {
        const Hero& H = heroes[h];
        if (!H.present || H.hall.active) continue;
        (H.player.hp > 0 ? anyUp : anyDown) = true;
    }
    if (anyDown && !anyUp) coopPartyFell();
    coopSendSnapshots();
}

// A joiner's shop or window asked for something for their hero.
void Game::coopCommand(int seat, const std::vector<uint8_t>& data) {
    wire::Reader r(data);
    int c = r.u8();
    int a = r.i32(), b = r.i32();
    if (!r.ok) return;
    int me = curHero;
    useHero(seat);
    switch ((Cmd)c) {
    case Cmd::CloseOverlay:
        if (overlay == Overlay::Shop || overlay == Overlay::Galin || overlay == Overlay::Drip || overlay == Overlay::RunComplete
            || (overlay == Overlay::Casino && !wheel.spinning)) overlay = Overlay::None;
        break;
    case Cmd::Buy:
        if (overlay == Overlay::Shop && a >= 0 && a < SHOP_COUNT) buy(a);
        break;
    case Cmd::BuyWeapon:
        if (overlay == Overlay::Galin && a >= 0 && a < WEAPON_COUNT) buyWeapon(a);
        break;
    case Cmd::SellWeapon:
        if (overlay == Overlay::Galin && a >= 0 && a < WEAPON_COUNT) sellWeapon(a);
        break;
    case Cmd::BuyUpgrade:
        if (overlay == Overlay::Galin && a >= 0 && a < UPGRADE_COUNT) buyUpgrade(a);
        break;
    case Cmd::Spin:
        if (overlay == Overlay::Casino && !wheel.spinning) {
            for (int bet : CASINO_BETS) if (bet == a) wheel.bet = a;
            wheel.pick = std::clamp(b, 0, 2);
            spinWheel();
        }
        break;
    case Cmd::BuyOutfit: {
        if (overlay != Overlay::Drip || a < 0 || a >= CHAMPION_COUNT || b <= 0 || b >= OUTFIT_COUNT) break;
        const OutfitDef& o = OUTFITS[a][b];
        if (player.gold < o.price) { shopMsg = "Not enough gold. Beray doesn't do credit."; shopMsgT = 2.0f; play("hurt"); break; }
        player.gold -= o.price;
        if (a == player.champion) heroOutfit = b;
        shopMsg = TextFormat("%s, fresh in the %s.", CHAMPIONS[a].name, o.name); shopMsgT = 2.4f;
        play("coin");
        break;
    }
    case Cmd::Wear:
        if (a == player.champion && b >= 0 && b < OUTFIT_COUNT) heroOutfit = b;
        break;
    }
    useHero(me);
}

// ------------------------------------------------------------------ the lobby
// The keys that make up an address, for phones (and anyone with a mouse in hand).
static const char* const KEYPAD[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "<" };

void Game::coopScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    drawMenuBackground();
    ui::veil(0.55f);
    float pw = fminf(1100.0f, sw - 40.0f), ph = fminf(660.0f, sh - 30.0f);
    Rectangle p = { (sw - pw) / 2, (sh - ph) / 2, pw, ph };
    ui::titledPanel(p, "Co-op");
    std::string mine = net::localAddress();

    auto back = [&]() {
        if (coop.active()) coopLeave();
        coop.finder.stop();
        screen = Screen::Menu; t = 0;
    };
    if (in::pressed(KEY_ESCAPE)) { play("ui"); back(); return; }
    if (ui::button({ p.x + pw - 50, p.y + 16, 32, 32 }, "X")) { play("ui"); back(); return; }

    float x = p.x + 30, y = p.y + 76;
    Color statusColor = ui::BAD;

    // ---------------------------------------------------------------- not in a game: host one, or find one
    if (!coop.active()) {
        if (!net::available()) {
            ui::textWrapped("Co-op is played between the Windows and Android apps, on the same Wi-Fi. The browser version can't join.",
                x, y, pw - 60, 20, ui::TEXT);
            return;
        }
        if (!coop.finder.listening()) coop.finder.start();
        coop.finder.update();

        float colW = (pw - 90) / 2;
        ui::textWrapped("Play one run together on the same Wi-Fi: up to four heroes, the same rooms, monsters and bosses. One device hosts; the others join it.",
            x, y, colW, 10, ui::MUTED);
        if (ui::button({ x, y + 50, colW, 54 }, "Host a game", ui::Style::Primary)) {
            play("ui"); coop.finder.stop(); coopHost();
            if (coop.hosting()) coopSendLobby();
            return;
        }
        ui::text(mine.empty() ? "Not on a network" : TextFormat("This device: %s", mine.c_str()), x, y + 116, 10, ui::MUTED);

        // games found on the network
        float ly = y + 150;
        ui::text("Games on this network", x, ly, 20, ui::ACCENT);
        ly += 30;
        const auto& games = coop.finder.games();
        if (games.empty()) ui::text(TextFormat("Looking%s", &"..."[2 - (int)fmodf(clock * 2, 3)]), x, ly + 6, 20, ui::MUTED);
        for (size_t i = 0; i < games.size() && i < 5; ++i) {
            const net::Found& g = games[i];
            Rectangle row = { x, ly, colW, 50 };
            DrawRectangleRec(row, ui::PANEL_LIGHT);
            DrawRectangleLinesEx(row, 1, ui::EDGE);
            ui::text(g.name.c_str(), row.x + 10, row.y + 6, 20, ui::TEXT);
            ui::text(TextFormat("%s   %d / %d players%s", g.address.c_str(), g.players, g.max, g.open ? "" : "   (playing)"),
                row.x + 10, row.y + 32, 10, ui::MUTED);
            if (ui::button({ row.x + colW - 90, row.y + 8, 80, 34 }, "Join", g.open ? ui::Style::Good : ui::Style::Normal, 20, g.open)) {
                play("ui"); coop.finder.stop(); coopJoin(g.address);
                return;
            }
            ly += 58;
        }

        // or by its address (shown on the host's screen)
        float rx = x + colW + 30;
        ui::text("Join by address", rx, y, 20, ui::ACCENT);
        ui::textWrapped("If the game doesn't show up, type the address its host shows.", rx, y + 26, colW, 10, ui::MUTED);
        Rectangle field = { rx, y + 50, colW - 100, 44 };
        DrawRectangleRec(field, ui::INK);
        DrawRectangleLinesEx(field, 2, ui::EDGE_LIGHT);
        bool caret = fmodf(clock, 1.0f) < 0.5f;
        ui::text(TextFormat("%s%s", coop.address.c_str(), caret ? "_" : ""), field.x + 10, field.y + 12, 20, ui::TEXT);
        for (int ch = GetCharPressed(); ch; ch = GetCharPressed())
            if (((ch >= '0' && ch <= '9') || ch == '.' || ch == ':') && coop.address.size() < 21) coop.address += (char)ch;
        if ((in::pressed(KEY_BACKSPACE)) && !coop.address.empty()) coop.address.pop_back();
        bool canJoin = coop.address.size() >= 7;
        if (ui::button({ rx + colW - 90, y + 50, 90, 44 }, "Join", ui::Style::Good, 20, canJoin) || (canJoin && in::pressed(KEY_ENTER))) {
            play("ui"); coop.finder.stop(); coopJoin(coop.address);
            return;
        }
        float kw = (colW - 2 * 8) / 3, kh = 40, ky = y + 106;
        for (int k = 0; k < 12; ++k) {
            Rectangle key = { rx + (k % 3) * (kw + 8), ky + (k / 3) * (kh + 8), kw, kh };
            if (ui::button(key, KEYPAD[k], ui::Style::Normal, 20)) {
                if (k == 11) { if (!coop.address.empty()) coop.address.pop_back(); }
                else if (coop.address.size() < 21) coop.address += KEYPAD[k];
            }
        }
        ui::textWrapped("Both devices need to be on the same Wi-Fi. On Windows, allow Spice Trip through the firewall when it asks.",
            rx, ky + 4 * (kh + 8) + 8, colW, 10, ui::MUTED);
        if (!coop.status.empty()) ui::textWrapped(coop.status.c_str(), x, p.y + ph - 40, pw - 60, 20, statusColor);
        return;
    }

    // ---------------------------------------------------------------- joining, not in yet
    if (coop.joined() && !coop.seated) {
        ui::textCentered(coop.status.c_str(), p.x + pw / 2, p.y + ph / 2 - 40, 20, ui::TEXT);
        if (ui::button({ p.x + pw / 2 - 100, p.y + ph / 2 + 10, 200, 50 }, "Cancel")) { play("ui"); coopLeave(); }
        return;
    }

    // ---------------------------------------------------------------- the lobby: who's in, on whom
    bool host = coop.hosting();
    if (host) {
        heroes[0].player.champion = profile.champion;
        ui::text(mine.empty() ? "Hosting" : TextFormat("Hosting on %s", mine.c_str()), x, y, 20, ui::ACCENT);
        ui::text("Others on this Wi-Fi find it under Co-op, or join by that address.", x, y + 28, 10, ui::MUTED);
    }
    else {
        ui::text(TextFormat("In the game at %s", coop.address.c_str()), x, y, 20, ui::ACCENT);
        ui::text("Waiting for the host to begin the run...", x, y + 28, 10, ui::MUTED);
    }
    float cardW = (pw - 60 - 3 * 14) / 4, cardH = fminf(250.0f, ph - 300), cy = y + 56;
    for (int s = 0; s < MAX_HEROES; ++s) {
        const Hero& h = heroes[s];
        Rectangle c = { x + s * (cardW + 14), cy, cardW, cardH };
        bool you = s == localHero;
        DrawRectangleRec(c, h.present ? ui::PANEL_LIGHT : Fade(ui::PANEL_LIGHT, 0.4f));
        DrawRectangleLinesEx(c, you ? 2.0f : 1.0f, you ? ui::ACCENT : ui::EDGE);
        ui::text(TextFormat("P%d%s", s + 1, you ? "  (you)" : s == 0 ? "  (host)" : ""), c.x + 10, c.y + 8, 20, you ? ui::ACCENT : ui::TEXT);
        if (!h.present) { ui::textCentered("open seat", c.x + cardW / 2, c.y + cardH / 2 - 5, 10, ui::MUTED); continue; }
        int champion = you ? profile.champion : h.player.champion;
        int outfit = you ? profile.outfit[champion] : h.outfit;
        drawFigureIn(outfitLook(champion, std::max(0, outfit)), FACE_DOWN, 0, { c.x + 20, c.y + 38, cardW - 40, cardH - 80 });
        drawName(champion, c.x + cardW / 2, c.y + cardH - 30, 10, ui::ACCENT, true);
    }

    // your champion
    float by = cy + cardH + 20;
    ui::text("Your champion", x, by + 12, 20, ui::MUTED);
    auto pick = [&](int dir) {
        profile.champion = (profile.champion + dir + CHAMPION_COUNT) % CHAMPION_COUNT;
        saveProfile();
        play("ui");
        if (host) coopSendLobby();
        else {
            uint8_t b[2] = { (uint8_t)profile.champion, (uint8_t)(int8_t)profile.outfit[profile.champion] };
            coop.host.send(MSG_PICK, b, 2);
        }
    };
    if (ui::button({ x + 170, by, 44, 44 }, "<")) pick(-1);
    drawName(profile.champion, x + 330, by + 12, 20, ui::TEXT, true);
    if (ui::button({ x + 446, by, 44, 44 }, ">")) pick(1);

    // the difficulty (the host's to choose)
    ui::text("Difficulty", x, by + 64, 20, ui::MUTED);
    if (host) {
        for (int d = 0; d < 3; ++d)
            if (ui::button({ x + 170 + d * 110, by + 54, 100, 40 }, DIFFICULTIES[d].name, coop.difficulty == d ? ui::Style::Good : ui::Style::Normal, 20)) {
                coop.difficulty = d; play("ui"); coopSendLobby();
            }
    }
    else ui::text(DIFFICULTIES[coop.difficulty].name, x + 170, by + 64, 20, ui::TEXT);

    float bw = 220;
    if (host && ui::button({ p.x + pw - 30 - bw, p.y + ph - 76, bw, 52 }, "Begin the run", ui::Style::Primary)) {
        play("ui"); coopStartRun();
        return;
    }
    if (ui::button({ p.x + pw - 30 - bw - (host ? bw + 14 : 0), p.y + ph - 76, bw, 52 }, "Leave", ui::Style::Danger)) {
        play("ui"); coopLeave();
        return;
    }
    if (!coop.status.empty()) ui::text(coop.status.c_str(), x, p.y + ph - 60, 20, statusColor);
}
