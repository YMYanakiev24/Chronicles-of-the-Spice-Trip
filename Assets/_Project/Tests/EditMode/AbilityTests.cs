// Phase 3: one test per row of the approved ability table. Kits come from the
// imported abilities.json, so these also check the data file.

using System.Linq;
using NUnit.Framework;
using SpiceTrip.Battle;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Tests
{
    public class AbilityTests
    {
        static GameDatabase Db => GameDatabase.Instance;

        static BattleSim Sim(string bossId, string champion = null, float rand = 0.5f) =>
            new BattleSim(Db.battle, Db.Boss(bossId), () => rand, champion != null ? Db.ChampionKit(champion) : null, Db.BossKit(bossId));

        static BattleSim.Inputs Special(Vector2 aim = default) => new BattleSim.Inputs { special = true, aim = aim };

        static void Kill(BattleSim s, BattleSim.Unit u)
        {
            u.hp = 1;
            s.PlayerShots.Add(new BattleSim.Shot { x = u.x, y = u.y, r = 5, life = 1 });
            s.Step(0.001f, default);
        }

        static void Run(BattleSim s, float seconds, BattleSim.Inputs inp = default, bool clearBossShots = false)
        {
            for (float t = 0; t < seconds; t += 0.02f)
            {
                if (clearBossShots) s.BossShots.Clear();
                s.Step(0.02f, inp);
            }
        }

        // ---- bosses ----------------------------------------------------------------

        [Test]
        public void GoodPlazmodiumSplitsIntoThreeSmallFastCopies()
        {
            var s = Sim("goodPlaz");
            Kill(s, s.Boss);
            Assert.IsTrue(s.Active, "not won yet");
            Assert.AreEqual(3, s.Units.Count);
            foreach (var u in s.Units)
            {
                Assert.AreEqual(1, u.form);
                Assert.AreEqual(25, u.maxHp);
                Assert.AreEqual(16, u.r);
                Assert.AreEqual(140, u.speed);
                Assert.AreEqual(1, u.spread, "the Good Plazmodium's copies fire single shots");
            }
            Assert.AreEqual(1, s.BossHpRatio, 1e-4, "one bar covers all three copies");
        }

        [Test]
        public void CopiesLeaveATinyCopyFortyPercentOfTheTime()
        {
            var leaves = Sim("goodPlaz", rand: 0.39f);
            Kill(leaves, leaves.Boss);
            Kill(leaves, leaves.Units[0]);
            var tiny = leaves.Units.Find(u => u.form == 2);
            Assert.IsNotNull(tiny, "rand < 0.4 leaves a tiny copy");
            Assert.AreEqual(10, tiny.maxHp);
            Assert.IsFalse(tiny.shooter, "touch damage only");
            Assert.IsTrue(tiny.chaser);

            var none = Sim("goodPlaz", rand: 0.41f);
            Kill(none, none.Boss);
            Kill(none, none.Units[0]);
            Assert.IsFalse(none.Units.Any(u => u.form == 2));
        }

        [Test]
        public void YouWinOnlyWhenEveryBodyIsGone()
        {
            var s = Sim("goodPlaz", rand: 0.9f);
            bool? won = null;
            s.OnEnd += w => won = w;
            Kill(s, s.Boss);
            for (int i = 0; i < 2; i++) { Kill(s, s.Units[0]); Assert.IsNull(won); }
            Kill(s, s.Units[0]);
            Assert.AreEqual(true, won);
        }

        [Test]
        public void BadPlasmodiumFiresFiveThenItsCopiesFireThree()
        {
            var s = Sim("badPlaz");
            Run(s, 1.25f);
            Assert.AreEqual(5, s.BossShots.Count, "the original fires a 5-shot spread");
            Kill(s, s.Boss);
            s.BossShots.Clear();
            Run(s, 0.62f);
            Assert.AreEqual(3, s.BossShots.Count, "the first copy fires a 3-shot spread");
        }

        [Test]
        public void MordekaiFiresAFanOfSevenFeathersAndDivesEveryThirdVolley()
        {
            var s = Sim("mordekai");
            Run(s, 1.22f);
            var feathers = s.BossShots.Where(b => b.kind == BattleSim.ShotKind.Feather).ToList();
            Assert.AreEqual(7, feathers.Count);
            Assert.AreEqual(300, new Vector2(feathers[0].vx, feathers[0].vy).magnitude, 0.5f);
            Assert.AreEqual(5, feathers[0].damage);
            Assert.AreEqual(0, s.Boss.diveT, "no dive after the first volley");
            s.Player.hp = 9999;
            Run(s, 3.25f);   // volleys at 1.2, 2.8, 4.4 s
            Assert.Greater(s.Boss.diveT, 0, "the third volley starts a dive");
            float y0 = s.Boss.y;
            Run(s, 0.1f);
            Assert.Greater(s.Boss.y - y0, 30, "dives toward the player at 420 px/s");
        }

        [Test]
        public void VladoThrowsTicketSpiralsAndSendsBusNumberNine()
        {
            var s = Sim("vlado");
            Run(s, 2.02f);
            var tickets = s.BossShots.Where(b => b.kind == BattleSim.ShotKind.Ticket).ToList();
            Assert.AreEqual(12, tickets.Count);
            Assert.AreEqual(6, tickets[0].damage);
            Assert.IsFalse(s.BossShots.Any(b => b.kind == BattleSim.ShotKind.Orb), "no ordinary spread");

            Run(s, 6.0f, default, clearBossShots: true);   // whistle at 8 s
            Assert.IsNotNull(s.ActiveBus, "the whistle calls the bus");
            Assert.IsFalse(s.ActiveBus.driving, "a 1 s warning first");
            Assert.AreEqual(s.Player.y, s.ActiveBus.y, 0.5f, "the bus takes the player's lane");
            float hp = s.Player.hp;
            Run(s, 1.8f, default, clearBossShots: true);
            Assert.AreEqual(hp - 20, s.Player.hp, 0.01f, "bus №9 hits for 20");
        }

        // ---- champions ----------------------------------------------------------------

        [Test]
        public void SpellwovenKeepsTheNormalShot()
        {
            var s = Sim("vlado", "hero");
            Assert.IsFalse(s.TrySpecial(Special()));
            s.Step(0.01f, new BattleSim.Inputs { shoot = true, aim = new Vector2(400, 0) });
            Assert.AreEqual(1, s.PlayerShots.Count);
            Assert.AreEqual(BattleSim.ShotKind.Orb, s.PlayerShots[0].kind);
        }

        [Test]
        public void AtanasHealsAndSlowsBossShots()
        {
            var s = Sim("vlado", "atanas");
            s.Player.hp = 50;
            s.Step(0.001f, Special());
            Assert.AreEqual(75, s.Player.hp, 0.01f);
            Assert.Greater(s.AuraT, 2.9f);
            s.BossShots.Add(new BattleSim.Shot { x = 100, y = 100, vx = 100, r = 2, life = 3 });
            s.Step(0.1f / 2, default);
            Assert.AreEqual(100 + 100 * 0.05f * 0.5f, s.BossShots.Last().x, 0.01f, "half speed");
            Assert.IsFalse(s.TrySpecial(Special()), "10 s cooldown");
        }

        [Test]
        public void GakevDropsHealingCoinsAndDashesUntouchable()
        {
            var s = Sim("vlado", "gakev", rand: 0.1f);
            s.PlayerShots.Add(new BattleSim.Shot { x = s.Boss.x, y = s.Boss.y, r = 5, life = 1 });
            s.Step(0.001f, default);
            Assert.AreEqual(1, s.Coins.Count, "60% of hits drop a coin");
            s.Player.hp = 50;
            s.Coins[0].x = 100; s.Coins[0].y = 400;   // away from the boss, so no touch damage
            s.Player.x = 100; s.Player.y = 400;
            s.Step(0.001f, default);
            Assert.AreEqual(55, s.Player.hp, 0.01f);
            s.Step(0.001f, Special());
            Assert.IsTrue(s.PlayerInvulnerable);
        }

        [Test]
        public void MomicheScaresTheBossAwayAndItHoldsItsFireForThreeSeconds()
        {
            var s = Sim("mordekai", "momiche");
            var b = s.Boss;
            s.Player.x = b.x; s.Player.y = b.y + 150;
            float Dist() => Vector2.Distance(new Vector2(b.x, b.y), new Vector2(s.Player.x, s.Player.y));
            float before = Dist();
            s.Step(0.001f, Special());
            Assert.AreEqual(3, s.ScareT, 0.01f);
            s.BossShots.Clear();
            Run(s, 1);
            Assert.Greater(Dist(), before + 20, "it runs from her");
            Assert.IsEmpty(s.BossShots, "and doesn't shoot");
            s.Player.x = b.x; s.Player.y = b.y;
            s.Step(0.02f, default);
            Assert.AreEqual(s.Player.maxHp, s.Player.hp, "a scared boss doesn't bite");
            Assert.IsFalse(s.TrySpecial(Special()), "8 s cooldown");
            s.Player.y = b.y + 200;
            Run(s, 2.1f);
            Assert.LessOrEqual(s.ScareT, 0);
            Run(s, 2);
            Assert.IsNotEmpty(s.BossShots, "the feathers come back once the fright wears off");
        }

        [Test]
        public void NikolayIsTougherSlowerAndBlocksShotsFromTheFront()
        {
            var s = Sim("vlado", "nikolay");
            Assert.AreEqual(125, s.Player.maxHp);
            Assert.AreEqual(190, s.PlayerSpeed);
            var up = new Vector2(s.Player.x, 0);
            s.Step(0.001f, Special(up));
            s.BossShots.Add(new BattleSim.Shot { x = s.Player.x, y = s.Player.y - 20, vy = 50, r = 3, life = 3 });
            s.Step(0.01f, new BattleSim.Inputs { aim = up });
            Assert.AreEqual(0, s.BossShots.Count, "blocked from the front");
            Assert.AreEqual(125, s.Player.hp);
            s.BossShots.Add(new BattleSim.Shot { x = s.Player.x, y = s.Player.y + 10, vy = -50, r = 3, life = 3 });
            s.Step(0.01f, new BattleSim.Inputs { aim = up });
            Assert.Less(s.Player.hp, 125, "not from behind");
        }

        [Test]
        public void PupeshShootsSeedsAndRollsThroughTheBoss()
        {
            var s = Sim("vlado", "pupesh");
            s.Step(0.01f, new BattleSim.Inputs { shoot = true, aim = new Vector2(400, 0) });
            Assert.AreEqual(3, s.PlayerShots.Count);
            Assert.IsTrue(s.PlayerShots.All(p => p.kind == BattleSim.ShotKind.Seed && p.damage == 4));
            s.PlayerShots.Clear();
            s.Step(0.001f, Special(new Vector2(s.Boss.x, s.Boss.y)));
            Assert.IsTrue(s.PlayerInvulnerable);
            s.Player.x = s.Boss.x; s.Player.y = s.Boss.y + 10;
            float hp = s.Boss.hp, php = s.Player.hp;
            s.Step(0.001f, default);
            s.Step(0.001f, default);
            Assert.AreEqual(hp - 20, s.Boss.hp, 0.01f, "20 damage, once per roll");
            Assert.AreEqual(php, s.Player.hp, "no touch damage while rolling");
        }

        [Test]
        public void RistyoDrinksForHealthButWobblesAndFiresFasterOnNicotine()
        {
            var s = Sim("vlado", "ristyo", rand: 1f);
            s.Player.hp = 50;
            s.Step(0.001f, Special());
            Assert.AreEqual(70, s.Player.hp, 0.01f);
            Assert.Greater(s.WobbleT, 3.9f);
            Assert.AreEqual(40, s.Drunk, 0.1f);
            s.Step(0.001f, new BattleSim.Inputs { shoot = true, aim = new Vector2(s.Player.x, 0) });
            var shot = s.PlayerShots[0];
            Assert.Greater(Mathf.Abs(shot.vx), 50, "aim wobbles");
            s.PlayerShots.Clear();
            int fired = s.ShotsFired;
            var hold = new BattleSim.Inputs { shoot = true, aim = new Vector2(400, 0) };
            for (int i = 0; i < 18; i++) s.Step(0.02f, hold);   // 0.36 s
            Assert.AreEqual(fired + 2, s.ShotsFired, "0.22 s / 1.3 ≈ 0.17 s between shots");
        }

        [Test]
        public void IliqnLeavesHoneyThatSlowsTheBossAndJumpsClear()
        {
            var s = Sim("vlado", "iliqnpuh");
            Run(s, 0.4f, new BattleSim.Inputs { move = new Vector2(1, 0) });
            Assert.GreaterOrEqual(s.Puddles.Count, 1);
            s.Puddles.Clear();
            s.Puddles.Add(new BattleSim.Puddle { x = s.Boss.x, y = s.Boss.y, r = 60, life = 6 });
            s.Boss.tx = s.Boss.x + 300; s.Boss.ty = s.Boss.y; s.Boss.moveCd = 5;
            float x0 = s.Boss.x;
            s.Step(0.02f, default);
            Assert.AreEqual(95 * 0.02f * 0.5f, s.Boss.x - x0, 0.05f, "half speed in honey");
            s.Step(0.001f, Special());
            Assert.IsTrue(s.PlayerInvulnerable, "airborne");
        }

        [Test]
        public void DaniilAndVasilFireTwinStreamsAndUnleashACoinRing()
        {
            var s = Sim("vlado", "daniilvasil");
            Assert.IsFalse(s.TrySpecial(Special()), "needs a full Coin Power meter");
            s.Step(0.01f, new BattleSim.Inputs { shoot = true, aim = new Vector2(400, 0) });
            Assert.AreEqual(2, s.PlayerShots.Count);
            Assert.AreEqual(5, s.PlayerShots[0].damage);
            s.PlayerShots.Clear();
            for (int i = 0; i < 20; i++)
            {
                s.Boss.hp = 1000;
                s.PlayerShots.Add(new BattleSim.Shot { x = s.Boss.x, y = s.Boss.y, r = 5, life = 1 });
                s.Step(0.001f, default);
            }
            Assert.AreEqual(100, s.CoinPower, 0.01f, "5 per hit");
            s.Step(0.001f, Special());
            Assert.AreEqual(16, s.PlayerShots.Count(p => p.kind == BattleSim.ShotKind.Coin));
            Assert.AreEqual(0, s.CoinPower);
        }

        [Test]
        public void TheStorytellerHealsStandingStillAndReleasesHomingKoi()
        {
            var s = Sim("vlado", "storyteller");
            s.Player.hp = 50;
            Run(s, 1f, default, clearBossShots: true);
            Assert.AreEqual(52, s.Player.hp, 0.1f, "2 HP/s while still");
            s.Step(0.001f, Special(new Vector2(s.Player.x, 0)));
            var koi = s.PlayerShots.Where(p => p.kind == BattleSim.ShotKind.Koi).ToList();
            Assert.AreEqual(3, koi.Count);
            float hp = s.Boss.hp;
            Run(s, 3.5f, default, clearBossShots: true);
            Assert.Less(s.Boss.hp, hp, "koi home in on the boss");
        }

        [Test]
        public void MitkoShootsHomingSparklesAndBlinksForDust()
        {
            var s = Sim("vlado", "fairy");
            s.Step(0.01f, new BattleSim.Inputs { shoot = true, aim = new Vector2(0, s.Player.y) });   // aimed away, sideways
            var sp = s.PlayerShots[0];
            Assert.AreEqual(BattleSim.ShotKind.Sparkle, sp.kind);
            Assert.AreEqual(6, sp.damage);
            float vy0 = sp.vy;
            s.Step(0.1f / 2, default);
            Assert.Less(sp.vy, vy0, "turns toward the boss above");
            float dust = s.Dust;
            var target = new Vector2(150, 200);
            s.Step(0.001f, Special(target));
            Assert.AreEqual(150, s.Player.x, 0.01f);
            Assert.AreEqual(200, s.Player.y, 0.01f);
            Assert.AreEqual(dust - 10, s.Dust, 0.1f);
            s.Dust = 5;
            Run(s, 0.35f);
            s.Dust = 5;
            Assert.IsFalse(s.TrySpecial(Special(new Vector2(600, 300))), "not enough Dust");
        }
    }
}
