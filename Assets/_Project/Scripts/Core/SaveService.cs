/*
 * SaveService.cs
 * ----------------------------------------------------------------------------
 * Persists the same three stores as the web game's SaveManager.js, as JSON
 * files in Application.persistentDataPath instead of localStorage:
 *   save.json      the RPG chronicle (Phase 2)
 *   settings.json  volumes and options (survive a deleted save)
 *   hub.json       Marin's wallet, the stash of shop purchases, defeated
 *                  bosses and (new in Unity) the selected champion
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
    public class HubData
    {
        public int gold;
        public List<StashEntry> stash = new List<StashEntry>();
        public List<string> defeated = new List<string>();
        public string selectedChar = "hero";

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
        public float mouseSensitivity = 1.0f;
        public bool shadows = true;
        public bool muted = false;
    }

    public class SaveService
    {
        public const string SaveFile = "save.json";
        public const string SettingsFile = "settings.json";
        public const string HubFile = "hub.json";

        readonly string _dir;

        public SaveService(string directory = null)
        {
            _dir = directory ?? Application.persistentDataPath;
        }

        public string Directory => _dir;
        string PathOf(string file) => Path.Combine(_dir, file);

        public bool HasSave() => File.Exists(PathOf(SaveFile));
        public void ClearSave() { try { File.Delete(PathOf(SaveFile)); } catch (Exception) { /* ignore */ } }

        public HubData LoadHub(int startingGold)
        {
            var hub = Read<HubData>(HubFile);
            if (hub == null) return new HubData { gold = startingGold };
            hub.stash ??= new List<StashEntry>();
            hub.defeated ??= new List<string>();
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
