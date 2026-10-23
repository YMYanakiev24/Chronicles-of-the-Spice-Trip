/*
 * ChronicleGame.cs
 * ----------------------------------------------------------------------------
 * Port of the RPG half of Game.js: the orchestrator for a Chronicle. Owns the
 * tilemap, player, enemies, NPCs, magic, combat and quests; handles new game /
 * continue / save, region loading, spawning and drip respawn, dialogue,
 * death/respawn, region unlocks, item use, interaction and the camera.
 *
 * It has no Unity rendering or input in it: ChronicleController feeds it an
 * RpgInput each frame and RpgView draws it through IWorldPainter.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Core;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    public class ChronicleGame
    {
        public const int TS = 16;
        public const int TargetEnemies = 12;

        public readonly GameDatabase Db;
        public readonly ISfx Audio;
        public IRpgUi Ui;
        public readonly TileMap Map;
        public readonly MagicSystem Magic;
        public readonly CombatSystem Combat;
        public QuestSystem Quests { get; private set; }
        public Player Player { get; private set; }
        public readonly List<Enemy> Enemies = new List<Enemy>();
        public readonly List<Npc> Npcs = new List<Npc>();
        public readonly HashSet<string> BestiarySeen = new HashSet<string>();
        public readonly HashSet<string> UnlockedRegions = new HashSet<string> { "mistwood" };
        public string CurrentRegion = "mistwood";
        public bool Playing { get; private set; }

        public float Elapsed { get; private set; }
        public float RealTimeMs;                 // performance.now() used by bobbing animations
        public float CamX, CamY;
        public float ViewW = 480, ViewH = 270;   // virtual screen in world pixels (set by the view)

        readonly System.Random _rng;
        float _enemyTimer;
        readonly List<(float at, Action fn)> _timers = new List<(float, Action)>();

        // dialogue state
        public Npc DialogueNpc { get; private set; }
        List<string> _dlgScript;
        int _dlgStep;

        public ChronicleGame(GameDatabase db, ISfx audio, IRpgUi ui, int? seed = null)
        {
            Db = db;
            Audio = audio;
            Ui = ui;
            _rng = seed.HasValue ? new System.Random(seed.Value) : new System.Random();
            Map = new TileMap(db, _rng);
            Magic = new MagicSystem(this);
            Combat = new CombatSystem(this);
            Quests = new QuestSystem(this);
        }

        /// <summary>randRange(min, max) — Math.random() in the JS.</summary>
        public float Rand(float min, float max) => min + (float)_rng.NextDouble() * (max - min);

        /// <summary>setTimeout replacement, run on game time.</summary>
        public void After(float seconds, Action fn) => _timers.Add((Elapsed + seconds, fn));

        public RegionDef Region => Db.Region(CurrentRegion);

        // =========================================================================
        // New game / continue / save
        // =========================================================================
        public void StartNewGame()
        {
            Player = new Player(this);
            Quests = new QuestSystem(this);
            BestiarySeen.Clear();
            CurrentRegion = "mistwood";
            UnlockedRegions.Clear();
            UnlockedRegions.Add("mistwood");
            EnterRegion("mistwood", true);
            Quests.Accept("awakening");
            BeginPlay();
            Ui.BigBanner("The Mistwood");
            After(1.4f, () => Ui.ShowHint("Move with <b>WASD</b>, aim with the <b>mouse</b>. Press <b>1</b> to loose an Emberhex at the mistlings. <b>Space</b> dodges.", 10));
        }

        public void Continue(ChronicleSave data)
        {
            Player = new Player(this);
            Quests = new QuestSystem(this);
            data.ApplyTo(this);
            EnterRegion(CurrentRegion, false);
            if (data.player.hasPosition) { Player.x = data.player.x; Player.y = data.player.y; }
            BeginPlay();
            Ui.BigBanner("Welcome back, Unmarked");
            Ui.RefreshHUD();
        }

        void BeginPlay()
        {
            Playing = true;
            ComputeCamera();
            Ui.RefreshHUD();
            Audio.PlayMusic(Region.music);
            Ui.SetRegionName(Region.name);
        }

        public void Stop()
        {
            Enemies.Clear();
            Npcs.Clear();
            Magic.Clear();
            _timers.Clear();
            Player = null;
            Playing = false;
            DialogueNpc = null;
        }

        // =========================================================================
        // Regions & spawning
        // =========================================================================
        public void TravelTo(string regionId)
        {
            if (!UnlockedRegions.Contains(regionId)) return;
            Audio.Sfx("quest");
            Ui.BigBanner(Db.Region(regionId).name);
            EnterRegion(regionId, true);
            Audio.PlayMusic(Db.Region(regionId).music);
            Ui.SetRegionName(Db.Region(regionId).name);
            ComputeCamera();
        }

        void EnterRegion(string regionId, bool reposition)
        {
            CurrentRegion = regionId;
            var region = Db.Region(regionId);
            Map.Load(regionId);
            Enemies.Clear();
            Npcs.Clear();
            Magic.Clear();
            if (reposition && Player != null)
            {
                Player.x = region.spawn.tx * TS + 8; Player.y = region.spawn.ty * TS + 8;
                Player.vx = Player.vy = 0;
            }
            foreach (var n in Db.npcs) if (n.region == regionId) Npcs.Add(new Npc(this, n));
            SpawnPopulation(region);
        }

        void SpawnPopulation(RegionDef region)
        {
            for (int i = 0; i < TargetEnemies; i++) SpawnEnemyAway(region, 140);
            if (region.id == "mistwood")
            {
                // The Cinder-Maw boss lurks in a far corner of the wood.
                float bx = (Map.W - 8) * TS, by = (Map.H - 8) * TS;
                Enemies.Add(new Enemy(this, Db.Creature("cinderMaw"), bx, by));
            }
        }

        void SpawnEnemyAway(RegionDef region, float minDist)
        {
            var type = region.enemies[_rng.Next(region.enemies.Count)];
            if (type == "cinderMaw") return;
            var tile = RandomWalkable(minDist);
            if (!tile.HasValue) return;
            Enemies.Add(new Enemy(this, Db.Creature(type), tile.Value.x, tile.Value.y));
        }

        Vector2? RandomWalkable(float minDist)
        {
            for (int i = 0; i < 60; i++)
            {
                int tx = 1 + (int)(_rng.NextDouble() * (Map.W - 2));
                int ty = 1 + (int)(_rng.NextDouble() * (Map.H - 2));
                if (Map.CellAt(tx, ty).solid) continue;
                float x = tx * TS + 8, y = ty * TS + 8;
                if (Player != null && Vector2.Distance(new Vector2(x, y), new Vector2(Player.x, Player.y)) < minDist) continue;
                return new Vector2(x, y);
            }
            return null;
        }

        // =========================================================================
        // Dialogue (data-driven; identical flow to the NPC quest scripts)
        // =========================================================================
        public void StartDialogue(Npc npc)
        {
            DialogueNpc = npc;
            _dlgStep = 0;
            _dlgScript = new List<string> { npc.def.greeting };
            _dlgScript.AddRange(npc.def.lines);
            AdvanceDialogue();
        }

        void AdvanceDialogue()
        {
            var npc = DialogueNpc;
            if (_dlgStep >= _dlgScript.Count) { PresentQuestChoices(npc, ""); return; }
            var text = _dlgScript[_dlgStep];
            _dlgStep++;
            if (_dlgStep >= _dlgScript.Count) PresentQuestChoices(npc, text);
            else Ui.ShowDialogue(npc, text, new[] { new DialogueChoice { label = "Continue ▸", primary = true, action = AdvanceDialogue } });
        }

        void PresentQuestChoices(Npc npc, string lead)
        {
            string prefix = lead.Length > 0 ? lead + "<br><br>" : "";
            var ready = Quests.ReadyFor(npc.def.id);
            var available = Quests.AvailableFrom(npc.def.id);
            var active = Quests.ActiveFrom(npc.def.id);
            if (ready.Count > 0)
            {
                var q = Db.Quest(ready[0]);
                Ui.ShowDialogue(npc, $"{prefix}<i>You have done what was asked.</i><br><b>{q.title}</b> — complete?",
                    new[] { new DialogueChoice { label = "Hand it over ✔", primary = true, action = () => { Quests.OnTalk(npc.def.id); EndDialogue(); } } });
            }
            else if (available.Count > 0)
            {
                var id = available[0];
                var q = Db.Quest(id);
                Ui.ShowDialogue(npc, $"{prefix}<b>{q.title}</b><br>{q.summary}", new[]
                {
                    new DialogueChoice { label = "Accept Quest", primary = true, action = () => { Quests.Accept(id); EndDialogue(); } },
                    new DialogueChoice { label = "Maybe later", action = EndDialogue },
                });
            }
            else if (active.Count > 0)
            {
                Ui.ShowDialogue(npc, $"{prefix}<i>\"You still have work to do. Return when it is done.\"</i>",
                    new[] { new DialogueChoice { label = "Farewell", primary = true, action = EndDialogue } });
            }
            else
            {
                Ui.ShowDialogue(npc, $"{prefix}<i>\"Walk well, Unmarked.\"</i>",
                    new[] { new DialogueChoice { label = "Farewell", primary = true, action = () => { Quests.OnTalk(npc.def.id); EndDialogue(); } } });
            }
        }

        public void EndDialogue()
        {
            Ui.HideDialogue();
            DialogueNpc = null;
        }

        // =========================================================================
        // Death / items / unlocks
        // =========================================================================
        public void OnPlayerDeath() => Ui.ShowDeath();

        public void RespawnPlayer()
        {
            Ui.HideDeath();
            Player.Respawn();
            Ui.RefreshHUD();
        }

        public void UseItem(string id)
        {
            var p = Player;
            if (p.ItemCount(id) == 0) return;
            var it = Db.Item(id);
            if (it == null || it.type != "consumable") return;
            if (it.heal > 0) p.Heal(it.heal);
            if (it.mana > 0) p.RestoreMana(it.mana);
            p.RemoveItem(id);
            Audio.Sfx("heal");
            Ui.Toast($"Used {it.name}", "pickup");
        }

        void RefreshUnlocks()
        {
            var p = Player;
            if (p == null) return;
            var u = UnlockedRegions;
            var q = Quests.state;
            u.Add("mistwood");
            if (q["trialBronze"] == "completed" || p.level >= 4) u.Add("temple");
            if (p.level >= 5) u.Add("ruins");
            if (p.level >= 6) u.Add("citadel");
            if (q["cinderMaw"] == "completed" || p.level >= 7) u.Add("caves");
            if (p.level >= 9) u.Add("godreach");
        }

        // =========================================================================
        // Loop
        // =========================================================================
        /// <summary>One frame. Returns false when the world is blocked (panel, dialogue or death).</summary>
        public bool Tick(float dt, in RpgInput inp)
        {
            if (!Playing) return false;
            bool blocked = Ui.IsBlocking || Player.dead;
            if (blocked) return false;

            Elapsed += dt;
            for (int i = _timers.Count - 1; i >= 0; i--)
            {
                if (_timers[i].at > Elapsed) continue;
                var fn = _timers[i].fn;
                _timers.RemoveAt(i);
                fn();
                if (!Playing) return false;
            }

            HandleInput(inp);
            Player.Update(dt, inp);
            for (int i = Enemies.Count - 1; i >= 0; i--)
            {
                var e = Enemies[i];
                e.Update(dt);
                if (e.dead && e.deathTimer <= 0) Enemies.RemoveAt(i);
            }
            foreach (var n in Npcs) n.Update(dt);
            Magic.Update(dt);
            Map.Update(dt);
            UpdateInteraction();
            RefreshUnlocks();
            DripRespawn(dt);
            ComputeCamera();
            return true;
        }

        void HandleInput(in RpgInput inp)
        {
            if (inp.castSlot >= 0 && inp.castSlot < 6) Player.TryCastSlot(inp.castSlot);
            if (inp.meleePressed) Player.Melee();            // left click = blade
            if (inp.castFirstPressed) Player.TryCastSlot(0);  // right click = slot 1
            if (inp.interactPressed) TryInteract();
        }

        Npc NearestNpc()
        {
            Npc near = null;
            float best = 30;
            foreach (var n in Npcs)
            {
                float d = n.DistanceTo(Player.x, Player.y);
                if (d < best) { best = d; near = n; }
            }
            return near;
        }

        void TryInteract() => NearestNpc()?.Interact();

        void UpdateInteraction()
        {
            var near = NearestNpc();
            if (near != null) Ui.ShowInteract($"<b>E</b> — Speak with {near.def.name.Split(',')[0]}");
            else Ui.HideInteract();

            for (int i = Map.Pickups.Count - 1; i >= 0; i--)
            {
                var pk = Map.Pickups[i];
                if (Vector2.Distance(new Vector2(pk.x, pk.y), new Vector2(Player.x, Player.y)) < 14) Collect(pk);
            }
        }

        void Collect(TileMap.Pickup pk)
        {
            Map.RemovePickup(pk);
            Player.AddItem(pk.item);
            Quests.OnCollect(pk.item);
            Audio.Sfx("pickup");
            if (pk.type == "asphodel") Player.RestoreMana(8);
            Ui.Toast($"Gathered {Db.Item(pk.item).name}", "pickup");
        }

        void DripRespawn(float dt)
        {
            _enemyTimer -= dt;
            int alive = Enemies.FindAll(e => !e.dead).Count;
            if (_enemyTimer <= 0 && alive < TargetEnemies)
            {
                _enemyTimer = Rand(4, 8);
                SpawnEnemyAway(Region, 200);
            }
        }

        public void ComputeCamera()
        {
            if (Player == null) return;
            float cx = Player.x - ViewW / 2, cy = Player.y - ViewH / 2;
            cx = Mathf.Clamp(cx, 0, Mathf.Max(0, Map.PixelW - ViewW));
            cy = Mathf.Clamp(cy, 0, Mathf.Max(0, Map.PixelH - ViewH));
            CamX = Mathf.Round(cx); CamY = Mathf.Round(cy);
        }

        // =========================================================================
        // Render (paint order identical to Game._renderWorld)
        // =========================================================================
        readonly List<(float baseline, int kind, object o)> _drawList = new List<(float, int, object)>();

        public void RenderWorld(IWorldPainter g, List<DynLight> lights)
        {
            Map.DrawPickups(g);

            // Y-sorted props + entities for correct overlap (stable, like Array.sort).
            _drawList.Clear();
            foreach (var p in Map.Props) _drawList.Add((p.baseline, 0, p));
            foreach (var e in Enemies) _drawList.Add((e.Baseline, 1, e));
            foreach (var n in Npcs) _drawList.Add((n.Baseline, 2, n));
            _drawList.Add((Player.Baseline, 3, Player));
            StableSort(_drawList);
            foreach (var (_, kind, o) in _drawList)
            {
                switch (kind)
                {
                    case 0: Map.DrawProp(g, (TileMap.Prop)o); break;
                    case 1: ((Enemy)o).Draw(g); break;
                    case 2: ((Npc)o).Draw(g); break;
                    default: ((Player)o).Draw(g); break;
                }
            }

            Magic.Draw(g);

            // Lighting: spell glows + the player's torch.
            lights.Clear();
            Magic.GetLights(lights);
            lights.Add(new DynLight { x = Player.x, y = Player.y - 6, r = Player.TorchRadius, color = Region.lightTint, intensity = 0.16f });
            if (Player.castFlash > 0) lights.Add(new DynLight { x = Player.x, y = Player.y - 8, r = 30, color = Player.castColor, intensity = 0.4f });
        }

        static void StableSort(List<(float baseline, int kind, object o)> list)
        {
            // insertion sort keeps equal baselines in insertion order, as Array.prototype.sort does
            for (int i = 1; i < list.Count; i++)
            {
                var item = list[i];
                int j = i - 1;
                while (j >= 0 && list[j].baseline > item.baseline) { list[j + 1] = list[j]; j--; }
                list[j + 1] = item;
            }
        }
    }
}
