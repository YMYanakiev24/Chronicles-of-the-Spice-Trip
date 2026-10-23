/*
 * IntroCinematic.cs
 * ----------------------------------------------------------------------------
 * Port of IntroScene.js: a moonlit, fog-drowned valley with the Pale Citadel
 * burning cold light on a black hill, a dark forest silhouette in front and
 * drifting embers, painted as pixel art. The title card (title, subtitle,
 * prophecy, "click to begin") sits on top. Skippable on any click or key;
 * advances on its own after 26 seconds.
 *
 * The web build skips this at boot. Here it plays when a new Chronicle starts.
 * The static layers (sky, hill and citadel, canopy, vignette) are painted
 * once per size; only stars, windows, fog and embers are redrawn each frame.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Rpg
{
    public class IntroCinematic
    {
        public readonly VisualElement Root;
        readonly VisualElement _scene;
        float _t;
        Action _onDone;
        bool _done = true;

        int _w, _h;
        PixelCanvas _frame, _sky, _hill, _canopy;
        float[] _vignette;
        Texture2D _tex;
        struct Star { public float x, y, b, tw; }
        struct Win { public float x, y, p; }
        struct Ember { public float x, y, sp, ph; }
        Star[] _stars;
        Win[] _windows;
        Ember[] _embers;

        public bool Playing => !_done;

        public IntroCinematic(GameContext ctx, VisualElement layer)
        {
            Root = El("intro hidden", "intro");
            _scene = El("intro-scene");
            Root.Add(_scene);
            var card = El("title-card");
            card.pickingMode = PickingMode.Ignore;
            card.Add(Lbl("The Mythic Chronicles", "game-title"));
            card.Add(Lbl("The Hidden World", "game-subtitle"));
            var prophecy = El("prophecy");
            foreach (var line in ctx.Db.prophecy) prophecy.Add(Lbl(line, "prophecy-line"));
            card.Add(prophecy);
            card.Add(Lbl("click to begin", "title-hint"));
            Root.Add(card);
            Root.RegisterCallback<PointerDownEvent>(_ => Finish());
            layer.Add(Root);
        }

        public void Play(int w, int h, Action onDone)
        {
            _onDone = onDone;
            _t = 0;
            _done = false;
            if (w != _w || h != _h) Build(w, h);
            Show(Root, true);
            Draw();
        }

        public void Tick(float dt, bool anyInput)
        {
            if (_done) return;
            _t += dt;
            if (anyInput || _t > 26) { Finish(); return; }   // fallback auto-advance
            Draw();
        }

        public void Finish()
        {
            if (_done) return;
            _done = true;
            Show(Root, false);
            _onDone?.Invoke();
        }

        void Build(int w, int h)
        {
            _w = w; _h = h;
            var rng = new Mulberry32(20260614);
            float R() => (float)rng.Next();
            _stars = new Star[90];
            for (int i = 0; i < 90; i++) _stars[i] = new Star { x = R() * w, y = R() * h * 0.55f, b = R(), tw = R() * 7 };
            _windows = new Win[26];
            for (int i = 0; i < 26; i++) _windows[i] = new Win { x = R(), y = R(), p = R() * 7 };
            var canopy = new List<float>();
            for (int x = -2; x <= w + 2; x += 4) canopy.Add(h - 26 - R() * 26 - (Mathf.Sin(x * 0.05f) + 1) * 8);
            _embers = new Ember[40];
            for (int i = 0; i < 40; i++) _embers[i] = new Ember { x = R() * w, y = R() * h, sp = 4 + R() * 10, ph = R() * 7 };

            // Sky gradient + moon with glow and craters.
            _sky = new PixelCanvas(w, h);
            _sky.FillVertical(0, 0, w, h, (0, PixelCanvas.Col("#070611")), (0.55f, PixelCanvas.Col("#120e26")), (1, PixelCanvas.Col("#1a1430")));
            float mx = w * 0.74f, my = h * 0.2f, mr = 16;
            _sky.FillRadial(mx - mr * 4, my - mr * 4, mr * 8, mr * 8, mx, my, mr * 4, new Color32(210, 220, 255, 128), new Color32(210, 220, 255, 0));
            _sky.FillCircle(mx, my, mr, PixelCanvas.Col("#dfe6ff"));
            _sky.FillCircle(mx - 5, my - 3, 3, PixelCanvas.Col("#c3cbe8"));
            _sky.FillCircle(mx + 4, my + 4, 2, PixelCanvas.Col("#c3cbe8"));

            // Distant hill rising to the right, crowned by the citadel.
            float hy = h * 0.6f;
            _hill = new PixelCanvas(w, h);
            var hill = new List<Vector2> { new Vector2(0, h), new Vector2(0, hy + 30) };
            Quad(hill, new Vector2(0, hy + 30), new Vector2(w * 0.55f, hy + 6), new Vector2(w * 0.74f, hy - 46));
            Quad(hill, new Vector2(w * 0.74f, hy - 46), new Vector2(w * 0.9f, hy - 10), new Vector2(w, hy + 6));
            hill.Add(new Vector2(w, h));
            _hill.FillPolygon(hill, PixelCanvas.Col("#0c0a16"));
            Citadel(_hill, w * 0.74f, hy - 36);

            // Foreground canopy + framing dead trees.
            _canopy = new PixelCanvas(w, h);
            var poly = new List<Vector2> { new Vector2(0, h) };
            for (int i = 0; i < canopy.Count; i++) poly.Add(new Vector2(i * 4 - 2, canopy[i]));
            poly.Add(new Vector2(w, h));
            _canopy.FillPolygon(poly, PixelCanvas.Col("#040308"));
            DeadTree(_canopy, w * 0.12f, h, 1);
            DeadTree(_canopy, w * 0.88f, h, 1.1f);

            // Vignette (0 at 0.3h → 0.7 black at 0.8h).
            _vignette = new float[w * h];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    float d = Mathf.Sqrt((x + 0.5f - w / 2f) * (x + 0.5f - w / 2f) + (y + 0.5f - h / 2f) * (y + 0.5f - h / 2f));
                    _vignette[y * w + x] = Mathf.Clamp01((d - h * 0.3f) / (h * 0.5f)) * 0.7f;
                }
            _frame = new PixelCanvas(w, h);
        }

        static void Quad(List<Vector2> pts, Vector2 a, Vector2 c, Vector2 b)
        {
            for (int i = 1; i <= 16; i++)
            {
                float t = i / 16f;
                pts.Add((1 - t) * (1 - t) * a + 2 * (1 - t) * t * c + t * t * b);
            }
        }

        static void Citadel(PixelCanvas c, float cx, float b)
        {
            var lt = PixelCanvas.Col("#262a3c");
            var dk = PixelCanvas.Col("#10111c");
            c.FillRect(cx - 16, b - 46, 32, 46, lt);          // central keep
            c.FillRect(cx - 16, b - 46, 4, 46, dk);
            c.FillRect(cx - 30, b - 34, 12, 34, lt);          // side towers
            c.FillRect(cx + 18, b - 34, 12, 34, lt);
            for (int x = -16; x < 16; x += 6) c.FillRect(cx + x, b - 50, 4, 5, lt);   // battlements
            void Tri(float x, float y, float w, float h) => c.FillTriangle(new Vector2(x - w / 2, y), new Vector2(x + w / 2, y), new Vector2(x, y - h), dk);
            Tri(cx - 24, b - 34, 12, 10);                       // spires
            Tri(cx + 24, b - 34, 12, 10);
            Tri(cx, b - 50, 22, 14);
        }

        static void DeadTree(PixelCanvas c, float x, float b, float s)
        {
            var k = PixelCanvas.Col("#040308");
            c.FillRect(x - 2 * s, b - 60 * s, 4 * s, 60 * s, k);
            c.FillRect(x - 12 * s, b - 50 * s, 12 * s, 3 * s, k);
            c.FillRect(x + 2 * s, b - 44 * s, 12 * s, 3 * s, k);
            c.FillRect(x - 14 * s, b - 56 * s, 3 * s, 8 * s, k);
            c.FillRect(x + 12 * s, b - 50 * s, 3 * s, 8 * s, k);
        }

        void Draw()
        {
            int w = _w, h = _h;
            float t = _t;
            var f = _frame;
            Array.Copy(_sky.Pixels, f.Pixels, f.Pixels.Length);

            foreach (var s in _stars)
            {
                float a = 0.3f + Mathf.Abs(Mathf.Sin(t * 0.7f + s.tw)) * 0.6f * s.b;
                f.FillRect((int)s.x, (int)s.y, 1, 1, new Color32(200, 210, 255, (byte)(Mathf.Clamp01(a) * 255)));
            }
            f.Draw(_hill, 0, 0);

            // glowing citadel windows (flicker) + the overall cold glow
            float cx = w * 0.74f, b = h * 0.6f - 36;
            foreach (var wn in _windows)
            {
                float fl = 0.55f + Mathf.Sin(t * 4 + wn.p) * 0.35f;
                f.FillRect((int)(cx - 26 + wn.x * 52), (int)(b - 44 + wn.y * 40), 2, 3, new Color32(180, 200, 255, (byte)(fl * 255)));
            }
            Additive(f, cx, b - 30, 60, new Color(120 / 255f, 150 / 255f, 1, 0.12f));

            // drifting fog bands over the valley (screen blend)
            float hy = h * 0.6f;
            for (int i = 0; i < 4; i++)
            {
                float fy = hy + 6 + i * 12;
                float off = (t * (6 + i * 3)) % (w + 80) - 40;
                float alpha = 0.05f + i * 0.015f;
                for (int x = -40; x < w; x += 60)
                {
                    float fx = x + off + Mathf.Sin(t * 0.5f + i + x) * 8;
                    ScreenEllipse(f, fx, fy, 40, 6, new Color(150 / 255f, 165 / 255f, 185 / 255f, alpha));
                }
            }

            f.Draw(_canopy, 0, 0);

            // rising embers / fireflies (additive)
            foreach (var e in _embers)
            {
                float ey = (e.y - t * e.sp) % h; float yy = ey < 0 ? ey + h : ey;
                float xx = e.x + Mathf.Sin(t * 0.8f + e.ph) * 6;
                float a = 0.3f + Mathf.Abs(Mathf.Sin(t * 2 + e.ph)) * 0.5f;
                AddPixel(f, (int)xx, (int)yy, new Color(1, 190 / 255f, 120 / 255f, a));
            }

            // vignette
            for (int i = 0; i < f.Pixels.Length; i++)
            {
                float k = 1 - _vignette[i];
                var p = f.Pixels[i];
                f.Pixels[i] = new Color32((byte)(p.r * k), (byte)(p.g * k), (byte)(p.b * k), 255);
            }

            bool first = _tex == null || _tex.width != w;
            _tex = f.ToTexture(first ? null : _tex);
            if (first) SetBackground(_scene, _tex);
            else _scene.MarkDirtyRepaint();
        }

        static void AddPixel(PixelCanvas f, int x, int y, Color c)
        {
            if (x < 0 || y < 0 || x >= f.Width || y >= f.Height) return;
            int i = y * f.Width + x;
            var p = f.Pixels[i];
            f.Pixels[i] = new Color32((byte)Mathf.Min(255, p.r + c.r * c.a * 255), (byte)Mathf.Min(255, p.g + c.g * c.a * 255), (byte)Mathf.Min(255, p.b + c.b * c.a * 255), 255);
        }

        static void Additive(PixelCanvas f, float cx, float cy, float r, Color c)
        {
            int x0 = Mathf.Max(0, (int)(cx - r)), x1 = Mathf.Min(f.Width - 1, (int)(cx + r));
            int y0 = Mathf.Max(0, (int)(cy - r)), y1 = Mathf.Min(f.Height - 1, (int)(cy + r));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++)
                {
                    float d = Mathf.Sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / r;
                    if (d < 1) AddPixel(f, x, y, new Color(c.r, c.g, c.b, c.a * (1 - d)));
                }
        }

        static void ScreenEllipse(PixelCanvas f, float cx, float cy, float rx, float ry, Color c)
        {
            int x0 = Mathf.Max(0, (int)(cx - rx)), x1 = Mathf.Min(f.Width - 1, (int)(cx + rx));
            int y0 = Mathf.Max(0, (int)(cy - ry)), y1 = Mathf.Min(f.Height - 1, (int)(cy + ry));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++)
                {
                    float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                    if (dx * dx + dy * dy > 1) continue;
                    int i = y * f.Width + x;
                    var p = f.Pixels[i];
                    byte S(byte d, float s) => (byte)(255 - (255 - d) * (1 - s * c.a));
                    f.Pixels[i] = new Color32(S(p.r, c.r), S(p.g, c.g), S(p.b, c.b), 255);
                }
        }
    }
}
