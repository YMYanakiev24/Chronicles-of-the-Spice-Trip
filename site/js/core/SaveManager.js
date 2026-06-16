/**
 * SaveManager.js
 * ----------------------------------------------------------------------------
 * Persists the player's chronicle to localStorage. A save is a small JSON
 * snapshot of progression (stats, learned spells, inventory, quest state,
 * unlocked regions, settings). The Game assembles/restores it.
 * ----------------------------------------------------------------------------
 */

const SAVE_KEY = 'mythic-chronicles-save-v1';
const SETTINGS_KEY = 'mythic-chronicles-settings-v1';
const HUB_KEY = 'mythic-chronicles-hub-v1';

export class SaveManager {
  hasSave() {
    try { return !!localStorage.getItem(SAVE_KEY); } catch (e) { return false; }
  }

  save(data) {
    try {
      const payload = { ...data, savedAt: Date.now(), version: 1 };
      localStorage.setItem(SAVE_KEY, JSON.stringify(payload));
      return true;
    } catch (e) {
      console.warn('Save failed:', e);
      return false;
    }
  }

  load() {
    try {
      const raw = localStorage.getItem(SAVE_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch (e) {
      console.warn('Load failed:', e);
      return null;
    }
  }

  clear() {
    try { localStorage.removeItem(SAVE_KEY); } catch (e) { /* ignore */ }
  }

  // Settings are stored separately so they survive a deleted save.
  saveSettings(settings) {
    try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch (e) { /* ignore */ }
  }

  loadSettings() {
    try {
      const raw = localStorage.getItem(SETTINGS_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch (e) { return null; }
  }

  // The Citadel hub (Marin's shop) keeps its own persistent wallet + a stash of
  // purchases that are granted to the player when a game begins.
  loadHub() {
    try {
      const raw = localStorage.getItem(HUB_KEY);
      return raw ? JSON.parse(raw) : { gold: 1450, stash: {} };
    } catch (e) { return { gold: 1450, stash: {} }; }
  }

  saveHub(hub) {
    try { localStorage.setItem(HUB_KEY, JSON.stringify(hub)); } catch (e) { /* ignore */ }
  }
}
