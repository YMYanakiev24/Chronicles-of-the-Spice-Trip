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

## Phase 2: the Chronicle (restored RPG): removed on 2026-10-28

The restored RPG (New / Continue Chronicle) was built and tested on
2026-10-23. It was removed at your request, together with its data, art,
tests and the realm quests added for it. It is still in the git history (commit
`d430755` and the later Phase 3 and "More things to do" commits).

## Phase 3: abilities from the art ✅ (built 2026-10-23, design approved)

| Area | Status | Verified by |
| --- | --- | --- |
| Plasmodia split ×3, tiny copies at 40%, one shared health bar, win when all gone | ✅ | `AbilityTests` + autopilot screenshot |
| Bad Plasmodium 5-shot / copies 3-shot; Good copies single shots | ✅ | `AbilityTests` |
| Mordekai: 7-feather fan every 1.6 s, dive every third volley | ✅ | `AbilityTests` |
| Vlado: 12-ticket spirals, whistle, bus №9 down your lane (20 damage) | ✅ | `AbilityTests` + autopilot screenshot |
| All ten champion kits (Spellwoven, Atanas, Gakev, Nikolay, Pupesh, Ristyo, Iliqn Puh, Daniil & Vasil, The Storyteller, Mitko) | ✅ | One `AbilityTests` test each; Atanas's right-click checked end to end with a real mouse click |
| Ability HUD (cooldown/meter bars) and hint text | ✅ | Screenshot |

**Test results:** EditMode 77/77, PlayMode 7/7, autopilot 71/71 checks.

**Not verified:** how the new fights feel to play. The numbers come straight
from the design table; tune them in `abilities.json` after playing.

## More things to do ✅ (built 2026-10-24)

| Area | Status | Verified by |
| --- | --- | --- |
| Boss bounties (first win / repeat), 1–3 star ratings, best times on the Path | ✅ | `BountyTests`, autopilot (bounty paid, Path shows the ratings) + screenshots |
| Marin's battle wares (Healing Draught, Swiftwing Elixir, Warding Charm), carry limit 3 each, keys 1–3 in fights | ✅ | `BattleItemTests`, `MarinBattleWaresTests`, autopilot (buys a draught, drinks it with a real key 1 press) |
| The Gauntlet: unlocks after all four, back to back, stage banners, health carried over, own bounty and best time | ✅ | `MarinBattleWaresTests`, PlayMode `TheGauntletRunsAllFourBossesAndPaysOut`, autopilot + screenshots |
| A quest in each realm beyond the Mistwood | removed with the Chronicle | |
| The autopilot empties its own `-savedir` first, so "first win" checks hold on every run | ✅ | Passes when run over the previous run's save |

**Test results:** EditMode 95/95, PlayMode 8/8, autopilot 84/84 checks.
Windows build: 0 errors, 0 warnings.

**Not verified:** the new prices, bounties and par times haven't been
play-tested by hand. They are all in `progression.json`, ready to tune.

## The Dungeon (the C++ game) ✅ (built 2026-10-28)

The C++ raylib game from the GitHub repo (`boss-fights` branch), ported as a
second game on the main menu.

| Area | Status | Verified by |
| --- | --- | --- |
| Floors: rooms joined by doors, the start room, Marin's Market in a dead end, the boss farthest away; room layouts, spike traps, chests | ✅ | `DungeonTests` (40 seeded floors, 200 layouts), autopilot |
| Movement, wall sliding, doors barred until a room is cleared, flow-field pathfinding | ✅ | `DungeonTests`, autopilot (real D key) |
| The blade, the three powers, weaknesses (x1.6), the five monster kinds, loot, levelling | ✅ | `DungeonTests`, autopilot (spells clear a room) |
| The five bosses: splitting Plasmodia, Mordekai's feathers and dive, Vlado's tickets and bus No.9, the Cinder-Maw's embers; painted arenas; round tokens; boss bar | ✅ | `DungeonTests` (each attack), autopilot + screenshots of every boss |
| Completing the Spice Trip, then descending with stronger bosses | ✅ | `DungeonTests`, autopilot |
| Ten champions: perks and Q specials | ✅ | One `DungeonTests` case each; Aura of Calmness with a real Q press |
| Marin's Market, the oracle, pause / save / continue, records | ✅ | `DungeonTests`, PlayMode, autopilot |
| Boss Rush with a best time | ✅ | `DungeonTests`, autopilot |

## Changes on 2026-10-28

| Change | Verified by |
| --- | --- |
| No squares when hovering the castle, the corridor doors, the bosses on the Path or the Path's castle (the labels and the boss tooltip stay) | Screenshot of the Path with a boss hovered |
| The player is the pixel hero in their champion's colours, walking in four directions, in the boss arena and in the Dungeon | Screenshots |
| The Chronicle removed (see Phase 2 above) | All tests and the autopilot pass without it |

**Test results:** EditMode 99/99, PlayMode 8/8, autopilot 97/97 checks.

**Not verified:** nobody has played the Dungeon by hand in Unity, so its
balance and feel are untested. No new Windows build was made.

## Open questions for you

1. Should the menu art's painted "GOLD: 1450" be covered with the live gold?
2. Which name: *Chronicles of the Spice Trip* or *The Mythic Chronicles*?
3. May I download the OFL fonts Press Start 2P and VT323 from Google Fonts?
4. Smart App Control: do you want to sign builds, play in the editor, or run
   builds on another machine?
