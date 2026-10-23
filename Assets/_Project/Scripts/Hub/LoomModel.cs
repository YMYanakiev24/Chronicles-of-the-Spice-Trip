/*
 * LoomModel.cs
 * ----------------------------------------------------------------------------
 * The rules of Marin's Enchanted Loom (Scenes.js): an image is pixelated into
 * a square tapestry, perforated into a cols×cols grid of rune-shards, and each
 * shard can be torn free to tumble away under gravity. No rendering here, so
 * the grid maths and tear physics can be unit-tested; LoomView draws it.
 *
 * The web version stepped the physics by a fixed 1/60 s per animation frame;
 * here it is stepped by real elapsed time so it plays the same at any frame rate.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.Hub
{
    public class LoomModel
    {
        public class Cell
        {
            public int c, r;
            public float gx, gy;            // grid position (top-left), canvas px
            public bool torn;
            public float x, y, vx, vy, rot, vr, alpha = 1;
        }

        public readonly LoomTuning T;
        public readonly List<Cell> Cells = new List<Cell>();
        public int Cols { get; private set; }
        public float CellSize { get; private set; }
        public int TornCount { get; private set; }
        readonly Func<float> _rand;

        public LoomModel(LoomTuning tuning, Func<float> rand = null)
        {
            T = tuning;
            var sys = new System.Random();
            _rand = rand ?? (() => (float)sys.NextDouble());
        }

        /// <summary>Side of the small square the source image is cover-fitted into before upscaling.</summary>
        public int PixelSizeFor(int cols) => Math.Max(T.minPixelSize, cols * T.pixelsPerCell);

        public void Build(int cols)
        {
            Cols = cols;
            CellSize = (float)T.canvasSize / cols;
            TornCount = 0;
            Cells.Clear();
            for (int r = 0; r < cols; r++)
                for (int c = 0; c < cols; c++)
                    Cells.Add(new Cell { c = c, r = r, gx = c * CellSize, gy = r * CellSize });
        }

        /// <summary>The intact shard under a canvas point, or null.</summary>
        public Cell HitTest(float x, float y)
        {
            if (Cells.Count == 0 || x < 0 || y < 0) return null;
            int c = Mathf.FloorToInt(x / CellSize), r = Mathf.FloorToInt(y / CellSize);
            return Cells.Find(k => k.c == c && k.r == r && !k.torn);
        }

        public void Tear(Cell cell)
        {
            cell.torn = true;
            cell.x = cell.gx; cell.y = cell.gy;
            cell.vx = (_rand() - 0.5f) * T.tearVx;
            cell.vy = -T.tearVyMin - _rand() * T.tearVyRange;
            cell.vr = (_rand() - 0.5f) * T.tearSpin;
            TornCount++;
        }

        /// <summary>Tears every intact shard; returns how many were torn.</summary>
        public int TearAll()
        {
            int n = 0;
            foreach (var cell in Cells) if (!cell.torn) { Tear(cell); n++; }
            return n;
        }

        public void Step(float dt)
        {
            foreach (var k in Cells)
            {
                if (!k.torn || k.alpha <= 0) continue;
                k.vy += T.gravity * dt;
                k.x += k.vx * dt; k.y += k.vy * dt;
                k.rot += k.vr * dt;
                k.alpha -= dt * T.fadePerSecond;
            }
        }

        /// <summary>Cover-fit source rect (in source pixels) for drawing an image into a square, as coverDraw().</summary>
        public static Rect CoverRect(int imgW, int imgH, float targetAspect = 1f)
        {
            float ir = (float)imgW / imgH;
            if (ir > targetAspect)
            {
                float sw = imgH * targetAspect;
                return new Rect((imgW - sw) / 2, 0, sw, imgH);
            }
            float sh = imgW / targetAspect;
            return new Rect(0, (imgH - sh) / 2, imgW, sh);
        }
    }
}
