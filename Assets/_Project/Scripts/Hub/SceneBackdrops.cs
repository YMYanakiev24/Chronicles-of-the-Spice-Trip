/*
 * SceneBackdrops.cs
 * ----------------------------------------------------------------------------
 * The procedural pixel scenes from Scenes.js, drawn on a 320×180 buffer.
 * They only appear when the painted art (corridor/shop/path images) is
 * missing, exactly as in the web version, and animate their torches, runes
 * and Marin's bob.
 * ----------------------------------------------------------------------------
 */

using SpiceTrip.Art;
using UnityEngine;

namespace SpiceTrip.Hub
{
    public static class SceneBackdrops
    {
        public const int BW = 320, BH = 180;   // scene buffer resolution

        static Color32 C(string s) => PixelCanvas.Col(s);
        static Color32 A(int r, int g, int b, double a) => new Color32((byte)r, (byte)g, (byte)b, (byte)Mathf.Clamp(Mathf.RoundToInt((float)a * 255), 0, 255));

        // Simple dark backdrop used only if the path image is missing.
        public static void DrawPathFallback(PixelCanvas c, float t)
        {
            c.Clear();
            c.FillVertical(0, 0, BW, BH, (0, C("#14102a")), (1, C("#0c1410")));
            c.FillRect(0, BH * 0.7, BW, BH * 0.3, C("#0a1410"));
            c.Text("THE PATH", BW / 2, 12, C("#caa84d"), center: true);
        }

        static void Bricks(PixelCanvas c, int x, int y, int w, int h, string a, string b, string mortar)
        {
            c.FillRect(x, y, w, h, b);
            for (int row = 0; row * 8 < h; row++)
            {
                int yy = y + row * 8, off = (row % 2) * 8;
                for (int bx = x - off; bx < x + w; bx += 16)
                {
                    c.FillRect(bx + 1, yy + 1, 14, 6, a);
                    c.FillRect(bx + 1, yy + 1, 14, 1, A(255, 255, 255, 0.05));
                }
                c.FillRect(x, yy + 7, w, 1, mortar);
            }
        }

        static void Torch(PixelCanvas c, int x, int y, float t)
        {
            c.FillRect(x - 1, y, 3, 8, "#2a1c10");          // bracket
            c.FillRect(x - 2, y + 7, 5, 2, "#1a120a");
            c.Draw(PixelArt.Flame((int)(t * 10)), x - 7, y - 12);   // flame sprite
            // glow
            c.FillRadial(x - 26, y - 30, 52, 52, x, y - 4, 26, A(255, 170, 80, 0.30), A(255, 170, 80, 0));
        }

        public static void DrawCorridor(PixelCanvas c, float t)
        {
            c.Clear();
            c.FillRect(0, 0, BW, BH, "#0b0a10");
            // ceiling beam
            c.FillRect(0, 0, BW, 16, "#3a2a18");
            for (int x = 0; x < BW; x += 24) c.FillRect(x, 0, 2, 16, "#2a1d10");
            // walls
            Bricks(c, 0, 16, BW, 120, "#4a4654", "#33303c", "#1c1a22");
            // floor
            Bricks(c, 0, 136, BW, 44, "#3a3744", "#2a2832", "#17151c");

            // rug with a magic circle
            c.FillRect(96, 150, 128, 26, "#5a2436");
            c.StrokeEllipse(160, 163, 44, 9, C("#caa84d"));
            c.StrokeEllipse(160, 163, 30, 6, C("#caa84d"));

            // Left: Marin's runed door (glowing arch)
            ArchDoor(c, 40, 40, 64, 96, t, true);
            // Right: changing room (curtain)
            CurtainDoor(c, 216, 48, 64, 88);

            // torches
            Torch(c, 130, 60, t);
            Torch(c, 196, 60, t + 1.3f);
            Torch(c, 18, 64, t + 0.6f);
            Torch(c, 302, 64, t + 2.1f);

            // a couple of framed paintings + a hanging sword
            c.FillRect(150, 70, 14, 18, "#caa84d"); c.FillRect(151, 71, 12, 16, "#1a2a3a");
            c.FillRect(176, 72, 14, 16, "#caa84d"); c.FillRect(177, 73, 12, 14, "#2a1a2a");
            c.FillRect(118, 96, 2, 22, "#b8c0cc"); c.FillRect(116, 116, 6, 3, "#caa84d");
        }

        static readonly string Runes = "ᚠᛉᛟᚹᛞᚱ";

        static void ArchDoor(PixelCanvas c, int x, int y, int w, int h, float t, bool glow)
        {
            // arch frame
            c.FillRect(x - 4, y, w + 8, h + 4, "#5a4632");
            c.FillRect(x, y + 8, w, h - 8, "#3a2c1c");
            // door wood
            c.FillRect(x + 4, y + 12, w - 8, h - 14, "#4a3420");
            for (int i = 0; i < 3; i++) c.FillRect(x + 6 + i * ((w - 12) / 3f), y + 14, 1, h - 18, "#5a4026");
            if (!glow) return;
            // runes (glowing)
            float pulse = 0.5f + Mathf.Sin(t * 3) * 0.4f;
            var runeCol = A(140, 200, 255, pulse);
            for (int i = 0; i < 6; i++)
            {
                int rx = x + (i % 2 == 1 ? -3 : w + 3);
                int ry = y + 18 + i * (h / 6) - 5;
                c.Text(Runes[i].ToString(), rx - 1, ry, runeCol);
            }
            // fairy lights along the arch
            for (int i = 0; i <= 10; i++)
            {
                float a = Mathf.PI * (i / 10f);
                float fx = x + w / 2f - Mathf.Cos(a) * (w / 2f + 4);
                float fy = y + 8 - Mathf.Sin(a) * 8;
                float tw = 0.4f + Mathf.Abs(Mathf.Sin(t * 4 + i)) * 0.6f;
                c.FillRect(fx, fy, 2, 2, A(255, 235, 150, tw));
            }
            // inner glow
            c.FillRadial(x - 20, y - 20, w + 40, h + 40, x + w / 2f, y + h / 2f, 40, A(120, 170, 255, 0.18), A(120, 170, 255, 0));
        }

        static void CurtainDoor(PixelCanvas c, int x, int y, int w, int h)
        {
            c.FillRect(x - 4, y - 4, w + 8, h + 6, "#3a2c1c");  // frame
            c.FillRect(x, y, w, h, "#120e16");                  // dark opening
            // purple curtain pulled to one side
            c.FillRect(x + w - 22, y, 22, h, "#5a2a6a");
            for (int i = 0; i < 4; i++) c.FillRect(x + w - 20 + i * 5, y, 2, h, "#6a3a7e");
            // a robe on a peg + stool
            c.FillRect(x + 6, y + 14, 12, 22, "#7a6a4a");
            c.FillRect(x + 10, y + h - 12, 10, 4, "#3a2a18"); c.FillRect(x + 12, y + h - 8, 2, 6, "#3a2a18"); c.FillRect(x + 16, y + h - 8, 2, 6, "#3a2a18");
        }

        public static void DrawShop(PixelCanvas c, float t)
        {
            c.Clear();
            c.FillRect(0, 0, BW, BH, "#0b0a10");
            c.FillRect(0, 0, BW, 16, "#3a2a18");
            Bricks(c, 0, 16, BW, 120, "#4a4654", "#33303c", "#1c1a22");
            Bricks(c, 0, 136, BW, 44, "#3a3744", "#2a2832", "#17151c");

            // Left: the same runed door (continuity with the corridor)
            ArchDoor(c, 26, 44, 58, 90, t, true);
            // Right: tall apothecary shelves stocked with potions + produce
            Shelf(c, 180, 34, 128, t);

            // Counter table with an open spellbook + mortar + candles
            c.FillRect(96, 118, 84, 30, "#4a3420");
            c.FillRect(96, 118, 84, 3, "#3a2818");
            // open book
            c.FillRect(108, 110, 26, 12, "#e8dcc0");
            c.FillRect(120, 110, 2, 12, "#caa84d");
            c.FillRect(110, 113, 9, 1, "#9a7a40"); c.FillRect(123, 113, 9, 1, "#9a7a40");
            c.FillRect(110, 116, 9, 1, "#9a7a40"); c.FillRect(123, 116, 9, 1, "#9a7a40");
            // candle
            c.Draw(PixelArt.Prop("candle"), 150, 104);
            float fl = 0.5f + Mathf.Sin(t * 8) * 0.5f;
            c.FillRect(152, 100, 2, 3, A(255, 200, 120, fl));

            // Marin behind the counter
            MarinSprite(c, 150, 64, t);

            // torches
            Torch(c, 110, 60, t);
            Torch(c, 300, 58, t + 1.1f);
        }

        static void Shelf(PixelCanvas c, int x, int y, int h, float t)
        {
            c.FillRect(x, y, 120, h, "#4a3420");
            c.FillRect(x, y, 120, 2, "#3a2818");
            string[] potColors = { "#ff5a8a", "#5ad0ff", "#9a6bff", "#76e06b", "#ffd24a", "#ff8a3a" };
            for (int row = 0; row < 4; row++)
            {
                float sy = y + 6 + row * (h / 4f);
                c.FillRect(x, sy + 18, 120, 3, "#2a1d10");   // plank
                for (int i = 0; i < 9; i++)
                {
                    int px = x + 6 + i * 13;
                    if (row == 2)
                    {
                        // a row of cabbages/herbs for charm
                        c.FillRect(px, sy + 10, 9, 8, "#3f7d3a");
                        c.FillRect(px + 1, sy + 11, 7, 3, "#5fa04a");
                    }
                    else
                    {
                        string col = potColors[(i + row) % potColors.Length];
                        c.FillRect(px + 1, sy + 6, 7, 12, "#2a2230");  // bottle
                        c.FillRect(px + 2, sy + 9, 5, 8, col);
                        float gl = 0.4f + Mathf.Abs(Mathf.Sin(t * 2 + i + row)) * 0.4f;
                        c.FillRect(px + 2, sy + 9, 2, 3, A(255, 255, 255, gl * 0.5f));
                        c.FillRect(px + 3, sy + 5, 3, 2, "#caa84d");   // cork
                    }
                }
            }
        }

        /// <summary>Marin standing in his shop (drawn on a 48×76 buffer over the shop photo).</summary>
        public static void DrawMarin(PixelCanvas c, float t)
        {
            c.Clear();
            MarinSprite(c, 24, 28, t);
        }

        // A little pixel wizard: pointed blue hat, face, purple robe with gold trim.
        static void MarinSprite(PixelCanvas c, float x, float y, float t)
        {
            y += Mathf.Sin(t * 2) * 1;
            // robe
            c.FillRect(x - 8, y + 14, 16, 26, "#5a2e8a");
            c.FillRect(x - 8, y + 14, 2, 26, "#caa84d"); c.FillRect(x + 6, y + 14, 2, 26, "#caa84d");
            // arms
            c.FillRect(x - 12, y + 16, 4, 14, "#5a2e8a"); c.FillRect(x + 8, y + 16, 4, 14, "#5a2e8a");
            // head
            c.FillRect(x - 5, y + 4, 10, 11, "#d8a878");
            // beard
            c.FillRect(x - 5, y + 11, 10, 5, "#cfcfd6"); c.FillRect(x - 3, y + 15, 6, 3, "#cfcfd6");
            // eyes
            c.FillRect(x - 3, y + 8, 1, 2, "#1a1414"); c.FillRect(x + 2, y + 8, 1, 2, "#1a1414");
            // hat
            c.FillTriangle(new Vector2(x - 7, y + 5), new Vector2(x + 7, y + 5), new Vector2(x + 1, y - 12), C("#2a3aa0"));
            c.FillRect(x - 8, y + 4, 16, 3, "#3a4ac0");
            // star on the hat
            float tw = 0.5f + Mathf.Abs(Mathf.Sin(t * 4)) * 0.5f;
            c.FillRect(x - 2, y - 4, 3, 3, A(255, 230, 140, tw));
            // wand with a glowing tip
            c.FillRect(x + 11, y + 6, 2, 12, "#3a2a18");
            c.FillRect(x + 10, y + 4, 4, 4, A(180, 220, 255, tw));
        }
    }
}
