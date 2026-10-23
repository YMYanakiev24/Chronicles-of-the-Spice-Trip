/*
 * GameRoot.cs
 * ----------------------------------------------------------------------------
 * The bootstrap: the only object in the Boot scene. Creates the services,
 * the UI Toolkit document and its layers, the hub screens and the boss arena,
 * then runs the loading screen and shows the main menu (Game.js boot()).
 *
 * Layers are stacked in the same order as the web game's z-indexes:
 * menu screens < hub scenes (30) < fog wipe (35) < boss battle (40) < modals (45).
 *
 * Command line (used by the automated checks):
 *   -savedir <dir>     keep saves somewhere other than persistentDataPath
 *   -autopilot <dir>   play through every screen, write screenshots + a log, quit
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections;
using SpiceTrip.Battle;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using UnityEngine.UIElements;

namespace SpiceTrip.Core
{
    public class GameRoot : MonoBehaviour
    {
        public GameContext Ctx { get; private set; }
        public AudioSynth Audio { get; private set; }
        public HubController Hub { get; private set; }
        public BossBattleView Battle { get; private set; }
        public MainMenuScreen Menu { get; private set; }
        public Rpg.ChronicleController Chronicle { get; private set; }
        public LoadingScreen Loading { get; private set; }
        public VisualElement UiRoot { get; private set; }
        public bool Booted { get; private set; }
        public string State { get; private set; } = "loading";   // loading | menu | hub | chronicle

        float _time;

        public static GameRoot Instance { get; private set; }

        /// <summary>Tests set this so they never touch the player's real saves.</summary>
        public static string SaveDirOverride;

        void Awake()
        {
            Instance = this;
            var db = GameDatabase.Instance;
            if (db == null)
            {
                Debug.LogError("GameDatabase asset missing: run Spice Trip ▸ Rebuild Project in the editor.");
                enabled = false;
                return;
            }

            if (FindAnyObjectByType<AudioListener>() == null) gameObject.AddComponent<AudioListener>();
            Audio = gameObject.AddComponent<AudioSynth>();
            Ctx = new GameContext(db, new SaveService(SaveDirOverride ?? Arg("-savedir")), Audio);
            Audio.Apply(Ctx.Settings);

            BuildEventSystem();
            BuildUi();
        }

        void Start() => StartCoroutine(Boot());

        static string Arg(string name)
        {
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; i++)
                if (args[i] == name) return args[i + 1];
            return null;
        }

        void BuildEventSystem()
        {
            if (FindAnyObjectByType<EventSystem>() != null) return;
            var go = new GameObject("EventSystem", typeof(EventSystem));
            go.transform.SetParent(transform, false);
            var module = go.AddComponent<InputSystemUIInputModule>();
            module.AssignDefaultActions();
        }

        void BuildUi()
        {
            var uiGo = new GameObject("UI");
            uiGo.SetActive(false);
            uiGo.transform.SetParent(transform, false);
            var doc = uiGo.AddComponent<UIDocument>();
            doc.panelSettings = Resources.Load<PanelSettings>("UI/PanelSettings");
            uiGo.SetActive(true);

            var docRoot = doc.rootVisualElement;
            var sheet = Resources.Load<StyleSheet>("UI/Spice");
            if (sheet != null) docRoot.styleSheets.Add(sheet);
            var rpgSheet = Resources.Load<StyleSheet>("UI/Rpg");
            if (rpgSheet != null) docRoot.styleSheets.Add(rpgSheet);
            UiRoot = UiKit.El("root", "ui");
            docRoot.Add(UiRoot);
            UiFonts.Apply(UiRoot);

            var menuLayer = Layer("menu-layer");
            var sceneLayer = Layer("scene-layer");
            var rpgLayer = Layer("rpg-layer");
            var fogLayer = Layer("fog-layer");
            var battleLayer = Layer("battle-layer");
            var modalLayer = Layer("modal-layer");

            Loading = new LoadingScreen(Ctx);
            Menu = new MainMenuScreen(Ctx, () => Hub.EnterPath(),
                () => { HideMainMenu(); State = "chronicle"; Chronicle.StartNew(); },
                () => { HideMainMenu(); State = "chronicle"; Chronicle.ContinueSaved(); },
                () => Chronicle != null && Chronicle.HasSave);
            menuLayer.Add(Menu.Root);
            menuLayer.Add(Loading.Root);

            var battleGo = new GameObject("Battle");
            battleGo.transform.SetParent(transform, false);
            Battle = battleGo.AddComponent<BossBattleView>();
            Battle.Init(Ctx, battleLayer,
                Resources.Load<Material>("Materials/SpriteAlpha"),
                Resources.Load<Material>("Materials/SpriteAdditive"),
                Resources.Load<Material>("Materials/SpriteScreen"),
                () => Hub.EnterPath(true),
                id => Hub.MarkBossDefeated(id));

            Hub = new HubController(Ctx, sceneLayer, modalLayer, fogLayer,
                ShowMainMenu, HideMainMenu, (boss, ch) => Battle.Begin(boss, ch));

            var chronicleGo = new GameObject("Chronicle");
            chronicleGo.transform.SetParent(transform, false);
            Chronicle = chronicleGo.AddComponent<Rpg.ChronicleController>();
            Chronicle.Init(Ctx, Audio, UiRoot, rpgLayer,
                Resources.Load<Material>("Materials/SpriteAlpha"),
                Resources.Load<Material>("Materials/SpriteAdditive"),
                Resources.Load<Material>("Materials/SpriteScreen"),
                ShowMainMenu);
        }

        VisualElement Layer(string name)
        {
            var e = UiKit.El(null, name);
            e.style.position = Position.Absolute;
            e.style.left = 0; e.style.top = 0; e.style.right = 0; e.style.bottom = 0;
            e.pickingMode = PickingMode.Ignore;
            UiRoot.Add(e);
            return e;
        }

        IEnumerator Boot()
        {
            for (float p = 0; p <= 1.0001f; p += 0.1f)
            {
                Loading.SetProgress(p);
                yield return new WaitForSeconds(0.05f);
            }
            UiKit.Show(Loading.Root, false);
            // The web build skips its intro cinematic and boots straight to the main menu.
            ShowMainMenu();
            Booted = true;

            var autopilot = Arg("-autopilot");
            if (autopilot != null) gameObject.AddComponent<Autopilot>().Run(this, autopilot);
        }

        public void ShowMainMenu()
        {
            State = "menu";
            Menu.Show(true);
            Audio.PlayMusic(HubController.MusicMenu);
        }

        public void HideMainMenu()
        {
            State = "hub";
            Menu.Show(false);
        }

        void Update()
        {
            float dt = Mathf.Min(0.05f, Time.deltaTime);
            _time += dt;
            if (!Booted) Loading?.Tick(_time);
            if (State == "menu") Menu.Tick(_time);
            Hub?.Tick(_time, dt);
        }
    }

    /// <summary>
    /// Typeface selection in one place. The web game asked for Cinzel / EB Garamond
    /// but never loaded them, so browsers drew Georgia; the same OS serif is used
    /// here. Swap these names (or load a bundled font) to change the whole UI.
    /// </summary>
    public static class UiFonts
    {
        public static bool UseSystemSerif = true;
        public static readonly string[] Serif = { "Georgia", "Times New Roman" };

        public static void Apply(VisualElement root)
        {
            if (!UseSystemSerif) return;
            try
            {
                var font = Font.CreateDynamicFontFromOSFont(Serif, 32);
                if (font != null) root.style.unityFontDefinition = FontDefinition.FromFont(font);
            }
            catch (Exception e)
            {
                Debug.LogWarning($"System serif font unavailable, using the default UI font: {e.Message}");
            }
        }
    }
}
