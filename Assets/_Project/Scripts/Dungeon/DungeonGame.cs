/*
 * DungeonGame.cs
 * ----------------------------------------------------------------------------
 * The rules of the Dungeon mode, with no rendering: runs and floors, rooms and
 * doors, movement, the blade and the three powers, champion specials, monster
 * AI, boss fights, loot, Marin's market, and Boss Rush. DungeonView draws it,
 * DungeonHud shows its HUD and panels, DungeonController feeds it input.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Core;
using UnityEngine;
using static SpiceTrip.Dungeon.Geo;

namespace SpiceTrip.Dungeon
{
    public enum DungeonMode { Run, BossRush }
    public enum Overlay { None, Pause, Shop, RunComplete, RushOver }

    public struct DungeonInput
    {
        public Vector2 move;                 // each axis -1..1, y down
        public Vector2 aim;                  // room pixels
        public bool run, rollPressed, interactPressed, specialPressed, castPressed, meleePressed;
        public int slot;                     // power chosen this frame, -1 = none
        public static DungeonInput None => new DungeonInput { slot = -1 };
    }

    [Serializable]
    public class DungeonProfile
    {
        public int deepestFloor, runsCompleted;
        public int bossesBeaten;             // a bit per Defs.Bosses entry
        public float bestRush;               // seconds, 0 = not won yet
        public bool Beaten(int i) => (bossesBeaten & (1 << i)) != 0;
    }

    [Serializable]
    public class DungeonRunSave
    {
        public Player player;
        public Floor floor;
        public int floorNumber, kills;
        public float runTime;
    }

    public class DungeonGame
    {
        static readonly string[] OracleLines =
        {
            "So. The Veil spat you out at last.",
            "Press 1 and right-click to loose an Emberhex. Q calls your champion's gift.",
            "Clear a room to open its doors. Something old waits at the bottom of every floor.",
        };

        readonly ISfx _audio;
        readonly DRandom _rng;
        readonly int[] _flow = new int[RoomTW * RoomTH];

        public DungeonMode Mode { get; private set; }
        public Overlay Overlay = Overlay.None;
        public DungeonProfile Profile = new DungeonProfile();
        public event Action ProfileChanged;

        public Player Player = new Player();
        public Floor Floor = new Floor();
        public readonly List<Enemy> Enemies = new List<Enemy>(), Corpses = new List<Enemy>();
        readonly List<Enemy> _spawned = new List<Enemy>();
        public readonly List<Shot> Shots = new List<Shot>(), EnemyShots = new List<Shot>();
        public readonly List<Particle> Particles = new List<Particle>();
        public readonly List<Popup> Popups = new List<Popup>();
        public readonly List<Coin> Coins = new List<Coin>();
        public readonly List<Floater> Floaters = new List<Floater>();
        public readonly BossFight Fight;

        public int FloorNumber = 1, Kills;
        public float RunTime, ChestOpenT = 99, Shake, TrapClock, Time;
        public bool InFight;
        public string BannerText = "", BannerSub = "";
        public float BannerT;
        public string Dialogue = "";
        int _dialogueIdx;
        public string ShopMessage = "";
        public float ShopMessageT;

        // Boss Rush
        public int RushStage;
        public float RushTime, RushNextT = -1;
        public bool RushWon, RushRecord;

        public DungeonGame(ISfx audio, int seed = 0)
        {
            _audio = audio;
            _rng = new DRandom(seed == 0 ? Environment.TickCount : seed);
            Fight = new BossFight(_rng);
            Fight.OnSfx += Sfx;
        }

        void Sfx(string name) => _audio?.Sfx(name);
        public ChampionKit Kit => Player.Kit;
        public Room Room => Floor.Cur;

        // =====================================================================
        // Runs
        // =====================================================================
        static Player FreshPlayer(string champion, int level)
        {
            var p = new Player { champion = champion };
            for (int l = 1; l < level; l++) { p.maxHp += 18; p.maxMana += 10; }
            p.level = level;
            p.xpNext = (int)(60 * Mathf.Pow(level, 1.5f));
            var kit = p.Kit;
            p.maxHp += kit.hpBonus;
            p.hp = p.maxHp; p.mana = p.maxMana;
            if (kit.kind == Special.Blink) p.charge = Defs.DustMax;
            return p;
        }

        public void StartRun(string champion)
        {
            Mode = DungeonMode.Run;
            Player = FreshPlayer(champion, 1);
            Player.pos = Center;
            Floor.Generate(_rng);
            FloorNumber = 1; Kills = 0; RunTime = 0;
            Dialogue = ""; Overlay = Overlay.None;
            EnterRoom();
            ShowBanner("The Mistwood Depths", $"{CurrentBoss.name} waits at the bottom of floor 1", 3);
        }

        public void StartBossRush(string champion)
        {
            Mode = DungeonMode.BossRush;
            Player = FreshPlayer(champion, 5);
            Player.pos = new Vector2(Center.x, RoomH - Tile * 2f);
            Floor.GenerateArena(_rng);
            FloorNumber = 1; Kills = 0;
            RushStage = 0; RushTime = 0; RushNextT = -1; RushWon = RushRecord = false;
            Dialogue = ""; Overlay = Overlay.None;
            EnterRoom();
        }

        public void Continue(DungeonRunSave save)
        {
            Mode = DungeonMode.Run;
            Player = save.player;
            if (Player.vapeT == null || Player.vapeT.Length != 4) Player.vapeT = new float[4];
            Floor = save.floor;
            FloorNumber = save.floorNumber; Kills = save.kills; RunTime = save.runTime;
            Dialogue = ""; Overlay = Overlay.None;
            EnterRoom();
            ShowBanner($"Floor {FloorNumber}", "Welcome back", 2);
        }

        public DungeonRunSave MakeSave() => Mode != DungeonMode.Run ? null : new DungeonRunSave
        {
            player = Player, floor = Floor, floorNumber = FloorNumber, kills = Kills, runTime = RunTime,
        };

        public void ShowBanner(string text, string sub = "", float seconds = 2.2f)
        {
            BannerText = text; BannerSub = sub; BannerT = seconds;
        }

        public int CurrentBossIndex => Mode == DungeonMode.BossRush ? RushStage : Defs.BossIndexForFloor(FloorNumber);
        public DungeonBoss CurrentBoss => Defs.Bosses[CurrentBossIndex];
        public float CurrentStrength => Mode == DungeonMode.BossRush ? 1f : Defs.BossStrength(FloorNumber);

        void SaveProfile() => ProfileChanged?.Invoke();

        // =====================================================================
        // Rooms and floors
        // =====================================================================
        public void EnterRoom()
        {
            Enemies.Clear(); Corpses.Clear(); _spawned.Clear();
            Shots.Clear(); EnemyShots.Clear();
            Particles.Clear(); Popups.Clear(); Coins.Clear(); Floaters.Clear();
            Fight.End();
            ChestOpenT = 99;
            if (Overlay == Overlay.Shop) Overlay = Overlay.None;

            var r = Room;
            if (r.isShop && !r.visited) ShowBanner("Marin's Market", "Press E to trade", 2);
            r.visited = true;
            InFight = false;
            _audio?.PlayMusic(r.isBoss && !r.cleared ? "caves" : "mist");
            if (r.cleared) return;

            if (r.isBoss)
            {
                var b = CurrentBoss;
                Fight.Begin(CurrentBossIndex, CurrentStrength, new Vector2(Center.x, Center.y - 30), Enemies);
                ShowBanner(b.name, Mode == DungeonMode.BossRush
                    ? $"Stage {RushStage + 1} / {Defs.Bosses.Length}  -  weak to {b.weakness}"
                    : $"{b.epithet}  -  weak to {b.weakness}", 3);
                Sfx("ultimate");
                Shake = 0.6f;
            }
            else
            {
                // deeper floors send more, and tougher, monsters
                float tough = 1 + 0.15f * (FloorNumber - 1);
                int extra = (FloorNumber - 1) / 2;
                for (int i = 0, n = 3 + _rng.Range(0, 2) + extra; i < n; i++)
                    Spawn("Mistling", 30 * tough, 60, 6, 12, 12, new Color32(138, 168, 154, 255), "Fire", EnemyKind.Skeleton);
                for (int i = 0, n = _rng.Range(0, 1); i < n; i++)
                    Spawn("Hollow Hound", 70 * tough, 110, 14, 26, 13, new Color32(42, 31, 46, 255), "Fire", EnemyKind.Hound);
                for (int i = 0, n = _rng.Range(0, 2); i < n; i++)
                    Spawn("Wisp", 20 * tough, 85, 5, 10, 10, new Color32(120, 180, 230, 255), "Lightning", EnemyKind.Wisp);
                if (_rng.Range(0, 2) == 0)
                    Spawn("Coin Thief", 45 * tough, 120, 9, 30, 12, new Color32(40, 60, 140, 255), "Lightning", EnemyKind.Thief);
                if (_rng.Range(0, 2) == 0)
                    Spawn("Melon Warrior", 110 * tough, 50, 16, 24, 14, new Color32(230, 150, 60, 255), "Fire", EnemyKind.Melon);
            }
            InFight = true;
        }

        void Spawn(string name, float hp, float speed, int dmg, int xp, float r, Color color, string weak, EnemyKind kind)
        {
            int tx = 0, ty = 0;
            for (int tries = 0; tries < 100; tries++)       // an open floor spot away from the walls
            {
                tx = _rng.Range(3, RoomTW - 4);
                ty = _rng.Range(3, RoomTH - 4);
                if (Room.IsFloor(tx, ty)) break;
            }
            Enemies.Add(new Enemy
            {
                name = name, hp = hp, maxHp = hp, speed = speed, dmg = dmg, xp = xp, r = r, color = color, weakness = weak, kind = kind,
                wobble = _rng.F(0, 6.28f), animT = _rng.F(0, 1), flankSide = _rng.Range(0, 1) == 1 ? 1 : -1, lungeCd = _rng.F(0.5f, 2),
                pos = new Vector2(tx * Tile + 16, ty * Tile + 16),
            });
        }

        public void Descend()
        {
            FloorNumber++;
            Profile.deepestFloor = Mathf.Max(Profile.deepestFloor, FloorNumber);
            SaveProfile();
            Floor.Generate(_rng);
            Player.hp = Mathf.Min(Player.maxHp, Player.hp + 30);    // a breather between floors
            Player.pos = Center;
            Overlay = Overlay.None;
            EnterRoom();
            ShowBanner($"Floor {FloorNumber}", $"{CurrentBoss.name} waits at the bottom", 2.5f);
            Sfx("ultimate");
        }

        void BossDefeated()
        {
            var b = CurrentBoss;
            Profile.bossesBeaten |= 1 << CurrentBossIndex;
            EnemyShots.Clear();
            Fight.End();
            _audio?.PlayMusic("mist");
            Sfx("quest");

            if (Mode == DungeonMode.Run)
            {
                Profile.deepestFloor = Mathf.Max(Profile.deepestFloor, FloorNumber);
                if (FloorNumber % Defs.Bosses.Length == 0) { Profile.runsCompleted++; Overlay = Overlay.RunComplete; }
                else ShowBanner($"{b.name} falls!", "The stairs lead down", 2.5f);
            }
            else if (RushStage + 1 >= Defs.Bosses.Length)
            {
                RushWon = true;
                RushRecord = Profile.bestRush <= 0 || RushTime < Profile.bestRush;
                if (RushRecord) Profile.bestRush = RushTime;
                Overlay = Overlay.RushOver;
            }
            else
            {
                ShowBanner($"{b.name} falls!", "The next challenger approaches...", 2.5f);
                RushNextT = 2.5f;
            }
            SaveProfile();
        }

        // =====================================================================
        // The frame
        // =====================================================================
        public float HurtMul => (Player.Has(Skill.IronSkin) ? 0.75f : 1) * (Player.Buzzing(Vape.Mint) ? 0.5f : 1);
        public bool SpikesUp => TrapClock % 2.4f < 1.2f;
        public bool NearOracle => Room.isStart && Mode == DungeonMode.Run && Vector2.Distance(Player.pos, OraclePos) < 48;
        public bool NearMerchant => Room.isShop && Vector2.Distance(Player.pos, MerchantPos) < 56;
        public bool NearChest => Room.hasChest && !Room.chestOpened && Room.cleared && Vector2.Distance(Player.pos, Room.ChestPos) < 40;
        public bool StairsOpen => Mode == DungeonMode.Run && Room.isBoss && Room.cleared;

        public void Tick(float dt, DungeonInput inp)
        {
            Time += dt;
            if (Overlay != Overlay.None) return;
            if (ShopMessageT > 0) ShopMessageT -= dt;
            if (Shake > 0) Shake = Mathf.Max(0, Shake - dt);
            var room = Room;
            var p = Player;
            var kit = Kit;
            bool talking = Dialogue.Length > 0;
            float slow = p.auraT > 0 ? 0.5f : 1;      // Atanas' aura

            // --- timers ---
            if (BannerT > 0) BannerT -= dt;
            if (Mode == DungeonMode.Run) RunTime += dt;
            else if (Fight.Active) RushTime += dt;
            p.castCd = Mathf.Max(0, p.castCd - dt); p.meleeCd = Mathf.Max(0, p.meleeCd - dt);
            p.rollCd = Mathf.Max(0, p.rollCd - dt); p.invuln = Mathf.Max(0, p.invuln - dt);
            p.hitFlash = Mathf.Max(0, p.hitFlash - dt); p.specialCd = Mathf.Max(0, p.specialCd - dt);
            p.auraT = Mathf.Max(0, p.auraT - dt); p.shieldT = Mathf.Max(0, p.shieldT - dt); p.wobbleT = Mathf.Max(0, p.wobbleT - dt);
            p.scareT = Mathf.Max(0, p.scareT - dt);
            p.meleeAnim = Mathf.Max(0, p.meleeAnim - dt * 4);
            p.mana = Mathf.Min(p.maxMana, p.mana + (p.Has(Skill.Arcane) ? 14.4f : 9f) * dt);
            if (kit.kind == Special.Blink) p.charge = Mathf.Min(Defs.DustMax, p.charge + Defs.DustRegen * dt);
            for (int v = 0; v < 4; v++)
            {
                if (p.vapeT[v] <= 0) continue;
                p.vapeT[v] = Mathf.Max(0, p.vapeT[v] - dt);
                if (_rng.F(0, 1) < dt * 5)     // a lazy cloud drifting up off the player
                    Particles.Add(new Particle { pos = p.pos + new Vector2(_rng.F(-6, 6), -18), vel = new Vector2(_rng.F(-12, 12), _rng.F(-40, -20)),
                        life = 1, maxLife = 1, size = _rng.F(2, 4), color = Defs.VapeColor[v] });
            }
            TrapClock += dt;

            if (inp.slot >= 0 && inp.slot < Defs.Spells.Length) { p.selected = inp.slot; Sfx("ui"); }

            // --- movement ---
            var mv = talking ? Vector2.zero : Vector2.ClampMagnitude(inp.move, 1);
            if (mv.sqrMagnitude > 0) { mv = mv.normalized; p.facing = mv; p.moveDir = mv; }
            p.moving = mv.sqrMagnitude > 0 || p.roll > 0 || p.dashT > 0 || p.rollAtkT > 0;
            var aim = inp.aim - p.pos;
            if (aim.magnitude > 1) p.facing = aim.normalized;
            if (!talking && inp.rollPressed && p.rollCd <= 0 && mv.sqrMagnitude > 0)
            {
                p.roll = 0.28f; p.rollCd = p.Has(Skill.Dash) ? 0.35f : 0.7f;
                p.invuln = Mathf.Max(p.invuln, 0.3f); p.rollDir = mv;
            }
            if (!talking && inp.specialPressed) UseSpecial(mv, inp.aim);
            MovePlayer(dt, mv, inp.run);
            if (kit.stillRegen > 0 && mv.sqrMagnitude == 0 && p.hp < p.maxHp) p.hp = Mathf.Min(p.maxHp, p.hp + kit.stillRegen * dt);

            bool airborne = p.jumpT > 0;
            if (SpikesUp && !airborne && p.roll <= 0 && p.invuln <= 0 && RoomPhysics.OnSpikes(room, p.pos)) HurtPlayer(8, 0.6f);

            // --- chest (only once the room is safe) ---
            if (NearChest && !talking && inp.interactPressed)
            {
                room.chestOpened = true; Sfx("pickup"); ChestOpenT = 0;
                bool mana = _rng.Range(0, 1) == 0;
                var cp = room.ChestPos;
                Popups.Add(new Popup { mana = mana, pos = cp + new Vector2(0, -14) });
                if (mana) { p.maxMana += 10; p.mana = Mathf.Min(p.maxMana, p.mana + 10); ShowBanner("Chest: +10 max mana", "", 1.6f); }
                else { p.hp = Mathf.Min(p.maxHp, p.hp + 10); ShowBanner("Chest: +10 health", "", 1.6f); }
                for (int k = 0; k < 4; k++) Coins.Add(new Coin { pos = cp, vel = new Vector2(_rng.F(-90, 90), _rng.F(-140, -40)) });
                Burst(cp, 12, 80, 0.6f, new Color32(245, 215, 120, 255));
            }

            // --- the oracle (start room) and Marin (shop room) ---
            if (NearOracle && inp.interactPressed)
            {
                Sfx("ui");
                if (!talking) { _dialogueIdx = 0; Dialogue = OracleLines[0]; }
                else if (++_dialogueIdx >= OracleLines.Length) Dialogue = "";
                else Dialogue = OracleLines[_dialogueIdx];
            }
            if (!NearOracle) Dialogue = "";
            if (NearMerchant && !talking && inp.interactPressed) { Overlay = Overlay.Shop; Sfx("ui"); }

            // --- attacks ---
            if (!talking && inp.castPressed) CastSpell(inp.aim);
            if (!talking && inp.meleePressed) SwingBlade();

            UpdateShots(dt);
            Fight.Scared = p.scareT > 0;
            Fight.Update(dt, p.pos, Enemies, EnemyShots, room, slow);
            UpdateEnemies(dt, slow);
            UpdateEnemyShots(dt, slow);
            if (Fight.BusHits(p.pos, PlayerR) && p.invuln <= 0 && !airborne)
            {
                Fight.Bus.hit = true;
                HurtPlayer(Fight.BusDamage, 0.8f);
                Shake = 0.5f;
            }

            // --- deaths: corpses, loot, and the Plasmodia multiplying ---
            foreach (var e in Enemies) if (e.alive && e.hp <= 0) KillEnemy(e);
            Enemies.RemoveAll(e => !e.alive);
            Enemies.AddRange(_spawned);
            _spawned.Clear();

            UpdateLoot(dt);

            // --- room cleared? open the doors ---
            if (InFight && Enemies.Count == 0)
            {
                room.cleared = true; InFight = false;
                if (room.isBoss) BossDefeated();
                else { ShowBanner("Room cleared", "", 1.4f); Sfx("quest"); }
            }
            if (Overlay != Overlay.None) return;

            // --- leave through an open door, or take the stairs down ---
            CheckRoomExits();
            if (StairsOpen && Coins.Count == 0 && Corpses.Count == 0 && Vector2.Distance(p.pos, Center) < 18) { Descend(); return; }
            if (Mode == DungeonMode.BossRush && RushNextT > 0)
            {
                RushNextT -= dt;
                if (RushNextT <= 0)
                {
                    RushNextT = -1;
                    RushStage++;
                    p.hp = Mathf.Min(p.maxHp, p.hp + p.maxHp * 0.35f);
                    p.mana = p.maxMana;
                    p.pos = new Vector2(Center.x, RoomH - Tile * 2f);
                    Room.cleared = false;
                    EnterRoom();
                }
            }

            // --- falling ---
            if (p.hp <= 0)
            {
                Sfx("death");
                if (Mode == DungeonMode.BossRush) { RushWon = false; Overlay = Overlay.RushOver; return; }
                p.hp = p.maxHp; p.mana = p.maxMana;
                Floor.cx = Floor.cy = FloorSize / 2;
                p.pos = Center;
                p.invuln = 1.5f;
                EnterRoom();
                ShowBanner("You fall...", "back to the entrance", 2.2f);
            }
        }

        void Burst(Vector2 at, int n, float speed, float life, Color c)
        {
            for (int k = 0; k < n; k++)
                Particles.Add(new Particle { pos = at, vel = new Vector2(_rng.F(-speed, speed), _rng.F(-speed, speed)), life = life, maxLife = life, size = _rng.F(2, 4), color = c });
        }

        // =====================================================================
        // Movement and champion specials
        // =====================================================================
        void MovePlayer(float dt, Vector2 mv, bool running)
        {
            var p = Player;
            var kit = Kit;
            var room = Room;
            Vector2 delta;
            if (p.dashT > 0)                                // Coin Dash
            {
                p.dashT -= dt;
                delta = p.dashDir * 520 * dt;
            }
            else if (p.rollAtkT > 0)                        // Melon Roll
            {
                p.rollAtkT -= dt;
                delta = p.dashDir * Run * 1.8f * dt;
                foreach (var e in Enemies)
                {
                    if (!e.alive || e.lastRollHit == p.rollId) continue;
                    if (Vector2.Distance(e.pos, p.pos) > e.r + PlayerR + 6) continue;
                    e.lastRollHit = p.rollId;
                    HitEnemy(e, 20, new Color32(120, 220, 90, 255));
                    Sfx("hit");
                }
            }
            else if (p.roll > 0)
            {
                p.roll -= dt;
                delta = p.rollDir * RollSpeed * dt;
            }
            else
            {
                float speed = (running ? Run : Walk) * kit.speedMul
                    * (p.Has(Skill.Swift) ? 1.15f : 1) * (p.Buzzing(Vape.Mango) ? 1.35f : 1) * (p.jumpT > 0 ? 1.5f : 1);
                delta = mv * speed * dt;
            }
            p.pos = RoomPhysics.Move(p.pos, delta, PlayerR, room, room.cleared);

            if (p.jumpT > 0)                                // Bear Jump: land with a slam
            {
                p.jumpT -= dt;
                if (p.jumpT <= 0)
                {
                    foreach (var e in Enemies)
                        if (e.alive && Vector2.Distance(e.pos, p.pos) < 56 + e.r) HitEnemy(e, 18, new Color32(230, 180, 90, 255));
                    for (int k = 0; k < 20; k++)
                    {
                        float a = k * Mathf.PI * 2 / 20;
                        var d = new Vector2(Mathf.Cos(a), Mathf.Sin(a));
                        Particles.Add(new Particle { pos = p.pos + d * 10, vel = d * 160, life = 0.45f, maxLife = 0.45f, size = 3, color = new Color32(200, 170, 120, 255) });
                    }
                    Shake = 0.3f; Sfx("hit");
                }
            }
        }

        public void UseSpecial(Vector2 mv, Vector2 aim)
        {
            var p = Player;
            var kit = Kit;
            if (p.specialCd > 0) return;
            var dir = mv.sqrMagnitude > 0 ? mv : p.facing;
            switch (kit.kind)
            {
                case Special.ArcaneSurge:
                    p.mana = Mathf.Min(p.maxMana, p.mana + 40);
                    Burst(p.pos, 16, 90, 0.6f, new Color32(120, 170, 255, 255)); Sfx("heal");
                    break;
                case Special.Aura:
                    p.hp = Mathf.Min(p.maxHp, p.hp + 25);
                    p.auraT = 3;
                    Burst(p.pos, 16, 90, 0.7f, new Color32(150, 220, 255, 255)); Sfx("heal");
                    break;
                case Special.CoinDash:
                    p.dashDir = dir; p.dashT = 0.25f; p.invuln = Mathf.Max(p.invuln, 0.3f);
                    Sfx("coin");
                    break;
                case Special.MugShield:
                    p.shieldT = 2; Sfx("shield");
                    break;
                case Special.MelonRoll:
                    p.dashDir = dir; p.rollAtkT = 0.6f; p.invuln = Mathf.Max(p.invuln, 0.6f);
                    p.rollId++;
                    Sfx("ui");
                    break;
                case Special.Drink:
                    p.hp = Mathf.Min(p.maxHp, p.hp + 20);
                    p.wobbleT = 4; Sfx("heal");
                    break;
                case Special.BearJump:
                    p.jumpT = 0.6f; p.invuln = Mathf.Max(p.invuln, 0.6f); Sfx("ui");
                    break;
                case Special.CoinBarrage:
                    if (p.charge < Defs.CoinPowerMax) return;
                    p.charge = 0;
                    for (int i = 0; i < 16; i++)
                    {
                        float a = i * Mathf.PI * 2 / 16;
                        Shots.Add(new Shot { pos = p.pos, vel = new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * 300, damage = 10, r = 6, life = 1.2f,
                            kind = ShotKind.Coin, color = new Color32(245, 215, 120, 255), glow = new Color32(255, 240, 180, 255) });
                    }
                    Sfx("coin");
                    break;
                case Special.Koi:
                    float baseA = Mathf.Atan2(p.facing.y, p.facing.x);
                    for (int i = -1; i <= 1; i++)
                    {
                        float a = baseA + i * 0.5f;
                        Shots.Add(new Shot { pos = p.pos, vel = new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * 180, damage = 12, r = 7, life = 4,
                            kind = ShotKind.Koi, homing = 3, color = new Color32(255, 140, 50, 255), glow = new Color32(255, 200, 140, 255) });
                    }
                    Sfx("nature");
                    break;
                case Special.Blink:
                    if (p.charge < Defs.DustPerBlink) return;
                    if (RoomPhysics.WallAt(Room, aim, false)) return;       // can't blink into a wall
                    p.charge -= Defs.DustPerBlink;
                    Burst(p.pos, 12, 70, 0.5f, new Color32(240, 170, 255, 255));
                    p.pos = aim;
                    p.invuln = Mathf.Max(p.invuln, 0.2f);
                    Burst(p.pos, 12, 70, 0.5f, new Color32(240, 170, 255, 255));
                    Sfx("shield");
                    break;
                case Special.Scare:
                    p.scareT = Defs.ScareTime;
                    Burst(p.pos, 18, 160, 0.5f, new Color32(170, 190, 255, 255));
                    Shake = Mathf.Max(Shake, 0.15f);
                    Sfx("scare");
                    break;
            }
            p.specialCd = kit.cooldown;
        }

        // =====================================================================
        // Attacks
        // =====================================================================
        public int SpellCost(int i) => Player.Buzzing(Vape.Razz) ? 0 : Mathf.CeilToInt(Defs.Spells[i].manaCost * Kit.manaCostMul);

        void CastSpell(Vector2 aim)
        {
            var p = Player;
            if (p.castCd > 0) return;
            var s = Defs.Spells[p.selected];
            int cost = SpellCost(p.selected);
            if (p.mana < cost) return;
            p.mana -= cost;
            p.castCd = 0.28f * Kit.castCdMul;
            Sfx(s.sfx);
            var dir = (aim - p.pos).normalized;
            if (dir.sqrMagnitude < 0.0001f) dir = p.facing;
            if (p.wobbleT > 0)             // Ristyo's drink
            {
                float a = Mathf.Atan2(dir.y, dir.x) + _rng.F(-0.35f, 0.35f);
                dir = new Vector2(Mathf.Cos(a), Mathf.Sin(a));
            }
            Shots.Add(new Shot
            {
                pos = p.pos + dir * 14, vel = dir * s.speed, damage = s.damage, color = s.color, glow = s.glow, school = s.school,
                life = 1.4f, r = p.selected == 2 ? 4 : 6, homing = Kit.spellHoming,
            });
        }

        public int BladeDamage => 10 + Player.level * 2 + (Player.Has(Skill.Blade) ? 8 : 0) + (Player.Buzzing(Vape.Melon) ? 12 : 0) + Kit.bladeBonus;

        void SwingBlade()
        {
            var p = Player;
            if (p.meleeCd > 0) return;
            p.meleeCd = 0.4f; p.meleeAnim = 1;
            bool hitAny = false;
            foreach (var e in Enemies)
            {
                if (!e.alive) continue;
                var to = e.pos - p.pos;
                if (to.magnitude >= 40 + e.r || Vector2.Dot(to.normalized, p.facing) <= 0.3f) continue;
                HitEnemy(e, BladeDamage, new Color32(234, 255, 255, 255));
                if (p.Has(Skill.Leech)) p.hp = Mathf.Min(p.maxHp, p.hp + 2);
                hitAny = true;
            }
            if (hitAny) Sfx("hit");
        }

        public void HitEnemy(Enemy e, int damage, Color fx)
        {
            e.hp -= damage; e.hitFlash = 0.12f;
            Floaters.Add(new Floater { pos = e.pos + new Vector2(_rng.F(-6, 6), -e.r - 8), value = damage, color = fx });
            Burst(e.pos, 5, 60, 0.4f, fx);
            if (Kit.kind == Special.CoinBarrage) Player.charge = Mathf.Min(Defs.CoinPowerMax, Player.charge + Defs.CoinPowerPerHit);
        }

        public void HurtPlayer(float damage, float invuln)
        {
            float dealt = damage * HurtMul;
            Player.hp -= dealt;
            Player.hitFlash = 0.15f; Player.invuln = invuln;
            Floaters.Add(new Floater { pos = Player.pos + new Vector2(0, -26), value = Mathf.CeilToInt(dealt), color = new Color32(255, 110, 110, 255) });
            Sfx("hurt");
        }

        void UpdateShots(float dt)
        {
            var room = Room;
            foreach (var s in Shots)
            {
                if (!s.alive) continue;
                if (s.homing > 0)            // curve toward the nearest enemy in reach
                {
                    Enemy target = null;
                    float best = 300;
                    foreach (var e in Enemies)
                    {
                        float d = Vector2.Distance(e.pos, s.pos);
                        if (e.alive && d < best) { best = d; target = e; }
                    }
                    if (target != null)
                    {
                        float want = Mathf.Atan2(target.pos.y - s.pos.y, target.pos.x - s.pos.x), have = Mathf.Atan2(s.vel.y, s.vel.x);
                        float diff = Mathf.DeltaAngle(have * Mathf.Rad2Deg, want * Mathf.Rad2Deg) * Mathf.Deg2Rad;
                        float a = have + Mathf.Clamp(diff, -s.homing * dt, s.homing * dt);
                        s.vel = new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * s.vel.magnitude;
                    }
                }
                s.pos += s.vel * dt;
                s.life -= dt;
                if (s.life <= 0 || RoomPhysics.WallAt(room, s.pos, room.cleared)) { s.alive = false; continue; }
                foreach (var e in Enemies)
                {
                    if (!e.alive || Vector2.Distance(s.pos, e.pos) >= e.r + s.r) continue;
                    bool weak = e.weakness != null && s.school != null && e.weakness == s.school;
                    HitEnemy(e, weak ? (int)(s.damage * 1.6f) : s.damage, weak ? (Color)new Color32(255, 230, 120, 255) : s.glow);
                    s.alive = false; Sfx("hit");
                    Burst(s.pos, 8, 90, 0.5f, s.glow);
                    break;
                }
            }
            Shots.RemoveAll(s => !s.alive);
        }

        // =====================================================================
        // Monsters
        // =====================================================================
        void UpdateEnemies(float dt, float slow)
        {
            var room = Room;
            var p = Player;
            bool doorsOpen = room.cleared;
            bool untouchable = p.invuln > 0 || p.jumpT > 0;
            bool scared = p.scareT > 0;           // Momiche's scare: everyone runs, nobody bites
            // One flow field per frame: every monster reads its next step from it, so
            // they path around pillars and (while it's cheaper) around spike traps.
            RoomPhysics.FlowField(room, p.pos, true, _flow);

            foreach (var e in Enemies)
            {
                if (!e.alive) continue;
                float edt = dt * slow;
                e.hitFlash = Mathf.Max(0, e.hitFlash - dt);
                e.atkCd = Mathf.Max(0, e.atkCd - edt);
                e.wobble += edt * 4;
                e.animT += edt;
                e.atkAnim = Mathf.Max(0, e.atkAnim - edt);
                var to = p.pos - e.pos;
                float d = to.magnitude;

                if (e.bossForm < 0 && scared)
                {
                    if (Mathf.Abs(to.x) > 2) e.flip = to.x > 0;            // facing away, running
                    e.moving = true;
                    e.lunge = 0; e.atkAnim = 0;
                    var away = d > 0.01f ? -to / d : Vector2.right;
                    e.pos = RoomPhysics.Move(e.pos, away * e.speed * Defs.FleeSpeed * edt, e.r, room, doorsOpen);
                }
                else if (e.bossForm < 0)          // boss bodies are moved by the BossFight
                {
                    if (Mathf.Abs(to.x) > 2) e.flip = to.x < 0;
                    e.moving = d > 1 && e.atkAnim <= 0;        // plant feet while swinging
                    e.lunge = Mathf.Max(0, e.lunge - edt);
                    e.lungeCd = Mathf.Max(0, e.lungeCd - edt);
                    if (e.moving)
                    {
                        bool sees = RoomPhysics.LineClear(room, e.pos, p.pos, e.r);
                        Vector2 goal;
                        if (!sees) goal = RoomPhysics.NextWaypoint(room, _flow, e.pos);    // route around walls
                        else if (e.kind == EnemyKind.Skeleton && d > 70)
                        {
                            // foot soldiers fan out to the sides so they surround you
                            var n = to.normalized;
                            var flank = p.pos + new Vector2(-n.y, n.x) * 40 * e.flankSide;
                            goal = RoomPhysics.LineClear(room, e.pos, flank, e.r) ? flank : p.pos;
                        }
                        else goal = p.pos;
                        var dir = (goal - e.pos).normalized;
                        if (e.kind == EnemyKind.Wisp)
                        {
                            // wisps weave side to side, harder to hit with spells
                            var side = new Vector2(-dir.y, dir.x);
                            dir = (dir + side * Mathf.Sin(e.wobble * 1.3f) * 0.8f).normalized;
                        }
                        // hounds that see you crouch and lunge
                        if (e.kind == EnemyKind.Hound && sees && d < 130 && d > 40 && e.lungeCd <= 0) { e.lunge = 0.35f; e.lungeCd = 2.5f; }
                        float spd = e.speed * (e.lunge > 0 ? 2.6f : 1);
                        e.pos = RoomPhysics.Move(e.pos, dir * spd * edt, e.r, room, doorsOpen);
                    }
                }

                // solid bodies: push out of the player and out of each other
                var off = e.pos - p.pos;
                float dist = off.magnitude, minD = e.r + PlayerR;
                if (dist < minD && p.jumpT <= 0)
                {
                    var n = dist > 0.01f ? off / dist : Vector2.right;
                    e.pos = RoomPhysics.Move(e.pos, n * (minD - dist), e.r, room, doorsOpen);
                }
                foreach (var o in Enemies)
                {
                    if (o == e || !o.alive) continue;
                    var sep = e.pos - o.pos;
                    float sd = sep.magnitude, need = e.r + o.r;
                    if (sd >= need) continue;
                    var n = sd > 0.01f ? sep / sd : Vector2.up;
                    e.pos = RoomPhysics.Move(e.pos, n * (need - sd) * 0.5f, e.r, room, doorsOpen);
                }

                // touch damage
                if (d < e.r + PlayerR + 4 && e.atkCd <= 0 && !untouchable && !scared)
                {
                    HurtPlayer(e.dmg, 0.4f);
                    e.atkCd = 1;
                    if (e.kind != EnemyKind.Wisp && e.bossForm < 0) e.atkAnim = 0.4f;
                    untouchable = true;
                }
            }
        }

        void UpdateEnemyShots(float dt, float slow)
        {
            var room = Room;
            var p = Player;
            foreach (var s in EnemyShots)
            {
                if (!s.alive) continue;
                s.pos += s.vel * dt * slow;
                s.life -= dt * slow;
                if (s.life <= 0 || RoomPhysics.WallAt(room, s.pos, false)) { s.alive = false; continue; }
                if (Vector2.Distance(s.pos, p.pos) >= s.r + PlayerR) continue;
                if (p.shieldT > 0)           // Nikolay's mug-shield
                {
                    s.alive = false; Sfx("shield");
                    Burst(s.pos, 6, 70, 0.3f, new Color32(220, 200, 160, 255));
                    continue;
                }
                if (p.invuln > 0 || p.jumpT > 0) continue;
                s.alive = false;
                HurtPlayer(s.damage, 0.5f);
            }
            EnemyShots.RemoveAll(s => !s.alive);
        }

        void KillEnemy(Enemy e)
        {
            var p = Player;
            e.alive = false;
            Kills++;
            p.xp += e.xp;
            Sfx("pickup");
            var corpse = e.Clone();
            corpse.dying = true; corpse.deathT = 0; corpse.hitFlash = 0; corpse.atkAnim = 0;
            Corpses.Add(corpse);
            int n = e.bossForm == 0 ? 12 + 2 * FloorNumber : e.bossForm > 0 ? 1 : 1 + _rng.Range(0, 2);
            n += Kit.extraCoins;
            for (int k = 0; k < n; k++) Coins.Add(new Coin { pos = e.pos, vel = new Vector2(_rng.F(-90, 90), _rng.F(-140, -40)) });
            Burst(e.pos, 14, 120, 0.7f, e.color);
            if (e.bossForm == 0) Shake = 0.5f;
            Fight.OnDeath(e, _spawned);

            while (p.xp >= p.xpNext)
            {
                p.xp -= p.xpNext; p.level++;
                p.xpNext = (int)(60 * Mathf.Pow(p.level, 1.5f));
                p.maxHp += 18; p.hp = p.maxHp;
                p.maxMana += 10; p.mana = p.maxMana;
                ShowBanner("The Unmarked grows stronger", $"Level {p.level}", 2.5f);
                Sfx("levelup");
            }
        }

        // =====================================================================
        // Loot, doors, the market
        // =====================================================================
        public static float DeathLength(Enemy c) => c.bossForm >= 0 && c.kind != EnemyKind.Vampire ? 0.45f : c.kind == EnemyKind.Wisp ? 0.5f : 1.0f;

        void UpdateLoot(float dt)
        {
            for (int i = Particles.Count - 1; i >= 0; i--)
            {
                var pt = Particles[i];
                pt.pos += pt.vel * dt; pt.life -= dt;
                if (pt.life <= 0) Particles.RemoveAt(i); else Particles[i] = pt;
            }
            foreach (var c in Corpses) c.deathT += dt;
            Corpses.RemoveAll(c => c.deathT > DeathLength(c));
            ChestOpenT += dt;
            foreach (var pu in Popups) { pu.t += dt; pu.pos.y -= 26 * dt; }
            Popups.RemoveAll(pu => pu.t > 1.4f);
            foreach (var f in Floaters) { f.t += dt; f.pos.y -= 34 * dt; }
            Floaters.RemoveAll(f => f.t > 0.8f);

            var p = Player;
            foreach (var c in Coins)
            {
                c.t += dt;
                if (c.t < 0.45f) { c.vel.y += 360 * dt; c.pos += c.vel * dt; }       // pop out with a little gravity
                else                                                                   // then fly to the player
                {
                    var to = p.pos - c.pos;
                    float spd = 160 + 600 * (c.t - 0.45f);
                    c.pos += to.normalized * Mathf.Min(spd * dt, to.magnitude);
                }
            }
            int before = Coins.Count;
            Coins.RemoveAll(c => c.t > 0.45f && Vector2.Distance(c.pos, p.pos) < 8);
            if (Coins.Count < before) { p.gold += before - Coins.Count; Sfx("coin"); }
        }

        void CheckRoomExits()
        {
            var r = Room;
            if (!r.cleared) return;
            float dcx = (RoomTW / 2) * Tile + Tile * 0.5f, dcy = (RoomTH / 2) * Tile + Tile * 0.5f;
            var p = Player;
            bool moved = true;
            if ((r.doors & Dir.N) != 0 && p.pos.y < Tile + 8 && Mathf.Abs(p.pos.x - dcx) < Tile) { Floor.cy--; p.pos = new Vector2(dcx, (RoomTH - 1) * Tile - 28); }
            else if ((r.doors & Dir.S) != 0 && p.pos.y > (RoomTH - 1) * Tile - 8 && Mathf.Abs(p.pos.x - dcx) < Tile) { Floor.cy++; p.pos = new Vector2(dcx, Tile + 28); }
            else if ((r.doors & Dir.W) != 0 && p.pos.x < Tile + 8 && Mathf.Abs(p.pos.y - dcy) < Tile) { Floor.cx--; p.pos = new Vector2((RoomTW - 1) * Tile - 28, dcy); }
            else if ((r.doors & Dir.E) != 0 && p.pos.x > (RoomTW - 1) * Tile - 8 && Mathf.Abs(p.pos.y - dcy) < Tile) { Floor.cx++; p.pos = new Vector2(Tile + 28, dcy); }
            else moved = false;
            if (moved) EnterRoom();
        }

        /// <summary>Buys Marin's ware i. Returns false (and says why) when it can't be bought.</summary>
        public bool Buy(int i)
        {
            var it = Defs.Shop[i];
            var p = Player;
            if (it.skill != Skill.None && p.Has(it.skill)) return false;
            if (p.gold < it.price) { ShopMessageT = 1.5f; ShopMessage = "Not enough gold"; Sfx("hurt"); return false; }
            p.gold -= it.price;
            if (it.skill != Skill.None) { p.skills |= it.skill; Sfx("levelup"); }
            else if (it.vape >= 0) { p.vapeT[it.vape] = Defs.VapeTime[it.vape]; Sfx("pickup"); }
            else
            {
                if (i == 0) p.hp = Mathf.Min(p.maxHp, p.hp + 35);
                else p.mana = Mathf.Min(p.maxMana, p.mana + 50);
                Sfx("pickup");
            }
            ShopMessageT = 1.5f; ShopMessage = $"Bought {it.name}";
            return true;
        }
    }
}
