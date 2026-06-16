/**
 * UI.js
 * ----------------------------------------------------------------------------
 * The entire interface, styled (via style.css) like an ancient illuminated
 * spellbook. One manager owns:
 *   - the persistent HUD (health/mana/XP bars, spell hotbar, quest tracker,
 *     minimap, gold, crosshair, interaction prompt, toasts, damage numbers,
 *     banners, screen shake & damage vignette), and
 *   - the modal panels (main menu, pause, settings, inventory, skill book +
 *     bestiary, realm map, dialogue, loading & death screens, tutorial hints).
 *
 * Gameplay code never touches the DOM directly — it calls methods here.
 * ----------------------------------------------------------------------------
 */

import {
  SPELLS, SPELL_ORDER, ITEMS, CREATURES, REGIONS, REGION_ORDER,
  QUESTS, QUEST_ORDER, GODS, TIPS, PROPHECY, xpForLevel,
} from '../data/GameData.js';

const SCHOOL_ICON = {
  Fire: '🔥', Water: '🌊', Lightning: '⚡', Life: '🌿',
  Ward: '🛡️', Divine: '☀️',
};

export class UI {
  constructor(game) {
    this.game = game;
    this.root = document.getElementById('ui');
    this.activePanel = null;     // 'inventory' | 'skills' | 'map' | 'pause' | 'settings' | 'dialogue'
    this.toasts = [];
    this.damageNumbers = [];
    this.bestiarySeen = new Set();
    this.shakeAmount = 0;
    this.bannerTimer = 0;
    this.hintTimer = 0;
    this._dialogue = null;

    this.canvasWrap = document.getElementById('canvas-wrap');

    this._buildHUD();
    this._buildOverlays();
  }

  // =========================================================================
  // HUD construction
  // =========================================================================
  _buildHUD() {
    const hud = document.createElement('div');
    hud.id = 'hud';
    hud.className = 'hud hidden';
    hud.innerHTML = `
      <div id="crosshair"></div>

      <!-- Bottom-left vitals -->
      <div class="vitals">
        <div class="portrait"><span id="level-badge">1</span></div>
        <div class="bars">
          <div class="bar-row"><div class="bar health"><div class="fill" id="hp-fill"></div><span class="bar-label" id="hp-label">100/100</span></div></div>
          <div class="bar-row"><div class="bar mana"><div class="fill" id="mp-fill"></div><span class="bar-label" id="mp-label">100/100</span></div></div>
          <div class="bar-row"><div class="bar xp"><div class="fill" id="xp-fill"></div></div></div>
          <div class="shield-pip hidden" id="shield-pip">🛡 <span id="shield-val">0</span></div>
        </div>
      </div>

      <!-- Spell hotbar -->
      <div class="hotbar" id="hotbar"></div>

      <!-- Gold + clock -->
      <div class="topleft">
        <div class="gold-pill">🪙 <span id="gold-val">0</span></div>
        <div class="region-pill" id="region-pill">The Whispering Glade</div>
      </div>

      <!-- Quest tracker -->
      <div class="quest-tracker" id="quest-tracker"></div>

      <!-- Minimap -->
      <div class="minimap-frame">
        <canvas id="minimap" width="170" height="170"></canvas>
        <div class="minimap-compass">N</div>
      </div>

      <!-- Interaction prompt -->
      <div class="interact-prompt hidden" id="interact-prompt"></div>

      <!-- Tutorial hint -->
      <div class="hint-bar hidden" id="hint-bar"></div>

      <!-- Toasts -->
      <div class="toast-stack" id="toast-stack"></div>

      <!-- Banner -->
      <div class="banner hidden" id="banner"></div>

      <!-- Damage vignette -->
      <div class="damage-vignette" id="damage-vignette"></div>

      <!-- Floating damage numbers -->
      <div class="dmg-layer" id="dmg-layer"></div>

      <!-- Controls reminder -->
      <div class="controls-reminder" id="controls-reminder">
        <b>1-6</b> Cast &nbsp; <b>WASD</b> Move &nbsp; <b>E</b> Interact &nbsp;
        <b>Tab</b> Journal &nbsp; <b>I</b> Satchel &nbsp; <b>K</b> Spellbook &nbsp;
        <b>M</b> Map &nbsp; <b>Esc</b> Menu
      </div>
    `;
    this.root.appendChild(hud);
    this.hud = hud;
    this.minimap = document.getElementById('minimap');
    this.minimapCtx = this.minimap.getContext('2d');
    this._buildHotbar();
  }

  _buildHotbar() {
    const bar = document.getElementById('hotbar');
    bar.innerHTML = '';
    this.hotbarSlots = [];
    for (let i = 0; i < 6; i++) {
      const slot = document.createElement('div');
      slot.className = 'spell-slot empty';
      slot.innerHTML = `<span class="key">${i + 1}</span><span class="icon"></span><div class="cooldown"></div>`;
      slot.addEventListener('click', () => this.game.player.tryCastSlot(i));
      bar.appendChild(slot);
      this.hotbarSlots.push(slot);
    }
  }

  // =========================================================================
  // Overlays (panels + screens)
  // =========================================================================
  _buildOverlays() {
    const ov = document.createElement('div');
    ov.id = 'overlays';
    ov.innerHTML = `
      <!-- Title card (intro) -->
      <div class="title-card hidden" id="title-card">
        <h1 class="game-title">The Mythic Chronicles</h1>
        <h2 class="game-subtitle">The Hidden World</h2>
        <div class="prophecy">${PROPHECY.map((l) => `<p>${l}</p>`).join('')}</div>
        <p class="title-hint">click to begin</p>
      </div>

      <!-- Loading -->
      <div class="screen" id="loading-screen">
        <div class="book-emblem">❖</div>
        <h1 class="game-title small">The Mythic Chronicles</h1>
        <div class="loading-bar"><div class="loading-fill" id="loading-fill"></div></div>
        <p class="tip" id="loading-tip"></p>
      </div>

      <!-- Main menu -->
      <div class="screen hidden" id="main-menu">
        <h1 class="game-title">The Mythic Chronicles</h1>
        <h2 class="game-subtitle">The Hidden World</h2>
        <div class="menu-buttons" id="main-menu-buttons"></div>
        <p class="footer-note">An original fantasy realm · forge your legend</p>
      </div>

      <!-- Modal panel host (book) -->
      <div class="book-modal hidden" id="book-modal">
        <div class="book">
          <div class="book-tabs" id="book-tabs"></div>
          <div class="book-page left" id="page-left"></div>
          <div class="book-spine"></div>
          <div class="book-page right" id="page-right"></div>
          <button class="book-close" id="book-close">✕</button>
        </div>
      </div>

      <!-- Dialogue -->
      <div class="dialogue hidden" id="dialogue">
        <div class="dialogue-box">
          <div class="speaker" id="dlg-speaker"></div>
          <div class="dlg-text" id="dlg-text"></div>
          <div class="dlg-choices" id="dlg-choices"></div>
        </div>
      </div>

      <!-- Death -->
      <div class="screen hidden death" id="death-screen">
        <h1 class="death-title">You Have Fallen</h1>
        <p class="death-sub">But the hidden world is not done with you...</p>
        <div class="menu-buttons">
          <button class="menu-btn" id="respawn-btn">Return to the last shrine</button>
        </div>
      </div>
    `;
    this.root.appendChild(ov);

    document.getElementById('book-close').addEventListener('click', () => this.closePanel());
    document.getElementById('respawn-btn').addEventListener('click', () => this.game.respawnPlayer());

    // Fill loading tip.
    document.getElementById('loading-tip').textContent = TIPS[Math.floor(Math.random() * TIPS.length)];
  }

  // --- Loading / main menu --------------------------------------------------
  setLoadingProgress(p) {
    document.getElementById('loading-fill').style.width = `${Math.floor(p * 100)}%`;
  }
  hideLoading() { document.getElementById('loading-screen').classList.add('hidden'); }

  showMainMenu(hasSave) {
    const menu = document.getElementById('main-menu');
    const btns = document.getElementById('main-menu-buttons');
    btns.innerHTML = '';
    const add = (label, fn, cls = '') => {
      const b = document.createElement('button');
      b.className = 'menu-btn ' + cls;
      b.textContent = label;
      b.addEventListener('click', () => { this.game.audio.sfx('ui'); fn(); });
      btns.appendChild(b);
    };
    // Play leads to the Path — the boss map (same as clicking the castle).
    add('▶ Play', () => this.game.scenes.enterPath(), 'primary');
    menu.classList.remove('hidden');
    this.game.audio.playMusic('menu');
  }
  hideMainMenu() { document.getElementById('main-menu').classList.add('hidden'); }

  showTitle(show) {
    document.getElementById('title-card').classList.toggle('hidden', !show);
  }

  // =========================================================================
  // HUD refresh
  // =========================================================================
  showHUD(show) { this.hud.classList.toggle('hidden', !show); if (show) this.refreshHUD(); }

  refreshHUDSmooth() {
    const p = this.game.player; if (!p) return;
    document.getElementById('hp-fill').style.width = `${(p.hp / p.maxHp) * 100}%`;
    document.getElementById('mp-fill').style.width = `${(p.mana / p.maxMana) * 100}%`;
    document.getElementById('xp-fill').style.width = `${(p.xp / xpForLevel(p.level)) * 100}%`;
    // Cooldown sweeps.
    for (let i = 0; i < 6; i++) {
      const id = p.hotbar[i];
      const cd = this.hotbarSlots[i].querySelector('.cooldown');
      if (id && p.cooldowns[id] > 0) {
        const ratio = p.cooldowns[id] / SPELLS[id].cooldown;
        cd.style.height = `${ratio * 100}%`;
      } else cd.style.height = '0%';
    }
    // Shield pip.
    const pip = document.getElementById('shield-pip');
    if (p.shield > 0) { pip.classList.remove('hidden'); document.getElementById('shield-val').textContent = Math.ceil(p.shield); }
    else pip.classList.add('hidden');
  }

  refreshHUD() {
    const p = this.game.player; if (!p) return;
    this.refreshHUDSmooth();
    document.getElementById('hp-label').textContent = `${Math.ceil(p.hp)}/${p.maxHp}`;
    document.getElementById('mp-label').textContent = `${Math.ceil(p.mana)}/${p.maxMana}`;
    document.getElementById('level-badge').textContent = p.level;
    document.getElementById('gold-val').textContent = p.gold;
    // Hotbar icons.
    for (let i = 0; i < 6; i++) {
      const id = p.hotbar[i];
      const slot = this.hotbarSlots[i];
      const icon = slot.querySelector('.icon');
      if (id) {
        slot.classList.remove('empty');
        icon.textContent = SCHOOL_ICON[SPELLS[id].school] || '✦';
        slot.title = SPELLS[id].name;
      } else { slot.classList.add('empty'); icon.textContent = ''; slot.title = ''; }
    }
    this.refreshQuestTracker();
  }

  setRegionName(name) { document.getElementById('region-pill').textContent = name; }

  refreshQuestTracker() {
    const t = document.getElementById('quest-tracker');
    const q = this.game.quests.trackedQuest();
    if (!q) { t.innerHTML = '<div class="qt-title">No active quest</div>'; return; }
    const objs = q.objectives.map((o) =>
      `<li class="${o.done ? 'done' : ''}">${o.done ? '✔' : '◇'} ${o.text}</li>`).join('');
    const stateTag = q.state === 'ready' ? '<span class="ready-tag">READY</span>' : '';
    t.innerHTML = `<div class="qt-title">❖ ${q.def.title} ${stateTag}</div><ul>${objs}</ul>`;
  }

  // =========================================================================
  // Toasts / banners / numbers / shake
  // =========================================================================
  toast(msg, type = 'info', dur = 2.4) {
    const stack = document.getElementById('toast-stack');
    const el = document.createElement('div');
    el.className = `toast ${type}`;
    el.textContent = msg;
    stack.appendChild(el);
    this.toasts.push({ el, life: dur });
    // Cap to avoid clutter.
    while (this.toasts.length > 5) { const old = this.toasts.shift(); old.el.remove(); }
  }

  bigBanner(text) {
    const b = document.getElementById('banner');
    b.textContent = text;
    b.classList.remove('hidden');
    b.classList.remove('banner-anim');
    void b.offsetWidth;           // restart CSS animation
    b.classList.add('banner-anim');
    this.bannerTimer = 2.2;
  }

  damageFlash() {
    const v = document.getElementById('damage-vignette');
    v.classList.remove('flash'); void v.offsetWidth; v.classList.add('flash');
  }

  shake(amount) { this.shakeAmount = Math.min(1.2, this.shakeAmount + amount); }

  // World-space (wx, wy) in map pixels; reprojected to screen each frame.
  spawnDamageNumber(wx, wy, amount, crit) {
    const el = document.createElement('div');
    el.className = 'dmg-number' + (crit ? ' crit' : '');
    el.textContent = crit ? `${amount}!` : amount;
    document.getElementById('dmg-layer').appendChild(el);
    this.damageNumbers.push({
      el, wx, wy, life: 1.0, vy: 26, offX: (Math.random() - 0.5) * 14,
    });
  }

  recordBestiary(id) { this.bestiarySeen.add(id); }

  // Interaction prompt (e.g. "Press E to speak with Elare").
  showInteract(text) {
    const p = document.getElementById('interact-prompt');
    p.innerHTML = text; p.classList.remove('hidden');
  }
  hideInteract() { document.getElementById('interact-prompt').classList.add('hidden'); }

  showHint(text, dur = 6) {
    const h = document.getElementById('hint-bar');
    h.innerHTML = `✦ ${text}`;
    h.classList.remove('hidden');
    this.hintTimer = dur;
  }

  // =========================================================================
  // Per-frame UI update (called from the game loop)
  // =========================================================================
  update(dt) {
    // Toast lifetimes.
    for (let i = this.toasts.length - 1; i >= 0; i--) {
      const t = this.toasts[i];
      t.life -= dt;
      if (t.life < 0.5) t.el.style.opacity = Math.max(0, t.life / 0.5);
      if (t.life <= 0) { t.el.remove(); this.toasts.splice(i, 1); }
    }

    // Banner.
    if (this.bannerTimer > 0) {
      this.bannerTimer -= dt;
      if (this.bannerTimer <= 0) document.getElementById('banner').classList.add('hidden');
    }
    // Hint.
    if (this.hintTimer > 0) {
      this.hintTimer -= dt;
      if (this.hintTimer <= 0) document.getElementById('hint-bar').classList.add('hidden');
    }

    // Floating damage numbers — reproject world→screen each frame.
    for (let i = this.damageNumbers.length - 1; i >= 0; i--) {
      const d = this.damageNumbers[i];
      d.life -= dt; d.wy -= d.vy * dt;
      const s = this.game.worldToScreen(d.wx, d.wy);
      d.el.style.left = `${s.x + d.offX}px`;
      d.el.style.top = `${s.y}px`;
      d.el.style.opacity = Math.max(0, d.life);
      if (d.life <= 0) { d.el.remove(); this.damageNumbers.splice(i, 1); }
    }

    // Screen shake (CSS transform on the canvas).
    if (this.shakeAmount > 0) {
      this.shakeAmount = Math.max(0, this.shakeAmount - dt * 2);
      const s = this.shakeAmount * 12;
      this.canvasWrap.style.transform =
        `translate(${(Math.random() - 0.5) * s}px, ${(Math.random() - 0.5) * s}px)`;
    } else if (this.canvasWrap.style.transform) {
      this.canvasWrap.style.transform = '';
    }

    if (!this.hud.classList.contains('hidden')) this._drawMinimap();
  }

  // Top-down minimap of the current tilemap centered on the player.
  _drawMinimap() {
    const ctx = this.minimapCtx;
    const W = 170, H = 170, R = 85;
    const map = this.game.map, p = this.game.player;
    ctx.clearRect(0, 0, W, H);
    ctx.save();
    ctx.beginPath(); ctx.arc(R, R, R - 2, 0, Math.PI * 2); ctx.clip();
    ctx.fillStyle = 'rgba(8,8,16,0.9)'; ctx.fillRect(0, 0, W, H);
    if (!p || !map.region) { ctx.restore(); return; }

    const tilePx = 5;           // minimap pixels per tile
    const scale = tilePx / 16;  // world px → minimap px
    const toMap = (wx, wy) => ({ x: R + (wx - p.x) * scale, y: R + (wy - p.y) * scale });

    // Walls in a window around the player.
    const ptx = Math.floor(p.x / 16), pty = Math.floor(p.y / 16), rad = 18;
    for (let ty = pty - rad; ty <= pty + rad; ty++) {
      for (let tx = ptx - rad; tx <= ptx + rad; tx++) {
        if (tx < 0 || ty < 0 || tx >= map.w || ty >= map.h) continue;
        const c = map.cells[ty * map.w + tx];
        const m = toMap(tx * 16 + 8, ty * 16 + 8);
        if (c.wall) { ctx.fillStyle = 'rgba(120,120,150,0.5)'; ctx.fillRect(m.x - 2, m.y - 2, tilePx, tilePx); }
        else { ctx.fillStyle = 'rgba(40,46,60,0.45)'; ctx.fillRect(m.x - 2, m.y - 2, tilePx, tilePx); }
      }
    }

    const dot = (x, y, col, r = 2.5) => { ctx.fillStyle = col; ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill(); };
    for (const pk of map.pickups) { const m = toMap(pk.x, pk.y); dot(m.x, m.y, pk.type === 'asphodel' ? '#cfe0ff' : '#9fe0ff', 1.6); }
    for (const npc of this.game.npcs) { const m = toMap(npc.x, npc.y); dot(m.x, m.y, '#ffd24a', 2.6); }
    for (const e of this.game.enemies) { if (e.dead) continue; const m = toMap(e.x, e.y); dot(m.x, m.y, '#ff5a4a', 2.2); }

    // Player marker + facing tick.
    dot(R, R, '#eafff0', 3);
    const f = { up: [0, -1], down: [0, 1], left: [-1, 0], right: [1, 0] }[p.facing] || [0, 1];
    ctx.strokeStyle = '#eafff0'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(R, R); ctx.lineTo(R + f[0] * 7, R + f[1] * 7); ctx.stroke();
    ctx.restore();

    ctx.strokeStyle = 'rgba(150,130,90,0.7)'; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(R, R, R - 2, 0, Math.PI * 2); ctx.stroke();
  }

  // =========================================================================
  // Book modal panels: inventory / skills / map / settings / controls
  // =========================================================================
  isBlocking() { return this.activePanel !== null || !document.getElementById('dialogue').classList.contains('hidden'); }

  _openBook(tabs, activeTab) {
    const modal = document.getElementById('book-modal');
    const tabBar = document.getElementById('book-tabs');
    tabBar.innerHTML = '';
    for (const t of tabs) {
      const b = document.createElement('button');
      b.className = 'book-tab' + (t.id === activeTab ? ' active' : '');
      b.textContent = t.label;
      b.addEventListener('click', () => { this.game.audio.sfx('ui'); t.open(); });
      tabBar.appendChild(b);
    }
    modal.classList.remove('hidden');
    // Restart the fade-in reliably each open (reflow trick).
    modal.classList.remove('show'); void modal.offsetWidth; modal.classList.add('show');
    this.game.audio.sfx('ui');
  }

  _bookTabs() {
    return [
      { id: 'inventory', label: '📜 Satchel', open: () => this.openInventory() },
      { id: 'skills', label: '✦ Spellbook', open: () => this.openSkills() },
      { id: 'bestiary', label: '🐾 Bestiary', open: () => this.openBestiary() },
      { id: 'map', label: '🗺 World Map', open: () => this.openMap() },
      { id: 'quests', label: '❖ Journal', open: () => this.openJournal() },
      { id: 'settings', label: '⚙ Settings', open: () => this.openSettings(true) },
    ];
  }

  closePanel() {
    document.getElementById('book-modal').classList.add('hidden');
    document.getElementById('main-menu').classList.add('hidden');
    this.activePanel = null;
    this.game.audio.sfx('ui');
    this.game.onPanelClosed();
  }

  // Toggle helper so the same key opens & closes a panel.
  togglePanel(name) {
    if (this.activePanel === name) { this.closePanel(); return; }
    switch (name) {
      case 'inventory': this.openInventory(); break;
      case 'skills': this.openSkills(); break;
      case 'map': this.openMap(); break;
      case 'quests': this.openJournal(); break;
    }
  }

  openInventory() {
    this.activePanel = 'inventory';
    this._openBook(this._bookTabs(), 'inventory');
    const p = this.game.player;
    const left = document.getElementById('page-left');
    const right = document.getElementById('page-right');
    left.innerHTML = `<h2 class="page-title">Satchel</h2>
      <p class="page-flourish">Carried treasures & tonics</p>`;
    const entries = Object.keys(p.inventory).filter((id) => p.inventory[id] > 0);
    if (!entries.length) {
      left.innerHTML += `<p class="empty-note">Your satchel is empty. The Realm is full of things to find.</p>`;
    } else {
      const grid = entries.map((id) => {
        const it = ITEMS[id];
        const usable = it.type === 'consumable';
        return `<div class="inv-item ${usable ? 'usable' : ''}" data-item="${id}">
          <span class="inv-icon">${it.icon}</span>
          <span class="inv-name">${it.name}</span>
          <span class="inv-count">×${p.inventory[id]}</span>
          ${usable ? '<span class="inv-use">use ▸</span>' : ''}
        </div>`;
      }).join('');
      left.innerHTML += `<div class="inv-grid">${grid}</div>`;
    }
    // Relics + character sheet on the right page.
    const relics = [...p.relics].map((id) => `<li>${ITEMS[id].icon} ${ITEMS[id].name} — <i>${ITEMS[id].desc}</i></li>`).join('') || '<li class="empty-note">No relics yet.</li>';
    right.innerHTML = `<h2 class="page-title">The Unmarked</h2>
      <div class="char-sheet">
        <div class="char-row"><span>Level</span><b>${p.level}</b></div>
        <div class="char-row"><span>Health</span><b>${Math.ceil(p.hp)} / ${p.maxHp}</b></div>
        <div class="char-row"><span>Mana</span><b>${Math.ceil(p.mana)} / ${p.maxMana}</b></div>
        <div class="char-row"><span>Mana Regen</span><b>${p.manaRegen.toFixed(1)}/s</b></div>
        <div class="char-row"><span>Gold</span><b>🪙 ${p.gold}</b></div>
        <div class="char-row"><span>Spells Known</span><b>${p.learnedSpells.size}</b></div>
      </div>
      <h3 class="sub-title">Relics</h3><ul class="relic-list">${relics}</ul>`;
    // Wire "use" clicks.
    left.querySelectorAll('.inv-item.usable').forEach((el) => {
      el.addEventListener('click', () => { this.game.useItem(el.dataset.item); this.openInventory(); });
    });
  }

  openSkills() {
    this.activePanel = 'skills';
    this._openBook(this._bookTabs(), 'skills');
    const p = this.game.player;
    const left = document.getElementById('page-left');
    const right = document.getElementById('page-right');
    left.innerHTML = `<h2 class="page-title">Spellbook</h2><p class="page-flourish">Six schools of the arcane</p>`;
    const list = SPELL_ORDER.map((id) => {
      const s = SPELLS[id];
      const known = p.learnedSpells.has(id);
      const lvl = p.spellLevels[id] || 0;
      const locked = !known;
      const slotIdx = p.hotbar.indexOf(id);
      return `<div class="spell-entry ${locked ? 'locked' : ''}" data-spell="${id}">
        <span class="spell-school" style="--c:${s.color}">${SCHOOL_ICON[s.school]}</span>
        <div class="spell-meta">
          <div class="spell-name">${s.name} ${known ? `<span class="tier">Tier ${lvl}</span>` : `<span class="lock">Lv.${s.unlockLevel}</span>`}</div>
          <div class="spell-school-name">${s.school}${slotIdx >= 0 ? ` · slot ${slotIdx + 1}` : ''}</div>
        </div>
      </div>`;
    }).join('');
    left.innerHTML += `<div class="spell-list">${list}</div>`;

    const showDetail = (id) => {
      const s = SPELLS[id];
      const known = p.learnedSpells.has(id);
      const stats = [];
      if (s.damage) stats.push(`Damage: ${s.damage}`);
      if (s.heal) stats.push(`Heal: ${s.heal}`);
      if (s.shield) stats.push(`Shield: ${s.shield}`);
      stats.push(`Mana: ${s.manaCost}`, `Cooldown: ${s.cooldown}s`);
      right.innerHTML = `<h2 class="page-title" style="color:${s.color}">${s.name}</h2>
        <p class="spell-school-name">${s.school} · unlocks at level ${s.unlockLevel}</p>
        <p class="spell-desc">${s.desc}</p>
        <div class="spell-stats">${stats.map((x) => `<span>${x}</span>`).join('')}</div>
        <div class="upgrade-box"><b>Upgrade:</b> ${s.upgrade}</div>
        ${known ? `<p class="hint-line">Known at tier ${p.spellLevels[id]}. Level up or complete quests to upgrade.</p>`
                : `<p class="hint-line locked-line">Reach level ${s.unlockLevel} to learn this spell.</p>`}`;
    };
    left.querySelectorAll('.spell-entry').forEach((el) => {
      el.addEventListener('click', () => { this.game.audio.sfx('ui'); showDetail(el.dataset.spell); });
    });
    showDetail(SPELL_ORDER[0]);
  }

  openBestiary() {
    this.activePanel = 'skills';
    this._openBook(this._bookTabs(), 'bestiary');
    const left = document.getElementById('page-left');
    const right = document.getElementById('page-right');
    left.innerHTML = `<h2 class="page-title">Bestiary</h2><p class="page-flourish">Know thy enemy</p>`;
    const ids = Object.keys(CREATURES);
    const list = ids.map((id) => {
      const c = CREATURES[id];
      const seen = this.bestiarySeen.has(id);
      return `<div class="beast-entry ${seen ? '' : 'unknown'}" data-beast="${id}">
        <span class="beast-dot" style="background:${c.color}"></span>
        <span class="beast-name">${seen ? c.name : '??? ' + (c.boss ? '(Boss)' : '')}</span>
      </div>`;
    }).join('');
    left.innerHTML += `<div class="beast-list">${list}</div>`;
    const show = (id) => {
      const c = CREATURES[id];
      const seen = this.bestiarySeen.has(id);
      right.innerHTML = seen ? `<h2 class="page-title" style="color:${c.color}">${c.name}${c.boss ? ' ⚔' : ''}</h2>
        <div class="spell-stats"><span>HP: ${c.hp}</span><span>Damage: ${c.damage}</span><span>Speed: ${c.speed}</span><span>Weakness: ${c.weakness}</span></div>
        <p class="spell-desc">${c.lore}</p>`
        : `<h2 class="page-title">Unknown Creature</h2><p class="empty-note">You have not yet encountered this being. Slay one to record it here.</p>`;
    };
    left.querySelectorAll('.beast-entry').forEach((el) => el.addEventListener('click', () => { this.game.audio.sfx('ui'); show(el.dataset.beast); }));
    show(ids[0]);
  }

  openJournal() {
    this.activePanel = 'quests';
    this._openBook(this._bookTabs(), 'quests');
    const left = document.getElementById('page-left');
    const right = document.getElementById('page-right');
    left.innerHTML = `<h2 class="page-title">Journal</h2><p class="page-flourish">Deeds & errands</p>`;
    const q = this.game.quests;
    const list = QUEST_ORDER.map((id) => {
      const st = q.state[id];
      const def = QUESTS[id];
      const tag = { available: 'New', active: 'Active', ready: 'Ready', completed: '✔' }[st] || '';
      return `<div class="quest-entry ${st}" data-quest="${id}">
        <span class="quest-tag ${st}">${tag}</span>
        <span class="quest-name">${def.title}</span>
      </div>`;
    }).join('');
    left.innerHTML += `<div class="quest-list">${list}</div>`;
    const show = (id) => {
      const def = QUESTS[id];
      const objs = q.objectiveText(id).map((o) => `<li class="${o.done ? 'done' : ''}">${o.done ? '✔' : '◇'} ${o.text}</li>`).join('');
      const r = def.rewards;
      const rewards = [r.xp ? `${r.xp} XP` : '', r.gold ? `🪙 ${r.gold}` : '', r.spell ? `Spell: ${SPELLS[r.spell].name}` : '',
        ...(r.items || []).map((it) => ITEMS[it].name)].filter(Boolean).join(' · ');
      right.innerHTML = `<h2 class="page-title">${def.title}</h2>
        <p class="quest-summary">${def.summary}</p>
        <p class="spell-desc"><i>${def.lore}</i></p>
        <h3 class="sub-title">Objectives</h3><ul class="obj-list">${objs}</ul>
        <div class="upgrade-box"><b>Rewards:</b> ${rewards}</div>
        ${q.state[id] === 'available' ? `<button class="menu-btn small" id="track-btn">Track this quest</button>` : ''}`;
      const tb = document.getElementById('track-btn');
      if (tb) tb.addEventListener('click', () => { q.tracked = id; this.refreshQuestTracker(); this.toast('Now tracking: ' + def.title, 'gold'); });
    };
    left.querySelectorAll('.quest-entry').forEach((el) => el.addEventListener('click', () => { this.game.audio.sfx('ui'); show(el.dataset.quest); }));
    show(q.tracked || QUEST_ORDER[0]);
  }

  openMap() {
    this.activePanel = 'map';
    this._openBook(this._bookTabs(), 'map');
    const left = document.getElementById('page-left');
    const right = document.getElementById('page-right');
    left.innerHTML = `<h2 class="page-title">Map of the Hidden World</h2><p class="page-flourish">Six realms beyond the Veil</p>`;
    const list = REGION_ORDER.map((id) => {
      const r = REGIONS[id];
      const unlocked = this.game.unlockedRegions.has(id);
      const current = this.game.currentRegion === id;
      return `<div class="region-entry ${unlocked ? '' : 'locked'} ${current ? 'current' : ''}" data-region="${id}">
        <span class="region-orb" style="--c:${r.sky.top}"></span>
        <div class="region-meta">
          <div class="region-name">${unlocked ? r.name : '???'} ${current ? '<span class="here">you are here</span>' : ''}</div>
          <div class="region-sub">${unlocked ? r.subtitle : 'Locked region'}</div>
        </div>
      </div>`;
    }).join('');
    left.innerHTML += `<div class="region-list">${list}</div>`;
    const show = (id) => {
      const r = REGIONS[id];
      const unlocked = this.game.unlockedRegions.has(id);
      const current = this.game.currentRegion === id;
      right.innerHTML = `<h2 class="page-title">${unlocked ? r.name : 'Uncharted'}</h2>
        <p class="region-sub">${unlocked ? r.subtitle : ''}</p>
        <p class="spell-desc">${unlocked ? r.description : 'You have not yet earned passage to this region. Grow in power and complete quests to unlock it.'}</p>
        ${unlocked && !current ? `<button class="menu-btn small" id="travel-btn">Travel here</button>` : ''}
        ${current ? '<p class="hint-line">You are currently exploring this region.</p>' : ''}`;
      const tb = document.getElementById('travel-btn');
      if (tb) tb.addEventListener('click', () => { this.closePanel(); this.game.travelTo(id); });
    };
    left.querySelectorAll('.region-entry:not(.locked)').forEach((el) => el.addEventListener('click', () => { this.game.audio.sfx('ui'); show(el.dataset.region); }));
    show(this.game.currentRegion);
  }

  openSettings(inBook = false) {
    this.activePanel = 'settings';
    const s = this.game.settings;
    const content = `<h2 class="page-title">Settings</h2><p class="page-flourish">Attune the Realm to your liking</p>
      <div class="settings-list">
        <label class="set-row">Music Volume <input type="range" id="set-music" min="0" max="100" value="${s.musicVolume * 100}"></label>
        <label class="set-row">Effects Volume <input type="range" id="set-sfx" min="0" max="100" value="${s.sfxVolume * 100}"></label>
        <label class="set-row">Mouse Sensitivity <input type="range" id="set-sens" min="20" max="200" value="${s.mouseSensitivity * 100}"></label>
        <label class="set-row checkbox">Shadows <input type="checkbox" id="set-shadows" ${s.shadows ? 'checked' : ''}></label>
        <label class="set-row checkbox">Mute All <input type="checkbox" id="set-mute" ${s.muted ? 'checked' : ''}></label>
      </div>
      <div class="settings-buttons">
        <button class="menu-btn small" id="set-save">Save Game</button>
        <button class="menu-btn small danger" id="set-delete">Delete Save</button>
      </div>`;
    if (inBook) {
      this._openBook(this._bookTabs(), 'settings');
      document.getElementById('page-left').innerHTML = content;
      document.getElementById('page-right').innerHTML = `<h2 class="page-title">Controls</h2>${this._controlsHTML()}`;
    } else {
      // From main menu / pause — use the book modal standalone.
      this._openBook([{ id: 'settings', label: '⚙ Settings', open: () => this.openSettings() }], 'settings');
      document.getElementById('page-left').innerHTML = content;
      document.getElementById('page-right').innerHTML = `<h2 class="page-title">Controls & Lore</h2>${this._controlsHTML()}`;
    }
    this._wireSettings();
  }

  _wireSettings() {
    const s = this.game.settings;
    const music = document.getElementById('set-music');
    const sfx = document.getElementById('set-sfx');
    const sens = document.getElementById('set-sens');
    music?.addEventListener('input', () => { s.musicVolume = music.value / 100; this.game.audio.setMusicVolume(s.musicVolume); });
    sfx?.addEventListener('input', () => { s.sfxVolume = sfx.value / 100; this.game.audio.setSfxVolume(s.sfxVolume); });
    sens?.addEventListener('input', () => { s.mouseSensitivity = sens.value / 100; });
    document.getElementById('set-shadows')?.addEventListener('change', (e) => { s.shadows = e.target.checked; this.game.applyQuality(); });
    document.getElementById('set-mute')?.addEventListener('change', (e) => { s.muted = e.target.checked; this.game.audio.setMuted(s.muted); });
    document.getElementById('set-save')?.addEventListener('click', () => { this.game.saveGame(); this.toast('Chronicle saved.', 'gold'); });
    document.getElementById('set-delete')?.addEventListener('click', () => { this.game.save.clear(); this.toast('Save deleted.', 'warn'); });
    const close = () => { this.game.saveSettings(); };
    document.getElementById('book-close').addEventListener('click', close, { once: true });
  }

  _controlsHTML() {
    return `<table class="controls-table">
      <tr><td><b>W A S D</b></td><td>Move</td></tr>
      <tr><td><b>Mouse</b></td><td>Aim</td></tr>
      <tr><td><b>1 – 6</b></td><td>Wield a god's power</td></tr>
      <tr><td><b>Left Click</b></td><td>Swing the godless blade</td></tr>
      <tr><td><b>Right Click</b></td><td>Cast power in slot 1</td></tr>
      <tr><td><b>Shift</b></td><td>Run</td></tr>
      <tr><td><b>Space</b></td><td>Dodge-roll</td></tr>
      <tr><td><b>E</b></td><td>Talk / interact / collect</td></tr>
      <tr><td><b>Tab</b></td><td>Quest journal</td></tr>
      <tr><td><b>I</b></td><td>Satchel (inventory)</td></tr>
      <tr><td><b>K</b></td><td>Tome of powers</td></tr>
      <tr><td><b>M</b></td><td>Map of the hidden world</td></tr>
      <tr><td><b>Esc</b></td><td>Pause / menu</td></tr>
    </table>
    <h3 class="sub-title">The Theurgoi</h3>
    <ul class="god-list">${Object.values(GODS).map((g) => `<li><b style="color:${g.color}">${g.name}</b> — ${g.domain}</li>`).join('')}</ul>`;
  }

  openControls() {
    // Lore + controls reachable from the main menu.
    this.activePanel = 'settings';
    this._openBook([{ id: 'settings', label: '📖 Lore & Controls', open: () => this.openControls() }], 'settings');
    document.getElementById('page-left').innerHTML = `<h2 class="page-title">The Hidden World</h2>
      <p class="spell-desc">You are <b>Unmarked</b> — a demigod whose divine parent has not yet claimed you. The Veil between the mortal world and the old one has thinned around you, and the last Oracle, Theira, found you wandering the cursed Mistwood. The sleeping gods, the <b>Theurgoi</b>, are stirring.</p>
      <p class="spell-desc">Six realms wait beyond the wood: a drowned temple, a city of shades, the caves that descend to the Veiled Queen, a citadel of cold light, and the Threshold of the gods themselves. An ancient prophecy walks with you.</p>
      <h3 class="sub-title">The Asphodel & the Dead</h3>
      <p class="spell-desc"><i>Pale asphodel blooms among the graves of the hidden world — Koravel's mercy to the dead. While it glows, the shades rest. Gather it where the dead lie thick, and the priestess Myrrha will teach you the Green Crown's mending.</i></p>`;
    document.getElementById('page-right').innerHTML = `<h2 class="page-title">Controls</h2>${this._controlsHTML()}`;
  }

  // --- Pause ----------------------------------------------------------------
  openPause() {
    this.activePanel = 'pause';
    this._openBook([{ id: 'pause', label: '⏸ Paused', open: () => {} }], 'pause');
    document.getElementById('book-tabs').innerHTML = '';
    document.getElementById('page-left').innerHTML = `<h2 class="page-title">Paused</h2>
      <p class="page-flourish">The Realm holds its breath</p>
      <div class="menu-buttons pause-buttons">
        <button class="menu-btn" id="pause-resume">Resume</button>
        <button class="menu-btn" id="pause-journal">Quest Journal</button>
        <button class="menu-btn" id="pause-skills">Spellbook</button>
        <button class="menu-btn" id="pause-map">World Map</button>
        <button class="menu-btn" id="pause-settings">Settings</button>
        <button class="menu-btn" id="pause-save">Save Chronicle</button>
        <button class="menu-btn danger" id="pause-quit">Quit to Title</button>
      </div>`;
    document.getElementById('page-right').innerHTML = `<h2 class="page-title">The Unmarked</h2>${this._controlsHTML()}`;
    const b = (id, fn) => document.getElementById(id).addEventListener('click', () => { this.game.audio.sfx('ui'); fn(); });
    b('pause-resume', () => this.closePanel());
    b('pause-journal', () => this.openJournal());
    b('pause-skills', () => this.openSkills());
    b('pause-map', () => this.openMap());
    b('pause-settings', () => this.openSettings(true));
    b('pause-save', () => { this.game.saveGame(); this.toast('Chronicle saved.', 'gold'); });
    b('pause-quit', () => { this.closePanel(); this.game.quitToTitle(); });
  }

  // =========================================================================
  // Dialogue
  // =========================================================================
  showDialogue(npc, opts) {
    const dlg = document.getElementById('dialogue');
    document.getElementById('dlg-speaker').textContent = npc.def.name;
    document.getElementById('dlg-text').innerHTML = opts.text;
    const choices = document.getElementById('dlg-choices');
    choices.innerHTML = '';
    for (const c of opts.choices) {
      const b = document.createElement('button');
      b.className = 'dlg-choice' + (c.primary ? ' primary' : '');
      b.textContent = c.label;
      b.addEventListener('click', () => { this.game.audio.sfx('ui'); c.action(); });
      choices.appendChild(b);
    }
    dlg.classList.remove('hidden');
    // Restart the slide-in reliably each time (reflow trick).
    const box = dlg.querySelector('.dialogue-box');
    box.classList.remove('show'); void box.offsetWidth; box.classList.add('show');
  }
  hideDialogue() { document.getElementById('dialogue').classList.add('hidden'); }

  // --- Death ----------------------------------------------------------------
  showDeath() { document.getElementById('death-screen').classList.remove('hidden'); }
  hideDeath() { document.getElementById('death-screen').classList.add('hidden'); }
}
