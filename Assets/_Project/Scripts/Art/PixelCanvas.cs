/*
 * PixelCanvas.cs
 * ----------------------------------------------------------------------------
 * A tiny software canvas that stands in for the HTML <canvas> the web game
 * drew its pixel art on. Rows are stored top-down (canvas order) and flipped
 * when converted to a Texture2D. It implements the 2D-context calls the
 * original art code used: fillRect with source-over blending, single pixels,
 * blits (with mirroring), plus gradients, ellipses, triangles and a tiny glyph
 * set for the procedural scene backdrops.
 *
 * Blend() uses the same arithmetic as the Node stub in Tools~/pixel-parity.mjs,
 * so sprite hashes can be compared between the JS original and this port.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace SpiceTrip.Art
{
    public sealed class PixelCanvas
    {
        public readonly int Width, Height;
        public readonly Color32[] Pixels;     // row-major, y = 0 is the TOP row

        public PixelCanvas(int width, int height)
        {
            Width = width; Height = height;
            Pixels = new Color32[width * height];
        }

        public void Clear() => Array.Clear(Pixels, 0, Pixels.Length);

        // --- colours -----------------------------------------------------------
        static readonly Dictionary<string, Color32?> ColorCache = new Dictionary<string, Color32?>();

        /// <summary>Parses #rgb, #rrggbb, #rrggbbaa, rgb() and rgba(); null if invalid (canvas ignores it).</summary>
        public static Color32? ParseColor(string s)
        {
            if (s == null) return null;
            if (ColorCache.TryGetValue(s, out var cached)) return cached;
            Color32? c = null;
            var t = s.Trim();
            if (t.StartsWith("#"))
            {
                var h = t.Substring(1);
                if (h.Length == 3) c = new Color32(Hex(h[0], h[0]), Hex(h[1], h[1]), Hex(h[2], h[2]), 255);
                else if (h.Length == 6) c = new Color32(Hex(h[0], h[1]), Hex(h[2], h[3]), Hex(h[4], h[5]), 255);
                else if (h.Length == 8) c = new Color32(Hex(h[0], h[1]), Hex(h[2], h[3]), Hex(h[4], h[5]), Hex(h[6], h[7]));
            }
            else if (t.StartsWith("rgb"))
            {
                int open = t.IndexOf('('), close = t.LastIndexOf(')');
                if (open > 0 && close > open)
                {
                    var parts = t.Substring(open + 1, close - open - 1).Split(',');
                    if (parts.Length >= 3
                        && int.TryParse(parts[0].Trim(), out var r)
                        && int.TryParse(parts[1].Trim(), out var g)
                        && int.TryParse(parts[2].Trim(), out var b)
                        && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
                    {
                        byte a = 255;
                        if (parts.Length >= 4 && float.TryParse(parts[3].Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var af))
                            a = (byte)Mathf.Clamp(Mathf.RoundToInt(af * 255f), 0, 255);
                        c = new Color32((byte)r, (byte)g, (byte)b, a);
                    }
                }
            }
            ColorCache[s] = c;
            return c;
        }

        public static Color32 Col(string s) => ParseColor(s) ?? new Color32(0, 0, 0, 255);

        static byte Hex(char a, char b) => (byte)(HexDigit(a) * 16 + HexDigit(b));
        static int HexDigit(char c) => c <= '9' ? c - '0' : (char.ToLowerInvariant(c) - 'a' + 10);

        /// <summary>The web game's shadeLighten: adds amt to each channel, caps at 255 (no floor, like the JS).</summary>
        public static string ShadeLighten(string hex, int amt)
        {
            int n = int.Parse(hex.Substring(1), NumberStyles.HexNumber);
            int r = Math.Min(255, (n >> 16) + amt);
            int g = Math.Min(255, ((n >> 8) & 255) + amt);
            int b = Math.Min(255, (n & 255) + amt);
            return $"rgb({r},{g},{b})";
        }

        // --- blending ------------------------------------------------------------
        public void Blend(int i, Color32 c)
        {
            double sa = c.a / 255.0;
            if (sa >= 1) { Pixels[i] = new Color32(c.r, c.g, c.b, 255); return; }
            if (sa <= 0) return;
            var d = Pixels[i];
            double da = d.a / 255.0;
            double oa = sa + da * (1 - sa);
            byte Mix(byte sc, byte dc) => (byte)JsRound((sc * sa + dc * da * (1 - sa)) / oa);
            Pixels[i] = new Color32(Mix(c.r, d.r), Mix(c.g, d.g), Mix(c.b, d.b), (byte)JsRound(oa * 255));
        }

        /// <summary>JavaScript Math.round (rounds .5 up, toward +∞).</summary>
        public static int JsRound(double v) => (int)Math.Floor(v + 0.5);

        // --- canvas primitives -------------------------------------------------------
        /// <summary>ctx.fillRect with integer-aligned edges (rounded like the parity stub).</summary>
        public void FillRect(double x, double y, double w, double h, Color32 c)
        {
            int x0 = Math.Max(0, JsRound(x)), y0 = Math.Max(0, JsRound(y));
            int x1 = Math.Min(Width, JsRound(x + w)), y1 = Math.Min(Height, JsRound(y + h));
            for (int yy = y0; yy < y1; yy++)
            {
                int row = yy * Width;
                for (int xx = x0; xx < x1; xx++) Blend(row + xx, c);
            }
        }

        public void FillRect(double x, double y, double w, double h, string col)
        {
            var c = ParseColor(col);
            if (c.HasValue) FillRect(x, y, w, h, c.Value);
        }

        /// <summary>The art code's R(): truncates to ints and never draws narrower than 1px.</summary>
        public void R(double x, double y, double w, double h, string col)
        {
            FillRect(Trunc(x), Trunc(y), Math.Max(1, Trunc(w)), Math.Max(1, Trunc(h)), col);
        }

        /// <summary>The art code's px(): one pixel.</summary>
        public void Px(double x, double y, string col) => FillRect(Trunc(x), Trunc(y), 1, 1, col);

        /// <summary>JavaScript `v | 0` (truncation toward zero, wrapped to int32).</summary>
        public static int Trunc(double v) => unchecked((int)(long)v);

        /// <summary>ctx.drawImage(src, x, y), optionally mirrored horizontally inside this canvas.</summary>
        public void Draw(PixelCanvas src, int dx, int dy, bool mirror = false, float alpha = 1f)
        {
            for (int y = 0; y < src.Height; y++)
            {
                int ty = dy + y;
                if (ty < 0 || ty >= Height) continue;
                for (int x = 0; x < src.Width; x++)
                {
                    int tx = mirror ? Width - (dx + x) - 1 : dx + x;
                    if (tx < 0 || tx >= Width) continue;
                    var c = src.Pixels[y * src.Width + x];
                    if (alpha < 1f) c.a = (byte)(c.a * alpha);
                    Blend(ty * Width + tx, c);
                }
            }
        }

        // --- extras for the scene backdrops ------------------------------------------
        /// <summary>fillRect with a two-stop radial gradient centred at (cx, cy).</summary>
        public void FillRadial(double x, double y, double w, double h, double cx, double cy, double radius, Color32 inner, Color32 outer)
        {
            int x0 = Math.Max(0, (int)Math.Floor(x)), y0 = Math.Max(0, (int)Math.Floor(y));
            int x1 = Math.Min(Width, (int)Math.Ceiling(x + w)), y1 = Math.Min(Height, (int)Math.Ceiling(y + h));
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    double d = Math.Sqrt((xx + 0.5 - cx) * (xx + 0.5 - cx) + (yy + 0.5 - cy) * (yy + 0.5 - cy)) / radius;
                    Blend(yy * Width + xx, Lerp(inner, outer, (float)Math.Min(1, d)));
                }
        }

        /// <summary>fillRect with a vertical gradient through the given stops (positions 0..1 of the canvas height).</summary>
        public void FillVertical(double x, double y, double w, double h, params (float pos, Color32 col)[] stops)
        {
            int x0 = Math.Max(0, (int)x), y0 = Math.Max(0, (int)y);
            int x1 = Math.Min(Width, (int)(x + w)), y1 = Math.Min(Height, (int)(y + h));
            for (int yy = y0; yy < y1; yy++)
            {
                float t = (yy + 0.5f) / Height;
                var c = Stop(stops, t);
                for (int xx = x0; xx < x1; xx++) Blend(yy * Width + xx, c);
            }
        }

        static Color32 Stop((float pos, Color32 col)[] stops, float t)
        {
            if (t <= stops[0].pos) return stops[0].col;
            for (int i = 1; i < stops.Length; i++)
                if (t <= stops[i].pos)
                    return Lerp(stops[i - 1].col, stops[i].col, (t - stops[i - 1].pos) / (stops[i].pos - stops[i - 1].pos));
            return stops[stops.Length - 1].col;
        }

        public static Color32 Lerp(Color32 a, Color32 b, float t) => Color32.Lerp(a, b, t);

        public void StrokeEllipse(double cx, double cy, double rx, double ry, Color32 c)
        {
            int steps = (int)Math.Max(24, (rx + ry) * 4);
            var seen = new HashSet<int>();
            for (int i = 0; i < steps; i++)
            {
                double a = i * Math.PI * 2 / steps;
                int x = (int)Math.Round(cx + Math.Cos(a) * rx), y = (int)Math.Round(cy + Math.Sin(a) * ry);
                if (x < 0 || y < 0 || x >= Width || y >= Height) continue;
                int idx = y * Width + x;
                if (seen.Add(idx)) Blend(idx, c);
            }
        }

        public void FillCircle(double cx, double cy, double r, Color32 c) => FillEllipse(cx, cy, r, r, c);

        public void FillEllipse(double cx, double cy, double rx, double ry, Color32 c)
        {
            int x0 = Math.Max(0, (int)Math.Floor(cx - rx)), x1 = Math.Min(Width - 1, (int)Math.Ceiling(cx + rx));
            int y0 = Math.Max(0, (int)Math.Floor(cy - ry)), y1 = Math.Min(Height - 1, (int)Math.Ceiling(cy + ry));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++)
                {
                    double dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry;
                    if (dx * dx + dy * dy <= 1) Blend(y * Width + x, c);
                }
        }

        public void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color32 col)
        {
            int x0 = Math.Max(0, (int)Math.Floor(Mathf.Min(a.x, Mathf.Min(b.x, c.x))));
            int x1 = Math.Min(Width - 1, (int)Math.Ceiling(Mathf.Max(a.x, Mathf.Max(b.x, c.x))));
            int y0 = Math.Max(0, (int)Math.Floor(Mathf.Min(a.y, Mathf.Min(b.y, c.y))));
            int y1 = Math.Min(Height - 1, (int)Math.Ceiling(Mathf.Max(a.y, Mathf.Max(b.y, c.y))));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++)
                {
                    var p = new Vector2(x + 0.5f, y + 0.5f);
                    float d1 = Edge(p, a, b), d2 = Edge(p, b, c), d3 = Edge(p, c, a);
                    bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
                    if (!(neg && pos)) Blend(y * Width + x, col);
                }
        }

        static float Edge(Vector2 p, Vector2 a, Vector2 b) => (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);

        /// <summary>Draws text with a built-in 3×5 pixel font (A–Z, digits, a few runes).</summary>
        public void Text(string s, int x, int y, Color32 c, bool center = false)
        {
            if (center) x -= (s.Length * 4 - 1) / 2;
            foreach (char ch in s)
            {
                if (PixelFont.Glyphs.TryGetValue(char.ToUpperInvariant(ch), out var rows))
                    for (int ry = 0; ry < rows.Length; ry++)
                        for (int rx = 0; rx < rows[ry].Length; rx++)
                            if (rows[ry][rx] == '#') FillRect(x + rx, y + ry, 1, 1, c);
                x += 4;
            }
        }

        // --- export ------------------------------------------------------------------
        public byte[] ToRgbaBytes()
        {
            var bytes = new byte[Pixels.Length * 4];
            for (int i = 0; i < Pixels.Length; i++)
            {
                bytes[i * 4] = Pixels[i].r; bytes[i * 4 + 1] = Pixels[i].g;
                bytes[i * 4 + 2] = Pixels[i].b; bytes[i * 4 + 3] = Pixels[i].a;
            }
            return bytes;
        }

        /// <summary>"WxH:hash" fingerprint, same format as Tools~/pixel-parity.mjs.</summary>
        public string Fingerprint() => $"{Width}x{Height}:{Core.Hash.Bytes(ToRgbaBytes()):x8}";

        /// <summary>Copies into a point-filtered texture (rows flipped to Unity's bottom-up order).</summary>
        public Texture2D ToTexture(Texture2D reuse = null, FilterMode filter = FilterMode.Point)
        {
            var tex = reuse;
            if (tex == null || tex.width != Width || tex.height != Height)
            {
                tex = new Texture2D(Width, Height, TextureFormat.RGBA32, false)
                {
                    filterMode = filter,
                    wrapMode = TextureWrapMode.Clamp,
                    hideFlags = HideFlags.DontSave,
                };
            }
            var flipped = new Color32[Pixels.Length];
            for (int y = 0; y < Height; y++)
                Array.Copy(Pixels, y * Width, flipped, (Height - 1 - y) * Width, Width);
            tex.SetPixels32(flipped);
            tex.Apply(false);
            return tex;
        }
    }
}
