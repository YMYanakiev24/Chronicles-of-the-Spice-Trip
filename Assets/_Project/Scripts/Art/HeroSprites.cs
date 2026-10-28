/*
 * HeroSprites.cs
 * ----------------------------------------------------------------------------
 * The player's walking figure, shared by the boss arena and the Dungeon: the
 * pixel hero seen from the front, the back and both sides, with a two-frame
 * walk, in the chosen champion's cloak colour. Spellwoven keeps the original
 * hero colours.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using UnityEngine;

namespace SpiceTrip.Art
{
    public sealed class HeroSprites
    {
        public const int W = PixelArt.HeroW, H = PixelArt.HeroH;

        // Cloak colour per champion id (the Changing Room's characters).
        static readonly Dictionary<string, string> Cloaks = new Dictionary<string, string>
        {
            ["atanas"] = "#4a6a9a", ["gakev"] = "#9a7a2a", ["nikolay"] = "#7a4e2e", ["pupesh"] = "#3e7a34", ["ristyo"] = "#7a3434",
            ["iliqnpuh"] = "#8a5e24", ["daniilvasil"] = "#3e3e70", ["storyteller"] = "#2e6068", ["fairy"] = "#8a4a8a",
        };

        /// <summary>The champion's cloak colour (the original hero blue for Spellwoven and unknown ids).</summary>
        public static Color Cloak(string championId) =>
            PixelCanvas.Col(championId != null && Cloaks.TryGetValue(championId, out var c) ? c : "#2a3550");

        static readonly Dictionary<string, HeroSprites> Cache = new Dictionary<string, HeroSprites>();

        public static HeroSprites For(string championId)
        {
            championId ??= "hero";
            if (Cache.TryGetValue(championId, out var s)) return s;
            var set = Cloaks.TryGetValue(championId, out var cloak) ? PixelArt.HeroIn(cloak) : PixelArt.Hero();
            return Cache[championId] = new HeroSprites(set);
        }

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

        /// <summary>Which way a direction faces, in screen terms (y pointing down).</summary>
        public static string Facing(Vector2 dir)
        {
            if (Mathf.Abs(dir.x) > Mathf.Abs(dir.y)) return dir.x > 0 ? "right" : "left";
            return dir.y < 0 ? "up" : "down";
        }

        /// <summary>The frame for a direction (y down): standing, or stepping through the walk cycle.</summary>
        public Sprite Frame(Vector2 dir, bool walking, float time)
        {
            var frames = _frames[Facing(dir)];
            return walking ? frames[1 + (int)(time * 8) % 2] : frames[0];
        }
    }
}
