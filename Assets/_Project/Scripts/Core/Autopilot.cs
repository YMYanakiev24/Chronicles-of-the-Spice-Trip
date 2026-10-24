/*
 * Autopilot.cs
 * ----------------------------------------------------------------------------
 * An end-to-end check that plays through every hub screen in a real build.
 * Clicks are injected as genuine mouse events through the Input System, so
 * they travel the same path as a player's clicks (EventSystem → UI Toolkit).
 * Each step writes a screenshot and a PASS/FAIL line to <dir>/autopilot.log,
 * then the game quits with exit code 0 (all passed) or 1.
 *
 * Started with:  SpiceTrip.exe -autopilot <dir> -savedir <dir>/save
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections;
using System.IO;
using System.Text;
using SpiceTrip.Battle;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace SpiceTrip.Core
{
    public class Autopilot : MonoBehaviour
    {
        GameRoot _g;
        string _dir;
        readonly StringBuilder _log = new StringBuilder();
        int _failures, _shot;

        public void Run(GameRoot g, string dir)
        {
            _g = g;
            _dir = dir;
            Directory.CreateDirectory(dir);
            StartCoroutine(Tour());
        }

        void Check(string what, bool ok)
        {
            if (!ok) _failures++;
            var line = $"{(ok ? "PASS" : "FAIL")}  {what}";
            _log.AppendLine(line);
            Debug.Log("[autopilot] " + line);
        }

        IEnumerator Shot(string name)
        {
            yield return new WaitForEndOfFrame();
            var file = Path.Combine(_dir, $"{++_shot:00}-{name}.png");
            var tex = ScreenCapture.CaptureScreenshotAsTexture();
            File.WriteAllBytes(file, tex.EncodeToPNG());
            Destroy(tex);
            _log.AppendLine($"shot  {Path.GetFileName(file)}");
        }

        static IEnumerator Wait(float s) { yield return new WaitForSecondsRealtime(s); }

        /// <summary>Moves the mouse to the element's centre and clicks it with real input events.</summary>
        IEnumerator Click(VisualElement e, string what = null)
        {
            if (e == null) { Check($"click {what}: element exists", false); yield break; }
            var root = _g.UiRoot;
            var c = e.worldBound.center;
            float sx = c.x * Screen.width / root.layout.width;
            float sy = Screen.height - c.y * Screen.height / root.layout.height;
            yield return MouseTo(new Vector2(sx, sy));
            var mouse = Mouse.current;
            InputSystem.QueueStateEvent(mouse, new MouseState { position = new Vector2(sx, sy) }.WithButton(UnityEngine.InputSystem.LowLevel.MouseButton.Left, true));
            yield return null;
            yield return null;
            InputSystem.QueueStateEvent(mouse, new MouseState { position = new Vector2(sx, sy) }.WithButton(UnityEngine.InputSystem.LowLevel.MouseButton.Left, false));
            yield return null;
            yield return null;
        }

        IEnumerator MouseTo(Vector2 screen)
        {
            var mouse = Mouse.current ?? InputSystem.AddDevice<Mouse>();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = screen });
            yield return null;
            yield return null;
        }

        VisualElement Q(string name) => _g.UiRoot.Q(name);

        IEnumerator Tour()
        {
            var hub = _g.Hub;
            var ctx = _g.Ctx;
            yield return Wait(0.5f);

            // --- main menu ---
            Check("boots to the main menu", _g.State == "menu");
            yield return Shot("menu");

            yield return Click(Q("castle-hotspot"), "castle");
            yield return Wait(0.3f);
            Check("castle opens the Path", hub.Active == "path");
            yield return Shot("path");

            // --- boss battle: lose, then win ---
            var boss = ctx.Db.bosses[0];
            yield return Click(hub.Path.BossButton(boss.id), "boss 1");
            yield return Wait(0.4f);
            var battle = _g.Battle;
            Check("boss hotspot starts the battle", battle.Active && hub.Active == "battle");
            Check($"boss starts with {boss.hp} HP", battle.Sim != null && Mathf.Approximately(battle.Sim.Boss.maxHp, boss.hp));
            Check($"{boss.name} fights in its own arena", battle.ShowingArenaArt);
            // Hold Space and strafe for a moment with real keyboard input.
            var kb = Keyboard.current ?? InputSystem.AddDevice<Keyboard>();
            InputSystem.QueueStateEvent(kb, new KeyboardState(Key.Space, Key.D));
            yield return Wait(1.2f);
            InputSystem.QueueStateEvent(kb, new KeyboardState());
            Check("Space fires player shots", battle.Sim.ShotsFired >= 3);
            Check("D strafes the player right", battle.Sim.Player.x > 450);
            yield return Shot("battle");

            battle.Sim.Player.hp = 1;
            battle.ScriptedInput = new BattleSim.Inputs { move = new Vector2(0, -1), aim = new Vector2(400, 90) };
            yield return WaitFor(() => battle.ResultShowing, 15);
            battle.ScriptedInput = null;
            Check("losing shows DEFEATED", battle.ResultTitle == "DEFEATED");
            yield return Shot("defeated");

            yield return Click(Q("bb-again"), "fight again");
            yield return Wait(0.3f);
            Check("Fight again restarts at full HP", battle.Active && !battle.ResultShowing && battle.Sim.Player.hp == battle.Sim.Player.maxHp);
            battle.Sim.Boss.hp = 8;
            bool splitShot = false;
            for (float t = 0; t < 15 && !battle.ResultShowing; t += Time.unscaledDeltaTime)
            {
                var sim = battle.Sim;
                sim.Player.hp = sim.Player.maxHp;
                if (sim.Units.Count > 1 && !splitShot)
                {
                    // Phase 3: the Good Plazmodium multiplies ×3 when killed
                    splitShot = true;
                    Check("the Good Plazmodium splits into three copies", sim.Units.Count == 3 && sim.Units.TrueForAll(u => u.form == 1));
                    battle.ScriptedInput = new BattleSim.Inputs();
                    yield return Shot("plazmodium-split");
                }
                if (splitShot) foreach (var u in sim.Units) u.hp = Mathf.Min(u.hp, 1);
                // aim at a boss body every frame, like a player tracking it with the mouse
                battle.ScriptedInput = new BattleSim.Inputs { aim = new Vector2(sim.Boss.x, sim.Boss.y), shoot = true };
                yield return null;
            }
            battle.ScriptedInput = null;
            Check("winning shows VICTORY", battle.ResultTitle == "VICTORY");
            Check("the first victory pays the bounty", battle.LastReward.HasValue && battle.LastReward.Value.firstWin && battle.LastReward.Value.gold == ctx.Db.Bounty(boss.id).first);
            Check("victory is saved", ctx.Hub.defeated.Contains(boss.id));
            yield return Shot("victory");

            yield return Click(Q("bb-return"), "return to the path");
            yield return Wait(0.3f);
            Check("Return goes back to the Path", hub.Active == "path" && !battle.Active);
            Check("defeated boss is ticked on the map", hub.Path.BossButton(boss.id).ClassListContains("defeated"));
            yield return Shot("path-defeated");

            // --- castle: corridor, shop, loom ---
            yield return Click(Q("path-castle"), "enter the castle");
            yield return Wait(0.3f);
            Check("castle opens the corridor", hub.Active == "corridor");
            yield return Shot("corridor");

            yield return Click(hub.Corridor.WizardDoor, "Marin's door");
            yield return Wait(0.15f);
            Check("fog wipe plays", hub.FogShowing);
            yield return Shot("fog");
            yield return Wait(0.8f);
            Check("Marin's door opens the shop", hub.Active == "shop");
            yield return Shot("shop");

            int gold = ctx.Hub.gold;
            var first = ctx.Db.shopStock[0];
            yield return Click(Q("shop-item-" + first.id), "buy " + first.id);
            yield return Wait(0.2f);
            Check($"buying {first.id} costs {first.price}", ctx.Hub.gold == gold - first.price);
            Check("purchase goes to the stash", ctx.Hub.StashCount(first.id) >= 1);
            Check("Marin thanks you", hub.Shop.MarinLine.Contains("A fine choice"));
            yield return Click(Q("shop-battle-healingDraught"), "buy a Healing Draught");
            yield return Wait(0.2f);
            Check("Marin's battle wares go to the battle satchel", ctx.Hub.SatchelCount("healingDraught") == 1);
            yield return Shot("shop-bought");

            int saved = ctx.Hub.gold;
            ctx.Hub.gold = 10;
            yield return Click(Q("shop-item-cinderSigil"), "buy the Cinder Sigil while poor");
            yield return Wait(0.2f);
            Check("too poor: no purchase", ctx.Hub.gold == 10);
            Check("too poor: Marin says so", hub.Shop.MarinLine.Contains("Not enough coin"));
            ctx.Hub.gold = saved;
            ctx.SaveHub();

            yield return Click(Q("shop-item-loom"), "the Enchanted Loom");
            yield return Wait(0.3f);
            Check("Loom opens", hub.Loom.IsOpen);
            Check("Loom weaves an image", hub.Loom.LoadImageFile(Path.Combine(Application.streamingAssetsPath, "assets", "path.jpg")));
            yield return Wait(0.2f);
            Check("8×8 grid by default", hub.Loom.Model.Cells.Count == 64);
            yield return Shot("loom");
            hub.Loom.TearAt(new Vector2(10, 10));
            hub.Loom.TearAt(new Vector2(200, 300));
            Check("clicking shards tears them", hub.Loom.Model.TornCount == 2);
            hub.Loom.TearAll();
            yield return Wait(0.35f);
            Check("Tear All tears the rest", hub.Loom.Model.TornCount == 64);
            yield return Shot("loom-torn");
            hub.Loom.Close();
            yield return Wait(0.2f);

            yield return Click(Q("scene-shop").Q(className: "scene-back"), "back to the hall");
            yield return Wait(0.9f);
            Check("back from the shop returns to the corridor", hub.Active == "corridor");

            // --- changing room ---
            yield return Click(hub.Corridor.ChangingDoor, "Changing Room");
            yield return Wait(0.3f);
            Check("Changing Room opens character select", hub.CharSelect.IsOpen);
            yield return Shot("changing-room");
            yield return Click(Q("char-atanas"), "Atanas");
            yield return Wait(0.5f);
            Check("picking a champion closes the modal", !hub.CharSelect.IsOpen);
            Check("champion is saved", ctx.Hub.selectedChar == "atanas" && File.ReadAllText(Path.Combine(ctx.Save.Directory, SaveService.HubFile)).Contains("atanas"));

            // --- fight as the champion, then flee ---
            yield return Click(Q("scene-corridor").Q(className: "scene-back"), "back to the path");
            yield return Wait(0.3f);
            var boss4 = ctx.Db.bosses[ctx.Db.bosses.Count - 1];
            yield return Click(hub.Path.BossButton(boss4.id), "boss 4");
            yield return Wait(0.8f);
            Check($"{boss4.name} fight starts with {boss4.hp} HP", battle.Active && Mathf.Approximately(battle.Sim.Boss.maxHp, boss4.hp));
            yield return Shot("battle-champion");

            // A Healing Draught on key 1
            battle.Sim.Player.hp = 40;
            yield return Press(Key.Digit1);
            yield return Wait(0.1f);
            Check("key 1 drinks the Healing Draught", battle.Sim.Player.hp >= 74 && ctx.Hub.SatchelCount("healingDraught") == 0);

            // Phase 3: Atanas' Aura of Calmness on right-click, then Vlado's tickets and bus №9
            battle.Sim.Player.hp = 40;
            var mouse = Mouse.current;
            var mp = mouse.position.ReadValue();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = mp }.WithButton(UnityEngine.InputSystem.LowLevel.MouseButton.Right, true));
            yield return null;
            yield return null;
            InputSystem.QueueStateEvent(mouse, new MouseState { position = mp }.WithButton(UnityEngine.InputSystem.LowLevel.MouseButton.Right, false));
            yield return Wait(0.2f);
            Check("right-click casts Aura of Calmness (heals 25)", battle.Sim.AuraT > 0 && battle.Sim.Player.hp >= 64);
            yield return Shot("atanas-aura");
            bool sawBus = false, sawTickets = false;
            for (float t = 0; t < 10.5f && battle.Active; t += Time.unscaledDeltaTime)
            {
                battle.Sim.Player.hp = battle.Sim.Player.maxHp;
                sawTickets |= battle.Sim.BossShots.Exists(s => s.kind == BattleSim.ShotKind.Ticket);
                if (!sawBus && battle.Sim.ActiveBus != null && battle.Sim.ActiveBus.driving && battle.Sim.ActiveBus.x > 250)
                {
                    sawBus = true;
                    yield return Shot("vlado-bus");
                }
                yield return null;
            }
            Check("Vlado throws ticket spirals", sawTickets);
            Check("Vlado's whistle sends bus №9", sawBus);
            yield return Click(Q("bb-flee"), "Flee");
            yield return Wait(0.3f);
            Check("Flee returns to the Path", hub.Active == "path" && !battle.Active);

            // the other two arenas
            foreach (var other in new[] { ctx.Db.bosses[2], ctx.Db.bosses[1] })
            {
                yield return Click(hub.Path.BossButton(other.id), other.name);
                yield return Wait(1.5f);
                Check($"{other.name} fights in its own arena", battle.Active && battle.ShowingArenaArt);
                yield return Shot("arena-" + other.id);
                yield return Click(Q("bb-flee"), "Flee");
                yield return Wait(0.3f);
            }

            // The Gauntlet (all four bosses back to back) once every boss has fallen
            foreach (var b in ctx.Db.bosses) hub.MarkBossDefeated(b.id);
            hub.EnterPath(true);
            yield return Wait(0.3f);
            int goldBefore = ctx.Hub.gold;
            yield return Click(Q("btn-gauntlet"), "The Gauntlet");
            yield return Wait(0.5f);
            Check("the Gauntlet starts with the first boss", battle.InGauntlet && battle.GauntletStage == 0);
            var order = ctx.Db.gauntlet.order;
            for (int stage = 0; stage < order.Count; stage++)
            {
                for (float t = 0; t < 8 && battle.Sim.Active; t += Time.unscaledDeltaTime)
                {
                    var sim = battle.Sim;
                    sim.Player.hp = Mathf.Max(sim.Player.hp, sim.Player.maxHp * 0.6f);
                    foreach (var u in sim.Units) u.hp = Mathf.Min(u.hp, 1);
                    battle.ScriptedInput = new BattleSim.Inputs { aim = new Vector2(sim.Boss.x, sim.Boss.y), shoot = true };
                    yield return null;
                }
                battle.ScriptedInput = null;
                if (stage < order.Count - 1)
                {
                    yield return Wait(0.3f);
                    if (stage == 0) yield return Shot("gauntlet-stage");
                    battle.SkipStageTransition();
                    yield return Wait(0.3f);
                }
            }
            yield return Wait(0.4f);
            Check("the Gauntlet is conquered", battle.ResultTitle == "GAUNTLET CONQUERED");
            Check("the Gauntlet pays its bounty", ctx.Hub.gold == goldBefore + ctx.Db.gauntlet.first);
            yield return Shot("gauntlet-won");
            yield return Click(Q("bb-return"), "return to the path");
            yield return Wait(0.4f);
            Check("the Path shows best times and stars", Q("scene-path").Q(className: "boss-best") != null);
            yield return Shot("path-progress");

            yield return Click(Q("scene-path").Q(className: "scene-back"), "leave");
            yield return Wait(0.3f);
            Check("Leave returns to the main menu", _g.State == "menu");

            yield return ChronicleTour();

            _log.AppendLine(_failures == 0 ? "RESULT  all checks passed" : $"RESULT  {_failures} check(s) failed");
            File.WriteAllText(Path.Combine(_dir, "autopilot.log"), _log.ToString());
            yield return Wait(0.2f);
#if UNITY_EDITOR
            UnityEditor.EditorApplication.Exit(_failures == 0 ? 0 : 1);
#else
            Application.Quit(_failures == 0 ? 0 : 1);
#endif
        }

        // =====================================================================
        // The Chronicle (restored RPG)
        // =====================================================================
        IEnumerator Press(params Key[] keys)
        {
            var kb = Keyboard.current ?? InputSystem.AddDevice<Keyboard>();
            InputSystem.QueueStateEvent(kb, new KeyboardState(keys));
            yield return null;
            yield return null;
            InputSystem.QueueStateEvent(kb, new KeyboardState());
            yield return null;
            yield return null;
        }

        IEnumerator Hold(float seconds, params Key[] keys)
        {
            var kb = Keyboard.current ?? InputSystem.AddDevice<Keyboard>();
            InputSystem.QueueStateEvent(kb, new KeyboardState(keys));
            yield return Wait(seconds);
            InputSystem.QueueStateEvent(kb, new KeyboardState());
            yield return null;
        }

        IEnumerator ChronicleTour()
        {
            var ctx = _g.Ctx;
            var ch = _g.Chronicle;
            int stashed = ctx.Hub.StashCount(ctx.Db.shopStock[0].id);
            Check("Marin's purchase is waiting in the stash", stashed >= 1);

            yield return Click(Q("btn-new-chronicle"), "New Chronicle");
            yield return Wait(1.0f);
            Check("New Chronicle plays the intro", ch.Intro.Playing);
            yield return Shot("intro");
            yield return Click(Q("intro"), "skip intro");
            yield return Wait(0.6f);
            var game = ch.Game;
            Check("the Chronicle starts in the Mistwood", !ch.Intro.Playing && game.Playing && game.CurrentRegion == "mistwood");
            Check("the first quest is active", game.Quests.state["awakening"] == "active");
            Check("Marin's wares are handed over", game.Player.ItemCount(ctx.Db.shopStock[0].id) >= stashed && ctx.Hub.stash.Count == 0);
            Check("region music plays", _g.Audio.CurrentMood == "mist");
            yield return Wait(1.2f);
            yield return Shot("mistwood");

            // walk with the keyboard
            float x0 = game.Player.x;
            yield return Hold(0.6f, Key.D);
            Check("D walks the Unmarked right", game.Player.x > x0 + 20);

            // cast Emberhex with key 1, aimed by the mouse
            game.Enemies.Clear();
            yield return MouseTo(new Vector2(Screen.width * 0.75f, Screen.height * 0.5f));
            float mana = game.Player.mana;
            yield return Press(Key.Digit1);
            yield return Wait(0.05f);
            Check("key 1 casts Emberhex", game.Player.mana < mana && (game.Magic.Projectiles.Count > 0 || game.Quests.progress["awakening"][0] == 1));
            yield return Shot("emberhex");
            Check("casting advances the quest", game.Quests.progress["awakening"][0] == 1);

            // banish three mistlings
            for (int i = 0; i < 3; i++)
            {
                var m = new Rpg.Enemy(game, ctx.Db.Creature("mistling"), game.Player.x + 30, game.Player.y);
                game.Enemies.Add(m);
                game.Combat.DamageEnemy(m, 999, "Fire");
            }
            yield return Wait(0.3f);
            Check("three mistlings make the quest ready", game.Quests.state["awakening"] == "ready");

            // talk to the Oracle with E and hand the quest in through the dialogue buttons
            var oracle = game.Npcs.Find(n => n.def.id == "oracle");
            game.Player.x = oracle.x + 12; game.Player.y = oracle.y;
            yield return Wait(0.2f);
            Check("the interact prompt appears near the Oracle", ch.Hud.InteractShowing);
            yield return Press(Key.E);
            yield return Wait(0.3f);
            Check("E opens the dialogue", ch.Hud.DialogueOpen);
            yield return Shot("dialogue");
            for (int i = 0; i < 4 && ch.Hud.DialogueOpen; i++)
            {
                VisualElement first = null;
                foreach (var c in ch.Hud.DialogueChoices) { first = c; break; }
                yield return Click(first, "dialogue choice");
                yield return Wait(0.25f);
            }
            Check("the quest is handed in", game.Quests.state["awakening"] == "completed");
            Check("the reward gold is paid", game.Player.gold >= 20);
            yield return Wait(0.3f);
            yield return Shot("quest-complete");

            // the book panels, opened with their hotkeys
            (Key key, string panel, string shot)[] panels =
            {
                (Key.Tab, "quests", "journal"), (Key.I, "inventory", "satchel"), (Key.K, "skills", "spellbook"),
                (Key.M, "map", "world-map"), (Key.B, "skills", "bestiary"),
            };
            foreach (var (key, panel, shot) in panels)
            {
                yield return Press(key);
                yield return Wait(0.25f);
                Check($"{key} opens the {shot}", ch.Book.ActivePanel == panel);
                yield return Shot(shot);
                yield return Press(Key.Escape);
                yield return Wait(0.2f);
                Check($"Esc closes the {shot}", ch.Book.ActivePanel == null);
            }
            Check("the satchel holds Marin's ware", game.Player.ItemCount(ctx.Db.shopStock[0].id) >= 1);

            // pause → save → quit to title → continue
            yield return Press(Key.Escape);
            yield return Wait(0.25f);
            Check("Esc pauses", ch.Book.ActivePanel == "pause");
            yield return Shot("pause");
            yield return Click(Q("pause-save"), "Save Chronicle");
            yield return Wait(0.2f);
            Check("saving writes save.json", File.Exists(Path.Combine(ctx.Save.Directory, SaveService.SaveFile)));
            float sx = game.Player.x, sy = game.Player.y;
            int level = game.Player.level;
            yield return Click(Q("pause-quit"), "Quit to Title");
            yield return Wait(0.4f);
            Check("Quit to Title returns to the menu", _g.State == "menu" && !ch.Active);
            yield return Click(Q("btn-continue-chronicle"), "Continue Chronicle");
            yield return Wait(0.6f);
            Check("Continue restores the Chronicle", ch.Active && game.Player != null && Mathf.Abs(game.Player.x - sx) < 1 && Mathf.Abs(game.Player.y - sy) < 1 && game.Player.level == level);
            Check("the completed quest stays completed", game.Quests.state["awakening"] == "completed");

            // death and respawn
            game.Player.invuln = 0;
            game.Player.TakeDamage(9999);
            yield return Wait(0.3f);
            Check("dying shows the death screen", ch.Hud.DeathShowing);
            yield return Shot("death");
            yield return Click(Q("respawn-btn"), "Return to the last shrine");
            yield return Wait(0.3f);
            Check("respawning restores the Unmarked", !game.Player.dead && !ch.Hud.DeathShowing);

            // fast travel through the World Map
            game.Player.level = 9;
            yield return Wait(0.2f);
            yield return Press(Key.M);
            yield return Wait(0.25f);
            yield return Click(Q("region-caves"), "the Underway");
            yield return Wait(0.2f);
            yield return Click(Q("travel-btn"), "Travel here");
            yield return Wait(1.5f);
            Check("fast travel reaches the Underway", game.CurrentRegion == "caves" && ch.Book.ActivePanel == null);
            yield return Shot("underway");

            // A new realm quest: Damon in the Hollow Reach
            game.TravelTo("ruins");
            yield return Wait(1.0f);
            var damon = game.Npcs.Find(n => n.def.id == "damon");
            Check("the Hollow Reach has its quest-giver", damon != null && game.Map.Pickups.FindAll(p => p.item == "graveToken").Count == 5);
            game.Enemies.Clear();
            game.Player.x = damon.x + 12; game.Player.y = damon.y;
            yield return Wait(0.2f);
            yield return Press(Key.E);
            yield return Wait(0.3f);
            Check("Damon talks", ch.Hud.DialogueOpen);
            yield return Shot("damon");
            for (int i = 0; i < 4 && ch.Hud.DialogueOpen; i++)
            {
                VisualElement first = null;
                foreach (var c in ch.Hud.DialogueChoices) { first = c; break; }
                yield return Click(first, "dialogue choice");
                yield return Wait(0.25f);
            }
            Check("Names of the Fallen is accepted", game.Quests.state["namesOfTheFallen"] == "active");
            foreach (var pk in game.Map.Pickups.FindAll(p => p.item == "graveToken"))
            {
                game.Player.x = pk.x; game.Player.y = pk.y;
                yield return null;
                yield return null;
            }
            Check("five Grave Tokens make it ready", game.Quests.state["namesOfTheFallen"] == "ready");
            yield return Shot("grave-tokens");
            game.TravelTo("citadel");
            yield return Wait(1.5f);
            yield return Shot("pale-citadel");

            yield return Press(Key.Escape);
            yield return Wait(0.25f);
            yield return Click(Q("pause-quit"), "Quit to Title");
            yield return Wait(0.4f);
            Check("the Chronicle ends back at the menu", _g.State == "menu");
        }

        static IEnumerator WaitFor(Func<bool> cond, float timeout)
        {
            float t = 0;
            while (!cond() && t < timeout) { t += Time.unscaledDeltaTime; yield return null; }
        }
    }
}
