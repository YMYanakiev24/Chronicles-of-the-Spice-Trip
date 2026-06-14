import * as THREE from 'three';

// Procedural world builder — terrain, trees, ruins, castle, ambient objects
export class WorldBuilder {
  constructor(scene) {
    this.scene = scene;
    this.collidables = []; // For terrain height queries
    this.interactables = []; // NPCs, items, portals
    this.terrainMesh = null;
    this._heightData = null;
    this._terrainSize = 200;
    this._terrainSegments = 128;
  }

  build() {
    this._buildSky();
    this._buildTerrain();
    this._buildForest();
    this._buildCastle();
    this._buildVillage();
    this._buildRuins();
    this._buildTemple();
    this._buildLighting();
    this._buildFog();
  }

  _buildSky() {
    // Gradient sky using a large dome
    const skyGeom = new THREE.SphereGeometry(500, 32, 16);
    skyGeom.scale(-1, 1, 1); // Flip inside out

    const skyMat = new THREE.ShaderMaterial({
      uniforms: {
        topColor: { value: new THREE.Color(0x060318) },
        bottomColor: { value: new THREE.Color(0x1a0a30) },
        offset: { value: 20 },
        exponent: { value: 0.6 }
      },
      vertexShader: `
        varying vec3 vWorldPosition;
        void main() {
          vec4 worldPosition = modelMatrix * vec4(position, 1.0);
          vWorldPosition = worldPosition.xyz;
          gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
      `,
      fragmentShader: `
        uniform vec3 topColor, bottomColor;
        uniform float offset, exponent;
        varying vec3 vWorldPosition;
        void main() {
          float h = normalize(vWorldPosition + offset).y;
          gl_FragColor = vec4(mix(bottomColor, topColor, max(pow(max(h, 0.0), exponent), 0.0)), 1.0);
        }
      `,
      side: THREE.BackSide
    });

    this.scene.add(new THREE.Mesh(skyGeom, skyMat));

    // Stars
    const starCount = 800;
    const starGeom = new THREE.BufferGeometry();
    const starPos = new Float32Array(starCount * 3);
    for (let i = 0; i < starCount; i++) {
      const i3 = i * 3;
      const phi = Math.random() * Math.PI;
      const theta = Math.random() * Math.PI * 2;
      const r = 400 + Math.random() * 50;
      starPos[i3]     = r * Math.sin(phi) * Math.cos(theta);
      starPos[i3 + 1] = r * Math.abs(Math.cos(phi));
      starPos[i3 + 2] = r * Math.sin(phi) * Math.sin(theta);
    }
    starGeom.setAttribute('position', new THREE.Float32BufferAttribute(starPos, 3));
    const starMat = new THREE.PointsMaterial({
      color: 0xffffff, size: 0.5, transparent: true, opacity: 0.8,
      blending: THREE.AdditiveBlending, depthWrite: false
    });
    this.scene.add(new THREE.Points(starGeom, starMat));

    // Moon
    const moonGeom = new THREE.SphereGeometry(8, 16, 16);
    const moonMat = new THREE.MeshBasicMaterial({ color: 0xd4c8a8 });
    const moon = new THREE.Mesh(moonGeom, moonMat);
    moon.position.set(150, 200, -300);
    this.scene.add(moon);

    // Moon glow
    const moonGlowGeom = new THREE.SphereGeometry(14, 16, 16);
    const moonGlowMat = new THREE.MeshBasicMaterial({
      color: 0x8899bb, transparent: true, opacity: 0.15, side: THREE.BackSide
    });
    moon.add(new THREE.Mesh(moonGlowGeom, moonGlowMat));
  }

  _buildTerrain() {
    const size = this._terrainSize;
    const segs = this._terrainSegments;

    const geom = new THREE.PlaneGeometry(size, size, segs, segs);
    geom.rotateX(-Math.PI / 2);

    const positions = geom.attributes.position.array;
    const heights = new Float32Array((segs + 1) * (segs + 1));

    // Procedural height using layered noise
    for (let i = 0; i <= segs; i++) {
      for (let j = 0; j <= segs; j++) {
        const idx = i * (segs + 1) + j;
        const x = (j / segs - 0.5) * size;
        const z = (i / segs - 0.5) * size;

        let h = 0;
        h += this._noise(x * 0.02, z * 0.02) * 8;
        h += this._noise(x * 0.05, z * 0.05) * 4;
        h += this._noise(x * 0.12, z * 0.12) * 2;
        h += this._noise(x * 0.3, z * 0.3) * 0.8;

        // Flatten the village center
        const distFromCenter = Math.sqrt(x * x + z * z);
        if (distFromCenter < 18) h *= (distFromCenter / 18);

        // Big hill for castle (position: 40, -60)
        const dx = x - 40, dz = z + 60;
        const hillDist = Math.sqrt(dx * dx + dz * dz);
        if (hillDist < 25) {
          const hillH = Math.cos((hillDist / 25) * Math.PI * 0.5) * 18;
          h += hillH;
        }

        heights[idx] = h;
      }
    }

    // Apply heights
    for (let i = 0; i <= segs; i++) {
      for (let j = 0; j <= segs; j++) {
        const idx = (i * (segs + 1) + j) * 3;
        positions[idx + 1] = heights[i * (segs + 1) + j];
      }
    }

    this._heightData = heights;
    geom.computeVertexNormals();

    const mat = new THREE.MeshLambertMaterial({ color: 0x1a3d18 });
    const terrain = new THREE.Mesh(geom, mat);
    terrain.receiveShadow = true;
    terrain.name = 'terrain';
    this.scene.add(terrain);
    this.terrainMesh = terrain;

    // Ground paths
    this._addPaths();
  }

  _addPaths() {
    // Dirt paths between areas
    const pathMat = new THREE.MeshLambertMaterial({ color: 0x5c4a2e });
    const addPath = (from, to, width = 2) => {
      const dx = to.x - from.x, dz = to.z - from.z;
      const len = Math.sqrt(dx * dx + dz * dz);
      const geom = new THREE.PlaneGeometry(width, len, 1, 1);
      geom.rotateX(-Math.PI / 2);
      const mesh = new THREE.Mesh(geom, pathMat);
      mesh.position.set((from.x + to.x) / 2, 0.05, (from.z + to.z) / 2);
      mesh.rotation.y = Math.atan2(dx, dz);
      this.scene.add(mesh);
    };
    addPath({ x: 0, z: 0 }, { x: 40, z: -30 });
    addPath({ x: 0, z: 0 }, { x: 30, z: 20 });
    addPath({ x: 0, z: 0 }, { x: -20, z: -15 });
  }

  getTerrainHeight(x, z) {
    if (!this._heightData) return 0;
    const size = this._terrainSize;
    const segs = this._terrainSegments;
    // Convert world coords to grid coords
    const gx = ((x + size / 2) / size) * segs;
    const gz = ((z + size / 2) / size) * segs;
    const ix = Math.floor(gx), iz = Math.floor(gz);
    const fx = gx - ix, fz = gz - iz;
    const clamp = (v, min, max) => Math.max(min, Math.min(max, v));
    const i0 = clamp(ix, 0, segs);
    const i1 = clamp(ix + 1, 0, segs);
    const j0 = clamp(iz, 0, segs);
    const j1 = clamp(iz + 1, 0, segs);
    const h00 = this._heightData[j0 * (segs + 1) + i0];
    const h10 = this._heightData[j0 * (segs + 1) + i1];
    const h01 = this._heightData[j1 * (segs + 1) + i0];
    const h11 = this._heightData[j1 * (segs + 1) + i1];
    return h00 * (1 - fx) * (1 - fz) + h10 * fx * (1 - fz) + h01 * (1 - fx) * fz + h11 * fx * fz;
  }

  _noise(x, z) {
    // Simple value noise
    const xi = Math.floor(x), zi = Math.floor(z);
    const xf = x - xi, zf = z - zi;
    const u = xf * xf * (3 - 2 * xf);
    const v = zf * zf * (3 - 2 * zf);
    const h = (n) => {
      let s = Math.sin(n * 127.1 + 311.7) * 43758.5453;
      return s - Math.floor(s);
    };
    const r = (a, b) => h(a * 1000 + b);
    return (r(xi, zi) * (1 - u) + r(xi + 1, zi) * u) * (1 - v)
         + (r(xi, zi + 1) * (1 - u) + r(xi + 1, zi + 1) * u) * v;
  }

  _buildForest() {
    const treePositions = [];
    const rng = (min, max) => min + Math.random() * (max - min);

    // Dense forest to the north and west
    for (let i = 0; i < 200; i++) {
      let x, z;
      // Avoid village center
      do {
        x = rng(-90, 90);
        z = rng(-90, 90);
      } while (Math.sqrt(x * x + z * z) < 20 || (x > 30 && z > 10) || (x > 35 && z < -50));
      treePositions.push({ x, z });
    }

    treePositions.forEach(pos => {
      const tree = this._makeTree(pos.x, pos.z);
      this.scene.add(tree);
    });

    // Enchanted glowing trees scattered around
    for (let i = 0; i < 20; i++) {
      const x = rng(-70, 70);
      const z = rng(-70, 70);
      if (Math.sqrt(x * x + z * z) < 15) continue;
      this._makeGlowTree(x, z);
    }
  }

  _makeTree(x, z) {
    const group = new THREE.Group();
    const h = this.getTerrainHeight(x, z);
    group.position.set(x, h, z);

    const treeH = 3 + Math.random() * 3;
    const scale = 0.7 + Math.random() * 0.6;

    // Trunk
    const trunkGeom = new THREE.CylinderGeometry(0.2 * scale, 0.35 * scale, treeH, 6);
    const trunkMat = new THREE.MeshLambertMaterial({ color: 0x3d2b1f });
    const trunk = new THREE.Mesh(trunkGeom, trunkMat);
    trunk.position.y = treeH / 2;
    trunk.castShadow = true;
    group.add(trunk);

    // Foliage layers
    const leafColor = new THREE.Color().setHSL(0.28 + Math.random() * 0.05, 0.55 + Math.random() * 0.2, 0.12 + Math.random() * 0.08);
    const leafMat = new THREE.MeshLambertMaterial({ color: leafColor });

    for (let i = 0; i < 3; i++) {
      const coneGeom = new THREE.ConeGeometry((1.8 - i * 0.3) * scale, (2 + i * 0.3) * scale, 7);
      const cone = new THREE.Mesh(coneGeom, leafMat);
      cone.position.y = treeH - 0.5 + i * 1.2 * scale;
      cone.rotation.y = Math.random() * Math.PI;
      cone.castShadow = true;
      group.add(cone);
    }

    group.rotation.y = Math.random() * Math.PI * 2;
    return group;
  }

  _makeGlowTree(x, z) {
    const group = new THREE.Group();
    const h = this.getTerrainHeight(x, z);
    group.position.set(x, h, z);

    const trunk = new THREE.Mesh(
      new THREE.CylinderGeometry(0.15, 0.28, 4, 6),
      new THREE.MeshLambertMaterial({ color: 0x1a0a2a })
    );
    trunk.position.y = 2;
    group.add(trunk);

    const glowColors = [0x44ff99, 0xaa44ff, 0x44aaff];
    const gc = glowColors[Math.floor(Math.random() * glowColors.length)];

    const leafMat = new THREE.MeshBasicMaterial({ color: gc, transparent: true, opacity: 0.7 });
    for (let i = 0; i < 2; i++) {
      const cone = new THREE.Mesh(new THREE.ConeGeometry(1.2 - i * 0.2, 2.5, 7), leafMat);
      cone.position.y = 3.5 + i * 1.2;
      group.add(cone);
    }

    // Magical glow light
    const light = new THREE.PointLight(gc, 1.5, 8);
    light.position.y = 5;
    group.add(light);

    this.scene.add(group);
  }

  _buildCastle() {
    const group = new THREE.Group();
    // Castle sits on the hill at (40, h, -60)
    const hillY = this.getTerrainHeight(40, -60);
    group.position.set(40, hillY, -60);

    const stoneMat = new THREE.MeshLambertMaterial({ color: 0x8a7d6b });
    const darkStoneMat = new THREE.MeshLambertMaterial({ color: 0x4a3d30 });

    // Main keep
    const keep = new THREE.Mesh(new THREE.BoxGeometry(16, 22, 16), stoneMat);
    keep.position.y = 11;
    keep.castShadow = true;
    group.add(keep);

    // Battlements
    for (let i = 0; i < 8; i++) {
      const angle = (i / 8) * Math.PI * 2;
      const b = new THREE.Mesh(new THREE.BoxGeometry(2, 3, 2), stoneMat);
      b.position.set(Math.cos(angle) * 7.5, 23, Math.sin(angle) * 7.5);
      group.add(b);
    }

    // Corner towers
    const towerPositions = [[-7, -7], [7, -7], [-7, 7], [7, 7]];
    towerPositions.forEach(([tx, tz]) => {
      const tower = new THREE.Mesh(new THREE.CylinderGeometry(2.8, 3, 28, 10), stoneMat);
      tower.position.set(tx, 14, tz);
      tower.castShadow = true;
      group.add(tower);

      // Tower roof
      const roof = new THREE.Mesh(new THREE.ConeGeometry(3.2, 7, 10), darkStoneMat);
      roof.position.set(tx, 31.5, tz);
      group.add(roof);

      // Glowing window
      const winMat = new THREE.MeshBasicMaterial({ color: 0xffaa22, transparent: true, opacity: 0.9 });
      const win = new THREE.Mesh(new THREE.BoxGeometry(0.6, 1.0, 0.1), winMat);
      win.position.set(tx, 18, tz + 3);
      const winLight = new THREE.PointLight(0xffaa22, 2.5, 12);
      winLight.position.copy(win.position);
      group.add(win);
      group.add(winLight);

      // Floating tower effect — slight hover animation reference stored
      tower._baseY = 14;
    });

    // Main gate
    const gate = new THREE.Mesh(new THREE.BoxGeometry(5, 8, 1), darkStoneMat);
    gate.position.set(0, 4, 8.5);
    group.add(gate);

    // Gate arch
    const archGeom = new THREE.TorusGeometry(2, 0.4, 8, 12, Math.PI);
    const arch = new THREE.Mesh(archGeom, stoneMat);
    arch.position.set(0, 8, 8.5);
    arch.rotation.z = Math.PI;
    group.add(arch);

    // Magical rune circle at entrance
    const runeGeom = new THREE.TorusGeometry(4, 0.15, 8, 32);
    const runeMat = new THREE.MeshBasicMaterial({ color: 0x4466ff, transparent: true, opacity: 0.6 });
    const runeRing = new THREE.Mesh(runeGeom, runeMat);
    runeRing.rotation.x = -Math.PI / 2;
    runeRing.position.set(0, 0.1, 12);
    group.add(runeRing);

    // Central glowing orb atop keep
    const orb = new THREE.Mesh(new THREE.SphereGeometry(1.5, 16, 16), new THREE.MeshBasicMaterial({ color: 0x8844ff }));
    orb.position.y = 26;
    group.add(orb);
    const orbLight = new THREE.PointLight(0x8844ff, 4, 30);
    orbLight.position.y = 26;
    group.add(orbLight);
    const orbGlow = new THREE.Mesh(new THREE.SphereGeometry(2.5, 16, 16), new THREE.MeshBasicMaterial({ color: 0xaa66ff, transparent: true, opacity: 0.2, side: THREE.BackSide }));
    orbGlow.position.y = 26;
    group.add(orbGlow);

    // Magical symbol on keep wall (simplified as emissive plane)
    const symbolMat = new THREE.MeshBasicMaterial({ color: 0x4466ff, transparent: true, opacity: 0.5 });
    const symbol = new THREE.Mesh(new THREE.PlaneGeometry(6, 6), symbolMat);
    symbol.position.set(0, 12, 8.1);
    group.add(symbol);

    // Courtyard wall
    const wallMat = new THREE.MeshLambertMaterial({ color: 0x7a6e5c });
    const walls = [
      { x: 0, z: 14, w: 20, d: 1, h: 5 },
      { x: -10, z: 7, w: 1, d: 14, h: 5 },
      { x: 10, z: 7, w: 1, d: 14, h: 5 }
    ];
    walls.forEach(w => {
      const wall = new THREE.Mesh(new THREE.BoxGeometry(w.w, w.h, w.d), wallMat);
      wall.position.set(w.x, w.h / 2, w.z);
      group.add(wall);
    });

    // Animated floating rune stones
    for (let i = 0; i < 4; i++) {
      const angle = (i / 4) * Math.PI * 2;
      const stone = new THREE.Mesh(
        new THREE.OctahedronGeometry(0.6, 0),
        new THREE.MeshBasicMaterial({ color: 0x6688ff, transparent: true, opacity: 0.8 })
      );
      stone.position.set(Math.cos(angle) * 12, 18, Math.sin(angle) * 12);
      stone._floatAngle = angle;
      stone._floatBase = 18;
      stone.name = 'floatingRune';
      group.add(stone);
      const stoneLight = new THREE.PointLight(0x6688ff, 1.5, 8);
      stone.add(stoneLight);
    }

    this.scene.add(group);
    this.castleGroup = group;
  }

  updateCastle(time) {
    if (!this.castleGroup) return;
    this.castleGroup.traverse(obj => {
      if (obj.name === 'floatingRune') {
        obj.position.y = obj._floatBase + Math.sin(time * 1.5 + obj._floatAngle) * 1.5;
        obj.rotation.y = time * 0.8;
        obj.rotation.x = time * 0.5;
      }
    });
  }

  _buildVillage() {
    const buildings = [
      { x: 8, z: -8, w: 5, h: 5, d: 5, roof: 0xaa4422, color: 0xc8a882, name: 'inn' },
      { x: -10, z: -6, w: 4, h: 4, d: 5, roof: 0x554433, color: 0xb09070, name: 'shop' },
      { x: 12, z: 8, w: 4, h: 4.5, d: 4, roof: 0x885533, color: 0xc0a070, name: 'house1' },
      { x: -12, z: 8, w: 4, h: 4, d: 4, roof: 0x664422, color: 0xb09060, name: 'house2' },
      { x: 0, z: -14, w: 7, h: 7, d: 7, roof: 0x446688, color: 0xd0c0a0, name: 'temple' }
    ];

    const addBuilding = (b) => {
      const group = new THREE.Group();
      const gh = this.getTerrainHeight(b.x, b.z);
      group.position.set(b.x, gh, b.z);

      const walls = new THREE.Mesh(
        new THREE.BoxGeometry(b.w, b.h, b.d),
        new THREE.MeshLambertMaterial({ color: b.color })
      );
      walls.position.y = b.h / 2;
      walls.castShadow = true;
      group.add(walls);

      // Roof
      const roof = new THREE.Mesh(
        new THREE.ConeGeometry(Math.max(b.w, b.d) * 0.8, b.h * 0.7, 4),
        new THREE.MeshLambertMaterial({ color: b.roof })
      );
      roof.position.y = b.h + b.h * 0.3;
      roof.rotation.y = Math.PI / 4;
      group.add(roof);

      // Door
      const doorMat = new THREE.MeshLambertMaterial({ color: 0x5c3a1e });
      const door = new THREE.Mesh(new THREE.BoxGeometry(0.9, 1.6, 0.15), doorMat);
      door.position.set(0, 0.8, b.d / 2 + 0.05);
      group.add(door);

      // Window light
      const winMat = new THREE.MeshBasicMaterial({ color: 0xffcc66, transparent: true, opacity: 0.8 });
      [[-1, 0.5], [1, 0.5]].forEach(([wx, wy]) => {
        const win = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.6, 0.1), winMat);
        win.position.set(wx, wy + b.h * 0.5, b.d / 2 + 0.05);
        group.add(win);
      });

      const warmLight = new THREE.PointLight(0xff9933, 1.5, 8);
      warmLight.position.set(0, b.h * 0.8, b.d / 2);
      group.add(warmLight);

      this.scene.add(group);
    };

    buildings.forEach(addBuilding);

    // Village well
    const wellGroup = new THREE.Group();
    wellGroup.position.set(0, this.getTerrainHeight(0, 0), 0);
    const wellBase = new THREE.Mesh(new THREE.CylinderGeometry(1.2, 1.2, 1, 12), new THREE.MeshLambertMaterial({ color: 0x888877 }));
    wellBase.position.y = 0.5;
    const wellInner = new THREE.Mesh(new THREE.CylinderGeometry(0.7, 0.7, 1.2, 12), new THREE.MeshLambertMaterial({ color: 0x0a1a2a }));
    wellInner.position.y = 0.6;
    wellGroup.add(wellBase);
    wellGroup.add(wellInner);
    // Magical water glow
    const waterLight = new THREE.PointLight(0x4488ff, 1, 5);
    waterLight.position.y = 0.5;
    wellGroup.add(waterLight);
    this.scene.add(wellGroup);

    // Torches
    for (let i = 0; i < 6; i++) {
      const angle = (i / 6) * Math.PI * 2;
      this._placeTorch(Math.cos(angle) * 14, Math.sin(angle) * 14);
    }
  }

  _placeTorch(x, z) {
    const group = new THREE.Group();
    const h = this.getTerrainHeight(x, z);
    group.position.set(x, h, z);

    const post = new THREE.Mesh(new THREE.CylinderGeometry(0.08, 0.1, 1.5, 6), new THREE.MeshLambertMaterial({ color: 0x5c3a1e }));
    post.position.y = 0.75;
    group.add(post);

    const head = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.1, 0.25, 6), new THREE.MeshLambertMaterial({ color: 0x333333 }));
    head.position.y = 1.6;
    group.add(head);

    const flame = new THREE.Mesh(new THREE.ConeGeometry(0.12, 0.3, 6), new THREE.MeshBasicMaterial({ color: 0xff6600 }));
    flame.position.y = 1.85;
    group.add(flame);

    const torchLight = new THREE.PointLight(0xff6600, 2, 7);
    torchLight.position.y = 2;
    group.add(torchLight);

    this.scene.add(group);
  }

  _buildRuins() {
    // Ancient ruins to the east
    const group = new THREE.Group();
    const h = this.getTerrainHeight(35, 0);
    group.position.set(35, h, 0);

    const stoneMat = new THREE.MeshLambertMaterial({ color: 0x7a7060 });
    const mossyMat = new THREE.MeshLambertMaterial({ color: 0x506040 });

    // Broken columns
    for (let i = 0; i < 6; i++) {
      const angle = (i / 6) * Math.PI * 2;
      const heightVal = 2 + Math.random() * 3;
      const col = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.6, heightVal, 8), i % 2 === 0 ? stoneMat : mossyMat);
      col.position.set(Math.cos(angle) * 5, heightVal / 2, Math.sin(angle) * 5);
      col.rotation.z = (Math.random() - 0.5) * 0.15;
      col.castShadow = true;
      group.add(col);
    }

    // Fallen arch
    const archLeft = new THREE.Mesh(new THREE.BoxGeometry(0.8, 5, 0.8), stoneMat);
    archLeft.position.set(-3, 2.5, 0);
    group.add(archLeft);
    const archRight = new THREE.Mesh(new THREE.BoxGeometry(0.8, 4, 0.8), stoneMat);
    archRight.position.set(3, 2, 0);
    group.add(archRight);
    const archTop = new THREE.Mesh(new THREE.BoxGeometry(6.8, 0.8, 0.8), stoneMat);
    archTop.position.set(0, 5, 0);
    archTop.rotation.z = 0.1;
    group.add(archTop);

    // Rubble
    for (let i = 0; i < 15; i++) {
      const s = 0.3 + Math.random() * 0.7;
      const rock = new THREE.Mesh(new THREE.BoxGeometry(s, s * 0.6, s), stoneMat);
      rock.position.set((Math.random() - 0.5) * 14, s * 0.3, (Math.random() - 0.5) * 14);
      rock.rotation.set(Math.random(), Math.random(), Math.random());
      group.add(rock);
    }

    // Glowing rune at center
    const runeMat = new THREE.MeshBasicMaterial({ color: 0x4422aa, transparent: true, opacity: 0.7 });
    const runeGeom = new THREE.CircleGeometry(3, 12);
    const runeCircle = new THREE.Mesh(runeGeom, runeMat);
    runeCircle.rotation.x = -Math.PI / 2;
    runeCircle.position.y = 0.1;
    group.add(runeCircle);
    const runeLight = new THREE.PointLight(0x4422aa, 2, 10);
    runeLight.position.y = 1;
    group.add(runeLight);

    this.scene.add(group);
    this.ruinsGroup = group;

    // Marker interactable
    this.interactables.push({
      type: 'location',
      id: 'ancient_ruins',
      position: new THREE.Vector3(35, h, 0),
      radius: 8
    });
  }

  _buildTemple() {
    // Forgotten temple to the west
    const group = new THREE.Group();
    const h = this.getTerrainHeight(-35, -20);
    group.position.set(-35, h, -20);

    const stoneMat = new THREE.MeshLambertMaterial({ color: 0x9a8870 });
    const goldMat = new THREE.MeshBasicMaterial({ color: 0xddaa00, transparent: true, opacity: 0.9 });

    // Temple platform
    const platform = new THREE.Mesh(new THREE.BoxGeometry(22, 1.2, 18), stoneMat);
    platform.position.y = 0.6;
    group.add(platform);

    // Steps
    for (let i = 0; i < 3; i++) {
      const step = new THREE.Mesh(new THREE.BoxGeometry(14 - i * 2, 0.5, 2), stoneMat);
      step.position.set(0, 0.7 + i * 0.5, 9 + i * 2);
      group.add(step);
    }

    // Columns
    for (let i = 0; i < 5; i++) {
      const x = -8 + i * 4;
      [-7, 7].forEach(z => {
        const col = new THREE.Mesh(new THREE.CylinderGeometry(0.6, 0.7, 9, 8), stoneMat);
        col.position.set(x, 5.7, z);
        col.castShadow = true;
        group.add(col);
      });
    }

    // Roof
    const roof = new THREE.Mesh(new THREE.BoxGeometry(20, 1, 16), stoneMat);
    roof.position.y = 10.5;
    group.add(roof);
    const frontierRoof = new THREE.Mesh(new THREE.BoxGeometry(22, 0.8, 1), stoneMat);
    frontierRoof.position.set(0, 10.5, 8.5);
    group.add(frontierRoof);

    // Pediment (triangular front)
    const pedimentGeom = new THREE.CylinderGeometry(0, 10, 3, 3);
    const pediment = new THREE.Mesh(pedimentGeom, stoneMat);
    pediment.position.set(0, 12.5, 0);
    pediment.rotation.y = Math.PI;
    group.add(pediment);

    // Divine altar inside
    const altar = new THREE.Mesh(new THREE.BoxGeometry(3, 2, 2), new THREE.MeshLambertMaterial({ color: 0xddccaa }));
    altar.position.set(0, 2.2, -2);
    group.add(altar);

    // Sacred flame
    const flame = new THREE.Mesh(new THREE.ConeGeometry(0.5, 1.5, 8), goldMat);
    flame.position.set(0, 4, -2);
    group.add(flame);
    const divineLight = new THREE.PointLight(0xffdd44, 4, 15);
    divineLight.position.set(0, 5, -2);
    group.add(divineLight);

    // Gold trim on columns
    for (let i = 0; i < 5; i++) {
      const x = -8 + i * 4;
      [-7, 7].forEach(z => {
        const cap = new THREE.Mesh(new THREE.CylinderGeometry(0.8, 0.65, 0.4, 8), new THREE.MeshLambertMaterial({ color: 0xddaa00 }));
        cap.position.set(x, 10.3, z);
        group.add(cap);
      });
    }

    this.scene.add(group);

    this.interactables.push({
      type: 'location',
      id: 'forgotten_temple',
      position: new THREE.Vector3(-35, h, -20),
      radius: 12
    });
  }

  _buildLighting() {
    // Moon directional light
    const moonLight = new THREE.DirectionalLight(0x8899cc, 0.6);
    moonLight.position.set(50, 80, -100);
    moonLight.castShadow = true;
    moonLight.shadow.mapSize.width = 2048;
    moonLight.shadow.mapSize.height = 2048;
    moonLight.shadow.camera.near = 1;
    moonLight.shadow.camera.far = 300;
    moonLight.shadow.camera.left = -80;
    moonLight.shadow.camera.right = 80;
    moonLight.shadow.camera.top = 80;
    moonLight.shadow.camera.bottom = -80;
    this.scene.add(moonLight);

    // Warm ambient
    const ambient = new THREE.AmbientLight(0x0a0818, 0.8);
    this.scene.add(ambient);

    // Hemisphere light for sky/ground distinction
    const hemi = new THREE.HemisphereLight(0x1a1040, 0x0d1a0d, 0.5);
    this.scene.add(hemi);
  }

  _buildFog() {
    this.scene.fog = new THREE.FogExp2(0x050310, 0.012);
  }

  dispose() {
    this.scene.traverse(obj => {
      if (obj.geometry) obj.geometry.dispose();
      if (obj.material) {
        if (Array.isArray(obj.material)) obj.material.forEach(m => m.dispose());
        else obj.material.dispose();
      }
    });
  }
}
