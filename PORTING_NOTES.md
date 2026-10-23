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
