# Makes every champion's walking figure in each of Beray's outfits:
#   assets/outfits/<outfit>/walk-<champion>.png
# The head stays as it is; the body is redressed: a jacket over the torso and arms (hands
# stay bare), trousers over the legs, sneakers on the feet, and for some outfits a cap and
# a gold chain. Shading comes from the figure's own light and shade.
#
# Run from Chronicles-of-the-Spice-Trip/ after changing a walk sheet or an outfit:
#   python3 tools/outfits.py assets/walk assets/outfits      (needs numpy, scipy, pillow)
import colorsys, glob, os, sys
import numpy as np
from PIL import Image
from scipy import ndimage

def C(h): return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)

# name: jacket ramp (dark, mid, light), second jacket ramp (drips/stripes/camo), trousers ramp, sneakers, extras
OUTFITS = {
    'drip':     dict(top=['3a1f6b', '7b3fc4', 'b07ef0'], top2=['13706a', '22c2b4', '8ff5e8'], pattern='drip',
                     legs=['141418', '26262e', '44444f'], shoes=['1fae9f', 'eaf6f4'], cap=['22c2b4', '7b3fc4'], chain=True),
    'midnight': dict(top=['0c0c12', '1c1c26', '343444'], top2=['c8c8d4', 'f0f0f6', 'ffffff'], pattern='stripe',
                     legs=['0c0c12', '1c1c26', '343444'], shoes=['f0f0f6', '1c1c26'], cap=None, chain=True),
    'gold':     dict(top=['8a5a10', 'd9a520', 'ffe27a'], top2=['b9b2a0', 'f2ecdc', 'ffffff'], pattern='stripe',
                     legs=['b9b2a0', 'ece5d2', 'ffffff'], shoes=['d9a520', 'fff3c4'], cap=None, chain=True),
    'camo':     dict(top=['3b4228', '5e6a3c', '8a925a'], top2=['2a2418', '6e5a3a', 'a8956a'], pattern='camo',
                     legs=['3b4228', '5e6a3c', '8a925a'], shoes=['3a2a1c', '5c4630'], cap=['5e6a3c', '3b4228'], chain=False),
    'pink':     dict(top=['8c1d5a', 'e0459a', 'ff9fd0'], top2=['ffffff', 'ffffff', 'ffffff'], pattern='plain',
                     legs=['8c1d5a', 'e0459a', 'ff9fd0'], shoes=['ffffff', 'e0459a'], cap=None, chain=False),
    'ice':      dict(top=['7fa6c4', 'd2ecfa', 'ffffff'], top2=['2f7fbf', '5fb7ef', 'b9e6ff'], pattern='drip',
                     legs=['5c7a94', '9fc0d8', 'dceefa'], shoes=['ffffff', '5fb7ef'], cap=['ffffff', '5fb7ef'], chain=True),
}

def ramp(cols, t):
    cols = [C(c) for c in cols]
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

def dress_cell(cell, o, view, scale):
    """cell: HxWx4 uint8 (one frame). view: 0 front, 1 back, 2 side (facing right)."""
    rgb = cell[..., :3].astype(float); a = cell[..., 3] > 100
    if not a.any(): return cell
    ys, xs = np.nonzero(a)
    top, bot = ys.min(), ys.max()
    h = bot - top + 1
    y = np.arange(cell.shape[0])[:, None]; x = np.arange(cell.shape[1])[None, :]
    rows = lambda f0, f1: (y >= top + f0 * h) & (y < top + f1 * h)
    head_end, torso_end, shoe = o['_z']
    torso, legs, feet = rows(head_end, torso_end), rows(torso_end, shoe), rows(shoe, 1.01)
    lum = rgb @ np.array([0.3, 0.59, 0.11])
    edge = a & ~ndimage.binary_erosion(a, iterations=max(1, round(scale)))
    outline = edge & (lum < 70)
    skin = is_skin(rgb)
    # the body's core: how wide the legs are, so things held out to the side keep their colour
    lr = a & legs
    lc = np.nonzero(lr.any(0))[0]
    core0, core1 = (lc.min(), lc.max()) if len(lc) else (xs.min(), xs.max())
    pad = 2 * scale
    near_core = (x >= core0 - pad * 2.5) & (x <= core1 + pad * 2.5)
    in_core = (x >= core0) & (x <= core1)
    # hands: skin low in the torso, outside the core
    hands = skin & torso & (y >= top + (head_end + (torso_end - head_end) * 0.55) * h) & ~in_core
    jacket = a & torso & ~outline & near_core & ~hands & (~skin | in_core | (y < top + (head_end + 0.12) * h) | True)
    jacket &= ~(skin & ~in_core & (y >= top + (head_end + (torso_end - head_end) * 0.5) * h))
    jacket &= ~(skin & (y < top + (head_end + (torso_end - head_end) * 0.22) * h))   # the neck and chin stay
    if o.get('_hair'):                      # long hair over the shoulders stays hair
        head = a & (y < top + head_end * h) & ~skin & ~outline
        if head.sum() > 5:
            hc = np.median(rgb[head], axis=0)
            near = np.sqrt(((rgb - hc) ** 2).sum(-1)) < 60
            jacket &= ~(near & (y < top + (head_end + (torso_end - head_end) * 0.85) * h))
    trousers = a & legs & ~outline & (x >= core0 - pad) & (x <= core1 + pad)
    shoes = a & feet & ~outline
    out = rgb.copy()
    def shade(mask):
        v = lum[mask]
        if v.size < 3: return np.zeros(0)
        lo, hi = np.percentile(v, 8), np.percentile(v, 92)
        return np.clip((v - lo) / max(hi - lo, 1), 0, 1) * 0.8 + 0.12
    # jacket, with its pattern
    t = shade(jacket)
    col = ramp(o['top'], t)
    jy, jx = np.nonzero(jacket)
    if o['pattern'] == 'drip':              # the second colour from the waist up in drips
        t0, t1 = top + head_end * h, top + torso_end * h
        drip = t0 + (t1 - t0) * (0.55 - 0.18 * ((jx // max(1, round(scale)) * 7 % 5) < 2))
        m = jy >= drip
        col[m] = ramp(o['top2'], t[m])
    elif o['pattern'] == 'stripe':          # a stripe down each side
        side = (jx <= core0 - pad * 0.5 + scale) | (jx >= core1 + pad * 0.5 - scale) if view != 2 else (np.abs(jx - (core0 + core1) / 2) < scale)
        col[side] = ramp(o['top2'], t[side])
    elif o['pattern'] == 'camo':
        k = max(1, round(scale * 1.5))
        n = ((jx // k) * 73856093 ^ (jy // k) * 19349663) % 7
        m = n < 3
        col[m] = ramp(o['top2'], t[m])
    out[jacket] = col
    t = shade(trousers)
    col = ramp(o['legs'], t)
    ty, tx = np.nonzero(trousers)
    if o['pattern'] == 'stripe':
        cen = (core0 + core1) / 2
        side = (np.abs(tx - core0) < scale) | (np.abs(tx - core1) < scale) if view != 2 else (np.abs(tx - cen) < scale)
        col[side] = ramp(o['top2'], t[side])
    elif o['pattern'] == 'camo':
        k = max(1, round(scale * 1.5))
        n = ((tx // k) * 73856093 ^ (ty // k) * 19349663) % 7
        col[n < 3] = ramp(o['top2'], t[n < 3])
    out[trousers] = col
    sy, sx = np.nonzero(shoes)
    if len(sy):
        s0 = C(o['shoes'][0]); s1 = C(o['shoes'][1])
        sole = sy >= sy.max() - max(0, round(scale) - 1)
        out[sy, sx] = np.where(sole[:, None], s1, s0)
    # cap: over the top of the head, and a brim
    if o['cap']:
        c0, c1 = C(o['cap'][0]), C(o['cap'][1])
        cap_end = top + 0.17 * h
        hm = a & (y < cap_end) & ~outline
        out[hm] = c0 * (0.75 + 0.35 * (lum[hm] / 255.0))[:, None]
        by = int(round(cap_end))
        row = np.nonzero(a[by])[0]
        if len(row):
            x0, x1 = row.min(), row.max()
            bh = max(1, round(scale))
            if view == 0:
                out[by:by + bh, x0:x1 + 1] = c1; a2 = a.copy()
            elif view == 2:
                ext = int(round(3 * scale))
                out[by:by + bh, (x0 + x1) // 2:min(cell.shape[1], x1 + ext + 1)] = c1
                cell[by:by + bh, (x0 + x1) // 2:min(cell.shape[1], x1 + ext + 1), 3] = 255
                cell[by + bh:by + bh + 1, x1 + 1:min(cell.shape[1], x1 + ext + 1), :3] = 20
                cell[by + bh:by + bh + 1, x1 + 1:min(cell.shape[1], x1 + ext + 1), 3] = 255
            else:
                out[by:by + bh, x0:x1 + 1] = c0 * 0.8
    # chain: a gold V at the neck, from the front
    if o['chain'] and view == 0:
        cy = int(round(top + (head_end + 0.02) * h)); cx = (core0 + core1) / 2
        gold = np.array([255, 214, 70.0])
        n = max(2, round(2.5 * scale))
        for k in range(n):
            for sgn in (-1, 1):
                px = int(round(cx + sgn * (n - k) * 0.8)); py = cy + k
                if 0 <= py < cell.shape[0] and 0 <= px < cell.shape[1] and a[py, px]:
                    out[py, px] = gold
        py = cy + n
        px = int(round(cx))
        if py < cell.shape[0] and a[py, px]: out[py, px] = gold * 0.85
    cell = cell.copy()
    cell[..., :3] = np.clip(out, 0, 255).astype(np.uint8)
    return cell

# how far down the figure the head and the torso end, and where the shoes start (fractions of its height)
ZONES = {'default': (0.40, 0.68, 0.90), 'momiche': (0.33, 0.62, 0.92), 'gymrat': (0.27, 0.58, 0.93),
         'storyteller': (0.42, 0.72, 0.90), 'fairy': (0.40, 0.70, 0.90)}

def dress(src, outfit):
    im = np.array(Image.open(src).convert('RGBA'))
    fh = im.shape[0] // 3; fw = fh * 3 // 4
    frames = im.shape[1] // fw
    scale = fh / 32.0
    name = os.path.basename(src)[5:-4]
    o = dict(OUTFITS[outfit]); o['_z'] = ZONES.get(name, ZONES['default']); o['_hair'] = name in ('momiche',)
    out = im.copy()
    for r in range(3):
        for f in range(frames):
            out[r * fh:(r + 1) * fh, f * fw:(f + 1) * fw] = dress_cell(im[r * fh:(r + 1) * fh, f * fw:(f + 1) * fw], o, r, scale)
    return Image.fromarray(out, 'RGBA')

if __name__ == '__main__':
    walk_dir, out_dir = sys.argv[1], sys.argv[2]
    for outfit in OUTFITS:
        os.makedirs(os.path.join(out_dir, outfit), exist_ok=True)
        for src in sorted(glob.glob(os.path.join(walk_dir, 'walk-*.png'))):
            n = os.path.basename(src)
            if n in ('walk-triceps.png', 'walk-vlado.png', 'walk-mordekai.png', 'walk-goodplazmodium.png',
                     'walk-badplasmodium.png', 'walk-sergeant.png'):
                continue
            dress(src, outfit).save(os.path.join(out_dir, outfit, n), optimize=True)
