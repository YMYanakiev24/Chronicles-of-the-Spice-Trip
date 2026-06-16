/**
 * Player.js
 * ----------------------------------------------------------------------------
 * The Unmarked demigod: a top-down 2D hero. Owns 8-directional movement with
 * tile collision and sliding, a dodge-roll with i-frames, a fallback melee
 * with the "godless blade", mouse-aimed spellcasting, and all RPG progression
 * (HP, mana, XP, level, gold, learned powers, inventory). Spells are aimed at
 * the cursor's world position.
 * ----------------------------------------------------------------------------
 */

import { hero } from '../world/PixelArt.js';
import { clamp, damp, dir4, dist } from '../core/Utils.js';
import { SPELLS, xpForLevel } from '../data/GameData.js';

const WALK = 74, RUN = 122, ROLL_SPEED = 210;
const RADIUS = 5;          // collision radius in px

export class Player {
  constructor(game) {
    this.game = game;
    this.frames = hero();

    // Kinematics
    this.x = 100; this.y = 100;
    this.vx = 0; this.vy = 0;
    this.facing = 'down';
    this.moving = false;
    this.animTime = 0; this.frame = 0;

    // Aim (world coords of the cursor)
    this.aimX = this.x; this.aimY = this.y - 1;

    // Roll
    this.rolling = 0; this.rollCd = 0; this.rollDX = 0; this.rollDY = 1;

    // Melee
    this.meleeCd = 0; this.meleeTime = 0;

    // Stats & progression
    this.level = 1; this.xp = 0;
    this.maxHp = 100; this.hp = 100;
    this.maxMana = 100; this.mana = 100;
    this.gold = 0; this.manaRegen = 8; this.hpRegen = 1.5;
    this.shield = 0; this.shieldTimer = 0; this.regenBuff = 0;
    this.combatTimer = 99; this.invuln = 0;
    this.cooldowns = {};

    this.learnedSpells = new Set(['emberhex']);
    this.spellLevels = { emberhex: 1 };
    this.hotbar = ['emberhex', null, null, null, null, null];
    this.inventory = {};
    this.relics = new Set();
    this.dead = false;

    this.castFlash = 0; this.castColor = '#bfe0ff';
  }

  get baseline() { return this.y; }
  get torchRadius() {
    // Brighter torch in darker regions so you can always see a little.
    const d = this.game.map.region ? this.game.map.region.darkness : 0.6;
    return 56 + d * 46;
  }

  // --- Progression (engine-agnostic) ---------------------------------------
  learnSpell(id) {
    if (this.learnedSpells.has(id)) {
      this.spellLevels[id] = (this.spellLevels[id] || 1) + 1;
      this.game.ui.toast(`${SPELLS[id].name} deepens — tier ${this.spellLevels[id]}!`, 'gold');
      return;
    }
    this.learnedSpells.add(id);
    this.spellLevels[id] = 1;
    const slot = this.hotbar.indexOf(null);
    if (slot >= 0) this.hotbar[slot] = id;
    this.game.ui.toast(`New power: ${SPELLS[id].name}! (slot ${slot + 1})`, 'gold');
    this.game.audio.sfx('levelup');
  }

  addItem(id, n = 1) { this.inventory[id] = (this.inventory[id] || 0) + n; if (id === 'cinderSigil') this.relics.add('cinderSigil'); }
  removeItem(id, n = 1) { if (!this.inventory[id]) return; this.inventory[id] = Math.max(0, this.inventory[id] - n); if (!this.inventory[id]) delete this.inventory[id]; }
  itemCount(id) { return this.inventory[id] || 0; }
  addGold(n) { this.gold += n; }

  addXP(n) {
    this.xp += n;
    this.game.ui.toast(`+${n} XP`, 'xp');
    while (this.xp >= xpForLevel(this.level)) { this.xp -= xpForLevel(this.level); this.levelUp(); }
    this.game.ui.refreshHUD();
  }
  levelUp() {
    this.level++; this.maxHp += 15; this.maxMana += 12;
    this.hp = this.maxHp; this.mana = this.maxMana; this.manaRegen += 0.6;
    this.game.audio.sfx('levelup');
    this.game.ui.toast(`Level ${this.level}! The god-blood burns brighter.`, 'gold');
    this.game.ui.bigBanner(`LEVEL ${this.level}`);
    for (const id of Object.keys(SPELLS)) {
      if (SPELLS[id].unlockLevel === this.level && !this.learnedSpells.has(id)) this.learnSpell(id);
    }
  }

  // --- Combat --------------------------------------------------------------
  takeDamage(amount) {
    if (this.dead || this.invuln > 0) return;
    this.combatTimer = 0;
    if (this.shield > 0) {
      const a = Math.min(this.shield, amount); this.shield -= a; amount -= a;
      if (this.shield <= 0) this.game.ui.toast('Your Grey Ward shatters!', 'warn');
    }
    if (amount <= 0) { this.game.ui.refreshHUD(); return; }
    this.hp = clamp(this.hp - amount, 0, this.maxHp);
    this.invuln = 0.4;
    this.game.audio.sfx('hurt');
    this.game.ui.damageFlash();
    this.game.ui.refreshHUD();
    if (this.hp <= 0) this.die();
  }
  heal(a) { this.hp = clamp(this.hp + a, 0, this.maxHp); this.game.ui.refreshHUD(); }
  restoreMana(a) { this.mana = clamp(this.mana + a, 0, this.maxMana); this.game.ui.refreshHUD(); }
  applyShield(a, d) { this.shield = a; this.shieldTimer = d; }

  die() { if (this.dead) return; this.dead = true; this.game.audio.sfx('death'); this.game.onPlayerDeath(); }
  respawn() {
    this.dead = false; this.hp = this.maxHp; this.mana = this.maxMana; this.shield = 0;
    const sp = this.game.map.region.spawn;
    this.x = sp.tx * 16 + 8; this.y = sp.ty * 16 + 8; this.vx = this.vy = 0; this.invuln = 2;
  }

  // --- Casting -------------------------------------------------------------
  tryCastSlot(slot) { const id = this.hotbar[slot]; if (id) this.castSpell(id); }
  castSpell(id) {
    if (this.dead || this.rolling > 0) return;
    const spell = SPELLS[id];
    if (!spell || !this.learnedSpells.has(id)) return;
    if ((this.cooldowns[id] || 0) > 0) { this.game.ui.toast(`${spell.name} is gathering...`, 'warn', 0.6); return; }
    if (this.mana < spell.manaCost) { this.game.ui.toast('Not enough mana!', 'warn', 0.6); this.game.audio.sfx('ui'); return; }
    this.mana -= spell.manaCost;
    this.cooldowns[id] = spell.cooldown;
    this.castFlash = 0.25; this.castColor = spell.glow;
    // Face the cursor when casting.
    this.facing = dir4(this.aimX - this.x, this.aimY - this.y);
    this.game.magic.cast(id, this, this.spellLevels[id] || 1);
    this.game.quests.onCast(id);
    this.game.ui.refreshHUD();
  }

  melee() {
    if (this.meleeCd > 0 || this.dead || this.rolling > 0) return;
    this.meleeCd = 0.45; this.meleeTime = 0.2;
    this.facing = dir4(this.aimX - this.x, this.aimY - this.y);
    this.game.audio.sfx('ui');
    // Hit enemies in a short arc toward the aim.
    const ang = Math.atan2(this.aimY - this.y, this.aimX - this.x);
    for (const e of this.game.enemies) {
      if (e.dead) continue;
      const d = dist(this.x, this.y, e.x, e.y);
      if (d > 24 + e.r) continue;
      const ea = Math.atan2(e.y - this.y, e.x - this.x);
      let diff = Math.abs(((ea - ang + Math.PI) % (Math.PI * 2)) - Math.PI);
      if (diff < 1.1) this.game.combat.damageEnemy(e, 12 + this.level * 2, 'Blade');
    }
  }

  roll() {
    if (this.rollCd > 0 || this.dead) return;
    this.rolling = 0.32; this.rollCd = 0.7; this.invuln = 0.32;
    // Roll in movement dir, or facing if standing still.
    let dx = this.vx, dy = this.vy;
    if (Math.abs(dx) + Math.abs(dy) < 1) { const f = { down: [0, 1], up: [0, -1], left: [-1, 0], right: [1, 0] }[this.facing]; dx = f[0]; dy = f[1]; }
    const m = Math.hypot(dx, dy) || 1; this.rollDX = dx / m; this.rollDY = dy / m;
    this.game.audio.sfx('ui');
  }

  // --- Per-frame -----------------------------------------------------------
  update(dt) {
    const inp = this.game.input;
    // Aim from cursor.
    const aim = this.game.screenToWorld(inp.mouseX, inp.mouseY);
    this.aimX = aim.x; this.aimY = aim.y;

    // Timers
    for (const k in this.cooldowns) if (this.cooldowns[k] > 0) this.cooldowns[k] = Math.max(0, this.cooldowns[k] - dt);
    if (this.rollCd > 0) this.rollCd -= dt;
    if (this.meleeCd > 0) this.meleeCd -= dt;
    if (this.meleeTime > 0) this.meleeTime -= dt;
    if (this.invuln > 0) this.invuln -= dt;
    if (this.castFlash > 0) this.castFlash -= dt;
    if (this.shieldTimer > 0) { this.shieldTimer -= dt; if (this.shieldTimer <= 0) this.shield = 0; }
    this.combatTimer += dt;

    // Movement
    let mvx = 0, mvy = 0;
    if (this.rolling > 0) {
      this.rolling -= dt;
      mvx = this.rollDX * ROLL_SPEED; mvy = this.rollDY * ROLL_SPEED;
      this.moving = true;
    } else {
      const ax = inp.moveAxis();
      const len = Math.hypot(ax.x, ax.y);
      this.moving = len > 0.01;
      const speed = (inp.isDown('ShiftLeft') || inp.isDown('ShiftRight')) ? RUN : WALK;
      if (this.moving) {
        mvx = (ax.x / len) * speed; mvy = (-ax.y / len) * speed;   // axis.y is +1 up
        this.facing = dir4(mvx, mvy);
      }
      // Roll / melee inputs
      if (inp.wasPressed('Space')) this.roll();
    }
    this.vx = mvx; this.vy = mvy;

    // Integrate with axis-separated collision (slide along walls).
    this._move(mvx * dt, 0);
    this._move(0, mvy * dt);

    // Keep inside the map.
    this.x = clamp(this.x, RADIUS + 2, this.game.map.pixelW - RADIUS - 2);
    this.y = clamp(this.y, RADIUS + 2, this.game.map.pixelH - RADIUS - 2);

    // Animation
    if (this.moving) { this.animTime += dt; this.frame = (Math.floor(this.animTime * 8) % 2) + 1; }
    else { this.animTime = 0; this.frame = 0; }

    // Regen
    this.mana = clamp(this.mana + this.manaRegen * dt, 0, this.maxMana);
    let hpr = this.hpRegen + this.regenBuff;
    if (this.combatTimer < 3) hpr *= 0.25;
    this.hp = clamp(this.hp + hpr * dt, 0, this.maxHp);
    if (this.regenBuff > 0) this.regenBuff = Math.max(0, this.regenBuff - dt * 2);

    this.game.ui.refreshHUDSmooth();
  }

  _move(dx, dy) {
    const nx = this.x + dx, ny = this.y + dy;
    const m = this.game.map;
    // Probe the leading edge of the collision circle.
    const ex = dx !== 0 ? nx + Math.sign(dx) * RADIUS : this.x;
    const ey = dy !== 0 ? ny + Math.sign(dy) * RADIUS : this.y;
    if (dx !== 0 && !m.solidAt(ex, this.y - RADIUS + 2) && !m.solidAt(ex, this.y + RADIUS - 2)) this.x = nx;
    if (dy !== 0 && !m.solidAt(this.x - RADIUS + 2, ey) && !m.solidAt(this.x + RADIUS - 2, ey)) this.y = ny;
  }

  draw(ctx, camX, camY) {
    // Blink while invulnerable.
    if (this.invuln > 0 && Math.floor(this.invuln * 20) % 2 === 0 && !this.rolling) return;
    const spr = this.frames[this.facing][this.frame];
    let sx = Math.round(this.x - spr.width / 2 - camX);
    let sy = Math.round(this.y - spr.height + 3 - camY);
    if (this.rolling > 0) { ctx.save(); ctx.translate(sx + spr.width / 2, sy + spr.height / 2); ctx.rotate((0.32 - this.rolling) * 12); ctx.drawImage(spr, -spr.width / 2, -spr.height / 2); ctx.restore(); }
    else ctx.drawImage(spr, sx, sy);

    // Melee blade arc.
    if (this.meleeTime > 0) {
      const ang = Math.atan2(this.aimY - this.y, this.aimX - this.x);
      ctx.strokeStyle = '#cdefff'; ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(this.x - camX, this.y - 6 - camY, 16, ang - 0.8, ang + 0.8);
      ctx.stroke();
    }
    // Grey Ward bubble.
    if (this.shield > 0) {
      ctx.strokeStyle = 'rgba(200,210,230,0.6)'; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.arc(this.x - camX, this.y - 8 - camY, 12, 0, 7); ctx.stroke();
    }
  }

  // --- Save/load -----------------------------------------------------------
  serialize() {
    return {
      level: this.level, xp: this.xp, maxHp: this.maxHp, hp: this.hp,
      maxMana: this.maxMana, mana: this.mana, gold: this.gold, manaRegen: this.manaRegen,
      learnedSpells: [...this.learnedSpells], spellLevels: this.spellLevels,
      hotbar: this.hotbar, inventory: this.inventory, relics: [...this.relics],
      x: this.x, y: this.y, facing: this.facing,
    };
  }
  deserialize(d) {
    Object.assign(this, {
      level: d.level, xp: d.xp, maxHp: d.maxHp, hp: d.hp, maxMana: d.maxMana,
      mana: d.mana, gold: d.gold, manaRegen: d.manaRegen,
      spellLevels: d.spellLevels || { emberhex: 1 },
      hotbar: d.hotbar || ['emberhex', null, null, null, null, null],
      inventory: d.inventory || {}, facing: d.facing || 'down',
    });
    this.learnedSpells = new Set(d.learnedSpells || ['emberhex']);
    this.relics = new Set(d.relics || []);
    if (d.x != null) { this.x = d.x; this.y = d.y; }
  }
}
