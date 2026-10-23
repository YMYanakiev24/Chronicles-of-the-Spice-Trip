// Generates every region with the original TileMap.js under Node and records a
// signature of the result — wall/solid grid, props (tile + name), pickups and
// the deterministic light anchors — so the C# TileMap can be checked against it.
//
//   node map-parity.mjs <web-repo> <out.json>
//
// TileMap.js draws nothing during generation, but it imports PixelArt.js (which
// needs a canvas) and creates two canvases in its constructor, so a minimal
// canvas stub is installed first.

import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const [, , repo, out] = process.argv;
if (!repo || !out) { console.error('usage: node map-parity.mjs <web-repo> <out.json>'); process.exit(2); }

class Ctx {
  constructor(c) { this.canvas = c; }
  set fillStyle(v) {} get fillStyle() { return '#000'; }
  fillRect() {} clearRect() {} drawImage() {} beginPath() {} arc() {} fill() {}
  translate() {} scale() {}
  createRadialGradient() { return { addColorStop() {} }; }
}
class Canvas { constructor() { this.width = 0; this.height = 0; this._c = new Ctx(this); } getContext() { return this._c; } }
globalThis.document = { createElement: () => new Canvas() };

const { TileMap } = await import(pathToFileURL(path.join(repo, 'js/world/TileMap.js')).href);
const gd = await import(pathToFileURL(path.join(repo, 'js/data/GameData.js')).href);

function fnv(str) {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) { h ^= str.charCodeAt(i); h = Math.imul(h, 16777619); }
  return (h >>> 0).toString(16).padStart(8, '0');
}

const result = {};
for (const id of gd.REGION_ORDER) {
  const map = new TileMap({});
  map.load(id);
  const grid = map.cells.map((c) => (c.wall ? 'W' : c.solid ? 'S' : '.')).join('');
  const props = map.props.map((p) => `${p.tx},${p.ty},${p.name}`).join(';');
  const pickups = map.pickups.map((p) => `${p.x},${p.y},${p.type}`).join(';');
  const lights = map.lights.map((l) => `${l.x},${l.y},${l.r},${l.color}`).join(';');
  result[id] = {
    w: map.w, h: map.h,
    walls: map.cells.filter((c) => c.wall).length,
    solids: map.cells.filter((c) => c.solid).length,
    props: map.props.length, pickups: map.pickups.length, lights: map.lights.length,
    gridHash: fnv(grid), propsHash: fnv(props), pickupsHash: fnv(pickups), lightsHash: fnv(lights),
  };
}
fs.writeFileSync(out, JSON.stringify(result, null, 2) + '\n');
console.log(`wrote map signatures for ${Object.keys(result).length} regions to ${out}`);
