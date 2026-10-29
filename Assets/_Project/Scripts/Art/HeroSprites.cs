/*
 * HeroSprites.cs
 * ----------------------------------------------------------------------------
 * The player's walking figure, shared by the boss arena and the Dungeon: the
 * chosen champion seen from the front, the back and both sides, standing or
 * walking. A champion with a walk sheet (assets/walk-<id>.png: rows down, up,
 * right; the standing frame then the walk cycle; cut by
 * Tools~/cut-walk-sheet.ps1) walks with those frames. Every other champion is
 * the pixel figure drawn for them in ChampionFigures.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace SpiceTrip.Art
{
    public sealed class HeroSprites
    {
        /// <summary>The drawn figures' size; every frame (drawn or from a sheet) is this 3:4 shape.</summary>
        public const int W = ChampionFigures.W, H = ChampionFigures.H;

        static readonly Dictionary<string, HeroSprites> Cache = new Dictionary<string, HeroSprites>();

        public static HeroSprites For(string championId)
        {
            championId ??= "hero";
            if (Cache.TryGetValue(championId, out var s)) return s;
            return Cache[championId] = FromWalkSheet(championId) ?? new HeroSprites(ChampionFigures.Build(championId));
        }

        /// <summary>Forgets loaded figures, so a replaced walk sheet is read again (tests and art swaps).</summary>
        public static void ClearCache() => Cache.Clear();

        /// <summary>Do these frames come from the champion's walk sheet (rather than a drawn figure)?</summary>
        public bool FromSheet { get; }

        readonly Dictionary<string, Sprite[]> _frames = new Dictionary<string, Sprite[]>();

        HeroSprites(Dictionary<string, PixelCanvas[]> set)
        {
            foreach (var kv in set)
            {
                var sprites = new Sprite[kv.Value.Length];
                for (int i = 0; i < sprites.Length; i++)
                {
                    var tex = kv.Value[i].ToTexture(null, FilterMode.Point);
                    sprites[i] = Sprite.Create(tex, new Rect(0, 0, tex.width, tex.height), new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect);
                }
                _frames[kv.Key] = sprites;
            }
        }

        HeroSprites(Dictionary<string, Sprite[]> frames)
        {
            _frames = frames;
            FromSheet = true;
        }

        static readonly string[] SheetRows = { "down", "up", "right", "left" };

        /// <summary>Loads assets/walk-&lt;id&gt;.png and adds a mirrored "left" row; null if there's no usable sheet.</summary>
        static HeroSprites FromWalkSheet(string id)
        {
            var path = Path.Combine(ArtLibrary.Folder, $"walk-{id}.png");
            if (!File.Exists(path)) return null;
            var src = new Texture2D(2, 2, TextureFormat.RGBA32, false);
            int cellH = 0, cellW = 0, cols = 0;
            if (src.LoadImage(File.ReadAllBytes(path)))
            {
                cellH = src.height / 3;
                cellW = cellH * 3 / 4;
                cols = cellW > 0 ? src.width / cellW : 0;
            }
            if (cols < 2)
            {
                Debug.LogWarning($"{path} is not a walk sheet (three rows of 3:4 frames); drawing the figure instead.");
                ArtLibrary.SafeDestroy(src);
                return null;
            }

            // Texture rows run bottom-up: sheet row r (0 = the top) starts at y = height - (r + 1) * cellH.
            var inPx = src.GetPixels32();
            int ow = cols * cellW, oh = 4 * cellH;
            var outPx = new Color32[ow * oh];
            for (int r = 0; r < 4; r++)
            {
                int from = r < 3 ? r : 2;           // left is the right row mirrored, cell by cell
                for (int y = 0; y < cellH; y++)
                {
                    int sy = src.height - (from + 1) * cellH + y, dy = oh - (r + 1) * cellH + y;
                    for (int c = 0; c < cols; c++)
                        for (int x = 0; x < cellW; x++)
                        {
                            int sx = c * cellW + (r == 3 ? cellW - 1 - x : x);
                            outPx[dy * ow + c * cellW + x] = inPx[sy * src.width + sx];
                        }
                }
            }
            ArtLibrary.SafeDestroy(src);
            var tex = new Texture2D(ow, oh, TextureFormat.RGBA32, true)
            {
                filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, name = $"walk-{id}",
            };
            tex.SetPixels32(outPx);
            tex.Apply(true);

            var frames = new Dictionary<string, Sprite[]>();
            for (int r = 0; r < 4; r++)
            {
                var row = new Sprite[cols];
                for (int c = 0; c < cols; c++)
                    row[c] = Sprite.Create(tex, new Rect(c * cellW, oh - (r + 1) * cellH, cellW, cellH), new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect);
                frames[SheetRows[r]] = row;
            }
            return new HeroSprites(frames);
        }

        /// <summary>Which way a direction faces, in screen terms (y pointing down).</summary>
        public static string Facing(Vector2 dir)
        {
            if (Mathf.Abs(dir.x) > Mathf.Abs(dir.y)) return dir.x > 0 ? "right" : "left";
            return dir.y < 0 ? "up" : "down";
        }

        /// <summary>The number of frames per direction: standing plus the walk cycle.</summary>
        public int FrameCount => _frames["down"].Length;

        /// <summary>The frame for a direction (y down): standing, or stepping through the walk cycle at 8 frames a second.</summary>
        public Sprite Frame(Vector2 dir, bool walking, float time)
        {
            var frames = _frames[Facing(dir)];
            return walking ? frames[1 + (int)(time * 8) % (frames.Length - 1)] : frames[0];
        }

        /// <summary>A frame by facing ("down", "up", "left", "right") and index (0 = standing), for galleries and tests.</summary>
        public Sprite Frame(string facing, int index) => _frames[facing][index];
    }
}
