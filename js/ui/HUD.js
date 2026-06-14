// HUD — health/mana/xp bars, quest tracker, spell cooldowns, notifications
export class HUD {
  constructor() {
    this.elements = {
      hud: document.getElementById('hud'),
      healthBar: document.getElementById('healthBar'),
      healthText: document.getElementById('healthText'),
      manaBar: document.getElementById('manaBar'),
      manaText: document.getElementById('manaText'),
      xpBar: document.getElementById('xpBar'),
      xpText: document.getElementById('xpText'),
      playerLevel: document.getElementById('playerLevel'),
      questTitle: document.getElementById('activeQuestTitle'),
      questObj: document.getElementById('activeQuestObjective'),
      questProgress: document.getElementById('activeQuestProgress'),
      interactPrompt: document.getElementById('interactPrompt'),
      interactText: document.getElementById('interactText'),
      toast: document.getElementById('toastContainer'),
      levelUpNotif: document.getElementById('levelUpNotif'),
      levelUpNumber: document.getElementById('levelUpNumber'),
      damageVignette: document.getElementById('damageVignette'),
      healVignette: document.getElementById('healVignette'),
    };

    this._toastQueue = [];
  }

  show() { this.elements.hud?.classList.remove('hidden'); }
  hide() { this.elements.hud?.classList.add('hidden'); }

  updateStats(player) {
    const hp = Math.max(0, Math.ceil(player.health));
    const mp = Math.max(0, Math.ceil(player.mana));
    const hpPct = (player.health / player.maxHealth * 100).toFixed(1);
    const mpPct = (player.mana / player.maxMana * 100).toFixed(1);
    const xpPct = (player.xp / player.xpToNextLevel * 100).toFixed(1);

    if (this.elements.healthBar) this.elements.healthBar.style.width = `${hpPct}%`;
    if (this.elements.healthText) this.elements.healthText.textContent = `${hp} / ${player.maxHealth}`;
    if (this.elements.manaBar) this.elements.manaBar.style.width = `${mpPct}%`;
    if (this.elements.manaText) this.elements.manaText.textContent = `${mp} / ${player.maxMana}`;
    if (this.elements.xpBar) this.elements.xpBar.style.width = `${xpPct}%`;
    if (this.elements.xpText) this.elements.xpText.textContent = `${player.xp} / ${player.xpToNextLevel}`;
    if (this.elements.playerLevel) this.elements.playerLevel.textContent = player.level;
  }

  updateSpellCooldowns(spellSystem) {
    ['Q', 'E', 'R', 'T', 'G'].forEach(key => {
      const el = document.getElementById(`cd-${key}`);
      const slot = document.getElementById(`spell-${key}`);
      if (!el || !slot) return;
      const ratio = spellSystem.getCooldownRatio(key);
      if (ratio > 0) {
        slot.classList.add('on-cooldown');
        el.style.transform = `scaleY(${ratio})`;
      } else {
        slot.classList.remove('on-cooldown');
        el.style.transform = 'scaleY(0)';
      }
    });
  }

  updateQuestTracker(questSystem) {
    const q = questSystem.getActiveQuest();
    if (!q) {
      if (this.elements.questTitle) this.elements.questTitle.textContent = '—';
      if (this.elements.questObj) this.elements.questObj.textContent = '';
      if (this.elements.questProgress) this.elements.questProgress.textContent = '';
      return;
    }

    if (this.elements.questTitle) this.elements.questTitle.textContent = q.title;

    const nextObj = q.objectives.find(o => !o.done);
    if (nextObj) {
      if (this.elements.questObj) this.elements.questObj.textContent = nextObj.text;
      if (this.elements.questProgress) {
        const pct = Math.round((nextObj.progress / nextObj.count) * 100);
        this.elements.questProgress.textContent = nextObj.count > 1 ? `${nextObj.progress} / ${nextObj.count}` : '';
      }
    } else {
      if (this.elements.questObj) this.elements.questObj.textContent = '✓ All objectives complete!';
    }
  }

  showInteractPrompt(text) {
    const el = this.elements.interactPrompt;
    const t = this.elements.interactText;
    if (!el) return;
    el.classList.remove('hidden');
    if (t) t.textContent = text || 'Interact';
  }

  hideInteractPrompt() {
    this.elements.interactPrompt?.classList.add('hidden');
  }

  showDamageFlash() {
    const el = this.elements.damageVignette;
    if (!el) return;
    el.classList.remove('hidden');
    el.style.animation = 'none';
    void el.offsetHeight;
    el.style.animation = 'vignette-flash 0.4s ease-out';
    setTimeout(() => el.classList.add('hidden'), 420);
  }

  showHealFlash() {
    const el = this.elements.healVignette;
    if (!el) return;
    el.classList.remove('hidden');
    el.style.animation = 'none';
    void el.offsetHeight;
    el.style.animation = 'vignette-flash 0.6s ease-out';
    setTimeout(() => el.classList.add('hidden'), 640);
  }

  showLevelUp(level) {
    const el = this.elements.levelUpNotif;
    const num = this.elements.levelUpNumber;
    if (!el) return;
    if (num) num.textContent = `Level ${level}`;
    el.classList.remove('hidden');
    el.style.animation = 'none';
    void el.offsetHeight;
    el.style.animation = 'level-up-appear 3s ease-out forwards';
    setTimeout(() => el.classList.add('hidden'), 3100);
  }

  showToast(message, type = '') {
    const container = this.elements.toast;
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    container.appendChild(toast);

    setTimeout(() => {
      toast.remove();
    }, 3700);
  }

  refreshQuestLog(questSystem) {
    const el = document.getElementById('questLogContent');
    if (!el) return;

    const tab = document.querySelector('.tab-btn.active')?.dataset.tab || 'active';
    el.innerHTML = '';

    Object.values(questSystem.quests)
      .filter(q => tab === 'active' ? q.status === 'active' : q.status === 'completed')
      .forEach(q => {
        const entry = document.createElement('div');
        entry.className = 'quest-entry';

        const title = document.createElement('div');
        title.className = 'quest-entry-title';
        title.textContent = q.title;

        const desc = document.createElement('div');
        desc.className = 'quest-entry-desc';
        desc.textContent = q.description;

        entry.appendChild(title);
        entry.appendChild(desc);

        q.objectives.forEach(obj => {
          const objEl = document.createElement('div');
          objEl.className = `quest-objective ${obj.done ? 'done' : ''}`;
          objEl.textContent = `${obj.text}${obj.count > 1 ? ` (${obj.progress}/${obj.count})` : ''}`;
          entry.appendChild(objEl);
        });

        if (q.rewards) {
          const reward = document.createElement('div');
          reward.style.cssText = 'font-size:0.7rem;color:#c9a227;margin-top:8px;';
          reward.textContent = `Reward: ${q.rewards.xp} XP${q.rewards.gold ? ` • ${q.rewards.gold} Gold` : ''}`;
          entry.appendChild(reward);
        }

        el.appendChild(entry);
      });

    if (!el.children.length) {
      el.innerHTML = `<p style="color:#9a8860;font-style:italic;font-family:'IM Fell English',serif;">No ${tab} quests.</p>`;
    }
  }

  refreshInventory(inventory) {
    const grid = document.getElementById('inventoryGrid');
    if (!grid) return;
    grid.innerHTML = '';

    for (let i = 0; i < inventory.maxSlots; i++) {
      const item = inventory.items[i];
      const slot = document.createElement('div');
      slot.className = 'inv-slot';

      if (item) {
        slot.textContent = item.icon || '?';
        if (item.qty > 1) {
          const qty = document.createElement('span');
          qty.className = 'inv-slot-qty';
          qty.textContent = item.qty;
          slot.appendChild(qty);
        }
        slot.title = item.name;
        slot.addEventListener('click', () => {
          document.querySelectorAll('.inv-slot').forEach(s => s.classList.remove('selected'));
          slot.classList.add('selected');
          this._showItemDetail(item);
        });
      }
      grid.appendChild(slot);
    }
  }

  _showItemDetail(item) {
    const details = document.getElementById('inventoryDetails');
    if (!details) return;
    details.innerHTML = `
      <div class="inv-item-name">${item.icon || ''} ${item.name}</div>
      <div class="inv-item-desc">${item.desc || ''}</div>
      ${Object.keys(item.bonus || {}).length ? `<div class="inv-item-stat">Bonus: ${JSON.stringify(item.bonus)}</div>` : ''}
    `;
  }
}
