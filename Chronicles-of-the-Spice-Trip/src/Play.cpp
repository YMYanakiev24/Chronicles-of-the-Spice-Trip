// ============================================================================
//  Play.cpp — the dungeon rules: movement, champions, combat, bosses, loot.
// ============================================================================
#include "Game.h"
#include "Input.h"
#include "Touch.h"
#include "Render.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// Vasil's tale for each floor of a cycle (THEMES order; the boss at the bottom matches).
static const char* const TALES[6][4] = {
    { "Ah, a listener! Sit, sit. I'm Vasil, and every floor down here has a story. This one starts with you.",
      "The Mistwood Depths. Clear a room and its doors open. Press 1 and right-click (or Enter) to loose an Emberhex; Q calls your champion's gift.",
      "At the bottom waits the Good Plazmodium. Kill it once and it comes back as three. Fire does it no favours.",
      "Off you go. I'll be waiting at the top of the next floor, with the next story." },
    { "You made it down! This is the Drowned Crypt. The water here remembers everyone it ever swallowed.",
      "The Bad Plasmodium sits at the bottom: a head as big as a cart. It never moves. It doesn't need to; its old body walks for it.",
      "Lightning is what it fears. Don't stand close when it slams, and watch for the rocks it throws.",
      nullptr },
    { "The Ember Halls. Warm, isn't it? Take your coat off and listen.",
      "Mordekai lives at the bottom. He fans his feathers out like knives, and every third volley he dives at you.",
      "Feathers burn. Fire, friend. You didn't hear that from me.",
      nullptr },
    { "The Bone Gardens. Nothing grows here but stories, and I grow those.",
      "Vlado runs the line at the bottom: tickets in spirals, and when he whistles, bus No.9 comes down your lane.",
      "Lightning stops his engine. And never, ever stand in the bus lane.",
      nullptr },
    { "The Toxic Sewers. Hold your nose, not your tongue.",
      "Triceps waits in her cave, and every attack she throws is real physics: grenades on a parabola, magnetic boots, a staff on a fulcrum, a live circuit.",
      "Water shorts her circuits. Stay off the wires when they light up.",
      nullptr },
    { "The Gilded Vault, the last floor. The gold down here is real. The courage has to be yours.",
      "Pyraketh, the Cinder-Maw, lord of the lowest floor. He chases, he breathes rings of embers, and once he's wounded he throws fire.",
      "Water puts out even him. Finish this, and the Spice Trip is a story I'll be telling for years.",
      nullptr },
};

std::vector<std::string> Game::vasilTale() const {
    std::vector<std::string> lines;
    if (floor > BOSS_COUNT)
        lines.push_back(TextFormat("Round %d, and you're still here? Every time round the bosses get half again as strong. Listen closely.",
            (floor - 1) / BOSS_COUNT + 1));
    for (const char* line : TALES[theme()]) if (line) lines.push_back(line);
    return lines;
}

std::string Game::vasilGreeting() const {
    if (floor == 1) return "Come here, come! Every floor has a story, and I know them all.";
    return TextFormat("Floor %d! Sit down and hear the tale of %s.", floor, THEMES[theme()].name);
}

Camera2D Game::camera() const {
    if (hunt.active) return toiletCamera();
    if (hall.active) return castleCamera();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Camera2D cam{};
    cam.offset = { sw / 2.0f, sh / 2.0f };
    cam.zoom = fminf(sw / (ROOM_PX_W * 1.12f), sh / (ROOM_PX_H * 1.18f));   // the whole room, centred
    cam.target = ROOM_CENTER;
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
    return 10 + player.level * 2 + player.bonusDamage + WEAPONS[player.weapon].damage + player.upgrades[player.weapon][UP_SHARPEN] * 3
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
    vasilGreetT = fmaxf(0, vasilGreetT - dt);
    castleDoorCd = fmaxf(0, castleDoorCd - dt);
    vasilBowT = fmaxf(0, vasilBowT - dt);
    gymT = fmaxf(0, gymT - dt);
    for (auto& l : lasers) l.t += dt;
    std::erase_if(lasers, [](const Laser& l) { return l.t >= GYM_POSE_TIME; });
    player.mana = fminf(player.maxMana, player.mana + 9.0f * (player.has(VP_GRAPE) ? 1.5f : 1.0f) * dt);
    if (mode == Mode::Run && !inFight && (autosaveT += dt) > 20) autosave();     // every so often in a safe room
    if (c.kind == Special::Blink) player.charge = fminf(DUST_MAX, player.charge + DUST_REGEN * dt);
    for (int v = 0; v < VP_COUNT; ++v)
        if (player.has((Vape)v) && frand(0, 1) < dt * 0.5f)   // the odd lazy puff drifting up off the player
            parts.push_back({ { player.pos.x + frand(-6, 6), player.pos.y - 18 }, { frand(-12, 12), frand(-40, -20) },
                1.0f, 1.0f, frand(2, 4), VAPE_COLOR[v] });
    trapClock += dt;

    // --- hotbar ---
    for (int i = 0; i < SPELL_COUNT; ++i)
        if (in::pressed(KEY_ONE + i)) { player.selected = i; play("ui"); }
    if (!talking && in::pressed(K.potionHp)) usePotion(0);
    if (!talking && in::pressed(K.potionMana)) usePotion(1);

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
    if (!airborne && player.roll <= 0 && player.invuln <= 0 && inFlames(room, player.pos, trapClock)) {
        hurtPlayer(12, 0.7f);
        burst(parts, player.pos, 8, 70, 0.4f, { 255, 140, 40, 255 });
    }

    // --- the boss key (lies in the middle once its room is safe) ---
    if (room.hasKey && room.cleared && !dungeon.keyFound && Vector2Distance(player.pos, ROOM_CENTER) < 26) {
        dungeon.keyFound = true; play("levelup");
        showBanner("Boss key found", "The lair is open", 2.2f);
        burst(parts, ROOM_CENTER, 16, 100, 0.7f, { 245, 215, 120, 255 });
    }

    // --- the silver key (beside the middle, once its room is safe) ---
    if (room.hasSilverKey && room.cleared && !dungeon.silverFound && Vector2Distance(player.pos, SILVER_POS) < 26) {
        dungeon.silverFound = true; play("levelup");
        showBanner("Silver key found", "The treasure room is open", 2.2f);
        burst(parts, SILVER_POS, 16, 100, 0.7f, { 210, 220, 240, 255 });
    }

    // --- the treasure room's hoard ---
    if (room.isTreasure && !room.chestOpened && !talking && Vector2Distance(player.pos, ROOM_CENTER) < 48 && in::pressed(K.interact)) {
        room.chestOpened = true; play("lid"); play("victory"); shake = fmaxf(shake, 0.15f);
        int gold = 60 + 10 * floor;
        for (int k = 0; k < gold; ++k) coins.push_back({ ROOM_CENTER, { frand(-140, 140), frand(-200, -40) }, 0 });
        for (int k = 0; k < 2; ++k) coins.push_back({ ROOM_CENTER, { frand(-50, 50), -170 }, 0, true });
        player.hp = player.maxHp; player.mana = player.maxMana;
        std::string got = TextFormat("+%d gold, 2 packs, full health", gold);
        int missing[VP_COUNT], n = 0;                 // and a vape Marin sells, if you're missing one
        for (int v = 0; v < VP_COUNT; ++v) if (!player.has((Vape)v)) missing[n++] = v;
        if (n > 0) {
            int v = missing[GetRandomValue(0, n - 1)];
            player.vapes |= 1u << v;
            for (const ShopItem& it : SHOP_ITEMS) if (it.vape == v) got += std::string(", and ") + it.name;
        }
        showBanner("Treasure!", got, 3.2f);
        for (int k = 0; k < 30; ++k) {
            float life = frand(0.8f, 1.3f);
            parts.push_back({ { ROOM_CENTER.x, ROOM_CENTER.y - 10 }, Vector2Scale(fromAngle(-PI / 2 + frand(-1.0f, 1.0f)), frand(120, 300)), life, life,
                frand(3, 6), k % 3 ? Color{ 255, 222, 110, 255 } : Color{ 255, 255, 235, 255 }, 320, false, true });
        }
    }

    // --- chest (only once the room is safe) ---
    Vector2 chestPos = { room.chestTx * (float)TILE + 16, room.chestTy * (float)TILE + 16 };
    bool nearChest = room.hasChest && !room.chestOpened && room.cleared && Vector2Distance(player.pos, chestPos) < 40;
    if (nearChest && !talking && in::pressed(K.interact) && !testMode && mode == Mode::Run && GetRandomValue(1, 100) <= MIMIC_PERCENT)
        openMimic();
    else if (nearChest && !talking && in::pressed(K.interact)) {
        room.chestOpened = true; play("rattle"); chestOpenT = 0;     // it shakes, then slamChest throws it open
        chestMana = GetRandomValue(0, 1) == 0;
        chestPack = GetRandomValue(1, 100) <= CHEST_PACK_PERCENT;
        if (chestMana) { player.maxMana += 10; player.mana = fminf(player.maxMana, player.mana + 10); }
        else player.hp = fminf(player.maxHp, player.hp + 10);
    }

    // --- the castle door in the first room: E, or walk up into it ---
    if (room.isStart && mode == Mode::Run && !talking && castleDoorCd <= 0 && fabsf(player.pos.x - CASTLE_DOOR_X) < 30 && player.pos.y < CASTLE_DOOR_FRONT.y + 10
        && (in::pressed(K.interact) || (mv.y < -0.5f && player.pos.y < TILE + PLAYER_R + 6))) {
        enterCastle();
        return;
    }

    // --- Vasil's tale (start room) ---
    bool nearVasil = room.isStart && mode == Mode::Run && Vector2Distance(player.pos, VASIL_POS) < 56;
    if (nearVasil && in::pressed(K.interact)) {
        play("ui");
        std::vector<std::string> tale = vasilTale();
        if (!talking) { dialogueIdx = 0; dialogue = tale[0]; vasilGreetT = 0; vasilBowT = 0; }
        else if (++dialogueIdx >= (int)tale.size()) { dialogue.clear(); vasilBowT = 1.2f; }   // the end: he takes a bow
        else dialogue = tale[dialogueIdx];
    }
    if (!nearVasil) dialogue.clear();
    if (!dialogue.empty() && frand(0, 1) < dt * 5) {   // the story glows off his hands
        float life = frand(0.7f, 1.1f);
        float side = GetRandomValue(0, 1) ? 1.0f : -1.0f;
        parts.push_back({ { VASIL_POS.x + side * frand(10, 20), VASIL_POS.y - frand(14, 24) }, { side * frand(4, 16), frand(-36, -16) },
            life, life, frand(1.5f, 2.5f), { 255, 226, 150, 255 }, 0, true, true });
    }

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
    updateChallenge(dt);

    // --- room cleared? open the doors ---
    if (inFight && enemies.empty() && finaleT <= 0 && challengeT <= 0) {
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
        if (currentBoss().kind == BossKind::Physics) startToilet();   // after Triceps: the toilets, then on down
        else descend();
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
    if (player.hp <= 0) playerDied();
}

void Game::playerDied() {
    play("death");
    hapticBuzz(300);
    if (mode == Mode::BossRush) { rushWon = false; overlay = Overlay::RushOver; return; }
    stats.deaths++;
    if (hardcore) {                 // one life: the run is over, and the save goes with it
        profile.bestHardcore = std::max(profile.bestHardcore, floor);
        saveProfile();
        save::deleteRun(); hasSave = false;
        summaryWon = false; overlay = Overlay::Summary;
        return;
    }
    // Toshko is never far: he goes through your pockets while you're down
    int lost = player.gold < 10 ? player.gold : std::max(10, player.gold * 3 / 10);
    player.gold -= lost; stats.goldLost += lost;
    player.hp = player.maxHp; player.mana = player.maxMana;
    dungeon.cx = DUNGEON_G / 2; dungeon.cy = DUNGEON_G / 2;
    player.pos = ROOM_CENTER;
    player.invuln = 1.5f;
    enterRoom();
    showBanner("You fall...", lost > 0 ? TextFormat("Toshko picked your pockets: -%d gold", lost) : "back to the entrance", 2.6f);
    autosave();
}

// A challenge room: waves keep coming until the clock runs out, then the room is yours.
void Game::updateChallenge(float dt) {
    if (challengeT <= 0) return;
    Room& room = dungeon.cur();
    challengeT -= dt;
    if (enemies.empty()) challengeWaveT = fminf(challengeWaveT, 1.0f);   // wave beaten early: the next comes sooner
    if ((challengeWaveT -= dt) <= 0 && challengeT > 3) {
        static const int pool[] = { MT_MISTLING, MT_HOUND, MT_WISP, MT_SOLDIER, MT_BLOOD, MT_ORC, MT_DEMON };
        int kinds = floor >= 2 ? 7 : 6;
        float tough = 1.0f + 0.15f * (floor - 1);
        for (int i = 0, n = 3 + floor / 2; i < n; ++i) spawnMonster(pool[GetRandomValue(0, kinds - 1)], tough, 140);
        challengeWaveT = CHALLENGE_WAVE;
        play("roar");
    }
    if (challengeT > 0) return;
    challengeT = 0;
    for (auto& e : enemies) if (e.alive) burst(parts, e.pos, 10, 100, 0.5f, e.color);
    enemies.clear();
    room.cleared = true; inFight = false;
    int gold = 30 + 6 * floor;
    for (int k = 0; k < gold; ++k) coins.push_back({ player.pos, { frand(-120, 120), frand(-180, -40) }, 0 });
    coins.push_back({ player.pos, { 0, -170 }, 0, true });
    showBanner("Challenge complete", TextFormat("+%d gold and a cigarette pack", gold), 2.4f);
    play("victory");
}

void Game::usePotion(int which) {
    if (player.potions[which] <= 0) { showBanner(which ? "No mana potions" : "No health potions", "Marin sells them", 1.2f); play("hurt"); return; }
    if (which == 0 ? player.hp >= player.maxHp : player.mana >= player.maxMana) return;
    player.potions[which]--;
    if (which == 0) player.hp = fminf(player.maxHp, player.hp + 35);
    else player.mana = fminf(player.maxMana, player.mana + 50);
    popups.push_back({ which ? &SPR.flaskBlue : &SPR.flaskRed, { player.pos.x, player.pos.y - 30 }, 0 });
    play("heal");
}

// The chest bites back: the room locks again until the Mimic is dead.
void Game::openMimic() {
    Room& room = dungeon.cur();
    Vector2 at = { room.chestTx * (float)TILE + 16, room.chestTy * (float)TILE + 16 };
    room.chestOpened = true; room.mimic = true; room.cleared = false; inFight = true;
    Enemy& e = spawnEnemy("Mimic", 90 * (1.0f + 0.15f * (floor - 1)), 95, 14, 40, 13, { 150, 100, 50, 255 }, "Fire", EK_MIMIC);
    e.pos = at;
    showBanner("It's a Mimic!", "The chest bites back", 2.0f);
    play("roar"); shake = fmaxf(shake, 0.3f);
    hapticBuzz(120);
}

// ---------------------------------------------------------------- the wheel
void Game::spinWheel() {
    if (wheel.spinning) return;
    if (wheel.casino) {
        if (player.gold < wheel.bet) { wheel.message = "Not enough gold"; play("hurt"); return; }
        player.gold -= wheel.bet;
    }
    wheel.spinning = true; wheel.done = false; wheel.result = -1; wheel.message.clear();
    wheel.speed = frand(560, 900);
    play("rattle");
}

void Game::updateWheel(float dt) {
    if (!wheel.spinning) return;
    wheel.angle = fmodf(wheel.angle + wheel.speed * dt, 360);
    wheel.speed = fmaxf(0, wheel.speed - (90 + wheel.speed * 0.55f) * dt);
    if (wheel.speed > 0) return;
    wheel.spinning = false;
    int n = wheel.casino ? CASINO_SLOTS : PZ_COUNT;
    float under = fmodf(270.0f - wheel.angle + 720.0f, 360.0f);     // the pointer sits at the top
    wheel.result = std::min(n - 1, (int)(under / (360.0f / n)));
    wheelLanded();
}

void Game::wheelLanded() {
    wheel.done = true;
    if (!wheel.casino) {            // the level-up prize
        stats.spins++;
        switch (wheel.result) {
        case PZ_HEALTH:  player.maxHp += 25; player.hp = fminf(player.maxHp, player.hp + 25); break;
        case PZ_MANA:    player.maxMana += 20; player.mana = fminf(player.maxMana, player.mana + 20); break;
        case PZ_DAMAGE:  player.bonusDamage += 5; break;
        case PZ_SPEED:   player.speedBonus += 0.08f; break;
        case PZ_GOLD:    player.gold += 40; stats.goldEarned += 40; break;
        case PZ_PACK:    player.packs++; break;
        case PZ_JACKPOT: player.gold += 120; stats.goldEarned += 120; break;
        default: break;             // Toshko took it
        }
        wheel.message = PRIZES[wheel.result].won;
        play(wheel.result == PZ_TOSHKO ? "hurt" : wheel.result == PZ_JACKPOT ? "victory" : "levelup");
        return;
    }
    int colour = wheel.result == 0 ? 2 : wheel.result % 2 ? 0 : 1;      // 0 red, 1 black, 2 Toshko's zero
    if (colour == wheel.pick) {
        int win = wheel.bet * (colour == 2 ? 14 : 2);
        player.gold += win; stats.goldEarned += win - wheel.bet;
        wheel.message = TextFormat("%s! You win %d gold", colour == 2 ? "TOSHKO'S ZERO" : colour == 0 ? "Red" : "Black", win);
        play(colour == 2 ? "victory" : "quest");
    }
    else {
        stats.goldLost += wheel.bet;
        wheel.message = colour == 2 ? "Toshko's zero. The house wins." : "No luck. Toshko pockets your bet.";
        play("hurt");
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
        float speed = (in::down(settings.keys.run) ? RUN : WALK) * c.speedMul * (1 + player.speedBonus)
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
    case Special::CreatineCpp:
        creatineAndCpp(player.facing);
        break;
    }
    player.specialCd = c.cooldown;
}

// The Gym Rat throws his arms out: a fan of C++ lasers toward the aim, each running on until
// a wall stops it and hitting everything it touches, and a cloud of creatine out the other
// hand that shoves whatever is behind him away.
void Game::creatineAndCpp(Vector2 dir) {
    Room& room = dungeon.cur();
    gymDir = dir; gymT = GYM_POSE_TIME;
    float base = atan2f(dir.y, dir.x);
    Vector2 from = { player.pos.x + dir.x * 14, player.pos.y - 8 + dir.y * 10 };
    std::vector<Enemy*> struck;
    for (int i = 0; i < CPP_BEAMS; ++i) {
        Vector2 d = fromAngle(base + (i - (CPP_BEAMS - 1) / 2.0f) * CPP_SPREAD);
        float len = 0;
        while (len < CPP_RANGE) {                 // march along it until a wall gets in the way
            Vector2 p = Vector2Add(from, Vector2Scale(d, len + 6));
            if (roomWallSolid(room, (int)floorf(p.x / TILE), (int)floorf(p.y / TILE), false)) break;
            len += 6;
        }
        Vector2 to = Vector2Add(from, Vector2Scale(d, len));
        lasers.push_back({ from, to, 0 });
        for (auto& e : enemies) {
            if (!e.alive || std::find(struck.begin(), struck.end(), &e) != struck.end()) continue;
            float along = Clamp(Vector2DotProduct(Vector2Subtract(e.pos, from), d), 0, len);
            if (Vector2Distance(e.pos, Vector2Add(from, Vector2Scale(d, along))) < e.r + 4) struck.push_back(&e);
        }
    }
    for (Enemy* e : struck) {
        hitEnemy(*e, CPP_DAMAGE, { 90, 255, 120, 255 });
        burst(parts, e->pos, 6, 90, 0.35f, { 120, 255, 140, 255 });
    }

    Vector2 back = Vector2Negate(dir);                // the creatine, out the other hand
    for (auto& e : enemies) {
        if (!e.alive) continue;
        Vector2 to = Vector2Subtract(e.pos, player.pos);
        float d = Vector2Length(to);
        if (d > CREATINE_RANGE + e.r || (d > 1 && Vector2DotProduct(Vector2Scale(to, 1 / d), back) < cosf(CREATINE_CONE))) continue;
        hitEnemy(e, CREATINE_DAMAGE, { 245, 245, 255, 255 });
        if (e.bossForm < 0) e.knock = Vector2Scale(d > 1 ? Vector2Scale(to, 1 / d) : back, CREATINE_SHOVE * KNOCK_DECAY);
    }
    Vector2 hand = { player.pos.x - dir.x * 14, player.pos.y - 8 - dir.y * 10 };
    for (int k = 0; k < 46; ++k) {                    // a spray of white powder
        float life = frand(0.35f, 0.75f);
        Vector2 v = Vector2Scale(fromAngle(atan2f(back.y, back.x) + frand(-CREATINE_CONE, CREATINE_CONE)), frand(90, 260));
        parts.push_back({ hand, v, life, life, frand(1.5f, 3.5f), k % 4 ? Color{ 244, 246, 255, 255 } : Color{ 200, 214, 235, 255 }, -20 });
    }
    shake = fmaxf(shake, 0.12f);
    play("beam"); play("swing");
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
    stats.damageDealt += damage;
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
    hapticBuzz(dealt >= 15 ? 70 : 35);
    // for the summary: who did it (the boss, the nearest monster, or the room itself)
    stats.damageTaken += (int)ceilf(dealt);
    const char* by = "a trap";
    if (fight.active()) by = fight.def->name;
    else {
        float best = 1e9f;
        for (auto& e : enemies)
            if (e.alive && Vector2Distance(e.pos, player.pos) < best) { best = Vector2Distance(e.pos, player.pos); by = e.name; }
    }
    snprintf(stats.killer, sizeof stats.killer, "%s", by);
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
// Who shoots at you, and how long between shots.
static float shotGap(const Enemy& e) {
    if (e.bossForm >= 0) return 0;
    if (e.kind == EK_SOLDIER) return 2.4f;
    if (e.kind == EK_SKULL) return 2.8f;
    return e.kind == EK_BLOOD && e.scale >= 1 ? 3.2f : 0;     // its halves have only their claws
}

void Game::enemyShoot(const Enemy& e) {
    Projectile p;
    p.pos = { e.pos.x, e.pos.y - e.r * 0.6f };
    bool arrow = e.kind == EK_SOLDIER;
    p.vel = Vector2Scale(Vector2Normalize(Vector2Subtract(player.pos, p.pos)), arrow ? ARROW_SPEED : ORB_SHOT_SPEED);
    p.r = arrow ? 4.0f : 5.0f; p.life = 3; p.damage = e.dmg;
    p.kind = arrow ? ShotKind::Arrow : ShotKind::Orb;
    p.color = e.color;
    enemyShots.push_back(p);
    play("throw");
}

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
            e.lunge = 0; e.atkAnim = 0; e.swinging = false;
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
            if (e.kind == EK_SOLDIER && d < SOLDIER_RANGE && d > 0.01f && lineClear(room, e.pos, player.pos, e.r + 4)) {
                e.moving = e.moving && d < SOLDIER_CLOSE;       // in bowshot: he stands, backing off if you crowd him
                if (e.moving) e.pos = moveInRoom(e.pos, Vector2Scale(to, -e.speed * edt / d), e.r, room, doorsOpen);
            }
            else if (e.moving) {
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
                // just found jammed: step back to the middle of its tile first, off whatever corner holds it
                Vector2 tileMid = { floorf(e.pos.x / TILE) * TILE + TILE / 2.0f, floorf(e.pos.y / TILE) * TILE + TILE / 2.0f };
                if (e.ghostT > 0.8f && Vector2Distance(tileMid, e.pos) > 2) goal = tileMid;
                Vector2 dir = Vector2Normalize(Vector2Subtract(goal, e.pos));
                if (e.kind == EK_SKULL) {
                    // wisps weave side to side, harder to hit with spells
                    Vector2 side = { -dir.y, dir.x };
                    dir = Vector2Normalize(Vector2Add(dir, Vector2Scale(side, sinf(e.wobble * 1.3f) * 0.8f)));
                }
                // only out in the open: a push off the path pins a wide body on a corner in a tight lane
                if (sees) dir = Vector2Normalize(Vector2Add(dir, Vector2Scale(wallAvoid(room, e.pos, e.r, doorsOpen), 1.2f)));
                // hounds that see you crouch and lunge; demons leap from further off
                bool leaps = e.kind == EK_SKELETON2 || e.kind == EK_DEMON;
                if (leaps && sees && d < (e.kind == EK_DEMON ? 190 : 130) && d > 40 && e.lungeCd <= 0) { e.lunge = 0.35f; e.lungeCd = 2.5f; }
                if (e.kind == EK_DEMON && e.lunge > 0 && frand(0, 1) < edt * 70)     // a streak of embers behind it
                    parts.push_back({ { e.pos.x + frand(-6, 6), e.pos.y - frand(0, 16) }, { frand(-12, 12), frand(-24, 6) }, 0.35f, 0.35f, frand(1.5f, 3.2f), { 200, 40, 50, 255 }, 0, true });
                float spd = e.speed * (e.lunge > 0 ? 2.6f : 1.0f);
                e.pos = moveInRoom(e.pos, Vector2Scale(dir, spd * edt), e.r, room, doorsOpen);
            }
        }

        // An enemy that stops getting closer is jammed behind its friends in a narrow gap:
        // let it slip through them for a moment.
        if (d < e.bestDist - 8) { e.bestDist = d; e.stuckT = 0; }
        else if ((e.stuckT += edt) > 1.5f) { e.ghostT = 1.2f; e.stuckT = 0; e.bestDist = d; }
        e.ghostT = fmaxf(0, e.ghostT - edt);

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
            if (&o == &e || !o.alive || e.ghostT > 0 || o.ghostT > 0) continue;
            Vector2 sep = Vector2Subtract(e.pos, o.pos);
            float sd = Vector2Length(sep), need = e.r + o.r;
            if (sd >= need) continue;
            Vector2 n = sd > 0.01f ? Vector2Scale(sep, 1.0f / sd) : Vector2{ 0, 1 };
            e.pos = moveInRoom(e.pos, Vector2Scale(n, (need - sd) * 0.5f), e.r, room, doorsOpen);
        }

        // A swing takes a moment: what it does comes partway through the animation
        if (e.swinging && e.atkAnim <= (e.kind == EK_SOLDIER ? 2 : 3) / attackFps(e.kind)) {
            e.swinging = false;
            if (e.kind != EK_ORC) enemyShoot(e);
            else if (d < e.r + PLAYER_R + ORC_REACH && !untouchable) {
                hurtPlayer((float)e.dmg, 0.4f);
                shake = fmaxf(shake, 0.2f);
                play("bonk");
                untouchable = true;
            }
        }
        if (shotGap(e) > 0 && !scared && (e.shootCd -= edt) <= 0 && d < 340 && lineClear(room, e.pos, player.pos, 4)) {
            e.shootCd = shotGap(e);
            if (e.kind == EK_SKULL) enemyShoot(e);     // a wisp has no wind-up to show
            else {
                e.swinging = true;
                e.atkAnim = SPR.enemy[e.kind].attack.length(attackFps(e.kind));
            }
        }
        if (e.kind == EK_ORC) {                    // it winds up, so you can step out of the way
            if (!e.swinging && e.atkCd <= 0 && d < e.r + PLAYER_R + 5 && !scared) {
                e.swinging = true; e.atkCd = 1.6f;
                e.atkAnim = SPR.enemy[e.kind].attack.length(attackFps(e.kind));
                play("swing");
            }
            continue;
        }

        // Toshko doesn't hit you: he snatches up to 50 gold and runs
        if (e.kind == EK_TOSHKO) {
            if (e.stolen == 0 && d < e.r + PLAYER_R + 4 && !scared && player.gold > 0) {
                e.stolen = std::min(50, player.gold);
                player.gold -= e.stolen;
                e.atkAnim = SPR.enemy[e.kind].attack.length(attackFps(e.kind));
                showBanner(TextFormat("Toshko snatched %d gold!", e.stolen), "Kill him before he gets away", 2.0f);
                play("coin");
            }
            continue;
        }

        // touch damage
        if (d < e.r + PLAYER_R + 4 && e.atkCd <= 0 && !untouchable && !scared) {
            hurtPlayer((float)e.dmg, 0.4f);
            e.atkCd = 1.0f;
            if (e.kind != EK_SKULL && e.bossForm < 0) e.atkAnim = SPR.enemy[e.kind].attack.length(attackFps(e.kind));
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
    if (e.elite) n += 15;
    if (e.kind == EK_MIMIC) n += 10;
    if (e.stolen > 0) {                    // caught Toshko: every snatched coin drops back
        n += e.stolen;
        showBanner("Your gold is back", "", 1.6f);
    }
    for (int k = 0; k < n; ++k) coins.push_back({ e.pos, { frand(-90, 90), frand(-140, -40) }, 0 });
    if (e.kind == EK_BLOOD && e.scale >= 1)    // it bursts into two smaller ones
        for (float side : { -1.0f, 1.0f }) {
            Enemy half = e;
            half.alive = true; half.hp = half.maxHp = e.maxHp * 0.35f;
            half.scale = 0.7f; half.r = 10; half.speed = e.speed * 1.4f; half.xp = e.xp / 3;
            half.pos.x += side * 12; half.knock = { side * 120, 0 };
            half.hitFlash = 0; half.burnT = 0;
            spawned.push_back(half);
        }
    if (e.kind == EK_GAKEV || e.elite || (e.kind == EK_MIMIC && GetRandomValue(0, 1)))
        coins.push_back({ e.pos, { frand(-40, 40), -160 }, 0, true });
    if (e.elite) { showBanner(TextFormat("%s falls", e.name), "+15 gold and a cigarette pack", 2.0f); hapticBuzz(120); }
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
        if (mode == Mode::Run) player.spins++;     // the prize wheel comes up as soon as it can
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
        int worth = player.has(VP_PEACH) && GetRandomValue(1, 4) == 1 ? 2 : 1;
        player.gold += worth; stats.goldEarned += worth;
        play("coin");
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
        int lock = dungeon.doorLock(dir);
        if (!lock) continue;
        bool at = dir == DIR_N ? p.y < TILE + 8 && fabsf(p.x - dcx) < TILE
                : dir == DIR_S ? p.y > (ROOM_TH - 1) * TILE - 8 && fabsf(p.x - dcx) < TILE
                : dir == DIR_W ? p.x < TILE + 8 && fabsf(p.y - dcy) < TILE
                :                p.x > (ROOM_TW - 1) * TILE - 8 && fabsf(p.y - dcy) < TILE;
        if (!at) continue;
        p.y += dir == DIR_N ? 14.0f : dir == DIR_S ? -14.0f : 0.0f;
        p.x += dir == DIR_W ? 14.0f : dir == DIR_E ? -14.0f : 0.0f;
        if (lock == 1) showBanner("The lair is locked", "Find the boss key on this floor", 1.8f);
        else showBanner("The treasure room is locked", "Find the silver key on this floor", 1.8f);
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
    autosave();
}

// --------------------------------------------------------------------- shops
void Game::buy(int i) {
    const ShopItem& it = SHOP_ITEMS[i];
    if (it.vape >= 0 && player.has((Vape)it.vape)) return;
    if (it.vape < 0 && player.potions[i] >= POTION_MAX) { shopMsgT = 1.5f; shopMsg = "Your belt is full"; play("hurt"); return; }
    if (player.gold < it.price) { shopMsgT = 1.5f; shopMsg = "Not enough gold"; play("hurt"); return; }
    player.gold -= it.price;
    if (it.vape >= 0) { player.vapes |= 1u << it.vape; play("levelup"); }
    else {
        player.potions[i]++;          // into the belt, for when you need it
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
