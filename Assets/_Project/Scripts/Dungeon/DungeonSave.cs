/*
 * DungeonSave.cs
 * ----------------------------------------------------------------------------
 * The Dungeon's two files, next to the other saves: dungeon.json (the run in
 * progress) and dungeon-profile.json (records kept between runs).
 * ----------------------------------------------------------------------------
 */

using System;
using System.IO;
using SpiceTrip.Core;
using UnityEngine;

namespace SpiceTrip.Dungeon
{
    public static class DungeonSave
    {
        public const string RunFile = "dungeon.json", ProfileFile = "dungeon-profile.json";

        static string PathOf(SaveService s, string file) => Path.Combine(s.Directory, file);

        public static bool HasRun(SaveService s) => File.Exists(PathOf(s, RunFile));

        public static void WriteRun(SaveService s, DungeonRunSave run) => Write(PathOf(s, RunFile), run);
        public static DungeonRunSave ReadRun(SaveService s)
        {
            var run = Read<DungeonRunSave>(PathOf(s, RunFile));
            return run?.player != null && run.floor?.rooms != null && run.floor.rooms.Length == Geo.FloorSize * Geo.FloorSize ? run : null;
        }
        public static void DeleteRun(SaveService s)
        {
            try { File.Delete(PathOf(s, RunFile)); } catch (Exception) { /* nothing to delete */ }
        }

        public static void WriteProfile(SaveService s, DungeonProfile p) => Write(PathOf(s, ProfileFile), p);
        public static DungeonProfile ReadProfile(SaveService s) => Read<DungeonProfile>(PathOf(s, ProfileFile)) ?? new DungeonProfile();

        static void Write(string path, object value)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                File.WriteAllText(path, JsonUtility.ToJson(value));
            }
            catch (Exception e) { Debug.LogWarning($"Could not save {path}: {e.Message}"); }
        }

        static T Read<T>(string path) where T : class
        {
            try { return File.Exists(path) ? JsonUtility.FromJson<T>(File.ReadAllText(path)) : null; }
            catch (Exception e) { Debug.LogWarning($"Could not read {path}: {e.Message}"); return null; }
        }
    }
}
