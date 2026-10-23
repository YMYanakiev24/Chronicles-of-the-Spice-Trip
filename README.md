# Chronicles of the Spice Trip (Unity)

A Unity 6 port of the browser game in `../Chronicles-of-the-Spice-Trip`
(vanilla JavaScript + HTML5 Canvas). Phase 1 ports the game that is playable
today: the main menu, the Path boss map, the four boss fights, and the Citadel
hub (corridor, Marin's Apothecary, the Enchanted Loom and the Changing Room).

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

## Tests

```
Unity.exe -batchmode -nographics -projectPath . -runTests -testPlatform EditMode -testResults Logs/editmode-results.xml
Unity.exe -batchmode -projectPath . -runTests -testPlatform PlayMode -testResults Logs/playmode-results.xml
```

- **EditMode tests (44):** data parity with the JS, pixel parity of all 75 procedural sprites, RNG/hash bit-exactness, battle rules, Loom grid and physics, saves, hotspot geometry, synth output, and shop/navigation logic.
- **PlayMode tests (6):** boot, every boss fight, win/lose, the fog wipe into the shop, buying, the Loom, and champion persistence across a reload.

**End-to-end autopilot:** this plays through every screen with real injected
mouse and keyboard events, saves screenshots and writes a PASS/FAIL log. It
runs in the editor's Play mode, so it also works where unsigned builds are
blocked:

```
Unity.exe -projectPath . -executeMethod SpiceTrip.EditorTools.AutopilotRunner.Run -autopilot Captures/autopilot -savedir Captures/autopilot/save
```

In a build, run `SpiceTrip.exe -autopilot <dir> -savedir <dir>/save`.

## Where the data comes from

`Assets/_Project/Data/Source/source-data.json` and `pixel-hashes.json` are
exported from the web game's JavaScript:

```
node Tools~/export-source-data.mjs ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/source-data.json
node Tools~/pixel-parity.mjs       ../Chronicles-of-the-Spice-Trip Assets/_Project/Data/Source/pixel-hashes.json
```

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
  Scripts/Battle/    BattleSim (rules) + BossBattleView (rendering and input)
  Shaders/           unlit sprite shader (alpha / additive / screen blending)
  Resources/UI/      Spice.uss (port of style.css), panel settings, theme
  Editor/            ProjectBuilder, DataImporter, BuildScript, AutopilotRunner
  Tests/             EditMode + PlayMode
Assets/StreamingAssets/assets/   painted art (replaceable)
Tools~/              Node exporters that read the web game's JS
```

See `PORTING_NOTES.md` for every place the port differs from the web version
and why, and `PROGRESS.md` for status and open questions.
