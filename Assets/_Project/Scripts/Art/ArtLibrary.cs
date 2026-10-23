/*
 * ArtLibrary.cs
 * ----------------------------------------------------------------------------
 * Loads the painted scene art and character sheets. Like the web version's
 * "custom art drop-in", images are plain files that can be replaced without
 * rebuilding: they live in StreamingAssets/assets (next to the game's data
 * folder in a build). Each lookup tries .png then .jpg, and returns null when
 * neither exists so every caller can fall back to procedural art.
 *
 * Textures are decoded uncompressed with bilinear filtering and no mipmaps:
 * the images are painted "pixel-style" art with JPEG artefacts shown at
 * non-integer scales, so point filtering would shimmer.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace SpiceTrip.Art
{
    public static class ArtLibrary
    {
        static readonly Dictionary<string, Texture2D> Textures = new Dictionary<string, Texture2D>();
        static readonly Dictionary<string, Sprite> Sprites = new Dictionary<string, Sprite>();

        /// <summary>Folder searched for art; overridable for tests.</summary>
        public static string Folder = Path.Combine(Application.streamingAssetsPath, "assets");

        /// <summary>Loads "name" trying name.png then name.jpg; null if neither exists.</summary>
        public static Texture2D Load(string baseName)
        {
            if (string.IsNullOrEmpty(baseName)) return null;
            string key = Path.GetFileNameWithoutExtension(baseName);
            if (Textures.TryGetValue(key, out var cached)) return cached;

            Texture2D tex = null;
            string ext = Path.GetExtension(baseName);
            var candidates = string.IsNullOrEmpty(ext)
                ? new[] { key + ".png", key + ".jpg" }
                : new[] { baseName };
            foreach (var file in candidates)
            {
                var p = Path.Combine(Folder, Path.GetFileName(file));
                if (!File.Exists(p)) continue;
                tex = new Texture2D(2, 2, TextureFormat.RGBA32, false)
                {
                    filterMode = FilterMode.Bilinear,
                    wrapMode = TextureWrapMode.Clamp,
                    name = Path.GetFileName(file),
                };
                if (tex.LoadImage(File.ReadAllBytes(p), true)) break;
                SafeDestroy(tex);
                tex = null;
            }
            Textures[key] = tex;
            return tex;
        }

        /// <summary>
        /// A sprite cut from an image by a normalised rect with a top-left origin
        /// (the way crops are authored), or the whole image when rect is empty.
        /// </summary>
        public static Sprite Crop(string baseName, Rect rect, float pixelsPerUnit = 100)
        {
            var tex = Load(baseName);
            if (tex == null) return null;
            string key = $"{Path.GetFileNameWithoutExtension(baseName)}:{rect}:{pixelsPerUnit}";
            if (Sprites.TryGetValue(key, out var s) && s != null) return s;
            if (rect.width <= 0 || rect.height <= 0) rect = new Rect(0, 0, 1, 1);
            var px = new Rect(
                Mathf.Round(rect.x * tex.width),
                Mathf.Round((1 - rect.y - rect.height) * tex.height),
                Mathf.Round(rect.width * tex.width),
                Mathf.Round(rect.height * tex.height));
            px.width = Mathf.Min(px.width, tex.width - px.x);
            px.height = Mathf.Min(px.height, tex.height - px.y);
            s = Sprite.Create(tex, px, new Vector2(0.5f, 0.5f), pixelsPerUnit, 0, SpriteMeshType.FullRect);
            s.name = key;
            Sprites[key] = s;
            return s;
        }

        /// <summary>Drops cached textures so the next Load re-reads the files (tests and art swaps).</summary>
        public static void ClearCache()
        {
            foreach (var s in Sprites.Values) SafeDestroy(s);
            foreach (var t in Textures.Values) SafeDestroy(t);
            Sprites.Clear();
            Textures.Clear();
        }

        public static void SafeDestroy(Object o)
        {
            if (o == null) return;
            if (Application.isPlaying) Object.Destroy(o);
            else Object.DestroyImmediate(o);
        }
    }
}
