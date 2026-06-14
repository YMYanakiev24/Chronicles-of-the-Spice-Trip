import * as THREE from 'three';

// Particle system for spells, ambience, and effects
export class ParticleSystem {
  constructor(scene) {
    this.scene = scene;
    this.systems = [];
  }

  update(delta) {
    for (let i = this.systems.length - 1; i >= 0; i--) {
      const sys = this.systems[i];
      sys.life -= delta;
      if (sys.life <= 0) {
        this.scene.remove(sys.points);
        sys.points.geometry.dispose();
        sys.points.material.dispose();
        this.systems.splice(i, 1);
        continue;
      }

      const positions = sys.points.geometry.attributes.position.array;
      const t = 1 - sys.life / sys.maxLife;

      for (let j = 0; j < sys.count; j++) {
        const j3 = j * 3;
        positions[j3]     += sys.velocities[j3]     * delta;
        positions[j3 + 1] += sys.velocities[j3 + 1] * delta;
        positions[j3 + 2] += sys.velocities[j3 + 2] * delta;

        // Gravity for certain types
        if (sys.gravity) sys.velocities[j3 + 1] -= 4 * delta;
        // Damping
        sys.velocities[j3]     *= (1 - sys.drag * delta);
        sys.velocities[j3 + 2] *= (1 - sys.drag * delta);
      }
      sys.points.geometry.attributes.position.needsUpdate = true;

      // Fade out
      sys.points.material.opacity = Math.max(0, 1 - t * t);
    }
  }

  // Burst of particles at a world position
  burst(position, options = {}) {
    const {
      count = 30,
      color = 0xffffff,
      size = 0.15,
      spread = 1.0,
      life = 0.8,
      speed = 5,
      gravity = true,
      drag = 2
    } = options;

    const geom = new THREE.BufferGeometry();
    const positions = new Float32Array(count * 3);
    const velocities = new Float32Array(count * 3);

    for (let i = 0; i < count; i++) {
      const i3 = i * 3;
      positions[i3]     = position.x + (Math.random() - 0.5) * 0.2;
      positions[i3 + 1] = position.y + (Math.random() - 0.5) * 0.2;
      positions[i3 + 2] = position.z + (Math.random() - 0.5) * 0.2;

      const phi = Math.random() * Math.PI * 2;
      const theta = Math.random() * Math.PI;
      const s = (0.5 + Math.random() * 0.5) * speed;
      velocities[i3]     = Math.sin(theta) * Math.cos(phi) * s * spread;
      velocities[i3 + 1] = Math.cos(theta) * s * spread * 0.8 + speed * 0.5;
      velocities[i3 + 2] = Math.sin(theta) * Math.sin(phi) * s * spread;
    }

    geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));

    const mat = new THREE.PointsMaterial({
      color,
      size,
      transparent: true,
      opacity: 1,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      sizeAttenuation: true
    });

    const points = new THREE.Points(geom, mat);
    this.scene.add(points);

    this.systems.push({ points, velocities, count, life, maxLife: life, gravity, drag });
    return points;
  }

  // Continuous emitter (used for spell projectiles, ambient effects)
  createTrail(sourceRef, options = {}) {
    const {
      color = 0xffffff,
      size = 0.1,
      life = 0.4,
      rate = 15
    } = options;

    let elapsed = 0;
    const interval = 1 / rate;

    const emitter = {
      update: (delta) => {
        elapsed += delta;
        while (elapsed >= interval) {
          elapsed -= interval;
          if (!sourceRef.active) return;
          this.burst(sourceRef.position, {
            count: 3, color, size, spread: 0.2, life, speed: 0.5, gravity: false, drag: 5
          });
        }
      },
      active: true
    };

    return emitter;
  }

  // Ambient floating particles for a zone
  createAmbient(center, options = {}) {
    const {
      count = 60,
      color = 0x88ff88,
      size = 0.06,
      radius = 15,
      height = 4
    } = options;

    const geom = new THREE.BufferGeometry();
    const positions = new Float32Array(count * 3);
    const velocities = new Float32Array(count * 3);

    for (let i = 0; i < count; i++) {
      const i3 = i * 3;
      const angle = Math.random() * Math.PI * 2;
      const r = Math.random() * radius;
      positions[i3]     = center.x + Math.cos(angle) * r;
      positions[i3 + 1] = center.y + Math.random() * height;
      positions[i3 + 2] = center.z + Math.sin(angle) * r;
      velocities[i3]     = (Math.random() - 0.5) * 0.3;
      velocities[i3 + 1] = 0.2 + Math.random() * 0.4;
      velocities[i3 + 2] = (Math.random() - 0.5) * 0.3;
    }

    geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    const mat = new THREE.PointsMaterial({
      color, size, transparent: true, opacity: 0.7,
      depthWrite: false, blending: THREE.AdditiveBlending
    });

    const points = new THREE.Points(geom, mat);
    this.scene.add(points);

    const posArr = geom.attributes.position.array;
    const bounds = { minX: center.x - radius, maxX: center.x + radius, minZ: center.z - radius, maxZ: center.z + radius, minY: center.y, maxY: center.y + height };

    // Continuous ambient updater
    const ambientUpdater = {
      update: (delta) => {
        for (let i = 0; i < count; i++) {
          const i3 = i * 3;
          posArr[i3]     += velocities[i3]     * delta;
          posArr[i3 + 1] += velocities[i3 + 1] * delta;
          posArr[i3 + 2] += velocities[i3 + 2] * delta;

          // Wrap particles that escape bounds
          if (posArr[i3 + 1] > bounds.maxY) {
            posArr[i3 + 1] = bounds.minY;
            const angle = Math.random() * Math.PI * 2;
            const r = Math.random() * radius;
            posArr[i3] = center.x + Math.cos(angle) * r;
            posArr[i3 + 2] = center.z + Math.sin(angle) * r;
          }
        }
        geom.attributes.position.needsUpdate = true;
      },
      dispose: () => {
        this.scene.remove(points);
        geom.dispose();
        mat.dispose();
      }
    };

    return ambientUpdater;
  }

  // Circular ring explosion
  ringBurst(position, color = 0xffffff, radius = 3) {
    const count = 48;
    const geom = new THREE.BufferGeometry();
    const positions = new Float32Array(count * 3);
    const velocities = new Float32Array(count * 3);

    for (let i = 0; i < count; i++) {
      const i3 = i * 3;
      const angle = (i / count) * Math.PI * 2;
      positions[i3]     = position.x;
      positions[i3 + 1] = position.y + 0.5;
      positions[i3 + 2] = position.z;
      velocities[i3]     = Math.cos(angle) * radius;
      velocities[i3 + 1] = 2;
      velocities[i3 + 2] = Math.sin(angle) * radius;
    }

    geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    const mat = new THREE.PointsMaterial({
      color, size: 0.25, transparent: true, opacity: 1,
      depthWrite: false, blending: THREE.AdditiveBlending
    });

    const points = new THREE.Points(geom, mat);
    this.scene.add(points);
    this.systems.push({ points, velocities, count, life: 0.7, maxLife: 0.7, gravity: true, drag: 3 });
  }

  dispose() {
    this.systems.forEach(sys => {
      this.scene.remove(sys.points);
      sys.points.geometry.dispose();
      sys.points.material.dispose();
    });
    this.systems = [];
  }
}
