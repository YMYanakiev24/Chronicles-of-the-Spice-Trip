// "More things to do": boss bounties and star ratings, Marin's battle items,
// and the Gauntlet unlock.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using NUnit.Framework;
using SpiceTrip.Battle;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.UIElements;

namespace SpiceTrip.Tests
{
    public class BountyTests
    {
        [TestCase(0.2f, 10f, 45f, 1)]
        [TestCase(0.5f, 60f, 45f, 2)]
        [TestCase(0.9f, 40f, 45f, 3)]
        [TestCase(0.3f, 10f, 45f, 1)]   // fast but hurt: par alone doesn't give stars
        public void StarRatings(float hp, float time, float par, int stars) =>
            Assert.AreEqual(stars, Progression.Stars(hp, time, par));

        [Test]
        public void FirstWinPaysTheBountyThenTheRepeatReward()
        {
            var hub = new HubData { gold = 100 };
            var r1 = Progression.RecordWin(hub, "vlado", 250, 60, 70, 0.8f, 65);
            Assert.IsTrue(r1.firstWin);
            Assert.AreEqual(250, r1.gold);
            Assert.AreEqual(3, r1.stars);
            Assert.AreEqual(350, hub.gold);
            var r2 = Progression.RecordWin(hub, "vlado", 250, 60, 70, 0.3f, 50);
            Assert.IsFalse(r2.firstWin);
            Assert.AreEqual(60, r2.gold);
            Assert.AreEqual(1, r2.stars);
            Assert.IsTrue(r2.newBestTime);
            var rec = hub.Record("vlado");
            Assert.AreEqual(2, rec.wins);
            Assert.AreEqual(3, rec.bestStars, "best rating is kept");
            Assert.AreEqual(50, rec.bestTime, 1e-4);
            Assert.AreEqual("1:05", Progression.Clock(65));
        }

        [Test]
        public void EveryBossHasABountyAndTheGauntletCoversAllFour()
        {
            var db = GameDatabase.Instance;
            foreach (var b in db.bosses) Assert.IsNotNull(db.Bounty(b.id), b.id);
            CollectionAssert.AreEquivalent(db.bosses.Select(b => b.id), db.gauntlet.order);
        }
    }

    public class BattleItemTests
    {
        static BattleSim Sim(params int[] counts)
        {
            var db = GameDatabase.Instance;
            var s = new BattleSim(db.battle, db.Boss("vlado"), () => 0.5f, null, null);
            s.LoadItems(db.battleItems, counts);
            return s;
        }

        [Test]
        public void HealingDraughtHealsButIsNotWastedAtFullHealth()
        {
            var s = Sim(2, 0, 0);
            Assert.IsFalse(s.UseItem(0), "full health");
            Assert.AreEqual(2, s.ItemCounts[0]);
            s.Player.hp = 40;
            s.Step(0.001f, new BattleSim.Inputs { itemSlot = 1 });
            Assert.AreEqual(75, s.Player.hp, 0.01f);
            Assert.AreEqual(1, s.ItemCounts[0]);
        }

        [Test]
        public void SwiftwingElixirSpeedsFeetAndShots()
        {
            var s = Sim(0, 1, 0);
            s.Step(0.001f, new BattleSim.Inputs { itemSlot = 2 });
            Assert.Greater(s.SwiftT, 5.9f);
            float x0 = s.Player.x;
            s.Step(0.05f, new BattleSim.Inputs { move = new Vector2(1, 0) });
            Assert.AreEqual(220 * 1.4f * 0.05f, s.Player.x - x0, 0.05f);
            Assert.IsFalse(s.UseItem(1), "none left");
        }

        [Test]
        public void WardingCharmTurnsAsideTheNextThreeHits()
        {
            var s = Sim(0, 0, 1);
            s.Step(0.001f, new BattleSim.Inputs { itemSlot = 3 });
            Assert.AreEqual(3, s.WardHits);
            for (int i = 0; i < 3; i++)
            {
                s.Player.hurt = 0;
                s.BossShots.Add(new BattleSim.Shot { x = s.Player.x, y = s.Player.y, r = 5, life = 1 });
                s.Step(0.001f, default);
            }
            Assert.AreEqual(100, s.Player.hp, "three hits absorbed");
            Assert.AreEqual(0, s.WardHits);
            s.Player.hurt = 0;
            s.BossShots.Add(new BattleSim.Shot { x = s.Player.x, y = s.Player.y, r = 5, life = 1 });
            s.Step(0.001f, default);
            Assert.AreEqual(92, s.Player.hp, "the fourth hit lands");
        }
    }

    public class MarinBattleWaresTests
    {
        class Quiet : ISfx { public void Sfx(string n) { } public void PlayMusic(string m) { } }

        string _dir;
        GameContext _ctx;
        HubController _hub;

        [SetUp]
        public void Build()
        {
            _dir = Path.Combine(Path.GetTempPath(), "spicetrip-wares-" + Guid.NewGuid().ToString("N"));
            _ctx = new GameContext(GameDatabase.Instance, new SaveService(_dir), new Quiet());
            _hub = new HubController(_ctx, new VisualElement(), new VisualElement(), new VisualElement(), () => { }, () => { }, (b, c) => { });
        }

        [TearDown] public void Clean() { if (Directory.Exists(_dir)) Directory.Delete(_dir, true); }

        [Test]
        public void BuyingBattleItemsFillsTheSatchelUpToTheLimit()
        {
            int gold = _ctx.Hub.gold;
            for (int i = 0; i < 3; i++) Assert.IsTrue(_hub.BuyBattleItem("healingDraught"));
            Assert.AreEqual(3, _ctx.Hub.SatchelCount("healingDraught"));
            Assert.AreEqual(gold - 180, _ctx.Hub.gold);
            Assert.IsFalse(_hub.BuyBattleItem("healingDraught"), "carry limit");
            StringAssert.Contains("only carry 3", _hub.Shop.MarinLine);
            Assert.AreEqual(3, new SaveService(_dir).LoadHub(1450).SatchelCount("healingDraught"), "saved");
        }

        [Test]
        public void TheGauntletOpensOnlyAfterAllFourBossesFall()
        {
            Assert.IsFalse(_hub.FightGauntlet());
            foreach (var b in _ctx.Db.bosses) _hub.MarkBossDefeated(b.id);
            Assert.IsTrue(_hub.FightGauntlet());
            Assert.AreEqual("battle", _hub.Active);
        }
    }
}
