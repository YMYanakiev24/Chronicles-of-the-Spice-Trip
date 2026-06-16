/**
 * NPC.js
 * ----------------------------------------------------------------------------
 * A 2D friendly character: idles in the world, shows a floating quest marker
 * (! available, ? in-progress, ✔ ready-to-turn-in) and opens dialogue when the
 * player steps close and presses E.
 * ----------------------------------------------------------------------------
 */

import { npc } from '../world/PixelArt.js';
import { dist } from '../core/Utils.js';

export class NPC {
  constructor(game, def) {
    this.game = game;
    this.def = def;
    this.frames = npc(def.color);
    this.x = def.tx * 16 + 8;
    this.y = def.ty * 16 + 8;
    this.facing = 'down';
    this.bob = Math.random() * 7;
    this.talkedTo = false;
  }

  get baseline() { return this.y; }
  distanceTo(px, py) { return dist(this.x, this.y, px, py); }
  interact() { this.talkedTo = true; this.game.startDialogue(this); }

  update(dt, time) {
    // Face the player when near.
    const p = this.game.player;
    if (dist(this.x, this.y, p.x, p.y) < 40) {
      const dx = p.x - this.x, dy = p.y - this.y;
      this.facing = Math.abs(dx) > Math.abs(dy) ? (dx < 0 ? 'left' : 'right') : (dy < 0 ? 'up' : 'down');
    }
  }

  draw(ctx, camX, camY) {
    const bob = Math.sin(this.bob + performance.now() * 0.002) * 1;
    const spr = this.frames[this.facing][0];
    const sx = Math.round(this.x - spr.width / 2 - camX);
    const sy = Math.round(this.y - spr.height + 3 - camY + bob);
    ctx.drawImage(spr, sx, sy);

    // Quest marker
    const glyph = this.game.quests.markerFor(this.def.id);
    if (glyph) {
      const my = sy - 8 + Math.sin(performance.now() * 0.004 + this.bob) * 1.5;
      const col = glyph === '!' ? '#ffd24a' : glyph === '✔' ? '#7fe06b' : '#9fd0ff';
      ctx.font = 'bold 11px monospace'; ctx.textAlign = 'center';
      ctx.fillStyle = 'rgba(0,0,0,0.7)'; ctx.fillText(glyph, this.x - camX + 1, my + 1);
      ctx.fillStyle = col; ctx.fillText(glyph, this.x - camX, my);
      ctx.textAlign = 'left';
    }
  }
}
