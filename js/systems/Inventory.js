// Item inventory system
export class Inventory {
  constructor(ui, audio) {
    this.ui = ui;
    this.audio = audio;
    this.items = [];
    this.maxSlots = 24;
    this._bonuses = {};
  }

  addItem(item) {
    if (this.items.length >= this.maxSlots) {
      this.ui.showToast('Inventory is full!', 'combat');
      return false;
    }

    // Stack if same id
    const existing = this.items.find(i => i.id === item.id);
    if (existing && existing.stackable) {
      existing.qty = (existing.qty || 1) + 1;
    } else {
      this.items.push({ ...item, qty: 1 });
    }

    this.audio?.playPickup();
    this.ui.showToast(`Obtained: ${item.name}`, 'item');
    this.ui.refreshInventory(this);

    // Apply passive bonus immediately
    if (item.bonus) {
      Object.entries(item.bonus).forEach(([k, v]) => {
        this._bonuses[k] = (this._bonuses[k] || 0) + v;
      });
    }

    return true;
  }

  removeItem(itemId) {
    const idx = this.items.findIndex(i => i.id === itemId);
    if (idx === -1) return false;
    const item = this.items[idx];
    if (item.bonus) {
      Object.entries(item.bonus).forEach(([k, v]) => {
        this._bonuses[k] = (this._bonuses[k] || 0) - v;
      });
    }
    if (item.qty > 1) {
      item.qty--;
    } else {
      this.items.splice(idx, 1);
    }
    this.ui.refreshInventory(this);
    return true;
  }

  useItem(itemId, player) {
    const item = this.items.find(i => i.id === itemId);
    if (!item || !item.effect) return false;

    const { effect } = item;
    let used = false;

    if (effect.type === 'heal') {
      if (player.health < player.maxHealth) {
        player.heal(effect.value);
        this.ui.showToast(`+${effect.value} Vitality`, 'item');
        used = true;
      }
    } else if (effect.type === 'mana') {
      if (player.mana < player.maxMana) {
        player.mana = Math.min(player.maxMana, player.mana + effect.value);
        this.ui.showToast(`+${effect.value} Arcane`, 'item');
        used = true;
      }
    } else if (effect.type === 'buff') {
      player.addBuff(effect.stat, effect.value, effect.duration);
      this.ui.showToast(`${item.name} activated!`, 'item');
      used = true;
    }

    if (used) {
      this.removeItem(itemId);
      this.audio?.playHeal();
    }
    return used;
  }

  hasItem(itemId) {
    return this.items.some(i => i.id === itemId);
  }

  getBonus(stat) {
    return this._bonuses[stat] || 0;
  }

  serialize() {
    return { items: this.items, bonuses: this._bonuses };
  }

  load(data) {
    if (!data) return;
    this.items = data.items || [];
    this._bonuses = data.bonuses || {};
  }
}
