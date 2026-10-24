/*
 * RpgDefs.cs
 * ----------------------------------------------------------------------------
 * The RPG content from GameData.js: the Theurgoi, the six regions, the six
 * divine powers, the bestiary, the quest chain and the NPCs. Field names match
 * the JavaScript so the importer can read source-data.json directly
 * (`@base` and `@float` because those names are C# keywords).
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;

namespace SpiceTrip.Data
{
    [Serializable]
    public class GodDef
    {
        public string key, name, domain, color, lore;   // GODS is keyed by id in the JS; the exporter writes it as "key"
    }

    [Serializable] public class GroundDef { public string @base, accent; }
    [Serializable] public class FogDef { public string color; public float density; }
    [Serializable] public class TilePos { public int tx, ty; }

    [Serializable]
    public class RegionDef
    {
        public string id, name, subtitle, description, tileset, lightTint, music;
        public bool locked;
        public GroundDef ground;
        public float darkness;      // 0 = bright day, 1 = pitch black
        public FogDef fog;
        public List<string> enemies = new List<string>();
        public TilePos spawn;
    }

    [Serializable]
    public class SpellDef
    {
        public string id, name, school, god, kind, color, glow, desc, upgrade;   // kind: projectile | beam | self | aoe
        public float manaCost, cooldown, damage, speed, radius, splash, slow, range, heal, shield, duration;
        public int chain, unlockLevel;
    }

    [Serializable]
    public class CreatureDef
    {
        public string id, name, body, color, weakness, lore;
        public float hp, speed, damage, attackRange, attackRate, aggroRange, size;
        public int xp;
        public bool @float, ranged, boss;
    }

    [Serializable]
    public class ObjectiveDef
    {
        public string type;          // kill | collect | talk | reach | cast
        public string creature, spell, item, npc, loc, text;
        public int count;
    }

    [Serializable]
    public class RewardDef
    {
        public int xp, gold;
        public string spell;
        public List<string> items = new List<string>();
    }

    [Serializable]
    public class QuestDef
    {
        public string id, title, giver, region, summary, lore, next;
        public List<ObjectiveDef> objectives = new List<ObjectiveDef>();
        public RewardDef rewards = new RewardDef();
    }

    /// <summary>Quest items scattered in a realm (chronicle-extra.json).</summary>
    [Serializable]
    public class PickupDef
    {
        public string region, item, type, color;
        public int count;
    }

    [Serializable]
    public class NpcDef
    {
        public string id, name, region, color, role, greeting;
        public int tx, ty;
        public List<string> lines = new List<string>();
    }
}
