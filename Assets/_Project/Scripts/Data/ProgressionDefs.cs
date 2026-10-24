/*
 * ProgressionDefs.cs
 * ----------------------------------------------------------------------------
 * Boss-fight progression (from Data/Source/progression.json): the gold bounty
 * each boss pays into Marin's wallet, its par time for the third star, the
 * Gauntlet (all four bosses back to back) and the battle items Marin sells.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;

namespace SpiceTrip.Data
{
    [Serializable]
    public class BountyDef
    {
        public string id;
        public int first, repeat;    // gold for the first victory / every later one
        public float par;            // seconds; winning within par earns the third star
    }

    [Serializable]
    public class GauntletDef
    {
        public List<string> order = new List<string>();
        public int first, repeat;
        public float par;
        public float healBetween;    // health restored between stages
    }

    /// <summary>effect: heal | swift | ward</summary>
    [Serializable]
    public class BattleItemDef
    {
        public string id, name, icon, desc, effect;
        public int price, max;
        public float value, duration;
    }
}
