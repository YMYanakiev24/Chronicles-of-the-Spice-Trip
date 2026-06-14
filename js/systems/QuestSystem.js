import { QUESTS } from '../data/quests.js';

// Manages quest state, progress, and completion
export class QuestSystem {
  constructor(ui, audio) {
    this.ui = ui;
    this.audio = audio;

    this.quests = {};
    this.activeQuestId = null;
    this._callbacks = {};

    // Deep-clone quest data
    Object.entries(QUESTS).forEach(([id, q]) => {
      this.quests[id] = {
        ...q,
        status: 'available', // available | active | completed
        objectives: q.objectives.map(o => ({ ...o }))
      };
    });
  }

  startQuest(questId) {
    const q = this.quests[questId];
    if (!q || q.status !== 'available') return false;

    q.status = 'active';
    if (!this.activeQuestId) this.activeQuestId = questId;

    this.ui.showToast(`Quest Started: ${q.title}`, 'quest');
    this.ui.updateQuestTracker(this);
    this.ui.refreshQuestLog(this);
    this._emit('questStarted', questId);

    return true;
  }

  // Advance a 'kill' or 'destroy' objective
  reportKill(enemyTypeId) {
    let changed = false;
    Object.values(this.quests).forEach(q => {
      if (q.status !== 'active') return;
      q.objectives.forEach(obj => {
        if (obj.done) return;
        if ((obj.type === 'kill' || obj.type === 'destroy' || obj.type === 'survive') && obj.target === enemyTypeId) {
          obj.progress = Math.min(obj.count, obj.progress + 1);
          if (obj.progress >= obj.count) {
            obj.done = true;
            this.ui.showToast(`✓ ${obj.text}`, 'quest');
          }
          changed = true;
        }
      });
      if (changed) this._checkCompletion(q);
    });
    if (changed) {
      this.ui.updateQuestTracker(this);
      this.ui.refreshQuestLog(this);
    }
  }

  // Report reaching a location
  reportLocation(locationId) {
    let changed = false;
    Object.values(this.quests).forEach(q => {
      if (q.status !== 'active') return;
      q.objectives.forEach(obj => {
        if (obj.done || obj.type !== 'reach') return;
        if (obj.target === locationId) {
          obj.progress = 1;
          obj.done = true;
          this.ui.showToast(`✓ ${obj.text}`, 'quest');
          changed = true;
        }
      });
      if (changed) this._checkCompletion(q);
    });
    if (changed) {
      this.ui.updateQuestTracker(this);
      this.ui.refreshQuestLog(this);
    }
  }

  // Report talking to an NPC
  reportTalk(npcId) {
    let changed = false;
    Object.values(this.quests).forEach(q => {
      if (q.status !== 'active') return;
      q.objectives.forEach(obj => {
        if (obj.done || obj.type !== 'talk') return;
        if (obj.target === npcId) {
          obj.progress = 1;
          obj.done = true;
          this.ui.showToast(`✓ ${obj.text}`, 'quest');
          changed = true;
        }
      });
      if (changed) this._checkCompletion(q);
    });
    if (changed) {
      this.ui.updateQuestTracker(this);
      this.ui.refreshQuestLog(this);
    }
  }

  // Report collecting an item
  reportCollect(itemId) {
    let changed = false;
    Object.values(this.quests).forEach(q => {
      if (q.status !== 'active') return;
      q.objectives.forEach(obj => {
        if (obj.done || obj.type !== 'collect') return;
        if (obj.target === itemId) {
          obj.progress = 1;
          obj.done = true;
          this.ui.showToast(`✓ ${obj.text}`, 'quest');
          changed = true;
        }
      });
      if (changed) this._checkCompletion(q);
    });
    if (changed) {
      this.ui.updateQuestTracker(this);
      this.ui.refreshQuestLog(this);
    }
  }

  _checkCompletion(q) {
    if (q.status !== 'active') return;
    if (q.objectives.every(o => o.done)) {
      q.status = 'completed';
      this.audio?.playQuestComplete();
      this.ui.showToast(`Quest Complete: ${q.title}!`, 'quest');
      this._emit('questCompleted', q.id);

      if (this.activeQuestId === q.id) {
        // Switch to next active quest
        const next = Object.values(this.quests).find(x => x.status === 'active');
        this.activeQuestId = next?.id || null;
      }

      this.ui.updateQuestTracker(this);
      this.ui.refreshQuestLog(this);
    }
  }

  isActive(questId) {
    return this.quests[questId]?.status === 'active';
  }

  isComplete(questId) {
    return this.quests[questId]?.status === 'completed';
  }

  getActiveQuest() {
    return this.activeQuestId ? this.quests[this.activeQuestId] : null;
  }

  setActive(questId) {
    if (this.quests[questId]?.status === 'active') this.activeQuestId = questId;
    this.ui.updateQuestTracker(this);
  }

  on(event, fn) {
    if (!this._callbacks[event]) this._callbacks[event] = [];
    this._callbacks[event].push(fn);
  }

  _emit(event, ...args) {
    (this._callbacks[event] || []).forEach(fn => fn(...args));
  }

  serialize() {
    return {
      quests: this.quests,
      activeQuestId: this.activeQuestId
    };
  }

  load(data) {
    if (!data) return;
    Object.entries(data.quests || {}).forEach(([id, q]) => {
      if (this.quests[id]) Object.assign(this.quests[id], q);
    });
    this.activeQuestId = data.activeQuestId;
  }
}
