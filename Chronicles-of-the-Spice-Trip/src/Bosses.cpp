// ============================================================================
//  Bosses.cpp — boss table, boss AI and boss drawing.
//  The patterns follow the original Path boss fights, scaled to a room.
// ============================================================================
#include "Bosses.h"
#include "Champions.h"
#include "Render.h"
#include "Sprites.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
#include <string>

const BossDef BOSSES[BOSS_COUNT] = {
    { "The Good Plazmodium", "Multiplies when it dies", "thegoodplazmodium.png", { 0.00f, 0.325f, 0.50f, 0.231f },
      "arenas/goodplazmodium.png", BossKind::Split, 260, 26, 80, 12, { 95, 208, 106, 255 }, "Fire", 3, 1.1f },
    { "The Bad Plasmodium", "Multiplies when it dies", "thebadplasmodium.png", { 0.04f, 0.300f, 0.49f, 0.226f },
      "arenas/badplasmodium.png", BossKind::Split, 320, 26, 80, 14, { 154, 95, 208, 255 }, "Lightning", 5, 1.1f },
    { "Mordekai", "Fans of feathers, and a dive", "mordekai.png", { 0.22f, 0.300f, 0.70f, 0.323f },
      "arenas/mordekai.png", BossKind::Feathers, 380, 26, 85, 14, { 74, 122, 208, 255 }, "Fire", 7, 1.6f },
    { "Vlado", "The conductor of bus No.9", "vlado.png", { 0.18f, 0.170f, 0.62f, 0.286f },
      "arenas/vlado.png", BossKind::Conductor, 440, 26, 70, 14, { 90, 130, 200, 255 }, "Lightning", 0, 2.0f },
    { "Triceps", "Every attack is real physics", "triceps.png", { 0.047f, 0.0295f, 0.156f, 0.0722f },
      "arenas/triceps.png", BossKind::Physics, 480, 24, 95, 14, { 120, 210, 255, 255 }, "Water", 3, 1.5f,
      "walk-triceps.png", "triceps-poses.png" },
    { "Pyraketh, the Cinder-Maw", "Lord of the lowest floor", nullptr, { 0, 0, 1, 1 },
      nullptr, BossKind::Inferno, 520, 24, 55, 26, { 255, 120, 50, 255 }, "Water", 3, 1.6f },
};

int bossIndexForFloor(int floor) { return (floor - 1) % BOSS_COUNT; }
float bossStrength(int floor) { return 1.0f + 0.5f * ((floor - 1) / BOSS_COUNT); }

namespace {
    // Plasmodium globs
    constexpr float ORB_SPEED = 200, ORB_R = 6, SPREAD_ANGLE = 0.22f;
    constexpr int   ORB_DAMAGE = 8;
    // The Plasmodia multiply: three copies, and each copy may leave a tiny one
    constexpr int   COPIES = 3;
    constexpr float COPY_HP = 40, COPY_R = 15, COPY_SPEED = 120, COPY_INTERVAL = 1.4f;
    constexpr float TINY_CHANCE = 0.4f, TINY_HP = 14, TINY_R = 9, TINY_SPEED = 105;
    // Mordekai
    constexpr float FAN_STEP = 0.15f, FEATHER_SPEED = 250, DIVE_SPEED = 360, DIVE_TIME = 0.6f;
    constexpr int   FEATHER_DAMAGE = 6, DIVE_EVERY = 3;
    // Vlado
    constexpr int   SPIRAL_COUNT = 12, TICKET_DAMAGE = 6, BUS_DAMAGE = 20;
    constexpr float SPIRAL_TURN = 0.26f, TICKET_SPEED = 170;
    constexpr float WHISTLE_EVERY = 8, BUS_WARN = 1.0f, BUS_SPEED = 640, BUS_W = 140, BUS_H = 52;
    // The Cinder-Maw
    constexpr int   RING_COUNT = 10, EMBER_DAMAGE = 8;
    constexpr float RING_EVERY = 3.2f, RING_SPEED = 150, FIREBALL_SPEED = 220;
    // Triceps: walks between attacks, which take turns (grenade, boots, staff, circuit)
    constexpr float LAB_WALK = 1.5f, LAB_WALK_WOUNDED = 0.9f, WINDUP = 0.5f, CHASE_TIME = 1.6f, CHASE_MUL = 1.7f;
    // gravity grenade: a parabola, a well that pulls (F = ma), then the blast
    constexpr float GRENADE_FLIGHT = 0.9f, GRENADE_PEAK = 70, HAND_H = 40;
    constexpr float WELL_TIME = 2.0f, WELL_R = 120, WELL_PULL = 95, BLAST_R = 58, BLAST_TIME = 0.25f;
    constexpr int   BLAST_DAMAGE = 16;
    // magnetic boots: each one's velocity turns at a steady rate (F = q v x B)
    constexpr float BOOT_SPEED = 230, BOOT_TURN = 1.4f;
    constexpr int   BOOT_DAMAGE = 9;
    // fulcrum staff: wound up, swung round her, and the far end bats iron balls at you
    constexpr float STAFF_LEN = 96, STAFF_SWEEP = 3.3f, SWING_TIME = 0.32f, STAFF_REACH = 110;
    constexpr float BALL_SPEED = 300, BALL_SPREAD = 0.2f;
    constexpr int   STAFF_DAMAGE = 12, BALL_DAMAGE = 8;
    // circuit chain: the nodes charge, then the wires carry current
    constexpr float CHARGE_TIME = 1.0f, LIVE_TIME = 1.6f, WIRE_W = 5, SHOCK_EVERY = 0.5f;
    constexpr int   WIRE_DAMAGE = 10;
    constexpr float FORMULA_TIME = 2.0f;

    float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Keep a body's centre inside the room's walls.
    Vector2 insideRoom(Vector2 p, float r) {
        return { clampf(p.x, TILE + r, ROOM_PX_W - TILE - r), clampf(p.y, TILE + r, ROOM_PX_H - TILE - r) };
    }

    float segmentDistance(Vector2 p, Vector2 a, Vector2 b) {
        Vector2 ab = Vector2Subtract(b, a);
        float len2 = Vector2DotProduct(ab, ab);
        float k = len2 > 0 ? clampf(Vector2DotProduct(Vector2Subtract(p, a), ab) / len2, 0, 1) : 0;
        return Vector2Distance(p, Vector2Add(a, Vector2Scale(ab, k)));
    }
}

Vector2 Grenade::at() const { return Vector2Lerp(from, to, clampf(t / flight, 0, 1)); }
float Grenade::height() const {
    float k = clampf(t / flight, 0, 1);
    return HAND_H * (1 - k) + 4 * peak * k * (1 - k);
}
float Staff::angle() const { return from + sweep * clampf(t / SWING_TIME, 0, 1); }

// ----------------------------------------------------------------------- fight
Enemy BossFight::body(int form, Vector2 at) const {
    Enemy e;
    e.name = def->name; e.weakness = def->weakness; e.color = def->color;
    e.boss = true; e.bossForm = form;
    e.kind = def->kind == BossKind::Inferno ? EK_VAMPIRE : EK_SKELETON;
    e.wobble = frand(0, 6.28f); e.animT = frand(0, 1);
    e.pos = insideRoom(at, def->radius); e.roamTo = e.pos;
    if (form == 0) {
        e.hp = e.maxHp = def->hp * strength; e.r = def->radius; e.speed = def->speed;
        e.dmg = (int)(def->contact * strength); e.xp = 150;
        e.spread = def->spread; e.fireInterval = def->fireInterval;
        e.shootCd = 1.6f;          // a moment to take in the arena
    }
    else if (form == 1) {
        e.hp = e.maxHp = COPY_HP * strength; e.r = COPY_R; e.speed = COPY_SPEED;
        e.dmg = (int)(8 * strength); e.xp = 14;
        e.spread = def->spread >= 5 ? 3 : 1; e.fireInterval = COPY_INTERVAL;
        e.shootCd = frand(0.6f, 1.2f); e.scale = COPY_R / def->radius;
    }
    else {
        e.hp = e.maxHp = TINY_HP * strength; e.r = TINY_R; e.speed = TINY_SPEED;
        e.dmg = (int)(6 * strength); e.xp = 6;
        e.chaser = true; e.scale = TINY_R / def->radius;
    }
    return e;
}

void BossFight::begin(const BossDef& d, float s, Vector2 at, std::vector<Enemy>& enemies) {
    end();
    def = &d; strength = s; time = 0;
    spiralT = 1.6f; spiralBase = 0; whistleT = 4.0f; emberT = 2.0f; aimT = 1.0f;
    lab = Lab::Walk; labT = 1.6f; attack = 0;
    Enemy boss = body(0, at);
    poolMax = boss.maxHp + (d.kind == BossKind::Split ? COPIES * COPY_HP * s : 0);
    enemies.push_back(boss);
}

void BossFight::end() {
    def = nullptr; bus = Bus{};
    for (auto& g : grenades) g = Grenade{};
    circuit = Circuit{}; staff = Staff{};
    formula = nullptr; formulaT = 0;
}

void BossFight::shoot(const Enemy& from, float angle, float speed, float r, int damage, ShotKind kind,
    std::vector<Projectile>& shots) const {
    shootAt(Vector2Add(from.pos, Vector2Scale(fromAngle(angle), from.r * 0.6f)), angle, speed, r, damage, kind, shots);
}

void BossFight::shootAt(Vector2 at, float angle, float speed, float r, int damage, ShotKind kind,
    std::vector<Projectile>& shots) const {
    Projectile p;
    p.pos = at;
    p.vel = Vector2Scale(fromAngle(angle), speed);
    p.r = r; p.life = 4.0f; p.kind = kind;
    p.damage = (int)(damage * strength + 0.5f);
    p.color = def->color; p.glow = ColorBrightness(def->color, 0.5f);
    shots.push_back(p);
}

void BossFight::roam(Enemy& e, float dt, const Room& room) {
    e.moveCd -= dt;
    if (e.moveCd <= 0) {
        e.moveCd = 1.4f + frand(0, 1.2f);
        e.roamTo = { frand(TILE * 3.0f, ROOM_PX_W - TILE * 3.0f), frand(TILE * 2.5f, ROOM_PX_H - TILE * 2.5f) };
    }
    Vector2 to = Vector2Subtract(e.roamTo, e.pos);
    float d = Vector2Length(to);
    if (d > 2) e.pos = moveInRoom(e.pos, Vector2Scale(to, fminf(e.speed * dt, d) / d), e.r, room, false);
    e.moving = d > 2;
}

void BossFight::update(float dt, Vector2 player, std::vector<Enemy>& enemies, std::vector<Projectile>& shots,
    const Room& room, float slow) {
    if (!def) return;
    time += dt;
    float bdt = dt * slow;
    const Enemy* primary = nullptr;

    for (auto& e : enemies) {
        if (!e.alive || e.bossForm < 0) continue;
        if (e.bossForm == 0) primary = &e;
        Vector2 to = Vector2Subtract(player, e.pos);
        float d = fmaxf(0.01f, Vector2Length(to));
        if (fabsf(to.x) > 2) e.flip = to.x < 0;

        if (scared) {               // run from the player and hold every attack
            e.diveT = 0;
            e.pos = moveInRoom(e.pos, Vector2Scale(to, -e.speed * FLEE_SPEED * bdt / d), e.r, room, false);
            e.moving = true;
            e.look = Vector2Scale(to, -1 / d); e.pose = -1;
            if (def->kind == BossKind::Physics && lab != Lab::Walk) {   // the attack she was winding up is dropped
                lab = Lab::Walk; labT = 0.6f;
                if (!staff.swinging) staff = Staff{};
            }
            continue;
        }

        // --- movement ---
        if (def->kind == BossKind::Physics) { updateLab(e, bdt, player, shots, room); continue; }
        if (e.diveT > 0) {
            e.diveT -= bdt;
            e.pos = moveInRoom(e.pos, Vector2Scale(e.diveDir, DIVE_SPEED * bdt), e.r, room, false);
            e.moving = true;
        }
        else if (e.chaser || def->kind == BossKind::Inferno) {
            float spd = e.speed * (def->kind == BossKind::Inferno && e.hp < e.maxHp * 0.4f ? 1.5f : 1.0f);
            e.pos = moveInRoom(e.pos, Vector2Scale(to, spd * bdt / d), e.r, room, false);
            e.moving = true;
        }
        else roam(e, bdt, room);

        // --- aimed volleys (Plasmodia and Mordekai) ---
        if (e.chaser || def->kind == BossKind::Conductor || def->kind == BossKind::Inferno) continue;
        e.shootCd -= bdt;
        if (e.shootCd > 0) continue;
        e.shootCd = e.fireInterval;
        float base = atan2f(to.y, to.x);
        if (def->kind == BossKind::Feathers) {
            for (int i = 0; i < e.spread; ++i)
                shoot(e, base + (i - (e.spread - 1) / 2.0f) * FAN_STEP, FEATHER_SPEED, 4, FEATHER_DAMAGE, ShotKind::Feather, shots);
            if (sfx) sfx("nature");
            if (++e.volleys % DIVE_EVERY == 0) { e.diveDir = Vector2Scale(to, 1.0f / d); e.diveT = DIVE_TIME; }
        }
        else {
            int half = e.spread / 2;
            for (int s = -half; s <= half; ++s)
                shoot(e, base + s * SPREAD_ANGLE, ORB_SPEED, ORB_R * (e.bossForm == 0 ? 1.0f : 0.8f), ORB_DAMAGE, ShotKind::Orb, shots);
            if (sfx) sfx("lightning");
        }
    }

    // --- Vlado: ticket spirals and the whistle ---
    if (def->kind == BossKind::Conductor && primary && !scared) {
        spiralT -= bdt;
        if (spiralT <= 0) {
            spiralT = def->fireInterval;
            for (int i = 0; i < SPIRAL_COUNT; ++i)
                shoot(*primary, spiralBase + i * 2 * PI / SPIRAL_COUNT, TICKET_SPEED, 5, TICKET_DAMAGE, ShotKind::Ticket, shots);
            spiralBase += SPIRAL_TURN;
            if (sfx) sfx("ui");
        }
        whistleT -= bdt;
        if (whistleT <= 0 && !bus.active) {
            whistleT = WHISTLE_EVERY;
            bus = Bus{};
            bus.active = true; bus.w = BUS_W; bus.h = BUS_H; bus.warn = BUS_WARN;
            bus.y = clampf(player.y, TILE + BUS_H / 2, ROOM_PX_H - TILE - BUS_H / 2);
            bus.x = -BUS_W / 2;
            if (sfx) sfx("whistle");
        }
    }

    // --- the Cinder-Maw: ember rings, and fireballs once it is wounded ---
    if (def->kind == BossKind::Inferno && primary && !scared) {
        emberT -= bdt;
        if (emberT <= 0) {
            emberT = RING_EVERY;
            float off = frand(0, PI);
            for (int i = 0; i < RING_COUNT; ++i)
                shoot(*primary, off + i * 2 * PI / RING_COUNT, RING_SPEED, 6, EMBER_DAMAGE, ShotKind::Ember, shots);
            if (sfx) sfx("fire");
        }
        if (primary->hp < primary->maxHp * 0.5f) {
            aimT -= bdt;
            if (aimT <= 0) {
                aimT = def->fireInterval;
                Vector2 to = Vector2Subtract(player, primary->pos);
                float base = atan2f(to.y, to.x);
                for (int s = -1; s <= 1; ++s)
                    shoot(*primary, base + s * SPREAD_ANGLE, FIREBALL_SPEED, 7, EMBER_DAMAGE, ShotKind::Ember, shots);
            }
        }
    }

    if (def->kind == BossKind::Physics) updateHazards(bdt, player, shots);   // what she threw keeps going while she runs
    updateBus(bdt, player);
}

// ------------------------------------------------------------------- Triceps
void BossFight::updateLab(Enemy& e, float dt, Vector2 player, std::vector<Projectile>& shots, const Room& room) {
    Vector2 to = Vector2Subtract(player, e.pos);
    float d = fmaxf(0.01f, Vector2Length(to));
    labT -= dt;
    switch (lab) {
    case Lab::Walk:             // she walks her cave like a champion, facing the way she goes
        roam(e, dt, room);
        e.look = e.moving ? Vector2Normalize(Vector2Subtract(e.roamTo, e.pos)) : Vector2Scale(to, 1 / d);
        e.pose = -1;
        if (labT > 0) break;
        if (attack == 2 && d > STAFF_REACH) { lab = Lab::Chase; labT = CHASE_TIME; }   // the staff needs her close
        else windUp(e, player);
        break;
    case Lab::Chase:            // runs at you with the staff
        e.pos = moveInRoom(e.pos, Vector2Scale(to, fmaxf(0, fminf(e.speed * CHASE_MUL * dt, d - 30)) / d), e.r, room, false);
        e.moving = true; e.look = Vector2Scale(to, 1 / d);
        if (d < STAFF_REACH * 0.8f || labT <= 0) windUp(e, player);
        break;
    case Lab::WindUp:           // the attack pose, aimed at you (the staff stays where it was wound up)
        e.moving = false;
        if (attack != 2) e.look = Vector2Scale(to, 1 / d);
        if (labT > 0) break;
        release(e, player, shots);
        lab = Lab::Recover; labT = attack == 2 ? SWING_TIME + 0.3f : 0.4f;
        break;
    case Lab::Recover:          // holds the pose a moment (and points while her circuit is on)
        e.moving = false;
        if (labT > 0 || (attack == 3 && circuit.active)) break;
        e.pose = -1;
        attack = (attack + 1) % 4;
        lab = Lab::Walk; labT = e.hp < e.maxHp * 0.5f ? LAB_WALK_WOUNDED : LAB_WALK;
        break;
    }
}

void BossFight::windUp(Enemy& e, Vector2 player) {
    Vector2 to = Vector2Subtract(player, e.pos);
    lab = Lab::WindUp; labT = WINDUP;
    e.pose = attack; e.moving = false;
    if (Vector2Length(to) > 0.01f) e.look = Vector2Normalize(to);
    if (attack == 2) {          // the staff's reach shows on the floor while she winds up
        staff = Staff{};
        staff.active = true; staff.at = e.pos;
        staff.sweep = STAFF_SWEEP * (frand(0, 1) < 0.5f ? 1.0f : -1.0f);
        staff.from = atan2f(to.y, to.x) - staff.sweep / 2;
    }
}

void BossFight::release(Enemy& e, Vector2 player, std::vector<Projectile>& shots) {
    bool wounded = e.hp < e.maxHp * 0.5f;
    Vector2 to = Vector2Subtract(player, e.pos);
    float d = fmaxf(1, Vector2Length(to)), aim = atan2f(to.y, to.x);
    Vector2 over = { e.pos.x, e.pos.y - e.r * 3.4f };          // above her head
    switch (attack) {
    case 0: {   // gravity grenade: a parabola onto where you stand (and one beside you once she's wounded)
        Vector2 side = { -to.y / d, to.x / d };
        for (int i = 0; i < (wounded ? 2 : 1); ++i) {
            Grenade& g = grenades[i];
            g = Grenade{};
            g.active = true; g.from = e.pos; g.flight = GRENADE_FLIGHT; g.peak = GRENADE_PEAK;
            g.to = insideRoom(i == 0 ? player : Vector2Add(player, Vector2Scale(side, frand(0, 1) < 0.5f ? 90.0f : -90.0f)), 24);
        }
        showFormula("y = x tan(a) - gx^2 / 2(v cos a)^2", over);
        if (sfx) sfx("throw");
        break;
    }
    case 1: {   // magnetic boots: each turns at a steady rate, so the outer ones swing wide and close in on you
        static const float turns[5] = { 0, 1, -1, 2, -2 };
        for (int i = 0; i < (wounded ? 5 : 3); ++i) {
            float w = turns[i] * BOOT_TURN;
            // a circle of radius v/w through you: leave at half the arc's angle off the straight line
            float half = asinf(fminf(0.95f, d * fabsf(w) / (2 * BOOT_SPEED)));
            shoot(e, aim - (w > 0 ? half : w < 0 ? -half : 0), BOOT_SPEED, 7, BOOT_DAMAGE, ShotKind::Boot, shots);
            shots.back().curve = w; shots.back().life = 2.6f;
        }
        showFormula("F = q(E + v x B)", over);
        if (sfx) sfx("kick");
        break;
    }
    case 2:     // the fulcrum staff swings round (drawn turning, so she stands without the posed one)
        staff.swinging = true; staff.t = 0; staff.hit = false;
        e.pose = -1;
        showFormula("T = r x F", over);
        if (sfx) sfx("swing");
        break;
    case 3: {   // the circuit chain: the first wire runs through where you stand
        circuit = Circuit{};
        circuit.active = true; circuit.charge = CHARGE_TIME; circuit.live = LIVE_TIME;
        circuit.count = wounded ? 5 : 4;
        circuit.node[0] = e.pos;
        circuit.node[1] = insideRoom(Vector2Add(player, Vector2Scale(to, 80 / d)), 20);
        for (int i = 2; i < circuit.count; ++i) {
            Vector2 best = circuit.node[i - 1];
            float bestGap = -1;
            for (int tries = 0; tries < 10; ++tries) {      // a spot well clear of the other nodes
                Vector2 p = insideRoom(Vector2Add(circuit.node[i - 1], Vector2Scale(fromAngle(frand(0, 2 * PI)), frand(110, 190))), 20);
                float gap = 1e9f;
                for (int j = 0; j < i; ++j) gap = fminf(gap, Vector2Distance(p, circuit.node[j]));
                if (gap > bestGap) { bestGap = gap; best = p; }
            }
            circuit.node[i] = best;
        }
        showFormula("I = V / R", over);
        if (sfx) sfx("ui");
        break;
    }
    }
}

void BossFight::updateHazards(float dt, Vector2 player, std::vector<Projectile>& shots) {
    formulaT += dt;
    for (auto& g : grenades) {
        if (!g.active) continue;
        if (g.t < g.flight) {
            g.t += dt;
            if (g.t >= g.flight) { g.well = WELL_TIME; showFormula("F = ma", { g.to.x, g.to.y - 34 }); if (sfx) sfx("well"); }
        }
        else if (g.well > 0) {
            g.well -= dt;
            if (g.well <= 0) { g.boom = BLAST_TIME; if (sfx) sfx("boom"); }
        }
        else if ((g.boom -= dt) <= 0) g.active = false;
    }
    if (circuit.active) {
        circuit.shockCd -= dt;
        if (circuit.charge > 0) { if ((circuit.charge -= dt) <= 0 && sfx) sfx("lightning"); }
        else if ((circuit.live -= dt) <= 0) circuit.active = false;
    }
    if (staff.swinging && (staff.t += dt) >= SWING_TIME) {     // the far end bats iron balls at you: F1 d1 = F2 d2
        Vector2 tip = Vector2Add(staff.at, Vector2Scale(fromAngle(staff.angle()), STAFF_LEN));
        Vector2 to = Vector2Subtract(player, tip);
        float aim = atan2f(to.y, to.x);
        for (int s = -1; s <= 1; ++s) shootAt(tip, aim + s * BALL_SPREAD, BALL_SPEED, 5, BALL_DAMAGE, ShotKind::Ball, shots);
        showFormula("F1 d1 = F2 d2", { tip.x, tip.y - 20 });
        staff = Staff{};
    }
}

Vector2 BossFight::gravityPull(Vector2 p) const {
    Vector2 v{ 0, 0 };
    if (!def) return v;
    for (auto& g : grenades) {
        if (!g.active || g.well <= 0) continue;
        Vector2 to = Vector2Subtract(g.to, p);
        float d = Vector2Length(to);
        if (d < 3 || d > WELL_R) continue;
        v = Vector2Add(v, Vector2Scale(to, WELL_PULL * (0.35f + 0.65f * (1 - d / WELL_R)) / d));   // stronger near the middle
    }
    return v;
}

int BossFight::hazardHit(Vector2 p, float r) {
    if (!def) return 0;
    if (staff.swinging && !staff.hit) {         // inside the arc swept so far
        Vector2 off = Vector2Subtract(p, staff.at);
        float d = Vector2Length(off);
        if (d < STAFF_LEN + r) {
            float side = staff.sweep < 0 ? -1.0f : 1.0f;
            float rel = fmodf(side * (atan2f(off.y, off.x) - staff.from) + 4 * PI, 2 * PI);
            float swept = fabsf(staff.sweep) * clampf(staff.t / SWING_TIME, 0, 1);
            if (rel <= swept + (d > r ? asinf(r / d) : PI)) {
                staff.hit = true;          // the far end moves fastest and hits hardest
                return (int)(STAFF_DAMAGE * (0.6f + 0.8f * clampf(d / STAFF_LEN, 0, 1)) * strength + 0.5f);
            }
        }
    }
    for (auto& g : grenades)
        if (g.active && g.boom > 0 && !g.hit && Vector2Distance(p, g.to) < BLAST_R + r) {
            g.hit = true;
            return (int)(BLAST_DAMAGE * strength + 0.5f);
        }
    if (circuit.active && circuit.charge <= 0 && circuit.shockCd <= 0)
        for (int i = 0; i + 1 < circuit.count; ++i)
            if (segmentDistance(p, circuit.node[i], circuit.node[i + 1]) < r + WIRE_W) {
                circuit.shockCd = SHOCK_EVERY;
                return (int)(WIRE_DAMAGE * strength + 0.5f);
            }
    return 0;
}

void BossFight::updateBus(float dt, Vector2 /*player*/) {
    if (!bus.active) return;
    if (!bus.driving) {
        bus.warn -= dt;
        if (bus.warn <= 0) { bus.driving = true; if (sfx) sfx("bus"); }
        return;
    }
    bus.x += BUS_SPEED * dt;
    if (bus.x - bus.w / 2 > ROOM_PX_W) bus = Bus{};
}

void BossFight::onDeath(const Enemy& dead, std::vector<Enemy>& spawned) {
    if (!def || def->kind != BossKind::Split || dead.bossForm < 0) return;
    if (dead.bossForm == 0) {
        for (int i = 0; i < COPIES; ++i)
            spawned.push_back(body(1, Vector2Add(dead.pos, Vector2Scale(fromAngle(i * 2 * PI / COPIES - PI / 2), 26))));
        if (sfx) sfx("nature");
    }
    else if (dead.bossForm == 1 && frand(0, 1) < TINY_CHANCE) {
        spawned.push_back(body(2, dead.pos));
    }
}

bool BossFight::busHits(Vector2 p, float r) const {
    if (!bus.active || !bus.driving || bus.hit) return false;
    float cx = clampf(p.x, bus.x - bus.w / 2, bus.x + bus.w / 2);
    float cy = clampf(p.y, bus.y - bus.h / 2, bus.y + bus.h / 2);
    return Vector2Distance(p, { cx, cy }) < r;
}

int BossFight::busDamage() const { return (int)(BUS_DAMAGE * strength + 0.5f); }

float BossFight::health(const std::vector<Enemy>& enemies) const {
    if (!def) return 0;
    float hp = 0;
    bool firstForm = false;
    for (auto& e : enemies) {
        if (!e.alive || e.bossForm < 0) continue;
        hp += fmaxf(0, e.hp);
        if (e.bossForm == 0) firstForm = true;
    }
    if (firstForm && def->kind == BossKind::Split) hp += COPIES * COPY_HP * strength;   // copies still to come
    return clampf(hp / poolMax, 0, 1);
}

// ------------------------------------------------------------------------- art
// Cut the boss out of its character sheet as a round token with a soft edge.
static Texture2D makeToken(const BossDef& def) {
    std::string p = def.sheet ? findAsset(def.sheet) : "";
    if (p.empty()) return Texture2D{};
    Image img = LoadImage(p.c_str());
    ImageCrop(&img, { def.crop.x * img.width, def.crop.y * img.height, def.crop.width * img.width, def.crop.height * img.height });
    constexpr int SIZE = 192;
    ImageResize(&img, SIZE, SIZE);          // the crops are square
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px = (Color*)img.data;
    float c = (SIZE - 1) / 2.0f, radius = SIZE / 2.0f - 1;
    for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x) {
            float edge = clampf((radius - hypotf(x - c, y - c)) / 1.5f, 0, 1);
            px[y * SIZE + x].a = (unsigned char)(px[y * SIZE + x].a * edge);
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
}

void BossArt::load() {
    for (int i = 0; i < BOSS_COUNT; ++i) {
        token[i] = makeToken(BOSSES[i]);
        if (BOSSES[i].walk) walk[i].load(BOSSES[i].walk);
        std::string pp = BOSSES[i].poses ? findAsset((std::string("walk/") + BOSSES[i].poses).c_str()) : "";
        if (!pp.empty()) {      // cut from a big sheet and drawn far smaller, like the walk sheet
            poses[i] = LoadTexture(pp.c_str());
            GenTextureMipmaps(&poses[i]);
            SetTextureFilter(poses[i], TEXTURE_FILTER_TRILINEAR);
        }
        std::string p = BOSSES[i].arena ? findAsset(BOSSES[i].arena) : "";
        if (p.empty()) continue;
        arena[i] = LoadTexture(p.c_str());
        SetTextureFilter(arena[i], TEXTURE_FILTER_BILINEAR);
    }
}

void BossArt::unload() {
    for (int i = 0; i < BOSS_COUNT; ++i) {
        if (token[i].id) UnloadTexture(token[i]);
        if (arena[i].id) UnloadTexture(arena[i]);
        if (poses[i].id) UnloadTexture(poses[i]);
        walk[i].unload();
        token[i] = arena[i] = poses[i] = Texture2D{};
    }
}

void drawBossToken(int index, const BossArt& art, Vector2 centre, float radius, Color tint) {
    const BossDef& def = BOSSES[index];
    Texture2D tx = art.token[index];
    if (!tx.id) {           // the Cinder-Maw is drawn from the dungeon's vampire sprite
        const Anim& a = SPR.enemy[EK_VAMPIRE].idle;
        DrawCircleV(centre, radius, Fade({ 40, 16, 12, 255 }, 0.9f * tint.a / 255.0f));
        drawAnim(a, 0, { centre.x, centre.y - radius * 0.3f }, radius * 2.6f / 32, false, tint);
        DrawRing(centre, radius - 2, radius, 0, 360, 48, Fade(def.color, tint.a / 255.0f));
        return;
    }
    DrawCircleV(centre, radius + 3, Fade(def.color, 0.35f * tint.a / 255.0f));          // glow
    DrawTexturePro(tx, { 0, 0, (float)tx.width, (float)tx.height }, { centre.x, centre.y, radius * 2, radius * 2 },
        { radius, radius }, 0, tint);
    DrawRing(centre, radius - 1.5f, radius + 1, 0, 360, 48, Fade(def.color, tint.a / 255.0f));
}

// A walking boss stands this many radii tall, feet a little below her centre.
static constexpr float FIGURE_H = 3.4f, FEET = 0.7f;

// Triceps walks as her own figure, facing the way she goes, and strikes a pose to attack.
static void drawWalkingBoss(const Enemy& e, int index, const BossArt& art) {
    float alpha = e.dying ? fmaxf(0, 1 - e.deathT / 0.45f) : 1;
    float h = e.r * FIGURE_H, bottom = e.pos.y + e.r * FEET;
    Color tint = Fade(WHITE, alpha);
    bool flash = e.hitFlash > 0 && !e.dying;
    if (!e.dying) drawShadow(e.pos, e.r);
    Texture2D poses = art.poses[index];
    if (e.pose >= 0 && poses.id && !e.dying) {    // the poses face right, with her feet in the middle of each frame
        float cw = poses.width / 4.0f, pw = h * cw / poses.height;
        Rectangle src = { e.pose * cw, 0, e.look.x < 0 ? -cw : cw, (float)poses.height };
        Rectangle dst = { e.pos.x - pw / 2, bottom - h, pw, h };
        DrawTexturePro(poses, src, dst, { 0, 0 }, 0, tint);
        if (flash) { BeginBlendMode(BLEND_ADDITIVE); DrawTexturePro(poses, src, dst, { 0, 0 }, 0, WHITE); EndBlendMode(); }
        return;
    }
    const WalkSheet& w = art.walk[index];
    float fw = h * 3 / 4;
    w.draw(facingOf(e.look), w.frameAt(e.moving && !e.dying, e.animT), { e.pos.x - fw / 2, bottom - h, fw, h }, tint, flash);
}

float bossTop(const Enemy& e, int index, const BossArt& art) {
    if (art.walk[index].ok()) return e.pos.y + e.r * (FEET - FIGURE_H);
    return e.pos.y - e.r * 0.4f - e.r * 1.3f;      // the round token
}

void drawBossBody(const Enemy& e, int index, const BossArt& art) {
    if (art.walk[index].ok()) { drawWalkingBoss(e, index, art); return; }
    if (!art.token[index].id) { drawEnemy(e); return; }
    float bob = e.dying ? 0 : sinf(e.wobble * 0.8f) * 2.5f;
    float alpha = e.dying ? fmaxf(0, 1 - e.deathT / 0.45f) : 1;
    float grow = e.dying ? 1 + e.deathT * 0.6f : 1;
    if (!e.dying) drawShadow(e.pos, e.r);
    Vector2 at = { e.pos.x, e.pos.y - e.r * 0.4f + bob };
    float radius = e.r * 1.3f * grow;
    drawBossToken(index, art, at, radius, Fade(WHITE, alpha));
    if (e.hitFlash > 0) {          // struck: flash bright
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleV(at, radius, Fade(WHITE, 0.35f));
        EndBlendMode();
    }
    if (e.bossForm > 0 && !e.dying && e.hp < e.maxHp) {   // copies get their own little bar
        float bw = e.r * 2.4f, top = at.y - radius - 7;
        DrawRectangle((int)(e.pos.x - bw / 2), (int)top, (int)bw, 3, { 20, 10, 10, 200 });
        DrawRectangle((int)(e.pos.x - bw / 2), (int)top, (int)(bw * (e.hp / e.maxHp)), 3, BOSSES[index].color);
    }
}

void drawBus(const Bus& bus, float t) {
    if (!bus.active) return;
    float top = bus.y - bus.h / 2;
    if (!bus.driving) {   // the warning lane flashes
        float a = 0.18f + 0.14f * sinf(t * 22);
        DrawRectangle(TILE, (int)top, (int)(ROOM_PX_W - 2 * TILE), (int)bus.h, Fade({ 230, 60, 50, 255 }, a));
        DrawRectangleLines(TILE, (int)top, (int)(ROOM_PX_W - 2 * TILE), (int)bus.h, Fade({ 255, 120, 100, 255 }, 0.8f));
        DrawText("BUS No.9!", TILE + 10, (int)bus.y - 10, 20, Fade({ 255, 220, 200, 255 }, 0.9f));
        return;
    }
    float x = bus.x - bus.w / 2;
    DrawRectangle((int)x + 6, (int)(top + bus.h - 6), (int)bus.w, 10, { 0, 0, 0, 90 });            // shadow
    DrawRectangle((int)x, (int)top, (int)bus.w, (int)bus.h - 6, { 232, 178, 48, 255 });             // body
    DrawRectangle((int)x, (int)top, (int)bus.w, 6, { 250, 214, 110, 255 });                         // roof
    for (int i = 0; i < 5; ++i)                                                                     // windows
        DrawRectangle((int)x + 10 + i * 24, (int)top + 10, 18, 14, { 60, 80, 110, 255 });
    DrawRectangle((int)(x + bus.w - 10), (int)top + 10, 8, 22, { 250, 250, 200, 255 });             // headlight
    DrawText("9", (int)(x + bus.w - 30), (int)top + 26, 20, { 40, 30, 20, 255 });
    for (float wx : { x + 26, x + bus.w - 34 })                                                     // wheels
        DrawCircle((int)wx, (int)(top + bus.h - 6), 8, { 24, 22, 26, 255 });
}

// ------------------------------------------------------------ Triceps' hazards
void drawLabFloor(const BossFight& f, float t) {
    if (!f.active() || f.def->kind != BossKind::Physics) return;
    Color c = f.def->color;

    // the staff's reach while she winds it up
    if (f.staff.active && !f.staff.swinging) {
        float a0 = f.staff.from, a1 = f.staff.from + f.staff.sweep;
        if (a1 < a0) std::swap(a0, a1);
        float pulse = 0.14f + 0.08f * sinf(t * 20);
        DrawCircleSector(f.staff.at, STAFF_LEN, a0 * RAD2DEG, a1 * RAD2DEG, 32, Fade({ 230, 70, 60, 255 }, pulse));
        DrawCircleSectorLines(f.staff.at, STAFF_LEN, a0 * RAD2DEG, a1 * RAD2DEG, 32, Fade({ 255, 130, 110, 255 }, 0.7f));
    }

    for (auto& g : f.grenades) {
        if (!g.active) continue;
        if (g.t < g.flight) {               // where it will land, and its shadow on the way
            float k = g.t / g.flight;
            DrawRing(g.to, BLAST_R - 2, BLAST_R, 0, 360, 48, Fade(c, 0.25f + 0.4f * k));
            DrawCircleV(g.to, 3, Fade(c, 0.8f));
            Vector2 s = g.at();
            DrawEllipse((int)s.x, (int)s.y, 6, 3, { 0, 0, 0, 90 });
        }
        else if (g.well > 0) {              // the well: rings falling inward round a dark core
            for (int i = 0; i < 4; ++i) {
                float k = fmodf(t * 0.9f + i / 4.0f, 1.0f), rr = WELL_R * (1 - k);
                DrawRing(g.to, fmaxf(0, rr - 2), rr, 0, 360, 48, Fade(c, 0.5f * k));
            }
            float urgency = g.well < 0.6f ? 0.5f + 0.5f * sinf(t * 30) : 0;     // about to go off
            DrawRing(g.to, BLAST_R - 2, BLAST_R, 0, 360, 48, Fade({ 255, 150, 90, 255 }, 0.3f + 0.5f * urgency));
            DrawCircleV(g.to, 9, { 20, 24, 40, 230 });
            DrawCircleLinesV(g.to, 9, c);
        }
        else {                              // the blast
            float k = 1 - g.boom / BLAST_TIME;
            DrawCircleV(g.to, BLAST_R * (0.4f + 0.6f * k), Fade({ 255, 200, 120, 255 }, 0.6f * (1 - k)));
            DrawCircleV(g.to, BLAST_R * 0.5f * (0.4f + 0.6f * k), Fade(WHITE, 0.7f * (1 - k)));
        }
    }

    const Circuit& cc = f.circuit;
    if (!cc.active) return;
    bool live = cc.charge <= 0;
    float charged = live ? 1 : 1 - cc.charge / CHARGE_TIME;
    for (int i = 0; i + 1 < cc.count; ++i) {
        Vector2 a = cc.node[i], ab = Vector2Subtract(cc.node[i + 1], a);
        float len = fmaxf(1, Vector2Length(ab));
        Vector2 dir = Vector2Scale(ab, 1 / len), side = { -dir.y, dir.x };
        if (!live) {                        // a dashed wire, brighter as the nodes charge
            for (float s = 0; s < len; s += 12)
                DrawLineEx(Vector2Add(a, Vector2Scale(dir, s)), Vector2Add(a, Vector2Scale(dir, fminf(len, s + 6))), 1.5f, Fade(c, 0.25f + 0.5f * charged));
            continue;
        }
        int n = std::max(3, (int)(len / 14));   // current arcs along it, jagged and flickering
        Vector2 prev = a;
        for (int s = 1; s <= n; ++s) {
            Vector2 q = Vector2Add(Vector2Add(a, Vector2Scale(dir, len * s / n)), Vector2Scale(side, s < n ? frand(-6, 6) : 0));
            DrawLineEx(prev, q, 6, Fade(c, 0.3f));
            DrawLineEx(prev, q, 2, { 230, 250, 255, 255 });
            prev = q;
        }
    }
    static const char* LABELS[Circuit::MAX_NODES] = { "12V", "R1", "C1", "R2", "C2", "R3" };
    for (int i = 0; i < cc.count; ++i) {
        Vector2 p = cc.node[i];
        if (i > 0) {                        // a capacitor's two plates, and its charge filling round it (C = Q / V)
            DrawRectangle((int)p.x - 6, (int)p.y - 7, 3, 14, { 210, 230, 245, 255 });
            DrawRectangle((int)p.x + 3, (int)p.y - 7, 3, 14, { 210, 230, 245, 255 });
            DrawRing(p, 10, 12, -90, -90 + 360 * charged, 32, Fade(c, 0.85f));
        }
        ui::textCentered(LABELS[i], p.x, p.y + 13, 10, Fade(c, 0.9f));
    }
}

void drawLabAir(const BossFight& f, float t) {
    if (!f.active() || f.def->kind != BossKind::Physics) return;
    for (auto& g : f.grenades) {            // grenades on their parabola
        if (!g.active || g.t >= g.flight) continue;
        Vector2 s = g.at(), p = { s.x, s.y - g.height() };
        DrawCircleV(p, 10, Fade(f.def->color, 0.25f));
        DrawCircleV(p, 7, { 60, 64, 78, 255 });
        DrawCircleV({ p.x - 2, p.y - 2 }, 2.5f, { 190, 196, 210, 255 });
    }
    if (f.staff.swinging) {                 // the staff mid-swing, and the arc it has swept
        Vector2 pivot = { f.staff.at.x, f.staff.at.y - 16 };
        float a = f.staff.angle(), lo = fminf(f.staff.from, a), hi = fmaxf(f.staff.from, a);
        DrawCircleSector(pivot, STAFF_LEN, lo * RAD2DEG, hi * RAD2DEG, 32, Fade(WHITE, 0.16f));
        Vector2 dir = fromAngle(a);
        Vector2 tip = Vector2Add(pivot, Vector2Scale(dir, STAFF_LEN)), butt = Vector2Subtract(pivot, Vector2Scale(dir, 20));
        DrawLineEx(butt, tip, 5, { 50, 56, 80, 255 });
        DrawLineEx(butt, tip, 2.5f, { 170, 182, 214, 255 });
        DrawCircleV(tip, 4.5f, { 205, 214, 232, 255 });
        DrawCircleV(butt, 3.5f, { 120, 130, 160, 255 });
    }
    if (f.formula && f.formulaT < FORMULA_TIME) {   // the physics behind the attack, rising and fading
        float alpha = fminf(1, fminf(f.formulaT / 0.15f, (FORMULA_TIME - f.formulaT) / 0.4f));
        float w = (float)ui::measure(f.formula, 10);
        float x = clampf(f.formulaAt.x, TILE + w / 2 + 6, ROOM_PX_W - TILE - w / 2 - 6);
        float y = fmaxf(TILE + 4.0f, f.formulaAt.y - f.formulaT * 6);
        DrawRectangleRounded({ x - w / 2 - 5, y - 3, w + 10, 16 }, 0.4f, 4, Fade({ 10, 12, 22, 255 }, 0.6f * alpha));
        ui::textCentered(f.formula, x, y, 10, Fade({ 225, 242, 255, 255 }, alpha));
    }
}
