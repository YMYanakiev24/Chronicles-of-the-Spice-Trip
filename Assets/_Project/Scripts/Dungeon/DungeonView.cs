/*
 * DungeonView.cs
 * ----------------------------------------------------------------------------
 * Draws the Dungeon with its own camera: the room (tiles, or a boss's painted
 * arena framed by the walls), torches and props, monsters, bosses, the hero,
 * projectiles, bus No.9 and particles. Everything is a pooled SpriteRenderer
 * placed in room pixels each frame (the same approach as the boss arena).
 * Text over the room (damage numbers, prompts) is drawn by DungeonHud.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Data;
using UnityEngine;
using static SpiceTrip.Dungeon.Geo;

namespace SpiceTrip.Dungeon
{
    public class DungeonView : MonoBehaviour
    {
        const float Ppu = 100f;

        DungeonGame _g;
        public DungeonArt Art { get; private set; }
        Camera _cam;
        Transform _world;
        Material _alpha, _additive;
        MaterialPropertyBlock _mpb;
        Sprite _white, _circle, _glow, _ring, _ellipse, _feather, _ticket, _koi, _bus;
        readonly List<SpriteRenderer> _pool = new List<SpriteRenderer>();
        int _drawn;
        float _gloom;

        public Camera Camera => _cam;

        public void Init(DungeonGame game, GameDatabase db, Material alpha, Material additive)
        {
            _g = game;
            _alpha = alpha; _additive = additive;
            _mpb = new MaterialPropertyBlock();
            Art = new DungeonArt(db);
            BuildSprites();
            var camGo = new GameObject("DungeonCamera");
            camGo.transform.SetParent(transform, false);
            camGo.transform.position = new Vector3(0, 0, -10);
            _cam = camGo.AddComponent<Camera>();
            _cam.orthographic = true;
            _cam.clearFlags = CameraClearFlags.SolidColor;
            _cam.backgroundColor = new Color32(8, 8, 12, 255);
            _cam.nearClipPlane = 0.1f; _cam.farClipPlane = 100;
            _world = new GameObject("DungeonWorld").transform;
            _world.SetParent(transform, false);
            Show(false);
        }

        public void Show(bool show)
        {
            _cam.enabled = show;
            _world.gameObject.SetActive(show);
        }

        void BuildSprites()
        {
            Sprite Spr(PixelCanvas c, FilterMode f = FilterMode.Bilinear)
            {
                var t = c.ToTexture(null, f);
                return Sprite.Create(t, new Rect(0, 0, t.width, t.height), new Vector2(0.5f, 0.5f), t.width, 0, SpriteMeshType.FullRect);
            }
            var w = new PixelCanvas(4, 4);
            w.FillRect(0, 0, 4, 4, new Color32(255, 255, 255, 255));
            _white = Spr(w, FilterMode.Point);
            var circle = new PixelCanvas(64, 64);
            circle.FillCircle(32, 32, 31.5, new Color32(255, 255, 255, 255));
            _circle = Spr(circle);
            var glow = new PixelCanvas(64, 64);
            glow.FillRadial(0, 0, 64, 64, 32, 32, 32, new Color32(255, 255, 255, 255), new Color32(255, 255, 255, 0));
            _glow = Spr(glow);
            var ellipse = new PixelCanvas(64, 32);
            ellipse.FillEllipse(32, 16, 31.5, 15.5, new Color32(255, 255, 255, 255));
            _ellipse = Spr(ellipse);
            var ring = new PixelCanvas(128, 128);
            for (int y = 0; y < 128; y++)
                for (int x = 0; x < 128; x++)
                {
                    float d = Mathf.Sqrt((x + 0.5f - 64) * (x + 0.5f - 64) + (y + 0.5f - 64) * (y + 0.5f - 64));
                    if (d <= 63.5f && d >= 58) ring.Pixels[y * 128 + x] = new Color32(255, 255, 255, 255);
                }
            _ring = Spr(ring);
            _feather = Spr(Grid(new[] { "..bbbbbbbbbb..", "wwwwwwwwwwwwwk", "..bbbbbbbbbb..", "...cccccccc..." },
                ('b', "#5a8ad8"), ('w', "#f4f6ff"), ('c', "#9fc4ff"), ('k', "#2a3a6a")), FilterMode.Point);
            _ticket = Spr(Grid(new[] { "kkkkkkkkkkkk", "kppppppppppk", "kprrrrrrrrpk", "kppppppppppk", "kpddpdpddppk", "kkkkkkkkkkkk" },
                ('k', "#9a7a40"), ('p', "#efe6cc"), ('d', "#3a2a1a"), ('r', "#c83c3c")), FilterMode.Point);
            _koi = Spr(Grid(new[] { "o.....oooo....", "oo..oowwoooo..", "ooooowoooooke.", "oo..oooowoo...", "o.....oooo...." },
                ('o', "#ff8a3a"), ('w', "#ffffff"), ('k', "#1a1414"), ('e', "#ffb070")), FilterMode.Point);
            _bus = Spr(Grid(new[]
            {
                "llllllllllllllllllllllllllllllll",
                "lyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyl",
                "lyywwwywwwywwwywwwywwwywwwyyhhyl",
                "lyywwwywwwywwwywwwywwwywwwyyhhyh",
                "lyyyyyyyyyyyyyyyyyyyyyyyyyyyhhyh",
                "lyyyyyyyyyyyyyyyyyyyyyyyynnnyyyl",
                "lyyyyyyyyyyyyyyyyyyyyyyyynyyyyyl",
                "lyyyyyyyyyyyyyyyyyyyyyyyynnnyyyl",
                "lssssssssssssssssssssssssyynssyl",
                "llkkkllllllllllllllllllllnnnkkll",
                "..kkk....................kkk....",
            }, ('l', "#7a5a1a"), ('y', "#e8b230"), ('w', "#3c5070"), ('h', "#fffac8"), ('n', "#2a1e14"),
               ('s', "#fad66e"), ('k', "#18161a")), FilterMode.Point);
        }

        static PixelCanvas Grid(string[] rows, params (char key, string color)[] pal)
        {
            var c = new PixelCanvas(rows[0].Length, rows.Length);
            for (int y = 0; y < rows.Length; y++)
                for (int x = 0; x < rows[y].Length; x++)
                    foreach (var (key, color) in pal)
                        if (rows[y][x] == key) c.Px(x, y, color);
            return c;
        }

        // ---------------------------------------------------------------- coordinates
        static Vector3 W(float x, float y) => new Vector3((x - RoomW / 2) / Ppu, (RoomH / 2 - y) / Ppu, 0);

        /// <summary>Screen pixels (y up) to room pixels.</summary>
        public Vector2 ScreenToRoom(Vector2 screen)
        {
            var w = _cam.ScreenToWorldPoint(new Vector3(screen.x, screen.y, 10));
            return new Vector2(w.x * Ppu + RoomW / 2, RoomH / 2 - w.y * Ppu);
        }

        /// <summary>Room pixels to screen pixels (y up).</summary>
        public Vector2 RoomToScreen(Vector2 room)
        {
            var s = _cam.WorldToScreenPoint(W(room.x, room.y));
            return new Vector2(s.x, s.y);
        }

        /// <summary>The whole room on screen, centred, with a margin for the HUD.</summary>
        void FitCamera()
        {
            float aspect = Mathf.Max(0.1f, (float)Screen.width / Mathf.Max(1, Screen.height));
            _cam.orthographicSize = Mathf.Max(RoomH * 1.18f / 2, RoomW * 1.12f / 2 / aspect) / Ppu;
            float amp = 14 * _g.Shake / 1.7f;
            _cam.transform.localPosition = new Vector3(Random.Range(-amp, amp) / Ppu, Random.Range(-amp, amp) / Ppu, -10);
        }

        SpriteRenderer D(Sprite sprite, int order, float x, float y, float w, float h, Color? color = null, float rotDeg = 0, bool additive = false, bool flip = false, float brightness = 1)
        {
            if (sprite == null) return null;
            if (_drawn == _pool.Count)
            {
                var go = new GameObject("D");
                go.transform.SetParent(_world, false);
                _pool.Add(go.AddComponent<SpriteRenderer>());
            }
            var r = _pool[_drawn++];
            r.enabled = true;
            r.sprite = sprite;
            r.sharedMaterial = additive ? _additive : _alpha;
            r.sortingOrder = order;
            r.color = color ?? Color.white;
            r.flipX = flip;
            r.transform.localPosition = W(x, y);
            r.transform.localRotation = Quaternion.Euler(0, 0, -rotDeg);     // room y points down
            var b = sprite.bounds.size;
            r.transform.localScale = new Vector3(w / Ppu / b.x, h / Ppu / b.y, 1);
            _mpb.SetFloat("_Brightness", brightness);
            r.SetPropertyBlock(_mpb);
            return r;
        }

        void Rect(int order, float x, float y, float w, float h, Color c) => D(_white, order, x + w / 2, y + h / 2, w, h, c);

        /// <summary>A strip frame centred on (x, y), scaled from its 16 px or 32 px source.</summary>
        void Anim(Sprite s, int order, float x, float y, float scale, bool flip = false, Color? tint = null, float rot = 0)
        {
            if (s == null) return;
            var r = s.rect;
            D(s, order, x, y, r.width * scale, r.height * scale, tint, rot, false, flip);
        }

        // ================================================================== the frame
        public void Render()
        {
            if (_g == null) return;
            FitCamera();
            _drawn = 0;
            DrawRoom();
            DrawActors();
            var room = _g.Room;
            bool painted = room.isBoss && Art.Arenas[_g.CurrentBossIndex] != null;
            float target = room.isBoss && !room.cleared ? (painted ? 0.16f : 0.38f) : 0;
            _gloom += (target - _gloom) * Mathf.Min(1, Time.deltaTime * 2.5f);
            float big = RoomW * 3;
            if (_gloom > 0.001f) D(_white, 200, Center.x, Center.y, big, big, new Color(4 / 255f, 2 / 255f, 10 / 255f, _gloom));
            D(_white, 201, Center.x, Center.y, big, big, new Color(122 / 255f, 154 / 255f, 138 / 255f, 0.04f));
            if (_g.Player.auraT > 0) D(_white, 202, Center.x, Center.y, big, big, new Color(120 / 255f, 180 / 255f, 1, 0.06f));
            for (int i = _drawn; i < _pool.Count; i++) _pool[i].enabled = false;
        }

        void DrawRoom()
        {
            var r = _g.Room;
            var arena = r.isBoss ? Art.Arenas[_g.CurrentBossIndex] : null;
            bool bossDark = r.isBoss && !r.cleared;
            float trapPh = _g.TrapClock % 2.4f;
            int spikeFrame = trapPh < 1.2f ? 0 : trapPh < 1.35f ? 1 : trapPh < 2.25f ? 2 : 3;
            int midX = RoomTW / 2, midY = RoomTH / 2;
            float t = _g.Time;

            for (int ty = 0; ty < RoomTH; ty++)
                for (int tx = 0; tx < RoomTW; tx++)
                {
                    float px = tx * Tile + Tile / 2f, py = ty * Tile + Tile / 2f;
                    bool border = tx == 0 || ty == 0 || tx == RoomTW - 1 || ty == RoomTH - 1;
                    if (arena != null && !border) continue;        // the painting covers the floor
                    Dir dir = Dir.None;
                    if (ty == 0 && tx == midX) dir = Dir.N;
                    else if (ty == RoomTH - 1 && tx == midX) dir = Dir.S;
                    else if (tx == 0 && ty == midY) dir = Dir.W;
                    else if (tx == RoomTW - 1 && ty == midY) dir = Dir.E;
                    bool isDoor = dir != Dir.None && (r.doors & dir) != 0;
                    bool innerWall = !border && r.At(tx, ty) == RoomTile.Wall;
                    uint h = (uint)(tx * 73856093) ^ (uint)(ty * 19349663) ^ (uint)(_g.Floor.cx * 83492791 + _g.Floor.cy * 2654435761u);

                    if (Art.Tileset == null)    // plain shapes when the tileset is missing
                    {
                        bool open = (!border || isDoor) && !innerWall;
                        Rect(0, px - 16, py - 16, Tile, Tile, open ? (((tx + ty) & 1) == 1 ? (Color)new Color32(46, 44, 60, 255) : new Color32(40, 38, 52, 255)) : new Color32(24, 22, 32, 255));
                        if (isDoor && !r.cleared) Rect(1, px - 13, py - 13, Tile - 6, Tile - 6, new Color32(120, 86, 54, 255));
                        if (!border && r.At(tx, ty) == RoomTile.Spikes && _g.SpikesUp) Rect(1, px - 10, py - 10, Tile - 12, Tile - 12, new Color32(200, 200, 210, 255));
                        continue;
                    }
                    Sprite tile;
                    if ((!border || isDoor) && !innerWall)
                    {
                        D(Art.Tile(6 + (int)(h % 4), (int)((h >> 4) % 3)), 0, px, py, Tile, Tile);       // stone floor variants
                        if (isDoor && !r.cleared) D(Art.Tile(8, 3), 1, px, py, Tile, Tile);               // barred door
                        else if (!border && r.At(tx, ty) == RoomTile.Spikes) Anim(Art.Peaks.At(spikeFrame), 1, px, py, 2);
                        if (ty == 1 && tx == 1 && !r.isBoss) D(Art.Tile(4, 6), 1, px, py, Tile, Tile);    // cobwebs
                        if (ty == 1 && tx == RoomTW - 2 && !r.isBoss) D(Art.Tile(5, 6), 1, px, py, Tile, Tile);
                        continue;
                    }
                    if (innerWall) tile = Art.Tile(1 + (int)((h >> 4) % 4), 0);                     // brick block
                    else if (ty == 0) tile = Art.Tile(tx == 0 ? 0 : tx == RoomTW - 1 ? 5 : 1 + tx % 4, 0);
                    else if (ty == RoomTH - 1) tile = Art.Tile(tx == 0 ? 0 : tx == RoomTW - 1 ? 5 : 1 + tx % 4, 4);
                    else tile = Art.Tile(tx == 0 ? 0 : 5, 1 + ty % 3);
                    D(tile, 0, px, py, Tile, Tile);
                }

            if (arena != null)          // the boss's painted arena, framed by the walls
            {
                float iw = RoomW - 2 * Tile, ih = RoomH - 2 * Tile;
                D(ArenaCover(_g.CurrentBossIndex, iw, ih), 0, Center.x, Center.y, iw, ih, r.cleared ? Color.white : (Color)new Color32(214, 208, 226, 255));
                float k = 3;
                Rect(1, Tile, Tile, iw, k, new Color(0, 0, 0, 0.6f)); Rect(1, Tile, RoomH - Tile - k, iw, k, new Color(0, 0, 0, 0.6f));
                Rect(1, Tile, Tile, k, ih, new Color(0, 0, 0, 0.6f)); Rect(1, RoomW - Tile - k, Tile, k, ih, new Color(0, 0, 0, 0.6f));
            }

            // wall torches (unlit in the boss lair), banners, candles
            foreach (float x in new[] { 4f * Tile + 16, (RoomTW - 5f) * Tile + 16 })
            {
                if (bossDark) D(Art.Tile(2, 9), 2, x, 14, Tile, Tile);
                else Anim(Art.Torch.Frame(t + x * 0.01f, 8, true), 2, x, 14, 2);
            }
            foreach (int ty in new[] { 2, RoomTH - 3 })
                foreach (float x in new[] { 18f, RoomW - 18 })
                {
                    float y = ty * Tile + 16;
                    if (bossDark) D(Art.Tile(2, 9), 2, x, y, Tile, Tile);
                    else Anim(Art.SideTorch.Frame(t + x * 0.01f, 8, true), 2, x, y, 2, x > RoomW / 2);
                }
            if (r.isBoss && arena == null)
                for (int i = 0; i < 4; i++)
                    Anim(Art.Flag.Frame(t + i * 0.3f, 6, true), 2, (2 + i * 5 + (i > 1 ? 2 : 0)) * Tile + 16, 14, 2);
            if (r.isStart && _g.Mode == DungeonMode.Run)
            {
                Anim(Art.Candle.Frame(t, 7, true), 2, OraclePos.x - 34, OraclePos.y - 4, 2);
                Anim(Art.Candle.Frame(t + 0.3f, 7, true), 2, OraclePos.x + 34, OraclePos.y - 4, 2);
                Anim(Art.Candle2.Frame(t + 0.5f, 7, true), 2, RoomW - 3 * Tile, 2.5f * Tile, 2);
                Anim(Art.Candle2.Frame(t + 0.1f, 7, true), 2, RoomW - 3 * Tile, RoomH - 2.5f * Tile, 2);
            }
        }

        // Each arena painting, cropped once to cover the room's floor.
        readonly Sprite[] _covers = new Sprite[Defs.Bosses.Length];
        Sprite ArenaCover(int i, float w, float h)
        {
            if (_covers[i] != null) return _covers[i];
            var tex = Art.Arenas[i].texture;
            float sc = Mathf.Max(w / tex.width, h / tex.height), sw = w / sc, sh = h / sc;
            return _covers[i] = Sprite.Create(tex, new UnityEngine.Rect((tex.width - sw) / 2, (tex.height - sh) / 2, sw, sh),
                new Vector2(0.5f, 0.5f), 100, 0, SpriteMeshType.FullRect);
        }

        void Shadow(int order, Vector2 p, float r) => D(_ellipse, order, p.x, p.y + r * 0.7f, r * 1.8f, r * 0.9f, new Color(0, 0, 0, 0.35f));

        void DrawActors()
        {
            var g = _g;
            var r = g.Room;
            float t = g.Time;

            if (g.StairsOpen)        // the staircase down, once the boss falls
            {
                float sx = Center.x, sy = Center.y;
                Rect(3, sx - 30, sy - 24, 60, 48, new Color32(8, 8, 12, 255));
                for (int i = 0; i < 4; i++)
                    Rect(4, sx - 26 + i * 3, sy - 18 + i * 11, 52 - i * 6, 9, new Color32((byte)(78 - i * 14), (byte)(74 - i * 14), (byte)(96 - i * 16), 255));
                var gold = new Color32(150, 140, 90, 255);
                Rect(5, sx - 30, sy - 24, 60, 2, gold); Rect(5, sx - 30, sy + 22, 60, 2, gold);
                Rect(5, sx - 30, sy - 24, 2, 48, gold); Rect(5, sx + 28, sy - 24, 2, 48, gold);
            }

            if (r.hasChest)
            {
                var c = r.ChestPos;
                Shadow(5, new Vector2(c.x, c.y + 8), 12);
                if (Art.Chest.Ok)
                    Anim(r.chestOpened ? Art.ChestOpen.Frame(g.ChestOpenT, 10, false)
                        : (t % 2.5f < 0.5f ? Art.Chest.Frame(t, 8, true) : Art.Chest.At(0)), 6, c.x, c.y, 2);    // rattles now and then
                else Rect(6, c.x - 11, c.y - 6, 22, 14, new Color32(120, 78, 40, 255));
            }

            if (r.isShop)            // rug, candles and Marin, gently bobbing
            {
                var m = MerchantPos;
                Rect(5, m.x - 70, m.y - 6, 140, 60, new Color32(200, 160, 80, 255));
                Rect(5, m.x - 67, m.y - 3, 134, 54, new Color32(70, 34, 70, 255));
                Anim(Art.Candle.Frame(t, 7, true), 6, m.x - 54, m.y + 6, 2);
                Anim(Art.Candle.Frame(t + 0.4f, 7, true), 6, m.x + 54, m.y + 6, 2);
                Shadow(6, new Vector2(m.x, m.y + 14), 16);
                if (Art.Marin != null)
                {
                    float hgt = 70, wid = hgt * Art.Marin.rect.width / Art.Marin.rect.height, bob = Mathf.Sin(t * 2.2f) * 2;
                    D(Art.Marin, 7, m.x, m.y - hgt / 2 + 26 + bob, wid, hgt);
                }
                else D(_circle, 7, m.x, m.y, 28, 28, new Color32(90, 60, 160, 255));
                for (int i = 0; i < 3; i++) Anim(Art.CoinAnim.Frame(t + i * 0.25f, 10, true), 7, m.x - 30 + i * 30, m.y + 40, 1.2f);
            }

            if (r.isStart && g.Mode == DungeonMode.Run)
            {
                Shadow(5, OraclePos, 12);
                if (Art.Oracle.Ok) Anim(Art.Oracle.Frame(t, 5, true), 7, OraclePos.x, OraclePos.y - 6, 2.2f, g.Player.pos.x < OraclePos.x);
                else Rect(7, OraclePos.x - 7, OraclePos.y - 12, 14, 22, new Color32(202, 168, 232, 255));
            }

            foreach (var c in g.Corpses) DrawEnemy(c, 10);
            foreach (var e in g.Enemies) DrawEnemy(e, 12);
            foreach (var c in g.Coins) Anim(Art.CoinAnim.Frame(c.t, 12, true), 14, c.pos.x, c.pos.y, 1.4f);

            DrawHero(20);
            foreach (var s in g.Shots) DrawShot(s, 30);
            foreach (var s in g.EnemyShots) DrawShot(s, 31);
            DrawBus(g.Fight.Bus, 40);

            foreach (var pt in g.Particles)
            {
                var c = pt.color; c.a = pt.life / pt.maxLife;
                D(_circle, 50, pt.pos.x, pt.pos.y, pt.size * 2, pt.size * 2, c);
            }
            foreach (var pu in g.Popups)
            {
                var strip = pu.mana ? Art.FlaskBlue : Art.FlaskRed;
                Anim(strip.Frame(pu.t, 8, true), 51, pu.pos.x, pu.pos.y, 2, false, new Color(1, 1, 1, Mathf.Min(1, 1.4f - pu.t)));
            }
        }

        static float EnemyScale(Enemy e)
        {
            if (e.boss) return 3.2f;
            if (e.kind == EnemyKind.Wisp) return 2;
            if (e.kind == EnemyKind.Thief || e.kind == EnemyKind.Melon) return 1.05f;
            return e.kind == EnemyKind.Hound ? 1.9f : 1.7f;
        }

        void DrawEnemy(Enemy e, int order)
        {
            var token = e.bossForm >= 0 ? Art.BossTokens[_g.CurrentBossIndex] : null;
            if (token != null) { DrawBossBody(e, token, order); return; }

            bool hasSprite = e.kind == EnemyKind.Wisp ? Art.Skull.Ok : Art.Enemies.TryGetValue(e.kind, out var set) && set.idle.Ok;
            if (!e.dying) Shadow(order - 1, e.pos, e.r);
            if (hasSprite)
            {
                float sc = EnemyScale(e);
                var at = new Vector2(e.pos.x, e.pos.y - e.r * 0.6f);
                if (e.kind == EnemyKind.Wisp)
                {
                    at.y += Mathf.Sin(e.wobble) * 3 - 6;      // a floating wisp bobs, and flashes when struck
                    var tint = e.dying ? new Color(1, 1, 1, 1 - e.deathT / 0.5f) : e.hitFlash > 0 ? new Color(1, 0.63f, 0.63f) : Color.white;
                    Anim(Art.Skull.Frame(e.animT, 8, true), order, at.x, at.y, sc, e.flip, tint);
                }
                else
                {
                    var a = Art.Enemies[e.kind];
                    Sprite s;
                    if (e.dying) s = a.death.Frame(e.deathT, 14, false);
                    else if (e.hitFlash > 0) s = a.hurt.Frame(0.12f - e.hitFlash, 40, false);
                    else if (e.atkAnim > 0) s = a.attack.Frame(0.4f - e.atkAnim, a.attack.frames.Length / 0.4f, false);
                    else if (e.moving) s = a.move.Frame(e.animT, 12, true);
                    else s = a.idle.Frame(e.animT, 8, true);
                    Anim(s, order, at.x, at.y, sc, e.flip);
                }
            }
            else D(_circle, order, e.pos.x, e.pos.y, e.r * 2, e.r * 2, e.hitFlash > 0 ? Color.white : e.color);

            if (!e.dying && !e.boss && e.hp < e.maxHp)   // bosses use the big bar instead
            {
                float bw = e.r * 2.4f, top = e.pos.y - e.r * 2 - 6;
                Rect(order + 1, e.pos.x - bw / 2, top, bw, 4, new Color32(20, 10, 10, 200));
                Rect(order + 2, e.pos.x - bw / 2, top, bw * Mathf.Clamp01(e.hp / e.maxHp), 4, new Color32(200, 60, 60, 255));
            }
        }

        void DrawBossBody(Enemy e, Sprite token, int order)
        {
            var def = _g.CurrentBoss;
            float bob = e.dying ? 0 : Mathf.Sin(e.wobble * 0.8f) * 2.5f;
            float alpha = e.dying ? Mathf.Max(0, 1 - e.deathT / 0.45f) : 1;
            float grow = e.dying ? 1 + e.deathT * 0.6f : 1;
            if (!e.dying) Shadow(order - 1, e.pos, e.r);
            float x = e.pos.x, y = e.pos.y - e.r * 0.4f + bob, rad = e.r * 1.3f * grow;
            var col = def.color;
            D(_circle, order, x, y, rad * 2 + 6, rad * 2 + 6, new Color(col.r, col.g, col.b, 0.35f * alpha));     // glow
            D(token, order + 1, x, y, rad * 2, rad * 2, new Color(1, 1, 1, alpha), 0, false, e.flip);
            D(_ring, order + 2, x, y, rad * 2 + 2, rad * 2 + 2, new Color(col.r, col.g, col.b, alpha));
            if (e.hitFlash > 0) D(_circle, order + 3, x, y, rad * 2, rad * 2, new Color(1, 1, 1, 0.35f), 0, true);
            if (e.bossForm > 0 && !e.dying && e.hp < e.maxHp)      // copies get their own little bar
            {
                float bw = e.r * 2.4f, top = y - rad - 7;
                Rect(order + 4, x - bw / 2, top, bw, 3, new Color32(20, 10, 10, 200));
                Rect(order + 5, x - bw / 2, top, bw * Mathf.Clamp01(e.hp / e.maxHp), 3, col);
            }
        }

        void DrawHero(int order)
        {
            var p = _g.Player;
            float lift = p.jumpT > 0 ? Mathf.Sin((1 - p.jumpT / 0.6f) * Mathf.PI) * 22 : 0;
            if (p.auraT > 0)
            {
                float pulse = 34 + Mathf.Sin(_g.Time * 6) * 3;
                D(_circle, order - 2, p.pos.x, p.pos.y, pulse * 2, pulse * 2, new Color(130 / 255f, 200 / 255f, 1, 0.12f));
                D(_ring, order - 1, p.pos.x, p.pos.y, pulse * 2, pulse * 2, new Color(170 / 255f, 220 / 255f, 1, 0.7f));
            }
            if (p.dashT > 0 || p.rollAtkT > 0)
                D(_circle, order - 1, p.pos.x, p.pos.y, PlayerR * 2 + 8, PlayerR * 2 + 8,
                    p.rollAtkT > 0 ? new Color(110 / 255f, 200 / 255f, 80 / 255f, 0.35f) : new Color(245 / 255f, 210 / 255f, 110 / 255f, 0.35f));

            Shadow(order, p.pos, PlayerR * (1 - lift * 0.02f));
            float x = p.pos.x, y = p.pos.y - lift;
            // the pixel hero, facing the way they walk (or aim, standing still), white for a moment when hit
            var look = p.moving ? p.moveDir : p.facing;
            var frame = HeroSprites.For(p.champion).Frame(look, p.moving, _g.Time);
            D(frame, order + 1, x, y - 10, HeroSprites.W * 2, HeroSprites.H * 2, null, 0, false, false, p.hitFlash > 0 ? 3 : 1);
            if (p.meleeAnim > 0)        // the godless blade's slash
            {
                float ang = Mathf.Atan2(p.facing.y, p.facing.x);
                float a = ang - Mathf.Sin((1 - p.meleeAnim) * Mathf.PI) * 0.9f;
                D(_white, order + 4, x + Mathf.Cos(a) * 17, y - 4 + Mathf.Sin(a) * 17, 22, 3, new Color32(234, 255, 255, 255), a * Mathf.Rad2Deg);
            }

            if (p.shieldT > 0)
            {
                D(_circle, order + 5, x, y - 6, 44, 44, new Color(230 / 255f, 190 / 255f, 120 / 255f, 0.18f));
                D(_ring, order + 6, x, y - 6, 44, 44, new Color(250 / 255f, 220 / 255f, 150 / 255f, 0.9f));
            }
        }

        void DrawShot(Shot s, int order)
        {
            float ang = Mathf.Atan2(s.vel.y, s.vel.x) * Mathf.Rad2Deg, t = _g.Time;
            switch (s.kind)
            {
                case ShotKind.Spell:
                    if (Art.Flame.Ok && s.school == "Fire")
                    {
                        D(_glow, order, s.pos.x, s.pos.y, (s.r + 5) * 3, (s.r + 5) * 3, new Color(s.glow.r, s.glow.g, s.glow.b, 0.5f), 0, true);
                        Anim(Art.Flame.Frame(1.4f - s.life, 14, true), order + 1, s.pos.x, s.pos.y, 1.3f, false, null, ang + 180);
                    }
                    else
                    {
                        D(_glow, order, s.pos.x, s.pos.y, (s.r + 3) * 3, (s.r + 3) * 3, new Color(s.glow.r, s.glow.g, s.glow.b, 0.7f), 0, true);
                        D(_circle, order + 1, s.pos.x, s.pos.y, s.r * 2, s.r * 2, s.color);
                    }
                    break;
                case ShotKind.Coin:
                    if (Art.CoinAnim.Ok) Anim(Art.CoinAnim.Frame(t + s.pos.x * 0.01f, 14, true), order, s.pos.x, s.pos.y, 1.2f);
                    else D(_circle, order, s.pos.x, s.pos.y, s.r * 2, s.r * 2, s.color);
                    break;
                case ShotKind.Koi:
                    D(_koi, order, s.pos.x, s.pos.y, 22, 9, null, ang);
                    break;
                case ShotKind.Orb:
                    D(_glow, order, s.pos.x, s.pos.y, s.r * 4, s.r * 4, new Color(s.color.r, s.color.g, s.color.b, 0.6f), 0, true);
                    D(_circle, order + 1, s.pos.x, s.pos.y, s.r * 2, s.r * 2, s.color);
                    D(_circle, order + 2, s.pos.x - s.r * 0.3f, s.pos.y - s.r * 0.3f, s.r * 0.7f, s.r * 0.7f, new Color(1, 1, 1, 0.7f));
                    break;
                case ShotKind.Feather:
                    D(_feather, order, s.pos.x, s.pos.y, 18, 6, null, ang);
                    break;
                case ShotKind.Ticket:
                    D(_ticket, order, s.pos.x, s.pos.y, 14, 8, null, t * 6 * Mathf.Rad2Deg + s.pos.x);
                    break;
                case ShotKind.Ember:
                    D(_glow, order, s.pos.x, s.pos.y, s.r * 4.4f, s.r * 4.4f, new Color(1, 110 / 255f, 40 / 255f, 0.6f), 0, true);
                    D(_circle, order + 1, s.pos.x, s.pos.y, s.r * 2, s.r * 2, new Color32(255, 150, 60, 255));
                    D(_circle, order + 2, s.pos.x, s.pos.y, s.r, s.r, new Color32(255, 236, 180, 255));
                    break;
            }
        }

        void DrawBus(Bus bus, int order)
        {
            if (bus == null) return;
            if (!bus.driving)        // the warning lane flashes
            {
                float a = 0.18f + 0.14f * Mathf.Sin(_g.Time * 22);
                Rect(order, Tile, bus.y - bus.h / 2, RoomW - 2 * Tile, bus.h, new Color(230 / 255f, 60 / 255f, 50 / 255f, a));
                return;
            }
            Rect(order, bus.x - bus.w / 2 + 6, bus.y + bus.h / 2 - 6, bus.w, 10, new Color(0, 0, 0, 0.35f));
            D(_bus, order + 1, bus.x, bus.y, bus.w, bus.h);
        }

        /// <summary>Sprites the HUD shows: a boss token, a spell icon, a vape.</summary>
        public Sprite BossToken(int i) => Art.BossTokens[i];
    }
}
