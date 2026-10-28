// Runs the original PixelArt.js under Node with a tiny software canvas and
// records an FNV-1a hash of every sprite it produces. The Unity EditMode tests
// render the same sprites with the C# port and compare against these hashes.
//
//   node pixel-parity.mjs <web-repo> <out.json>
//
// The stub implements only what PixelArt.js uses: fillStyle (#rgb, #rrggbb,
// #rrggbbaa, rgb()), fillRect with source-over blending, and drawImage with a
// translate/scale transform (used to mirror the left-facing frames). Blending
// uses the same formula as PixelCanvas.cs, so the hashes test the port's logic.

import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const [, , repo, out] = process.argv;
if (!repo || !out) { console.error('usage: node pixel-parity.mjs <web-repo> <out.json>'); process.exit(2); }

function parseColor(s) {
  s = String(s).trim();
  if (s[0] === '#') {
    const h = s.slice(1);
    if (h.length === 3) return [...h].map((c) => parseInt(c + c, 16)).concat(255);
    if (h.length === 6) return [0, 2, 4].map((i) => parseInt(h.slice(i, i + 2), 16)).concat(255);
    if (h.length === 8) return [0, 2, 4, 6].map((i) => parseInt(h.slice(i, i + 2), 16));
    return null;
  }
  const m = s.match(/^rgba?\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*(?:,\s*([\d.]+)\s*)?\)$/);
  if (m) {
    const c = [+m[1], +m[2], +m[3]];
    if (c.some((v) => v < 0 || v > 255)) return null;   // invalid CSS colour: canvas ignores it
    return c.concat(m[4] === undefined ? 255 : Math.round(+m[4] * 255));
  }
  return null;
}

// Same arithmetic as PixelCanvas.Blend in C#.
function blend(d, i, c) {
  const sa = c[3] / 255;
  if (sa >= 1) { d[i] = c[0]; d[i + 1] = c[1]; d[i + 2] = c[2]; d[i + 3] = 255; return; }
  if (sa <= 0) return;
  const da = d[i + 3] / 255;
  const oa = sa + da * (1 - sa);
  for (let k = 0; k < 3; k++) d[i + k] = Math.round((c[k] * sa + d[i + k] * da * (1 - sa)) / oa);
  d[i + 3] = Math.round(oa * 255);
}

class Ctx {
  constructor(canvas) { this.canvas = canvas; this._fill = [0, 0, 0, 255]; this.tx = 0; this.sx = 1; this.imageSmoothingEnabled = true; }
  set fillStyle(v) { const c = parseColor(v); if (c) this._fill = c; }
  get fillStyle() { return this._fill; }
  fillRect(x, y, w, h) {
    const { width: W, height: H, data } = this.canvas;
    const x0 = Math.max(0, Math.round(x)), y0 = Math.max(0, Math.round(y));
    const x1 = Math.min(W, Math.round(x + w)), y1 = Math.min(H, Math.round(y + h));
    for (let yy = y0; yy < y1; yy++) for (let xx = x0; xx < x1; xx++) blend(data, (yy * W + xx) * 4, this._fill);
  }
  translate(x) { this.tx += x * this.sx; }
  scale(sx) { this.sx *= sx; }
  drawImage(src, dx, dy) {
    const { width: W, data } = this.canvas;
    for (let y = 0; y < src.height; y++) for (let x = 0; x < src.width; x++) {
      const si = (y * src.width + x) * 4;
      const tx = this.sx < 0 ? this.tx - (dx + x) - 1 : this.tx + dx + x;
      const ty = dy + y;
      if (tx < 0 || tx >= W || ty < 0 || ty >= this.canvas.height) continue;
      blend(data, (ty * W + tx) * 4, [src.data[si], src.data[si + 1], src.data[si + 2], src.data[si + 3]]);
    }
  }
}

class Canvas {
  constructor() { this._w = 0; this._h = 0; this.data = new Uint8ClampedArray(0); this._ctx = new Ctx(this); }
  set width(v) { this._w = v; this.data = new Uint8ClampedArray(this._w * this._h * 4); }
  get width() { return this._w; }
  set height(v) { this._h = v; this.data = new Uint8ClampedArray(this._w * this._h * 4); }
  get height() { return this._h; }
  getContext() { return this._ctx; }
}

globalThis.document = { createElement: () => new Canvas() };

const pa = await import(pathToFileURL(path.join(repo, 'js/world/PixelArt.js')).href);

function fnv(c) {
  let h = 2166136261;
  for (let i = 0; i < c.data.length; i++) { h ^= c.data[i]; h = Math.imul(h, 16777619); }
  return `${c.width}x${c.height}:${(h >>> 0).toString(16).padStart(8, '0')}`;
}

const hashes = {};
const hero = pa.hero();
for (const dir of ['down', 'up', 'left', 'right']) hero[dir].forEach((c, f) => { hashes[`hero.${dir}.${f}`] = fnv(c); });
for (let f = 0; f < 3; f++) hashes[`flame.${f}`] = fnv(pa.flame(f));
for (const name of ['deadTree', 'pine', 'pillar', 'statue', 'brazier', 'grave', 'altar', 'rubble', 'bush', 'candle']) hashes[`prop.${name}`] = fnv(pa.prop(name));

fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, JSON.stringify(hashes, null, 2) + '\n');
console.log(`wrote ${Object.keys(hashes).length} sprite hashes to ${out}`);
