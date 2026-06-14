import * as THREE from 'three';
import { Input } from './Input.js';
import { Audio } from './Audio.js';

// Central game engine — manages renderer, scene switching, game loop
export class Engine {
  constructor() {
    this.canvas = document.getElementById('gameCanvas');
    this.renderer = new THREE.WebGLRenderer({
      canvas: this.canvas,
      antialias: true,
      powerPreference: 'high-performance'
    });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.setSize(window.innerWidth, window.innerHeight);
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.2;

    this.clock = new THREE.Clock();
    this.input = new Input();
    this.audio = new Audio();

    this.currentScene = null;
    this._rafId = null;
    this._paused = false;

    this.settings = {
      musicVolume: 0.6,
      sfxVolume: 0.8,
      camSensitivity: 5,
      quality: 'medium'
    };

    window.addEventListener('resize', () => this._onResize());
    this._loadSettings();
  }

  _onResize() {
    this.renderer.setSize(window.innerWidth, window.innerHeight);
    if (this.currentScene && this.currentScene.camera) {
      this.currentScene.camera.aspect = window.innerWidth / window.innerHeight;
      this.currentScene.camera.updateProjectionMatrix();
    }
  }

  start() {
    this.audio.init();
    this._loop();
  }

  _loop() {
    this._rafId = requestAnimationFrame(() => this._loop());
    const delta = Math.min(this.clock.getDelta(), 0.1); // cap at 100ms

    if (!this._paused && this.currentScene) {
      this.currentScene.update(delta);
      this.renderer.render(this.currentScene.scene, this.currentScene.camera);
    }
  }

  switchScene(newScene) {
    if (this.currentScene && this.currentScene.dispose) {
      this.currentScene.dispose();
    }
    this.currentScene = newScene;
    if (newScene.init) newScene.init();
  }

  pause() { this._paused = true; }
  resume() { this._paused = false; }

  saveSettings() {
    localStorage.setItem('mythic_settings', JSON.stringify(this.settings));
    this.audio.setMusicVolume(this.settings.musicVolume);
    this.audio.setSfxVolume(this.settings.sfxVolume);
  }

  _loadSettings() {
    try {
      const saved = localStorage.getItem('mythic_settings');
      if (saved) Object.assign(this.settings, JSON.parse(saved));
    } catch(e) {}
  }

  getSaveData() {
    try {
      const data = localStorage.getItem('mythic_save');
      return data ? JSON.parse(data) : null;
    } catch(e) { return null; }
  }

  setSaveData(data) {
    localStorage.setItem('mythic_save', JSON.stringify(data));
  }
}
