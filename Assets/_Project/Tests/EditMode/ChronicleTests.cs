// Tests for the restored RPG ("Chronicle"): map generation parity with the JS,
// player movement/combat/progression, magic, quests, dialogue, unlocks and saves.
// The rules run headless with null UI and audio.

using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using NUnit.Framework;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.Rpg;
using UnityEngine;
using UnityEngine.TestTools;

namespace SpiceTrip.Tests
{
    class NullUi : IRpgUi
    {
        public readonly List<string> Toasts = new List<string>();
        public readonly List<string> Banners = new List<string>();
        public string DialogueText;
        public List<DialogueChoice> Choices = new List<DialogueChoice>();
        public bool Death, Blocking;
        public void Toast(string msg, string type = "info", float dur = 2.4f) => Toasts.Add(msg);
        public void BigBanner(string text) => Banners.Add(text);
        public void RefreshHUD() { }
        public void RefreshQuestTracker() { }
        public void DamageFlash() { }
        public void Shake(float amount) { }
        public void SpawnDamageNumber(float wx, float wy, int amount, bool crit) { }
        public void ShowInteract(string richText) { }
        public void HideInteract() { }
        public void ShowHint(string richText, float dur = 6) { }
        public void ShowDialogue(Npc npc, string richText, IReadOnlyList<DialogueChoice> choices) { DialogueText = richText; Choices = choices.ToList(); }
        public void HideDialogue() { DialogueText = null; Choices.Clear(); }
        public void ShowDeath() => Death = true;
        public void HideDeath() => Death = false;
        public void SetRegionName(string name) { }
        public bool IsBlocking => Blocking || DialogueText != null;
    }

    class NullAudio : ISfx
    {
        public readonly List<string> Played = new List<string>();
        public string Music;
        public void Sfx(string name) => Played.Add(name);
        public void PlayMusic(string mood) => Music = mood;
    }

    public class MapParityTests
    {
        static string Fnv(string s) => Hash.Str(s).ToString("x8");
        static string F(float v) => v.ToString(CultureInfo.InvariantCulture);

        [Test]
        public void EveryRegionGeneratesTheSameWorldAsTheJs()
        {
            var text = File.ReadAllText("Assets/_Project/Data/Source/map-signatures.json");
            var db = GameDatabase.Instance;
            var map = new TileMap(db, new System.Random(1));
            foreach (var id in db.regionOrder)
            {
                map.Load(id);
                var block = Regex.Match(text, "\"" + id + "\": \\{([^}]*)\\}").Groups[1].Value;
                string Field(string k) => Regex.Match(block, "\"" + k + "\": \"?([^\",\\n]+)").Groups[1].Value;
                var grid = string.Concat(map.Cells.Select(c => c.wall ? "W" : c.solid ? "S" : "."));
                var props = string.Join(";", map.Props.Select(p => $"{p.tx},{p.ty},{p.name}"));
                var pickups = string.Join(";", map.Pickups.Select(p => $"{F(p.x)},{F(p.y)},{p.type}"));
                var lights = string.Join(";", map.Lights.Select(l => $"{F(l.x)},{F(l.y)},{F(l.r)},{l.color}"));
                Assert.AreEqual(Field("w"), map.W.ToString(), id);
                Assert.AreEqual(Field("h"), map.H.ToString(), id);
                Assert.AreEqual(Field("props"), map.Props.Count.ToString(), id + " prop count");
                Assert.AreEqual(Field("pickups"), map.Pickups.Count.ToString(), id + " pickup count");
                Assert.AreEqual(Field("gridHash"), Fnv(grid), id + " walls/solids");
                Assert.AreEqual(Field("propsHash"), Fnv(props), id + " props");
                Assert.AreEqual(Field("pickupsHash"), Fnv(pickups), id + " pickups");
                Assert.AreEqual(Field("lightsHash"), Fnv(lights), id + " lights");
            }
        }

        [Test]
        public void SpawnAndNpcTilesAreClear()
        {
            var db = GameDatabase.Instance;
            var map = new TileMap(db, new System.Random(1));
            foreach (var id in db.regionOrder)
            {
                map.Load(id);
                var sp = db.Region(id).spawn;
                Assert.IsFalse(map.CellAt(sp.tx, sp.ty).solid, id);
            }
            foreach (var n in db.npcs)
            {
                map.Load(n.region);
                Assert.IsFalse(map.CellAt(n.tx, n.ty).solid, n.id);
            }
        }
    }

    public class ChronicleRulesTests
    {
        NullUi _ui;
        NullAudio _audio;
        ChronicleGame _g;

        [SetUp]
        public void NewGame()
        {
            _ui = new NullUi();
            _audio = new NullAudio();
            _g = new ChronicleGame(GameDatabase.Instance, _audio, _ui, seed: 42);
            _g.StartNewGame();
        }

        static RpgInput Move(float x, float y) { var i = RpgInput.None; i.moveX = x; i.moveY = y; return i; }

        void Step(float seconds, RpgInput inp)
        {
            for (float t = 0; t < seconds; t += 0.02f) _g.Tick(0.02f, inp);
        }

        [Test]
        public void NewGameStartsInTheMistwoodWithTheFirstQuest()
        {
            Assert.AreEqual("mistwood", _g.CurrentRegion);
            var sp = _g.Db.Region("mistwood").spawn;
            Assert.AreEqual(sp.tx * 16 + 8, _g.Player.x);
            Assert.AreEqual(sp.ty * 16 + 8, _g.Player.y);
            Assert.AreEqual("active", _g.Quests.state["awakening"]);
            Assert.AreEqual(4, _g.Npcs.Count);
            Assert.AreEqual(13, _g.Enemies.Count, "12 roamers + the Cinder-Maw");
            Assert.IsTrue(_g.Enemies.Any(e => e.def.id == "cinderMaw"));
            Assert.Contains("The Mistwood", _ui.Banners);
            Assert.AreEqual("mist", _audio.Music);
        }

        [Test]
        public void PlayerWalksRunsAndCollidesWithWalls()
        {
            _g.Enemies.Clear();
            float x0 = _g.Player.x;
            Step(0.5f, Move(1, 0));
            Assert.AreEqual(x0 + Player.Walk * 0.5f, _g.Player.x, 2f, "walk 74 px/s");
            var run = Move(1, 0); run.run = true;
            float x1 = _g.Player.x;
            Step(0.2f, run);
            Assert.Greater(_g.Player.x - x1, Player.Walk * 0.2f + 4, "run is faster");
            Step(30, Move(-1, 0));
            Assert.GreaterOrEqual(_g.Player.x, 16 + Player.Radius - 0.01f, "stopped by the border wall");
        }

        [Test]
        public void CastingEmberhexSpendsManaStartsTheCooldownAndAdvancesTheQuest()
        {
            var inp = RpgInput.None;
            inp.castSlot = 0;
            inp.aimWorld = new Vector2(_g.Player.x + 50, _g.Player.y);
            _g.Tick(0.016f, inp);
            Assert.AreEqual(1, _g.Magic.Projectiles.Count);
            Assert.Less(_g.Player.mana, 100 - 11.9f + 0.2f);
            Assert.Greater(_g.Player.Cooldown("emberhex"), 0.3f);
            Assert.AreEqual(1, _g.Quests.progress["awakening"][0], "Cast Emberhex objective");
            _g.Tick(0.016f, inp);
            Assert.AreEqual(1, _g.Magic.Projectiles.Count, "0.4 s cooldown");
        }

        [Test]
        public void EmberhexKillsAMistlingInOneHitBecauseOfItsFireWeakness()
        {
            _g.Enemies.Clear();
            var m = new Enemy(_g, _g.Db.Creature("mistling"), _g.Player.x + 40, _g.Player.y - 8);
            _g.Enemies.Add(m);
            var inp = RpgInput.None;
            inp.aimWorld = new Vector2(m.x, m.y - 8);
            _g.Tick(0.016f, inp);      // as in Game.js, a cast uses the aim from the previous frame's update
            inp.castSlot = 0;
            _g.Tick(0.016f, inp);
            inp.castSlot = -1;
            for (float t = 0; t < 0.5f; t += 0.02f) _g.Tick(0.02f, inp);
            Assert.IsTrue(m.dead, "22 × 2 (weakness) > 30 HP");
            Assert.Contains("mistling", _g.BestiarySeen.ToList());
            Assert.AreEqual(12, _g.Player.xp + 0, 0.001, "12 XP for a mistling");
            Assert.AreEqual(1, _g.Quests.progress["awakening"][1]);
        }

        [Test]
        public void MeleeHitsEnemiesInTheArcOnly()
        {
            _g.Enemies.Clear();
            var front = new Enemy(_g, _g.Db.Creature("hollowHound"), _g.Player.x + 15, _g.Player.y);
            var behind = new Enemy(_g, _g.Db.Creature("hollowHound"), _g.Player.x - 15, _g.Player.y);
            _g.Enemies.Add(front); _g.Enemies.Add(behind);
            _g.Player.aimX = _g.Player.x + 100; _g.Player.aimY = _g.Player.y;
            _g.Player.Melee();
            Assert.AreEqual(70 - (12 + 2), front.hp, 0.01, "12 + level×2");
            Assert.AreEqual(70, behind.hp);
        }

        [Test]
        public void LevellingUpRaisesStatsAndTeachesSpells()
        {
            _g.Player.AddXP(GameDatabase.XpForLevel(1));
            Assert.AreEqual(2, _g.Player.level);
            Assert.AreEqual(115, _g.Player.maxHp);
            Assert.AreEqual(112, _g.Player.maxMana);
            Assert.IsTrue(_g.Player.learnedSpells.Contains("tidelash"));
            Assert.IsTrue(_g.Player.learnedSpells.Contains("wildmend"));
            CollectionAssert.AreEqual(new[] { "emberhex", "tidelash", "wildmend", null, null, null }, _g.Player.hotbar);
            Assert.Contains("LEVEL 2", _ui.Banners);
        }

        [Test]
        public void GreyWardAbsorbsDamageBeforeHealth()
        {
            _g.Player.ApplyShield(20, 8);
            _g.Player.TakeDamage(30);
            Assert.AreEqual(0, _g.Player.shield);
            Assert.AreEqual(90, _g.Player.hp, 0.01);
            Assert.Contains("Your Grey Ward shatters!", _ui.Toasts);
            _g.Player.TakeDamage(10);
            Assert.AreEqual(90, _g.Player.hp, 0.01, "0.4 s of i-frames after a hit");
        }

        [Test]
        public void DyingShowsTheDeathScreenAndRespawnRestores()
        {
            _g.Player.TakeDamage(1000);
            Assert.IsTrue(_g.Player.dead);
            Assert.IsTrue(_ui.Death);
            Assert.IsFalse(_g.Tick(0.016f, RpgInput.None), "the world pauses while dead");
            _g.RespawnPlayer();
            Assert.IsFalse(_g.Player.dead);
            Assert.AreEqual(_g.Player.maxHp, _g.Player.hp);
            Assert.IsFalse(_ui.Death);
        }

        [Test]
        public void TheAwakeningQuestCompletesThroughTheOracle()
        {
            _g.Quests.OnCast("emberhex");
            for (int i = 0; i < 3; i++) _g.Quests.OnKill("mistling");
            Assert.AreEqual("ready", _g.Quests.state["awakening"]);
            Assert.AreEqual("✔", _g.Quests.MarkerFor("oracle"));
            var oracle = _g.Npcs.First(n => n.def.id == "oracle");
            oracle.Interact();
            // greeting, then two lines, then the hand-in choice
            while (_ui.Choices.Count == 1 && _ui.Choices[0].label == "Continue ▸") _ui.Choices[0].action();
            StringAssert.Contains("The Unmarked", _ui.DialogueText);
            _ui.Choices[0].action();   // Hand it over ✔
            Assert.AreEqual("completed", _g.Quests.state["awakening"]);
            Assert.Contains("QUEST COMPLETE", _ui.Banners);
            Assert.AreEqual(20, _g.Player.gold, "quest gold");
            Assert.AreEqual(1, _g.Player.ItemCount("nectarVial"));
            Assert.AreEqual("stolenRelic", _g.Quests.tracked, "chains to the next quest");
        }

        [Test]
        public void UnknownRewardItemsAreSkippedWithAWarning()
        {
            _g.Quests.Accept("asphodel");
            for (int i = 0; i < 6; i++) _g.Quests.OnCollect("asphodel");
            LogAssert.Expect(LogType.Warning, new Regex("unknown item 'TopChuk'"));
            _g.Quests.OnTalk("myrrha");
            Assert.AreEqual("completed", _g.Quests.state["asphodel"]);
            Assert.AreEqual(0, _g.Player.ItemCount("TopChuk"));
            Assert.IsTrue(_g.Player.learnedSpells.Contains("wildmend"));
        }

        [Test]
        public void PickupsAreCollectedByWalkingOverThem()
        {
            _g.Enemies.Clear();
            var pk = _g.Map.Pickups.First(p => p.type == "relicShard");
            _g.Player.x = pk.x; _g.Player.y = pk.y;
            _g.Tick(0.016f, RpgInput.None);
            Assert.AreEqual(1, _g.Player.ItemCount("relicShard"));
            Assert.IsFalse(_g.Map.Pickups.Contains(pk));
            Assert.Contains("Gathered Relic Shard", _ui.Toasts);
        }

        [Test]
        public void RegionsUnlockByLevelAndTravelWorks()
        {
            Assert.IsFalse(_g.UnlockedRegions.Contains("temple"));
            _g.Player.level = 9;
            _g.Tick(0.016f, RpgInput.None);
            CollectionAssert.IsSubsetOf(_g.Db.regionOrder, _g.UnlockedRegions.ToList());
            _g.TravelTo("caves");
            Assert.AreEqual("caves", _g.CurrentRegion);
            Assert.AreEqual("caves", _audio.Music);
            var sp = _g.Db.Region("caves").spawn;
            Assert.AreEqual(sp.tx * 16 + 8, _g.Player.x);
            Assert.IsFalse(_g.Enemies.Any(e => e.def.id == "cinderMaw"), "the boss lives in the Mistwood");
        }

        [Test]
        public void UsingConsumablesHealsAndSpendsThem()
        {
            _g.Player.AddItem("ambrosiaCake", 2);
            _g.Player.hp = 10;
            _g.UseItem("ambrosiaCake");
            Assert.AreEqual(60, _g.Player.hp, 0.01);
            Assert.AreEqual(1, _g.Player.ItemCount("ambrosiaCake"));
        }

        [Test]
        public void SaveRoundTripRestoresTheChronicle()
        {
            _g.Player.AddXP(500);
            _g.Player.AddItem("cinderSigil");
            _g.Player.gold = 321;
            _g.Player.x = 200; _g.Player.y = 210;
            _g.Quests.OnCast("emberhex");
            _g.BestiarySeen.Add("strix");
            var json = JsonUtility.ToJson(ChronicleSave.From(_g));

            var g2 = new ChronicleGame(GameDatabase.Instance, new NullAudio(), new NullUi(), seed: 7);
            g2.Continue(JsonUtility.FromJson<ChronicleSave>(json));
            Assert.AreEqual(_g.Player.level, g2.Player.level);
            Assert.AreEqual(_g.Player.xp, g2.Player.xp, 0.001);
            Assert.AreEqual(321, g2.Player.gold);
            Assert.AreEqual(200, g2.Player.x); Assert.AreEqual(210, g2.Player.y);
            CollectionAssert.AreEquivalent(_g.Player.learnedSpells, g2.Player.learnedSpells);
            CollectionAssert.AreEqual(_g.Player.hotbar, g2.Player.hotbar);
            Assert.IsTrue(g2.Player.relics.Contains("cinderSigil"));
            Assert.AreEqual(1, g2.Quests.progress["awakening"][0]);
            Assert.IsTrue(g2.BestiarySeen.Contains("strix"));
            CollectionAssert.AreEquivalent(_g.UnlockedRegions, g2.UnlockedRegions);
        }

        [Test]
        public void EnemiesChaseAndHitThePlayer()
        {
            _g.Enemies.Clear();
            var hound = new Enemy(_g, _g.Db.Creature("hollowHound"), _g.Player.x + 60, _g.Player.y);
            _g.Enemies.Add(hound);
            Step(3, RpgInput.None);
            Assert.AreEqual("chase", hound.state);
            Assert.Less(_g.Player.hp, 100);
        }
    }
}
