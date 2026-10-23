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

        public class Shot
        {
            public float x, y, vx, vy, r, life;
        }

        public struct Inputs
        {
            public Vector2 move;     // -1..1 per axis, y down (+1 = S/down)
            public Vector2 aim;      // logical arena coords
            public bool shoot;
        }

        public readonly BattleTuning T;
        public readonly BossDef Def;
        public readonly Fighter Player = new Fighter();
        public readonly Fighter Boss = new Fighter();
        public readonly List<Shot> PlayerShots = new List<Shot>();
        public readonly List<Shot> BossShots = new List<Shot>();
        public bool Active { get; private set; }
        public bool? Won { get; private set; }
        public float Time { get; private set; }
        public int ShotsFired { get; private set; }

        public event Action<string> OnSfx;
        public event Action<bool> OnEnd;

        float _playerCd, _bossMoveCd, _bossShootCd, _tx, _ty;
        readonly Func<float> _rand;

        public BattleSim(BattleTuning tuning, BossDef def, Func<float> rand = null)
        {
            T = tuning;
            Def = def;
            var sys = new System.Random();
            _rand = rand ?? (() => (float)sys.NextDouble());
            Reset();
        }

        public void Reset()
        {
            Player.x = T.arenaWidth / 2; Player.y = T.arenaHeight - T.playerStartYFromBottom;
            Player.r = T.playerRadius; Player.hp = Player.maxHp = T.playerHp; Player.hurt = 0;
            Boss.x = T.arenaWidth / 2; Boss.y = T.bossStartY;
            Boss.r = T.bossRadius; Boss.hp = Boss.maxHp = Def.hp; Boss.hurt = 0;
            _playerCd = 0;
            _bossShootCd = T.bossFirstShot;
            _bossMoveCd = 0;
            _tx = T.arenaWidth / 2; _ty = T.bossStartY;
            PlayerShots.Clear();
            BossShots.Clear();
            Time = 0;
            Won = null;
            Active = true;
        }

        /// <summary>Advances the fight; dt is clamped to maxDt like the JS loop.</summary>
        public void Step(float dt, Inputs inp)
        {
            dt = Mathf.Min(T.maxDt, dt);
            Time += dt;
            if (!Active) return;
            var p = Player;
            var b = Boss;
            if (p.hurt > 0) p.hurt -= dt;
            if (b.hurt > 0) b.hurt -= dt;

            // --- Player movement ---
            float mx = Mathf.Clamp(inp.move.x, -1, 1), my = Mathf.Clamp(inp.move.y, -1, 1);
            float ml = Mathf.Sqrt(mx * mx + my * my);
            if (ml == 0) ml = 1;
            float moving = mx != 0 || my != 0 ? 1 : 0;
            p.x = Mathf.Clamp(p.x + (mx / ml) * T.playerSpeed * dt * moving, T.playerEdge, T.arenaWidth - T.playerEdge);
            p.y = Mathf.Clamp(p.y + (my / ml) * T.playerSpeed * dt * moving, T.playerEdge, T.arenaHeight - T.playerEdge);

            // --- Player shooting (aim at mouse) ---
            _playerCd -= dt;
            if (inp.shoot && _playerCd <= 0)
            {
                _playerCd = T.playerFireCooldown;
                float a = Mathf.Atan2(inp.aim.y - p.y, inp.aim.x - p.x);
                PlayerShots.Add(new Shot
                {
                    x = p.x, y = p.y,
                    vx = Mathf.Cos(a) * T.playerShotSpeed, vy = Mathf.Sin(a) * T.playerShotSpeed,
                    r = T.playerShotRadius, life = T.playerShotLife,
                });
                ShotsFired++;
                OnSfx?.Invoke("fire");
            }

            // --- Boss AI: roam toward waypoints, fire spreads ---
            _bossMoveCd -= dt;
            if (_bossMoveCd <= 0)
            {
                _bossMoveCd = T.bossMoveMin + _rand() * T.bossMoveJitter;
                _tx = T.bossWaypointMarginX + _rand() * (T.arenaWidth - 2 * T.bossWaypointMarginX);
                _ty = T.bossWaypointMinY + _rand() * (T.arenaHeight * T.bossWaypointRangeY);
            }
            float bdx = _tx - b.x, bdy = _ty - b.y, bd = Mathf.Sqrt(bdx * bdx + bdy * bdy);
            if (bd == 0) bd = 1;
            b.x = Mathf.Clamp(b.x + (bdx / bd) * T.bossSpeed * dt, T.bossClampEdge, T.arenaWidth - T.bossClampEdge);
            b.y = Mathf.Clamp(b.y + (bdy / bd) * T.bossSpeed * dt, T.bossClampEdge, T.arenaHeight * T.bossClampMaxY);

            _bossShootCd -= dt;
            if (_bossShootCd <= 0)
            {
                _bossShootCd = T.bossFireInterval;
                float baseA = Mathf.Atan2(p.y - b.y, p.x - b.x);
                int half = T.bossSpreadCount / 2;
                for (int s = -half; s <= half; s++)
                {
                    float a = baseA + s * T.bossSpreadAngle;
                    BossShots.Add(new Shot
                    {
                        x = b.x, y = b.y + T.bossShotYOffset,
                        vx = Mathf.Cos(a) * T.bossShotSpeed, vy = Mathf.Sin(a) * T.bossShotSpeed,
                        r = T.bossShotRadius, life = T.bossShotLife,
                    });
                }
                OnSfx?.Invoke("lightning");
            }

            // --- Projectiles ---
            for (int i = PlayerShots.Count - 1; i >= 0; i--)
            {
                var s = PlayerShots[i];
                s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
                if (Dist(s.x, s.y, b.x, b.y) < s.r + b.r)
                {
                    b.hp -= T.playerShotDamage; b.hurt = T.bossHurtFlash; OnSfx?.Invoke("hit");
                    PlayerShots.RemoveAt(i);
                    if (b.hp <= 0) { End(true); return; }
                    continue;
                }
                if (OutOrExpired(s)) PlayerShots.RemoveAt(i);
            }
            for (int i = BossShots.Count - 1; i >= 0; i--)
            {
                var s = BossShots[i];
                s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
                if (p.hurt <= 0 && Dist(s.x, s.y, p.x, p.y) < s.r + p.r)
                {
                    p.hp -= T.bossShotDamage; p.hurt = T.playerHitInvuln; OnSfx?.Invoke("hurt");
                    BossShots.RemoveAt(i);
                    if (p.hp <= 0) { End(false); return; }
                    continue;
                }
                if (OutOrExpired(s)) BossShots.RemoveAt(i);
            }

            // Touch damage from the boss body.
            if (p.hurt <= 0 && Dist(p.x, p.y, b.x, b.y) < p.r + b.r)
            {
                p.hp -= T.contactDamage; p.hurt = T.contactInvuln; OnSfx?.Invoke("hurt");
                if (p.hp <= 0) End(false);
            }
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
