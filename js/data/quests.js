// Quest database for the Chronicles of the Emerald Realm
export const QUESTS = {
  stolen_codex: {
    id: 'stolen_codex',
    title: 'The Stolen Codex',
    giver: 'magister_aelion',
    description: 'Magister Aelion\'s ancient spell codex has been stolen by Shadow Wraiths lurking in the Dark Ruins to the northeast. Recover it before its dark magic is unleashed.',
    objectives: [
      { id: 'talk_to_magister', text: 'Speak with Magister Aelion', type: 'talk', target: 'magister_aelion', count: 1, progress: 0, done: false },
      { id: 'kill_wraiths', text: 'Defeat Shadow Wraiths', type: 'kill', target: 'shadow_wraith', count: 5, progress: 0, done: false },
      { id: 'find_codex', text: 'Recover the Arcane Codex', type: 'collect', target: 'arcane_codex', count: 1, progress: 0, done: false }
    ],
    rewards: {
      xp: 200,
      items: [{ id: 'arcane_focus', name: 'Arcane Focus Crystal', icon: '💎', desc: 'Amplifies spell damage by 20%.', bonus: { spellDamage: 0.2 } }],
      gold: 50
    },
    lore: 'The Codex of Aelion contains forbidden knowledge of the first god-mages who shaped the Emerald Realm.'
  },
  stone_curse: {
    id: 'stone_curse',
    title: 'The Stone Curse',
    giver: 'wounded_traveler',
    description: 'A Great Basilisk has made its lair in the Ancient Ruins to the east, turning travelers to stone with its petrifying gaze. End this menace.',
    objectives: [
      { id: 'find_ruins', text: 'Find the Ancient Ruins', type: 'reach', target: 'ancient_ruins', count: 1, progress: 0, done: false },
      { id: 'slay_basilisk', text: 'Defeat the Great Basilisk', type: 'kill', target: 'basilisk', count: 1, progress: 0, done: false }
    ],
    rewards: {
      xp: 350,
      items: [
        { id: 'basilisk_scale', name: 'Basilisk Scale', icon: '🐍', desc: 'A shimmering scale from the Great Basilisk. Highly prized by alchemists.', bonus: {} },
        { id: 'stonebreaker', name: 'Stonebreaker Charm', icon: '⚒', desc: '+25 max health.', bonus: { maxHealth: 25 } }
      ],
      gold: 80
    },
    lore: 'The Great Basilisk is said to be a creation of the Titan Petros, who wept tears of stone when the gods abandoned him.'
  },
  corrupted_grove: {
    id: 'corrupted_grove',
    title: "The Grove's Corruption",
    giver: 'old_bramble',
    description: 'The northern grove has been tainted by dark magic. Three corruption nodes pulse with malevolent energy, and a Dark Treant guards the heart of the blight. Cleanse the forest.',
    objectives: [
      { id: 'destroy_nodes', text: 'Destroy Corruption Nodes', type: 'destroy', target: 'corruption_node', count: 3, progress: 0, done: false },
      { id: 'defeat_treant', text: 'Defeat the Dark Treant', type: 'kill', target: 'dark_treant', count: 1, progress: 0, done: false }
    ],
    rewards: {
      xp: 280,
      items: [{ id: 'natures_blessing', name: "Nature's Blessing Amulet", icon: '🌟', desc: 'Mana regenerates 50% faster.', bonus: { manaRegen: 1.5 } }],
      gold: 60
    },
    lore: 'The Emerald Realm\'s forests are living beings. When they weep, the world bleeds.'
  },
  gods_relic: {
    id: 'gods_relic',
    title: "The God's Lost Relic",
    giver: 'ancient_spirit',
    description: 'Deep within the Forgotten Temple lies the Aureate Sigil — an artifact belonging to the sky-god Aurelius. Ancient guardians protect it. Recover the relic and restore divine balance.',
    objectives: [
      { id: 'enter_temple', text: 'Enter the Forgotten Temple', type: 'reach', target: 'forgotten_temple', count: 1, progress: 0, done: false },
      { id: 'defeat_guardians', text: 'Defeat Temple Guardians', type: 'kill', target: 'stone_golem', count: 3, progress: 0, done: false },
      { id: 'claim_sigil', text: 'Claim the Aureate Sigil', type: 'collect', target: 'aureate_sigil', count: 1, progress: 0, done: false }
    ],
    rewards: {
      xp: 450,
      items: [
        { id: 'aureate_sigil', name: 'Aureate Sigil', icon: '✦', desc: 'Blessed by Aurelius. All stats increased by 15%.', bonus: { allStats: 0.15 } },
        { id: 'temple_key', name: 'Temple Key', icon: '🗝', desc: 'Unlocks the passage to the God Realm.', bonus: {} }
      ],
      gold: 120
    },
    lore: 'Aurelius, god of sky and storms, lost the Sigil when the Titans drove the gods from the mortal realm.'
  },
  village_siege: {
    id: 'village_siege',
    title: 'Village Under Siege',
    giver: 'sentinel_gareth',
    description: 'The village of Emberkeep is under attack! Dark creatures have breached the outer wards. Defend the villagers through three waves of increasingly powerful enemies.',
    objectives: [
      { id: 'wave_1', text: 'Survive Wave 1 — Shadow Wraiths', type: 'survive', target: 'wave_1', count: 5, progress: 0, done: false },
      { id: 'wave_2', text: 'Survive Wave 2 — Golem Advance', type: 'survive', target: 'wave_2', count: 3, progress: 0, done: false },
      { id: 'wave_3', text: 'Defeat the Wave Commander', type: 'kill', target: 'dark_mage_commander', count: 1, progress: 0, done: false }
    ],
    rewards: {
      xp: 300,
      items: [
        { id: 'heroes_emblem', name: "Hero's Emblem", icon: '🏅', desc: 'Worn by the defenders of Emberkeep. +15 max mana.', bonus: { maxMana: 15 } },
        { id: 'merchant_favor', name: "Merchant's Favor Token", icon: '🪙', desc: '20% discount at Kaelos\'s shop.', bonus: {} }
      ],
      gold: 100
    },
    lore: 'Emberkeep was the last village to fall during the First Shadow War. Its people have endured darkness before.'
  }
};

export const QUEST_IDS = Object.keys(QUESTS);
