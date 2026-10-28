// Plays through the hub in Play Mode with the real GameRoot: boot, the Path,
// boss fights (won, lost, fled), the fog wipe into Marin's shop, buying, the
// Loom and the Changing Room. Saves go to a temp folder.

using System;
using System.Collections;
using System.IO;
using NUnit.Framework;
using SpiceTrip.Battle;
using SpiceTrip.Core;
using UnityEngine;
using UnityEngine.TestTools;

namespace SpiceTrip.Tests
{
    public class HubFlowTests
    {
        string _dir;
        GameRoot _g;

        [UnitySetUp]
        public IEnumerator Boot()
        {
            _dir = Path.Combine(Path.GetTempPath(), "spicetrip-play-" + Guid.NewGuid().ToString("N"));
            GameRoot.SaveDirOverride = _dir;
            _g = new GameObject("GameRoot").AddComponent<GameRoot>();
            float t = 0;
            while (!_g.Booted && t < 5) { t += Time.unscaledDeltaTime; yield return null; }
            Assert.IsTrue(_g.Booted, "boot finished");
        }

        [UnityTearDown]
        public IEnumerator Teardown()
        {
            UnityEngine.Object.Destroy(_g.gameObject);
            yield return null;
            GameRoot.SaveDirOverride = null;
            if (Directory.Exists(_dir)) Directory.Delete(_dir, true);
        }

        [UnityTest]
        public IEnumerator BootsToTheMainMenu()
        {
            Assert.AreEqual("menu", _g.State);
            Assert.AreEqual("menu", _g.Audio.CurrentMood);
            yield return null;
        }

        [UnityTest]
        public IEnumerator EachBossFightStartsWithItsHp()
        {
            _g.Hub.EnterPath();
            foreach (var boss in _g.Ctx.Db.bosses)
            {
                _g.Hub.FightBoss(boss);
                yield return null;
                Assert.IsTrue(_g.Battle.Active, boss.id);
                Assert.AreEqual(boss.hp, _g.Battle.Sim.Boss.maxHp, boss.id);
                Assert.AreEqual("caves", _g.Audio.CurrentMood);
                _g.Battle.Stop();   // Flee
                yield return null;
                Assert.AreEqual("path", _g.Hub.Active);
                Assert.AreEqual("menu", _g.Audio.CurrentMood);
            }
        }

        [UnityTest]
        public IEnumerator WinningMarksTheBossDefeatedAndLosingShowsDefeated()
        {
            var boss = _g.Ctx.Db.bosses[1];
            _g.Hub.EnterPath();
            _g.Hub.FightBoss(boss);
            yield return null;
            // The Bad Plasmodium multiplies ×3 when killed (Phase 3), so every body has to fall.
            bool split = false;
            for (int frame = 0; frame < 120 && !_g.Battle.ResultShowing; frame++)
            {
                var sim = _g.Battle.Sim;
                if (sim.Units.Count > 1) split = true;
                foreach (var u in sim.Units)
                {
                    u.hp = Mathf.Min(u.hp, 1);
                    sim.PlayerShots.Add(new BattleSim.Shot { x = u.x, y = u.y, r = 5, life = 1 });
                }
                yield return null;
            }
            Assert.IsTrue(split, "the Plasmodium split into copies");
            Assert.IsTrue(_g.Battle.ResultShowing);
            Assert.AreEqual("VICTORY", _g.Battle.ResultTitle);
            CollectionAssert.Contains(new SaveService(_dir).LoadHub(1450).defeated, boss.id);
            _g.Battle.Stop();
            yield return null;
            Assert.IsTrue(_g.Hub.Path.BossButton(boss.id).ClassListContains("defeated"));

            _g.Hub.FightBoss(boss);
            yield return null;
            _g.Battle.Sim.Player.hp = 1;
            _g.Battle.Sim.BossShots.Add(new BattleSim.Shot { x = _g.Battle.Sim.Player.x, y = _g.Battle.Sim.Player.y, r = 7, life = 3 });
            yield return null;
            yield return null;
            Assert.AreEqual("DEFEATED", _g.Battle.ResultTitle);
        }

        [UnityTest]
        public IEnumerator FogWipeLeadsToTheShopAndBuyingWorks()
        {
            _g.Hub.EnterPath();
            _g.Hub.EnterCorridor();
            _g.Hub.EnterShop();
            yield return new WaitForSecondsRealtime(0.1f);
            Assert.IsTrue(_g.Hub.FogShowing, "fog is up");
            Assert.AreEqual("corridor", _g.Hub.Active, "scene swaps at the peak (360 ms), not before");
            yield return new WaitForSecondsRealtime(0.6f);
            Assert.AreEqual("shop", _g.Hub.Active);
            Assert.IsFalse(_g.Hub.FogShowing);

            Assert.IsTrue(_g.Hub.Buy("nectarVial", 40));
            Assert.AreEqual(1410, _g.Ctx.Hub.gold);
            Assert.AreEqual(1, new SaveService(_dir).LoadHub(1450).StashCount("nectarVial"));

            _g.Hub.Back("corridor");
            yield return new WaitForSecondsRealtime(0.6f);
            Assert.AreEqual("corridor", _g.Hub.Active);
        }

        [UnityTest]
        public IEnumerator LoomWeavesAndTearsAnImage()
        {
            var loom = _g.Hub.Loom;
            loom.Open();
            Assert.IsTrue(loom.IsOpen);
            Assert.IsTrue(loom.LoadImageFile(Path.Combine(Application.streamingAssetsPath, "assets", "menu.jpg")));
            Assert.AreEqual(64, loom.Model.Cells.Count);
            Assert.AreEqual(64, loom.Tapestry.width, "8×8 → max(48, 64) px");
            loom.SetGrid(16);
            Assert.AreEqual(256, loom.Model.Cells.Count);
            loom.TearAt(new Vector2(5, 5));
            Assert.AreEqual(1, loom.Model.TornCount);
            var cell = loom.Model.Cells[0];
            float y0 = cell.y;
            yield return new WaitForSecondsRealtime(0.3f);
            Assert.AreNotEqual(y0, cell.y, "torn shard moves");
            loom.TearAll();
            Assert.AreEqual(256, loom.Model.TornCount);
            loom.Close();
            Assert.IsFalse(loom.IsOpen);
        }

        [UnityTest]
        public IEnumerator TheGauntletRunsAllFourBossesAndPaysOut()
        {
            foreach (var b in _g.Ctx.Db.bosses) _g.Hub.MarkBossDefeated(b.id);
            _g.Hub.EnterPath();
            int gold = _g.Ctx.Hub.gold;
            Assert.IsTrue(_g.Hub.FightGauntlet());
            yield return null;
            var order = _g.Ctx.Db.gauntlet.order;
            for (int stage = 0; stage < order.Count; stage++)
            {
                Assert.AreEqual(stage, _g.Battle.GauntletStage);
                Assert.AreEqual(order[stage], _g.Battle.Sim.Def.id);
                for (int frame = 0; frame < 200 && _g.Battle.Sim.Active; frame++)
                {
                    var sim = _g.Battle.Sim;
                    sim.Player.hp = sim.Player.maxHp;
                    foreach (var u in sim.Units)
                    {
                        u.hp = Mathf.Min(u.hp, 1);
                        sim.PlayerShots.Add(new BattleSim.Shot { x = u.x, y = u.y, r = 5, life = 1 });
                    }
                    yield return null;
                }
                if (stage < order.Count - 1)
                {
                    Assert.IsTrue(_g.Battle.StageTransition, "a pause before the next boss");
                    _g.Battle.SkipStageTransition();
                    yield return null;
                    yield return null;
                }
            }
            Assert.IsTrue(_g.Battle.ResultShowing);
            Assert.AreEqual("GAUNTLET CONQUERED", _g.Battle.ResultTitle);
            Assert.AreEqual(gold + _g.Ctx.Db.gauntlet.first, _g.Ctx.Hub.gold, "first-clear bounty");
            Assert.AreEqual(1, new SaveService(_dir).LoadHub(1450).Record(Progression.GauntletId).wins);
            _g.Battle.Stop();
        }

        [UnityTest]
        public IEnumerator TheDungeonRunsFromTheLobbyToABossAndBackToTheMenu()
        {
            var dc = _g.Dungeon;
            dc.AutoPause = false;
            dc.OpenLobby();
            Assert.IsTrue(dc.InLobby);
            dc.NewRun();
            yield return null;
            var game = dc.Game;
            Assert.IsTrue(dc.Playing && game.Room.isStart);
            Assert.IsTrue(dc.View.Camera.enabled, "the dungeon camera draws the room");

            // walk into the boss room and finish the boss and its copies
            for (int i = 0; i < game.Floor.rooms.Length; i++)
                if (game.Floor.rooms[i].isBoss) { game.Floor.cx = i % Dungeon.Geo.FloorSize; game.Floor.cy = i / Dungeon.Geo.FloorSize; }
            game.EnterRoom();
            Assert.IsTrue(game.Fight.Active);
            Assert.AreEqual("caves", _g.Audio.CurrentMood, "boss music");
            for (int frame = 0; frame < 300 && !game.Room.cleared; frame++)
            {
                game.Player.invuln = 1;
                foreach (var e in game.Enemies) e.hp = 0;
                yield return null;
            }
            Assert.IsTrue(game.Room.cleared && game.StairsOpen);

            dc.SaveRun();
            Assert.IsTrue(dc.HasSave);
            dc.QuitToMenu();
            yield return null;
            Assert.AreEqual("menu", _g.State);
            Assert.IsFalse(dc.View.Camera.enabled);
        }

        [UnityTest]
        public IEnumerator ChampionChoiceSurvivesAReload()
        {
            _g.Hub.EnterPath();
            _g.Hub.EnterCorridor();
            _g.Hub.OpenCharacterSelect();
            Assert.IsTrue(_g.Hub.CharSelect.IsOpen);
            _g.Hub.SelectChar(_g.Ctx.Db.Character("fairy"));
            yield return new WaitForSecondsRealtime(0.4f);
            Assert.IsFalse(_g.Hub.CharSelect.IsOpen, "closes 0.22 s after picking");

            // "Reload": a fresh GameRoot reads the same save folder.
            UnityEngine.Object.Destroy(_g.gameObject);
            yield return null;
            _g = new GameObject("GameRoot").AddComponent<GameRoot>();
            yield return null;
            Assert.AreEqual("fairy", _g.Hub.SelectedChar.id);
        }
    }
}
