/*
 * SaveService.cs
 * ----------------------------------------------------------------------------
 * Persists the game's stores as JSON files in Application.persistentDataPath
 * (the web game's SaveManager.js used localStorage):
 *   settings.json  volumes and mute
 *   hub.json       Marin's wallet, the stash of shop purchases, defeated
 *                  bosses and (new in Unity) the selected champion
 * The Dungeon keeps its own two files next to these (DungeonSave).
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace SpiceTrip.Core
{
    [Serializable]
    public class StashEntry
    {
        public string id;
        public int count;
    }

    [Serializable]
    public class BossRecord
    {
        public string id;
        public int wins, bestStars;
        public float bestTime;   // seconds; 0 = none yet
    }

    [Serializable]
    public class HubData
    {
        public int gold;
        public List<StashEntry> stash = new List<StashEntry>();
        public List<string> defeated = new List<string>();
        public string selectedChar = "hero";
        public List<BossRecord> records = new List<BossRecord>();      // per boss, plus "gauntlet"
        public List<StashEntry> battleSatchel = new List<StashEntry>(); // Marin's battle items

        public BossRecord Record(string id)
        {
            var r = records.Find(x => x.id == id);
            if (r == null) { r = new BossRecord { id = id }; records.Add(r); }
            return r;
        }

        public int SatchelCount(string id) => battleSatchel.Find(s => s.id == id)?.count ?? 0;

        public void SetSatchel(string id, int count)
        {
            var e = battleSatchel.Find(s => s.id == id);
            if (e == null) battleSatchel.Add(new StashEntry { id = id, count = count });
            else e.count = count;
        }

        public int StashCount(string id) => stash.Find(s => s.id == id)?.count ?? 0;

        public void AddToStash(string id, int n = 1)
        {
            var e = stash.Find(s => s.id == id);
            if (e == null) stash.Add(new StashEntry { id = id, count = n });
            else e.count += n;
        }
    }

    [Serializable]
    public class SettingsData
    {
        public float musicVolume = 0.5f;
        public float sfxVolume = 0.7f;
        public bool muted = false;
    }

    public class SaveService
    {
        public const string SettingsFile = "settings.json";
        public const string HubFile = "hub.json";

        readonly string _dir;

        public SaveService(string directory = null)
        {
            _dir = directory ?? Application.persistentDataPath;
        }

        public string Directory => _dir;
        string PathOf(string file) => Path.Combine(_dir, file);


        public HubData LoadHub(int startingGold)
        {
            var hub = Read<HubData>(HubFile);
            if (hub == null) return new HubData { gold = startingGold };
            hub.stash ??= new List<StashEntry>();
            hub.defeated ??= new List<string>();
            hub.records ??= new List<BossRecord>();
            hub.battleSatchel ??= new List<StashEntry>();
            if (string.IsNullOrEmpty(hub.selectedChar)) hub.selectedChar = "hero";
            return hub;
        }

        public void SaveHub(HubData hub) => Write(HubFile, hub);

        public SettingsData LoadSettings() => Read<SettingsData>(SettingsFile) ?? new SettingsData();
        public void SaveSettings(SettingsData s) => Write(SettingsFile, s);

        T Read<T>(string file) where T : class
        {
            try
            {
                var p = PathOf(file);
                return File.Exists(p) ? JsonUtility.FromJson<T>(File.ReadAllText(p)) : null;
            }
            catch (Exception e)
            {
                Debug.LogWarning($"Load of {file} failed: {e.Message}");
                return null;
            }
        }

        void Write(string file, object data)
        {
            try
            {
                System.IO.Directory.CreateDirectory(_dir);
                File.WriteAllText(PathOf(file), JsonUtility.ToJson(data, true));
            }
            catch (Exception e)
            {
                Debug.LogWarning($"Save of {file} failed: {e.Message}");
            }
        }
    }
}
