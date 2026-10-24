# Chronicles of the Spice Trip (Unity)

A Unity 6 port of the browser game in `../Chronicles-of-the-Spice-Trip`
(vanilla JavaScript + HTML5 Canvas).

- **Play** (the game the web version shows today): the Path boss map, the four
  boss fights, and the Citadel hub (corridor, Marin's Apothecary, the Enchanted
  Loom, the Changing Room).
- **New / Continue Chronicle** (restored in Phase 2): the full top-down RPG that
  was in the web code but unreachable. It has six realms, six divine powers,
  monsters, NPCs and dialogue, a five-quest chain, the HUD and the book panels,
  saves, fast travel and the intro cinematic.
- **More to do** (added after Phase 3): gold bounties, star ratings and best
  times for every boss; the Gauntlet (all four bosses back to back); battle
  items from Marin you can use mid-fight; and a quest-giver with a new quest in
  each of the five realms beyond the Mistwood.

- **Engine:** Unity 6000.6.5f1, URP 2D Renderer, Input System, UI Toolkit
- **Target:** Windows standalone (64-bit)
- **No audio files:** music and sound effects are synthesised at runtime, as in the web game
- **Procedural art:** the pixel sprites from `PixelArt.js` are regenerated in C# and checked pixel-for-pixel against the JavaScript

## Running it

Open the project in Unity 6000.6.5f1, open `Assets/_Project/Scenes/Boot.unity`
and press Play.

To make a Windows build, use **Spice Trip ▸ Build Windows** in the editor, or
run it from the command line:

```
Unity.exe -batchmode -quit -projectPath . -executeMethod SpiceTrip.EditorTools.BuildScript.BuildWindows
```

The build goes to `Build/Windows/SpiceTrip.exe`.

> **Smart App Control:** on a PC with Windows Smart App Control turned on, the
> unsigned `SpiceTrip.exe` is blocked from starting. Play in the editor, sign
> the build with a code-signing certificate, or run it on a machine without
> Smart App Control.

## Controls

| Input | Action |
| --- | --- |
| Mouse | Click scenes, doors, bosses and shop items; aim in battle |
| W A S D / arrows | Move in battle |
| Left click / Space | Shoot toward the mouse |
| Right click / Q | Your champion's special ability (Phase 3) |
| 1 / 2 / 3 | Healing Draught / Swiftwing Elixir / Warding Charm (bought from Marin) |

Each champion and boss has an ability designed from their character sheet.
All the numbers live in `Assets/_Project/Data/Source/abilities.json`. Edit it,
then run **Spice Trip ▸ Rebuild Project**.

### Bounties, stars and the Gauntlet

- **Bounties.** The first win against a boss pays its bounty into Marin's
  wallet; every later win pays a smaller reward.
- **Stars.** Every win is rated one to three stars: one for winning, a second
  for finishing with at least half your health, and a third for doing that
  under the boss's par time. The Path shows each boss's best rating and best
  time when you hover it.
- **The Gauntlet.** It opens once all four bosses have fallen. You fight them
  back to back, keeping your health between fights plus 20. It has its own
  bounty, par time and best time.
- **Battle items.** Marin sells battle items alongside her usual wares. You can
  carry three of each, and you use them with 1, 2 and 3 in any boss fight.

All the numbers are in `Assets/_Project/Data/Source/progression.json`.

In a Chronicle:

| Input | Action |
| --- | --- |
| W A S D / arrows | Move (Shift runs) |
| Mouse | Aim spells and the blade |
| 1 – 6 | Cast the power in that slot |
| Left click / Right click | Godless blade / cast slot 1 |
| Space | Dodge-roll |
| E | Talk to an NPC (pickups collect themselves) |
| Tab / I / K / B / M | Journal / Satchel / Spellbook / Bestiary / World Map |
| Esc | Pause, or close a panel or dialogue |

## Replacing the art

Painted scenes and character sheets are plain files in
`Assets/StreamingAssets/assets/` (in a build: `SpiceTrip_Data/StreamingAssets/assets/`).
Replace any of them without rebuilding, as with the web version's `assets/`
folder. Each name is tried as `.png` first, then `.jpg`. If a scene image is
missing, the procedural pixel scene is drawn instead.

| File | Used for |
| --- | --- |
| `menu.jpg` | Main menu background (its title is part of the art) |
| `path.jpg` | The boss map; hotspots follow each boss's `cx`/`cy` |
| `corridor.jpg` | The stone hall; doors at 9–34% and 65–91% of the width |
| `shop.jpg` | Marin's Apothecary |
| `marin.png` | Marin's portrait (transparent cut-out, top 64% shown) |
| champion and boss sheets | Changing Room cards and battle sprites (cropped, see `DataImporter.PortraitCrops`) |
| `arena-<bossId>.jpg` | That boss's fight background, cover-fitted to the 16:9 arena: `arena-mordekai`, `arena-vlado`, `arena-badPlaz`, `arena-goodPlaz`. Without one, the fight uses the original moonlit forest. |

## Tests

```
Unity.exe -batchmode -nographics -projectPath . -runTests -testPlatform EditMode -testResults Logs/editmode-results.xml
Unity.exe -batchmode -projectPath . -runTests -testPlatform PlayMode -testResults Logs/playmode-results.xml
```

- **EditMode tests (95):** the 18 progression tests (star ratings, bounties, each battle item, Marin's carry limit, the Gauntlet unlock, every realm's quest-giver, scattered quest items, The Last Rite, the hunt quests), the 16 Phase 3 ability tests (one per boss and champion kit), plus data parity with the JS, pixel parity of all 75 procedural sprites, map parity for all six regions, RNG/hash bit-exactness, battle rules, Loom grid and physics, saves, hotspot geometry, synth output, shop/navigation logic, and the Chronicle rules (movement, collision, casting, weaknesses, melee, levelling, wards, death, quests and dialogue, pickups, unlocks, travel, consumables, save round-trip, enemy AI).
- **PlayMode tests (8):** boot, every boss fight, win/lose, the full Gauntlet and its bounty, the fog wipe into the shop, buying, the Loom, champion persistence, and a Chronicle started, saved, quit and continued.

**End-to-end autopilot:** this plays through every screen with real injected
mouse and keyboard events, saves screenshots and writes a PASS/FAIL log. It
runs in the editor's Play mode, so it also works where unsigned builds are
blocked:

```
Unity.exe -projectPath . -executeMethod SpiceTrip.EditorTools.AutopilotRunner.Run -autopilot Captures/autopilot -savedir Captures/autopilot/save
```

In a build, run `SpiceTrip.exe -autopilot <dir> -savedir <dir>/save`. The
autopilot empties its `-savedir` before it starts, so always give it a folder
of its own.

## Where the data comes from

`Assets/_Project/Data/Source/source-data.json` and `pixel-hashes.json` are
exported from the web game's JavaScript:

```
node Tools~/export-source-data.mjs ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/source-data.json
node Tools~/pixel-parity.mjs       ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/pixel-hashes.json
node Tools~/map-parity.mjs         ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/map-signatures.json
```

Three files are written by hand rather than exported, because they hold new
content: `abilities.json` (Phase 3 kits), `progression.json` (bounties, the
Gauntlet, battle items) and `chronicle-extra.json` (the five realm quests,
their NPCs and quest items). The importer merges `chronicle-extra.json` into
the exported RPG data.

Then run **Spice Trip ▸ Rebuild Project**. It regenerates the `GameDatabase`
asset, the UI panel settings, the sprite materials and the Boot scene, so
nothing in the project needs to be put together by hand.

## Layout

```
Assets/_Project/
  Scripts/Core/      GameRoot (bootstrap), saves, RNG, audio synth, autopilot
  Scripts/Data/      GameDatabase + tuning (generated from the JS)
  Scripts/Art/       PixelCanvas, PixelArt (port of PixelArt.js), icons, art loading
  Scripts/Hub/       menu, Path, corridor, shop, Loom, Changing Room, navigation
  Scripts/Battle/    BattleSim (rules) + BossBattleView (rendering and input),
                     Progression (stars, bounties, best times)
  Scripts/Rpg/       the Chronicle: ChronicleGame (Game.js), TileMap, Player/Enemy/Npc,
                     Magic/Combat/Quest systems, RpgView (renderer), RpgHud, BookPanels,
                     IntroCinematic, ChronicleController, ChronicleSave
  Shaders/           unlit sprite shader (alpha / additive / screen blending)
  Resources/UI/      Spice.uss (port of style.css), panel settings, theme
  Editor/            ProjectBuilder, DataImporter, BuildScript, AutopilotRunner
  Tests/             EditMode + PlayMode
Assets/StreamingAssets/assets/   painted art (replaceable)
Tools~/              Node exporters that read the web game's JS
```

See `PORTING_NOTES.md` for every place the port differs from the web version
and why, and `PROGRESS.md` for status and open questions.
