import * as THREE from 'three';
import { SPELLS } from '../data/spells.js';
import { ParticleSystem } from './Particles.js';

// Manages active spell projectiles and spell casting
export class SpellSystem {
  constructor(scene, audio) {
    this.scene = scene;
    this.audio = audio;
    this.particles = new ParticleSystem(scene);
    this.projectiles = [];
    this.cooldowns = {};
    this.SPELLS = SPELLS;

    Object.keys(SPELLS).forEach(k => { this.cooldowns[k] = 0; });
  }

  update(delta) {
    // Tick cooldowns
    Object.keys(this.cooldowns).forEach(k => {
      if (this.cooldowns[k] > 0) this.cooldowns[k] = Math.max(0, this.cooldowns[k] - delta);
    });

    this.particles.update(delta);

    // Move projectiles
    for (let i = this.projectiles.length - 1; i >= 0; i--) {
      const proj = this.projectiles[i];
      proj.life -= delta;
      if (proj.life <= 0) {
        this._destroyProjectile(proj, i);
        continue;
      }
      proj.mesh.position.addScaledVector(proj.velocity, delta);
      proj.mesh.rotation.y += delta * 3;
      proj.trailEmitter?.update(delta);
    }
  }

  canCast(key, player) {
    const spell = SPELLS[key];
    if (!spell) return false;
    if (this.cooldowns[key] > 0) return false;
    if (player.mana < spell.manaCost) return false;
    return true;
  }

  cast(key, player, camera) {
    const spell = SPELLS[key];
    if (!this.canCast(key, player)) return false;

    player.mana -= spell.manaCost;
    this.cooldowns[key] = spell.cooldown;

    switch(spell.type) {
      case 'fire':      this._castProjectile(key, spell, player, camera); this.audio.playSpellFire(); break;
      case 'lightning': this._castProjectile(key, spell, player, camera); this.audio.playSpellLightning(); break;
      case 'nature':    this._castHeal(spell, player); this.audio.playHeal(); break;
      case 'shield':    this._castShield(spell, player); this.audio.playShield(); break;
      case 'ultimate':  this._castNova(spell, player, camera); this.audio.playExplosion(); break;
    }

    return true;
  }

  _castProjectile(key, spell, player, camera) {
    const dir = new THREE.Vector3();
    camera.getWorldDirection(dir);

    const spawnPos = player.position.clone();
    spawnPos.y += 1.2;
    spawnPos.addScaledVector(dir, 1.5);

    const geom = new THREE.SphereGeometry(0.3, 8, 8);
    const mat = new THREE.MeshBasicMaterial({
      color: spell.color,
      transparent: true,
      opacity: 0.9
    });
    const mesh = new THREE.Mesh(geom, mat);
    mesh.position.copy(spawnPos);

    // Glow
    const glowGeom = new THREE.SphereGeometry(0.55, 8, 8);
    const glowMat = new THREE.MeshBasicMaterial({
      color: spell.glowColor || spell.color,
      transparent: true,
      opacity: 0.25,
      side: THREE.BackSide
    });
    const glow = new THREE.Mesh(glowGeom, glowMat);
    mesh.add(glow);

    // Point light
    const light = new THREE.PointLight(spell.color, 3, 6);
    mesh.add(light);

    this.scene.add(mesh);

    const velocity = dir.multiplyScalar(spell.speed);

    const trailEmitter = this.particles.createTrail(
      { position: mesh.position, active: true },
      { color: spell.particles?.color || spell.color, size: spell.particles?.size || 0.1, life: 0.3, rate: 20 }
    );

    this.projectiles.push({
      mesh, velocity, trailEmitter,
      spell, key,
      life: 4.0,
      radius: spell.radius || 1.0,
      active: true
    });
  }

  _castHeal(spell, player) {
    player.heal(spell.heal);
    this.particles.burst(player.position.clone().add(new THREE.Vector3(0, 1, 0)), {
      count: spell.particles.count,
      color: spell.particles.color,
      size: spell.particles.size,
      spread: spell.particles.spread,
      life: 1.2,
      speed: 3,
      gravity: false
    });
  }

  _castShield(spell, player) {
    player.activateShield(spell.duration, spell.damageReduction);
    this.particles.ringBurst(player.position, 0x5599ff, 2);
    document.getElementById('shieldIndicator')?.classList.remove('hidden');
    setTimeout(() => document.getElementById('shieldIndicator')?.classList.add('hidden'), spell.duration * 1000);
  }

  _castNova(spell, player, camera) {
    this.particles.ringBurst(player.position, spell.particles.color, spell.blastRadius);
    this.particles.burst(player.position.clone().add(new THREE.Vector3(0, 1, 0)), {
      count: spell.particles.count,
      color: spell.particles.color,
      size: spell.particles.size,
      spread: spell.particles.spread,
      life: 1.5,
      speed: 8
    });

    // Area damage effect — return the blast info for GameScene to process
    return { type: 'nova', position: player.position.clone(), radius: spell.blastRadius, damage: spell.damage };
  }

  // Called by GameScene when projectile hits enemy
  getProjectileHit(enemies) {
    const hits = [];
    for (let i = this.projectiles.length - 1; i >= 0; i--) {
      const proj = this.projectiles[i];
      for (const enemy of enemies) {
        if (!enemy.alive) continue;
        const dist = proj.mesh.position.distanceTo(enemy.mesh.position);
        if (dist < proj.radius + 1.0) {
          hits.push({ projectileIdx: i, enemy, damage: proj.spell.damage, spellType: proj.spell.type });
          this._detonateProjectile(proj, i);
          break;
        }
      }
    }
    return hits;
  }

  _detonateProjectile(proj, idx) {
    this.particles.burst(proj.mesh.position.clone(), {
      count: 40,
      color: proj.spell.particles?.color || proj.spell.color,
      size: 0.2,
      spread: 0.8,
      life: 0.6
    });
    proj.trailEmitter && (proj.trailEmitter.active = false);
    this._destroyProjectile(proj, idx);
  }

  _destroyProjectile(proj, idx) {
    proj.trailEmitter && (proj.trailEmitter.active = false);
    this.scene.remove(proj.mesh);
    proj.mesh.geometry.dispose();
    proj.mesh.material.dispose();
    this.projectiles.splice(idx, 1);
  }

  getCooldownRatio(key) {
    const spell = SPELLS[key];
    if (!spell) return 0;
    return this.cooldowns[key] / spell.cooldown;
  }

  dispose() {
    this.projectiles.forEach(p => {
      this.scene.remove(p.mesh);
      p.mesh.geometry.dispose();
      p.mesh.material.dispose();
    });
    this.projectiles = [];
    this.particles.dispose();
  }
}
