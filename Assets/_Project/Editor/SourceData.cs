/*
 * SourceData.cs
 * ----------------------------------------------------------------------------
 * The shape of Data/Source/source-data.json, written by
 * Tools~/export-source-data.mjs from the web game's JavaScript. Field names
 * match the JS objects so JsonUtility can read the file directly.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Data;
using UnityEngine;

namespace SpiceTrip.EditorTools
{
    [Serializable]
    public class SourceData
    {
        public List<SrcItem> items;
        public List<SrcBoss> bosses;
        public List<SrcCharacter> characters;
        public List<SrcShop> shopStock;
        public List<string> tips;
        public List<string> prophecy;
        public int hubStartingGold;
        public List<int> xpForLevel;
        public List<SrcCreature> creatures;
        public List<SrcNpc> npcs;

        // Full RPG definitions (parsed straight into the runtime types)
        public List<GodDef> gods;
        public List<RegionDef> regions;
        public List<string> regionOrder;
        public List<SpellDef> spells;
        public List<string> spellOrder;
        public List<QuestDef> quests;
        public List<string> questOrder;

        /// <summary>The same file read as runtime creature/NPC definitions.</summary>
        [Serializable]
        public class RpgView
        {
            public List<CreatureDef> creatures;
            public List<NpcDef> npcs;
        }

        public static RpgView LoadRpg() => JsonUtility.FromJson<RpgView>(System.IO.File.ReadAllText(AssetPath));

        public const string AssetPath = "Assets/_Project/Data/Source/source-data.json";

        public static SourceData Load()
        {
            var text = System.IO.File.ReadAllText(AssetPath);
            return JsonUtility.FromJson<SourceData>(text);
        }
    }

    [Serializable] public class SrcPassive { public float fireDamage; }

    [Serializable]
    public class SrcCreature
    {
        public string id, name, body, color;
        public float size;
        public bool boss;
    }

    [Serializable]
    public class SrcNpc
    {
        public string id, name, color;
    }

    [Serializable]
    public class SrcItem
    {
        public string id, name, type, icon, desc;
        public int heal, mana;
        public SrcPassive passive;
    }

    [Serializable]
    public class SrcBoss
    {
        public string id, name, img, color;
        public int hp;
        public float cx, cy;
    }

    [Serializable]
    public class SrcCharacter
    {
        public string id, name, img;
        public bool empty;
    }

    [Serializable]
    public class SrcShop
    {
        public string id;
        public int price;
    }
}
