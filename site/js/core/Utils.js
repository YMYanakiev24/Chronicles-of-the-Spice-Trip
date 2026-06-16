/**
 * Utils.js
 * ----------------------------------------------------------------------------
 * Small, dependency-free helpers used everywhere: math, easing, a seeded RNG
 * and a cheap value-noise function used to drive the terrain heightfield and
 * scatter props deterministically.
 * ----------------------------------------------------------------------------
 */

export const clamp = (v, min, max) => Math.min(max, Math.max(min, v));
export const lerp = (a, b, t) => a + (b - a) * t;
export const invLerp = (a, b, v) => (v - a) / (b - a);

/** Frame-rate independent smoothing: moves `a` toward `b` by `rate` per second. */
export const damp = (a, b, rate, dt) => lerp(a, b, 1 - Math.exp(-rate * dt));

export const easeInOut = (t) => (t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2);
export const easeOutCubic = (t) => 1 - Math.pow(1 - t, 3);
export const easeOutBack = (t) => {
  const c1 = 1.70158, c3 = c1 + 1;
  return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2);
};

export const randRange = (min, max) => min + Math.random() * (max - min);
export const randInt = (min, max) => Math.floor(randRange(min, max + 1));
export const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
export const chance = (p) => Math.random() < p;

/**
 * A tiny seedable pseudo-random generator (mulberry32). Deterministic output
 * for a given seed — used so the world scatters props the same way every load.
 */
export function makeRNG(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// --- 2D value noise -------------------------------------------------------
// Smooth, tileable-enough noise for terrain. Not Perlin, but visually fine and
// extremely cheap, which matters for sampling ground height every frame.
function hash2(x, y) {
  let h = x * 374761393 + y * 668265263;
  h = (h ^ (h >>> 13)) * 1274126177;
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

function smoothNoise(x, y) {
  const xi = Math.floor(x), yi = Math.floor(y);
  const xf = x - xi, yf = y - yi;
  const u = xf * xf * (3 - 2 * xf);
  const v = yf * yf * (3 - 2 * yf);
  const a = hash2(xi, yi);
  const b = hash2(xi + 1, yi);
  const c = hash2(xi, yi + 1);
  const d = hash2(xi + 1, yi + 1);
  return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

/** Fractal (layered) noise in roughly the 0..1 range. */
export function fbm(x, y, octaves = 4) {
  let total = 0, amp = 0.5, freq = 1, norm = 0;
  for (let i = 0; i < octaves; i++) {
    total += smoothNoise(x * freq, y * freq) * amp;
    norm += amp;
    amp *= 0.5;
    freq *= 2;
  }
  return total / norm;
}

/** Format a number with thousands separators (for gold, XP, etc.). */
export const fmt = (n) => Math.floor(n).toLocaleString('en-US');

/** Degrees → radians convenience. */
export const deg = (d) => (d * Math.PI) / 180;

// --- 2D helpers -----------------------------------------------------------
export const dist = (ax, ay, bx, by) => Math.hypot(ax - bx, ay - by);
export const dist2 = (ax, ay, bx, by) => { const dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; };
export const angleTo = (ax, ay, bx, by) => Math.atan2(by - ay, bx - ax);

/** Map a movement vector to one of 4 facing directions: 'down|up|left|right'. */
export function dir4(dx, dy) {
  if (Math.abs(dx) > Math.abs(dy)) return dx < 0 ? 'left' : 'right';
  return dy < 0 ? 'up' : 'down';
}
