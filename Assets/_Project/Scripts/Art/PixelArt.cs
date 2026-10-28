/*
 * PixelArt.cs
 * ----------------------------------------------------------------------------
 * Line-by-line port of the parts of the web game's PixelArt.js the game still
 * uses, drawn as pixel art on a PixelCanvas:
 *   - gothic props and the brazier flame (the hub's fallback scenes),
 *   - the hero, with a 3-frame walk in four directions (HeroSprites).
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
        static readonly Dictionary<string, Dictionary<string, PixelCanvas[]>> HeroColours = new Dictionary<string, Dictionary<string, PixelCanvas[]>>();

        /// <summary>The hero (with the godless blade) in another cloak colour: one per champion.</summary>
        public static Dictionary<string, PixelCanvas[]> HeroIn(string cloak)
        {
            if (HeroColours.TryGetValue(cloak, out var cached)) return cached;
            var set = BuildHumanoidSet(new HumanoidPalette
            {
                cloak = cloak, cloakLt = PixelCanvas.ShadeLighten(cloak, 26), cloakDk = PixelCanvas.ShadeLighten(cloak, -26), trim = "#caa84d",
                hood = PixelCanvas.ShadeLighten(cloak, -14), hoodLt = PixelCanvas.ShadeLighten(cloak, 12), skin = "#caa07a", boot = "#1a1620",
                eye = "#9fe0ff", blade = "#bff6ff", bladeTip = "#ffffff",
            }, true);
            HeroColours[cloak] = set;
            return set;
        }

        public static Dictionary<string, PixelCanvas[]> Hero()
        {
            return _heroSet ??= BuildHumanoidSet(new HumanoidPalette
            {
                cloak = "#2a3550", cloakLt = "#3a4a70", cloakDk = "#1a2236", trim = "#caa84d",
                hood = "#202840", hoodLt = "#33405e", skin = "#caa07a", boot = "#1a1620",
                eye = "#9fe0ff", blade = "#bff6ff", bladeTip = "#ffffff",
            }, true);
        }

    }
}
