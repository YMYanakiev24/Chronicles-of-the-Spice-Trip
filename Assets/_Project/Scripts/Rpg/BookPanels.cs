/*
 * BookPanels.cs
 * ----------------------------------------------------------------------------
 * The "ancient tome" modal from UI.js: a two-page parchment book with tabs —
 * Satchel (inventory + character sheet + relics), Spellbook, Bestiary, World
 * Map (with fast travel), Journal (quests, tracking), Settings — plus the
 * pause menu, all built from the same book.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using System.Linq;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Rpg
{
    public class BookPanels
    {
        public readonly VisualElement Root;
        public string ActivePanel { get; private set; }   // inventory | skills | bestiary | map | quests | settings | pause

        readonly ChronicleGame _g;
        readonly GameContext _ctx;
        readonly RpgHud _hud;
        readonly VisualElement _book, _tabs;
        readonly ScrollView _left, _right;
        readonly Action _save, _quit, _deleteSave, _onSettingsChanged;

        public BookPanels(ChronicleGame game, GameContext ctx, RpgHud hud, VisualElement layer,
            Action save, Action quit, Action deleteSave, Action onSettingsChanged)
        {
            _g = game;
            _ctx = ctx;
            _hud = hud;
            _save = save;
            _quit = quit;
            _deleteSave = deleteSave;
            _onSettingsChanged = onSettingsChanged;

            Root = El("book-modal hidden", "book-modal");
            var frame = El("book-frame");
            _tabs = El("book-tabs", "book-tabs");
            frame.Add(_tabs);
            _book = El("book");
            SetBackground(_book, ParchmentTexture());
            _left = new ScrollView(ScrollViewMode.Vertical) { name = "page-left" };
            _left.AddToClassList("book-page");
            _right = new ScrollView(ScrollViewMode.Vertical) { name = "page-right" };
            _right.AddToClassList("book-page");
            _book.Add(_left);
            _book.Add(El("book-spine"));
            _book.Add(_right);
            var close = Btn(null, "book-close", ClosePanel, "close", 10);
            close.name = "book-close";
            _book.Add(close);
            frame.Add(_book);
            Root.Add(frame);
            layer.Add(Root);
            Root.RegisterCallback<GeometryChangedEvent>(_ =>
            {
                // width: min(940px, 94vw); height: min(620px, 88vh)
                frame.style.width = Mathf.Min(940, 0.94f * Root.resolvedStyle.width);
                _book.style.height = Mathf.Min(620, 0.88f * Root.resolvedStyle.height) - 40;
            });
        }

        static Texture2D _parchment;

        // radial-gradient(ellipse at 50% 0%, #efe3c6, #d8c69e 70%, #c2ac7e 100%) with darkened edges
        static Texture2D ParchmentTexture()
        {
            if (_parchment != null) return _parchment;
            const int w = 188, h = 124;
            var c = new PixelCanvas(w, h);
            Color32 a = PixelCanvas.Col("#efe3c6"), b = PixelCanvas.Col("#d8c69e"), d = PixelCanvas.Col("#c2ac7e");
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    float dx = (x + 0.5f - w / 2f) / (w / 2f), dy = (y + 0.5f) / h;
                    float t = Mathf.Clamp01(Mathf.Sqrt(dx * dx + dy * dy) / 1.2f);
                    var col = t < 0.7f ? Color32.Lerp(a, b, t / 0.7f) : Color32.Lerp(b, d, (t - 0.7f) / 0.3f);
                    float edge = Mathf.Min(x, w - 1 - x) / (w * 0.08f);
                    if (edge < 1) col = Color32.Lerp(new Color32((byte)(col.r * 0.75f), (byte)(col.g * 0.75f), (byte)(col.b * 0.75f), 255), col, edge);
                    c.Pixels[y * w + x] = col;
                }
            return _parchment = c.ToTexture(null, FilterMode.Bilinear);
        }

        // =========================================================================
        // Book plumbing
        // =========================================================================
        struct Tab { public string id, label, icon; public Action open; }

        List<Tab> BookTabs() => new List<Tab>
        {
            new Tab { id = "inventory", label = "Satchel", icon = "scroll", open = OpenInventory },
            new Tab { id = "skills", label = "Spellbook", icon = "star", open = OpenSkills },
            new Tab { id = "bestiary", label = "Bestiary", icon = "paw", open = OpenBestiary },
            new Tab { id = "map", label = "World Map", icon = "map", open = OpenMap },
            new Tab { id = "quests", label = "Journal", icon = "emblem", open = OpenJournal },
            new Tab { id = "settings", label = "Settings", icon = "gear", open = () => OpenSettings(true) },
        };

        void OpenBook(List<Tab> tabs, string activeTab)
        {
            _tabs.Clear();
            foreach (var t in tabs)
            {
                var tab = t;
                var b = Btn(t.label, "book-tab" + (t.id == activeTab ? " active" : ""), () => { _ctx.Audio.Sfx("ui"); tab.open(); }, t.icon, 11);
                b.name = "tab-" + t.id;
                _tabs.Add(b);
            }
            _left.Clear();
            _right.Clear();
            Show(Root, true);
            _ctx.Audio.Sfx("ui");
        }

        public void ClosePanel()
        {
            Show(Root, false);
            ActivePanel = null;
            _ctx.Audio.Sfx("ui");
            _onSettingsChanged();   // Game.onPanelClosed saves settings
        }

        // Toggle helper so the same key opens & closes a panel.
        public void TogglePanel(string name)
        {
            if (ActivePanel == name) { ClosePanel(); return; }
            switch (name)
            {
                case "inventory": OpenInventory(); break;
                case "skills": OpenSkills(); break;
                case "map": OpenMap(); break;
                case "quests": OpenJournal(); break;
            }
        }

        // --- page helpers --------------------------------------------------------------
        static Label Title(VisualElement page, string text, string color = null)
        {
            var l = Lbl(text, "page-title");
            if (color != null) l.style.color = Css.C(color);
            page.Add(l);
            return l;
        }

        static void Flourish(VisualElement page, string text) => page.Add(Lbl(text, "page-flourish"));
        static void Para(VisualElement page, string text, string cls = "spell-desc") => page.Add(Lbl(text, cls));
        static void Sub(VisualElement page, string text) => page.Add(Lbl(text, "sub-title"));

        static VisualElement Stats(VisualElement page, params string[] stats)
        {
            var row = El("spell-stats");
            foreach (var s in stats) row.Add(Lbl(s, "stat-chip"));
            page.Add(row);
            return row;
        }

        static void UpgradeBox(VisualElement page, string label, string text)
        {
            var box = El("upgrade-box");
            box.Add(Lbl($"<b>{label}</b> {text}", "upgrade-text"));
            page.Add(box);
        }

        // =========================================================================
        // Panels
        // =========================================================================
        public void OpenInventory()
        {
            ActivePanel = "inventory";
            OpenBook(BookTabs(), "inventory");
            var p = _g.Player;
            Title(_left, "Satchel");
            Flourish(_left, "Carried treasures & tonics");
            var entries = p.inventory.Where(kv => kv.Value > 0).Select(kv => kv.Key).ToList();
            if (entries.Count == 0) Para(_left, "Your satchel is empty. The Realm is full of things to find.", "empty-note");
            else
            {
                var grid = El("inv-grid");
                foreach (var id in entries)
                {
                    var it = _ctx.Db.Item(id);
                    if (it == null) continue;
                    bool usable = it.type == "consumable";
                    var itemId = id;
                    var row = new ClickEl(() => { if (usable) { _g.UseItem(itemId); OpenInventory(); } }) { name = "inv-" + id };
                    row.AddToClassList("inv-item");
                    if (usable) row.AddToClassList("usable");
                    row.Add(Icon(Icons.Has(id) ? id : "coin", 22, "inv-icon"));
                    row.Add(Lbl(it.name, "inv-name"));
                    row.Add(Lbl($"×{p.inventory[id]}", "inv-count"));
                    if (usable) row.Add(Lbl("use ▸", "inv-use"));
                    grid.Add(row);
                }
                _left.Add(grid);
            }

            // Relics + character sheet on the right page.
            Title(_right, "The Unmarked");
            var sheet = El("char-sheet");
            void Row(string k, string v) { var r = El("char-row"); r.Add(Lbl(k, "char-key")); r.Add(Lbl(v, "char-val")); sheet.Add(r); }
            Row("Level", p.level.ToString());
            Row("Health", $"{Mathf.CeilToInt(p.hp)} / {Mathf.RoundToInt(p.maxHp)}");
            Row("Mana", $"{Mathf.CeilToInt(p.mana)} / {Mathf.RoundToInt(p.maxMana)}");
            Row("Mana Regen", $"{p.manaRegen:0.0}/s");
            Row("Gold", p.gold.ToString());
            Row("Spells Known", p.learnedSpells.Count.ToString());
            _right.Add(sheet);
            Sub(_right, "Relics");
            if (p.relics.Count == 0) Para(_right, "No relics yet.", "empty-note");
            foreach (var id in p.relics)
            {
                var it = _ctx.Db.Item(id);
                var li = El("relic-row");
                li.Add(Icon(Icons.Has(id) ? id : "coin", 14, "btn-icon"));
                li.Add(Lbl($"{it.name} — <i>{it.desc}</i>", "relic-text"));
                _right.Add(li);
            }
        }

        public void OpenSkills()
        {
            ActivePanel = "skills";
            OpenBook(BookTabs(), "skills");
            var p = _g.Player;
            Title(_left, "Spellbook");
            Flourish(_left, "Six schools of the arcane");
            var list = El("entry-list");
            foreach (var id in _ctx.Db.spellOrder)
            {
                var s = _ctx.Db.Spell(id);
                bool known = p.learnedSpells.Contains(id);
                int slot = Array.IndexOf(p.hotbar, id);
                var spellId = id;
                var e = new ClickEl(() => { _ctx.Audio.Sfx("ui"); ShowSpell(spellId); }) { name = "spell-" + id };
                e.AddToClassList("book-entry");
                if (!known) e.AddToClassList("locked");
                var badge = El("spell-school");
                badge.style.borderTopColor = badge.style.borderBottomColor = badge.style.borderLeftColor = badge.style.borderRightColor = Css.C(s.color);
                badge.Add(Icon("school-" + s.school, 22));
                e.Add(badge);
                var meta = El("entry-meta");
                var nameRow = El("entry-name-row");
                nameRow.Add(Lbl(s.name, "entry-name"));
                nameRow.Add(known ? Lbl($"Tier {p.spellLevels[id]}", "tier") : Lbl($"Lv.{s.unlockLevel}", "lock"));
                meta.Add(nameRow);
                meta.Add(Lbl(s.school + (slot >= 0 ? $" · slot {slot + 1}" : ""), "entry-sub"));
                e.Add(meta);
                list.Add(e);
            }
            _left.Add(list);
            ShowSpell(_ctx.Db.spellOrder[0]);
        }

        void ShowSpell(string id)
        {
            var p = _g.Player;
            var s = _ctx.Db.Spell(id);
            bool known = p.learnedSpells.Contains(id);
            _right.Clear();
            Title(_right, s.name, s.color);
            Para(_right, $"{s.school} · unlocks at level {s.unlockLevel}", "entry-sub");
            Para(_right, s.desc);
            var stats = new List<string>();
            if (s.damage > 0) stats.Add($"Damage: {s.damage}");
            if (s.heal > 0) stats.Add($"Heal: {s.heal}");
            if (s.shield > 0) stats.Add($"Shield: {s.shield}");
            stats.Add($"Mana: {s.manaCost}");
            stats.Add($"Cooldown: {s.cooldown}s");
            Stats(_right, stats.ToArray());
            UpgradeBox(_right, "Upgrade:", s.upgrade);
            Para(_right, known ? $"Known at tier {p.spellLevels[id]}. Level up or complete quests to upgrade." : $"Reach level {s.unlockLevel} to learn this spell.",
                known ? "hint-line" : "hint-line locked-line");
        }

        public void OpenBestiary()
        {
            ActivePanel = "skills";   // the web version reuses 'skills' as the active panel id here
            OpenBook(BookTabs(), "bestiary");
            Title(_left, "Bestiary");
            Flourish(_left, "Know thy enemy");
            var list = El("entry-list");
            foreach (var c in _ctx.Db.creatures)
            {
                bool seen = _g.BestiarySeen.Contains(c.id);
                var id = c.id;
                var e = new ClickEl(() => { _ctx.Audio.Sfx("ui"); ShowBeast(id); }) { name = "beast-" + id };
                e.AddToClassList("book-entry");
                if (!seen) e.AddToClassList("unknown");
                var dot = El("beast-dot");
                dot.style.backgroundColor = Css.C(c.color);
                e.Add(dot);
                e.Add(Lbl(seen ? c.name : "??? " + (c.boss ? "(Boss)" : ""), "entry-name"));
                list.Add(e);
            }
            _left.Add(list);
            ShowBeast(_ctx.Db.creatures[0].id);
        }

        void ShowBeast(string id)
        {
            var c = _ctx.Db.Creature(id);
            _right.Clear();
            if (_g.BestiarySeen.Contains(id))
            {
                Title(_right, c.name + (c.boss ? " ⚔" : ""), c.color);
                Stats(_right, $"HP: {c.hp}", $"Damage: {c.damage}", $"Speed: {c.speed}", $"Weakness: {c.weakness}");
                Para(_right, c.lore);
            }
            else
            {
                Title(_right, "Unknown Creature");
                Para(_right, "You have not yet encountered this being. Slay one to record it here.", "empty-note");
            }
        }

        public void OpenJournal()
        {
            ActivePanel = "quests";
            OpenBook(BookTabs(), "quests");
            Title(_left, "Journal");
            Flourish(_left, "Deeds & errands");
            var q = _g.Quests;
            var list = El("entry-list");
            foreach (var id in _ctx.Db.questOrder)
            {
                string st = q.state[id];
                var questId = id;
                var e = new ClickEl(() => { _ctx.Audio.Sfx("ui"); ShowQuest(questId); }) { name = "quest-" + id };
                e.AddToClassList("book-entry");
                e.AddToClassList(st);
                string tag = st switch { "available" => "New", "active" => "Active", "ready" => "Ready", "completed" => "✔", _ => "" };
                e.Add(Lbl(tag, "quest-tag " + st));
                e.Add(Lbl(_ctx.Db.Quest(id).title, "entry-name"));
                list.Add(e);
            }
            _left.Add(list);
            ShowQuest(q.tracked ?? _ctx.Db.questOrder[0]);
        }

        void ShowQuest(string id)
        {
            var q = _g.Quests;
            var def = _ctx.Db.Quest(id);
            _right.Clear();
            Title(_right, def.title);
            Para(_right, def.summary, "quest-summary");
            Para(_right, $"<i>{def.lore}</i>");
            Sub(_right, "Objectives");
            foreach (var (text, done) in q.ObjectiveText(id))
            {
                var li = El("obj-row" + (done ? " done" : ""));
                li.Add(Icon(done ? "check" : "diamond", 9, "btn-icon"));
                li.Add(Lbl(done ? $"<s>{text}</s>" : text, "obj-text"));
                _right.Add(li);
            }
            var r = def.rewards;
            var parts = new List<string>();
            if (r.xp > 0) parts.Add($"{r.xp} XP");
            if (r.gold > 0) parts.Add($"{r.gold} gold");
            if (!string.IsNullOrEmpty(r.spell)) parts.Add($"Spell: {_ctx.Db.Spell(r.spell).name}");
            foreach (var it in r.items) parts.Add(_ctx.Db.Item(it)?.name ?? it);
            UpgradeBox(_right, "Rewards:", string.Join(" · ", parts));
            if (q.state[id] == "available")
            {
                var track = Btn("Track this quest", "menu-btn small", () =>
                {
                    q.tracked = id;
                    _hud.RefreshQuestTracker();
                    _hud.Toast("Now tracking: " + def.title, "gold");
                });
                track.name = "track-btn";
                track.style.marginTop = 12;
                _right.Add(track);
            }
        }

        public void OpenMap()
        {
            ActivePanel = "map";
            OpenBook(BookTabs(), "map");
            Title(_left, "Map of the Hidden World");
            Flourish(_left, "Six realms beyond the Veil");
            var list = El("entry-list");
            foreach (var id in _ctx.Db.regionOrder)
            {
                var r = _ctx.Db.Region(id);
                bool unlocked = _g.UnlockedRegions.Contains(id);
                bool current = _g.CurrentRegion == id;
                var regionId = id;
                var e = new ClickEl(() => { if (unlocked) { _ctx.Audio.Sfx("ui"); ShowRegion(regionId); } }) { name = "region-" + id };
                e.AddToClassList("book-entry");
                if (!unlocked) e.AddToClassList("locked");
                if (current) e.AddToClassList("current");
                var orb = El("region-orb");
                // The web code read r.sky.top, which regions don't have; the light tint is used instead.
                orb.style.backgroundColor = Css.C(r.lightTint);
                e.Add(orb);
                var meta = El("entry-meta");
                var nameRow = El("entry-name-row");
                nameRow.Add(Lbl(unlocked ? r.name : "???", "entry-name"));
                if (current) nameRow.Add(Lbl("you are here", "here"));
                meta.Add(nameRow);
                meta.Add(Lbl(unlocked ? r.subtitle : "Locked region", "entry-sub"));
                e.Add(meta);
                list.Add(e);
            }
            _left.Add(list);
            ShowRegion(_g.CurrentRegion);
        }

        void ShowRegion(string id)
        {
            var r = _ctx.Db.Region(id);
            bool unlocked = _g.UnlockedRegions.Contains(id);
            bool current = _g.CurrentRegion == id;
            _right.Clear();
            Title(_right, unlocked ? r.name : "Uncharted");
            Para(_right, unlocked ? r.subtitle : "", "entry-sub");
            Para(_right, unlocked ? r.description : "You have not yet earned passage to this region. Grow in power and complete quests to unlock it.");
            if (unlocked && !current)
            {
                var travel = Btn("Travel here", "menu-btn small", () => { ClosePanel(); _g.TravelTo(id); });
                travel.name = "travel-btn";
                travel.style.marginTop = 12;
                _right.Add(travel);
            }
            if (current) Para(_right, "You are currently exploring this region.", "hint-line");
        }

        public void OpenSettings(bool inBook)
        {
            ActivePanel = "settings";
            var s = _ctx.Settings;
            if (inBook) OpenBook(BookTabs(), "settings");
            else OpenBook(new List<Tab> { new Tab { id = "settings", label = "Settings", icon = "gear", open = () => OpenSettings(false) } }, "settings");
            Title(_left, "Settings");
            Flourish(_left, "Attune the Realm to your liking");
            var listEl = El("settings-list");
            Slider SliderRow(string label, float min, float max, float value, Action<float> set)
            {
                var row = El("set-row");
                row.Add(Lbl(label, "set-label"));
                var sl = new Slider(min, max) { value = value };
                sl.AddToClassList("set-slider");
                sl.RegisterValueChangedCallback(e => set(e.newValue));
                row.Add(sl);
                listEl.Add(row);
                return sl;
            }
            Toggle ToggleRow(string label, bool value, Action<bool> set)
            {
                var t = new Toggle(label) { value = value };
                t.AddToClassList("set-toggle");
                t.RegisterValueChangedCallback(e => set(e.newValue));
                listEl.Add(t);
                return t;
            }
            SliderRow("Music Volume", 0, 100, s.musicVolume * 100, v => { s.musicVolume = v / 100; _onSettingsChanged(); }).name = "set-music";
            SliderRow("Effects Volume", 0, 100, s.sfxVolume * 100, v => { s.sfxVolume = v / 100; _onSettingsChanged(); }).name = "set-sfx";
            SliderRow("Mouse Sensitivity", 20, 200, s.mouseSensitivity * 100, v => { s.mouseSensitivity = v / 100; });
            ToggleRow("Shadows", s.shadows, v => s.shadows = v);
            ToggleRow("Mute All", s.muted, v => { s.muted = v; _onSettingsChanged(); }).name = "set-mute";
            _left.Add(listEl);
            var buttons = El("settings-buttons");
            buttons.Add(Btn("Save Game", "menu-btn small", () => { _save(); _hud.Toast("Chronicle saved.", "gold"); }));
            buttons.Add(Btn("Delete Save", "menu-btn small danger", () => { _deleteSave(); _hud.Toast("Save deleted.", "warn"); }));
            _left.Add(buttons);
            Title(_right, inBook ? "Controls" : "Controls & Lore");
            ControlsInto(_right);
        }

        void ControlsInto(VisualElement page)
        {
            var table = El("controls-table");
            (string, string)[] rows =
            {
                ("W A S D", "Move"), ("Mouse", "Aim"), ("1 – 6", "Wield a god's power"), ("Left Click", "Swing the godless blade"),
                ("Right Click", "Cast power in slot 1"), ("Shift", "Run"), ("Space", "Dodge-roll"), ("E", "Talk / interact / collect"),
                ("Tab", "Quest journal"), ("I", "Satchel (inventory)"), ("K", "Tome of powers"), ("M", "Map of the hidden world"), ("Esc", "Pause / menu"),
            };
            foreach (var (k, v) in rows)
            {
                var r = El("control-row");
                r.Add(Lbl($"<b>{k}</b>", "control-key"));
                r.Add(Lbl(v, "control-val"));
                table.Add(r);
            }
            page.Add(table);
            Sub(page, "The Theurgoi");
            foreach (var g in _ctx.Db.gods)
                page.Add(Lbl($"<b><color={g.color}>{g.name}</color></b> — {g.domain}", "god-row"));
        }

        // --- Pause ----------------------------------------------------------------
        public void OpenPause()
        {
            ActivePanel = "pause";
            OpenBook(new List<Tab>(), "pause");
            Title(_left, "Paused");
            Flourish(_left, "The Realm holds its breath");
            var buttons = El("menu-buttons pause-buttons");
            void B(string id, string label, Action fn, string cls = "menu-btn")
            {
                var b = Btn(label, cls, () => { _ctx.Audio.Sfx("ui"); fn(); });
                b.name = id;
                buttons.Add(b);
            }
            B("pause-resume", "Resume", ClosePanel);
            B("pause-journal", "Quest Journal", OpenJournal);
            B("pause-skills", "Spellbook", OpenSkills);
            B("pause-map", "World Map", OpenMap);
            B("pause-settings", "Settings", () => OpenSettings(true));
            B("pause-save", "Save Chronicle", () => { _save(); _hud.Toast("Chronicle saved.", "gold"); });
            B("pause-quit", "Quit to Title", () => { ClosePanel(); _quit(); }, "menu-btn danger");
            _left.Add(buttons);
            Title(_right, "The Unmarked");
            ControlsInto(_right);
        }
    }
}
