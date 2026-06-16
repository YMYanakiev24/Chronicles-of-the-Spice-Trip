/**
 * main.js — entry point.
 * ----------------------------------------------------------------------------
 * Boots the game once the DOM is ready. Kept intentionally tiny: all real work
 * lives in the Game orchestrator. Surfaces a friendly message if the 2D canvas
 * API is unavailable so the page never just shows a blank screen.
 * ----------------------------------------------------------------------------
 */

import { Game } from './core/Game.js';

// The 2D pixel renderer only needs a Canvas 2D context (no WebGL).
function canvas2dAvailable() {
  try {
    return !!document.createElement('canvas').getContext('2d');
  } catch (e) { return false; }
}

window.addEventListener('DOMContentLoaded', () => {
  if (!canvas2dAvailable()) {
    document.body.innerHTML =
      '<div style="position:fixed;inset:0;display:grid;place-items:center;' +
      'background:#0b1a18;color:#e8dcc0;font-family:serif;text-align:center;padding:24px;">' +
      'Your browser does not support the HTML5 Canvas, which this game requires.<br>' +
      'Try a recent version of Chrome, Edge or Firefox.</div>';
    return;
  }

  try {
    const game = new Game();
    // Expose for debugging in the console.
    window.__MYTHIC__ = game;
    game.boot();
  } catch (err) {
    console.error('Failed to start The Mythic Chronicles:', err);
    const tip = document.getElementById('loading-tip');
    if (tip) tip.textContent = 'A spell misfired while loading. See the console for details.';
  }
});
