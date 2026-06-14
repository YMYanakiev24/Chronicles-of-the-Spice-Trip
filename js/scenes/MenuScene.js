import * as THREE from 'three';

// Main menu scene — continues from intro with a live 3D background
export class MenuScene {
  constructor(engine, callbacks) {
    this.engine = engine;
    this.callbacks = callbacks;

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.1, 800);

    this._time = 0;
    this._cameraAngle = 0;
  }

  init() {
    this._buildBackground();
    this._showMenuUI();
    this._bindButtons();
  }

  _buildBackground() {
    this.scene.background = new THREE.Color(0x020108);
    this.scene.fog = new THREE.FogExp2(0x040212, 0.016);

    // Sky
    const skyGeom = new THREE.SphereGeometry(400, 16, 8);
    skyGeom.scale(-1, 1, 1);
    const skyMat = new THREE.MeshBasicMaterial({ color: 0x020108 });
    this.scene.add(new THREE.Mesh(skyGeom, skyMat));

    // Moon
    const moon = new THREE.Mesh(new THREE.SphereGeometry(8, 16, 16), new THREE.MeshBasicMaterial({ color: 0xd0c8aa }));
    moon.position.set(80, 100, -200);
    this.scene.add(moon);

    // Stars
    const starCount = 500;
    const starGeom = new THREE.BufferGeometry();
    const sp = new Float32Array(starCount * 3);
    for (let i = 0; i < starCount; i++) {
      sp[i * 3]     = (Math.random() - 0.5) * 700;
      sp[i * 3 + 1] = 50 + Math.random() * 200;
      sp[i * 3 + 2] = (Math.random() - 0.5) * 700;
    }
    starGeom.setAttribute('position', new THREE.Float32BufferAttribute(sp, 3));
    this.scene.add(new THREE.Points(starGeom, new THREE.PointsMaterial({
      color: 0xffffff, size: 0.7, transparent: true, opacity: 0.8,
      blending: THREE.AdditiveBlending, depthWrite: false
    })));

    // Ground
    const groundMesh = new THREE.Mesh(
      new THREE.PlaneGeometry(300, 300),
      new THREE.MeshLambertMaterial({ color: 0x080e06 })
    );
    groundMesh.rotation.x = -Math.PI / 2;
    this.scene.add(groundMesh);

    // Forest silhouettes
    for (let i = 0; i < 60; i++) {
      const x = (Math.random() - 0.5) * 150;
      const z = -20 - Math.random() * 80;
      const h = 5 + Math.random() * 8;
      const sc = 0.8 + Math.random() * 0.6;

      const trunk = new THREE.Mesh(
        new THREE.CylinderGeometry(0.15 * sc, 0.28 * sc, h, 6),
        new THREE.MeshLambertMaterial({ color: 0x0a0808 })
      );
      trunk.position.set(x, h / 2, z);
      this.scene.add(trunk);

      const leafColor = Math.random() < 0.15 ? 0x180a28 : 0x0a1208;
      const leafMat = new THREE.MeshLambertMaterial({ color: leafColor });
      for (let j = 0; j < 2; j++) {
        const cone = new THREE.Mesh(new THREE.ConeGeometry((1.5 - j * 0.3) * sc, (2 + j * 0.3) * sc, 7), leafMat);
        cone.position.set(x, h * 0.6 + j * 1.1 * sc, z);
        this.scene.add(cone);
      }

      // Occasional glow tree
      if (Math.random() < 0.1) {
        const gc = [0x44ff88, 0xaa44ff, 0x4488ff][Math.floor(Math.random() * 3)];
        const gl = new THREE.PointLight(gc, 0.8, 8);
        gl.position.set(x, h + 1, z);
        this.scene.add(gl);
      }
    }

    // Castle silhouette on hill
    this._buildMenuCastle();

    // Lighting
    const dirLight = new THREE.DirectionalLight(0x556699, 0.4);
    dirLight.position.set(60, 80, -150);
    this.scene.add(dirLight);
    this.scene.add(new THREE.AmbientLight(0x060412, 0.5));
    this.scene.add(new THREE.HemisphereLight(0x0a0820, 0x060c04, 0.35));

    // Ambient particles
    const parCount = 150;
    const parGeom = new THREE.BufferGeometry();
    const parPos = new Float32Array(parCount * 3);
    for (let i = 0; i < parCount; i++) {
      parPos[i * 3]     = (Math.random() - 0.5) * 80;
      parPos[i * 3 + 1] = Math.random() * 15;
      parPos[i * 3 + 2] = -5 - Math.random() * 40;
    }
    parGeom.setAttribute('position', new THREE.Float32BufferAttribute(parPos, 3));
    this._ambientParticles = new THREE.Points(parGeom, new THREE.PointsMaterial({
      color: 0x44ff88, size: 0.1, transparent: true, opacity: 0.6, blending: THREE.AdditiveBlending, depthWrite: false
    }));
    this.scene.add(this._ambientParticles);
    this._parPos = parPos;

    // Camera position
    this.camera.position.set(0, 5, 30);
    this.camera.lookAt(0, 8, -60);
  }

  _buildMenuCastle() {
    const group = new THREE.Group();
    group.position.set(0, 5, -90);

    const stone = new THREE.MeshLambertMaterial({ color: 0x252018 });
    const darkS = new THREE.MeshLambertMaterial({ color: 0x100c0a });

    const keep = new THREE.Mesh(new THREE.BoxGeometry(12, 18, 12), stone);
    keep.position.y = 9;
    group.add(keep);

    [[-6, -6], [6, -6], [-6, 6], [6, 6]].forEach(([tx, tz]) => {
      const t = new THREE.Mesh(new THREE.CylinderGeometry(2.2, 2.5, 24, 8), stone);
      t.position.set(tx, 12, tz);
      group.add(t);
      const r = new THREE.Mesh(new THREE.ConeGeometry(2.8, 5, 8), darkS);
      r.position.set(tx, 26.5, tz);
      group.add(r);
      const wl = new THREE.PointLight(0xff8800, 2, 10);
      wl.position.set(tx, 15, tz + 2.6);
      group.add(wl);
    });

    const orb = new THREE.Mesh(new THREE.SphereGeometry(1.2, 12, 12), new THREE.MeshBasicMaterial({ color: 0x8844ff }));
    orb.position.y = 22;
    group.add(orb);
    const ol = new THREE.PointLight(0x8844ff, 6, 40);
    ol.position.y = 22;
    group.add(ol);

    // Hill
    const hill = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 22, 20, 14), stone);
    hill.position.y = -10;
    group.add(hill);

    this.scene.add(group);
    this._menuCastle = group;
    this._menuOrbLight = ol;
  }

  _showMenuUI() {
    const el = document.getElementById('mainMenu');
    if (el) el.classList.remove('hidden');

    // Check save
    const save = this.engine.getSaveData();
    const continueBtn = document.getElementById('btnContinue');
    if (continueBtn) {
      continueBtn.style.opacity = save ? '1' : '0.4';
      continueBtn.disabled = !save;
    }
  }

  _bindButtons() {
    document.getElementById('btnNewGame')?.addEventListener('click', () => {
      this._hideMenu();
      this.callbacks?.onNewGame();
    });

    document.getElementById('btnContinue')?.addEventListener('click', () => {
      const save = this.engine.getSaveData();
      if (save) {
        this._hideMenu();
        this.callbacks?.onContinue(save);
      }
    });

    document.getElementById('btnSettings')?.addEventListener('click', () => {
      document.getElementById('settingsPanel')?.classList.remove('hidden');
    });

    document.getElementById('closeSettings')?.addEventListener('click', () => {
      document.getElementById('settingsPanel')?.classList.add('hidden');
    });

    document.getElementById('saveSettings')?.addEventListener('click', () => {
      const settings = this.engine.settings;
      settings.musicVolume = document.getElementById('musicVolume')?.value / 100 || 0.6;
      settings.sfxVolume = document.getElementById('sfxVolume')?.value / 100 || 0.8;
      settings.camSensitivity = parseInt(document.getElementById('camSensitivity')?.value) || 5;
      this.engine.saveSettings();
      document.getElementById('settingsPanel')?.classList.add('hidden');
    });

    document.getElementById('btnCredits')?.addEventListener('click', () => {
      document.getElementById('creditsPanel')?.classList.remove('hidden');
    });

    document.getElementById('closeCredits')?.addEventListener('click', () => {
      document.getElementById('creditsPanel')?.classList.add('hidden');
    });
  }

  _hideMenu() {
    const el = document.getElementById('mainMenu');
    if (el) {
      el.style.transition = 'opacity 0.5s ease';
      el.style.opacity = '0';
      setTimeout(() => el.classList.add('hidden'), 500);
    }
  }

  update(delta) {
    this._time += delta;

    // Slow camera pan
    this._cameraAngle += delta * 0.05;
    this.camera.position.x = Math.sin(this._cameraAngle) * 15;
    this.camera.position.y = 5 + Math.sin(this._time * 0.2) * 1.5;
    this.camera.lookAt(Math.sin(this._cameraAngle) * 3, 10, -70);

    // Orb pulse
    if (this._menuOrbLight) {
      this._menuOrbLight.intensity = 5 + Math.sin(this._time * 1.5) * 2;
    }

    // Particle drift
    if (this._parPos) {
      for (let i = 0; i < this._parPos.length / 3; i++) {
        this._parPos[i * 3 + 1] += 0.02 * delta * 60;
        if (this._parPos[i * 3 + 1] > 18) this._parPos[i * 3 + 1] = 0;
      }
      this._ambientParticles.geometry.attributes.position.needsUpdate = true;
    }
  }

  dispose() {
    document.getElementById('mainMenu')?.classList.add('hidden');
  }
}
