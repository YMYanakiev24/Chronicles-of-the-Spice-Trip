import * as THREE from 'three';
import { Entity } from '../entities/Entity.js';

export class Player extends Entity {
  constructor(scene) {
    super({ name: 'Hero', maxHealth: 100 });

    this.scene = scene;

    // Stats
    this.maxMana = 100;
    this.mana = 100;
    this.manaRegen = 5; // per second
    this.speed = 7;
    this.runSpeed = 12;
    this.jumpForce = 8;

    this.level = 1;
    this.xp = 0;
    this.xpToNextLevel = 200;
    this.gold = 0;

    // State
    this._onGround = true;
    this._velY = 0;
    this._shieldActive = false;
    this._shieldDamageReduction = 0;
    this._shieldTimer = 0;
    this._buffs = {};
    this._footstepTimer = 0;

    // Invulnerability frames after taking damage
    this._iFrames = 0;

    this._buildMesh();
  }

  _buildMesh() {
    const group = new THREE.Group();

    // Body parts
    const bodyMat = new THREE.MeshLambertMaterial({ color: 0x2a1a0a });
    const armorMat = new THREE.MeshLambertMaterial({ color: 0x4a7a8a });
    const skinMat = new THREE.MeshLambertMaterial({ color: 0xd4a870 });
    const cloakMat = new THREE.MeshLambertMaterial({ color: 0x2a0a4a });

    // Legs
    [-0.22, 0.22].forEach((xOff, i) => {
      const leg = new THREE.Mesh(new THREE.BoxGeometry(0.28, 0.6, 0.28), armorMat);
      leg.position.set(xOff, 0.3, 0);
      leg.castShadow = true;
      group.add(leg);

      const boot = new THREE.Mesh(new THREE.BoxGeometry(0.3, 0.2, 0.36), bodyMat);
      boot.position.set(xOff, -0.05, 0.04);
      group.add(boot);
    });

    // Torso
    const torso = new THREE.Mesh(new THREE.BoxGeometry(0.7, 0.75, 0.4), armorMat);
    torso.position.y = 0.97;
    torso.castShadow = true;
    group.add(torso);

    // Cloak
    const cloak = new THREE.Mesh(new THREE.BoxGeometry(0.72, 0.9, 0.15), cloakMat);
    cloak.position.set(0, 0.9, -0.24);
    group.add(cloak);

    // Arms
    [-0.46, 0.46].forEach((xOff, i) => {
      const arm = new THREE.Mesh(new THREE.BoxGeometry(0.25, 0.55, 0.25), armorMat);
      arm.position.set(xOff, 0.95, 0);
      arm.castShadow = true;
      group.add(arm);

      const hand = new THREE.Mesh(new THREE.BoxGeometry(0.22, 0.22, 0.22), skinMat);
      hand.position.set(xOff, 0.64, 0);
      group.add(hand);
    });

    // Neck
    const neck = new THREE.Mesh(new THREE.CylinderGeometry(0.1, 0.12, 0.2, 6), skinMat);
    neck.position.y = 1.42;
    group.add(neck);

    // Head
    const head = new THREE.Mesh(new THREE.BoxGeometry(0.46, 0.46, 0.44), skinMat);
    head.position.y = 1.73;
    head.castShadow = true;
    group.add(head);

    // Helmet
    const helmet = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.28, 0.48), new THREE.MeshLambertMaterial({ color: 0x556677 }));
    helmet.position.y = 1.88;
    group.add(helmet);

    // Helmet crest
    const crest = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.22, 0.42), new THREE.MeshLambertMaterial({ color: 0x993322 }));
    crest.position.y = 2.06;
    group.add(crest);

    // Eyes
    const eyeMat = new THREE.MeshBasicMaterial({ color: 0x88aaff });
    [-0.1, 0.1].forEach(ex => {
      const eye = new THREE.Mesh(new THREE.SphereGeometry(0.045, 6, 6), eyeMat);
      eye.position.set(ex, 1.74, 0.23);
      group.add(eye);
    });

    // Weapon — magical staff / sword hybrid
    const weaponGroup = new THREE.Group();
    weaponGroup.position.set(0.55, 0.7, 0.2);
    weaponGroup.rotation.z = -0.2;
    weaponGroup.rotation.x = 0.3;

    const blade = new THREE.Mesh(new THREE.BoxGeometry(0.07, 1.1, 0.04), new THREE.MeshLambertMaterial({ color: 0xaaccdd }));
    blade.position.y = 0.55;
    weaponGroup.add(blade);

    const hilt = new THREE.Mesh(new THREE.BoxGeometry(0.3, 0.07, 0.07), new THREE.MeshLambertMaterial({ color: 0xddaa00 }));
    hilt.position.y = 0.02;
    weaponGroup.add(hilt);

    const gem = new THREE.Mesh(new THREE.SphereGeometry(0.07, 8, 8), new THREE.MeshBasicMaterial({ color: 0x44aaff }));
    gem.position.y = 1.15;
    const gemLight = new THREE.PointLight(0x44aaff, 2, 4);
    gemLight.position.copy(gem.position);
    weaponGroup.add(gem);
    weaponGroup.add(gemLight);

    group.add(weaponGroup);
    this._weaponGroup = weaponGroup;

    // Shield sphere when active
    this._shieldMesh = new THREE.Mesh(
      new THREE.SphereGeometry(1.2, 16, 16),
      new THREE.MeshBasicMaterial({ color: 0x4488ff, transparent: true, opacity: 0, side: THREE.BackSide, wireframe: false })
    );
    group.add(this._shieldMesh);

    // Shadow
    const shadowGeom = new THREE.CircleGeometry(0.5, 12);
    const shadowMat = new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.3 });
    const shadowMesh = new THREE.Mesh(shadowGeom, shadowMat);
    shadowMesh.rotation.x = -Math.PI / 2;
    shadowMesh.position.y = 0.01;
    group.add(shadowMesh);

    group.position.set(0, 1, 0);
    this.mesh = group;
    this.scene.add(group);
  }

  update(delta, input, camera, worldBuilder, audio) {
    this._iFrames = Math.max(0, this._iFrames - delta);

    // Mana regeneration
    const manaRegenRate = this.manaRegen * (1 + (this._buffs.manaRegen || 0) - 1);
    this.mana = Math.min(this.maxMana, this.mana + manaRegenRate * delta);

    // Shield timer
    if (this._shieldActive) {
      this._shieldTimer -= delta;
      if (this._shieldTimer <= 0) {
        this._shieldActive = false;
        this._shieldDamageReduction = 0;
        this._shieldMesh.material.opacity = 0;
      } else {
        this._shieldMesh.material.opacity = 0.15 + Math.sin(performance.now() * 0.005) * 0.05;
      }
    }

    // Buff timers
    Object.keys(this._buffs).forEach(stat => {
      if (this._buffs[stat]?.duration !== undefined) {
        this._buffs[stat].duration -= delta;
        if (this._buffs[stat].duration <= 0) delete this._buffs[stat];
      }
    });

    this._handleMovement(delta, input, camera, worldBuilder, audio);
    this._animateWeapon(delta, input);
    this.position.copy(this.mesh.position);
  }

  _handleMovement(delta, input, camera, worldBuilder, audio) {
    // WASD movement relative to camera
    const forward = new THREE.Vector3();
    camera.getWorldDirection(forward);
    forward.y = 0;
    forward.normalize();

    const right = new THREE.Vector3();
    right.crossVectors(forward, new THREE.Vector3(0, 1, 0)).normalize();

    const move = new THREE.Vector3();
    if (input.isDown('W') || input.isDown('ARROWUP')) move.add(forward);
    if (input.isDown('S') || input.isDown('ARROWDOWN')) move.sub(forward);
    if (input.isDown('A') || input.isDown('ARROWLEFT')) move.sub(right);
    if (input.isDown('D') || input.isDown('ARROWRIGHT')) move.add(right);

    const running = input.isDown('SHIFT');
    const spd = (running ? this.runSpeed : this.speed) * (this._buffs.speed?.value || 1);

    if (move.length() > 0) {
      move.normalize().multiplyScalar(spd * delta);
      this.mesh.position.add(move);

      // Rotate player to face movement direction
      const angle = Math.atan2(move.x, move.z);
      this.mesh.rotation.y = angle;

      // Footstep sounds
      this._footstepTimer -= delta;
      if (this._footstepTimer <= 0) {
        audio?.playFootstep();
        this._footstepTimer = running ? 0.28 : 0.45;
      }
    }

    // Jump
    if (input.isDown(' ') && this._onGround) {
      this._velY = this.jumpForce;
      this._onGround = false;
    }

    // Gravity
    this._velY -= 20 * delta;
    this.mesh.position.y += this._velY * delta;

    // Terrain height clamping
    const terrainH = worldBuilder.getTerrainHeight(this.mesh.position.x, this.mesh.position.z);
    if (this.mesh.position.y <= terrainH) {
      this.mesh.position.y = terrainH;
      this._velY = 0;
      this._onGround = true;
    }

    // World bounds
    const halfSize = 95;
    this.mesh.position.x = Math.max(-halfSize, Math.min(halfSize, this.mesh.position.x));
    this.mesh.position.z = Math.max(-halfSize, Math.min(halfSize, this.mesh.position.z));
  }

  _animateWeapon(delta, input) {
    if (!this._weaponGroup) return;
    const t = performance.now() * 0.001;
    this._weaponGroup.rotation.y = Math.sin(t * 1.5) * 0.05;
    // Attack swing
    if (input.isMouseDown(0)) {
      this._weaponGroup.rotation.x = Math.sin(t * 8) * 0.4 + 0.3;
    } else {
      this._weaponGroup.rotation.x += (0.3 - this._weaponGroup.rotation.x) * 0.1;
    }
  }

  takeDamage(amount, source) {
    if (this._iFrames > 0) return 0;
    if (!this.alive) return 0;

    let actual = amount;
    if (this._shieldActive) actual *= (1 - this._shieldDamageReduction);
    actual = Math.round(actual);

    this._iFrames = 0.6; // brief invulnerability

    return super.takeDamage(actual, source);
  }

  heal(amount) {
    const actual = super.heal(amount);
    return actual;
  }

  activateShield(duration, reduction) {
    this._shieldActive = true;
    this._shieldTimer = duration;
    this._shieldDamageReduction = reduction;
  }

  addBuff(stat, value, duration) {
    this._buffs[stat] = { value, duration };
  }

  gainXP(amount) {
    this.xp += amount;
    let leveled = false;
    while (this.xp >= this.xpToNextLevel) {
      this.xp -= this.xpToNextLevel;
      this.level++;
      this.xpToNextLevel = Math.floor(this.xpToNextLevel * 1.5);
      this.maxHealth += 15;
      this.health = Math.min(this.health + 30, this.maxHealth);
      this.maxMana += 10;
      this.mana = Math.min(this.mana + 20, this.maxMana);
      leveled = true;
    }
    return leveled;
  }

  serialize() {
    return {
      position: { x: this.mesh.position.x, y: this.mesh.position.y, z: this.mesh.position.z },
      health: this.health, maxHealth: this.maxHealth,
      mana: this.mana, maxMana: this.maxMana,
      level: this.level, xp: this.xp, xpToNextLevel: this.xpToNextLevel,
      gold: this.gold
    };
  }

  load(data) {
    if (!data) return;
    this.mesh.position.set(data.position.x, data.position.y, data.position.z);
    this.health = data.health;
    this.maxHealth = data.maxHealth;
    this.mana = data.mana;
    this.maxMana = data.maxMana;
    this.level = data.level;
    this.xp = data.xp;
    this.xpToNextLevel = data.xpToNextLevel;
    this.gold = data.gold || 0;
  }
}
