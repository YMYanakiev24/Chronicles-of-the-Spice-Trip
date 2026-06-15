/**
 * GameData.js
 * ----------------------------------------------------------------------------
 * The single source of truth for the world of "The Mythic Chronicles".
 *
 * SETTING — a dark, hidden world of Greek-flavored myth that has existed
 * alongside the mortal one for thousands of years. The player is an "Unmarked"
 * demigod who crosses the Veil and discovers cursed forests, drowned temples,
 * haunted ruins and sleeping gods. Everything here — the pantheon (the
 * Theurgoi), the heroes, the monsters — is ORIGINAL. It evokes the feeling of
 * Greek-myth adventure without reusing any real or fictional names, characters
 * or locations.
 * ----------------------------------------------------------------------------
 */

// ===========================================================================
// THE THEURGOI — the original pantheon of sleeping gods.
// ===========================================================================
export const GODS = {
  hyperaxis: {
    name: 'Hyperaxis, the Stormcrown',
    domain: 'Sky · Thunder · Kingship',
    color: '#9fd0ff',
    lore: 'First among the Theurgoi. He hammered the sky into being and rules ' +
      'the storm. His lightning answers only the worthy — and the doomed.',
  },
  thalassar: {
    name: 'Thalassar, the Deep Sovereign',
    domain: 'Sea · Drowned Depths',
    color: '#4fd2c8',
    lore: 'Lord of every black fathom. The Drowned Temple was his once, before ' +
      'the mortals there forgot to fear the tide.',
  },
  nyxara: {
    name: 'Nyxara, the Veiled Queen',
    domain: 'Night · Death · The Underway',
    color: '#9a6bff',
    lore: 'Keeper of the dead and the dark between stars. The shades that haunt ' +
      'the ruins are hers — strayed, or sent.',
  },
  sophiel: {
    name: 'Sophiel, the Grey-Eyed',
    domain: 'Wisdom · War-craft · Wards',
    color: '#bfc8d8',
    lore: 'She wove the first ward and the first stratagem. Her grey gaze misses ' +
      'nothing, and forgives little.',
  },
  pyrrhon: {
    name: 'Pyrrhon, the Forgefather',
    domain: 'Fire · The Forge',
    color: '#ff7a3a',
    lore: 'Smith of the gods. He bound bronze guardians to walk a thousand ' +
      'years, and lit the first hex-fire in a mortal\'s hand.',
  },
  koravel: {
    name: 'Koravel, the Green Crown',
    domain: 'Wild Growth · The Harvest',
    color: '#76e06b',
    lore: 'Goddess of root and ruin alike. The asphodel that blooms among the ' +
      'graves is her mercy to the dead.',
  },
  aurelith: {
    name: 'Aurelith, the Dawnbearer',
    domain: 'Sun · Prophecy · Light',
    color: '#ffd76b',
    lore: 'Bearer of the dawn and the dread gift of foresight. It was her ' +
      'oracle who first spoke your name.',
  },
};

// The central prophecy that frames the whole adventure (original wording).
export const PROPHECY = [
  'When the Veil grows thin and the old powers stir,',
  'an Unmarked child shall take the godless blade.',
  'Through cursed wood and drowned and hollow hall,',
  'they will wake what sleeps — and be claimed, or unmade.',
];

// ===========================================================================
// REGIONS — each is a themed 2D map preset: palette, tileset, darkness level,
// fog, ambient light tint, roaming monsters and music mood.
// ===========================================================================
export const REGIONS = {
  mistwood: {
    id: 'mistwood',
    name: 'The Mistwood',
    subtitle: 'Cursed Forest',
    description: 'A black wood drowned in fog, where the trees lean toward you ' +
      'when you are not looking.',
    locked: false,
    tileset: 'forest',
    ground: { base: '#1a2a1e', accent: '#243a28' },
    darkness: 0.62,            // 0 = bright day, 1 = pitch black
    fog: { color: '#7a9a8a', density: 0.5 },
    lightTint: '#cfe6d0',
    enemies: ['mistling', 'hollowHound', 'gorgythe'],
    music: 'mist',
    spawn: { tx: 16, ty: 28 },
  },
  temple: {
    id: 'temple',
    name: 'The Drowned Temple',
    subtitle: 'Forgotten Temple',
    description: 'Marble halls half-claimed by black water, raised to a sea-god ' +
      'no one dares name aloud.',
    locked: true,
    tileset: 'temple',
    ground: { base: '#16242e', accent: '#21323e' },
    darkness: 0.55,
    fog: { color: '#6a8a9a', density: 0.35 },
    lightTint: '#9fd0e0',
    enemies: ['mistling', 'bronzeWatcher', 'strix'],
    music: 'temple',
    spawn: { tx: 8, ty: 30 },
  },
  ruins: {
    id: 'ruins',
    name: 'The Hollow Reach',
    subtitle: 'Haunted Ruins',
    description: 'A fallen city of broken colonnades, where the shades of its ' +
      'people still keep their endless watch.',
    locked: true,
    tileset: 'ruins',
    ground: { base: '#241f28', accent: '#352c3a' },
    darkness: 0.6,
    fog: { color: '#8a7a9a', density: 0.4 },
    lightTint: '#d8c0e8',
    enemies: ['mistling', 'gorgythe', 'strix', 'bronzeWatcher'],
    music: 'ruins',
    spawn: { tx: 30, ty: 30 },
  },
  caves: {
    id: 'caves',
    name: 'The Underway',
    subtitle: 'Shadowed Caves',
    description: 'Lightless tunnels that wind down toward Nyxara\'s realm. The ' +
      'dark here is old, and it is listening.',
    locked: true,
    tileset: 'cave',
    ground: { base: '#16121c', accent: '#241a2e' },
    darkness: 0.85,
    fog: { color: '#3a2a4a', density: 0.25 },
    lightTint: '#b48aff',
    enemies: ['hollowHound', 'gorgythe', 'mistling'],
    music: 'caves',
    spawn: { tx: 10, ty: 10 },
  },
  citadel: {
    id: 'citadel',
    name: 'The Pale Citadel',
    subtitle: 'Ancient Castle',
    description: 'A fortress of bone-white stone on a black hill, its windows ' +
      'burning with cold, sourceless light.',
    locked: true,
    tileset: 'citadel',
    ground: { base: '#1c1d24', accent: '#2a2c36' },
    darkness: 0.5,
    fog: { color: '#9aa0b8', density: 0.3 },
    lightTint: '#cdd6ff',
    enemies: ['bronzeWatcher', 'strix', 'hollowHound'],
    music: 'citadel',
    spawn: { tx: 24, ty: 38 },
  },
  godreach: {
    id: 'godreach',
    name: 'The Threshold',
    subtitle: 'Realm of the Theurgoi',
    description: 'Where the marble of the gods meets the edge of the mortal ' +
      'sky. Lightning sleeps in the grass here.',
    locked: true,
    tileset: 'godreach',
    ground: { base: '#1e2436', accent: '#2c3650' },
    darkness: 0.4,
    fog: { color: '#aab6e0', density: 0.25 },
    lightTint: '#ffe8b0',
    enemies: ['bronzeWatcher', 'strix'],
    music: 'godreach',
    spawn: { tx: 24, ty: 40 },
  },
};

export const REGION_ORDER = ['mistwood', 'temple', 'ruins', 'caves', 'citadel', 'godreach'];

// ===========================================================================
// DIVINE POWERS — six "spells", each a gift of one of the Theurgoi.
//   kind: 'projectile' | 'beam' | 'self' | 'aoe'
// ===========================================================================
export const SPELLS = {
  emberhex: {
    id: 'emberhex', name: 'Emberhex', school: 'Fire', god: 'pyrrhon',
    kind: 'projectile', color: '#ff7a3a', glow: '#ffd089',
    manaCost: 12, cooldown: 0.4, damage: 22, speed: 230, radius: 4, splash: 22,
    unlockLevel: 1,
    desc: 'Pyrrhon\'s forge-fire, hurled from the hand. Bursts on impact.',
    upgrade: 'Cinderburst — the impact scatters burning embers.',
  },
  tidelash: {
    id: 'tidelash', name: 'Tidelash', school: 'Water', god: 'thalassar',
    kind: 'projectile', color: '#4fd2c8', glow: '#bff6ee',
    manaCost: 10, cooldown: 0.55, damage: 16, speed: 200, radius: 4, splash: 16, slow: 0.5,
    unlockLevel: 2,
    desc: 'A whip of black brine that chills and slows what it strikes.',
    upgrade: 'Undertow — struck foes are dragged and briefly rooted.',
  },
  stormbolt: {
    id: 'stormbolt', name: 'Stormbolt', school: 'Lightning', god: 'hyperaxis',
    kind: 'beam', color: '#9fd0ff', glow: '#eaffff',
    manaCost: 24, cooldown: 1.0, damage: 40, range: 150, chain: 3,
    unlockLevel: 3,
    desc: 'A spear of the Stormcrown\'s lightning that leaps between foes.',
    upgrade: 'Chain Tempest — arcs to two additional enemies.',
  },
  wildmend: {
    id: 'wildmend', name: 'Wildmend', school: 'Life', god: 'koravel',
    kind: 'self', color: '#76e06b', glow: '#d6ffc8',
    manaCost: 28, cooldown: 6, heal: 55,
    unlockLevel: 2,
    desc: 'Koravel\'s green mercy, knitting flesh and steadying the heart.',
    upgrade: 'Greater Bloom — also grants a lingering renewal.',
  },
  greyward: {
    id: 'greyward', name: 'Grey Ward', school: 'Ward', god: 'sophiel',
    kind: 'self', color: '#bfc8d8', glow: '#ffffff',
    manaCost: 22, cooldown: 9, shield: 60, duration: 8,
    unlockLevel: 3,
    desc: 'Sophiel\'s ward — a shell of grey light that drinks the next blows.',
    upgrade: 'Mirror Ward — reflects a portion of damage back.',
  },
  stormcrownWrath: {
    id: 'stormcrownWrath', name: "Stormcrown's Wrath", school: 'Divine', god: 'hyperaxis',
    kind: 'aoe', color: '#ffe08a', glow: '#ffffff',
    manaCost: 60, cooldown: 18, damage: 120, radius: 90,
    unlockLevel: 5,
    desc: 'Call down the judgement of the Stormcrown. The sky itself answers.',
    upgrade: 'Reckoning — the storm lingers and strikes a second time.',
  },
};

export const SPELL_ORDER = [
  'emberhex', 'tidelash', 'stormbolt', 'wildmend', 'greyward', 'stormcrownWrath',
];

// ===========================================================================
// THE BESTIARY — original monsters of Greek-myth flavor.
// ===========================================================================
export const CREATURES = {
  mistling: {
    id: 'mistling', name: 'Mistling', hp: 30, speed: 46, damage: 6,
    attackRange: 18, attackRate: 1.3, aggroRange: 150, xp: 12, size: 12,
    body: 'shade', color: '#8aa89a', weakness: 'Fire', float: true,
    lore: 'A scrap of a forgotten soul, woven from fog. Drifts toward the ' +
      'living warmth it can no longer remember having.',
  },
  hollowHound: {
    id: 'hollowHound', name: 'Hollow Hound', hp: 70, speed: 88, damage: 14,
    attackRange: 20, attackRate: 0.9, aggroRange: 180, xp: 26, size: 14,
    body: 'beast', color: '#2a1f2e', weakness: 'Fire',
    lore: 'A black hound of the Underway, hollow where its heart should be. It ' +
      'hunts the moment your torch gutters low.',
  },
  gorgythe: {
    id: 'gorgythe', name: 'Gorgythe', hp: 90, speed: 60, damage: 16,
    attackRange: 22, attackRate: 1.1, aggroRange: 160, xp: 34, size: 15,
    body: 'serpent', color: '#3f6d4a', weakness: 'Lightning',
    lore: 'A serpent-maned thing of the ruins. Do not meet the green light of ' +
      'its eyes for long — stone has a way of creeping in.',
  },
  strix: {
    id: 'strix', name: 'Strix', hp: 60, speed: 70, damage: 14,
    attackRange: 130, attackRate: 1.5, aggroRange: 200, ranged: true,
    xp: 30, size: 13, body: 'bird', color: '#4a3a5a', weakness: 'Lightning', float: true,
    lore: 'A night-bird out of the old curses, screaming omens. It flings ' +
      'feathers of shadow from above.',
  },
  bronzeWatcher: {
    id: 'bronzeWatcher', name: 'Bronze Watcher', hp: 150, speed: 38, damage: 24,
    attackRange: 26, attackRate: 1.8, aggroRange: 150, xp: 48, size: 18,
    body: 'automaton', color: '#9a7a3a', weakness: 'Water',
    lore: 'A guardian forged by Pyrrhon and bound to its post for an age. The ' +
      'sea\'s salt is its only undoing.',
  },
  cinderMaw: {
    id: 'cinderMaw', name: 'Pyraketh, the Cinder-Maw', hp: 340, speed: 40, damage: 30,
    attackRange: 30, attackRate: 1.6, aggroRange: 200, xp: 150, size: 26,
    body: 'beast', color: '#6a2a1f', weakness: 'Water', boss: true, ranged: true,
    lore: 'A lion-bodied horror with a furnace for a throat, loosed from the ' +
      'deep caves. The Mistwood will not be free of fire until it falls.',
  },
};

// ===========================================================================
// DIVINE QUESTS
//   type: 'kill' | 'collect' | 'talk' | 'reach' | 'cast'
// ===========================================================================
export const QUESTS = {
  awakening: {
    id: 'awakening', title: 'The Unmarked', giver: 'oracle', region: 'mistwood',
    summary: 'The last Oracle, Theira, will teach you to channel a god\'s power.',
    lore: 'The Veil has thinned around you for a reason. Learn to wield the ' +
      'fire in your blood, and the hidden world will open.',
    objectives: [
      { type: 'cast', spell: 'emberhex', count: 1, text: 'Cast Emberhex (press 1)' },
      { type: 'kill', creature: 'mistling', count: 3, text: 'Banish 3 Mistlings' },
    ],
    rewards: { xp: 40, gold: 20, items: ['nectarVial'] },
    next: 'stolenRelic',
  },
  stolenRelic: {
    id: 'stolenRelic', title: 'The Stolen Relic', giver: 'kassander', region: 'mistwood',
    summary: 'The hero Kassander\'s heirloom — shards of a godless blade — were ' +
      'scattered by the shades. Recover them.',
    lore: 'A demigod without their relic is half a hero. Kassander paces the ' +
      'wood, hunting the shades that robbed him.',
    objectives: [
      { type: 'collect', item: 'relicShard', count: 4, text: 'Recover 4 relic shards' },
      { type: 'talk', npc: 'kassander', text: 'Return the shards to Kassander' },
    ],
    rewards: { xp: 80, gold: 50, spell: 'stormbolt' },
  },
  asphodel: {
    id: 'asphodel', title: 'Flowers for the Dead', giver: 'myrrha', region: 'mistwood',
    summary: 'Sister Myrrha needs sacred Asphodel blooms to keep the wood\'s ' +
      'restless dead at peace.',
    lore: 'The asphodel grows only where the dead lie thick. Its pale glow keeps ' +
      'the shades from waking — but the flowers are fading, and the dead stir.',
    objectives: [
      { type: 'collect', item: 'asphodel', count: 6, text: 'Gather 6 Asphodel blooms' },
      { type: 'talk', npc: 'myrrha', text: 'Bring the blooms to Sister Myrrha' },
    ],
    rewards: { xp: 90, gold: 40, spell: 'wildmend', items: ['TopChuk'] },
  },
  trialBronze: {
    id: 'trialBronze', title: 'Trial of Bronze', giver: 'kassander', region: 'mistwood',
    summary: 'Prove your worth against Pyrrhon\'s bound guardians to earn passage ' +
      'toward the drowned temple.',
    lore: 'No demigod passes deeper into the hidden world without besting the ' +
      'bronze watchers. They do not tire. You must.',
    objectives: [
      { type: 'kill', creature: 'bronzeWatcher', count: 2, text: 'Destroy 2 Bronze Watchers' },
    ],
    rewards: { xp: 160, gold: 80, items: ['ambrosiaCake', 'ambrosiaCake'] },
  },
  cinderMaw: {
    id: 'cinderMaw', title: 'The Cinder-Maw', giver: 'oracle', region: 'mistwood',
    summary: 'A fire-throated horror has crawled up from the caves. End it before ' +
      'it burns the wood to ash.',
    lore: 'The prophecy spoke of waking what sleeps. Something has woken early, ' +
      'and it is hungry. This is the first true test of the Unmarked.',
    objectives: [
      { type: 'kill', creature: 'cinderMaw', count: 1, text: 'Slay Pyraketh, the Cinder-Maw' },
      { type: 'talk', npc: 'oracle', text: 'Tell Theira the beast is slain' },
    ],
    rewards: { xp: 240, gold: 120, spell: 'greyward', items: ['cinderSigil'] },
  },
};

export const QUEST_ORDER = [
  'awakening', 'stolenRelic', 'asphodel', 'trialBronze', 'cinderMaw',
];

// ===========================================================================
// NPCs — quest-givers and the folk of the hidden world.
// ===========================================================================
export const NPCS = {
  oracle: {
    id: 'oracle', name: 'Theira, the Last Oracle', region: 'mistwood',
    color: '#caa8e8', tx: 18, ty: 26, role: 'Oracle of Aurelith',
    greeting: 'So. The Veil spat you out at last. I have waited a long time for you, Unmarked one.',
    lines: [
      'You crossed from the mortal world without knowing it — most never do. ' +
        'The glow in your blood is divine. You are a demigod, child of a god ' +
        'who has not yet claimed you.',
      'Press 1 to loose an Emberhex — Pyrrhon\'s gift, the first power most ' +
        'demigods find. Try it on those mistlings before they drink your warmth.',
    ],
  },
  kassander: {
    id: 'kassander', name: 'Kassander the Unclaimed', region: 'mistwood',
    color: '#c89a5a', tx: 8, ty: 18, role: 'Demigod',
    greeting: 'Another Unmarked. Good. The wood eats the ones who walk it alone.',
    lines: [
      'I have hunted monsters in this hidden world for thirty years and still ' +
        'no god has claimed me. Do not let that frighten you. A blade does not ' +
        'need a name to cut.',
      'The shades shattered my heirloom — a godless blade, forged before the ' +
        'Theurgoi. Bring me its shards and I will teach you the Stormcrown\'s lightning.',
    ],
  },
  myrrha: {
    id: 'myrrha', name: 'Sister Myrrha', region: 'mistwood',
    color: '#88c0a0', tx: 26, ty: 16, role: 'Priestess of Koravel',
    greeting: 'Walk softly here. The dead sleep light beneath this moss.',
    lines: [
      'I tend the asphodel for Koravel, that the wood\'s dead may rest. While ' +
        'the pale flowers glow, the shades stay still.',
      'But the blooms are withering and the dead grow restless. Gather fresh ' +
        'asphodel — it glows among the graves — and the Green Crown will teach ' +
        'you her mending.',
    ],
  },
  ferryman: {
    id: 'ferryman', name: 'The Veiled Ferryman', region: 'mistwood',
    color: '#7a7a8a', tx: 22, ty: 36, role: '???',
    greeting: 'You burn brighter than the others, Unmarked. The Veiled Queen has noticed.',
    lines: [
      'Six realms lie beyond this wood: a drowned temple, a city of shades, ' +
        'the caves that go down to Nyxara, a citadel of cold light, and the ' +
        'Threshold of the gods themselves.',
      'The prophecy walks with you whether you will it or not. Grow strong ' +
        'here first. The hidden world does not forgive the unready.',
    ],
  },
};

// ===========================================================================
// ITEMS
// ===========================================================================
export const ITEMS = {
  ambrosiaCake: {
    id: 'ambrosiaCake', name: 'Ambrosia Cake', type: 'consumable',
    icon: '🍯', heal: 50, desc: 'A morsel of the food of the gods. Restores 50 ' +
      'health. Too much would burn a mortal to ash.',
  },
  nectarVial: {
    id: 'nectarVial', name: 'Vial of Nectar', type: 'consumable',
    icon: '🏺', mana: 50, desc: 'The drink of the Theurgoi. Restores 50 mana. ' +
      'Cold, golden, and faintly humming.',
  },
  ambrosiaCake2: {
    id: 'ambrosiaCake2', name: 'Honeyed Ambrosia', type: 'consumable',
    icon: '🍮', heal: 30, mana: 30, desc: 'A richer cake that mends body and ' +
      'spirit at once.',
  },
  relicShard: {
    id: 'relicShard', name: 'Relic Shard', type: 'quest',
    icon: '🗡️', desc: 'A shard of Kassander\'s godless blade. It hums when the ' +
      'dead draw near.',
  },
  asphodel: {
    id: 'asphodel', name: 'Asphodel Bloom', type: 'quest',
    icon: '🌼', desc: 'A pale flower of the underworld\'s meadows. It glows ' +
      'softly and smells of cold stone and rain.',
  },
  cinderSigil: {
    id: 'cinderSigil', name: 'Cinder Sigil', type: 'relic',
    icon: '🔥', desc: 'A coal that burns without dying, taken from the Cinder-Maw. ' +
      '+10% fire damage.', passive: { fireDamage: 0.1 },
  },
  goldCoin: {
    id: 'goldCoin', name: 'Obol', type: 'currency',
    icon: '🪙', desc: 'An old coin for the ferryman. The dead still expect it.',
  },
};

// ===========================================================================
// PROGRESSION
// ===========================================================================
export function xpForLevel(level) {
  return Math.floor(60 * Math.pow(level, 1.5));
}

export const TIPS = [
  'Press 1–6 to wield a god\'s power. Mana returns on its own, slowly.',
  'Every monster fears a different god. Read the bestiary in your tome.',
  'Asphodel glows among the graves. Gather it to keep the dead at rest.',
  'Hold SHIFT to run. SPACE to dodge-roll through a blow.',
  'Press TAB for your quests, I for your satchel, M for the map of the hidden world.',
  'Raise the Grey Ward before a hard fight — a shattered ward still saves a life.',
  'The Stormcrown\'s Wrath is slow to gather but unmakes everything near you.',
  'Light is safety. Stay near braziers and moonlit clearings in the dark.',
  'Speak to everyone twice. The hidden world keeps its secrets for the patient.',
];
