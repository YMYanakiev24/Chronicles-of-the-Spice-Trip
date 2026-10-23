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
            for (float t = 0; t < 10 && !battle.ResultShowing; t += Time.unscaledDeltaTime)
            {
                // aim at the boss every frame, like a player tracking it with the mouse
                battle.ScriptedInput = new BattleSim.Inputs { aim = new Vector2(battle.Sim.Boss.x, battle.Sim.Boss.y), shoot = true };
                yield return null;
            }
            battle.ScriptedInput = null;
            Check("winning shows VICTORY", battle.ResultTitle == "VICTORY");
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
            yield return Click(Q("bb-flee"), "Flee");
            yield return Wait(0.3f);
            Check("Flee returns to the Path", hub.Active == "path" && !battle.Active);

            yield return Click(Q("scene-path").Q(className: "scene-back"), "leave");
            yield return Wait(0.3f);
            Check("Leave returns to the main menu", _g.State == "menu");

            _log.AppendLine(_failures == 0 ? "RESULT  all checks passed" : $"RESULT  {_failures} check(s) failed");
            File.WriteAllText(Path.Combine(_dir, "autopilot.log"), _log.ToString());
            yield return Wait(0.2f);
#if UNITY_EDITOR
            UnityEditor.EditorApplication.Exit(_failures == 0 ? 0 : 1);
#else
            Application.Quit(_failures == 0 ? 0 : 1);
#endif
        }

        static IEnumerator WaitFor(Func<bool> cond, float timeout)
        {
            float t = 0;
            while (!cond() && t < timeout) { t += Time.unscaledDeltaTime; yield return null; }
        }
    }
}
