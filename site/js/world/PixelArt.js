/**
 * PixelArt.js
 * ----------------------------------------------------------------------------
 * Generates every sprite in the game as pixel art on offscreen <canvas>
 * elements — there are no image files. It covers:
 *   - terrain tiles (dithered ground + brick/rock walls) per region tileset,
 *   - gothic props (dead trees, broken pillars, statues, braziers, graves…),
 *   - animated characters (the hero, NPCs) and monsters, drawn from primitives
 *     with 2–3 frame walk/idle/flap cycles.
 *
 * Everything is cached so a sprite is only rasterized once. The art targets a
 * dark, low-palette 90s-RPG look: 16px tiles, chunky readable silhouettes.
 * ----------------------------------------------------------------------------
 */

import { makeRNG } from '../core/Utils.js';

export const TS = 16;   // tile size in pixels

// --- low-level canvas helpers --------------------------------------------
function mk(w, h) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  const ctx = c.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  return { c, ctx };
}
// Filled rect with integer rounding (keeps everything on the pixel grid).
function R(ctx, x, y, w, h, col) { ctx.fillStyle = col; ctx.fillRect(x | 0, y | 0, Math.max(1, w | 0), Math.max(1, h | 0)); }
function px(ctx, x, y, col) { ctx.fillStyle = col; ctx.fillRect(x | 0, y | 0, 1, 1); }

/** Build a sprite from an array of equal-length strings + a char→color map. */
function fromGrid(rows, pal) {
  const h = rows.length, w = rows[0].length;
  const { c, ctx } = mk(w, h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const ch = rows[y][x];
      const col = pal[ch];
      if (col) px(ctx, x, y, col);
    }
  }
  return c;
}

// =========================================================================
// TILE PALETTES — a few shades per region, gothic and desaturated.
// =========================================================================
const TILE_THEMES = {
  forest:   { floor: ['#1a2a1e', '#223526', '#16231a'], path: ['#2e2a1e', '#3a3424'], wall: ['#243a28', '#16241a'], style: 'grass' },
  temple:   { floor: ['#1b2730', '#23323d', '#16212a'], path: ['#2c3b46', '#374b58'], wall: ['#33424e', '#1d2a32'], style: 'marble' },
  ruins:    { floor: ['#241f28', '#2f2834', '#1d1922'], path: ['#352c3a', '#43374a'], wall: ['#3a3142', '#241d2a'], style: 'cobble' },
  cave:     { floor: ['#16121c', '#1f1828', '#120e18'], path: ['#241a2e', '#2e2238'], wall: ['#2a2036', '#160f20'], style: 'cave' },
  citadel:  { floor: ['#1d1e26', '#272934', '#17181f'], path: ['#2f313e', '#3a3d4c'], wall: ['#3a3d4c', '#222430'], style: 'marble' },
  godreach: { floor: ['#1f2536', '#2a3450', '#191e2c'], path: ['#33406a', '#3f4f80'], wall: ['#3a4870', '#222a40'], style: 'marble' },
};

const tileCache = new Map();

function ditherGround(shades, style, seed) {
  const { c, ctx } = mk(TS, TS);
  const rng = makeRNG(seed);
  R(ctx, 0, 0, TS, TS, shades[0]);
  // base speckle
  for (let i = 0; i < 70; i++) {
    const x = (rng() * TS) | 0, y = (rng() * TS) | 0;
    px(ctx, x, y, shades[(rng() * shades.length) | 0]);
  }
  if (style === 'grass') {
    for (let i = 0; i < 10; i++) {
      const x = (rng() * TS) | 0, y = (rng() * (TS - 2)) | 0;
      px(ctx, x, y, shades[1]); px(ctx, x, y + 1, shades[1]);
      if (rng() < 0.15) px(ctx, x, y - 1, '#6a8a5a'); // a blade tip
    }
    if (rng() < 0.2) px(ctx, (rng() * TS) | 0, (rng() * TS) | 0, '#b9c79a'); // pale bloom
  } else if (style === 'marble') {
    // soft diagonal veins
    for (let i = 0; i < 3; i++) {
      let x = (rng() * TS) | 0, y = 0;
      while (y < TS) { px(ctx, x, y, shades[1]); x = (x + (rng() < 0.5 ? 1 : 0)) % TS; y++; }
    }
  } else if (style === 'cobble') {
    for (let gy = 0; gy < TS; gy += 5) for (let gx = 0; gx < TS; gx += 5) {
      const ox = gx + ((rng() * 2) | 0), oy = gy + ((rng() * 2) | 0);
      R(ctx, ox, oy, 3, 3, shades[1]);
      px(ctx, ox, oy, shades[2] || shades[0]);          // tiny highlight
      px(ctx, ox + 2, oy + 2, shades[2] || shades[0]);   // tiny shadow
    }
  } else if (style === 'cave') {
    for (let i = 0; i < 18; i++) px(ctx, (rng() * TS) | 0, (rng() * TS) | 0, shades[2] || shades[1]);
    if (rng() < 0.3) { const y = (rng() * TS) | 0; for (let x = 2; x < 12; x++) if (rng() < 0.6) px(ctx, x, y, '#0c0810'); } // crack
  }
  return c;
}

function brickWall(shades, seed) {
  const { c, ctx } = mk(TS, TS);
  const rng = makeRNG(seed + 51);
  R(ctx, 0, 0, TS, TS, shades[1]);
  const mortar = shades[1].length ? '#0e0c12' : '#0e0c12';
  for (let row = 0; row < 4; row++) {
    const y = row * 4;
    const offset = (row % 2) * 4;
    for (let bx = -offset; bx < TS; bx += 8) {
      R(ctx, bx, y, 7, 3, shades[0]);
      // top highlight + bottom shade
      R(ctx, bx, y, 7, 1, shadeLighten(shades[0], 12));
      R(ctx, bx, y + 2, 7, 1, '#00000033');
      if (rng() < 0.3) px(ctx, bx + 2 + ((rng() * 3) | 0), y + 1, mortar); // chip
    }
    R(ctx, 0, y + 3, TS, 1, mortar); // mortar line
  }
  return c;
}

function shadeLighten(hex, amt) {
  const n = parseInt(hex.slice(1), 16);
  let r = (n >> 16) + amt, g = ((n >> 8) & 255) + amt, b = (n & 255) + amt;
  r = Math.min(255, r); g = Math.min(255, g); b = Math.min(255, b);
  return `rgb(${r},${g},${b})`;
}

function liquidTile(top, seed) {
  const { c, ctx } = mk(TS, TS);
  const rng = makeRNG(seed + 13);
  R(ctx, 0, 0, TS, TS, '#0a1822');
  R(ctx, 0, 0, TS, TS, top + '');
  ctx.fillStyle = '#0a1822';
  for (let i = 0; i < 40; i++) px(ctx, (rng() * TS) | 0, (rng() * TS) | 0, '#0a1822');
  for (let y = 1; y < TS; y += 4) for (let x = 0; x < TS; x += 1) if (rng() < 0.3) px(ctx, x, y, '#3a6a7a');
  return c;
}

/** Get (and cache) a tile canvas for a tileset + semantic kind + variant seed. */
export function getTile(tileset, kind, variant = 0) {
  const key = `${tileset}:${kind}:${variant}`;
  if (tileCache.has(key)) return tileCache.get(key);
  const t = TILE_THEMES[tileset] || TILE_THEMES.forest;
  let canvas;
  const seed = hashStr(key);
  if (kind === 'wall') canvas = brickWall(t.wall, seed);
  else if (kind === 'water') canvas = liquidTile(t.floor[0], seed);
  else if (kind === 'path') canvas = ditherGround(t.path.concat(t.floor[0]), t.style === 'grass' ? 'dirt' : t.style, seed);
  else canvas = ditherGround(t.floor, t.style, seed + variant * 7);
  tileCache.set(key, canvas);
  return canvas;
}

// =========================================================================
// PROPS — hand-authored gothic objects. ' ' / '.' are transparent.
// =========================================================================
const PROP_GRIDS = {
  deadTree: {
    pal: { k: '#0d0a0c', t: '#241a18', h: '#2f2422', b: '#161012' },
    rows: [
      '......k.k.......',
      '...k..khk...k...',
      '....k.hh.kk.k...',
      '.....khh.hk.....',
      '...k..thh.k.....',
      '....k.thtk......',
      '......tht.......',
      '.....ththk......',
      '......tht.......',
      '......tht.......',
      '.....bthtb......',
      '......tht.......',
      '......btb.......',
      '.....bbtbb......',
      '....bbbtbbb.....',
      '...kbbbbbbbk....',
    ],
  },
  pine: {
    pal: { d: '#16241a', g: '#1f3324', l: '#2a4530', t: '#241a12', s: '#0c0a0c' },
    rows: [
      '.......g.......',
      '......ggg......',
      '......glg......',
      '.....ggggg.....',
      '.....gglgg.....',
      '....ggdgggg....',
      '....gglgggg....',
      '...gggdggggg...',
      '...ggglggggg...',
      '..gggdgggdgg...',
      '..gggglgggggg..',
      '.ggdggggggdgg..',
      '.ggggglggggggg.',
      '......ttt......',
      '......ttt......',
      '.....stttss....',
    ],
  },
  pillar: {
    pal: { s: '#3a3a44', l: '#50505c', d: '#22222a', k: '#101016' },
    rows: [
      '..llssssddk....',
      '..llssssddk....',
      '...lsssdd......',
      '...lsssdd......',
      '...lsssdd......',
      '...lsssdd......',
      '...lsssdd......',
      '...lsssdd......',
      '...lsssdd......',
      '..llsssddk.....',
      '..lksssddk.....',
      '.k.lsskd..k....',
      '...lss.........',
      '..klsdk........',
      '.kkssddk.......',
      'kkdddddkk......',
    ],
  },
  statue: {
    pal: { s: '#42434e', l: '#585a66', d: '#26262e', k: '#101016', e: '#9fd0ff' },
    rows: [
      '.....kssk......',
      '.....lssd......',
      '.....lesd......',  // glowing eyes line
      '.....lssd......',
      '....klssdk.....',
      '...kllssddk....',
      '...ll.ss.dd....',
      '...l..ss..d....',
      '....klssdk.....',
      '.....lssd......',
      '.....lssd......',
      '....llssdd.....',
      '....lsssdd.....',
      '...kllssddk....',
      '..kssssssddk...',
      '.kkddddddddkk..',
    ],
  },
  brazier: {
    pal: { m: '#3a2e22', d: '#22190f', k: '#100b08', e: '#1a1410' },
    rows: [
      '...............',
      '...............',
      '...............',
      '....eeeeee.....',
      '...emmmmmme....',
      '...dmmmmmmd....',
      '....dmmmmd.....',
      '.....dmmd......',
      '......mm.......',
      '......mm.......',
      '.....dmmd......',
      '....mmmmmm.....',
      '...kmmmmmmk....',
      '....k.mm.k.....',
      '......mm.......',
      '....kkmmkk.....',
    ],
  },
  grave: {
    pal: { s: '#3a3a42', l: '#4e4e58', d: '#24242a', k: '#101015', g: '#1c2a1e' },
    rows: [
      '...............',
      '....kkkkk......',
      '...klsssdk.....',
      '...lssssdd.....',
      '...lsddssd.....',  // a faint glyph
      '...lssssdd.....',
      '...lssssdd.....',
      '...lsddssd.....',
      '...lssssdd.....',
      '...klsssdk.....',
      '....ksssk......',
      '....lsssd......',
      '...gglsssdgg...',
      '..ggggsssgggg..',
      '. gggggggggg g.',
      '...............',
    ],
  },
  altar: {
    pal: { s: '#3e3a46', l: '#54505e', d: '#26242c', k: '#100f14', e: '#b48aff' },
    rows: [
      '...............',
      '...............',
      '....eeeeee.....',
      '...kssssssk....',
      '...lssssssd....',
      '...lssssssd....',
      '...kdssssdk....',
      '....lssssd.....',
      '....lssssd.....',
      '...klssssdk....',
      '...lsssssssd...',
      '..kssssssssdk..',
      '..lsssssssssd..',
      '..kdssssssddk..',
      '...kkddddkk....',
      '...............',
    ],
  },
  rubble: {
    pal: { s: '#34343c', l: '#46464e', d: '#202028', k: '#101014' },
    rows: [
      '...............',
      '...............',
      '...............',
      '...............',
      '......kk.......',
      '....klsdk......',
      '...kldssdk.....',
      '..klsssdsdk....',
      '.klsdksldssk...',
      'klsssdklssddk..',
      'ldsskddlsssddk.',
      'kkddkkkddddkkk.',
      '...............',
      '...............',
      '...............',
      '...............',
    ],
  },
  bush: {
    pal: { d: '#16241a', g: '#1f3324', l: '#2a4530', b: '#3a2a1a' },
    rows: [
      '...............',
      '...............',
      '...............',
      '....gg.gg......',
      '...ggggggg.....',
      '..gglgggdgg....',
      '..ggggglggg....',
      '.gggdgggggg....',
      '.ggggggglgg....',
      '.gglgggggdg....',
      '.. gggdgg g....',
      '....b...b......',
      '...............',
      '...............',
      '...............',
      '...............',
    ],
  },
  candle: {
    pal: { w: '#d8d0b0', d: '#9a8a60', k: '#100f0c' },
    rows: [
      '......',
      '..ww..',
      '..ww..',
      '..ww..',
      '.kddk.',
      '.kkkk.',
    ],
  },
};

const propCache = new Map();
export function prop(name) {
  if (propCache.has(name)) return propCache.get(name);
  const g = PROP_GRIDS[name];
  const c = g ? fromGrid(g.rows, g.pal) : mk(16, 16).c;
  propCache.set(name, c);
  return c;
}

// Animated brazier flame (3 frames). Returned as an array of canvases.
const flameCache = [];
export function flame(frame) {
  if (flameCache.length) return flameCache[frame % flameCache.length];
  for (let f = 0; f < 3; f++) {
    const { c, ctx } = mk(16, 16);
    const rng = makeRNG(f + 1);
    const h = 6 + f;
    for (let i = 0; i < 26; i++) {
      const t = rng();
      const x = 8 + Math.round(Math.sin(t * 6 + f) * (1 - t) * 3);
      const y = 8 - Math.round(t * h);
      const col = t > 0.7 ? '#fff2c0' : t > 0.4 ? '#ffb347' : '#ff6a2a';
      px(ctx, x, y, col); px(ctx, x + (rng() < .5 ? 1 : 0), y, col);
    }
    flameCache.push(c);
  }
  return flameCache[frame % 3];
}

// =========================================================================
// CHARACTERS — procedural humanoid with 3 frames × 4 directions.
// =========================================================================
const HERO_W = 16, HERO_H = 20;

function drawHumanoid(ctx, dir, frame, pal) {
  // frame: 0 idle, 1 & 2 walk. Legs swing on walk frames.
  const swing = frame === 1 ? 1 : frame === 2 ? -1 : 0;
  // shadow
  R(ctx, 4, 19, 8, 1, '#00000055');
  // legs
  R(ctx, 6, 15 + (swing < 0 ? 1 : 0), 2, 4 - (swing < 0 ? 1 : 0), pal.boot);
  R(ctx, 9, 15 + (swing > 0 ? 1 : 0), 2, 4 - (swing > 0 ? 1 : 0), pal.boot);
  // cloak / body
  R(ctx, 4, 8, 8, 8, pal.cloak);
  R(ctx, 4, 8, 8, 1, pal.cloakLt);                 // shoulder highlight
  R(ctx, 4, 15, 8, 1, pal.cloakDk);
  R(ctx, 5, 11, 6, 1, pal.trim);                   // belt/trim
  // head + hood
  R(ctx, 5, 2, 6, 6, pal.hood);
  R(ctx, 6, 4, 4, 4, pal.skin);                    // face area
  R(ctx, 5, 2, 6, 1, pal.hoodLt);

  // facing details
  if (dir === 'down') {
    px(ctx, 7, 6, pal.eye); px(ctx, 9, 6, pal.eye);
    R(ctx, 6, 2, 4, 2, pal.hood);                  // hood brim
  } else if (dir === 'up') {
    R(ctx, 5, 2, 6, 5, pal.hood);                  // back of hood, no face
  } else { // left/right profile (caller mirrors for left)
    px(ctx, 9, 6, pal.eye);
    R(ctx, 6, 2, 5, 2, pal.hood);
  }

  // glowing blade at the side (the "godless blade")
  if (pal.blade) {
    R(ctx, 12, 9, 1, 6, pal.blade);
    px(ctx, 12, 8, pal.bladeTip);
    px(ctx, 13, 10, pal.bladeTip);
  }
  // arms
  R(ctx, 3, 9, 1, 4, pal.cloakDk);
  R(ctx, 12, 9, 1, 4, pal.cloakDk);
}

function buildHumanoidSet(pal, withBlade) {
  const p = { ...pal };
  if (!withBlade) { p.blade = null; }
  const set = {};
  for (const dir of ['down', 'up', 'left', 'right']) {
    set[dir] = [];
    for (let f = 0; f < 3; f++) {
      const { c, ctx } = mk(HERO_W, HERO_H);
      const drawDir = dir === 'left' ? 'right' : dir;
      drawHumanoid(ctx, drawDir, f, p);
      if (dir === 'left') {
        // mirror horizontally
        const { c: m, ctx: mctx } = mk(HERO_W, HERO_H);
        mctx.translate(HERO_W, 0); mctx.scale(-1, 1);
        mctx.drawImage(c, 0, 0);
        set[dir].push(m);
      } else set[dir].push(c);
    }
  }
  return set;
}

let heroSet = null;
export function hero() {
  if (heroSet) return heroSet;
  heroSet = buildHumanoidSet({
    cloak: '#2a3550', cloakLt: '#3a4a70', cloakDk: '#1a2236', trim: '#caa84d',
    hood: '#202840', hoodLt: '#33405e', skin: '#caa07a', boot: '#1a1620',
    eye: '#9fe0ff', blade: '#bff6ff', bladeTip: '#ffffff',
  }, true);
  return heroSet;
}

const npcSetCache = new Map();
export function npc(color) {
  if (npcSetCache.has(color)) return npcSetCache.get(color);
  const set = buildHumanoidSet({
    cloak: color, cloakLt: shadeLighten(color, 24), cloakDk: '#1a1620', trim: '#e0d6b0',
    hood: shadeLighten(color, -10) || color, hoodLt: shadeLighten(color, 18),
    skin: '#caa07a', boot: '#241c16', eye: '#1a1414',
  }, false);
  npcSetCache.set(color, set);
  return set;
}

// =========================================================================
// MONSTERS — procedural per body type, 2 frames, sized by def.size.
// =========================================================================
const monsterCache = new Map();

export function monster(def) {
  if (monsterCache.has(def.id)) return monsterCache.get(def.id);
  const s = Math.max(16, Math.round(def.size * 2));
  const frames = [];
  for (let f = 0; f < 2; f++) {
    const { c, ctx } = mk(s, s);
    drawMonster(ctx, s, f, def);
    frames.push(c);
  }
  monsterCache.set(def.id, frames);
  return frames;
}

function drawMonster(ctx, s, f, def) {
  const col = def.color;
  const dk = shadeLighten(col, -22);
  const lt = shadeLighten(col, 26);
  const eye = def.boss ? '#ffcaa0' : '#ff5a3a';
  const cx = s / 2;
  const bob = f === 1 ? 1 : 0;
  R(ctx, cx - s * 0.28, s - 2, s * 0.56, 1, '#00000055'); // shadow

  switch (def.body) {
    case 'shade': {
      // floating wraith: tapered glowing body + wispy tail
      for (let y = 0; y < s - 2; y++) {
        const w = (s * 0.5) * (1 - y / (s * 1.6));
        const wob = Math.sin((y * 0.6) + f * 1.5) * 1.2;
        R(ctx, cx - w / 2 + wob, 1 + y - bob, w, 1, y < 3 ? lt : col);
      }
      px(ctx, cx - 2, 5 - bob, eye); px(ctx, cx + 1, 5 - bob, eye);
      break;
    }
    case 'beast': {
      const y0 = s * 0.35 - bob;
      R(ctx, cx - s * 0.34, y0, s * 0.68, s * 0.34, col);       // body
      R(ctx, cx - s * 0.34, y0, s * 0.68, 2, lt);
      R(ctx, cx + s * 0.18, y0 - s * 0.12, s * 0.22, s * 0.2, col); // head
      px(ctx, cx + s * 0.3, y0 - s * 0.05, eye);
      // ears / horns
      R(ctx, cx + s * 0.2, y0 - s * 0.2, 1, 2, dk);
      R(ctx, cx + s * 0.34, y0 - s * 0.2, 1, 2, dk);
      // legs (swing)
      const sw = f === 1 ? 1 : -1;
      R(ctx, cx - s * 0.28, y0 + s * 0.34, 2, s * 0.22 + sw, dk);
      R(ctx, cx + s * 0.16, y0 + s * 0.34, 2, s * 0.22 - sw, dk);
      if (def.boss) { // cinder maw: furnace mouth
        R(ctx, cx + s * 0.3, y0 + s * 0.02, 3, 3, '#ff8a3a');
        px(ctx, cx + s * 0.32, y0 + s * 0.04, '#fff2c0');
      }
      break;
    }
    case 'serpent': {
      // coiled body that sways + snake-hair
      for (let i = 0; i < 6; i++) {
        const yy = s - 3 - i * (s * 0.12);
        const xx = cx + Math.sin(i * 0.9 + f) * (s * 0.22) - 2;
        R(ctx, xx, yy, 4, 3, i === 5 ? lt : col);
      }
      const hx = cx + Math.sin(5 * 0.9 + f) * (s * 0.22) - 2;
      const hy = s - 3 - 5 * (s * 0.12);
      R(ctx, hx - 1, hy - 2, 6, 4, col);          // head
      px(ctx, hx, hy - 1, '#9fff7a'); px(ctx, hx + 3, hy - 1, '#9fff7a'); // green eyes
      for (let h = 0; h < 4; h++) px(ctx, hx + h - 1, hy - 3 - (h % 2) - f, dk); // hair
      break;
    }
    case 'bird': {
      // strix: body + flapping wings
      const wy = f === 1 ? -1 : 1;
      R(ctx, cx - 2, s * 0.4, 4, s * 0.3, col);   // body
      R(ctx, cx - s * 0.4, s * 0.4 + wy, s * 0.3, 2, dk);   // L wing
      R(ctx, cx + s * 0.12, s * 0.4 - wy, s * 0.3, 2, dk);  // R wing
      px(ctx, cx - 1, s * 0.44, eye); px(ctx, cx + 1, s * 0.44, eye);
      px(ctx, cx, s * 0.5, '#caa84d');            // beak
      break;
    }
    case 'automaton': {
      // bronze golem: blocky, glowing eye slit
      const y0 = s * 0.2;
      R(ctx, cx - s * 0.3, y0, s * 0.6, s * 0.5, col);
      R(ctx, cx - s * 0.3, y0, s * 0.6, 2, lt);
      R(ctx, cx - s * 0.3, y0 + s * 0.5 - 2, s * 0.6, 2, dk);
      R(ctx, cx - s * 0.18, y0 - s * 0.16, s * 0.36, s * 0.18, col);  // head
      R(ctx, cx - s * 0.12, y0 - s * 0.1, s * 0.24, 2, '#9fd0ff');    // eye slit (water-weak glow)
      // legs
      const sw = f === 1 ? 1 : 0;
      R(ctx, cx - s * 0.22, y0 + s * 0.5, 3, s * 0.26 - sw, dk);
      R(ctx, cx + s * 0.1, y0 + s * 0.5, 3, s * 0.26 - (1 - sw), dk);
      // arms
      R(ctx, cx - s * 0.4, y0 + s * 0.1, 2, s * 0.3, dk);
      R(ctx, cx + s * 0.32, y0 + s * 0.1, 2, s * 0.3, dk);
      break;
    }
    default:
      R(ctx, cx - 4, cx - 4, 8, 8, col);
  }
}

// Deterministic string hash for tile variant seeding.
function hashStr(s) {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
