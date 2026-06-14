// Spell definitions for the magic system
export const SPELLS = {
  Q: {
    id: 'pyroclast',
    name: 'Pyroclast',
    key: 'Q',
    icon: '🔥',
    type: 'fire',
    damage: 30,
    manaCost: 15,
    cooldown: 1.2,
    speed: 28,
    radius: 1.2,
    color: 0xff4400,
    glowColor: 0xff8800,
    description: 'Launch a bolt of concentrated fire that scorches enemies on impact.',
    particles: { count: 40, color: 0xff6600, size: 0.15, spread: 0.5 }
  },
  E: {
    id: 'galestrike',
    name: 'Galestrike',
    key: 'E',
    icon: '⚡',
    type: 'lightning',
    damage: 45,
    manaCost: 25,
    cooldown: 2.0,
    speed: 40,
    radius: 1.0,
    color: 0x66aaff,
    glowColor: 0xaaddff,
    description: 'Hurl a crackling bolt of arcane lightning that chains to nearby foes.',
    particles: { count: 60, color: 0x88ccff, size: 0.1, spread: 0.8 }
  },
  R: {
    id: 'verdant_mend',
    name: 'Verdant Mend',
    key: 'R',
    icon: '🌿',
    type: 'nature',
    heal: 35,
    manaCost: 20,
    cooldown: 3.0,
    description: 'Channel the life force of the Emerald Realm to restore your vitality.',
    particles: { count: 50, color: 0x44ee88, size: 0.12, spread: 1.0 }
  },
  T: {
    id: 'arcane_aegis',
    name: 'Arcane Aegis',
    key: 'T',
    icon: '🛡',
    type: 'shield',
    manaCost: 30,
    cooldown: 8.0,
    duration: 5.0,
    damageReduction: 0.6,
    description: 'Surround yourself with a shimmering barrier of arcane energy.',
    particles: { count: 80, color: 0x5599ff, size: 0.08, spread: 1.5 }
  },
  G: {
    id: 'mythic_nova',
    name: 'Mythic Nova',
    key: 'G',
    icon: '✨',
    type: 'ultimate',
    damage: 90,
    manaCost: 60,
    cooldown: 12.0,
    blastRadius: 8,
    color: 0xcc44ff,
    glowColor: 0xee88ff,
    description: 'Unleash a devastating explosion of pure mythic energy, annihilating all nearby foes.',
    particles: { count: 150, color: 0xcc66ff, size: 0.2, spread: 2.5 }
  }
};

export const SPELL_KEYS = ['Q', 'E', 'R', 'T', 'G'];
