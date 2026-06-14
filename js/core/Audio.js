// Procedural audio using Web Audio API
export class Audio {
  constructor() {
    this.ctx = null;
    this.masterGain = null;
    this.musicGain = null;
    this.sfxGain = null;
    this.musicVolume = 0.5;
    this.sfxVolume = 0.7;
    this._musicNode = null;
    this._initialized = false;
  }

  init() {
    if (this._initialized) return;
    try {
      this.ctx = new (window.AudioContext || window.webkitAudioContext)();
      this.masterGain = this.ctx.createGain();
      this.masterGain.connect(this.ctx.destination);
      this.musicGain = this.ctx.createGain();
      this.musicGain.gain.value = this.musicVolume;
      this.musicGain.connect(this.masterGain);
      this.sfxGain = this.ctx.createGain();
      this.sfxGain.gain.value = this.sfxVolume;
      this.sfxGain.connect(this.masterGain);
      this._initialized = true;
    } catch(e) {
      console.warn('Web Audio not available');
    }
  }

  resume() {
    if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume();
  }

  setMusicVolume(v) {
    this.musicVolume = v;
    if (this.musicGain) this.musicGain.gain.value = v;
  }

  setSfxVolume(v) {
    this.sfxVolume = v;
    if (this.sfxGain) this.sfxGain.gain.value = v;
  }

  // Procedurally generate magical ambient music
  playAmbientMusic() {
    if (!this._initialized || !this.ctx) return;
    this.stopMusic();

    const createDrone = (freq, detune = 0, vol = 0.08) => {
      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();
      const filter = this.ctx.createBiquadFilter();

      osc.type = 'sine';
      osc.frequency.value = freq;
      osc.detune.value = detune;

      filter.type = 'lowpass';
      filter.frequency.value = 800;
      filter.Q.value = 2;

      gain.gain.value = 0;
      gain.gain.linearRampToValueAtTime(vol, this.ctx.currentTime + 3);

      osc.connect(filter);
      filter.connect(gain);
      gain.connect(this.musicGain);
      osc.start();
      return { osc, gain };
    };

    // Deep mysterious drone chord
    const nodes = [
      createDrone(55, 0, 0.06),    // A1
      createDrone(82.5, 5, 0.04),  // E2
      createDrone(110, -3, 0.05),  // A2
      createDrone(165, 8, 0.03),   // E3
      createDrone(220, -5, 0.02),  // A3 (high)
    ];

    // Slow melodic arpeggiation
    const melodyNotes = [220, 246.9, 261.6, 293.7, 329.6, 293.7, 261.6, 246.9];
    let noteIdx = 0;
    const playMelodyNote = () => {
      if (!this._initialized) return;
      const freq = melodyNotes[noteIdx % melodyNotes.length];
      noteIdx++;

      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();
      const reverb = this.ctx.createConvolver();

      osc.type = 'triangle';
      osc.frequency.value = freq;

      gain.gain.setValueAtTime(0, this.ctx.currentTime);
      gain.gain.linearRampToValueAtTime(0.04, this.ctx.currentTime + 0.3);
      gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + 2.5);

      osc.connect(gain);
      gain.connect(this.musicGain);
      osc.start(this.ctx.currentTime);
      osc.stop(this.ctx.currentTime + 2.5);
    };

    this._melodyInterval = setInterval(playMelodyNote, 1800);
    playMelodyNote();

    this._musicNodes = nodes;
  }

  stopMusic() {
    if (this._musicNodes) {
      this._musicNodes.forEach(n => {
        try {
          n.gain.gain.linearRampToValueAtTime(0, this.ctx.currentTime + 1);
          n.osc.stop(this.ctx.currentTime + 1.1);
        } catch(e) {}
      });
      this._musicNodes = null;
    }
    if (this._melodyInterval) {
      clearInterval(this._melodyInterval);
      this._melodyInterval = null;
    }
  }

  // SFX generators
  playSpellFire() { this._playNoise('fire', 0.3, 0.4); }
  playSpellLightning() { this._playNoise('lightning', 0.2, 0.3); }
  playHeal() { this._playTone(440, 0.15, 'sine', 0.3); this._playTone(660, 0.12, 'sine', 0.4); }
  playShield() { this._playTone(220, 0.15, 'triangle', 0.5); }
  playExplosion() { this._playNoise('explosion', 0.6, 0.6); }
  playHit() { this._playNoise('hit', 0.25, 0.15); }
  playLevelUp() {
    [261.6, 329.6, 392, 523.2].forEach((f, i) => {
      setTimeout(() => this._playTone(f, 0.2, 'sine', 0.3), i * 150);
    });
  }
  playQuestComplete() {
    [392, 523.2, 659.2, 783.9].forEach((f, i) => {
      setTimeout(() => this._playTone(f, 0.15, 'triangle', 0.4), i * 120);
    });
  }
  playPickup() { this._playTone(880, 0.1, 'sine', 0.1); this._playTone(1100, 0.08, 'sine', 0.12); }
  playDialogueOpen() { this._playTone(330, 0.08, 'sine', 0.15); }
  playFootstep() { this._playNoise('thud', 0.08, 0.08); }
  playEnemyDeath() { this._playNoise('death', 0.3, 0.4); }

  _playTone(freq, vol, type, duration) {
    if (!this._initialized || !this.ctx) return;
    const osc = this.ctx.createOscillator();
    const gain = this.ctx.createGain();
    osc.type = type;
    osc.frequency.value = freq;
    gain.gain.setValueAtTime(vol * this.sfxVolume, this.ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
    osc.connect(gain);
    gain.connect(this.sfxGain);
    osc.start();
    osc.stop(this.ctx.currentTime + duration + 0.05);
  }

  _playNoise(type, vol, duration) {
    if (!this._initialized || !this.ctx) return;
    const bufferSize = this.ctx.sampleRate * duration;
    const buffer = this.ctx.createBuffer(1, bufferSize, this.ctx.sampleRate);
    const data = buffer.getChannelData(0);

    for (let i = 0; i < bufferSize; i++) {
      data[i] = (Math.random() * 2 - 1);
    }

    const source = this.ctx.createBufferSource();
    source.buffer = buffer;

    const filter = this.ctx.createBiquadFilter();
    const gain = this.ctx.createGain();

    switch(type) {
      case 'fire':
        filter.type = 'bandpass'; filter.frequency.value = 200; filter.Q.value = 0.8;
        gain.gain.setValueAtTime(vol * this.sfxVolume, this.ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
        break;
      case 'lightning':
        filter.type = 'highpass'; filter.frequency.value = 1000; filter.Q.value = 2;
        gain.gain.setValueAtTime(vol * this.sfxVolume, this.ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
        break;
      case 'explosion':
        filter.type = 'lowpass'; filter.frequency.value = 300; filter.Q.value = 1;
        gain.gain.setValueAtTime(vol * this.sfxVolume, this.ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
        break;
      case 'hit':
        filter.type = 'bandpass'; filter.frequency.value = 500; filter.Q.value = 3;
        gain.gain.setValueAtTime(vol * this.sfxVolume, this.ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
        break;
      case 'death':
        filter.type = 'lowpass'; filter.frequency.value = 400;
        gain.gain.setValueAtTime(vol * this.sfxVolume, this.ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
        break;
      case 'thud':
        filter.type = 'lowpass'; filter.frequency.value = 150; filter.Q.value = 1;
        gain.gain.setValueAtTime(vol * this.sfxVolume * 0.5, this.ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + duration);
        break;
    }

    source.connect(filter);
    filter.connect(gain);
    gain.connect(this.sfxGain);
    source.start();
  }
}
