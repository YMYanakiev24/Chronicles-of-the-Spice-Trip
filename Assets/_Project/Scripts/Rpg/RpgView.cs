/*
 * RpgView.cs
 * ----------------------------------------------------------------------------
 * Draws a Chronicle the way Game._render / TileMap.drawLighting did, with
 * pooled SpriteRenderers standing in for canvas calls. Every draw call gets
 * the next sorting order, so the paint order is exactly the original's:
 *
 *   ground → pickups → y-sorted props/entities → spells → fog (screen) →
 *   darkness with carved light pools → additive glows → vignette
 *
 * Pixel scale and the virtual screen follow Game._resize: an integer scale
 * from the window height (BASE_VH 270, clamped 2–5) and a virtual buffer that
 * fills the window at that scale. 1 world unit = 1 world pixel; y is flipped.
 *
 * The darkness layer is computed on the CPU at half the virtual resolution
 * with the original formula — rgba(5,6,14,darkness), each light multiplying
 * the alpha by (1 − gradient), the gradient running 1 → 0.7 at 60% → 0 — and
 * drawn bilinear-filtered over the scene.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    public class RpgView : MonoBehaviour, IWorldPainter
    {
        const int BaseVH = 270;

        ChronicleGame _g;
        Camera _cam;
        Transform _root;
        Material _alpha, _additive, _screen;
        Sprite _white, _whiteLine, _glow;
        readonly Dictionary<PixelCanvas, Sprite> _sprites = new Dictionary<PixelCanvas, Sprite>();
        readonly Dictionary<PixelCanvas, Sprite> _centred = new Dictionary<PixelCanvas, Sprite>();
        readonly Dictionary<string, PixelCanvas> _glyphs = new Dictionary<string, PixelCanvas>();
        readonly List<SpriteRenderer> _pool = new List<SpriteRenderer>();
        int _used;
        short _order;

        SpriteRenderer _ground;
        int _groundVersion = -1;
        PixelCanvas _fogTex;
        Texture2D _darkTex, _vignetteTex;
        Sprite _darkSprite, _vignetteSprite;
        Color32[] _darkPx;
        float[] _darkF;
        int _maskW, _maskH;
        Vector2Int _vignetteFor;
        readonly List<DynLight> _lights = new List<DynLight>();

        public int PixelScale { get; private set; } = 2;
        public float ViewW { get; private set; } = 480;
        public float ViewH { get; private set; } = 270;
        public float ShakeX, ShakeY;   // screen-pixel offset (screen shake)
        public Camera Camera => _cam;

        public void Init(ChronicleGame game, Material alpha, Material additive, Material screen)
        {
            _g = game;
            _alpha = alpha; _additive = additive; _screen = screen;
            _root = new GameObject("ChronicleWorld").transform;
            _root.SetParent(transform, false);

            var camGo = new GameObject("ChronicleCamera");
            camGo.transform.SetParent(transform, false);
            _cam = camGo.AddComponent<Camera>();
            _cam.orthographic = true;
            _cam.clearFlags = CameraClearFlags.SolidColor;
            _cam.depth = 1;
            _cam.nearClipPlane = 0.1f;
            _cam.farClipPlane = 100;
            _cam.enabled = false;

            _white = MakeSprite(Solid(1, 1), new Vector2(0, 1));
            _whiteLine = MakeSprite(Solid(1, 1), new Vector2(0, 0.5f));
            var glow = new PixelCanvas(64, 64);
            glow.FillRadial(0, 0, 64, 64, 32, 32, 32, new Color32(255, 255, 255, 255), new Color32(255, 255, 255, 0));
            _glow = MakeSprite(glow, new Vector2(0.5f, 0.5f), FilterMode.Bilinear);
            _fogTex = BuildFogTexture();
            _ground = NewRenderer("Ground");
            _ground.sortingOrder = short.MinValue;
            Show(false);
        }

        public void Show(bool on)
        {
            _root.gameObject.SetActive(on);
            _cam.enabled = on;
        }

        static PixelCanvas Solid(int w, int h)
        {
            var c = new PixelCanvas(w, h);
            c.FillRect(0, 0, w, h, new Color32(255, 255, 255, 255));
            return c;
        }

        static Sprite MakeSprite(PixelCanvas c, Vector2 pivot, FilterMode filter = FilterMode.Point)
        {
            var t = c.ToTexture(null, filter);
            return UnityEngine.Sprite.Create(t, new Rect(0, 0, t.width, t.height), pivot, 1, 0, SpriteMeshType.FullRect);
        }

        // The web fog texture: 40 soft grey radial blobs on a 128px tile (seed 777).
        static PixelCanvas BuildFogTexture()
        {
            const int size = 128;
            var c = new PixelCanvas(size, size);
            var rng = new Mulberry32(777);
            for (int i = 0; i < 40; i++)
            {
                double x = rng.Next() * size, y = rng.Next() * size, r = 12 + rng.Next() * 28;
                byte v = (byte)(90 + PixelCanvas.Trunc(rng.Next() * 80));
                c.FillRadial(x - r, y - r, r * 2, r * 2, x, y, r, new Color32(v, v, v, 128), new Color32(0, 0, 0, 0));
            }
            return c;
        }

        // =========================================================================
        // Frame
        // =========================================================================
        /// <summary>Recomputes the pixel scale and virtual screen for the window (Game._resize).</summary>
        public void Resize(int screenW, int screenH)
        {
            PixelScale = Mathf.Max(2, Mathf.Min(5, Mathf.RoundToInt(screenH / (float)BaseVH)));
            ViewW = Mathf.Max(200, Mathf.Ceil(screenW / (float)PixelScale));
            ViewH = Mathf.Max(140, Mathf.Ceil(screenH / (float)PixelScale));
        }

        /// <summary>
        /// The real size of the frame being drawn. In the editor on a high-DPI display
        /// Screen.width/height can report a scaled size; the camera knows the true one.
        /// </summary>
        public int RenderWidth => _cam != null && _cam.pixelWidth > 0 ? _cam.pixelWidth : Screen.width;
        public int RenderHeight => _cam != null && _cam.pixelHeight > 0 ? _cam.pixelHeight : Screen.height;

        public Vector2 ScreenToWorld(Vector2 screenTopLeft) =>
            new Vector2(_g.CamX + screenTopLeft.x * ViewW / Screen.width, _g.CamY + screenTopLeft.y * ViewH / Screen.height);

        public void Render()
        {
            if (_g?.Player == null) return;
            _used = 0;
            _order = (short)(short.MinValue + 10);
            var region = _g.Map.Region;

            _cam.backgroundColor = Css.C(region.ground.@base);
            _cam.orthographicSize = ViewH / 2;
            float sx = ShakeX * ViewW / Mathf.Max(1, Screen.width), sy = ShakeY * ViewH / Mathf.Max(1, Screen.height);
            _cam.transform.position = new Vector3(_g.CamX + ViewW / 2 + sx, -(_g.CamY + ViewH / 2 + sy), -10);

            if (_groundVersion != _g.Map.Version) BakeGround();

            _g.RenderWorld(this, _lights);
            DrawLighting(region);

            for (int i = _used; i < _pool.Count; i++) _pool[i].enabled = false;
        }

        void BakeGround()
        {
            var m = _g.Map;
            var c = new PixelCanvas(m.PixelW, m.PixelH);
            for (int ty = 0; ty < m.H; ty++)
                for (int tx = 0; tx < m.W; tx++)
                    c.Draw(m.CellAt(tx, ty).tile, tx * TileMap.TS, ty * TileMap.TS);
            if (_ground.sprite != null) { ArtLibrary.SafeDestroy(_ground.sprite.texture); ArtLibrary.SafeDestroy(_ground.sprite); }
            _ground.sprite = MakeSprite(c, new Vector2(0, 1));
            _ground.sharedMaterial = _alpha;
            _ground.transform.localPosition = Vector3.zero;
            _ground.transform.localScale = Vector3.one;
            _groundVersion = m.Version;
        }

        // --- The lighting + fog pass (TileMap.drawLighting) ---------------------------
        void DrawLighting(Data.RegionDef region)
        {
            float vw = ViewW, vh = ViewH, camX = _g.CamX, camY = _g.CamY;
            DrawFog(region);

            // Darkness, with light pools carved out of it.
            int mw = Mathf.CeilToInt(vw / 2), mh = Mathf.CeilToInt(vh / 2);
            if (_darkTex == null || mw != _maskW || mh != _maskH)
            {
                _maskW = mw; _maskH = mh;
                if (_darkTex != null) { ArtLibrary.SafeDestroy(_darkSprite); ArtLibrary.SafeDestroy(_darkTex); }
                _darkTex = new Texture2D(mw, mh, TextureFormat.RGBA32, false) { filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp };
                _darkSprite = UnityEngine.Sprite.Create(_darkTex, new Rect(0, 0, mw, mh), new Vector2(0, 1), 1, 0, SpriteMeshType.FullRect);
                _darkPx = new Color32[mw * mh];
                _darkF = new float[mw * mh];
            }
            for (int i = 0; i < _darkF.Length; i++) _darkF[i] = 1;
            foreach (var l in _g.Map.Lights)
            {
                float lx = l.x - camX, ly = l.y - camY;
                if (lx < -120 || ly < -120 || lx > vw + 120 || ly > vh + 120) continue;
                Carve(lx, ly, _g.Map.LightRadius(l));
            }
            foreach (var l in _lights) Carve(l.x - camX, l.y - camY, l.r);
            byte maxA = (byte)Mathf.RoundToInt(region.darkness * 255);
            for (int y = 0; y < mh; y++)
            {
                int src = y * mw, dst = (mh - 1 - y) * mw;   // texture rows run bottom-up
                for (int x = 0; x < mw; x++)
                    _darkPx[dst + x] = new Color32(5, 6, 14, (byte)(maxA * _darkF[src + x]));
            }
            _darkTex.SetPixels32(_darkPx);
            _darkTex.Apply(false);
            var dark = Next(_darkSprite, _alpha, Color.white);
            dark.transform.localPosition = new Vector3(camX, -camY, 0);
            dark.transform.localRotation = Quaternion.identity;
            dark.transform.localScale = new Vector3(vw / mw, vh / mh, 1);

            // Coloured glow on top (additive) for warmth/magic.
            foreach (var l in _g.Map.Lights)
            {
                float lx = l.x - camX, ly = l.y - camY;
                if (lx < -120 || ly < -120 || lx > vw + 120 || ly > vh + 120) continue;
                Glow(l.x, l.y, l.r * 0.9f, Css.A(l.color, 0.22f));
            }
            foreach (var l in _lights) Glow(l.x, l.y, l.r * 0.8f, Css.A(l.color ?? "#bfe0ff", l.intensity));

            // Vignette to frame the screen.
            var vs = new Vector2Int(Mathf.RoundToInt(vw), Mathf.RoundToInt(vh));
            if (_vignetteTex == null || vs != _vignetteFor) BuildVignette(vs);
            var vg = Next(_vignetteSprite, _alpha, Color.white);
            vg.transform.localPosition = new Vector3(camX, -camY, 0);
            vg.transform.localRotation = Quaternion.identity;
            vg.transform.localScale = new Vector3(vw / _vignetteTex.width, vh / _vignetteTex.height, 1);
        }

        // destination-out of a radial gradient: alpha *= 1 − g(d/r), g = 1 → 0.7 at 0.6 → 0 at 1
        void Carve(float sx, float sy, float r)
        {
            if (r <= 0) return;
            float cx = sx / 2, cy = sy / 2, rr = r / 2;
            int x0 = Mathf.Max(0, Mathf.FloorToInt(cx - rr)), x1 = Mathf.Min(_maskW - 1, Mathf.CeilToInt(cx + rr));
            int y0 = Mathf.Max(0, Mathf.FloorToInt(cy - rr)), y1 = Mathf.Min(_maskH - 1, Mathf.CeilToInt(cy + rr));
            for (int y = y0; y <= y1; y++)
            {
                float dy = y + 0.5f - cy;
                for (int x = x0; x <= x1; x++)
                {
                    float dx = x + 0.5f - cx;
                    float t = Mathf.Sqrt(dx * dx + dy * dy) / rr;
                    if (t >= 1) continue;
                    float g = t < 0.6f ? 1 - 0.3f * (t / 0.6f) : 0.7f * (1 - (t - 0.6f) / 0.4f);
                    _darkF[y * _maskW + x] *= 1 - g;
                }
            }
        }

        void DrawFog(Data.RegionDef region)
        {
            float vw = ViewW, vh = ViewH, camX = _g.CamX, camY = _g.CamY;
            float a = region.fog.density * 0.5f;
            float t = _g.Map.Time;
            const int S = 128;
            float ox1 = -JsMod(camX * 0.3f + t * 7, S), oy1 = -JsMod(camY * 0.3f, S);
            float ox2 = -JsMod(camX * 0.5f - t * 4, S), oy2 = -JsMod(camY * 0.5f + t * 2, S);
            for (float y = oy1; y < vh; y += S)
                for (float x = ox1; x < vw; x += S)
                    Sprite(_fogTex, camX + x, camY + y, a, Blend.Screen);
            for (float y = oy2; y < vh; y += S)
                for (float x = ox2; x < vw; x += S)
                    Sprite(_fogTex, camX + x, camY + y, a * 0.7f, Blend.Screen);
        }

        static float JsMod(float a, float b) => a - b * (float)Math.Truncate(a / b);

        void BuildVignette(Vector2Int size)
        {
            if (_vignetteTex != null) { ArtLibrary.SafeDestroy(_vignetteSprite); ArtLibrary.SafeDestroy(_vignetteTex); }
            int w = 192, h = Mathf.Max(32, Mathf.RoundToInt(192f * size.y / size.x));
            var px = new Color32[w * h];
            float vw = size.x, vh = size.y;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    float dx = (x + 0.5f) / w * vw - vw / 2, dy = (y + 0.5f) / h * vh - vh / 2;
                    float d = Mathf.Sqrt(dx * dx + dy * dy);
                    float k = Mathf.Clamp01((d - vh * 0.35f) / (vh * 0.4f));
                    px[y * w + x] = new Color32(0, 0, 0, (byte)(k * 0.55f * 255));
                }
            _vignetteTex = new Texture2D(w, h, TextureFormat.RGBA32, false) { filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp };
            _vignetteTex.SetPixels32(px);
            _vignetteTex.Apply(false);
            _vignetteSprite = UnityEngine.Sprite.Create(_vignetteTex, new Rect(0, 0, w, h), new Vector2(0, 1), 1, 0, SpriteMeshType.FullRect);
            _vignetteFor = size;
        }

        // =========================================================================
        // IWorldPainter
        // =========================================================================
        SpriteRenderer NewRenderer(string name)
        {
            var go = new GameObject(name);
            go.transform.SetParent(_root, false);
            return go.AddComponent<SpriteRenderer>();
        }

        SpriteRenderer Next(Sprite sprite, Material mat, Color color)
        {
            if (_used == _pool.Count) _pool.Add(NewRenderer("Draw"));
            var r = _pool[_used++];
            r.enabled = true;
            r.sprite = sprite;
            r.sharedMaterial = mat;
            r.color = color;
            r.sortingOrder = _order < short.MaxValue ? _order++ : short.MaxValue;
            return r;
        }

        Material Mat(Blend b) => b == Blend.Additive ? _additive : b == Blend.Screen ? _screen : _alpha;

        Sprite SpriteFor(PixelCanvas c, bool centred = false)
        {
            var map = centred ? _centred : _sprites;
            if (!map.TryGetValue(c, out var s) || s == null)
            {
                s = MakeSprite(c, centred ? new Vector2(0.5f, 0.5f) : new Vector2(0, 1));
                map[c] = s;
            }
            return s;
        }

        public void Sprite(PixelCanvas sprite, float x, float y, float alpha = 1, Blend blend = Blend.Alpha)
        {
            var r = Next(SpriteFor(sprite), Mat(blend), new Color(1, 1, 1, alpha));
            r.transform.localPosition = new Vector3(x, -y, 0);
            r.transform.localRotation = Quaternion.identity;
            r.transform.localScale = Vector3.one;
        }

        public void SpriteRotated(PixelCanvas sprite, float centreX, float centreY, float radians, float alpha = 1)
        {
            var r = Next(SpriteFor(sprite, true), _alpha, new Color(1, 1, 1, alpha));
            r.transform.localPosition = new Vector3(centreX, -centreY, 0);
            r.transform.localRotation = Quaternion.Euler(0, 0, -radians * Mathf.Rad2Deg);   // canvas rotates clockwise
            r.transform.localScale = Vector3.one;
        }

        public void Rect(float x, float y, float w, float h, Color color, Blend blend = Blend.Alpha)
        {
            if (w <= 0 || h <= 0) return;
            var r = Next(_white, Mat(blend), color);
            r.transform.localPosition = new Vector3(x, -y, 0);
            r.transform.localRotation = Quaternion.identity;
            r.transform.localScale = new Vector3(w, h, 1);
        }

        public void Line(float x1, float y1, float x2, float y2, float width, Color color, Blend blend = Blend.Alpha)
        {
            float dx = x2 - x1, dy = y2 - y1, len = Mathf.Sqrt(dx * dx + dy * dy);
            if (len < 0.01f) return;
            var r = Next(_whiteLine, Mat(blend), color);
            r.transform.localPosition = new Vector3(x1, -y1, 0);
            r.transform.localRotation = Quaternion.Euler(0, 0, Mathf.Atan2(-dy, dx) * Mathf.Rad2Deg);
            r.transform.localScale = new Vector3(len + width * 0.5f, width, 1);
        }

        public void Arc(float cx, float cy, float rad, float a0, float a1, float width, Color color, Blend blend = Blend.Alpha)
        {
            if (rad <= 0) return;
            float span = Mathf.Min(a1 - a0, Mathf.PI * 2);
            int segs = Mathf.Clamp(Mathf.CeilToInt(rad * span / 3), 6, 64);
            float px = cx + Mathf.Cos(a0) * rad, py = cy + Mathf.Sin(a0) * rad;
            for (int i = 1; i <= segs; i++)
            {
                float a = a0 + span * i / segs;
                float nx = cx + Mathf.Cos(a) * rad, ny = cy + Mathf.Sin(a) * rad;
                Line(px, py, nx, ny, width, color, blend);
                px = nx; py = ny;
            }
        }

        public void Glow(float cx, float cy, float rad, Color color, Blend blend = Blend.Additive)
        {
            if (rad <= 0 || color.a <= 0) return;
            var r = Next(_glow, Mat(blend), color);
            r.transform.localPosition = new Vector3(cx, -cy, 0);
            r.transform.localRotation = Quaternion.identity;
            float s = rad * 2 / 64f;
            r.transform.localScale = new Vector3(s, s, 1);
        }

        public void Glyph(string glyph, float cx, float baselineY, Color color)
        {
            var c = GlyphCanvas(glyph);
            if (c == null) return;
            var r = Next(SpriteFor(c), _alpha, color);
            r.transform.localPosition = new Vector3(Mathf.Round(cx - c.Width / 2f), -(Mathf.Round(baselineY) - c.Height), 0);
            r.transform.localRotation = Quaternion.identity;
            r.transform.localScale = Vector3.one;
        }

        // Quest markers (bold 11px monospace in the web version) as white pixel glyphs, tinted per call.
        PixelCanvas GlyphCanvas(string glyph)
        {
            if (_glyphs.TryGetValue(glyph, out var c)) return c;
            string[] rows = glyph switch
            {
                "!" => new[] { "##", "##", "##", "##", "##", "##", "..", "##", "##" },
                "?" => new[] { ".####.", "##..##", "....##", "...##.", "..##..", "..##..", "......", "..##..", "..##.." },
                "✔" => new[] { "........##", ".......##.", "......##..", "##..###...", "######....", ".####.....", "..##......" },
                _ => null,
            };
            if (rows == null) return null;
            c = new PixelCanvas(rows[0].Length, rows.Length);
            for (int y = 0; y < rows.Length; y++)
                for (int x = 0; x < rows[y].Length; x++)
                    if (rows[y][x] == '#') c.FillRect(x, y, 1, 1, new Color32(255, 255, 255, 255));
            _glyphs[glyph] = c;
            return c;
        }
    }
}
