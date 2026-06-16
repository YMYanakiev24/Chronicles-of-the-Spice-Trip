/**
 * IntroScene.js
 * ----------------------------------------------------------------------------
 * The cinematic opening, painted as a 2D pixel scene directly into the game's
 * virtual buffer: a moonlit, fog-drowned valley with the Pale Citadel burning
 * cold light on a black hill, a dark forest silhouette in the foreground, and
 * drifting embers. The DOM title card overlays the title and the prophecy.
 *
 * It loops behind the main menu too, so the menu has a living backdrop.
 * Skippable on any click/key (handled by Game).
 * ----------------------------------------------------------------------------
 */

import { makeRNG } from '../core/Utils.js';

export class IntroScene {
  constructor(game) {
    this.game = game;
    this.t = 0;
    this.done = false;
    this._built = false;
  }

  start() {
    this.t = 0;
    this.done = false;
    this.game.ui.showTitle(true);
  }

  _build(w, h) {
    const rng = makeRNG(20260614);
    // Stars
    this.stars = [];
    for (let i = 0; i < 90; i++) this.stars.push({ x: rng() * w, y: rng() * h * 0.55, b: rng(), tw: rng() * 7 });
    // Citadel windows (relative to the keep), flicker phases
    this.windows = [];
    for (let i = 0; i < 26; i++) this.windows.push({ x: rng(), y: rng(), p: rng() * 7 });
    // Foreground tree silhouette heights (a jagged black canopy)
    this.canopy = [];
    for (let x = -2; x <= w + 2; x += 4) this.canopy.push(h - 26 - rng() * 26 - (Math.sin(x * 0.05) + 1) * 8);
    // A couple of framing dead trees
    this.frameTrees = [{ x: w * 0.12, s: 1 }, { x: w * 0.88, s: 1.1 }];
    // Embers
    this.embers = [];
    for (let i = 0; i < 40; i++) this.embers.push({ x: rng() * w, y: rng() * h, sp: 4 + rng() * 10, ph: rng() * 7 });
    this._built = true;
  }

  update(dt) {
    if (this.done) return;
    this.t += dt;
    if (this.t > 26) this.finish();   // fallback auto-advance
  }

  finish() {
    if (this.done) return;
    this.done = true;
    this.game.ui.showTitle(false);
    this.game.onIntroComplete();
  }

  draw(ctx) {
    const w = ctx.canvas.width, h = ctx.canvas.height;
    if (!this._built) this._build(w, h);
    const t = this.t;

    // Sky gradient
    const sky = ctx.createLinearGradient(0, 0, 0, h);
    sky.addColorStop(0, '#070611');
    sky.addColorStop(0.55, '#120e26');
    sky.addColorStop(1, '#1a1430');
    ctx.fillStyle = sky; ctx.fillRect(0, 0, w, h);

    // Stars
    for (const s of this.stars) {
      const a = 0.3 + Math.abs(Math.sin(t * 0.7 + s.tw)) * 0.6 * s.b;
      ctx.fillStyle = `rgba(200,210,255,${a})`;
      ctx.fillRect(s.x | 0, s.y | 0, 1, 1);
    }

    // Moon with glow
    const mx = w * 0.74, my = h * 0.2, mr = 16;
    const mg = ctx.createRadialGradient(mx, my, 0, mx, my, mr * 4);
    mg.addColorStop(0, 'rgba(210,220,255,0.5)'); mg.addColorStop(1, 'rgba(210,220,255,0)');
    ctx.fillStyle = mg; ctx.beginPath(); ctx.arc(mx, my, mr * 4, 0, 7); ctx.fill();
    ctx.fillStyle = '#dfe6ff'; ctx.beginPath(); ctx.arc(mx, my, mr, 0, 7); ctx.fill();
    ctx.fillStyle = '#c3cbe8'; // craters
    ctx.beginPath(); ctx.arc(mx - 5, my - 3, 3, 0, 7); ctx.arc(mx + 4, my + 4, 2, 0, 7); ctx.fill();

    // Distant hill that rises toward the right, crowned by the citadel.
    const hy = h * 0.6;
    ctx.fillStyle = '#0c0a16';
    ctx.beginPath();
    ctx.moveTo(0, h);
    ctx.lineTo(0, hy + 30);
    ctx.quadraticCurveTo(w * 0.55, hy + 6, w * 0.74, hy - 46);
    ctx.quadraticCurveTo(w * 0.9, hy - 10, w, hy + 6);
    ctx.lineTo(w, h); ctx.closePath(); ctx.fill();

    // The Pale Citadel on the hill (upper-right, where the menu hotspot sits)
    this._drawCitadel(ctx, w * 0.74, hy - 36, t);

    // Drifting fog bands over the valley
    ctx.globalCompositeOperation = 'screen';
    for (let i = 0; i < 4; i++) {
      const fy = hy + 6 + i * 12;
      const off = (t * (6 + i * 3)) % (w + 80) - 40;
      ctx.fillStyle = `rgba(150,165,185,${0.05 + i * 0.015})`;
      for (let x = -40; x < w; x += 60) {
        const fx = x + off + Math.sin(t * 0.5 + i + x) * 8;
        ctx.beginPath(); ctx.ellipse(fx, fy, 40, 6, 0, 0, 7); ctx.fill();
      }
    }
    ctx.globalCompositeOperation = 'source-over';

    // Foreground forest canopy (solid black) + framing trees
    ctx.fillStyle = '#040308';
    ctx.beginPath(); ctx.moveTo(0, h);
    for (let i = 0; i < this.canopy.length; i++) ctx.lineTo(i * 4 - 2, this.canopy[i]);
    ctx.lineTo(w, h); ctx.closePath(); ctx.fill();
    for (const ft of this.frameTrees) this._drawDeadTree(ctx, ft.x, h, ft.s);

    // Rising embers / fireflies
    ctx.globalCompositeOperation = 'lighter';
    for (const e of this.embers) {
      const ey = (e.y - t * e.sp) % h; const yy = ey < 0 ? ey + h : ey;
      const xx = e.x + Math.sin(t * 0.8 + e.ph) * 6;
      const a = 0.3 + Math.abs(Math.sin(t * 2 + e.ph)) * 0.5;
      ctx.fillStyle = `rgba(255,190,120,${a})`;
      ctx.fillRect(xx | 0, yy | 0, 1, 1);
    }
    ctx.globalCompositeOperation = 'source-over';

    // Vignette
    const vg = ctx.createRadialGradient(w / 2, h / 2, h * 0.3, w / 2, h / 2, h * 0.8);
    vg.addColorStop(0, 'rgba(0,0,0,0)'); vg.addColorStop(1, 'rgba(0,0,0,0.7)');
    ctx.fillStyle = vg; ctx.fillRect(0, 0, w, h);
  }

  _drawCitadel(ctx, cx, base, t) {
    const lt = '#262a3c', dk = '#10111c';
    // central keep
    ctx.fillStyle = lt; ctx.fillRect(cx - 16, base - 46, 32, 46);
    ctx.fillStyle = dk; ctx.fillRect(cx - 16, base - 46, 4, 46);
    // side towers
    ctx.fillStyle = lt;
    ctx.fillRect(cx - 30, base - 34, 12, 34);
    ctx.fillRect(cx + 18, base - 34, 12, 34);
    // battlements
    for (let x = -16; x < 16; x += 6) ctx.fillRect(cx + x, base - 50, 4, 5);
    // spires
    ctx.fillStyle = dk;
    this._tri(ctx, cx - 24, base - 34, 12, 10);
    this._tri(ctx, cx + 24, base - 34, 12, 10);
    this._tri(ctx, cx, base - 50, 22, 14);
    // glowing windows (flicker)
    for (const wn of this.windows) {
      const wx = cx - 26 + wn.x * 52;
      const wy = base - 44 + wn.y * 40;
      const fl = 0.55 + Math.sin(t * 4 + wn.p) * 0.35;
      ctx.fillStyle = `rgba(180,200,255,${fl})`;
      ctx.fillRect(wx | 0, wy | 0, 2, 3);
    }
    // overall cold glow
    ctx.globalCompositeOperation = 'lighter';
    const g = ctx.createRadialGradient(cx, base - 30, 0, cx, base - 30, 60);
    g.addColorStop(0, 'rgba(120,150,255,0.12)'); g.addColorStop(1, 'rgba(120,150,255,0)');
    ctx.fillStyle = g; ctx.fillRect(cx - 60, base - 90, 120, 120);
    ctx.globalCompositeOperation = 'source-over';
  }

  _drawDeadTree(ctx, x, base, s) {
    ctx.fillStyle = '#040308';
    ctx.fillRect(x - 2 * s, base - 60 * s, 4 * s, 60 * s);
    // branches
    ctx.fillRect(x - 12 * s, base - 50 * s, 12 * s, 3 * s);
    ctx.fillRect(x + 2 * s, base - 44 * s, 12 * s, 3 * s);
    ctx.fillRect(x - 14 * s, base - 56 * s, 3 * s, 8 * s);
    ctx.fillRect(x + 12 * s, base - 50 * s, 3 * s, 8 * s);
  }

  _tri(ctx, cx, y, w, h) {
    ctx.beginPath(); ctx.moveTo(cx - w / 2, y); ctx.lineTo(cx + w / 2, y); ctx.lineTo(cx, y - h); ctx.closePath(); ctx.fill();
  }
}
