/*
 * UiKit.cs
 * ----------------------------------------------------------------------------
 * Small helpers for building the UI Toolkit tree from code, plus the generated
 * textures that stand in for CSS gradients (USS has none): the fog wipe, the
 * shop panel's fade and the Loom's checkerboard.
 * ----------------------------------------------------------------------------
 */

using System;
using SpiceTrip.Art;
using UnityEngine;
using UnityEngine.UIElements;

namespace SpiceTrip.Hub
{
    /// <summary>
    /// A clickable element. Used instead of UI Toolkit's Button so the default
    /// theme's button chrome (hover/pressed backgrounds) never fights our styles.
    /// </summary>
    public class ClickEl : VisualElement
    {
        public readonly Clickable Clickable;

        public ClickEl(Action onClick)
        {
            Clickable = new Clickable(() => onClick?.Invoke());
            this.AddManipulator(Clickable);
        }
    }

    public static class UiKit
    {
        public static VisualElement El(string classes = null, string name = null)
        {
            var e = new VisualElement();
            Classes(e, classes);
            if (name != null) e.name = name;
            return e;
        }

        public static Label Lbl(string text, string classes = null)
        {
            var l = new Label(text);
            Classes(l, classes);
            return l;
        }

        public static ClickEl Btn(string text, string classes, Action onClick, string icon = null, float iconSize = 12)
        {
            var b = new ClickEl(() => onClick?.Invoke());
            Classes(b, classes);
            if (icon != null)
            {
                var ic = Icon(icon, iconSize);
                ic.AddToClassList("btn-icon");
                b.Add(ic);
            }
            if (!string.IsNullOrEmpty(text))
            {
                var l = new Label(text) { pickingMode = PickingMode.Ignore };
                l.AddToClassList("btn-text");
                b.Add(l);
            }
            return b;
        }

        /// <summary>A pixel icon from Icons, height-scaled to `size` panel pixels, crisp.</summary>
        public static VisualElement Icon(string name, float size, string classes = null)
        {
            var e = El(classes);
            var tex = Icons.Get(name);
            e.pickingMode = PickingMode.Ignore;
            if (tex == null) return e;
            e.style.backgroundImage = new StyleBackground(tex);
            e.style.height = size;
            e.style.width = size * tex.width / tex.height;
            e.style.flexShrink = 0;
            return e;
        }

        /// <summary>The order a turnaround shows a champion in: front, left side, back, right side.</summary>
        public static readonly string[] TurnaroundOrder = { "down", "left", "up", "right" };

        /// <summary>
        /// A champion's walking figure from every side (front, left, back, right) in a row,
        /// walking in place, `figureHeight` panel pixels tall.
        /// </summary>
        public static VisualElement Turnaround(string championId, float figureHeight, string classes = null)
        {
            var hero = HeroSprites.For(championId);
            var row = El("turnaround");
            Classes(row, classes);
            row.pickingMode = PickingMode.Ignore;
            var figures = new VisualElement[TurnaroundOrder.Length];
            for (int i = 0; i < figures.Length; i++)
            {
                var f = figures[i] = El("turnaround-figure");
                f.name = "turn-" + TurnaroundOrder[i];
                f.pickingMode = PickingMode.Ignore;
                f.style.height = figureHeight;
                f.style.width = figureHeight * HeroSprites.W / HeroSprites.H;
                SetBackground(f, hero.Frame(TurnaroundOrder[i], 0));
                row.Add(f);
            }
            row.schedule.Execute(() =>
            {
                for (int i = 0; i < figures.Length; i++)
                    SetBackground(figures[i], hero.Walk(TurnaroundOrder[i], Time.unscaledTime));
            }).Every(125);
            return row;
        }

        public static void Classes(VisualElement e, string classes)
        {
            if (string.IsNullOrEmpty(classes)) return;
            foreach (var c in classes.Split(' '))
                if (c.Length > 0) e.AddToClassList(c);
        }

        public static void Show(VisualElement e, bool show) => e.EnableInClassList("hidden", !show);

        public static void SetBackground(VisualElement e, Texture2D tex)
        {
            e.style.backgroundImage = tex != null ? new StyleBackground(tex) : new StyleBackground(StyleKeyword.None);
        }

        public static void SetBackground(VisualElement e, Sprite sprite)
        {
            e.style.backgroundImage = sprite != null ? new StyleBackground(sprite) : new StyleBackground(StyleKeyword.None);
        }

        // --- generated textures --------------------------------------------------------
        static Texture2D _fog, _shopFade, _checker;

        /// <summary>The fog wipe: three radial blobs over a vertical wash (.scene-fog in style.css).</summary>
        public static Texture2D FogTexture()
        {
            if (_fog != null) return _fog;
            const int w = 256, h = 144;
            var c = new PixelCanvas(w, h);
            c.FillVertical(0, 0, w, h, (0, new Color32(150, 165, 190, 217)), (1, new Color32(120, 135, 165, 235)));
            Blob(c, 0.30f, 0.40f, 0.45f, new Color32(180, 190, 210, 230));
            Blob(c, 0.70f, 0.60f, 0.45f, new Color32(160, 175, 200, 230));
            Blob(c, 0.50f, 0.50f, 0.60f, new Color32(200, 205, 225, 242));
            return _fog = c.ToTexture(null, FilterMode.Bilinear);
        }

        // CSS "radial-gradient(circle at x y, col, transparent R%)": R is a fraction of the
        // gradient ray to the farthest corner.
        static void Blob(PixelCanvas c, float fx, float fy, float stop, Color32 col)
        {
            float cx = fx * c.Width, cy = fy * c.Height;
            float far = Mathf.Sqrt(Mathf.Pow(Mathf.Max(cx, c.Width - cx), 2) + Mathf.Pow(Mathf.Max(cy, c.Height - cy), 2));
            var clear = new Color32(col.r, col.g, col.b, 0);
            c.FillRadial(0, 0, c.Width, c.Height, cx, cy, far * stop, col, clear);
        }

        /// <summary>The shop panel background: transparent at the top, 92% dark from 30% down.</summary>
        public static Texture2D ShopFadeTexture()
        {
            if (_shopFade != null) return _shopFade;
            var c = new PixelCanvas(1, 64);
            c.FillVertical(0, 0, 1, 64, (0, new Color32(8, 6, 14, 0)), (0.3f, new Color32(8, 6, 14, 235)), (1, new Color32(8, 6, 14, 235)));
            return _shopFade = c.ToTexture(null, FilterMode.Bilinear);
        }

        /// <summary>The Loom canvas's checkerboard (repeating-conic-gradient, 24px cells of 12px squares).</summary>
        public static Texture2D CheckerTexture()
        {
            if (_checker != null) return _checker;
            var c = new PixelCanvas(512, 512);
            var a = PixelCanvas.Col("#1a1622");
            var b = PixelCanvas.Col("#221c2e");
            for (int y = 0; y < 512; y++)
                for (int x = 0; x < 512; x++)
                    c.Pixels[y * 512 + x] = ((x / 12) + (y / 12)) % 2 == 0 ? b : a;
            return _checker = c.ToTexture();
        }
    }
}
