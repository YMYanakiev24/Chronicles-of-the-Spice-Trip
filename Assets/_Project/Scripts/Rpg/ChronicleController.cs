/*
 * ChronicleController.cs
 * ----------------------------------------------------------------------------
 * Runs a Chronicle inside the Unity app: owns the ChronicleGame rules, the
 * RpgView renderer, the HUD, the book panels and the intro; turns Input
 * System state into RpgInput; handles the hotkeys from Game._bindKeys (Esc,
 * Tab, I, K, M, B), auto-pause on focus loss, saving, deleting, quitting to
 * the title, and Marin's stash being handed over when a Chronicle begins.
 * ----------------------------------------------------------------------------
 */

using System;
using SpiceTrip.Core;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace SpiceTrip.Rpg
{
    public class ChronicleController : MonoBehaviour
    {
        GameContext _ctx;
        AudioSynth _audio;
        VisualElement _uiRoot;
        Action _showMenu;

        public ChronicleGame Game { get; private set; }
        public RpgView View { get; private set; }
        public RpgHud Hud { get; private set; }
        public BookPanels Book { get; private set; }
        public IntroCinematic Intro { get; private set; }
        public bool Active { get; private set; }

        /// <summary>Synthetic input for tests and the autopilot; when set, devices are ignored.</summary>
        public RpgInput? ScriptedInput { get; set; }

        public void Init(GameContext ctx, AudioSynth audio, VisualElement uiRoot, VisualElement layer,
            Material alpha, Material additive, Material screen, Action showMenu)
        {
            _ctx = ctx;
            _audio = audio;
            _uiRoot = uiRoot;
            _showMenu = showMenu;
            Game = new ChronicleGame(ctx.Db, audio, null);
            View = gameObject.AddComponent<RpgView>();
            View.Init(Game, alpha, additive, screen);
            Hud = new RpgHud(Game, ctx, View, layer, () => Game.RespawnPlayer());
            Book = new BookPanels(Game, ctx, Hud, layer, SaveGame, QuitToTitle, DeleteSave, ApplySettings);
            Hud.Book = Book;
            Game.Ui = Hud;
            Intro = new IntroCinematic(ctx, layer);
        }

        public bool HasSave => System.IO.File.Exists(ChronicleSave.PathIn(_ctx.Save));

        // =========================================================================
        // Start / continue / save / quit
        // =========================================================================
        public void StartNew(bool skipIntro = false)
        {
            Active = true;
            View.Resize(View.RenderWidth, View.RenderHeight);
            if (skipIntro) { BeginNew(); return; }
            Intro.Play(Mathf.RoundToInt(View.ViewW), Mathf.RoundToInt(View.ViewH), BeginNew);
        }

        void BeginNew()
        {
            Game.StartNewGame();
            BeginPlay();
        }

        public void ContinueSaved()
        {
            var data = ChronicleSave.Read(_ctx.Save);
            if (data == null) { StartNew(); return; }
            Active = true;
            View.Resize(View.RenderWidth, View.RenderHeight);
            Game.Continue(data);
            BeginPlay();
        }

        void BeginPlay()
        {
            Game.ViewW = View.ViewW;
            Game.ViewH = View.ViewH;
            Game.ComputeCamera();
            View.Show(true);
            Hud.ShowHUD(true);
            // Deliver anything bought from Marin's shop while at the menu.
            if (ApplyStash()) Hud.Toast("Marin's wares are in your satchel.", "gold");
            Hud.RefreshHUD();
        }

        bool ApplyStash()
        {
            bool any = false;
            foreach (var e in _ctx.Hub.stash)
                if (e.count > 0) { Game.Player.AddItem(e.id, e.count); any = true; }
            if (any) { _ctx.Hub.stash.Clear(); _ctx.SaveHub(); }
            return any;
        }

        public void SaveGame()
        {
            if (Game.Player == null) return;
            ChronicleSave.From(Game).Write(_ctx.Save);
            _audio.Sfx("quest");
        }

        void DeleteSave()
        {
            try { System.IO.File.Delete(ChronicleSave.PathIn(_ctx.Save)); } catch (Exception) { /* ignore */ }
        }

        void ApplySettings()
        {
            _audio.Apply(_ctx.Settings);
            _ctx.Save.SaveSettings(_ctx.Settings);
        }

        public void QuitToTitle()
        {
            SaveGame();
            Game.Stop();
            Hud.ShowHUD(false);
            Hud.HideDialogue();
            Hud.HideDeath();
            View.Show(false);
            Active = false;
            _showMenu();
        }

        // =========================================================================
        // Loop
        // =========================================================================
        void Update()
        {
            if (!Active) return;
            float dt = Mathf.Min(0.05f, Time.deltaTime);
            var kb = Keyboard.current;
            var mouse = Mouse.current;

            if (Intro.Playing)
            {
                bool any = (kb != null && kb.anyKey.wasPressedThisFrame) || (mouse != null && mouse.leftButton.wasPressedThisFrame);
                Intro.Tick(dt, any);
                return;
            }
            if (!Game.Playing) return;

            HandleHotkeys(kb);
            View.Resize(View.RenderWidth, View.RenderHeight);
            Game.ViewW = View.ViewW;
            Game.ViewH = View.ViewH;
            Game.RealTimeMs = Time.realtimeSinceStartup * 1000;
            Game.Tick(dt, ScriptedInput ?? ReadInput(kb, mouse));
            if (!Game.Playing) return;
            View.Render();
            Hud.Tick(dt);
        }

        void OnApplicationFocus(bool focus)
        {
            if (!focus && Active && Game.Playing && !Hud.IsBlocking && !Game.Player.dead && !Intro.Playing) Book.OpenPause();
        }

        void HandleHotkeys(Keyboard kb)
        {
            if (kb == null) return;
            if (kb.escapeKey.wasPressedThisFrame)
            {
                if (Game.DialogueNpc != null) Game.EndDialogue();
                else if (Book.ActivePanel != null) Book.ClosePanel();
                else Book.OpenPause();
                return;
            }
            if (Game.DialogueNpc != null) return;
            if (kb.tabKey.wasPressedThisFrame) Book.TogglePanel("quests");
            else if (kb.iKey.wasPressedThisFrame) Book.TogglePanel("inventory");
            else if (kb.kKey.wasPressedThisFrame) Book.TogglePanel("skills");
            else if (kb.mKey.wasPressedThisFrame) Book.TogglePanel("map");
            else if (kb.bKey.wasPressedThisFrame) { if (Book.ActivePanel != null) Book.ClosePanel(); else Book.OpenBestiary(); }
        }

        RpgInput ReadInput(Keyboard kb, Mouse mouse)
        {
            var inp = RpgInput.None;
            if (kb != null)
            {
                if (kb.wKey.isPressed || kb.upArrowKey.isPressed) inp.moveY += 1;
                if (kb.sKey.isPressed || kb.downArrowKey.isPressed) inp.moveY -= 1;
                if (kb.dKey.isPressed || kb.rightArrowKey.isPressed) inp.moveX += 1;
                if (kb.aKey.isPressed || kb.leftArrowKey.isPressed) inp.moveX -= 1;
                inp.run = kb.leftShiftKey.isPressed || kb.rightShiftKey.isPressed;
                inp.rollPressed = kb.spaceKey.wasPressedThisFrame;
                inp.interactPressed = kb.eKey.wasPressedThisFrame;
                Key[] digits = { Key.Digit1, Key.Digit2, Key.Digit3, Key.Digit4, Key.Digit5, Key.Digit6 };
                Key[] numpad = { Key.Numpad1, Key.Numpad2, Key.Numpad3, Key.Numpad4, Key.Numpad5, Key.Numpad6 };
                for (int i = 0; i < 6; i++)
                    if (kb[digits[i]].wasPressedThisFrame || kb[numpad[i]].wasPressedThisFrame) { inp.castSlot = i; break; }
            }
            if (mouse != null)
            {
                var sp = mouse.position.ReadValue();
                inp.aimWorld = View.ScreenToWorld(new Vector2(sp.x, Screen.height - sp.y));
                bool overUi = PointerOverClickable(sp);
                inp.meleePressed = mouse.leftButton.wasPressedThisFrame && !overUi;
                inp.castFirstPressed = mouse.rightButton.wasPressedThisFrame && !overUi;
            }
            return inp;
        }

        // Clicks on HUD buttons (e.g. hotbar slots) must not also swing the blade.
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
