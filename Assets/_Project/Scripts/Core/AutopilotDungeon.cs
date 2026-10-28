/*
 * AutopilotDungeon.cs
 * ----------------------------------------------------------------------------
 * The autopilot's tour of the Dungeon: the lobby and champion pick, a room
 * fight, the oracle, Marin's market, all five boss fights with their attacks,
 * the end of the Spice Trip, pause / save / continue, and a Boss Rush. Menus
 * are clicked and keys pressed with real input events; fights are sped up by
 * finishing the bosses once the attack being checked has been seen.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections;
using System.Linq;
using SpiceTrip.Dungeon;
using UnityEngine;
using UnityEngine.InputSystem;
using static SpiceTrip.Dungeon.Geo;

namespace SpiceTrip.Core
{
    public partial class Autopilot
    {
        IEnumerator DungeonTour()
        {
            var dc = _g.Dungeon;
            var game = dc.Game;
            dc.AutoPause = false;

            // --- the lobby ---
            yield return Click(Q("btn-dungeon"), "The Dungeon");
            yield return Wait(0.4f);
            Check("The Dungeon opens its lobby", dc.InLobby && _g.State == "dungeon");
            yield return Click(Q("dg-champ-atanas"), "Atanas");
            yield return Wait(0.3f);
            Check("clicking a champion picks them", _g.Ctx.Hub.selectedChar == "atanas");
            yield return Shot("dungeon-lobby");
            yield return Click(Q("dg-new-run"), "New Run");
            yield return Wait(1.0f);
            Check("a run starts in the first room of floor 1", dc.Playing && game.Mode == DungeonMode.Run && game.FloorNumber == 1 && game.Room.isStart);
            Check("Atanas' perk: 120 max health", game.Player.maxHp == 120);
            yield return Shot("dungeon-start");

            // --- walking and the oracle (real keys) ---
            float x0 = game.Player.pos.x;
            yield return Hold(0.5f, Key.D);
            Check("holding D walks right", game.Player.pos.x > x0 + 40);
            game.Player.pos = OraclePos + new Vector2(24, 16);
            yield return Wait(0.2f);
            yield return Press(Key.E);
            yield return Wait(0.2f);
            Check("E talks to the oracle", game.Dialogue.Length > 0);
            yield return Shot("dungeon-oracle");
            for (int i = 0; i < 4 && game.Dialogue.Length > 0; i++) { yield return Press(Key.E); yield return Wait(0.1f); }
            Check("the oracle's words end", game.Dialogue.Length == 0);

            // --- a room fight, with spells aimed at the nearest monster ---
            Check("there is a combat room", Enter(game, r => !r.isStart && !r.isShop && !r.isBoss));
            Check("its monsters wake and the doors bar", game.InFight && game.Enemies.Count > 0 && !game.Room.cleared);
            bool shot = false;
            for (float t = 0; t < 30 && !game.Room.cleared; t += Time.unscaledDeltaTime)
            {
                game.Player.invuln = 1;
                game.Player.mana = game.Player.maxMana;
                var target = game.Enemies.OrderBy(e => Vector2.Distance(e.pos, game.Player.pos)).FirstOrDefault();
                if (target != null && !RoomPhysics.LineClear(game.Room, game.Player.pos, target.pos, 6))
                    for (int k = 0; k < 8; k++)
                    {
                        var spot = target.pos + new Vector2(Mathf.Cos(k * Mathf.PI / 4), Mathf.Sin(k * Mathf.PI / 4)) * 90;
                        if (game.Room.IsFloor((int)(spot.x / Tile), (int)(spot.y / Tile)) && RoomPhysics.LineClear(game.Room, spot, target.pos, 6)) { game.Player.pos = spot; break; }
                    }
                dc.ScriptedInput = new DungeonInput { slot = Mathf.FloorToInt(t * 2) % 3, aim = target?.pos ?? Center, castPressed = target != null };
                if (t > 1 && !shot) { shot = true; dc.ScriptedInput = null; yield return Shot("dungeon-combat"); }
                yield return null;
            }
            dc.ScriptedInput = null;
            Check("spells clear the room and open its doors", game.Room.cleared);
            yield return Wait(1.2f);
            yield return Shot("dungeon-room-cleared");

            // --- Marin's market ---
            Check("Marin's market is on the floor", Enter(game, r => r.isShop));
            game.Player.gold = 200;
            game.Player.pos = MerchantPos + new Vector2(0, 40);
            yield return Wait(0.3f);
            yield return Press(Key.E);
            yield return Wait(0.3f);
            Check("E opens Marin's market", game.Overlay == Overlay.Shop);
            game.Player.hp = game.Player.maxHp - 40;
            yield return Shot("dungeon-shop");
            yield return Click(Q("dg-buy-0"), "buy a Health Potion");
            yield return Wait(0.2f);
            Check("a Health Potion costs 8 gold and heals", game.Player.gold == 192 && game.Player.hp > game.Player.maxHp - 40);
            yield return Press(Key.Escape);
            yield return Wait(0.2f);
            Check("Esc closes the market", game.Overlay == Overlay.None);

            // --- the five floor bosses ---
            for (int floor = 1; floor <= Defs.Bosses.Length; floor++)
            {
                var boss = Defs.Bosses[floor - 1];
                Check($"floor {floor}'s boss room holds {boss.name}", Enter(game, r => r.isBoss) && game.Fight.Active && game.Fight.Def == boss);
                game.Player.pos = new Vector2(Center.x, RoomH - Tile * 2f);
                if (boss.id != null) Check($"{boss.name}'s painted arena and token are loaded", dc.View.Art.Arenas[floor - 1] != null && dc.View.Art.BossTokens[floor - 1] != null);
                switch (boss.kind)
                {
                    case BossKind.Split:
                        yield return Survive(game, 4, () => game.EnemyShots.Count(s => s.kind == ShotKind.Orb) >= boss.spread);
                        Check($"{boss.name} fires a {boss.spread}-shot spread", game.EnemyShots.Count(s => s.kind == ShotKind.Orb) >= boss.spread);
                        yield return Shot($"dungeon-boss-{floor}");
                        if (floor == 1)
                        {
                            game.Player.invuln = 1; game.Player.hp = 50; game.Player.specialCd = 0;
                            yield return Press(Key.Q);
                            Check("Q: Aura of Calmness heals 25 and slows the fight", Mathf.Abs(game.Player.hp - 75) < 0.5f && game.Player.auraT > 0);
                            game.Enemies.First(e => e.bossForm == 0).hp = 0;
                            yield return Survive(game, 2, () => game.Enemies.Count(e => e.bossForm == 1) == 3);
                            Check("the Plazmodium multiplies into three copies", game.Enemies.Count(e => e.bossForm == 1) == 3);
                            yield return Shot("dungeon-split");
                        }
                        break;
                    case BossKind.Feathers:
                        yield return Survive(game, 4, () => game.EnemyShots.Count(s => s.kind == ShotKind.Feather) >= 7);
                        Check("Mordekai fires a fan of 7 feathers", game.EnemyShots.Count(s => s.kind == ShotKind.Feather) >= 7);
                        yield return Shot($"dungeon-boss-{floor}");
                        yield return Survive(game, 8, () => game.Enemies.Any(e => e.bossForm == 0 && e.diveT > 0));
                        Check("Mordekai dives every third volley", game.Enemies.Any(e => e.bossForm == 0 && e.diveT > 0));
                        break;
                    case BossKind.Conductor:
                        yield return Survive(game, 4, () => game.EnemyShots.Count(s => s.kind == ShotKind.Ticket) >= BossFight.SpiralCount);
                        Check("Vlado throws a spiral of 12 tickets", game.EnemyShots.Count(s => s.kind == ShotKind.Ticket) >= BossFight.SpiralCount);
                        yield return Survive(game, 8, () => game.Fight.Bus != null);
                        Check("the whistle calls bus No.9 with a warning lane", game.Fight.Bus != null && !game.Fight.Bus.driving);
                        yield return Shot("dungeon-bus-warning");
                        yield return Survive(game, 3, () => game.Fight.Bus != null && game.Fight.Bus.driving);
                        if (game.Fight.Bus != null)
                        {
                            game.EnemyShots.Clear();
                            game.Player.hp = game.Player.maxHp; game.Player.invuln = 0;
                            game.Player.pos = new Vector2(Mathf.Min(game.Fight.Bus.x + game.Fight.Bus.w / 2 + 50, RoomW - Tile * 2), game.Fight.Bus.y);
                            float hp = game.Player.hp;
                            yield return WaitFor(() => game.Fight.Bus == null || game.Fight.Bus.hit, 2);
                            Check("bus No.9 hits for 20", game.Fight.Bus != null && game.Fight.Bus.hit && hp - game.Player.hp >= 19.5f);
                            yield return Shot("dungeon-bus");
                        }
                        break;
                    case BossKind.Inferno:
                        yield return Survive(game, 5, () => game.EnemyShots.Count(s => s.kind == ShotKind.Ember) >= BossFight.RingCount);
                        Check("the Cinder-Maw breathes a ring of embers", game.EnemyShots.Count(s => s.kind == ShotKind.Ember) >= BossFight.RingCount);
                        yield return Shot($"dungeon-boss-{floor}");
                        break;
                }
                for (float t = 0; t < 10 && !game.Room.cleared; t += Time.unscaledDeltaTime)
                {
                    game.Player.invuln = 1;
                    foreach (var e in game.Enemies) if (e.bossForm >= 0) e.hp = 0;
                    yield return null;
                }
                Check($"{boss.name} falls and the room opens", game.Room.cleared && game.Profile.Beaten(floor - 1));
                if (floor < Defs.Bosses.Length)
                {
                    for (float t = 0; t < 6 && game.FloorNumber == floor; t += Time.unscaledDeltaTime) { game.Player.pos = Center; yield return null; }
                    Check($"the stairs lead down to floor {floor + 1}", game.FloorNumber == floor + 1 && game.Room.isStart);
                }
            }
            yield return Wait(0.6f);
            Check("beating floor 5 completes the Spice Trip", game.Overlay == Overlay.RunComplete && game.Profile.runsCompleted == 1);
            yield return Shot("dungeon-complete");
            yield return Click(Q("dg-descend"), "Descend deeper");
            yield return Wait(0.4f);
            Check("floor 6 starts the cycle again, stronger", game.FloorNumber == 6 && game.Overlay == Overlay.None && game.CurrentStrength > 1.4f);

            // --- pause, save, continue ---
            yield return Press(Key.Escape);
            yield return Wait(0.3f);
            Check("Esc pauses", game.Overlay == Overlay.Pause);
            yield return Shot("dungeon-pause");
            yield return Click(Q("dg-save"), "Save");
            yield return Wait(0.2f);
            Check("Save writes the run", dc.HasSave);
            yield return Click(Q("dg-main-menu"), "Main Menu");
            yield return Wait(0.4f);
            Check("Main Menu leaves the run", _g.State == "menu" && !dc.Playing);
            yield return Click(Q("btn-dungeon"), "The Dungeon");
            yield return Wait(0.3f);
            yield return Click(Q("dg-continue"), "Continue");
            yield return Wait(0.5f);
            Check("Continue resumes on floor 6", dc.Playing && game.FloorNumber == 6);

            // --- Boss Rush ---
            yield return Press(Key.Escape);
            yield return Wait(0.2f);
            yield return Click(Q("dg-main-menu"), "Main Menu");
            yield return Wait(0.3f);
            yield return Click(Q("btn-dungeon"), "The Dungeon");
            yield return Wait(0.3f);
            yield return Click(Q("dg-boss-rush"), "Boss Rush");
            yield return Wait(0.4f);
            Check("Boss Rush starts with the Good Plazmodium at level 5", game.Mode == DungeonMode.BossRush && game.Fight.Def == Defs.Bosses[0] && game.Player.level == 5);
            bool stageShot = false;
            for (float t = 0; t < 60 && game.Overlay != Overlay.RushOver; t += Time.unscaledDeltaTime)
            {
                game.Player.invuln = 1;
                if (game.RushStage == 1 && game.Fight.Active && game.Fight.Time > 1 && !stageShot) { stageShot = true; yield return Shot("dungeon-rush-stage-2"); }
                if (game.Fight.Active && game.Fight.Time > 1.2f) foreach (var e in game.Enemies) if (e.bossForm >= 0) e.hp = 0;
                yield return null;
            }
            yield return Wait(0.4f);
            Check("all five bosses back to back wins the Boss Rush", game.RushWon && game.Profile.bestRush > 0);
            yield return Shot("dungeon-rush-won");
            yield return Click(Q("dg-rush-menu"), "Main menu");
            yield return Wait(0.4f);
            Check("the Dungeon ends back at the menu", _g.State == "menu" && !dc.Playing);
        }

        /// <summary>Walks into the first room that matches, as if through its door.</summary>
        static bool Enter(DungeonGame g, Func<Room, bool> pred)
        {
            for (int y = 0; y < FloorSize; y++)
                for (int x = 0; x < FloorSize; x++)
                {
                    var r = g.Floor[x, y];
                    if (!r.exists || !pred(r)) continue;
                    g.Floor.cx = x; g.Floor.cy = y;
                    g.Player.pos = Center;
                    g.EnterRoom();
                    return true;
                }
            return false;
        }

        /// <summary>Lets a fight run, untouchable, until cond holds or time runs out.</summary>
        static IEnumerator Survive(DungeonGame g, float timeout, Func<bool> cond)
        {
            for (float t = 0; t < timeout && !cond(); t += Time.unscaledDeltaTime)
            {
                g.Player.invuln = 1;
                g.Player.hp = Mathf.Max(g.Player.hp, g.Player.maxHp * 0.5f);
                yield return null;
            }
        }
    }
}
