/**
 * Game.js
 * ----------------------------------------------------------------------------
 * The orchestrator for the 2D pixel RPG. Owns the canvas + a low-res virtual
 * buffer (rendered crisp, then integer-scaled to the window), the master loop
 * and the state machine (loading → intro → menu → playing), and wires every
 * subsystem together: tilemap, player, magic, combat, quests, UI, input,
 * audio and saves. Region loading, spawning, dialogue, death/respawn,
 * fast-travel and the tutorial live here.
 * ----------------------------------------------------------------------------
 */

import { Input } from './Input.js';
import { AudioManager } from './AudioManager.js';
import { SaveManager } from './SaveManager.js';
import { TileMap } from '../world/TileMap.js';
import { Player } from '../entities/Player.js';
import { Enemy } from '../entities/Enemy.js';
import { NPC } from '../entities/NPC.js';
import { MagicSystem } from '../systems/MagicSystem.js';
import { CombatSystem } from '../systems/CombatSystem.js';
import { QuestSystem } from '../systems/QuestSystem.js';
import { UI } from '../ui/UI.js';
import { Scenes } from '../ui/Scenes.js';
import { IntroScene } from '../intro/IntroScene.js';
import { REGIONS, CREATURES, NPCS, ITEMS, QUESTS } from '../data/GameData.js';
import { clamp, randRange } from './Utils.js';

const TS = 16;                   // tile size
const BASE_VH = 270;             // target virtual height; pixel scale derives from it
const TARGET_ENEMIES = 12;

export class Game {
  constructor() {
    this.state = 'loading';
    this.enemies = [];
    this.npcs = [];
    this.elapsed = 0;
    this.currentRegion = 'mistwood';
    this.unlockedRegions = new Set(['mistwood']);
    this.camX = 0; this.camY = 0;
    this._enemyTimer = 0;
    this._prevM0 = false; this._prevM2 = false;

    this.settings = { musicVolume: 0.5, sfxVolume: 0.7, mouseSensitivity: 1.0, shadows: true, muted: false };

    this._initCanvas();
    this.input = new Input(this.canvas);
    this.audio = new AudioManager();
    this.save = new SaveManager();
    const saved = this.save.loadSettings();
    if (saved) Object.assign(this.settings, saved);

    this.map = new TileMap(this);
    this.magic = new MagicSystem(this);
    this.combat = new CombatSystem(this);
    this.quests = new QuestSystem(this);
    this.ui = new UI(this);
    this.scenes = new Scenes(this);
    this.intro = new IntroScene(this);

    this._bindKeys();
    this._bindWindow();
    this._armAudioGesture();
  }

  _initCanvas() {
    this.canvas = document.getElementById('game-canvas');
    this.ctx = this.canvas.getContext('2d');
    this.ctx.imageSmoothingEnabled = false;
    // Virtual buffer everything is drawn into at native pixel scale. Its size
    // is recomputed on resize so it matches the window aspect (no letterbox).
    this.buffer = document.createElement('canvas');
    this.bctx = this.buffer.getContext('2d');
    this.vw = 480; this.vh = 270;
    this._resize();
    this._lastTime = performance.now();
  }

  _resize() {
    const w = window.innerWidth, h = window.innerHeight;
    this.canvas.width = w; this.canvas.height = h;
    this.ctx.imageSmoothingEnabled = false;
    // Pick an integer pixel scale, then size the virtual buffer to fill the
    // window at that scale → chunky pixels, full-screen, any aspect ratio.
    this.pixelScale = Math.max(2, Math.min(5, Math.round(h / BASE_VH)));
    this.vw = Math.max(200, Math.ceil(w / this.pixelScale));
    this.vh = Math.max(140, Math.ceil(h / this.pixelScale));
    this.buffer.width = this.vw; this.buffer.height = this.vh;
    this.bctx.imageSmoothingEnabled = false;
    this.sxScale = w / this.vw; this.syScale = h / this.vh;
  }

  // --- Coordinate conversions ----------------------------------------------
  screenToWorld(sx, sy) { return { x: this.camX + sx / this.sxScale, y: this.camY + sy / this.syScale }; }
  worldToScreen(wx, wy) { return { x: (wx - this.camX) * this.sxScale, y: (wy - this.camY) * this.syScale }; }

  // =========================================================================
  // Boot
  // =========================================================================
  async boot() {
    for (let p = 0; p <= 1; p += 0.1) { this.ui.setLoadingProgress(p); await new Promise((r) => setTimeout(r, 50)); }
    this.ui.hideLoading();
    // Skip the intro cinematic — boot straight to the main menu.
    this.state = 'menu';
    this.ui.showMainMenu(this.save.hasSave());
    this._loop();
  }

  onIntroComplete() {
    if (this.state !== 'intro') return;
    this.state = 'menu';
    this.ui.showMainMenu(this.save.hasSave());
  }

  // =========================================================================
  // New game / continue / save
  // =========================================================================
  startNewGame() {
    this.ui.hideMainMenu();
    this.player = new Player(this);
    this.currentRegion = 'mistwood';
    this.unlockedRegions = new Set(['mistwood']);
    this._enterRegion('mistwood', true);
    this.quests.accept('awakening');
    this._beginPlay();
    this.ui.bigBanner('The Mistwood');
    setTimeout(() => this.ui.showHint('Move with <b>WASD</b>, aim with the <b>mouse</b>. Press <b>1</b> to loose an Emberhex at the mistlings. <b>Space</b> dodges.', 10), 1400);
  }

  continueGame() {
    const data = this.save.load();
    if (!data) { this.startNewGame(); return; }
    this.ui.hideMainMenu();
    this.player = new Player(this);
    this.player.deserialize(data.player);
    this.quests.deserialize(data.quests);
    this.currentRegion = data.currentRegion || 'mistwood';
    this.unlockedRegions = new Set(data.unlockedRegions || ['mistwood']);
    if (data.bestiary) this.ui.bestiarySeen = new Set(data.bestiary);
    this._enterRegion(this.currentRegion, false);
    if (data.player.x != null) { this.player.x = data.player.x; this.player.y = data.player.y; }
    this._beginPlay();
    this.ui.bigBanner('Welcome back, Unmarked');
    this.ui.refreshHUD();
  }

  _beginPlay() {
    this.state = 'playing';
    this._computeCamera();
    // Deliver anything bought from Marin's shop while at the menu.
    if (this.scenes.applyStashToPlayer(this.player)) this.ui.toast("Marin's wares are in your satchel.", 'gold');
    this.ui.showHUD(true);
    this.ui.refreshHUD();
    this.audio.playMusic(REGIONS[this.currentRegion].music);
    this.ui.setRegionName(REGIONS[this.currentRegion].name);
  }

  saveGame() {
    if (!this.player) return;
    this.save.save({
      player: this.player.serialize(),
      quests: this.quests.serialize(),
      currentRegion: this.currentRegion,
      unlockedRegions: [...this.unlockedRegions],
      bestiary: [...this.ui.bestiarySeen],
    });
    this.audio.sfx('quest');
  }
  saveSettings() { this.save.saveSettings(this.settings); }

  quitToTitle() {
    this.saveGame();
    this.enemies.length = 0; this.npcs.length = 0;
    this.magic.clear();
    this.ui.showHUD(false);
    this.player = null;
    this.state = 'menu';
    this.ui.showMainMenu(true);
  }

  // =========================================================================
  // Regions & spawning
  // =========================================================================
  travelTo(regionId) {
    if (!this.unlockedRegions.has(regionId)) return;
    this.audio.sfx('quest');
    this.ui.bigBanner(REGIONS[regionId].name);
    this._enterRegion(regionId, true);
    this.audio.playMusic(REGIONS[regionId].music);
    this.ui.setRegionName(REGIONS[regionId].name);
    this._computeCamera();
  }

  _enterRegion(regionId, reposition) {
    this.currentRegion = regionId;
    const region = REGIONS[regionId];
    this.map.load(regionId);
    this.enemies.length = 0; this.npcs.length = 0;
    this.magic.clear();
    if (reposition && this.player) { this.player.x = region.spawn.tx * TS + 8; this.player.y = region.spawn.ty * TS + 8; this.player.vx = this.player.vy = 0; }
    this._spawnNPCs(regionId);
    this._spawnPopulation(region);
  }

  _spawnNPCs(regionId) {
    for (const id of Object.keys(NPCS)) if (NPCS[id].region === regionId) this.npcs.push(new NPC(this, NPCS[id]));
  }

  _spawnPopulation(region) {
    for (let i = 0; i < TARGET_ENEMIES; i++) this._spawnEnemyAway(region, 140);
    if (region.id === 'mistwood') {
      // The Cinder-Maw boss lurks in a far corner of the wood.
      const bx = (this.map.w - 8) * TS, by = (this.map.h - 8) * TS;
      this.enemies.push(new Enemy(this, CREATURES.cinderMaw, bx, by));
    }
  }

  _spawnEnemyAway(region, minDist) {
    const type = region.enemies[(Math.random() * region.enemies.length) | 0];
    if (type === 'cinderMaw') return;
    const tile = this._randomWalkable(minDist);
    if (!tile) return;
    this.enemies.push(new Enemy(this, CREATURES[type], tile.x, tile.y));
  }

  _randomWalkable(minDist) {
    for (let i = 0; i < 60; i++) {
      const tx = 1 + ((Math.random() * (this.map.w - 2)) | 0);
      const ty = 1 + ((Math.random() * (this.map.h - 2)) | 0);
      const c = this.map.cell(tx, ty);
      if (c.solid) continue;
      const x = tx * TS + 8, y = ty * TS + 8;
      if (this.player && Math.hypot(x - this.player.x, y - this.player.y) < minDist) continue;
      return { x, y };
    }
    return null;
  }

  // =========================================================================
  // Dialogue (data-driven; identical flow to NPC quest scripts)
  // =========================================================================
  startDialogue(npc) {
    this._dlgNPC = npc; this._dlgStep = 0;
    this._dlgScript = [npc.def.greeting, ...npc.def.lines];
    this._advanceDialogue();
  }
  _advanceDialogue() {
    const npc = this._dlgNPC;
    if (this._dlgStep >= this._dlgScript.length) { this._presentQuestChoices(npc, ''); return; }
    const text = this._dlgScript[this._dlgStep];
    this._dlgStep++;
    if (this._dlgStep >= this._dlgScript.length) this._presentQuestChoices(npc, text);
    else this.ui.showDialogue(npc, { text, choices: [{ label: 'Continue ▸', primary: true, action: () => this._advanceDialogue() }] });
  }
  _presentQuestChoices(npc, lead = '') {
    const prefix = lead ? `${lead}<br><br>` : '';
    const ready = this.quests.readyFor(npc.def.id);
    const available = this.quests.availableFrom(npc.def.id);
    const active = Object.keys(QUESTS).filter((id) => QUESTS[id].giver === npc.def.id && this.quests.state[id] === 'active');
    if (ready.length) {
      const q = QUESTS[ready[0]];
      this.ui.showDialogue(npc, { text: `${prefix}<i>You have done what was asked.</i><br><b>${q.title}</b> — complete?`, choices: [{ label: 'Hand it over ✔', primary: true, action: () => { this.quests.onTalk(npc.def.id); this._endDialogue(); } }] });
    } else if (available.length) {
      const q = QUESTS[available[0]];
      this.ui.showDialogue(npc, { text: `${prefix}<b>${q.title}</b><br>${q.summary}`, choices: [
        { label: 'Accept Quest', primary: true, action: () => { this.quests.accept(available[0]); this._endDialogue(); } },
        { label: 'Maybe later', action: () => this._endDialogue() }] });
    } else if (active.length) {
      this.ui.showDialogue(npc, { text: `${prefix}<i>"You still have work to do. Return when it is done."</i>`, choices: [{ label: 'Farewell', primary: true, action: () => this._endDialogue() }] });
    } else {
      this.ui.showDialogue(npc, { text: `${prefix}<i>"Walk well, Unmarked."</i>`, choices: [{ label: 'Farewell', primary: true, action: () => { this.quests.onTalk(npc.def.id); this._endDialogue(); } }] });
    }
  }
  _endDialogue() { this.ui.hideDialogue(); this._dlgNPC = null; }

  // =========================================================================
  // Death / items / unlocks
  // =========================================================================
  onPlayerDeath() { this.ui.showDeath(); }
  respawnPlayer() { this.ui.hideDeath(); this.player.respawn(); this.ui.refreshHUD(); }

  useItem(id) {
    const p = this.player; if (!p.itemCount(id)) return;
    const it = ITEMS[id]; if (it.type !== 'consumable') return;
    if (it.heal) p.heal(it.heal);
    if (it.mana) p.restoreMana(it.mana);
    p.removeItem(id);
    this.audio.sfx('heal');
    this.ui.toast(`Used ${it.name}`, 'pickup');
  }

  onPanelClosed() { this.saveSettings(); }
  applyQuality() { /* 2D: quality toggle reserved for future post-fx */ }

  _refreshUnlocks() {
    const p = this.player; if (!p) return;
    const u = this.unlockedRegions, q = this.quests.state;
    u.add('mistwood');
    if (q.trialBronze === 'completed' || p.level >= 4) u.add('temple');
    if (p.level >= 5) u.add('ruins');
    if (p.level >= 6) u.add('citadel');
    if (q.cinderMaw === 'completed' || p.level >= 7) u.add('caves');
    if (p.level >= 9) u.add('godreach');
  }

  // =========================================================================
  // Input wiring
  // =========================================================================
  _bindKeys() {
    this.input.onKey = (code) => {
      if (this.state === 'intro') { this.intro.finish(); return; }
      if (this.state !== 'playing') return;
      if (code === 'Escape') {
        if (this._dlgNPC) this._endDialogue();
        else if (this.ui.activePanel) this.ui.closePanel();
        else this.ui.openPause();
        return;
      }
      if (this._dlgNPC) return;
      switch (code) {
        case 'Tab': this.ui.togglePanel('quests'); break;
        case 'KeyI': this.ui.togglePanel('inventory'); break;
        case 'KeyK': this.ui.togglePanel('skills'); break;
        case 'KeyM': this.ui.togglePanel('map'); break;
        case 'KeyB': if (this.ui.activePanel) this.ui.closePanel(); else this.ui.openBestiary(); break;
        default: break;
      }
    };
    // Any click skips the intro.
    window.addEventListener('pointerdown', () => { if (this.state === 'intro') this.intro.finish(); });
  }

  _bindWindow() {
    window.addEventListener('resize', () => this._resize());
    window.addEventListener('blur', () => { if (this.state === 'playing' && !this.ui.isBlocking()) this.ui.openPause(); });
  }

  _armAudioGesture() {
    const start = () => {
      this.audio.start();
      this.audio.setMusicVolume(this.settings.musicVolume);
      this.audio.setSfxVolume(this.settings.sfxVolume);
      this.audio.setMuted(this.settings.muted);
      if (this.state === 'menu' || this.state === 'intro') this.audio.playMusic('menu');
      window.removeEventListener('pointerdown', start);
      window.removeEventListener('keydown', start);
    };
    window.addEventListener('pointerdown', start);
    window.addEventListener('keydown', start);
  }

  // =========================================================================
  // Loop
  // =========================================================================
  _loop() {
    requestAnimationFrame(() => this._loop());
    const now = performance.now();
    const dt = Math.min(0.05, (now - this._lastTime) / 1000);
    this._lastTime = now;
    this.elapsed += dt;

    const blocked = this.ui.isBlocking() || this.player?.dead;

    if (this.state === 'intro' || this.state === 'menu') {
      this.intro.update(dt);
    } else if (this.state === 'playing' && !blocked) {
      this._handleInput();
      this.player.update(dt);
      for (let i = this.enemies.length - 1; i >= 0; i--) {
        const e = this.enemies[i]; e.update(dt, this.elapsed);
        if (e.dead && e.deathTimer <= 0) this.enemies.splice(i, 1);
      }
      for (const n of this.npcs) n.update(dt, this.elapsed);
      this.magic.update(dt);
      this.map.update(dt);
      this._updateInteraction();
      this._refreshUnlocks();
      this._dripRespawn(dt);
      this._computeCamera();
    }

    this.ui.update(dt);
    this._render();
    this.input.lateUpdate();
  }

  _handleInput() {
    const inp = this.input;
    for (let i = 0; i < 6; i++) if (inp.wasPressed('Digit' + (i + 1)) || inp.wasPressed('Numpad' + (i + 1))) this.player.tryCastSlot(i);
    if (inp.mouseWasPressed(0)) this.player.melee();       // left click = blade
    if (inp.mouseWasPressed(2)) this.player.tryCastSlot(0); // right click = slot 1
    if (inp.wasPressed('KeyE')) this._tryInteract();
  }

  _tryInteract() {
    let near = null, best = 30;
    for (const n of this.npcs) { const d = n.distanceTo(this.player.x, this.player.y); if (d < best) { best = d; near = n; } }
    if (near) near.interact();
  }

  _updateInteraction() {
    let near = null, best = 30;
    for (const n of this.npcs) { const d = n.distanceTo(this.player.x, this.player.y); if (d < best) { best = d; near = n; } }
    if (near) this.ui.showInteract(`<b>E</b> — Speak with ${near.def.name.split(',')[0]}`);
    else this.ui.hideInteract();

    for (let i = this.map.pickups.length - 1; i >= 0; i--) {
      const pk = this.map.pickups[i];
      if (Math.hypot(pk.x - this.player.x, pk.y - this.player.y) < 14) this._collect(pk);
    }
  }

  _collect(pk) {
    this.map.removePickup(pk);
    this.player.addItem(pk.item);
    this.quests.onCollect(pk.item);
    this.audio.sfx('pickup');
    if (pk.type === 'asphodel') this.player.restoreMana(8);
    this.ui.toast(`Gathered ${ITEMS[pk.item].name}`, 'pickup');
  }

  _dripRespawn(dt) {
    this._enemyTimer -= dt;
    const alive = this.enemies.filter((e) => !e.dead).length;
    if (this._enemyTimer <= 0 && alive < TARGET_ENEMIES) {
      this._enemyTimer = randRange(4, 8);
      this._spawnEnemyAway(REGIONS[this.currentRegion], 200);
    }
  }

  _computeCamera() {
    if (!this.player) return;
    let cx = this.player.x - this.vw / 2, cy = this.player.y - this.vh / 2;
    cx = clamp(cx, 0, Math.max(0, this.map.pixelW - this.vw));
    cy = clamp(cy, 0, Math.max(0, this.map.pixelH - this.vh));
    this.camX = Math.round(cx); this.camY = Math.round(cy);
  }

  // =========================================================================
  // Render
  // =========================================================================
  _render() {
    const b = this.bctx;
    if (this.state === 'playing' || (this.state === 'loading' && this.player)) {
      this._renderWorld(b);
    } else {
      this.intro.draw(b);
    }
    // Blit the virtual buffer to fill the whole window (crisp, no smoothing).
    this.ctx.imageSmoothingEnabled = false;
    this.ctx.drawImage(this.buffer, 0, 0, this.canvas.width, this.canvas.height);
  }

  _renderWorld(b) {
    const cx = this.camX, cy = this.camY;
    b.fillStyle = this.map.region.ground.base;
    b.fillRect(0, 0, this.vw, this.vh);
    this.map.drawGround(b, cx, cy);
    this.map.drawPickups(b, cx, cy);

    // Y-sorted props + entities for correct overlap.
    const list = [];
    for (const p of this.map.props) list.push(p);
    for (const e of this.enemies) list.push(e);
    for (const n of this.npcs) list.push(n);
    list.push(this.player);
    list.sort((a, z) => a.baseline - z.baseline);
    for (const o of list) {
      if (typeof o.draw === 'function') o.draw(b, cx, cy);
      else this.map.drawProp(b, o, cx, cy);
    }

    this.magic.draw(b, cx, cy);

    // Lighting: spell glows + the player's torch.
    const lights = this.magic.getLights();
    lights.push({ x: this.player.x, y: this.player.y - 6, r: this.player.torchRadius, color: this.map.region.lightTint, intensity: 0.16 });
    if (this.player.castFlash > 0) lights.push({ x: this.player.x, y: this.player.y - 8, r: 30, color: this.player.castColor, intensity: 0.4 });
    this.map.drawLighting(b, cx, cy, lights);
  }
}
