/**
 * Scenes.js
 * ----------------------------------------------------------------------------
 * The Citadel hub reachable from the main menu:
 *
 *   main menu → click the castle → CORRIDOR (torch-lit stone hall with two
 *   doors) → click Marin the Wizard's door → a mystical FOG wipe → Marin's
 *   APOTHECARY shop (buy potions/scrolls/relics with gold) and his ENCHANTED
 *   LOOM, where you upload any image, it is perforated into a grid of
 *   rune-shards, and you tear them off piece by piece.
 *
 * Scene backgrounds are drawn as pixel art into small canvases (upscaled crisp
 * by CSS); doors/items/labels are DOM on top so they stay clickable and legible.
 * ----------------------------------------------------------------------------
 */

import { ITEMS } from '../data/GameData.js';
import { flame, prop } from '../world/PixelArt.js';
import { BossBattle } from './BossBattle.js';

const BW = 320, BH = 180;   // scene buffer resolution

// The four bosses on the Path map (positions are fractions of the path image,
// matching where each figure stands so the click hotspots line up).
const BOSSES = [
  { id: 'goodPlaz', name: 'The Good Plazmodium', img: 'assets/plazmodiigood.jpg', hp: 120, color: '#5fd06a', cx: 0.27, cy: 0.72 },
  { id: 'badPlaz', name: 'The Bad Plasmodium', img: 'assets/thebadplasmodium.jpg', hp: 160, color: '#9a5fd0', cx: 0.43, cy: 0.31 },
  { id: 'mordekai', name: 'Mordekai', img: 'assets/mordekai.jpg', hp: 140, color: '#4a7ad0', cx: 0.58, cy: 0.68 },
  { id: 'vlado', name: 'Vlado', img: 'assets/vlado.jpg', hp: 190, color: '#3a5a8a', cx: 0.88, cy: 0.62 },
];

// Selectable champions (the player's fighter). img:null → procedural hero.
// Drop more art into /assets and add rows here later.
const CHARACTERS = [
  { id: 'hero', name: 'Spellwoven', img: null },
  { id: 'atanas', name: 'Atanas', img: 'assets/atanas.jpg' },
  { id: 'gakev', name: 'Gakev', img: 'assets/gakev.jpg' },
  { id: 'nikolay', name: 'Nikolay', img: 'assets/nikolay.jpg' },
  { id: 'pupesh', name: 'Pupesh', img: 'assets/pupesh.jpg' },
  { id: 'ristyo', name: 'Ristyo', img: 'assets/ristyo.jpg' },
  { id: 'iliqnpuh', name: 'Iliqn Puh', img: 'assets/iliqnpuh.jpg' },
  { id: 'daniilvasil', name: 'Daniil & Vasil', img: 'assets/daniilandvasil.jpg' },
  { id: 'storyteller', name: 'The Storyteller', img: 'assets/thestoryteller.jpg' },
  { id: 'fairy', name: 'The Fairy', img: 'assets/thefairy.jpg' },
  { id: 'soon1', name: 'Coming soon', img: null, empty: true },
  { id: 'soon2', name: 'Coming soon', img: null, empty: true },
];

// Items Marin stocks (all integrate with the player's real satchel).
const SHOP_STOCK = [
  { id: 'ambrosiaCake', price: 40 },
  { id: 'nectarVial', price: 40 },
  { id: 'ambrosiaCake2', price: 70 },
  { id: 'cinderSigil', price: 300 },
];

export class Scenes {
  constructor(game) {
    this.game = game;
    this.hub = game.save.loadHub();
    this.active = null;       // null | 'path' | 'corridor' | 'shop' | 'battle'
    this.t = 0;
    this._raf = null;
    this._tapRaf = null;
    this.tapCells = [];
    this.tornCount = 0;
    this.selectedChar = CHARACTERS[0];
    this.defeatedBosses = new Set(this.hub.defeated || []);

    this._buildDOM();
    this._wire();
    this._loadArt();
    this.battle = new BossBattle(this);
  }

  /**
   * If the player drops their own pixel art into assets/, use it. Each loads
   * independently and silently falls back to the procedural drawing on 404, so
   * the game works with zero, some, or all of these files present:
   *   assets/menu.png     — main-menu background
   *   assets/corridor.png — the stone hall
   *   assets/shop.png     — Marin's shop interior
   *   assets/marin.png    — Marin's character portrait (shown in the shop)
   */
  _loadArt() {
    this._art = {};
    // Try each candidate path in order (.png then .jpg); first that loads wins,
    // otherwise the procedural scene stays as the fallback.
    const use = (key, srcs, apply) => {
      let i = 0;
      const tryNext = () => {
        if (i >= srcs.length) return;
        const img = new Image();
        img.onload = () => { this._art[key] = img; apply(img); };
        img.onerror = () => { i++; tryNext(); };
        img.src = srcs[i];
      };
      tryNext();
    };
    use('menu', ['assets/menu.png', 'assets/menu.jpg'], (img) => {
      const el = document.getElementById('menu-bg');
      el.src = img.src; el.classList.remove('hidden');
      document.body.classList.add('has-menu-bg');   // hide procedural title
    });
    use('corridor', ['assets/corridor.png', 'assets/corridor.jpg'], (img) => {
      document.getElementById('corridor-img').src = img.src;
      document.getElementById('corridor-img').classList.remove('hidden');
      document.getElementById('corridor-bg').classList.add('hidden');
      document.body.classList.add('has-corridor-img');  // art has its own door signs
    });
    use('path', ['assets/path.png', 'assets/path.jpg'], (img) => {
      document.getElementById('path-img').src = img.src;
      document.getElementById('path-img').classList.remove('hidden');
      document.getElementById('path-bg').classList.add('hidden');
    });
    use('shop', ['assets/shop.png', 'assets/shop.jpg'], (img) => {
      document.getElementById('shop-img').src = img.src;
      document.getElementById('shop-img').classList.remove('hidden');
      document.getElementById('shop-bg').classList.add('hidden');
      // The photo has no character, so draw Marin's sprite on top of it.
      document.getElementById('marin-sprite').classList.remove('hidden');
    });
    use('marin', ['assets/marin.png'], (img) => {
      const cv = document.getElementById('marin-portrait');
      const c = cv.getContext('2d');
      c.imageSmoothingEnabled = false;
      if (/\.png(\?|$)/i.test(img.src)) {
        // A .png is a ready-made transparent cutout — shown raw, except the
        // bottom is trimmed so the objects in his hands aren't displayed.
        const cropH = Math.round(img.height * 0.64);
        cv.width = img.width; cv.height = cropH;
        c.clearRect(0, 0, cv.width, cv.height);
        c.drawImage(img, 0, 0, img.width, cropH, 0, 0, img.width, cropH);
      } else {
        // Raw photo: crop to Marin's hat + face (zoomed out a little, but still
        // stopping short of the "Buy Vape" button on the right and the objects
        // along the bottom), then fade the edges to transparent so the room
        // background drops away and he reads as a cutout rather than a boxed photo.
        const sx = img.width * 0.08, sy = img.height * 0.12;
        const sw = img.width * 0.50, sh = img.height * 0.44;
        const W = Math.round(sw), H = Math.round(sh);
        cv.width = W; cv.height = H;
        c.clearRect(0, 0, W, H);
        c.drawImage(img, sx, sy, sw, sh, 0, 0, W, H);
        c.globalCompositeOperation = 'destination-in';
        c.save();
        c.translate(W * 0.52, H * 0.44);
        c.scale(1, (H * 1.05) / W);
        const rad = c.createRadialGradient(0, 0, 0, 0, 0, W * 0.58);
        rad.addColorStop(0, 'rgba(0,0,0,1)');
        rad.addColorStop(0.50, 'rgba(0,0,0,1)');
        rad.addColorStop(1, 'rgba(0,0,0,0)');
        c.fillStyle = rad;
        c.beginPath(); c.arc(0, 0, W * 0.58, 0, 7); c.fill();
        c.restore();
        c.globalCompositeOperation = 'source-over';
      }
      cv.classList.remove('hidden');
      document.getElementById('marin-sprite').classList.add('hidden'); // photo replaces the sprite
      this._marinPhoto = true;
    });
  }

  // The gold shown/spent: the live player's purse if in a game, else the hub wallet.
  get wallet() { return this.game.player ? this.game.player.gold : this.hub.gold; }
  set wallet(v) { if (this.game.player) this.game.player.gold = v; else { this.hub.gold = v; this.game.save.saveHub(this.hub); } }

  _buildDOM() {
    // Castle hotspot lives inside the main menu so it shows/hides with it.
    const menu = document.getElementById('main-menu');
    const hot = document.createElement('button');
    hot.className = 'castle-hotspot';
    hot.id = 'castle-hotspot';
    hot.innerHTML = '<span class="castle-label">▶ Click the castle to enter</span>';
    menu.appendChild(hot);
    this.castleHotspot = hot;

    // Optional custom main-menu background image (assets/menu.png), behind the
    // menu content. Falls back to the procedural intro scene if absent.
    const mbg = document.createElement('img');
    mbg.id = 'menu-bg'; mbg.className = 'menu-bg hidden'; mbg.alt = '';
    menu.insertBefore(mbg, menu.firstChild);

    const wrap = document.createElement('div');
    wrap.id = 'scenes';
    wrap.innerHTML = `
      <!-- Corridor -->
      <div class="scene-overlay hidden" id="scene-corridor">
        <div class="scene-stage">
          <canvas class="scene-bg" id="corridor-bg" width="${BW}" height="${BH}"></canvas>
          <img class="scene-img hidden" id="corridor-img" alt="">
          <button class="door-hotspot door-wizard" id="door-wizard"><span class="door-label wiz">Marin the Wizard</span></button>
          <button class="door-hotspot door-changing" id="door-changing"><span class="door-label chg">Changing Room</span></button>
        </div>
        <button class="scene-back" data-back="path">⟵ Back to the path</button>
        <div class="scene-hint">Click a door to enter</div>
      </div>

      <!-- Shop -->
      <div class="scene-overlay hidden" id="scene-shop">
        <div class="scene-stage">
          <canvas class="scene-bg" id="shop-bg" width="${BW}" height="${BH}"></canvas>
          <img class="scene-img hidden" id="shop-img" alt="">
          <canvas class="marin-sprite hidden" id="marin-sprite" width="48" height="76"></canvas>
        </div>
        <canvas class="marin-portrait hidden" id="marin-portrait"></canvas>
        <button class="scene-back" data-back="corridor">⟵ Back to the hall</button>
        <div class="shop-gold">GOLD: <span id="shop-gold-val">0</span></div>
        <div class="shop-marin-line" id="marin-line">"Welcome, traveler. Coin for cures?"</div>
        <div class="shop-panel">
          <div class="shop-title">✦ Marin's Apothecary ✦</div>
          <div class="shop-counter" id="shop-items"></div>
        </div>
      </div>

      <!-- Path (boss map) -->
      <div class="scene-overlay hidden" id="scene-path">
        <div class="scene-stage">
          <canvas class="scene-bg" id="path-bg" width="${BW}" height="${BH}"></canvas>
          <img class="scene-img hidden" id="path-img" alt="">
          <div id="path-bosses"></div>
          <button class="path-castle" id="path-castle"><span>🏰 Enter the Castle</span></button>
        </div>
        <button class="scene-back" data-back="menu">⟵ Leave</button>
        <div class="scene-hint">Click a boss to fight · enter the castle for the shop &amp; changing room</div>
      </div>

      <!-- Character select (Changing Room) -->
      <div class="char-modal hidden" id="char-modal">
        <div class="char-box">
          <button class="tap-close" id="char-close">✕</button>
          <h2>Choose Your Champion</h2>
          <p class="tap-sub">Pick who you fight as. Drop more art into <code>/assets</code> and add them later.</p>
          <div class="char-grid" id="char-grid"></div>
        </div>
      </div>

      <!-- Fog transition -->
      <div class="scene-fog" id="scene-fog"></div>

      <!-- Enchanted Loom (upload + tear) -->
      <div class="tapestry-modal hidden" id="tapestry-modal">
        <div class="tapestry-box">
          <button class="tap-close" id="tap-close">✕</button>
          <h2>Marin's Enchanted Loom</h2>
          <p class="tap-sub">Weave any image into a tapestry of rune-shards — then tear them free, one by one.</p>
          <div class="tap-controls">
            <label class="tap-upload">📷 Upload image<input type="file" id="tap-file" accept="image/*"></label>
            <label class="tap-sel">Shards
              <select id="tap-cols">
                <option value="4">4×4</option>
                <option value="8" selected>8×8</option>
                <option value="12">12×12</option>
                <option value="16">16×16</option>
              </select>
            </label>
            <button class="tap-btn" id="tap-tearall">Tear All</button>
            <button class="tap-btn" id="tap-reset">Re-weave</button>
            <span class="tap-count">Shards torn: <b id="tap-torn">0</b></span>
          </div>
          <div class="tap-stage">
            <canvas id="tap-canvas" width="512" height="512"></canvas>
            <div class="tap-empty" id="tap-empty">Upload an image to weave your tapestry</div>
          </div>
          <p class="tap-foot">Click a shard to tear it free.</p>
        </div>
      </div>
    `;
    document.getElementById('ui').appendChild(wrap);

    this.corridor = document.getElementById('scene-corridor');
    this.shop = document.getElementById('scene-shop');
    this.path = document.getElementById('scene-path');
    this.fog = document.getElementById('scene-fog');
    this.corridorCtx = document.getElementById('corridor-bg').getContext('2d');
    this.shopCtx = document.getElementById('shop-bg').getContext('2d');
    this.pathCtx = document.getElementById('path-bg').getContext('2d');
    this.corridorCtx.imageSmoothingEnabled = false;
    this.shopCtx.imageSmoothingEnabled = false;
    this.pathCtx.imageSmoothingEnabled = false;
    this.marinCtx = document.getElementById('marin-sprite').getContext('2d');
    this.marinCtx.imageSmoothingEnabled = false;
    this.tapCanvas = document.getElementById('tap-canvas');
    this.tapCtx = this.tapCanvas.getContext('2d');
    this.tapCtx.imageSmoothingEnabled = false;

    this._buildBossHotspots();
    this._buildCharGrid();
  }

  // Place a clickable hotspot over each boss on the path map.
  _buildBossHotspots() {
    const host = document.getElementById('path-bosses');
    host.innerHTML = '';
    this.bossEls = {};
    BOSSES.forEach((boss, i) => {
      const b = document.createElement('button');
      b.className = 'boss-hotspot';
      b.style.left = `${(boss.cx - 0.08) * 100}%`;
      b.style.top = `${(boss.cy - 0.20) * 100}%`;
      b.innerHTML = `<span class="boss-tip">⚔ Boss ${i + 1}<br>${boss.name}</span><span class="boss-check">✓</span>`;
      b.addEventListener('click', () => this._fightBoss(boss));
      host.appendChild(b);
      this.bossEls[boss.id] = b;
    });
  }

  _buildCharGrid() {
    const grid = document.getElementById('char-grid');
    grid.innerHTML = '';
    CHARACTERS.forEach((ch) => {
      const card = document.createElement('button');
      card.className = 'char-card' + (ch.empty ? ' empty' : '') + (ch.id === this.selectedChar.id ? ' selected' : '');
      card.dataset.id = ch.id;
      card.innerHTML = ch.empty
        ? `<div class="char-thumb soon">?</div><div class="char-name">${ch.name}</div>`
        : `<div class="char-thumb">${ch.img ? `<img src="${ch.img}" alt="" onerror="this.parentNode.classList.add('noimg')">` : '<span class="char-hero">🧙</span>'}</div><div class="char-name">${ch.name}</div>`;
      if (!ch.empty) card.addEventListener('click', () => this._selectChar(ch));
      grid.appendChild(card);
    });
  }

  _wire() {
    this.castleHotspot.addEventListener('click', () => { this.game.audio.sfx('ui'); this.enterPath(); });
    document.getElementById('path-castle').addEventListener('click', () => { this.game.audio.sfx('ui'); this.enterCorridor(); });
    document.getElementById('door-wizard').addEventListener('click', () => this.enterShop());
    document.getElementById('door-changing').addEventListener('click', () => { this.game.audio.sfx('ui'); this.openCharacterSelect(); });
    document.getElementById('char-close').addEventListener('click', () => { this.game.audio.sfx('ui'); document.getElementById('char-modal').classList.add('hidden'); });
    document.querySelectorAll('.scene-back').forEach((b) =>
      b.addEventListener('click', () => { this.game.audio.sfx('ui'); this.back(b.dataset.back); }));

    // Tapestry station
    document.getElementById('tap-close').addEventListener('click', () => this.closeTapestry());
    document.getElementById('tap-file').addEventListener('change', (e) => this._onUpload(e));
    document.getElementById('tap-cols').addEventListener('change', () => this._buildTapestry());
    document.getElementById('tap-tearall').addEventListener('click', () => this._tearAll());
    document.getElementById('tap-reset').addEventListener('click', () => this._buildTapestry());
    this.tapCanvas.addEventListener('click', (e) => this._onTapClick(e));
  }

  // =========================================================================
  // Navigation
  // =========================================================================
  _hideAllScenes() {
    this.corridor.classList.add('hidden');
    this.shop.classList.add('hidden');
    this.path.classList.add('hidden');
  }

  // Play / menu-castle land here: the boss map.
  enterPath(fromBattle) {
    if (!fromBattle) this.game.ui.hideMainMenu();
    this._hideAllScenes();
    this.path.classList.remove('hidden');
    this.active = 'path';
    this._refreshBossMarks();
    this._startRaf();
  }

  enterCorridor() {
    this.game.ui.hideMainMenu();
    this._hideAllScenes();
    this.corridor.classList.remove('hidden');
    this.active = 'corridor';
    this._startRaf();
  }

  enterShop() {
    this.game.audio.sfx('quest');
    this._fog(() => {
      this._hideAllScenes();
      this.shop.classList.remove('hidden');
      this.active = 'shop';
      this._renderShopItems();
      this._updateGold();
    });
  }

  back(target) {
    if (target === 'menu') {
      this._hideAllScenes();
      this.active = null;
      this._stopRaf();
      this.game.ui.showMainMenu(this.game.save.hasSave());
    } else if (target === 'path') {
      this._hideAllScenes();
      this.path.classList.remove('hidden');
      this.active = 'path';
      this._refreshBossMarks();
    } else if (target === 'corridor') {
      this._fog(() => {
        this._hideAllScenes();
        this.corridor.classList.remove('hidden');
        this.active = 'corridor';
      });
    }
  }

  // --- Bosses ---------------------------------------------------------------
  _fightBoss(boss) {
    this.game.audio.sfx('quest');
    this._stopRaf();
    this._hideAllScenes();
    this.active = 'battle';
    this.battle.start(boss, this.selectedChar);
  }
  markBossDefeated(id) {
    this.defeatedBosses.add(id);
    this.hub.defeated = [...this.defeatedBosses];
    this.game.save.saveHub(this.hub);
  }
  _refreshBossMarks() {
    if (!this.bossEls) return;
    for (const id in this.bossEls) this.bossEls[id].classList.toggle('defeated', this.defeatedBosses.has(id));
  }

  // --- Character select (Changing Room) -------------------------------------
  openCharacterSelect() {
    this._buildCharGrid();
    document.getElementById('char-modal').classList.remove('hidden');
  }
  _selectChar(ch) {
    this.selectedChar = ch;
    this.game.audio.sfx('pickup');
    this._buildCharGrid();
    setTimeout(() => document.getElementById('char-modal').classList.add('hidden'), 220);
  }

  // A mystical fog wipe: fade fog in, swap scenes at the peak, fade out.
  _fog(swap) {
    this.fog.classList.add('show');
    setTimeout(() => { swap(); }, 360);
    setTimeout(() => this.fog.classList.remove('show'), 420);
  }

  _flashHint(text) {
    const h = this.corridor.querySelector('.scene-hint');
    if (!h) return;
    const old = h.textContent; h.textContent = text; h.classList.add('flash');
    setTimeout(() => { h.textContent = old; h.classList.remove('flash'); }, 1600);
  }

  // =========================================================================
  // Shop
  // =========================================================================
  _renderShopItems() {
    const host = document.getElementById('shop-items');
    host.innerHTML = '';
    for (const s of SHOP_STOCK) {
      const it = ITEMS[s.id];
      const card = document.createElement('button');
      card.className = 'shop-item';
      card.innerHTML = `<span class="si-icon">${it.icon}</span>
        <span class="si-name">${it.name}</span>
        <span class="si-desc">${it.desc}</span>
        <span class="si-price">🪙 ${s.price}</span>`;
      card.addEventListener('click', () => this._buy(s.id, s.price));
      host.appendChild(card);
    }
    // The Enchanted Loom tile.
    const loom = document.createElement('button');
    loom.className = 'shop-item loom';
    loom.innerHTML = `<span class="si-icon">🧵</span>
      <span class="si-name">Enchanted Loom</span>
      <span class="si-desc">Weave any image into a tapestry of rune-shards and tear it apart.</span>
      <span class="si-price">try it ▸</span>`;
    loom.addEventListener('click', () => this.openTapestry());
    host.appendChild(loom);
  }

  _buy(itemId, price) {
    if (this.wallet < price) { this._marin('Not enough coin, friend. Come back richer.'); this.game.audio.sfx('ui'); return; }
    this.wallet = this.wallet - price;
    if (this.game.player) { this.game.player.addItem(itemId); this.game.saveGame(); this.game.ui.refreshHUD(); }
    else { this.hub.stash[itemId] = (this.hub.stash[itemId] || 0) + 1; this.game.save.saveHub(this.hub); }
    this._updateGold();
    this._marin(`A fine choice — one ${ITEMS[itemId].name}. It will await you on the road.`);
    this.game.audio.sfx('pickup');
  }

  _updateGold() { document.getElementById('shop-gold-val').textContent = this.wallet; }
  _marin(line) { document.getElementById('marin-line').textContent = `"${line}"`; }

  // Grant menu-bought items to the player when a chronicle begins.
  applyStashToPlayer(player) {
    let any = false;
    for (const id in this.hub.stash) { const n = this.hub.stash[id]; if (n > 0) { player.addItem(id, n); any = true; } }
    if (any) { this.hub.stash = {}; this.game.save.saveHub(this.hub); }
    return any;
  }

  // =========================================================================
  // Enchanted Loom — upload, perforate, tear
  // =========================================================================
  openTapestry() {
    document.getElementById('tapestry-modal').classList.remove('hidden');
    this.game.audio.sfx('ui');
  }
  closeTapestry() {
    document.getElementById('tapestry-modal').classList.add('hidden');
    if (this._tapRaf) { cancelAnimationFrame(this._tapRaf); this._tapRaf = null; }
    this.game.audio.sfx('ui');
  }

  _onUpload(e) {
    const file = e.target.files && e.target.files[0];
    if (!file) return;
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => { this.tapImg = img; this._buildTapestry(); URL.revokeObjectURL(url); };
    img.src = url;
  }

  // Pixelate the source image, lay out the perforated grid, start the loop.
  _buildTapestry() {
    if (!this.tapImg) return;
    document.getElementById('tap-empty').style.display = 'none';
    const cols = parseInt(document.getElementById('tap-cols').value, 10);
    this.tapCols = cols; this.tapRows = cols;
    this.tornCount = 0; this._updateTorn();

    // Pixelate: cover-fit into a small canvas, then upscale crisp to 512².
    const SMALL = Math.max(48, cols * 8);
    const small = document.createElement('canvas'); small.width = small.height = SMALL;
    coverDraw(small.getContext('2d'), this.tapImg, SMALL, SMALL);
    this.tapSrc = document.createElement('canvas'); this.tapSrc.width = this.tapSrc.height = 512;
    const tc = this.tapSrc.getContext('2d'); tc.imageSmoothingEnabled = false;
    tc.drawImage(small, 0, 0, 512, 512);

    const cell = 512 / cols;
    this.tapCell = cell;
    this.tapCells = [];
    for (let r = 0; r < cols; r++) for (let c = 0; c < cols; c++) {
      this.tapCells.push({ c, r, gx: c * cell, gy: r * cell, torn: false, x: 0, y: 0, vx: 0, vy: 0, rot: 0, vr: 0, alpha: 1 });
    }
    if (!this._tapRaf) this._tapTick();
  }

  _onTapClick(e) {
    if (!this.tapCells.length) return;
    const rect = this.tapCanvas.getBoundingClientRect();
    const x = (e.clientX - rect.left) / rect.width * 512;
    const y = (e.clientY - rect.top) / rect.height * 512;
    const c = Math.floor(x / this.tapCell), r = Math.floor(y / this.tapCell);
    const cell = this.tapCells.find((k) => k.c === c && k.r === r && !k.torn);
    if (cell) { this._tear(cell); this.game.audio.sfx('hit'); }
  }

  _tear(cell) {
    cell.torn = true;
    cell.x = cell.gx; cell.y = cell.gy;
    cell.vx = (Math.random() - 0.5) * 260;
    cell.vy = -120 - Math.random() * 120;
    cell.vr = (Math.random() - 0.5) * 8;
    this.tornCount++; this._updateTorn();
  }

  _tearAll() {
    let n = 0;
    for (const cell of this.tapCells) if (!cell.torn) { this._tear(cell); n++; }
    if (n) this.game.audio.sfx('lightning');
  }

  _updateTorn() { const el = document.getElementById('tap-torn'); if (el) el.textContent = this.tornCount; }

  _tapTick() {
    this._tapRaf = requestAnimationFrame(() => this._tapTick());
    const ctx = this.tapCtx;
    ctx.clearRect(0, 0, 512, 512);
    if (!this.tapSrc) return;
    const cell = this.tapCell, dt = 1 / 60;

    for (const k of this.tapCells) {
      if (!k.torn) {
        ctx.drawImage(this.tapSrc, k.gx, k.gy, cell, cell, k.gx, k.gy, cell, cell);
      } else if (k.alpha > 0) {
        k.vy += 520 * dt; k.x += k.vx * dt; k.y += k.vy * dt; k.rot += k.vr * dt; k.alpha -= dt * 0.7;
        ctx.save();
        ctx.globalAlpha = Math.max(0, k.alpha);
        ctx.translate(k.x + cell / 2, k.y + cell / 2);
        ctx.rotate(k.rot);
        ctx.drawImage(this.tapSrc, k.gx, k.gy, cell, cell, -cell / 2, -cell / 2, cell, cell);
        ctx.restore();
      }
    }

    // Perforation overlay over the intact shards.
    ctx.strokeStyle = 'rgba(255,240,200,0.35)';
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    for (let i = 1; i < this.tapCols; i++) {
      ctx.beginPath(); ctx.moveTo(i * cell, 0); ctx.lineTo(i * cell, 512); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(0, i * cell); ctx.lineTo(512, i * cell); ctx.stroke();
    }
    ctx.setLineDash([]);
  }

  // =========================================================================
  // Pixel scene backgrounds (animated torches/candles)
  // =========================================================================
  _startRaf() { if (!this._raf) this._loop(); }
  _stopRaf() { if (this._raf) { cancelAnimationFrame(this._raf); this._raf = null; } }
  _loop() {
    this._raf = requestAnimationFrame(() => this._loop());
    this.t += 1 / 60;
    // Only animate the procedural pixel scene when no custom image is provided.
    if (this.active === 'corridor' && !this._art.corridor) this._drawCorridor(this.t);
    else if (this.active === 'shop') {
      if (!this._art.shop) this._drawShop(this.t);   // procedural draw already includes Marin
      else if (!this._marinPhoto) this._drawMarin(this.t); // sprite only if no photo portrait
    } else if (this.active === 'path' && !this._art.path) {
      this._drawPathFallback(this.t);
    }
  }

  // Simple dark backdrop used only if assets/path.jpg is missing.
  _drawPathFallback(t) {
    const ctx = this.pathCtx;
    const sky = ctx.createLinearGradient(0, 0, 0, BH);
    sky.addColorStop(0, '#14102a'); sky.addColorStop(1, '#0c1410');
    ctx.fillStyle = sky; ctx.fillRect(0, 0, BW, BH);
    ctx.fillStyle = '#0a1410'; ctx.fillRect(0, BH * 0.7, BW, BH * 0.3);
    ctx.fillStyle = '#caa84d'; ctx.font = '10px monospace'; ctx.textAlign = 'center';
    ctx.fillText('THE PATH', BW / 2, 18); ctx.textAlign = 'left';
  }

  // Marin the Wizard standing in his shop (drawn over the photo background).
  _drawMarin(t) {
    const ctx = this.marinCtx;
    ctx.clearRect(0, 0, 48, 76);
    this._marinSprite(ctx, 24, 28, t);
  }

  _bricks(ctx, x, y, w, h, a, b, mortar) {
    ctx.fillStyle = b; ctx.fillRect(x, y, w, h);
    for (let row = 0; row * 8 < h; row++) {
      const yy = y + row * 8, off = (row % 2) * 8;
      for (let bx = x - off; bx < x + w; bx += 16) {
        ctx.fillStyle = a; ctx.fillRect(bx + 1, yy + 1, 14, 6);
        ctx.fillStyle = 'rgba(255,255,255,0.05)'; ctx.fillRect(bx + 1, yy + 1, 14, 1);
      }
      ctx.fillStyle = mortar; ctx.fillRect(x, yy + 7, w, 1);
    }
  }

  _torch(ctx, x, y, t) {
    ctx.fillStyle = '#2a1c10'; ctx.fillRect(x - 1, y, 3, 8);          // bracket
    ctx.fillStyle = '#1a120a'; ctx.fillRect(x - 2, y + 7, 5, 2);
    ctx.drawImage(flame((t * 10) | 0), x - 7, y - 12);                 // flame sprite
    // glow
    const g = ctx.createRadialGradient(x, y - 4, 0, x, y - 4, 26);
    g.addColorStop(0, 'rgba(255,170,80,0.30)'); g.addColorStop(1, 'rgba(255,170,80,0)');
    ctx.fillStyle = g; ctx.fillRect(x - 26, y - 30, 52, 52);
  }

  _drawCorridor(t) {
    const ctx = this.corridorCtx;
    ctx.clearRect(0, 0, BW, BH);
    ctx.fillStyle = '#0b0a10'; ctx.fillRect(0, 0, BW, BH);
    // ceiling beam
    ctx.fillStyle = '#3a2a18'; ctx.fillRect(0, 0, BW, 16);
    ctx.fillStyle = '#2a1d10'; for (let x = 0; x < BW; x += 24) ctx.fillRect(x, 0, 2, 16);
    // walls
    this._bricks(ctx, 0, 16, BW, 120, '#4a4654', '#33303c', '#1c1a22');
    // floor
    this._bricks(ctx, 0, 136, BW, 44, '#3a3744', '#2a2832', '#17151c');

    // rug with a magic circle
    ctx.fillStyle = '#5a2436'; ctx.fillRect(96, 150, 128, 26);
    ctx.strokeStyle = '#caa84d'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.ellipse(160, 163, 44, 9, 0, 0, 7); ctx.stroke();
    ctx.beginPath(); ctx.ellipse(160, 163, 30, 6, 0, 0, 7); ctx.stroke();

    // Left: Marin's runed door (glowing arch)
    this._archDoor(ctx, 40, 40, 64, 96, t, true);
    // Right: changing room (curtain)
    this._curtainDoor(ctx, 216, 48, 64, 88);

    // torches
    this._torch(ctx, 130, 60, t);
    this._torch(ctx, 196, 60, t + 1.3);
    this._torch(ctx, 18, 64, t + 0.6);
    this._torch(ctx, 302, 64, t + 2.1);

    // a couple of framed paintings + a hanging sword
    ctx.fillStyle = '#caa84d'; ctx.fillRect(150, 70, 14, 18); ctx.fillStyle = '#1a2a3a'; ctx.fillRect(151, 71, 12, 16);
    ctx.fillStyle = '#caa84d'; ctx.fillRect(176, 72, 14, 16); ctx.fillStyle = '#2a1a2a'; ctx.fillRect(177, 73, 12, 14);
    ctx.fillStyle = '#b8c0cc'; ctx.fillRect(118, 96, 2, 22); ctx.fillStyle = '#caa84d'; ctx.fillRect(116, 116, 6, 3);
  }

  _archDoor(ctx, x, y, w, h, t, glow) {
    // arch frame
    ctx.fillStyle = '#5a4632'; ctx.fillRect(x - 4, y, w + 8, h + 4);
    ctx.fillStyle = '#3a2c1c'; ctx.fillRect(x, y + 8, w, h - 8);
    // door wood
    ctx.fillStyle = '#4a3420'; ctx.fillRect(x + 4, y + 12, w - 8, h - 14);
    ctx.fillStyle = '#5a4026'; for (let i = 0; i < 3; i++) ctx.fillRect(x + 6 + i * ((w - 12) / 3), y + 14, 1, h - 18);
    // runes (glowing)
    if (glow) {
      const pulse = 0.5 + Math.sin(t * 3) * 0.4;
      ctx.fillStyle = `rgba(140,200,255,${pulse})`;
      const runes = ['ᚠ', 'ᛉ', 'ᛟ', 'ᚹ', 'ᛞ', 'ᚱ'];
      ctx.font = '7px monospace'; ctx.textAlign = 'center';
      for (let i = 0; i < 6; i++) ctx.fillText(runes[i], x + (i % 2 ? -3 : w + 3), y + 18 + (i) * ((h) / 6));
      ctx.textAlign = 'left';
      // fairy lights along the arch
      for (let i = 0; i <= 10; i++) {
        const a = Math.PI * (i / 10);
        const fx = x + w / 2 - Math.cos(a) * (w / 2 + 4);
        const fy = y + 8 - Math.sin(a) * 8;
        const tw = 0.4 + Math.abs(Math.sin(t * 4 + i)) * 0.6;
        ctx.fillStyle = `rgba(255,235,150,${tw})`; ctx.fillRect(fx, fy, 2, 2);
      }
      // warm inner glow
      const g = ctx.createRadialGradient(x + w / 2, y + h / 2, 0, x + w / 2, y + h / 2, 40);
      g.addColorStop(0, 'rgba(120,170,255,0.18)'); g.addColorStop(1, 'rgba(120,170,255,0)');
      ctx.fillStyle = g; ctx.fillRect(x - 20, y - 20, w + 40, h + 40);
    }
  }

  _curtainDoor(ctx, x, y, w, h) {
    ctx.fillStyle = '#3a2c1c'; ctx.fillRect(x - 4, y - 4, w + 8, h + 6);  // frame
    ctx.fillStyle = '#120e16'; ctx.fillRect(x, y, w, h);                  // dark opening
    // purple curtain pulled to one side
    ctx.fillStyle = '#5a2a6a'; ctx.fillRect(x + w - 22, y, 22, h);
    ctx.fillStyle = '#6a3a7e'; for (let i = 0; i < 4; i++) ctx.fillRect(x + w - 20 + i * 5, y, 2, h);
    // a robe on a peg + stool
    ctx.fillStyle = '#7a6a4a'; ctx.fillRect(x + 6, y + 14, 12, 22);
    ctx.fillStyle = '#3a2a18'; ctx.fillRect(x + 10, y + h - 12, 10, 4); ctx.fillRect(x + 12, y + h - 8, 2, 6); ctx.fillRect(x + 16, y + h - 8, 2, 6);
  }

  _drawShop(t) {
    const ctx = this.shopCtx;
    ctx.clearRect(0, 0, BW, BH);
    ctx.fillStyle = '#0b0a10'; ctx.fillRect(0, 0, BW, BH);
    ctx.fillStyle = '#3a2a18'; ctx.fillRect(0, 0, BW, 16);
    this._bricks(ctx, 0, 16, BW, 120, '#4a4654', '#33303c', '#1c1a22');
    this._bricks(ctx, 0, 136, BW, 44, '#3a3744', '#2a2832', '#17151c');

    // Left: the same runed door (continuity with the corridor)
    this._archDoor(ctx, 26, 44, 58, 90, t, true);

    // Right: tall apothecary shelves stocked with potions + produce
    this._shelf(ctx, 180, 34, 128, t);

    // Counter table with an open spellbook + mortar + candles
    ctx.fillStyle = '#4a3420'; ctx.fillRect(96, 118, 84, 30);
    ctx.fillStyle = '#3a2818'; ctx.fillRect(96, 118, 84, 3);
    // open book
    ctx.fillStyle = '#e8dcc0'; ctx.fillRect(108, 110, 26, 12);
    ctx.fillStyle = '#caa84d'; ctx.fillRect(120, 110, 2, 12);
    ctx.fillStyle = '#9a7a40'; ctx.fillRect(110, 113, 9, 1); ctx.fillRect(123, 113, 9, 1); ctx.fillRect(110, 116, 9, 1); ctx.fillRect(123, 116, 9, 1);
    // candle
    ctx.drawImage(prop('candle'), 150, 104);
    const fl = 0.5 + Math.sin(t * 8) * 0.5; ctx.fillStyle = `rgba(255,200,120,${fl})`; ctx.fillRect(152, 100, 2, 3);

    // Marin behind the counter
    this._marinSprite(ctx, 150, 64, t);

    // torches
    this._torch(ctx, 110, 60, t);
    this._torch(ctx, 300, 58, t + 1.1);
  }

  _shelf(ctx, x, y, h, t) {
    ctx.fillStyle = '#4a3420'; ctx.fillRect(x, y, 120, h);
    ctx.fillStyle = '#3a2818'; ctx.fillRect(x, y, 120, 2);
    const potColors = ['#ff5a8a', '#5ad0ff', '#9a6bff', '#76e06b', '#ffd24a', '#ff8a3a'];
    for (let row = 0; row < 4; row++) {
      const sy = y + 6 + row * (h / 4);
      ctx.fillStyle = '#2a1d10'; ctx.fillRect(x, sy + 18, 120, 3);   // plank
      for (let i = 0; i < 9; i++) {
        const px = x + 6 + i * 13;
        if (row === 2) { // a row of cabbages/herbs for charm
          ctx.fillStyle = '#3f7d3a'; ctx.fillRect(px, sy + 10, 9, 8);
          ctx.fillStyle = '#5fa04a'; ctx.fillRect(px + 1, sy + 11, 7, 3);
        } else {
          const col = potColors[(i + row) % potColors.length];
          ctx.fillStyle = '#2a2230'; ctx.fillRect(px + 1, sy + 6, 7, 12);  // bottle
          ctx.fillStyle = col; ctx.fillRect(px + 2, sy + 9, 5, 8);
          const gl = 0.4 + Math.abs(Math.sin(t * 2 + i + row)) * 0.4;
          ctx.fillStyle = `rgba(255,255,255,${gl * 0.5})`; ctx.fillRect(px + 2, sy + 9, 2, 3);
          ctx.fillStyle = '#caa84d'; ctx.fillRect(px + 3, sy + 5, 3, 2);   // cork
        }
      }
    }
  }

  // A little pixel wizard: pointed blue hat, face, purple robe with gold trim.
  _marinSprite(ctx, x, y, t) {
    const bob = Math.sin(t * 2) * 1;
    y += bob;
    // robe
    ctx.fillStyle = '#5a2e8a'; ctx.fillRect(x - 8, y + 14, 16, 26);
    ctx.fillStyle = '#caa84d'; ctx.fillRect(x - 8, y + 14, 2, 26); ctx.fillRect(x + 6, y + 14, 2, 26);
    // arms
    ctx.fillStyle = '#5a2e8a'; ctx.fillRect(x - 12, y + 16, 4, 14); ctx.fillRect(x + 8, y + 16, 4, 14);
    // head
    ctx.fillStyle = '#d8a878'; ctx.fillRect(x - 5, y + 4, 10, 11);
    // beard
    ctx.fillStyle = '#cfcfd6'; ctx.fillRect(x - 5, y + 11, 10, 5); ctx.fillRect(x - 3, y + 15, 6, 3);
    // eyes
    ctx.fillStyle = '#1a1414'; ctx.fillRect(x - 3, y + 8, 1, 2); ctx.fillRect(x + 2, y + 8, 1, 2);
    // hat
    ctx.fillStyle = '#2a3aa0'; ctx.beginPath(); ctx.moveTo(x - 7, y + 5); ctx.lineTo(x + 7, y + 5); ctx.lineTo(x + 1, y - 12); ctx.closePath(); ctx.fill();
    ctx.fillStyle = '#3a4ac0'; ctx.fillRect(x - 8, y + 4, 16, 3);
    // star on the hat
    const tw = 0.5 + Math.abs(Math.sin(t * 4)) * 0.5;
    ctx.fillStyle = `rgba(255,230,140,${tw})`; ctx.fillRect(x - 2, y - 4, 3, 3);
    // wand with a glowing tip
    ctx.fillStyle = '#3a2a18'; ctx.fillRect(x + 11, y + 6, 2, 12);
    ctx.fillStyle = `rgba(180,220,255,${tw})`; ctx.fillRect(x + 10, y + 4, 4, 4);
  }
}

// Draw an image into a square canvas using cover-fit (fills, center-crops).
function coverDraw(ctx, img, w, h) {
  const ir = img.width / img.height, cr = w / h;
  let sw, sh, sx, sy;
  if (ir > cr) { sh = img.height; sw = sh * cr; sx = (img.width - sw) / 2; sy = 0; }
  else { sw = img.width; sh = sw / cr; sx = 0; sy = (img.height - sh) / 2; }
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(img, sx, sy, sw, sh, 0, 0, w, h);
}
