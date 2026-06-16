/**
 * BossBattle.js
 * ----------------------------------------------------------------------------
 * A self-contained top-down arena fight, launched from the Path map. The
 * player (the chosen character) moves with WASD/arrows and shoots toward the
 * mouse; the boss roams and fires back. Dark-forest backdrop, health bars,
 * and a win/lose card. Independent of the (dormant) main RPG — it runs its own
 * loop and reads input from the shared Input manager.
 * ----------------------------------------------------------------------------
 */

import { hero } from '../world/PixelArt.js';

const AW = 800, AH = 450;        // logical arena size (canvas drawn at this res)

export class BossBattle {
  constructor(scenes) {
    this.scenes = scenes;
    this.game = scenes.game;
    this.active = false;
    this._raf = null;
    this._imgCache = new Map();
    this.heroFrames = hero();
    this._buildDOM();
  }

  _buildDOM() {
    const wrap = document.createElement('div');
    wrap.id = 'boss-battle';
    wrap.className = 'boss-battle hidden';
    wrap.innerHTML = `
      <canvas id="bb-canvas" width="${AW}" height="${AH}"></canvas>
      <button class="bb-flee" id="bb-flee">⟵ Flee</button>
      <div class="bb-hint">WASD / arrows to move · aim &amp; click (or Space) to shoot</div>
      <div class="bb-result hidden" id="bb-result">
        <h1 id="bb-result-title">VICTORY</h1>
        <p id="bb-result-sub"></p>
        <div class="bb-result-btns">
          <button class="menu-btn" id="bb-again">Fight again</button>
          <button class="menu-btn primary" id="bb-return">Return to the path</button>
        </div>
      </div>`;
    document.getElementById('ui').appendChild(wrap);
    this.root = wrap;
    this.canvas = document.getElementById('bb-canvas');
    this.ctx = this.canvas.getContext('2d');
    this.ctx.imageSmoothingEnabled = false;

    document.getElementById('bb-flee').addEventListener('click', () => this.stop());
    document.getElementById('bb-return').addEventListener('click', () => this.stop());
    document.getElementById('bb-again').addEventListener('click', () => this.start(this._boss, this._char));
  }

  // Lazy image loader (boss/character art).
  _img(src) {
    if (!src) return null;
    if (this._imgCache.has(src)) return this._imgCache.get(src);
    const img = new Image();
    img.onload = () => { img._ready = true; };
    img.src = src;
    this._imgCache.set(src, img);
    return img;
  }

  start(boss, char) {
    this._boss = boss; this._char = char;
    this.root.classList.remove('hidden');
    document.getElementById('bb-result').classList.add('hidden');
    this.active = true;

    this.bossImg = this._img(boss.img);
    this.charImg = char && char.img ? this._img(char.img) : null;

    // Player state (bottom-center).
    this.player = { x: AW / 2, y: AH - 70, r: 14, hp: 100, maxHp: 100, cd: 0, face: 'up', hurt: 0 };
    // Boss state (top-center).
    this.boss = {
      x: AW / 2, y: 90, r: 32, hp: boss.hp, maxHp: boss.hp,
      vx: 0, vy: 0, shootCd: 1.2, moveCd: 0, tx: AW / 2, ty: 90, hurt: 0,
    };
    this.pShots = [];
    this.bShots = [];
    this.t = 0;
    this._last = performance.now();
    this._spawnTrees();

    this.game.audio.playMusic('caves');   // tense bed
    if (!this._raf) this._loop();
  }

  stop() {
    this.active = false;
    if (this._raf) { cancelAnimationFrame(this._raf); this._raf = null; }
    this.root.classList.add('hidden');
    this.game.audio.playMusic('menu');
    this.scenes.enterPath(true);          // back to the path map
  }

  _spawnTrees() {
    // Static silhouette trees framing the arena (drawn each frame).
    this.trees = [];
    const rng = () => Math.random();
    for (let i = 0; i < 22; i++) {
      const edge = rng();
      const x = edge < 0.5 ? rng() * AW : (rng() < 0.5 ? rng() * 120 : AW - rng() * 120);
      this.trees.push({ x, y: 60 + rng() * (AH - 120), s: 0.6 + rng() * 0.9, sway: rng() * 7 });
    }
  }

  _mouseArena() {
    const r = this.canvas.getBoundingClientRect();
    const mx = (this.game.input.mouseX - r.left) / r.width * AW;
    const my = (this.game.input.mouseY - r.top) / r.height * AH;
    return { x: mx, y: my };
  }

  // ---- Loop -----------------------------------------------------------------
  _loop() {
    this._raf = requestAnimationFrame(() => this._loop());
    const now = performance.now();
    const dt = Math.min(0.05, (now - this._last) / 1000);
    this._last = now;
    this.t += dt;
    if (this.active) this._update(dt);
    this._render();
  }

  _update(dt) {
    const inp = this.game.input;
    const p = this.player, b = this.boss;
    if (p.hurt > 0) p.hurt -= dt;
    if (b.hurt > 0) b.hurt -= dt;

    // --- Player movement ---
    let mx = 0, my = 0;
    if (inp.isDown('KeyW') || inp.isDown('ArrowUp')) my -= 1;
    if (inp.isDown('KeyS') || inp.isDown('ArrowDown')) my += 1;
    if (inp.isDown('KeyA') || inp.isDown('ArrowLeft')) mx -= 1;
    if (inp.isDown('KeyD') || inp.isDown('ArrowRight')) mx += 1;
    const ml = Math.hypot(mx, my) || 1;
    const speed = 220;
    p.x = clamp(p.x + (mx / ml) * speed * dt * (mx || my ? 1 : 0), 16, AW - 16);
    p.y = clamp(p.y + (my / ml) * speed * dt * (mx || my ? 1 : 0), 16, AH - 16);

    // --- Player shooting (aim at mouse) ---
    const aim = this._mouseArena();
    p.cd -= dt;
    const wantShoot = inp.mouseDown(0) || inp.isDown('Space');
    if (wantShoot && p.cd <= 0) {
      p.cd = 0.22;
      const a = Math.atan2(aim.y - p.y, aim.x - p.x);
      this.pShots.push({ x: p.x, y: p.y, vx: Math.cos(a) * 380, vy: Math.sin(a) * 380, r: 5, life: 1.4 });
      this.game.audio.sfx('fire');
    }

    // --- Boss AI: roam toward waypoints, fire spreads ---
    b.moveCd -= dt;
    if (b.moveCd <= 0) {
      b.moveCd = 1.4 + Math.random() * 1.2;
      b.tx = 120 + Math.random() * (AW - 240);
      b.ty = 60 + Math.random() * (AH * 0.4);
    }
    const bdx = b.tx - b.x, bdy = b.ty - b.y, bd = Math.hypot(bdx, bdy) || 1;
    const bspeed = 95;
    b.x = clamp(b.x + (bdx / bd) * bspeed * dt, 40, AW - 40);
    b.y = clamp(b.y + (bdy / bd) * bspeed * dt, 40, AH * 0.55);

    b.shootCd -= dt;
    if (b.shootCd <= 0) {
      b.shootCd = 1.1;
      const base = Math.atan2(p.y - b.y, p.x - b.x);
      for (let s = -1; s <= 1; s++) {
        const a = base + s * 0.22;
        this.bShots.push({ x: b.x, y: b.y + 10, vx: Math.cos(a) * 240, vy: Math.sin(a) * 240, r: 7, life: 3 });
      }
      this.game.audio.sfx('lightning');
    }

    // --- Projectiles ---
    for (let i = this.pShots.length - 1; i >= 0; i--) {
      const s = this.pShots[i]; s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
      if (dist(s.x, s.y, b.x, b.y) < s.r + b.r) {
        b.hp -= 8; b.hurt = 0.1; this.game.audio.sfx('hit');
        this.pShots.splice(i, 1); if (b.hp <= 0) return this._end(true); continue;
      }
      if (s.life <= 0 || s.x < 0 || s.x > AW || s.y < 0 || s.y > AH) this.pShots.splice(i, 1);
    }
    for (let i = this.bShots.length - 1; i >= 0; i--) {
      const s = this.bShots[i]; s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
      if (p.hurt <= 0 && dist(s.x, s.y, p.x, p.y) < s.r + p.r) {
        p.hp -= 8; p.hurt = 0.6; this.game.audio.sfx('hurt');
        this.bShots.splice(i, 1); if (p.hp <= 0) return this._end(false); continue;
      }
      if (s.life <= 0 || s.x < 0 || s.x > AW || s.y < 0 || s.y > AH) this.bShots.splice(i, 1);
    }

    // Touch damage from the boss body.
    if (p.hurt <= 0 && dist(p.x, p.y, b.x, b.y) < p.r + b.r) { p.hp -= 12; p.hurt = 0.8; this.game.audio.sfx('hurt'); if (p.hp <= 0) this._end(false); }
  }

  _end(win) {
    this.active = false;
    const r = document.getElementById('bb-result');
    document.getElementById('bb-result-title').textContent = win ? 'VICTORY' : 'DEFEATED';
    document.getElementById('bb-result-title').className = win ? 'win' : 'lose';
    document.getElementById('bb-result-sub').textContent = win
      ? `${this._boss.name} has fallen.`
      : `${this._boss.name} bested you. Steel yourself and try again.`;
    r.classList.remove('hidden');
    this.game.audio.sfx(win ? 'levelup' : 'death');
    if (win) this.scenes.markBossDefeated(this._boss.id);
  }

  // ---- Render ---------------------------------------------------------------
  _render() {
    const ctx = this.ctx;
    this._drawForest(ctx);
    if (!this.player) return;
    const p = this.player, b = this.boss;

    // Boss
    this._drawFighter(ctx, b.x, b.y, b.r, this.bossImg, this._boss.color, b.hurt > 0);
    // Player
    this._drawPlayer(ctx, p);

    // Projectiles
    ctx.globalCompositeOperation = 'lighter';
    for (const s of this.pShots) glow(ctx, s.x, s.y, s.r, '#9fe0ff');
    for (const s of this.bShots) glow(ctx, s.x, s.y, s.r, this._boss.color || '#ff6a3a');
    ctx.globalCompositeOperation = 'source-over';

    // Boss HP bar (top)
    bar(ctx, AW / 2 - 220, 16, 440, 16, b.hp / b.maxHp, '#b03a3a', this._boss.name.toUpperCase());
    // Player HP bar (bottom-left)
    bar(ctx, 20, AH - 28, 240, 14, p.hp / p.maxHp, '#3aa05a', 'YOU');
  }

  _drawForest(ctx) {
    const sky = ctx.createLinearGradient(0, 0, 0, AH);
    sky.addColorStop(0, '#0a0716'); sky.addColorStop(0.6, '#120a22'); sky.addColorStop(1, '#0c1410');
    ctx.fillStyle = sky; ctx.fillRect(0, 0, AW, AH);
    // moon
    const mx = AW * 0.8, my = AH * 0.18;
    const mg = ctx.createRadialGradient(mx, my, 0, mx, my, 90);
    mg.addColorStop(0, 'rgba(180,170,210,0.35)'); mg.addColorStop(1, 'rgba(180,170,210,0)');
    ctx.fillStyle = mg; ctx.fillRect(mx - 90, my - 90, 180, 180);
    ctx.fillStyle = '#c9c2dd'; ctx.beginPath(); ctx.arc(mx, my, 26, 0, 7); ctx.fill();
    // ground
    ctx.fillStyle = '#0e1a12'; ctx.fillRect(0, AH * 0.82, AW, AH * 0.18);
    // trees (silhouettes)
    for (const t of this.trees) {
      const sway = Math.sin(this.t * 0.6 + t.sway) * 3;
      ctx.fillStyle = '#05060a';
      ctx.fillRect(t.x - 3 * t.s + sway, t.y - 70 * t.s, 6 * t.s, 80 * t.s);
      for (let b = 0; b < 4; b++) {
        ctx.fillRect(t.x - 18 * t.s + sway, t.y - 60 * t.s + b * 16 * t.s, 16 * t.s, 3);
        ctx.fillRect(t.x + 2 * t.s + sway, t.y - 54 * t.s + b * 16 * t.s, 16 * t.s, 3);
      }
    }
    // fog band
    ctx.globalCompositeOperation = 'screen';
    for (let i = 0; i < 3; i++) {
      const y = AH * 0.55 + i * 30, off = (this.t * (10 + i * 6)) % (AW + 120) - 60;
      ctx.fillStyle = `rgba(120,130,150,${0.05 + i * 0.02})`;
      for (let x = -60; x < AW; x += 90) { ctx.beginPath(); ctx.ellipse(x + off, y, 60, 9, 0, 0, 7); ctx.fill(); }
    }
    ctx.globalCompositeOperation = 'source-over';
    // vignette
    const vg = ctx.createRadialGradient(AW / 2, AH / 2, AH * 0.3, AW / 2, AH / 2, AH * 0.8);
    vg.addColorStop(0, 'rgba(0,0,0,0)'); vg.addColorStop(1, 'rgba(0,0,0,0.6)');
    ctx.fillStyle = vg; ctx.fillRect(0, 0, AW, AH);
  }

  _drawFighter(ctx, x, y, r, img, color, hurt) {
    ctx.save();
    if (hurt) ctx.filter = 'brightness(2)';
    if (img && img._ready) {
      const s = r * 2.6;
      ctx.drawImage(img, x - s / 2, y - s / 2, s, s);
    } else {
      ctx.fillStyle = color || '#9a5fd0';
      ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.fill();
      ctx.fillStyle = '#ff5a4a'; ctx.fillRect(x - 8, y - 6, 4, 4); ctx.fillRect(x + 4, y - 6, 4, 4);
    }
    ctx.restore();
    // shadow
    ctx.fillStyle = 'rgba(0,0,0,0.35)'; ctx.beginPath(); ctx.ellipse(x, y + r, r * 0.8, r * 0.3, 0, 0, 7); ctx.fill();
  }

  _drawPlayer(ctx, p) {
    if (this.charImg && this.charImg._ready) {
      const s = 56;
      ctx.save(); if (p.hurt > 0 && Math.floor(p.hurt * 12) % 2) ctx.globalAlpha = 0.4;
      ctx.drawImage(this.charImg, p.x - s / 2, p.y - s / 2, s, s);
      ctx.restore();
    } else {
      const spr = this.heroFrames.up[0];
      const sc = 2.2;
      ctx.save(); if (p.hurt > 0 && Math.floor(p.hurt * 12) % 2) ctx.globalAlpha = 0.4;
      ctx.drawImage(spr, p.x - (spr.width * sc) / 2, p.y - (spr.height * sc) / 2, spr.width * sc, spr.height * sc);
      ctx.restore();
    }
    ctx.fillStyle = 'rgba(0,0,0,0.35)'; ctx.beginPath(); ctx.ellipse(p.x, p.y + 16, 14, 5, 0, 0, 7); ctx.fill();
  }
}

// --- helpers ---------------------------------------------------------------
function clamp(v, a, b) { return Math.min(b, Math.max(a, v)); }
function dist(ax, ay, bx, by) { return Math.hypot(ax - bx, ay - by); }
function glow(ctx, x, y, r, color) {
  const g = ctx.createRadialGradient(x, y, 0, x, y, r * 2.2);
  g.addColorStop(0, color); g.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, r * 2.2, 0, 7); ctx.fill();
  ctx.fillStyle = '#fff'; ctx.fillRect(x - 1, y - 1, 2, 2);
}
function bar(ctx, x, y, w, h, ratio, color, label) {
  ctx.fillStyle = 'rgba(8,6,14,0.8)'; ctx.fillRect(x - 2, y - 2, w + 4, h + 4);
  ctx.fillStyle = '#1a1014'; ctx.fillRect(x, y, w, h);
  ctx.fillStyle = color; ctx.fillRect(x, y, Math.max(0, w * ratio), h);
  ctx.fillStyle = '#eee'; ctx.font = 'bold 11px monospace'; ctx.textAlign = 'center';
  ctx.fillText(label, x + w / 2, y + h - 3); ctx.textAlign = 'left';
}
