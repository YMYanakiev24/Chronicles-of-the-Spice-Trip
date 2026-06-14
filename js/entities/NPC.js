import * as THREE from 'three';
import { NPCS } from '../data/npcs.js';

// NPCs — interactive characters with dialogue trees
export class NPC {
  constructor(npcId, scene, worldBuilder) {
    this.id = npcId;
    this.def = NPCS[npcId];
    this.scene = scene;
    this.worldBuilder = worldBuilder;
    this.interactionRadius = 4.5;
    this._currentDialogueNode = 'greeting';
    this._questsStarted = new Set();

    this._build();
  }

  _build() {
    const d = this.def;
    const group = new THREE.Group();

    const px = d.position.x;
    const pz = d.position.z;
    const py = this.worldBuilder.getTerrainHeight(px, pz);
    group.position.set(px, py, pz);

    // Body
    const bodyMat = new THREE.MeshLambertMaterial({ color: d.color });
    const body = new THREE.Mesh(new THREE.CylinderGeometry(0.4, 0.4, 1.2, 8), bodyMat);
    body.position.y = 0.6;
    group.add(body);

    // Head
    const headMat = new THREE.MeshLambertMaterial({ color: 0xd4a868 });
    const head = new THREE.Mesh(new THREE.SphereGeometry(0.28, 8, 8), headMat);
    head.position.y = 1.55;
    group.add(head);

    // Role-specific features
    if (d.id === 'magister_aelion') {
      // Wizard hat
      const hat = new THREE.Mesh(new THREE.ConeGeometry(0.28, 0.6, 8), new THREE.MeshLambertMaterial({ color: 0x223388 }));
      hat.position.y = 1.95;
      group.add(hat);
      // Staff
      const staff = new THREE.Mesh(new THREE.CylinderGeometry(0.04, 0.04, 1.6, 6), new THREE.MeshLambertMaterial({ color: 0x5a3010 }));
      staff.position.set(0.5, 0.8, 0);
      group.add(staff);
      const staffOrb = new THREE.Mesh(new THREE.SphereGeometry(0.12, 8, 8), new THREE.MeshBasicMaterial({ color: 0x4488ff }));
      staffOrb.position.set(0.5, 1.65, 0);
      const staffLight = new THREE.PointLight(0x4488ff, 1.5, 5);
      staffLight.position.copy(staffOrb.position);
      group.add(staffOrb);
      group.add(staffLight);
    } else if (d.id === 'sentinel_gareth') {
      // Shield
      const shield = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.7, 0.1), new THREE.MeshLambertMaterial({ color: 0x888899 }));
      shield.position.set(-0.55, 0.7, 0.1);
      // Emblem on shield
      const emblem = new THREE.Mesh(new THREE.CircleGeometry(0.15, 8), new THREE.MeshBasicMaterial({ color: 0xddaa00 }));
      emblem.position.set(-0.55, 0.7, 0.15);
      group.add(shield);
      group.add(emblem);
      // Helmet
      const helm = new THREE.Mesh(new THREE.SphereGeometry(0.3, 8, 8, 0, Math.PI * 2, 0, Math.PI * 0.55), new THREE.MeshLambertMaterial({ color: 0x888899 }));
      helm.position.y = 1.55;
      group.add(helm);
    } else if (d.id === 'old_bramble') {
      // Make it look like a tree spirit
      const branchMat = new THREE.MeshLambertMaterial({ color: 0x3d2b0f });
      const trunkNPC = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.45, 1.8, 8), branchMat);
      trunkNPC.position.y = 0.9;
      group.add(trunkNPC);
      // Leaf clusters
      const leafMat = new THREE.MeshLambertMaterial({ color: 0x1d5c1a });
      for (let i = 0; i < 4; i++) {
        const leaf = new THREE.Mesh(new THREE.SphereGeometry(0.4, 6, 6), leafMat);
        leaf.position.set((Math.random() - 0.5) * 0.8, 2.0 + Math.random() * 0.6, (Math.random() - 0.5) * 0.8);
        group.add(leaf);
      }
    } else if (d.id === 'ancient_spirit') {
      // Glowing ethereal form
      const spiritMat = new THREE.MeshBasicMaterial({ color: 0xffcc44, transparent: true, opacity: 0.6 });
      const spiritBody = new THREE.Mesh(new THREE.SphereGeometry(0.6, 12, 12), spiritMat);
      spiritBody.position.y = 1;
      group.add(spiritBody);
      const spiritGlow = new THREE.PointLight(0xffcc44, 3, 10);
      spiritGlow.position.y = 1;
      group.add(spiritGlow);
      // Inner core
      const core = new THREE.Mesh(new THREE.SphereGeometry(0.2, 8, 8), new THREE.MeshBasicMaterial({ color: 0xffffff }));
      core.position.y = 1;
      group.add(core);
    } else if (d.id === 'kaelos_alchemist') {
      // Apron / merchant
      const apron = new THREE.Mesh(new THREE.PlaneGeometry(0.7, 0.9), new THREE.MeshLambertMaterial({ color: 0x4a3a20, side: THREE.DoubleSide }));
      apron.position.set(0, 0.6, 0.41);
      group.add(apron);
    }

    // Floating name tag
    this._buildNameTag(group);

    // Interaction indicator (floating exclamation mark)
    this._exclamation = this._buildExclamation();
    group.add(this._exclamation);

    // Idle animation reference
    this._baseY = py;
    this._bobTime = Math.random() * Math.PI * 2;

    this.mesh = group;
    this.scene.add(group);
  }

  _buildNameTag(group) {
    // Simple colored sphere above head to indicate interaction
    const tagMesh = new THREE.Mesh(
      new THREE.SphereGeometry(0.08, 6, 6),
      new THREE.MeshBasicMaterial({ color: 0xffdd44 })
    );
    tagMesh.position.y = 2.1;
    group.add(tagMesh);
    const tagLight = new THREE.PointLight(0xffdd44, 1, 4);
    tagLight.position.y = 2.1;
    group.add(tagLight);
    this._nameTagLight = tagLight;
  }

  _buildExclamation() {
    const group = new THREE.Group();
    group.position.y = 2.4;

    // "!" symbol as two stacked cylinders
    const mat = new THREE.MeshBasicMaterial({ color: 0xffdd00 });
    const bar = new THREE.Mesh(new THREE.CylinderGeometry(0.06, 0.06, 0.35, 6), mat);
    bar.position.y = 0.18;
    const dot = new THREE.Mesh(new THREE.SphereGeometry(0.07, 6, 6), mat);
    dot.position.y = -0.04;

    group.add(bar);
    group.add(dot);

    return group;
  }

  update(delta, time) {
    if (!this.mesh) return;

    // Idle bob animation
    this._bobTime += delta;
    if (this.id !== 'old_bramble') {
      this.mesh.position.y = this._baseY + Math.sin(this._bobTime * 0.8) * 0.05;
    }

    // Exclamation bob
    if (this._exclamation) {
      this._exclamation.position.y = 2.4 + Math.sin(this._bobTime * 2) * 0.1;
      this._exclamation.rotation.y = this._bobTime * 0.5;
    }

    // Ancient spirit — rotate and pulse
    if (this.id === 'ancient_spirit') {
      this.mesh.rotation.y = time * 0.3;
      if (this._nameTagLight) this._nameTagLight.intensity = 1 + Math.sin(time * 2) * 0.5;
    }
  }

  get position() {
    return this.mesh ? this.mesh.position : new THREE.Vector3();
  }

  markQuestComplete() {
    if (this._exclamation) {
      this._exclamation.children.forEach(c => {
        if (c.material) c.material.color.setHex(0x44ff88);
      });
    }
  }

  setDialogueNode(node) {
    this._currentDialogueNode = node;
  }

  getDialogueNode(node) {
    return this.def.dialogues[node] || this.def.dialogues.greeting;
  }

  dispose() {
    this.scene.remove(this.mesh);
    this.mesh.traverse(c => {
      if (c.geometry) c.geometry.dispose();
      if (c.material) c.material.dispose();
    });
  }
}
