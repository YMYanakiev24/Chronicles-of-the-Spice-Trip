/*
 * BossFight.cs
 * ----------------------------------------------------------------------------
 * The rules of a dungeon boss fight: how each boss moves and fires, the
 * Plasmodia multiplying, Mordekai's dive, Vlado's tickets and bus No.9, and
 * the Cinder-Maw's embers. Boss bodies are ordinary Enemies (so spells and the
 * blade hit them as usual); this class moves them and spawns their shots.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using UnityEngine;
using static SpiceTrip.Dungeon.Geo;

namespace SpiceTrip.Dungeon
{
    public class Bus
    {
        public float x, y, w, h, warn;
        public bool driving, hit;
    }

    public class BossFight
    {
        // Plasmodium globs
        const float OrbSpeed = 200, OrbR = 6, SpreadAngle = 0.22f;
        const int OrbDamage = 8;
        // The Plasmodia multiply: three copies, and each copy may leave a tiny one
        public const int Copies = 3;
        const float CopyHp = 40, CopyR = 15, CopySpeed = 120, CopyInterval = 1.4f;
        const float TinyChance = 0.4f, TinyHp = 14, TinyR = 9, TinySpeed = 105;
        // Mordekai
        const float FanStep = 0.15f, FeatherSpeed = 250, DiveSpeed = 360, DiveTime = 0.6f;
        const int FeatherDamage = 6, DiveEvery = 3;
        // Vlado
        public const int SpiralCount = 12;
        const int TicketDamage = 6, BusHitDamage = 20;
        const float SpiralTurn = 0.26f, TicketSpeed = 170;
        const float WhistleEvery = 8, BusWarn = 1.0f, BusSpeed = 640, BusW = 140, BusH = 52;
        // The Cinder-Maw
        public const int RingCount = 10;
        const int EmberDamage = 8;
        const float RingEvery = 3.2f, RingSpeed = 150, FireballSpeed = 220;

        public DungeonBoss Def { get; private set; }
        public int Index { get; private set; }
        public float Strength { get; private set; } = 1;
        public float Time { get; private set; }
        public Bus Bus { get; private set; }
        public bool Active => Def != null;
        public event Action<string> OnSfx;

        readonly DRandom _rng;
        float _spiralT, _spiralBase, _whistleT, _emberT, _aimT, _poolMax = 1;

        public BossFight(DRandom rng) { _rng = rng; }

        public void Begin(int index, float strength, Vector2 at, List<Enemy> enemies)
        {
            Index = index;
            Def = Defs.Bosses[index];
            Strength = strength;
            Time = 0;
            Bus = null;
            _spiralT = 1.6f; _spiralBase = 0; _whistleT = 4; _emberT = 2; _aimT = 1;
            var boss = Body(0, at);
            _poolMax = boss.maxHp + (Def.kind == BossKind.Split ? Copies * CopyHp * strength : 0);
            enemies.Add(boss);
        }

        public void End() { Def = null; Bus = null; }

        static Vector2 InsideRoom(Vector2 p, float r) =>
            new Vector2(Mathf.Clamp(p.x, Tile + r, RoomW - Tile - r), Mathf.Clamp(p.y, Tile + r, RoomH - Tile - r));

        Enemy Body(int form, Vector2 at)
        {
            var e = new Enemy
            {
                name = Def.name, weakness = Def.weakness, color = Def.color, boss = true, bossForm = form,
                kind = Def.kind == BossKind.Inferno ? EnemyKind.Vampire : EnemyKind.Skeleton,
                wobble = _rng.F(0, 6.28f), animT = _rng.F(0, 1),
            };
            e.pos = e.roamTo = InsideRoom(at, Def.radius);
            if (form == 0)
            {
                e.hp = e.maxHp = Def.hp * Strength; e.r = Def.radius; e.speed = Def.speed;
                e.dmg = (int)(Def.contact * Strength); e.xp = 150;
                e.spread = Def.spread; e.fireInterval = Def.fireInterval;
                e.shootCd = 1.6f;                // a moment to take in the arena
            }
            else if (form == 1)
            {
                e.hp = e.maxHp = CopyHp * Strength; e.r = CopyR; e.speed = CopySpeed;
                e.dmg = (int)(8 * Strength); e.xp = 14;
                e.spread = Def.spread >= 5 ? 3 : 1; e.fireInterval = CopyInterval;
                e.shootCd = _rng.F(0.6f, 1.2f); e.scale = CopyR / Def.radius;
            }
            else
            {
                e.hp = e.maxHp = TinyHp * Strength; e.r = TinyR; e.speed = TinySpeed;
                e.dmg = (int)(6 * Strength); e.xp = 6;
                e.chaser = true; e.scale = TinyR / Def.radius;
            }
            return e;
        }

        void Fire(Enemy from, float angle, float speed, float r, int damage, ShotKind kind, List<Shot> shots)
        {
            var dir = new Vector2(Mathf.Cos(angle), Mathf.Sin(angle));
            shots.Add(new Shot
            {
                pos = from.pos + dir * from.r * 0.6f, vel = dir * speed, r = r, life = 4, kind = kind,
                damage = Mathf.RoundToInt(damage * Strength), color = Def.color, glow = Color.Lerp(Def.color, Color.white, 0.5f),
            });
        }

        void Roam(Enemy e, float dt, Room room)
        {
            e.moveCd -= dt;
            if (e.moveCd <= 0)
            {
                e.moveCd = 1.4f + _rng.F(0, 1.2f);
                e.roamTo = new Vector2(_rng.F(Tile * 3f, RoomW - Tile * 3f), _rng.F(Tile * 2.5f, RoomH - Tile * 2.5f));
            }
            var to = e.roamTo - e.pos;
            float d = to.magnitude;
            if (d > 2) e.pos = RoomPhysics.Move(e.pos, to * (Mathf.Min(e.speed * dt, d) / d), e.r, room, false);
            e.moving = d > 2;
        }

        /// <summary>Moves and fires every boss body. `slow` scales the bosses' time (Atanas' aura).</summary>
        public void Update(float dt, Vector2 player, List<Enemy> enemies, List<Shot> shots, Room room, float slow)
        {
            if (Def == null) return;
            Time += dt;
            float bdt = dt * slow;
            Enemy primary = null;

            foreach (var e in enemies)
            {
                if (!e.alive || e.bossForm < 0) continue;
                if (e.bossForm == 0) primary = e;
                var to = player - e.pos;
                float d = Mathf.Max(0.01f, to.magnitude);
                if (Mathf.Abs(to.x) > 2) e.flip = to.x < 0;

                // --- movement ---
                if (e.diveT > 0)
                {
                    e.diveT -= bdt;
                    e.pos = RoomPhysics.Move(e.pos, e.diveDir * DiveSpeed * bdt, e.r, room, false);
                    e.moving = true;
                }
                else if (e.chaser || Def.kind == BossKind.Inferno)
                {
                    float spd = e.speed * (Def.kind == BossKind.Inferno && e.hp < e.maxHp * 0.4f ? 1.5f : 1f);
                    e.pos = RoomPhysics.Move(e.pos, to * (spd * bdt / d), e.r, room, false);
                    e.moving = true;
                }
                else Roam(e, bdt, room);

                // --- aimed volleys (Plasmodia and Mordekai) ---
                if (e.chaser || Def.kind == BossKind.Conductor || Def.kind == BossKind.Inferno) continue;
                e.shootCd -= bdt;
                if (e.shootCd > 0) continue;
                e.shootCd = e.fireInterval;
                float baseA = Mathf.Atan2(to.y, to.x);
                if (Def.kind == BossKind.Feathers)
                {
                    for (int i = 0; i < e.spread; i++)
                        Fire(e, baseA + (i - (e.spread - 1) / 2f) * FanStep, FeatherSpeed, 4, FeatherDamage, ShotKind.Feather, shots);
                    OnSfx?.Invoke("nature");
                    if (++e.volleys % DiveEvery == 0) { e.diveDir = to / d; e.diveT = DiveTime; }
                }
                else
                {
                    int half = e.spread / 2;
                    for (int s = -half; s <= half; s++)
                        Fire(e, baseA + s * SpreadAngle, OrbSpeed, OrbR * (e.bossForm == 0 ? 1 : 0.8f), OrbDamage, ShotKind.Orb, shots);
                    OnSfx?.Invoke("lightning");
                }
            }

            // --- Vlado: ticket spirals and the whistle ---
            if (Def.kind == BossKind.Conductor && primary != null)
            {
                _spiralT -= bdt;
                if (_spiralT <= 0)
                {
                    _spiralT = Def.fireInterval;
                    for (int i = 0; i < SpiralCount; i++)
                        Fire(primary, _spiralBase + i * Mathf.PI * 2 / SpiralCount, TicketSpeed, 5, TicketDamage, ShotKind.Ticket, shots);
                    _spiralBase += SpiralTurn;
                    OnSfx?.Invoke("ui");
                }
                _whistleT -= bdt;
                if (_whistleT <= 0 && Bus == null)
                {
                    _whistleT = WhistleEvery;
                    Bus = new Bus
                    {
                        w = BusW, h = BusH, warn = BusWarn, x = -BusW / 2,
                        y = Mathf.Clamp(player.y, Tile + BusH / 2, RoomH - Tile - BusH / 2),
                    };
                    OnSfx?.Invoke("whistle");
                }
            }

            // --- the Cinder-Maw: ember rings, and fireballs once wounded ---
            if (Def.kind == BossKind.Inferno && primary != null)
            {
                _emberT -= bdt;
                if (_emberT <= 0)
                {
                    _emberT = RingEvery;
                    float off = _rng.F(0, Mathf.PI);
                    for (int i = 0; i < RingCount; i++)
                        Fire(primary, off + i * Mathf.PI * 2 / RingCount, RingSpeed, 6, EmberDamage, ShotKind.Ember, shots);
                    OnSfx?.Invoke("fire");
                }
                if (primary.hp < primary.maxHp * 0.5f)
                {
                    _aimT -= bdt;
                    if (_aimT <= 0)
                    {
                        _aimT = Def.fireInterval;
                        var to = player - primary.pos;
                        float baseA = Mathf.Atan2(to.y, to.x);
                        for (int s = -1; s <= 1; s++)
                            Fire(primary, baseA + s * SpreadAngle, FireballSpeed, 7, EmberDamage, ShotKind.Ember, shots);
                    }
                }
            }

            // --- the bus crosses the room once its warning runs out ---
            if (Bus != null)
            {
                if (!Bus.driving)
                {
                    Bus.warn -= bdt;
                    if (Bus.warn <= 0) { Bus.driving = true; OnSfx?.Invoke("bus"); }
                }
                else
                {
                    Bus.x += BusSpeed * bdt;
                    if (Bus.x - Bus.w / 2 > RoomW) Bus = null;
                }
            }
        }

        /// <summary>A boss body died. The Plasmodia multiply: new bodies go into `spawned`.</summary>
        public void OnDeath(Enemy dead, List<Enemy> spawned)
        {
            if (Def == null || Def.kind != BossKind.Split || dead.bossForm < 0) return;
            if (dead.bossForm == 0)
            {
                for (int i = 0; i < Copies; i++)
                {
                    float a = i * Mathf.PI * 2 / Copies - Mathf.PI / 2;
                    spawned.Add(Body(1, dead.pos + new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * 26));
                }
                OnSfx?.Invoke("nature");
            }
            else if (dead.bossForm == 1 && _rng.Chance(TinyChance)) spawned.Add(Body(2, dead.pos));
        }

        public bool BusHits(Vector2 p, float r)
        {
            if (Bus == null || !Bus.driving || Bus.hit) return false;
            float cx = Mathf.Clamp(p.x, Bus.x - Bus.w / 2, Bus.x + Bus.w / 2);
            float cy = Mathf.Clamp(p.y, Bus.y - Bus.h / 2, Bus.y + Bus.h / 2);
            return Vector2.Distance(p, new Vector2(cx, cy)) < r;
        }

        public int BusDamage => Mathf.RoundToInt(BusHitDamage * Strength);

        /// <summary>0..1 across every body, counting copies that are still to come.</summary>
        public float Health(List<Enemy> enemies)
        {
            if (Def == null) return 0;
            float hp = 0;
            bool firstForm = false;
            foreach (var e in enemies)
            {
                if (!e.alive || e.bossForm < 0) continue;
                hp += Mathf.Max(0, e.hp);
                if (e.bossForm == 0) firstForm = true;
            }
            if (firstForm && Def.kind == BossKind.Split) hp += Copies * CopyHp * Strength;
            return Mathf.Clamp01(hp / _poolMax);
        }
    }
}
