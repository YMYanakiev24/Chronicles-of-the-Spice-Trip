// Input manager — keyboard, mouse, pointer lock
export class Input {
  constructor() {
    this.keys = {};
    this.mouse = { x: 0, y: 0, dx: 0, dy: 0, buttons: {} };
    this.locked = false;
    this._listeners = {};
    this._bind();
  }

  _bind() {
    document.addEventListener('keydown', e => {
      const key = e.key.toUpperCase();
      if (!this.keys[key]) {
        this.keys[key] = true;
        this._emit('keydown', key);
      }
    });

    document.addEventListener('keyup', e => {
      const key = e.key.toUpperCase();
      this.keys[key] = false;
      this._emit('keyup', key);
    });

    document.addEventListener('mousedown', e => {
      this.mouse.buttons[e.button] = true;
      this._emit('mousedown', e.button);
    });

    document.addEventListener('mouseup', e => {
      this.mouse.buttons[e.button] = false;
      this._emit('mouseup', e.button);
    });

    document.addEventListener('mousemove', e => {
      if (this.locked) {
        this.mouse.dx = e.movementX || 0;
        this.mouse.dy = e.movementY || 0;
      } else {
        this.mouse.x = e.clientX;
        this.mouse.y = e.clientY;
        this.mouse.dx = 0;
        this.mouse.dy = 0;
      }
    });

    document.addEventListener('pointerlockchange', () => {
      this.locked = document.pointerLockElement === document.getElementById('gameCanvas');
      this._emit('lockchange', this.locked);
    });
  }

  requestPointerLock() {
    document.getElementById('gameCanvas').requestPointerLock();
  }

  exitPointerLock() {
    document.exitPointerLock();
  }

  isDown(key) {
    return !!this.keys[key.toUpperCase()];
  }

  isMouseDown(btn = 0) {
    return !!this.mouse.buttons[btn];
  }

  consumeDelta() {
    const dx = this.mouse.dx;
    const dy = this.mouse.dy;
    this.mouse.dx = 0;
    this.mouse.dy = 0;
    return { dx, dy };
  }

  on(event, fn) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(fn);
    return () => this.off(event, fn);
  }

  off(event, fn) {
    if (this._listeners[event]) {
      this._listeners[event] = this._listeners[event].filter(f => f !== fn);
    }
  }

  _emit(event, data) {
    if (this._listeners[event]) {
      this._listeners[event].forEach(fn => fn(data));
    }
  }

  dispose() {
    this._listeners = {};
  }
}
