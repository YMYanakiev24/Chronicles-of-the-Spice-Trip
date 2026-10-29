/*
 * CharacterGallery.cs
 * ----------------------------------------------------------------------------
 * Spice Trip ▸ Export Character Gallery: one picture with every playable
 * character's walking frames (front, back, left, right; standing then the walk
 * cycle), to check the figures at a glance. Writes Captures/characters.png.
 *
 *   Unity.exe -batchmode -quit -projectPath . -executeMethod SpiceTrip.EditorTools.CharacterGallery.Export
 * ----------------------------------------------------------------------------
 */

using System.IO;
using System.Linq;
using SpiceTrip.Art;
using SpiceTrip.Data;
using UnityEditor;
using UnityEngine;

namespace SpiceTrip.EditorTools
{
    public static class CharacterGallery
    {
        const int CellW = 72, CellH = 96, Gap = 6;

        [MenuItem("Spice Trip/Export Character Gallery")]
        public static void Export()
        {
            var ids = GameDatabase.Instance.characters.Where(c => !c.empty).Select(c => c.id).ToList();
            int cols = ChampionFigures.Directions.Length * (1 + ChampionFigures.WalkFrames);
            int w = cols * (CellW + Gap) + Gap, h = ids.Count * (CellH + Gap) + Gap;
            var px = Enumerable.Repeat(new Color32(38, 34, 52, 255), w * h).ToArray();
            for (int row = 0; row < ids.Count; row++)
            {
                var hero = HeroSprites.For(ids[row]);
                int col = 0;
                foreach (var dir in ChampionFigures.Directions)
                    for (int f = 0; f < hero.FrameCount; f++, col++)
                        Blit(px, w, hero.Frame(dir, f), Gap + col * (CellW + Gap), Gap + row * (CellH + Gap));
            }
            var tex = new Texture2D(w, h, TextureFormat.RGBA32, false);
            // rows were filled top-down; textures run bottom-up
            var flipped = new Color32[px.Length];
            for (int y = 0; y < h; y++) System.Array.Copy(px, y * w, flipped, (h - 1 - y) * w, w);
            tex.SetPixels32(flipped);
            var path = Path.GetFullPath("Captures/characters.png");
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllBytes(path, tex.EncodeToPNG());
            Object.DestroyImmediate(tex);
            Debug.Log($"Character gallery ({ids.Count} characters) written to {path}");
        }

        // Nearest-neighbour copy of a sprite into a cell, alpha-blended over the backdrop (top-left origin).
        static void Blit(Color32[] px, int w, Sprite s, int ox, int oy)
        {
            var r = s.rect;
            var tex = s.texture;
            for (int y = 0; y < CellH; y++)
                for (int x = 0; x < CellW; x++)
                {
                    int sx = (int)(r.x + x * r.width / CellW), sy = (int)(r.y + r.height - 1 - y * r.height / CellH);
                    Color32 c = tex.GetPixel(sx, sy);
                    if (c.a == 0) continue;
                    int i = (oy + y) * w + ox + x;
                    px[i] = Color32.Lerp(px[i], new Color32(c.r, c.g, c.b, 255), c.a / 255f);
                }
        }
    }
}
