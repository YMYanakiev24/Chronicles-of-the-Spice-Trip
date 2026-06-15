/**
 * Enemy.js
 * ----------------------------------------------------------------------------
 * A 2D monster with a small state machine (wander → chase → attack), tile-aware
 * movement, melee + ranged behaviors, weakness-based damage, a floating health
 * bar and a hit-flash. Stats/appearance come from a CREATURES entry.
 * ----------------------------------------------------------------------------
 */

import { monster } from '../world/PixelArt.js';
import { clamp, dist, randRange } from '../core/Utils.js';

export class Enemy {
  constructor(game, def, x, y) {
    this.game = game;
    this.def = def;
    this.frames = monster(def);
    this.spr = this.frames[0];
    this.x = x; this.y = y;
    this.vx = 0; this.vy = 0;
    this.hp = def.hp; this.maxHp = def.hp;
    this.r = def.size * 0.45;
    this.state = 'wander';
    this.attackCd = randRange(0, def.attackRate);
    this.slowT = 0; this.slowF = 1; this.rootT = 0;
    this.hitFlash = 0; this.dead = false; this.deathTimer = 0;
    this.bob = Math.random() * 7; this.animTime = Math.random();
    this.frame = 0; this.lunge = 0;
    this.homeX = x; this.homeY = y;
    this._pickWander();
    this.barTimer = 0;
  }

  get baseline() { return this.y; }

  takeDamage(amount, element) {
    if (this.dead) return;
    if (element && element === this.def.weakness) amount *= 2;
    this.hp = clamp(this.hp - amount, 0, this.maxHp);
    this.hitFlash = 0.12; this.barTimer = 3; this.state = 'chase';
    this.game.audio.sfx('hit');
    this.game.ui.spawnDamageNumber(this.x, this.y - this.spr.height * 0.6, Math.round(amount), element === this.def.weakness);
    if (this.hp <= 0) this._die();
  }
  applySlow(f, d) { this.slowT = d; this.slowF = f; }
  applyRoot(d) { this.rootT = d; }

  _die() { this.dead = true; this.deathTimer = 0.6; this.game.audio.sfx('death'); this.game.combat.onEnemyKilled(this); }

  _pickWander() {
    const a = Math.random() * 7, r = randRange(20, 80);
    this.wx = this.homeX + Math.cos(a) * r; this.wy = this.homeY + Math.sin(a) * r;
    this.wWait = randRange(1, 3);
  }

  update(dt, time) {
    if (this.dead) { this.deathTimer -= dt; return; }
    const p = this.game.player;
    const dx = p.x - this.x, dy = p.y - this.y;
    const d = Math.hypot(dx, dy);

    if (this.attackCd > 0) this.attackCd -= dt;
    if (this.slowT > 0) this.slowT -= dt;
    if (this.rootT > 0) this.rootT -= dt;
    if (this.hitFlash > 0) this.hitFlash -= dt;
    if (this.lunge > 0) this.lunge -= dt;
    if (this.barTimer > 0) this.barTimer -= dt;

    let speed = this.def.speed * (this.slowT > 0 ? this.slowF : 1);
    if (this.rootT > 0) speed = 0;

    if (!p.dead && d < this.def.aggroRange) this.state = 'chase';
    else if (this.state === 'chase' && d > this.def.aggroRange * 1.6) this.state = 'wander';

    let mvx = 0, mvy = 0;
    if (this.state === 'chase' && !p.dead) {
      const inRange = d <= this.def.attackRange;
      const nd = d || 1;
      if (this.def.ranged) {
        // Keep at mid distance and fire.
        if (d < this.def.attackRange * 0.5) { mvx = -dx / nd * speed; mvy = -dy / nd * speed; }
        else if (d > this.def.attackRange) { mvx = dx / nd * speed; mvy = dy / nd * speed; }
        if (this.attackCd <= 0 && d < this.def.attackRange * 1.1) this._attack(dx, dy, d);
      } else {
        if (inRange) { if (this.attackCd <= 0) this._attack(dx, dy, d); }
        else { mvx = dx / nd * speed; mvy = dy / nd * speed; }
      }
    } else {
      const wdx = this.wx - this.x, wdy = this.wy - this.y, wd = Math.hypot(wdx, wdy);
      if (wd < 4) { this.wWait -= dt; if (this.wWait <= 0) this._pickWander(); }
      else { mvx = wdx / wd * speed * 0.4; mvy = wdy / wd * speed * 0.4; }
    }

    this.vx = mvx; this.vy = mvy;
    this._move(mvx * dt, 0); this._move(0, mvy * dt);

    // Animation
    if (Math.abs(mvx) + Math.abs(mvy) > 4 || this.def.float) { this.animTime += dt; this.frame = Math.floor(this.animTime * 6) % 2; }
    else this.frame = 0;
    this.spr = this.frames[this.frame];
  }

  _move(dx, dy) {
    const m = this.game.map;
    const nx = this.x + dx, ny = this.y + dy;
    if (dx !== 0 && !m.solidAt(nx + Math.sign(dx) * this.r, this.y)) this.x = nx;
    if (dy !== 0 && !m.solidAt(this.x, ny + Math.sign(dy) * this.r)) this.y = ny;
  }

  _attack(dx, dy, d) {
    this.attackCd = this.def.attackRate;
    this.lunge = 0.2;
    if (this.def.ranged) {
      const nd = d || 1;
      this.game.magic.enemyProjectile(this.x, this.y - this.spr.height * 0.4, dx / nd, dy / nd, this.def.damage, this.def.color);
    } else if (d <= this.def.attackRange + 4) {
      this.game.combat.damagePlayer(this.def.damage);
    }
  }

  draw(ctx, camX, camY) {
    const a = this.dead ? Math.max(0, this.deathTimer / 0.6) : 1;
    const bob = this.def.float ? Math.sin(this.bob + performance.now() * 0.004) * 2 : 0;
    const lunge = this.lunge > 0 ? 2 : 0;
    const sx = Math.round(this.x - this.spr.width / 2 - camX);
    const sy = Math.round(this.y - this.spr.height + 3 - camY + bob - lunge);
    ctx.globalAlpha = a;
    ctx.drawImage(this.spr, sx, sy);
    if (this.hitFlash > 0) { ctx.globalCompositeOperation = 'lighter'; ctx.globalAlpha = 0.8; ctx.drawImage(this.spr, sx, sy); ctx.globalCompositeOperation = 'source-over'; }
    ctx.globalAlpha = 1;

    // Health bar
    if (this.barTimer > 0 && !this.dead) {
      const w = Math.max(14, this.spr.width);
      const bx = Math.round(this.x - w / 2 - camX), by = sy - 4;
      ctx.fillStyle = '#120a10'; ctx.fillRect(bx, by, w, 3);
      const ratio = this.hp / this.maxHp;
      ctx.fillStyle = ratio > 0.5 ? '#7fd06a' : ratio > 0.25 ? '#e0c24a' : '#e0533a';
      ctx.fillRect(bx, by, Math.round(w * ratio), 3);
    }
  }
}
