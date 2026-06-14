import * as THREE from 'three';
import { WorldBuilder } from '../world/WorldBuilder.js';
import { Player } from '../player/Player.js';
import { ThirdPersonCamera } from '../player/ThirdPersonCamera.js';
import { SpellSystem } from '../magic/SpellSystem.js';
import { ParticleSystem } from '../magic/Particles.js';
import { Enemy } from '../entities/Enemy.js';
import { NPC } from '../entities/NPC.js';
import { QuestSystem } from '../systems/QuestSystem.js';
import { Inventory } from '../systems/Inventory.js';
import { HUD } from '../ui/HUD.js';
import { DialogueSystem } from '../ui/Dialogue.js';
import { NPCS } from '../data/npcs.js';
import { CREATURES } from '../data/creatures.js';

export class GameScene {
  constructor(engine, saveData) {
    this.engine = engine;
    this.saveData = saveData;

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.1, 500);

    this._time = 0;
    this._paused = false;
    this._waveActive = false;
    this._waveEnemies = [];
    this._waveNumber = 0;

    this.enemies = [];
    this.npcs = [];
    this._nearbyNPC = null;
    this._ambientUpdaters = [];
  }

  init() {
    this._setupSystems();
    this._buildWorld();
    this._spawnNPCs();
    this._spawnEnemies();
    this._setupInput();
    this._showHUD();

    // Apply save if continuing
    if (this.saveData) {
      this.player.load(this.saveData.player);
      this.questSystem.load(this.saveData.quests);
      this.inventory.load(this.saveData.inventory);
    }

    // Tutorial notification
    setTimeout(() => {
      this.hud.showToast('Welcome to the Emerald Realm! Speak with Sentinel Gareth to begin your journey.', 'quest');
    }, 1500);

    setTimeout(() => {
      this.hud.showToast('Controls: WASD move • Q/E/R/T/G spells • F interact • I inventory • J quests', '');
    }, 5000);
  }

  _setupSystems() {
    // World
    this.worldBuilder = new WorldBuilder(this.scene);

    // Player
    this.player = new Player(this.scene);
    this.player.mesh.position.set(0, 1, 10);
    this.player.on('damage', () => {
      this.hud.showDamageFlash();
      this.engine.audio.playHit();
    });
    this.player.on('death', () => this._onPlayerDeath());
    this.player.on('heal', () => this.hud.showHealFlash());

    // Camera
    this.tpCamera = new ThirdPersonCamera(this.camera, this.engine.settings);

    // Spell system
    this.spellSystem = new SpellSystem(this.scene, this.engine.audio);

    // Ambient particles
    this.ambientParticles = new ParticleSystem(this.scene);

    // UI
    this.hud = new HUD();

    // Quest system
    this.questSystem = new QuestSystem(this.hud, this.engine.audio);
    this.questSystem.on('questCompleted', (id) => this._onQuestComplete(id));

    // Inventory
    this.inventory = new Inventory(this.hud, this.engine.audio);

    // Dialogue
    this.dialogue = new DialogueSystem(this.questSystem, this.inventory, this.engine.audio);
  }

  _buildWorld() {
    this.worldBuilder.build();

    // Add ambient particles in forest area
    const forestAmbient = this.ambientParticles.createAmbient(
      new THREE.Vector3(-30, 0, -20),
      { count: 80, color: 0x44ff88, size: 0.07, radius: 25, height: 10 }
    );
    const templeAmbient = this.ambientParticles.createAmbient(
      new THREE.Vector3(-35, 5, -20),
      { count: 50, color: 0xffdd44, size: 0.08, radius: 15, height: 8 }
    );
    this._ambientUpdaters.push(forestAmbient, templeAmbient);
  }

  _spawnNPCs() {
    Object.keys(NPCS).forEach(id => {
      const npc = new NPC(id, this.scene, this.worldBuilder);
      this.npcs.push(npc);
    });
  }

  _spawnEnemies() {
    // Shadow Wraiths in northeast
    for (let i = 0; i < 8; i++) {
      const x = 25 + (Math.random() - 0.5) * 20;
      const z = -15 + (Math.random() - 0.5) * 20;
      const y = this.worldBuilder.getTerrainHeight(x, z);
      this._spawnEnemy('shadow_wraith', new THREE.Vector3(x, y, z));
    }

    // Stone Golems near temple
    for (let i = 0; i < 4; i++) {
      const x = -30 + (Math.random() - 0.5) * 15;
      const z = -25 + (Math.random() - 0.5) * 15;
      const y = this.worldBuilder.getTerrainHeight(x, z);
      this._spawnEnemy('stone_golem', new THREE.Vector3(x, y, z));
    }

    // Dark Mages scattered
    for (let i = 0; i < 5; i++) {
      const angle = Math.random() * Math.PI * 2;
      const r = 30 + Math.random() * 25;
      const x = Math.cos(angle) * r;
      const z = Math.sin(angle) * r - 10;
      const y = this.worldBuilder.getTerrainHeight(x, z);
      this._spawnEnemy('dark_mage', new THREE.Vector3(x, y, z));
    }

    // Basilisk boss at ruins
    const bossY = this.worldBuilder.getTerrainHeight(35, 0);
    this._spawnEnemy('basilisk', new THREE.Vector3(38, bossY, -5));

    // Dark Treant in northern forest
    const treantY = this.worldBuilder.getTerrainHeight(5, -30);
    this._spawnEnemy('dark_treant', new THREE.Vector3(5, treantY, -30));

    // Corrupted Treants
    for (let i = 0; i < 3; i++) {
      const x = -5 + (Math.random() - 0.5) * 15;
      const z = -20 + (Math.random() - 0.5) * 15;
      const y = this.worldBuilder.getTerrainHeight(x, z);
      this._spawnEnemy('dark_treant', new THREE.Vector3(x, y, z));
    }
  }

  _spawnEnemy(typeId, position) {
    const enemy = new Enemy(typeId, position, this.scene);
    enemy.on('death', () => this._onEnemyDeath(enemy));
    // Melee attack event
    if (!enemy.def.ranged) {
      enemy.on('attack', () => {
        const dist = enemy.mesh.position.distanceTo(this.player.mesh.position);
        if (dist < enemy.attackRange + 0.8) {
          const actual = this.player.takeDamage(enemy.damage);
          if (actual > 0) this.engine.audio.playHit();
        }
      });
    }
    this.enemies.push(enemy);
    return enemy;
  }

  _setupInput() {
    const input = this.engine.input;

    // Pointer lock on click (when not in UI)
    document.getElementById('gameCanvas').addEventListener('click', () => {
      if (!this.dialogue.isOpen && !this._paused) {
        input.requestPointerLock();
        this.engine.audio.resume();
      }
    });

    // Spell casting
    input.on('keydown', (key) => {
      if (this.dialogue.isOpen || this._paused) return;

      if (['Q', 'E', 'R', 'T', 'G'].includes(key)) {
        const result = this.spellSystem.cast(key, this.player, this.camera);
        if (!result && this.player.mana < (this.spellSystem.SPELLS[key]?.manaCost || 999)) {
          this.hud.showToast('Not enough mana!', 'combat');
        }
        // Nova: area damage
        if (key === 'G' && result) {
          this._processNovaDamage();
        }
      }

      if (key === 'F') {
        if (this.dialogue.isOpen) {
          this.dialogue.close();
        } else if (this._nearbyNPC) {
          this.dialogue.open(this._nearbyNPC);
        }
      }

      if (key === 'I') {
        const panel = document.getElementById('inventoryPanel');
        if (panel?.classList.contains('hidden')) {
          this.hud.refreshInventory(this.inventory);
          panel.classList.remove('hidden');
          input.exitPointerLock();
        } else {
          panel?.classList.add('hidden');
          if (!this.dialogue.isOpen) input.requestPointerLock();
        }
      }

      if (key === 'J') {
        const panel = document.getElementById('questLog');
        if (panel?.classList.contains('hidden')) {
          this.hud.refreshQuestLog(this.questSystem);
          panel.classList.remove('hidden');
          input.exitPointerLock();
        } else {
          panel?.classList.add('hidden');
          if (!this.dialogue.isOpen) input.requestPointerLock();
        }
      }

      if (key === 'ESCAPE') {
        if (this.dialogue.isOpen) { this.dialogue.close(); return; }
        if (!this._paused) this._pauseGame();
        else this._resumeGame();
      }

      if (key === 'M') {
        // Quick save
        this._saveGame();
        this.hud.showToast('Game saved!', 'quest');
      }
    });

    // Mouse scroll for camera zoom
    window.addEventListener('wheel', e => {
      this.tpCamera.handleScroll(e.deltaY);
    });

    // Panel close buttons
    document.getElementById('closeInventory')?.addEventListener('click', () => {
      document.getElementById('inventoryPanel')?.classList.add('hidden');
      input.requestPointerLock();
    });

    document.getElementById('closeQuestLog')?.addEventListener('click', () => {
      document.getElementById('questLog')?.classList.add('hidden');
      input.requestPointerLock();
    });

    // Quest log tabs
    document.querySelectorAll('.tab-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        this.hud.refreshQuestLog(this.questSystem);
      });
    });

    // Pause menu
    document.getElementById('btnResume')?.addEventListener('click', () => this._resumeGame());
    document.getElementById('btnPauseSettings')?.addEventListener('click', () => {
      document.getElementById('settingsPanel')?.classList.remove('hidden');
    });
    document.getElementById('btnMainMenu')?.addEventListener('click', () => {
      this._saveGame();
      this.dispose();
      this.engine.audio.stopMusic();
      window.location.reload();
    });
    document.getElementById('saveSettings')?.addEventListener('click', () => {
      this.engine.saveSettings();
      document.getElementById('settingsPanel')?.classList.add('hidden');
    });
  }

  _showHUD() {
    this.hud.show();
    this.hud.updateStats(this.player);
    this.hud.updateQuestTracker(this.questSystem);
  }

  update(delta) {
    if (this._paused) return;
    this._time += delta;

    // Player movement
    const { dx, dy } = this.engine.input.consumeDelta();
    this.tpCamera.handleMouseMove(dx, dy);
    this.player.update(delta, this.engine.input, this.camera, this.worldBuilder, this.engine.audio);

    // Camera
    this.tpCamera.update(delta, this.player.mesh.position);

    // Spell system
    this.spellSystem.update(delta);

    // Check spell hits
    const hits = this.spellSystem.getProjectileHit(this.enemies.filter(e => e.alive));
    hits.forEach(hit => {
      const dmg = hit.damage * (1 + (this.inventory.getBonus('spellDamage') || 0));
      const actual = hit.enemy.takeDamage(dmg, hit.spellType);
      if (actual > 0) this.engine.audio.playHit();
    });

    // Enemy updates + ranged projectile damage
    this.enemies.forEach(enemy => {
      if (!enemy.alive) return;
      enemy.scene = this.scene;
      enemy.update(delta, this.player.mesh.position);

      // Ranged projectile hit check
      if (enemy.def.ranged) {
        const dmg = enemy.checkProjectileHit(this.player.mesh.position);
        if (dmg > 0) {
          const actual = this.player.takeDamage(dmg);
          if (actual > 0) this.engine.audio.playHit();
        }
      }
    });

    // NPC proximity
    this._checkNPCProximity();

    // Location triggers
    this._checkLocationTriggers();

    // Ambient particle update
    this._ambientUpdaters.forEach(u => u.update(delta));

    // Collectible items update
    this._collectibles?.forEach(c => c.update(delta));

    // Castle animation
    this.worldBuilder.updateCastle(this._time);

    // NPC idle animations
    this.npcs.forEach(npc => npc.update(delta, this._time));

    // HUD updates
    this.hud.updateStats(this.player);
    this.hud.updateSpellCooldowns(this.spellSystem);

    // Auto-save every 60 seconds
    if (Math.floor(this._time) % 60 === 0 && Math.floor(this._time) !== this._lastAutoSave) {
      this._lastAutoSave = Math.floor(this._time);
      this._saveGame();
    }
  }

  _checkNPCProximity() {
    let nearest = null;
    let nearestDist = Infinity;

    this.npcs.forEach(npc => {
      const dist = npc.position.distanceTo(this.player.mesh.position);
      if (dist < npc.interactionRadius && dist < nearestDist) {
        nearest = npc;
        nearestDist = dist;
      }
    });

    if (nearest && !this.dialogue.isOpen) {
      this._nearbyNPC = nearest;
      const questId = nearest.def.questId;
      const isQuestGiver = questId && (
        this.questSystem.quests[questId]?.status === 'available' ||
        this.questSystem.quests[questId]?.status === 'active'
      );
      this.hud.showInteractPrompt(
        isQuestGiver ? `Speak with ${nearest.def.name}` : nearest.def.name
      );
    } else if (!this.dialogue.isOpen) {
      this._nearbyNPC = null;
      this.hud.hideInteractPrompt();
    }
  }

  _checkLocationTriggers() {
    this.worldBuilder.interactables.forEach(loc => {
      if (loc.type === 'location') {
        const dist = this.player.mesh.position.distanceTo(loc.position);
        if (dist < loc.radius) {
          this.questSystem.reportLocation(loc.id);
        }
      }
    });
  }

  _processNovaDamage() {
    const novaRadius = this.spellSystem.SPELLS.G.blastRadius;
    const novaDamage = this.spellSystem.SPELLS.G.damage;

    this.enemies.forEach(enemy => {
      if (!enemy.alive) return;
      const dist = enemy.mesh.position.distanceTo(this.player.mesh.position);
      if (dist < novaRadius + 1) {
        enemy.takeDamage(novaDamage, 'ultimate');
      }
    });
  }

  _onEnemyDeath(enemy) {
    this.engine.audio.playEnemyDeath();
    const xpGain = enemy.def.xpReward;
    const leveled = this.player.gainXP(xpGain);
    this.hud.showToast(`+${xpGain} XP`, 'item');

    if (leveled) {
      this.engine.audio.playLevelUp();
      this.hud.showLevelUp(this.player.level);
      this.hud.showToast(`Level Up! Now Level ${this.player.level}`, 'level');
    }

    // Quest kill report
    this.questSystem.reportKill(enemy.typeId);

    // Drop items
    if (enemy.def.drops) {
      enemy.def.drops.forEach(drop => {
        if (Math.random() < drop.chance) {
          this.inventory.addItem({
            id: drop.id, name: drop.name, icon: drop.icon,
            desc: `Dropped by ${enemy.def.name}`, bonus: {}
          });
        }
      });
    }

    // Special: collect codex if in ruins area
    if (enemy.typeId === 'shadow_wraith') {
      const killedCount = this.questSystem.quests.stolen_codex?.objectives.find(o => o.id === 'kill_wraiths')?.progress || 0;
      if (killedCount >= 5) {
        // Spawn codex pickup near ruins
        this._spawnCollectible('arcane_codex', 'Arcane Codex', '📖', enemy.mesh.position);
      }
    }

    // Particle burst at death
    this.spellSystem.particles.burst(
      enemy.mesh.position.clone().add(new THREE.Vector3(0, 1, 0)),
      { count: 30, color: enemy.def.emissiveColor || 0x660033, size: 0.15, life: 0.8, speed: 5 }
    );
  }

  _spawnCollectible(id, name, icon, position) {
    const geom = new THREE.BoxGeometry(0.5, 0.5, 0.5);
    const mat = new THREE.MeshBasicMaterial({ color: 0xffdd44 });
    const mesh = new THREE.Mesh(geom, mat);
    mesh.position.copy(position).add(new THREE.Vector3(0, 0.5, 0));
    const light = new THREE.PointLight(0xffdd44, 2, 5);
    mesh.add(light);
    this.scene.add(mesh);

    const collectible = {
      id, name, icon, mesh,
      collected: false,
      update: (delta) => {
        if (collectible.collected) return;
        mesh.rotation.y += delta * 2;
        mesh.position.y = position.y + 0.5 + Math.sin(this._time * 2) * 0.2;

        const dist = mesh.position.distanceTo(this.player.mesh.position);
        if (dist < 2) {
          collectible.collected = true;
          this.scene.remove(mesh);
          this.questSystem.reportCollect(id);
          this.inventory.addItem({ id, name, icon, desc: 'A legendary artifact', bonus: {} });
        }
      }
    };

    this._collectibles = this._collectibles || [];
    this._collectibles.push(collectible);
  }

  _onPlayerDeath() {
    this.engine.audio.stopMusic();
    this.hud.showToast('You have fallen... Respawning...', 'combat');
    setTimeout(() => {
      this.player.health = this.player.maxHealth * 0.5;
      this.player.mana = this.player.maxMana * 0.5;
      this.player.mesh.position.set(0, 2, 10);
      this.player.alive = true;
      this.engine.audio.playAmbientMusic();
      this.hud.showToast('Restored at the village shrine', 'quest');
    }, 2500);
  }

  _onQuestComplete(questId) {
    const quest = this.questSystem.quests[questId];
    if (!quest) return;

    // Grant rewards
    if (quest.rewards?.xp) {
      const leveled = this.player.gainXP(quest.rewards.xp);
      this.hud.showToast(`+${quest.rewards.xp} XP rewarded`, 'level');
      if (leveled) {
        this.engine.audio.playLevelUp();
        this.hud.showLevelUp(this.player.level);
      }
    }

    if (quest.rewards?.gold) {
      this.player.gold += quest.rewards.gold;
      this.hud.showToast(`+${quest.rewards.gold} Gold`, 'item');
    }

    if (quest.rewards?.items) {
      quest.rewards.items.forEach(item => {
        this.inventory.addItem(item);
      });
    }

    // Mark NPC as quest-complete
    const npc = this.npcs.find(n => n.def.questId === questId);
    npc?.markQuestComplete();

    // Save after quest completion
    this._saveGame();
  }

  _pauseGame() {
    this._paused = true;
    this.engine.input.exitPointerLock();
    document.getElementById('pauseMenu')?.classList.remove('hidden');
  }

  _resumeGame() {
    this._paused = false;
    document.getElementById('pauseMenu')?.classList.add('hidden');
    document.getElementById('settingsPanel')?.classList.add('hidden');
    this.engine.input.requestPointerLock();
  }

  _saveGame() {
    this.engine.setSaveData({
      player: this.player.serialize(),
      quests: this.questSystem.serialize(),
      inventory: this.inventory.serialize(),
      savedAt: new Date().toISOString()
    });
  }

  dispose() {
    document.getElementById('hud')?.classList.add('hidden');
    document.getElementById('pauseMenu')?.classList.add('hidden');
    document.getElementById('questLog')?.classList.add('hidden');
    document.getElementById('inventoryPanel')?.classList.add('hidden');
    document.getElementById('dialogueBox')?.classList.add('hidden');

    this.spellSystem.dispose();
    this.ambientParticles.dispose();
    this._ambientUpdaters.forEach(u => u.dispose?.());
    this.npcs.forEach(n => n.dispose());
    this.worldBuilder.dispose();
  }
}
