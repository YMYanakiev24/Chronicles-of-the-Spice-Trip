/*
 * SceneScreens.cs
 * ----------------------------------------------------------------------------
 * The three full-screen hub scenes from Scenes.js, each a 16:9 stage with the
 * painted art (or the procedural fallback) and clickable hotspots:
 *   PathScreen      the boss map: four bosses and the castle
 *   CorridorScreen  the torch-lit hall: Marin's door and the Changing Room
 *   ShopScreen      Marin's Apothecary: portrait, speech line, gold, counter
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Hub
{
    /// <summary>Shared shell: overlay + letterboxed stage + optional procedural backdrop.</summary>
    public abstract class SceneScreen
    {
        public readonly VisualElement Root;
        public readonly Stage Stage;
        public readonly bool HasArt;
        protected readonly GameContext Ctx;
        readonly PixelCanvas _canvas;
        Texture2D _canvasTex;

        protected SceneScreen(GameContext ctx, string id, string artName)
        {
            Ctx = ctx;
            Root = El("scene-overlay hidden", id);
            Stage = new Stage();
            Root.Add(Stage);
            Root.RegisterCallback<GeometryChangedEvent>(_ => Stage.Fit());

            var art = ArtLibrary.Load(artName);
            HasArt = art != null;
            if (HasArt) SetBackground(Stage, art);
            else _canvas = new PixelCanvas(SceneBackdrops.BW, SceneBackdrops.BH);
        }

        protected abstract void DrawFallback(PixelCanvas c, float t);

        /// <summary>Animates the procedural scene; only used when the painted art is missing.</summary>
        public virtual void Tick(float t)
        {
            if (_canvas == null) return;
            DrawFallback(_canvas, t);
            bool first = _canvasTex == null;
            _canvasTex = _canvas.ToTexture(_canvasTex);
            if (first) SetBackground(Stage, _canvasTex);
            else Stage.MarkDirtyRepaint();
        }

        public void Show(bool show) => UiKit.Show(Root, show);

        protected ClickEl BackButton(string text, Action onClick)
        {
            var b = Btn(text, "scene-back", onClick, "back", 10);
            Root.Add(b);
            return b;
        }

        protected Label Hint(string text)
        {
            var row = El("hint-row");
            row.pickingMode = PickingMode.Ignore;
            var l = Lbl(text, "scene-hint");
            row.Add(l);
            Root.Add(row);
            return l;
        }
    }

    public class PathScreen : SceneScreen
    {
        readonly Dictionary<string, VisualElement> _bossEls = new Dictionary<string, VisualElement>();
        readonly Dictionary<string, (VisualElement stars, Label best)> _ratings = new Dictionary<string, (VisualElement, Label)>();
        readonly Label _gold, _gauntletText;
        public readonly ClickEl GauntletButton;

        public PathScreen(GameContext ctx, Action<BossDef> onFight, Action onCastle, Action onLeave, Action onGauntlet = null)
            : base(ctx, "scene-path", "path")
        {
            var host = El(null, "path-bosses");
            host.style.position = Position.Absolute;
            host.style.left = 0; host.style.top = 0; host.style.right = 0; host.style.bottom = 0;
            host.pickingMode = PickingMode.Ignore;
            Stage.Add(host);

            // A clickable hotspot over each boss on the path map.
            for (int i = 0; i < ctx.Db.bosses.Count; i++)
            {
                var boss = ctx.Db.bosses[i];
                var b = new ClickEl(() => onFight(boss)) { name = "boss-" + boss.id };
                b.AddToClassList("boss-hotspot");
                b.style.left = Length.Percent((boss.cx - 0.08f) * 100);
                b.style.top = Length.Percent((boss.cy - 0.20f) * 100);
                var tip = El("boss-tip");
                var row = El("boss-tip-row");
                row.Add(Icon("swords", 9, "btn-icon"));
                row.Add(Lbl($"Boss {i + 1}"));
                tip.Add(row);
                tip.Add(Lbl(boss.name));
                var rating = El("boss-tip-row boss-rating");
                var stars = El("boss-stars");
                rating.Add(stars);
                var best = Lbl("", "boss-best");
                rating.Add(best);
                tip.Add(rating);
                _ratings[boss.id] = (stars, best);
                tip.pickingMode = PickingMode.Ignore;
                b.Add(tip);
                b.Add(Icon("check", 20, "boss-check"));
                host.Add(b);
                _bossEls[boss.id] = b;
            }

            var castle = new ClickEl(onCastle) { name = "path-castle" };
            castle.AddToClassList("path-castle");
            var label = El("path-castle-label");
            label.Add(Icon("castle", 13, "btn-icon"));
            label.Add(Lbl("Enter the Castle"));
            label.pickingMode = PickingMode.Ignore;
            castle.Add(label);
            Stage.Add(castle);

            BackButton("Leave", onLeave);
            Hint("Click a boss to fight · enter the castle for the shop & changing room");

            // Marin's wallet: boss bounties are paid into it.
            var goldBox = El("path-gold");
            goldBox.Add(Icon("coin", 14, "btn-icon"));
            _gold = Lbl("0", "path-gold-text");
            goldBox.Add(_gold);
            Root.Add(goldBox);

            // The Gauntlet: all four bosses back to back, once each has fallen.
            var gauntletRow = El("gauntlet-row");
            gauntletRow.pickingMode = PickingMode.Ignore;
            GauntletButton = new ClickEl(() => onGauntlet?.Invoke()) { name = "btn-gauntlet" };
            GauntletButton.AddToClassList("gauntlet-btn");
            GauntletButton.Add(Icon("swords", 14, "btn-icon"));
            _gauntletText = Lbl("The Gauntlet", "gauntlet-text");
            GauntletButton.Add(_gauntletText);
            gauntletRow.Add(GauntletButton);
            Root.Add(gauntletRow);
        }

        public bool GauntletUnlocked { get; private set; }

        /// <summary>Gold, star ratings and best times, and whether the Gauntlet is open.</summary>
        public void RefreshProgress(HubData hub, ICollection<string> defeated)
        {
            _gold.text = hub.gold.ToString();
            foreach (var kv in _ratings)
            {
                var rec = hub.records.Find(r => r.id == kv.Key);
                kv.Value.stars.Clear();
                for (int i = 0; i < 3; i++) kv.Value.stars.Add(Icon(rec != null && i < rec.bestStars ? "star" : "star-empty", 9));
                kv.Value.best.text = rec != null && rec.bestTime > 0 ? "best " + Battle.Progression.Clock(rec.bestTime) : "unbeaten";
            }
            GauntletUnlocked = Ctx.Db.bosses.TrueForAll(b => defeated.Contains(b.id));
            GauntletButton.EnableInClassList("locked", !GauntletUnlocked);
            var g = hub.records.Find(r => r.id == Battle.Progression.GauntletId);
            _gauntletText.text = !GauntletUnlocked ? "The Gauntlet — defeat all four to unlock"
                : g != null && g.bestTime > 0 ? $"The Gauntlet — best {Battle.Progression.Clock(g.bestTime)}"
                : "The Gauntlet — all four, back to back";
        }

        public VisualElement BossButton(string id) => _bossEls.TryGetValue(id, out var e) ? e : null;

        public void RefreshBossMarks(ICollection<string> defeated)
        {
            foreach (var kv in _bossEls) kv.Value.EnableInClassList("defeated", defeated.Contains(kv.Key));
        }

        protected override void DrawFallback(PixelCanvas c, float t) => SceneBackdrops.DrawPathFallback(c, t);
    }

    public class CorridorScreen : SceneScreen
    {
        public readonly ClickEl WizardDoor, ChangingDoor;
        readonly Label _hint;

        public CorridorScreen(GameContext ctx, Action onWizard, Action onChanging, Action onBack)
            : base(ctx, "scene-corridor", "corridor")
        {
            if (HasArt) Root.AddToClassList("has-corridor-img");   // the art has its own door signs
            WizardDoor = Door("door-wizard", "Marin the Wizard", "wiz", onWizard);
            ChangingDoor = Door("door-changing", "Changing Room", "chg", onChanging);
            BackButton("Back to the path", onBack);
            _hint = Hint("Click a door to enter");
        }

        ClickEl Door(string id, string label, string labelClass, Action onClick)
        {
            var b = new ClickEl(onClick) { name = id };
            b.AddToClassList("door-hotspot");
            b.AddToClassList(id);
            var l = Lbl(label, "door-label " + labelClass);
            l.pickingMode = PickingMode.Ignore;
            b.Add(l);
            Stage.Add(b);
            return b;
        }

        protected override void DrawFallback(PixelCanvas c, float t) => SceneBackdrops.DrawCorridor(c, t);
    }

    public class ShopScreen : SceneScreen
    {
        readonly Label _gold, _marinLine;
        readonly VisualElement _counter, _marinSprite;
        readonly PixelCanvas _marinCanvas;
        Texture2D _marinTex;
        readonly bool _marinPhoto;

        readonly Dictionary<string, Label> _carried = new Dictionary<string, Label>();
        readonly Action<BattleItemDef> _onBuyBattle;

        public ShopScreen(GameContext ctx, Action<ShopEntry> onBuy, Action onLoom, Action onBack, Action<BattleItemDef> onBuyBattle = null)
            : base(ctx, "scene-shop", "shop")
        {
            _onBuyBattle = onBuyBattle;
            // The shop photo has no character, so Marin's pixel sprite stands in it
            // unless his portrait art (marin.png) replaces it.
            _marinSprite = El("marin-sprite hidden");
            _marinSprite.pickingMode = PickingMode.Ignore;
            Stage.Add(_marinSprite);

            // A .png is a ready-made transparent cutout: shown raw, except the bottom
            // 36% is trimmed so the objects in his hands aren't displayed.
            var marin = ArtLibrary.Crop("marin.png", new Rect(0, 0, 1, 0.64f));
            if (marin != null)
            {
                _marinPhoto = true;
                var portrait = El("marin-portrait", "marin-portrait");
                SetBackground(portrait, marin);
                portrait.pickingMode = PickingMode.Ignore;
                float aspect = marin.rect.width / marin.rect.height;
                Root.RegisterCallback<GeometryChangedEvent>(_ =>
                {
                    // height: min(50vh, 440px); width follows the art's aspect.
                    float h = Mathf.Min(0.5f * Root.resolvedStyle.height, 440);
                    portrait.style.height = h;
                    portrait.style.width = h * aspect;
                });
                Root.Add(portrait);
            }
            else if (HasArt)
            {
                UiKit.Show(_marinSprite, true);
                _marinCanvas = new PixelCanvas(48, 76);
                // left: 47%; top: 26%; height: 42%; width auto; translateX(-50%)
                _marinSprite.style.top = Length.Percent(26);
                _marinSprite.style.height = Length.Percent(42);
                Stage.RegisterCallback<GeometryChangedEvent>(_ =>
                {
                    float h = Stage.resolvedStyle.height * 0.42f, w = h * 48f / 76f;
                    _marinSprite.style.width = w;
                    _marinSprite.style.left = Stage.resolvedStyle.width * 0.47f - w / 2;
                });
            }

            BackButton("Back to the hall", onBack);

            var goldBox = El("shop-gold");
            goldBox.style.flexDirection = FlexDirection.Row;
            goldBox.Add(Lbl("GOLD: ", "shop-gold-text"));
            _gold = Lbl("0", "shop-gold-text");
            _gold.name = "shop-gold-val";
            goldBox.Add(_gold);
            Root.Add(goldBox);

            var lineRow = El("shop-marin-row");
            lineRow.pickingMode = PickingMode.Ignore;
            _marinLine = Lbl("\"Welcome, traveler. Coin for cures?\"", "shop-marin-line");
            _marinLine.name = "marin-line";
            lineRow.Add(_marinLine);
            Root.Add(lineRow);

            var panel = El("shop-panel");
            SetBackground(panel, ShopFadeTexture());
            var titleRow = El();
            titleRow.style.flexDirection = FlexDirection.Row;
            titleRow.style.justifyContent = Justify.Center;
            titleRow.style.alignItems = Align.Center;
            titleRow.Add(Icon("star", 9, "btn-icon"));
            titleRow.Add(Lbl("Marin's Apothecary", "shop-title"));
            var star2 = Icon("star", 9);
            star2.style.marginLeft = 10;
            titleRow.Add(star2);
            panel.Add(titleRow);
            _counter = El("shop-counter", "shop-items");
            panel.Add(_counter);
            Root.Add(panel);

            RenderItems(onBuy, onLoom);
        }

        void RenderItems(Action<ShopEntry> onBuy, Action onLoom)
        {
            _counter.Clear();
            foreach (var s in Ctx.Db.shopStock)
            {
                var it = Ctx.Db.Item(s.id);
                if (it == null) continue;
                var entry = s;
                var card = new ClickEl(() => onBuy(entry)) { name = "shop-item-" + s.id };
                card.AddToClassList("shop-item");
                card.Add(Icon(Icons.Has(s.id) ? s.id : "coin", 24, "si-icon"));
                card.Add(Lbl(it.name, "si-name"));
                card.Add(Lbl(it.desc, "si-desc"));
                var price = El("si-price-row");
                price.Add(Icon("coin", 12, "si-coin"));
                price.Add(Lbl(s.price.ToString(), "si-price"));
                card.Add(price);
                _counter.Add(card);
            }
            // The Enchanted Loom tile.
            var loom = new ClickEl(onLoom) { name = "shop-item-loom" };
            loom.AddToClassList("shop-item");
            loom.AddToClassList("loom");
            loom.Add(Icon("loom", 24, "si-icon"));
            loom.Add(Lbl("Enchanted Loom", "si-name"));
            loom.Add(Lbl("Weave any image into a tapestry of rune-shards and tear it apart.", "si-desc"));
            var tryRow = El("si-price-row");
            tryRow.Add(Lbl("try it", "si-price"));
            tryRow.Add(Icon("play", 7, "si-coin"));
            tryRow[1].style.marginLeft = 4;
            loom.Add(tryRow);
            _counter.Add(loom);

            // Battle wares: carried into boss fights (keys 1–3), up to a few of each.
            foreach (var bi in Ctx.Db.battleItems)
            {
                var def = bi;
                var card = new ClickEl(() => _onBuyBattle?.Invoke(def)) { name = "shop-battle-" + bi.id };
                card.AddToClassList("shop-item");
                card.AddToClassList("battle");
                card.Add(Icon(bi.icon, 24, "si-icon"));
                card.Add(Lbl(bi.name, "si-name"));
                card.Add(Lbl(bi.desc, "si-desc"));
                var price = El("si-price-row");
                price.Add(Icon("coin", 12, "si-coin"));
                price.Add(Lbl(bi.price.ToString(), "si-price"));
                var carried = Lbl("", "si-carried");
                price.Add(carried);
                _carried[bi.id] = carried;
                card.Add(price);
                _counter.Add(card);
            }
            RefreshCarried();
        }

        public void RefreshCarried()
        {
            foreach (var bi in Ctx.Db.battleItems)
                if (_carried.TryGetValue(bi.id, out var l)) l.text = $"carried {Ctx.Hub.SatchelCount(bi.id)}/{bi.max}";
        }

        public void SetGold(int gold) => _gold.text = gold.ToString();
        public void SetMarinLine(string line) => _marinLine.text = $"\"{line}\"";
        public string MarinLine => _marinLine.text;

        public override void Tick(float t)
        {
            base.Tick(t);   // procedural shop (which already includes Marin) when the photo is missing
            if (_marinCanvas == null || _marinPhoto) return;
            SceneBackdrops.DrawMarin(_marinCanvas, t);
            bool first = _marinTex == null;
            _marinTex = _marinCanvas.ToTexture(_marinTex);
            if (first) SetBackground(_marinSprite, _marinTex);
            else _marinSprite.MarkDirtyRepaint();
        }

        protected override void DrawFallback(PixelCanvas c, float t) => SceneBackdrops.DrawShop(c, t);
    }
}
