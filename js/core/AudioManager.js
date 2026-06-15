/**
 * AudioManager.js
 * ----------------------------------------------------------------------------
 * All sound is synthesized at runtime with the Web Audio API — no audio files
 * to ship. It provides:
 *   - layered, evolving ambient "music" beds that shift per region (drones +
 *     a slow, modal arpeggio), and
 *   - short procedural SFX for spells, hits, pickups, level-ups and UI.
 *
 * Browsers block audio until a user gesture, so start() is called on the first
 * click/keypress from the Game.
 * ----------------------------------------------------------------------------
 */

export class AudioManager {
  constructor() {
    this.ctx = null;
    this.started = false;
    this.master = null;
    this.musicGain = null;
    this.sfxGain = null;
    this.musicVolume = 0.5;
    this.sfxVolume = 0.7;
    this.muted = false;
    this._musicNodes = [];
    this._arpTimer = null;
    this._scaleRoot = 220;
    // A gentle, mysterious scale (natural minor / aeolian) in semitone steps.
    this._scale = [0, 2, 3, 5, 7, 8, 10, 12];
    this._currentMood = null;
  }

  start() {
    if (this.started) return;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    this.ctx = new AC();
    this.master = this.ctx.createGain();
    this.master.gain.value = this.muted ? 0 : 1;
    this.master.connect(this.ctx.destination);

    this.musicGain = this.ctx.createGain();
    this.musicGain.gain.value = this.musicVolume;
    this.musicGain.connect(this.master);

    this.sfxGain = this.ctx.createGain();
    this.sfxGain.gain.value = this.sfxVolume;
    this.sfxGain.connect(this.master);

    this.started = true;
  }

  resume() { if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume(); }

  setMusicVolume(v) { this.musicVolume = v; if (this.musicGain) this.musicGain.gain.value = v; }
  setSfxVolume(v) { this.sfxVolume = v; if (this.sfxGain) this.sfxGain.gain.value = v; }
  setMuted(m) { this.muted = m; if (this.master) this.master.gain.value = m ? 0 : 1; }

  // --- Music ---------------------------------------------------------------
  // Each "mood" picks a root note and drone character. We build 2 detuned
  // drone oscillators + a slow randomized arpeggio that evokes the region.
  playMusic(mood = 'forest') {
    if (!this.started || this._currentMood === mood) return;
    this._currentMood = mood;
    this.stopMusic();

    // Dark, modal beds for the hidden world. Lower roots = more dread.
    const moods = {
      mist:     { root: 130.81, type: 'sine',     arp: 'triangle', rate: 2.8 }, // cursed forest
      temple:   { root: 146.83, type: 'sine',     arp: 'triangle', rate: 2.6 }, // drowned temple
      ruins:    { root: 123.47, type: 'sawtooth', arp: 'sine',     rate: 3.0 }, // haunted ruins
      caves:    { root: 97.999, type: 'sawtooth', arp: 'sine',     rate: 3.6 }, // shadowed caves
      citadel:  { root: 138.59, type: 'triangle', arp: 'square',   rate: 2.7 }, // pale citadel
      godreach: { root: 164.81, type: 'triangle', arp: 'triangle', rate: 2.2 }, // realm of gods
      menu:     { root: 130.81, type: 'sine',     arp: 'sine',     rate: 2.6 },
    };
    const m = moods[mood] || moods.mist;
    this._scaleRoot = m.root;

    // Two slightly detuned drones + a sub for warmth.
    const t = this.ctx.currentTime;
    [0, 7, -12].forEach((semi, idx) => {
      const osc = this.ctx.createOscillator();
      osc.type = m.type;
      osc.frequency.value = m.root * Math.pow(2, semi / 12);
      osc.detune.value = (idx - 1) * 6;
      const g = this.ctx.createGain();
      g.gain.value = 0;
      g.gain.linearRampToValueAtTime(idx === 2 ? 0.12 : 0.07, t + 4);
      // Slow tremolo via an LFO for a breathing pad.
      const lfo = this.ctx.createOscillator();
      lfo.frequency.value = 0.08 + idx * 0.03;
      const lfoGain = this.ctx.createGain();
      lfoGain.gain.value = 0.03;
      lfo.connect(lfoGain).connect(g.gain);
      osc.connect(g).connect(this.musicGain);
      osc.start(); lfo.start();
      this._musicNodes.push(osc, lfo, g);
    });

    // Slow arpeggio scheduler.
    const step = () => {
      if (this._currentMood !== mood) return;
      const note = this._scale[Math.floor(Math.random() * this._scale.length)];
      const oct = Math.random() < 0.4 ? 12 : 0;
      this._pluck(m.root * Math.pow(2, (note + oct) / 12), m.arp, 0.06);
      this._arpTimer = setTimeout(step, (m.rate + Math.random() * 1.5) * 1000);
    };
    this._arpTimer = setTimeout(step, 1200);
  }

  stopMusic() {
    if (this._arpTimer) { clearTimeout(this._arpTimer); this._arpTimer = null; }
    const t = this.ctx ? this.ctx.currentTime : 0;
    for (const n of this._musicNodes) {
      try {
        if (n.gain) n.gain.linearRampToValueAtTime(0, t + 1.5);
        if (n.stop) n.stop(t + 1.6);
      } catch (e) { /* node already stopped */ }
    }
    this._musicNodes = [];
  }

  _pluck(freq, type = 'triangle', vol = 0.08) {
    if (!this.started) return;
    const t = this.ctx.currentTime;
    const osc = this.ctx.createOscillator();
    osc.type = type;
    osc.frequency.value = freq;
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(vol, t + 0.02);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 1.6);
    const filt = this.ctx.createBiquadFilter();
    filt.type = 'lowpass';
    filt.frequency.value = 1800;
    osc.connect(filt).connect(g).connect(this.musicGain);
    osc.start(t); osc.stop(t + 1.7);
  }

  // --- SFX -----------------------------------------------------------------
  // Generic one-shot tone with an envelope + optional pitch sweep.
  _tone({ freq = 440, freq2 = null, type = 'sine', dur = 0.2, vol = 0.4, dest = null }) {
    if (!this.started) return;
    const t = this.ctx.currentTime;
    const osc = this.ctx.createOscillator();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, t);
    if (freq2) osc.frequency.exponentialRampToValueAtTime(Math.max(20, freq2), t + dur);
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + 0.01);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    osc.connect(g).connect(dest || this.sfxGain);
    osc.start(t); osc.stop(t + dur + 0.02);
    return { osc, g };
  }

  // A burst of filtered noise — good for fire, impacts, wind.
  _noise({ dur = 0.3, vol = 0.4, freq = 1000, q = 1, type = 'bandpass' }) {
    if (!this.started) return;
    const t = this.ctx.currentTime;
    const buf = this.ctx.createBuffer(1, this.ctx.sampleRate * dur, this.ctx.sampleRate);
    const data = buf.getChannelData(0);
    for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
    const src = this.ctx.createBufferSource();
    src.buffer = buf;
    const filt = this.ctx.createBiquadFilter();
    filt.type = type; filt.frequency.value = freq; filt.Q.value = q;
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(vol, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    src.connect(filt).connect(g).connect(this.sfxGain);
    src.start(t); src.stop(t + dur);
  }

  // Named SFX used by the game. Each is a little sound-design recipe.
  sfx(name) {
    switch (name) {
      case 'fire':
        this._tone({ freq: 320, freq2: 120, type: 'sawtooth', dur: 0.35, vol: 0.3 });
        this._noise({ dur: 0.4, vol: 0.25, freq: 800, q: 0.7 });
        break;
      case 'nature':
        this._tone({ freq: 520, freq2: 760, type: 'triangle', dur: 0.3, vol: 0.25 });
        break;
      case 'lightning':
        this._noise({ dur: 0.25, vol: 0.4, freq: 3000, q: 0.5, type: 'highpass' });
        this._tone({ freq: 1200, freq2: 200, type: 'square', dur: 0.2, vol: 0.2 });
        break;
      case 'heal':
        this._tone({ freq: 440, freq2: 880, type: 'sine', dur: 0.5, vol: 0.3 });
        this._tone({ freq: 660, freq2: 990, type: 'sine', dur: 0.6, vol: 0.2 });
        break;
      case 'shield':
        this._tone({ freq: 200, freq2: 500, type: 'sine', dur: 0.5, vol: 0.3 });
        break;
      case 'ultimate':
        this._tone({ freq: 80, freq2: 400, type: 'sawtooth', dur: 0.8, vol: 0.4 });
        this._noise({ dur: 0.9, vol: 0.3, freq: 600, q: 0.4 });
        break;
      case 'hit':
        this._noise({ dur: 0.15, vol: 0.35, freq: 400, q: 1 });
        this._tone({ freq: 180, freq2: 80, type: 'square', dur: 0.12, vol: 0.2 });
        break;
      case 'hurt':
        this._tone({ freq: 260, freq2: 110, type: 'sawtooth', dur: 0.25, vol: 0.35 });
        break;
      case 'pickup':
        this._tone({ freq: 660, freq2: 990, type: 'triangle', dur: 0.15, vol: 0.3 });
        break;
      case 'quest':
        this._tone({ freq: 523, type: 'sine', dur: 0.18, vol: 0.3 });
        setTimeout(() => this._tone({ freq: 784, type: 'sine', dur: 0.3, vol: 0.3 }), 140);
        break;
      case 'levelup':
        [523, 659, 784, 1047].forEach((f, i) =>
          setTimeout(() => this._tone({ freq: f, type: 'triangle', dur: 0.25, vol: 0.3 }), i * 110));
        break;
      case 'ui':
        this._tone({ freq: 600, type: 'sine', dur: 0.06, vol: 0.18 });
        break;
      case 'death':
        this._tone({ freq: 300, freq2: 60, type: 'sawtooth', dur: 1.2, vol: 0.4 });
        break;
      default: break;
    }
  }
}
