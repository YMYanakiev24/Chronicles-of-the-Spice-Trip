/*
 * MenuScreens.cs
 * ----------------------------------------------------------------------------
 * The loading screen and the main menu (UI.js). The menu shows the painted
 * menu art with its baked-in title, a single ▶ Play button and a clickable
 * castle; both lead to the Path. Without the art it falls back to the text
 * title on a dark screen.
 * ----------------------------------------------------------------------------
 */

using System;
using SpiceTrip.Art;
using SpiceTrip.Core;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Hub
{
    public class LoadingScreen
    {
        public readonly VisualElement Root;
        readonly VisualElement _fill, _emblem;

        public LoadingScreen(GameContext ctx)
        {
            Root = El("screen", "loading-screen");
            _emblem = Icon("emblem", 44, "book-emblem");
            Root.Add(_emblem);
            Root.Add(Lbl("The Mythic Chronicles", "game-title small"));
            var bar = El("loading-bar");
            _fill = El("loading-fill");
            bar.Add(_fill);
            Root.Add(bar);
            var tips = ctx.Db.tips;
            Root.Add(Lbl(tips.Count > 0 ? tips[UnityEngine.Random.Range(0, tips.Count)] : "", "tip"));
        }

        public void SetProgress(float p) => _fill.style.width = Length.Percent(Mathf.Floor(Mathf.Clamp01(p) * 100));

        public void Tick(float time) => _emblem.style.rotate = new Rotate(new Angle(time / 8f * 360f, AngleUnit.Degree));
    }

    public class MainMenuScreen
    {
        public readonly VisualElement Root;
        public readonly ClickEl CastleHotspot;
        readonly VisualElement _castleLabel;
        readonly Action _onPlay;

        public MainMenuScreen(GameContext ctx, Action onPlay)
        {
            _onPlay = onPlay;
            Root = El("screen hidden", "main-menu");

            var art = ArtLibrary.Load("menu");
            if (art != null)
            {
                var bg = El("menu-bg");
                bg.style.backgroundImage = new StyleBackground(art);
                bg.pickingMode = PickingMode.Ignore;
                Root.Add(bg);
                Root.AddToClassList("has-menu-bg");   // hide the text title; the art has its own
            }

            Root.Add(Lbl("The Mythic Chronicles", "game-title"));
            Root.Add(Lbl("The Hidden World", "game-subtitle"));
            var buttons = El("menu-buttons", "main-menu-buttons");
            buttons.Add(Btn("Play", "menu-btn primary", () => { ctx.Audio.Sfx("ui"); _onPlay(); }, "play", 12));
            Root.Add(buttons);
            Root.Add(Lbl("An original fantasy realm · forge your legend", "footer-note"));

            // Castle hotspot lives inside the main menu so it shows/hides with it.
            CastleHotspot = new ClickEl(() => { ctx.Audio.Sfx("ui"); _onPlay(); }) { name = "castle-hotspot" };
            CastleHotspot.AddToClassList("castle-hotspot");
            _castleLabel = El("castle-label");
            _castleLabel.Add(Icon("play", 14, "btn-icon"));
            _castleLabel.Add(Lbl("Click the castle to enter"));
            _castleLabel.pickingMode = PickingMode.Ignore;
            CastleHotspot.Add(_castleLabel);
            Root.Add(CastleHotspot);

            Root.RegisterCallback<GeometryChangedEvent>(_ => Layout());
        }

        // width: clamp(240px, 46vw, 620px); height: clamp(200px, 54vh, 500px)
        void Layout()
        {
            float w = Root.resolvedStyle.width, h = Root.resolvedStyle.height;
            if (float.IsNaN(w) || float.IsNaN(h)) return;
            CastleHotspot.style.width = Mathf.Clamp(0.46f * w, 240, 620);
            CastleHotspot.style.height = Mathf.Clamp(0.54f * h, 200, 500);
        }

        /// <summary>The castle label's gentle bob (castlePulse, 2 s ease-in-out).</summary>
        public void Tick(float time)
        {
            float phase = (time % 2f) / 2f;
            float e = 0.5f - 0.5f * Mathf.Cos(phase * Mathf.PI * 2);
            _castleLabel.style.translate = new Translate(0, -5 * e);
        }

        public void Show(bool show) => UiKit.Show(Root, show);
    }
}
