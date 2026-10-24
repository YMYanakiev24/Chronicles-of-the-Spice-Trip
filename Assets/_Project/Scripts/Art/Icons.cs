/*
 * Icons.cs
 * ----------------------------------------------------------------------------
 * Small pixel-art icons that replace the emoji and symbols the web UI used
 * (🍯 🏺 🍮 🔥 🧵 🪙 🏰 ⚔ ✓ ✕ ⟵ ▶ ✦ ❖ 📷 🧙). Unity's UI text can't be relied on
 * to render colour emoji, and drawing them keeps the UI in the game's own
 * pixel style. Each icon is a character grid like the prop grids in PixelArt.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using UnityEngine;

namespace SpiceTrip.Art
{
    public static class Icons
    {
        static readonly Dictionary<string, (string[] rows, Dictionary<char, string> pal)> Grids =
            new Dictionary<string, (string[], Dictionary<char, string>)>
        {
            // 🍯 Ambrosia Cake — honey pot
            ["ambrosiaCake"] = (new[]
            {
                "............",
                "....kkkk....",
                "...kyyyyk...",
                "..kkkkkkkk..",
                "..kooooook..",
                ".kooyoooook.",
                ".koyyoooook.",
                ".kooooooook.",
                ".kooooooodk.",
                "..kooooodk..",
                "...kkkkkk...",
                "............",
            }, P(('k', "#3a2412"), ('o', "#d98a1f"), ('y', "#ffd76b"), ('d', "#9a5a10"))),
            // 🏺 Vial of Nectar — amphora
            ["nectarVial"] = (new[]
            {
                "....kkkk....",
                ".....tt.....",
                "...kkttkk...",
                "..k.kttk.k..",
                "..k.tttt.k..",
                "...ttyyttt..",
                "..ttyyyttdt.",
                "..ttttttddt.",
                "...ttttddt..",
                "....ttddt...",
                "....kkkkk...",
                "............",
            }, P(('k', "#3a2412"), ('t', "#c86a3a"), ('y', "#ffd76b"), ('d', "#8a3a1a"))),
            // 🍮 Honeyed Ambrosia — custard
            ["ambrosiaCake2"] = (new[]
            {
                "............",
                "............",
                "....bbbb....",
                "...bccccb...",
                "...yyyyyy...",
                "..yyyyyyyy..",
                "..yyyyyyyy..",
                ".yyyyyyyyyd.",
                ".yyyyyyyydd.",
                "kkkkkkkkkkkk",
                ".kkkkkkkkkk.",
                "............",
            }, P(('b', "#6a3a10"), ('c', "#9a5a20"), ('y', "#ffd76b"), ('d', "#d9a83a"), ('k', "#cfcfd6"))),
            // 🔥 Cinder Sigil — flame
            ["cinderSigil"] = (new[]
            {
                ".....r......",
                ".....rr.....",
                "....rro.....",
                "...rroor..r.",
                "...rooor.rr.",
                "..rroyoorrr.",
                "..rooyyoorr.",
                ".rroyyyyoorr",
                ".rooywwyyorr",
                ".rooywwyyor.",
                "..rooyyyor..",
                "...rrrrrr...",
            }, P(('r', "#d8381a"), ('o', "#ff8a3a"), ('y', "#ffd24a"), ('w', "#fff2c0"))),
            // 🗡️ Relic Shard
            ["relicShard"] = (new[]
            {
                "..........w.",
                ".........wb.",
                "........wb..",
                ".......wb...",
                "......wb....",
                ".....wb.....",
                "..g.wb......",
                "...gb.......",
                "..kgg.......",
                ".kk..g......",
                "kk..........",
                "............",
            }, P(('w', "#eaffff"), ('b', "#9fd0ff"), ('g', "#caa84d"), ('k', "#4a3420"))),
            // 🌼 Asphodel Bloom
            ["asphodel"] = (new[]
            {
                "............",
                "....w.w.....",
                "...wwywww...",
                "..wwyyyww...",
                "...wwyww....",
                "....w.w.....",
                ".....g......",
                ".....g..l...",
                "..l..g.ll...",
                "..ll.gll....",
                "....lg......",
                ".....g......",
            }, P(('w', "#f0ecd8"), ('y', "#ffd24a"), ('g', "#3f7d3a"), ('l', "#5fa04a"))),
            // 🪙 Obol — coin
            ["coin"] = (new[]
            {
                "............",
                "....kkkk....",
                "...kyyyyk...",
                "..kyywwyyk..",
                ".kyywyyyydk.",
                ".kyyyddyydk.",
                ".kyyyddyydk.",
                ".kyyyyyyydk.",
                "..kyyyyydk..",
                "...kddddk...",
                "....kkkk....",
                "............",
            }, P(('k', "#5a4010"), ('y', "#ffd76b"), ('w', "#fff2c0"), ('d', "#caa84d"))),
            // 🧵 Enchanted Loom — spool of thread
            ["loom"] = (new[]
            {
                "..kkkkkkkk..",
                "..kwwwwwwk..",
                "...pppppp...",
                "...pPpppp...",
                "...ppPppp...",
                "...pppPpp...",
                "...ppppPp..p",
                "...pppppp.p.",
                "...pppppp.p.",
                "..kwwwwwwkp.",
                "..kkkkkkkk..",
                "............",
            }, P(('k', "#4a3420"), ('w', "#8a6a40"), ('p', "#b98aff"), ('P', "#e0c8ff"))),
            // 🏰 Castle
            ["castle"] = (new[]
            {
                "k.k......k.k",
                "kkk..f...kkk",
                "kkk.kkk..kkk",
                "kkk.kkk..kkk",
                "kkkkkkkkkkkk",
                "kkkkkkkkkkkk",
                "kk.kkkkkk.kk",
                "kkkkkkkkkkkk",
                "kkkkk..kkkkk",
                "kkkk....kkkk",
                "kkkk....kkkk",
                "kkkk....kkkk",
            }, P(('k', "#bfc8d8"), ('f', "#ff5a4a"))),
            // ⚔ Crossed swords
            ["swords"] = (new[]
            {
                "w..........w",
                ".w........w.",
                "..w......w..",
                "...w....w...",
                "....w..w....",
                ".....ww.....",
                ".....ww.....",
                "....w..w....",
                "..gw....wg..",
                "...g....g...",
                "..k.g..g.k..",
                ".k........k.",
            }, P(('w', "#eaeaf0"), ('g', "#caa84d"), ('k', "#5a3a20"))),
            // ✓ Check
            ["check"] = (new[]
            {
                "............",
                "..........gg",
                ".........ggg",
                "........ggg.",
                ".......ggg..",
                "gg....ggg...",
                "ggg..ggg....",
                ".ggggggg....",
                "..ggggg.....",
                "...ggg......",
                "....g.......",
                "............",
            }, P(('g', "#7fe06b"))),
            // ✕ Close
            ["close"] = (new[]
            {
                "..........",
                ".ww....ww.",
                "..ww..ww..",
                "...wwww...",
                "....ww....",
                "...wwww...",
                "..ww..ww..",
                ".ww....ww.",
                "..........",
            }, P(('w', "#e9dcbf"))),
            // ⟵ Back arrow
            ["back"] = (new[]
            {
                "...w........",
                "..ww........",
                ".wwwwwwwwwww",
                "..ww........",
                "...w........",
            }, P(('w', "#e9dcbf"))),
            // ▶ Play
            ["play"] = (new[]
            {
                "w.....",
                "www...",
                "wwwww.",
                "wwwwww",
                "wwwww.",
                "www...",
                "w.....",
            }, P(('w', "#ffffff"))),
            // ✦ Star
            ["star"] = (new[]
            {
                "....y....",
                "....y....",
                "...yyy...",
                "..yyyyy..",
                "yyyyyyyyy",
                "..yyyyy..",
                "...yyy...",
                "....y....",
                "....y....",
            }, P(('y', "#ffd76b"))),
            // ❖ Book emblem
            ["emblem"] = (new[]
            {
                ".....y.....",
                "....yyy....",
                "...y.y.y...",
                "..yy.y.yy..",
                ".yyyy.yyyy.",
                "yyy.....yyy",
                ".yyyy.yyyy.",
                "..yy.y.yy..",
                "...y.y.y...",
                "....yyy....",
                ".....y.....",
            }, P(('y', "#ffd76b"))),
            // 📷 Camera (upload)
            ["camera"] = (new[]
            {
                "............",
                "...kkk......",
                ".kkkkkkkkkk.",
                ".kgggggggrk.",
                ".kggwwwgggk.",
                ".kgw...wggk.",
                ".kgw.b.wggk.",
                ".kgw...wggk.",
                ".kggwwwgggk.",
                ".kkkkkkkkkk.",
                "............",
            }, P(('k', "#2a2a34"), ('g', "#6a6a7a"), ('w', "#cfcfd6"), ('b', "#5ad0ff"), ('r', "#ff5a4a"))),
            // ☆ empty rating star
            ["star-empty"] = (new[]
            {
                "....y....",
                "....y....",
                "...y.y...",
                "..y...y..",
                "yyy...yyy",
                "..y...y..",
                "...y.y...",
                "....y....",
                "....y....",
            }, P(('y', "#6a5a3a"))),
            // --- realm quest items ---
            ["graveToken"] = (new[]
            {
                "............",
                "...kkkkkk...",
                "..kcccccck..",
                ".kccdddccck.",
                ".kcdcccdcck.",
                ".kccdddccck.",
                ".kcccccdcck.",
                ".kccdddccck.",
                "..kcccccck..",
                "...kkkkkk...",
                "............",
                "............",
            }, P(('k', "#4a3a5a"), ('c', "#c8b8e8"), ('d', "#6a5a7a"))),
            ["starEmber"] = (new[]
            {
                ".....y......",
                ".....y......",
                "....yoy.....",
                "y..yowoy..y.",
                ".yyowwwoyy..",
                "..yowwwoy...",
                ".yyowwwoyy..",
                "y..yowoy..y.",
                "....yoy.....",
                ".....y......",
                ".....y......",
                "............",
            }, P(('y', "#ffd24a"), ('o', "#ff9a3a"), ('w', "#fff2c0"))),
            // --- Marin's battle items ---
            ["draught"] = (new[]
            {
                "....kk......",
                "....ww......",
                "...kwwk.....",
                "..krrrrk....",
                ".krrwrrrk...",
                ".krwrrrrk...",
                ".krrrrrdk...",
                ".krrrrddk...",
                "..krrddk....",
                "...kkkk.....",
                "............",
                "............",
            }, P(('k', "#3a1a1a"), ('w', "#e9dcbf"), ('r', "#e0403a"), ('d', "#9a2020"))),
            ["swift"] = (new[]
            {
                "....kk...g..",
                "....ww..gg..",
                "...kwwkggl..",
                "..kggggglg..",
                ".kgglggglg..",
                ".kglgggkg...",
                ".kgggggdk...",
                ".kggggddk...",
                "..kggddk....",
                "...kkkk.....",
                "............",
                "............",
            }, P(('k', "#1a3a1a"), ('w', "#e9dcbf"), ('g', "#5fd06a"), ('l', "#d6ffc8"), ('d', "#2a8a3a"))),
            ["charm"] = (new[]
            {
                "...gggg.....",
                "..g....g....",
                "..g....g....",
                "...gggg.....",
                "....bb......",
                "...bccb.....",
                "..bcwccb....",
                "..bccccb....",
                "...bccb.....",
                "....bb......",
                "............",
                "............",
            }, P(('g', "#caa84d"), ('b', "#3a6ad0"), ('c', "#7fc8ff"), ('w', "#ffffff"))),
            // --- Chronicle UI: spell schools (🔥 🌊 ⚡ 🌿 🛡️ ☀️) ---
            ["school-Fire"] = (new[]
            {
                ".....r......", "....rr......", "....roor....", "...rooor.r..", "..rroyorrr..", "..royyyorr..",
                ".rroyyyyorr.", ".roywwwyyor.", ".royywwyyor.", "..royyyyor..", "...rrrrrr...", "............",
            }, P(('r', "#d8381a"), ('o', "#ff8a3a"), ('y', "#ffd24a"), ('w', "#fff2c0"))),
            ["school-Water"] = (new[]
            {
                "............", "....bb......", "...bccb.....", "..bc..cb..b.", ".bc....cbbc.", "bc......cc..",
                "............", "....bb......", "...bccb.....", "..bc..cb..b.", ".bc....cbbc.", "bc......cc..",
            }, P(('b', "#4fd2c8"), ('c', "#bff6ee"))),
            ["school-Lightning"] = (new[]
            {
                "......yyy...", ".....yyy....", "....yyy.....", "...yyy......", "..yyyyyyy...", ".....yyy....",
                "....yyy.....", "...yyy......", "..yy........", ".yy.........", "y...........", "............",
            }, P(('y', "#ffe08a"))),
            ["school-Life"] = (new[]
            {
                "..........gg", "........gglg", "......gglgg.", ".....glggg..", "....glggg...", "...glggg....",
                "..gggg......", "..gg........", ".g..........", "g...........", "............", "............",
            }, P(('g', "#3f9d3a"), ('l', "#9fe07a"))),
            ["school-Ward"] = (new[]
            {
                "kkkkkkkkkk..", "kssssssssk..", "kslsssssdk..", "kslsssssdk..", "kslsssssdk..", "kslsssssdk..",
                ".kssssssk...", ".kssssssk...", "..kssssk....", "...kssk.....", "....kk......", "............",
            }, P(('k', "#5a5a6a"), ('s', "#bfc8d8"), ('l', "#ffffff"), ('d', "#8a90a8"))),
            ["school-Divine"] = (new[]
            {
                ".....y......", "..y..y..y...", "...y.y.y....", "....yyy.....", "yyyyywyyyyy.", "....yyy.....",
                "...y.y.y....", "..y..y..y...", ".....y......", "............", "............", "............",
            }, P(('y', "#ffd76b"), ('w', "#ffffff"))),
            // --- book tabs (📜 ✦ 🐾 🗺 ❖ ⚙ ⏸) ---
            ["scroll"] = (new[]
            {
                ".pppppppp...", "pkppppppkp..", ".pllllllp...", ".pkkkkkkp...", ".pllllllp...", ".pkkkkkp....",
                ".pllllllp...", ".pkkkkkkp...", "pkppppppkp..", ".pppppppp...", "............", "............",
            }, P(('p', "#e9dcbf"), ('k', "#7a5a30"), ('l', "#d8c8a0"))),
            ["paw"] = (new[]
            {
                "..b....b....", ".bbb..bbb...", ".bbb..bbb...", "b..........b", "bb.......bbb", "bb..bbbb..b.",
                "...bbbbbb...", "..bbbbbbbb..", "..bbbbbbbb..", "...bb..bb...", "............", "............",
            }, P(('b', "#e9dcbf"))),
            ["map"] = (new[]
            {
                "kkkkkkkkkkkk", "kpppgpppppbk", "kppgggpppbbk", "kppggppppbbk", "kpppppprpppk", "kpbpppprrppk",
                "kbbpppppprpk", "kbbbppgggppk", "kpbpppggpppk", "kkkkkkkkkkkk", "............", "............",
            }, P(('k', "#7a5a30"), ('p', "#e9dcbf"), ('g', "#5fa04a"), ('b', "#5aa8ff"), ('r', "#d8381a"))),
            ["gear"] = (new[]
            {
                ".....gg.....", "..g.gggg.g..", ".gggggggggg.", "..gggkkggg..", ".gggk..kggg.", "gggk....kggg",
                "gggk....kggg", ".gggk..kggg.", "..gggkkggg..", ".gggggggggg.", "..g.gggg.g..", ".....gg.....",
            }, P(('g', "#e9dcbf"), ('k', "#7a5a30"))),
            ["pause"] = (new[] { "ww..ww", "ww..ww", "ww..ww", "ww..ww", "ww..ww", "ww..ww" }, P(('w', "#e9dcbf"))),
            ["diamond"] = (new[] { "...w...", "..w.w..", ".w...w.", "w.....w", ".w...w.", "..w.w..", "...w..." }, P(('w', "#e9dcbf"))),
            ["shield"] = (new[]
            {
                "bbbbbbbb", "bccccccb", "bccccccb", "bccccccb", ".bccccb.", ".bccccb.", "..bccb..", "...bb...",
            }, P(('b', "#7fc8ff"), ('c', "#cfe8ff"))),
        };

        static Dictionary<char, string> P(params (char, string)[] e)
        {
            var d = new Dictionary<char, string>();
            foreach (var (k, v) in e) d[k] = v;
            return d;
        }

        static readonly Dictionary<string, Texture2D> Cache = new Dictionary<string, Texture2D>();

        public static bool Has(string name) => Grids.ContainsKey(name) || name == "hero";

        public static Texture2D Get(string name)
        {
            if (Cache.TryGetValue(name, out var t) && t != null) return t;
            PixelCanvas c;
            if (name == "hero") c = PixelArt.Hero()["down"][0];
            else if (Grids.TryGetValue(name, out var g))
            {
                c = new PixelCanvas(g.rows[0].Length, g.rows.Length);
                for (int y = 0; y < g.rows.Length; y++)
                    for (int x = 0; x < g.rows[y].Length; x++)
                        if (g.pal.TryGetValue(g.rows[y][x], out var col)) c.Px(x, y, col);
            }
            else return null;
            t = c.ToTexture();
            t.name = "icon-" + name;
            Cache[name] = t;
            return t;
        }
    }
}
