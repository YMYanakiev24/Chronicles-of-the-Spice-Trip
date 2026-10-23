/*
 * ChronicleSave.cs
 * ----------------------------------------------------------------------------
 * The save.json snapshot, same content as the web game's save: player stats,
 * learned powers and tiers, hotbar, inventory, relics, position, quest state
 * and progress, current and unlocked regions, and the bestiary. Dictionaries
 * become lists of pairs because JsonUtility can't serialise dictionaries.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using System.IO;
using SpiceTrip.Core;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    [Serializable] public class IdCount { public string id; public int count; }
    [Serializable] public class QuestProgress { public string id, state; public int[] progress; }

    [Serializable]
    public class PlayerSave
    {
        public int level, gold;
        public float xp, maxHp, hp, maxMana, mana, manaRegen;
        public List<string> learnedSpells = new List<string>();
        public List<IdCount> spellLevels = new List<IdCount>();
        public string[] hotbar = new string[6];
        public List<IdCount> inventory = new List<IdCount>();
        public List<string> relics = new List<string>();
        public bool hasPosition;
        public float x, y;
        public string facing;
    }

    [Serializable]
    public class ChronicleSave
    {
        public int version = 1;
        public long savedAt;
        public PlayerSave player = new PlayerSave();
        public List<QuestProgress> quests = new List<QuestProgress>();
        public string tracked;
        public string currentRegion;
        public List<string> unlockedRegions = new List<string>();
        public List<string> bestiary = new List<string>();

        public static ChronicleSave From(ChronicleGame g)
        {
            var p = g.Player;
            var s = new ChronicleSave
            {
                savedAt = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
                currentRegion = g.CurrentRegion,
                unlockedRegions = new List<string>(g.UnlockedRegions),
                bestiary = new List<string>(g.BestiarySeen),
                tracked = g.Quests.tracked,
            };
            var ps = s.player;
            ps.level = p.level; ps.xp = p.xp; ps.maxHp = p.maxHp; ps.hp = p.hp;
            ps.maxMana = p.maxMana; ps.mana = p.mana; ps.gold = p.gold; ps.manaRegen = p.manaRegen;
            ps.learnedSpells = new List<string>(p.learnedSpells);
            foreach (var kv in p.spellLevels) ps.spellLevels.Add(new IdCount { id = kv.Key, count = kv.Value });
            ps.hotbar = (string[])p.hotbar.Clone();
            foreach (var kv in p.inventory) ps.inventory.Add(new IdCount { id = kv.Key, count = kv.Value });
            ps.relics = new List<string>(p.relics);
            ps.hasPosition = true; ps.x = p.x; ps.y = p.y; ps.facing = p.facing;
            foreach (var id in g.Db.questOrder)
                s.quests.Add(new QuestProgress { id = id, state = g.Quests.state[id], progress = (int[])g.Quests.progress[id].Clone() });
            return s;
        }

        /// <summary>Restores everything except the map, which ChronicleGame reloads.</summary>
        public void ApplyTo(ChronicleGame g)
        {
            var p = g.Player;
            var ps = player;
            p.level = ps.level; p.xp = ps.xp; p.maxHp = ps.maxHp; p.hp = ps.hp;
            p.maxMana = ps.maxMana; p.mana = ps.mana; p.gold = ps.gold; p.manaRegen = ps.manaRegen;
            p.facing = string.IsNullOrEmpty(ps.facing) ? "down" : ps.facing;
            p.learnedSpells.Clear();
            foreach (var id in ps.learnedSpells) p.learnedSpells.Add(id);
            if (p.learnedSpells.Count == 0) p.learnedSpells.Add("emberhex");
            p.spellLevels.Clear();
            foreach (var e in ps.spellLevels) p.spellLevels[e.id] = e.count;
            if (p.spellLevels.Count == 0) p.spellLevels["emberhex"] = 1;
            for (int i = 0; i < 6; i++) p.hotbar[i] = ps.hotbar != null && i < ps.hotbar.Length && !string.IsNullOrEmpty(ps.hotbar[i]) ? ps.hotbar[i] : null;
            if (ps.hotbar == null) p.hotbar[0] = "emberhex";
            p.inventory.Clear();
            foreach (var e in ps.inventory) if (e.count > 0) p.inventory[e.id] = e.count;
            p.relics.Clear();
            foreach (var r in ps.relics) p.relics.Add(r);

            foreach (var q in quests)
            {
                if (!g.Quests.state.ContainsKey(q.id)) continue;
                g.Quests.state[q.id] = q.state;
                if (q.progress != null && q.progress.Length == g.Quests.progress[q.id].Length) g.Quests.progress[q.id] = q.progress;
            }
            if (!string.IsNullOrEmpty(tracked)) g.Quests.tracked = tracked;
            g.CurrentRegion = string.IsNullOrEmpty(currentRegion) ? "mistwood" : currentRegion;
            g.UnlockedRegions.Clear();
            foreach (var r in unlockedRegions) g.UnlockedRegions.Add(r);
            if (g.UnlockedRegions.Count == 0) g.UnlockedRegions.Add("mistwood");
            g.BestiarySeen.Clear();
            foreach (var b in bestiary) g.BestiarySeen.Add(b);
        }

        // --- file I/O (save.json next to hub.json and settings.json) -------------------
        public static string PathIn(SaveService s) => Path.Combine(s.Directory, SaveService.SaveFile);

        public void Write(SaveService s)
        {
            try
            {
                Directory.CreateDirectory(s.Directory);
                File.WriteAllText(PathIn(s), JsonUtility.ToJson(this, true));
            }
            catch (Exception e) { Debug.LogWarning($"Save failed: {e.Message}"); }
        }

        public static ChronicleSave Read(SaveService s)
        {
            try
            {
                var p = PathIn(s);
                return File.Exists(p) ? JsonUtility.FromJson<ChronicleSave>(File.ReadAllText(p)) : null;
            }
            catch (Exception e) { Debug.LogWarning($"Load failed: {e.Message}"); return null; }
        }
    }
}
