// ============================================================================
//  Play.cpp — the dungeon rules: movement, champions, combat, bosses, loot.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Touch.h"
#include "Render.h"
#include <algorithm>
#include <cmath>

static const char* ORACLE_LINES[] = {
    "So. The Veil spat you out at last.",
    "Press 1 and right-click (or Enter) to loose an Emberhex. Q calls your champion's gift.",
    "Clear a room to open its doors. Something old waits at the bottom of every floor.",
};
static constexpr int ORACLE_COUNT = (int)(sizeof ORACLE_LINES / sizeof ORACLE_LINES[0]);

Camera2D Game::camera() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Camera2D cam{};
    cam.target = ROOM_CENTER;
    cam.offset = { sw / 2.0f, sh / 2.0f };
    cam.zoom = fminf(sw / (ROOM_PX_W * 1.12f), sh / (ROOM_PX_H * 1.18f));   // the whole room, centred
    return cam;
}

Vector2 Game::mouseWorld() const {
    if (!touch::enabled()) return GetScreenToWorld2D(in::mouse(), camera());
    // No pointer on a phone: aim at the nearest enemy, or wherever you're heading.
    const Enemy* best = nullptr;
    for (auto& e : enemies)
        if (e.alive && (!best || Vector2Distance(e.pos, player.pos) < Vector2Distance(best->pos, player.pos))) best = &e;
    if (best) return best->pos;
    Vector2 dir = touch::stickDir();
    return Vector2Add(player.pos, Vector2Scale(dir.x || dir.y ? dir : player.facing, 100));
}

float Game::hurtMul() const { return player.has(VP_MINT) ? 0.85f : 1.0f; }

int Game::spellCost(int spell) const {
    return (int)ceilf(SPELLS[spell].manaCost * champ().manaCostMul * (player.has(VP_RAZZ) ? 0.8f : 1.0f));
}

int Game::bladeDamage() const {
    return 10 + player.level * 2 + WEAPONS[player.weapon].damage + player.upgrades[player.weapon][UP_SHARPEN] * 3
        + (player.has(VP_MELON) ? 4 : 0) + champ().bladeBonus;
}

float Game::swingTime() const {
    return WEAPONS[player.weapon].swing * (1 - 0.08f * player.upgrades[player.weapon][UP_BALANCE]);
}

float Game::bladeReach() const {
    return WEAPONS[player.weapon].reach + 4.0f * player.upgrades[player.weapon][UP_EXTEND];
}

static void burst(std::vector<Particle>& parts, Vector2 at, int n, float speed, float life, Color c) {
    for (int k = 0; k < n; ++k)
        parts.push_back({ at, { frand(-speed, speed), frand(-speed, speed) }, life, life, frand(2, 4), c });
}

// ------------------------------------------------------------------ the frame
void Game::updatePlay(float dt) {
    if (hitStop > 0) { hitStop -= dt; dt = 0; }       // a solid hit: everything holds its breath
    finaleT = fmaxf(0, finaleT - dt);
    if (slowMo > 0) { slowMo -= dt; dt *= 0.3f; }     // a boss's last moments
    Room& room = dungeon.cur();
    const ChampionDef& c = champ();
    Keybinds& K = settings.keys;
    bool talking = !dialogue.empty();
    float slow = player.auraT > 0 ? 0.5f : 1.0f;      // Atanas' aura

    // --- timers ---
    if (banner > 0) banner -= dt;
    if (mode == Mode::Run) runTime += dt;
    else if (fight.active()) rushTime += dt;
    for (float* timer : { &player.castCd, &player.meleeCd, &player.rollCd, &player.invuln, &player.hitFlash,
                          &player.specialCd, &player.auraT, &player.shieldT, &player.wobbleT })
        *timer = fmaxf(0, *timer - dt);
    player.meleeAnim = fmaxf(0, player.meleeAnim - dt * 4);
    scareT = fmaxf(0, scareT - dt);
    player.mana = fminf(player.maxMana, player.mana + 9.0f * dt);
    if (c.kind == Special::Blink) player.charge = fminf(DUST_MAX, player.charge + DUST_REGEN * dt);
    for (int v = 0; v < VP_COUNT; ++v)
        if (player.has((Vape)v) && frand(0, 1) < dt * 0.5f)   // the odd lazy puff drifting up off the player
            parts.push_back({ { player.pos.x + frand(-6, 6), player.pos.y - 18 }, { frand(-12, 12), frand(-40, -20) },
                1.0f, 1.0f, frand(2, 4), VAPE_COLOR[v] });
    trapClock += dt;

    // --- hotbar ---
    for (int i = 0; i < SPELL_COUNT; ++i)
        if (in::pressed(KEY_ONE + i)) { player.selected = i; play("ui"); }

    // --- movement ---
    Vector2 mv{ 0, 0 };
    if (!talking) {
        if (in::down(K.up))    mv.y -= 1;
        if (in::down(K.down))  mv.y += 1;
        if (in::down(K.left))  mv.x -= 1;
        if (in::down(K.right)) mv.x += 1;
    }
    if (mv.x || mv.y) { mv = Vector2Normalize(mv); player.facing = mv; heroMoveDir = mv; }
    heroMoving = mv.x || mv.y || player.roll > 0 || player.dashT > 0 || player.rollAtkT > 0;
    Vector2 aim = Vector2Subtract(mouseWorld(), player.pos);
    if (Vector2Length(aim) > 1) player.facing = Vector2Normalize(aim);
    if (!talking && in::pressed(K.roll) && player.rollCd <= 0 && (mv.x || mv.y)) {
        player.roll = 0.28f; player.rollCd = 0.7f;
        player.invuln = fmaxf(player.invuln, 0.3f); player.rollDir = mv;
    }
    if (!talking && in::pressed(K.special)) useSpecial(mv);
    movePlayer(dt, mv);
    Vector2 offRock = fight.rockPush(player.pos, PLAYER_R);
    if (offRock.x || offRock.y) player.pos = moveInRoom(player.pos, offRock, PLAYER_R, room, room.cleared);
    if (c.stillRegen > 0 && !(mv.x || mv.y) && player.hp < player.maxHp)
        player.hp = fminf(player.maxHp, player.hp + c.stillRegen * dt);

    bool airborne = player.jumpT > 0;
    bool spikesUp = fmodf(trapClock, 2.4f) < 1.2f;
    if (spikesUp && !airborne && player.roll <= 0 && player.invuln <= 0 && onSpikes(room, player.pos))
        hurtPlayer(8, 0.6f);

    // --- the boss key (lies in the middle once its room is safe) ---
    if (room.hasKey && room.cleared && !dungeon.keyFound && Vector2Distance(player.pos, ROOM_CENTER) < 26) {
        dungeon.keyFound = true; play("levelup");
        showBanner("Boss key found", "The lair is open", 2.2f);
        burst(parts, ROOM_CENTER, 16, 100, 0.7f, { 245, 215, 120, 255 });
    }

    // --- chest (only once the room is safe) ---
    Vector2 chestPos = { room.chestTx * (float)TILE + 16, room.chestTy * (float)TILE + 16 };
    bool nearChest = room.hasChest && !room.chestOpened && room.cleared && Vector2Distance(player.pos, chestPos) < 40;
    if (nearChest && !talking && in::pressed(K.interact)) {
        room.chestOpened = true; play("rattle"); chestOpenT = 0;     // it shakes, then slamChest throws it open
        chestMana = GetRandomValue(0, 1) == 0;
        chestPack = GetRandomValue(1, 100) <= CHEST_PACK_PERCENT;
        if (chestMana) { player.maxMana += 10; player.mana = fminf(player.maxMana, player.mana + 10); }
        else player.hp = fminf(player.maxHp, player.hp + 10);
    }

    // --- the oracle (start room) and the merchant (shop room) ---
    bool nearOracle = room.isStart && mode == Mode::Run && Vector2Distance(player.pos, ORACLE_POS) < 48;
    if (nearOracle && in::pressed(K.interact)) {
        play("ui");
        if (!talking) { dialogueIdx = 0; dialogue = ORACLE_LINES[0]; }
        else if (++dialogueIdx >= ORACLE_COUNT) dialogue.clear();
        else dialogue = ORACLE_LINES[dialogueIdx];
    }
    if (!nearOracle) dialogue.clear();
    bool nearMerchant = room.isShop && Vector2Distance(player.pos, MERCHANT_POS) < 56;
    if (nearMerchant && !talking && in::pressed(K.interact)) { overlay = room.isGalin ? Overlay::Galin : Overlay::Shop; play("ui"); }

    // --- attacks ---
    bool castKey = in::mousePressed(MOUSE_BUTTON_RIGHT) || in::down(KEY_ENTER) || in::down(KEY_KP_ENTER);   // hold Enter to keep casting
    if (!talking && castKey) castSpell();
    if (!talking && in::mousePressed(MOUSE_BUTTON_LEFT)) swingBlade();
    if (castT > 0) {                               // motes drawn in to the hand, then away it goes
        Vector2 hand = castHand();
        const SpellDef& s = SPELLS[castingSpell];
        if (frand(0, 1) < dt * 70) {
            Vector2 from = Vector2Add(hand, Vector2Scale(fromAngle(frand(0, 2 * PI)), frand(16, 30)));
            parts.push_back({ from, Vector2Scale(Vector2Subtract(hand, from), 5), 0.2f, 0.2f, frand(1.2f, 2.4f), s.glow, 0, true });
        }
        castT -= dt;
        if (castT <= 0) releaseSpell(castingSpell);
    }

    updateShots(dt);
    fight.scared = scareT > 0;
    fight.update(dt, player.pos, enemies, enemyShots, room, slow);
    if (fight.quake > 0) { shake = fmaxf(shake, fight.quake); fight.quake = 0; }
    if (fight.headline) { showBanner(fight.headline, fight.subline, 1.8f); fight.headline = nullptr; }
    if (fight.arenaBroke) {                        // stone flying up out of the floor
        fight.arenaBroke = false;
        for (int k = 0; k < 50; ++k)
            parts.push_back({ { frand(TILE * 2.0f, ROOM_PX_W - TILE * 2.0f), frand(TILE * 2.0f, ROOM_PX_H - TILE * 2.0f) }, { frand(-60, 60), frand(-240, -80) },
                0.9f, 0.9f, frand(1.5f, 4), k % 2 ? Color{ 96, 88, 106, 255 } : fight.def->color, 420, true });
    }
    updateEnemies(dt, slow);
    updateEnemyShots(dt, slow);
    // Triceps: her gravity wells drag you in, and blasts, live wires and the staff hurt
    Vector2 pull = fight.gravityPull(player.pos);
    if ((pull.x || pull.y) && !airborne)
        player.pos = moveInRoom(player.pos, Vector2Scale(pull, dt * slow), PLAYER_R, room, room.cleared);
    if (player.invuln <= 0 && !airborne)
        if (int dmg = fight.hazardHit(player.pos, PLAYER_R)) { hurtPlayer((float)dmg, 0.5f); shake = fmaxf(shake, 0.25f); }
    if (fight.busHits(player.pos, PLAYER_R) && player.invuln <= 0 && !airborne) {
        fight.bus.hit = true;
        hurtPlayer((float)fight.busDamage(), 0.8f);
        shake = 0.5f;
    }

    // --- deaths: corpses, loot, and the Plasmodia multiplying ---
    for (auto& e : enemies) if (e.alive && e.hp <= 0) killEnemy(e);
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
    enemies.insert(enemies.end(), spawned.begin(), spawned.end());
    spawned.clear();

    updateLoot(dt);

    // --- room cleared? open the doors ---
    if (inFight && enemies.empty() && finaleT <= 0) {
        room.cleared = true; inFight = false;
        if (room.isBoss) bossDefeated();
        else { showBanner("Room cleared", "", 1.4f); play("quest"); }
    }
    if (rewardT > 0 && (rewardT -= dt) <= 0) overlay = rewardOverlay;
    if (overlay != Overlay::None) return;

    // --- leave through an open door, or take the stairs down ---
    checkRoomExits();
    bool lootTaken = coins.empty() && corpses.empty();
    if (mode == Mode::Run && room.isBoss && room.cleared && lootTaken && rewardT <= 0 && Vector2Distance(player.pos, ROOM_CENTER) < 18) {
        descend();
        return;
    }
    if (mode == Mode::BossRush && rushNextT > 0) {
        rushNextT -= dt;
        if (rushNextT <= 0) {
            rushNextT = -1;
            rushStage++;
            player.hp = fminf(player.maxHp, player.hp + player.maxHp * 0.35f);
            player.mana = player.maxMana;
            player.pos = { ROOM_CENTER.x, ROOM_PX_H - TILE * 2.0f };
            room.cleared = false;
            enterRoom();
        }
    }

    // --- falling ---
    if (player.hp <= 0) {
        play("death");
        if (mode == Mode::BossRush) { rushWon = false; overlay = Overlay::RushOver; return; }
        player.hp = player.maxHp; player.mana = player.maxMana;
        dungeon.cx = DUNGEON_G / 2; dungeon.cy = DUNGEON_G / 2;
        player.pos = ROOM_CENTER;
        player.invuln = 1.5f;
        enterRoom();
        showBanner("You fall...", "back to the entrance", 2.2f);
    }
}

// ------------------------------------------------------------------ movement
void Game::movePlayer(float dt, Vector2 mv) {
    const ChampionDef& c = champ();
    Room& room = dungeon.cur();
    bool doorsOpen = room.cleared;
    Vector2 delta;

    if (player.dashT > 0) {                        // Coin Dash
        player.dashT -= dt;
        delta = Vector2Scale(player.dashDir, 520 * dt);
    }
    else if (player.rollAtkT > 0) {                // Melon Roll
        player.rollAtkT -= dt;
        delta = Vector2Scale(player.dashDir, RUN * 1.8f * dt);
        for (auto& e : enemies) {
            if (!e.alive || e.lastRollHit == player.rollId) continue;
            if (Vector2Distance(e.pos, player.pos) > e.r + PLAYER_R + 6) continue;
            e.lastRollHit = player.rollId;
            hitEnemy(e, 20, { 120, 220, 90, 255 });
            play("hit");
        }
    }
    else if (player.roll > 0) {
        player.roll -= dt;
        delta = Vector2Scale(player.rollDir, ROLL_SPEED * dt);
    }
    else {
        float speed = (in::down(settings.keys.run) ? RUN : WALK) * c.speedMul
            * (player.has(VP_MANGO) ? 1.1f : 1.0f) * (player.jumpT > 0 ? 1.5f : 1.0f);
        delta = Vector2Scale(mv, speed * dt);
    }
    player.pos = moveInRoom(player.pos, delta, PLAYER_R, room, doorsOpen);

    if (player.jumpT > 0) {                        // Bear Jump: land with a slam
        player.jumpT -= dt;
        if (player.jumpT <= 0) {
            for (auto& e : enemies)
                if (e.alive && Vector2Distance(e.pos, player.pos) < 56 + e.r) hitEnemy(e, 18, { 230, 180, 90, 255 });
            for (int k = 0; k < 20; ++k) {
                Vector2 d = fromAngle(k * 2 * PI / 20);
                parts.push_back({ Vector2Add(player.pos, Vector2Scale(d, 10)), Vector2Scale(d, 160), 0.45f, 0.45f, 3, { 200, 170, 120, 255 } });
            }
            shake = 0.3f; play("hit");
        }
    }
}

void Game::useSpecial(Vector2 mv) {
    const ChampionDef& c = champ();
    if (player.specialCd > 0) return;
    Vector2 dir = (mv.x || mv.y) ? mv : player.facing;
    switch (c.kind) {
    case Special::ArcaneSurge:
        player.mana = fminf(player.maxMana, player.mana + 40);
        burst(parts, player.pos, 16, 90, 0.6f, { 120, 170, 255, 255 }); play("heal");
        break;
    case Special::Aura:
        player.hp = fminf(player.maxHp, player.hp + 25);
        player.auraT = 3;
        burst(parts, player.pos, 16, 90, 0.7f, { 150, 220, 255, 255 }); play("heal");
        break;
    case Special::CoinDash:
        player.dashDir = dir; player.dashT = 0.25f; player.invuln = fmaxf(player.invuln, 0.3f);
        play("coin");
        break;
    case Special::MugShield:
        player.shieldT = 2; play("shield");
        break;
    case Special::MelonRoll:
        player.dashDir = dir; player.rollAtkT = 0.6f; player.invuln = fmaxf(player.invuln, 0.6f);
        player.rollId++;
        play("ui");
        break;
    case Special::Drink:
        player.hp = fminf(player.maxHp, player.hp + 20);
        player.wobbleT = 4; play("heal");
        break;
    case Special::BearJump:
        player.jumpT = 0.6f; player.invuln = fmaxf(player.invuln, 0.6f); play("ui");
        break;
    case Special::CoinBarrage: {
        if (player.charge < COIN_POWER_MAX) return;
        player.charge = 0;
        for (int i = 0; i < 16; ++i) {
            Projectile p;
            p.pos = player.pos; p.vel = Vector2Scale(fromAngle(i * 2 * PI / 16), 300);
            p.damage = 10; p.r = 6; p.life = 1.2f; p.kind = ShotKind::Coin;
            p.color = { 245, 215, 120, 255 }; p.glow = { 255, 240, 180, 255 };
            shots.push_back(p);
        }
        play("coin");
        break;
    }
    case Special::Koi: {
        float base = atan2f(player.facing.y, player.facing.x);
        for (int i = -1; i <= 1; ++i) {
            Projectile p;
            p.pos = player.pos; p.vel = Vector2Scale(fromAngle(base + i * 0.5f), 180);
            p.damage = 12; p.r = 7; p.life = 4; p.kind = ShotKind::Koi; p.homing = 3;
            p.color = { 255, 140, 50, 255 }; p.glow = { 255, 200, 140, 255 };
            shots.push_back(p);
        }
        play("nature");
        break;
    }
    case Special::Blink: {
        if (player.charge < DUST_PER_BLINK) return;
        Vector2 to = mouseWorld();
        int tx = (int)floorf(to.x / TILE), ty = (int)floorf(to.y / TILE);
        if (roomWallSolid(dungeon.cur(), tx, ty, false)) return;   // can't blink into a wall
        player.charge -= DUST_PER_BLINK;
        burst(parts, player.pos, 12, 70, 0.5f, { 240, 170, 255, 255 });
        player.pos = to;
        player.invuln = fmaxf(player.invuln, 0.2f);
        burst(parts, player.pos, 12, 70, 0.5f, { 240, 170, 255, 255 });
        play("shield");
        break;
    }
    case Special::Scare:
        scareT = SCARE_TIME;
        burst(parts, player.pos, 18, 160, 0.5f, { 170, 190, 255, 255 });
        shake = fmaxf(shake, 0.15f);
        play("scare");
        break;
    }
    player.specialCd = c.cooldown;
}

// ------------------------------------------------------------------- attacks
void Game::castSpell() {
    const ChampionDef& c = champ();
    if (player.castCd > 0 || castT > 0) return;
    const SpellDef& s = SPELLS[player.selected];
    int cost = spellCost(player.selected);
    if (player.mana < cost) return;
    player.mana -= cost;
    player.castCd = fmaxf(0.28f * c.castCdMul, s.charge + 0.04f);
    if (s.charge > 0) {
        castingSpell = player.selected; castT = s.charge;
        play(castingSpell == SP_FIRE ? "charge" : "chill");
    }
    else releaseSpell(player.selected);
}

Vector2 Game::aimDir() const {
    Vector2 dir = Vector2Normalize(Vector2Subtract(mouseWorld(), player.pos));
    return Vector2Length(dir) < 0.01f ? player.facing : dir;
}

Vector2 Game::castHand() const {
    Vector2 d = aimDir();
    return { player.pos.x + d.x * 18, player.pos.y - 6 + d.y * 18 };
}

void Game::releaseSpell(int spell) {
    const ChampionDef& c = champ();
    const SpellDef& s = SPELLS[spell];
    play(s.sfx);
    Vector2 dir = aimDir();
    if (player.wobbleT > 0) dir = fromAngle(atan2f(dir.y, dir.x) + frand(-0.35f, 0.35f));   // Ristyo's drink
    Projectile p;
    p.pos = Vector2Add(player.pos, Vector2Scale(dir, 14));
    p.vel = Vector2Scale(dir, s.speed);
    p.damage = s.damage; p.color = s.color; p.glow = s.glow; p.school = s.school;
    p.life = 1.4f; p.r = spell == SP_BOLT ? 4.0f : 6.0f;
    p.homing = c.spellHoming;
    p.spell = spell; p.trail = ++iceTrails;
    if (spell == SP_BOLT) {
        p.lastJag = p.pos; p.nextSpike = BOLT_JAG;
        flashes.push_back({ p.pos, 0, 0.1f, 18, frand(0, PI), { 210, 230, 255, 255 }, false });
    }
    shots.push_back(p);
}

void Game::swingBlade() {
    if (player.meleeCd > 0) return;
    player.meleeCd = swingTime(); player.meleeAnim = 1.0f;
    play("slash");
    const WeaponDef& w = WEAPONS[player.weapon];
    int damage = bladeDamage();
    float reach = bladeReach();
    bool hitAny = false;
    for (auto& e : enemies) {
        if (!e.alive) continue;
        Vector2 to = Vector2Subtract(e.pos, player.pos);
        float d = Vector2Length(to);
        if (d >= reach + e.r || Vector2DotProduct(Vector2Normalize(to), player.facing) <= 0.3f) continue;
        hitEnemy(e, damage, { 234, 255, 255, 255 });
        hitAny = true;
        Vector2 dir = Vector2Normalize(to);
        float ang = atan2f(dir.y, dir.x);
        if (e.bossForm < 0) e.knock = Vector2Scale(dir, w.knockback * KNOCK_DECAY);   // bosses stand their ground
        Vector2 at = Vector2Subtract(e.pos, Vector2Scale(dir, e.r * 0.6f));
        flashes.push_back({ at, 0, 0.14f, 28, ang + 0.6f, WHITE, false });
        for (int k = 0; k < 9; ++k)                // sparks thrown on through the target
            parts.push_back({ at, Vector2Scale(fromAngle(ang + frand(-0.7f, 0.7f)), frand(110, 280)), 0.28f, 0.28f,
                frand(1.2f, 2.6f), k % 3 ? Color{ 255, 244, 200, 255 } : Color{ 255, 190, 90, 255 }, 0, true });
    }
    if (hitAny) { play(w.sfx); shake = fmaxf(shake, 0.14f); hitStop = HIT_FREEZE; }
}

void Game::hitEnemy(Enemy& e, int damage, Color fx) {
    bool boss = e.bossForm == 0 && fight.active();
    if (boss && fight.shielded()) { burst(parts, e.pos, 3, 50, 0.25f, { 210, 210, 220, 255 }); return; }   // roaring: nothing gets through
    if (!boss || !fight.strikeCore(damage)) {
        float before = e.hp;
        e.hp -= damage;
        if (boss && before > fight.gate(e)) e.hp = fmaxf(e.hp, fight.gate(e));    // every stage gets its turn
    }
    e.hitFlash = 0.12f;
    floaters.push_back({ { e.pos.x + frand(-6, 6), e.pos.y - e.r - 8 }, 0, damage, fx });
    burst(parts, e.pos, 5, 60, 0.4f, fx);
    if (champ().kind == Special::CoinBarrage)
        player.charge = fminf(COIN_POWER_MAX, player.charge + COIN_POWER_PER_HIT);
}

void Game::hurtPlayer(float damage, float invuln) {
    float dealt = damage * hurtMul();
    if (fight.active() && player.hp <= player.maxHp * MERCY_BELOW) dealt *= 0.5f;
    player.hp -= dealt;
    player.hitFlash = 0.15f; player.invuln = invuln;
    floaters.push_back({ { player.pos.x, player.pos.y - 26 }, 0, (int)ceilf(dealt), { 255, 110, 110, 255 } });
    play("hurt");
}

void Game::updateShots(float dt) {
    Room& room = dungeon.cur();
    for (auto& p : shots) {
        if (!p.alive) continue;
        if (p.homing > 0) {           // curve toward the nearest enemy in reach
            const Enemy* target = nullptr; float best = 300;
            for (auto& e : enemies) {
                float d = Vector2Distance(e.pos, p.pos);
                if (e.alive && d < best) { best = d; target = &e; }
            }
            if (target) {
                float want = atan2f(target->pos.y - p.pos.y, target->pos.x - p.pos.x), have = atan2f(p.vel.y, p.vel.x);
                float diff = atan2f(sinf(want - have), cosf(want - have));
                float turn = fmaxf(-p.homing * dt, fminf(p.homing * dt, diff));
                p.vel = Vector2Scale(fromAngle(have + turn), Vector2Length(p.vel));
            }
        }
        p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
        p.life -= dt;
        p.travelled += Vector2Length(p.vel) * dt;
        if (p.spell == SP_FIRE && frand(0, 1) < dt * 150) {     // embers and smoke streaming off behind
            int shade = GetRandomValue(0, 3);
            Color c = shade == 0 ? Color{ 255, 226, 130, 255 } : shade == 3 ? Color{ 92, 84, 84, 255 } : Color{ 255, 120, 40, 255 };
            parts.push_back({ { p.pos.x + frand(-3, 3), p.pos.y + frand(-3, 3) },
                { p.vel.x * -0.14f + frand(-14, 14), p.vel.y * -0.14f + frand(-22, 8) }, 0.4f, 0.4f, frand(2, 4.6f), c, 0, true });
        }
        for (; p.spell == SP_ICE && p.travelled >= p.nextSpike; p.nextSpike += SPIKE_GAP)
            plantSpikes(p, Vector2Subtract(p.pos, Vector2Scale(Vector2Normalize(p.vel), p.travelled - p.nextSpike)));
        for (; p.spell == SP_BOLT && p.travelled >= p.nextSpike; p.nextSpike += BOLT_JAG) {
            Vector2 d = Vector2Normalize(p.vel), side = { -d.y, d.x };
            Vector2 on = Vector2Add(Vector2Subtract(p.pos, Vector2Scale(d, p.travelled - p.nextSpike)), Vector2Scale(side, frand(-9, 9)));
            arcs.push_back({ p.lastJag, on, 0, 0.26f, 2.4f, frand(0, 100) });
            if (GetRandomValue(0, 3) == 0)         // now and then the bolt forks
                zapFork(on, atan2f(d.y, d.x) + (GetRandomValue(0, 1) ? 1 : -1) * frand(0.6f, 1.2f), frand(18, 34));
            p.lastJag = on;
        }
        if (p.life <= 0 || roomWallSolid(room, (int)floorf(p.pos.x / TILE), (int)floorf(p.pos.y / TILE), room.cleared)) {
            p.alive = false; spellEnded(p, nullptr); continue;
        }
        for (auto& e : enemies) {
            if (!e.alive || Vector2Distance(p.pos, e.pos) >= e.r + p.r) continue;
            bool weak = e.weakness && p.school && TextIsEqual(e.weakness, p.school);
            hitEnemy(e, weak ? (int)(p.damage * 1.6f) : p.damage, weak ? Color{ 255, 230, 120, 255 } : p.glow);
            if (p.spell == SP_FIRE) e.burnT = BURN_TIME;
            if (p.spell == SP_BOLT) e.hitFlash = 0.22f;   // lit up white a little longer
            p.alive = false;
            burst(parts, p.pos, 8, 90, 0.5f, p.glow);
            spellEnded(p, &e);
            break;
        }
    }
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Projectile& p) { return !p.alive; }), shots.end());
}

// Tidelash's next stretch of trail: a spike that stands taller the further out it is, with
// a small one on each side leaning away from the line.
void Game::plantSpikes(const Projectile& p, Vector2 at) {
    float k = fminf(1, p.nextSpike / 340), h = 11 + 37 * k;
    Vector2 d = Vector2Normalize(p.vel), side = { -d.y, d.x };
    int seq = (int)(p.nextSpike / SPIKE_GAP);
    const Room& room = dungeon.cur();
    spikes.push_back({ at, { frand(-2, 2), -h }, h * 0.22f, 0, -1, p.trail, seq });
    for (float s : { -1.0f, 1.0f }) {
        float small = h * frand(0.46f, 0.6f);
        Vector2 out = Vector2Scale(side, s), foot = Vector2Add(at, Vector2Scale(out, 6 + 8 * k));
        if (roomWallSolid(room, (int)floorf(foot.x / TILE), (int)floorf(foot.y / TILE), true)) continue;
        spikes.push_back({ foot, { out.x * small * 0.85f, out.y * small * 0.85f - small * 0.35f }, small * 0.26f, 0, -1, p.trail, seq });
    }
    std::stable_sort(spikes.begin(), spikes.end(), [](const IceSpike& a, const IceSpike& b) { return a.pos.y < b.pos.y; });
}

void Game::zapFork(Vector2 from, float ang, float len) {
    for (int k = 0; k < 3; ++k) {
        Vector2 to = Vector2Add(from, Vector2Scale(fromAngle(ang + frand(-0.6f, 0.6f)), len / 3));
        arcs.push_back({ from, to, 0, 0.16f, 1.4f, frand(0, 100) });
        from = to;
    }
}

// A god-power has hit something, or the wall, or run out.
void Game::spellEnded(const Projectile& p, const Enemy* hit) {
    if (p.spell == SP_FIRE) {
        flashes.push_back({ p.pos, 0, 0.32f, 46, 0, { 255, 130, 50, 255 }, true });
        for (int k = 0; k < 20; ++k) {
            bool smoke = k % 4 == 0;
            parts.push_back({ p.pos, Vector2Scale(fromAngle(frand(0, 2 * PI)), frand(40, smoke ? 90.0f : 230.0f)), smoke ? 0.7f : 0.42f, smoke ? 0.7f : 0.42f,
                frand(2, smoke ? 6.0f : 4.0f), smoke ? Color{ 86, 78, 78, 255 } : k % 3 ? Color{ 255, 150, 50, 255 } : Color{ 255, 232, 150, 255 }, 0, true });
        }
        play("fireboom");
    }
    else if (p.spell == SP_ICE) {
        if (hit) { cracks.push_back({ hit->pos, 0, hit->r * 2.4f + 14, frand(0, 100), { 180, 235, 255, 255 } }); play("icecrack"); }
        for (auto& s : spikes)
            if (s.trail == p.trail) s.breakAt = s.t + 0.35f + s.seq * 0.018f;   // from the caster out to the tip
    }
    else if (p.spell == SP_BOLT) {
        arcs.push_back({ p.lastJag, p.pos, 0, 0.26f, 2.4f, frand(0, 100) });
        flashes.push_back({ p.pos, 0, hit ? 0.2f : 0.12f, hit ? 46.0f : 24.0f, frand(0, PI), { 210, 230, 255, 255 }, false });
        if (hit) {                                 // lightning crawling off the target, and the room lit up
            for (int k = 0; k < 6; ++k) zapFork(hit->pos, k * PI / 3 + frand(-0.3f, 0.3f), hit->r + frand(18, 32));
            for (int k = 0; k < 8; ++k)
                parts.push_back({ p.pos, Vector2Scale(fromAngle(frand(0, 2 * PI)), frand(90, 240)), 0.25f, 0.25f, frand(1.2f, 2.4f), { 215, 235, 255, 255 }, 0, true });
            skyFlash = 0.5f;
            play("thunder");
        }
    }
    else if (hit) play("hit");
}

// ------------------------------------------------------------------- enemies
// A gentle push away from nearby walls, so monsters round corners instead of
// scraping along them and snagging.
static Vector2 wallAvoid(const Room& room, Vector2 pos, float r, bool doorsOpen) {
    Vector2 push{};
    float reach = r + 10;
    int tx0 = (int)floorf((pos.x - reach) / TILE), tx1 = (int)floorf((pos.x + reach) / TILE);
    int ty0 = (int)floorf((pos.y - reach) / TILE), ty1 = (int)floorf((pos.y + reach) / TILE);
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int tx = tx0; tx <= tx1; ++tx) {
            if (!roomWallSolid(room, tx, ty, doorsOpen)) continue;
            Vector2 c = { Clamp(pos.x, tx * (float)TILE, (tx + 1) * (float)TILE),
                          Clamp(pos.y, ty * (float)TILE, (ty + 1) * (float)TILE) };
            Vector2 off = Vector2Subtract(pos, c);
            float d = Vector2Length(off);
            if (d < 0.01f || d >= reach) continue;
            push = Vector2Add(push, Vector2Scale(off, (reach - d) / reach / d));
        }
    return push;
}

void Game::updateEnemies(float dt, float slow) {
    static FlowField flow;          // path distances to the player, rebuilt each frame
    Room& room = dungeon.cur();
    bool doorsOpen = room.cleared;
    bool untouchable = player.invuln > 0 || player.jumpT > 0;
    bool scared = scareT > 0;               // Momiche's scare: everyone runs, nobody bites
    // One flow field per frame: every monster reads its next step from it, so
    // they path around pillars and (while it's cheaper) around spike traps.
    buildFlowField(room, player.pos, true, flow);

    for (auto& e : enemies) {
        if (!e.alive) continue;
        float edt = dt * slow;
        e.hitFlash = fmaxf(0, e.hitFlash - dt);
        if (e.burnT > 0) {
            e.burnT = fmaxf(0, e.burnT - dt);
            if (frand(0, 1) < dt * 18)
                parts.push_back({ { e.pos.x + frand(-e.r, e.r), e.pos.y - frand(0, e.r * 1.4f) }, { frand(-10, 10), frand(-70, -30) },
                    0.5f, 0.5f, frand(1.2f, 2.6f), GetRandomValue(0, 1) ? Color{ 255, 150, 50, 255 } : Color{ 255, 226, 130, 255 }, 0, true });
        }
        if (Vector2LengthSqr(e.knock) > 1) {
            e.pos = moveInRoom(e.pos, Vector2Scale(e.knock, dt), e.r, room, doorsOpen);
            e.knock = Vector2Scale(e.knock, expf(-KNOCK_DECAY * dt));
        }
        e.atkCd = fmaxf(0, e.atkCd - edt);
        e.wobble += edt * 4;
        e.animT += edt;
        e.atkAnim = fmaxf(0, e.atkAnim - edt);
        Vector2 to = Vector2Subtract(player.pos, e.pos);
        float d = Vector2Length(to);

        if (e.bossForm < 0 && scared) {
            if (fabsf(to.x) > 2) e.flip = to.x > 0;            // facing away, running
            e.moving = true;
            e.lunge = 0; e.atkAnim = 0;
            Vector2 away = d > 0.01f ? Vector2Scale(to, -1.0f / d) : Vector2{ 1, 0 };
            e.pos = moveInRoom(e.pos, Vector2Scale(away, e.speed * FLEE_SPEED * edt), e.r, room, doorsOpen);
        }
        else if (e.kind == EK_TOSHKO && e.stolen > 0) {
            // got the gold: run from the player, and after a few seconds he's gone with it
            e.fleeT += edt;
            if (fabsf(to.x) > 2) e.flip = to.x > 0;
            e.moving = true;
            Vector2 away = d > 0.01f ? Vector2Scale(to, -1.0f / d) : Vector2{ 1, 0 };
            Vector2 side = { -away.y, away.x };
            away = Vector2Normalize(Vector2Add(away, Vector2Scale(side, sinf(e.wobble) * 0.5f)));
            e.pos = moveInRoom(e.pos, Vector2Scale(away, e.speed * edt), e.r, room, doorsOpen);
            if (e.fleeT > 4.0f) {
                e.alive = false;
                burst(parts, e.pos, 16, 140, 0.5f, { 200, 200, 210, 255 });   // vape cloud
                showBanner(TextFormat("Toshko got away with %d gold", e.stolen), "", 2.0f);
                play("hurt");
                continue;
            }
        }
        else if (e.bossForm < 0) {  // boss bodies are moved by the BossFight
            if (fabsf(to.x) > 2) e.flip = to.x < 0;
            e.moving = d > 1 && e.atkAnim <= 0 && Vector2LengthSqr(e.knock) < 400;     // plant feet while swinging or reeling
            e.lunge = fmaxf(0, e.lunge - edt);
            e.lungeCd = fmaxf(0, e.lungeCd - edt);
            if (e.moving) {
                bool sees = lineClear(room, e.pos, player.pos, e.r + 4);
                Vector2 goal;
                if (!sees) goal = nextWaypoint(room, flow, e.pos, e.r);   // route around walls
                else if (e.kind == EK_SKELETON && d > 70) {
                    // foot soldiers fan out to the sides so they surround you
                    Vector2 n = Vector2Normalize(to);
                    Vector2 flank = Vector2Add(player.pos, Vector2Scale({ -n.y, n.x }, 40 * e.flankSide));
                    goal = lineClear(room, e.pos, flank, e.r) ? flank : player.pos;
                }
                else goal = player.pos;
                Vector2 dir = Vector2Normalize(Vector2Subtract(goal, e.pos));
                if (e.kind == EK_SKULL) {
                    // wisps weave side to side, harder to hit with spells
                    Vector2 side = { -dir.y, dir.x };
                    dir = Vector2Normalize(Vector2Add(dir, Vector2Scale(side, sinf(e.wobble * 1.3f) * 0.8f)));
                }
                // only out in the open: a push off the path pins a wide body on a corner in a tight lane
                if (sees) dir = Vector2Normalize(Vector2Add(dir, Vector2Scale(wallAvoid(room, e.pos, e.r, doorsOpen), 1.2f)));
                // hounds that see you crouch and lunge
                if (e.kind == EK_SKELETON2 && sees && d < 130 && d > 40 && e.lungeCd <= 0) { e.lunge = 0.35f; e.lungeCd = 2.5f; }
                float spd = e.speed * (e.lunge > 0 ? 2.6f : 1.0f);
                e.pos = moveInRoom(e.pos, Vector2Scale(dir, spd * edt), e.r, room, doorsOpen);
            }
        }

        // solid bodies: push out of the player and out of each other (nothing shifts the Bad Plasmodium)
        bool rooted = e.bossForm == 0 && fight.active() && fight.def->kind == BossKind::Rooted;
        Vector2 off = Vector2Subtract(e.pos, player.pos);
        float dist = Vector2Length(off), minD = e.r + PLAYER_R;
        if (dist < minD && player.jumpT <= 0) {
            Vector2 n = dist > 0.01f ? Vector2Scale(off, 1.0f / dist) : Vector2{ 1, 0 };
            if (rooted) player.pos = moveInRoom(player.pos, Vector2Scale(n, dist - minD), PLAYER_R, room, doorsOpen);
            else e.pos = moveInRoom(e.pos, Vector2Scale(n, minD - dist), e.r, room, doorsOpen);
        }
        for (auto& o : enemies) {
            if (rooted) break;
            if (&o == &e || !o.alive) continue;
            Vector2 sep = Vector2Subtract(e.pos, o.pos);
            float sd = Vector2Length(sep), need = e.r + o.r;
            if (sd >= need) continue;
            Vector2 n = sd > 0.01f ? Vector2Scale(sep, 1.0f / sd) : Vector2{ 0, 1 };
            e.pos = moveInRoom(e.pos, Vector2Scale(n, (need - sd) * 0.5f), e.r, room, doorsOpen);
        }

        // Toshko doesn't hit you: he snatches up to 50 gold and runs
        if (e.kind == EK_TOSHKO) {
            if (e.stolen == 0 && d < e.r + PLAYER_R + 4 && !scared && player.gold > 0) {
                e.stolen = std::min(50, player.gold);
                player.gold -= e.stolen;
                e.atkAnim = SPR.enemy[e.kind].attack.length(16);
                showBanner(TextFormat("Toshko snatched %d gold!", e.stolen), "Kill him before he gets away", 2.0f);
                play("coin");
            }
            continue;
        }

        // touch damage
        if (d < e.r + PLAYER_R + 4 && e.atkCd <= 0 && !untouchable && !scared) {
            hurtPlayer((float)e.dmg, 0.4f);
            e.atkCd = 1.0f;
            if (e.kind != EK_SKULL && e.bossForm < 0) e.atkAnim = SPR.enemy[e.kind].attack.length(16);
            untouchable = true;
        }
    }
}

void Game::updateEnemyShots(float dt, float slow) {
    Room& room = dungeon.cur();
    for (auto& p : enemyShots) {
        if (!p.alive) continue;
        if (p.curve != 0) p.vel = Vector2Rotate(p.vel, p.curve * dt * slow);    // Triceps' boots in their field
        p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt * slow));
        p.life -= dt * slow;
        if (p.life <= 0 || roomWallSolid(room, (int)floorf(p.pos.x / TILE), (int)floorf(p.pos.y / TILE), false)) {
            p.alive = false; continue;
        }
        if (Vector2Distance(p.pos, player.pos) >= p.r + PLAYER_R) continue;
        if (player.shieldT > 0) {          // Nikolay's mug-shield
            p.alive = false; play("shield");
            burst(parts, p.pos, 6, 70, 0.3f, { 220, 200, 160, 255 });
            continue;
        }
        if (player.invuln > 0 || player.jumpT > 0) continue;
        p.alive = false;
        hurtPlayer((float)p.damage, 0.5f);
    }
    enemyShots.erase(std::remove_if(enemyShots.begin(), enemyShots.end(), [](const Projectile& p) { return !p.alive; }), enemyShots.end());
}

void Game::killEnemy(Enemy& e) {
    e.alive = false;
    kills++;
    player.xp += e.xp;
    play("pickup");
    Enemy corpse = e;
    corpse.dying = true; corpse.deathT = 0; corpse.hitFlash = 0; corpse.atkAnim = 0; corpse.burnT = 0;
    corpses.push_back(corpse);
    int n = e.bossForm == 0 ? 12 + 2 * floor : e.bossForm > 0 ? 1 : 1 + GetRandomValue(0, 2);
    n += champ().extraCoins;
    if (e.stolen > 0) {                    // caught Toshko: every snatched coin drops back
        n += e.stolen;
        showBanner("Your gold is back", "", 1.6f);
    }
    for (int k = 0; k < n; ++k) coins.push_back({ e.pos, { frand(-90, 90), frand(-140, -40) }, 0 });
    if (e.kind == EK_GAKEV)
        coins.push_back({ e.pos, { frand(-40, 40), -160 }, 0, true });
    burst(parts, e.pos, 14, 120, 0.7f, e.color);
    if (e.bossForm == 0) shake = 0.5f;
    fight.onDeath(e, spawned);
    if (fight.active() && e.bossForm >= 0) {
        if (e.bossForm == 0 && fight.def->kind != BossKind::Split)     // its minions go down with it
            for (auto& o : enemies)
                if (o.alive && o.bossForm > 0) { o.alive = false; burst(parts, o.pos, 8, 90, 0.5f, o.color); }
        bool left = !spawned.empty();
        for (auto& o : enemies) left |= o.alive && o.bossForm >= 0 && o.hp > 0;
        if (!left && finaleT <= 0) finalBlow(e);
    }

    while (player.xp >= player.xpNext) {
        player.xp -= player.xpNext; player.level++;
        player.xpNext = (int)(60 * powf((float)player.level, 1.5f));
        player.maxHp += 18; player.hp = player.maxHp;
        player.maxMana += 10; player.mana = player.maxMana;
        showBanner("The Unmarked grows stronger", TextFormat("Level %d", player.level), 2.5f);
        play("levelup");
    }
}

void Game::finalBlow(const Enemy& e) {
    corpses.pop_back();                        // it doesn't fade away: it comes apart
    BodyCut cut = bossCut(e, currentBossIndex(), bossArt);
    const int cols = 6, rows = 7;
    float cw = fabsf(cut.src.width) / cols, ch = cut.src.height / rows;
    Vector2 size = { cut.dst.width / cols, cut.dst.height / rows };
    float ground = cut.dst.y + cut.dst.height;
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            int col = cut.src.width < 0 ? cols - 1 - x : x;
            Rectangle src = { cut.src.x + col * cw, cut.src.y + y * ch, cut.src.width < 0 ? -cw : cw, ch };
            Vector2 at = { cut.dst.x + (x + 0.5f) * size.x, cut.dst.y + (y + 0.5f) * size.y };
            rubble.push_back({ cut.tex, src, at, size, { frand(-70, 70), frand(-130, -20) }, y * 0.16f + frand(0, 0.35f), 0, 0, frand(-6, 6), ground + frand(-4, 8) });
        }
    fight.calm();
    enemyShots.clear();
    finaleT = FINALE_TIME; hitStop = FINAL_FREEZE; slowMo = FINAL_SLOW;
    player.invuln = fmaxf(player.invuln, FINALE_TIME + 1);
    shake = 0.8f; skyFlash = 0.6f;
    flashes.push_back({ e.pos, 0, 0.2f, 90, frand(0, PI), WHITE, false });
    flashes.push_back({ e.pos, 0, 0.3f, 120, 0, e.color, true });
    play("finalhit");
    play("crumble");
}

// ---------------------------------------------------------------------- loot
// The lid flies up: the loot, the light and a shower of stars all come out at once.
void Game::slamChest() {
    Room& room = dungeon.cur();
    Vector2 chestPos = { room.chestTx * (float)TILE + 16, room.chestTy * (float)TILE + 16 };
    play("lid");
    shake = fmaxf(shake, 0.12f);
    chestLight = 1; chestLooting = true;
    popups.push_back({ chestMana ? &SPR.flaskBlue : &SPR.flaskRed, { chestPos.x, chestPos.y - 14 }, 0 });
    showBanner(chestMana ? "Chest: +10 max mana" : "Chest: +10 health", "", 1.6f);
    for (int k = 0; k < 4; ++k) coins.push_back({ chestPos, { frand(-90, 90), frand(-140, -40) }, 0 });
    if (chestPack) coins.push_back({ chestPos, { frand(-40, 40), -160 }, 0, true });
    for (int k = 0; k < 16; ++k) {
        Vector2 v = Vector2Scale(fromAngle(-PI / 2 + frand(-0.9f, 0.9f)), frand(110, 260));
        float life = frand(0.7f, 1.1f);
        parts.push_back({ { chestPos.x, chestPos.y - 8 }, v, life, life, frand(3, 5.5f),
            k % 3 ? Color{ 255, 222, 110, 255 } : Color{ 255, 255, 235, 255 }, 340, false, true });
    }
}

// Flashes, cracks and ice run on their own clocks, and frozen frames stop them too.
void Game::updateEffects(float dt) {
    for (auto& f : flashes) f.t += dt;
    flashes.erase(std::remove_if(flashes.begin(), flashes.end(), [](const Flash& f) { return f.t >= f.life; }), flashes.end());
    for (auto& a : arcs) a.t += dt;
    arcs.erase(std::remove_if(arcs.begin(), arcs.end(), [](const Arc& a) { return a.t >= a.life; }), arcs.end());
    skyFlash = fmaxf(0, skyFlash - dt * 3);
    for (auto& c : cracks) c.t += dt;
    cracks.erase(std::remove_if(cracks.begin(), cracks.end(), [](const FloorCrack& c) { return c.t >= CRACK_TIME; }), cracks.end());

    bool shattered = false;
    for (auto& s : spikes) {
        s.t += dt;
        if (s.breakAt < 0 || s.t < s.breakAt) continue;
        for (int k = 0; k < 4; ++k)                // it falls to shards
            parts.push_back({ Vector2Add(s.pos, Vector2Scale(s.rise, frand(0, 1))), { frand(-70, 70), frand(-120, -20) }, 0.5f, 0.5f,
                frand(1.2f, 2.8f), k % 2 ? Color{ 214, 244, 255, 255 } : Color{ 128, 205, 240, 255 }, 380, true });
        shattered |= s.seq == 0;
    }
    if (shattered) play("shatter");
    spikes.erase(std::remove_if(spikes.begin(), spikes.end(), [](const IceSpike& s) { return s.breakAt >= 0 && s.t >= s.breakAt; }), spikes.end());
}

void Game::updateLoot(float dt) {
    for (auto& p : parts) { p.vel.y += p.gravity * dt; p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt)); p.life -= dt; }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
    updateEffects(dt);
    for (auto& r : rubble) {
        r.t += dt;
        if (r.t < r.delay) continue;
        if (r.t - dt < r.delay)                // a puff of dust as each piece lets go
            parts.push_back({ r.pos, { frand(-20, 20), frand(-40, -10) }, 0.5f, 0.5f, frand(2, 4), { 120, 112, 124, 255 }, 0, true });
        r.vel.y += 420 * dt;
        r.pos = Vector2Add(r.pos, Vector2Scale(r.vel, dt));
        r.rot += r.spin * dt;
        if (r.pos.y > r.ground) { r.pos.y = r.ground; r.vel = { r.vel.x * 0.4f, r.vel.y * -0.3f }; r.spin *= 0.4f; }
    }

    for (auto& c : corpses) c.deathT += dt;
    corpses.erase(std::remove_if(corpses.begin(), corpses.end(), [](const Enemy& c) {
        float len = c.bossForm >= 0 && c.kind != EK_VAMPIRE ? 0.45f
            : c.kind == EK_SKULL ? 0.5f : SPR.enemy[c.kind].death.length(14) + 0.6f;   // linger a moment
        return c.deathT > len; }), corpses.end());

    if (chestOpenT < CHEST_SLAM && chestOpenT + dt >= CHEST_SLAM) slamChest();
    chestOpenT += dt;
    if (chestLooting && coins.empty() && popups.empty()) chestLooting = false;   // all taken: the light goes out
    if (!chestLooting) chestLight = fmaxf(0, chestLight - dt * 2.5f);
    for (auto& p : popups) { p.t += dt; p.pos.y -= 26 * dt; }
    popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& p) { return p.t > 1.4f; }), popups.end());

    for (auto& f : floaters) { f.t += dt; f.pos.y -= 34 * dt; }
    floaters.erase(std::remove_if(floaters.begin(), floaters.end(), [](const FloatText& f) { return f.t > 0.8f; }), floaters.end());

    for (auto& c : coins) {
        c.t += dt;
        if (c.t < 0.45f) {                         // pop out with a little gravity
            c.vel.y += 360 * dt;
            c.pos = Vector2Add(c.pos, Vector2Scale(c.vel, dt));
        }
        else {                                     // then get pulled to the player
            Vector2 to = Vector2Subtract(player.pos, c.pos);
            float spd = 160 + 600 * (c.t - 0.45f);
            c.pos = Vector2Add(c.pos, Vector2Scale(Vector2Normalize(to), fminf(spd * dt, Vector2Length(to))));
        }
    }
    coins.erase(std::remove_if(coins.begin(), coins.end(), [&](const Coin& c) {
        if (c.t <= 0.45f || Vector2Distance(c.pos, player.pos) >= 8) return false;
        if (c.pack) {
            player.packs++; play("quest");
            showBanner("Cigarette pack", "Galin can upgrade your weapon with this", 2.2f);
            return true;
        }
        player.gold++; play("coin");
        return true; }), coins.end());
}

// --------------------------------------------------------------------- doors
void Game::checkRoomExits() {
    Room& r = dungeon.cur();
    if (!r.cleared) return;
    float dcx = (ROOM_TW / 2) * TILE + TILE * 0.5f;
    float dcy = (ROOM_TH / 2) * TILE + TILE * 0.5f;
    Vector2& p = player.pos;
    // the boss lair stays shut until the key is found: step back out of the doorway
    for (int dir : { DIR_N, DIR_E, DIR_S, DIR_W }) {
        if (!dungeon.bossDoorLocked(dir)) continue;
        bool at = dir == DIR_N ? p.y < TILE + 8 && fabsf(p.x - dcx) < TILE
                : dir == DIR_S ? p.y > (ROOM_TH - 1) * TILE - 8 && fabsf(p.x - dcx) < TILE
                : dir == DIR_W ? p.x < TILE + 8 && fabsf(p.y - dcy) < TILE
                :                p.x > (ROOM_TW - 1) * TILE - 8 && fabsf(p.y - dcy) < TILE;
        if (!at) continue;
        p.y += dir == DIR_N ? 14.0f : dir == DIR_S ? -14.0f : 0.0f;
        p.x += dir == DIR_W ? 14.0f : dir == DIR_E ? -14.0f : 0.0f;
        showBanner("The lair is locked", "Find the boss key on this floor", 1.8f);
        play("hurt");
        return;
    }
    bool moved = true;
    if ((r.doors & DIR_N) && p.y < TILE + 8 && fabsf(p.x - dcx) < TILE) { dungeon.cy--; p = { dcx, (ROOM_TH - 1) * TILE - 28.0f }; }
    else if ((r.doors & DIR_S) && p.y > (ROOM_TH - 1) * TILE - 8 && fabsf(p.x - dcx) < TILE) { dungeon.cy++; p = { dcx, TILE + 28.0f }; }
    else if ((r.doors & DIR_W) && p.x < TILE + 8 && fabsf(p.y - dcy) < TILE) { dungeon.cx--; p = { (ROOM_TW - 1) * TILE - 28.0f, dcy }; }
    else if ((r.doors & DIR_E) && p.x > (ROOM_TW - 1) * TILE - 8 && fabsf(p.y - dcy) < TILE) { dungeon.cx++; p = { TILE + 28.0f, dcy }; }
    else moved = false;
    if (!moved) return;
    for (auto& c : coins) player.packs += c.pack;   // a pack still in the air comes along
    enterRoom();
}

// --------------------------------------------------------------------- shops
void Game::buy(int i) {
    const ShopItem& it = SHOP_ITEMS[i];
    if (it.vape >= 0 && player.has((Vape)it.vape)) return;
    if (player.gold < it.price) { shopMsgT = 1.5f; shopMsg = "Not enough gold"; play("hurt"); return; }
    player.gold -= it.price;
    if (it.vape >= 0) { player.vapes |= 1u << it.vape; play("levelup"); }
    else {
        if (i == 0) player.hp = fminf(player.maxHp, player.hp + 35);
        else player.mana = fminf(player.maxMana, player.mana + 50);
        play("pickup");
    }
    shopMsgT = 1.5f; shopMsg = std::string("Bought ") + it.name;
}

void Game::buyWeapon(int i) {
    const WeaponDef& w = WEAPONS[i];
    if (i == player.weapon) return;
    if (!player.owns(i)) {
        if (player.gold < w.price) { shopMsgT = 1.5f; shopMsg = "Not enough gold"; play("hurt"); return; }
        player.gold -= w.price;
        player.weaponsOwned |= 1u << i;
        shopMsg = std::string("Bought ") + w.name;
        play("levelup");
    }
    else { shopMsg = std::string("Equipped ") + w.name; play("ui"); }
    player.weapon = i;
    shopMsgT = 1.5f;
}

void Game::buyUpgrade(int u) {
    int& level = player.upgrades[player.weapon][u];
    if (level >= UPGRADE_MAX) return;
    int price = UPGRADES[u].gold * (level + 1);
    shopMsgT = 1.5f;
    if (player.packs < 1) { shopMsg = "No cigarette pack: Gakev drops them"; play("hurt"); return; }
    if (player.gold < price) { shopMsg = "Not enough gold"; play("hurt"); return; }
    player.packs--; player.gold -= price; level++;
    shopMsg = TextFormat("%s: %s %d/%d", WEAPONS[player.weapon].name, UPGRADES[u].name, level, UPGRADE_MAX);
    play("levelup");
}
