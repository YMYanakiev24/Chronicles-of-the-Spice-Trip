# Chronicles of the Spice Trip

A dark pixel-art dungeon crawler written in C++ with [raylib](https://www.raylib.com/).
Pick a champion, fight down through randomly built floors of the Mistwood
Depths, and beat the boss at the bottom of each one: the four bosses from the
Path in their painted arenas, then Pyraketh, the Cinder-Maw.

## Building and running

You need Visual Studio 2026 with the **Desktop development with C++** workload.
raylib is included in `Chronicles-of-the-Spice-Trip/vendor/`, so there is
nothing else to install.

1. Open `Chronicles-of-the-Spice-Trip/Chronicles-of-the-Spice-Trip.slnx`.
2. Pick **Release | x64** (or Debug) and press **F5**.

From a Developer Command Prompt:

```
cd Chronicles-of-the-Spice-Trip
msbuild Chronicles-of-the-Spice-Trip.vcxproj -p:Configuration=Release -p:Platform=x64
build\x64\Release\ChroniclesOfTheSpiceTrip.exe
```

Start the game from the `Chronicles-of-the-Spice-Trip` folder (Visual Studio
already does), or from the build folder: it looks a few folders up for `assets/`.
The build is not signed, so Windows SmartScreen or Smart App Control may stop it
the first time it runs.

## Controls

| Input | Action |
| --- | --- |
| W A S D | Move (Shift runs) |
| Mouse | Aim |
| Left click | Swing the blade |
| Right click | Cast the selected power |
| 1 / 2 / 3 | Emberhex (fire), Tidelash (water), Stormbolt (lightning) |
| Q | Your champion's special |
| Space | Dodge-roll |
| E | Talk, trade, open chests |
| Esc | Pause (save, controls, main menu) |
| F11 | Fullscreen on/off |

Movement, run, roll, interact and the special can be rebound in **Settings**.

## What's in the game

- **The dungeon.** Each floor is a grid of single-screen rooms. Doors stay
  barred until a room is cleared. There are spike traps, chests, a start room
  with Theira the Oracle, and Marin's Market, where potions, timed vape buffs
  and permanent skills are sold for gold.
- **Boss fights.** The last room of every floor holds a boss (see below).
- **Champions.** Ten champions, each with a passive perk and a special on **Q**:
  Arcane Surge, Aura of Calmness, Coin Dash, Mug-Shield, Melon Roll, Drink,
  Bear Jump, Coin Barrage, Release the Koi and Blink.
- **Boss Rush.** All five bosses back to back at level 5, timed. The best time
  is kept.
- **Saves.** Save from the pause menu and **Continue** from the main menu.
  Records (deepest floor, bosses beaten, best Boss Rush) and settings are kept
  between runs.

### The bosses

Each fight shows the boss's name, a health bar, and the spell school it's weak
to (x1.6 damage).

| Floor | Boss | How it fights |
| --- | --- | --- |
| 1 | The Good Plazmodium | 3-shot spreads, then splits into three copies; copies can leave tiny ones |
| 2 | The Bad Plasmodium | The same, with 5-shot spreads and copies that fire 3 |
| 3 | Mordekai | Fans of 7 feathers, and a dive at you every third volley |
| 4 | Vlado | Spirals of 12 tickets, and a whistle that sends bus No.9 down your lane |
| 5 | Pyraketh, the Cinder-Maw | Chases you, breathes rings of embers, and throws fireballs once wounded |

Beating floor 5 completes the Spice Trip. You can keep descending; every cycle
the bosses get 50% stronger. The numbers for each boss are at the top of
`src/Bosses.cpp`.

## Project layout

```
Chronicles-of-the-Spice-Trip/
  src/main.cpp        window setup and the frame loop
  include/Game.h      all game state; the code is split across:
    src/Game.cpp        runs, rooms, floors, records
    src/Play.cpp        movement, combat, champions, enemies, loot
    src/PlayDraw.cpp    the room, actors, HUD and overlays
    src/Screens.cpp     menu, champion select, settings, intro
  src/Bosses.cpp      the five bosses: table, attack patterns, drawing
  src/Champions.cpp   the champion roster (perks and specials)
  src/Rooms.cpp       floor generation, room layouts, collision, pathfinding
  src/Render.cpp      hero, monsters and projectiles
  src/Sprites.cpp     the dungeon sprite pack
  src/Audio.cpp       sound effects and music, synthesised at runtime
  src/Ui.cpp          the UI style: palette, text, panels, buttons, bars
  src/Input.cpp       keyboard and mouse (real or scripted)
  src/Save.cpp        savegame.dat, profile.dat, settings.dat
  src/Autopilot.cpp   the automated play-through (see below)
  assets/             art; assets/arenas/ holds the four painted boss arenas
  vendor/raylib/      raylib headers and library
```

## Automated play-through

`--autopilot <folder>` plays the whole game with scripted input. It clicks
through the menus, fights a room, opens a chest, shops, beats all five bosses,
completes a run, saves and continues, wins a Boss Rush, and tries every
champion's special. It writes a PASS/FAIL line for each check to
`<folder>/autopilot.log`, saves screenshots next to it, and exits with the
number of failures:

```
build\x64\Release\ChroniclesOfTheSpiceTrip.exe --autopilot autopilot
```

It uses its own save folder (`<folder>/save`, emptied first), so your saves are
never touched. `--savedir <folder>` moves the normal saves too.
