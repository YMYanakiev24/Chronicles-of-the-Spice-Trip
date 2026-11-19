"""Draws the stairs that open in a boss's lair once it falls: a stairwell of black stone
cut into the floor, steps going down into a red glow, runes round its rim, skulls on its posts.

    python tools/stairs.py

Writes assets/dungeon/boss_stairs.png: two 60 x 52 frames (drawn at 2x, like the dungeon
tiles). Frame 0 is the stairwell; frame 1 is its glow (the runes and the light from below),
drawn over it additively so it can pulse. The middle of a frame sits on the middle of the room.
"""
import os
import random
from PIL import Image

W, H = 60, 52
OUT = (14, 10, 18)
ST0, ST1, ST2, ST3, ST4 = (30, 24, 36), (44, 36, 52), (60, 50, 70), (80, 68, 90), (104, 92, 112)
BONE_D, BONE, BONE_L = (96, 86, 76), (168, 158, 140), (214, 206, 188)
EMBER_D, EMBER, EMBER_L = (88, 14, 22), (150, 26, 32), (214, 58, 44)
RUNES = [  # 3x4 glyphs, carved
    ["x.x", ".x.", "x.x", ".x."],
    ["xx.", "x.x", "xx.", "x.."],
    [".x.", "xxx", ".x.", ".x."],
    ["x..", "xx.", "x.x", "x.."],
    ["xxx", "..x", ".x.", "x.."],
    [".xx", "x..", ".x.", "xx."],
]


def frames():
    rnd = random.Random(13)
    art = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    a, g = art.load(), glow.load()

    def put(x, y, c, alpha=255):
        if 0 <= x < W and 0 <= y < H:
            a[x, y] = c + (alpha,)

    def lit(x, y, c, alpha=255):
        if 0 <= x < W and 0 <= y < H:
            g[x, y] = c + (alpha,)

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                put(x, y, c)

    # cracks in the floor round it, where the stone gave way
    for (sx, sy, dx, dy, n) in [(3, 30, -1, 1, 6), (56, 14, 1, -1, 5), (8, 47, -1, 1, 5), (52, 46, 1, 1, 6), (30, 49, 0, 1, 3)]:
        x, y = sx, sy
        for i in range(n):
            put(x, y, OUT, 200)
            x += dx if rnd.random() < 0.7 else 0
            y += dy if rnd.random() < 0.8 else 0

    # the rim: big blocks of black stone, lit from above
    ox0, oy0, ox1, oy1 = 5, 7, 54, 46               # outside of the rim
    ix0, iy0, ix1, iy1 = 11, 12, 48, 41             # the opening
    rect(ox0, oy0, ox1, oy1, OUT)
    blocks = []
    x = ox0 + 1
    while x < ox1:                                   # far edge and near edge
        w = rnd.randint(5, 8)
        blocks.append((x, oy0 + 1, min(x + w - 1, ox1 - 1), iy0 - 1))
        blocks.append((x, iy1 + 1, min(x + w - 1, ox1 - 1), oy1 - 1))
        x += w + 1
    y = iy0
    while y <= iy1:                                  # the sides
        h = rnd.randint(5, 8)
        blocks.append((ox0 + 1, y, ix0 - 1, min(y + h - 1, iy1)))
        blocks.append((ix1 + 1, y, ox1 - 1, min(y + h - 1, iy1)))
        y += h + 1
    for (x0, y0, x1, y1) in blocks:
        rect(x0, y0, x1, y1, ST2)
        for xx in range(x0, x1 + 1):
            put(xx, y0, ST4 if xx < x1 else ST3)
        for yy in range(y0, y1 + 1):
            put(x0, yy, ST3)
        for xx in range(x0 + 1, x1 + 1):
            put(xx, y1, ST1)
        if rnd.random() < 0.5:
            put(rnd.randint(x0 + 1, max(x0 + 1, x1 - 1)), rnd.randint(y0 + 1, max(y0 + 1, y1 - 1)), ST1)

    # the stairwell: walls going down on either side, steps going away into the dark
    rect(ix0, iy0, ix1, iy1, OUT)
    steps = 5
    for s in range(steps):                           # s = 0: the top step, nearest you
        y1 = iy1 - s * 5
        y0 = y1 - 4
        inset = s                                    # narrower as they go down
        dark = 1 - 0.17 * s
        tread = tuple(int(c * dark) for c in ST2)
        riser = tuple(int(c * dark * 0.7) for c in ST1)
        for yy in range(y0, y1 + 1):
            for xx in range(ix0 + 1 + inset, ix1 - inset):
                put(xx, yy, tread if yy <= y0 + 2 else riser)
        for xx in range(ix0 + 1 + inset, ix1 - inset):  # each tread's lip catches the light
            put(xx, y0, tuple(int(c * dark) for c in ST4))
        for yy in range(y0, y1 + 1):                 # the walls of the well, either side
            for k in range(inset + 1):
                put(ix0 + 1 + k, yy, tuple(int(c * dark * 0.5) for c in ST1))
                put(ix1 - 1 - k, yy, tuple(int(c * dark * 0.4) for c in ST1))
        for xx in range(ix0 + 2 + inset, ix1 - 1 - inset):   # the light from below on the lips, stronger as they go down
            lit(xx, y0, EMBER, int(25 + 40 * s))
    # the bottom: blackness, and a red light coming up out of it
    deep = iy1 - steps * 5
    for yy in range(iy0, deep + 1):
        for xx in range(ix0 + 1 + steps, ix1 - steps):
            put(xx, yy, OUT)
            k = (yy - iy0) / max(1, deep - iy0)      # brighter where it falls away
            lit(xx, yy, EMBER_L if k > 0.6 else EMBER, int(90 + 165 * k))
        for k in range(steps + 1):                   # the walls go down into it too
            put(ix0 + 1 + k, yy, (6, 4, 8)); put(ix1 - 1 - k, yy, (6, 4, 8))

    # runes cut in the near rim and the sides
    spots = [(15, 43), (22, 43), (29, 43), (36, 43), (43, 43), (7, 18), (7, 30), (50, 18), (50, 30)]
    for i, (rx, ry) in enumerate(spots):
        glyph = RUNES[i % len(RUNES)]
        for gy, row in enumerate(glyph):
            for gx, ch in enumerate(row):
                if ch == "x":
                    put(rx + gx, ry + gy - 1, EMBER_D)
                    lit(rx + gx, ry + gy - 1, EMBER_L, 255)

    # posts at the far corners, a skull on each; bones on the rim
    for (px, py) in [(3, 2), (51, 2)]:
        rect(px, py + 3, px + 5, py + 10, OUT)
        rect(px + 1, py + 4, px + 4, py + 9, ST2)
        for yy in range(py + 4, py + 10):
            put(px + 1, yy, ST3)
        skull = [".xxxx.", "xxxxxx", "x.xx.x", "xxxxxx", ".x.x.x"]
        for sy, row in enumerate(skull):
            for sx, ch in enumerate(row):
                if ch == "x":
                    put(px + sx, py - 1 + sy, BONE if sy < 2 else BONE_D if sy == 4 else BONE)
                elif sy == 2:
                    put(px + sx, py - 1 + sy, OUT)       # its eyes
                    lit(px + sx, py - 1 + sy, EMBER_L, 255)
        put(px + 1, py - 1, BONE_L); put(px + 2, py - 1, BONE_L)
    for (bx, by) in [(16, 9), (40, 45), (9, 38)]:
        for k in range(4):
            put(bx + k, by, BONE if k % 3 else BONE_L)
        put(bx - 1, by - 1, BONE_D); put(bx + 4, by + 1, BONE_D)
    return art, glow


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "assets", "dungeon", "boss_stairs.png")
    art, glow = frames()
    sheet = Image.new("RGBA", (W * 2, H), (0, 0, 0, 0))
    sheet.paste(art, (0, 0))
    sheet.paste(glow, (W, 0))
    sheet.save(out)
    print("wrote", os.path.normpath(out))


if __name__ == "__main__":
    main()
