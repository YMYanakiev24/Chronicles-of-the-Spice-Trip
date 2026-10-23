/*
 * BattleSim.cs
 * ----------------------------------------------------------------------------
 * The rules of the boss fight from the web game's BossBattle.js, with no Unity
 * rendering in it so it can be unit-tested. Coordinates are the original
 * logical canvas pixels (800×450, y pointing down); BossBattleView converts
 * them to world units for drawing.
 *
 * The player moves with WASD/arrows and shoots toward the mouse; the boss
 * roams between random waypoints and fires a 3-shot spread at the player.
 *
 * Phase 3 adds the approved abilities on top, all driven by abilities.json:
 *   boss kits      split (the Plasmodia multiply ×3 on death), feathers
 *                  (Mordekai's fan + dive), conductor (Vlado's ticket spirals,
 *                  whistle and bus №9)
 *   champion kits  a right-click/Q special, passives and meters per champion
 * With no kit (Spellwoven, or a boss without one) the fight is exactly the
 * original.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Battle
{
    public class BattleSim
    {
        public class Fighter
        {
            public float x, y, r, hp, maxHp, hurt;
        }

        /// <summary>A boss body. The Plasmodia split into several; everyone else has one.</summary>
        public class Unit : Fighter
        {
            public int form;                    // 0 original, 1 copy, 2 tiny copy
            public float speed, moveCd, shootCd, tx, ty;
            public int spread;
            public float fireInterval;
            public bool shooter = true, chaser;
            public float diveT, diveDx, diveDy;
            public int volleys;
            public float scale = 1;             // drawing size relative to the original
        }

        public enum ShotKind { Orb, Feather, Ticket, Seed, Coin, Koi, Sparkle }

        public class Shot
        {
            public float x, y, vx, vy, r, life;
            public float damage;                // 0 → the original damage for that side
            public ShotKind kind;
            public float homingTurn;            // rad/s toward the nearest boss body
            public float spin;
        }

        public class Puddle { public float x, y, r, life; }
        public class Coin { public float x, y, life; }

        public class Bus
        {
            public float y, x, w, h, warn;
            public bool driving, hit;
        }

        public struct Inputs
        {
            public Vector2 move;     // -1..1 per axis, y down (+1 = S/down)
            public Vector2 aim;      // logical arena coords
            public bool shoot;
            public bool special;     // right-click / Q pressed this frame
        }

        public readonly BattleTuning T;
        public readonly BossDef Def;
        public readonly BossKitDef BossKit;
        public readonly ChampionKitDef Kit;
        public readonly Fighter Player = new Fighter();
        public readonly List<Unit> Units = new List<Unit>();
        public readonly List<Shot> PlayerShots = new List<Shot>();
        public readonly List<Shot> BossShots = new List<Shot>();
        public readonly List<Puddle> Puddles = new List<Puddle>();
        public readonly List<Coin> Coins = new List<Coin>();
        public Bus ActiveBus { get; private set; }
        public bool Active { get; private set; }
        public bool? Won { get; private set; }
        public float Time { get; private set; }
        public int ShotsFired { get; private set; }

        /// <summary>The original single boss (the first body, or the last one standing).</summary>
        public Unit Boss => _primary;

        // champion state
        public float SpecialCooldown { get; private set; }
        public float AuraT, ShieldT, DashT, RollT, JumpT, WobbleT, TeleportT;
        public float CoinPower, Drunk, Nicotine, Dust;
        public float PlayerSpeed { get; private set; }
        public Vector2 FacingAim { get; private set; }

        public event Action<string> OnSfx;
        public event Action<bool> OnEnd;

        float _playerCd, _honeyT, _spiralT, _spiralBase, _whistleT;
        Vector2 _moveDir = new Vector2(0, -1);
        readonly HashSet<Unit> _rollHits = new HashSet<Unit>();
        Unit _primary;
        readonly Func<float> _rand;

        public BattleSim(BattleTuning tuning, BossDef def, Func<float> rand = null, ChampionKitDef champion = null, BossKitDef bossKit = null)
        {
            T = tuning;
            Def = def;
            Kit = champion;
            BossKit = bossKit;
            var sys = new System.Random();
            _rand = rand ?? (() => (float)sys.NextDouble());
            Reset();
        }

        string Special => Kit?.special ?? "none";
        float K(string key, float fallback = 0) => Kit != null ? Kit.P(key, fallback) : fallback;
        float B(string key, float fallback = 0) => BossKit != null ? BossKit.P(key, fallback) : fallback;
        string BossKind => BossKit?.kit ?? "none";

        public void Reset()
        {
            Player.x = T.arenaWidth / 2; Player.y = T.arenaHeight - T.playerStartYFromBottom;
            Player.r = T.playerRadius;
            Player.hp = Player.maxHp = T.playerHp + K("hpBonus");
            Player.hurt = 0;
            PlayerSpeed = K("speed", T.playerSpeed);

            Units.Clear();
            var boss = new Unit
            {
                x = T.arenaWidth / 2, y = T.bossStartY, r = T.bossRadius, hp = Def.hp, maxHp = Def.hp,
                speed = T.bossSpeed, shootCd = T.bossFirstShot, moveCd = 0, tx = T.arenaWidth / 2, ty = T.bossStartY,
                spread = BossKind == "split" ? (int)B("form1Spread", T.bossSpreadCount) : T.bossSpreadCount,
                fireInterval = BossKind == "feathers" ? B("fanInterval", T.bossFireInterval) : T.bossFireInterval,
            };
            Units.Add(boss);
            _primary = boss;
            _playerCd = 0;
            PlayerShots.Clear();
            BossShots.Clear();
            Puddles.Clear();
            Coins.Clear();
            ActiveBus = null;
            Time = 0;
            Won = null;
            Active = true;

            SpecialCooldown = 0;
            AuraT = ShieldT = DashT = RollT = JumpT = WobbleT = TeleportT = 0;
            CoinPower = 0; Drunk = 0;
            Nicotine = K("nicotineStart");
            Dust = K("dustMax");
            _honeyT = 0;
            _spiralT = B("spiralInterval", 2);
            _spiralBase = 0;
            _whistleT = B("whistleEvery", 8);
        }

        /// <summary>Invulnerable from a recent hit, or while rolling, jumping, dashing or blinking.</summary>
        public bool PlayerInvulnerable => Player.hurt > 0 || RollT > 0 || JumpT > 0 || DashT > 0 || TeleportT > 0;

        /// <summary>Combined health of every boss body still standing (one bar covers them all).</summary>
        public float BossHpRatio
        {
            get
            {
                float hp = 0, max = 0;
                foreach (var u in Units) { hp += Mathf.Max(0, u.hp); max += u.maxHp; }
                return max > 0 ? hp / max : 0;
            }
        }

        /// <summary>Advances the fight; dt is clamped to maxDt like the JS loop.</summary>
        public void Step(float dt, Inputs inp)
        {
            dt = Mathf.Min(T.maxDt, dt);
            Time += dt;
            if (!Active) return;
            var p = Player;
            if (p.hurt > 0) p.hurt -= dt;
            foreach (var u in Units) if (u.hurt > 0) u.hurt -= dt;
            TickChampionTimers(dt);

            // --- Player movement ---
            float mx = Mathf.Clamp(inp.move.x, -1, 1), my = Mathf.Clamp(inp.move.y, -1, 1);
            float ml = Mathf.Sqrt(mx * mx + my * my);
            bool moving = mx != 0 || my != 0;
            if (ml == 0) ml = 1;
            if (moving) _moveDir = new Vector2(mx / ml, my / ml);
            FacingAim = (inp.aim - new Vector2(p.x, p.y)).normalized;
            if (DashT > 0 || RollT > 0)
            {
                float mul = DashT > 0 ? K("speedMul", 3) : K("speedMul", 1.8f);
                p.x = Mathf.Clamp(p.x + _moveDir.x * PlayerSpeed * mul * dt, T.playerEdge, T.arenaWidth - T.playerEdge);
                p.y = Mathf.Clamp(p.y + _moveDir.y * PlayerSpeed * mul * dt, T.playerEdge, T.arenaHeight - T.playerEdge);
            }
            else
            {
                float moveF = moving ? 1 : 0;
                p.x = Mathf.Clamp(p.x + (mx / ml) * PlayerSpeed * dt * moveF, T.playerEdge, T.arenaWidth - T.playerEdge);
                p.y = Mathf.Clamp(p.y + (my / ml) * PlayerSpeed * dt * moveF, T.playerEdge, T.arenaHeight - T.playerEdge);
            }

            ChampionPassives(dt, moving);
            if (inp.special) TrySpecial(inp);

            // --- Player shooting (aim at mouse) ---
            _playerCd -= dt;
            if (inp.shoot && _playerCd <= 0)
            {
                float cd = T.playerFireCooldown;
                if (Nicotine > 0 && K("fireRateBonus") > 0) cd /= 1 + K("fireRateBonus");
                _playerCd = cd;
                float a = Mathf.Atan2(inp.aim.y - p.y, inp.aim.x - p.x);
                if (WobbleT > 0) a += (_rand() - 0.5f) * 2 * K("wobble");
                FirePlayerPattern(a);
                ShotsFired++;
                OnSfx?.Invoke("fire");
            }

            // --- Boss AI: roam toward waypoints, fire ---
            for (int i = 0; i < Units.Count; i++) UpdateUnit(Units[i], dt);
            if (BossKind == "conductor") UpdateConductor(dt);

            // --- Projectiles ---
            for (int i = PlayerShots.Count - 1; i >= 0; i--)
            {
                var s = PlayerShots[i];
                if (s.homingTurn > 0) Steer(s, dt);
                s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
                Unit hit = null;
                foreach (var u in Units)
                    if (Dist(s.x, s.y, u.x, u.y) < s.r + u.r) { hit = u; break; }
                if (hit != null)
                {
                    hit.hp -= s.damage > 0 ? s.damage : T.playerShotDamage;
                    hit.hurt = T.bossHurtFlash;
                    OnSfx?.Invoke("hit");
                    OnPlayerHit(hit, s);
                    PlayerShots.RemoveAt(i);
                    if (ResolveDeaths()) return;
                    continue;
                }
                if (OutOrExpired(s)) PlayerShots.RemoveAt(i);
            }
            float shotSpeed = AuraT > 0 ? K("shotSlow", 1) : 1;
            for (int i = BossShots.Count - 1; i >= 0; i--)
            {
                var s = BossShots[i];
                s.x += s.vx * dt * shotSpeed; s.y += s.vy * dt * shotSpeed; s.life -= dt;
                if (ShieldT > 0 && Blocks(s)) { BossShots.RemoveAt(i); OnSfx?.Invoke("shield"); continue; }
                if (!PlayerInvulnerable && Dist(s.x, s.y, p.x, p.y) < s.r + p.r)
                {
                    p.hp -= s.damage > 0 ? s.damage : T.bossShotDamage;
                    p.hurt = T.playerHitInvuln;
                    OnSfx?.Invoke("hurt");
                    BossShots.RemoveAt(i);
                    if (p.hp <= 0) { End(false); return; }
                    continue;
                }
                if (OutOrExpired(s)) BossShots.RemoveAt(i);
            }

            // Bus №9
            if (ActiveBus != null && ActiveBus.driving && !ActiveBus.hit && !PlayerInvulnerable && BusHits(ActiveBus))
            {
                ActiveBus.hit = true;
                p.hp -= B("busDamage", 20);
                p.hurt = T.contactInvuln;
                OnSfx?.Invoke("hurt");
                if (p.hp <= 0) { End(false); return; }
            }

            // Coins (Gakev) heal when touched.
            for (int i = Coins.Count - 1; i >= 0; i--)
            {
                var c = Coins[i];
                c.life -= dt;
                if (Dist(c.x, c.y, p.x, p.y) < p.r + 6)
                {
                    p.hp = Mathf.Min(p.maxHp, p.hp + K("coinHeal", 5));
                    OnSfx?.Invoke("pickup");
                    Coins.RemoveAt(i);
                }
                else if (c.life <= 0) Coins.RemoveAt(i);
            }

            // Touch damage from the boss bodies (a rolling melon hits back instead).
            foreach (var u in Units)
            {
                if (Dist(p.x, p.y, u.x, u.y) >= p.r + u.r) continue;
                if (RollT > 0)
                {
                    if (_rollHits.Add(u)) { u.hp -= K("rollDamage", 20); u.hurt = T.bossHurtFlash; OnSfx?.Invoke("hit"); }
                    continue;
                }
                if (PlayerInvulnerable) continue;
                p.hp -= T.contactDamage; p.hurt = T.contactInvuln; OnSfx?.Invoke("hurt");
                if (p.hp <= 0) { End(false); return; }
            }
            ResolveDeaths();
        }

        // =====================================================================
        // Boss bodies
        // =====================================================================
        void UpdateUnit(Unit b, float dt)
        {
            var p = Player;
            float slow = InHoney(b) ? K("honeySlow", 1) : 1;

            if (b.diveT > 0)
            {
                // Mordekai's dive: a fast straight dash at where the player was.
                b.diveT -= dt;
                float ds = B("diveSpeed", 420) * slow;
                b.x = Mathf.Clamp(b.x + b.diveDx * ds * dt, T.bossClampEdge, T.arenaWidth - T.bossClampEdge);
                b.y = Mathf.Clamp(b.y + b.diveDy * ds * dt, T.bossClampEdge, T.arenaHeight - T.bossClampEdge);
            }
            else if (b.chaser)
            {
                // Tiny copies just chase.
                float cdx = p.x - b.x, cdy = p.y - b.y, cd = Mathf.Sqrt(cdx * cdx + cdy * cdy);
                if (cd > 0.01f) { b.x += cdx / cd * b.speed * slow * dt; b.y += cdy / cd * b.speed * slow * dt; }
            }
            else
            {
                b.moveCd -= dt;
                if (b.moveCd <= 0)
                {
                    b.moveCd = T.bossMoveMin + _rand() * T.bossMoveJitter;
                    b.tx = T.bossWaypointMarginX + _rand() * (T.arenaWidth - 2 * T.bossWaypointMarginX);
                    b.ty = T.bossWaypointMinY + _rand() * (T.arenaHeight * T.bossWaypointRangeY);
                }
                float bdx = b.tx - b.x, bdy = b.ty - b.y, bd = Mathf.Sqrt(bdx * bdx + bdy * bdy);
                if (bd == 0) bd = 1;
                b.x = Mathf.Clamp(b.x + (bdx / bd) * b.speed * slow * dt, T.bossClampEdge, T.arenaWidth - T.bossClampEdge);
                b.y = Mathf.Clamp(b.y + (bdy / bd) * b.speed * slow * dt, T.bossClampEdge, T.arenaHeight * T.bossClampMaxY);
            }

            if (!b.shooter || BossKind == "conductor") return;
            b.shootCd -= dt;
            if (b.shootCd > 0) return;
            b.shootCd = b.fireInterval;
            float baseA = Mathf.Atan2(p.y - b.y, p.x - b.x);
            if (BossKind == "feathers")
            {
                int n = (int)B("fanCount", 7);
                float step = B("fanStep", 0.15f);
                for (int i = 0; i < n; i++)
                {
                    float a = baseA + (i - (n - 1) / 2f) * step;
                    BossShots.Add(MakeBossShot(b, a, B("featherSpeed", 300), 4, B("featherDamage", 5), ShotKind.Feather));
                }
                OnSfx?.Invoke("nature");
                b.volleys++;
                int every = (int)B("diveEvery", 3);
                if (every > 0 && b.volleys % every == 0)
                {
                    float dx = p.x - b.x, dy = p.y - b.y, d = Mathf.Max(0.01f, Mathf.Sqrt(dx * dx + dy * dy));
                    b.diveDx = dx / d; b.diveDy = dy / d; b.diveT = B("diveTime", 0.6f);
                }
                return;
            }
            int half = b.spread / 2;
            for (int s = -half; s <= half; s++)
            {
                if (b.spread % 2 == 0 && s == 0) continue;
                float a = baseA + s * T.bossSpreadAngle;
                BossShots.Add(MakeBossShot(b, a, T.bossShotSpeed, T.bossShotRadius, 0, ShotKind.Orb));
            }
            OnSfx?.Invoke("lightning");
        }

        Shot MakeBossShot(Unit b, float a, float speed, float r, float damage, ShotKind kind) => new Shot
        {
            x = b.x, y = b.y + T.bossShotYOffset * b.scale,
            vx = Mathf.Cos(a) * speed, vy = Mathf.Sin(a) * speed,
            r = r, life = T.bossShotLife, damage = damage, kind = kind,
        };

        // Vlado: ticket spirals, and every few seconds a whistle and bus №9 down the player's lane.
        void UpdateConductor(float dt)
        {
            var b = _primary;
            if (b == null || !Units.Contains(b)) return;
            _spiralT -= dt;
            if (_spiralT <= 0)
            {
                _spiralT = B("spiralInterval", 2);
                int n = (int)B("spiralCount", 12);
                for (int i = 0; i < n; i++)
                {
                    float a = _spiralBase + i * Mathf.PI * 2 / n;
                    var s = MakeBossShot(b, a, B("ticketSpeed", 200), 5, B("ticketDamage", 6), ShotKind.Ticket);
                    s.spin = 6;
                    BossShots.Add(s);
                }
                _spiralBase += B("spiralTurn", 0.26f);
                OnSfx?.Invoke("ui");
            }
            _whistleT -= dt;
            if (_whistleT <= 0 && ActiveBus == null)
            {
                _whistleT = B("whistleEvery", 8);
                float h = B("busHeight", 60);
                ActiveBus = new Bus
                {
                    y = Mathf.Clamp(Player.y, h / 2, T.arenaHeight - h / 2),
                    w = B("busWidth", 160), h = h, warn = B("busWarn", 1), x = -B("busWidth", 160),
                };
                OnSfx?.Invoke("whistle");
            }
            if (ActiveBus != null)
            {
                var bus = ActiveBus;
                if (!bus.driving) { bus.warn -= dt; if (bus.warn <= 0) bus.driving = true; }
                else
                {
                    bus.x += B("busSpeed", 700) * dt;
                    if (bus.x > T.arenaWidth + bus.w) ActiveBus = null;
                }
            }
        }

        bool BusHits(Bus bus)
        {
            var p = Player;
            float cx = Mathf.Clamp(p.x, bus.x - bus.w / 2, bus.x + bus.w / 2);
            float cy = Mathf.Clamp(p.y, bus.y - bus.h / 2, bus.y + bus.h / 2);
            return Dist(p.x, p.y, cx, cy) < p.r;
        }

        /// <summary>Removes dead bodies, splits Plasmodia, and ends the fight when none are left.</summary>
        bool ResolveDeaths()
        {
            for (int i = Units.Count - 1; i >= 0; i--)
            {
                var u = Units[i];
                if (u.hp > 0) continue;
                Units.RemoveAt(i);
                if (BossKind == "split") Split(u);
            }
            if (Units.Count == 0) { End(true); return true; }
            if (!Units.Contains(_primary)) _primary = Units[0];
            return false;
        }

        void Split(Unit dead)
        {
            if (dead.form == 0)
            {
                int n = (int)B("copies", 3);
                for (int i = 0; i < n; i++)
                {
                    float a = i * Mathf.PI * 2 / n - Mathf.PI / 2;
                    Units.Add(new Unit
                    {
                        form = 1, x = dead.x + Mathf.Cos(a) * 26, y = dead.y + Mathf.Sin(a) * 26,
                        r = B("copyRadius", 16), hp = B("copyHp", 25), maxHp = B("copyHp", 25),
                        speed = B("copySpeed", 140), spread = (int)B("copySpread", 1),
                        fireInterval = B("copyFireInterval", 1.4f), shootCd = 0.6f + i * 0.2f,
                        moveCd = 0, tx = dead.x, ty = dead.y, scale = B("copyRadius", 16) / T.bossRadius,
                    });
                }
                OnSfx?.Invoke("nature");
            }
            else if (dead.form == 1 && _rand() < B("tinyChance", 0.4f))
            {
                Units.Add(new Unit
                {
                    form = 2, x = dead.x, y = dead.y, r = B("tinyRadius", 9), hp = B("tinyHp", 10), maxHp = B("tinyHp", 10),
                    speed = B("tinySpeed", 120), shooter = false, chaser = true, scale = B("tinyRadius", 9) / T.bossRadius,
                });
            }
        }

        // =====================================================================
        // Champions
        // =====================================================================
        void TickChampionTimers(float dt)
        {
            if (SpecialCooldown > 0) SpecialCooldown -= dt;
            if (AuraT > 0) AuraT -= dt;
            if (ShieldT > 0) ShieldT -= dt;
            if (DashT > 0) DashT -= dt;
            if (RollT > 0) RollT -= dt;
            if (JumpT > 0) JumpT -= dt;
            if (WobbleT > 0) WobbleT -= dt;
            if (TeleportT > 0) TeleportT -= dt;
            for (int i = Puddles.Count - 1; i >= 0; i--) { Puddles[i].life -= dt; if (Puddles[i].life <= 0) Puddles.RemoveAt(i); }
        }

        void ChampionPassives(float dt, bool moving)
        {
            var p = Player;
            if (K("stillRegen") > 0 && !moving) p.hp = Mathf.Min(p.maxHp, p.hp + K("stillRegen") * dt);
            if (Nicotine > 0) Nicotine = Mathf.Max(0, Nicotine - K("nicotineDrain") * dt);
            if (Drunk > 0) Drunk = Mathf.Max(0, Drunk - K("drunkDecay") * dt);
            if (K("dustMax") > 0) Dust = Mathf.Min(K("dustMax"), Dust + K("dustRegen") * dt);
            if (K("honeyInterval") > 0 && moving)
            {
                _honeyT -= dt;
                if (_honeyT <= 0)
                {
                    _honeyT = K("honeyInterval");
                    Puddles.Add(new Puddle { x = p.x, y = p.y + 8, r = K("honeyRadius", 18), life = K("honeyLife", 6) });
                }
            }
        }

        bool InHoney(Unit u)
        {
            foreach (var pd in Puddles) if (Dist(u.x, u.y, pd.x, pd.y) < pd.r + u.r * 0.5f) return true;
            return false;
        }

        /// <summary>Uses the champion's special if it's ready. Returns whether it fired.</summary>
        public bool TrySpecial(Inputs inp)
        {
            var p = Player;
            if (Special == "none" || SpecialCooldown > 0) return false;
            switch (Special)
            {
                case "aura":
                    p.hp = Mathf.Min(p.maxHp, p.hp + K("heal", 25));
                    AuraT = K("duration", 3);
                    OnSfx?.Invoke("heal");
                    break;
                case "dash":
                    DashT = K("duration", 0.25f);
                    if (!(inp.move.sqrMagnitude > 0)) _moveDir = FacingAim.sqrMagnitude > 0 ? FacingAim : _moveDir;
                    OnSfx?.Invoke("ui");
                    break;
                case "shield":
                    ShieldT = K("duration", 2);
                    OnSfx?.Invoke("shield");
                    break;
                case "roll":
                    RollT = K("duration", 0.6f);
                    _rollHits.Clear();
                    if (!(inp.move.sqrMagnitude > 0)) _moveDir = FacingAim.sqrMagnitude > 0 ? FacingAim : _moveDir;
                    OnSfx?.Invoke("ui");
                    break;
                case "drink":
                    p.hp = Mathf.Min(p.maxHp, p.hp + K("heal", 20));
                    WobbleT = K("wobbleTime", 4);
                    Drunk = Mathf.Min(100, Drunk + K("drunkPerDrink", 40));
                    OnSfx?.Invoke("heal");
                    break;
                case "jump":
                    JumpT = K("duration", 0.6f);
                    OnSfx?.Invoke("ui");
                    break;
                case "coinring":
                    if (CoinPower < K("powerMax", 100)) return false;
                    CoinPower = 0;
                    int n = (int)K("ringCount", 16);
                    for (int i = 0; i < n; i++)
                    {
                        float a = i * Mathf.PI * 2 / n;
                        PlayerShots.Add(new Shot
                        {
                            x = p.x, y = p.y, vx = Mathf.Cos(a) * K("ringSpeed", 300), vy = Mathf.Sin(a) * K("ringSpeed", 300),
                            r = 5, life = T.playerShotLife * 1.5f, damage = K("ringDamage", 10), kind = ShotKind.Coin,
                        });
                    }
                    OnSfx?.Invoke("levelup");
                    break;
                case "koi":
                    int k = (int)K("koiCount", 3);
                    for (int i = 0; i < k; i++)
                    {
                        float a = Mathf.Atan2(FacingAim.y, FacingAim.x) + (i - (k - 1) / 2f) * 0.5f;
                        PlayerShots.Add(new Shot
                        {
                            x = p.x, y = p.y, vx = Mathf.Cos(a) * K("koiSpeed", 180), vy = Mathf.Sin(a) * K("koiSpeed", 180),
                            r = 6, life = K("koiLife", 4), damage = K("koiDamage", 12), kind = ShotKind.Koi, homingTurn = K("koiTurn", 3),
                        });
                    }
                    OnSfx?.Invoke("nature");
                    break;
                case "teleport":
                    if (Dust < K("dustCost", 10)) return false;
                    Dust -= K("dustCost", 10);
                    p.x = Mathf.Clamp(inp.aim.x, T.playerEdge, T.arenaWidth - T.playerEdge);
                    p.y = Mathf.Clamp(inp.aim.y, T.playerEdge, T.arenaHeight - T.playerEdge);
                    TeleportT = K("invuln", 0.2f);
                    OnSfx?.Invoke("shield");
                    break;
                default:
                    return false;
            }
            SpecialCooldown = Kit.cooldown;
            return true;
        }

        void FirePlayerPattern(float a)
        {
            var p = Player;
            Shot Make(float ang, float ox, float oy, float damage, ShotKind kind, float r) => new Shot
            {
                x = p.x + ox, y = p.y + oy,
                vx = Mathf.Cos(ang) * T.playerShotSpeed, vy = Mathf.Sin(ang) * T.playerShotSpeed,
                r = r, life = T.playerShotLife, damage = damage, kind = kind,
            };
            if (K("seedCount") > 0)
            {
                int n = (int)K("seedCount");
                for (int i = 0; i < n; i++)
                    PlayerShots.Add(Make(a + (i - (n - 1) / 2f) * K("seedStep", 0.18f), 0, 0, K("seedDamage", 4), ShotKind.Seed, 4));
            }
            else if (K("twinOffset") > 0)
            {
                float o = K("twinOffset");
                float px = -Mathf.Sin(a) * o, py = Mathf.Cos(a) * o;
                PlayerShots.Add(Make(a, px, py, K("twinDamage", 5), ShotKind.Coin, T.playerShotRadius));
                PlayerShots.Add(Make(a, -px, -py, K("twinDamage", 5), ShotKind.Coin, T.playerShotRadius));
            }
            else if (K("homingTurn") > 0)
            {
                var s = Make(a, 0, 0, K("shotDamage", 6), ShotKind.Sparkle, T.playerShotRadius);
                s.homingTurn = K("homingTurn");
                PlayerShots.Add(s);
            }
            else
            {
                PlayerShots.Add(Make(a, 0, 0, 0, ShotKind.Orb, T.playerShotRadius));
            }
        }

        void OnPlayerHit(Unit hit, Shot s)
        {
            if (K("powerPerHit") > 0) CoinPower = Mathf.Min(K("powerMax", 100), CoinPower + K("powerPerHit"));
            if (K("coinChance") > 0 && _rand() < K("coinChance"))
                Coins.Add(new Coin { x = s.x, y = Mathf.Max(s.y, hit.y + hit.r), life = K("coinLife", 6) });
        }

        void Steer(Shot s, float dt)
        {
            Unit target = null;
            float best = float.MaxValue;
            foreach (var u in Units) { float d = Dist(s.x, s.y, u.x, u.y); if (d < best) { best = d; target = u; } }
            if (target == null) return;
            float speed = Mathf.Sqrt(s.vx * s.vx + s.vy * s.vy);
            float cur = Mathf.Atan2(s.vy, s.vx), want = Mathf.Atan2(target.y - s.y, target.x - s.x);
            float diff = Mathf.DeltaAngle(cur * Mathf.Rad2Deg, want * Mathf.Rad2Deg) * Mathf.Deg2Rad;
            float turn = Mathf.Clamp(diff, -s.homingTurn * dt, s.homingTurn * dt);
            cur += turn;
            s.vx = Mathf.Cos(cur) * speed; s.vy = Mathf.Sin(cur) * speed;
        }

        // Nikolay's mug-shield stops shots arriving from the front (toward the aim).
        bool Blocks(Shot s)
        {
            var p = Player;
            float d = Dist(s.x, s.y, p.x, p.y);
            if (d > p.r + s.r + 10) return false;
            var to = new Vector2(s.x - p.x, s.y - p.y).normalized;
            return Vector2.Angle(to, FacingAim) * Mathf.Deg2Rad <= K("arc", 2.1f) / 2;
        }

        bool OutOrExpired(Shot s) => s.life <= 0 || s.x < 0 || s.x > T.arenaWidth || s.y < 0 || s.y > T.arenaHeight;

        void End(bool win)
        {
            Active = false;
            Won = win;
            OnSfx?.Invoke(win ? "levelup" : "death");
            OnEnd?.Invoke(win);
        }

        static float Dist(float ax, float ay, float bx, float by)
        {
            float dx = ax - bx, dy = ay - by;
            return Mathf.Sqrt(dx * dx + dy * dy);
        }
    }
}
