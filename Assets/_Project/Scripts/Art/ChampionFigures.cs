/*
 * ChampionFigures.cs
 * ----------------------------------------------------------------------------
 * Every champion as a small walking pixel figure, drawn in code from their
 * character sheet (hair, outfit, props), because the sheets only show them
 * from the front. Each is seen from the front, the back and the side (left is
 * the side mirrored), standing plus a four-frame walk: a step, a passing frame
 * one pixel higher, the other step, and the passing frame again. Everything is
 * outlined in one dark colour at the end, like the hand-drawn sheets.
 *
 * A champion with a walk sheet of their own (assets/walk-<id>.png, see
 * Tools~/cut-walk-sheet.ps1) uses that instead; HeroSprites decides.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using UnityEngine;

namespace SpiceTrip.Art
{
    public static class ChampionFigures
    {
        public const int W = 24, H = 32;
        public const int WalkFrames = 4;
        public static readonly string[] Directions = { "down", "up", "left", "right" };

        const string Ink = "#1c1420";

        /// <summary>One pose: the direction, which leg is forward (-1, 0, 1), and the bob (0 or 1 px up).</summary>
        public readonly struct Pose
        {
            public readonly string dir;
            public readonly int phase, bob;
            public Pose(string dir, int phase, int bob) { this.dir = dir; this.phase = phase; this.bob = bob; }
            public bool Front => dir == "down";
            public bool Back => dir == "up";
            public bool Side => dir == "right";
            public int HeadY => 3 - bob;        // top of the head
            public int TorsoY => 13 - bob;      // top of the shoulders
            public int HipY => 21 - bob;        // the waistband; feet stay on row 30
        }

        enum Hair { None, Short, Buzz, Messy, Pixie }

        sealed class Look
        {
            public string skin = "#f0c8a0", skinDk = "#d4a07a", eye = "#2a1c18";
            public Hair hair = Hair.Short;
            public string hairC = "#5a3a22", hairDk = "#3e2614";
            public string top = "#6a6a7a", topDk = "#4a4a5a";
            public string sleeve, sleeveDk;     // null: bare arms
            public bool shortSleeves;
            public string pants, pantsDk;       // null: bare legs
            public string shoes;                // null: bare feet
            public int wide;                    // extra torso width on each side
            public int robeTo;                  // > 0: the top hangs down to this row
            public Action<PixelCanvas, Pose> behind, over;
        }

        static readonly Dictionary<string, Dictionary<string, PixelCanvas[]>> Cache = new Dictionary<string, Dictionary<string, PixelCanvas[]>>();

        /// <summary>Is there a drawn figure for this champion id (every playable character)?</summary>
        public static bool Has(string id) => id != null && Looks.ContainsKey(id);

        /// <summary>Frames per direction: [0] standing, then the walk cycle.</summary>
        public static Dictionary<string, PixelCanvas[]> Build(string id)
        {
            id = Has(id) ? id : "hero";
            if (Cache.TryGetValue(id, out var cached)) return cached;
            var look = Looks[id]();
            var set = new Dictionary<string, PixelCanvas[]>();
            foreach (var dir in new[] { "down", "up", "right" })
            {
                var frames = new PixelCanvas[1 + WalkFrames];
                frames[0] = Draw(look, new Pose(dir, 0, 0));
                frames[1] = Draw(look, new Pose(dir, 1, 0));
                frames[2] = Draw(look, new Pose(dir, 0, 1));
                frames[3] = Draw(look, new Pose(dir, -1, 0));
                frames[4] = frames[2];
                set[dir] = frames;
            }
            var left = new PixelCanvas[set["right"].Length];
            for (int i = 0; i < left.Length; i++)
            {
                left[i] = new PixelCanvas(W, H);
                left[i].Draw(set["right"][i], 0, 0, mirror: true);
            }
            set["left"] = left;
            return Cache[id] = set;
        }

        // =====================================================================
        // The figure
        // =====================================================================
        static PixelCanvas Draw(Look k, Pose p)
        {
            var c = new PixelCanvas(W, H);
            k.behind?.Invoke(c, p);
            if (p.Side)
            {
                if (p.phase != 0) LegSide(c, k, p, -p.phase, false);
                ArmSide(c, k, p, -p.phase, false);
                LegSide(c, k, p, p.phase, true);
                Torso(c, k, p);
                ArmSide(c, k, p, p.phase, true);
            }
            else
            {
                LegFront(c, k, p, 8, p.phase < 0, true);
                LegFront(c, k, p, 13, p.phase > 0, false);
                Torso(c, k, p);
                ArmFront(c, k, p, 5 - k.wide, p.phase);
                ArmFront(c, k, p, 17 + k.wide, -p.phase);
            }
            Head(c, k, p);
            k.over?.Invoke(c, p);
            Outline(c);
            return c;
        }

        static void LegFront(PixelCanvas c, Look k, Pose p, int x, bool raised, bool leftLeg)
        {
            int top = p.HipY + 1, bottom = raised ? 27 : 28;
            string col = k.pants ?? k.skin, dk = k.pantsDk ?? k.skinDk;
            c.R(x, top, 3, bottom - top + 1, col);
            c.R(x + 2, top, 1, bottom - top + 1, dk);
            int sy = bottom + 1, sx = leftLeg ? x - 1 : x;
            if (k.shoes != null) { c.R(sx, sy, 4, 2, k.shoes); c.R(sx, sy + 1, 4, 1, Shade(k.shoes, -18)); }
            else { c.R(x, sy, 3, 1, k.skin); c.R(sx, sy + 1, 4, 1, k.skinDk); }
        }

        static void LegSide(PixelCanvas c, Look k, Pose p, int mode, bool near)
        {
            string col = near ? k.pants ?? k.skin : k.pantsDk ?? k.skinDk;
            string shoe = k.shoes == null ? (near ? k.skin : k.skinDk) : (near ? k.shoes : Shade(k.shoes, -20));
            int hip = p.HipY;
            if (mode == 0)
            {
                c.R(10, hip + 1, 4, 28 - hip, col);
                c.R(10, 29, 5, 2, shoe);
            }
            else if (mode > 0)          // stepping forward
            {
                c.R(11, hip + 1, 3, 3, col);
                c.R(12, hip + 4, 3, 1, col);
                c.R(13, hip + 5, 3, 24 - hip, col);
                c.R(13, 29, 5, 2, shoe);
            }
            else                        // pushing off behind, heel up
            {
                c.R(10, hip + 1, 3, 3, col);
                c.R(9, hip + 4, 3, 1, col);
                c.R(8, hip + 5, 3, 23 - hip, col);
                c.R(6, 28, 4, 2, shoe);
            }
        }

        static void Torso(PixelCanvas c, Look k, Pose p)
        {
            int t = p.TorsoY;
            int x0 = (p.Side ? 9 : 7) - k.wide, w = (p.Side ? 7 : 10) + 2 * k.wide;
            c.R(x0 + 1, t, w - 2, 1, k.top);
            c.R(x0, t + 1, w, 7, k.top);
            if (p.Side) c.R(x0, t + 1, 1, 7, k.topDk);
            else c.R(x0 + w - 1, t + 1, 1, 7, k.topDk);
            c.R(x0, t + 7, w, 1, k.topDk);
            string hips = k.pants ?? k.skin;
            if (p.Side) c.R(10, p.HipY, 5, 1, hips); else c.R(8, p.HipY, 8, 1, hips);
            if (k.robeTo > 0)
            {
                c.R(x0, t + 8, w, k.robeTo - t - 7, k.top);
                if (p.Side) c.R(x0, t + 8, 1, k.robeTo - t - 7, k.topDk);
                else c.R(x0 + w - 1, t + 8, 1, k.robeTo - t - 7, k.topDk);
                c.R(x0, k.robeTo, w, 1, k.topDk);
            }
        }

        static void ArmFront(PixelCanvas c, Look k, Pose p, int x, int swing)
        {
            int t = p.TorsoY;
            string col = k.sleeve ?? k.skin, dk = k.sleeveDk ?? k.skinDk;
            int len = 6 + (swing > 0 ? 1 : swing < 0 ? -1 : 0);
            if (k.shortSleeves && k.sleeve != null)
            {
                c.R(x, t + 1, 2, 3, col);
                c.R(x, t + 4, 2, len - 3, k.skin);
            }
            else c.R(x, t + 1, 2, len, col);
            c.R(x + 1, t + 1, 1, 2, dk);
            c.R(x, t + 1 + len, 2, 2, k.skin);
        }

        /// <summary>The near or far arm in profile; swing &gt; 0 reaches forward, &lt; 0 back.</summary>
        static void ArmSide(PixelCanvas c, Look k, Pose p, int swing, bool near)
        {
            int t = p.TorsoY;
            string col = near ? k.sleeve ?? k.skin : k.sleeveDk ?? k.skinDk;
            string fore = k.shortSleeves && k.sleeve != null ? (near ? k.skin : k.skinDk) : col;
            string hand = near ? k.skin : k.skinDk;
            c.R(11, t + 1, 2, 3, col);
            if (swing == 0) { c.R(11, t + 4, 2, 3, fore); c.R(11, t + 7, 2, 2, hand); }
            else if (swing > 0) { c.R(12, t + 4, 2, 1, fore); c.R(13, t + 5, 2, 1, fore); c.R(14, t + 6, 2, 2, hand); }
            else { c.R(10, t + 4, 2, 1, fore); c.R(9, t + 5, 2, 1, fore); c.R(8, t + 6, 2, 2, hand); }
        }

        static void Head(PixelCanvas c, Look k, Pose p)
        {
            int y = p.HeadY;
            if (p.Side)
            {
                c.R(11, y + 9, 3, 1, k.skinDk);                   // neck
                c.R(9, y, 7, 9, k.skin);
                c.R(8, y + 1, 9, 7, k.skin);
                c.Px(17, y + 5, k.skin);                          // nose
                c.Px(12, y + 5, k.skinDk);                        // ear
                c.R(15, y + 4, 1, 2, k.eye);
                c.Px(16, y + 7, k.skinDk);                        // mouth
            }
            else
            {
                c.R(10, y + 9, 4, 1, k.skinDk);
                c.R(8, y, 8, 9, k.skin);
                c.R(7, y + 1, 10, 7, k.skin);
                if (p.Front)
                {
                    c.R(9, y + 4, 1, 2, k.eye);
                    c.R(14, y + 4, 1, 2, k.eye);
                    c.R(11, y + 7, 2, 1, k.skinDk);
                }
            }
            HairOn(c, k, p);
        }

        static void HairOn(PixelCanvas c, Look k, Pose p)
        {
            int y = p.HeadY;
            string h = k.hairC, d = k.hairDk;
            switch (k.hair)
            {
                case Hair.None: return;
                case Hair.Buzz:
                    if (p.Front) { c.R(8, y, 8, 2, h); c.R(7, y + 1, 10, 1, h); c.Px(7, y + 2, d); c.Px(16, y + 2, d); }
                    else if (p.Back) { c.R(8, y, 8, 6, h); c.R(7, y + 1, 10, 4, h); c.R(8, y + 5, 8, 1, d); }
                    else { c.R(9, y, 7, 2, h); c.R(8, y + 1, 8, 1, h); c.R(8, y + 2, 4, 3, h); c.R(8, y + 4, 4, 1, d); }
                    return;
            }
            // Short, Messy and Pixie share the cut; they differ at the top and the fringe.
            if (p.Front)
            {
                c.R(8, y, 8, 3, h); c.R(7, y + 1, 10, 2, h);
                c.R(7, y + 3, 1, 2, h); c.R(16, y + 3, 1, 2, h);
                c.R(8, y + 2, 8, 1, d);
                if (k.hair == Hair.Pixie) { c.R(8, y + 3, 5, 1, h); c.Px(8, y + 4, h); }
                else c.R(8, y + 3, 3, 1, h);
                if (k.hair == Hair.Messy) { c.R(7, y + 5, 1, 1, h); c.R(16, y + 5, 1, 1, h); c.R(11, y + 3, 2, 1, h); }
            }
            else if (p.Back)
            {
                c.R(8, y, 8, 7, h); c.R(7, y + 1, 10, 6, h);
                c.R(8, y + 6, 8, 1, d);
                if (k.hair == Hair.Messy) c.R(8, y + 7, 8, 1, h);
            }
            else
            {
                c.R(9, y, 7, 3, h); c.R(8, y + 1, 9, 2, h);
                c.R(8, y + 3, 4, 4, h);
                c.R(8, y + 6, 4, 1, d);
                c.Px(16, y + 3, h);
                if (k.hair == Hair.Messy) c.R(8, y + 7, 3, 1, h);
                if (k.hair == Hair.Pixie) c.R(14, y + 3, 3, 1, h);
            }
            if (k.hair == Hair.Messy)
            {
                c.Px(9, y - 1, h); c.Px(12, y - 1, h); c.Px(14, y - 1, h);
            }
        }

        /// <summary>A one-pixel outline around everything drawn, like the sheets' black line.</summary>
        static void Outline(PixelCanvas c)
        {
            var src = (Color32[])c.Pixels.Clone();
            var ink = PixelCanvas.Col(Ink);
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    int i = y * W + x;
                    if (src[i].a >= 100) continue;
                    bool edge = (x > 0 && src[i - 1].a >= 100) || (x < W - 1 && src[i + 1].a >= 100)
                             || (y > 0 && src[i - W].a >= 100) || (y < H - 1 && src[i + W].a >= 100);
                    if (edge) c.Pixels[i] = ink;
                }
        }

        static string Shade(string hex, int amt)
        {
            var col = PixelCanvas.Col(hex);
            byte S(byte v) => (byte)Mathf.Clamp(v + amt, 0, 255);
            return $"#{S(col.r):x2}{S(col.g):x2}{S(col.b):x2}";
        }

        // =====================================================================
        // The champions (from their character sheets)
        // =====================================================================
        static readonly Dictionary<string, Func<Look>> Looks = new Dictionary<string, Func<Look>>
        {
            // Spellwoven: the hooded hero in the dark blue cloak, with the glowing blade.
            ["hero"] = () => new Look
            {
                skin = "#caa07a", skinDk = "#a07a58", eye = "#9fe0ff", hair = Hair.None,
                top = "#2a3550", topDk = "#1a2236", sleeve = "#2a3550", sleeveDk = "#1a2236",
                pants = "#1a1620", pantsDk = "#100c14", shoes = "#1a1620", robeTo = 26,
                over = (c, p) =>
                {
                    int y = p.HeadY, t = p.TorsoY;
                    Hood(c, p, "#202840", "#33405e");
                    if (p.Front) { c.R(9, y + 4, 1, 1, "#9fe0ff"); c.R(14, y + 4, 1, 1, "#9fe0ff"); }
                    if (!p.Back) c.R(p.Side ? 9 : 8, t + 4, p.Side ? 7 : 8, 1, "#caa84d");     // belt
                    if (p.Front) { c.R(20, t + 1, 1, 7, "#bff6ff"); c.Px(20, t, "#ffffff"); c.Px(19, t + 6, "#caa84d"); c.Px(21, t + 6, "#caa84d"); }
                    if (p.Side) { c.R(7, t + 2, 1, 7, "#bff6ff"); c.Px(7, t + 1, "#ffffff"); }
                },
            },

            // Atanas, the low-cortisol monk: blond, white gi to the knee, black belt, green lanyard and medal, sandals.
            ["atanas"] = () => new Look
            {
                skin = "#f0c8a4", skinDk = "#d4a27e", hair = Hair.Short, hairC = "#e2c470", hairDk = "#b8984a",
                top = "#f2efe6", topDk = "#cbc6ba", sleeve = "#f2efe6", sleeveDk = "#cbc6ba",
                pants = "#e8e4da", pantsDk = "#c4beb0", shoes = "#8a5a3a", robeTo = 25,
                over = (c, p) =>
                {
                    int t = p.TorsoY;
                    if (p.Front)
                    {
                        c.Px(11, t, "#cbc6ba"); c.Px(12, t + 1, "#cbc6ba"); c.Px(11, t + 2, "#cbc6ba");    // the gi's fold
                        c.Px(9, t, "#3a9a4a"); c.Px(14, t, "#3a9a4a"); c.Px(10, t + 1, "#3a9a4a"); c.Px(13, t + 1, "#3a9a4a");
                        c.Px(11, t + 2, "#3a9a4a"); c.Px(12, t + 2, "#3a9a4a");
                        c.R(11, t + 3, 2, 2, "#d8b040");                                                   // the medal
                        c.R(7, t + 5, 10, 1, "#2a2420");                                                   // belt
                        c.R(10, t + 6, 1, 2, "#2a2420"); c.R(13, t + 6, 1, 2, "#2a2420");
                    }
                    else if (p.Back) c.R(7, t + 5, 10, 1, "#2a2420");
                    else { c.R(9, t + 5, 7, 1, "#2a2420"); c.Px(15, t + 1, "#3a9a4a"); c.R(15, t + 2, 1, 2, "#d8b040"); }
                },
            },

            // Gakev: black buzz cut, blue varsity jacket with grey sleeves, the green arm, black trainers.
            ["gakev"] = () => new Look
            {
                skin = "#c8915e", skinDk = "#a47246", hair = Hair.Buzz, hairC = "#1e1a1a", hairDk = "#141010",
                top = "#2a4aa0", topDk = "#1c3474", sleeve = "#c4c4cc", sleeveDk = "#9a9aa4",
                pants = "#1c1c24", pantsDk = "#121218", shoes = "#101014",
                over = (c, p) =>
                {
                    int t = p.TorsoY;
                    string green = "#5a9a3a", greenDk = "#3e7a28";
                    if (p.Front)
                    {
                        c.R(10, t, 4, 1, "#1c3474"); c.R(11, t + 1, 2, 6, "#e8e8f0");                     // collar and zip
                        c.R(17, t + 4, 2, 5 - p.phase, green);                                             // his left forearm and fist
                        c.R(17, t + 3, 2, 1, "#f0f0f0"); c.R(5, t + 6 + p.phase, 2, 1, "#f0f0f0");         // cuffs
                    }
                    else if (p.Back)
                    {
                        c.R(8, t + 2, 8, 3, "#1c3474"); c.R(9, t + 3, 6, 1, "#c4c4cc");                     // the back panel stripe
                        c.R(5, t + 4, 2, 5 + p.phase, green); c.R(5, t + 3, 2, 1, "#f0f0f0");
                    }
                    else
                    {
                        // the near arm is the green one
                        if (p.phase == 0) { c.R(11, t + 4, 2, 3, green); c.R(11, t + 7, 2, 2, greenDk); }
                        else if (p.phase > 0) { c.R(13, t + 5, 2, 1, green); c.R(14, t + 6, 2, 2, greenDk); }
                        else { c.R(9, t + 5, 2, 1, green); c.R(8, t + 6, 2, 2, greenDk); }
                        c.R(14, t, 2, 1, "#1c3474");
                    }
                    if (!p.Back) { c.Px(p.Side ? 14 : 8, 30, "#e8e8e8"); if (!p.Side) c.Px(15, 30, "#e8e8e8"); }   // white soles
                },
            },

            // Nikolay, the barrel knight: open helmet with a beer-foam plume, barrel armour, mug and shield.
            ["nikolay"] = () => new Look
            {
                skin = "#7a4e32", skinDk = "#5c3822", eye = "#120c0a", hair = Hair.None, wide = 1,
                top = "#8a5a30", topDk = "#6a4220", sleeve = "#7a4a28", sleeveDk = "#5a341a",
                pants = "#5a3a22", pantsDk = "#422816", shoes = "#6a4024",
                over = (c, p) =>
                {
                    int y = p.HeadY, t = p.TorsoY;
                    string steel = "#9aa0aa", steelDk = "#6c727c", copper = "#c87a40";
                    // helmet
                    if (p.Back) { c.R(8, y, 8, 9, steel); c.R(7, y + 1, 10, 7, steel); c.R(8, y + 7, 8, 1, steelDk); }
                    else if (p.Front)
                    {
                        c.R(8, y, 8, 4, steel); c.R(7, y + 1, 10, 7, steel); c.R(8, y + 8, 8, 1, steel);
                        c.R(9, y + 3, 6, 5, "#7a4e32"); c.R(9, y + 4, 1, 2, "#120c0a"); c.R(14, y + 4, 1, 2, "#120c0a");
                        c.R(9, y + 3, 6, 1, steelDk); c.R(11, y + 7, 2, 1, "#5c3822");
                    }
                    else
                    {
                        c.R(9, y, 7, 4, steel); c.R(8, y + 1, 9, 7, steel); c.R(9, y + 8, 7, 1, steel);
                        c.R(13, y + 3, 4, 5, "#7a4e32"); c.R(15, y + 4, 1, 2, "#120c0a"); c.Px(17, y + 5, "#7a4e32");
                        c.R(13, y + 3, 4, 1, steelDk);
                    }
                    // the foam plume
                    int px = p.Side ? 9 : 13;
                    c.R(px, y - 2, 3, 2, "#f0c040"); c.Px(px + 3, y - 1, "#f0c040"); c.Px(px + 1, y - 3, "#f8e8a0"); c.Px(px, y, "#c89020");
                    // barrel bands and knee caps
                    int x0 = p.Side ? 8 : 6, w = p.Side ? 9 : 12;
                    c.R(x0, t + 1, w, 1, copper); c.R(x0, t + 5, w, 1, copper);
                    if (!p.Side) { c.R(8, 25, 3, 1, copper); c.R(13, 25, 3, 1, copper); }
                    if (p.Front)
                    {
                        // the mug in his right hand, the shield on his left arm
                        c.R(0, t + 6, 3, 4, "#e0a020"); c.R(0, t + 5, 3, 1, "#f8f4e8"); c.Px(1, t + 7, "#f8d060"); c.R(3, t + 7, 1, 2, "#c0c0c0");
                        c.R(16, t + 1, 1, 8, Ink);                                                           // the shield's near rim
                        c.R(17, t + 1, 6, 8, "#9a6a3a"); c.R(18, t, 4, 10, "#9a6a3a");
                        c.R(17, t + 1, 1, 8, copper); c.R(22, t + 1, 1, 8, copper); c.R(18, t, 4, 1, copper); c.R(18, t + 9, 4, 1, copper);
                        c.R(19, t + 3, 2, 3, "#f8f4e8"); c.Px(21, t + 4, "#f8f4e8");                         // a beer mug painted on it
                    }
                    else if (p.Back)
                    {
                        c.R(1, t + 1, 6, 8, "#7a4e28"); c.R(2, t, 4, 10, "#7a4e28"); c.R(1, t + 1, 1, 8, copper); c.R(6, t + 1, 1, 8, copper);
                    }
                    else
                    {
                        c.R(12, t + 1, 5, 8, "#8a5a30"); c.R(13, t, 3, 10, "#8a5a30"); c.R(16, t + 1, 1, 8, copper); c.R(13, t + 4, 2, 2, "#f8f4e8");
                    }
                },
            },

            // Pupesh, the melon warrior: melon helmet, cantaloupe armour, pale green limbs, a wooden sword.
            ["pupesh"] = () => new Look
            {
                skin = "#f0c8a0", skinDk = "#d4a07a", hair = Hair.None,
                top = "#e0a040", topDk = "#b87a28", sleeve = "#d8e8b0", sleeveDk = "#a8c080",
                pants = "#d8e8b0", pantsDk = "#a8c080", shoes = "#c8d8a0",
                over = (c, p) =>
                {
                    int y = p.HeadY, t = p.TorsoY;
                    string rind = "#d8e8b0", rindDk = "#a8c080", orange = "#e89a40";
                    if (p.Back) { c.R(8, y, 8, 9, rind); c.R(7, y + 1, 10, 7, rind); c.R(11, y, 2, 8, orange); c.R(8, y + 7, 8, 1, rindDk); }
                    else if (p.Front)
                    {
                        c.R(8, y, 8, 3, rind); c.R(7, y + 1, 10, 2, rind); c.R(7, y + 3, 2, 5, rind); c.R(15, y + 3, 2, 5, rind);
                        c.R(11, y, 2, 3, orange); c.R(7, y + 7, 2, 1, rindDk); c.R(15, y + 7, 2, 1, rindDk);
                        c.R(9, y + 3, 6, 1, "#5a3a22");                                                      // his hair under the rim
                    }
                    else
                    {
                        c.R(9, y, 7, 3, rind); c.R(8, y + 1, 9, 2, rind); c.R(8, y + 3, 5, 5, rind); c.R(12, y, 2, 3, orange);
                        c.R(8, y + 7, 5, 1, rindDk);
                    }
                    // the cantaloupe netting on the chest
                    int x0 = p.Side ? 10 : 8, x1 = p.Side ? 15 : 16;
                    for (int yy = t + 1; yy < t + 7; yy += 2)
                        for (int xx = x0 + (yy / 2) % 2; xx < x1; xx += 2) c.Px(xx, yy, "#f4c878");
                    if (!p.Side) { c.R(8, 25, 3, 1, orange); c.R(13, 25, 3, 1, orange); }
                    if (p.Front) { c.R(3, t + 6, 1, 9, "#b89070"); c.R(2, t + 7, 3, 1, "#5a3a20"); c.Px(3, t + 15, "#d8c0a0"); }
                    else if (p.Back) c.R(20, t + 6, 1, 9, "#8a6a50");
                    else { c.R(14, t + 7, 7, 1, "#b89070"); c.R(13, t + 6, 1, 3, "#5a3a20"); c.Px(21, t + 7, "#d8c0a0"); }
                },
            },

            // Ristyo: short brown hair, blue polo with a green lanyard, a cigarette and a beer.
            ["ristyo"] = () => new Look
            {
                skin = "#f0c4a0", skinDk = "#d29e7a", hair = Hair.Short, hairC = "#5a3a22", hairDk = "#3e2614",
                top = "#2a5ab8", topDk = "#1e4290", sleeve = "#2a5ab8", sleeveDk = "#1e4290", shortSleeves = true,
                pants = "#2a2e40", pantsDk = "#1c1f2c", shoes = "#4a3020",
                over = (c, p) =>
                {
                    int y = p.HeadY, t = p.TorsoY;
                    string lime = "#58c040";
                    if (p.Front)
                    {
                        c.R(10, t, 4, 1, "#3a6ad0");                                                         // collar
                        c.Px(9, t + 1, lime); c.Px(14, t + 1, lime); c.Px(10, t + 2, lime); c.Px(13, t + 2, lime);
                        c.Px(11, t + 3, lime); c.Px(12, t + 3, lime); c.R(11, t + 4, 2, 2, "#f0f0f0");       // lanyard and badge
                        c.Px(14, t + 3, "#ffffff");                                                         // the logo
                        c.R(13, y + 7, 2, 1, "#f0f0f0"); c.Px(15, y + 7, "#ff8030");                          // cigarette
                        c.R(19, t + 6, 3, 4, "#e8a020"); c.R(19, t + 5, 3, 1, "#f8f4e8"); c.Px(22, t + 7, "#c0c0c0");   // beer
                    }
                    else if (p.Back) c.R(9, t, 6, 1, "#3a6ad0");
                    else
                    {
                        c.R(14, t, 2, 1, "#3a6ad0"); c.Px(15, t + 1, lime); c.Px(15, t + 2, lime);
                        c.Px(17, y + 7, "#f0f0f0"); c.Px(18, y + 7, "#ff8030");                              // cigarette
                    }
                },
            },

            // Iliqn Puh, Mecho the bear: bear ears, red shirt, yellow fur, a blue backpack with the honey hose.
            ["iliqnpuh"] = () => new Look
            {
                skin = "#e2b48a", skinDk = "#c0906a", hair = Hair.Short, hairC = "#1a1414", hairDk = "#100c0c",
                top = "#d02828", topDk = "#a01818", sleeve = "#f0b020", sleeveDk = "#c88a10",
                pants = "#f0b020", pantsDk = "#c88a10", shoes = "#f0b020",
                behind = (c, p) =>
                {
                    int t = p.TorsoY;
                    if (p.Side) { c.R(5, t, 5, 7, "#3a6ad0"); c.R(5, t + 6, 5, 1, "#2a4ea0"); }
                },
                over = (c, p) =>
                {
                    int y = p.HeadY, t = p.TorsoY;
                    string fur = "#f0b020", furDk = "#c88a10";
                    // ears
                    if (p.Side) { c.R(9, y - 2, 3, 3, fur); c.Px(10, y - 1, furDk); }
                    else { c.R(6, y - 2, 3, 3, fur); c.R(15, y - 2, 3, 3, fur); c.Px(7, y - 1, furDk); c.Px(16, y - 1, furDk); }
                    // yellow belly below the short shirt
                    if (p.Side) c.R(10, t + 6, 6, 2, fur); else c.R(8, t + 6, 8, 2, fur);
                    if (p.Front) { c.R(8, t, 1, 6, "#2a4ea0"); c.R(15, t, 1, 6, "#2a4ea0"); }               // backpack straps
                    else if (p.Back)
                    {
                        c.R(7, t, 10, 7, "#3a6ad0"); c.R(7, t + 6, 10, 1, "#2a4ea0"); c.R(9, t + 2, 6, 2, "#5a8ae0");
                        c.R(16, t + 6, 2, 3, "#e0a020");                                                     // the hose
                    }
                    else c.R(12, t, 1, 4, "#2a4ea0");
                },
            },

            // Daniil & Vasil, drawn as Vasil: messy brown hair, red varsity jacket over a blue shirt, jeans.
            ["daniilvasil"] = () => new Look
            {
                skin = "#f0c8a0", skinDk = "#d4a07a", hair = Hair.Messy, hairC = "#6a4024", hairDk = "#4a2a14",
                top = "#c02a2a", topDk = "#901c1c", sleeve = "#c02a2a", sleeveDk = "#901c1c",
                pants = "#262a3a", pantsDk = "#1a1d28", shoes = "#5a3620",
                over = (c, p) =>
                {
                    int t = p.TorsoY;
                    if (p.Front)
                    {
                        c.R(11, t, 2, 8, "#2a5ab8");                                                         // the shirt under the open jacket
                        c.R(5, t + 6, 2, 1, "#f0f0f0"); c.R(17, t + 6, 2, 1, "#f0f0f0");                     // cuffs
                        c.R(13, t + 2, 2, 2, "#f0c040");                                                     // the letter patch
                    }
                    else if (p.Back) { c.R(8, t + 2, 8, 1, "#f0f0f0"); c.R(10, t + 3, 4, 2, "#f0c040"); }
                    else c.R(15, t + 1, 1, 6, "#2a5ab8");
                },
            },

            // The Storyteller: buzzed dark hair, bare round belly, a white towel with a blue stripe, bare feet.
            ["storyteller"] = () => new Look
            {
                skin = "#f2b8a0", skinDk = "#d8957c", hair = Hair.Buzz, hairC = "#2a2220", hairDk = "#1c1614", wide = 1,
                top = "#f2b8a0", topDk = "#d8957c", sleeve = null, pants = null, shoes = null,
                over = (c, p) =>
                {
                    int t = p.TorsoY;
                    if (p.Front)
                    {
                        c.Px(9, p.HeadY + 6, "#f09090"); c.Px(14, p.HeadY + 6, "#f09090");                   // rosy cheeks
                        c.R(8, t + 3, 8, 1, "#e6a890"); c.Px(11, t + 5, "#c07a64");                          // belly crease, navel
                        c.R(6, t + 7, 12, 4, "#f4f4f0"); c.R(6, t + 9, 12, 1, "#6aa0d0"); c.R(6, t + 10, 12, 1, "#cfcfc8");
                    }
                    else if (p.Back) { c.R(6, t + 7, 12, 4, "#f4f4f0"); c.R(6, t + 9, 12, 1, "#6aa0d0"); c.R(6, t + 10, 12, 1, "#cfcfc8"); }
                    else
                    {
                        c.R(16, t + 3, 1, 4, "#f2b8a0"); c.Px(17, t + 4, "#f2b8a0"); c.Px(17, t + 5, "#d8957c");  // the belly
                        c.R(8, t + 7, 10, 4, "#f4f4f0"); c.R(8, t + 9, 10, 1, "#6aa0d0"); c.R(8, t + 10, 10, 1, "#cfcfc8");
                    }
                },
            },

            // The Fairy (Mitko): pixie cut, pink corset and tutu, bare legs, gauzy wings and a star wand.
            ["fairy"] = () => new Look
            {
                skin = "#f0c4a4", skinDk = "#d4a080", hair = Hair.Pixie, hairC = "#2a2020", hairDk = "#1a1414",
                top = "#f080b8", topDk = "#c85a90", sleeve = null, pants = null, shoes = null,
                behind = (c, p) =>
                {
                    int t = p.TorsoY;
                    if (p.Side) Wings(c, t, side: true);
                    else if (p.Front) Wings(c, t, side: false);
                },
                over = (c, p) =>
                {
                    int t = p.TorsoY;
                    string tutu = "#f8a0d0", tutuDk = "#e078b0";
                    // bare shoulders above the corset
                    if (p.Side) c.R(10, t, 5, 1, "#f0c4a4"); else c.R(8, t, 8, 1, "#f0c4a4");
                    if (p.Side) { c.R(8, t + 5, 9, 3, tutu); c.R(8, t + 7, 9, 1, tutuDk); }
                    else { c.R(5, t + 5, 14, 3, tutu); c.R(5, t + 7, 14, 1, tutuDk); c.Px(7, t + 6, "#ffffff"); c.Px(15, t + 6, "#ffffff"); }
                    if (p.Back) Wings(c, t, side: false);
                    if (p.Front) { c.R(3, t + 2, 1, 6, "#8a5a3a"); c.Px(3, t, "#fff080"); c.R(2, t + 1, 3, 1, "#fff080"); c.Px(3, t + 2, "#fff080"); }
                    else if (p.Side) { c.R(16, t + 3, 3, 1, "#8a5a3a"); c.R(19, t + 2, 1, 3, "#fff080"); c.R(18, t + 3, 3, 1, "#fff080"); }
                },
            },
        };

        // The Fairy's gauzy wings: an upper and a lower lobe each side (one pair behind her in profile).
        static readonly string[] UpperWing = { "..xxx.", ".xxxxx", "xxvxxx", "xxxvxx", ".xxxvx", "..xxxx", "....xx" };
        static readonly string[] LowerWing = { ".xxxx", "xxvxx", ".xxx.", "..x.." };

        static void Wings(PixelCanvas c, int t, bool side)
        {
            void Lobe(string[] rows, int x0, int y0, bool mirror)
            {
                for (int r = 0; r < rows.Length; r++)
                    for (int k = 0; k < rows[r].Length; k++)
                    {
                        char ch = rows[r][k];
                        if (ch == '.') continue;
                        c.Px(x0 + (mirror ? rows[r].Length - 1 - k : k), y0 + r, ch == 'v' ? "#ffffff" : "#d2e8ffe8");
                    }
            }
            if (side) { Lobe(UpperWing, 3, t - 4, false); Lobe(LowerWing, 4, t + 3, false); return; }
            Lobe(UpperWing, 1, t - 4, false); Lobe(LowerWing, 2, t + 3, false);
            Lobe(UpperWing, 17, t - 4, true); Lobe(LowerWing, 17, t + 3, true);
        }

        // A hood over the head with the face showing in front, a pointed back and the hem on the shoulders.
        static void Hood(PixelCanvas c, Pose p, string hood, string hoodLt)
        {
            int y = p.HeadY;
            if (p.Back) { c.R(8, y - 1, 8, 10, hood); c.R(7, y + 1, 10, 8, hood); c.R(8, y - 1, 8, 1, hoodLt); return; }
            if (p.Front)
            {
                c.R(8, y - 1, 8, 4, hood); c.R(7, y + 1, 10, 8, hood); c.R(8, y - 1, 8, 1, hoodLt);
                c.R(9, y + 3, 6, 5, "#8a6a50"); c.R(9, y + 3, 6, 1, "#5a4a3a");
                return;
            }
            c.R(8, y - 1, 8, 4, hood); c.R(7, y + 1, 7, 8, hood); c.R(8, y - 1, 8, 1, hoodLt);
            c.R(14, y + 3, 3, 5, "#8a6a50"); c.Px(15, y + 4, "#9fe0ff");
        }

        // =====================================================================
        // The scared face shown over enemies Momiche frightens
        // =====================================================================
        static PixelCanvas _fear;

        /// <summary>An 11 × 11 "fearful face": a yellow face going blue at the top, wide eyes, an open mouth.</summary>
        public static PixelCanvas FearFace()
        {
            if (_fear != null) return _fear;
            var c = new PixelCanvas(11, 11);
            c.R(3, 0, 5, 11, "#f8c838"); c.R(1, 1, 9, 9, "#f8c838"); c.R(0, 3, 11, 5, "#f8c838");
            c.R(3, 0, 5, 2, "#6aa0e8"); c.R(1, 1, 9, 2, "#6aa0e8"); c.R(1, 3, 9, 1, "#9ac0f0");
            c.R(3, 4, 2, 2, "#ffffff"); c.R(6, 4, 2, 2, "#ffffff"); c.Px(4, 5, "#2a1c18"); c.Px(6, 5, "#2a1c18");
            c.R(4, 7, 3, 2, "#6a2a2a"); c.R(4, 8, 3, 1, "#c84848");
            c.Px(2, 3, "#2a1c18"); c.Px(8, 3, "#2a1c18");                // raised brows
            return _fear = c;
        }
    }
}
