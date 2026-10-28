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

## Phase 2 (the Chronicle): removed

The restored RPG was removed on 2026-10-28 at your request, with its data,
procedural tiles, monsters and NPCs, tests and realm quests. Notes 14–24 that
described it are in the git history.

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

## More things to do (new content)

None of this is in the web game. It was added on request, after Phase 3.

- **Where the gold goes.** Bounties and battle items use the hub wallet
  (Marin's gold), the same one the Apothecary already used.
- **Stars.** One for the win, one for ending with half your health or more, and
  one for that plus beating par. Being fast but badly hurt earns one star, so
  speed can't stand in for skill. The best rating and best time are kept
  separately, so they can come from different wins.
- **The Gauntlet** uses the four fights as they are, kits included. Between
  fights the player keeps their health and gets 20 back. The score is the total
  fight time, not counting the pauses between stages. Fleeing ends the run with
  nothing recorded.
- **Battle items** are kept in `hub.json` (`battleSatchel`). A Healing Draught isn't used up at full health. A ward
  absorbs any hit, including contact and bus №9.
## The Dungeon (the C++ game): differences and why

- **One game, two modes.** The Dungeon is the second entry on the main menu.
  Its lobby picks the champion, and the choice is shared with the Changing
  Room. Its gold is its own, like the C++ game's.
- **The UI uses the Unity game's look.** The HUD, lobby and panels use UI
  Toolkit with the same serif font, gold edges and violet panels as the rest of
  the game, instead of raylib's pixel font. The layout follows the C++ HUD.
- **The art is shared.** The dungeon sprite pack is copied into
  `StreamingAssets/assets/dungeon`. The painted arenas and boss sheets are the
  same files the Path uses, so replacing an arena changes both games.
- **The hero walks.** The C++ hero is a block with a sword. Here it is the pixel
  hero from `PixelArt.js`, facing the way it walks, in the champion's colours.
- **Saves are JSON** (`dungeon.json` and `dungeon-profile.json` next to the
  other saves) instead of raw binary structs.
- **The rules are a separate class** (`DungeonGame`), so the EditMode tests
  play it without rendering, with a seeded random generator.

## The player's look on the Path

The web game drew the champion's portrait card as the player. The arena now
uses the walking pixel hero in the champion's colours, so the player faces the
way they move. The portraits are still shown in the Changing Room and the
Dungeon lobby.

## Problems found in the web source

1. **`TopChuk`:** a Chronicle quest rewarded an item that doesn't exist. The
   Chronicle has been removed, so this no longer matters.
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
   entry, so the web game today is only the Path and the hub. Phase 2 restored
   it as a separate menu mode; it has since been removed again.
