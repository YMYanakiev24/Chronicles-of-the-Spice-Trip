"""Draws the castle ladder: a wooden ladder up the first room's top wall to a small door.

    python tools/ladder.py

Writes assets/dungeon/castle_ladder.png: two frames side by side, 36 x 62 pixels each (drawn at
2x, like the dungeon tiles), the door shut and the door open. The art follows the dungeon
tileset's palette. Frame pixel (18, 24) sits on the castle door's spot at the top of the room
(CASTLE_DOOR_X, 32): the door is set in the wall, the ladder stands on the floor below it.
"""
import os
import random
from PIL import Image

W, H = 36, 62
OUT = (37, 19, 26)
WOOD_D, WOOD, WOOD_L, WOOD_LL = (92, 54, 44), (137, 90, 69), (176, 112, 80), (204, 142, 98)
IRON_D, IRON, IRON_L = (70, 64, 82), (144, 145, 158), (190, 200, 214)
STONE_D, STONE, STONE_L, STONE_LL = (61, 37, 59), (84, 55, 64), (110, 74, 72), (132, 92, 86)
GLOW, GLOW_L = (255, 176, 92), (255, 226, 160)


def frame(open_door):
    rnd = random.Random(7)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = im.load()

    def put(x, y, c, a=255):
        if 0 <= x < W and 0 <= y < H:
            px[x, y] = c + (a,)

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                put(x, y, c)

    # --- the stone surround: a lintel with a keystone over the door, jambs down the wall ---
    rect(6, 0, 29, 25, OUT)
    stones = [(7, 1, 12, 4), (13, 1, 16, 5), (17, 0, 18, 6), (19, 1, 22, 5), (23, 1, 28, 4),   # lintel and keystone
              (7, 5, 9, 9), (7, 10, 9, 15), (7, 16, 9, 20), (7, 21, 9, 24),                  # left jamb
              (26, 5, 28, 9), (26, 10, 28, 15), (26, 16, 28, 20), (26, 21, 28, 24)]          # right jamb
    for (x0, y0, x1, y1) in stones:
        rect(x0, y0, x1, y1, STONE)
        for x in range(x0, x1 + 1):
            put(x, y0, STONE_LL if x < x1 else STONE_L)       # lit from above
        for y in range(y0, y1 + 1):
            put(x0, y, STONE_L)
        for x in range(x0 + 1, x1 + 1):
            put(x, y1, STONE_D)
        if (x1 - x0) > 2 and rnd.random() < 0.6:                # a chip or two
            put(rnd.randint(x0 + 1, x1 - 1), rnd.randint(y0 + 1, max(y0 + 1, y1 - 1)), STONE_D)
    for x in range(17, 19):                                     # the keystone stands proud
        put(x, 0, STONE_LL)

    # --- the doorway ---
    rect(10, 6, 25, 24, OUT)
    if not open_door:
        # six planks, seams between, iron straps on the hinge side, a ring to pull
        for x in range(11, 25):
            plank = (x - 11) % 3
            for y in range(6, 24):
                c = WOOD_L if plank == 0 else WOOD if plank == 1 else WOOD_D
                if plank == 1 and rnd.random() < 0.08:
                    c = WOOD_D                                   # grain
                put(x, y, c)
        for y in range(6, 24):                                   # the top of the door catches the light
            put(11, y, WOOD_LL if y < 9 else WOOD_L)
        for sy in (9, 19):                                       # hinge straps, studded
            for x in range(11, 21):
                put(x, sy, IRON if x < 20 else IRON_D)
                put(x, sy + 1, IRON_D)
            for x in (12, 15, 18):
                put(x, sy, IRON_L)
        for (x, y) in [(21, 13), (22, 12), (23, 13), (21, 15), (23, 15), (22, 16), (21, 14), (23, 14)]:   # the ring
            put(x, y, IRON)
        put(22, 12, IRON_L)
        put(22, 11, IRON_D)                                      # its mount
        put(22, 17, OUT); put(22, 18, OUT)                       # the keyhole
    else:
        # swung in: warm light from the castle, the door's edge against the left jamb
        for y in range(6, 24):
            for x in range(11, 25):
                t = (y - 6) / 17
                c = GLOW_L if (x > 14 and y > 12 and rnd.random() < 0.25) else GLOW
                dim = 0.55 + 0.45 * t                            # brighter towards the sill
                put(x, y, tuple(int(v * dim) for v in c))
        for y in range(6, 24):
            put(11, y, WOOD_D); put(12, y, WOOD)
        for x in (13, 14):
            for y in range(6, 24):
                r, g, b, _ = px[x, y]
                put(x, y, (r // 2, g // 2, b // 2))
    rect(10, 24, 25, 25, STONE_LL)                               # the sill
    for x in range(10, 26):
        put(x, 25, STONE_D)

    # --- the ladder: two rails from the sill down to the floor, splaying a little at the foot ---
    top, bottom = 22, 61

    def rails(y):                                                # left edges of the two-pixel rails at row y
        k = (y - top) / (bottom - top)
        return 12 - round(2 * k), 22 + round(2 * k)

    rungs = list(range(top + 4, bottom - 2, 5))
    for y in range(top + 2, bottom + 1):                         # its shadow, a little to the right
        for x0 in rails(y):
            for x in (x0 + 2, x0 + 3):
                if px[x, y][3] == 0 or px[x, y][:3] in (STONE_D,):
                    put(x, y, (12, 6, 10), 110)
    for ry in rungs:
        l, r = rails(ry)
        for x in range(l + 2, r + 4):
            if px[x, ry + 2][3] == 0:
                put(x, ry + 2, (12, 6, 10), 90)
    for y in range(top, bottom + 1):
        for x0 in rails(y):
            put(x0 - 1, y, OUT)
            put(x0, y, WOOD_LL if y < top + 3 else WOOD_L)
            put(x0 + 1, y, WOOD_D if rnd.random() < 0.12 else WOOD)
            put(x0 + 2, y, OUT)
    for x0 in rails(top):
        put(x0, top, OUT); put(x0 + 1, top, OUT)                 # rounded ends
    for x0 in rails(bottom):
        put(x0, bottom, WOOD_D); put(x0 + 1, bottom, OUT)
    for ry in rungs:
        l, r = rails(ry)
        for x in range(l + 3, r - 1):
            put(x, ry - 1, OUT)
            put(x, ry, WOOD_L if (x + ry) % 7 else WOOD_LL)
            put(x, ry + 1, WOOD_D)
            put(x, ry + 2, OUT)
        put(l + 1, ry, IRON_L); put(r, ry, IRON_L)              # nails through the rails
    return im


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "assets", "dungeon", "castle_ladder.png")
    sheet = Image.new("RGBA", (W * 2, H), (0, 0, 0, 0))
    sheet.paste(frame(False), (0, 0))
    sheet.paste(frame(True), (W, 0))
    sheet.save(out)
    print("wrote", os.path.normpath(out))


if __name__ == "__main__":
    main()
