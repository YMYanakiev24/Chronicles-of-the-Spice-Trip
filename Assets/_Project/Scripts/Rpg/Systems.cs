/*
 * Systems.cs
 * ----------------------------------------------------------------------------
 * Ports of MagicSystem.js, CombatSystem.js and QuestSystem.js.
 *
 *   MagicSystem   spell id + caster → live visuals and damage: projectiles
 *                 (Emberhex, Tidelash, enemy bolts), the chaining Stormbolt
 *                 beam, the self-buffs (Wildmend, Grey Ward) and the
 *                 screen-shaking ultimate; owns transient effects and the
 *                 lights spells cast into the dark
 *   CombatSystem  the single arbiter of damage, XP, gold, loot and quest hooks
 *   QuestSystem   available → active → ready → completed, objective counters,
 *                 rewards, tracking and NPC markers
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    public struct DynLight
    {
        public float x, y, r, intensity;
        public string color;
    }

    public class MagicSystem
    {
        public class Projectile
        {
            public float x, y, vx, vy, life, damage, radius, splash, slow, root;
            public bool fromEnemy, ring;
            public string element, color, glow;
            public readonly List<Vector2> trail = new List<Vector2>();
        }

        public abstract class Effect
        {
            protected float age;
            public abstract bool Update(float dt);
            public abstract void Draw(IWorldPainter g);
            public virtual DynLight? Light() => null;
        }

        readonly ChronicleGame _g;
        public readonly List<Projectile> Projectiles = new List<Projectile>();
        public readonly List<Effect> Effects = new List<Effect>();

        public MagicSystem(ChronicleGame game) { _g = game; }

        public void Cast(string id, Player caster, int level = 1)
        {
            var spell = _g.Db.Spell(id);
            float ox = caster.x, oy = caster.y - 8;
            switch (spell.kind)
            {
                case "projectile": ProjectileSpell(spell, caster, ox, oy, level); break;
                case "beam": Beam(spell, caster, ox, oy, level); break;
                case "self": Self(spell, caster, level); break;
                case "aoe": Aoe(spell, caster, level, spell.radius); break;
            }
        }

        static Vector2 AimDir(Player caster, float ox, float oy)
        {
            var v = new Vector2(caster.aimX - ox, caster.aimY - oy);
            float m = v.magnitude;
            return m == 0 ? new Vector2(v.x, v.y) : v / m;
        }

        // --- Projectiles ----------------------------------------------------------
        void ProjectileSpell(SpellDef spell, Player caster, float ox, float oy, int level)
        {
            var d = AimDir(caster, ox, oy);
            float dmg = spell.damage * (1 + (level - 1) * 0.25f);
            if (spell.school == "Fire" && caster.relics.Contains("cinderSigil")) dmg *= 1.1f;
            Projectiles.Add(new Projectile
            {
                x = ox + d.x * 8, y = oy + d.y * 8, vx = d.x * spell.speed, vy = d.y * spell.speed,
                life = 1.6f, fromEnemy = false, damage = dmg, element = spell.school,
                radius = spell.radius, splash = spell.splash, slow = spell.slow,
                root = level >= 2 && spell.id == "tidelash" ? 1.0f : 0,
                ring = level >= 2 && spell.id == "emberhex",
                color = spell.color, glow = spell.glow,
            });
            _g.Audio.Sfx(spell.school == "Fire" ? "fire" : "nature");
            Burst(ox + d.x * 6, oy + d.y * 6, spell.color, 6, 0.3f, 40);
        }

        public void EnemyProjectile(float x, float y, float dx, float dy, float damage, string color)
        {
            Projectiles.Add(new Projectile
            {
                x = x, y = y, vx = dx * 130, vy = dy * 130, life = 2.4f, fromEnemy = true,
                damage = damage, element = null, radius = 4, splash = 0, color = color, glow = color,
            });
            _g.Audio.Sfx("lightning");
        }

        // --- Beam (Stormbolt): instant hit + chain --------------------------------
        void Beam(SpellDef spell, Player caster, float ox, float oy, int level)
        {
            var d = AimDir(caster, ox, oy);
            int maxChain = spell.chain + (level - 1);
            var hit = new HashSet<Enemy>();
            float fx = ox, fy = oy;
            var target = ConeTarget(ox, oy, d.x, d.y, spell.range, hit);
            float dmg = spell.damage * (1 + (level - 1) * 0.25f);
            int n = 0;
            while (target != null && n <= maxChain)
            {
                hit.Add(target);
                Lightning(fx, fy, target.x, target.y - 8, spell.color);
                _g.Combat.DamageEnemy(target, dmg, spell.school);
                fx = target.x; fy = target.y - 8; dmg *= 0.7f; n++;
                target = Nearest(fx, fy, 70, hit);
            }
            if (n == 0) Lightning(ox, oy, ox + d.x * spell.range, oy + d.y * spell.range, spell.color);
            _g.Audio.Sfx("lightning");
            _g.Ui.Shake(0.25f);
        }

        Enemy ConeTarget(float ox, float oy, float dx, float dy, float range, HashSet<Enemy> exclude)
        {
            Enemy best = null;
            float bestScore = -1;
            foreach (var e in _g.Enemies)
            {
                if (e.dead || exclude.Contains(e)) continue;
                float ex = e.x - ox, ey = e.y - oy, dd = Mathf.Sqrt(ex * ex + ey * ey);
                if (dd > range) continue;
                float dot = (ex / dd) * dx + (ey / dd) * dy;
                if (dot < 0.5f) continue;
                float score = dot - dd / range;
                if (score > bestScore) { bestScore = score; best = e; }
            }
            return best;
        }

        Enemy Nearest(float x, float y, float range, HashSet<Enemy> exclude)
        {
            Enemy best = null;
            float bd = range;
            foreach (var e in _g.Enemies)
            {
                if (e.dead || exclude.Contains(e)) continue;
                float d = Vector2.Distance(new Vector2(x, y), new Vector2(e.x, e.y));
                if (d < bd) { bd = d; best = e; }
            }
            return best;
        }

        // --- Self -----------------------------------------------------------------
        void Self(SpellDef spell, Player caster, int level)
        {
            if (spell.id == "wildmend")
            {
                caster.Heal(spell.heal * (1 + (level - 1) * 0.3f));
                if (level >= 2) caster.regenBuff = 8;
                _g.Audio.Sfx("heal");
                Effects.Add(new Aura(caster, "#76e06b"));
            }
            else if (spell.id == "greyward")
            {
                caster.ApplyShield(spell.shield * (1 + (level - 1) * 0.3f), spell.duration);
                _g.Audio.Sfx("shield");
                Effects.Add(new Aura(caster, "#cdd6ff"));
            }
        }

        // --- AoE ------------------------------------------------------------------
        void Aoe(SpellDef spell, Player caster, int level, float radius)
        {
            _g.Audio.Sfx("ultimate");
            _g.Ui.Shake(1.0f);
            _g.Ui.BigBanner("STORMCROWN'S WRATH");
            float dmg = spell.damage * (1 + (level - 1) * 0.25f);
            foreach (var e in _g.Enemies.ToArray())
                if (!e.dead && Vector2.Distance(new Vector2(caster.x, caster.y), new Vector2(e.x, e.y)) <= radius)
                    _g.Combat.DamageEnemy(e, dmg, "Lightning");
            for (int i = 0; i < 12; i++)
            {
                float a = _g.Rand(0, 7), r = _g.Rand(0, radius);
                float gx = caster.x + Mathf.Cos(a) * r, gy = caster.y + Mathf.Sin(a) * r;
                _g.After(i * 0.05f, () => Lightning(gx, gy - 60, gx, gy, spell.color, 2));
            }
            Effects.Add(new Shockwave(caster.x, caster.y, radius, spell.color));
            // Reckoning: the storm strikes again at 80% radius.
            if (level >= 2) _g.After(0.8f, () => { if (!caster.dead) Aoe(spell, caster, 1, radius * 0.8f); });
        }

        // =========================================================================
        // Effects
        // =========================================================================
        void Burst(float x, float y, string color, int count, float life, float speed) =>
            Effects.Add(new BurstFx(_g, x, y, color, count, life, speed));

        void Lightning(float x1, float y1, float x2, float y2, string color, float width = 1)
        {
            Effects.Add(new LightningFx(_g, x1, y1, x2, y2, color, width));
            Burst(x2, y2, color, 6, 0.25f, 50);
        }

        class BurstFx : Effect
        {
            readonly Vector2[] _p, _v;
            readonly float _life;
            readonly Color _c;

            public BurstFx(ChronicleGame g, float x, float y, string color, int count, float life, float speed)
            {
                _life = life; _c = Css.C(color);
                _p = new Vector2[count]; _v = new Vector2[count];
                for (int i = 0; i < count; i++)
                {
                    float a = g.Rand(0, 7), s = g.Rand(speed * 0.4f, speed);
                    _p[i] = new Vector2(x, y); _v[i] = new Vector2(Mathf.Cos(a) * s, Mathf.Sin(a) * s);
                }
            }

            public override bool Update(float dt)
            {
                age += dt;
                for (int i = 0; i < _p.Length; i++) { _p[i] += _v[i] * dt; _v[i] *= 0.9f; }
                return age < _life;
            }

            public override void Draw(IWorldPainter g)
            {
                var c = _c; c.a = Mathf.Clamp01(1 - age / _life);
                foreach (var p in _p) g.Rect(Mathf.Round(p.x), Mathf.Round(p.y), 2, 2, c, Blend.Additive);
            }
        }

        class LightningFx : Effect
        {
            const float Life = 0.22f;
            readonly Vector2[] _pts = new Vector2[8];
            readonly float _x2, _y2, _w;
            readonly string _color;

            public LightningFx(ChronicleGame g, float x1, float y1, float x2, float y2, string color, float width)
            {
                _x2 = x2; _y2 = y2; _color = color; _w = width;
                const int segs = 7;
                for (int i = 0; i <= segs; i++)
                {
                    float t = (float)i / segs;
                    float x = x1 + (x2 - x1) * t, y = y1 + (y2 - y1) * t;
                    if (i > 0 && i < segs) { x += g.Rand(-4, 4) * width; y += g.Rand(-4, 4) * width; }
                    _pts[i] = new Vector2(x, y);
                }
            }

            public override bool Update(float dt) { age += dt; return age < Life; }
            public override DynLight? Light() => new DynLight { x = _x2, y = _y2, r = 34, color = _color, intensity = 0.3f * Mathf.Max(0, 1 - age / Life) };

            public override void Draw(IWorldPainter g)
            {
                var c = Css.A(_color, Mathf.Max(0, 1 - age / Life));
                for (int i = 1; i < _pts.Length; i++) g.Line(_pts[i - 1].x, _pts[i - 1].y, _pts[i].x, _pts[i].y, _w + 1, c, Blend.Additive);
            }
        }

        class Aura : Effect
        {
            const float Life = 0.7f;
            readonly Player _caster;
            readonly string _color;

            public Aura(Player caster, string color) { _caster = caster; _color = color; }
            public override bool Update(float dt) { age += dt; return age < Life; }
            public override DynLight? Light() => new DynLight { x = _caster.x, y = _caster.y - 8, r = 30, color = _color, intensity = 0.3f };

            public override void Draw(IWorldPainter g)
            {
                float r = 4 + age * 24;
                g.Arc(_caster.x, _caster.y - 8 - age * 10, r, 0, 7, 2, Css.A(_color, Mathf.Max(0, 1 - age / Life)), Blend.Additive);
            }
        }

        public class Shockwave : Effect
        {
            const float Life = 0.6f;
            readonly float _x, _y, _radius;
            readonly string _color;

            public Shockwave(float x, float y, float radius, string color) { _x = x; _y = y; _radius = radius; _color = color; }
            public override bool Update(float dt) { age += dt; return age < Life; }
            public override DynLight? Light() => new DynLight { x = _x, y = _y, r = _radius, color = _color, intensity = 0.4f * Mathf.Max(0, 1 - age / Life) };

            public override void Draw(IWorldPainter g) =>
                g.Arc(_x, _y, (age / Life) * _radius, 0, 7, 3, Css.A(_color, Mathf.Max(0, 0.9f * (1 - age / Life))), Blend.Additive);
        }

        // =========================================================================
        // Update / draw / lights
        // =========================================================================
        public void Update(float dt)
        {
            for (int i = Projectiles.Count - 1; i >= 0; i--)
            {
                var p = Projectiles[i];
                p.life -= dt;
                p.trail.Add(new Vector2(p.x, p.y));
                if (p.trail.Count > 6) p.trail.RemoveAt(0);
                p.x += p.vx * dt; p.y += p.vy * dt;
                bool hit = _g.Map.SolidAt(p.x, p.y);

                if (p.fromEnemy)
                {
                    var pl = _g.Player;
                    if (!pl.dead && Vector2.Distance(new Vector2(p.x, p.y), new Vector2(pl.x, pl.y - 8)) < p.radius + 7) { _g.Combat.DamagePlayer(p.damage); hit = true; }
                }
                else
                {
                    foreach (var e in _g.Enemies)
                    {
                        if (e.dead) continue;
                        if (Vector2.Distance(new Vector2(p.x, p.y), new Vector2(e.x, e.y - e.spr.Height * 0.4f)) < p.radius + e.r) { Impact(p, e); hit = true; break; }
                    }
                }
                if (hit || p.life <= 0)
                {
                    if (hit) Burst(p.x, p.y, p.color, 8, 0.3f, 60);
                    if (hit && p.ring) Effects.Add(new Shockwave(p.x, p.y, 26, p.color));
                    Projectiles.RemoveAt(i);
                }
            }
            for (int i = Effects.Count - 1; i >= 0; i--)
                if (!Effects[i].Update(dt)) Effects.RemoveAt(i);
        }

        void Impact(Projectile p, Enemy primary)
        {
            _g.Combat.DamageEnemy(primary, p.damage, p.element);
            if (p.slow > 0) primary.ApplySlow(p.slow, 2.5f);
            if (p.root > 0) primary.ApplyRoot(p.root);
            if (p.splash > 0)
                foreach (var e in _g.Enemies.ToArray())
                {
                    if (e.dead || e == primary) continue;
                    if (Vector2.Distance(new Vector2(p.x, p.y), new Vector2(e.x, e.y)) <= p.splash)
                    {
                        _g.Combat.DamageEnemy(e, p.damage * 0.5f, p.element);
                        if (p.slow > 0) e.ApplySlow(p.slow, 1.5f);
                    }
                }
        }

        public void Draw(IWorldPainter g)
        {
            foreach (var p in Projectiles)
            {
                // trail
                var glow = Css.C(p.glow);
                for (int i = 0; i < p.trail.Count; i++)
                {
                    float t = (float)i / p.trail.Count;
                    var c = glow; c.a = t * 0.5f;
                    g.Rect(Mathf.Round(p.trail[i].x) - 1, Mathf.Round(p.trail[i].y) - 1, 2, 2, c, Blend.Additive);
                }
                // glow
                float sx = Mathf.Round(p.x), sy = Mathf.Round(p.y);
                g.Glow(sx, sy, p.radius * 2.4f, glow);
                g.Rect(sx - 1, sy - 1, 2, 2, Color.white, Blend.Additive);
            }
            foreach (var e in Effects) e.Draw(g);
        }

        public void GetLights(List<DynLight> into)
        {
            foreach (var p in Projectiles) into.Add(new DynLight { x = p.x, y = p.y, r = 26, color = p.glow, intensity = 0.22f });
            foreach (var e in Effects) { var l = e.Light(); if (l.HasValue) into.Add(l.Value); }
        }

        public void Clear() { Projectiles.Clear(); Effects.Clear(); }
    }

    public class CombatSystem
    {
        readonly ChronicleGame _g;
        public CombatSystem(ChronicleGame game) { _g = game; }

        public void DamageEnemy(Enemy enemy, float amount, string element)
        {
            if (enemy == null || enemy.dead) return;
            enemy.TakeDamage(amount, element);
        }

        public void DamagePlayer(float amount) => _g.Player.TakeDamage(amount);

        public void OnEnemyKilled(Enemy enemy)
        {
            var def = enemy.def;
            var player = _g.Player;

            // Reward XP + gold.
            player.AddXP(def.xp);
            int gold = Mathf.RoundToInt(def.xp * (0.4f + _g.Rand(0, 1) * 0.6f));
            player.AddGold(gold);

            // Loot: bosses always drop something nice; others sometimes drop a draught.
            if (def.boss)
            {
                player.AddItem("ambrosiaCake", 2);
                _g.Ui.Toast($"{def.name} defeated! Loot acquired.", "gold");
            }
            else if (_g.Rand(0, 1) < 0.25f)
            {
                var drop = _g.Rand(0, 1) < 0.5f ? "ambrosiaCake" : "nectarVial";
                player.AddItem(drop);
                _g.Ui.Toast($"Found {_g.Db.Item(drop).name}", "pickup");
            }

            // Quest hook + bestiary.
            _g.Quests.OnKill(def.id);
            _g.BestiarySeen.Add(def.id);
        }
    }

    public class QuestSystem
    {
        readonly ChronicleGame _g;
        public readonly Dictionary<string, string> state = new Dictionary<string, string>();   // available | active | ready | completed
        public readonly Dictionary<string, int[]> progress = new Dictionary<string, int[]>();
        public string tracked;

        public QuestSystem(ChronicleGame game)
        {
            _g = game;
            foreach (var id in game.Db.questOrder)
            {
                state[id] = "available";
                progress[id] = new int[game.Db.Quest(id).objectives.Count];
            }
            tracked = game.Db.questOrder.Count > 0 ? game.Db.questOrder[0] : null;
        }

        QuestDef Q(string id) => _g.Db.Quest(id);
        static int Need(ObjectiveDef o) => o.count > 0 ? o.count : 1;

        // --- Lifecycle ------------------------------------------------------------
        public void Accept(string id)
        {
            if (state[id] != "available") return;
            state[id] = "active";
            tracked = id;
            _g.Audio.Sfx("quest");
            _g.Ui.Toast($"Quest accepted: {Q(id).title}", "gold");
            _g.Ui.RefreshQuestTracker();
            CheckReady(id);
        }

        // Advance an objective of a given type if its predicate matches.
        void Advance(Func<ObjectiveDef, bool> match)
        {
            bool changed = false;
            foreach (var id in _g.Db.questOrder)
            {
                if (state[id] != "active") continue;
                var q = Q(id);
                for (int i = 0; i < q.objectives.Count; i++)
                {
                    if (state[id] != "active") break;
                    var obj = q.objectives[i];
                    if (progress[id][i] >= Need(obj)) continue;
                    if (match(obj)) { progress[id][i] = Mathf.Min(Need(obj), progress[id][i] + 1); changed = true; }
                }
                CheckReady(id);
            }
            if (changed)
            {
                _g.Audio.Sfx("pickup");
                _g.Ui.RefreshQuestTracker();
            }
        }

        // A quest is "ready" when all non-talk objectives are done. Talk objectives
        // that target the giver are completed by turning the quest in.
        void CheckReady(string id)
        {
            var q = Q(id);
            bool done = true;
            for (int i = 0; i < q.objectives.Count; i++)
                if (q.objectives[i].type != "talk" && progress[id][i] < Need(q.objectives[i])) done = false;
            if (done && state[id] == "active")
            {
                state[id] = "ready";
                _g.Audio.Sfx("quest");
                _g.Ui.Toast($"Objective complete — return to {_g.Db.Npc(q.giver).name.Split(',')[0]}.", "gold");
                _g.Ui.RefreshQuestTracker();
            }
        }

        public bool TurnIn(string id)
        {
            if (state[id] != "ready") return false;
            var q = Q(id);
            state[id] = "completed";
            var p = _g.Player;
            var r = q.rewards;
            if (r.xp > 0) p.AddXP(r.xp);
            if (r.gold > 0) p.AddGold(r.gold);
            if (!string.IsNullOrEmpty(r.spell)) p.LearnSpell(r.spell);
            foreach (var it in r.items)
            {
                // The web data rewards an item ("TopChuk") that isn't defined; skip unknown items.
                if (_g.Db.Item(it) == null) { Debug.LogWarning($"Quest '{id}' rewards unknown item '{it}'; skipped."); continue; }
                p.AddItem(it);
            }
            _g.Audio.Sfx("levelup");
            _g.Ui.BigBanner("QUEST COMPLETE");
            _g.Ui.Toast($"Completed: {q.title}", "gold");

            // Chain to the next quest if defined.
            if (!string.IsNullOrEmpty(q.next) && state.TryGetValue(q.next, out var ns) && ns == "available") tracked = q.next;
            else AutoTrack();

            _g.Ui.RefreshQuestTracker();
            return true;
        }

        void AutoTrack()
        {
            foreach (var id in _g.Db.questOrder)
                if (state[id] == "active" || state[id] == "ready") { tracked = id; return; }
            foreach (var id in _g.Db.questOrder)
                if (state[id] == "available") { tracked = id; return; }
            tracked = null;
        }

        // --- Event hooks ----------------------------------------------------------
        public void OnKill(string creatureId) => Advance(o => o.type == "kill" && o.creature == creatureId);
        public void OnCast(string spellId) => Advance(o => o.type == "cast" && o.spell == spellId);
        public void OnCollect(string itemId) => Advance(o => o.type == "collect" && o.item == itemId);
        public void OnReach(string locId) => Advance(o => o.type == "reach" && o.loc == locId);

        // Talking to an NPC can both accept their quest and turn in a finished one.
        public void OnTalk(string npcId)
        {
            Advance(o => o.type == "talk" && o.npc == npcId);
            foreach (var id in _g.Db.questOrder)
            {
                if (state[id] == "ready" && Q(id).giver == npcId)
                {
                    var objs = Q(id).objectives;
                    for (int i = 0; i < objs.Count; i++)
                        if (objs[i].type == "talk" && objs[i].npc == npcId) progress[id][i] = 1;
                    TurnIn(id);
                }
            }
        }

        // --- Queries used by NPCs/UI ----------------------------------------------
        public List<string> AvailableFrom(string npcId) => _g.Db.questOrder.FindAll(id => Q(id).giver == npcId && state[id] == "available");
        public List<string> ReadyFor(string npcId) => _g.Db.questOrder.FindAll(id => Q(id).giver == npcId && state[id] == "ready");
        public List<string> ActiveFrom(string npcId) => _g.Db.questOrder.FindAll(id => Q(id).giver == npcId && state[id] == "active");

        public string MarkerFor(string npcId)
        {
            if (ReadyFor(npcId).Count > 0) return "✔";
            if (AvailableFrom(npcId).Count > 0) return "!";
            return ActiveFrom(npcId).Count > 0 ? "?" : "";
        }

        public List<(string text, bool done)> ObjectiveText(string id)
        {
            var q = Q(id);
            var list = new List<(string, bool)>();
            for (int i = 0; i < q.objectives.Count; i++)
            {
                var obj = q.objectives[i];
                int have = progress[id][i], need = Need(obj);
                bool done = obj.type == "talk" ? state[id] == "completed" : have >= need;
                string counter = obj.count > 1 ? $" ({have}/{need})" : "";
                list.Add((obj.text + counter, done));
            }
            return list;
        }
    }
}
