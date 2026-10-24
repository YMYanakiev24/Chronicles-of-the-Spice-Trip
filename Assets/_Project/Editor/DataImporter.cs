/*
 * DataImporter.cs
 * ----------------------------------------------------------------------------
 * Generates Resources/Data/GameDatabase.asset from source-data.json (exported
 * from the web game's JS). The only values not in the source are the portrait
 * crops below, chosen by looking at each character sheet: the JS drew the
 * whole sheet squashed into a square, which shows the titles and pose strips.
 * ----------------------------------------------------------------------------
 */

using System.Collections.Generic;
using System.IO;
using SpiceTrip.Data;
using UnityEditor;
using UnityEngine;

namespace SpiceTrip.EditorTools
{
    public static class DataImporter
    {
        public const string DatabasePath = "Assets/_Project/Resources/Data/GameDatabase.asset";
        public const string AbilitiesPath = "Assets/_Project/Data/Source/abilities.json";

        [System.Serializable]
        class AbilityFile
        {
            public List<BossKitDef> bosses = new List<BossKitDef>();
            public List<ChampionKitDef> champions = new List<ChampionKitDef>();
        }

        // Normalised crops (x, y, w, h; top-left origin) around each sheet's main figure,
        // square in pixels so the arena sprites keep their proportions.
        public static readonly Dictionary<string, Rect> PortraitCrops = new Dictionary<string, Rect>
        {
            // champions
            ["atanas"] = new Rect(0.20f, 0.000f, 0.60f, 0.277f),       // 443×960: head and shoulders portrait
            ["gakev"] = new Rect(0.18f, 0.060f, 0.58f, 0.435f),        // 720×960: crouching figure
            ["nikolay"] = new Rect(0.14f, 0.100f, 0.76f, 0.760f),      // 720×720: barrel knight
            ["pupesh"] = new Rect(0.10f, 0.085f, 0.80f, 0.369f),       // 443×960: melon warrior
            ["ristyo"] = new Rect(0.17f, 0.220f, 0.58f, 0.435f),       // 720×960: portrait at the bar
            ["iliqnpuh"] = new Rect(0.08f, 0.250f, 0.52f, 0.240f),     // 443×960: bear with the honey hose
            ["daniilvasil"] = new Rect(0.24f, 0.000f, 0.546f, 1.000f), // 720×393: the duo
            ["storyteller"] = new Rect(0.14f, 0.150f, 0.66f, 0.495f),  // 720×960: hot tub
            ["fairy"] = new Rect(0.38f, 0.320f, 0.56f, 0.258f),        // 443×960: fairy by the trailer
            // bosses
            ["goodPlaz"] = new Rect(0.00f, 0.325f, 0.50f, 0.231f),     // 443×960: form 1
            ["badPlaz"] = new Rect(0.04f, 0.300f, 0.49f, 0.226f),      // 443×960: form 1 (left of the "multiplies" caption)
            ["mordekai"] = new Rect(0.22f, 0.300f, 0.70f, 0.323f),     // 443×960: bird-man
            ["vlado"] = new Rect(0.18f, 0.170f, 0.62f, 0.286f),        // 443×960: the conductor
        };

        [MenuItem("Spice Trip/Import Game Data")]
        public static GameDatabase Import()
        {
            var src = SourceData.Load();
            var db = AssetDatabase.LoadAssetAtPath<GameDatabase>(DatabasePath);
            bool create = db == null;
            if (create) db = ScriptableObject.CreateInstance<GameDatabase>();

            db.items.Clear();
            foreach (var i in src.items)
                db.items.Add(new ItemDef
                {
                    id = i.id, name = i.name, type = i.type, desc = i.desc,
                    heal = i.heal, mana = i.mana, fireDamage = i.passive?.fireDamage ?? 0,
                });

            db.bosses.Clear();
            foreach (var b in src.bosses)
                db.bosses.Add(new BossDef
                {
                    id = b.id, name = b.name, img = FileName(b.img), hp = b.hp, color = b.color,
                    cx = b.cx, cy = b.cy, portraitRect = Crop(b.id),
                });

            db.characters.Clear();
            foreach (var c in src.characters)
                db.characters.Add(new CharacterDef
                {
                    id = c.id, name = c.name, img = FileName(c.img), empty = c.empty, portraitRect = Crop(c.id),
                });

            db.shopStock.Clear();
            foreach (var s in src.shopStock) db.shopStock.Add(new ShopEntry { id = s.id, price = s.price });

            db.tips = new List<string>(src.tips);
            db.prophecy = new List<string>(src.prophecy);
            db.hubStartingGold = src.hubStartingGold;
            db.battle = new BattleTuning();
            db.loom = new LoomTuning();

            var rpg = SourceData.LoadRpg();
            db.gods = src.gods;
            db.regions = src.regions;
            db.regionOrder = src.regionOrder;
            db.spells = src.spells;
            db.spellOrder = src.spellOrder;
            db.creatures = rpg.creatures;
            db.quests = src.quests;
            db.questOrder = src.questOrder;
            db.npcs = rpg.npcs;

            var kits = JsonUtility.FromJson<AbilityFile>(File.ReadAllText(AbilitiesPath));
            db.bossKits = kits.bosses;
            db.championKits = kits.champions;

            if (create)
            {
                Directory.CreateDirectory(Path.GetDirectoryName(DatabasePath));
                AssetDatabase.CreateAsset(db, DatabasePath);
            }
            EditorUtility.SetDirty(db);
            AssetDatabase.SaveAssets();
            Debug.Log($"GameDatabase: {db.items.Count} items, {db.bosses.Count} bosses, {db.characters.Count} characters, {db.shopStock.Count} shop entries, " +
                $"{db.regions.Count} regions, {db.spells.Count} spells, {db.creatures.Count} creatures, {db.quests.Count} quests, {db.npcs.Count} NPCs, " +
                $"{db.bossKits.Count} boss kits, {db.championKits.Count} champion kits");
            return db;
        }

        static string FileName(string img) => string.IsNullOrEmpty(img) ? null : Path.GetFileName(img);
        static Rect Crop(string id) => PortraitCrops.TryGetValue(id, out var r) ? r : new Rect(0, 0, 1, 1);
    }
}
