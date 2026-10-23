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

## Phase 2: the Chronicle (restored RPG) ✅ (Checkpoint B, 2026-10-23)

| Area | Status | Verified by |
| --- | --- | --- |
| RPG data (gods, regions, spells, creatures, quests, NPCs) | ✅ | Imported from `source-data.json` |
| Map generation for all six regions | ✅ | `MapParityTests`: walls, props, pickups and lights hash-identical to `TileMap.js` |
| Player: walk/run, collision and sliding, roll, blade, casting, regen, levelling | ✅ | `ChronicleRulesTests` + autopilot (real keys) |
| Spells: projectiles, chaining beam, heal, ward, ultimate, upgrades, lights | ✅ | Tests (weakness one-shot, ward absorb) + screenshots |
| Enemies: wander/chase/attack, melee + ranged, weakness, hit flash, bars, Cinder-Maw, drip respawn | ✅ | Tests + screenshots |
| Quests, NPC markers, data-driven dialogue, rewards, chaining | ✅ | Tests + autopilot (E to talk, dialogue buttons, hand-in) |
| Darkness, light pools, fog, glows, vignette (original maths) | ✅ | Screenshots of the Mistwood, the Underway and the Pale Citadel |
| HUD: vitals, hotbar + cooldowns, minimap, tracker, toasts, banners, damage numbers, hint, vignette, shake | ✅ | Screenshots |
| Book: Satchel (use items), Spellbook, Bestiary, World Map (travel), Journal (track), Settings, Pause | ✅ | Autopilot opens each with its hotkey + screenshots |
| Death screen and respawn | ✅ | Tests + autopilot |
| Saves: save, quit to title, continue (position, stats, quests, regions, bestiary) | ✅ | Save round-trip test, PlayMode test, autopilot |
| Region unlocks and fast travel | ✅ | Tests + autopilot through the World Map |
| Intro cinematic + title card (skippable) | ✅ | Autopilot + screenshot |
| Marin's stash handed to a new Chronicle | ✅ | PlayMode + autopilot |

**Test results:** EditMode 61/61, PlayMode 7/7, autopilot 66/66 checks (hub +
Chronicle, real input in the editor's Play mode). Windows build: 0 errors, 0
warnings.

**Not verified:**
- **Sound** was not listened to; it is still checked numerically only.
- **The built `.exe`** still can't be launched on this PC (Smart App Control).
- **Balance over a full playthrough.** The tests cover each system and the
  first quest end to end, but nobody has played all five quests and six realms
  by hand.

## Phase 3: abilities from the art (waiting on a design approval)

## Open questions for you

1. What is the `TopChuk` quest reward?
2. Should the menu art's painted "GOLD: 1450" be covered with the live gold?
3. Which name: *Chronicles of the Spice Trip* or *The Mythic Chronicles*?
4. May I download the OFL fonts Press Start 2P and VT323 from Google Fonts?
5. Smart App Control: do you want to sign builds, play in the editor, or run
   builds on another machine?
