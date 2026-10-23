/*
 * TileMap.cs
 * ----------------------------------------------------------------------------
 * Port of TileMap.js: the 2D world — a tile grid per region with collision,
 * themed terrain, gothic props, collectible pickups and static light sources.
 * Maps are generated procedurally and deterministically per region (seeded by
 * the region id), with bespoke touches for the starting Mistwood.
 * Coordinates are world pixels; TS-sized tiles.
 *
 * The darkness/fog/glow pass lives in RpgView, which reads `Lights` from here.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    public class TileMap
    {
        public const int TS = PixelArt.TS;

        static readonly Dictionary<string, (int w, int h)> Sizes = new Dictionary<string, (int, int)>
        {
            ["mistwood"] = (48, 42), ["temple"] = (46, 38), ["ruins"] = (50, 42),
            ["caves"] = (54, 46), ["citadel"] = (46, 40), ["godreach"] = (46, 44),
        };

        public class Cell
        {
            public PixelCanvas tile;
            public bool solid, wall, water;
        }

        public class Prop
        {
            public int tx, ty;
            public string name;
            public PixelCanvas sprite;
            public float x, baseline, drawX, drawY;
            public bool animated;
        }

        public class Pickup
        {
            public float x, y, bob;
            public string type, item;
        }

        public class Light
        {
            public float x, y, r, flick, phase;
            public string color;
        }

        public RegionDef Region { get; private set; }
        public int W { get; private set; }
        public int H { get; private set; }
        public Cell[] Cells { get; private set; } = new Cell[0];
        public readonly List<Prop> Props = new List<Prop>();
        public readonly List<Pickup> Pickups = new List<Pickup>();
        public readonly List<Light> Lights = new List<Light>();
        public float Time;
        public int Version { get; private set; }   // bumps on every load so the view can rebuild

        readonly GameDatabase _db;
        readonly System.Random _fx;   // Math.random() in the JS (light flicker phases)

        public TileMap(GameDatabase db, System.Random fx)
        {
            _db = db;
            _fx = fx;
        }

        public int PixelW => W * TS;
        public int PixelH => H * TS;

        public Cell CellAt(int tx, int ty) => Cells[ty * W + tx];

        // --- Collision ------------------------------------------------------------
        public bool SolidAt(float px, float py)
        {
            int tx = Mathf.FloorToInt(px / TS), ty = Mathf.FloorToInt(py / TS);
            if (tx < 0 || ty < 0 || tx >= W || ty >= H) return true;
            return Cells[ty * W + tx].solid;
        }

        // =========================================================================
        // Generation
        // =========================================================================
        public void Load(string regionId)
        {
            var region = _db.Region(regionId);
            Region = region;
            var (w, h) = Sizes.TryGetValue(regionId, out var s) ? s : (48, 40);
            W = w; H = h;
            Cells = new Cell[w * h];
            Props.Clear();
            Pickups.Clear();
            Lights.Clear();
            var rng = new Mulberry32(Hash.Str(regionId));

            // 1) Lay base ground everywhere.
            for (int ty = 0; ty < h; ty++)
                for (int tx = 0; tx < w; tx++)
                {
                    int variant = PixelCanvas.Trunc(rng.Next() * 3);
                    Cells[ty * w + tx] = new Cell { tile = PixelArt.GetTile(region.tileset, "floor", variant) };
                }

            // 2) Border wall ring.
            for (int tx = 0; tx < w; tx++) { SetWall(tx, 0, region); SetWall(tx, h - 1, region); }
            for (int ty = 0; ty < h; ty++) { SetWall(0, ty, region); SetWall(w - 1, ty, region); }

            // 3) Region-specific layout.
            if (regionId == "caves") GenCaves(region, rng);
            else GenOverworld(region, rng, regionId);

            // 4) Guarantee the spawn + NPC tiles (and a little around them) are clear.
            ClearArea(region.spawn.tx, region.spawn.ty, 3);
            if (regionId == "mistwood")
                foreach (var n in _db.npcs)
                    if (n.region == regionId) ClearArea(n.tx, n.ty, 2);

            // 5) Pickups (asphodel everywhere; relic shards only in the Mistwood).
            ScatterPickups(rng, regionId);
            Time = 0;
            Version++;
        }

        void SetWall(int tx, int ty, RegionDef region)
        {
            var c = CellAt(tx, ty);
            c.tile = PixelArt.GetTile(region.tileset, "wall");
            c.solid = true; c.wall = true;
        }

        void ClearArea(int tx, int ty, int r)
        {
            for (int y = ty - r; y <= ty + r; y++)
                for (int x = tx - r; x <= tx + r; x++)
                {
                    if (x <= 0 || y <= 0 || x >= W - 1 || y >= H - 1) continue;
                    var c = CellAt(x, y);
                    c.solid = false; c.wall = false;
                }
            // Remove any props that landed in the cleared area.
            Props.RemoveAll(p => p.tx >= tx - r && p.tx <= tx + r && p.ty >= ty - r && p.ty <= ty + r);
        }

        struct Density { public double trees, bushes, pillars, graves, statues, rubble; }

        static Density DensityFor(string regionId)
        {
            switch (regionId)
            {
                case "forest": return new Density { trees = 0.10, bushes = 0.05, pillars = 0.0, graves = 0.02, statues = 0.004, rubble = 0.01 };
                case "temple": return new Density { trees = 0.01, bushes = 0.01, pillars = 0.05, graves = 0.0, statues = 0.01, rubble = 0.03 };
                case "ruins": return new Density { trees = 0.02, bushes = 0.02, pillars = 0.06, graves = 0.02, statues = 0.012, rubble = 0.05 };
                case "citadel": return new Density { trees = 0.005, bushes = 0.0, pillars = 0.04, graves = 0.0, statues = 0.014, rubble = 0.02 };
                case "godreach": return new Density { trees = 0.01, bushes = 0.0, pillars = 0.05, graves = 0.0, statues = 0.02, rubble = 0.01 };
                // Note: the JS keys this table by region id, so the Mistwood (whose
                // tileset is "forest") gets the default densities below.
                default: return new Density { trees = 0.06, bushes = 0.03, pillars = 0.01, graves = 0.01, statues = 0.005, rubble = 0.02 };
            }
        }

        // Open, scatter-based overworld for forest/temple/ruins/citadel/godreach.
        void GenOverworld(RegionDef region, Mulberry32 rng, string regionId)
        {
            int w = W, h = H;

            // Wandering dirt/marble paths for visual structure.
            int px = w / 2, py = h - 3;
            for (int i = 0; i < w * 1.4; i++)
            {
                var c = CellAt(Mathf.Clamp(px, 1, w - 2), Mathf.Clamp(py, 1, h - 2));
                if (!c.wall) c.tile = PixelArt.GetTile(region.tileset, "path");
                if (rng.Next() < 0.5) px += rng.Next() < 0.5 ? 1 : -1; else py += rng.Next() < 0.6 ? -1 : 1;
                px = Mathf.Clamp(px, 2, w - 3); py = Mathf.Clamp(py, 2, h - 3);
            }

            var cfg = DensityFor(regionId);
            for (int ty = 2; ty < h - 2; ty++)
                for (int tx = 2; tx < w - 2; tx++)
                {
                    var c = CellAt(tx, ty);
                    if (c.wall) continue;
                    double r = rng.Next();
                    if (r < cfg.trees) AddProp(tx, ty, regionId == "forest" || regionId == "mistwood" ? (rng.Next() < 0.5 ? "pine" : "deadTree") : "deadTree", true);
                    else if (r < cfg.trees + cfg.pillars) AddProp(tx, ty, "pillar", true);
                    else if (r < cfg.trees + cfg.pillars + cfg.statues) AddProp(tx, ty, "statue", true);
                    else if (r < cfg.trees + cfg.pillars + cfg.statues + cfg.graves) AddProp(tx, ty, "grave", false);
                    else if (r < cfg.trees + cfg.pillars + cfg.statues + cfg.graves + cfg.bushes) AddProp(tx, ty, "bush", false);
                    else if (r < cfg.trees + cfg.pillars + cfg.statues + cfg.graves + cfg.bushes + cfg.rubble) AddProp(tx, ty, "rubble", false);
                }

            // Braziers / altars as light anchors, sprinkled around.
            int lightCount = regionId == "caves" ? 14 : 10;
            for (int i = 0; i < lightCount; i++)
            {
                int tx = 2 + PixelCanvas.Trunc(rng.Next() * (w - 4)), ty = 2 + PixelCanvas.Trunc(rng.Next() * (h - 4));
                var c = CellAt(tx, ty);
                if (c.wall || c.solid) continue;
                if (rng.Next() < 0.25) AddProp(tx, ty, "altar", true, "#b48aff", 3);
                else AddProp(tx, ty, "brazier", true, "#ff9a3a", 4, true);
            }

            // Citadel: glowing windows along the top wall.
            if (regionId == "citadel")
                for (int tx = 4; tx < w - 4; tx += 3)
                    Lights.Add(new Light { x = tx * TS + 8, y = 1 * TS + 8, r = 34, color = "#cdd6ff", flick = 0.4f, phase = (float)(rng.Next() * 7) });
        }

        // Cellular-automata caves for the Underway.
        void GenCaves(RegionDef region, Mulberry32 rng)
        {
            int w = W, h = H;
            var grid = new int[w * h];
            for (int ty = 0; ty < h; ty++)
                for (int tx = 0; tx < w; tx++)
                    grid[ty * w + tx] = (tx < 2 || ty < 2 || tx > w - 3 || ty > h - 3) ? 1 : (rng.Next() < 0.46 ? 1 : 0);
            for (int pass = 0; pass < 4; pass++)
            {
                var next = (int[])grid.Clone();
                for (int ty = 1; ty < h - 1; ty++)
                    for (int tx = 1; tx < w - 1; tx++)
                    {
                        int walls = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++)
                                if (dx != 0 || dy != 0) walls += grid[(ty + dy) * w + (tx + dx)];
                        next[ty * w + tx] = walls > 4 ? 1 : (walls < 3 ? 0 : grid[ty * w + tx]);
                    }
                grid = next;
            }
            for (int ty = 0; ty < h; ty++)
                for (int tx = 0; tx < w; tx++)
                    if (grid[ty * w + tx] == 1) SetWall(tx, ty, region);
            // A few scattered rubble + braziers for orientation.
            for (int i = 0; i < 16; i++)
            {
                int tx = 2 + PixelCanvas.Trunc(rng.Next() * (w - 4)), ty = 2 + PixelCanvas.Trunc(rng.Next() * (h - 4));
                var c = CellAt(tx, ty);
                if (c.solid) continue;
                if (rng.Next() < 0.5) AddProp(tx, ty, "brazier", true, "#b48aff", 4, true);
                else AddProp(tx, ty, "rubble", false);
            }
        }

        void AddProp(int tx, int ty, string name, bool solid, string lightColor = null, int lightR = 0, bool flicker = false)
        {
            var sprite = PixelArt.Prop(name);
            float baseline = ty * TS + TS;                 // feet line for y-sort
            Props.Add(new Prop
            {
                tx = tx, ty = ty, name = name, sprite = sprite,
                x = tx * TS + TS / 2f, baseline = baseline,
                drawX = tx * TS + (TS - sprite.Width) / 2f, drawY = baseline - sprite.Height,
                animated = name == "brazier",
            });
            if (solid) CellAt(tx, ty).solid = true;
            if (lightColor != null && lightR != 0)
                Lights.Add(new Light { x = tx * TS + 8, y = ty * TS + 8, r = lightR * TS / 4f + 18, color = lightColor, flick = flicker ? 0.6f : 0.15f, phase = (float)(_fx.NextDouble() * 7) });
        }

        void ScatterPickups(Mulberry32 rng, string regionId)
        {
            void Place(string type, string item, int count)
            {
                int placed = 0, tries = 0;
                while (placed < count && tries < count * 40)
                {
                    tries++;
                    int tx = 2 + PixelCanvas.Trunc(rng.Next() * (W - 4)), ty = 2 + PixelCanvas.Trunc(rng.Next() * (H - 4));
                    if (CellAt(tx, ty).solid) continue;
                    Pickups.Add(new Pickup { x = tx * TS + TS / 2f, y = ty * TS + TS / 2f, type = type, item = item, bob = (float)(rng.Next() * 7) });
                    placed++;
                }
            }
            Place("asphodel", "asphodel", 10);
            if (regionId == "mistwood") Place("relicShard", "relicShard", 4);
        }

        public void RemovePickup(Pickup pk) => Pickups.Remove(pk);

        public void Update(float dt) => Time += dt;

        // =========================================================================
        // Rendering (props and pickups; ground is baked by the view)
        // =========================================================================
        public void DrawProp(IWorldPainter g, Prop p)
        {
            g.Sprite(p.sprite, Mathf.Round(p.drawX), Mathf.Round(p.drawY));
            if (p.animated)
            {
                int fr = PixelCanvas.Trunc(Time * 8);
                g.Sprite(PixelArt.Flame(fr), p.tx * TS, p.ty * TS - 6);
            }
        }

        public void DrawPickups(IWorldPainter g)
        {
            foreach (var pk in Pickups)
            {
                float yb = Mathf.Sin(Time * 3 + pk.bob) * 2;
                float sx = Mathf.Round(pk.x), sy = Mathf.Round(pk.y + yb);
                if (pk.type == "asphodel")
                {
                    // glowing pale flower
                    g.Rect(sx - 1, sy - 3, 2, 2, Css.C("#dfe6ff"));
                    var petal = Css.C("#aab6e0");
                    g.Rect(sx - 3, sy - 1, 2, 2, petal); g.Rect(sx + 1, sy - 1, 2, 2, petal);
                    g.Rect(sx - 1, sy + 1, 2, 2, petal);
                    g.Rect(sx - 1, sy - 1, 2, 2, Css.C("#ffe8a0"));
                    g.Rect(sx, sy + 2, 1, 3, Css.C("#3a6d3a"));
                }
                else
                {
                    // relic shard
                    g.Rect(sx - 1, sy - 4, 2, 8, Css.C("#9fe0ff"));
                    g.Rect(sx - 1, sy - 4, 1, 8, Css.C("#eaffff"));
                    g.Rect(sx - 1, sy + 3, 2, 2, Css.C("#caa84d"));
                }
            }
        }

        /// <summary>Static light radius this frame, with flicker (drawLighting in the JS).</summary>
        public float LightRadius(Light l) => l.r * (1 + Mathf.Sin(Time * 6 + l.phase) * l.flick * 0.18f);
    }
}
