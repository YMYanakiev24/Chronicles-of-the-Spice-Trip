# Draws the swords Galin added to his stock: assets/swords/<name>.png, 128 x 128 pixel art,
# the hilt at the lower left and the tip at the upper right (a 45 degree slant), like the Iron Sword.
#
# Each sword is drawn on a 64 x 64 grid and doubled. Every pixel is placed by where it falls
# along the sword (u, from the pommel to the tip) and across it (v, + toward the lower right):
# the pommel, the grip, the guard and the blade each say what they cover and what colour it is.
#
# Run from Chronicles-of-the-Spice-Trip/:   python3 tools/swords.py assets/swords      (needs pillow, numpy)
import math, os, sys
import numpy as np
from PIL import Image

N = 64
P0 = np.array([4.0, 59.5])          # the end of the pommel
P1 = np.array([60.0, 3.5])          # the tip
D = (P1 - P0) / np.linalg.norm(P1 - P0)
NRM = np.array([-D[1], D[0]])        # across the sword, + toward the lower right
L = float(np.linalg.norm(P1 - P0))


def C(h, a=255):
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


def mix(a, b, t):
    return tuple(int(round(a[i] * (1 - t) + b[i] * t)) for i in range(4))


class Sword:
    """Parts are functions (u, v) -> colour or None, tried in order: the first that covers a pixel wins."""

    def __init__(self):
        self.parts = []
        self.outline = C('1a1420')
        self.sparks = []            # (x, y, colour) drawn after the outline

    def add(self, fn):
        self.parts.append(fn)
        return self

    def render(self):
        img = np.zeros((N, N, 4), np.uint8)
        filled = np.zeros((N, N), bool)
        for y in range(N):
            for x in range(N):
                p = np.array([x + 0.5, y + 0.5]) - P0
                u, v = float(p @ D), float(p @ NRM)
                for fn in self.parts:
                    c = fn(u, v)
                    if c is not None:
                        img[y, x] = c
                        filled[y, x] = True
                        break
        # a dark outline round everything, the way the shop's other weapons have one
        out = img.copy()
        for y in range(N):
            for x in range(N):
                if filled[y, x]:
                    continue
                if (y > 0 and filled[y - 1, x]) or (y < N - 1 and filled[y + 1, x]) or \
                   (x > 0 and filled[y, x - 1]) or (x < N - 1 and filled[y, x + 1]):
                    out[y, x] = self.outline
        for (x, y, c) in self.sparks:
            if 0 <= x < N and 0 <= y < N and out[y, x, 3] == 0:
                out[y, x] = c
        return Image.fromarray(out, 'RGBA').resize((N * 2, N * 2), Image.NEAREST)


# ---- parts ------------------------------------------------------------------------------

def pommel(at, r, cols, gem=None):
    """A round pommel centred `at` along the sword. cols: dark, mid, light."""
    def fn(u, v):
        d = math.hypot(u - at, v)
        if d > r:
            return None
        if gem and d < r * 0.5:
            return gem[1] if (u - at) - v < 0 else gem[0]
        light = (-(u - at) - v) / (r * 1.4)          # lit from the upper left
        return cols[2] if light > 0.35 else cols[1] if light > -0.3 else cols[0]
    return fn


def grip(u0, u1, w, cols, wrap=3.0, wrap2=None):
    """A grip from u0 to u1, `w` either side, wrapped every `wrap` (cols: dark, mid, light)."""
    def fn(u, v):
        if not (u0 <= u <= u1 and abs(v) <= w):
            return None
        k = (u - u0 + v * 0.9) % wrap
        if wrap2 is not None and (u - u0) % (wrap * 2) < wrap:
            band = wrap2
        else:
            band = cols
        if k < 0.8:
            return band[0]
        return band[2] if v < -w * 0.3 else band[1]
    return fn


def guard(at, half, thick, cols, droop=0.0, tips=None):
    """A crossguard at `at`, reaching `half` either side; droop bends its ends toward the hilt."""
    def fn(u, v):
        bend = droop * (v / half) ** 2 if half else 0
        if abs(v) > half or abs(u - at + bend) > thick:
            if tips and abs(abs(v) - half) < tips[0] and abs(u - at + droop) < tips[0]:
                return cols[1]
            return None
        if (u - at + bend) < -thick * 0.2:
            return cols[0]
        return cols[2] if v < 0 else cols[1]
    return fn


def blade(u0, u1, half, cols, tip=12.0, curve=0.0, fuller=None, edge=None, back=None, profile=None, extra=None):
    """A blade from u0 to the tip at u1. cols: shade side, middle, lit side.
    curve bends it (sabre); profile(t) scales the half-width along it (t: 0 at the guard, 1 at the tip)."""
    def fn(u, v):
        if not (u0 <= u <= u1):
            return None
        t = (u - u0) / (u1 - u0)
        vv = v - curve * t * t                           # curve: how far (in pixels) the tip bends aside
        w = half * (profile(t) if profile else 1.0)
        if u > u1 - tip:                                  # the point
            w *= max(0.0, (u1 - u) / tip)
        if w <= 0.15 or abs(vv) > w:
            return None
        if extra:
            c = extra(u, vv, w, t)
            if c is not None:
                return c
        if back is not None and vv < -w + 1.3:            # a single-edged blade's thick back
            return back
        if edge is not None and abs(vv) > w - 1.0:        # the honed edge catches the light
            return edge
        if fuller is not None and abs(vv) < 0.55 and t < 0.75:
            return fuller
        return cols[2] if vv < 0 else cols[0] if vv > w * 0.5 else cols[1]
    return fn


# ---- the swords -------------------------------------------------------------------------
# u runs 0 (the end of the pommel) to about 79 (the tip); v is across, in grid pixels.

def machete():
    s = Sword()
    steel = (C('6f7682'), C('9aa1ab'), C('d4d9df'))
    s.add(pommel(2.4, 2.4, (C('1c1c22'), C('2c2c34'), C('4a4a56'))))
    rivets = lambda u, v: C('b8bec8') if min(abs(u - 8), abs(u - 16)) < 0.8 and abs(v) < 0.8 else None
    s.add(rivets)
    s.add(grip(3.4, 21, 2.6, (C('141418'), C('26262e'), C('3c3c48')), wrap=3.0))
    s.add(guard(21.6, 3.8, 1.2, (C('2a2a32'), C('3e3e4a'), C('5a5a68'))))
    # a wide, single-edged blade: a dark straight back, a belly toward the tip, a clipped point
    s.add(blade(22.5, 79, 5.0, steel, tip=11, back=C('545a64'), edge=C('f2f5f8'),
                profile=lambda t: 0.8 + 0.4 * math.sin(t * math.pi * 0.85)))
    return s


def rapier():
    s = Sword()
    gold = (C('8a5a10'), C('d9a520'), C('ffe27a'))
    s.add(pommel(2.6, 2.6, gold))
    s.add(grip(4.6, 18.5, 1.9, (C('3a1f12'), C('6e3a1e'), C('9a5a32')), wrap=2.4, wrap2=gold))
    # the swept hilt: a ring round the hand, and a slim cross with knobbed ends
    def ring(u, v):
        d = math.hypot((u - 16.5) / 1.3, v + 3.2)
        return (gold[2] if v < -3.5 else gold[1]) if 4.0 < d < 5.4 and v < 1.6 else None
    s.add(ring)
    s.add(guard(20.2, 6.6, 1.0, gold, droop=-1.2, tips=(1.3,)))
    s.add(blade(21, 79, 1.7, (C('9aa1ab'), C('c8ced6'), C('f6f9fb')), tip=16, profile=lambda t: 1.2 - 0.4 * t))
    return s


def claymore():
    s = Sword()
    steel = (C('5f6672'), C('939aa6'), C('d9dee5'))
    s.add(pommel(3.0, 3.0, (C('5a5f6a'), C('8a909c'), C('c9ced6')), gem=(C('7a1420'), C('d8343f'))))
    s.add(grip(5.6, 22.5, 2.3, (C('4a1418'), C('7c2028'), C('a83a40')), wrap=2.6))
    s.add(guard(23.6, 10.5, 1.5, (C('4a4f5a'), C('7d838e'), C('c4cad2')), droop=3.2, tips=(1.9,)))
    s.add(blade(25, 79.5, 4.3, steel, tip=10, fuller=C('6a717c'), edge=C('f0f3f6')))
    return s


def vampire():
    s = Sword()
    blood = (C('4a0610'), C('8c1020'), C('d42c3e'))
    s.add(pommel(2.7, 2.7, (C('1a1418'), C('2e262c'), C('4a3e46')), gem=(C('8c1020'), C('ff4a5e'))))
    s.add(grip(4.8, 19.5, 2.2, (C('120c10'), C('241a20'), C('3a2c34')), wrap=2.8, wrap2=(C('4a0610'), C('8c1020'), C('b42030'))))
    # a bat's wings for a guard, swept back toward the hilt, with a scalloped trailing edge
    def wings(u, v):
        a = abs(v)
        if a > 8.6 or a < 1.8:
            return None
        top = 21.4 + 0.12 * a
        bottom = 21.4 - 2.2 - 0.6 * a + (1.3 if math.sin(a * 1.6) > 0.3 else 0)
        if bottom <= u <= top:
            return C('2e262c') if v > 0 else C('51444d')
        return None
    s.add(wings)
    s.add(guard(21.2, 2.2, 1.6, (C('1a1418'), C('2e262c'), C('4a3e46'))))
    s.add(blade(22.5, 78, 3.1, blood, tip=13, curve=-6.0, edge=C('ff5c6c'),
                profile=lambda t: 1.0 + 0.3 * math.sin(t * math.pi)))
    s.sparks = [(51, 26, C('d42c3e')), (55, 31, C('8c1020')), (49, 33, C('8c1020'))]   # drops off the blade
    return s


def ember():
    s = Sword()
    iron = (C('231c1e'), C('382a2c'), C('56423e'))
    s.add(pommel(2.6, 2.6, (C('1a1416'), C('2e2426'), C('4a3a3a')), gem=(C('c4501a'), C('ffb040'))))
    s.add(grip(4.6, 19.5, 2.3, (C('1a1214'), C('2a1c1c'), C('402a26')), wrap=2.8, wrap2=(C('7a2a10'), C('c4501a'), C('ff8a30'))))
    s.add(guard(20.6, 6.4, 1.5, (C('241c1e'), C('3a2e2e'), C('5e4a48')), droop=-1.6, tips=(1.5,)))
    def glow(u, vv, w, t):
        a = abs(vv)
        if a > w - 0.9:
            return C('ffd870') if (int(u * 1.3) % 3) else C('ff9a3a')     # the burning edge
        if a > w - 1.7:
            return C('d8521c')
        crack = math.sin(u * 0.55) * w * 0.4
        if abs(vv - crack) < 0.5 and t < 0.9:
            return C('ff9a30')                                           # a vein of fire inside
        return None
    s.add(blade(22, 79, 4.0, iron, tip=11, extra=glow))
    s.sparks = [(46, 15, C('ffb040')), (54, 10, C('ff8a30')), (59, 19, C('ffd870')), (40, 24, C('ff8a30')), (62, 9, C('ffb040')), (50, 24, C('ffd870'))]
    return s


def cinder():
    s = Sword()
    obsid = (C('140f16'), C('241b27'), C('3e2f43'))
    s.add(pommel(3.0, 3.0, (C('2a1a14'), C('4a2a1a'), C('6a3a20')), gem=(C('ff6a10'), C('ffe060'))))
    s.add(grip(5.6, 20.5, 2.4, (C('1a1210'), C('2c1e18'), C('46301e')), wrap=2.6, wrap2=(C('8a2a08'), C('d8501a'), C('ff9a40'))))
    # the guard is the Cinder-Maw's jaw: bone teeth either side
    def jaw(u, v):
        a = abs(v)
        if a > 8.8:
            return None
        tooth = (a % 2.2) < 1.2
        if 20.8 <= u <= 23.4 + (2.2 * (1 - (a % 2.2) / 1.2) if tooth and a > 2.2 else 0):
            return C('efe4c8') if v < 0 else C('bfb194')
        return None
    s.add(jaw)
    s.add(guard(21.4, 3.6, 1.7, (C('2a1a14'), C('4a2a1a'), C('6a3a20'))))
    def lava(u, vv, w, t):
        if (int(u * 0.8) % 3 == 0) and vv > w - 1.4:
            return None                                                  # a serrated edge
        crack = math.sin(u * 0.5 + 1) * w * 0.45 + math.sin(u * 1.7) * 0.5
        if abs(vv - crack) < 0.45 and t < 0.95:
            return C('ffc050')
        if abs(vv - crack) < 1.0 and t < 0.95:
            return C('d8501a')
        if abs(vv) > w - 0.8:
            return C('ff6a20')
        return None
    s.add(blade(24, 79.5, 4.7, obsid, tip=12, extra=lava, profile=lambda t: 1.0 - 0.2 * t))
    s.sparks = [(50, 13, C('ffb040')), (58, 8, C('ff6a20')), (61, 17, C('ffe060')), (43, 21, C('ff6a20'))]
    return s


SWORDS = {'machete': machete, 'rapier': rapier, 'claymore': claymore, 'vampire': vampire, 'ember': ember, 'cinder': cinder}

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/swords'
    os.makedirs(out, exist_ok=True)
    for name, make in SWORDS.items():
        make().render().save(os.path.join(out, name + '.png'), optimize=True)
        print('wrote', os.path.join(out, name + '.png'))
