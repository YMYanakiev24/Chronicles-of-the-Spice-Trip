/*
 * RpgHud.cs
 * ----------------------------------------------------------------------------
 * The Chronicle HUD and its overlays (UI.js): vitals with level badge, the
 * six-slot hotbar with cooldown sweeps, gold and region pills, the quest
 * tracker, the circular minimap, interaction prompt, tutorial hint, toasts,
 * banners, floating damage numbers, damage vignette, screen shake, the
 * dialogue box and the death screen. Implements IRpgUi for the rules; the
 * book panels live in BookPanels.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Rpg
{
    public class RpgHud : IRpgUi
    {
        public readonly VisualElement Root, Hud;
        public BookPanels Book;
        readonly ChronicleGame _g;
        readonly GameContext _ctx;
        readonly RpgView _view;
        readonly Action _onRespawn;

        // HUD parts
        readonly VisualElement _hpFill, _mpFill, _xpFill, _shieldPip, _interact, _hint, _toastStack, _dmgLayer, _vignette, _minimap;
        readonly Label _hpLabel, _mpLabel, _level, _gold, _region, _shieldVal, _banner, _interactText, _hintText;
        readonly VisualElement _tracker;
        readonly List<(VisualElement slot, VisualElement icon, VisualElement cd)> _slots = new List<(VisualElement, VisualElement, VisualElement)>();
        // overlays
        readonly VisualElement _dialogue, _dlgChoices, _death;
        readonly Label _dlgSpeaker, _dlgText;

        readonly List<(Label el, float life)> _toasts = new List<(Label, float)>();
        readonly List<(Label el, float wx, float wy, float life, float offX)> _numbers = new List<(Label, float, float, float, float)>();
        float _bannerTimer, _hintTimer, _shake, _vignetteT, _time;
        PixelCanvas _miniCanvas;
        Texture2D _miniTex;

        public RpgHud(ChronicleGame game, GameContext ctx, RpgView view, VisualElement layer, Action onRespawn)
        {
            _g = game;
            _ctx = ctx;
            _view = view;
            _onRespawn = onRespawn;
            Root = El("rpg-root", "rpg-root");
            Root.pickingMode = PickingMode.Ignore;
            layer.Add(Root);

            Hud = El("hud hidden", "hud");
            Hud.pickingMode = PickingMode.Ignore;
            Root.Add(Hud);

            Hud.Add(El("crosshair"));

            // Bottom-left vitals
            var vitals = El("vitals");
            var portrait = El("portrait");
            _level = Lbl("1", "level-badge");
            portrait.Add(_level);
            vitals.Add(portrait);
            var bars = El("bars");
            _hpFill = Bar(bars, "health", out _hpLabel);
            _mpFill = Bar(bars, "mana", out _mpLabel);
            _xpFill = Bar(bars, "xp", out _);
            _shieldPip = El("shield-pip hidden");
            _shieldPip.Add(Icon("shield", 11, "btn-icon"));
            _shieldVal = Lbl("0", "shield-val");
            _shieldPip.Add(_shieldVal);
            bars.Add(_shieldPip);
            vitals.Add(bars);
            Hud.Add(vitals);

            // Spell hotbar
            var hotbar = El("hotbar", "hotbar");
            for (int i = 0; i < 6; i++)
            {
                int slotIndex = i;
                var slot = new ClickEl(() => { if (!IsBlocking) _g.Player?.TryCastSlot(slotIndex); }) { name = "slot-" + (i + 1) };
                slot.AddToClassList("spell-slot");
                slot.AddToClassList("empty");
                var icon = El("slot-icon");
                icon.pickingMode = PickingMode.Ignore;
                var cd = El("slot-cooldown");
                cd.pickingMode = PickingMode.Ignore;
                slot.Add(icon);
                slot.Add(cd);
                var key = Lbl((i + 1).ToString(), "slot-key");
                key.pickingMode = PickingMode.Ignore;
                slot.Add(key);
                hotbar.Add(slot);
                _slots.Add((slot, icon, cd));
            }
            Hud.Add(hotbar);

            // Gold + region
            var topleft = El("topleft");
            var goldPill = El("gold-pill");
            goldPill.Add(Icon("coin", 14, "btn-icon"));
            _gold = Lbl("0", "gold-val");
            goldPill.Add(_gold);
            topleft.Add(goldPill);
            var regionPill = El("region-pill");
            _region = Lbl("The Mistwood", "region-text");
            regionPill.Add(_region);
            topleft.Add(regionPill);
            Hud.Add(topleft);

            // Quest tracker
            _tracker = El("quest-tracker", "quest-tracker");
            Hud.Add(_tracker);

            // Minimap
            var mframe = El("minimap-frame");
            _minimap = El("minimap");
            mframe.Add(_minimap);
            mframe.Add(Lbl("N", "minimap-compass"));
            Hud.Add(mframe);

            // Interaction prompt + hint
            var irow = El("interact-row");
            irow.pickingMode = PickingMode.Ignore;
            _interact = El("interact-prompt hidden");
            _interactText = Lbl("", "interact-text");
            _interact.Add(_interactText);
            irow.Add(_interact);
            Hud.Add(irow);
            var hrow = El("hint-row-top");
            hrow.pickingMode = PickingMode.Ignore;
            _hint = El("hint-bar hidden");
            _hint.Add(Icon("star", 9, "btn-icon"));
            _hintText = Lbl("", "hint-text");
            _hint.Add(_hintText);
            hrow.Add(_hint);
            Hud.Add(hrow);

            _toastStack = El("toast-stack");
            _toastStack.pickingMode = PickingMode.Ignore;
            Hud.Add(_toastStack);
            var brow = El("banner-row");
            brow.pickingMode = PickingMode.Ignore;
            _banner = Lbl("", "banner hidden");
            brow.Add(_banner);
            Hud.Add(brow);

            _vignette = El("damage-vignette");
            _vignette.pickingMode = PickingMode.Ignore;
            SetBackground(_vignette, DamageVignetteTexture());
            Hud.Add(_vignette);
            _dmgLayer = El("dmg-layer");
            _dmgLayer.pickingMode = PickingMode.Ignore;
            Hud.Add(_dmgLayer);

            var reminder = Lbl("<b>1-6</b> Cast   <b>WASD</b> Move   <b>E</b> Interact   <b>Tab</b> Journal   <b>I</b> Satchel   <b>K</b> Spellbook   <b>M</b> Map   <b>Esc</b> Menu", "controls-reminder");
            reminder.text = Gold(reminder.text, "#ffd76bcc");
            var rrow = El("reminder-row");
            rrow.pickingMode = PickingMode.Ignore;
            rrow.Add(reminder);
            Hud.Add(rrow);

            // Dialogue
            _dialogue = El("dialogue hidden", "dialogue");
            var box = El("dialogue-box");
            _dlgSpeaker = Lbl("", "speaker");
            _dlgText = Lbl("", "dlg-text");
            _dlgChoices = El("dlg-choices");
            box.Add(_dlgSpeaker);
            box.Add(_dlgText);
            box.Add(_dlgChoices);
            _dialogue.Add(box);
            Root.Add(_dialogue);

            // Death
            _death = El("screen death hidden", "death-screen");
            _death.Add(Lbl("You Have Fallen", "death-title"));
            _death.Add(Lbl("But the hidden world is not done with you...", "death-sub"));
            var respawn = Btn("Return to the last shrine", "menu-btn", () => _onRespawn());
            respawn.name = "respawn-btn";
            _death.Add(respawn);
            Root.Add(_death);
        }

        static VisualElement Bar(VisualElement host, string kind, out Label label)
        {
            var row = El("bar-row");
            var bar = El("bar " + kind);
            var fill = El("bar-fill " + kind + "-fill");
            bar.Add(fill);
            label = Lbl("", "bar-label");
            if (kind != "xp") bar.Add(label);
            row.Add(bar);
            host.Add(row);
            return fill;
        }

        /// <summary>CSS gave &lt;b&gt; text a gold colour; rich text needs the colour inline.</summary>
        public static string Gold(string html, string color = "#ffd76b") =>
            html.Replace("<b>", $"<b><color={color}>").Replace("</b>", "</color></b>");

        static Texture2D _dmgTex;

        // box-shadow: inset 0 0 160px rgba(180,20,20,0.85)
        static Texture2D DamageVignetteTexture()
        {
            if (_dmgTex != null) return _dmgTex;
            const int w = 160, h = 90;
            var c = new PixelCanvas(w, h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    float edge = Mathf.Min(Mathf.Min(x, w - 1 - x) / (float)w, Mathf.Min(y, h - 1 - y) / (float)h) * 2;
                    float a = Mathf.Clamp01(1 - edge / 0.35f);
                    c.Pixels[y * w + x] = new Color32(180, 20, 20, (byte)(a * a * 0.85f * 255));
                }
            return _dmgTex = c.ToTexture(null, FilterMode.Bilinear);
        }

        public void ShowHUD(bool show)
        {
            Show(Hud, show);
            if (show) RefreshHUD();
        }

        public bool HudVisible => !Hud.ClassListContains("hidden");

        // =========================================================================
        // IRpgUi
        // =========================================================================
        public bool IsBlocking => (Book != null && Book.ActivePanel != null) || !_dialogue.ClassListContains("hidden");
        public bool DialogueOpen => !_dialogue.ClassListContains("hidden");
        public bool DeathShowing => !_death.ClassListContains("hidden");

        public void RefreshHUD()
        {
            var p = _g.Player;
            if (p == null) return;
            RefreshSmooth();
            _hpLabel.text = $"{Mathf.CeilToInt(p.hp)}/{Mathf.RoundToInt(p.maxHp)}";
            _mpLabel.text = $"{Mathf.CeilToInt(p.mana)}/{Mathf.RoundToInt(p.maxMana)}";
            _level.text = p.level.ToString();
            _gold.text = p.gold.ToString();
            for (int i = 0; i < 6; i++)
            {
                var id = p.hotbar[i];
                var (slot, icon, _) = _slots[i];
                icon.Clear();
                if (id != null)
                {
                    slot.RemoveFromClassList("empty");
                    icon.Add(Icon("school-" + _g.Db.Spell(id).school, 28));
                    slot.tooltip = _g.Db.Spell(id).name;
                }
                else { slot.AddToClassList("empty"); slot.tooltip = ""; }
            }
            RefreshQuestTracker();
        }

        void RefreshSmooth()
        {
            var p = _g.Player;
            if (p == null) return;
            _hpFill.style.width = Length.Percent(p.hp / p.maxHp * 100);
            _mpFill.style.width = Length.Percent(p.mana / p.maxMana * 100);
            _xpFill.style.width = Length.Percent(Mathf.Clamp01(p.xp / GameDatabase.XpForLevel(p.level)) * 100);
            _hpLabel.text = $"{Mathf.CeilToInt(p.hp)}/{Mathf.RoundToInt(p.maxHp)}";
            _mpLabel.text = $"{Mathf.CeilToInt(p.mana)}/{Mathf.RoundToInt(p.maxMana)}";
            // Cooldown sweeps.
            for (int i = 0; i < 6; i++)
            {
                var id = p.hotbar[i];
                float ratio = id != null && p.Cooldown(id) > 0 ? p.Cooldown(id) / _g.Db.Spell(id).cooldown : 0;
                _slots[i].cd.style.height = Length.Percent(ratio * 100);
            }
            // Shield pip.
            Show(_shieldPip, p.shield > 0);
            if (p.shield > 0) _shieldVal.text = Mathf.CeilToInt(p.shield).ToString();
        }

        public void SetRegionName(string name) => _region.text = name;

        public void RefreshQuestTracker()
        {
            _tracker.Clear();
            var id = _g.Quests.tracked;
            if (id == null) { _tracker.Add(Lbl("No active quest", "qt-title")); return; }
            var def = _g.Db.Quest(id);
            var title = El("qt-title-row");
            title.Add(Icon("emblem", 11, "btn-icon"));
            title.Add(Lbl(def.title, "qt-title"));
            if (_g.Quests.state[id] == "ready") title.Add(Lbl("READY", "ready-tag"));
            _tracker.Add(title);
            foreach (var (text, done) in _g.Quests.ObjectiveText(id))
            {
                var li = El("qt-item" + (done ? " done" : ""));
                li.Add(Icon(done ? "check" : "diamond", 9, "btn-icon"));
                li.Add(Lbl(done ? $"<s>{text}</s>" : text, "qt-text"));
                _tracker.Add(li);
            }
        }

        public void Toast(string msg, string type = "info", float dur = 2.4f)
        {
            var el = Lbl(msg, "toast " + type);
            _toastStack.Add(el);
            _toasts.Add((el, dur));
            // Cap to avoid clutter.
            while (_toasts.Count > 5) { _toasts[0].el.RemoveFromHierarchy(); _toasts.RemoveAt(0); }
        }

        public IReadOnlyList<string> ToastTexts
        {
            get { var l = new List<string>(); foreach (var t in _toasts) l.Add(t.el.text); return l; }
        }

        public void BigBanner(string text)
        {
            _banner.text = text;
            Show(_banner, true);
            _bannerTimer = 2.2f;
        }

        public string BannerText => _banner.text;
        public void DamageFlash() => _vignetteT = 0.4f;
        public void Shake(float amount) => _shake = Mathf.Min(1.2f, _shake + amount);

        public void SpawnDamageNumber(float wx, float wy, int amount, bool crit)
        {
            var el = Lbl(crit ? $"{amount}!" : amount.ToString(), "dmg-number" + (crit ? " crit" : ""));
            el.pickingMode = PickingMode.Ignore;
            _dmgLayer.Add(el);
            _numbers.Add((el, wx, wy, 1.0f, (UnityEngine.Random.value - 0.5f) * 14));
        }

        public void ShowInteract(string richText)
        {
            var t = Gold(richText);
            if (_interactText.text != t) _interactText.text = t;
            Show(_interact, true);
        }

        public void HideInteract() => Show(_interact, false);
        public bool InteractShowing => !_interact.ClassListContains("hidden");

        public void ShowHint(string richText, float dur = 6)
        {
            _hintText.text = Gold(richText);
            Show(_hint, true);
            _hintTimer = dur;
        }

        public void ShowDialogue(Npc npc, string richText, IReadOnlyList<DialogueChoice> choices)
        {
            _dlgSpeaker.text = npc.def.name;
            _dlgText.text = Gold(richText);
            _dlgChoices.Clear();
            foreach (var c in choices)
            {
                var choice = c;
                var b = Btn(c.label, "dlg-choice" + (c.primary ? " primary" : ""), () => { _ctx.Audio.Sfx("ui"); choice.action(); });
                _dlgChoices.Add(b);
            }
            Show(_dialogue, true);
        }

        public string DialogueText => _dlgText.text;
        public IEnumerable<VisualElement> DialogueChoices => _dlgChoices.Children();
        public void HideDialogue() => Show(_dialogue, false);
        public void ShowDeath() => Show(_death, true);
        public void HideDeath() => Show(_death, false);

        // =========================================================================
        // Per-frame UI update
        // =========================================================================
        public void Tick(float dt)
        {
            _time += dt;
            for (int i = _toasts.Count - 1; i >= 0; i--)
            {
                var (el, life) = _toasts[i];
                life -= dt;
                if (life < 0.5f) el.style.opacity = Mathf.Max(0, life / 0.5f);
                if (life <= 0) { el.RemoveFromHierarchy(); _toasts.RemoveAt(i); }
                else _toasts[i] = (el, life);
            }

            // Banner: pop in over the first 18%, hold, fade out from 78% (bannerPop).
            if (_bannerTimer > 0)
            {
                _bannerTimer -= dt;
                float k = 1 - Mathf.Clamp01(_bannerTimer / 2.2f);
                float pop = Mathf.Clamp01(k / 0.18f);
                float ease = 1 - (1 - pop) * (1 - pop);
                _banner.style.scale = new Scale(Vector2.one * Mathf.Lerp(0.7f, 1, ease));
                _banner.style.letterSpacing = Mathf.Lerp(0.4f, 0.16f, ease) * 48;
                _banner.style.opacity = k < 0.18f ? ease : k < 0.78f ? 1 : 1 - (k - 0.78f) / 0.22f;
                if (_bannerTimer <= 0) Show(_banner, false);
            }
            if (_hintTimer > 0) { _hintTimer -= dt; if (_hintTimer <= 0) Show(_hint, false); }

            // Floating damage numbers — reproject world→screen each frame.
            float pw = Root.resolvedStyle.width, ph = Root.resolvedStyle.height;
            for (int i = _numbers.Count - 1; i >= 0; i--)
            {
                var (el, wx, wy, life, offX) = _numbers[i];
                life -= dt; wy -= 26 * dt;
                float sx = (wx - _g.CamX) / _view.ViewW * pw, sy = (wy - _g.CamY) / _view.ViewH * ph;
                el.style.left = sx + offX;
                el.style.top = sy;
                el.style.opacity = Mathf.Max(0, life);
                if (life <= 0) { el.RemoveFromHierarchy(); _numbers.RemoveAt(i); }
                else _numbers[i] = (el, wx, wy, life, offX);
            }

            // Screen shake (camera offset in screen pixels).
            if (_shake > 0)
            {
                _shake = Mathf.Max(0, _shake - dt * 2);
                float s = _shake * 12;
                _view.ShakeX = (UnityEngine.Random.value - 0.5f) * s;
                _view.ShakeY = (UnityEngine.Random.value - 0.5f) * s;
            }
            else { _view.ShakeX = _view.ShakeY = 0; }

            if (_vignetteT > 0) _vignetteT -= dt;
            _vignette.style.opacity = Mathf.Max(0, _vignetteT / 0.4f);

            // interact prompt float (floatPulse 1.6s)
            _interact.style.translate = new Translate(0, -4 * (0.5f - 0.5f * Mathf.Cos(_time / 1.6f * Mathf.PI * 2)));

            if (HudVisible && _g.Player != null)
            {
                RefreshSmooth();
                DrawMinimap();
            }
        }

        // Top-down minimap of the current tilemap centred on the player.
        void DrawMinimap()
        {
            const int W = 170, R = 85;
            _miniCanvas ??= new PixelCanvas(W, W);
            var c = _miniCanvas;
            var map = _g.Map;
            var p = _g.Player;
            c.Clear();
            c.FillRect(0, 0, W, W, new Color32(8, 8, 16, 230));

            const float tilePx = 5, scale = tilePx / 16f;
            Vector2 ToMap(float wx, float wy) => new Vector2(R + (wx - p.x) * scale, R + (wy - p.y) * scale);

            int ptx = Mathf.FloorToInt(p.x / 16), pty = Mathf.FloorToInt(p.y / 16);
            const int rad = 18;
            var wall = new Color32(120, 120, 150, 128);
            var floor = new Color32(40, 46, 60, 115);
            for (int ty = pty - rad; ty <= pty + rad; ty++)
                for (int tx = ptx - rad; tx <= ptx + rad; tx++)
                {
                    if (tx < 0 || ty < 0 || tx >= map.W || ty >= map.H) continue;
                    var m = ToMap(tx * 16 + 8, ty * 16 + 8);
                    c.FillRect(m.x - 2, m.y - 2, tilePx, tilePx, map.CellAt(tx, ty).wall ? wall : floor);
                }
            void Dot(Vector2 at, string col, float r) => c.FillCircle(at.x, at.y, r, PixelCanvas.Col(col));
            foreach (var pk in map.Pickups) Dot(ToMap(pk.x, pk.y), pk.type == "asphodel" ? "#cfe0ff" : "#9fe0ff", 1.6f);
            foreach (var n in _g.Npcs) Dot(ToMap(n.x, n.y), "#ffd24a", 2.6f);
            foreach (var e in _g.Enemies) if (!e.dead) Dot(ToMap(e.x, e.y), "#ff5a4a", 2.2f);

            // Player marker + facing tick.
            Dot(new Vector2(R, R), "#eafff0", 3);
            var f = Dir.Vec(p.facing);
            for (int i = 0; i <= 7; i++) c.FillRect(R + f.x * i - 1, R + f.y * i - 1, 2, 2, PixelCanvas.Col("#eafff0"));

            // circular clip + rim
            var rim = new Color32(150, 130, 90, 178);
            for (int y = 0; y < W; y++)
                for (int x = 0; x < W; x++)
                {
                    float d = Mathf.Sqrt((x + 0.5f - R) * (x + 0.5f - R) + (y + 0.5f - R) * (y + 0.5f - R));
                    int i = y * W + x;
                    if (d > R - 2 + 1.5f) c.Pixels[i] = default;
                    else if (d > R - 2 - 1.5f) c.Pixels[i] = rim;
                }
            bool first = _miniTex == null;
            _miniTex = c.ToTexture(_miniTex, FilterMode.Bilinear);
            if (first) SetBackground(_minimap, _miniTex);
            else _minimap.MarkDirtyRepaint();
        }
    }
}
