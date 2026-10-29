# Cuts a character's walk frames out of a sprite sheet with a flat background
# and writes them as a transparent atlas the game reads from
# StreamingAssets/assets/walk-<id>.png: rows down, up, right; columns the
# standing frame then the walk cycle (cells 3:4, any size). Every frame is
# scaled to the height of the tallest standing frame (frames on a sheet drift
# in size) and stands bottom-centre in its cell. The game mirrors the right
# row for walking left and draws the shadow itself.
#
#   powershell -File Tools~/cut-walk-sheet.ps1 Tools~/walk-layouts/momiche.json
#
# Paths in the layout are relative to the project folder. Windows only (it
# uses System.Drawing).
param([Parameter(Mandatory = $true)][string]$Layout)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$spec = Get-Content -Raw -Path $Layout | ConvertFrom-Json
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class WalkSheetCutter
{
    // The sheet's backdrop and grid lines: mid-bright, clearly bluer than red.
    static bool Backdrop(int r, int g, int b) { return b - r > 28 && (r + g + b) / 3 > 58; }
    // JPEG fringe left around the outline after the backdrop is gone.
    static bool Fringe(int r, int g, int b) { return b - r > 14 && (r + g + b) / 3 > 48; }

    class Figure { public int Width, Height, MinX, CellW; public int[] Pixels; }

    public static string Cut(string sheetPath, string outPath, int cellW, int cellH, int[] rects, int framesPerRow)
    {
        var sheet = new Bitmap(sheetPath);
        int sw = sheet.Width, sh = sheet.Height;
        var src = new int[sw * sh];
        var sd = sheet.LockBits(new Rectangle(0, 0, sw, sh), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        Marshal.Copy(sd.Scan0, src, 0, src.Length);
        sheet.UnlockBits(sd);
        sheet.Dispose();

        int frames = rects.Length / 4, rows = frames / framesPerRow;
        var figs = new Figure[frames];
        for (int f = 0; f < frames; f++) figs[f] = Isolate(src, sw, rects[f * 4], rects[f * 4 + 1], rects[f * 4 + 2], rects[f * 4 + 3]);
        int height = 0;
        for (int r = 0; r < rows; r++) height = Math.Max(height, figs[r * framesPerRow].Height);

        int ow = cellW * framesPerRow, oh = cellH * rows;
        var dst = new int[ow * oh];
        var log = new System.Text.StringBuilder();
        for (int f = 0; f < frames; f++)
        {
            var fig = figs[f];
            double k = (double)height / fig.Height;
            int tw = (int)Math.Round(fig.Width * k), th = height;
            int col = f % framesPerRow, row = f / framesPerRow;
            // The figure keeps its offset from its sheet cell's centre, so the walk doesn't slide sideways.
            int left = col * cellW + cellW / 2 + (int)Math.Round((fig.MinX - fig.CellW / 2.0) * k);
            int top = row * cellH + cellH - 10 - th;                  // feet 10 px above the cell's bottom
            for (int y = 0; y < th; y++)
                for (int x = 0; x < tw; x++)
                {
                    int c = fig.Pixels[Math.Min(fig.Height - 1, (int)(y / k)) * fig.Width + Math.Min(fig.Width - 1, (int)(x / k))];
                    int tx = left + x, ty = top + y;
                    if (c == 0 || tx < col * cellW || tx >= (col + 1) * cellW || ty < row * cellH) continue;
                    dst[ty * ow + tx] = c;
                }
            log.AppendLine("frame " + f + ": " + fig.Width + "x" + fig.Height + ", scaled x" + k.ToString("0.00"));
        }

        var outBmp = new Bitmap(ow, oh, PixelFormat.Format32bppArgb);
        var od = outBmp.LockBits(new Rectangle(0, 0, ow, oh), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        Marshal.Copy(dst, 0, od.Scan0, dst.Length);
        outBmp.UnlockBits(od);
        System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(outPath));
        outBmp.Save(outPath, ImageFormat.Png);
        outBmp.Dispose();
        return log.ToString();
    }

    // One frame: the backdrop removed, cropped to the figure (0 = transparent).
    static Figure Isolate(int[] src, int sw, int rx, int ry, int rw, int rh)
    {
        var px = new int[rw * rh];
        for (int y = 0; y < rh; y++) for (int x = 0; x < rw; x++) px[y * rw + x] = src[(ry + y) * sw + rx + x];
        var clear = new bool[rw * rh];

        // Flood the backdrop in from the cell's edges.
        var q = new Queue<int>();
        for (int x = 0; x < rw; x++) { q.Enqueue(x); q.Enqueue((rh - 1) * rw + x); }
        for (int y = 0; y < rh; y++) { q.Enqueue(y * rw); q.Enqueue(y * rw + rw - 1); }
        while (q.Count > 0)
        {
            int i = q.Dequeue();
            if (clear[i]) continue;
            int c = px[i];
            if (!Backdrop((c >> 16) & 255, (c >> 8) & 255, c & 255)) continue;
            clear[i] = true;
            int x = i % rw, y = i / rw;
            if (x > 0) q.Enqueue(i - 1);
            if (x < rw - 1) q.Enqueue(i + 1);
            if (y > 0) q.Enqueue(i - rw);
            if (y < rh - 1) q.Enqueue(i + rw);
        }
        // Peel the bluish fringe off the outline, two pixels deep.
        for (int pass = 0; pass < 2; pass++)
        {
            var peel = new List<int>();
            for (int y = 0; y < rh; y++)
                for (int x = 0; x < rw; x++)
                {
                    int i = y * rw + x;
                    if (clear[i]) continue;
                    bool edge = (x > 0 && clear[i - 1]) || (x < rw - 1 && clear[i + 1]) || (y > 0 && clear[i - rw]) || (y < rh - 1 && clear[i + rw]);
                    int c = px[i];
                    if (edge && Fringe((c >> 16) & 255, (c >> 8) & 255, c & 255)) peel.Add(i);
                }
            foreach (int i in peel) clear[i] = true;
        }
        // Drop specks: keep only the figure's largest piece.
        var keep = Largest(clear, rw, rh);

        int minX = rw, maxX = -1, minY = rh, maxY = -1;
        for (int y = 0; y < rh; y++)
            for (int x = 0; x < rw; x++)
                if (keep[y * rw + x]) { minX = Math.Min(minX, x); maxX = Math.Max(maxX, x); minY = Math.Min(minY, y); maxY = Math.Max(maxY, y); }
        if (maxX < 0) throw new Exception("the frame at " + rx + "," + ry + " is empty");

        var fig = new Figure { Width = maxX - minX + 1, Height = maxY - minY + 1, MinX = minX, CellW = rw };
        fig.Pixels = new int[fig.Width * fig.Height];
        for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++)
                if (keep[y * rw + x]) fig.Pixels[(y - minY) * fig.Width + x - minX] = px[y * rw + x] | unchecked((int)0xFF000000);
        return fig;
    }

    static bool[] Largest(bool[] clear, int w, int h)
    {
        var label = new int[w * h];
        int best = 0, bestSize = 0, next = 0;
        var q = new Queue<int>();
        for (int s = 0; s < w * h; s++)
        {
            if (clear[s] || label[s] != 0) continue;
            int id = ++next, size = 0;
            label[s] = id; q.Enqueue(s);
            while (q.Count > 0)
            {
                int i = q.Dequeue(); size++;
                int x = i % w, y = i / w;
                foreach (int n in new[] { x > 0 ? i - 1 : -1, x < w - 1 ? i + 1 : -1, y > 0 ? i - w : -1, y < h - 1 ? i + w : -1 })
                    if (n >= 0 && !clear[n] && label[n] == 0) { label[n] = id; q.Enqueue(n); }
            }
            if (size > bestSize) { bestSize = size; best = id; }
        }
        var keep = new bool[w * h];
        for (int i = 0; i < keep.Length; i++) keep[i] = label[i] == best;
        return keep;
    }
}
'@

$rects = New-Object System.Collections.Generic.List[int]
foreach ($dir in 'down', 'up', 'right') { foreach ($r in $spec.$dir) { $rects.AddRange([int[]]$r) } }
$per = @($spec.down).Count
$sheet = Join-Path $root $spec.sheet
$out = Join-Path $root $spec.out
[WalkSheetCutter]::Cut($sheet, $out, [int]$spec.cell[0], [int]$spec.cell[1], $rects.ToArray(), $per)
"Wrote $out"
