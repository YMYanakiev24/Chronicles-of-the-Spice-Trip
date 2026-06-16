/**
 * Input.js
 * ----------------------------------------------------------------------------
 * Keyboard + mouse for the 2D game. Exposes a polled interface (isDown /
 * wasPressed), the absolute cursor position (mouseX/mouseY in CSS pixels, used
 * for aiming), mouse buttons and a one-shot key callback for UI hotkeys.
 * ----------------------------------------------------------------------------
 */

export class Input {
  constructor(domElement) {
    this.dom = domElement;
    this.keys = new Set();          // currently held
    this.pressed = new Set();       // pressed this frame (cleared in lateUpdate)
    this.mouseButtons = new Set();
    this.mousePressed = new Set();  // mouse buttons pressed this frame
    this.mouseX = window.innerWidth / 2;
    this.mouseY = window.innerHeight / 2;
    this.wheel = 0;
    this.onKey = null;
    this._bind();
  }

  _bind() {
    window.addEventListener('keydown', (e) => {
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
      const k = e.code;
      if (!this.keys.has(k)) this.pressed.add(k);
      this.keys.add(k);
      if (this.onKey) this.onKey(k, e);
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab'].includes(k)) e.preventDefault();
    });
    window.addEventListener('keyup', (e) => this.keys.delete(e.code));

    this.dom.addEventListener('mousedown', (e) => { this.mouseButtons.add(e.button); this.mousePressed.add(e.button); });
    window.addEventListener('mouseup', (e) => this.mouseButtons.delete(e.button));
    window.addEventListener('contextmenu', (e) => e.preventDefault()); // allow right-click as a game button
    window.addEventListener('mousemove', (e) => { this.mouseX = e.clientX; this.mouseY = e.clientY; });
    window.addEventListener('wheel', (e) => { this.wheel += Math.sign(e.deltaY); }, { passive: true });
  }

  isDown(code) { return this.keys.has(code); }
  wasPressed(code) { return this.pressed.has(code); }
  mouseDown(btn = 0) { return this.mouseButtons.has(btn); }
  mouseWasPressed(btn = 0) { return this.mousePressed.has(btn); }
  consumeWheel() { const w = this.wheel; this.wheel = 0; return w; }

  // WASD + arrows → axis. y is +1 for "up" (caller flips for screen space).
  moveAxis() {
    let x = 0, y = 0;
    if (this.isDown('KeyW') || this.isDown('ArrowUp')) y += 1;
    if (this.isDown('KeyS') || this.isDown('ArrowDown')) y -= 1;
    if (this.isDown('KeyD') || this.isDown('ArrowRight')) x += 1;
    if (this.isDown('KeyA') || this.isDown('ArrowLeft')) x -= 1;
    return { x, y };
  }

  lateUpdate() { this.pressed.clear(); this.mousePressed.clear(); }
}
