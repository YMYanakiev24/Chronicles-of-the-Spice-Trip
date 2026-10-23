# Progress

## Phase 1: faithful port of the live game ✅ (Checkpoint A, 2026-10-23)

| Area | Status | Verified by |
| --- | --- | --- |
| Project setup (URP 2D, Input System, UI Toolkit, built from code) | ✅ | `ProjectBuilder.RebuildAll` in batchmode |
| Data from the JS (items, bosses, champions, shop, tips, hub gold) | ✅ | `DataParityTests` against `source-data.json` |
| Procedural pixel art (`PixelArt.js` → C#) | ✅ | `PixelArtParityTests`: 75/75 sprites hash-identical to the JS |
| RNG and hash (mulberry32, FNV-1a) | ✅ | `RngTests` against values from Node |
| Synthesised music and SFX | ✅ | `SynthTests`; menu/battle music switching in PlayMode |
| Loading screen and main menu (castle + Play) | ✅ | Autopilot (real clicks) + screenshot |
| The Path: boss hotspots, castle, defeated ticks | ✅ | Autopilot + `HubControllerTests` |
| Boss battle: movement, shooting, spread, i-frames, contact, win/lose, Flee, Fight again | ✅ | `BattleSimTests` (9), PlayMode, autopilot |
| Corridor, fog wipe, Marin's shop, buying, too-poor line | ✅ | Autopilot, PlayMode, `HubControllerTests` |
| Enchanted Loom: weave, 4/8/12/16 grids, tear, Tear All, Re-weave | ✅ | `LoomModelTests`, PlayMode, autopilot |
| Loom "Upload image" file dialog | ⚠️ written, not exercised | Editor uses `OpenFilePanel`; build uses comdlg32. Opening a native dialog can't be automated headlessly; loading a file from a path is tested |
| Changing Room and saved champion | ✅ | Autopilot, PlayMode reload test |
| Saves (`hub.json`, `settings.json`) | ✅ | `SaveServiceTests` |
| Windows build | ✅ builds, 0 errors / 0 warnings | `BuildScript.BuildWindows`; **can't be launched on this PC** (Smart App Control blocks unsigned exes) |

**Test results:** EditMode 44/44, PlayMode 6/6, autopilot 32/32 checks (Unity
Editor Play mode, real mouse and keyboard events through the Input System into
UI Toolkit).

**Not verified:**
- **Sound** was not listened to. It is synthesised and checked numerically
  (finite, audible levels, fade in/out), and the "no audio listener" problem
  was fixed.
- **The built `.exe` was not launched** because of Smart App Control. The same
  code was exercised in the editor's Play mode.

## Phase 2: restore the dormant RPG (not started)

Regions and tilemaps, lighting and fog, player/enemy/NPC, spells, quests and
dialogue, the full HUD and panels, and the intro cinematic. The data for all
of it is already exported in `source-data.json`, and its procedural sprites
(tiles, props, NPCs, monsters) already pass the pixel-parity test.

## Phase 3: abilities from the art (waiting on a design approval)

## Open questions for you

1. What is the `TopChuk` quest reward?
2. Should the menu art's painted "GOLD: 1450" be covered with the live gold?
3. Which name: *Chronicles of the Spice Trip* or *The Mythic Chronicles*?
4. May I download the OFL fonts Press Start 2P and VT323 from Google Fonts?
5. Smart App Control: do you want to sign builds, play in the editor, or run
   builds on another machine?
