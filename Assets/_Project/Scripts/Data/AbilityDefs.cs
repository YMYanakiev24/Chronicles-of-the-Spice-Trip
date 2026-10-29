/*
 * AbilityDefs.cs
 * ----------------------------------------------------------------------------
 * Phase 3: the boss kits and champion abilities designed from the character
 * sheets. The numbers come from Data/Source/abilities.json (imported into the
 * GameDatabase), so tuning never needs a code change.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;

namespace SpiceTrip.Data
{
    [Serializable]
    public class AbilityParam
    {
        public string k;
        public float v;
    }

    [Serializable]
    public abstract class KitBase
    {
        public string id;
        public List<AbilityParam> @params = new List<AbilityParam>();

        public float P(string key, float fallback = 0)
        {
            foreach (var p in @params) if (p.k == key) return p.v;
            return fallback;
        }
    }

    /// <summary>kit: split | feathers | conductor</summary>
    [Serializable]
    public class BossKitDef : KitBase
    {
        public string kit, name;
    }

    /// <summary>special: none | aura | dash | shield | roll | drink | jump | coinring | koi | teleport | scare</summary>
    [Serializable]
    public class ChampionKitDef : KitBase
    {
        public string ability, special;
        public float cooldown;
    }
}
