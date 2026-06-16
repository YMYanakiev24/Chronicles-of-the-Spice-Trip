/**
 * TileMap.js
 * ----------------------------------------------------------------------------
 * The 2D world: a tile grid per region with collision, themed terrain, gothic
 * props, collectible pickups and static light sources. It also owns the
 * atmosphere that sells the dark mythic mood:
 *   - a darkness pass that plunges each region into gloom and then carves pools
 *     of candle/brazier/moon light back out (so you really feel the dark), and
 *   - drifting fog layers.
 *
 * Maps are generated procedurally (deterministic per region) with bespoke
 * touches for the starting Mistwood. Coordinates are world pixels; TS-sized
 * tiles. The Game y-sorts props + entities for correct overlap.
 * ----------------------------------------------------------------------------
 */

import { getTile, prop, flame, TS } from './PixelArt.js';
import { makeRNG, clamp } from '../core/Utils.js';
import { REGIONS, NPCS } from '../data/GameData.js';

const SIZES = {
  mistwood: [48, 42], temple: [46, 38], ruins: [50, 42],
  caves: [54, 46], citadel: [46, 40], godreach: [46, 44],
};

export class TileMap {
  constructor(game) {
    this.game = game;
    this.region = null;
    this.w = 0; this.h = 0;
    this.cells = [];      // flat array of { tile, solid, wall, water }
    this.props = [];      // tall objects, y-sorted with entities
    this.pickups = [];    // collectibles
    this.lights = [];     // static light sources
    this.time = 0;

    // Reusable offscreen darkness buffer (sized to the virtual screen).
    this._dark = document.createElement('canvas');
    this._darkCtx = this._dark.getContext('2d');
    this._fog = this._buildFogTexture();
  }

  get pixelW() { return this.w * TS; }
  get pixelH() { return this.h * TS; }

  cell(tx, ty) { return this.cells[ty * this.w + tx]; }

  // --- Collision ------------------------------------------------------------
  solidAt(px, py) {
    const tx = Math.floor(px / TS), ty = Math.floor(py / TS);
    if (tx < 0 || ty < 0 || tx >= this.w || ty >= this.h) return true;
    return this.cells[ty * this.w + tx].solid;
  }

  // =========================================================================
  // Generation
  // =========================================================================
  load(regionId) {
    const region = REGIONS[regionId];
    this.region = region;
    const [w, h] = SIZES[regionId] || [48, 40];
    this.w = w; this.h = h;
    this.cells = new Array(w * h);
    this.props = [];
    this.pickups = [];
    this.lights = [];
    const rng = makeRNG(hashStr(regionId));

    // 1) Lay base ground everywhere.
    for (let ty = 0; ty < h; ty++) {
      for (let tx = 0; tx < w; tx++) {
        const variant = (rng() * 3) | 0;
        this.cells[ty * w + tx] = {
          tile: getTile(region.tileset, 'floor', variant),
          solid: false, wall: false, water: false,
        };
      }
    }

    // 2) Border wall ring.
    for (let tx = 0; tx < w; tx++) { this._setWall(tx, 0, region); this._setWall(tx, h - 1, region); }
    for (let ty = 0; ty < h; ty++) { this._setWall(0, ty, region); this._setWall(w - 1, ty, region); }

    // 3) Region-specific layout.
    if (regionId === 'caves') this._genCaves(region, rng);
    else this._genOverworld(region, rng, regionId);

    // 4) Guarantee the spawn + NPC tiles (and a little around them) are clear.
    this._clearArea(region.spawn.tx, region.spawn.ty, 3);
    if (regionId === 'mistwood') {
      for (const id of Object.keys(NPCS)) {
        if (NPCS[id].region === regionId) this._clearArea(NPCS[id].tx, NPCS[id].ty, 2);
      }
    }

    // 5) Pickups (asphodel everywhere; relic shards only in the Mistwood).
    this._scatterPickups(region, rng, regionId);
  }

  _setWall(tx, ty, region) {
    const c = this.cell(tx, ty);
    c.tile = getTile(region.tileset, 'wall');
    c.solid = true; c.wall = true;
  }

  _clearArea(tx, ty, r) {
    for (let y = ty - r; y <= ty + r; y++) {
      for (let x = tx - r; x <= tx + r; x++) {
        if (x <= 0 || y <= 0 || x >= this.w - 1 || y >= this.h - 1) continue;
        const c = this.cell(x, y);
        c.solid = false; c.wall = false;
      }
    }
    // Remove any props that landed in the cleared area.
    this.props = this.props.filter((p) => {
      const px = Math.floor(p.tx), py = Math.floor(p.ty);
      return !(px >= tx - r && px <= tx + r && py >= ty - r && py <= ty + r);
    });
  }

  // Open, scatter-based overworld for forest/temple/ruins/citadel/godreach.
  _genOverworld(region, rng, regionId) {
    const w = this.w, h = this.h;

    // Wandering dirt/marble paths for visual structure.
    let px = (w / 2) | 0, py = h - 3;
    for (let i = 0; i < w * 1.4; i++) {
      const c = this.cell(clamp(px, 1, w - 2), clamp(py, 1, h - 2));
      if (!c.wall) c.tile = getTile(region.tileset, 'path');
      if (rng() < 0.5) px += rng() < 0.5 ? 1 : -1; else py += rng() < 0.6 ? -1 : 1;
      px = clamp(px, 2, w - 3); py = clamp(py, 2, h - 3);
    }

    // Prop densities per tileset.
    const cfg = {
      forest:   { trees: 0.10, bushes: 0.05, pillars: 0.0, graves: 0.02, statues: 0.004, rubble: 0.01 },
      temple:   { trees: 0.01, bushes: 0.01, pillars: 0.05, graves: 0.0, statues: 0.01, rubble: 0.03 },
      ruins:    { trees: 0.02, bushes: 0.02, pillars: 0.06, graves: 0.02, statues: 0.012, rubble: 0.05 },
      citadel:  { trees: 0.005, bushes: 0.0, pillars: 0.04, graves: 0.0, statues: 0.014, rubble: 0.02 },
      godreach: { trees: 0.01, bushes: 0.0, pillars: 0.05, graves: 0.0, statues: 0.02, rubble: 0.01 },
    }[regionId] || { trees: 0.06, bushes: 0.03, pillars: 0.01, graves: 0.01, statues: 0.005, rubble: 0.02 };

    for (let ty = 2; ty < h - 2; ty++) {
      for (let tx = 2; tx < w - 2; tx++) {
        const c = this.cell(tx, ty);
        if (c.wall) continue;
        const r = rng();
        if (r < cfg.trees) this._addProp(tx, ty, regionId === 'forest' || regionId === 'mistwood' ? (rng() < 0.5 ? 'pine' : 'deadTree') : 'deadTree', true);
        else if (r < cfg.trees + cfg.pillars) this._addProp(tx, ty, 'pillar', true);
        else if (r < cfg.trees + cfg.pillars + cfg.statues) this._addProp(tx, ty, 'statue', true);
        else if (r < cfg.trees + cfg.pillars + cfg.statues + cfg.graves) this._addProp(tx, ty, 'grave', false);
        else if (r < cfg.trees + cfg.pillars + cfg.statues + cfg.graves + cfg.bushes) this._addProp(tx, ty, 'bush', false);
        else if (r < cfg.trees + cfg.pillars + cfg.statues + cfg.graves + cfg.bushes + cfg.rubble) this._addProp(tx, ty, 'rubble', false);
      }
    }

    // Braziers / altars as light anchors, sprinkled around.
    const lightCount = regionId === 'caves' ? 14 : 10;
    for (let i = 0; i < lightCount; i++) {
      const tx = 2 + ((rng() * (w - 4)) | 0), ty = 2 + ((rng() * (h - 4)) | 0);
      const c = this.cell(tx, ty);
      if (c.wall || c.solid) continue;
      if (rng() < 0.25) this._addProp(tx, ty, 'altar', true, '#b48aff', 3);
      else this._addProp(tx, ty, 'brazier', true, '#ff9a3a', 4, true);
    }

    // Citadel: glowing windows along the top wall.
    if (regionId === 'citadel') {
      for (let tx = 4; tx < w - 4; tx += 3) {
        this.lights.push({ x: tx * TS + 8, y: 1 * TS + 8, r: 34, color: '#cdd6ff', flick: 0.4, phase: rng() * 7 });
      }
    }
  }

  // Cellular-automata caves for the Underway.
  _genCaves(region, rng) {
    const w = this.w, h = this.h;
    let grid = [];
    for (let ty = 0; ty < h; ty++) for (let tx = 0; tx < w; tx++) {
      grid[ty * w + tx] = (tx < 2 || ty < 2 || tx > w - 3 || ty > h - 3) ? 1 : (rng() < 0.46 ? 1 : 0);
    }
    for (let pass = 0; pass < 4; pass++) {
      const next = grid.slice();
      for (let ty = 1; ty < h - 1; ty++) for (let tx = 1; tx < w - 1; tx++) {
        let walls = 0;
        for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) if (dx || dy) walls += grid[(ty + dy) * w + (tx + dx)];
        next[ty * w + tx] = walls > 4 ? 1 : (walls < 3 ? 0 : grid[ty * w + tx]);
      }
      grid = next;
    }
    for (let ty = 0; ty < h; ty++) for (let tx = 0; tx < w; tx++) {
      if (grid[ty * w + tx]) this._setWall(tx, ty, region);
    }
    // A few scattered rubble + braziers for orientation.
    for (let i = 0; i < 16; i++) {
      const tx = 2 + ((rng() * (w - 4)) | 0), ty = 2 + ((rng() * (h - 4)) | 0);
      const c = this.cell(tx, ty);
      if (c.solid) continue;
      if (rng() < 0.5) this._addProp(tx, ty, 'brazier', true, '#b48aff', 4, true);
      else this._addProp(tx, ty, 'rubble', false);
    }
  }

  _addProp(tx, ty, name, solid, lightColor = null, lightR = 0, flicker = false) {
    const sprite = prop(name);
    const baseline = ty * TS + TS;                 // feet line for y-sort
    const drawX = tx * TS + (TS - sprite.width) / 2;
    const drawY = baseline - sprite.height;
    const p = { tx, ty, name, sprite, x: tx * TS + TS / 2, baseline, drawX, drawY, animated: name === 'brazier' };
    this.props.push(p);
    if (solid) { const c = this.cell(tx, ty); c.solid = true; }
    if (lightColor && lightR) {
      this.lights.push({ x: tx * TS + 8, y: ty * TS + 8, r: lightR * TS / 4 + 18, color: lightColor, flick: flicker ? 0.6 : 0.15, phase: Math.random() * 7 });
    }
  }

  _scatterPickups(region, rng, regionId) {
    const w = this.w, h = this.h;
    const place = (type, item, count) => {
      let placed = 0, tries = 0;
      while (placed < count && tries < count * 40) {
        tries++;
        const tx = 2 + ((rng() * (w - 4)) | 0), ty = 2 + ((rng() * (h - 4)) | 0);
        const c = this.cell(tx, ty);
        if (c.solid) continue;
        this.pickups.push({ x: tx * TS + TS / 2, y: ty * TS + TS / 2, type, item, bob: rng() * 7, taken: false });
        placed++;
      }
    };
    place('asphodel', 'asphodel', 10);
    if (regionId === 'mistwood') place('relicShard', 'relicShard', 4);
  }

  removePickup(pk) {
    const i = this.pickups.indexOf(pk);
    if (i >= 0) this.pickups.splice(i, 1);
  }

  // =========================================================================
  // Rendering
  // =========================================================================
  update(dt) { this.time += dt; }

  drawGround(ctx, camX, camY) {
    const vw = ctx.canvas.width, vh = ctx.canvas.height;
    const x0 = Math.max(0, Math.floor(camX / TS));
    const y0 = Math.max(0, Math.floor(camY / TS));
    const x1 = Math.min(this.w - 1, Math.ceil((camX + vw) / TS));
    const y1 = Math.min(this.h - 1, Math.ceil((camY + vh) / TS));
    for (let ty = y0; ty <= y1; ty++) {
      for (let tx = x0; tx <= x1; tx++) {
        const c = this.cells[ty * this.w + tx];
        ctx.drawImage(c.tile, tx * TS - camX, ty * TS - camY);
      }
    }
  }

  // Props are returned to the Game so they can be y-sorted with entities.
  drawProp(ctx, p, camX, camY) {
    ctx.drawImage(p.sprite, Math.round(p.drawX - camX), Math.round(p.drawY - camY));
    if (p.animated) {
      const fr = (this.time * 8) | 0;
      ctx.drawImage(flame(fr), Math.round(p.tx * TS - camX), Math.round(p.ty * TS - camY - 6));
    }
  }

  drawPickups(ctx, camX, camY) {
    for (const pk of this.pickups) {
      const yb = Math.sin(this.time * 3 + pk.bob) * 2;
      const sx = Math.round(pk.x - camX), sy = Math.round(pk.y - camY + yb);
      if (pk.type === 'asphodel') {
        // glowing pale flower
        ctx.fillStyle = '#dfe6ff'; ctx.fillRect(sx - 1, sy - 3, 2, 2);
        ctx.fillStyle = '#aab6e0';
        ctx.fillRect(sx - 3, sy - 1, 2, 2); ctx.fillRect(sx + 1, sy - 1, 2, 2);
        ctx.fillRect(sx - 1, sy + 1, 2, 2);
        ctx.fillStyle = '#ffe8a0'; ctx.fillRect(sx - 1, sy - 1, 2, 2);
        ctx.fillStyle = '#3a6d3a'; ctx.fillRect(sx, sy + 2, 1, 3);
      } else {
        // relic shard
        ctx.fillStyle = '#9fe0ff'; ctx.fillRect(sx - 1, sy - 4, 2, 8);
        ctx.fillStyle = '#eaffff'; ctx.fillRect(sx - 1, sy - 4, 1, 8);
        ctx.fillStyle = '#caa84d'; ctx.fillRect(sx - 1, sy + 3, 2, 2);
      }
    }
  }

  // --- The lighting + fog pass (drawn last over the scene buffer) ----------
  drawLighting(ctx, camX, camY, dynamicLights) {
    const vw = ctx.canvas.width, vh = ctx.canvas.height;
    if (this._dark.width !== vw || this._dark.height !== vh) { this._dark.width = vw; this._dark.height = vh; }
    const d = this._darkCtx;
    const region = this.region;

    // Fog (drawn first, under the darkness, so light cuts through it).
    this._drawFog(ctx, camX, camY, region);

    // Darkness layer.
    d.globalCompositeOperation = 'source-over';
    d.clearRect(0, 0, vw, vh);
    d.fillStyle = `rgba(5,6,14,${region.darkness})`;
    d.fillRect(0, 0, vw, vh);

    // Carve light pools out of the darkness.
    d.globalCompositeOperation = 'destination-out';
    const carve = (sx, sy, r) => {
      const g = d.createRadialGradient(sx, sy, 0, sx, sy, r);
      g.addColorStop(0, 'rgba(0,0,0,1)');
      g.addColorStop(0.6, 'rgba(0,0,0,0.7)');
      g.addColorStop(1, 'rgba(0,0,0,0)');
      d.fillStyle = g; d.beginPath(); d.arc(sx, sy, r, 0, 7); d.fill();
    };
    for (const L of this.lights) {
      const sx = L.x - camX, sy = L.y - camY;
      if (sx < -120 || sy < -120 || sx > vw + 120 || sy > vh + 120) continue;
      const r = L.r * (1 + Math.sin(this.time * 6 + L.phase) * L.flick * 0.18);
      carve(sx, sy, r);
    }
    for (const L of dynamicLights) carve(L.x - camX, L.y - camY, L.r);

    d.globalCompositeOperation = 'source-over';
    ctx.drawImage(this._dark, 0, 0);

    // Colored glow on top (additive) for warmth/magic.
    ctx.globalCompositeOperation = 'lighter';
    const glow = (sx, sy, r, color, a) => {
      const g = ctx.createRadialGradient(sx, sy, 0, sx, sy, r);
      g.addColorStop(0, hexA(color, a));
      g.addColorStop(1, hexA(color, 0));
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(sx, sy, r, 0, 7); ctx.fill();
    };
    for (const L of this.lights) {
      const sx = L.x - camX, sy = L.y - camY;
      if (sx < -120 || sy < -120 || sx > vw + 120 || sy > vh + 120) continue;
      glow(sx, sy, L.r * 0.9, L.color, 0.22);
    }
    for (const L of dynamicLights) glow(L.x - camX, L.y - camY, L.r * 0.8, L.color || '#bfe0ff', L.intensity ?? 0.18);
    ctx.globalCompositeOperation = 'source-over';

    // Vignette to frame the screen.
    const vg = ctx.createRadialGradient(vw / 2, vh / 2, vh * 0.35, vw / 2, vh / 2, vh * 0.75);
    vg.addColorStop(0, 'rgba(0,0,0,0)');
    vg.addColorStop(1, 'rgba(0,0,0,0.55)');
    ctx.fillStyle = vg; ctx.fillRect(0, 0, vw, vh);
  }

  _drawFog(ctx, camX, camY, region) {
    const vw = ctx.canvas.width, vh = ctx.canvas.height;
    const a = region.fog.density * 0.5;
    ctx.globalAlpha = a;
    ctx.globalCompositeOperation = 'screen';
    const s = this._fog;
    const t = this.time;
    const ox1 = -((camX * 0.3 + t * 7) % s.width);
    const oy1 = -((camY * 0.3) % s.height);
    const ox2 = -((camX * 0.5 - t * 4) % s.width);
    const oy2 = -((camY * 0.5 + t * 2) % s.height);
    ctx.fillStyle = region.fog.color;
    // Tint the fog by drawing the texture then a color multiply isn't trivial;
    // instead the texture is already soft grey and 'screen' lifts it toward white.
    for (let y = oy1; y < vh; y += s.height) for (let x = ox1; x < vw; x += s.width) ctx.drawImage(s, x, y);
    ctx.globalAlpha = a * 0.7;
    for (let y = oy2; y < vh; y += s.height) for (let x = ox2; x < vw; x += s.width) ctx.drawImage(s, x, y);
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }

  _buildFogTexture() {
    const size = 128;
    const c = document.createElement('canvas'); c.width = c.height = size;
    const ctx = c.getContext('2d');
    const rng = makeRNG(777);
    ctx.clearRect(0, 0, size, size);
    for (let i = 0; i < 40; i++) {
      const x = rng() * size, y = rng() * size, r = 12 + rng() * 28;
      const g = ctx.createRadialGradient(x, y, 0, x, y, r);
      const v = 90 + ((rng() * 80) | 0);
      g.addColorStop(0, `rgba(${v},${v},${v},0.5)`);
      g.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.fill();
    }
    return c;
  }
}

function hexA(hex, a) {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${a})`;
}
function hashStr(s) {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
