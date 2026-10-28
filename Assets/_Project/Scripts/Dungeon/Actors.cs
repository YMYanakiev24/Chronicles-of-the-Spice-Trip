/*
 * Actors.cs
 * ----------------------------------------------------------------------------
 * The player, monsters, projectiles and loot of the Dungeon mode. The player
 * is serialisable because the run save stores it whole.
 * ----------------------------------------------------------------------------
 */

using System;
using UnityEngine;

namespace SpiceTrip.Dungeon
{
    /// <summary>Sprite sets in the dungeon pack.</summary>
    public enum EnemyKind { Skeleton, Hound, Vampire, Wisp, Thief, Melon }

    public enum ShotKind
    {
        Spell,      // the player's god-powers
        Coin,       // Daniil & Vasil's coin barrage
        Koi,        // the Storyteller's koi
        Orb,        // a plasmodium's glob
        Feather,    // Mordekai's fan
        Ticket,     // Vlado's tickets
        Ember,      // the Cinder-Maw's fire
    }

    [Serializable]
    public class Player
    {
        public Vector2 pos, facing = new Vector2(0, 1);
        public int level = 1, xp, xpNext = 60;
        public float maxHp = 100, hp = 100, maxMana = 100, mana = 100;
        public int selected, gold;
        public Skill skills;
        public float[] vapeT = new float[4];
        public string champion = "hero";

        // Timers and the champion's special (not worth saving, but harmless to).
        public float castCd, meleeCd, meleeAnim, roll, rollCd, invuln, hitFlash;
        public Vector2 rollDir = new Vector2(0, 1), dashDir = new Vector2(0, 1);
        public float specialCd, auraT, dashT, shieldT, rollAtkT, wobbleT, jumpT;
        public float charge;            // coin power (Daniil & Vasil) or fairy dust (the Fairy)
        public int rollId;
        [NonSerialized] public bool moving;                     // walking this frame (the sprite faces moveDir)
        [NonSerialized] public Vector2 moveDir = new Vector2(0, 1);

        public bool Has(Skill s) => (skills & s) != 0;
        public bool Buzzing(Vape v) => vapeT[(int)v] > 0;
        public ChampionKit Kit => Defs.Champion(champion);
    }

    public class Enemy
    {
        public Vector2 pos;
        public float hp = 1, maxHp = 1, speed, r = 12;
        public int dmg, xp;
        public Color color;
        public string name = "", weakness;
        public bool boss, alive = true;
        public float hitFlash, atkCd, wobble, lunge, lungeCd, flankSide = 1;
        public EnemyKind kind;
        public float animT, atkAnim, deathT;
        public bool dying, flip, moving;
        public int lastRollHit = -1;        // Pupesh's roll hits each enemy once per roll

        // Boss bodies (bossForm >= 0) are moved and fired by the BossFight.
        public int bossForm = -1;           // 0 = the boss, 1 = a copy, 2 = a tiny copy
        public float scale = 1;
        public Vector2 roamTo;
        public float moveCd, shootCd = 1.2f, fireInterval = 1.1f;
        public int spread = 3, volleys;
        public float diveT;
        public Vector2 diveDir;
        public bool chaser;

        public Enemy Clone() => (Enemy)MemberwiseClone();
    }

    public class Shot
    {
        public Vector2 pos, vel;
        public int damage;
        public Color color, glow;
        public string school;
        public float life = 1.4f, r = 6;
        public bool alive = true;
        public ShotKind kind = ShotKind.Spell;
        public float homing;                // turn rate toward the nearest target (rad/s)
    }

    public struct Particle { public Vector2 pos, vel; public float life, maxLife, size; public Color color; }
    public class Coin { public Vector2 pos, vel; public float t; }
    public class Popup { public bool mana; public Vector2 pos; public float t; }       // a flask rising from a chest
    public class Floater { public Vector2 pos; public float t; public int value; public Color color; }
}
