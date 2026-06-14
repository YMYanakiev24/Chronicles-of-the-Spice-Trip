import * as THREE from 'three';

// Base class for all living entities (player, NPCs, enemies)
export class Entity {
  constructor(options = {}) {
    this.id = options.id || Math.random().toString(36).slice(2);
    this.name = options.name || 'Entity';

    this.maxHealth = options.maxHealth || 100;
    this.health = this.maxHealth;
    this.alive = true;

    this.position = new THREE.Vector3();
    this.velocity = new THREE.Vector3();
    this.mesh = null;

    this._eventListeners = {};
  }

  on(event, fn) {
    if (!this._eventListeners[event]) this._eventListeners[event] = [];
    this._eventListeners[event].push(fn);
  }

  emit(event, ...args) {
    (this._eventListeners[event] || []).forEach(fn => fn(...args));
  }

  takeDamage(amount, source) {
    if (!this.alive) return 0;
    const actual = Math.min(this.health, Math.max(0, amount));
    this.health -= actual;
    this.emit('damage', actual, source);
    if (this.health <= 0) this._die(source);
    return actual;
  }

  heal(amount) {
    if (!this.alive) return;
    const actual = Math.min(this.maxHealth - this.health, amount);
    this.health += actual;
    this.emit('heal', actual);
    return actual;
  }

  _die(killer) {
    this.alive = false;
    this.emit('death', killer);
  }

  get healthPercent() {
    return this.health / this.maxHealth;
  }

  syncMeshPosition() {
    if (this.mesh) {
      this.position.copy(this.mesh.position);
    }
  }

  dispose() {
    this._eventListeners = {};
  }
}
