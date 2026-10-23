// Unit tests for the ported rules: boss battle, Enchanted Loom, saves, hotspot
// geometry, the synth and the shop/navigation logic of the hub.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using NUnit.Framework;
using SpiceTrip.Battle;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.UIElements;

namespace SpiceTrip.Tests
{
    public class BattleSimTests
    {
        static BossDef Boss(int hp = 120) => new BossDef { id = "b", name = "Test", hp = hp, color = "#5fd06a" };
        static BattleSim Sim(int hp = 120, float rand = 0.5f) => new BattleSim(new BattleTuning(), Boss(hp), () => rand);

        [Test]
        public void StartsWhereTheJsDid()
        {
            var s = Sim();
            Assert.AreEqual(400, s.Player.x); Assert.AreEqual(380, s.Player.y);
            Assert.AreEqual(14, s.Player.r); Assert.AreEqual(100, s.Player.hp);
            Assert.AreEqual(400, s.Boss.x); Assert.AreEqual(90, s.Boss.y);
            Assert.AreEqual(32, s.Boss.r); Assert.AreEqual(120, s.Boss.hp);
        }

        [Test]
        public void PlayerMovesAt220AndIsClampedToTheEdge()
        {
            var s = Sim();
            s.Step(0.05f, new BattleSim.Inputs { move = new Vector2(1, 0) });
            Assert.AreEqual(400 + 220 * 0.05f, s.Player.x, 1e-3);
            for (int i = 0; i < 200; i++) s.Step(0.05f, new BattleSim.Inputs { move = new Vector2(1, 0) });
            Assert.AreEqual(784, s.Player.x, 1e-3);
            // diagonal movement is normalised
            var d = Sim();
            d.Step(0.05f, new BattleSim.Inputs { move = new Vector2(-1, -1) });
            Assert.AreEqual(220 * 0.05f, Vector2.Distance(new Vector2(400, 380), new Vector2(d.Player.x, d.Player.y)), 1e-3);
        }

        [Test]
        public void DtIsClampedTo50ms()
        {
            var s = Sim();
            s.Step(1f, new BattleSim.Inputs { move = new Vector2(1, 0) });
            Assert.AreEqual(400 + 220 * 0.05f, s.Player.x, 1e-3);
        }

        [Test]
        public void ShootingRespectsTheCooldownAndAimsAtTheMouse()
        {
            var s = Sim();
            var aim = new BattleSim.Inputs { shoot = true, aim = new Vector2(400, 0) };
            s.Step(0.01f, aim);
            Assert.AreEqual(1, s.PlayerShots.Count);
            Assert.AreEqual(0, s.PlayerShots[0].vx, 1e-3);
            Assert.AreEqual(-380, s.PlayerShots[0].vy, 1e-3);
            for (int i = 0; i < 4; i++) s.Step(0.05f, aim);   // 0.20 s later
            Assert.AreEqual(1, s.PlayerShots.Count, "0.22 s cooldown");
            s.Step(0.05f, aim);                               // 0.25 s later
            Assert.AreEqual(2, s.PlayerShots.Count);
        }

        [Test]
        public void BossFiresAThreeShotSpreadAfter1Point2Seconds()
        {
            var s = Sim();
            for (int i = 0; i < 23; i++) s.Step(0.05f, default);   // 1.15 s
            Assert.AreEqual(0, s.BossShots.Count);
            s.Step(0.05f, default);
            s.Step(0.05f, default);   // float rounding may put the 1.2 s mark in either frame
            Assert.AreEqual(3, s.BossShots.Count);
            float a0 = Mathf.Atan2(s.BossShots[0].vy, s.BossShots[0].vx);
            float a2 = Mathf.Atan2(s.BossShots[2].vy, s.BossShots[2].vx);
            Assert.AreEqual(0.44f, Mathf.Abs(a2 - a0), 1e-3, "±0.22 rad spread");
            Assert.AreEqual(240, new Vector2(s.BossShots[1].vx, s.BossShots[1].vy).magnitude, 1e-2);
        }

        [Test]
        public void BossStaysInsideItsClampBox()
        {
            var s = Sim(rand: 1f);
            for (int i = 0; i < 600; i++) s.Step(0.05f, default);
            Assert.That(s.Boss.x, Is.InRange(40f, 760f));
            Assert.That(s.Boss.y, Is.InRange(40f, 247.5f));
        }

        [Test]
        public void PlayerShotsDamageTheBossAndWinTheFight()
        {
            var s = Sim(hp: 16);
            bool? ended = null;
            var sfx = new List<string>();
            s.OnEnd += w => ended = w;
            s.OnSfx += sfx.Add;
            s.PlayerShots.Add(new BattleSim.Shot { x = 400, y = 90, r = 5, life = 1 });
            s.Step(0.01f, default);
            Assert.AreEqual(8, s.Boss.hp);
            Assert.Greater(s.Boss.hurt, 0);
            s.PlayerShots.Add(new BattleSim.Shot { x = 400, y = 90, r = 5, life = 1 });
            s.Step(0.01f, default);
            Assert.AreEqual(true, ended);
            Assert.IsFalse(s.Active);
            Assert.Contains("levelup", sfx);
        }

        [Test]
        public void HitsGiveInvulnerabilityAndCanLoseTheFight()
        {
            var s = Sim();
            bool? ended = null;
            s.OnEnd += w => ended = w;
            s.BossShots.Add(new BattleSim.Shot { x = 400, y = 380, r = 7, life = 3 });
            s.BossShots.Add(new BattleSim.Shot { x = 400, y = 380, r = 7, life = 3 });
            s.Step(0.01f, default);
            Assert.AreEqual(92, s.Player.hp, "second shot ignored during the 0.6 s i-frames");
            Assert.AreEqual(0.6f, s.Player.hurt, 1e-3);
            s.Player.hp = 8; s.Player.hurt = 0;
            s.BossShots.Clear();
            s.BossShots.Add(new BattleSim.Shot { x = 400, y = 380, r = 7, life = 3 });
            s.Step(0.01f, default);
            Assert.AreEqual(false, ended);
        }

        [Test]
        public void TouchingTheBossHurtsFor12()
        {
            var s = Sim();
            s.Player.x = s.Boss.x; s.Player.y = s.Boss.y + 10;
            s.Step(0.001f, default);
            Assert.AreEqual(88, s.Player.hp, 0.01);
            Assert.AreEqual(0.8f, s.Player.hurt, 0.01);
        }
    }

    public class LoomModelTests
    {
        [TestCase(4, 16, 48)]
        [TestCase(8, 64, 64)]
        [TestCase(12, 144, 96)]
        [TestCase(16, 256, 128)]
        public void GridsAndPixelationMatchTheJs(int cols, int cells, int pixelSize)
        {
            var m = new LoomModel(new LoomTuning());
            m.Build(cols);
            Assert.AreEqual(cells, m.Cells.Count);
            Assert.AreEqual(512f / cols, m.CellSize, 1e-4);
            Assert.AreEqual(pixelSize, m.PixelSizeFor(cols));
        }

        [TestCase(4)]
        [TestCase(8)]
        [TestCase(12)]
        [TestCase(16)]
        public void HitTestFindsTheShardUnderThePoint(int cols)
        {
            var m = new LoomModel(new LoomTuning());
            m.Build(cols);
            float cell = 512f / cols;
            Assert.AreEqual(0, m.HitTest(1, 1).c);
            var last = m.HitTest(511, 511);
            Assert.AreEqual(cols - 1, last.c); Assert.AreEqual(cols - 1, last.r);
            var mid = m.HitTest(cell * 2.5f, cell * 1.5f);
            Assert.AreEqual(2, mid.c); Assert.AreEqual(1, mid.r);
            m.Tear(mid);
            Assert.IsNull(m.HitTest(cell * 2.5f, cell * 1.5f), "a torn shard can't be torn again");
            Assert.IsNull(m.HitTest(600, 10));
        }

        [Test]
        public void TearingLaunchesShardsUpwardAndTheyFallAndFade()
        {
            var m = new LoomModel(new LoomTuning(), () => 1f);
            m.Build(8);
            var c = m.Cells[0];
            m.Tear(c);
            Assert.AreEqual(130, c.vx, 1e-3);
            Assert.AreEqual(-240, c.vy, 1e-3);
            Assert.AreEqual(4, c.vr, 1e-3);
            var m0 = new LoomModel(new LoomTuning(), () => 0f);
            m0.Build(8);
            m0.Tear(m0.Cells[0]);
            Assert.AreEqual(-130, m0.Cells[0].vx, 1e-3);
            Assert.AreEqual(-120, m0.Cells[0].vy, 1e-3);
            m.Step(0.5f);
            Assert.AreEqual(-240 + 520 * 0.5f, c.vy, 1e-3);
            Assert.AreEqual(1 - 0.35f, c.alpha, 1e-3);
        }

        [Test]
        public void TearAllCountsEveryRemainingShard()
        {
            var m = new LoomModel(new LoomTuning());
            m.Build(4);
            m.Tear(m.Cells[3]);
            Assert.AreEqual(15, m.TearAll());
            Assert.AreEqual(16, m.TornCount);
            Assert.AreEqual(0, m.TearAll());
        }

        [Test]
        public void CoverRectCentreCrops()
        {
            Assert.AreEqual(new Rect(160, 0, 400, 400), LoomModel.CoverRect(720, 400));
            Assert.AreEqual(new Rect(0, 150, 300, 300), LoomModel.CoverRect(300, 600));
        }
    }

    public class SaveServiceTests
    {
        string _dir;

        [SetUp] public void Dir() => _dir = Path.Combine(Path.GetTempPath(), "spicetrip-tests-" + Guid.NewGuid().ToString("N"));
        [TearDown] public void Clean() { if (Directory.Exists(_dir)) Directory.Delete(_dir, true); }

        [Test]
        public void NewHubStartsWith1450GoldAndAnEmptyStash()
        {
            var hub = new SaveService(_dir).LoadHub(1450);
            Assert.AreEqual(1450, hub.gold);
            Assert.IsEmpty(hub.stash);
            Assert.IsEmpty(hub.defeated);
            Assert.AreEqual("hero", hub.selectedChar);
        }

        [Test]
        public void HubAndSettingsRoundTrip()
        {
            var s = new SaveService(_dir);
            var hub = s.LoadHub(1450);
            hub.gold = 999; hub.AddToStash("nectarVial"); hub.AddToStash("nectarVial"); hub.defeated.Add("vlado"); hub.selectedChar = "fairy";
            s.SaveHub(hub);
            s.SaveSettings(new SettingsData { musicVolume = 0.2f, muted = true });
            var s2 = new SaveService(_dir);
            var back = s2.LoadHub(1450);
            Assert.AreEqual(999, back.gold);
            Assert.AreEqual(2, back.StashCount("nectarVial"));
            CollectionAssert.AreEqual(new[] { "vlado" }, back.defeated);
            Assert.AreEqual("fairy", back.selectedChar);
            Assert.AreEqual(0.2f, s2.LoadSettings().musicVolume, 1e-6);
            Assert.IsTrue(s2.LoadSettings().muted);
        }

        [Test]
        public void CorruptFilesFallBackToDefaults()
        {
            Directory.CreateDirectory(_dir);
            File.WriteAllText(Path.Combine(_dir, SaveService.HubFile), "{not json");
            UnityEngine.TestTools.LogAssert.Expect(LogType.Warning, new Regex("Load of hub.json failed"));
            Assert.AreEqual(1450, new SaveService(_dir).LoadHub(1450).gold);
        }
    }

    public class LayoutTests
    {
        [TestCase(1920, 1080, 0, 0, 1920, 1080)]
        [TestCase(1000, 1000, 0, 218.75f, 1000, 562.5f)]
        [TestCase(2560, 1080, 320, 0, 1920, 1080)]
        public void StageLetterboxesTo16By9(float w, float h, float x, float y, float sw, float sh)
        {
            var r = Stage.Fit(w, h);
            Assert.AreEqual(x, r.x, 0.01); Assert.AreEqual(y, r.y, 0.01);
            Assert.AreEqual(sw, r.width, 0.01); Assert.AreEqual(sh, r.height, 0.01);
        }

        [Test]
        public void HotspotRectsMatchTheWebCss()
        {
            var uss = File.ReadAllText("Assets/_Project/Resources/UI/Spice.uss");
            StringAssert.Contains(".door-wizard { left: 9%; top: 19%; width: 25%; height: 60%; }", uss);
            StringAssert.Contains(".door-changing { left: 65%; top: 24%; width: 26%; height: 54%; }", uss);
            StringAssert.Contains("width: 16%; height: 28%;", uss);                       // boss hotspot
            StringAssert.Contains("right: 1%; top: 4%; width: 17%; height: 44%;", uss);  // path castle
            StringAssert.Contains("top: 8%; right: 6%;", uss);                           // menu castle
        }
    }

    public class SynthTests
    {
        [Test]
        public void EverySfxRendersAudibleFiniteSamples()
        {
            foreach (var name in SynthDsp.SfxNames)
            {
                var buf = SynthDsp.RenderSfx(name, 48000);
                Assert.Greater(buf.Length, 48000 * 0.05, name);
                float peak = 0;
                foreach (var v in buf) { Assert.IsFalse(float.IsNaN(v) || float.IsInfinity(v), name); peak = Mathf.Max(peak, Mathf.Abs(v)); }
                Assert.Greater(peak, 0.05f, name);
                Assert.Less(peak, 1.5f, name);
            }
        }

        [Test]
        public void MusicVoiceFadesInAndOut()
        {
            var v = new SynthDsp.MusicVoice(SynthDsp.MoodFor("menu"), 8000, 1);
            float early = 0, late = 0;
            for (int i = 0; i < 8000 * 6; i++)
            {
                float s = Mathf.Abs(v.Next());
                if (i < 800) early = Mathf.Max(early, s);
                if (i > 8000 * 5) late = Mathf.Max(late, s);
            }
            Assert.Greater(late, early, "4 s fade-in");
            v.FadeOut();
            for (int i = 0; i < 8000 * 4; i++) v.Next();
            Assert.IsTrue(v.Finished);
        }
    }

    public class HubControllerTests
    {
        class FakeAudio : ISfx
        {
            public readonly List<string> Played = new List<string>();
            public string Music;
            public void Sfx(string name) => Played.Add(name);
            public void PlayMusic(string mood) => Music = mood;
        }

        string _dir;
        FakeAudio _audio;
        GameContext _ctx;
        HubController _hub;
        bool _menuShown;
        BossDef _battleBoss;

        [SetUp]
        public void Build()
        {
            _dir = Path.Combine(Path.GetTempPath(), "spicetrip-hub-" + Guid.NewGuid().ToString("N"));
            _audio = new FakeAudio();
            _ctx = new GameContext(GameDatabase.Instance, new SaveService(_dir), _audio);
            _hub = new HubController(_ctx, new VisualElement(), new VisualElement(), new VisualElement(),
                () => _menuShown = true, () => _menuShown = false, (b, c) => _battleBoss = b);
        }

        [TearDown] public void Clean() { if (Directory.Exists(_dir)) Directory.Delete(_dir, true); }

        [Test]
        public void NavigatesLikeScenesJs()
        {
            _hub.EnterPath();
            Assert.AreEqual("path", _hub.Active);
            Assert.IsFalse(_menuShown);
            _hub.EnterCorridor();
            Assert.AreEqual("corridor", _hub.Active);
            _hub.Back("path");
            Assert.AreEqual("path", _hub.Active);
            _hub.FightBoss(_ctx.Db.bosses[2]);
            Assert.AreEqual("battle", _hub.Active);
            Assert.AreEqual("mordekai", _battleBoss.id);
            _hub.EnterPath(true);
            _hub.Back("menu");
            Assert.IsNull(_hub.Active);
            Assert.IsTrue(_menuShown);
        }

        [Test]
        public void BuyingSpendsGoldAndFillsTheStash()
        {
            Assert.AreEqual(1450, _hub.Wallet);
            Assert.IsTrue(_hub.Buy("cinderSigil", 300));
            Assert.AreEqual(1150, _hub.Wallet);
            Assert.AreEqual(1, _ctx.Hub.StashCount("cinderSigil"));
            StringAssert.Contains("A fine choice — one Cinder Sigil. It will await you on the road.", _hub.Shop.MarinLine);
            Assert.Contains("pickup", _audio.Played);
            var reloaded = new SaveService(_dir).LoadHub(1450);
            Assert.AreEqual(1150, reloaded.gold);
            Assert.AreEqual(1, reloaded.StashCount("cinderSigil"));
        }

        [Test]
        public void TooPoorToBuy()
        {
            _ctx.Hub.gold = 39;
            Assert.IsFalse(_hub.Buy("ambrosiaCake", 40));
            Assert.AreEqual(39, _hub.Wallet);
            Assert.AreEqual(0, _ctx.Hub.StashCount("ambrosiaCake"));
            StringAssert.Contains("Not enough coin, friend. Come back richer.", _hub.Shop.MarinLine);
        }

        [Test]
        public void DefeatsAndChampionPersist()
        {
            _hub.MarkBossDefeated("vlado");
            _hub.SelectChar(_ctx.Db.Character("pupesh"));
            Assert.AreEqual("pupesh", _hub.SelectedChar.id);
            var reloaded = new SaveService(_dir).LoadHub(1450);
            CollectionAssert.Contains(reloaded.defeated, "vlado");
            Assert.AreEqual("pupesh", reloaded.selectedChar);
            _hub.EnterPath();
            Assert.IsTrue(_hub.Path.BossButton("vlado").ClassListContains("defeated"));
            Assert.IsFalse(_hub.Path.BossButton("goodPlaz").ClassListContains("defeated"));
        }
    }
}
