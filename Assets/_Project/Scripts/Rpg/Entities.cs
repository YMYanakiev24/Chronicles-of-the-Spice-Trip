/*
 * Entities.cs
 * ----------------------------------------------------------------------------
 * Ports of Player.js, Enemy.js and NPC.js.
 *
 *   Player  the Unmarked demigod: 8-directional movement with tile collision
 *           and sliding, a dodge-roll with i-frames, the godless-blade melee,
 *           mouse-aimed casting and all progression (HP, mana, XP, level,
 *           gold, learned powers, inventory)
 *   Enemy   wander → chase → attack state machine, melee + ranged, weakness
 *           damage, floating health bar, hit flash
 *   Npc     quest-givers with floating markers (! available, ? active,
 *           ✔ ready to turn in)
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    public static class Dir
    {
        /// <summary>dir4(): map a vector to 'down|up|left|right'.</summary>
        public static string Of(float dx, float dy)
        {
            if (Mathf.Abs(dx) > Mathf.Abs(dy)) return dx < 0 ? "left" : "right";
            return dy < 0 ? "up" : "down";
        }

        public static Vector2 Vec(string facing) => facing switch
        {
            "up" => new Vector2(0, -1),
            "left" => new Vector2(-1, 0),
            "right" => new Vector2(1, 0),
            _ => new Vector2(0, 1),
        };
    }

    public class Player
    {
        public const float Walk = 74, Run = 122, RollSpeed = 210;
        public const float Radius = 5;          // collision radius in px

        readonly ChronicleGame _g;
        readonly Dictionary<string, PixelCanvas[]> _frames;

        // Kinematics
        public float x = 100, y = 100, vx, vy;
        public string facing = "down";
        public bool moving;
        float _animTime;
        public int frame;

        // Aim (world coords of the cursor)
        public float aimX = 100, aimY = 99;

        // Roll
        public float rolling, rollCd, rollDX, rollDY = 1;

        // Melee
        public float meleeCd, meleeTime;

        // Stats & progression
        public int level = 1;
        public float xp;
        public float maxHp = 100, hp = 100, maxMana = 100, mana = 100;
        public int gold;
        public float manaRegen = 8, hpRegen = 1.5f;
        public float shield, shieldTimer, regenBuff;
        public float combatTimer = 99, invuln;
        public readonly Dictionary<string, float> cooldowns = new Dictionary<string, float>();

        public readonly HashSet<string> learnedSpells = new HashSet<string> { "emberhex" };
        public readonly Dictionary<string, int> spellLevels = new Dictionary<string, int> { ["emberhex"] = 1 };
        public readonly string[] hotbar = { "emberhex", null, null, null, null, null };
        public readonly Dictionary<string, int> inventory = new Dictionary<string, int>();
        public readonly HashSet<string> relics = new HashSet<string>();
        public bool dead;

        public float castFlash;
        public string castColor = "#bfe0ff";

        public Player(ChronicleGame game)
        {
            _g = game;
            _frames = PixelArt.Hero();
        }

        public float Baseline => y;

        // Brighter torch in darker regions so you can always see a little.
        public float TorchRadius => 56 + (_g.Map.Region != null ? _g.Map.Region.darkness : 0.6f) * 46;

        // --- Progression ---------------------------------------------------------
        public void LearnSpell(string id)
        {
            var spell = _g.Db.Spell(id);
            if (learnedSpells.Contains(id))
            {
                spellLevels[id] = (spellLevels.TryGetValue(id, out var l) ? l : 1) + 1;
                _g.Ui.Toast($"{spell.name} deepens — tier {spellLevels[id]}!", "gold");
                return;
            }
            learnedSpells.Add(id);
            spellLevels[id] = 1;
            int slot = Array.IndexOf(hotbar, null);
            if (slot >= 0) hotbar[slot] = id;
            _g.Ui.Toast($"New power: {spell.name}! (slot {slot + 1})", "gold");
            _g.Audio.Sfx("levelup");
        }

        public void AddItem(string id, int n = 1)
        {
            inventory[id] = ItemCount(id) + n;
            if (id == "cinderSigil") relics.Add("cinderSigil");
        }

        public void RemoveItem(string id, int n = 1)
        {
            if (ItemCount(id) == 0) return;
            int left = Mathf.Max(0, inventory[id] - n);
            if (left == 0) inventory.Remove(id); else inventory[id] = left;
        }

        public int ItemCount(string id) => inventory.TryGetValue(id, out var n) ? n : 0;
        public void AddGold(int n) => gold += n;

        public void AddXP(int n)
        {
            xp += n;
            _g.Ui.Toast($"+{n} XP", "xp");
            while (xp >= GameDatabase.XpForLevel(level)) { xp -= GameDatabase.XpForLevel(level); LevelUp(); }
            _g.Ui.RefreshHUD();
        }

        void LevelUp()
        {
            level++; maxHp += 15; maxMana += 12;
            hp = maxHp; mana = maxMana; manaRegen += 0.6f;
            _g.Audio.Sfx("levelup");
            _g.Ui.Toast($"Level {level}! The god-blood burns brighter.", "gold");
            _g.Ui.BigBanner($"LEVEL {level}");
            foreach (var s in _g.Db.spells)
                if (s.unlockLevel == level && !learnedSpells.Contains(s.id)) LearnSpell(s.id);
        }

        // --- Combat -----------------------------------------------------------------
        public void TakeDamage(float amount)
        {
            if (dead || invuln > 0) return;
            combatTimer = 0;
            if (shield > 0)
            {
                float a = Mathf.Min(shield, amount); shield -= a; amount -= a;
                if (shield <= 0) _g.Ui.Toast("Your Grey Ward shatters!", "warn");
            }
            if (amount <= 0) { _g.Ui.RefreshHUD(); return; }
            hp = Mathf.Clamp(hp - amount, 0, maxHp);
            invuln = 0.4f;
            _g.Audio.Sfx("hurt");
            _g.Ui.DamageFlash();
            _g.Ui.RefreshHUD();
            if (hp <= 0) Die();
        }

        public void Heal(float a) { hp = Mathf.Clamp(hp + a, 0, maxHp); _g.Ui.RefreshHUD(); }
        public void RestoreMana(float a) { mana = Mathf.Clamp(mana + a, 0, maxMana); _g.Ui.RefreshHUD(); }
        public void ApplyShield(float a, float d) { shield = a; shieldTimer = d; }

        void Die()
        {
            if (dead) return;
            dead = true;
            _g.Audio.Sfx("death");
            _g.OnPlayerDeath();
        }

        public void Respawn()
        {
            dead = false; hp = maxHp; mana = maxMana; shield = 0;
            var sp = _g.Map.Region.spawn;
            x = sp.tx * 16 + 8; y = sp.ty * 16 + 8; vx = vy = 0; invuln = 2;
        }

        // --- Casting --------------------------------------------------------------
        public void TryCastSlot(int slot) { var id = hotbar[slot]; if (id != null) CastSpell(id); }

        public void CastSpell(string id)
        {
            if (dead || rolling > 0) return;
            var spell = _g.Db.Spell(id);
            if (spell == null || !learnedSpells.Contains(id)) return;
            if (Cooldown(id) > 0) { _g.Ui.Toast($"{spell.name} is gathering...", "warn", 0.6f); return; }
            if (mana < spell.manaCost) { _g.Ui.Toast("Not enough mana!", "warn", 0.6f); _g.Audio.Sfx("ui"); return; }
            mana -= spell.manaCost;
            cooldowns[id] = spell.cooldown;
            castFlash = 0.25f; castColor = spell.glow;
            // Face the cursor when casting.
            facing = Dir.Of(aimX - x, aimY - y);
            _g.Magic.Cast(id, this, spellLevels.TryGetValue(id, out var l) ? l : 1);
            _g.Quests.OnCast(id);
            _g.Ui.RefreshHUD();
        }

        public float Cooldown(string id) => cooldowns.TryGetValue(id, out var c) ? c : 0;

        public void Melee()
        {
            if (meleeCd > 0 || dead || rolling > 0) return;
            meleeCd = 0.45f; meleeTime = 0.2f;
            facing = Dir.Of(aimX - x, aimY - y);
            _g.Audio.Sfx("ui");
            // Hit enemies in a short arc toward the aim.
            float ang = Mathf.Atan2(aimY - y, aimX - x);
            foreach (var e in _g.Enemies.ToArray())
            {
                if (e.dead) continue;
                float d = Vector2.Distance(new Vector2(x, y), new Vector2(e.x, e.y));
                if (d > 24 + e.r) continue;
                float ea = Mathf.Atan2(e.y - y, e.x - x);
                float diff = Mathf.Abs(JsMod(ea - ang + Mathf.PI, Mathf.PI * 2) - Mathf.PI);
                if (diff < 1.1f) _g.Combat.DamageEnemy(e, 12 + level * 2, "Blade");
            }
        }

        /// <summary>JavaScript's % (keeps the dividend's sign).</summary>
        static float JsMod(float a, float b) => a - b * (float)Math.Truncate(a / b);

        void Roll()
        {
            if (rollCd > 0 || dead) return;
            rolling = 0.32f; rollCd = 0.7f; invuln = 0.32f;
            // Roll in movement dir, or facing if standing still.
            float dx = vx, dy = vy;
            if (Mathf.Abs(dx) + Mathf.Abs(dy) < 1) { var f = Dir.Vec(facing); dx = f.x; dy = f.y; }
            float m = Mathf.Sqrt(dx * dx + dy * dy);
            if (m == 0) m = 1;
            rollDX = dx / m; rollDY = dy / m;
            _g.Audio.Sfx("ui");
        }

        // --- Per-frame -----------------------------------------------------------------
        public void Update(float dt, in RpgInput inp)
        {
            aimX = inp.aimWorld.x; aimY = inp.aimWorld.y;

            // Timers
            foreach (var k in new List<string>(cooldowns.Keys))
                if (cooldowns[k] > 0) cooldowns[k] = Mathf.Max(0, cooldowns[k] - dt);
            if (rollCd > 0) rollCd -= dt;
            if (meleeCd > 0) meleeCd -= dt;
            if (meleeTime > 0) meleeTime -= dt;
            if (invuln > 0) invuln -= dt;
            if (castFlash > 0) castFlash -= dt;
            if (shieldTimer > 0) { shieldTimer -= dt; if (shieldTimer <= 0) shield = 0; }
            combatTimer += dt;

            // Movement
            float mvx = 0, mvy = 0;
            if (rolling > 0)
            {
                rolling -= dt;
                mvx = rollDX * RollSpeed; mvy = rollDY * RollSpeed;
                moving = true;
            }
            else
            {
                float len = Mathf.Sqrt(inp.moveX * inp.moveX + inp.moveY * inp.moveY);
                moving = len > 0.01f;
                float speed = inp.run ? Run : Walk;
                if (moving)
                {
                    mvx = (inp.moveX / len) * speed; mvy = (-inp.moveY / len) * speed;   // axis.y is +1 up
                    facing = Dir.Of(mvx, mvy);
                }
                if (inp.rollPressed) Roll();
            }
            vx = mvx; vy = mvy;

            // Integrate with axis-separated collision (slide along walls).
            Move(mvx * dt, 0);
            Move(0, mvy * dt);

            // Keep inside the map.
            x = Mathf.Clamp(x, Radius + 2, _g.Map.PixelW - Radius - 2);
            y = Mathf.Clamp(y, Radius + 2, _g.Map.PixelH - Radius - 2);

            // Animation
            if (moving) { _animTime += dt; frame = (Mathf.FloorToInt(_animTime * 8) % 2) + 1; }
            else { _animTime = 0; frame = 0; }

            // Regen
            mana = Mathf.Clamp(mana + manaRegen * dt, 0, maxMana);
            float hpr = hpRegen + regenBuff;
            if (combatTimer < 3) hpr *= 0.25f;
            hp = Mathf.Clamp(hp + hpr * dt, 0, maxHp);
            if (regenBuff > 0) regenBuff = Mathf.Max(0, regenBuff - dt * 2);
        }

        void Move(float dx, float dy)
        {
            float nx = x + dx, ny = y + dy;
            var m = _g.Map;
            // Probe the leading edge of the collision circle.
            float ex = dx != 0 ? nx + Mathf.Sign(dx) * Radius : x;
            float ey = dy != 0 ? ny + Mathf.Sign(dy) * Radius : y;
            if (dx != 0 && !m.SolidAt(ex, y - Radius + 2) && !m.SolidAt(ex, y + Radius - 2)) x = nx;
            if (dy != 0 && !m.SolidAt(x - Radius + 2, ey) && !m.SolidAt(x + Radius - 2, ey)) y = ny;
        }

        public void Draw(IWorldPainter g)
        {
            // Blink while invulnerable.
            if (invuln > 0 && Mathf.FloorToInt(invuln * 20) % 2 == 0 && !(rolling > 0)) return;
            var spr = _frames[facing][frame];
            float sx = Mathf.Round(x - spr.Width / 2f), sy = Mathf.Round(y - spr.Height + 3);
            if (rolling > 0) g.SpriteRotated(spr, sx + spr.Width / 2f, sy + spr.Height / 2f, (0.32f - rolling) * 12);
            else g.Sprite(spr, sx, sy);

            // Melee blade arc.
            if (meleeTime > 0)
            {
                float ang = Mathf.Atan2(aimY - y, aimX - x);
                g.Arc(x, y - 6, 16, ang - 0.8f, ang + 0.8f, 2, Css.C("#cdefff"));
            }
            // Grey Ward bubble.
            if (shield > 0) g.Arc(x, y - 8, 12, 0, 7, 1, new Color(200 / 255f, 210 / 255f, 230 / 255f, 0.6f));
        }
    }

    public class Enemy
    {
        readonly ChronicleGame _g;
        public readonly CreatureDef def;
        readonly PixelCanvas[] _frames;
        public PixelCanvas spr;
        public float x, y, vx, vy, hp, maxHp, r;
        public string state = "wander";
        float _attackCd, _slowT, _slowF = 1, _rootT;
        public float hitFlash, deathTimer, lunge, barTimer;
        public bool dead;
        readonly float _bob;
        float _animTime;
        int _frame;
        readonly float _homeX, _homeY;
        float _wx, _wy, _wWait;

        public Enemy(ChronicleGame game, CreatureDef d, float x, float y)
        {
            _g = game;
            def = d;
            _frames = PixelArt.Monster(new PixelArt.MonsterLook { id = d.id, body = d.body, color = d.color, size = d.size, boss = d.boss });
            spr = _frames[0];
            this.x = x; this.y = y;
            hp = maxHp = d.hp;
            r = d.size * 0.45f;
            _attackCd = game.Rand(0, d.attackRate);
            _bob = game.Rand(0, 7);
            _animTime = game.Rand(0, 1);
            _homeX = x; _homeY = y;
            PickWander();
        }

        public float Baseline => y;

        public void TakeDamage(float amount, string element)
        {
            if (dead) return;
            bool weak = element != null && element == def.weakness;
            if (weak) amount *= 2;
            hp = Mathf.Clamp(hp - amount, 0, maxHp);
            hitFlash = 0.12f; barTimer = 3; state = "chase";
            _g.Audio.Sfx("hit");
            _g.Ui.SpawnDamageNumber(x, y - spr.Height * 0.6f, Mathf.RoundToInt(amount), weak);
            if (hp <= 0) Die();
        }

        public void ApplySlow(float f, float d) { _slowT = d; _slowF = f; }
        public void ApplyRoot(float d) { _rootT = d; }

        void Die()
        {
            dead = true; deathTimer = 0.6f;
            _g.Audio.Sfx("death");
            _g.Combat.OnEnemyKilled(this);
        }

        void PickWander()
        {
            float a = _g.Rand(0, 7), rr = _g.Rand(20, 80);
            _wx = _homeX + Mathf.Cos(a) * rr; _wy = _homeY + Mathf.Sin(a) * rr;
            _wWait = _g.Rand(1, 3);
        }

        public void Update(float dt)
        {
            if (dead) { deathTimer -= dt; return; }
            var p = _g.Player;
            float dx = p.x - x, dy = p.y - y;
            float d = Mathf.Sqrt(dx * dx + dy * dy);

            if (_attackCd > 0) _attackCd -= dt;
            if (_slowT > 0) _slowT -= dt;
            if (_rootT > 0) _rootT -= dt;
            if (hitFlash > 0) hitFlash -= dt;
            if (lunge > 0) lunge -= dt;
            if (barTimer > 0) barTimer -= dt;

            float speed = def.speed * (_slowT > 0 ? _slowF : 1);
            if (_rootT > 0) speed = 0;

            if (!p.dead && d < def.aggroRange) state = "chase";
            else if (state == "chase" && d > def.aggroRange * 1.6f) state = "wander";

            float mvx = 0, mvy = 0;
            if (state == "chase" && !p.dead)
            {
                bool inRange = d <= def.attackRange;
                float nd = d == 0 ? 1 : d;
                if (def.ranged)
                {
                    // Keep at mid distance and fire.
                    if (d < def.attackRange * 0.5f) { mvx = -dx / nd * speed; mvy = -dy / nd * speed; }
                    else if (d > def.attackRange) { mvx = dx / nd * speed; mvy = dy / nd * speed; }
                    if (_attackCd <= 0 && d < def.attackRange * 1.1f) Attack(dx, dy, d);
                }
                else
                {
                    if (inRange) { if (_attackCd <= 0) Attack(dx, dy, d); }
                    else { mvx = dx / nd * speed; mvy = dy / nd * speed; }
                }
            }
            else
            {
                float wdx = _wx - x, wdy = _wy - y, wd = Mathf.Sqrt(wdx * wdx + wdy * wdy);
                if (wd < 4) { _wWait -= dt; if (_wWait <= 0) PickWander(); }
                else { mvx = wdx / wd * speed * 0.4f; mvy = wdy / wd * speed * 0.4f; }
            }

            vx = mvx; vy = mvy;
            Move(mvx * dt, 0); Move(0, mvy * dt);

            // Animation
            if (Mathf.Abs(mvx) + Mathf.Abs(mvy) > 4 || def.@float) { _animTime += dt; _frame = Mathf.FloorToInt(_animTime * 6) % 2; }
            else _frame = 0;
            spr = _frames[_frame];
        }

        void Move(float dx, float dy)
        {
            var m = _g.Map;
            float nx = x + dx, ny = y + dy;
            if (dx != 0 && !m.SolidAt(nx + Mathf.Sign(dx) * r, y)) x = nx;
            if (dy != 0 && !m.SolidAt(x, ny + Mathf.Sign(dy) * r)) y = ny;
        }

        void Attack(float dx, float dy, float d)
        {
            _attackCd = def.attackRate;
            lunge = 0.2f;
            if (def.ranged)
            {
                float nd = d == 0 ? 1 : d;
                _g.Magic.EnemyProjectile(x, y - spr.Height * 0.4f, dx / nd, dy / nd, def.damage, def.color);
            }
            else if (d <= def.attackRange + 4)
            {
                _g.Combat.DamagePlayer(def.damage);
            }
        }

        public void Draw(IWorldPainter g)
        {
            float a = dead ? Mathf.Max(0, deathTimer / 0.6f) : 1;
            float bob = def.@float ? Mathf.Sin(_bob + _g.RealTimeMs * 0.004f) * 2 : 0;
            float lungeOff = lunge > 0 ? 2 : 0;
            float sx = Mathf.Round(x - spr.Width / 2f);
            float sy = Mathf.Round(y - spr.Height + 3 + bob - lungeOff);
            g.Sprite(spr, sx, sy, a);
            if (hitFlash > 0) g.Sprite(spr, sx, sy, 0.8f, Blend.Additive);

            // Health bar
            if (barTimer > 0 && !dead)
            {
                float w = Mathf.Max(14, spr.Width);
                float bx = Mathf.Round(x - w / 2), by = sy - 4;
                g.Rect(bx, by, w, 3, Css.C("#120a10"));
                float ratio = hp / maxHp;
                g.Rect(bx, by, Mathf.Round(w * ratio), 3, Css.C(ratio > 0.5f ? "#7fd06a" : ratio > 0.25f ? "#e0c24a" : "#e0533a"));
            }
        }
    }

    public class Npc
    {
        readonly ChronicleGame _g;
        public readonly NpcDef def;
        readonly Dictionary<string, PixelCanvas[]> _frames;
        public readonly float x, y;
        public string facing = "down";
        readonly float _bob;
        public bool talkedTo;

        public Npc(ChronicleGame game, NpcDef d)
        {
            _g = game;
            def = d;
            _frames = PixelArt.Npc(d.color);
            x = d.tx * 16 + 8;
            y = d.ty * 16 + 8;
            _bob = game.Rand(0, 7);
        }

        public float Baseline => y;
        public float DistanceTo(float px, float py) => Vector2.Distance(new Vector2(x, y), new Vector2(px, py));
        public void Interact() { talkedTo = true; _g.StartDialogue(this); }

        public void Update(float dt)
        {
            // Face the player when near.
            var p = _g.Player;
            if (DistanceTo(p.x, p.y) < 40)
            {
                float dx = p.x - x, dy = p.y - y;
                facing = Mathf.Abs(dx) > Mathf.Abs(dy) ? (dx < 0 ? "left" : "right") : (dy < 0 ? "up" : "down");
            }
        }

        public void Draw(IWorldPainter g)
        {
            float bob = Mathf.Sin(_bob + _g.RealTimeMs * 0.002f) * 1;
            var spr = _frames[facing][0];
            float sx = Mathf.Round(x - spr.Width / 2f), sy = Mathf.Round(y - spr.Height + 3 + bob);
            g.Sprite(spr, sx, sy);

            // Quest marker
            var glyph = _g.Quests.MarkerFor(def.id);
            if (!string.IsNullOrEmpty(glyph))
            {
                float my = sy - 8 + Mathf.Sin(_g.RealTimeMs * 0.004f + _bob) * 1.5f;
                var col = Css.C(glyph == "!" ? "#ffd24a" : glyph == "✔" ? "#7fe06b" : "#9fd0ff");
                g.Glyph(glyph, x + 1, my + 1, new Color(0, 0, 0, 0.7f));
                g.Glyph(glyph, x, my, col);
            }
        }
    }
}
