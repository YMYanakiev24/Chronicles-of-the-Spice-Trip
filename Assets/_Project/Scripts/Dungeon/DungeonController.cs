/*
 * DungeonController.cs
 * ----------------------------------------------------------------------------
 * Runs the Dungeon inside the Unity app: opens the lobby from the main menu,
 * starts runs and Boss Rushes with the champion chosen in the lobby (shared
 * with the Changing Room), turns the Input System into DungeonInput, handles
 * Esc and focus loss, and keeps the run save and the records on disk.
 * ----------------------------------------------------------------------------
 */

using System;
using SpiceTrip.Core;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace SpiceTrip.Dungeon
{
    public class DungeonController : MonoBehaviour, DungeonHud.IActions
    {
        GameContext _ctx;
        VisualElement _uiRoot;
        Action _showMenu;

        public DungeonGame Game { get; private set; }
        public DungeonView View { get; private set; }
        public DungeonHud Hud { get; private set; }
        public bool InLobby { get; private set; }
        public bool Playing { get; private set; }

        /// <summary>Synthetic input for tests and the autopilot; when set, devices are ignored.</summary>
        public DungeonInput? ScriptedInput { get; set; }
        /// <summary>Pause when the window loses focus (off for automated runs).</summary>
        public bool AutoPause { get; set; } = true;

        public void Init(GameContext ctx, VisualElement uiRoot, VisualElement layer, Material alpha, Material additive, Action showMenu)
        {
            _ctx = ctx;
            _uiRoot = uiRoot;
            _showMenu = showMenu;
            Game = new DungeonGame(ctx.Audio) { Profile = DungeonSave.ReadProfile(ctx.Save) };
            Game.ProfileChanged += () => DungeonSave.WriteProfile(_ctx.Save, Game.Profile);
            View = gameObject.AddComponent<DungeonView>();
            View.Init(Game, ctx.Db, alpha, additive);
            Hud = new DungeonHud(Game, ctx, View, layer, this);
        }

        public bool HasSave => DungeonSave.HasRun(_ctx.Save);

        public void OpenLobby()
        {
            InLobby = true;
            Playing = false;
            View.Show(false);
            Hud.ShowLobby(HasSave);
            _ctx.Audio.PlayMusic("menu");
        }

        // ---- IActions (the lobby and panel buttons) ----
        public void NewRun() { Game.StartRun(_ctx.Hub.selectedChar); BeginPlay(); }
        public void BossRush() { Game.StartBossRush(_ctx.Hub.selectedChar); BeginPlay(); }

        public void ContinueRun()
        {
            var run = DungeonSave.ReadRun(_ctx.Save);
            if (run == null) { NewRun(); return; }
            Game.Continue(run);
            BeginPlay();
        }

        public void LeaveLobby()
        {
            InLobby = false;
            Hud.ShowAll(false);
            _showMenu();
        }

        public void SaveRun()
        {
            var run = Game.MakeSave();
            if (run == null) return;
            DungeonSave.WriteRun(_ctx.Save, run);
            _ctx.Audio.Sfx("quest");
        }

        public void QuitToMenu()
        {
            Playing = false;
            InLobby = false;
            Game.Fight.End();
            Game.Overlay = Overlay.None;
            View.Show(false);
            Hud.ShowAll(false);
            _showMenu();
        }

        void BeginPlay()
        {
            InLobby = false;
            Playing = true;
            Hud.BeginRun();
            View.Show(true);
            View.Render();
        }

        // =====================================================================
        // Loop
        // =====================================================================
        void Update()
        {
            if (!Playing) return;
            float dt = Mathf.Min(0.05f, Time.deltaTime);
            var kb = Keyboard.current;
            if (kb != null && kb.escapeKey.wasPressedThisFrame) TogglePause();
            Game.Tick(dt, ScriptedInput ?? ReadInput(kb, Mouse.current));
            if (!Playing) return;           // a panel button may have left for the menu
            View.Render();
            Hud.Tick(dt);
        }

        public void TogglePause()
        {
            switch (Game.Overlay)
            {
                case Overlay.None: Game.Overlay = Overlay.Pause; break;
                case Overlay.Pause:
                case Overlay.Shop: Game.Overlay = Overlay.None; break;
                default: return;            // the end-of-run panels need a button
            }
            _ctx.Audio.Sfx("ui");
        }

        void OnApplicationFocus(bool focus)
        {
            if (!focus && AutoPause && Playing && Game.Overlay == Overlay.None) Game.Overlay = Overlay.Pause;
        }

        DungeonInput ReadInput(Keyboard kb, Mouse mouse)
        {
            var inp = DungeonInput.None;
            if (kb != null)
            {
                if (kb.wKey.isPressed || kb.upArrowKey.isPressed) inp.move.y -= 1;
                if (kb.sKey.isPressed || kb.downArrowKey.isPressed) inp.move.y += 1;
                if (kb.aKey.isPressed || kb.leftArrowKey.isPressed) inp.move.x -= 1;
                if (kb.dKey.isPressed || kb.rightArrowKey.isPressed) inp.move.x += 1;
                inp.run = kb.leftShiftKey.isPressed || kb.rightShiftKey.isPressed;
                inp.rollPressed = kb.spaceKey.wasPressedThisFrame;
                inp.interactPressed = kb.eKey.wasPressedThisFrame;
                inp.specialPressed = kb.qKey.wasPressedThisFrame;
                if (kb.digit1Key.wasPressedThisFrame || kb.numpad1Key.wasPressedThisFrame) inp.slot = 0;
                else if (kb.digit2Key.wasPressedThisFrame || kb.numpad2Key.wasPressedThisFrame) inp.slot = 1;
                else if (kb.digit3Key.wasPressedThisFrame || kb.numpad3Key.wasPressedThisFrame) inp.slot = 2;
            }
            if (mouse != null)
            {
                var sp = mouse.position.ReadValue();
                inp.aim = View.ScreenToRoom(sp);
                bool overUi = PointerOverClickable(sp);
                inp.meleePressed = mouse.leftButton.wasPressedThisFrame && !overUi;
                inp.castPressed = mouse.rightButton.wasPressedThisFrame && !overUi;
            }
            else inp.aim = Game.Player.pos + Game.Player.facing * 40;
            return inp;
        }

        // Clicks on panel buttons must not also swing the blade.
        bool PointerOverClickable(Vector2 screenPos)
        {
            var panel = _uiRoot.panel;
            if (panel == null) return false;
            var p = RuntimePanelUtils.ScreenToPanel(panel, new Vector2(screenPos.x, Screen.height - screenPos.y));
            for (var e = panel.Pick(p); e != null; e = e.parent)
                if (e is ClickEl) return true;
            return false;
        }
    }
}
