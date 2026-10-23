/*
 * PixelArt.cs
 * ----------------------------------------------------------------------------
 * Line-by-line port of the web game's PixelArt.js. Generates every procedural
 * sprite as pixel art on a PixelCanvas — there are no image files:
 *   - terrain tiles (dithered ground + brick/rock walls) per region tileset,
 *   - gothic props (dead trees, broken pillars, statues, braziers, graves…),
 *   - animated characters (the hero, NPCs) and monsters, drawn from primitives
 *     with 2–3 frame walk/idle/flap cycles.
 *
 * Everything is cached so a sprite is only rasterised once. Output is checked
 * pixel-for-pixel against the JavaScript by the PixelArtParity tests.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Core;

namespace SpiceTrip.Art
{
    public static class PixelArt
    {
        public const int TS = 16;   // tile size in pixels

        static PixelCanvas FromGrid(string[] rows, Dictionary<char, string> pal)
        {
            int h = rows.Length, w = rows[0].Length;
            var c = new PixelCanvas(w, h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if (pal.TryGetValue(rows[y][x], out var col)) c.Px(x, y, col);
            return c;
        }

        // =====================================================================
        // TILE PALETTES — a few shades per region, gothic and desaturated.
        // =====================================================================
        sealed class TileTheme
        {
            public string[] floor, path, wall;
            public string style;
        }

        static readonly Dictionary<string, TileTheme> TileThemes = new Dictionary<string, TileTheme>
        {
            ["forest"] = new TileTheme { floor = new[] { "#1a2a1e", "#223526", "#16231a" }, path = new[] { "#2e2a1e", "#3a3424" }, wall = new[] { "#243a28", "#16241a" }, style = "grass" },
            ["temple"] = new TileTheme { floor = new[] { "#1b2730", "#23323d", "#16212a" }, path = new[] { "#2c3b46", "#374b58" }, wall = new[] { "#33424e", "#1d2a32" }, style = "marble" },
            ["ruins"] = new TileTheme { floor = new[] { "#241f28", "#2f2834", "#1d1922" }, path = new[] { "#352c3a", "#43374a" }, wall = new[] { "#3a3142", "#241d2a" }, style = "cobble" },
            ["cave"] = new TileTheme { floor = new[] { "#16121c", "#1f1828", "#120e18" }, path = new[] { "#241a2e", "#2e2238" }, wall = new[] { "#2a2036", "#160f20" }, style = "cave" },
            ["citadel"] = new TileTheme { floor = new[] { "#1d1e26", "#272934", "#17181f" }, path = new[] { "#2f313e", "#3a3d4c" }, wall = new[] { "#3a3d4c", "#222430" }, style = "marble" },
            ["godreach"] = new TileTheme { floor = new[] { "#1f2536", "#2a3450", "#191e2c" }, path = new[] { "#33406a", "#3f4f80" }, wall = new[] { "#3a4870", "#222a40" }, style = "marble" },
        };

        static readonly Dictionary<string, PixelCanvas> TileCache = new Dictionary<string, PixelCanvas>();

        static PixelCanvas DitherGround(string[] shades, string style, long seed)
        {
            var c = new PixelCanvas(TS, TS);
            var rng = new Mulberry32(seed);
            c.R(0, 0, TS, TS, shades[0]);
            // base speckle
            for (int i = 0; i < 70; i++)
            {
                int x = PixelCanvas.Trunc(rng.Next() * TS), y = PixelCanvas.Trunc(rng.Next() * TS);
                c.Px(x, y, shades[PixelCanvas.Trunc(rng.Next() * shades.Length)]);
            }
            if (style == "grass")
            {
                for (int i = 0; i < 10; i++)
                {
                    int x = PixelCanvas.Trunc(rng.Next() * TS), y = PixelCanvas.Trunc(rng.Next() * (TS - 2));
                    c.Px(x, y, shades[1]); c.Px(x, y + 1, shades[1]);
                    if (rng.Next() < 0.15) c.Px(x, y - 1, "#6a8a5a"); // a blade tip
                }
                if (rng.Next() < 0.2) c.Px(PixelCanvas.Trunc(rng.Next() * TS), PixelCanvas.Trunc(rng.Next() * TS), "#b9c79a"); // pale bloom
            }
            else if (style == "marble")
            {
                // soft diagonal veins
                for (int i = 0; i < 3; i++)
                {
                    int x = PixelCanvas.Trunc(rng.Next() * TS), y = 0;
                    while (y < TS) { c.Px(x, y, shades[1]); x = (x + (rng.Next() < 0.5 ? 1 : 0)) % TS; y++; }
                }
            }
            else if (style == "cobble")
            {
                for (int gy = 0; gy < TS; gy += 5)
                    for (int gx = 0; gx < TS; gx += 5)
                    {
                        int ox = gx + PixelCanvas.Trunc(rng.Next() * 2), oy = gy + PixelCanvas.Trunc(rng.Next() * 2);
                        c.R(ox, oy, 3, 3, shades[1]);
                        string hi = shades.Length > 2 ? shades[2] : shades[0];
                        c.Px(ox, oy, hi);            // tiny highlight
                        c.Px(ox + 2, oy + 2, hi);    // tiny shadow
                    }
            }
            else if (style == "cave")
            {
                string sp = shades.Length > 2 ? shades[2] : shades[1];
                for (int i = 0; i < 18; i++) c.Px(PixelCanvas.Trunc(rng.Next() * TS), PixelCanvas.Trunc(rng.Next() * TS), sp);
                if (rng.Next() < 0.3)
                {
                    int y = PixelCanvas.Trunc(rng.Next() * TS);
                    for (int x = 2; x < 12; x++) if (rng.Next() < 0.6) c.Px(x, y, "#0c0810"); // crack
                }
            }
            return c;
        }

        static PixelCanvas BrickWall(string[] shades, long seed)
        {
            var c = new PixelCanvas(TS, TS);
            var rng = new Mulberry32(seed + 51);
            c.R(0, 0, TS, TS, shades[1]);
            const string mortar = "#0e0c12";
            for (int row = 0; row < 4; row++)
            {
                int y = row * 4;
                int offset = (row % 2) * 4;
                for (int bx = -offset; bx < TS; bx += 8)
                {
                    c.R(bx, y, 7, 3, shades[0]);
                    // top highlight + bottom shade
                    c.R(bx, y, 7, 1, PixelCanvas.ShadeLighten(shades[0], 12));
                    c.R(bx, y + 2, 7, 1, "#00000033");
                    if (rng.Next() < 0.3) c.Px(bx + 2 + PixelCanvas.Trunc(rng.Next() * 3), y + 1, mortar); // chip
                }
                c.R(0, y + 3, TS, 1, mortar); // mortar line
            }
            return c;
        }

        static PixelCanvas LiquidTile(string top, long seed)
        {
            var c = new PixelCanvas(TS, TS);
            var rng = new Mulberry32(seed + 13);
            c.R(0, 0, TS, TS, "#0a1822");
            c.R(0, 0, TS, TS, top);
            for (int i = 0; i < 40; i++) c.Px(PixelCanvas.Trunc(rng.Next() * TS), PixelCanvas.Trunc(rng.Next() * TS), "#0a1822");
            for (int y = 1; y < TS; y += 4)
                for (int x = 0; x < TS; x += 1)
                    if (rng.Next() < 0.3) c.Px(x, y, "#3a6a7a");
            return c;
        }

        /// <summary>Get (and cache) a tile for a tileset + semantic kind + variant seed.</summary>
        public static PixelCanvas GetTile(string tileset, string kind, int variant = 0)
        {
            string key = $"{tileset}:{kind}:{variant}";
            if (TileCache.TryGetValue(key, out var cached)) return cached;
            if (!TileThemes.TryGetValue(tileset, out var t)) t = TileThemes["forest"];
            long seed = Hash.Str(key);
            PixelCanvas canvas;
            if (kind == "wall") canvas = BrickWall(t.wall, seed);
            else if (kind == "water") canvas = LiquidTile(t.floor[0], seed);
            else if (kind == "path")
            {
                var shades = new string[t.path.Length + 1];
                t.path.CopyTo(shades, 0);
                shades[t.path.Length] = t.floor[0];
                canvas = DitherGround(shades, t.style == "grass" ? "dirt" : t.style, seed);
            }
            else canvas = DitherGround(t.floor, t.style, seed + variant * 7);
            TileCache[key] = canvas;
            return canvas;
        }

        // =====================================================================
        // PROPS — hand-authored gothic objects. ' ' / '.' are transparent.
        // =====================================================================
        sealed class PropGrid
        {
            public Dictionary<char, string> pal;
            public string[] rows;
        }

        static Dictionary<char, string> Pal(params (char, string)[] e)
        {
            var d = new Dictionary<char, string>();
            foreach (var (k, v) in e) d[k] = v;
            return d;
        }

        static readonly Dictionary<string, PropGrid> PropGrids = new Dictionary<string, PropGrid>
        {
            ["deadTree"] = new PropGrid
            {
                pal = Pal(('k', "#0d0a0c"), ('t', "#241a18"), ('h', "#2f2422"), ('b', "#161012")),
                rows = new[]
                {
                    "......k.k.......", "...k..khk...k...", "....k.hh.kk.k...", ".....khh.hk.....",
                    "...k..thh.k.....", "....k.thtk......", "......tht.......", ".....ththk......",
                    "......tht.......", "......tht.......", ".....bthtb......", "......tht.......",
                    "......btb.......", ".....bbtbb......", "....bbbtbbb.....", "...kbbbbbbbk....",
                },
            },
            ["pine"] = new PropGrid
            {
                pal = Pal(('d', "#16241a"), ('g', "#1f3324"), ('l', "#2a4530"), ('t', "#241a12"), ('s', "#0c0a0c")),
                rows = new[]
                {
                    ".......g.......", "......ggg......", "......glg......", ".....ggggg.....",
                    ".....gglgg.....", "....ggdgggg....", "....gglgggg....", "...gggdggggg...",
                    "...ggglggggg...", "..gggdgggdgg...", "..gggglgggggg..", ".ggdggggggdgg..",
                    ".ggggglggggggg.", "......ttt......", "......ttt......", ".....stttss....",
                },
            },
            ["pillar"] = new PropGrid
            {
                pal = Pal(('s', "#3a3a44"), ('l', "#50505c"), ('d', "#22222a"), ('k', "#101016")),
                rows = new[]
                {
                    "..llssssddk....", "..llssssddk....", "...lsssdd......", "...lsssdd......",
                    "...lsssdd......", "...lsssdd......", "...lsssdd......", "...lsssdd......",
                    "...lsssdd......", "..llsssddk.....", "..lksssddk.....", ".k.lsskd..k....",
                    "...lss.........", "..klsdk........", ".kkssddk.......", "kkdddddkk......",
                },
            },
            ["statue"] = new PropGrid
            {
                pal = Pal(('s', "#42434e"), ('l', "#585a66"), ('d', "#26262e"), ('k', "#101016"), ('e', "#9fd0ff")),
                rows = new[]
                {
                    ".....kssk......", ".....lssd......", ".....lesd......", ".....lssd......",
                    "....klssdk.....", "...kllssddk....", "...ll.ss.dd....", "...l..ss..d....",
                    "....klssdk.....", ".....lssd......", ".....lssd......", "....llssdd.....",
                    "....lsssdd.....", "...kllssddk....", "..kssssssddk...", ".kkddddddddkk..",
                },
            },
            ["brazier"] = new PropGrid
            {
                pal = Pal(('m', "#3a2e22"), ('d', "#22190f"), ('k', "#100b08"), ('e', "#1a1410")),
                rows = new[]
                {
                    "...............", "...............", "...............", "....eeeeee.....",
                    "...emmmmmme....", "...dmmmmmmd....", "....dmmmmd.....", ".....dmmd......",
                    "......mm.......", "......mm.......", ".....dmmd......", "....mmmmmm.....",
                    "...kmmmmmmk....", "....k.mm.k.....", "......mm.......", "....kkmmkk.....",
                },
            },
            ["grave"] = new PropGrid
            {
                pal = Pal(('s', "#3a3a42"), ('l', "#4e4e58"), ('d', "#24242a"), ('k', "#101015"), ('g', "#1c2a1e")),
                rows = new[]
                {
                    "...............", "....kkkkk......", "...klsssdk.....", "...lssssdd.....",
                    "...lsddssd.....", "...lssssdd.....", "...lssssdd.....", "...lsddssd.....",
                    "...lssssdd.....", "...klsssdk.....", "....ksssk......", "....lsssd......",
                    "...gglsssdgg...", "..ggggsssgggg..", ". gggggggggg g.", "...............",
                },
            },
            ["altar"] = new PropGrid
            {
                pal = Pal(('s', "#3e3a46"), ('l', "#54505e"), ('d', "#26242c"), ('k', "#100f14"), ('e', "#b48aff")),
                rows = new[]
                {
                    "...............", "...............", "....eeeeee.....", "...kssssssk....",
                    "...lssssssd....", "...lssssssd....", "...kdssssdk....", "....lssssd.....",
                    "....lssssd.....", "...klssssdk....", "...lsssssssd...", "..kssssssssdk..",
                    "..lsssssssssd..", "..kdssssssddk..", "...kkddddkk....", "...............",
                },
            },
            ["rubble"] = new PropGrid
            {
                pal = Pal(('s', "#34343c"), ('l', "#46464e"), ('d', "#202028"), ('k', "#101014")),
                rows = new[]
                {
                    "...............", "...............", "...............", "...............",
                    "......kk.......", "....klsdk......", "...kldssdk.....", "..klsssdsdk....",
                    ".klsdksldssk...", "klsssdklssddk..", "ldsskddlsssddk.", "kkddkkkddddkkk.",
                    "...............", "...............", "...............", "...............",
                },
            },
            ["bush"] = new PropGrid
            {
                pal = Pal(('d', "#16241a"), ('g', "#1f3324"), ('l', "#2a4530"), ('b', "#3a2a1a")),
                rows = new[]
                {
                    "...............", "...............", "...............", "....gg.gg......",
                    "...ggggggg.....", "..gglgggdgg....", "..ggggglggg....", ".gggdgggggg....",
                    ".ggggggglgg....", ".gglgggggdg....", ".. gggdgg g....", "....b...b......",
                    "...............", "...............", "...............", "...............",
                },
            },
            ["candle"] = new PropGrid
            {
                pal = Pal(('w', "#d8d0b0"), ('d', "#9a8a60"), ('k', "#100f0c")),
                rows = new[] { "......", "..ww..", "..ww..", "..ww..", ".kddk.", ".kkkk." },
            },
        };

        public static IEnumerable<string> PropNames => PropGrids.Keys;

        static readonly Dictionary<string, PixelCanvas> PropCache = new Dictionary<string, PixelCanvas>();

        public static PixelCanvas Prop(string name)
        {
            if (PropCache.TryGetValue(name, out var cached)) return cached;
            var c = PropGrids.TryGetValue(name, out var g) ? FromGrid(g.rows, g.pal) : new PixelCanvas(16, 16);
            PropCache[name] = c;
            return c;
        }

        // Animated brazier flame (3 frames).
        static readonly List<PixelCanvas> FlameCache = new List<PixelCanvas>();

        public static PixelCanvas Flame(int frame)
        {
            if (FlameCache.Count == 0)
            {
                for (int f = 0; f < 3; f++)
                {
                    var c = new PixelCanvas(16, 16);
                    var rng = new Mulberry32(f + 1);
                    int h = 6 + f;
                    for (int i = 0; i < 26; i++)
                    {
                        double t = rng.Next();
                        int x = 8 + PixelCanvas.JsRound(Math.Sin(t * 6 + f) * (1 - t) * 3);
                        int y = 8 - PixelCanvas.JsRound(t * h);
                        string col = t > 0.7 ? "#fff2c0" : t > 0.4 ? "#ffb347" : "#ff6a2a";
                        c.Px(x, y, col); c.Px(x + (rng.Next() < .5 ? 1 : 0), y, col);
                    }
                    FlameCache.Add(c);
                }
            }
            return FlameCache[((frame % 3) + 3) % 3];
        }

        // =====================================================================
        // CHARACTERS — procedural humanoid with 3 frames × 4 directions.
        // =====================================================================
        public const int HeroW = 16, HeroH = 20;

        public sealed class HumanoidPalette
        {
            public string cloak, cloakLt, cloakDk, trim, hood, hoodLt, skin, boot, eye, blade, bladeTip;
        }

        static void DrawHumanoid(PixelCanvas c, string dir, int frame, HumanoidPalette pal)
        {
            // frame: 0 idle, 1 & 2 walk. Legs swing on walk frames.
            int swing = frame == 1 ? 1 : frame == 2 ? -1 : 0;
            // shadow
            c.R(4, 19, 8, 1, "#00000055");
            // legs
            c.R(6, 15 + (swing < 0 ? 1 : 0), 2, 4 - (swing < 0 ? 1 : 0), pal.boot);
            c.R(9, 15 + (swing > 0 ? 1 : 0), 2, 4 - (swing > 0 ? 1 : 0), pal.boot);
            // cloak / body
            c.R(4, 8, 8, 8, pal.cloak);
            c.R(4, 8, 8, 1, pal.cloakLt);                 // shoulder highlight
            c.R(4, 15, 8, 1, pal.cloakDk);
            c.R(5, 11, 6, 1, pal.trim);                   // belt/trim
            // head + hood
            c.R(5, 2, 6, 6, pal.hood);
            c.R(6, 4, 4, 4, pal.skin);                    // face area
            c.R(5, 2, 6, 1, pal.hoodLt);

            // facing details
            if (dir == "down")
            {
                c.Px(7, 6, pal.eye); c.Px(9, 6, pal.eye);
                c.R(6, 2, 4, 2, pal.hood);                // hood brim
            }
            else if (dir == "up")
            {
                c.R(5, 2, 6, 5, pal.hood);                // back of hood, no face
            }
            else
            {
                // left/right profile (caller mirrors for left)
                c.Px(9, 6, pal.eye);
                c.R(6, 2, 5, 2, pal.hood);
            }

            // glowing blade at the side (the "godless blade")
            if (pal.blade != null)
            {
                c.R(12, 9, 1, 6, pal.blade);
                c.Px(12, 8, pal.bladeTip);
                c.Px(13, 10, pal.bladeTip);
            }
            // arms
            c.R(3, 9, 1, 4, pal.cloakDk);
            c.R(12, 9, 1, 4, pal.cloakDk);
        }

        public static readonly string[] Directions = { "down", "up", "left", "right" };

        static Dictionary<string, PixelCanvas[]> BuildHumanoidSet(HumanoidPalette pal, bool withBlade)
        {
            var p = new HumanoidPalette
            {
                cloak = pal.cloak, cloakLt = pal.cloakLt, cloakDk = pal.cloakDk, trim = pal.trim,
                hood = pal.hood, hoodLt = pal.hoodLt, skin = pal.skin, boot = pal.boot, eye = pal.eye,
                blade = withBlade ? pal.blade : null, bladeTip = pal.bladeTip,
            };
            var set = new Dictionary<string, PixelCanvas[]>();
            foreach (var dir in Directions)
            {
                var frames = new PixelCanvas[3];
                for (int f = 0; f < 3; f++)
                {
                    var c = new PixelCanvas(HeroW, HeroH);
                    DrawHumanoid(c, dir == "left" ? "right" : dir, f, p);
                    if (dir == "left")
                    {
                        // mirror horizontally
                        var m = new PixelCanvas(HeroW, HeroH);
                        m.Draw(c, 0, 0, mirror: true);
                        frames[f] = m;
                    }
                    else frames[f] = c;
                }
                set[dir] = frames;
            }
            return set;
        }

        static Dictionary<string, PixelCanvas[]> _heroSet;

        public static Dictionary<string, PixelCanvas[]> Hero()
        {
            return _heroSet ??= BuildHumanoidSet(new HumanoidPalette
            {
                cloak = "#2a3550", cloakLt = "#3a4a70", cloakDk = "#1a2236", trim = "#caa84d",
                hood = "#202840", hoodLt = "#33405e", skin = "#caa07a", boot = "#1a1620",
                eye = "#9fe0ff", blade = "#bff6ff", bladeTip = "#ffffff",
            }, true);
        }

        static readonly Dictionary<string, Dictionary<string, PixelCanvas[]>> NpcCache = new Dictionary<string, Dictionary<string, PixelCanvas[]>>();

        public static Dictionary<string, PixelCanvas[]> Npc(string color)
        {
            if (NpcCache.TryGetValue(color, out var cached)) return cached;
            var set = BuildHumanoidSet(new HumanoidPalette
            {
                cloak = color, cloakLt = PixelCanvas.ShadeLighten(color, 24), cloakDk = "#1a1620", trim = "#e0d6b0",
                hood = PixelCanvas.ShadeLighten(color, -10), hoodLt = PixelCanvas.ShadeLighten(color, 18),
                skin = "#caa07a", boot = "#241c16", eye = "#1a1414",
            }, false);
            NpcCache[color] = set;
            return set;
        }

        // =====================================================================
        // MONSTERS — procedural per body type, 2 frames, sized by def.size.
        // =====================================================================
        public sealed class MonsterLook
        {
            public string id, body, color;
            public float size;
            public bool boss;
        }

        static readonly Dictionary<string, PixelCanvas[]> MonsterCache = new Dictionary<string, PixelCanvas[]>();

        public static PixelCanvas[] Monster(MonsterLook def)
        {
            if (MonsterCache.TryGetValue(def.id, out var cached)) return cached;
            int s = Math.Max(16, PixelCanvas.JsRound(def.size * 2));
            var frames = new PixelCanvas[2];
            for (int f = 0; f < 2; f++)
            {
                var c = new PixelCanvas(s, s);
                DrawMonster(c, s, f, def);
                frames[f] = c;
            }
            MonsterCache[def.id] = frames;
            return frames;
        }

        static void DrawMonster(PixelCanvas c, int s, int f, MonsterLook def)
        {
            string col = def.color;
            string dk = PixelCanvas.ShadeLighten(col, -22);
            string lt = PixelCanvas.ShadeLighten(col, 26);
            string eye = def.boss ? "#ffcaa0" : "#ff5a3a";
            double cx = s / 2.0;
            int bob = f == 1 ? 1 : 0;
            c.R(cx - s * 0.28, s - 2, s * 0.56, 1, "#00000055"); // shadow

            switch (def.body)
            {
                case "shade":
                {
                    // floating wraith: tapered glowing body + wispy tail
                    for (int y = 0; y < s - 2; y++)
                    {
                        double w = (s * 0.5) * (1 - y / (s * 1.6));
                        double wob = Math.Sin((y * 0.6) + f * 1.5) * 1.2;
                        c.R(cx - w / 2 + wob, 1 + y - bob, w, 1, y < 3 ? lt : col);
                    }
                    c.Px(cx - 2, 5 - bob, eye); c.Px(cx + 1, 5 - bob, eye);
                    break;
                }
                case "beast":
                {
                    double y0 = s * 0.35 - bob;
                    c.R(cx - s * 0.34, y0, s * 0.68, s * 0.34, col);       // body
                    c.R(cx - s * 0.34, y0, s * 0.68, 2, lt);
                    c.R(cx + s * 0.18, y0 - s * 0.12, s * 0.22, s * 0.2, col); // head
                    c.Px(cx + s * 0.3, y0 - s * 0.05, eye);
                    // ears / horns
                    c.R(cx + s * 0.2, y0 - s * 0.2, 1, 2, dk);
                    c.R(cx + s * 0.34, y0 - s * 0.2, 1, 2, dk);
                    // legs (swing)
                    int sw = f == 1 ? 1 : -1;
                    c.R(cx - s * 0.28, y0 + s * 0.34, 2, s * 0.22 + sw, dk);
                    c.R(cx + s * 0.16, y0 + s * 0.34, 2, s * 0.22 - sw, dk);
                    if (def.boss)
                    {
                        // cinder maw: furnace mouth
                        c.R(cx + s * 0.3, y0 + s * 0.02, 3, 3, "#ff8a3a");
                        c.Px(cx + s * 0.32, y0 + s * 0.04, "#fff2c0");
                    }
                    break;
                }
                case "serpent":
                {
                    // coiled body that sways + snake-hair
                    for (int i = 0; i < 6; i++)
                    {
                        double yy = s - 3 - i * (s * 0.12);
                        double xx = cx + Math.Sin(i * 0.9 + f) * (s * 0.22) - 2;
                        c.R(xx, yy, 4, 3, i == 5 ? lt : col);
                    }
                    double hx = cx + Math.Sin(5 * 0.9 + f) * (s * 0.22) - 2;
                    double hy = s - 3 - 5 * (s * 0.12);
                    c.R(hx - 1, hy - 2, 6, 4, col);          // head
                    c.Px(hx, hy - 1, "#9fff7a"); c.Px(hx + 3, hy - 1, "#9fff7a"); // green eyes
                    for (int h = 0; h < 4; h++) c.Px(hx + h - 1, hy - 3 - (h % 2) - f, dk); // hair
                    break;
                }
                case "bird":
                {
                    // strix: body + flapping wings
                    int wy = f == 1 ? -1 : 1;
                    c.R(cx - 2, s * 0.4, 4, s * 0.3, col);   // body
                    c.R(cx - s * 0.4, s * 0.4 + wy, s * 0.3, 2, dk);   // L wing
                    c.R(cx + s * 0.12, s * 0.4 - wy, s * 0.3, 2, dk);  // R wing
                    c.Px(cx - 1, s * 0.44, eye); c.Px(cx + 1, s * 0.44, eye);
                    c.Px(cx, s * 0.5, "#caa84d");            // beak
                    break;
                }
                case "automaton":
                {
                    // bronze golem: blocky, glowing eye slit
                    double y0 = s * 0.2;
                    c.R(cx - s * 0.3, y0, s * 0.6, s * 0.5, col);
                    c.R(cx - s * 0.3, y0, s * 0.6, 2, lt);
                    c.R(cx - s * 0.3, y0 + s * 0.5 - 2, s * 0.6, 2, dk);
                    c.R(cx - s * 0.18, y0 - s * 0.16, s * 0.36, s * 0.18, col);  // head
                    c.R(cx - s * 0.12, y0 - s * 0.1, s * 0.24, 2, "#9fd0ff");    // eye slit (water-weak glow)
                    // legs
                    int sw = f == 1 ? 1 : 0;
                    c.R(cx - s * 0.22, y0 + s * 0.5, 3, s * 0.26 - sw, dk);
                    c.R(cx + s * 0.1, y0 + s * 0.5, 3, s * 0.26 - (1 - sw), dk);
                    // arms
                    c.R(cx - s * 0.4, y0 + s * 0.1, 2, s * 0.3, dk);
                    c.R(cx + s * 0.32, y0 + s * 0.1, 2, s * 0.3, dk);
                    break;
                }
                default:
                    c.R(cx - 4, cx - 4, 8, 8, col);
                    break;
            }
        }
    }
}
