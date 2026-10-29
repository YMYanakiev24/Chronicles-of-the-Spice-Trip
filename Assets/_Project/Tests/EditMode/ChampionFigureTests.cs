// Every playable character walks as their own figure, seen from all four
// sides: the drawn ones from ChampionFigures, Momiche from her sheet.

using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;
using SpiceTrip.Art;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;

namespace SpiceTrip.Tests
{
    public class ChampionFigureTests
    {
        static IEnumerable<string> Playable => GameDatabase.Instance.characters.Where(c => !c.empty).Select(c => c.id);

        static string Hash(PixelCanvas c)
        {
            unchecked
            {
                ulong h = 1469598103934665603;
                foreach (var p in c.Pixels) h = (h ^ (uint)(p.r | p.g << 8 | p.b << 16 | p.a << 24)) * 1099511628211;
                return h.ToString("x16");
            }
        }

        [Test]
        public void EveryCharacterWalksFromAllFourSides()
        {
            foreach (var id in Playable)
            {
                var h = HeroSprites.For(id);
                Assert.AreEqual(1 + ChampionFigures.WalkFrames, h.FrameCount, id);
                foreach (var dir in ChampionFigures.Directions)
                    for (int i = 0; i < h.FrameCount; i++)
                    {
                        var s = h.Frame(dir, i);
                        Assert.IsNotNull(s, $"{id} {dir} {i}");
                        Assert.AreEqual(0.75f, s.rect.width / s.rect.height, 0.01f, $"{id}: every frame is 3:4");
                    }
                Assert.AreSame(h.Frame("left", 0), h.Frame(new Vector2(-1, 0.2f), false, 0), id);
                Assert.AreSame(h.Frame("up", 0), h.Frame(new Vector2(0.1f, -1), false, 0), id);
            }
        }

        [Test]
        public void EachDrawnChampionHasALookOfTheirOwn()
        {
            var fronts = new Dictionary<string, string>();
            foreach (var id in Playable)
            {
                if (HeroSprites.For(id).FromSheet) continue;
                Assert.IsTrue(ChampionFigures.Has(id), $"{id} has a drawn figure");
                var set = ChampionFigures.Build(id);
                CollectionAssert.AreEquivalent(ChampionFigures.Directions, set.Keys);
                string front = Hash(set["down"][0]);
                Assert.AreNotEqual(front, Hash(set["up"][0]), $"{id}: the back is not the front");
                Assert.AreNotEqual(front, Hash(set["right"][0]), $"{id}: the side is not the front");
                Assert.AreNotEqual(front, Hash(set["down"][1]), $"{id}: the legs move");
                Assert.AreNotEqual(Hash(set["right"][1]), Hash(set["right"][3]), $"{id}: two different steps");
                Assert.IsFalse(fronts.ContainsValue(front), $"{id} looks like {fronts.FirstOrDefault(kv => kv.Value == front).Key}");
                fronts[id] = front;

                var l = set["left"][2];
                var r = set["right"][2];
                for (int y = 0; y < ChampionFigures.H; y++)
                    for (int x = 0; x < ChampionFigures.W; x++)
                        Assert.AreEqual(r.Pixels[y * ChampionFigures.W + ChampionFigures.W - 1 - x], l.Pixels[y * ChampionFigures.W + x], $"{id}: left mirrors right");
            }
            Assert.GreaterOrEqual(fronts.Count, 10, "the ten original champions are drawn");
        }

        [Test]
        public void MomicheWalksWithTheFramesCutFromHerSheet()
        {
            var h = HeroSprites.For("momiche");
            Assert.IsTrue(h.FromSheet, "assets/walk-momiche.png is used");
            Assert.AreEqual(5, h.FrameCount);
            var steps = new HashSet<Sprite>();
            for (float t = 0; t < 1; t += 1 / 16f) steps.Add(h.Frame(Vector2.right, true, t));
            Assert.AreEqual(4, steps.Count, "walking steps through the four walk frames");
            Assert.IsFalse(steps.Contains(h.Frame("right", 0)), "and not the standing one");
        }

        [Test]
        public void TheTurnaroundShowsEachChampionFromEverySide()
        {
            CollectionAssert.AreEquivalent(ChampionFigures.Directions, UiKit.TurnaroundOrder);
            foreach (var id in Playable)
            {
                var hero = HeroSprites.For(id);
                var row = UiKit.Turnaround(id, 40);
                Assert.AreEqual(4, row.childCount, id);
                for (int i = 0; i < row.childCount; i++)
                {
                    var figure = row[i];
                    Assert.AreSame(hero.Frame(UiKit.TurnaroundOrder[i], 0), figure.style.backgroundImage.value.sprite, $"{id} {UiKit.TurnaroundOrder[i]}");
                    Assert.AreEqual(30f, figure.style.width.value.value, 0.01f, "3:4 like the frames");
                }
            }
        }

        [Test]
        public void ABrokenWalkSheetFallsBackToTheDrawnFigure()
        {
            Assert.IsFalse(HeroSprites.For("gakev").FromSheet, "no walk-gakev.png, so Gakev is drawn");
            Assert.IsFalse(HeroSprites.For("nobody").FromSheet);
            Assert.AreEqual(5, HeroSprites.For("nobody").FrameCount, "unknown ids walk as Spellwoven");
        }
    }
}
