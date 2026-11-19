"""Draws the floor spike trap: an iron plate let into the stone, five steel spikes that
punch up out of it, rust round the holes and old blood on two of the tips.

    python tools/spikes.py

Writes assets/dungeon/spike_trap.png: seven 16 x 24 frames (drawn at 2x, like the dungeon
tiles). The bottom 16 rows are the floor tile; the spikes rise into the 8 rows above it.

    0  down: just the holes            4  up, a glint running along the steel
    1  warning: the tips peep out      5  going down
    2  shooting up                     6  nearly down
    3  up
"""
import os
import random
from PIL import Image

FW, FH, TOP = 16, 24, 8          # frame size; the tile starts TOP rows down
DEEP = (16, 12, 20)
PLATE, PLATE_L, PLATE_D = (54, 46, 62), (84, 76, 96), (32, 26, 38)
RIVET, RIVET_D = (128, 120, 138), (40, 34, 46)
HOLE, HOLE_LIP = (8, 6, 10), (72, 64, 82)
RUST, RUST_L = (92, 52, 40), (124, 74, 46)
STEEL_D, STEEL, STEEL_L, STEEL_T = (58, 60, 78), (112, 118, 140), (176, 184, 204), (236, 240, 252)
BLOOD, BLOOD_D = (138, 20, 26), (86, 10, 16)

# where the spikes stand: (x, base row in the frame, full height, bloody)
SPIKES = [(5, TOP + 6, 10, False), (10, TOP + 6, 10, True),
          (3, TOP + 12, 11, True), (8, TOP + 12, 11, False), (13, TOP + 12, 11, False)]


def plate(px):
    rnd = random.Random(7)
    for y in range(TOP, FH):
        for x in range(FW):
            px[x, y] = PLATE + (255,)
    for x in range(FW):
        px[x, TOP] = PLATE_L + (255,)
        px[x, FH - 1] = DEEP + (255,)
        px[x, FH - 2] = PLATE_D + (255,)
    for y in range(TOP, FH):
        px[0, y] = PLATE_L + (255,) if y < FH - 2 else px[0, y]
        px[FW - 1, y] = PLATE_D + (255,) if y > TOP else px[FW - 1, y]
    for _ in range(9):                       # rust creeping over the iron
        x, y = rnd.randrange(1, FW - 1), rnd.randrange(TOP + 1, FH - 2)
        px[x, y] = (RUST if rnd.random() < 0.6 else RUST_L) + (255,)
    for x, y in ((1, TOP + 1), (FW - 2, TOP + 1), (1, FH - 3), (FW - 2, FH - 3)):
        px[x, y] = RIVET + (255,)
        if y + 1 < FH - 1:
            px[x, y + 1] = RIVET_D + (255,)


def hole(px, x, base):
    for dx in (-1, 0, 1):
        px[x + dx, base] = HOLE + (255,)
    px[x, base - 1] = HOLE + (255,)
    px[x - 1, base - 1] = PLATE_D + (255,)
    px[x + 1, base - 1] = PLATE_D + (255,)
    if base + 1 < FH - 2:
        px[x, base + 1] = RUST + (255,)       # rust weeping out of the hole


def spike(px, x, base, h, bloody, glint):
    """A steel needle `h` rows tall standing in its hole, lit from the top left."""
    if h <= 0:
        return
    rows = []
    for k in range(h):
        u = k / max(h - 1, 1)
        rows.append((base - k, 1 if u < 0.4 and h > 3 else 0))
    tip_y = rows[-1][0]
    for y, w in rows:                        # outline first, over whatever is behind
        for dx in range(-w - 1, w + 2):
            if 0 <= x + dx < FW and 0 <= y < FH:
                px[x + dx, y] = DEEP + (255,)
    if tip_y - 1 >= 0:
        px[x, tip_y - 1] = DEEP + (255,)
    for i, (y, w) in enumerate(rows):
        if w:
            px[x - 1, y] = STEEL + (255,)
            px[x, y] = STEEL_L + (255,)
            px[x + 1, y] = STEEL_D + (255,)
        else:
            px[x, y] = (STEEL_L if i < len(rows) - 3 else STEEL_T) + (255,)
    if glint:                                # a glint running up the edge
        y = rows[min(len(rows) - 1, len(rows) // 2)][0]
        px[x, y] = (255, 255, 255, 255)
        px[x, y + 1] = STEEL_T + (255,)
    if bloody and h >= 6:                    # old blood down the point
        for k in range(3):
            px[x, tip_y + k] = (BLOOD if k < 2 else BLOOD_D) + (255,)
    sy = base + 1                            # its shadow, down and to the right on the plate
    if sy < FH - 2:
        for dx in (1, 2):
            if x + dx < FW - 1 and px[x + dx, sy][:3] in (PLATE, RUST, RUST_L):
                px[x + dx, sy] = PLATE_D + (255,)


def frame(lift, glint=False):
    """lift: how far up the spikes are, 0 (down) to 1 (all the way)."""
    img = Image.new("RGBA", (FW, FH), (0, 0, 0, 0))
    px = img.load()
    plate(px)
    for x, base, full, _ in SPIKES:
        hole(px, x, base)
    for x, base, full, bloody in sorted(SPIKES, key=lambda s: s[1]):   # back row first
        h = round(full * lift) if lift < 1 else full
        if 0 < lift < 0.2:
            h = 2                            # just the tips, rattling in the holes
        spike(px, x, base, h, bloody and lift >= 1, glint)
    return img


def main():
    lifts = [(0, False), (0.1, False), (0.55, False), (1, False), (1, True), (0.5, False), (0.25, False)]
    sheet = Image.new("RGBA", (FW * len(lifts), FH), (0, 0, 0, 0))
    for i, (lift, glint) in enumerate(lifts):
        sheet.paste(frame(lift, glint), (i * FW, 0))
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "assets", "dungeon", "spike_trap.png")
    sheet.save(out)
    print("wrote", os.path.normpath(out))


if __name__ == "__main__":
    main()
