/**
 * CombatSystem.js
 * ----------------------------------------------------------------------------
 * The neutral arbiter of damage and death. MagicSystem and enemies route all
 * damage through here so XP rewards, gold/loot drops, quest progress and death
 * cleanup live in exactly one place.
 * ----------------------------------------------------------------------------
 */

import { CREATURES, ITEMS } from '../data/GameData.js';
import { chance } from '../core/Utils.js';

export class CombatSystem {
  constructor(game) {
    this.game = game;
  }

  damageEnemy(enemy, amount, element) {
    if (!enemy || enemy.dead) return;
    enemy.takeDamage(amount, element);
  }

  damagePlayer(amount, source) {
    this.game.player.takeDamage(amount, source);
  }

  onEnemyKilled(enemy) {
    const def = enemy.def;
    const player = this.game.player;

    // Reward XP + gold.
    player.addXP(def.xp);
    const gold = Math.round(def.xp * (0.4 + Math.random() * 0.6));
    player.addGold(gold);

    // Loot: bosses always drop something nice; others sometimes drop a draught.
    if (def.boss) {
      player.addItem('ambrosiaCake', 2);
      this.game.ui.toast(`${def.name} defeated! Loot acquired.`, 'gold');
    } else if (chance(0.25)) {
      const drop = chance(0.5) ? 'ambrosiaCake' : 'nectarVial';
      player.addItem(drop);
      this.game.ui.toast(`Found ${ITEMS[drop].name}`, 'pickup');
    }

    // Quest hook.
    this.game.quests.onKill(def.id);

    // Tell the bestiary we've seen/slain this creature.
    this.game.ui.recordBestiary(def.id);
  }

  // Convenience for the bestiary / lore lookups.
  creatureDef(id) { return CREATURES[id]; }
}
