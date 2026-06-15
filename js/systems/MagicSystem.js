/**
 * MagicSystem.js
 * ----------------------------------------------------------------------------
 * The 2D spell engine: turns a spell id + caster into live visuals and damage.
 * Projectiles (Emberhex, Tidelash, enemy bolts), an instant chaining beam
 * (Stormbolt), self-buffs (Wildmend, Grey Ward) and a screen-shaking AoE
 * ultimate (Stormcrown's Wrath). It owns transient particle/lightning effects
 * and exposes getLights() so spells illuminate the dark.
 * ----------------------------------------------------------------------------
 */

import { SPELLS } from '../data/GameData.js';
import { dist, randRange, clamp } from '../core/Utils.js';

export class MagicSystem {
  constructor(game) {
    this.game = game;
    this.projectiles = [];
    this.effects = [];
  }

  cast(id, caster, level = 1) {
    const spell = SPELLS[id];
    const ox = caster.x, oy = caster.y - 8;
    switch (spell.kind) {
      case 'projectile': this._projectile(spell, caster, ox, oy, level); break;
      case 'beam': this._beam(spell, caster, ox, oy, level); break;
      case 'self': this._self(spell, caster, level); break;
      case 'aoe': this._aoe(spell, caster, ox, oy, level); break;
    }
  }

  _aimDir(caster, ox, oy) {
    let dx = caster.aimX - ox, dy = caster.aimY - oy;
    const m = Math.hypot(dx, dy) || 1;
    return [dx / m, dy / m];
  }

  // --- Projectiles ----------------------------------------------------------
  _projectile(spell, caster, ox, oy, level) {
    const [dx, dy] = this._aimDir(caster, ox, oy);
    let dmg = spell.damage * (1 + (level - 1) * 0.25);
    if (spell.school === 'Fire' && caster.relics?.has('cinderSigil')) dmg *= 1.1;
    this.projectiles.push({
      x: ox + dx * 8, y: oy + dy * 8, vx: dx * spell.speed, vy: dy * spell.speed,
      life: 1.6, fromEnemy: false, damage: dmg, element: spell.school,
      radius: spell.radius, splash: spell.splash || 0, slow: spell.slow || 0,
      root: level >= 2 && spell.id === 'tidelash' ? 1.0 : 0,
      ring: level >= 2 && spell.id === 'emberhex',
      color: spell.color, glow: spell.glow, trail: [],
    });
    this.game.audio.sfx(spell.school === 'Fire' ? 'fire' : 'nature');
    this._burst(ox + dx * 6, oy + dy * 6, spell.color, 6, 0.3, 40);
  }

  enemyProjectile(x, y, dx, dy, damage, color) {
    this.projectiles.push({
      x, y, vx: dx * 130, vy: dy * 130, life: 2.4, fromEnemy: true,
      damage, element: null, radius: 4, splash: 0, color, glow: color, trail: [],
    });
    this.game.audio.sfx('lightning');
  }

  // --- Beam (Stormbolt): instant hit + chain --------------------------------
  _beam(spell, caster, ox, oy, level) {
    const [dx, dy] = this._aimDir(caster, ox, oy);
    const maxChain = spell.chain + (level - 1);
    const hit = new Set();
    let fx = ox, fy = oy;
    let target = this._coneTarget(ox, oy, dx, dy, spell.range, hit);
    let dmg = spell.damage * (1 + (level - 1) * 0.25);
    let n = 0;
    while (target && n <= maxChain) {
      hit.add(target);
      this._lightning(fx, fy, target.x, target.y - 8, spell.color);
      this.game.combat.damageEnemy(target, dmg, spell.school);
      fx = target.x; fy = target.y - 8; dmg *= 0.7; n++;
      target = this._nearest(fx, fy, 70, hit);
    }
    if (n === 0) this._lightning(ox, oy, ox + dx * spell.range, oy + dy * spell.range, spell.color);
    this.game.audio.sfx('lightning');
    this.game.ui.shake(0.25);
  }

  _coneTarget(ox, oy, dx, dy, range, exclude) {
    let best = null, bestScore = -1;
    for (const e of this.game.enemies) {
      if (e.dead || exclude.has(e)) continue;
      const ex = e.x - ox, ey = e.y - oy, d = Math.hypot(ex, ey);
      if (d > range) continue;
      const dot = (ex / d) * dx + (ey / d) * dy;
      if (dot < 0.5) continue;
      const score = dot - d / range;
      if (score > bestScore) { bestScore = score; best = e; }
    }
    return best;
  }
  _nearest(x, y, range, exclude) {
    let best = null, bd = range;
    for (const e of this.game.enemies) { if (e.dead || exclude.has(e)) continue; const d = dist(x, y, e.x, e.y); if (d < bd) { bd = d; best = e; } }
    return best;
  }

  // --- Self -----------------------------------------------------------------
  _self(spell, caster, level) {
    if (spell.id === 'wildmend') {
      caster.heal(spell.heal * (1 + (level - 1) * 0.3));
      if (level >= 2) caster.regenBuff = 8;
      this.game.audio.sfx('heal');
      this._aura(caster, '#76e06b');
    } else if (spell.id === 'greyward') {
      caster.applyShield(spell.shield * (1 + (level - 1) * 0.3), spell.duration);
      this.game.audio.sfx('shield');
      this._aura(caster, '#cdd6ff');
    }
  }

  // --- AoE ------------------------------------------------------------------
  _aoe(spell, caster, ox, oy, level) {
    this.game.audio.sfx('ultimate');
    this.game.ui.shake(1.0);
    this.game.ui.bigBanner("STORMCROWN'S WRATH");
    const dmg = spell.damage * (1 + (level - 1) * 0.25);
    for (const e of this.game.enemies) { if (!e.dead && dist(caster.x, caster.y, e.x, e.y) <= spell.radius) this.game.combat.damageEnemy(e, dmg, 'Lightning'); }
    for (let i = 0; i < 12; i++) {
      const a = Math.random() * 7, r = Math.random() * spell.radius;
      const gx = caster.x + Math.cos(a) * r, gy = caster.y + Math.sin(a) * r;
      setTimeout(() => this._lightning(gx, gy - 60, gx, gy, spell.color, 2), i * 50);
    }
    this._shockwave(caster.x, caster.y, spell.radius, spell.color);
    if (level >= 2) setTimeout(() => { if (!caster.dead) this._aoe({ ...spell, radius: spell.radius * 0.8 }, caster, caster.x, caster.y, 1); }, 800);
  }

  // =========================================================================
  // Effects
  // =========================================================================
  _burst(x, y, color, count, life, speed) {
    const parts = [];
    for (let i = 0; i < count; i++) { const a = Math.random() * 7; const s = randRange(speed * 0.4, speed); parts.push({ x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s, life }); }
    let age = 0;
    this.effects.push({
      light: () => null,
      update: (dt) => { age += dt; for (const p of parts) { p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 0.9; p.vy *= 0.9; } return age < life; },
      draw: (ctx, cx, cy) => { ctx.globalCompositeOperation = 'lighter'; for (const p of parts) { ctx.fillStyle = color; ctx.globalAlpha = clamp(1 - age / life, 0, 1); ctx.fillRect(Math.round(p.x - cx), Math.round(p.y - cy), 2, 2); } ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'source-over'; },
    });
  }

  _lightning(x1, y1, x2, y2, color, width = 1) {
    const segs = 7, pts = [];
    for (let i = 0; i <= segs; i++) {
      const t = i / segs;
      let x = x1 + (x2 - x1) * t, y = y1 + (y2 - y1) * t;
      if (i > 0 && i < segs) { x += randRange(-4, 4) * width; y += randRange(-4, 4) * width; }
      pts.push([x, y]);
    }
    let age = 0; const life = 0.22;
    this.effects.push({
      light: () => ({ x: x2, y: y2, r: 34, color, intensity: 0.3 * Math.max(0, 1 - age / life) }),
      update: (dt) => { age += dt; return age < life; },
      draw: (ctx, cx, cy) => {
        ctx.globalCompositeOperation = 'lighter';
        ctx.strokeStyle = color; ctx.globalAlpha = Math.max(0, 1 - age / life); ctx.lineWidth = width + 1;
        ctx.beginPath(); ctx.moveTo(pts[0][0] - cx, pts[0][1] - cy);
        for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0] - cx, pts[i][1] - cy);
        ctx.stroke(); ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'source-over';
      },
    });
    this._burst(x2, y2, color, 6, 0.25, 50);
  }

  _aura(caster, color) {
    let age = 0; const life = 0.7;
    this.effects.push({
      light: () => ({ x: caster.x, y: caster.y - 8, r: 30, color, intensity: 0.3 }),
      update: (dt) => { age += dt; return age < life; },
      draw: (ctx, cx, cy) => {
        ctx.globalCompositeOperation = 'lighter'; ctx.strokeStyle = color; ctx.globalAlpha = Math.max(0, 1 - age / life); ctx.lineWidth = 2;
        const r = 4 + age * 24;
        ctx.beginPath(); ctx.arc(caster.x - cx, caster.y - 8 - cy - age * 10, r, 0, 7); ctx.stroke();
        ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'source-over';
      },
    });
  }

  _shockwave(x, y, radius, color) {
    let age = 0; const life = 0.6;
    this.effects.push({
      light: () => ({ x, y, r: radius, color, intensity: 0.4 * Math.max(0, 1 - age / life) }),
      update: (dt) => { age += dt; return age < life; },
      draw: (ctx, cx, cy) => {
        ctx.globalCompositeOperation = 'lighter'; ctx.strokeStyle = color; ctx.globalAlpha = Math.max(0, 0.9 * (1 - age / life)); ctx.lineWidth = 3;
        ctx.beginPath(); ctx.arc(x - cx, y - cy, (age / life) * radius, 0, 7); ctx.stroke();
        ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'source-over';
      },
    });
  }

  // =========================================================================
  // Update / draw / lights
  // =========================================================================
  update(dt) {
    for (let i = this.projectiles.length - 1; i >= 0; i--) {
      const p = this.projectiles[i];
      p.life -= dt;
      p.trail.push([p.x, p.y]); if (p.trail.length > 6) p.trail.shift();
      p.x += p.vx * dt; p.y += p.vy * dt;
      let hit = this.game.map.solidAt(p.x, p.y);

      if (p.fromEnemy) {
        if (!this.game.player.dead && dist(p.x, p.y, this.game.player.x, this.game.player.y - 8) < p.radius + 7) { this.game.combat.damagePlayer(p.damage); hit = true; }
      } else {
        for (const e of this.game.enemies) {
          if (e.dead) continue;
          if (dist(p.x, p.y, e.x, e.y - e.spr.height * 0.4) < p.radius + e.r) { this._impact(p, e); hit = true; break; }
        }
      }
      if (hit || p.life <= 0) {
        if (hit) this._burst(p.x, p.y, p.color, 8, 0.3, 60);
        if (hit && p.ring) this._shockwave(p.x, p.y, 26, p.color);
        this.projectiles.splice(i, 1);
      }
    }
    for (let i = this.effects.length - 1; i >= 0; i--) if (!this.effects[i].update(dt)) this.effects.splice(i, 1);
  }

  _impact(p, primary) {
    this.game.combat.damageEnemy(primary, p.damage, p.element);
    if (p.slow) primary.applySlow(p.slow, 2.5);
    if (p.root) primary.applyRoot(p.root);
    if (p.splash > 0) {
      for (const e of this.game.enemies) {
        if (e.dead || e === primary) continue;
        if (dist(p.x, p.y, e.x, e.y) <= p.splash) { this.game.combat.damageEnemy(e, p.damage * 0.5, p.element); if (p.slow) e.applySlow(p.slow, 1.5); }
      }
    }
  }

  draw(ctx, camX, camY) {
    ctx.globalCompositeOperation = 'lighter';
    for (const p of this.projectiles) {
      // trail
      for (let i = 0; i < p.trail.length; i++) {
        const t = i / p.trail.length;
        ctx.globalAlpha = t * 0.5; ctx.fillStyle = p.glow;
        ctx.fillRect(Math.round(p.trail[i][0] - camX) - 1, Math.round(p.trail[i][1] - camY) - 1, 2, 2);
      }
      ctx.globalAlpha = 1;
      // glow
      const sx = Math.round(p.x - camX), sy = Math.round(p.y - camY);
      const g = ctx.createRadialGradient(sx, sy, 0, sx, sy, p.radius * 2.4);
      g.addColorStop(0, p.glow); g.addColorStop(1, hexA(p.glow, 0));
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(sx, sy, p.radius * 2.4, 0, 7); ctx.fill();
      ctx.fillStyle = '#ffffff'; ctx.fillRect(sx - 1, sy - 1, 2, 2);
    }
    ctx.globalCompositeOperation = 'source-over';
    for (const e of this.effects) e.draw(ctx, camX, camY);
  }

  getLights() {
    const out = [];
    for (const p of this.projectiles) out.push({ x: p.x, y: p.y, r: 26, color: p.glow, intensity: 0.22 });
    for (const e of this.effects) { const l = e.light && e.light(); if (l) out.push(l); }
    return out;
  }

  clear() { this.projectiles.length = 0; this.effects.length = 0; }
}

function hexA(hex, a) {
  if (hex[0] !== '#') return hex;
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${a})`;
}
