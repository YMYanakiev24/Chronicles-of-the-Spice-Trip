# Prompt: Port "Chronicles of the Spice Trip" to Unity

> Paste everything below the line into a fresh Claude Code session (Opus 5.5, highest effort),
> started in `C:\Users\Acer\Desktop\Chr\`. It is written for one long autonomous run.

---

## Your role and goal

You are a senior Unity engineer and game programmer. Port my browser game **Chronicles of the
Spice Trip** (repo title "The Mythic Chronicles: The Hidden World"), written in vanilla JavaScript
with HTML5 Canvas, to a **Unity 6 2D project** that runs as a Windows standalone build. The port
must be faithful: same screens, same flow, same feel, same tuning numbers. After that, bring back
the larger RPG that exists in the code but can't be reached right now.

This is a long task. Work through it on your own, milestone by milestone. Don't stop after
scaffolding, and don't hand back a half-built project with "next steps" for me. Stop to ask me only
at the checkpoints marked **⛔ CHECKPOINT** or when you're truly blocked (for example, no Unity
editor is installed).

## Where things are

- **Source (read-only):** `C:\Users\Acer\Desktop\Chr\Chronicles-of-the-Spice-Trip\` (git repo,
  about 6,500 lines of JS/CSS). Don't modify it.
- **Target (create it):** `C:\Users\Acer\Desktop\Chr\SpiceTrip-Unity\`. Make it its own git repo
  with a Unity `.gitignore` (ignore `Library/`, `Temp/`, `Obj/`, `Logs/`, `UserSettings/`, `Build/`).
  Commit at the end of each milestone, using clear messages.
- **Unity editor:** look in `C:\Program Files\Unity\Hub\Editor\`. Use the newest `6000.x` LTS
  version that's installed. Don't guess the path or version. If no Unity 6 editor is installed,
  stop and tell me.

## Read before you write

Read **every** source file in full before you design anything: `js/**/*.js`, `css/style.css`,
`index.html`, both READMEs. Also look at every image in `assets/` with your image-reading tool.
Several images contain designed mechanics that you need (see Phase 3). The root README is
**out of date**: it describes the RPG as the main game and says there are "no external assets".
Trust the code, not the README.

### What is actually live today

`Game.boot()` skips the intro and goes straight to the main menu. The menu has a single
**▶ Play** button, and it calls `scenes.enterPath()`. `startNewGame()` / `continueGame()` (the
full RPG) **can't be reached**. So the game people play today is:

```
Main menu (menu.jpg background, castle hotspot or ▶ Play)
  └─► THE PATH — boss map (path.jpg) with 4 boss hotspots + "Enter the Castle"
        ├─► click a boss ─► BOSS BATTLE arena (top-down twin-stick) ─► Victory/Defeat card
        │                     (Fight again | Return to the path; Flee at any time)
        └─► Enter the Castle ─► CORRIDOR (corridor.jpg) with two doors
              ├─► "Marin the Wizard" door ─► fog wipe ─► MARIN'S APOTHECARY (shop.jpg)
              │       shop counter (4 items + Enchanted Loom tile), Marin portrait + speech line
              │       └─► ENCHANTED LOOM modal (load image → perforated grid → tear shards)
              └─► "Changing Room" door ─► CHARACTER SELECT modal (12 cards: 10 picks + 2 "Coming soon")
Back buttons: shop → corridor (fog wipe), corridor → path, path → menu.
```

The dormant RPG (6 regions, tilemaps, spells, quests, NPCs, dialogue, HUD, journal, inventory,
bestiary, map, fast travel, saves) is fully coded in `Game.js`, `TileMap.js`, `Player.js`,
`Enemy.js`, `NPC.js`, `MagicSystem.js`, `CombatSystem.js`, `QuestSystem.js`, `UI.js`,
`IntroScene.js` and `GameData.js`.

## Tech stack (use these unless you hit a hard blocker, then explain why)

- **Unity 6 LTS** with **URP and the 2D Renderer**, so you can use 2D lights for the dark,
  torch-lit look.
- **Pixel Perfect Camera** (URP) for the in-world RPG: 16 px tiles, PPU 16, reference resolution
  480×270, integer upscaling. This matches the JS virtual buffer (`BASE_VH = 270`, scale 2–5).
- **New Input System**: WASD/arrows, mouse aim, LMB/RMB, Space, Shift, E, 1–6, Tab, I, K, B, M, Esc.
- **UI Toolkit (UXML + USS)** for all menus, HUD, shop, modals and cards. It's text-authored and
  maps closely to the existing CSS. Note that USS has no gradients and no `backdrop-filter`.
  Build gradients (fog wipe, panel fades, vignette) from small textures you generate at runtime,
  or from a simple full-screen material.
- **Unity Test Framework** for EditMode and PlayMode tests.
- Install packages through an editor script that calls
  `UnityEditor.PackageManager.Client.Add("<package-id>")`, run in batchmode. That way Unity picks
  versions compatible with the installed editor. **Don't write package versions from memory.**

### Working without clicking in the editor

You can't drive the Unity Editor GUI, so make the project **reproducible from code**:

- Put the Editor-only setup in `Assets/_Project/Editor/ProjectBuilder.cs` with `[MenuItem]`
  methods that also work through `-executeMethod`. Its job is to create the scenes, configure the
  URP 2D renderer and Pixel Perfect Camera, set texture import settings, generate the
  ScriptableObject data assets, and add scenes to Build Settings. Hand-editing `.unity` /
  `.prefab` YAML is a last resort.
- Keep scenes thin. A `Boot` scene holds one bootstrap GameObject, and everything else is spawned
  and wired from code or from prefabs that the builder generates.
- Compile check after every meaningful change:
  `"<Unity.exe>" -batchmode -nographics -quit -projectPath "<proj>" -logFile -` (read the log and
  fix **every** error and warning you introduced).
- Tests: `-runTests -testPlatform EditMode -testResults <file>`, then the same with `PlayMode`.
- Build: an `-executeMethod` build script that produces `Build/Windows/SpiceTrip.exe`.
- If a Unity MCP server is connected to this session, you may use it to inspect the running editor
  or play mode, but batchmode must keep working on its own.

## Architecture

Mirror the JS module split so each file maps cleanly from one to the other:

```
Assets/_Project/
  Scripts/
    Core/        GameRoot (state machine), InputRouter, AudioSynth, SaveService, Rng/Noise utils
    Data/        ScriptableObject types + generated assets: Gods, Regions, Spells, Creatures,
                 Quests, NPCs, Items, Bosses, Characters, ShopStock, BattleTuning
    Hub/         MenuScreen, PathScreen, CorridorScreen, ShopScreen, LoomModal,
                 CharacterSelectModal, FogTransition
    Battle/      BossBattleController, BattlePlayer, BossActor, Projectile, ArenaBackdrop
    World/       PixelCanvas + PixelArtGenerator (port of PixelArt.js), TileMapBuilder, Lighting/Fog
    Entities/    Player, Enemy, NPC
    Systems/     MagicSystem, CombatSystem, QuestSystem
    UI/          HUD, panels (journal, satchel, tome, bestiary, map, settings, pause), dialogue
  UI/            UXML + USS (port css/style.css: colour variables, ancient-tome panels, buttons)
  Art/           imported images (see below)
  Editor/        ProjectBuilder, data importers, build script
  Tests/         EditMode + PlayMode
```

- **Data-driven:** every number and string in `GameData.js`, `Scenes.js` (BOSSES, CHARACTERS,
  SHOP_STOCK) and `BossBattle.js` becomes ScriptableObject data, generated by an editor importer.
  Gameplay code must not hard-code content.
- **Port exactly:** keep the original tuning in **logical pixels / seconds** in the data assets and
  convert to world units in one place. For the arena, 1 world unit = 100 logical px, so the
  800×450 arena is 8×4.5 units. Replace the JS `dt` clamp (`min(0.05, …)`) with
  `Time.deltaTime` clamped the same way. Where the JS code is frame-rate dependent (the Loom and
  scene animations add a fixed `1/60` per frame), make it time-based instead.
- **Persistence:** JSON files in `Application.persistentDataPath`, with the same three stores and
  the same shape as the JS `localStorage` keys: `save` (RPG chronicle), `settings`, and `hub`
  (`{ gold: 1450, stash: {}, defeated: [] }` by default). Also persist the selected champion. The
  JS version forgets it on reload; fix that.

## Phase 1: faithful port of the live game (the main deliverable)

Port the exact specs below from `Scenes.js`, `BossBattle.js`, `UI.js` and `style.css`. Full-screen
scenes use a **16:9 stage** that is letterboxed to fit the window
(`width: min(100vw, 100vh·16/9)`). Every hotspot is a percentage of that stage, so the click
areas stay lined up with the art at any resolution.

**Main menu.** Background `menu.jpg` (cover-fit). The title, the "World: Silk Road" line and
"GOLD: 1450" are baked into the art, so the procedural title is hidden when the image is present.
Castle hotspot: `top 8%, right 6%`, roughly 46% wide × 54% tall, with the label
"▶ Click the castle to enter". Plus the **▶ Play** button. Both lead to the Path. Menu music plays.

**The Path.** `path.jpg` fills the stage. There are 4 boss hotspots, each `16% × 28%`, positioned
at `left = (cx − 0.08)`, `top = (cy − 0.20)`. Hovering shows a red glow and the tooltip
"⚔ Boss N / name". Defeated bosses get a ✓ and 70% opacity. "🏰 Enter the Castle" hotspot:
`right 1%, top 4%, 17% × 44%`, dashed gold border.

| id | name | image | HP | colour | cx | cy |
|---|---|---|---|---|---|---|
| goodPlaz | The Good Plazmodium | plazmodiigood.jpg | 120 | #5fd06a | 0.27 | 0.72 |
| badPlaz | The Bad Plasmodium | thebadplasmodium.jpg | 160 | #9a5fd0 | 0.43 | 0.31 |
| mordekai | Mordekai | mordekai.jpg | 140 | #4a7ad0 | 0.58 | 0.68 |
| vlado | Vlado | vlado.jpg | 190 | #3a5a8a | 0.88 | 0.62 |

**Boss battle** (`BossBattle.js`). Logical arena 800×450.
- *Player:* spawns at (400, 380), radius 14, 100 HP, speed 220, clamped 16 px from the edges.
  Shoots toward the mouse on LMB or Space with a 0.22 s cooldown. Shot: speed 380, radius 5,
  life 1.4 s, 8 damage, sfx `fire`.
- *Boss:* spawns at (400, 90), radius 32, speed 95. Every 1.4–2.6 s it picks a new waypoint with
  x ∈ [120, 680] and y ∈ [60, 240]. Its position is clamped to x ∈ [40, 760], y ∈ [40, 247.5].
  Every 1.1 s it fires a **3-shot spread** (±0.22 rad) aimed at the player: speed 240, radius 7,
  life 3 s, 8 damage, sfx `lightning`. The first volley comes 1.2 s after spawn.
- *Damage:* a boss shot hitting the player gives 0.6 s of i-frames. Body contact does 12 damage
  with 0.8 s of i-frames. A player shot on the boss flashes it for 0.1 s (brightness ×2), sfx
  `hit`. While hurt, the player blinks at 40% alpha.
- *Rendering:* the boss is drawn as its image at size r × 2.6. If the image is missing, draw a
  coloured circle with two red eyes instead. The player is drawn as the selected champion's art at
  56 px. If there's no art, use the procedural hero (`hero().up[0]` at ×2.2). Both get an
  elliptical shadow. Shots are additive glows (player `#9fe0ff`, boss in the boss's colour) with a
  white core.
- *Backdrop:* a night-sky gradient (#0a0716 → #120a22 → #0c1410), a moon with a halo at
  (80%, 18%), a ground band, 22 swaying black tree silhouettes, 3 drifting fog bands (screen
  blend), and a vignette.
- *HUD:* boss HP bar at the top centre (440×16, #b03a3a, boss name in caps). "YOU" bar at the
  bottom left (240×14, #3aa05a). Hint text and a "⟵ Flee" button.
- *End:* a "VICTORY" card (sfx `levelup`, saves the boss as defeated) or a "DEFEATED" card (sfx
  `death`), with the original subtitle text, "Fight again" and "Return to the path". Battle music
  is the `caves` mood; leaving the battle goes back to `menu`.

**Corridor.** `corridor.jpg` (the art already has the door signs). Door hotspots: wizard at
`left 9%, top 19%, 25% × 60%`; changing room at `left 65%, top 24%, 26% × 54%`. Hovering gives a
gold border and inner glow. Back → Path.

**Fog wipe.** A full-screen grey-blue fog overlay (layered radial blobs plus a blur) fades in over
0.36 s. The scene swaps at 360 ms and the fade-out starts at 420 ms. It's used for corridor ↔ shop.

**Marin's Apothecary.** `shop.jpg` background. Marin's portrait comes from `marin.png`, a
transparent cutout. Show only the **top 64%** (the hands are cropped out). It sits fixed at
`left 3%, bottom 18%`, height `min(50vh, 440px)`, with a drop shadow. GOLD readout. Marin's speech
line starts as *"Welcome, traveler. Coin for cures?"*. Bottom shop panel titled
"✦ Marin's Apothecary ✦" with one card per item (icon, name, description, 🪙 price):

| item | price |
|---|---|
| ambrosiaCake (Ambrosia Cake, heals 50) | 40 |
| nectarVial (Vial of Nectar, mana 50) | 40 |
| ambrosiaCake2 (Honeyed Ambrosia, 30/30) | 70 |
| cinderSigil (Cinder Sigil, +10% fire) | 300 |

Plus a 🧵 **Enchanted Loom** card ("try it ▸"). Buying: if the player can't afford it, Marin says
*"Not enough coin, friend. Come back richer."*. Otherwise deduct the gold, add the item to the hub
stash (or to the live player's satchel if an RPG run is active), and Marin says
*"A fine choice — one {name}. It will await you on the road."* (sfx `pickup`). The stash is
granted to the player when an RPG run starts, with the toast "Marin's wares are in your satchel."

**Enchanted Loom.** A modal with these controls: load an image, a shard grid dropdown
(4×4, 8×8 (default), 12×12, 16×16), *Tear All*, *Re-weave*, and a "Shards torn: N" counter.
- *Weave:* cover-fit the image into a square of `max(48, cols × 8)` px, then upscale it to 512²
  with point filtering (pixelated). Draw a dashed perforation grid on top
  (`rgba(255,240,200,0.35)`, dash 4/4).
- *Tear:* clicking an intact shard sets vx ∈ ±130, vy ∈ [−240, −120], spin ∈ ±4 rad/s, gravity
  520, and fades it at 0.7 alpha/s, rotating around its centre (sfx `hit`). Tear All tears every
  shard (sfx `lightning`).
- *Loading an image:* in the Editor, use `EditorUtility.OpenFilePanel`. In the Windows build, use a
  native open-file dialog via P/Invoke (`comdlg32 GetOpenFileName`). Don't add a third-party
  package for this. Read the file and load it with `Texture2D.LoadImage`.

**Character select (Changing Room).** A modal titled "Choose Your Champion" with a responsive card
grid. The selected card is highlighted. Clicking a card plays sfx `pickup` and closes the modal
after 0.22 s. Cards, in order:
Spellwoven (procedural hero), Atanas, Gakev, Nikolay, Pupesh, Ristyo, Iliqn Puh, Daniil & Vasil,
The Storyteller, The Fairy, then "Coming soon" ×2 (disabled, "?").

### Art import rules

- `assets/` has byte-identical duplicates: `mainscreen.jpg` = `menu.jpg`, `coridor.jpg` =
  `corridor.jpg`, `wizardroom.jpg` = `shop.jpg`. Import only one copy of each.
- The JPGs are AI-painted "pixel style" images at about 720 px, with JPEG artifacts. They aren't
  true low-res pixel art. Use **bilinear** filtering, no mipmaps, high-quality or no compression
  for these full-scene backgrounds and portraits, since they're shown at non-integer scales. Use
  **point** filtering only for the procedurally generated 16 px sprites.
- The champion and boss images are **character sheets**: they include titles, ability text and
  several poses. Give every Character/Boss data asset a `portraitRect` (normalised UV crop). Open
  each image and choose a crop around the main figure for in-arena use. The full sheet is used for
  the select-card thumbnail and as a "view sheet" option. Same approach the JS uses for Marin.
- Keep the fallback behaviour: if an image is missing, the game still runs (procedural backdrop,
  coloured-circle boss, procedural hero).

### Procedural art and audio (keep the "everything generated in code" property)

- **PixelArt.js → C#.** Write a small `PixelCanvas` class (a `Color32[]` buffer with
  `FillRect`, `Px`, `FromGrid`, line/ellipse/dither helpers that match the canvas calls used)
  and port `getTile`, `prop`, `flame`, `hero`, `npc` and `monster` line by line into
  `Texture2D` → `Sprite` with `FilterMode.Point`, cached. The pixels must come out identical.
- **Scene fallbacks** (`_drawCorridor`, `_drawShop`, `_drawPathFallback`, `_marinSprite`):
  port them too, on the 320×180 scene buffer, with animated torches and runes.
- **AudioManager.js → `AudioSynth`.** Port the Web Audio synth (the oscillator types, roots,
  arpeggio rates and envelopes for the music moods `mist, temple, ruins, caves, citadel, godreach,
  menu`, and every sfx: `fire, nature, lightning, heal, shield, ultimate, hit, hurt, pickup, quest,
  levelup, ui, death`). Render them into `AudioClip`s at startup with `AudioClip.Create` and PCM
  synthesis, or stream them with `OnAudioFilterRead`. No external audio files. Master music and
  sfx volume plus mute must come from settings.
- **Fonts:** the web version uses Google Fonts *Press Start 2P* and *VT323* (both OFL). **Ask me
  before downloading anything.** Until I say yes, use a built-in font behind one font setting so
  it's a one-line swap later.

### ⛔ CHECKPOINT A (end of Phase 1)

Build `Build/Windows/SpiceTrip.exe`, run all tests, commit, and report to me: what works, the
test results, and anything that differs from the web version, with the reason for each
difference.

## Phase 2: restore the dormant RPG

Port the full RPG as a second mode reached from the main menu ("📜 Chronicle": New / Continue).
Leave the Path flow unchanged. Port it faithfully from the JS:

- **World:** 6 regions (`REGIONS`) with their palettes, darkness, fog, light tint, enemy tables,
  music and spawn points. `TileMap.js` procedural map generation → Unity Tilemap (or a mesh built
  from your generated tiles), with tile collision, prop and pickup placement, and Y-sorting
  (Sorting Group / custom axis sort on Y).
- **Lighting:** the JS darkens the world by `region.darkness`, then cuts light back in for
  braziers, candles, the moon, spells and the player's torch, and adds drifting fog and a
  vignette. In URP 2D: a Global Light 2D at intensity ≈ `1 − darkness` tinted by region, Point
  Light 2D for every light source (torch radius from `Player.torchRadius`, cast flash, spell
  glows), fog as scrolling transparent layers, and a vignette through a post-processing Volume.
- **Player:** walk 74, run 122, roll 210 with i-frames and a cooldown, collision radius 5,
  melee with the godless blade, mouse-aimed casting, HP/mana regen, a shield, levels with
  `xpForLevel(L) = floor(60 · L^1.5)`, gold, inventory, relic passives, death and respawn.
- **Spells:** all 6 (`projectile | beam | self | aoe`) with chaining, slow, splash, heal, shield,
  the ultimate, upgrades, particles, screen shake and lights. **Enemies:** the
  wander → chase → attack state machine, melee and ranged attacks, weakness multipliers, hit flash,
  health bars, the Cinder-Maw boss in a far corner of the Mistwood, a target population of 12,
  and drip respawn every 4–8 s.
- **Quests, NPCs, dialogue:** the quest state machine with `kill | collect | talk | reach | cast`
  objectives, the 5-quest chain, data-driven dialogue with Accept / Maybe later / Hand it over
  choices, quest markers, and the region unlock rules in `_refreshUnlocks`.
- **UI:** HUD bars, the 6-slot hotbar with cooldown sweeps, minimap, floating damage numbers,
  banners and toasts, journal (Tab), satchel (I), tome (K), bestiary (B), map with fast travel
  (M), pause (Esc, and auto-pause on focus loss), settings, save/delete, the death screen, the
  tutorial hints, and the intro cinematic (`IntroScene.js`, skippable on any key or click).

### ⛔ CHECKPOINT B (end of Phase 2)

Same report as Checkpoint A.

## Phase 3: abilities from the art (design first, then build)

The character and boss sheets show mechanics the web version never implemented. **Don't
implement Phase 3 until I approve a design.** First send me a table: name → ability → battle
mechanic → numbers, all data-driven. Here's what the art shows:

- **Good Plazmodium / Bad Plasmodium:** "multiplies ×3 on death; each later form is weaker".
  Form 1 → 3× Form 2 (small and weak). Each Form 2 either becomes 1× Form 3 or is destroyed.
  Form 3 doesn't multiply. The battle must handle several bosses at once, with a combined HP bar.
- **Mordekai:** a bird-man who fires a **Feather Shot** fan.
- **Vlado the Conductor:** rides bus №9, throws **1.50 tickets** and uses a whistle.
- **Champions:** Atanas "The Low Cortisol Monk" (Aura of Calmness: heals and calms); Gakev
  (coin-grabbing thief, runs with a sack of coins); Nikolay (barrel-armoured knight with a mug
  and a mug-shield, "BUZZ"); Pupesh "The Melon Warrior" (melon armour and sword, rolls into a
  melon ball); Ristyo Biristyo (sheet shows "Drunk Level" and "Nicotine Status" meters);
  Iliqn Puh "Mecho the Bear" (honey hose, honey trail, jumping); Daniil & Vasil (a duo with a coin
  blaster and thrown coins, shared "Coin Power" meter); The Storyteller (hot tub, koi, a
  trading-card deck); Mitko the Fairy (wand sparkles, "Dust" resource, Route 66 trailer).
  Spellwoven keeps the default shot.

## Known issues in the source (flag these, don't paper over them)

1. The `asphodel` quest rewards item `'TopChuk'`, which doesn't exist in `ITEMS`. Log a warning
   and skip unknown reward items, and ask me what TopChuk should be.
2. The README contradicts the code (title, "no external assets", Play flow). Write a new README
   for the Unity project.
3. `menu.jpg` has "GOLD: 1450" baked in, while the hub wallet changes. Propose a fix, such as
   covering that area with the live gold value, and ask me before changing how the menu looks.
4. Frame-rate-dependent animation in the Loom and scene loops (fixed `1/60` steps). Make it
   time-based.
5. The selected champion isn't persisted. Persist it.

List anything else you find in `PORTING_NOTES.md`.

## Verification (prove it, don't claim it)

- Zero compile errors and zero new warnings in the batchmode log after every milestone.
- **EditMode tests:** every data asset matches `GameData.js` / `Scenes.js` (counts, ids, every
  number). `xpForLevel`. Quest state transitions. Save round-trip for all three stores. Shop
  purchase and insufficient-funds logic. Loom grid maths (cell hit-testing for 4/8/12/16). Hotspot
  rects match the CSS percentages. `PixelCanvas` output for `hero()` and one tile matches pixel
  hashes you record by running the original JS in Node (stub `canvas` with a tiny pixel buffer).
- **PlayMode tests:** Boot → menu → Path → each boss battle starts with the right HP. A scripted
  win marks the boss defeated and the ✓ shows on return. A scripted loss shows DEFEATED. Flee
  returns to the Path. Corridor → shop fog wipe → buy → gold drops and stash grows. Character select
  persists across a reload. In Phase 2: new game spawns in the Mistwood, quest 1 completes, a save
  and continue restores the position.
- Build the Windows player successfully at each checkpoint.
- Keep `PROGRESS.md` updated: milestones, what's verified and how, open questions for me.
- If something can't be verified headlessly (for example, how it looks), say so plainly in the
  checkpoint report. Don't claim it works.

## Don'ts

- Don't modify the original web repo.
- Don't download assets, fonts or packages beyond the Unity registry packages listed above without
  asking me.
- Don't invent content (new bosses, items, dialogue) outside Phase 3, and Phase 3 needs my
  approval first.
- Don't hand-tune numbers. If a value differs from the JS, write down why in `PORTING_NOTES.md`.
- Don't push to any remote.
