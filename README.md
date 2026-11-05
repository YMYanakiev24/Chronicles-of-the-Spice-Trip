# Chronicles of the Spice Trip

A dark pixel-art dungeon crawler written in C++ with [raylib](https://www.raylib.com/).
Pick a champion, fight down through randomly built floors of the Mistwood
Depths, and beat the boss at the bottom of each one: the four bosses from the
Path in their painted arenas, Triceps in her cave, then Pyraketh, the
Cinder-Maw.

## Building and running

You need Visual Studio 2026 with the **Desktop development with C++** workload.
raylib (and pl_mpeg, which plays the intro video) is included in
`Chronicles-of-the-Spice-Trip/vendor/`, so there is nothing else to install.

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
| Right click, or Enter | Cast the selected power (hold Enter to keep casting) |
| 1 / 2 / 3 | Emberhex (fire), Tidelash (water), Stormbolt (lightning) |
| Q | Your champion's special |
| Space | Dodge-roll |
| E | Talk, trade, open chests |
| Esc | Pause (save, controls, main menu) |
| F11 | Fullscreen on/off |

Movement, run, roll, interact and the special can be rebound in **Settings**.

## What's in the game

- **Marin's video.** Every time the game starts, Marin's video plays once
  before the main menu: the vape, the coughing fit, and waking up in the valley
  below the castle. A click, a tap or any of Enter, Space and Esc skips it. It
  is `assets/intro/marin-trip.mpg` (MPEG-1 video with MP2 sound, which
  `src/Video.cpp` plays). To swap in another video, convert it with ffmpeg:
  `ffmpeg -i in.mp4 -vf scale=480:640:flags=area -r 30 -c:v mpeg1video -q:v 4 -c:a mp2 -b:a 192k -ar 44100 -f mpeg marin-trip.mpg`

- **The dungeon.** Each floor is a grid of single-screen rooms, and the whole
  room is always on screen. Doors stay barred until a room is cleared. There are
  spike traps, chests, and a start room with Vasil the Storyteller and the door to
  the castle, where the shops are (see below).
- **Boss fights.** The last room of every floor holds a boss (see below).
- **Champions.** Sixteen champions, each with a passive perk and a special on **Q**:
  Arcane Surge, Aura of Calmness, Coin Dash, Mug-Shield, Melon Roll, Drink,
  Bear Jump, Coin Slingshot, Release the Koi, Blink, Scare, Creatine & C++,
  Blown Kiss, Smoke Break, Grinders and Haymaker. Each one walks as
  their own figure, seen from the front, the back and both sides, and the
  champion select shows every champion from all four sides.
- **Boss Rush.** All six bosses back to back at level 5, timed. The best time
  is kept.
- **Saves.** Save from the pause menu and **Continue** from the main menu.
  Records (deepest floor, bosses beaten, best Boss Rush) and settings are kept
  between runs.

### Runs, rooms and rewards

- **New Run** asks for a **difficulty** (Easy, Normal, Hard: monster health and
  damage) and whether to play **hardcore**: one life, and dying ends the run and
  deletes the save. Otherwise dying sends you back to the entrance, and Toshko
  picks 30% of your gold from your pockets.
- **Special rooms** on every floor (shown on the minimap): a **treasure room**
  behind a silver door (its silver key lies in a combat room) with a hoard of
  gold, two packs, a full heal and a free vape; a **challenge room** where waves
  keep coming for 30 seconds. One combat room a floor holds a **mini-boss**,
  and some chests are **Mimics**.
- **Floors** each have their own look: the Mistwood Depths, the Drowned Crypt,
  the Ember Halls, the Bone Gardens, the Toxic Sewers and the Gilded Vault.
- **Potions** from Marin go into your belt (three of each): **R** drinks health,
  **F** mana.
- **Every boss has its own theme**, and a run that ends shows a summary: kills,
  gold, damage, deaths and what got you.
- The game **saves itself** whenever you change rooms and every 20 seconds in a
  safe room.

### The castle and its shops

The first room of every floor has a wooden door in its top wall (the minimap
marks the room with it). Walk up into it, or press **E** in front of it, and you
are in the **Castle Corridor** (`assets/castle/corridor.png`). Its four doors lead
to four rooms, each with whoever keeps the shop behind their counter: walk up to
the counter and press **E**. A room's Hallway door on the left, or the carpet at
the bottom, takes you back to the corridor, and the corridor's side door takes
you back down to the dungeon.

- **Marin's Vape Shop** sells potions (buy as many as you like) and vapes. A vape
  is permanent and comes once: Mango Haze (move 10% faster), Blue Razz Ice
  (powers cost 20% less), Watermelon Rage (+4 blade damage), Cool Mint Fog (take
  15% less damage).
- **Todor's Casino**: Toshko's roulette. Bet gold on red, black (x2) or Toshko's
  zero (x14).
- **Beray's Drip Locker** sells outfits (see below).
- **Galin's Weapon Shop** has two sections. **Weapons**: every run starts with the
  Rusty Sword; better ones are bought here, and those you own swap for free.
  **Upgrades** of the weapon in hand: Sharpen, Balance and Extend, three levels
  each. Every level costs gold and one cigarette pack, and each weapon keeps its
  own upgrades.
- **Cigarette packs** drop from every **Gakev** the enemy, who turns up in about
  a third of combat rooms, and about one chest in four holds one too.

The weapons are the `WEAPONS` table in `include/Game.h`: the Rusty Sword, then
the Iron Sword, the quick but short Police Baton, the slow and heavy Baseball
Bat, and the Pink Sword. Each has its own knockback (bosses stand their ground)
and hit sound. To add one, raise `WEAPON_COUNT` and give it a line with its PNG
from `assets/` and the angle its art rises from hilt to tip.

### The enemies

Alongside the Mistlings, Hollow Hounds, Melon Warriors, Gakev and Toshko, every
combat room has a random pick of things that shoot at you:

- **Soldier:** an archer who stops at range and backs away if you crowd him.
- **Wisp:** weaves toward you, firing magic orbs.
- **Blood Monster:** spits blood, and bursts into two smaller, faster halves when
  it dies.

Then there are the **Orc**, tough, with a slow overhead axe you can step away
from, and from floor 2 the **Demon**, which leaps at you from across the room.

### The bosses

Every boss fight builds the same way. The notches on its health bar show what's
coming. At half health it roars and breaks the arena: cracks open and rocks
thrust up. It comes back with a beam you hide from behind a rock, minions, slams
that land late and leave fire behind, and its old moves in threes. At a fifth of
its health it charges the big one: hit its core before it blows, or get behind a
rock. Your last 30% of health counts double throughout. The last hit freezes the
world, slows it down, and the boss falls apart piece by piece.

Each fight shows the boss's name, a health bar, and the spell school it's weak
to (x1.6 damage).

| Floor | Boss | How it fights |
| --- | --- | --- |
| 1 | The Good Plazmodium | 3-shot spreads, then splits into three copies; copies can leave tiny ones |
| 2 | The Bad Plasmodium | A giant head on tentacles in the middle of the room that never moves: balls in every direction, a slam if you stand close, thrown rocks, and minions (its old body) that walk and fire 3 |
| 3 | Mordekai | Fans of 7 feathers, and a dive at you every third volley |
| 4 | Vlado | Spirals of 12 tickets, and a whistle that sends bus No.9 down your lane |
| 5 | Triceps | Walks her cave like a champion; gravity grenades, magnetic boots, the fulcrum staff and the circuit chain (see below) |
| 6 | Pyraketh, the Cinder-Maw | Chases you, breathes rings of embers, and throws fireballs once wounded |

When the Bad Plasmodium falls in a run, a cutscene plays once it has come apart
(`assets/cutscenes/red-eyes.mpg`, made the same way as Marin's video); a click,
a tap or a key skips it. Boss Rush skips it, to keep the clock fair.

Between Triceps and the Cinder-Maw the stairs lead to the toilets first (see
below).

Beating floor 6 completes the Spice Trip. You can keep descending; every cycle
the bosses get 50% stronger. The numbers for each boss are at the top of
`src/Bosses.cpp`.

### Triceps

Triceps fights in Triceps' Cave (`assets/arenas/triceps.png`). She walks
like the champions: a figure seen from every side, facing the way she goes
(`assets/walk/walk-triceps.png`). When she attacks she strikes the pose from her
character sheet (`assets/walk/triceps-poses.png`), and the formula behind the
attack rises over her. Her four attacks take turns, with a short walk between
them:

| Attack | What it does | The physics |
| --- | --- | --- |
| Gravity Grenade Toss | A grenade flies a parabola onto where you stand. It opens a well that pulls you in for 2 s, then goes off | y = x tan(a) - gx^2 / 2(v cos a)^2, F = ma |
| Magnetic Boot Kick | Three boots: one straight at you, two curving in from the sides | F = q(E + v x B): each boot turns at a steady rate |
| Fulcrum Staff Swing | She comes at you, winds up (the reach shows red), swings round, and the far end bats three iron balls at you. The tip hits hardest | T = r x F, F1 d1 = F2 d2 |
| Electrical Circuit Chain | Capacitor nodes charge for a second, then every wire between them is live. The first wire runs through where you stood | I = V / R |

Below half health she walks less, throws two grenades and kicks five boots, and
her circuit has five nodes. She is weak to Water. Momiche's scare sends her
running, but a grenade or circuit she has already started keeps going.

### The toilets: hiding from The Sergeant

After Triceps falls, her stairs lead to the school toilets
(`assets/arenas/toilets.png`) instead of straight down. The Sergeant
(`assets/sergeant.png`, walking as `assets/walk/walk-sergeant.png`) is hunting
you, and you have to stay hidden for one minute.

- **You smoke the whole time**, and the smoke gives you away: she smells it from
  a fair way off and follows it to where you were.
- **Stalls.** Walk up into a stall's door, or press **E** in front of it, to
  hide behind it; down or **E** again to come out. She can't see you in there,
  but your smoke still rises over the door, and she tries doors on her rounds.
- **Q is a pee** in this level (3 s, then 6 s before you can go again). While you
  pee you can't move and you don't smoke, and you leave a puddle. If she walks
  into a puddle she slips and is down for almost 2 seconds.
- **If she sees you** in the open she runs at you. If she catches you, or opens
  the stall you're in, the toilets start over with a fresh minute.
- **Last the minute** and you get 60 gold and a cigarette pack, and the stairs
  lead on to floor 6.

The floors, stalls and her rounds are at the top of `src/Toilet.cpp`, and the
numbers (time, speeds, how far she smells and sees) in `include/Toilet.h`.

### Beray and his outfits

Beray (`assets/castle/beray.png`) sells clothes for every champion: the Drip
Puffer, the Midnight Tracksuit, Gold Rush, Camo Fatigues, Pink Velour and Ice
Cold. Pick the champion along the top (the one you play is picked to start
with), point at an outfit to see them in it from every side, and buy it with the
run's gold. They put it on at once and walk the dungeon in it; an outfit bought
is theirs for good (it's kept in `profile.dat`, not the run), so you can change
back and forth for free, and their own clothes are always there.

Each champion's figure in each outfit is a walk sheet in
`assets/outfits/<outfit>/`, made from their own walk sheet by
`tools/outfits.py` (run it after changing a figure). The outfits are the
`OUTFITS` table in `include/Champions.h`.

### Vasil the Storyteller

Vasil sits on his stool in the first room of every floor, beside his
*Stories by Vasil* sign, with a few listeners round him (a different few on each
floor). As a floor begins he calls you over. Walk up and press **E**: he stands
up and tells that floor's tale, a line per press, about the floor and the boss
waiting at the bottom of it (and what it's weak to). The first floor's tale
also teaches the basics. At the end he takes a bow. He tells the prophecy on the
intro screen too. His art is in `assets/vasil/`, cut from his asset pack.

### The Gym Rat

Creatine in one hand, C++ in the other. His perk: +15 max health and +2 blade
damage. His special, **Creatine & C++** (Q, 6 s cooldown), throws both arms out:
five green C++ lasers fan out the way you aim, each running on until a wall
stops it and hitting everything it touches for 22, while a cloud of creatine out
of the other hand hits whatever is behind him for 8 and shoves it away. His walk
sheet (`assets/walk/walk-gymrat.png`) is cut from his character sheet
(`assets/gymrat.png`).

### Vanessa

Her perk, **Heartbreaker**: every kill heals her 3. Her special, **Blown Kiss**
(Q, 7 s cooldown), blows five hearts out in a fan the way you aim. They curve
after monsters and hit for 12, and a monster a heart lands on is lovestruck for
3.5 seconds: hearts float over it, and it stands gazing at her, can't shoot,
swing or bite (Toshko forgets to run off with your gold). Bosses have no heart
to lose and only take the damage. The numbers are under `KISS_` and
`CHARM_TIME` in `include/Champions.h`.

Her walk sheet (`assets/walk/walk-vanessa.png`) is cut from her two character
sheets: facing you and from behind are her portraits (a lower leg lifted in turn
to walk), and from the side are the strides from her sheet, the second with her
legs swapped (the shoes turned back round) so they take turns. Her card
art is `assets/vanessa.png`.

### Daniil

Blond hair, a black jacket and a slingshot. His perk, **Street fighter**: +5 blade
damage. His special, **Coin Slingshot** (Q): every hit he lands charges it, and
when the meter is full he draws the slingshot, aims and lets fly a tight volley of
8 coins at the aim, 12 damage each. The numbers are under `SLING_` in
`include/Champions.h`.

His walk sheet (`assets/walk/walk-daniil.png`) and his slingshot poses
(`assets/dungeon/daniil_sling.png`: draw, aim, let go, 100 x 80 frames facing
right) are cut from his character sheet (`assets/daniil.png`, also his card art).
Facing you and from behind he walks with a lower leg lifted in turn; from the
side he walks with the stride on his sheet.

### Filqcheto

Black hoodie, earbuds, and a pocket full of grinders. His perk, **Chilled out**:
he takes 12% less damage. His special, **Grinders** (Q, 4 s cooldown), throws
the next of three grinders, round and round (the slot under the special says
which is next):

1. **Weed Grinder Shot**: three grinders in a fan, 16 damage each.
2. **Meta-Grinder**: a cluster of five that hits for 20 and bursts into six
   grinders flying every way (12 each) where it lands, or after 0.8 s.
3. **Ultra-Grinder Storm**: a big grinder in a green and violet whirlwind drifts
   ahead for 3 s, dragging monsters in from 140 px away and grinding everything
   it touches for 7 every 0.3 s. Bosses aren't dragged, only ground.

He winds up and throws (arms out wide for the storm) with poses from his sheet.
The numbers are under `GRINDER_`, `META_` and `STORM_` in `include/Champions.h`.

### Krisa

Grey cap, long hair, and a right hook. Her perk, **Quick hands**: she swings her
weapon 25% faster. Her special, **Haymaker** (Q, 5 s cooldown): she dashes the way
you aim and punches at the end of it. Everything just in front of her takes 28
and flies back, seeing stars for 1.5 s (they can't move or attack); bosses only
take the damage. The numbers are under `HAYMAKER_` and `DAZE_TIME`.

Both walk sheets (`assets/walk/walk-filqcheto.png`, `walk-krisa.png`), their poses
(`assets/dungeon/filqcheto_throw.png`, `krisa_punch.png`: 100 x 80 frames facing
right) and his grinders (`grinder.png`, `grinder_big.png`) are cut from their
character sheets (`assets/filqcheto.png`, `assets/krisa.png`, also their card
art). Facing you and from behind they walk with a lower leg lifted in turn; from
the side with the stride from their sheet, then the same with the legs swapped.

### Galin

The weapon-shop owner, playable. His perk, **Smoke breaks**: standing still heals
him 1.5 a second. His special, **Smoke Break** (Q, 8 s cooldown): he stands and
smokes for 0.9 s, healing 30, then steps up to the nearest monster within reach
and stubs the cig out on its skin for 24 damage, setting it alight. With no one
close he just flicks it away. He also has his own dodge-roll. The numbers are
under `CIG_` in `include/Champions.h`.

His walk sheet (`assets/walk/walk-galin.png`) and his poses
(`assets/dungeon/galin_smoke.png`, the stub as its last frame, and
`assets/dungeon/roll-galin.png`, 60 x 80 frames facing right) were drawn with
ChatGPT from his picture. His card art is `assets/Galin.png`.

### Dodge-rolls

Every champion has a drawn dodge-roll in `assets/dungeon/roll-<champion>.png`
(six 60 x 80 frames facing right, mirrored when rolling left): crouch, curl into
a ball and tumble, land on a knee, stand. They are made from each champion's own
side-view walk frame by `tools/rolls.py` (run it after changing a figure);
Galin's is drawn by hand and left alone. A champion with no roll sheet tumbles
their walking figure instead.

### Momiche, Gakev and the walking figures

- **Momiche😨** walks with the frames cut from her own sprite sheet. Her
  special, **Scare** (Q, 8 s cooldown), makes every monster and boss run from
  her for 3 seconds. While scared they don't shoot and can't hurt her by touch
  (a bus that's already coming still drives). A fearful face bobs over each of
  them. Her perk is +3 blade damage.
- **Gakev** the champion's kills drop three extra coins.
- **The walking figures** are the same as in the Unity version of the game.
  They live in `assets/walk/walk-<champion>.png`: rows facing down, up and
  right (left is the right row mirrored), each the standing frame and then
  four walk frames, all 3:4. They are exported from the Unity project with
  **Spice Trip > Export Walk Sheets**, which writes them (and `fear.png`) to
  its `Captures/walk-sheets/`. Copy them into `assets/walk/` after changing a
  figure there. A missing sheet falls back to the hero drawn in code.

## Project layout

```
Chronicles-of-the-Spice-Trip/
  src/main.cpp        window setup and the frame loop
  include/Game.h      all game state; the code is split across:
    src/Game.cpp        runs, rooms, floors, records
    src/Play.cpp        movement, combat, champions, enemies, loot
    src/PlayDraw.cpp    the room, actors, HUD and overlays
    src/Screens.cpp     menu, champion select, settings, intro
  src/Bosses.cpp      the six bosses: table, attack patterns, drawing
  src/Champions.cpp   the champion roster (art, walking figures, perks and specials)
  src/Rooms.cpp       floor generation, room layouts, collision, pathfinding
  src/Render.cpp      the champions' figures, monsters and projectiles
  src/Sprites.cpp     the dungeon sprite pack and the champions' walk sheets
  src/Audio.cpp       sound effects and music, synthesised at runtime
  src/Ui.cpp          the UI style: palette, text, panels, buttons, bars
  src/Input.cpp       keyboard and mouse (real or scripted)
  src/Save.cpp        savegame.dat, profile.dat, settings.dat
  src/Autopilot.cpp   the automated play-through (see below)
  src/Video.cpp       plays the intro video and the cutscene (MPEG-1) with their sound
  src/Toilet.cpp      the toilets after Triceps: hiding from The Sergeant
  src/Castle.cpp      the castle corridor, the shopkeepers' rooms and Beray's outfits
  assets/             art; assets/arenas/ holds the bosses' painted arenas,
                      assets/walk/ the champions' and Triceps' walk sheets,
                      assets/vasil/ Vasil the Storyteller and his listeners,
                      assets/castle/ the castle's rooms, assets/outfits/ the outfits
  tools/outfits.py    makes the champions' figures in Beray's outfits
  vendor/raylib/      raylib headers and library
  vendor/pl_mpeg/     pl_mpeg, the MPEG-1 decoder (MIT, one header)
```

## Automated play-through

`--autopilot <folder>` plays the whole game with scripted input. It watches
the start of Marin's video and skips it, clicks through the menus, fights a room, opens a chest,
goes through the castle (Marin's, Galin's, Todor's and Beray's), beats all six bosses
(and checks each of Triceps' attacks),
hides from The Sergeant in the toilets (smoke, the pee and its puddle, a stall,
being found, a minute survived), completes a run, saves and continues, wins a
Boss Rush, checks Vasil has each floor's tale, and tries every
champion's special, Momiche's scare on a boss, Gakev's coins and casting with
Enter. It writes a PASS/FAIL line for each check (217) to
`<folder>/autopilot.log`, saves screenshots next to it, and exits with the
number of failures:

```
build\x64\Release\ChroniclesOfTheSpiceTrip.exe --autopilot autopilot
```

It uses its own save folder (`<folder>/save`, emptied first), so your saves are
never touched. `--savedir <folder>` moves the normal saves too.

## Android

Pushing a tag like `v0.2.0` makes GitHub Actions build the APK (.github/workflows/android.yml) and publish it as a release. The Android project is in `Chronicles-of-the-Spice-Trip/android`. Run the desktop build with `--touch` to try the phone controls with the mouse.

To sign every APK with the same key (so new versions install over old ones), add
the repository secrets `ANDROID_KEYSTORE_BASE64` and `ANDROID_KEYSTORE_PASSWORD`.
Without them each build uses a throwaway key. On phones, the game buzzes when
you're hit (Settings > Vibration).
