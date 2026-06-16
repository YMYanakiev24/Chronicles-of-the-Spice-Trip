/**
 * QuestSystem.js
 * ----------------------------------------------------------------------------
 * Tracks quest state and objective progress. Quests move through:
 *   locked → available → active → readyToTurnIn → completed
 * Gameplay events (kill / collect / cast / talk / reach) are funneled in via
 * the on* hooks, which advance objective counters and fire rewards.
 * ----------------------------------------------------------------------------
 */

import { QUESTS, QUEST_ORDER, NPCS } from '../data/GameData.js';

export class QuestSystem {
  constructor(game) {
    this.game = game;
    // state per quest id
    this.state = {};        // 'available' | 'active' | 'ready' | 'completed'
    this.progress = {};     // questId -> [counts per objective]
    for (const id of QUEST_ORDER) {
      this.state[id] = 'available';
      this.progress[id] = QUESTS[id].objectives.map(() => 0);
    }
    this.tracked = QUEST_ORDER[0];   // quest shown in the HUD tracker
  }

  // --- Lifecycle ------------------------------------------------------------
  accept(id) {
    if (this.state[id] !== 'available') return;
    this.state[id] = 'active';
    this.tracked = id;
    this.game.audio.sfx('quest');
    this.game.ui.toast(`Quest accepted: ${QUESTS[id].title}`, 'gold');
    this.game.ui.refreshQuestTracker();
    this._checkReady(id);
  }

  // Advance an objective of a given type if its predicate matches.
  _advance(matchFn) {
    let changed = false;
    for (const id of QUEST_ORDER) {
      if (this.state[id] !== 'active') continue;
      const q = QUESTS[id];
      q.objectives.forEach((obj, i) => {
        if (this.state[id] !== 'active') return;
        if (this.progress[id][i] >= (obj.count || 1)) return;
        if (matchFn(obj)) {
          this.progress[id][i] = Math.min(obj.count || 1, this.progress[id][i] + 1);
          changed = true;
        }
      });
      this._checkReady(id);
    }
    if (changed) {
      this.game.audio.sfx('pickup');
      this.game.ui.refreshQuestTracker();
    }
  }

  // A quest is "ready" when all non-talk objectives are done. Talk objectives
  // that target the giver are completed by turning the quest in.
  _checkReady(id) {
    const q = QUESTS[id];
    const done = q.objectives.every((obj, i) => {
      if (obj.type === 'talk') return true; // resolved at turn-in
      return this.progress[id][i] >= (obj.count || 1);
    });
    if (done && this.state[id] === 'active') {
      this.state[id] = 'ready';
      this.game.audio.sfx('quest');
      this.game.ui.toast(`Objective complete — return to ${NPCS[q.giver].name.split(',')[0]}.`, 'gold');
      this.game.ui.refreshQuestTracker();
    }
  }

  turnIn(id) {
    if (this.state[id] !== 'ready') return false;
    const q = QUESTS[id];
    this.state[id] = 'completed';
    const p = this.game.player;
    const r = q.rewards;
    if (r.xp) p.addXP(r.xp);
    if (r.gold) p.addGold(r.gold);
    if (r.spell) p.learnSpell(r.spell);
    if (r.items) for (const it of r.items) p.addItem(it);
    this.game.audio.sfx('levelup');
    this.game.ui.bigBanner('QUEST COMPLETE');
    this.game.ui.toast(`Completed: ${q.title}`, 'gold');

    // Chain to the next quest if defined.
    if (q.next && this.state[q.next] === 'available') this.tracked = q.next;
    else this._autoTrack();

    this.game.ui.refreshQuestTracker();
    return true;
  }

  _autoTrack() {
    // Track the first active/ready/available quest still open.
    for (const id of QUEST_ORDER) {
      if (this.state[id] === 'active' || this.state[id] === 'ready') { this.tracked = id; return; }
    }
    for (const id of QUEST_ORDER) {
      if (this.state[id] === 'available') { this.tracked = id; return; }
    }
    this.tracked = null;
  }

  // --- Event hooks ----------------------------------------------------------
  onKill(creatureId) { this._advance((o) => o.type === 'kill' && o.creature === creatureId); }
  onCast(spellId) { this._advance((o) => o.type === 'cast' && o.spell === spellId); }
  onCollect(itemId) { this._advance((o) => o.type === 'collect' && o.item === itemId); }
  onReach(locId) { this._advance((o) => o.type === 'reach' && o.loc === locId); }

  // Talking to an NPC can both accept their quest and turn in a finished one.
  onTalk(npcId) {
    this._advance((o) => o.type === 'talk' && o.npc === npcId);
    // Turn in any "ready" quest whose giver is this NPC.
    for (const id of QUEST_ORDER) {
      if (this.state[id] === 'ready' && QUESTS[id].giver === npcId) {
        // mark the talk objective(s) for this giver done, then complete
        QUESTS[id].objectives.forEach((obj, i) => {
          if (obj.type === 'talk' && obj.npc === npcId) this.progress[id][i] = 1;
        });
        this.turnIn(id);
      }
    }
  }

  // --- Queries used by NPCs/UI ----------------------------------------------
  availableFrom(npcId) {
    return QUEST_ORDER.filter((id) => QUESTS[id].giver === npcId && this.state[id] === 'available');
  }
  readyFor(npcId) {
    return QUEST_ORDER.filter((id) => QUESTS[id].giver === npcId && this.state[id] === 'ready');
  }

  // The floating marker glyph an NPC should show.
  markerFor(npcId) {
    if (this.readyFor(npcId).length) return '✔';
    if (this.availableFrom(npcId).length) return '!';
    // active quest from this giver still in progress?
    const active = QUEST_ORDER.some((id) => QUESTS[id].giver === npcId && this.state[id] === 'active');
    return active ? '?' : '';
  }

  objectiveText(id) {
    const q = QUESTS[id];
    return q.objectives.map((obj, i) => {
      const have = this.progress[id][i];
      const need = obj.count || 1;
      const done = obj.type === 'talk' ? (this.state[id] === 'completed') : have >= need;
      const counter = (obj.count && obj.count > 1) ? ` (${have}/${need})` : '';
      return { text: obj.text + counter, done };
    });
  }

  trackedQuest() {
    if (!this.tracked) return null;
    return { id: this.tracked, def: QUESTS[this.tracked], state: this.state[this.tracked], objectives: this.objectiveText(this.tracked) };
  }

  // --- Save/load ------------------------------------------------------------
  serialize() { return { state: this.state, progress: this.progress, tracked: this.tracked }; }
  deserialize(d) {
    if (!d) return;
    this.state = d.state || this.state;
    this.progress = d.progress || this.progress;
    this.tracked = d.tracked || this.tracked;
  }
}
