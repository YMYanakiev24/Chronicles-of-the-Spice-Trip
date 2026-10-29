// The Dungeon mode (the port of the C++ game): floors, combat, the five boss
// fights, every champion special, Marin's market, saves and Boss Rush.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using NUnit.Framework;
using SpiceTrip.Core;
using SpiceTrip.Dungeon;
using UnityEngine;
using static SpiceTrip.Dungeon.Geo;

namespace SpiceTrip.Tests
{
    public class DungeonTests
    {
        class Ears : ISfx
        {
            public readonly List<string> Heard = new List<string>();
            public void Sfx(string n) => Heard.Add(n);
            public void PlayMusic(string m) { }
        }

        static DungeonGame NewRun(string champion = "hero", int seed = 7)
        {
            var g = new DungeonGame(new Ears(), seed);
            g.StartRun(champion);
            return g;
        }

        static DungeonInput Idle(DungeonGame g) => new DungeonInput { slot = -1, aim = g.Player.pos + new Vector2(40, 0) };

        static void Run(DungeonGame g, float seconds, Func<DungeonInput> input = null)
        {
            for (float t = 0; t < seconds; t += 1 / 60f) g.Tick(1 / 60f, input != null ? input() : Idle(g));
        }

        static bool GoTo(DungeonGame g, Func<Room, bool> pred, bool cleared = false)
        {
            for (int y = 0; y < FloorSize; y++)
                for (int x = 0; x < FloorSize; x++)
                {
                    var r = g.Floor[x, y];
                    if (!r.exists || !pred(r)) continue;
                    g.Floor.cx = x; g.Floor.cy = y;
                    if (cleared) r.cleared = true;
                    g.Player.pos = Center;
                    g.EnterRoom();
                    return true;
                }
            return false;
        }

        static bool Combat(Room r) => !r.isStart && !r.isShop && !r.isBoss;

        static Enemy Dummy(Vector2 at, float hp = 200) =>
            new Enemy { name = "Dummy", hp = hp, maxHp = hp, speed = 0, dmg = 0, r = 12, pos = at, color = Color.gray };

        // =====================================================================
        // Floors and rooms
        // =====================================================================
        [Test]
        public void EveryFloorHasAStartAMarketAndTheBossInTheFarthestRoom()
        {
            for (int seed = 1; seed <= 40; seed++)
            {
                var f = new Floor();
                f.Generate(new DRandom(seed));
                var rooms = f.rooms.Where(r => r.exists).ToList();
                Assert.GreaterOrEqual(rooms.Count, 6, $"seed {seed}");
                Assert.AreEqual(1, rooms.Count(r => r.isStart));
                Assert.AreEqual(1, rooms.Count(r => r.isBoss));
                Assert.AreEqual(1, rooms.Count(r => r.isShop));
                Assert.IsTrue(f.Cur.isStart && f.Cur.cleared);

                // every room is reachable through doors, doors match on both sides,
                // and no room is farther from the start than the boss
                var dist = new Dictionary<(int, int), int> { [(f.cx, f.cy)] = 0 };
                var queue = new Queue<(int x, int y)>();
                queue.Enqueue((f.cx, f.cy));
                (int dx, int dy, Dir bit, Dir back)[] steps = { (0, -1, Dir.N, Dir.S), (1, 0, Dir.E, Dir.W), (0, 1, Dir.S, Dir.N), (-1, 0, Dir.W, Dir.E) };
                while (queue.Count > 0)
                {
                    var (x, y) = queue.Dequeue();
                    foreach (var (dx, dy, bit, back) in steps)
                    {
                        if ((f[x, y].doors & bit) == 0) continue;
                        Assert.IsTrue((f[x + dx, y + dy].doors & back) != 0, "doors come in pairs");
                        if (dist.ContainsKey((x + dx, y + dy))) continue;
                        dist[(x + dx, y + dy)] = dist[(x, y)] + 1;
                        queue.Enqueue((x + dx, y + dy));
                    }
                }
                Assert.AreEqual(rooms.Count, dist.Count, "every room is reachable");
                int bossDist = dist.First(kv => f[kv.Key.Item1, kv.Key.Item2].isBoss).Value;
                Assert.AreEqual(dist.Values.Max(), bossDist, "the boss waits farthest from the start");
            }
        }

        [Test]
        public void RoomLayoutsNeverCutOffADoorOrAChest()
        {
            var rng = new DRandom(11);
            for (int i = 0; i < 200; i++)
            {
                var r = new Room { exists = true };
                r.BuildLayout(rng);
                int midX = RoomTW / 2, midY = RoomTH / 2;
                // flood from the west door lane reaches the other three door lanes and the chest
                var field = new int[RoomTW * RoomTH];
                RoomPhysics.FlowField(r, new Vector2(1 * Tile + 16, midY * Tile + 16), false, field);
                Assert.GreaterOrEqual(field[midY * RoomTW + RoomTW - 2], 0, "east door");
                Assert.GreaterOrEqual(field[1 * RoomTW + midX], 0, "north door");
                Assert.GreaterOrEqual(field[(RoomTH - 2) * RoomTW + midX], 0, "south door");
                if (r.hasChest) Assert.IsTrue(r.IsFloor(r.chestTx, r.chestTy) && field[r.chestTy * RoomTW + r.chestTx] >= 0, "chest");
            }
        }

        [Test]
        public void MovementSlidesAlongWallsAndDoorsStayShutUntilCleared()
        {
            var r = new Room { exists = true, doors = Dir.E };
            var start = new Vector2(RoomW - Tile - 20, RoomH / 2);
            var shut = RoomPhysics.Move(start, new Vector2(60, 0), PlayerR, r, false);
            Assert.LessOrEqual(shut.x, RoomW - Tile - PlayerR + 0.1f, "a shut door is a wall");
            var open = RoomPhysics.Move(start, new Vector2(60, 0), PlayerR, r, true);
            Assert.Greater(open.x, RoomW - Tile, "an open door lets you through");
            var slide = RoomPhysics.Move(new Vector2(200, Tile + PlayerR + 1), new Vector2(30, -30), PlayerR, r, false);
            Assert.AreEqual(230, slide.x, 0.01f, "slides along the top wall");
            Assert.GreaterOrEqual(slide.y, Tile + PlayerR);
        }

        [Test]
        public void MonstersPathAroundWalls()
        {
            var r = new Room { exists = true };
            for (int y = 1; y < RoomTH - 2; y++) r.Set(10, y, RoomTile.Wall);       // a wall with a gap at the bottom
            var field = new int[RoomTW * RoomTH];
            var target = new Vector2(14 * Tile + 16, 3 * Tile + 16);
            RoomPhysics.FlowField(r, target, false, field);
            var from = new Vector2(6 * Tile + 16, 3 * Tile + 16);
            Assert.IsFalse(RoomPhysics.LineClear(r, from, target, 10));
            var step = RoomPhysics.NextWaypoint(r, field, from);
            Assert.Greater(step.y, from.y, "heads down toward the gap");
        }

        // =====================================================================
        // Combat
        // =====================================================================
        [Test]
        public void SpellsHitWeaknessesHarderAndTheBladeHitsWhatsInFront()
        {
            var g = NewRun();
            Assert.IsTrue(GoTo(g, Combat));
            g.Enemies.Clear();
            var weak = Dummy(Center + new Vector2(80, 0));
            weak.weakness = "Fire";
            g.Enemies.Add(weak);
            g.Tick(1 / 60f, new DungeonInput { slot = 0, aim = weak.pos });
            g.Tick(1 / 60f, new DungeonInput { slot = -1, aim = weak.pos, castPressed = true });
            Run(g, 0.4f, () => new DungeonInput { slot = -1, aim = weak.pos });
            Assert.AreEqual(200 - (int)(22 * 1.6f), weak.hp, 0.01f, "Emberhex on a Fire weakness: x1.6");

            var front = Dummy(g.Player.pos + new Vector2(26, 0));
            g.Enemies.Add(front);
            g.Tick(1 / 60f, new DungeonInput { slot = -1, aim = front.pos, meleePressed = true });
            Assert.AreEqual(200 - g.BladeDamage, front.hp, 0.01f);
        }

        [Test]
        public void ClearingARoomOpensItsDoorsAndKillsDropGold()
        {
            var g = NewRun();
            Assert.IsTrue(GoTo(g, Combat));
            Assert.IsTrue(g.InFight && g.Enemies.Count > 0);
            int gold = g.Player.gold;
            foreach (var e in g.Enemies) e.hp = 0;
            Run(g, 2);
            Assert.IsTrue(g.Room.cleared, "the doors open");
            Assert.Greater(g.Player.gold, gold, "the coins fly to you");
            Assert.Greater(g.Kills, 0);
        }

        [Test]
        public void FallingSendsYouBackToTheEntrance()
        {
            var g = NewRun();
            Assert.IsTrue(GoTo(g, Combat));
            g.Player.hp = 1;
            g.HurtPlayer(50, 0.4f);
            Run(g, 0.05f);
            Assert.IsTrue(g.Room.isStart);
            Assert.AreEqual(g.Player.maxHp, g.Player.hp);
        }

        [Test]
        public void MarinSellsPotionsVapesAndSkillsOnce()
        {
            var g = NewRun();
            g.Player.gold = 100;
            g.Player.hp = 40;
            Assert.IsTrue(g.Buy(0));
            Assert.AreEqual(75, g.Player.hp, 0.01f);
            Assert.IsTrue(g.Buy(2));
            Assert.IsTrue(g.Player.Buzzing(Vape.Mango));
            Assert.IsTrue(g.Buy(6));
            Assert.IsTrue(g.Player.Has(Skill.IronSkin));
            Assert.IsFalse(g.Buy(6), "a skill is learned once");
            Assert.AreEqual(100 - 8 - 12 - 30, g.Player.gold);
            g.Player.gold = 0;
            Assert.IsFalse(g.Buy(1));
            Assert.AreEqual("Not enough gold", g.ShopMessage);
        }

        // =====================================================================
        // Bosses
        // =====================================================================
        [Test]
        public void EachFloorEndsWithTheNextBossAndTheCycleGetsStronger()
        {
            Assert.AreEqual(new[] { "The Good Plazmodium", "The Bad Plasmodium", "Mordekai", "Vlado", "Pyraketh, the Cinder-Maw" },
                Enumerable.Range(1, 5).Select(f => Defs.Bosses[Defs.BossIndexForFloor(f)].name).ToArray());
            Assert.AreEqual(0, Defs.BossIndexForFloor(6));
            Assert.AreEqual(1f, Defs.BossStrength(5));
            Assert.AreEqual(1.5f, Defs.BossStrength(6));
            Assert.AreEqual(2f, Defs.BossStrength(11));
        }

        static DungeonGame BossRoom(int floor, int seed = 3, string champion = "hero")
        {
            var g = NewRun(champion, seed);
            while (g.FloorNumber < floor) g.Descend();
            Assert.IsTrue(GoTo(g, r => r.isBoss));
            g.Player.pos = new Vector2(Center.x, RoomH - Tile * 2f);
            Assert.IsTrue(g.Fight.Active);
            Assert.AreSame(Defs.Bosses[floor - 1], g.Fight.Def);
            return g;
        }

        static void KeepAlive(DungeonGame g) { g.Player.hp = g.Player.maxHp; g.Player.invuln = 1; }

        [Test]
        public void ThePlazmodiumSplitsIntoThreeCopiesAndTheRoomClearsOnlyWhenAllAreGone()
        {
            var g = BossRoom(1);
            Run(g, 2, () => { KeepAlive(g); return Idle(g); });
            Assert.GreaterOrEqual(g.EnemyShots.Count(s => s.kind == ShotKind.Orb), 3, "a 3-shot spread");
            var boss = g.Enemies.Single(e => e.bossForm == 0);
            float full = g.Fight.Health(g.Enemies);
            boss.hp = 0;
            Run(g, 0.05f, () => { KeepAlive(g); return Idle(g); });
            Assert.AreEqual(3, g.Enemies.Count(e => e.bossForm == 1));
            Assert.Less(g.Fight.Health(g.Enemies), full);
            Assert.IsFalse(g.Room.cleared);
            for (int i = 0; i < 10 && !g.Room.cleared; i++)
            {
                foreach (var e in g.Enemies) e.hp = 0;     // copies, then any tiny ones
                Run(g, 0.05f, () => { KeepAlive(g); return Idle(g); });
            }
            Assert.IsTrue(g.Room.cleared);
            Assert.IsTrue(g.Profile.Beaten(0));
            Assert.IsTrue(g.StairsOpen);
        }

        [Test]
        public void TheBadPlasmodiumFiresFiveShotSpreads()
        {
            var g = BossRoom(2);
            Run(g, 1.8f, () => { KeepAlive(g); return Idle(g); });
            Assert.GreaterOrEqual(g.EnemyShots.Count(s => s.kind == ShotKind.Orb), 5);
        }

        [Test]
        public void MordekaiFiresFansOfSevenFeathersAndDivesEveryThirdVolley()
        {
            var g = BossRoom(3);
            Run(g, 1.8f, () => { KeepAlive(g); return Idle(g); });
            Assert.GreaterOrEqual(g.EnemyShots.Count(s => s.kind == ShotKind.Feather), 7);
            bool dived = false;
            for (float t = 0; t < 6 && !dived; t += 1 / 60f)
            {
                KeepAlive(g);
                g.Tick(1 / 60f, Idle(g));
                dived = g.Enemies.Any(e => e.bossForm == 0 && e.diveT > 0);
            }
            Assert.IsTrue(dived);
        }

        [Test]
        public void VladoThrowsTicketSpiralsAndBusNineHitsForTwenty()
        {
            var g = BossRoom(4);
            Run(g, 1.8f, () => { KeepAlive(g); return Idle(g); });
            Assert.GreaterOrEqual(g.EnemyShots.Count(s => s.kind == ShotKind.Ticket), BossFight.SpiralCount);
            Run(g, 2.6f, () => { KeepAlive(g); return Idle(g); });
            Assert.IsNotNull(g.Fight.Bus, "the whistle calls the bus");
            Assert.IsFalse(g.Fight.Bus.driving, "with a warning first");
            while (!g.Fight.Bus.driving) { KeepAlive(g); g.Tick(1 / 60f, Idle(g)); }
            g.EnemyShots.Clear();
            g.Player.hp = g.Player.maxHp; g.Player.invuln = 0;
            g.Player.pos = new Vector2(g.Fight.Bus.x + g.Fight.Bus.w / 2 + 40, g.Fight.Bus.y);
            float hp = g.Player.hp;
            for (int i = 0; i < 30 && !g.Fight.Bus.hit; i++) { g.EnemyShots.Clear(); g.Tick(1 / 60f, Idle(g)); }
            Assert.IsTrue(g.Fight.Bus.hit);
            Assert.AreEqual(20, hp - g.Player.hp, 0.5f);
        }

        [Test]
        public void TheCinderMawBreathesEmberRingsAndCompletesTheRun()
        {
            var g = BossRoom(5);
            Run(g, 2.2f, () => { KeepAlive(g); return Idle(g); });
            Assert.GreaterOrEqual(g.EnemyShots.Count(s => s.kind == ShotKind.Ember), BossFight.RingCount);
            foreach (var e in g.Enemies) e.hp = 0;
            Run(g, 0.1f);
            Assert.AreEqual(Overlay.RunComplete, g.Overlay);
            Assert.AreEqual(1, g.Profile.runsCompleted);
            g.Descend();
            Assert.AreEqual(6, g.FloorNumber);
            Assert.AreEqual(Overlay.None, g.Overlay);
            Assert.AreEqual(0, g.CurrentBossIndex);
            Assert.AreEqual(1.5f, g.CurrentStrength);
        }

        [Test]
        public void BossRushRunsAllFiveBackToBackAndKeepsTheBestTime()
        {
            var g = new DungeonGame(new Ears(), 5);
            g.StartBossRush("atanas");
            Assert.AreEqual(5, g.Player.level);
            for (int stage = 0; stage < Defs.Bosses.Length; stage++)
            {
                Assert.AreEqual(stage, g.RushStage);
                Assert.AreSame(Defs.Bosses[stage], g.Fight.Def);
                Run(g, 0.5f, () => { KeepAlive(g); return Idle(g); });
                for (int i = 0; i < 20 && g.Fight.Active; i++)
                {
                    foreach (var e in g.Enemies) e.hp = 0;
                    Run(g, 0.05f, () => { KeepAlive(g); return Idle(g); });
                }
                if (stage < Defs.Bosses.Length - 1) Run(g, 2.6f, () => { KeepAlive(g); return Idle(g); });
            }
            Assert.AreEqual(Overlay.RushOver, g.Overlay);
            Assert.IsTrue(g.RushWon && g.RushRecord);
            Assert.AreEqual(g.RushTime, g.Profile.bestRush, 1e-4);
        }

        // =====================================================================
        // Champions
        // =====================================================================
        [TestCase("hero")]
        [TestCase("atanas")]
        [TestCase("gakev")]
        [TestCase("nikolay")]
        [TestCase("pupesh")]
        [TestCase("ristyo")]
        [TestCase("iliqnpuh")]
        [TestCase("daniilvasil")]
        [TestCase("storyteller")]
        [TestCase("fairy")]
        [TestCase("momiche")]
        public void EveryChampionsSpecialWorks(string id)
        {
            var g = NewRun(id);
            var kit = g.Kit;
            Assert.AreEqual(100 + kit.hpBonus, g.Player.maxHp, "perk: max health");
            Assert.IsTrue(GoTo(g, Combat));
            g.Enemies.Clear();
            var dummy = Dummy(Center + new Vector2(kit.kind == Special.BearJump ? 30 : 60, 0));
            g.Enemies.Add(dummy);
            var p = g.Player;
            p.pos = Center; p.facing = Vector2.right;
            var aim = Center + new Vector2(120, 0);
            switch (kit.kind)
            {
                case Special.ArcaneSurge: p.mana = 10; break;
                case Special.Aura: p.hp = 50; break;
                case Special.Drink: p.hp = 50; break;
                case Special.CoinBarrage: p.charge = Defs.CoinPowerMax; break;
                case Special.MugShield:
                    g.EnemyShots.Add(new Shot { pos = Center - new Vector2(60, 0), vel = new Vector2(200, 0), damage = 10, r = 6, life = 3, kind = ShotKind.Orb });
                    break;
            }
            var start = p.pos;
            g.Tick(1 / 60f, new DungeonInput { slot = -1, aim = aim, specialPressed = true });
            Assert.Greater(p.specialCd, 0, "the special went off");
            switch (kit.kind)
            {
                case Special.ArcaneSurge: Assert.GreaterOrEqual(p.mana, 50); break;
                case Special.Aura: Assert.AreEqual(75, p.hp, 0.1f); Assert.Greater(p.auraT, 0); break;
                case Special.Drink: Assert.AreEqual(70, p.hp, 0.1f); Assert.Greater(p.wobbleT, 0); break;
                case Special.CoinBarrage: Assert.AreEqual(16, g.Shots.Count(s => s.kind == ShotKind.Coin)); break;
                case Special.Koi: Assert.AreEqual(3, g.Shots.Count(s => s.kind == ShotKind.Koi && s.homing > 0)); break;
                case Special.Blink: Assert.AreEqual(aim.x, p.pos.x, 0.5f); Assert.Less(p.charge, Defs.DustMax); break;
            }
            Run(g, 0.8f, () => new DungeonInput { slot = -1, aim = aim });
            switch (kit.kind)
            {
                case Special.CoinDash: Assert.Greater(Vector2.Distance(p.pos, start), 80); break;
                case Special.MugShield: Assert.AreEqual(p.maxHp, p.hp); Assert.IsEmpty(g.EnemyShots); break;
                case Special.MelonRoll: Assert.LessOrEqual(dummy.hp, dummy.maxHp - 20); break;
                case Special.BearJump: Assert.LessOrEqual(dummy.hp, dummy.maxHp - 18); break;
                case Special.Scare: Assert.Greater(p.scareT, 0); break;
            }
        }

        [Test]
        public void MomichesScareSendsMonstersRunningAndNobodyBites()
        {
            var g = NewRun("momiche");
            Assert.IsTrue(GoTo(g, Combat, cleared: true));
            var p = g.Player;
            p.pos = Center;
            Enemy Walker(Vector2 at) { var e = Dummy(at); e.speed = 60; e.dmg = 10; return e; }
            var right = Walker(Center + new Vector2(50, 0));
            var left = Walker(Center - new Vector2(50, 0));
            var close = Walker(Center + new Vector2(0, 22));
            g.Enemies.AddRange(new[] { right, left, close });
            g.Tick(1 / 60f, new DungeonInput { slot = -1, aim = Center + new Vector2(100, 0), specialPressed = true });
            Assert.AreEqual(Defs.ScareTime, p.scareT, 0.05f);
            Run(g, 1);
            Assert.Greater(Vector2.Distance(right.pos, p.pos), 75, "they run");
            Assert.Greater(Vector2.Distance(left.pos, p.pos), 75);
            Assert.AreEqual(p.maxHp, p.hp, "and don't attack");
            Run(g, 2.1f);
            Assert.AreEqual(0, p.scareT);
            float d = Vector2.Distance(right.pos, p.pos);
            Run(g, 0.5f);
            Assert.Less(Vector2.Distance(right.pos, p.pos), d, "then come back");
        }

        [Test]
        public void AScaredBossRunsAndHoldsItsFire()
        {
            var g = BossRoom(3, champion: "momiche");     // Mordekai
            var boss = g.Enemies.Single(e => e.bossForm == 0);
            boss.pos = Center;
            g.Player.pos = Center + new Vector2(0, 90);
            g.Tick(1 / 60f, new DungeonInput { slot = -1, aim = Center, specialPressed = true });
            g.EnemyShots.Clear();
            Run(g, 2.5f, () => { KeepAlive(g); return Idle(g); });
            Assert.IsEmpty(g.EnemyShots, "no feathers while scared");
            Assert.Greater(Vector2.Distance(boss.pos, g.Player.pos), 150, "it fled");
            Run(g, 2.5f, () => { KeepAlive(g); return Idle(g); });
            Assert.IsNotEmpty(g.EnemyShots, "it fires again afterwards");
        }

        [Test]
        public void GakevsKillsDropThreeExtraCoins()
        {
            var g = NewRun("gakev");
            Assert.IsTrue(GoTo(g, Combat, cleared: true));
            var target = Dummy(Center + new Vector2(24, 0), hp: 1);
            g.Enemies.Add(target);
            g.Coins.Clear();
            g.Tick(1 / 60f, new DungeonInput { slot = -1, aim = target.pos, meleePressed = true });
            Assert.IsFalse(target.alive);
            Assert.GreaterOrEqual(g.Coins.Count, 1 + 3, "a monster's one or two coins, plus three");
        }

        [Test]
        public void PerksChangeTheNumbers()
        {
            Assert.AreEqual(9, NewRun("hero").SpellCost(0), "Spellwoven: 25% off Emberhex's 12");
            Assert.AreEqual(10 + 2 + 4, NewRun("pupesh").BladeDamage);
            var ristyo = NewRun("ristyo");
            Assert.IsTrue(GoTo(ristyo, Combat));
            ristyo.Tick(1 / 60f, new DungeonInput { slot = -1, aim = Center + new Vector2(100, 0), castPressed = true });
            Assert.AreEqual(0.28f * 0.7f, ristyo.Player.castCd, 0.02f);
        }

        // =====================================================================
        // Saves
        // =====================================================================
        [Test]
        public void ARunSavesAndContinuesWithItsFloorAndHero()
        {
            var dir = Path.Combine(Path.GetTempPath(), "spicetrip-dungeon-" + Guid.NewGuid().ToString("N"));
            try
            {
                var save = new SaveService(dir);
                var g = NewRun("nikolay");
                g.Descend();
                g.Player.gold = 77;
                g.Player.skills |= Skill.Blade;
                Assert.IsTrue(GoTo(g, r => r.isShop));
                DungeonSave.WriteRun(save, g.MakeSave());
                var cur = (g.Floor.cx, g.Floor.cy);

                var back = new DungeonGame(new Ears(), 99);
                back.Continue(DungeonSave.ReadRun(save));
                Assert.AreEqual(2, back.FloorNumber);
                Assert.AreEqual(77, back.Player.gold);
                Assert.IsTrue(back.Player.Has(Skill.Blade));
                Assert.AreEqual("nikolay", back.Player.champion);
                Assert.AreEqual(cur, (back.Floor.cx, back.Floor.cy));
                Assert.IsTrue(back.Room.isShop);
                for (int i = 0; i < g.Floor.rooms.Length; i++)
                    CollectionAssert.AreEqual(g.Floor.rooms[i].tiles, back.Floor.rooms[i].tiles);

                g.Profile.bestRush = 123;
                DungeonSave.WriteProfile(save, g.Profile);
                Assert.AreEqual(123, DungeonSave.ReadProfile(save).bestRush);
                Assert.IsNull(new DungeonGame(null).MakeSaveForRush(), "Boss Rush isn't saved");
            }
            finally { if (Directory.Exists(dir)) Directory.Delete(dir, true); }
        }
    }

    static class DungeonTestExtensions
    {
        public static DungeonRunSave MakeSaveForRush(this DungeonGame g)
        {
            g.StartBossRush("hero");
            return g.MakeSave();
        }
    }
}
