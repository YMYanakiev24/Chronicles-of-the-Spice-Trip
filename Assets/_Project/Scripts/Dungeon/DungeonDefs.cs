/*
 * DungeonDefs.cs
 * ----------------------------------------------------------------------------
 * The Dungeon mode: a port of the C++ raylib game (the repo's main branch).
 * Pick a champion, fight down through floors of single-screen rooms, and beat
 * the boss at the bottom of each: the four Path bosses in their painted arenas,
 * then Pyraketh, the Cinder-Maw.
 *
 * This file holds the fixed tables: room geometry, spells, Marin's wares,
 * champion perks and specials, and the five bosses. Positions are room pixels
 * with y pointing down, as in the C++ game.
 * ----------------------------------------------------------------------------
 */

using System;
using UnityEngine;

namespace SpiceTrip.Dungeon
{
    public static class Geo
    {
        public const int Tile = 32;
        public const int RoomTW = 21, RoomTH = 11;     // room size in tiles, walls included
        public const int FloorSize = 9;                 // a floor is FloorSize x FloorSize rooms
        public const float RoomW = RoomTW * Tile, RoomH = RoomTH * Tile;
        public const float Walk = 130, Run = 210, RollSpeed = 360, PlayerR = 11;
        public static readonly Vector2 Center = new Vector2(RoomW / 2, RoomH / 2);
        public static readonly Vector2 MerchantPos = new Vector2(RoomW / 2, RoomH / 2 - 10);
        public static readonly Vector2 OraclePos = new Vector2(RoomW / 2 - 90, RoomH / 2 - 40);
    }

    /// <summary>raylib's GetRandomValue/frand over a seeded System.Random, so tests are repeatable.</summary>
    public class DRandom
    {
        readonly System.Random _r;
        public DRandom(int seed) { _r = new System.Random(seed); }
        public int Range(int lo, int hiInclusive) => _r.Next(lo, hiInclusive + 1);
        public float F(float a, float b) => a + (float)_r.NextDouble() * (b - a);
        public bool Chance(float p) => _r.NextDouble() < p;
    }

    public struct SpellDef
    {
        public string name, school, sfx;
        public int manaCost, damage;
        public float speed;
        public Color color, glow;
    }

    [Flags]
    public enum Skill { None = 0, IronSkin = 1, Swift = 2, Blade = 4, Arcane = 8, Dash = 16, Leech = 32 }

    public enum Vape { Mango, Razz, Melon, Mint }

    public struct ShopItem
    {
        public string name, desc;
        public int price;
        public Skill skill;      // None = a consumable bought again and again
        public int vape;         // >= 0: a timed vape buff
    }

    public enum Special { ArcaneSurge, Aura, CoinDash, MugShield, MelonRoll, Drink, BearJump, CoinBarrage, Koi, Blink }

    public class ChampionKit
    {
        public string id;                    // CharacterDef id (shared with the Changing Room)
        public string perk, special, specialDesc;
        public Special kind;
        public float cooldown;
        public float hpBonus, manaCostMul = 1, castCdMul = 1, speedMul = 1, stillRegen, spellHoming;
        public int bladeBonus, extraCoins;
    }

    public enum BossKind { Split, Feathers, Conductor, Inferno }

    public class DungeonBoss
    {
        public string id;                    // BossDef id for its art (null: the Cinder-Maw)
        public string name, epithet, weakness;
        public BossKind kind;
        public float hp, radius, speed, fireInterval;
        public int contact, spread;
        public Color color;
    }

    public static class Defs
    {
        static Color C(int r, int g, int b) => new Color32((byte)r, (byte)g, (byte)b, 255);

        public static readonly SpellDef[] Spells =
        {
            new SpellDef { name = "Emberhex", school = "Fire", manaCost = 12, damage = 22, speed = 460, color = C(255, 122, 58), glow = C(255, 208, 137), sfx = "fire" },
            new SpellDef { name = "Tidelash", school = "Water", manaCost = 10, damage = 16, speed = 420, color = C(79, 210, 200), glow = C(191, 246, 238), sfx = "nature" },
            new SpellDef { name = "Stormbolt", school = "Lightning", manaCost = 24, damage = 40, speed = 900, color = C(159, 208, 255), glow = C(234, 255, 255), sfx = "lightning" },
        };

        public static readonly ShopItem[] Shop =
        {
            new ShopItem { name = "Health Potion", desc = "Heals 35 HP (never above max)", price = 8, vape = -1 },
            new ShopItem { name = "Mana Potion", desc = "Restores 50 mana", price = 6, vape = -1 },
            new ShopItem { name = "Mango Haze", desc = "Vape: move 35% faster for 20s", price = 12, vape = (int)Vape.Mango },
            new ShopItem { name = "Blue Razz Ice", desc = "Vape: powers cost no mana for 12s", price = 18, vape = (int)Vape.Razz },
            new ShopItem { name = "Watermelon Rage", desc = "Vape: +12 blade damage for 20s", price = 15, vape = (int)Vape.Melon },
            new ShopItem { name = "Cool Mint Fog", desc = "Vape: take 50% less damage for 15s", price = 15, vape = (int)Vape.Mint },
            new ShopItem { name = "Iron Skin", desc = "Take 25% less damage", price = 30, skill = Skill.IronSkin, vape = -1 },
            new ShopItem { name = "Swift Feet", desc = "Move 15% faster", price = 20, skill = Skill.Swift, vape = -1 },
            new ShopItem { name = "Sharpened Blade", desc = "+8 blade damage", price = 25, skill = Skill.Blade, vape = -1 },
            new ShopItem { name = "Arcane Flow", desc = "Mana regenerates 60% faster", price = 25, skill = Skill.Arcane, vape = -1 },
            new ShopItem { name = "Shadow Step", desc = "Dodge-roll recharges twice as fast", price = 15, skill = Skill.Dash, vape = -1 },
            new ShopItem { name = "Leeching Edge", desc = "Blade hits heal 2 HP", price = 35, skill = Skill.Leech, vape = -1 },
        };

        public static readonly float[] VapeTime = { 20, 12, 20, 15 };
        public static readonly Color[] VapeColor = { C(255, 176, 64), C(96, 160, 255), C(240, 80, 96), C(170, 250, 220) };

        // Coin Barrage and Blink run on a meter as well as a cooldown.
        public const float CoinPowerMax = 100, CoinPowerPerHit = 6;
        public const float DustMax = 50, DustRegen = 4, DustPerBlink = 10;

        public static readonly ChampionKit[] Champions =
        {
            new ChampionKit { id = "hero", perk = "Powers cost 25% less mana.",
                special = "Arcane Surge", specialDesc = "Refill 40 mana.", kind = Special.ArcaneSurge, cooldown = 12, manaCostMul = 0.75f },
            new ChampionKit { id = "atanas", perk = "+20 max health.",
                special = "Aura of Calmness", specialDesc = "Heal 25. Enemies and their shots move at half speed for 3 s.", kind = Special.Aura, cooldown = 10, hpBonus = 20 },
            new ChampionKit { id = "gakev", perk = "Every kill drops an extra coin.",
                special = "Coin Dash", specialDesc = "Dash a short way, untouchable.", kind = Special.CoinDash, cooldown = 4, extraCoins = 1 },
            new ChampionKit { id = "nikolay", perk = "+25 max health.",
                special = "Mug-Shield", specialDesc = "For 2 s, enemy shots shatter against you.", kind = Special.MugShield, cooldown = 6, hpBonus = 25 },
            new ChampionKit { id = "pupesh", perk = "+4 blade damage.",
                special = "Melon Roll", specialDesc = "Roll forward, untouchable, hitting each enemy for 20.", kind = Special.MelonRoll, cooldown = 5, bladeBonus = 4 },
            new ChampionKit { id = "ristyo", perk = "Casts 30% faster.",
                special = "Drink", specialDesc = "Heal 20, but your aim wobbles for 4 s.", kind = Special.Drink, cooldown = 6, castCdMul = 0.7f },
            new ChampionKit { id = "iliqnpuh", perk = "Moves 10% faster.",
                special = "Bear Jump", specialDesc = "Leap, untouchable, and slam down for 18 around you.", kind = Special.BearJump, cooldown = 3, speedMul = 1.1f },
            new ChampionKit { id = "daniilvasil", perk = "Twin strike: +5 blade damage.",
                special = "Coin Barrage", specialDesc = "Your hits charge it. When full, fire a ring of 16 coins.", kind = Special.CoinBarrage, cooldown = 0.5f, bladeBonus = 5 },
            new ChampionKit { id = "storyteller", perk = "Heals 2 health a second while standing still.",
                special = "Release the Koi", specialDesc = "Three koi swim out and hunt the nearest enemies.", kind = Special.Koi, cooldown = 7, stillRegen = 2 },
            new ChampionKit { id = "fairy", perk = "Spells curve toward enemies.",
                special = "Blink", specialDesc = "Teleport to the cursor for 10 fairy dust.", kind = Special.Blink, cooldown = 0.3f, spellHoming = 2.5f },
        };

        public static ChampionKit Champion(string id) => Array.Find(Champions, c => c.id == id) ?? Champions[0];

        public static readonly DungeonBoss[] Bosses =
        {
            new DungeonBoss { id = "goodPlaz", name = "The Good Plazmodium", epithet = "Multiplies when it dies", kind = BossKind.Split,
                hp = 260, radius = 26, speed = 80, contact = 12, color = C(95, 208, 106), weakness = "Fire", spread = 3, fireInterval = 1.1f },
            new DungeonBoss { id = "badPlaz", name = "The Bad Plasmodium", epithet = "Multiplies when it dies", kind = BossKind.Split,
                hp = 320, radius = 26, speed = 80, contact = 14, color = C(154, 95, 208), weakness = "Lightning", spread = 5, fireInterval = 1.1f },
            new DungeonBoss { id = "mordekai", name = "Mordekai", epithet = "Fans of feathers, and a dive", kind = BossKind.Feathers,
                hp = 380, radius = 26, speed = 85, contact = 14, color = C(74, 122, 208), weakness = "Fire", spread = 7, fireInterval = 1.6f },
            new DungeonBoss { id = "vlado", name = "Vlado", epithet = "The conductor of bus No.9", kind = BossKind.Conductor,
                hp = 440, radius = 26, speed = 70, contact = 14, color = C(90, 130, 200), weakness = "Lightning", spread = 0, fireInterval = 2.0f },
            new DungeonBoss { id = null, name = "Pyraketh, the Cinder-Maw", epithet = "Lord of the lowest floor", kind = BossKind.Inferno,
                hp = 520, radius = 24, speed = 55, contact = 26, color = C(255, 120, 50), weakness = "Water", spread = 3, fireInterval = 1.6f },
        };

        /// <summary>Floors 1-5 meet each boss in turn, then the cycle repeats.</summary>
        public static int BossIndexForFloor(int floor) => (floor - 1) % Bosses.Length;

        /// <summary>x1 on floors 1-5, x1.5 on 6-10, and so on.</summary>
        public static float BossStrength(int floor) => 1f + 0.5f * ((floor - 1) / Bosses.Length);
    }
}
