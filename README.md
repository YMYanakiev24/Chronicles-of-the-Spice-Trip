# Chronicles of the Spice Trip (Unity)

A Unity 6 version of the Spice Trip games. The main menu has two:

- **Play:** the browser game in `../Chronicles-of-the-Spice-Trip` (vanilla
  JavaScript + HTML5 Canvas). It has the Path boss map, the four boss fights in
  their painted arenas, and the Citadel hub: the corridor, Marin's Apothecary,
  the Enchanted Loom and the Changing Room. Bosses pay gold bounties and earn
  star ratings and best times. The Gauntlet (all four bosses back to back)
  unlocks once every boss has fallen, and Marin sells battle items you can use
  mid-fight.
- **The Dungeon:** the C++ raylib game from the GitHub repository. You pick a
  champion, fight down through floors of single-screen rooms, and beat the boss
  at the bottom of each one. The four Path bosses come first, each in its own
  painted arena, then Pyraketh, the Cinder-Maw. It also has Marin's Market,
  chests, spike traps, every champion's special on Q, and a timed Boss Rush.

In both games the player is the pixel hero, in their champion's colours. They
walk with a four-direction walk cycle, facing the way they move.

- **Engine:** Unity 6000.6.5f1, URP 2D Renderer, Input System, UI Toolkit
- **Target:** Windows standalone (64-bit)
- **No audio files:** music and sound effects are synthesised at runtime
- **Art:** painted scenes and character sheets in `StreamingAssets/assets`, the
  dungeon sprite pack in `StreamingAssets/assets/dungeon`, and the hero and
  props regenerated in C# from `PixelArt.js`, checked pixel for pixel

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
> unsigned `SpiceTrip.exe` may be blocked from starting. Play in the editor,
> sign the build with a code-signing certificate, or run it on a machine
> without Smart App Control.

## Controls

On the Path and in the hub:

| Input | Action |
| --- | --- |
| Mouse | Click scenes, doors, bosses and shop items; aim in battle |
| W A S D / arrows | Move in battle |
| Left click / Space | Shoot toward the mouse |
| Right click / Q | Your champion's special ability |
| 1 / 2 / 3 | Healing Draught / Swiftwing Elixir / Warding Charm (bought from Marin) |

In the Dungeon:

| Input | Action |
| --- | --- |
| W A S D / arrows | Move (Shift runs) |
| Mouse | Aim |
| Left click | Swing the blade |
| Right click | Cast the selected power |
| 1 / 2 / 3 | Emberhex (fire), Tidelash (water), Stormbolt (lightning) |
| Q | Your champion's special |
| Space | Dodge-roll |
| E | Talk, trade, open chests |
| Esc | Pause (save, controls, main menu) |

The champion picked in the Changing Room or the Dungeon lobby is used in both
games.

### The Path: bounties, stars and the Gauntlet

- **Bounties.** The first win against a boss pays its bounty into Marin's
  wallet; every later win pays a smaller reward.
- **Stars.** Every win is rated one to three stars: one for winning, a second
  for finishing with at least half your health, and a third for doing that
  under the boss's par time. Hover a boss on the Path to see its best rating
  and best time.
- **The Gauntlet.** It opens once all four bosses have fallen. You fight them
  back to back, keeping your health between fights plus 20.
- **Battle items.** You can carry three of each, used with 1, 2 and 3.

The numbers are in `Data/Source/abilities.json` (boss and champion abilities)
and `Data/Source/progression.json` (bounties, the Gauntlet, battle items). Edit
them, then run **Spice Trip ▸ Rebuild Project**.

### The Dungeon

| Floor | Boss | How it fights |
| --- | --- | --- |
| 1 | The Good Plazmodium | 3-shot spreads, then splits into three copies; copies can leave tiny ones |
| 2 | The Bad Plasmodium | The same, with 5-shot spreads and copies that fire 3 |
| 3 | Mordekai | Fans of 7 feathers, and a dive at you every third volley |
| 4 | Vlado | Spirals of 12 tickets, and a whistle that sends bus No.9 down your lane |
| 5 | Pyraketh, the Cinder-Maw | Chases you, breathes rings of embers, and throws fireballs once wounded |

Beating floor 5 completes the Spice Trip. You can keep descending; every cycle
the bosses get 50% stronger. Each champion has a perk and a special on Q:
Arcane Surge, Aura of Calmness, Coin Dash, Mug-Shield, Melon Roll, Drink, Bear
Jump, Coin Barrage, Release the Koi and Blink. Boss Rush (from the lobby)
fights all five back to back at level 5 and keeps your best time. A run can be
saved from the pause menu and continued from the lobby.

The tables (spells, Marin's wares, champions, bosses) are at the top of
`Scripts/Dungeon/DungeonDefs.cs`, and the boss attack numbers are at the top
of `Scripts/Dungeon/BossFight.cs`.

## Replacing the art

Painted scenes and character sheets are plain files in
`Assets/StreamingAssets/assets/` (in a build: `SpiceTrip_Data/StreamingAssets/assets/`).
Replace any of them without rebuilding. Each name is tried as `.png` first,
then `.jpg`. If a scene image is missing, a procedural pixel scene is drawn
instead.

| File | Used for |
| --- | --- |
| `menu.jpg` | Main menu background (its title is part of the art) |
| `path.jpg` | The boss map; hotspots follow each boss's `cx`/`cy` |
| `corridor.jpg` | The stone hall; doors at 9–34% and 65–91% of the width |
| `shop.jpg` | Marin's Apothecary (and the top of the Dungeon's market) |
| `marin.png` | Marin (transparent cut-out) |
| champion and boss sheets | Changing Room and lobby cards, boss sprites and tokens (cropped, see `DataImporter.PortraitCrops`) |
| `arena-<bossId>.jpg` | That boss's fight background, on the Path and in the Dungeon: `arena-goodPlaz`, `arena-badPlaz`, `arena-mordekai`, `arena-vlado` |
| `dungeon/*.png` | The dungeon sprite pack: 16 px tiles, props and animation strips |

## Tests

```
Unity.exe -batchmode -nographics -projectPath . -runTests -testPlatform EditMode -testResults Logs/editmode-results.xml
Unity.exe -batchmode -projectPath . -runTests -testPlatform PlayMode -testResults Logs/playmode-results.xml
```

- **EditMode (99):**
  - The Dungeon (27): floor generation, room layouts, collision and pathfinding, combat and weaknesses, the market, every boss attack, run completion and the stronger cycle, Boss Rush, every champion's special and the perks, and saving and continuing.
  - Bounties, stars and battle items.
  - The 16 ability tests for the Path.
  - Data and pixel parity with the JavaScript.
  - Battle rules, the Loom, saves, hotspots, the synth and the shop.
- **PlayMode (8):**
  - Boot, every boss fight, winning and losing, and the full Gauntlet.
  - The fog wipe into the shop, buying, the Loom and champion persistence.
  - A Dungeon run from the lobby to a beaten boss and back to the menu.

**End-to-end autopilot:** this plays through every screen with real injected
mouse and keyboard events, saves screenshots and writes a PASS/FAIL log (97
checks: the Path and hub, then a full Dungeon run and a Boss Rush). It runs in
the editor's Play mode:

```
Unity.exe -projectPath . -executeMethod SpiceTrip.EditorTools.AutopilotRunner.Run -autopilot Captures/autopilot -savedir Captures/autopilot/save
```

In a build, run `SpiceTrip.exe -autopilot <dir> -savedir <dir>/save`. The
autopilot empties its `-savedir` before it starts, so always give it a folder
of its own.

## Where the data comes from

`Data/Source/source-data.json` and `pixel-hashes.json` are exported from the
web game's JavaScript:

```
node Tools~/export-source-data.mjs ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/source-data.json
node Tools~/pixel-parity.mjs       ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/pixel-hashes.json
```

`abilities.json` and `progression.json` are written by hand, because they hold
new content. Then run **Spice Trip ▸ Rebuild Project**. It regenerates the
`GameDatabase` asset, the UI panel settings, the sprite materials and the Boot
scene, so nothing in the project needs to be put together by hand.

## Layout

```
Assets/_Project/
  Scripts/Core/      GameRoot (bootstrap), saves, RNG, audio synth, autopilot
  Scripts/Data/      GameDatabase + tuning (generated from the JS)
  Scripts/Art/       PixelCanvas, PixelArt (port of PixelArt.js), HeroSprites, icons, art loading
  Scripts/Hub/       menu, Path, corridor, shop, Loom, Changing Room, navigation
  Scripts/Battle/    BattleSim (rules) + BossBattleView (rendering and input),
                     Progression (stars, bounties, best times)
  Scripts/Dungeon/   the Dungeon: DungeonDefs (tables), Rooms (floors, collision,
                     pathfinding), Actors, BossFight, DungeonGame (rules), DungeonSave,
                     DungeonArt, DungeonView (renderer), DungeonHud (HUD, lobby, panels),
                     DungeonController
  Shaders/           unlit sprite shader (alpha / additive / screen blending)
  Resources/UI/      Spice.uss (port of style.css), Dungeon.uss, panel settings, theme
  Editor/            ProjectBuilder, DataImporter, BuildScript, AutopilotRunner
  Tests/             EditMode + PlayMode
Assets/StreamingAssets/assets/   painted art and the dungeon sprite pack (replaceable)
Tools~/              Node exporters that read the web game's JS
```

See `PORTING_NOTES.md` for every place the port differs from the originals and
why, and `PROGRESS.md` for status and open questions.
