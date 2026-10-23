# Porting notes

Everything here is a place where the Unity version knowingly differs from the
web game, or a problem found in the web source. Tuning numbers are unchanged
unless they're listed below.

## Differences from the web version, and why

1. **Clicking to shoot actually works.** The battle hint says "aim & click (or
   Space) to shoot", but in the web version clicks never registered. `Input.js`
   listens for `mousedown` on the main game canvas, and the boss-battle overlay
   sits on top of it, so only Space fired. In Unity, left click fires as the
   hint promises. Clicking the Flee button does not also fire a shot.
2. **Champion and boss sprites are cropped.** The web version drew each whole
   character sheet (titles, ability text, pose strips) squashed into a square.
   Each sheet now has a hand-picked `portraitRect` around its main figure
   (`DataImporter.PortraitCrops`). The Changing Room cards still show the full
   sheet, as before.
3. **The selected champion is saved** (`hub.json` → `selectedChar`). The web
   version forgot it on reload.
4. **Loom physics are time-based.** The web version advanced shards by a fixed
   1/60 s per animation frame, so they fell faster on high-refresh screens.
   Gravity, spin and fade now use real elapsed time.
5. **The Loom canvas shrinks to fit short windows.** The web box scrolled when
   it was taller than 92% of the viewport. Here the canvas gets smaller instead
   (at most 512 units), so the whole tapestry and its controls stay visible.
6. **Emoji became pixel icons.** 🍯 🏺 🍮 🔥 🧵 🪙 🏰 ⚔ ✓ ✕ ⟵ ▶ ✦ ❖ 📷 🧙 are drawn
   as small pixel-art icons (`Icons.cs`). Unity UI text can't be relied on to
   render colour emoji, and the icons match the game's style.
7. **Fonts.** `style.css` asks for Cinzel and EB Garamond, but `index.html`
   never loads them, so browsers fell back to Georgia. The Unity UI uses the
   system Georgia (`UiFonts`). Press Start 2P and VT323 (door labels, boss
   tooltips, gold and prices) haven't been downloaded yet, pending your
   approval. Those labels use the same serif for now; swapping is one setting.
8. **CSS effects USS can't do.** USS has no box-shadow, gradients,
   `backdrop-filter`, dashed borders or `gap`. Glows became border colour and
   a faint background tint. Gradients (fog wipe, shop panel fade, checkerboard)
   are generated textures. The blur behind the fog and the modals is omitted.
   The Path castle's dashed border is a faint solid line. Marin's drop shadow
   is omitted.
9. **Art loads from `StreamingAssets/assets`, not imported textures.** This
   keeps the web version's documented drop-in feature: replace an image file
   and it's used without a rebuild, with `.png` then `.jpg` fallback and a
   procedural scene if it's missing. Images are decoded uncompressed, with
   bilinear filtering and no mipmaps.
10. **Main menu without its art.** If `menu.jpg` is missing, the menu shows its
    text title on a dark screen. The web version drew the procedural
    `IntroScene` behind it; that scene is part of Phase 2.
11. **Colour space.** The project uses Linear colour (the URP template
    default), and browsers blend in sRGB. Additive glows and fog look very
    slightly different. Opaque colours are identical.
12. **Sound effects with noise** (fire, lightning, hit, ultimate) are rendered
    once at startup, so each play uses the same noise burst. Web Audio made new
    noise every time. It's barely audible; say if you want variants.
13. **No AudioListener in the scene.** GameRoot adds one at runtime. This was
    caught by the end-to-end run's log; without it the game would have been
    silent.

## Phase 2 (the Chronicle): differences and why

14. **The RPG has its own menu entries.** The web game's Play goes to the
    Path, and the RPG can't be reached. It's restored as **New Chronicle** and
    **Continue Chronicle** on the main menu, and Play is unchanged. Marin's hub
    wallet stays separate from the Chronicle's gold, as it was in the web game
    whenever no RPG run was active. Shop purchases are handed over when a
    Chronicle starts, with the original toast.
15. **The intro cinematic plays when a new Chronicle starts.** The web build
    skipped `IntroScene` at boot. It's skippable on any key or click, and moves
    on by itself after 26 seconds, as before.
16. **Lighting reproduces the original's maths instead of using URP Light2D.**
    The web game darkens the view with `rgba(5,6,14,darkness)`, cuts light
    pools out with `destination-out` radial gradients (1 → 0.7 at 60% → 0),
    then adds coloured glows and a vignette. URP's 2D lights multiply instead
    and use different falloff curves, and the screen-blended fog would need a
    custom lit shader. So the darkness layer is computed on the CPU at half the
    virtual resolution, with the same formula, and drawn over the scene. Glows,
    fog and vignette use the same blend modes as the canvas.
17. **The paint order is kept exactly.** The renderer is immediate-mode
    (`IWorldPainter`): each draw call gets the next sorting order. The original
    y-sorted draw() methods are ported line by line rather than rewritten as
    scene objects.
18. **Pixel scale follows `Game._resize`.** It's an integer scale from the
    window height (base 270, clamped 2–5), and the virtual screen fills the
    window at that scale. The Pixel Perfect Camera component isn't used,
    because it picks its zoom differently (it never rounds up), so it would
    show a different area at some window sizes.
19. **Timers run on game time.** `setTimeout` calls (the tutorial hint, the
    staggered Stormcrown's Wrath strikes and its Reckoning repeat) now pause
    with the game while a panel or dialogue is open.
20. **Invulnerability blink after rolling.** In `Player.draw`, the blink test
    is `!this.rolling`. `rolling` goes slightly negative after the first roll
    and never returns to 0, so the player never blinked again after rolling
    once. The port treats "not rolling" as `rolling <= 0`.
21. **World Map orbs.** `openMap` reads `r.sky.top`, which regions don't have,
    so the web orbs had no colour. Each region's light tint is used instead.
22. **Unknown quest rewards are skipped with a warning** (see `TopChuk` below).
    In the web game the item went into the satchel, and opening the satchel
    then threw an error, because `ITEMS['TopChuk']` is undefined.
23. **Emoji in the RPG UI** (school icons 🔥🌊⚡🌿🛡️☀️, book tabs, ✔/◇ in the
    tracker) are pixel icons, as in Phase 1. Quest markers (! ? ✔) above NPCs
    are drawn as pixel glyphs.
24. **Settings that do nothing in the web game** ("Shadows", "Mouse
    Sensitivity") are kept and saved, and still do nothing, as before.
    `Game.applyQuality` was empty in the web version too.

## Phase 3: abilities from the art (approved design)

New content, not in the web game. It was built from the approved design table:

- **Bosses.** The two Plasmodia multiply ×3 on death into small, fast copies
  (25 HP). Each copy has a 40% chance to leave a tiny copy (10 HP, touch damage
  only), and one health bar covers every body. The Bad Plasmodium fires a
  5-shot spread and its copies fire 3-shot spreads; the Good Plazmodium's
  copies fire single shots. Mordekai fires a fan of 7 feathers every 1.6 s and
  dives every third volley. Vlado throws 12-ticket spirals, and every 8 s his
  whistle sends bus №9 down the player's lane after a 1 s warning (20 damage).
- **Champions.** Right-click or Q triggers each champion's special, alongside
  their passives and meters, exactly as in the table. Spellwoven keeps the
  original shot and has no special.
- **Data.** Every number is in `Data/Source/abilities.json`.
- **Unchanged without a kit.** A fight with no kit is still the original, and
  the Phase 1 battle tests still pass unchanged.
- **New sound.** Vlado's whistle is a new synthesised effect (two sharp
  blasts).

## Problems found in the web source

1. **`TopChuk`:** the `asphodel` quest rewards item `'TopChuk'`, which doesn't
   exist in `ITEMS`. Quests are Phase 2. The port will log a warning and skip
   unknown reward items. *What should TopChuk be?*
2. **The README is out of date.** It describes the RPG as the main game and
   says there are no external assets, but Play goes to the Path and the
   painted art is in `assets/`. This project has its own README.
3. **"GOLD: 1450" is painted into `menu.jpg`**, while the real wallet changes
   as you shop. It's unchanged for now. A possible fix is to cover that corner
   with the live gold value. *Do you want that?*
4. **Three names.** The menu art says "Chronicles of the Spice Trip", the page
   title and loading screen say "The Mythic Chronicles", and the subtitle is
   "The Hidden World". The Unity product name is *Chronicles of the Spice
   Trip*, and the loading screen keeps the web text. *Which name should win?*
5. **Byte-identical duplicate images** (`mainscreen.jpg` = `menu.jpg`,
   `coridor.jpg` = `corridor.jpg`, `wizardroom.jpg` = `shop.jpg`). Only one
   copy of each was brought over.
6. **The RPG can't be reached.** `startNewGame` / `continueGame` have no menu
   entry, so the web game today is only the Path and the hub. Phase 2 restores
   the RPG as a separate menu mode.
