/*
 * DungeonArt.cs
 * ----------------------------------------------------------------------------
 * Sprites for the Dungeon: the dungeon pack in StreamingAssets/assets/dungeon
 * (16 px tiles and horizontal animation strips, point-filtered), round boss
 * tokens cut from the character sheets, and the champion portraits. Missing
 * files give empty strips, and the view falls back to plain shapes.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using System.IO;
using SpiceTrip.Art;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Dungeon
{
    /// <summary>An animation: equal-width frames side by side in one image.</summary>
    public class Strip
    {
        public Sprite[] frames = new Sprite[0];
        public bool Ok => frames.Length > 0;
        public float Length(float fps) => frames.Length / fps;

        public Sprite Frame(float time, float fps, bool loop)
        {
            if (!Ok) return null;
            int f = Mathf.Max(0, (int)(time * fps));
            return frames[loop ? f % frames.Length : Mathf.Min(f, frames.Length - 1)];
        }

        public Sprite At(int i) => Ok ? frames[Mathf.Clamp(i, 0, frames.Length - 1)] : null;
    }

    public class EnemyAnims { public Strip idle, move, attack, hurt, death; }

    public class DungeonArt
    {
        public static string Folder => Path.Combine(ArtLibrary.Folder, "dungeon");

        public Texture2D Tileset;
        public Strip Peaks, Chest, ChestOpen, Torch, SideTorch, Candle, Candle2, FlaskRed, FlaskBlue,
            Flag, CoinAnim, Skull, Oracle, Flame, Vapes, SpellIcons;
        public readonly Dictionary<EnemyKind, EnemyAnims> Enemies = new Dictionary<EnemyKind, EnemyAnims>();
        public readonly Sprite[] BossTokens = new Sprite[Defs.Bosses.Length];
        public readonly Sprite[] Arenas = new Sprite[Defs.Bosses.Length];
        public Sprite Marin;
        readonly Dictionary<int, Sprite> _tiles = new Dictionary<int, Sprite>();
        readonly Dictionary<string, Sprite> _portraits = new Dictionary<string, Sprite>();
        readonly GameDatabase _db;

        public DungeonArt(GameDatabase db)
        {
            _db = db;
            Tileset = LoadPoint("tileset.png");
            Peaks = Load("peaks.png", 16); Chest = Load("chest.png", 16); ChestOpen = Load("chest_open.png", 16);
            Torch = Load("torch.png", 16); SideTorch = Load("side_torch.png", 16);
            Candle = Load("candlestick.png", 16); Candle2 = Load("candlestick2.png", 16);
            FlaskRed = Load("flask_red.png", 16); FlaskBlue = Load("flask_blue.png", 16);
            Flag = Load("flag.png", 16); CoinAnim = Load("coin.png", 16); Skull = Load("skull.png", 16);
            Oracle = Load("oracle.png", 16); Flame = Load("flame.png", 32);
            Vapes = Load("vapes.png", 32); SpellIcons = Load("spell_icons.png", 32);
            Enemies[EnemyKind.Skeleton] = LoadEnemy("skel1");
            Enemies[EnemyKind.Hound] = LoadEnemy("skel2");
            Enemies[EnemyKind.Vampire] = LoadEnemy("vamp");
            Enemies[EnemyKind.Thief] = LoadEnemy("thief");
            Enemies[EnemyKind.Melon] = LoadEnemy("melon");

            for (int i = 0; i < Defs.Bosses.Length; i++)
            {
                var def = Defs.Bosses[i].id != null ? db.Boss(Defs.Bosses[i].id) : null;
                if (def == null) continue;
                BossTokens[i] = MakeToken(def.img, def.portraitRect);
                var arena = ArtLibrary.Load("arena-" + def.id);
                if (arena != null) Arenas[i] = Sprite.Create(arena, new Rect(0, 0, arena.width, arena.height), new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect);
            }
            var marin = ArtLibrary.Load("marin");
            if (marin != null) Marin = Sprite.Create(marin, new Rect(0, 0, marin.width, marin.height), new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect);
        }

        static Texture2D LoadPoint(string file)
        {
            var p = Path.Combine(Folder, file);
            if (!File.Exists(p)) return null;
            var t = new Texture2D(2, 2, TextureFormat.RGBA32, false) { filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp, name = file };
            return t.LoadImage(File.ReadAllBytes(p), true) ? t : null;
        }

        static Strip Load(string file, int frameW)
        {
            var s = new Strip();
            var t = LoadPoint(file);
            if (t == null) return s;
            int n = Mathf.Max(1, t.width / frameW);
            s.frames = new Sprite[n];
            for (int i = 0; i < n; i++)
                s.frames[i] = Sprite.Create(t, new Rect(i * frameW, 0, frameW, t.height), new Vector2(0.5f, 0.5f), 16, 0, SpriteMeshType.FullRect);
            return s;
        }

        static EnemyAnims LoadEnemy(string prefix) => new EnemyAnims
        {
            idle = Load(prefix + "_idle.png", 32), move = Load(prefix + "_move.png", 32), attack = Load(prefix + "_attack.png", 32),
            hurt = Load(prefix + "_hurt.png", 32), death = Load(prefix + "_death.png", 32),
        };

        /// <summary>A 16 px cell of the tileset, by column and row from the top-left.</summary>
        public Sprite Tile(int col, int row)
        {
            if (Tileset == null) return null;
            int key = row * 100 + col;
            if (_tiles.TryGetValue(key, out var s)) return s;
            s = Sprite.Create(Tileset, new Rect(col * 16, Tileset.height - (row + 1) * 16, 16, 16), new Vector2(0.5f, 0.5f), 16, 0, SpriteMeshType.FullRect);
            _tiles[key] = s;
            return s;
        }

        /// <summary>A champion's portrait, cropped from their sheet (null for the drawn hero).</summary>
        public Sprite Portrait(string championId)
        {
            if (_portraits.TryGetValue(championId, out var s)) return s;
            var ch = _db.Character(championId);
            s = ch != null && !string.IsNullOrEmpty(ch.img) ? ArtLibrary.Crop(ch.img, ch.portraitRect) : null;
            _portraits[championId] = s;
            return s;
        }

        /// <summary>Cuts a boss out of its character sheet as a round token with a soft edge.</summary>
        static Sprite MakeToken(string img, Rect crop)
        {
            if (string.IsNullOrEmpty(img)) return null;
            string name = Path.GetFileNameWithoutExtension(img), path = null;
            foreach (var ext in new[] { ".png", ".jpg" })
            {
                var p = Path.Combine(ArtLibrary.Folder, name + ext);
                if (File.Exists(p)) { path = p; break; }
            }
            if (path == null) return null;
            var sheet = new Texture2D(2, 2, TextureFormat.RGBA32, false);
            if (!sheet.LoadImage(File.ReadAllBytes(path), false)) return null;
            const int Size = 192;
            var px = new Color32[Size * Size];
            float c = (Size - 1) / 2f, radius = Size / 2f - 1;
            for (int y = 0; y < Size; y++)
                for (int x = 0; x < Size; x++)
                {
                    // crop rects use a top-left origin; texture rows run bottom-up
                    float u = crop.x + (x + 0.5f) / Size * crop.width;
                    float v = 1 - (crop.y + (Size - y - 0.5f) / Size * crop.height);
                    Color col = sheet.GetPixelBilinear(u, v);
                    col.a *= Mathf.Clamp01((radius - Mathf.Sqrt((x - c) * (x - c) + (y - c) * (y - c))) / 1.5f);
                    px[y * Size + x] = col;
                }
            ArtLibrary.SafeDestroy(sheet);
            var tex = new Texture2D(Size, Size, TextureFormat.RGBA32, false) { filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, name = name + "-token" };
            tex.SetPixels32(px);
            tex.Apply(false, true);
            return Sprite.Create(tex, new Rect(0, 0, Size, Size), new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect);
        }
    }
}
