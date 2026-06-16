# The Mythic Chronicles: The Hidden World

*(repo: Chronicles-of-the-Spice-Trip)*

A dark, retro **2D pixel-art Greek-mythology RPG** built from scratch with
vanilla JavaScript and the HTML5 Canvas — no engine, no build step, and **no
external art or audio files**. Every sprite and tile is generated procedurally
as pixel art, every sound is synthesized at runtime, and the whole thing runs in
a single browser tab.

> The Veil between the mortal world and the old one has thinned around you. You
> are **Unmarked** — a demigod whose divine parent has not yet claimed you —
> woken in the cursed **Mistwood** by the last Oracle. The sleeping gods, the
> **Theurgoi**, are stirring, and an ancient prophecy walks with you.

It plays like a lost late-90s RPG cartridge: fog-drowned forests, haunted ruins,
drowned temples, shadowed caves and a pale citadel of cold light — all lit by
candle, brazier and moon against a deep, mysterious dark.

---

## ▶ How to run

The game uses ES modules, so it must be served over HTTP (opening `index.html`
directly via `file://` will not load the modules). A tiny zero-dependency Node
server is included — **no `npm install`, no internet required**:

```bash
node serve.mjs
# then open http://localhost:8123
```

Any other static server works too:

```bash
python -m http.server 8000      # then open http://localhost:8000
npx serve .                     # Node alternative (needs internet the first time)
```

Open in a modern browser (Chrome, Edge or Firefox). Click once to enable audio.

---

## 🎮 Controls

| Input | Action |
| --- | --- |
| **W A S D** | Move (8-directional) |
| **Mouse** | Aim spells & the blade |
| **1 – 6** | Wield the god-power in that slot |
| **Left Click** | Swing the godless blade (no-mana melee) |
| **Right Click** | Cast the power in slot 1 |
| **Shift** | Run |
| **Space** | Dodge-roll (brief invulnerability) |
| **E** | Talk to an NPC / interact (asphodel & shards auto-collect) |
| **Tab** | Quest journal |
| **I** | Satchel (inventory) |
| **K** | Tome of powers |
| **B** | Bestiary |
| **M** | Map of the hidden world (fast-travel between unlocked realms) |
| **Esc** | Pause / close menus |

---

## ✦ Features

- **Pixel-art everything, generated in code** — dithered terrain tilesets, gothic
  props (dead trees, broken pillars, weathered statues, braziers, graves), and
  animated hero/NPC/monster sprites, all rasterized procedurally. No image files.
- **Dark, atmospheric lighting** — each region is plunged into gloom, then pools
  of candle/brazier/moon light are carved back out, with drifting fog layers, a
  player torch, glowing windows and a vignette. The dark is the point.
- **A cinematic intro** — a moonlit, fog-drowned valley with the Pale Citadel
  burning cold light on a black hill, framed by a dead-tree forest, while the
  prophecy fades in. Click to begin.
- **Six themed realms** — the cursed **Mistwood**, the **Drowned Temple**, the
  haunted **Hollow Reach**, the **Underway** caves, the **Pale Citadel** and the
  god-realm **Threshold** — each with its own palette, darkness, fog and monsters.
- **Six divine powers** — Emberhex (fire), Tidelash (water/slow), Stormbolt
  (chaining lightning), Wildmend (heal), Grey Ward (shield) and the AoE ultimate
  **Stormcrown's Wrath** — all mouse-aimed, with particles, dynamic light,
  screen-shake and synthesized sound. Powers upgrade as you level and quest.
- **Monsters with AI & lore** — mistling shades, hollow hounds, serpentine
  gorgythe, ranged strix, bronze automatons and the **Cinder-Maw** boss, each
  with weaknesses, behaviors and bestiary entries.
- **Quests, NPCs & dialogue** — an original chain of divine errands (a stolen
  relic, asphodel for the dead, the Trial of Bronze, the Cinder-Maw) with a full
  journal and quest-marker NPCs.
- **Progression & UI** — XP, levels, gold, inventory (ambrosia & nectar), a
  learnable tome and a fill-in bestiary, all wrapped in an "ancient tome" UI with
  HUD bars, a spell hotbar, a live minimap, floating damage numbers and banners.
- **Save system & settings** — localStorage saves, volume/sensitivity options, a
  pause menu and a tutorial for new players.

---

## 🗂 Project structure

```
index.html              Canvas + UI host + pixel font
serve.mjs               Tiny zero-dependency static dev server
css/style.css           The entire UI theme (HUD + ancient-tome panels)
js/
  main.js               Entry point / Canvas guard
  core/
    Game.js             Orchestrator: 2D loop, virtual buffer, state machine,
                        camera, region loading, dialogue, spawning, travel
    Input.js            Keyboard + mouse (absolute aim)
    AudioManager.js     Procedural Web Audio music & SFX
    SaveManager.js      localStorage save/load
    Utils.js            Math, easing, seeded RNG, value noise, 2D helpers
  world/
    PixelArt.js         Procedural pixel sprites: tiles, props, characters, monsters
    TileMap.js          Tile maps, collision, prop/pickup placement, lighting + fog
  entities/
    Player.js           Top-down controller, roll, melee, stats + progression
    Enemy.js            Creature AI + health bars + hit flash
    NPC.js              Quest-givers + dialogue triggers + markers
  systems/
    MagicSystem.js      Spells, 2D projectiles, chaining beams, effects, lights
    CombatSystem.js     Damage arbitration, XP/loot
    QuestSystem.js      Quest state machine & objectives
  ui/
    UI.js               HUD + minimap + all menu panels + dialogue
  intro/
    IntroScene.js       The pixel-painted cinematic opening
  data/
    GameData.js         All content: gods, prophecy, regions, spells, monsters,
                        quests, NPCs, items
```

---

## 📜 A note on the setting

The pantheon (**the Theurgoi**), the demigod framing, the prophecy, the monsters
and the sacred underworld flower (**asphodel**, tended for the restless dead) are
entirely original inventions. They aim for the *feeling* of Greek-myth adventure
fiction — a hidden world of gods, demigods, prophecies and beasts existing
alongside the mortal one — **without reusing any real or fictional names,
characters or locations**. It is worldbuilding and atmosphere, not realism.

---

*Built as a complete indie game prototype. No external assets — just code.*
