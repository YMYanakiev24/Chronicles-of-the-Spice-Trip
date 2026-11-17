# Makes each champion's walking figure in their own outfits from Beray's Drip Locker:
#   assets/outfits/<champion>/<outfit>.png         (the outfits are listed in OUTFITS below)
#
# Every outfit is made from the champion's own walk sheet (assets/walk/walk-<champion>.png),
# frame by frame, so it walks exactly as they do. The head and hands stay as they are; the
# rest is redressed from parts:
#   top / sleeves / legs / shoes   recoloured, keeping the figure's own light and shade
#   pattern                        stripes, plaid, dots, stars, hearts, flames, tie-dye...
#   robe                           a long garment over the legs, flaring to a hem that follows the stride
#   cape                           hanging behind (over the back, seen from behind)
#   trim, belt, emblem             a coloured edge, a band at the waist, a badge on the chest or back
#   neck, face, hat                chains, beads, a bow tie or tie; shades; caps, crowns, headbands...
#   glow                           a soft aura round the whole figure
# The names, prices and descriptions in the game are in include/Champions.h (OUTFITS); keep the
# keys there and here the same.
#
# Run from Chronicles-of-the-Spice-Trip/ after changing a walk sheet or an outfit:
#   python3 tools/outfits.py assets/walk assets/outfits      (needs numpy, scipy, pillow)
#   python3 tools/outfits.py assets/walk /tmp/out gakev       (just one champion)
import colorsys, glob, math, os, sys
import numpy as np
from PIL import Image
from scipy import ndimage


def C(h): return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)


# ----------------------------------------------------------------------------- the outfits
# Colour ramps are (shade, mid, light). A part left out keeps what the champion wears.
OUTFITS = {
    'hero': {      # Spellwoven
        'starlit': dict(top=['1a1450', '2e2a8a', '4a48c0'], legs=['1a1450', '2e2a8a', '4a48c0'], pattern='stars', pat=['c9a227', 'f2d060', 'fff4b0'],
                        robe=0.96, trim='f2c94c', hat='cap', hatc=['1a1450', '2e2a8a', 'f2c94c'], belt='f2c94c'),
        'ember':   dict(top=['5a0e0e', '9e1c14', 'd8401c'], legs=['5a0e0e', '9e1c14', 'd8401c'], pattern='flames', pat=['d86a10', 'ffa020', 'ffe070'],
                        robe=0.96, cape=['1a1212', '2c2020', '463434'], belt='1a1212', emblem='flame', emc='ffb030'),
        'archmage': dict(top=['b8b4c8', 'eae8f2', 'ffffff'], legs=['b8b4c8', 'eae8f2', 'ffffff'], robe=0.96, trim='d4a017',
                         cape=['3c1a6a', '6a34b0', '9a68e0'], belt='d4a017', emblem='star', emc='d4a017', glow='b07aff'),
    },
    'atanas': {
        'blackbelt': dict(top=['0e0e12', '1e1e26', '3a3a46'], legs=['0e0e12', '1e1e26', '3a3a46'], trim='d4a017', belt='c4202a'),
        'saffron':   dict(top=['a8460a', 'e07818', 'ffb050'], legs=['a8460a', 'e07818', 'ffb050'], robe=0.94, sleeves=['7a2e08', 'b0501a', 'e07818'],
                          neck='beads', neckc='5a2a10', belt='7a2e08'),
        'zen':       dict(top=['8ab4c4', 'cfe8f0', 'ffffff'], legs=['8ab4c4', 'cfe8f0', 'ffffff'], robe=0.94, trim='4ab8c8',
                          belt='4ab8c8', emblem='circle', emc='4ab8c8', glow='7ae8f0'),
    },
    'gakev': {
        'collector': dict(top=['0e4a26', '17803c', '3cb862'], legs=['14141a', '26262e', '44444f'], emblem='coin', emc='f2c94c', neck='chain',
                          shoes=['f2c94c', 'fff3c4']),
        'tracksuit': dict(top=['8a0e14', 'c8202a', 'f05058'], legs=['8a0e14', 'c8202a', 'f05058'], pattern='sidestripe', pat=['d0d0d8', 'f4f4f8', 'ffffff'],
                          shoes=['f4f4f8', 'c8202a'], neck='chain'),
        'goldplated': dict(top=['8a5a10', 'd9a520', 'ffe27a'], legs=['8a5a10', 'd9a520', 'ffe27a'], pattern='sparkle', pat=['ffe27a', 'fff6c8', 'ffffff'],
                           hat='crown', emblem='coin', emc='fff6c8', shoes=['fff3c4', 'd9a520'], glow='ffd040'),
    },
    'nikolay': {
        'knight':  dict(top=['5c6470', '96a0ac', 'dce2ea'], legs=['5c6470', '96a0ac', 'dce2ea'], pattern='plates', pat=['3c444e', '5c6470', '96a0ac'],
                        hatc=['5c6470', '96a0ac', 'dce2ea'], hat='helm', tabard=['143a7a', '2a5cb8', '5a8ae0'], emblem='mug', emc='f2c94c'),
        'tavern':  dict(top=['b8b0a0', 'ece6da', 'ffffff'], legs=['3a2614', '5c3c20', '86603a'], tabard=['6a3c1c', '9a5a2c', 'c48a50'],
                        hat='headband', hatc=['8a1414', 'c42020', 'e85050']),
        'paladin': dict(top=['8a5a10', 'd9a520', 'ffe27a'], legs=['8a5a10', 'd9a520', 'ffe27a'], pattern='plates', pat=['6a4208', '8a5a10', 'd9a520'],
                        hat='helm', hatc=['8a5a10', 'd9a520', 'ffe27a'], cape=['b0b0c0', 'ececf4', 'ffffff'], emblem='cross', emc='ffffff', glow='ffe27a'),
    },
    'pupesh': {
        'watermelon': dict(top=['14501e', '23802e', '4cb04a'], legs=['14501e', '23802e', '4cb04a'], pattern='hstripes', pat=['0a3412', '14501e', '23802e'],
                           trim='e84860', hat='cap', hatc=['c42040', 'e84860', 'ff8aa0']),
        'honeydew':   dict(top=['8aa86a', 'c8e0a0', 'eef8d8'], legs=['6a8a50', 'a0c080', 'c8e0a0'], pattern='dots', pat=['6a8a50', '8aa86a', 'c8e0a0'],
                           belt='6a8a50'),
        'melonlord':  dict(top=['b05a10', 'e88a28', 'ffc070'], legs=['b05a10', 'e88a28', 'ffc070'], pattern='net', pat=['f2d8a8', 'fff0d0', 'ffffff'],
                           cape=['14501e', '23802e', '4cb04a'], hat='crown', glow='ffb040'),
    },
    'ristyo': {
        'partyshirt': dict(top=['0a7a7a', '16b0a8', '50e0d0'], sleeves=['0a7a7a', '16b0a8', '50e0d0'], pattern='flowers', pat=['e8406a', 'ff8a4a', 'fff070'],
                           legs=['c8b48a', 'e8d8b0', 'fff4dc'], face='shades'),
        'bartender':  dict(top=['14141a', '24242e', '3c3c4a'], sleeves=['c8c4bc', 'f0ece4', 'ffffff'], legs=['14141a', '24242e', '3c3c4a'],
                           neck='bowtie', neckc='c42020'),
        'disco':      dict(top=['c8c4d0', 'f4f0fa', 'ffffff'], legs=['c8c4d0', 'f4f0fa', 'ffffff'], pattern='sparkle', pat=['e040c0', '60e0ff', 'fff070'],
                           neck='chain', face='shades', shoes=['f2c94c', 'fff3c4'], glow='ff60e0'),
    },
    'iliqnpuh': {
        'honey':    dict(top=['a8620a', 'e09a1c', 'ffd060'], legs=['5a3a14', '86582a', 'b0804a'], pattern='honeycomb', pat=['7a4808', 'a8620a', 'e09a1c'],
                         emblem='drop', emc='ffe070'),
        'grizzly':  dict(top=['3a2210', '5e3a1c', '8a5a30'], legs=['3a2210', '5e3a1c', '8a5a30'], pattern='fur', pat=['2a180a', '4a2c14', '6e4624'],
                         emblem='claw', emc='e8dcc0'),
        'polar':    dict(top=['b8c8d8', 'eaf2fa', 'ffffff'], legs=['b8c8d8', 'eaf2fa', 'ffffff'], pattern='fur', pat=['a0b4c8', 'd0e0ee', 'f4faff'],
                         trim='5fb7ef', hat='crown', hatc=['5f9fd0', 'a0d8ff', 'e8f8ff'], glow='a0e0ff'),
    },
    'daniil': {
        'fightergi': dict(top=['b8b4ac', 'ecebe6', 'ffffff'], legs=['b8b4ac', 'ecebe6', 'ffffff'], belt='14141a', hat='headband', hatc=['8a1010', 'cc2020', 'f05050'],
                          shoes=['c8a070', 'e8c898']),
        'biker':     dict(top=['3a2214', '5e3a20', '8a5a34'], legs=['14182a', '242c48', '3c4870'], pattern='studs', pat=['a0a4ac', 'd8dce4', 'ffffff'],
                          face='shades', neck='chain'),
        'goldsling': dict(top=['8a5a10', 'd9a520', 'ffe27a'], legs=['14141a', '26262e', '44444f'], trim='14141a', emblem='coin', emc='14141a',
                          neck='chain', shoes=['f2c94c', 'fff3c4'], glow='ffd040'),
    },
    'storyteller': {
        'bathrobe': dict(top=['b8bcc8', 'eceff6', 'ffffff'], sleeves=['b8bcc8', 'eceff6', 'ffffff'], legs=['b8bcc8', 'eceff6', 'ffffff'], robe=0.82,
                         trim='5f8fd0', belt='5f8fd0', shoes=['5f8fd0', '9ac0f0']),
        'koi':      dict(top=['8a0e14', 'c42020', 'f05050'], sleeves=['8a0e14', 'c42020', 'f05050'], legs=['8a0e14', 'c42020', 'f05050'], robe=0.95,
                         pattern='koi', pat=['ff8a20', 'ffffff', 'ffe0b0'], belt='14141a', shoes=['3a2a1c', '5c4630']),
        'bard':     dict(top=['14501e', '23802e', '4cb04a'], sleeves=['c8b48a', 'e8d8b0', 'fff4dc'], legs=['3a2614', '5c3c20', '86603a'], trim='d4a017',
                         cape=['4a1a6a', '7a34a8', 'a868d8'], hat='cap', hatc=['4a1a6a', '7a34a8', 'd4a017'], belt='5c3c20', shoes=['3a2614', '5c3c20']),
    },
    'fairy': {
        'moonlight': dict(top=['7a7298', 'b8b0d8', 'ece8ff'], legs=['7a7298', 'b8b0d8', 'ece8ff'], robe=0.9, pattern='stars', pat=['c8c8ff', 'ffffff', 'ffffff'],
                          trim='ffffff', glow='c8c0ff'),
        'sprite':    dict(top=['1e6a2a', '34a040', '70d870'], legs=['1e6a2a', '34a040', '70d870'], robe=0.9, pattern='leaves', pat=['145020', '1e6a2a', 'a0e870'],
                          hat='flowers', hatc=['ff6a9a', 'fff070', 'ffffff']),
        'queen':     dict(top=['8a5a10', 'd9a520', 'ffe27a'], legs=['8a5a10', 'd9a520', 'ffe27a'], robe=0.92, pattern='sparkle', pat=['fff6c8', 'ffffff', 'ffffff'],
                          hat='crown', glow='ffe060'),
    },
    'momiche': {
        'lumberjack': dict(top=['7a0e0e', 'b81c1c', 'e04848'], legs=['1c2c5a', '2e4688', '4a6ab8'], pattern='plaid', pat=['14141a', '24242e', '3c3c4a'],
                           hat='beanie', hatc=['3a2614', '5c3c20', '86603a'], shoes=['5c3c20', '86603a']),
        'pumpkin':    dict(top=['b0500a', 'e87a14', 'ffb050'], legs=['14141a', '26262e', '44444f'], pattern='hstripes_legs', pat=['b0500a', 'e87a14', 'ffb050'],
                           emblem='pumpkin', emc='14141a', hat='beanie', hatc=['14141a', '26262e', 'e87a14']),
        'screamqueen': dict(top=['14101a', '241c2e', '3c3048'], legs=['14101a', '241c2e', '3c3048'], robe=0.95, trim='c41c3c', emblem='bat', emc='c41c3c',
                            cape=['3a0814', '6a1024', '9a2038'], glow='b040ff'),
    },
    'gymrat': {
        'neon':    dict(top=['3a8a0a', '6ed01c', 'b8ff60'], arms='bare', bare_legs=True, legs=['14141a', '24242e', '3c3c4a'], hat='headband',
                        hatc=['c8c8d0', 'f4f4f8', 'ffffff'], shoes=['6ed01c', 'f4f4f8']),
        'cpp':     dict(top=['14204a', '22367a', '3a58b0'], legs=['3a3a46', '5a5a6a', '8a8a9a'], emblem='cpp', emc='7ac8ff', hat='headphones',
                        hatc=['14141a', '2a2a34', '7ac8ff']),
        'olympia': dict(top=['8a5a10', 'd9a520', 'ffe27a'], arms='bare', bare_legs=True, legs=['8a5a10', 'd9a520', 'ffe27a'], belt='fff0a0',
                        pattern='sparkle', pat=['fff6c8', 'ffffff', 'ffffff'], hat='crown', shoes=['f2c94c', 'fff3c4'], glow='ffd040'),
    },
    'vanessa': {
        'redcarpet': dict(top=['7a0a14', 'b8142a', 'e84858'], legs=['7a0a14', 'b8142a', 'e84858'], robe=0.97, pattern='sparkle', pat=['ff9aa8', 'ffd0d8', 'ffffff'],
                          neck='chain'),
        'pinkhearts': dict(top=['b0306a', 'e85a9a', 'ff9ac8'], arms='bare', bare_legs=True, legs=['b0306a', 'e85a9a', 'ff9ac8'], pattern='hearts', pat=['ffffff', 'ffffff', 'ffffff'],
                           face='heartshades', shoes=['ffffff', 'e85a9a']),
        'heartbreaker': dict(top=['14101a', '241c2e', '3c3048'], legs=['14101a', '241c2e', '3c3048'], robe=0.95, trim='e81c3c', pattern='hearts',
                             pat=['8a0a1c', 'c8142a', 'e81c3c'], hat='crown', glow='ff3050'),
    },
    'galin': {
        'tracksuit': dict(top=['0e1a3a', '1a2e6a', '2e4aa0'], legs=['0e1a3a', '1a2e6a', '2e4aa0'], pattern='sidestripe', pat=['d0d0d8', 'f4f4f8', 'ffffff'],
                          shoes=['f4f4f8', '14141a'], neck='chain'),
        'leather':   dict(top=['3a2214', '5e3a20', '8a5a34'], legs=['14141a', '24242e', '3c3c4a'], trim='2a1a10', neck='chain', belt='14141a'),
        'don':       dict(top=['14141a', '24242e', '3c3c4a'], legs=['14141a', '24242e', '3c3c4a'], pattern='pinstripe', pat=['5a5a6a', '8a8a9a', 'b0b0c0'],
                          neck='tie', neckc='b81c1c', hat='fedora', hatc=['14141a', '2a2a34', 'b81c1c'], shoes=['3a2214', '5e3a20'], glow='9a9aaa'),
    },
    'filqcheto': {
        'tiedye':  dict(top=['000000', '808080', 'ffffff'], pattern='tiedye', legs=['1c2c5a', '2e4688', '4a6ab8']),
        'grinder': dict(top=['145020', '23802e', '4cb04a'], legs=['14141a', '24242e', '3c3c4a'], emblem='leaf', emc='a0f070', hat='bucket',
                        hatc=['145020', '23802e', '4cb04a']),
        'cosmic':  dict(top=['1a0e3a', '2e1a6a', '4a30a0'], legs=['1a0e3a', '2e1a6a', '4a30a0'], pattern='stars', pat=['a0a0ff', 'ffffff', 'ffffff'],
                        hat='bucket', hatc=['1a0e3a', '2e1a6a', 'a0a0ff'], glow='9060ff'),
    },
    'krisa': {
        'boxer':   dict(top=['8a0e14', 'c8202a', 'f05058'], arms='bare', bare_legs=True, legs=['8a0e14', 'c8202a', 'f05058'], trim='ffffff', belt='f2c94c', hat='cap',
                        hatc=['8a0e14', 'c8202a', 'ffffff'], shoes=['f4f4f8', 'c8202a']),
        'athleisure': dict(top=['0a6a6a', '14a0a0', '50d8d0'], legs=['0a6a6a', '14a0a0', '50d8d0'], pattern='sidestripe', pat=['d0d0d8', 'f4f4f8', 'ffffff'],
                           hat='cap', hatc=['0a6a6a', '14a0a0', 'f4f4f8'], shoes=['f4f4f8', '14a0a0']),
        'goldenglove': dict(top=['14141a', '24242e', '3c3c4a'], legs=['14141a', '24242e', '3c3c4a'], trim='f2c94c', belt='f2c94c', emblem='crownmark',
                            emc='f2c94c', hat='cap', hatc=['8a5a10', 'd9a520', 'ffe27a'], shoes=['f2c94c', 'fff3c4'], glow='ffd040'),
    },
}


# ----------------------------------------------------------------------------- the figure
# Where the head and the torso end and the shoes start (fractions of the figure's height),
# where a hat's brim sits, how far a jacket reaches past the legs (in figure-pixels), and
# whether long hair over the shoulders has to stay hair.
ZONES = {'default': (0.40, 0.68, 0.90), 'momiche': (0.33, 0.62, 0.92), 'gymrat': (0.27, 0.58, 0.93),
         'storyteller': (0.42, 0.72, 0.90), 'fairy': (0.40, 0.70, 0.90), 'vanessa': (0.36, 0.57, 0.92),
         'daniil': (0.28, 0.64, 0.92), 'filqcheto': (0.27, 0.58, 0.92), 'krisa': (0.26, 0.50, 0.92),
         'galin': (0.36, 0.62, 0.90), 'nikolay': (0.36, 0.66, 0.90), 'pupesh': (0.38, 0.68, 0.90)}
CAP = {'vanessa': 0.10, 'daniil': 0.09, 'krisa': 0.11, 'filqcheto': 0.10, 'galin': 0.12, 'gymrat': 0.10, 'momiche': 0.10}
REACH = {'daniil': 4.2, 'nikolay': 1.0, 'pupesh': 2.0, 'fairy': 1.0, 'ristyo': 0.6}
HAIR = {'momiche': True, 'vanessa': 'warm', 'krisa': 'warm'}


def ramp(cols, t):
    cols = [C(c) if isinstance(c, str) else c for c in cols]
    t = np.clip(t, 0, 1)
    a = np.clip(t * 2, 0, 1)[..., None]; b = np.clip(t * 2 - 1, 0, 1)[..., None]
    return np.where((t < 0.5)[..., None], cols[0] * (1 - a) + cols[1] * a, cols[1] * (1 - b) + cols[2] * b)


def hsv(rgb):
    r, g, b = [rgb[..., k] / 255.0 for k in range(3)]
    mx, mn = np.maximum(np.maximum(r, g), b), np.minimum(np.minimum(r, g), b)
    d = mx - mn + 1e-9
    h = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    s = np.where(mx > 0, (mx - mn) / (mx + 1e-9), 0)
    return h, s, mx


def is_skin(rgb):
    h, s, v = hsv(rgb)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return (h >= 4) & (h <= 42) & (s >= 0.15) & (s <= 0.72) & (v >= 0.32) & (r > g) & (g >= b - 4)


def hashn(x, y, k=1):
    return ((x * 73856093) ^ (y * 19349663) ^ (k * 83492791)) & 0xffff


class Frame:
    """One frame of a walk sheet, and where its parts are. view: 0 front, 1 back, 2 side (facing right)."""

    def __init__(self, cell, view, scale, name, frame):
        self.view, self.s, self.name, self.frame = view, scale, name, frame
        self.u = max(1, int(round(scale)))                  # one figure-pixel, in pixels
        self.rgb = cell[..., :3].astype(float)
        self.alpha = cell[..., 3].astype(float)
        self.a = cell[..., 3] > 100
        self.H, self.W = cell.shape[:2]
        ys, xs = np.nonzero(self.a)
        self.ok = len(ys) > 0
        if not self.ok:
            return
        self.top, self.bot = ys.min(), ys.max()
        h = self.h = self.bot - self.top + 1
        self.y = np.arange(self.H)[:, None]; self.x = np.arange(self.W)[None, :]
        he, te, sh = ZONES.get(name, ZONES['default'])
        self.he, self.te, self.sh = he, te, sh
        self.y_head, self.y_waist, self.y_shoe = self.top + he * h, self.top + te * h, self.top + sh * h
        rows = lambda y0, y1: (self.y >= y0) & (self.y < y1)
        self.torso, self.legs, self.feet = rows(self.y_head, self.y_waist), rows(self.y_waist, self.y_shoe), rows(self.y_shoe, self.H + 1)
        self.headrows = rows(0, self.y_head)
        self.lum = self.rgb @ np.array([0.3, 0.59, 0.11])
        edge = self.a & ~ndimage.binary_erosion(self.a, iterations=max(1, round(scale)))
        self.outline = edge & (self.lum < 70)
        self.skin = is_skin(self.rgb)
        lr = self.a & rows((self.y_waist + self.y_shoe) / 2, self.y_shoe)      # the lower legs: nothing held hangs that low
        lc = np.nonzero(lr.any(0))[0]
        self.c0, self.c1 = (lc.min(), lc.max()) if len(lc) else (xs.min(), xs.max())
        self.cx = (self.c0 + self.c1) / 2
        hm = self.a & self.headrows
        hc = np.nonzero(hm.any(0))[0]
        self.hx0, self.hx1 = (hc.min(), hc.max()) if len(hc) else (xs.min(), xs.max())
        self.hcx = (self.hx0 + self.hx1) / 2


def masks(f, o):
    """The jacket (body and sleeves), trousers and shoes of a frame, as boolean masks."""
    s, y, x = f.s, f.y, f.x
    pad = 2 * s
    reach = REACH.get(f.name, 2.5)
    near_core = (x >= f.c0 - pad * reach) & (x <= f.c1 + pad * reach)
    in_core = (x >= f.c0) & (x <= f.c1)
    he, te = f.he, f.te
    yt = lambda frac: f.top + frac * f.h
    hands = f.skin & f.torso & (y >= yt(he + (te - he) * 0.55)) & ~in_core
    jacket = f.a & f.torso & ~f.outline & near_core & ~hands
    jacket &= ~(f.skin & ~in_core & (y >= yt(he + (te - he) * 0.5)))
    if s >= 2:                                                           # the neck and chin stay
        jacket &= ~(f.skin & (y < yt(he + (te - he) * 0.22)))
    else:
        jacket &= ~(f.skin & (y < f.y_head + 1) & (np.abs(x - f.hcx) < (f.hx1 - f.hx0) * 0.3))
    hair = HAIR.get(f.name)
    if hair:                                                             # long hair over the shoulders stays hair
        head = f.a & (y < yt(he)) & ~f.skin & ~f.outline
        if head.sum() > 5:
            hc = np.median(f.rgb[head], axis=0)
            near = np.sqrt(((f.rgb - hc) ** 2).sum(-1)) < 60
            if hair == 'warm':
                near = (f.rgb[..., 1] - f.rgb[..., 0] < -4) & (f.lum < 90) & ~f.skin
            jacket &= ~(near & (y < yt(he + (te - he) * 0.85)))
    sleeves = jacket & ~((x >= f.c0 - s * 0.5) & (x <= f.c1 + s * 0.5)) if f.view != 2 else np.zeros_like(jacket)
    if o.get('arms') == 'bare':                                          # a vest or a tank top: the arms stay as they are
        jacket &= ~sleeves
        jacket &= ~f.skin | ((x >= f.c0 + s) & (x <= f.c1 - s))
        sleeves = np.zeros_like(jacket)
    body = jacket & ~sleeves
    trousers = f.a & f.legs & ~f.outline & (x >= f.c0 - pad) & (x <= f.c1 + pad)
    if o.get('bare_legs'):                                               # shorts or a skirt: the legs stay bare
        trousers &= ~f.skin
    shoes = f.a & f.feet & ~f.outline
    return body, sleeves, trousers, shoes


def shade(f, mask):
    """The figure's own light and shade over a mask, as 0..1, with a little light from the upper left."""
    v = f.lum[mask]
    if v.size < 3:
        return np.zeros(v.size)
    lo, hi = np.percentile(v, 8), np.percentile(v, 92)
    t = np.clip((v - lo) / max(hi - lo, 1), 0, 1)
    my, mx = np.nonzero(mask)
    side = np.clip(0.5 - (mx - f.cx) / max(1.0, (f.c1 - f.c0 + 4 * f.s)), 0, 1)
    return np.clip(t * 0.62 + side * 0.28 + 0.14, 0, 1)


# ----------------------------------------------------------------------------- patterns
def pattern(f, o, name, mask, base, t, part):
    """Paints a pattern over `base` (colours for the pixels of mask); returns the new colours."""
    if not name or not mask.any():
        return base
    py, px = np.nonzero(mask)
    s = f.s; u = max(1.0, s)
    X = (px - f.cx) / u; Y = (py - f.y_head) / u          # in figure-pixels, from the middle of the chest
    pc = o.get('pat', ['000000', '808080', 'ffffff'])
    sec = ramp(pc, t)
    m = np.zeros(len(px), bool)
    if name == 'hstripes' or (name == 'hstripes_legs' and part == 'legs'):
        m = (np.floor(Y / 2) % 2) == 0
    elif name == 'sidestripe':                            # a tracksuit's stripe down each side
        if f.view == 2:
            m = np.abs(px - (f.cx + (f.c1 - f.c0) * (0.05 if part == 'legs' else 0.1))) < max(1.0, s * 0.7)
        else:
            w = max(1.0, s * 0.6)
            if part == 'sleeves':                         # down the outside of each arm
                m = np.abs(px - np.where(px < f.cx, f.c0 - 1.6 * s, f.c1 + 1.6 * s)) < w
            else:                                         # down each side of the body and legs
                m = (np.abs(px - f.c0 - s * 0.6) < w) | (np.abs(px - f.c1 + s * 0.6) < w)
    elif name == 'pinstripe':
        m = (np.floor(X * 2) % 4 == 0) & (np.floor(X * 2) == X * 2) if s < 2 else (np.abs((X * 1.0) % 2.0 - 1.0) < 0.12)
    elif name == 'plaid':
        k = 3.0
        gx = np.abs(((X + 100) % k) - k / 2) < 0.5
        gy = np.abs(((Y + 100) % k) - k / 2) < 0.5
        base = base.copy()
        base[gx & gy] = ramp(pc, t)[gx & gy]
        m = gx ^ gy
        sec = base * 0.55 + ramp(pc, t) * 0.45
    elif name == 'dots':
        k = 3.0
        dx = ((X + 100) % k) - k / 2; dy = ((Y + 100 + np.floor((X + 100) / k) % 2 * 1.5) % k) - k / 2
        m = dx * dx + dy * dy < (0.55 if s >= 2 else 0.3)
    elif name in ('stars', 'sparkle'):
        cell = 3 if name == 'stars' else 2
        gx, gy = np.floor((X + 100) / cell).astype(int), np.floor((Y + 100) / cell).astype(int)
        hx = hashn(gx, gy) % 1000
        cx = gx * cell - 100 + 0.5 + (hx % 7) / 7 * (cell - 1); cy = gy * cell - 100 + 0.5 + (hx // 7 % 7) / 7 * (cell - 1)
        d = np.maximum(np.abs(X - cx), np.abs(Y - cy)); dd = np.abs(X - cx) + np.abs(Y - cy)
        keep = (hx % 3 == 0) if name == 'stars' else (hx % 3 != 2)
        if s >= 2 and name == 'stars':                    # a little four-pointed star
            m = keep & ((dd < 0.45) | ((np.minimum(np.abs(X - cx), np.abs(Y - cy)) < 0.14) & (d < 0.75)))
        elif s >= 2:                                      # a glint
            m = keep & (d < 0.22)
        else:
            m = keep & (dd < 0.5)
        sec = ramp(pc, np.clip(t + 0.3, 0, 1))
    elif name == 'flames':                                 # flames licking up from the hem
        hem = f.top + f.h * (o.get('robe') or f.sh)
        rise = (hem - py) / u
        wave = 2.5 + 1.6 * np.sin(X * 1.3 + 1) + 0.9 * np.sin(X * 2.9)
        m = rise < wave
        hot = rise < wave * 0.5
        sec = ramp(pc, np.where(hot, 0.95, 0.45) * np.ones(len(px)))
    elif name == 'net':                                    # a cantaloupe's netting
        m = (np.abs(np.sin(X * 1.6 + np.sin(Y * 0.9) * 1.2)) < 0.22) | (np.abs(np.sin(Y * 1.4 + np.sin(X * 0.8))) < 0.18)
    elif name == 'flowers':
        cell = 3.0
        gx, gy = np.floor((X + 100) / cell), np.floor((Y + 100) / cell)
        cx_, cy_ = gx * cell - 100 + 1.5, gy * cell - 100 + 1.5
        d = np.hypot(X - cx_, Y - cy_)
        k = (hashn(gx.astype(int), gy.astype(int)) % 3)
        m = d < (0.9 if s >= 2 else 0.5)
        sec = np.where(d[:, None] < 0.35, C(pc[2]), np.where(k[:, None] == 0, C(pc[0]), C(pc[1])))
    elif name == 'honeycomb':
        q = X / 1.7; r = Y / 1.5 + (np.floor(q) % 2) * 0.5
        m = (np.abs(q - np.round(q)) < 0.16) | (np.abs(r - np.round(r)) < 0.14)
    elif name == 'fur':
        n = hashn(np.floor(X * 2).astype(int), np.floor(Y * 1.2).astype(int)) % 5
        m = n < 2
    elif name == 'plates':                                 # armour plates with a rivet at each end
        r = (Y + 100) % 2.6
        m = r < 0.35
        rv = (np.abs(r - 1.3) < 0.3) & ((np.abs(X - (f.c0 - f.cx) / u - 0.8) < 0.3) | (np.abs(X - (f.c1 - f.cx) / u + 0.8) < 0.3))
        base = base.copy(); base[rv] = ramp(o['top'], np.ones(rv.sum()))
    elif name == 'studs':                                 # along the shoulders and down the front
        dot = (np.abs(((X + 100) % 1.6) - 0.8) < 0.26) & (np.abs(((Y + 100) % 1.6) - 0.8) < 0.26)
        m = dot & ((Y < 1.8) | (np.abs(np.abs(X) - 1.3) < 0.5)) & (part != 'legs')
    elif name == 'koi':
        cell = 4.0
        gx, gy = np.floor((X + 100) / cell), np.floor((Y + 100) / cell)
        cx_, cy_ = gx * cell - 100 + 2, gy * cell - 100 + 2
        k = hashn(gx.astype(int), gy.astype(int)) % 4
        e = ((X - cx_) / 1.3) ** 2 + ((Y - cy_) / 0.7) ** 2
        m = (e < 1) & (k < 3)
        sec = np.where(((X - cx_) > 0.3)[:, None], C(pc[1]), C(pc[0]))
    elif name == 'leaves':
        cell = 2.5
        gx, gy = np.floor((X + 100) / cell), np.floor((Y + 100) / cell)
        k = hashn(gx.astype(int), gy.astype(int)) % 3
        cx_, cy_ = gx * cell - 100 + 1.25, gy * cell - 100 + 1.25
        m = (np.abs(X - cx_) + np.abs(Y - cy_) * 1.6 < 0.9) & (k == 0)
        sec = ramp(pc, np.clip(t + 0.4, 0, 1))
    elif name == 'hearts':
        cell = 3.5
        gx, gy = np.floor((X + 100) / cell), np.floor((Y + 100) / cell)
        cx_, cy_ = gx * cell - 100 + 1.75, gy * cell - 100 + 1.75
        dx, dy = (X - cx_) / 0.9, (Y - cy_) / 0.9
        if s >= 2:
            m = ((dx * dx + dy * dy - 1) ** 3 - dx * dx * (-dy) ** 3) < 0
        else:
            m = (np.abs(dx) < 0.6) & (np.abs(dy) < 0.6)
    elif name == 'tiedye':                                 # a spiral of every colour
        ang = np.arctan2(Y - 3, X) + np.hypot(X, Y - 3) * 0.55
        hue = (ang / (2 * math.pi)) % 1.0
        rgb = np.array([colorsys.hsv_to_rgb(hh, 0.75, 0.95) for hh in hue]) * 255
        return rgb * (0.55 + 0.6 * t[:, None])
    elif name == 'hstripes_legs':
        return base
    if m.any():
        base = base.copy()
        base[m] = sec[m] if sec.ndim == 2 else sec
    return base


# ----------------------------------------------------------------------------- small art
EMBLEMS = {    # 5 x 5, '#' = the colour, '.' = shadow
    'star':  ['..#..', '.###.', '#####', '.###.', '.#.#.'],
    'flame': ['..#..', '.##..', '.###.', '#####', '.###.'],
    'coin':  ['.###.', '##.##', '#.#.#', '##.##', '.###.'],
    'circle': ['.###.', '#...#', '#.#.#', '#...#', '.###.'],
    'mug':   ['####.', '#..##', '#..#.', '####.', '####.'],
    'cross': ['..#..', '..#..', '#####', '..#..', '..#..'],
    'drop':  ['..#..', '.###.', '#####', '#####', '.###.'],
    'claw':  ['#.#.#', '#.#.#', '#.#.#', '.###.', '..#..'],
    'bat':   ['#...#', '##.##', '#####', '.#.#.', '.....'],
    'pumpkin': ['.#.#.', '#####', '#.#.#', '##.##', '.###.'],
    'cpp':   ['.##.........', '#.....#...#.', '#....###.###', '#.....#...#.', '.##.........'],
    'leaf':  ['...##', '..###', '.###.', '###..', '#....'],
    'crownmark': ['#.#.#', '#####', '#####', '.....', '.....'],
}


def paste_bitmap(out, alpha, f, rows, cx, cy, col, unit):
    h, w = len(rows), len(rows[0])
    x0 = int(round(cx - w * unit / 2)); y0 = int(round(cy - h * unit / 2))
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == '#':
                ys, xs = y0 + j * unit, x0 + i * unit
                out[max(0, ys):ys + unit, max(0, xs):xs + unit] = col
                alpha[max(0, ys):ys + unit, max(0, xs):xs + unit] = 255


# ----------------------------------------------------------------------------- dressing
def dress_cell(cell, o, view, scale, name, frame):
    f = Frame(cell, view, scale, name, frame)
    if not f.ok:
        return cell
    out = f.rgb.copy(); alpha = f.alpha.copy()
    s, y, x = f.s, f.y, f.x
    u = max(1, int(round(s)))
    body, sleeves, trousers, shoes = masks(f, o)

    # --- a cape hanging behind (seen past the sides from the front, or behind the back from the side)
    if o.get('cape'):
        cc = o['cape']
        y0, y1 = f.top + f.h * (f.he + 0.04), f.top + f.h * 0.86
        span = np.clip((y - y0) / max(1, y1 - y0), 0, 1)
        if view == 2:
            sway = math.sin(frame * 1.6) * s * 0.8 if frame else 0
            back = f.c0 - s * 0.5
            left = back - s * (1.0 + 3.2 * span) + sway * span
            cape = (y >= y0) & (y <= y1) & (x >= left) & (x <= back + s)
        else:
            half = (f.c1 - f.c0) / 2 + s * (1.4 + 2.2 * span)
            cape = (y >= y0) & (y <= y1) & (np.abs(x - f.cx) <= half)
        behind = cape & ~f.a
        tt = np.clip(0.75 - span * 0.5 + 0.15 * np.sin(x / max(1.0, s) * 1.7), 0, 1)
        if view == 1:                                   # from behind it covers the back
            over = cape & ~(f.headrows & f.a) & ~(f.skin & f.feet)
            out[over] = ramp(cc, tt[over] * 0.8 + shade(f, over) * 0.2)
            alpha[over] = 255
        out[behind] = ramp(cc, tt[behind] * 0.7)
        alpha[behind] = 255

    # --- the clothes, recoloured over the figure's own light and shade
    def paint(mask, cols, part):
        if not mask.any():
            return
        t = shade(f, mask)
        col = ramp(cols, t)
        col = pattern(f, o, o.get('pattern'), mask, col, t, part)
        out[mask] = col

    if o.get('top'):
        paint(body, o['top'], 'body')
        paint(sleeves, o.get('sleeves', o['top']), 'sleeves')
    if o.get('tabard') and view != 2:                     # a tabard down the middle of the chest
        tb = body & (np.abs(x - f.cx) <= (f.c1 - f.c0) * 0.28 + s * 0.5)
        tb |= trousers & (np.abs(x - f.cx) <= (f.c1 - f.c0) * 0.28 + s * 0.5) & (y < f.top + f.h * 0.82)
        if tb.any():
            out[tb] = ramp(o['tabard'], shade(f, tb))
    if o.get('legs'):
        paint(trousers, o['legs'], 'legs')
    if o.get('shoes'):
        sy, sx = np.nonzero(shoes)
        if len(sy):
            s0, s1 = C(o['shoes'][0]), C(o['shoes'][1])
            sole = sy >= sy.max() - max(0, round(s) - 1)
            out[sy, sx] = np.where(sole[:, None], s1, s0)

    # --- a robe or gown over the legs, flaring to a hem that follows the stride
    if o.get('robe'):
        hem = f.top + f.h * o['robe']
        wy = int(round(f.y_waist))
        top_row = f.a[wy] if 0 <= wy < f.H else np.zeros(f.W, bool)
        wx = np.nonzero(top_row & ((x[0] >= f.c0 - 2 * s) & (x[0] <= f.c1 + 2 * s)))[0]
        w0, w1 = (wx.min(), wx.max()) if len(wx) else (f.c0, f.c1)
        hy = int(min(f.H - 1, round(hem)))
        hr = np.nonzero(f.a[hy])[0]
        h0, h1 = (hr.min(), hr.max()) if len(hr) else (w0, w1)
        flare = s * 0.8 if s >= 2 else 0.4
        h0, h1 = min(h0, w0) - flare, max(h1, w1) + flare
        robe = np.zeros_like(f.a)
        for yy in range(max(0, wy), hy + 1):
            k = (yy - wy) / max(1, hy - wy)
            k = k ** 0.8
            l = w0 + (h0 - w0) * k; r = w1 + (h1 - w1) * k
            robe[yy, int(math.floor(l)):int(math.ceil(r)) + 1] = True
        robe &= ~(f.skin & ~f.legs)                       # hands held in front stay
        t = np.clip(0.55 + 0.25 * np.sin((x - f.cx) / max(1.0, s) * 1.1) - 0.25 * ((y - wy) / max(1, hy - wy)), 0, 1)
        t = np.where(f.a, t * 0.55 + shade_map(f, f.a) * 0.45, t)
        cols = o.get('legs', o.get('top'))
        col = ramp(cols, t[robe])
        col = pattern(f, o, o.get('pattern'), robe, col, t[robe], 'robe')
        out[robe] = col
        alpha[robe] = 255
        # the hem: a darker line, and the feet beneath it as they were
        hemline = robe & (y >= hy - max(0, u - 1))
        out[hemline] = ramp(cols, np.zeros(hemline.sum()) + 0.05)

    # --- trim round the clothes' edges, and a belt
    if o.get('trim'):
        cl = (body | sleeves | trousers)
        if o.get('robe'):
            cl = cl | (np.isclose(alpha, 255) & (y >= f.y_waist) & (y <= f.top + f.h * o['robe']))
        inner = ndimage.binary_erosion(cl, iterations=max(1, int(round(s * 0.6))))
        edge = cl & ~inner & (y > f.y_head + s * 0.5) & ~f.skin
        out[edge] = out[edge] * 0.3 + C(o['trim']) * 0.7
    if o.get('belt'):
        band = (np.abs(y - f.y_waist) <= max(0.5, s * 0.55)) & np.isclose(alpha, 255) & ~f.skin & ~f.outline
        band &= (x >= f.c0 - 2 * s) & (x <= f.c1 + 2 * s)
        out[band] = C(o['belt']) * (0.75 + 0.25 * (np.arange(band.sum()) % 3 == 0))[:, None]

    # --- a badge on the chest (front) or between the shoulders (back)
    if o.get('emblem') and view != 2:
        rows = EMBLEMS[o['emblem']]
        unit = max(1, int(round(s * 0.55)))
        cy = f.top + f.h * (f.he + (f.te - f.he) * 0.42)
        if s < 2:                                       # too small for a picture: a single bright stud
            rows, unit = ['#'], 1
        paste_bitmap(out, alpha, f, rows, f.cx + (0 if view == 1 else 0), cy, C(o['emc']), unit)

    # --- at the neck
    neck = o.get('neck')
    if neck and view == 0:
        cy = int(round(f.y_head + s * 0.3)); cx = f.cx
        nc = C(o.get('neckc', 'ffd646'))
        if neck in ('chain', 'beads'):
            gold = C('ffd646') if neck == 'chain' else nc
            n = max(2, round(2.5 * s))
            for k in range(n):
                for sgn in (-1, 1):
                    px = int(round(cx + sgn * (n - k) * 0.8)); py = cy + k
                    if 0 <= py < f.H and 0 <= px < f.W and f.a[py, px] and (neck == 'chain' or k % 2 == 0):
                        out[py, px] = gold if neck == 'chain' else nc
            if neck == 'beads' and s >= 2:
                out[cy + n:cy + n + u, int(cx) - u // 2:int(cx) + u // 2 + 1] = nc
        elif neck == 'bowtie':
            w = max(1, round(s * 1.1))
            for dx in range(-w, w + 1):
                hh = abs(dx) * 0.6 + 0.5
                for dy in range(-int(hh * max(1, s * 0.6)), int(hh * max(1, s * 0.6)) + 1):
                    px, py = int(round(cx + dx * max(1, s * 0.6))), cy + dy
                    if 0 <= py < f.H and 0 <= px < f.W:
                        out[py, px] = nc
        elif neck == 'tie':
            for k in range(int((f.y_waist - cy) * 0.75)):
                w = max(0.5, s * 0.45) * (1.3 if k > (f.y_waist - cy) * 0.55 else 1.0)
                xs_ = np.arange(int(cx - w), int(cx + w) + 1)
                out[cy + k, xs_] = nc * (0.85 if k % max(2, u * 2) == 0 else 1.0)

    # --- on the face
    face = o.get('face')
    if face and view != 1:
        ey = int(round(f.top + f.h * f.he * 0.58))
        th = max(1, round(s * 0.7))
        col = C('14141a') if face == 'shades' else C('e8407a')
        if view == 0:
            x0, x1 = int(f.hx0 + (f.hx1 - f.hx0) * 0.14), int(f.hx1 - (f.hx1 - f.hx0) * 0.14)
            out[ey:ey + th, x0:x1 + 1] = col
            if s >= 2:                                   # two lenses, a bridge between
                out[ey + th:ey + th * 2, x0:int(f.hcx - s * 0.4)] = col
                out[ey + th:ey + th * 2, int(f.hcx + s * 0.4):x1 + 1] = col
                out[ey:ey + max(1, th // 2), x0:x1 + 1] = col * 0.6 + 255 * 0.4 if face == 'shades' else C('ff9ac0')
        else:
            x1 = int(f.hx1 - s * 0.2); x0 = int(x1 - (f.hx1 - f.hx0) * 0.35)
            out[ey:ey + th * (2 if s >= 2 else 1), x0:x1 + 1] = col

    # --- on the head
    hat = o.get('hat')
    if hat:
        hc = o.get('hatc', ['14141a', '2a2a34', 'f4f4f8'])
        c0, c1, c2 = C(hc[0]), C(hc[1]), C(hc[2])
        head = f.a & (y < f.y_head)
        headH = f.y_head - f.top
        brim = f.top + headH * CAP.get(name, 0.17) / max(0.01, f.he) * 1.0
        if hat in ('cap', 'beanie', 'bucket', 'fedora', 'helm'):
            line = f.top + (f.h * CAP.get(name, 0.17)) * (1.3 if hat in ('beanie', 'helm') else 1.0)
            crown = head & (y < line) & ~f.outline
            tt = np.clip(0.35 + 0.5 * (f.lum / 255.0), 0, 1)
            out[crown] = ramp([c0, c1, c1 * 0.6 + c2 * 0.4 if hat != 'helm' else c2], tt[crown])
            by = int(round(line))
            row = np.nonzero(f.a[min(f.H - 1, by)])[0]
            if len(row):
                x0, x1 = row.min(), row.max()
                bh = max(1, round(s * (0.7 if hat != 'beanie' else 0.9)))
                if hat == 'beanie':                       # a turned-up rim, and a bobble on top
                    out[by - bh:by + 1, x0:x1 + 1] = c2 if np.any(c2 != c0) else c1 * 0.8
                    out[by - bh:by + 1:max(1, u), x0:x1 + 1] *= 0.9
                    if s >= 2:
                        bx = int(f.hcx); r = max(1, round(s * 0.7))
                        yy, xx = np.ogrid[:f.H, :f.W]
                        bob = (yy - (f.top + r * 0.4)) ** 2 + (xx - bx) ** 2 <= r * r
                        out[bob] = c2; alpha[bob] = 255
                elif hat == 'helm':
                    out[by:by + bh, x0:x1 + 1] = c0
                elif hat in ('fedora', 'bucket'):
                    ext = int(round(s * (1.6 if hat == 'fedora' else 1.0)))
                    out[by:by + bh, max(0, x0 - ext):x1 + ext + 1] = c0
                    alpha[by:by + bh, max(0, x0 - ext):x1 + ext + 1] = 255
                    if hat == 'fedora':                   # a band round the crown
                        bb = head & (y >= by - bh * 2) & (y < by) & ~f.outline
                        out[bb] = c2
                    if hat == 'bucket':
                        out[by + bh:by + bh + max(1, u // 2), max(0, x0 - ext):x0 + 1] = c0 * 0.7
                        out[by + bh:by + bh + max(1, u // 2), x1:x1 + ext + 1] = c0 * 0.7
                else:                                     # a cap: the brim, forward from the side
                    if view == 0:
                        out[by:by + bh, x0:x1 + 1] = c2
                        out[by + bh:by + bh + max(1, u // 2), x0 + u:x1 - u + 1] = c0 * 0.6 + out[by + bh:by + bh + max(1, u // 2), x0 + u:x1 - u + 1] * 0.4
                    elif view == 2:
                        ext = int(round(3 * s))
                        mid = (x0 + x1) // 2
                        out[by:by + bh, mid:min(f.W, x1 + ext + 1)] = c2
                        alpha[by:by + bh, mid:min(f.W, x1 + ext + 1)] = 255
                    else:
                        out[by:by + bh, x0:x1 + 1] = c0 * 0.8
                        if s >= 2:                        # the strap at the back
                            out[by - bh:by, int(f.hcx - s):int(f.hcx + s)] = c2
        elif hat == 'headband':
            by = int(round(f.top + headH * 0.30))
            bh = max(1, round(s * 0.6))
            band = head & (y >= by) & (y < by + bh) & ~f.outline
            out[band] = c1
            if s >= 2 and view in (1, 2):                 # the knot's tails
                tx = int(f.hcx) if view == 1 else int(f.hx0)
                for k in range(int(s * 2.2)):
                    px = tx - (k // 2 if view == 2 else 0) + (int(math.sin(k * 0.4 + frame) * s * 0.4) if view == 1 else 0)
                    py = by + bh + k
                    if 0 <= px < f.W and 0 <= py < f.H:
                        out[py, max(0, px - u // 2):px + u // 2 + 1] = c0
                        alpha[py, max(0, px - u // 2):px + u // 2 + 1] = 255
        elif hat == 'crown' and s >= 2:
            cg = [C('8a5a10'), C('e8b82a'), C('fff0a0')] if 'hatc' not in o else [c0, c1, c2]
            hw = (f.hx1 - f.hx0) * (0.34 if view != 2 else 0.28)
            cxh = f.hcx + (s * 0.3 if view == 2 else 0)
            base = f.top + headH * 0.16                   # sits on the hair
            band = max(2, round(s * 0.9)); tall = s * 2.2
            yy, xx = np.ogrid[:f.H, :f.W]
            dxn = (xx - cxh) / hw
            inband = (np.abs(dxn) <= 1) & (yy <= base) & (yy > base - band)
            pts = np.zeros((f.H, f.W), bool)
            for px_, scale_ in ((-0.8, 0.8), (0.0, 1.0), (0.8, 0.8)):
                hgt = tall * scale_
                pts |= (yy <= base - band + 1) & (yy >= base - band - hgt + np.abs(dxn - px_) * hw * 2.2) & (np.abs(dxn - px_) < 0.45)
            crown = inband | pts
            crown &= yy >= 0
            out[crown] = cg[1]
            lit = crown & (xx < cxh - s * 0.4)
            out[lit] = cg[2]
            out[inband & (yy == int(base))] = cg[0]
            alpha[crown] = 255
            jy, jx = int(base - band / 2), int(cxh)
            out[jy - u // 2:jy + u // 2 + 1, jx - u // 2:jx + u // 2 + 1] = C('d8203a')
            for sx in (-0.6, 0.6):
                gx = int(cxh + sx * hw)
                out[jy:jy + 1, gx - u // 3:gx + u // 3 + 1] = C('3a8ae0')
        elif hat == 'crown':
            cg = [C('8a5a10'), C('e8b82a'), C('fff0a0')] if 'hatc' not in o else [c0, c1, c2]
            top = f.top
            ch = max(2, round(headH * 0.28))
            lift = min(int(round(s * 1.6)), max(0, int(f.top) - 1)) if s >= 2 else 0
            y0 = top - lift
            x0, x1 = int(f.hcx - (f.hx1 - f.hx0) * 0.30), int(f.hcx + (f.hx1 - f.hx0) * 0.30)
            for xx in range(x0, x1 + 1):
                k = (xx - x0) / max(1, x1 - x0)
                spike = (abs(math.sin(k * math.pi * 2.0)) > 0.6) if s >= 2 else ((xx - x0) % 2 == 0)
                ytop = y0 if spike else y0 + max(1, ch // 2)
                for yy in range(int(ytop), int(top + ch)):
                    if 0 <= yy < f.H:
                        out[yy, xx] = cg[2] if xx < f.hcx - s * 0.5 and yy < top + ch * 0.6 else cg[1]
                        alpha[yy, xx] = 255
                if 0 <= int(top + ch) < f.H:
                    out[int(top + ch), xx] = cg[0]
            if s >= 2:                                    # a jewel
                jy = int(top + ch * 0.55); jx = int(f.hcx)
                out[jy - u // 2:jy + u // 2 + 1, jx - u // 2:jx + u // 2 + 1] = C('d8203a')
        elif hat == 'flowers':
            by = int(round(f.top + headH * 0.18))
            xs_ = np.nonzero(head[min(f.H - 1, by)])[0]
            if len(xs_):
                step = max(2, int(round(s * 1.6)))
                for i, xx in enumerate(range(xs_.min() + 1, xs_.max(), step)):
                    col = C(hc[i % 2])
                    r = max(0, int(round(s * 0.5)))
                    out[by - r:by + r + 1, xx - r:xx + r + 1] = col
                    alpha[by - r:by + r + 1, xx - r:xx + r + 1] = 255
                    if s >= 2:
                        out[by, xx] = C(hc[2])
        elif hat == 'headphones':
            by = f.top + max(1, round(s * 0.4))
            band = head & (np.abs(y - by) <= max(0.5, s * 0.4)) & ~f.outline
            out[band] = c0
            if view != 1:
                ey = int(f.top + headH * 0.55)
                r = max(1, round(s * 1.0))
                for ex in ([f.hx0, f.hx1] if view == 0 else [f.hcx - s * 0.6]):
                    ex = int(ex)
                    out[ey - r:ey + r + 1, max(0, ex - r // 2):ex + r // 2 + 1] = c1
                    alpha[ey - r:ey + r + 1, max(0, ex - r // 2):ex + r // 2 + 1] = 255
                    if s >= 2:
                        out[ey, max(0, ex - r // 2):ex + r // 2 + 1] = c2

    # --- outline what's new past the figure's old edge
    newpx = (alpha > 100) & ~f.a
    if newpx.any():
        solid = alpha > 100
        ring = ndimage.binary_dilation(solid) & ~solid
        k = max(1, int(round(s * 0.5)))
        ringk = ndimage.binary_dilation(solid, iterations=k) & ~solid
        near_new = ndimage.binary_dilation(newpx, iterations=k + 1)
        ol = ringk & near_new
        out[ol] = C('1a1420'); alpha[ol] = 255

    # --- a glow round the whole figure
    if o.get('glow'):
        solid = alpha > 100
        gc = C(o['glow'])
        rings = [(1, 190), (2, 110), (3, 50)] if s >= 2 else [(1, 150)]
        prev = solid
        step = max(1, int(round(s * 0.45)))
        for i, (k, aa) in enumerate(rings):
            grown = ndimage.binary_dilation(solid, iterations=k * step)
            ring = grown & ~prev
            pulse = 1.0 if frame % 2 == 0 else 0.85
            out[ring] = gc; alpha[ring] = np.maximum(alpha[ring], aa * pulse)
            prev = grown

    res = cell.copy()
    res[..., :3] = np.clip(out, 0, 255).astype(np.uint8)
    res[..., 3] = np.clip(alpha, 0, 255).astype(np.uint8)
    return res


def shade_map(f, mask):
    """shade() as a full-size map (0 off the mask)."""
    t = np.zeros(f.a.shape)
    if mask.any():
        t[mask] = shade(f, mask)
    return t


def dress(src, o):
    im = np.array(Image.open(src).convert('RGBA'))
    fh = im.shape[0] // 3; fw = fh * 3 // 4
    frames = im.shape[1] // fw
    scale = fh / 32.0
    name = os.path.basename(src)[5:-4]
    out = im.copy()
    for r in range(3):
        for k in range(frames):
            out[r * fh:(r + 1) * fh, k * fw:(k + 1) * fw] = dress_cell(im[r * fh:(r + 1) * fh, k * fw:(k + 1) * fw].copy(), o, r, scale, name, k)
    return Image.fromarray(out, 'RGBA')


if __name__ == '__main__':
    walk_dir, out_dir = sys.argv[1], sys.argv[2]
    only = sys.argv[3:] or list(OUTFITS)
    for champ in only:
        src = os.path.join(walk_dir, 'walk-%s.png' % champ)
        os.makedirs(os.path.join(out_dir, champ), exist_ok=True)
        for key, o in OUTFITS[champ].items():
            dress(src, o).save(os.path.join(out_dir, champ, key + '.png'), optimize=True)
            print('wrote', os.path.join(out_dir, champ, key + '.png'))
