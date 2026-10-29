// ============================================================================
//  Bosses.cpp — boss table, boss AI and boss drawing.
//  The patterns follow the original Path boss fights, scaled to a room.
// ============================================================================
#include "Bosses.h"
#include "Champions.h"
#include "Render.h"
#include "Sprites.h"
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

    float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Keep a body's centre inside the room's walls.
    Vector2 insideRoom(Vector2 p, float r) {
        return { clampf(p.x, TILE + r, ROOM_PX_W - TILE - r), clampf(p.y, TILE + r, ROOM_PX_H - TILE - r) };
    }
}

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
    def = &d; strength = s; time = 0; bus = Bus{};
    spiralT = 1.6f; spiralBase = 0; whistleT = 4.0f; emberT = 2.0f; aimT = 1.0f;
    Enemy boss = body(0, at);
    poolMax = boss.maxHp + (d.kind == BossKind::Split ? COPIES * COPY_HP * s : 0);
    enemies.push_back(boss);
}

void BossFight::shoot(const Enemy& from, float angle, float speed, float r, int damage, ShotKind kind,
    std::vector<Projectile>& shots) const {
    Projectile p;
    p.pos = Vector2Add(from.pos, Vector2Scale(fromAngle(angle), from.r * 0.6f));
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
            continue;
        }

        // --- movement ---
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

    updateBus(bdt, player);
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
        token[i] = arena[i] = Texture2D{};
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

void drawBossBody(const Enemy& e, int index, const BossArt& art) {
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
