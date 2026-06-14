import * as THREE from 'three';

// Cinematic opening scene — dark forest, castle on hill, magical atmosphere
export class IntroScene {
  constructor(engine, onComplete) {
    this.engine = engine;
    this.onComplete = onComplete;

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.1, 1000);

    this._time = 0;
    this._phase = 'intro'; // intro | menu
    this._titleShown = false;
    this._canSkip = false;
    this._ambientParticles = [];

    this._overlay = this._createOverlay();
  }

  init() {
    this._buildScene();
    this._startCameraPath();

    // Audio
    setTimeout(() => {
      this.engine.audio.resume();
      this.engine.audio.playAmbientMusic();
    }, 500);

    // Allow skip after 2 seconds
    setTimeout(() => {
      this._canSkip = true;
      this._overlay.pressKey.style.opacity = '1';
    }, 2000);

    // Input handler
    this._skipHandler = (e) => {
      if (this._canSkip) this._finishIntro();
    };
    document.addEventListener('keydown', this._skipHandler);
    document.addEventListener('click', this._skipHandler);
  }

  _createOverlay() {
    const container = document.createElement('div');
    container.style.cssText = `
      position:fixed; inset:0; pointer-events:none; z-index:80;
      display:flex; flex-direction:column; align-items:center; justify-content:flex-end;
      padding-bottom:12%;
    `;

    const title = document.createElement('h1');
    title.style.cssText = `
      font-family:'Cinzel Decorative',serif; font-size:3rem; color:#c9a227;
      text-shadow:0 0 50px #c9a227,0 0 100px #8b6914; letter-spacing:0.25em;
      opacity:0; transition:opacity 2s ease; text-align:center; line-height:1.3;
    `;
    title.textContent = 'THE MYTHIC CHRONICLES';

    const subtitle = document.createElement('h2');
    subtitle.style.cssText = `
      font-family:'Cinzel',serif; font-size:1.1rem; color:#b44fc4;
      letter-spacing:0.7em; margin-top:0.5rem; opacity:0;
      transition:opacity 2s ease 0.5s; text-align:center;
    `;
    subtitle.textContent = 'THE EMERALD REALM';

    const pressKey = document.createElement('p');
    pressKey.style.cssText = `
      font-family:'IM Fell English',serif; font-style:italic; color:rgba(150,130,90,0.8);
      font-size:0.9rem; margin-top:2rem; opacity:0; transition:opacity 1.5s ease;
      animation:pulse-text 2s ease-in-out infinite;
    `;
    pressKey.textContent = '— press any key to continue —';

    container.appendChild(title);
    container.appendChild(subtitle);
    container.appendChild(pressKey);
    document.body.appendChild(container);

    return { container, title, subtitle, pressKey };
  }

  _buildScene() {
    this.scene.background = new THREE.Color(0x020108);
    this.scene.fog = new THREE.FogExp2(0x040212, 0.018);

    this._buildSky();
    this._buildGround();
    this._buildForest();
    this._buildHill();
    this._buildCastle();
    this._buildLighting();
    this._buildParticles();
  }

  _buildSky() {
    const skyGeom = new THREE.SphereGeometry(400, 32, 16);
    skyGeom.scale(-1, 1, 1);

    const skyMat = new THREE.ShaderMaterial({
      uniforms: {
        top: { value: new THREE.Color(0x020108) },
        bottom: { value: new THREE.Color(0x0d0520) }
      },
      vertexShader: `
        varying vec3 vPos;
        void main() { vPos = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }
      `,
      fragmentShader: `
        uniform vec3 top, bottom;
        varying vec3 vPos;
        void main() {
          float t = clamp((normalize(vPos).y + 0.2) * 1.5, 0.0, 1.0);
          gl_FragColor = vec4(mix(bottom, top, t), 1.0);
        }
      `,
      side: THREE.BackSide
    });
    this.scene.add(new THREE.Mesh(skyGeom, skyMat));

    // Stars
    const starCount = 600;
    const starGeom = new THREE.BufferGeometry();
    const sp = new Float32Array(starCount * 3);
    for (let i = 0; i < starCount; i++) {
      const phi = Math.random() * Math.PI;
      const theta = Math.random() * Math.PI * 2;
      const r = 300;
      sp[i * 3]     = r * Math.sin(phi) * Math.cos(theta);
      sp[i * 3 + 1] = Math.abs(r * Math.cos(phi));
      sp[i * 3 + 2] = r * Math.sin(phi) * Math.sin(theta);
    }
    starGeom.setAttribute('position', new THREE.Float32BufferAttribute(sp, 3));
    this.scene.add(new THREE.Points(starGeom, new THREE.PointsMaterial({
      color: 0xffffff, size: 0.6, transparent: true, opacity: 0.9,
      blending: THREE.AdditiveBlending, depthWrite: false
    })));

    // Moon
    const moon = new THREE.Mesh(new THREE.SphereGeometry(7, 16, 16), new THREE.MeshBasicMaterial({ color: 0xd0c8aa }));
    moon.position.set(120, 160, -280);
    this.scene.add(moon);
    const moonGlow = new THREE.Mesh(new THREE.SphereGeometry(12, 16, 16), new THREE.MeshBasicMaterial({ color: 0x8899bb, transparent: true, opacity: 0.12, side: THREE.BackSide }));
    moon.add(moonGlow);
  }

  _buildGround() {
    // Dark forest floor
    const groundGeom = new THREE.PlaneGeometry(300, 300, 64, 64);
    groundGeom.rotateX(-Math.PI / 2);

    const pos = groundGeom.attributes.position;
    for (let i = 0; i < pos.count; i++) {
      const x = pos.getX(i), z = pos.getZ(i);
      const h = this._noise(x * 0.05, z * 0.05) * 1.5 + this._noise(x * 0.12, z * 0.12) * 0.5;
      pos.setY(i, h);
    }
    groundGeom.computeVertexNormals();

    const groundMat = new THREE.MeshLambertMaterial({ color: 0x0a1208 });
    const ground = new THREE.Mesh(groundGeom, groundMat);
    this.scene.add(ground);

    // Grass patches
    const grassMat = new THREE.MeshLambertMaterial({ color: 0x0e2010 });
    for (let i = 0; i < 200; i++) {
      const x = (Math.random() - 0.5) * 120;
      const z = (Math.random() - 0.5) * 120;
      if (Math.sqrt(x * x + z * z) < 8) continue;
      const blade = new THREE.Mesh(new THREE.CylinderGeometry(0.01, 0.04, 0.4 + Math.random() * 0.4, 3), grassMat);
      blade.position.set(x, 0.2 + this._noise(x * 0.05, z * 0.05) * 1.5, z);
      this.scene.add(blade);
    }
  }

  _noise(x, z) {
    const xi = Math.floor(x), zi = Math.floor(z);
    const xf = x - xi, zf = z - zi;
    const u = xf * xf * (3 - 2 * xf);
    const v = zf * zf * (3 - 2 * zf);
    const h = n => { let s = Math.sin(n * 127.1 + 311.7) * 43758.5; return s - Math.floor(s); };
    const r = (a, b) => h(a * 1000 + b);
    return (r(xi, zi) * (1 - u) + r(xi + 1, zi) * u) * (1 - v) + (r(xi, zi + 1) * (1 - u) + r(xi + 1, zi + 1) * u) * v;
  }

  _buildForest() {
    const positions = [];
    for (let i = 0; i < 140; i++) {
      let x, z;
      do {
        x = (Math.random() - 0.5) * 140;
        z = (Math.random() - 0.5) * 140;
      } while (Math.sqrt(x * x + z * z) < 10);
      positions.push([x, z]);
    }

    positions.forEach(([x, z]) => {
      this._placeTree(x, z);
    });
  }

  _placeTree(x, z) {
    const group = new THREE.Group();
    const gy = this._noise(x * 0.05, z * 0.05) * 1.5;
    group.position.set(x, gy, z);

    const tH = 3 + Math.random() * 5;
    const sc = 0.6 + Math.random() * 0.8;

    const trunk = new THREE.Mesh(
      new THREE.CylinderGeometry(0.18 * sc, 0.32 * sc, tH, 6),
      new THREE.MeshLambertMaterial({ color: 0x1a0f08 })
    );
    trunk.position.y = tH / 2;
    group.add(trunk);

    // Dark foliage
    const leafHue = Math.random() < 0.15 ? 0.75 : 0.3; // sometimes purple-magic trees
    const leafColor = new THREE.Color().setHSL(leafHue, 0.5, 0.07 + Math.random() * 0.06);
    const leafMat = new THREE.MeshLambertMaterial({ color: leafColor });

    for (let i = 0; i < 3; i++) {
      const cone = new THREE.Mesh(new THREE.ConeGeometry((1.6 - i * 0.25) * sc, (1.8 + i * 0.2) * sc, 7), leafMat);
      cone.position.y = tH * 0.6 + i * 1.1 * sc;
      cone.rotation.y = Math.random() * Math.PI;
      group.add(cone);
    }

    // Occasionally add magical glow
    if (Math.random() < 0.12) {
      const glowColors = [0x44ff88, 0xaa44ff, 0x4488ff];
      const gc = glowColors[Math.floor(Math.random() * glowColors.length)];
      const gl = new THREE.PointLight(gc, 0.8, 6);
      gl.position.y = tH + 0.5;
      group.add(gl);
    }

    group.rotation.y = Math.random() * Math.PI * 2;
    this.scene.add(group);
  }

  _buildHill() {
    const hillGeom = new THREE.CylinderGeometry(0.1, 30, 25, 16);
    const hillMat = new THREE.MeshLambertMaterial({ color: 0x0a1208 });
    const hill = new THREE.Mesh(hillGeom, hillMat);
    hill.position.set(0, -2, -70);
    this.scene.add(hill);

    // Hill rim
    const rim = new THREE.Mesh(
      new THREE.TorusGeometry(22, 2, 8, 24),
      new THREE.MeshLambertMaterial({ color: 0x0d1a0a })
    );
    rim.rotation.x = Math.PI / 2;
    rim.position.set(0, 9, -70);
    this.scene.add(rim);
  }

  _buildCastle() {
    const group = new THREE.Group();
    group.position.set(0, 10, -70);

    const stone = new THREE.MeshLambertMaterial({ color: 0x3a3028 });
    const darkStone = new THREE.MeshLambertMaterial({ color: 0x1a1410 });

    // Main keep
    const keep = new THREE.Mesh(new THREE.BoxGeometry(14, 20, 14), stone);
    keep.position.y = 10;
    group.add(keep);

    // Towers
    [[-7, -7], [7, -7], [-7, 7], [7, 7]].forEach(([tx, tz]) => {
      const tower = new THREE.Mesh(new THREE.CylinderGeometry(2.5, 2.8, 26, 10), stone);
      tower.position.set(tx, 13, tz);
      group.add(tower);

      const roof = new THREE.Mesh(new THREE.ConeGeometry(3, 6, 10), darkStone);
      roof.position.set(tx, 29, tz);
      group.add(roof);

      // Glowing window
      const win = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.9, 0.1), new THREE.MeshBasicMaterial({ color: 0xffaa22, transparent: true, opacity: 0.9 }));
      win.position.set(tx, 16, tz + 2.85);
      group.add(win);
      const wl = new THREE.PointLight(0xff8800, 3, 10);
      wl.position.set(tx, 16, tz + 2);
      group.add(wl);
    });

    // Battlements
    for (let i = 0; i < 10; i++) {
      const angle = (i / 10) * Math.PI * 2;
      const b = new THREE.Mesh(new THREE.BoxGeometry(1.8, 2.5, 1.8), stone);
      b.position.set(Math.cos(angle) * 6.5, 21, Math.sin(angle) * 6.5);
      group.add(b);
    }

    // Magical orb
    const orb = new THREE.Mesh(new THREE.SphereGeometry(1.3, 16, 16), new THREE.MeshBasicMaterial({ color: 0x8844ff }));
    orb.position.y = 24;
    group.add(orb);
    const orbGlow = new THREE.Mesh(new THREE.SphereGeometry(2.2, 16, 16), new THREE.MeshBasicMaterial({ color: 0x6622cc, transparent: true, opacity: 0.2, side: THREE.BackSide }));
    orbGlow.position.y = 24;
    group.add(orbGlow);
    const orbLight = new THREE.PointLight(0x8844ff, 5, 35);
    orbLight.position.y = 24;
    group.add(orbLight);

    // Floating rune stones
    for (let i = 0; i < 4; i++) {
      const angle = (i / 4) * Math.PI * 2;
      const stone2 = new THREE.Mesh(
        new THREE.OctahedronGeometry(0.55, 0),
        new THREE.MeshBasicMaterial({ color: 0x5577ff, transparent: true, opacity: 0.85 })
      );
      stone2.position.set(Math.cos(angle) * 11, 20 + Math.sin(angle) * 2, Math.sin(angle) * 11);
      stone2.name = 'runeStone_' + i;
      stone2._angle = angle;
      stone2._baseY = 20;
      group.add(stone2);
      const sl = new THREE.PointLight(0x4455ff, 2, 8);
      stone2.add(sl);
    }

    // Gate with glowing arch
    const gate = new THREE.Mesh(new THREE.BoxGeometry(4, 7, 1), darkStone);
    gate.position.set(0, 3.5, 7.1);
    group.add(gate);

    // Magical rune circle
    const runeGeom = new THREE.TorusGeometry(4, 0.12, 8, 32);
    const runeRing = new THREE.Mesh(runeGeom, new THREE.MeshBasicMaterial({ color: 0x4466ff, transparent: true, opacity: 0.7 }));
    runeRing.rotation.x = -Math.PI / 2;
    runeRing.position.set(0, -8.8, 10);
    group.add(runeRing);

    this.scene.add(group);
    this._castleGroup = group;
  }

  _buildLighting() {
    // Moonlight
    const moonLight = new THREE.DirectionalLight(0x6677aa, 0.5);
    moonLight.position.set(100, 120, -200);
    this.scene.add(moonLight);

    // Subtle ambient
    this.scene.add(new THREE.AmbientLight(0x080614, 0.6));

    // Hemisphere
    this.scene.add(new THREE.HemisphereLight(0x0c0828, 0x080f04, 0.4));
  }

  _buildParticles() {
    // Floating magical particles
    const count = 200;
    const geom = new THREE.BufferGeometry();
    const pos = new Float32Array(count * 3);
    const vel = new Float32Array(count * 3);

    for (let i = 0; i < count; i++) {
      const i3 = i * 3;
      pos[i3]     = (Math.random() - 0.5) * 100;
      pos[i3 + 1] = Math.random() * 15;
      pos[i3 + 2] = -30 + (Math.random() - 0.5) * 60;
      vel[i3]     = (Math.random() - 0.5) * 0.3;
      vel[i3 + 1] = 0.1 + Math.random() * 0.4;
      vel[i3 + 2] = (Math.random() - 0.5) * 0.15;
    }

    geom.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));

    const matGreen = new THREE.PointsMaterial({ color: 0x44ff88, size: 0.12, transparent: true, opacity: 0.7, blending: THREE.AdditiveBlending, depthWrite: false });
    const matPurple = new THREE.PointsMaterial({ color: 0xaa44ff, size: 0.1, transparent: true, opacity: 0.6, blending: THREE.AdditiveBlending, depthWrite: false });

    this._introParticles = { geom, pos: geom.attributes.position.array, vel };
    this._greenParticles = new THREE.Points(geom, matGreen);
    this.scene.add(this._greenParticles);

    // Second layer (purple)
    const geom2 = new THREE.BufferGeometry();
    const pos2 = new Float32Array(count * 3);
    for (let i = 0; i < count; i++) {
      pos2[i * 3]     = (Math.random() - 0.5) * 120;
      pos2[i * 3 + 1] = Math.random() * 18;
      pos2[i * 3 + 2] = -40 + (Math.random() - 0.5) * 80;
    }
    geom2.setAttribute('position', new THREE.Float32BufferAttribute(pos2, 3));
    const purplePoints = new THREE.Points(geom2, matPurple);
    this.scene.add(purplePoints);
    this._purpleParticles = { geom: geom2, pos: geom2.attributes.position.array };
  }

  _startCameraPath() {
    // Cinematic camera: starts low, slowly rises and pushes toward castle
    this._cameraPath = [
      new THREE.Vector3(-20, 1.5, 35),
      new THREE.Vector3(-8, 3, 15),
      new THREE.Vector3(5, 6, -2),
      new THREE.Vector3(12, 12, -20),
      new THREE.Vector3(5, 18, -40)
    ];
    this._cameraLookAt = new THREE.Vector3(0, 15, -70);
    this._introDuration = 12;
    this._introT = 0;

    this.camera.position.copy(this._cameraPath[0]);
    this.camera.lookAt(this._cameraLookAt);

    // Show title after 4 seconds
    setTimeout(() => {
      this._overlay.title.style.opacity = '1';
      this._overlay.subtitle.style.opacity = '1';
    }, 4000);
  }

  update(delta) {
    this._time += delta;

    // Camera path
    this._introT = Math.min(1, this._introT + delta / this._introDuration);
    this._moveCameraAlongPath(this._introT);

    // Floating rune stones animation
    if (this._castleGroup) {
      this._castleGroup.traverse(obj => {
        if (obj.name?.startsWith('runeStone_')) {
          obj.position.y = obj._baseY + Math.sin(this._time * 1.2 + obj._angle) * 1.8;
          obj.rotation.x = this._time * 0.5;
          obj.rotation.y = this._time * 0.8;
        }
      });
    }

    // Animate intro particles
    if (this._introParticles) {
      const { pos, vel } = this._introParticles;
      const count = pos.length / 3;
      for (let i = 0; i < count; i++) {
        const i3 = i * 3;
        pos[i3]     += vel[i3]     * delta;
        pos[i3 + 1] += vel[i3 + 1] * delta;
        pos[i3 + 2] += vel[i3 + 2] * delta;
        if (pos[i3 + 1] > 16) { pos[i3 + 1] = 0; pos[i3] = (Math.random() - 0.5) * 100; }
      }
      this._introParticles.geom.attributes.position.needsUpdate = true;
    }

    // Purple particles drift
    if (this._purpleParticles) {
      const { pos } = this._purpleParticles;
      for (let i = 0; i < pos.length / 3; i++) {
        pos[i * 3 + 1] += 0.03 * delta * 60;
        if (pos[i * 3 + 1] > 20) pos[i * 3 + 1] = 0;
      }
      this._purpleParticles.geom.attributes.position.needsUpdate = true;
    }
  }

  _moveCameraAlongPath(t) {
    const path = this._cameraPath;
    const seg = (path.length - 1) * t;
    const idx = Math.min(Math.floor(seg), path.length - 2);
    const frac = seg - idx;

    // Smooth interpolation
    const sf = frac * frac * (3 - 2 * frac);
    const a = path[idx], b = path[idx + 1];
    this.camera.position.lerpVectors(a, b, sf);
    this.camera.lookAt(this._cameraLookAt);
  }

  _finishIntro() {
    // Fade out overlay
    this._overlay.container.style.transition = 'opacity 1s ease';
    this._overlay.container.style.opacity = '0';

    setTimeout(() => {
      this._overlay.container.remove();
      document.removeEventListener('keydown', this._skipHandler);
      document.removeEventListener('click', this._skipHandler);
      this.onComplete?.();
    }, 1000);
  }

  dispose() {
    this._overlay?.container?.remove();
  }
}
