// ============================================================================
//  CoopSync.cpp — co-op over the wire (see Coop.h): the host's snapshots of
//  the world and every hero, and a joiner playing from them. Also how the
//  others look: their heroes in the room and their health by the HUD.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Render.h"
#include "Touch.h"
#include "Ui.h"
#include "Wire.h"
#include <algorithm>
#include <cmath>

static_assert(std::is_trivially_copyable_v<Dungeon>, "the floor goes over the wire whole");

// Everything sent whole, by size: a joiner on a build that lays any of it out
// differently is turned away (Coop.cpp) instead of reading it wrong.
uint32_t coopLayout() {
    uint32_t h = 2166136261u;
    auto mix = [&](size_t v) { h ^= (uint32_t)v; h *= 16777619u; };
    for (size_t v : { sizeof(Vector2), sizeof(Color), sizeof(Rectangle), sizeof(bool), sizeof(Overlay), sizeof(ShotKind),
                      sizeof(Flash), sizeof(Arc), sizeof(FloorCrack), sizeof(IceSpike), sizeof(Laser), sizeof(Coin), sizeof(FloatText),
                      sizeof(castle::Hall), sizeof(Bus), sizeof(Grenade), sizeof(Circuit), sizeof(Staff), sizeof(Rock), sizeof(Beam),
                      sizeof(Slam), sizeof(Room), sizeof(Dungeon), sizeof(BossFight::Stage),
                      alignof(Room), alignof(Dungeon), alignof(Flash), alignof(Grenade) })
        mix(v);
    for (int v : { (int)COOP_VERSION, (int)WEAPON_COUNT, (int)UPGRADE_COUNT, (int)CHAMPION_COUNT, (int)OUTFIT_COUNT, (int)BOSS_COUNT, (int)SPELL_COUNT, (int)VP_COUNT,
                   (int)SHOP_COUNT, (int)DUNGEON_G, (int)ROOM_TW, (int)ROOM_TH })
        mix((size_t)v);
    return h;
}

// ------------------------------------------------------------------ what goes over
template <class S> static void io(S& s, Particle& p) {      // just what it takes to draw one, packed: there can be hundreds
    int16_t x = 0, y = 0;
    uint16_t life = 0, maxLife = 0;
    uint8_t size = 0, flags = 0;
    if constexpr (!S::reading) {
        x = (int16_t)Clamp(p.pos.x * 4, -32767, 32767); y = (int16_t)Clamp(p.pos.y * 4, -32767, 32767);   // quarter pixels
        life = (uint16_t)Clamp(p.life * 1000, 0, 65535); maxLife = (uint16_t)Clamp(p.maxLife * 1000, 1, 65535);   // milliseconds
        size = (uint8_t)Clamp(p.size * 16, 0, 255);
        flags = (uint8_t)((p.shrink ? 1 : 0) | (p.star ? 2 : 0));
    }
    s(x, y, life, maxLife, size, p.color.r, p.color.g, p.color.b, flags);
    if constexpr (S::reading) {
        p.pos = { x / 4.0f, y / 4.0f }; p.vel = {};
        p.life = life / 1000.0f; p.maxLife = maxLife / 1000.0f;
        p.size = size / 16.0f; p.color.a = 255; p.gravity = 0;
        p.shrink = flags & 1; p.star = (flags & 2) != 0;
    }
}

template <class S> static void io(S& s, Enemy& e) {         // likewise: the host does the thinking
    s(e.pos, e.hp, e.maxHp, e.r, e.color, e.speed);
    s.name(e.name); s.name(e.weakness);
    s(e.boss, e.alive, e.hitFlash, e.atkCd, e.wobble, e.knock, e.burnT, e.lunge, e.swinging, e.kind, e.animT, e.atkAnim,
      e.deathT, e.dying, e.flip, e.moving, e.stolen, e.elite, e.fleeT, e.charmT, e.dazeT, e.bossForm, e.scale, e.diveT,
      e.diveDir, e.chaser, e.look, e.pose, e.lastHit, e.walking, e.stride);
}

template <class S> static void io(S& s, Projectile& p) {
    s(p.pos, p.vel, p.damage, p.color, p.glow);
    s.name(p.school);
    s(p.life, p.r, p.alive, p.kind, p.homing, p.curve, p.spell, p.travelled, p.nextSpike, p.trail, p.lastJag, p.armAt, p.tick, p.owner);
}

template <class S> static void io(S& s, Player& p) {
    s(p.pos, p.facing, p.level, p.xp, p.xpNext, p.maxHp, p.hp, p.maxMana, p.mana, p.selected, p.gold, p.packs, p.vapes,
      p.weapon, p.weaponsOwned, p.potions, p.bonusDamage, p.speedBonus, p.castCd, p.meleeCd, p.meleeAnim, p.roll, p.rollCd,
      p.invuln, p.hitFlash, p.rollDir, p.champion, p.specialCd, p.auraT, p.dashT, p.shieldT, p.rollAtkT, p.wobbleT, p.jumpT,
      p.charge, p.rollId, p.dashDir);
    for (int w = 0; w < WEAPON_COUNT; ++w)          // the weapons there are, not the room saves keep for more
        for (int u = 0; u < UPGRADE_COUNT; ++u) {
            uint8_t level = (uint8_t)p.upgrades[w][u];
            s(level);
            p.upgrades[w][u] = level;
        }
}

template <class S> static void io(S& s, Hero& h) {
    s(h.present);
    if (!h.present) return;
    s.text(h.name);
    s(h.outfit);
    io(s, h.player);
    s(h.overlay, h.hall, h.castleDoorCd, h.dialogueIdx, h.downT, h.shopMsgT);
    s.text(h.dialogue); s.text(h.shopMsg);
    Wheel& w = h.wheel;
    s(w.casino, w.spinning, w.done, w.angle, w.speed, w.result, w.bet, w.pick);
    s.text(w.message);
    s(h.gymT, h.gymDir, h.cigT, h.stubT, h.grindStep, h.throwT, h.throwKind, h.thrown, h.punchT, h.punchLanded, h.punchDir,
      h.slingT, h.slingFired, h.castingSpell, h.castT, h.heroMoving, h.heroMoveDir, h.heroStepping, h.heroStride, h.climbT, h.climbFrom);
}

template <class S> static void io(S& s, BossFight& f) {
    int def = f.def ? (int)(f.def - BOSSES) : -1;
    s(def);
    if constexpr (S::reading) {
        if (def < 0 || def >= BOSS_COUNT) { if (f.active()) f.end(); f.def = nullptr; return; }
        f.def = &BOSSES[def];
    }
    else if (def < 0) return;
    s(f.strength, f.time, f.bus, f.grenades, f.circuit, f.staff);
    s.name(f.formula);
    s(f.formulaAt, f.formulaT, f.scared, f.stage, f.stageT, f.stunT, f.coreHp, f.coreMax, f.coreAt, f.rocks, f.rockCount,
      f.scars, f.scarCount, f.beam, f.slams, f.blast, f.blastFrom, f.meleeWind, f.meleeBoom, f.meleeAt);
}

// The pictures a beaten boss breaks into, by number (textures have their own ids on every device).
static std::vector<Texture2D> bodyArt(const Game& g) {
    std::vector<Texture2D> v;
    for (int i = 0; i < BOSS_COUNT; ++i) { v.push_back(g.bossArt.poses[i]); v.push_back(g.bossArt.walk[i].tex); v.push_back(g.bossArt.token[i]); }
    v.push_back(SPR.enemy[EK_VAMPIRE].idle.tex);
    return v;
}

template <class S> static void ioWorld(S& s, Game& g) {
    s(g.floor, g.kills, g.runTime, g.difficulty, g.banner, g.shake, g.skyFlash, g.scareT, g.vasilGreetT, g.vasilBowT, g.trapClock,
      g.finaleT, g.slowMo, g.chestOpenT, g.chestLight, g.chestLooting, g.chestMana, g.chestPack, g.inFight, g.challengeT,
      g.challengeWaveT, g.rewardT, g.coop.cutscenes);
    s.text(g.bannerText); s.text(g.bannerSub);
    auto each = [&](auto& x) { io(s, x); };
    s.list(g.enemies, each);
    s.list(g.corpses, each);
    s.list(g.shots, each);
    s.list(g.enemyShots, each);
    s.list(g.parts, each);
    s.list(g.flashes); s.list(g.arcs); s.list(g.cracks); s.list(g.spikes); s.list(g.lasers); s.list(g.coins); s.list(g.floaters);
    std::vector<Texture2D> art = bodyArt(g);
    s.list(g.rubble, [&](Rubble& r) {
        int which = -1;
        for (size_t i = 0; i < art.size(); ++i) if (art[i].id && art[i].id == r.tex.id) which = (int)i;
        s(which, r.src, r.pos, r.size, r.vel, r.delay, r.t, r.rot, r.spin, r.ground);
        if constexpr (S::reading) r.tex = which >= 0 && which < (int)art.size() ? art[which] : Texture2D{};
    });
    s.list(g.popups, [&](Popup& p) {
        int blue = p.anim == &SPR.flaskBlue;
        s(blue, p.pos, p.t);
        if constexpr (S::reading) p.anim = blue ? &SPR.flaskBlue : &SPR.flaskRed;
    });
    io(s, g.fight);
    for (Hero& h : g.heroes) io(s, h);
    s.list(g.coop.sounds, [&](std::string& n) { s.text(n); });
}

// ------------------------------------------------------------------ the host
void Game::writeSnapshot(std::vector<uint8_t>& out) {
    syncHeroes();
    Hero& me = heroes[localHero];
    int outfit = me.outfit;                         // the host's own outfit, as its profile has it
    if (outfit < 0) me.outfit = profile.outfit[me.player.champion];
    wire::Writer w(out);
    ioWorld(w, *this);
    me.outfit = outfit;
}

static uint64_t hashBytes(const void* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

void Game::coopSendSnapshots() {
    double now = net::now();
    if (now - coop.lastSent < 1.0 / 60 - 0.002) return;     // 60 a second is plenty, however fast this device runs
    coop.lastSent = now;
    static std::vector<uint8_t> body;
    body.clear();
    writeSnapshot(body);
    uint64_t floorHash = hashBytes(&dungeon, sizeof dungeon);
    for (int s = 1; s < MAX_HEROES; ++s) {
        net::Link& L = coop.links[s];
        if (!coop.welcomed[s] || !heroes[s].present || !L.connected()) continue;
        if (coop.dungeonSent[s] != floorHash) { L.send(MSG_DUNGEON, &dungeon, sizeof dungeon); coop.dungeonSent[s] = floorHash; }
        if (L.pending() > 256 * 1024) continue;     // it hasn't taken the last ones yet: skip this one
        L.send(MSG_SNAPSHOT, body);
        L.update();
    }
    coop.sounds.clear();
}

// ------------------------------------------------------------------ a joiner
void Game::applyDungeon(const std::vector<uint8_t>& data) {
    if (data.size() == sizeof(Dungeon)) memcpy((void*)&dungeon, data.data(), sizeof(Dungeon));
}

void Game::applySnapshot(const std::vector<uint8_t>& data) {
    float hpBefore = coop.haveWorld ? player.hp : -1;
    int bet = wheel.bet, pick = wheel.pick;
    wire::Reader r(data);
    ioWorld(r, *this);
    if (!r.ok) return;
    coop.haveWorld = true;
    curHero = localHero = coop.mySeat;
    heroes[curHero].present = true;
    loadHeroFrom(heroes[curHero]);

    // over the game: the host's window for this hero (a shop), or this device's own (Pause)
    hostOverlay = overlay;
    if (coop.closed != Overlay::None) {             // closed here: the host hears of it in a moment
        if (hostOverlay == coop.closed && net::now() - coop.closedAt < 1.0) hostOverlay = Overlay::None;
        else coop.closed = Overlay::None;
    }
    if (hostOverlay != coop.lastHostOverlay) {      // a window just opened: where it starts
        if (hostOverlay == Overlay::Galin) galinPick = player.weapon;
        if (hostOverlay == Overlay::Drip) { dripChampion = player.champion; dripHover = -1; }
    }
    else if (hostOverlay == Overlay::Casino && !wheel.spinning) { wheel.bet = bet; wheel.pick = pick; }   // the stake being chosen here
    coop.lastHostOverlay = hostOverlay;
    if (localOverlay == Overlay::Cutscene && !cutscene.playing()) localOverlay = Overlay::None;
    overlay = localOverlay != Overlay::None ? localOverlay : hostOverlay;

    // what the host heard, the Bad Plasmodium's cutscene, the music, and a buzz when you're hit
    for (const std::string& n : coop.sounds) play(n.c_str());
    coop.sounds.clear();
    if (coop.cutscenes != coop.cutscenesSeen) {
        coop.cutscenesSeen = coop.cutscenes;
        if (localOverlay == Overlay::None && cutscene.open(BAD_PLASMODIUM_CUTSCENE)) localOverlay = overlay = Overlay::Cutscene;
    }
    const Room& room = dungeon.cur();
    std::string mood = room.isBoss && !room.cleared ? TextFormat("boss%d", currentBossIndex()) : "mist";
    if (mood != coop.mood) { coop.mood = mood; music.setMood(mood); }
    if (hpBefore > 0 && player.hp < hpBefore - 0.5f) hapticBuzz(hpBefore - player.hp >= 15 ? 70 : 35);
    if (floor > profile.deepestFloor) { profile.deepestFloor = floor; saveProfile(); }   // it counts on this device too
}

void Game::coopJoinerFrame() {
    if (coop.snapshotWaiting) { coop.snapshotWaiting = false; applySnapshot(coop.snapshot); }
    if (!coop.haveWorld || !coop.host.connected()) return;
    NetInput ni;
    if (overlay == Overlay::None && player.hp > 0) {   // (in a window, the keys are for the window)
        const Keybinds& K = settings.keys;
        auto bit = [&](bool held, bool tapped, uint32_t b) { if (held) ni.held |= b; if (tapped) ni.tapped |= b; };
        auto key = [&](int k, uint32_t b) { bit(in::down(k), in::pressed(k), b); };
        key(K.up, NA_UP); key(K.down, NA_DOWN); key(K.left, NA_LEFT); key(K.right, NA_RIGHT); key(K.run, NA_RUN);
        key(K.roll, NA_ROLL); key(K.special, NA_SPECIAL); key(K.interact, NA_INTERACT);
        key(K.potionHp, NA_POTION_HP); key(K.potionMana, NA_POTION_MANA);
        for (int i = 0; i < SPELL_COUNT; ++i) key(KEY_ONE + i, NA_SPELL1 << i);
        bit(in::mouseDown(MOUSE_BUTTON_LEFT), in::mousePressed(MOUSE_BUTTON_LEFT), NA_BLADE);
        bit(in::mouseDown(MOUSE_BUTTON_RIGHT), in::mousePressed(MOUSE_BUTTON_RIGHT), NA_CAST);
        bit(in::down(KEY_ENTER) || in::down(KEY_KP_ENTER), false, NA_CAST_HOLD);
        ni.aim = hall.active ? player.pos : mouseWorld();
    }
    std::vector<uint8_t> b;
    wire::Writer w(b);
    w(ni.held, ni.tapped, ni.aim);
    coop.host.send(MSG_INPUT, b);
    coop.host.update();
}

void Game::sendCommand(Cmd c, int a, int b) {
    if (!coop.joined() || !coop.host.connected()) return;
    std::vector<uint8_t> d;
    wire::Writer w(d);
    w.u8((int)c); w.i32(a); w.i32(b);
    coop.host.send(MSG_CMD, d);
}

// The joiner's windows after a frame: shops are the host's to close, Pause is this device's own.
void Game::coopOverlayChanged(Overlay before) {
    if (overlay == before) return;
    auto hosts = [](Overlay o) {
        return o == Overlay::Shop || o == Overlay::Galin || o == Overlay::Casino || o == Overlay::Drip || o == Overlay::RunComplete;
    };
    if (hosts(before) && overlay == Overlay::None) {
        sendCommand(Cmd::CloseOverlay);
        coop.closed = before; coop.closedAt = net::now();
        hostOverlay = localOverlay = Overlay::None;
    }
    else if (!hosts(overlay)) localOverlay = overlay;
}

// ------------------------------------------------------------------ the others, on screen
void Game::drawHeroes() {
    if (!coop.active() || !coop.started) return;
    int me = curHero;
    for (int h = 0; h < heroCount; ++h) {
        if (h == me || !heroes[h].present || heroes[h].hall.active) continue;
        useHero(h);
        drawHero();
        float top = player.pos.y - (player.hp > 0 ? 58.0f : 34.0f);
        Color tag = h == 0 ? ui::ACCENT : h == 1 ? Color{ 120, 200, 255, 255 } : h == 2 ? Color{ 140, 230, 150, 255 } : Color{ 255, 150, 200, 255 };
        int w = ui::measure(heroName.c_str(), 10);
        DrawRectangleRounded({ player.pos.x - w / 2.0f - 4, top - 2, w + 8.0f, 14 }, 0.4f, 4, Fade(BLACK, 0.55f));
        ui::textCentered(heroName.c_str(), player.pos.x, top, 10, tag);
    }
    useHero(me);
}

void Game::drawTeam() {
    if (!coop.active() || !coop.started) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float x = 16, y = 222;
    for (int h = 0; h < heroCount; ++h) {
        if (h == curHero || !heroes[h].present) continue;
        const Hero& H = heroes[h];
        Rectangle r = { x, y, 230, 38 };
        DrawRectangleRec(r, Fade(ui::PANEL, 0.86f));
        DrawRectangleLinesEx(r, 1, ui::EDGE);
        Rectangle pic = { r.x + 4, r.y + 3, 24, 32 };
        drawFigureIn(outfitLook(H.player.champion, std::max(0, H.outfit)), FACE_DOWN, 0, pic,
            H.player.hp > 0 ? WHITE : Color{ 110, 110, 120, 255 });
        ui::text(TextFormat("%s  %s", H.name.c_str(), CHAMPIONS[H.player.champion].name), r.x + 34, r.y + 4, 10, ui::TEXT);
        if (H.hall.active) ui::text("in the castle", r.x + 34, r.y + 20, 10, ui::MUTED);
        else if (H.player.hp <= 0) ui::text("DOWN", r.x + 34, r.y + 20, 10, ui::BAD);
        else ui::bar({ r.x + 34, r.y + 21, r.width - 42, 10 }, H.player.hp / fmaxf(1, H.player.maxHp), ui::HP);
        y += 44;
    }
    if (player.hp <= 0) {
        float a = 0.75f + 0.25f * sinf(clock * 4);
        ui::textCentered("Knocked out", sw / 2.0f, sh * 0.62f, 40, Fade(ui::BAD, a));
        ui::textCentered("Your team gets you up when the room is clear", sw / 2.0f, sh * 0.62f + 48, 20, ui::TEXT);
    }
}
