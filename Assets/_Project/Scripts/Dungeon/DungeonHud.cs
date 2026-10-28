/*
 * DungeonHud.cs
 * ----------------------------------------------------------------------------
 * The Dungeon's UI Toolkit layer: the lobby (champion, records, New Run /
 * Continue / Boss Rush), the HUD (vitals, gold and floor, the boss bar,
 * minimap, hotbar with the special, banner, the oracle's words, prompts and
 * damage numbers over the room), and the panels: pause, Marin's market,
 * "Spice Trip complete" and the Boss Rush result.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Dungeon.Geo;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Dungeon
{
    public class DungeonHud
    {
        public interface IActions
        {
            void NewRun();
            void ContinueRun();
            void BossRush();
            void LeaveLobby();
            void SaveRun();
            void QuitToMenu();
        }

        readonly DungeonGame _g;
        readonly GameContext _ctx;
        readonly DungeonView _view;
        readonly IActions _act;
        public readonly VisualElement Root;
        public readonly VisualElement Lobby, Hud, Pause, Shop, Complete, RushOver;

        // vitals
        VisualElement _portrait, _hpFill, _manaFill, _xpFill;
        Label _champName, _level, _hpText, _manaText, _gold, _where, _effects;
        // boss bar
        VisualElement _boss, _bossToken, _bossFill;
        Label _bossName, _bossNote;
        // minimap
        VisualElement _minimap;
        readonly VisualElement[] _cells = new VisualElement[FloorSize * FloorSize];
        // hotbar
        readonly VisualElement[] _slots = new VisualElement[3], _slotIcons = new VisualElement[3];
        readonly Label[] _slotCosts = new Label[3], _slotNames = new Label[3];
        VisualElement _special, _specialArt, _specialCurtain, _meter, _meterFill;
        Label _specialName;
        // banner, dialogue, text over the room
        VisualElement _banner, _dialogue, _textLayer;
        Label _bannerText, _bannerSub, _dialogueText, _hint;
        readonly List<Label> _labels = new List<Label>();
        int _labelsUsed;
        // panels
        Label _shopGold, _shopMsg, _saveMsg, _completeStats, _rushTitle, _rushLine, _rushBest, _records;
        readonly List<(VisualElement row, ClickEl buy, Label owned)> _shopRows = new List<(VisualElement, ClickEl, Label)>();
        VisualElement _lobbyGrid, _lobbyDetailArt, _completeTokens, _pauseKit;
        Label _lobbyName, _lobbyPerk, _lobbySpecial;
        ClickEl _continueBtn;
        float _saveMsgT;

        public DungeonHud(DungeonGame game, GameContext ctx, DungeonView view, VisualElement layer, IActions actions)
        {
            _g = game; _ctx = ctx; _view = view; _act = actions;
            Root = El("dg-root", "dungeon");
            Root.pickingMode = PickingMode.Ignore;
            layer.Add(Root);
            Hud = BuildHud();
            Pause = BuildPause();
            Shop = BuildShop();
            Complete = BuildComplete();
            RushOver = BuildRushOver();
            Lobby = BuildLobby();
            ShowAll(false);
        }

        public void ShowAll(bool hud)
        {
            Show(Hud, hud);
            foreach (var p in new[] { Pause, Shop, Complete, RushOver, Lobby }) Show(p, false);
        }

        // =====================================================================
        // Lobby
        // =====================================================================
        VisualElement BuildLobby()
        {
            var screen = El("dg-screen dg-lobby", "dungeon-lobby");
            var panel = El("dg-panel dg-lobby-panel");
            screen.Add(panel);
            panel.Add(Lbl("The Dungeon", "dg-title"));
            panel.Add(Lbl("Fight down through the Mistwood Depths. Every floor ends with a boss.", "dg-sub"));
            var body = El("dg-lobby-body");
            panel.Add(body);
            _lobbyGrid = El("dg-champ-grid");
            body.Add(_lobbyGrid);
            foreach (var kit in Defs.Champions)
            {
                var k = kit;
                var card = new ClickEl(() => PickChampion(k.id)) { name = "dg-champ-" + k.id };
                card.AddToClassList("dg-champ");
                card.Add(PortraitEl(k.id, "dg-champ-art"));
                card.Add(Lbl(_ctx.Db.Character(k.id)?.name ?? k.id, "dg-champ-name"));
                card.RegisterCallback<MouseEnterEvent>(_ => ShowChampion(k.id));
                card.RegisterCallback<MouseLeaveEvent>(_ => ShowChampion(_ctx.Hub.selectedChar));
                _lobbyGrid.Add(card);
            }
            var detail = El("dg-champ-detail");
            body.Add(detail);
            _lobbyDetailArt = El("dg-detail-art");
            detail.Add(_lobbyDetailArt);
            _lobbyName = Lbl("", "dg-detail-name");
            detail.Add(_lobbyName);
            detail.Add(Lbl("Perk", "dg-detail-head"));
            _lobbyPerk = Lbl("", "dg-detail-text");
            detail.Add(_lobbyPerk);
            detail.Add(Lbl("Special (Q)", "dg-detail-head"));
            _lobbySpecial = Lbl("", "dg-detail-text");
            detail.Add(_lobbySpecial);
            _records = Lbl("", "dg-records");
            panel.Add(_records);
            var buttons = El("dg-buttons");
            panel.Add(buttons);
            _continueBtn = Button(buttons, "Continue", "dg-continue", () => _act.ContinueRun(), "primary");
            Button(buttons, "New Run", "dg-new-run", () => _act.NewRun(), "primary");
            Button(buttons, "Boss Rush", "dg-boss-rush", () => _act.BossRush());
            Button(buttons, "Back", "dg-lobby-back", () => _act.LeaveLobby());
            Root.Add(screen);
            return screen;
        }

        public void ShowLobby(bool hasSave)
        {
            ShowAll(false);
            Show(Lobby, true);
            Show(_continueBtn, hasSave);
            var p = _g.Profile;
            _records.text = $"Deepest floor {p.deepestFloor}     Bosses beaten {Bits(p.bossesBeaten)} / {Defs.Bosses.Length}     " +
                (p.bestRush > 0 ? $"Boss Rush best {Clock(p.bestRush)}" : "Boss Rush not yet won");
            ShowChampion(_ctx.Hub.selectedChar);
        }

        static int Bits(int v) { int n = 0; for (; v != 0; v &= v - 1) n++; return n; }

        void PickChampion(string id)
        {
            _ctx.Audio.Sfx("pickup");
            _ctx.Hub.selectedChar = id;
            _ctx.SaveHub();
            ShowChampion(id);
        }

        void ShowChampion(string id)
        {
            var kit = Defs.Champion(id);
            _lobbyName.text = _ctx.Db.Character(kit.id)?.name ?? kit.id;
            _lobbyPerk.text = kit.perk;
            _lobbySpecial.text = $"{kit.special}: {kit.specialDesc}";
            _lobbyDetailArt.Clear();
            _lobbyDetailArt.Add(PortraitEl(kit.id, "dg-detail-portrait"));
            foreach (var card in _lobbyGrid.Children())
                card.EnableInClassList("chosen", card.name == "dg-champ-" + _ctx.Hub.selectedChar);
        }

        VisualElement PortraitEl(string id, string classes)
        {
            var e = El(classes);
            e.pickingMode = PickingMode.Ignore;
            var s = _view.Art.Portrait(id);
            if (s != null) SetBackground(e, s);
            else { e.AddToClassList("drawn"); e.Add(Icon("hero", 48)); }
            return e;
        }

        static ClickEl Button(VisualElement parent, string text, string name, Action onClick, string extra = "")
        {
            var b = Btn(text, "menu-btn dg-btn " + extra, onClick);
            b.name = name;
            parent.Add(b);
            return b;
        }

        public static string Clock(float s) { int i = Mathf.RoundToInt(s); return $"{i / 60}:{i % 60:00}"; }

        // =====================================================================
        // HUD
        // =====================================================================
        VisualElement BuildHud()
        {
            var hud = El("dg-hud", "dungeon-hud");
            hud.pickingMode = PickingMode.Ignore;
            Root.Add(hud);

            // vitals: portrait, name, health, mana, experience
            var vit = El("dg-vitals");
            hud.Add(vit);
            _portrait = El("dg-portrait");
            vit.Add(_portrait);
            _level = Lbl("Lv 1", "dg-level");
            _portrait.Add(_level);
            var bars = El("dg-bars");
            vit.Add(bars);
            _champName = Lbl("", "dg-champ-label");
            bars.Add(_champName);
            _hpFill = Bar(bars, "dg-bar hp", out _hpText);
            _manaFill = Bar(bars, "dg-bar mana", out _manaText);
            _xpFill = Bar(bars, "dg-bar xp", out _);
            var under = El("dg-under");
            hud.Add(under);
            under.Add(Icon("coin", 16, "dg-coin"));
            _gold = Lbl("0", "dg-gold");
            under.Add(_gold);
            _where = Lbl("", "dg-where");
            under.Add(_where);
            _effects = Lbl("", "dg-effects");
            hud.Add(_effects);

            // the boss bar
            _boss = El("dg-boss");
            hud.Add(_boss);
            var bossHead = El("dg-boss-head");
            _boss.Add(bossHead);
            _bossToken = El("dg-boss-token");
            bossHead.Add(_bossToken);
            _bossName = Lbl("", "dg-boss-name");
            bossHead.Add(_bossName);
            var bossBar = El("dg-boss-bar");
            _boss.Add(bossBar);
            _bossFill = El("dg-boss-fill");
            bossBar.Add(_bossFill);
            _bossNote = Lbl("", "dg-boss-note");
            _boss.Add(_bossNote);

            // minimap
            _minimap = El("dg-minimap");
            hud.Add(_minimap);
            for (int y = 0; y < FloorSize; y++)
            {
                var row = El("dg-map-row");
                _minimap.Add(row);
                for (int x = 0; x < FloorSize; x++) { var c = El("dg-map-cell"); row.Add(c); _cells[y * FloorSize + x] = c; }
            }

            // hotbar: three powers and the special
            var bar = El("dg-hotbar");
            hud.Add(bar);
            for (int i = 0; i < 3; i++)
            {
                int slot = i;
                var s = El("dg-slot");
                bar.Add(s);
                s.Add(Lbl((i + 1).ToString(), "dg-slot-key"));
                _slotIcons[i] = El("dg-slot-icon");
                SetBackground(_slotIcons[i], _view.Art.SpellIcons.At(i));
                s.Add(_slotIcons[i]);
                _slotCosts[i] = Lbl("", "dg-slot-cost");
                s.Add(_slotCosts[i]);
                _slotNames[i] = Lbl(Defs.Spells[i].name, "dg-slot-name");
                s.Add(_slotNames[i]);
                _slots[i] = s;
            }
            _special = El("dg-slot special");
            bar.Add(_special);
            _specialArt = El("dg-special-art");
            _special.Add(_specialArt);
            _specialCurtain = El("dg-curtain");
            _special.Add(_specialCurtain);
            _meter = El("dg-meter");
            _meterFill = El("dg-meter-fill");
            _meter.Add(_meterFill);
            _special.Add(_meter);
            _special.Add(Lbl("Q", "dg-slot-key"));
            _specialName = Lbl("", "dg-slot-name");
            _special.Add(_specialName);

            // banner, dialogue, prompts and damage numbers, hint
            _textLayer = El("dg-text-layer");
            _textLayer.pickingMode = PickingMode.Ignore;
            hud.Add(_textLayer);
            _banner = El("dg-banner");
            hud.Add(_banner);
            _bannerText = Lbl("", "dg-banner-text");
            _banner.Add(_bannerText);
            _bannerSub = Lbl("", "dg-banner-sub");
            _banner.Add(_bannerSub);
            _dialogue = El("dg-dialogue");
            hud.Add(_dialogue);
            _dialogue.Add(Lbl("Theira, the Last Oracle", "dg-speaker"));
            _dialogueText = Lbl("", "dg-dialogue-text");
            _dialogue.Add(_dialogueText);
            _dialogue.Add(Lbl("E  continue", "dg-continue-hint"));
            _hint = Lbl("WASD move   LMB blade   RMB cast   1-3 power   Q special   Space roll   E interact   Esc pause", "dg-hint");
            hud.Add(_hint);
            return hud;
        }

        VisualElement Bar(VisualElement parent, string classes, out Label text)
        {
            var bar = El(classes);
            parent.Add(bar);
            var fill = El("dg-bar-fill");
            bar.Add(fill);
            text = Lbl("", "dg-bar-text");
            bar.Add(text);
            return fill;
        }

        static void Fill(VisualElement e, float ratio) => e.style.width = Length.Percent(Mathf.Clamp01(ratio) * 100);

        /// <summary>The champion's portrait on the vitals and the special slot.</summary>
        public void BeginRun()
        {
            ShowAll(true);
            var kit = _g.Kit;
            _champName.text = _ctx.Db.Character(kit.id)?.name ?? kit.id;
            foreach (var target in new[] { _portrait, _specialArt })
            {
                var s = _view.Art.Portrait(kit.id);
                SetBackground(target, s);
                target.EnableInClassList("drawn", s == null);
            }
            _specialName.text = kit.special;
            Show(_meter, kit.kind == Special.CoinBarrage || kit.kind == Special.Blink);
            BuildPauseKit();
        }

        public void Tick(float dt)
        {
            var g = _g;
            var p = g.Player;
            var kit = g.Kit;

            // vitals
            Fill(_hpFill, p.hp / p.maxHp);
            _hpText.text = $"{Mathf.CeilToInt(Mathf.Max(0, p.hp))} / {Mathf.RoundToInt(p.maxHp)}";
            Fill(_manaFill, p.mana / p.maxMana);
            _manaText.text = $"{Mathf.FloorToInt(p.mana)} / {Mathf.RoundToInt(p.maxMana)}";
            Fill(_xpFill, (float)p.xp / p.xpNext);
            _level.text = $"Lv {p.level}";
            _gold.text = p.gold.ToString();
            _where.text = g.Mode == DungeonMode.Run ? $"Floor {g.FloorNumber}   {Clock(g.RunTime)}"
                : $"Boss Rush {g.RushStage + 1}/{Defs.Bosses.Length}   {Clock(g.RushTime)}";
            var fx = new List<string>();
            for (int v = 0; v < 4; v++) if (p.vapeT[v] > 0) fx.Add($"{Defs.Shop[2 + v].name} {Mathf.CeilToInt(p.vapeT[v])}s");
            if (p.auraT > 0) fx.Add($"Calm {Mathf.CeilToInt(p.auraT)}s");
            if (p.shieldT > 0) fx.Add($"Mug-Shield {Mathf.CeilToInt(p.shieldT)}s");
            if (p.wobbleT > 0) fx.Add($"Tipsy {Mathf.CeilToInt(p.wobbleT)}s");
            _effects.text = string.Join("    ", fx);

            // boss bar
            Show(_boss, g.Fight.Active);
            if (g.Fight.Active)
            {
                var b = g.CurrentBoss;
                _bossName.text = b.name;
                Fill(_bossFill, g.Fight.Health(g.Enemies));
                _bossFill.style.backgroundColor = b.color;
                _bossNote.text = g.Fight.Strength > 1.01f ? $"weak to {b.weakness}   ·   x{g.Fight.Strength:0.0} strength" : $"weak to {b.weakness}";
                var token = _view.BossToken(g.CurrentBossIndex);
                SetBackground(_bossToken, token);
                _bossToken.style.borderTopColor = _bossToken.style.borderBottomColor = _bossToken.style.borderLeftColor = _bossToken.style.borderRightColor = b.color;
                _bossToken.EnableInClassList("blank", token == null);
            }

            // minimap (a run only)
            Show(_minimap, g.Mode == DungeonMode.Run);
            if (g.Mode == DungeonMode.Run) UpdateMinimap();

            // hotbar
            for (int i = 0; i < 3; i++)
            {
                int cost = g.SpellCost(i);
                bool castable = p.mana >= cost;
                _slots[i].EnableInClassList("selected", i == p.selected);
                _slotIcons[i].style.unityBackgroundImageTintColor = castable ? Color.white : new Color(0.35f, 0.35f, 0.4f);
                _slotCosts[i].text = cost.ToString();
                _slotCosts[i].EnableInClassList("short", !castable);
            }
            bool metered = kit.kind == Special.CoinBarrage || kit.kind == Special.Blink;
            float meter = kit.kind == Special.CoinBarrage ? p.charge / Defs.CoinPowerMax : p.charge / Defs.DustMax;
            bool ready = p.specialCd <= 0 && (kit.kind != Special.CoinBarrage || p.charge >= Defs.CoinPowerMax)
                && (kit.kind != Special.Blink || p.charge >= Defs.DustPerBlink);
            _specialCurtain.style.height = Length.Percent(p.specialCd > 0 ? 100 * p.specialCd / kit.cooldown : ready ? 0 : 100);
            _specialCurtain.EnableInClassList("waiting", p.specialCd <= 0 && !ready);
            _special.EnableInClassList("ready", ready);
            if (metered) Fill(_meterFill, meter);

            // banner and dialogue
            Show(_banner, g.BannerT > 0 && g.Overlay == Overlay.None);
            _banner.style.opacity = Mathf.Min(1, g.BannerT);
            _bannerText.text = g.BannerText;
            _bannerSub.text = g.BannerSub;
            Show(_bannerSub, g.BannerSub.Length > 0);
            Show(_dialogue, g.Dialogue.Length > 0);
            _dialogueText.text = g.Dialogue;
            Show(_hint, g.Room.isStart && g.Mode == DungeonMode.Run && g.FloorNumber == 1);

            // text over the room: damage numbers and prompts
            _labelsUsed = 0;
            foreach (var f in g.Floaters)
            {
                var c = f.color; c.a = Mathf.Clamp01(2.5f * (0.8f - f.t));
                WorldLabel(f.pos, f.value.ToString(), "dg-floater", c);
            }
            if (g.NearChest) WorldLabel(g.Room.ChestPos + new Vector2(0, -34), "E  open", "dg-prompt", Color.white);
            if (g.NearMerchant && g.Overlay != Overlay.Shop) WorldLabel(MerchantPos + new Vector2(0, -66), "E  trade with Marin", "dg-prompt", Color.white);
            if (g.NearOracle && g.Dialogue.Length == 0) WorldLabel(OraclePos + new Vector2(0, -40), "E  talk", "dg-prompt", Color.white);
            if (g.StairsOpen && Vector2.Distance(p.pos, Center) < 70) WorldLabel(Center + new Vector2(0, -46), "Descend", "dg-prompt gold", Color.white);
            for (int i = _labelsUsed; i < _labels.Count; i++) Show(_labels[i], false);

            // panels
            if (g.Overlay == Overlay.Shop) TickShop();
            if (g.Overlay == Overlay.Pause && _saveMsgT > 0) { _saveMsgT -= dt; _saveMsg.style.opacity = Mathf.Min(1, _saveMsgT); }
            Show(Pause, g.Overlay == Overlay.Pause);
            Show(Shop, g.Overlay == Overlay.Shop);
            if (g.Overlay == Overlay.RunComplete && Complete.ClassListContains("hidden")) FillComplete();
            Show(Complete, g.Overlay == Overlay.RunComplete);
            if (g.Overlay == Overlay.RushOver && RushOver.ClassListContains("hidden")) FillRushOver();
            Show(RushOver, g.Overlay == Overlay.RushOver);
        }

        void UpdateMinimap()
        {
            int minX = FloorSize, minY = FloorSize, maxX = -1, maxY = -1;
            for (int y = 0; y < FloorSize; y++)
                for (int x = 0; x < FloorSize; x++)
                    if (_g.Floor[x, y].exists) { minX = Mathf.Min(minX, x); maxX = Mathf.Max(maxX, x); minY = Mathf.Min(minY, y); maxY = Mathf.Max(maxY, y); }
            for (int y = 0; y < FloorSize; y++)
            {
                Show(_minimap[y], y >= minY && y <= maxY);
                for (int x = 0; x < FloorSize; x++)
                {
                    var cell = _cells[y * FloorSize + x];
                    var r = _g.Floor[x, y];
                    Show(cell, x >= minX && x <= maxX);
                    bool cur = x == _g.Floor.cx && y == _g.Floor.cy;
                    cell.EnableInClassList("room", r.exists);
                    cell.EnableInClassList("current", cur);
                    cell.EnableInClassList("visited", r.exists && r.visited && !cur);
                    cell.EnableInClassList("boss", r.exists && r.isBoss && !cur);
                    cell.EnableInClassList("shop", r.exists && r.isShop && r.visited && !cur);
                }
            }
        }

        /// <summary>A label pinned to a spot in the room (screen space, so it stays crisp).</summary>
        void WorldLabel(Vector2 room, string text, string classes, Color color)
        {
            var panel = Root.panel;
            if (panel == null) return;
            if (_labelsUsed == _labels.Count)
            {
                var l = Lbl("", null);
                l.pickingMode = PickingMode.Ignore;
                _textLayer.Add(l);
                _labels.Add(l);
            }
            var lab = _labels[_labelsUsed++];
            lab.ClearClassList();
            Classes(lab, "dg-world-text " + classes);
            lab.text = text;
            lab.style.color = color;
            var s = _view.RoomToScreen(room);
            var pp = RuntimePanelUtils.ScreenToPanel(panel, new Vector2(s.x, Screen.height - s.y));
            lab.style.left = pp.x; lab.style.top = pp.y;
            Show(lab, true);
        }

        // =====================================================================
        // Pause
        // =====================================================================
        VisualElement BuildPause()
        {
            var screen = El("dg-screen dg-veil", "dungeon-pause");
            screen.Add(Lbl("Paused", "dg-title big"));
            var row = El("dg-pause-row");
            screen.Add(row);
            var col = El("dg-pause-buttons");
            row.Add(col);
            Button(col, "Resume", "dg-resume", () => { _ctx.Audio.Sfx("ui"); _g.Overlay = Overlay.None; }, "primary");
            Button(col, "Save", "dg-save", () =>
            {
                bool ok = _g.Mode == DungeonMode.Run;
                if (ok) _act.SaveRun();
                _saveMsg.text = ok ? "Game saved" : "Boss Rush can't be saved";
                _saveMsgT = 2;
            });
            Button(col, "Main Menu", "dg-main-menu", () => { _ctx.Audio.Sfx("ui"); _act.QuitToMenu(); });
            _saveMsg = Lbl("", "dg-save-msg");
            col.Add(_saveMsg);
            _pauseKit = El("dg-panel dg-controls");
            row.Add(_pauseKit);
            Root.Add(screen);
            return screen;
        }

        void BuildPauseKit()
        {
            _pauseKit.Clear();
            _pauseKit.Add(Lbl("Controls", "dg-panel-title"));
            var kit = _g.Kit;
            (string key, string what)[] rows =
            {
                ("W A S D", "move"), ("Shift", "run"), ("Space", "dodge-roll"), ("E", "talk, trade, open"),
                ("LMB", "swing the blade"), ("RMB", "cast the selected power"), ("1 2 3", "choose a power"), ("Q", kit.special),
            };
            foreach (var (key, what) in rows)
            {
                var r = El("dg-control-row");
                r.Add(Lbl(key, "dg-key"));
                r.Add(Lbl(what, "dg-control-text"));
                _pauseKit.Add(r);
            }
            _pauseKit.Add(Lbl($"{kit.special}: {kit.specialDesc}", "dg-control-note"));
            _pauseKit.Add(Lbl($"Perk: {kit.perk}", "dg-control-note"));
        }

        // =====================================================================
        // Marin's market
        // =====================================================================
        VisualElement BuildShop()
        {
            var screen = El("dg-screen dg-veil", "dungeon-shop");
            var panel = El("dg-panel dg-shop");
            screen.Add(panel);
            var head = El("dg-shop-head");
            panel.Add(head);
            var shopArt = ArtLibrary.Load("shop");
            if (shopArt != null) SetBackground(head, shopArt);
            var marin = El("dg-shop-marin");
            if (_view.Art.Marin != null) SetBackground(marin, _view.Art.Marin);
            head.Add(marin);
            var words = El("dg-shop-words");
            head.Add(words);
            words.Add(Lbl("Marin the Wizard", "dg-title"));
            words.Add(Lbl("\"Potions, powers, a little spice... all for honest gold.\"", "dg-sub"));
            var purse = El("dg-purse");
            purse.Add(Icon("coin", 18));
            _shopGold = Lbl("0", "dg-gold");
            purse.Add(_shopGold);
            head.Add(purse);
            var close = Btn("✕", "menu-btn dg-close", () => { _ctx.Audio.Sfx("ui"); _g.Overlay = Overlay.None; });
            close.name = "dg-shop-close";
            head.Add(close);

            var grid = El("dg-wares");
            panel.Add(grid);
            for (int i = 0; i < Defs.Shop.Length; i++)
            {
                int item = i;
                var it = Defs.Shop[i];
                var row = El("dg-ware");
                grid.Add(row);
                var icon = El("dg-ware-icon");
                if (it.vape >= 0) SetBackground(icon, _view.Art.Vapes.At(it.vape));
                else if (it.skill == Skill.None) SetBackground(icon, (i == 0 ? _view.Art.FlaskRed : _view.Art.FlaskBlue).At(0));
                else { icon.AddToClassList("rune"); icon.Add(Lbl(it.name.Substring(0, 1), "dg-rune-letter")); }
                row.Add(icon);
                var words2 = El("dg-ware-words");
                words2.Add(Lbl(it.name, "dg-ware-name"));
                words2.Add(Lbl(it.desc, "dg-ware-desc"));
                row.Add(words2);
                var buy = Btn($"{it.price} g", "menu-btn dg-buy", () => _g.Buy(item));
                buy.name = "dg-buy-" + i;
                row.Add(buy);
                var owned = Lbl("Learned", "dg-owned");
                row.Add(owned);
                _shopRows.Add((row, buy, owned));
            }
            _shopMsg = Lbl("", "dg-shop-msg");
            panel.Add(_shopMsg);
            Root.Add(screen);
            return screen;
        }

        void TickShop()
        {
            var p = _g.Player;
            _shopGold.text = p.gold.ToString();
            for (int i = 0; i < _shopRows.Count; i++)
            {
                var it = Defs.Shop[i];
                bool owned = it.skill != Skill.None && p.Has(it.skill);
                Show(_shopRows[i].buy, !owned);
                Show(_shopRows[i].owned, owned);
                _shopRows[i].buy.EnableInClassList("primary", p.gold >= it.price);
                _shopRows[i].row.EnableInClassList("owned", owned);
            }
            _shopMsg.text = _g.ShopMessageT > 0 ? _g.ShopMessage : "";
        }

        // =====================================================================
        // The end of a run, and of a Boss Rush
        // =====================================================================
        VisualElement BuildComplete()
        {
            var screen = El("dg-screen dg-veil", "dungeon-complete");
            var panel = El("dg-panel dg-result");
            screen.Add(panel);
            panel.Add(Lbl("The Spice Trip is complete", "dg-title"));
            panel.Add(Lbl("Pyraketh, the Cinder-Maw, lies still. Every boss of the Path has fallen to you.", "dg-sub"));
            _completeTokens = El("dg-tokens");
            panel.Add(_completeTokens);
            _completeStats = Lbl("", "dg-stats");
            panel.Add(_completeStats);
            var b = El("dg-buttons");
            panel.Add(b);
            Button(b, "Descend deeper", "dg-descend", () => { _ctx.Audio.Sfx("ui"); _g.Descend(); }, "primary");
            Button(b, "Main menu", "dg-complete-menu", () => { _ctx.Audio.Sfx("ui"); _act.QuitToMenu(); });
            Root.Add(screen);
            return screen;
        }

        void FillComplete()
        {
            _completeTokens.Clear();
            for (int i = 0; i < Defs.Bosses.Length; i++)
            {
                var t = El("dg-token");
                var s = _view.BossToken(i);
                if (s != null) SetBackground(t, s); else { t.AddToClassList("blank"); t.Add(Lbl("✦", "dg-token-glyph")); }
                var c = Defs.Bosses[i].color;
                t.style.borderTopColor = t.style.borderBottomColor = t.style.borderLeftColor = t.style.borderRightColor = c;
                _completeTokens.Add(t);
            }
            var p = _g.Player;
            _completeStats.text = $"Floor {_g.FloorNumber}     Time {Clock(_g.RunTime)}     Kills {_g.Kills}     Gold {p.gold}     Level {p.level}";
        }

        VisualElement BuildRushOver()
        {
            var screen = El("dg-screen dg-veil", "dungeon-rush-over");
            var panel = El("dg-panel dg-result");
            screen.Add(panel);
            _rushTitle = Lbl("", "dg-title");
            panel.Add(_rushTitle);
            _rushLine = Lbl("", "dg-big-line");
            panel.Add(_rushLine);
            _rushBest = Lbl("", "dg-sub");
            panel.Add(_rushBest);
            var b = El("dg-buttons");
            panel.Add(b);
            Button(b, "Try again", "dg-rush-again", () => { _ctx.Audio.Sfx("ui"); _act.BossRush(); }, "primary");
            Button(b, "Main menu", "dg-rush-menu", () => { _ctx.Audio.Sfx("ui"); _act.QuitToMenu(); });
            Root.Add(screen);
            return screen;
        }

        void FillRushOver()
        {
            var g = _g;
            var best = g.Profile.bestRush;
            if (g.RushWon)
            {
                _rushTitle.text = "Boss Rush conquered";
                _rushLine.text = $"All five bosses in {Clock(g.RushTime)}";
                _rushBest.text = g.RushRecord ? "A new record!" : $"Best: {Clock(best)}";
                _rushBest.EnableInClassList("gold", g.RushRecord);
            }
            else
            {
                _rushTitle.text = "Defeated";
                _rushLine.text = $"{g.CurrentBoss.name} bested you at stage {g.RushStage + 1} of {Defs.Bosses.Length}.";
                _rushBest.text = best > 0 ? $"Best: {Clock(best)}" : "";
                _rushBest.EnableInClassList("gold", false);
            }
        }
    }
}
