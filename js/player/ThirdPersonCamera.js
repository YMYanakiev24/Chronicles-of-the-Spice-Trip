import * as THREE from 'three';

// Third-person camera that follows the player with orbit control
export class ThirdPersonCamera {
  constructor(camera, settings) {
    this.camera = camera;
    this.settings = settings;

    this.distance = 8;
    this.minDistance = 2;
    this.maxDistance = 20;

    this.yaw = 0;      // horizontal rotation
    this.pitch = 0.35; // vertical angle (radians)
    this.minPitch = -0.1;
    this.maxPitch = Math.PI * 0.4;

    this._targetPos = new THREE.Vector3();
    this._currentPos = new THREE.Vector3();
    this._lerpFactor = 8;

    // Cinematic mode for intro
    this.cinematicMode = false;
    this.cinematicPath = [];
    this.cinematicT = 0;
    this.cinematicDuration = 0;
    this.cinematicLookAt = new THREE.Vector3();
    this._onCinematicEnd = null;
  }

  handleMouseMove(dx, dy) {
    if (this.cinematicMode) return;
    const sensitivity = (this.settings?.camSensitivity || 5) * 0.001;
    this.yaw   -= dx * sensitivity;
    this.pitch += dy * sensitivity;
    this.pitch  = Math.max(this.minPitch, Math.min(this.maxPitch, this.pitch));
  }

  handleScroll(delta) {
    this.distance = Math.max(this.minDistance, Math.min(this.maxDistance, this.distance + delta * 0.01));
  }

  update(delta, target) {
    if (this.cinematicMode) {
      this._updateCinematic(delta);
      return;
    }

    // Compute ideal camera position
    const sinYaw   = Math.sin(this.yaw);
    const cosYaw   = Math.cos(this.yaw);
    const sinPitch = Math.sin(this.pitch);
    const cosPitch = Math.cos(this.pitch);

    const targetPos = target.clone().add(new THREE.Vector3(0, 1.4, 0)); // look at chest height

    this._targetPos.set(
      targetPos.x + sinYaw * cosPitch * this.distance,
      targetPos.y + sinPitch * this.distance,
      targetPos.z + cosYaw * cosPitch * this.distance
    );

    // Smooth follow
    const lerpT = 1 - Math.exp(-this._lerpFactor * delta);
    this._currentPos.lerp(this._targetPos, lerpT);
    this.camera.position.copy(this._currentPos);
    this.camera.lookAt(targetPos);
  }

  // Start a cinematic camera flight
  startCinematic(waypoints, duration, lookAt, onEnd) {
    this.cinematicMode = true;
    this.cinematicPath = waypoints;
    this.cinematicT = 0;
    this.cinematicDuration = duration;
    this.cinematicLookAt.copy(lookAt);
    this._onCinematicEnd = onEnd;
  }

  _updateCinematic(delta) {
    this.cinematicT += delta / this.cinematicDuration;
    if (this.cinematicT >= 1) {
      this.cinematicT = 1;
      this.cinematicMode = false;
      this._onCinematicEnd?.();
    }

    const t = this.cinematicT;
    const path = this.cinematicPath;

    if (path.length >= 2) {
      const seg = (path.length - 1) * t;
      const idx = Math.min(Math.floor(seg), path.length - 2);
      const frac = seg - idx;
      const a = path[idx];
      const b = path[idx + 1];
      this.camera.position.lerpVectors(a, b, frac);
    }

    this.camera.lookAt(this.cinematicLookAt);
  }

  // Set yaw to face the target direction
  faceDirection(dir) {
    this.yaw = Math.atan2(dir.x, dir.z);
  }
}
