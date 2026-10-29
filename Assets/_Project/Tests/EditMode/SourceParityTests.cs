// Checks that the Unity data and procedural art match the web game they were
// ported from. The expected values come from the JavaScript itself, exported by
// Tools~/export-source-data.mjs and Tools~/pixel-parity.mjs.

using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using NUnit.Framework;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.EditorTools;
using UnityEditor;
using UnityEngine;

namespace SpiceTrip.Tests
{
    public class DataParityTests
    {
        SourceData _src;
        GameDatabase _db;

        [OneTimeSetUp]
        public void Load()
        {
            _src = SourceData.Load();
            _db = AssetDatabase.LoadAssetAtPath<GameDatabase>(DataImporter.DatabasePath);
            Assert.IsNotNull(_db, "GameDatabase asset missing: run Spice Trip ▸ Rebuild Project");
        }

        [Test]
        public void ItemsMatchGameDataJs()
        {
            Assert.AreEqual(_src.items.Count, _db.items.Count);
            for (int i = 0; i < _src.items.Count; i++)
            {
                var s = _src.items[i];
                var d = _db.items[i];
                Assert.AreEqual(s.id, d.id);
                Assert.AreEqual(s.name, d.name, s.id);
                Assert.AreEqual(s.type, d.type, s.id);
                Assert.AreEqual(s.desc, d.desc, s.id);
                Assert.AreEqual(s.heal, d.heal, s.id);
                Assert.AreEqual(s.mana, d.mana, s.id);
            }
            Assert.AreEqual(0.1f, _db.Item("cinderSigil").fireDamage, 1e-6);
        }

        [Test]
        public void BossesMatchScenesJs()
        {
            Assert.AreEqual(4, _db.bosses.Count);
            for (int i = 0; i < _src.bosses.Count; i++)
            {
                var s = _src.bosses[i];
                var d = _db.bosses[i];
                Assert.AreEqual(s.id, d.id);
                Assert.AreEqual(s.name, d.name);
                Assert.AreEqual(Path.GetFileName(s.img), d.img);
                Assert.AreEqual(s.hp, d.hp);
                Assert.AreEqual(s.color, d.color);
                Assert.AreEqual(s.cx, d.cx, 1e-6);
                Assert.AreEqual(s.cy, d.cy, 1e-6);
            }
            // spot-check against the literal table in the port prompt
            CollectionAssert.AreEqual(new[] { 120, 160, 140, 190 }, _db.bosses.ConvertAll(b => b.hp));
        }

        [Test]
        public void CharactersMatchScenesJs()
        {
            Assert.AreEqual(12, _db.characters.Count);
            for (int i = 0; i < _src.characters.Count; i++)
            {
                if (_src.characters[i].empty && !_db.characters[i].empty) continue;     // a new champion took this Coming soon card
                Assert.AreEqual(_src.characters[i].id, _db.characters[i].id);
                Assert.AreEqual(_src.characters[i].name, _db.characters[i].name);
                Assert.AreEqual(_src.characters[i].empty, _db.characters[i].empty);
                Assert.AreEqual(string.IsNullOrEmpty(_src.characters[i].img), string.IsNullOrEmpty(_db.characters[i].img));
            }
            Assert.AreEqual("momiche", _db.characters[10].id, "Momiche takes the first Coming soon card");
            Assert.AreEqual(1, _db.characters.FindAll(c => c.empty).Count, "one Coming soon card left");
        }

        [Test]
        public void ShopStockAndPricesMatch()
        {
            CollectionAssert.AreEqual(new[] { "ambrosiaCake", "nectarVial", "ambrosiaCake2", "cinderSigil" }, _db.shopStock.ConvertAll(s => s.id));
            CollectionAssert.AreEqual(new[] { 40, 40, 70, 300 }, _db.shopStock.ConvertAll(s => s.price));
            foreach (var s in _db.shopStock) Assert.IsNotNull(_db.Item(s.id), s.id);
        }

        [Test]
        public void HubDefaultsAndTextMatch()
        {
            Assert.AreEqual(1450, _db.hubStartingGold);
            CollectionAssert.AreEqual(_src.tips, _db.tips);
        }

        [Test]
        public void EveryCharacterAndBossImageExists()
        {
            foreach (var c in _db.characters)
                if (!string.IsNullOrEmpty(c.img)) FileAssert.Exists(Path.Combine(Application.streamingAssetsPath, "assets", c.img));
            foreach (var b in _db.bosses) FileAssert.Exists(Path.Combine(Application.streamingAssetsPath, "assets", b.img));
        }

        [Test]
        public void EveryBossFightsInItsOwnArena()
        {
            // arena-<bossId>.jpg: Mordekai the blue ruins, Vlado the Sephora bus stop,
            // the Bad Plasmodium the purple ruins, the Good Plazmodium the dark green ruins
            foreach (var b in _db.bosses)
                FileAssert.Exists(Path.Combine(Application.streamingAssetsPath, "assets", $"arena-{b.id}.jpg"), b.id);
        }

        [Test]
        public void PortraitCropsStayInsideTheirImages()
        {
            foreach (var r in DataImporter.PortraitCrops.Values)
            {
                Assert.GreaterOrEqual(r.x, 0); Assert.GreaterOrEqual(r.y, 0);
                Assert.LessOrEqual(r.xMax, 1.0001f); Assert.LessOrEqual(r.yMax, 1.0001f);
            }
        }

        [Test]
        public void XpCurveMatchesJs()
        {
            // xpForLevel(L) = floor(60 · L^1.5), checked against values exported from GameData.js
            for (int l = 1; l <= _src.xpForLevel.Count; l++)
                Assert.AreEqual(_src.xpForLevel[l - 1], Mathf.FloorToInt(60 * Mathf.Pow(l, 1.5f) + 1e-4f), $"level {l}");
        }
    }

    public class RngTests
    {
        [Test]
        public void Mulberry32MatchesMakeRngInJs()
        {
            var r = new Mulberry32(1);
            Assert.AreEqual(0.62707394058816135, r.Next(), 1e-15);
            Assert.AreEqual(0.0027357211802154779, r.Next(), 1e-15);
            Assert.AreEqual(0.52744703995995224, r.Next(), 1e-15);
            var r2 = new Mulberry32(3735928559u);
            Assert.AreEqual(0.94136961409822106, r2.Next(), 1e-15);
            Assert.AreEqual(0.26719574979506433, r2.Next(), 1e-15);
        }

        [Test]
        public void HashStrMatchesJs()
        {
            Assert.AreEqual(2311017530u, Hash.Str("forest:floor:0"));
            Assert.AreEqual(585642488u, Hash.Str("ruins:wall:0"));
        }
    }

    public class PixelArtParityTests
    {
        static Dictionary<string, string> LoadHashes()
        {
            var text = File.ReadAllText("Assets/_Project/Data/Source/pixel-hashes.json");
            var d = new Dictionary<string, string>();
            foreach (Match m in Regex.Matches(text, "\"([^\"]+)\":\\s*\"([^\"]+)\"")) d[m.Groups[1].Value] = m.Groups[2].Value;
            return d;
        }

        [Test]
        public void EverySpriteMatchesTheJavaScriptPixelForPixel()
        {
            var expected = LoadHashes();
            Assert.Greater(expected.Count, 20);
            var failures = new List<string>();
            foreach (var kv in expected)
            {
                var actual = Render(kv.Key)?.Fingerprint();
                if (actual != kv.Value) failures.Add($"{kv.Key}: expected {kv.Value}, got {actual}");
            }
            Assert.IsEmpty(failures, string.Join("\n", failures));
        }

        static PixelCanvas Render(string key)
        {
            var p = key.Split('.');
            switch (p[0])
            {
                case "hero": return PixelArt.Hero()[p[1]][int.Parse(p[2])];
                case "flame": return PixelArt.Flame(int.Parse(p[1]));
                case "prop": return PixelArt.Prop(p[1]);
            }
            return null;
        }
    }
}
