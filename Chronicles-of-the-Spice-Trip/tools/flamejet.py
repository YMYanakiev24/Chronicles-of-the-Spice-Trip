"""Draws the wall fire-thrower's flame: a roaring jet of fire in chunky pixel-art bands,
white-hot at the nozzle, curling into red tongues and black smoke at its end, embers
streaming along it. And the iron nozzle it comes out of.

    python tools/flamejet.py

Writes (drawn at 2x, like the dungeon tiles; the jet points right and is turned to aim):

  assets/dungeon/flame_jet.png   8 frames across, 5 rows down, each frame 76 x 24.
        Rows 0-3: the jet reaching 1-4 tiles. Its mouth is the left middle of the frame,
        5 pixels out from the middle of the fire-thrower's tile.
        Row 4: the sputter before it fires (flickers and smoke at the mouth).
        The frames loop: the fire streams out of the nozzle and never jumps.
  assets/dungeon/flamer.png      3 frames, 16 x 16: the nozzle cold, warming, firing.
"""
import os
import random
import numpy as np
from PIL import Image

FW, FH, FRAMES, ROWS = 76, 24, 8, 5
TILE = 16
PERIOD = 64                       # the noise repeats every PERIOD pixels along the jet...
SHIFT = PERIOD // FRAMES          # ...and moves on SHIFT a frame, so the loop is seamless

# the fire's bands, hottest first: (threshold, colour, alpha)
BANDS = [(0.84, (255, 250, 228), 255), (0.66, (255, 220, 100), 255), (0.48, (255, 152, 44), 255),
         (0.31, (230, 80, 26), 255), (0.16, (150, 34, 24), 235)]
SMOKE = (38, 26, 34)
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.5


def lattice(rnd, cols, rows):
    return np.array([[rnd.random() for _ in range(cols)] for _ in range(rows)])


def noise(grid, xs, ys, cell):
    """Smooth value noise, wrapping round in x every grid-width cells."""
    rows, cols = grid.shape
    gx, gy = xs / cell, ys / cell
    x0, y0 = np.floor(gx).astype(int), np.floor(gy).astype(int)
    fx, fy = gx - x0, gy - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    x0 %= cols
    x1 = (x0 + 1) % cols
    y0 = np.clip(y0, 0, rows - 1)
    y1 = np.clip(y0 + 1, 0, rows - 1)
    a = grid[y0, x0] * (1 - fx) + grid[y0, x1] * fx
    b = grid[y1, x0] * (1 - fx) + grid[y1, x1] * fx
    return a * (1 - fy) + b * fy


rnd = random.Random(29)
G1 = lattice(rnd, PERIOD // 8, FH // 8 + 2)       # big billows
G2 = lattice(rnd, PERIOD // 4, FH // 4 + 2)       # licks along the edge
G3 = lattice(rnd, PERIOD // 8, FH // 8 + 2)       # smoke
EMBERS = [(rnd.random() * PERIOD * 2, rnd.uniform(-5, 5), rnd.uniform(0, 6.28)) for _ in range(9)]


def heat(frame, length, power=1.0, flicker=None):
    """The fire's temperature over a frame, for a jet `length` pixels from the mouth."""
    ys, xs = np.mgrid[0:FH, 0:FW].astype(float)
    cy = FH / 2 - 0.5
    s = xs
    t = frame
    n1 = noise(G1, (s - t * SHIFT) % PERIOD, ys + 4, 8)
    n2 = noise(G2, (s - t * SHIFT * 2) % PERIOD, ys + 4, 4)
    wob = (noise(G1, (s - t * SHIFT + 23) % PERIOD, np.full_like(ys, 6.0), 8) - 0.5) * 3.2 * np.clip(s / 18, 0, 1)
    d = np.abs(ys - cy - wob)
    radius = 1.8 + 9.6 * (1 - np.exp(-s / 15.0))
    tail = np.clip((s - length) / 10.0, 0, 1)
    radius *= 1 - tail ** 1.6
    edge = 0.62 + 0.7 * n1 + 0.3 * (n2 - 0.5)
    T = 1 - d / np.maximum(radius * edge, 0.01)
    u = np.clip(s / max(length, 1), 0, 1)
    T *= (1.0 - 0.5 * u * (0.6 + 0.8 * n1)) * (1 - tail) * power
    T += 0.45 * np.exp(-s / 5.0) * np.clip(1 - d / 2.5, 0, 1) * power          # white-hot at the nozzle
    T += (n2 - 0.5) * 0.22 * np.clip(s / 8, 0, 1)
    if flicker is not None:
        T *= flicker
    smoke = (np.clip((s - length * 0.55) / 14.0, 0, 1) * (1 - np.clip((s - length - 8) / 6, 0, 1))
             * np.clip(1 - d / (radius * 1.25 + 3), 0, 1) * noise(G3, (s - t * SHIFT) % PERIOD, ys + 4, 8))
    return T, smoke, s, ys, cy


def paint(T, smoke, s, ys, cy, frame, length, embers=True):
    img = np.zeros((FH, FW, 4), dtype=np.uint8)
    dith = BAYER[(ys.astype(int) % 4), (s.astype(int) % 4)] * 0.06
    Td = T + dith
    sm = smoke + dith * 0.8
    m = sm > 0.42
    img[m] = SMOKE + (150,)
    img[sm > 0.6] = (24, 16, 22, 190)
    for th, col, a in reversed(BANDS):
        img[Td > th] = col + (a,)
    if embers:                               # sparks streaming along with the fire
        for x0, off, ph in EMBERS:
            x = (x0 + frame * SHIFT * 1.5) % (PERIOD * 2)
            if x < 4 or x > length + 6:
                continue
            y = int(round(cy + off * min(1, x / 20) + np.sin(ph + x * 0.25) * 1.2))
            xi = int(x)
            if 0 <= y < FH and 0 <= xi < FW:
                img[y, xi] = (255, 236, 150, 255) if x < length else (255, 140, 60, 255)
    return img


def jets():
    sheet = np.zeros((FH * ROWS, FW * FRAMES, 4), dtype=np.uint8)
    for n in range(1, 5):
        length = 3 + TILE * n
        for f in range(FRAMES):
            T, smoke, s, ys, cy = heat(f, length)
            sheet[(n - 1) * FH:n * FH, f * FW:(f + 1) * FW] = paint(T, smoke, s, ys, cy, f, length)
    # the sputter: short coughs of flame out of the nozzle, and smoke
    coughs = [0.55, 0.9, 0.35, 0.8, 0.5, 1.0, 0.3, 0.7]
    for f in range(FRAMES):
        length = 4 + 5 * coughs[f]
        T, smoke, s, ys, cy = heat(f, length, power=0.75 + 0.3 * coughs[f])
        smoke = np.clip(smoke * 1.6, 0, 1)
        sheet[4 * FH:5 * FH, f * FW:(f + 1) * FW] = paint(T, smoke, s, ys, cy, f, length, embers=False)
    return Image.fromarray(sheet, "RGBA")


def nozzle():
    """An iron fire-spout bolted to the brick: a square plate, a fat barrel out to the right
    with a flared muzzle, soot round its lip. Frames: cold, warming (the barrel glows), firing."""
    DEEP, IRON_D, IRON, IRON_L, RIVET = (14, 10, 18), (38, 34, 46), (66, 62, 78), (108, 102, 122), (156, 150, 168)
    SOOT, HOT_D, HOT = (24, 18, 22), (120, 30, 22), (200, 70, 30)
    mouths = [((30, 20, 24), (20, 14, 18)), ((170, 46, 26), (240, 110, 40)), ((255, 176, 64), (255, 248, 214))]
    sheet = Image.new("RGBA", (16 * 3, 16), (0, 0, 0, 0))
    for f, (rim, core) in enumerate(mouths):
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = img.load()
        for y in range(1, 15):                       # the plate, bevelled
            for x in range(1, 12):
                edge = x == 1 or y == 1 or x == 11 or y == 14
                c = DEEP if edge else IRON_L if (x == 2 or y == 2) else IRON_D if (x == 10 or y == 13) else IRON
                px[x, y] = c + (255,)
        for x, y in ((3, 3), (9, 3), (3, 12), (9, 12)):   # rivets
            px[x, y] = RIVET + (255,)
            px[x + 1, y + 1] = IRON_D + (255,)
        for y in range(4, 12):                       # the barrel, out to the right
            for x in range(5, 14):
                if y in (4, 11):
                    px[x, y] = DEEP + (255,)
                else:
                    c = IRON_L if y == 5 else IRON_D if y in (9, 10) else IRON
                    if f >= 1 and 7 <= x <= 11 and y in (6, 7, 8):     # heat glowing through the iron
                        c = HOT_D if f == 1 else HOT
                    px[x, y] = c + (255,)
        for y in range(3, 13):                       # the flared muzzle
            px[14, y] = DEEP + (255,)
            if 4 <= y <= 11:
                px[13, y] = (IRON_L if y < 7 else IRON_D) + (255,)
        px[15, 4] = DEEP + (255,)
        px[15, 11] = DEEP + (255,)
        for y in range(5, 11):                       # its mouth, dark or burning
            px[15, y] = (core if 7 <= y <= 8 else rim) + (255,)
        for x, y in ((12, 4), (11, 11), (13, 3), (13, 12)):   # soot
            px[x, y] = SOOT + (255,)
        sheet.paste(img, (f * 16, 0))
    return sheet


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "assets", "dungeon")
    jets().save(os.path.join(out, "flame_jet.png"))
    nozzle().save(os.path.join(out, "flamer.png"))
    print("wrote", os.path.normpath(os.path.join(out, "flame_jet.png")), "and flamer.png")


if __name__ == "__main__":
    main()
