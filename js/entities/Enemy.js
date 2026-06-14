import * as THREE from 'three';
import { Entity } from './Entity.js';
import { CREATURES } from '../data/creatures.js';

export class Enemy extends Entity {
  constructor(typeId, position, scene) {
    const def = CREATURES[typeId];
    super({ name: def.name, maxHealth: def.hp });

    this.typeId = typeId;
    this.def = def;
    this.scene = scene;

    this.damage = def.damage;
    this.speed = def.speed;
    this.aggroRange = def.aggroRange;
    this.attackRange = def.attackRange;
    this.attackCooldown = def.attackCooldown;
    this._attackTimer = 0;
    this._aggrod = false;
    this._state = 'idle'; // idle, chase, attack, dead, returning
    this._spawnPos = position.clone();
    this._wanderTarget = position.clone();
    this._wanderTimer = 0;
    this._projectiles = []; // for ranged enemies
    this._petrifyTimer = 0;

    this._buildMesh(position);
    this._buildHealthBar();
  }

  _buildMesh(position) {
    const d = this.def;
    const group = new THREE.Group();
    group.position.copy(position);

    const bodyMat = new THREE.MeshLambertMaterial({
      color: d.color,
      emissive: d.emissiveColor,
      emissiveIntensity: 0.4
    });

    const body = new THREE.Mesh(new THREE.BoxGeometry(d.size.w, d.size.h * 0.6, d.size.d), bodyMat);
    body.position.y = d.size.h * 0.6;
    body.castShadow = true;
    group.add(body);

    // Head
    const head = new THREE.Mesh(new THREE.BoxGeometry(d.size.w * 0.7, d.size.h * 0.35, d.size.d * 0.7), bodyMat);
    head.position.y = d.size.h * 1.05;
    group.add(head);

    // Eyes (glowing)
    const eyeMat = new THREE.MeshBasicMaterial({ color: 0xff2200 });
    [-0.15, 0.15].forEach(ex => {
      const eye = new THREE.Mesh(new THREE.SphereGeometry(0.06, 6, 6), eyeMat);
      eye.position.set(ex * d.size.w, d.size.h * 1.1, d.size.d * 0.36);
      group.add(eye);
    });

    // Special visuals per type
    if (this.typeId === 'shadow_wraith') {
      // Smoky trail
      const fogMat = new THREE.MeshBasicMaterial({ color: 0x220044, transparent: true, opacity: 0.3 });
      const fog = new THREE.Mesh(new THREE.SphereGeometry(0.8, 8, 8), fogMat);
      fog.position.y = d.size.h * 0.4;
      group.add(fog);
    } else if (this.typeId === 'stone_golem') {
      // Rock texture patches
      const patchMat = new THREE.MeshLambertMaterial({ color: 0x888870 });
      for (let i = 0; i < 4; i++) {
        const patch = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.4, 0.1), patchMat);
        patch.position.set((Math.random() - 0.5) * d.size.w, d.size.h * (0.4 + Math.random() * 0.4), d.size.d * 0.5);
        group.add(patch);
      }
    } else if (this.typeId === 'basilisk') {
      // Tail
      const tailMat = new THREE.MeshLambertMaterial({ color: 0x224411 });
      for (let i = 0; i < 5; i++) {
        const seg = new THREE.Mesh(new THREE.CylinderGeometry(0.3 - i * 0.04, 0.35 - i * 0.04, 0.8, 6), tailMat);
        seg.position.set(0, d.size.h * 0.3, -(d.size.d * 0.5 + i * 0.8));
        group.add(seg);
      }
    } else if (this.typeId === 'dark_treant') {
      // Branch arms
      const branchMat = new THREE.MeshLambertMaterial({ color: 0x1a0a00 });
      [-1, 1].forEach(side => {
        const arm = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.25, 2, 6), branchMat);
        arm.position.set(side * d.size.w * 0.8, d.size.h * 0.7, 0);
        arm.rotation.z = -side * 0.6;
        group.add(arm);
      });
    } else if (this.typeId === 'dark_mage' || this.typeId === 'dark_mage_commander') {
      // Robe
      const robeMat = new THREE.MeshLambertMaterial({ color: this.typeId === 'dark_mage_commander' ? 0x110022 : 0x220033 });
      const robe = new THREE.Mesh(new THREE.ConeGeometry(d.size.w * 0.6, d.size.h * 0.7, 8), robeMat);
      robe.position.y = d.size.h * 0.35;
      group.add(robe);

      // Staff
      const staff = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, d.size.h * 1.2, 6), new THREE.MeshLambertMaterial({ color: 0x2a1a0a }));
      staff.position.set(d.size.w * 0.6, d.size.h * 0.6, 0);
      group.add(staff);
      const orb = new THREE.Mesh(new THREE.SphereGeometry(0.2, 8, 8), new THREE.MeshBasicMaterial({ color: 0xaa00ff }));
      orb.position.set(d.size.w * 0.6, d.size.h * 1.2, 0);
      group.add(orb);
      const orbLight = new THREE.PointLight(0xaa00ff, 2, 5);
      orbLight.position.copy(orb.position);
      group.add(orbLight);
    }

    // Boss visual — glow aura
    if (d.boss) {
      const aura = new THREE.Mesh(
        new THREE.SphereGeometry(d.size.w * 1.2, 12, 12),
        new THREE.MeshBasicMaterial({ color: d.emissiveColor, transparent: true, opacity: 0.15, side: THREE.BackSide })
      );
      group.add(aura);
      const bossLight = new THREE.PointLight(d.emissiveColor, 3, 12);
      group.add(bossLight);
    }

    this.mesh = group;
    this.position.copy(group.position);
    if (this.scene) this.scene.add(group);
  }

  _buildHealthBar() {
    const barGroup = new THREE.Group();
    barGroup.position.y = this.def.size.h * 1.6;

    const bgGeom = new THREE.PlaneGeometry(1.4, 0.14);
    const bgMat = new THREE.MeshBasicMaterial({ color: 0x330000, transparent: true, opacity: 0.85, side: THREE.DoubleSide });
    const bg = new THREE.Mesh(bgGeom, bgMat);
    barGroup.add(bg);

    const barGeom = new THREE.PlaneGeometry(1.4, 0.14);
    const barMat = new THREE.MeshBasicMaterial({ color: 0xee2222, transparent: true, opacity: 0.9, side: THREE.DoubleSide });
    this._hpBarMesh = new THREE.Mesh(barGeom, barMat);
    this._hpBarMesh.position.z = 0.01;
    barGroup.add(this._hpBarMesh);

    barGroup.name = 'healthBar';
    this.mesh.add(barGroup);
    this._hpBarGroup = barGroup;
  }

  addToScene(scene) {
    scene.add(this.mesh);
  }

  update(delta, playerPos) {
    if (!this.alive) return;
    this._attackTimer = Math.max(0, this._attackTimer - delta);

    const dist = this.mesh.position.distanceTo(playerPos);

    // State machine
    if (dist <= this.aggroRange && !this._aggrod) {
      this._aggrod = true;
      this._state = 'chase';
    }

    if (this._aggrod && dist > this.aggroRange * 2.5) {
      this._aggrod = false;
      this._state = 'returning';
    }

    switch (this._state) {
      case 'idle':    this._doIdle(delta); break;
      case 'chase':   this._doChase(delta, playerPos, dist); break;
      case 'attack':  this._doAttack(delta, playerPos, dist); break;
      case 'returning': this._doReturn(delta); break;
    }

    // Face player when aggrod
    if (this._aggrod) {
      const dir = playerPos.clone().sub(this.mesh.position);
      dir.y = 0;
      if (dir.length() > 0.1) {
        this.mesh.rotation.y = Math.atan2(dir.x, dir.z);
      }
    }

    // Update health bar to face camera
    if (this._hpBarGroup) {
      this._hpBarGroup.lookAt(playerPos.x, this._hpBarGroup.getWorldPosition(new THREE.Vector3()).y, playerPos.z);
      // Scale hp bar
      const pct = this.healthPercent;
      this._hpBarMesh.scale.x = pct;
      this._hpBarMesh.position.x = -(1 - pct) * 0.7;
    }

    this.position.copy(this.mesh.position);

    // Petrify effect for basilisk
    if (this.typeId === 'basilisk' && this._petrifyTimer > 0) {
      this._petrifyTimer -= delta;
    }

    // Ranged attack projectile update
    for (let i = this._projectiles.length - 1; i >= 0; i--) {
      const p = this._projectiles[i];
      p.life -= delta;
      if (p.life <= 0) {
        this.scene?.remove(p.mesh);
        this._projectiles.splice(i, 1);
        continue;
      }
      p.mesh.position.addScaledVector(p.vel, delta);
      p.mesh.rotation.y += delta * 4;
    }
  }

  _doIdle(delta) {
    this._wanderTimer -= delta;
    if (this._wanderTimer <= 0) {
      this._wanderTimer = 3 + Math.random() * 4;
      const angle = Math.random() * Math.PI * 2;
      const r = Math.random() * 5;
      this._wanderTarget.set(this._spawnPos.x + Math.cos(angle) * r, 0, this._spawnPos.z + Math.sin(angle) * r);
    }
    this._moveToward(this._wanderTarget, this.speed * 0.25, delta);
  }

  _doChase(delta, playerPos, dist) {
    if (dist <= this.attackRange) {
      this._state = 'attack';
    } else {
      this._moveToward(playerPos, this.speed, delta);
    }
  }

  _doAttack(delta, playerPos, dist) {
    if (dist > this.attackRange * 1.3) {
      this._state = 'chase';
      return;
    }

    if (this._attackTimer <= 0) {
      this._attackTimer = this.attackCooldown;
      this.emit('attack', playerPos);

      // Ranged: launch projectile
      if (this.def.ranged) this._launchProjectile(playerPos);
    }
  }

  _doReturn(delta) {
    const dist = this.mesh.position.distanceTo(this._spawnPos);
    if (dist < 1) {
      this._state = 'idle';
      this.health = Math.min(this.maxHealth, this.health + this.maxHealth * 0.3);
    } else {
      this._moveToward(this._spawnPos, this.speed * 1.2, delta);
    }
  }

  _moveToward(target, speed, delta) {
    const dir = target.clone().sub(this.mesh.position);
    dir.y = 0;
    const len = dir.length();
    if (len > 0.1) {
      dir.normalize().multiplyScalar(Math.min(speed * delta, len));
      this.mesh.position.add(dir);
    }
  }

  _launchProjectile(target) {
    const geom = new THREE.SphereGeometry(0.25, 6, 6);
    const mat = new THREE.MeshBasicMaterial({ color: 0xaa00ff, transparent: true, opacity: 0.85 });
    const mesh = new THREE.Mesh(geom, mat);
    mesh.position.copy(this.mesh.position).add(new THREE.Vector3(0, this.def.size.h, 0));
    const light = new THREE.PointLight(0xaa00ff, 2, 5);
    mesh.add(light);
    this.scene?.add(mesh);

    const vel = target.clone().add(new THREE.Vector3(0, 0.8, 0)).sub(mesh.position).normalize().multiplyScalar(12);
    this._projectiles.push({ mesh, vel, life: 3 });
  }

  // Check if any projectile hit the player
  checkProjectileHit(playerPos, playerRadius = 1.2) {
    for (let i = this._projectiles.length - 1; i >= 0; i--) {
      const p = this._projectiles[i];
      if (p.mesh.position.distanceTo(playerPos) < playerRadius + 0.3) {
        this.scene?.remove(p.mesh);
        this._projectiles.splice(i, 1);
        return this.damage;
      }
    }
    return 0;
  }

  _die(killer) {
    super._die(killer);
    this._state = 'dead';

    // Death animation — sink into ground
    const startY = this.mesh.position.y;
    const start = performance.now();
    const animate = (now) => {
      const t = (now - start) / 800;
      if (t >= 1) {
        this.mesh.visible = false;
        return;
      }
      this.mesh.position.y = startY - t * 2;
      this.mesh.scale.setScalar(1 - t * 0.5);
      this.mesh.children.forEach(c => {
        if (c.material) c.material.opacity = 1 - t;
      });
      requestAnimationFrame(animate);
    };
    requestAnimationFrame(animate);
  }

  takeDamage(amount, source) {
    if (!this.alive) return 0;
    // Weakness multiplier
    if (source && this.def.weakness && source === this.def.weakness) amount *= 1.5;
    // Shield resistance if player had it active (handled externally)

    // Hit flash
    this.mesh.traverse(c => {
      if (c.material && c.material.emissive) {
        const orig = c.material.emissiveIntensity;
        c.material.emissiveIntensity = 1.5;
        setTimeout(() => { if (c.material) c.material.emissiveIntensity = orig; }, 150);
      }
    });

    return super.takeDamage(amount, source);
  }

  get projectiles() { return this._projectiles; }
}
