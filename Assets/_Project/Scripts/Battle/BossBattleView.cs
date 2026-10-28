/*
 * BossBattleView.cs
 * ----------------------------------------------------------------------------
 * Draws and drives the boss fight (BossBattle.js). BattleSim holds the rules;
 * this view owns the arena camera and sprites, reads the input, and runs the
 * UI overlay (health bars, Flee, hint, Victory/Defeat card).
 *
 * The arena is 800×450 logical pixels, shown letterboxed at
 * width = min(98vw, 94vh·16/9) like #bb-canvas. 1 world unit = 100 logical px,
 * with the origin at the arena centre and y flipped (canvas y points down).
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using SpiceTrip.Hub;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Battle
{
    public class BossBattleView : MonoBehaviour
    {
        const float PxPerUnit = 100f;

        GameContext _ctx;
        BattleTuning T;
        Camera _cam;
        Transform _world;
        Material _alpha, _additive, _screen;
        Sprite _white, _circle, _glow, _ellipse;
        MaterialPropertyBlock _mpb;

        public BattleSim Sim { get; private set; }
        BossDef _boss;
        CharacterDef _char;
        public bool Active => Sim != null && _root != null && !_root.ClassListContains("hidden");

        // scene objects
        SpriteRenderer _playerSr, _playerShadow;
        SpriteRenderer _arenaArt;                       // a boss's own backdrop (arena-<bossId>.jpg)
        readonly List<SpriteRenderer> _forest = new List<SpriteRenderer>();   // the procedural night forest
        Sprite _bossSprite, _feather, _ticket, _seed, _koi, _bus, _ring;
        readonly List<SpriteRenderer> _draws = new List<SpriteRenderer>();
        int _drawn;
        ChampionKitDef _kit;
        readonly List<(SpriteRenderer r, float x, float y, float s, float sway, Transform[] parts)> _trees = new List<(SpriteRenderer, float, float, float, float, Transform[])>();
        readonly List<SpriteRenderer> _fog = new List<SpriteRenderer>();
        bool _bossHasArt;

        // UI overlay
        VisualElement _root, _hud, _result, _bossFill, _playerFill;
        Label _bossLabel, _resultTitle, _resultSub, _hint, _resultReward, _resultTime, _stageBanner;
        VisualElement _stars;
        readonly List<(VisualElement slot, Label count)> _itemSlots = new List<(VisualElement, Label)>();

        // The Gauntlet: all four bosses back to back, health carried between stages.
        class GauntletRun { public int stage; public float totalTime, carryHp; }
        GauntletRun _run;
        float _nextStageAt = -1;
        public bool InGauntlet => _run != null;
        public int GauntletStage => _run?.stage ?? -1;
        public Progression.Reward? LastReward { get; private set; }
        public bool StageTransition => _nextStageAt >= 0;
        readonly List<VisualElement> _kitBars = new List<VisualElement>();
        readonly List<(VisualElement fill, Label label, Func<float> value, Func<string> text)> _kitMeters = new List<(VisualElement, Label, Func<float>, Func<string>)>();
        ClickEl _flee;
        Action _onExit;
        Action<string> _onBossDefeated;

        /// <summary>Synthetic input for tests and the autopilot; when set, devices are ignored.</summary>
        public BattleSim.Inputs? ScriptedInput { get; set; }

        public void Init(GameContext ctx, VisualElement overlayLayer, Material alpha, Material additive, Material screen,
            Action onExit, Action<string> onBossDefeated)
        {
            _ctx = ctx;
            T = ctx.Db.battle;
            _alpha = alpha; _additive = additive; _screen = screen;
            _onExit = onExit;
            _onBossDefeated = onBossDefeated;
            _mpb = new MaterialPropertyBlock();
            BuildSprites();
            BuildCamera();
            BuildScene();
            BuildOverlay(overlayLayer);
            _world.gameObject.SetActive(false);
            _cam.enabled = false;
        }

        // =====================================================================
        // Setup
        // =====================================================================
        void BuildSprites()
        {
            Texture2D Tex(PixelCanvas c, FilterMode f = FilterMode.Bilinear) => c.ToTexture(null, f);
            Sprite Spr(Texture2D t) => Sprite.Create(t, new UnityEngine.Rect(0, 0, t.width, t.height), new Vector2(0.5f, 0.5f), t.width, 0, SpriteMeshType.FullRect);

            var w = new PixelCanvas(4, 4);
            w.FillRect(0, 0, 4, 4, new Color32(255, 255, 255, 255));
            _white = Spr(Tex(w, FilterMode.Point));

            var circle = new PixelCanvas(128, 128);
            circle.FillCircle(64, 64, 63.5, new Color32(255, 255, 255, 255));
            _circle = Spr(Tex(circle));

            // Radial glow: white centre fading to transparent (canvas radial gradient color → clear).
            var glow = new PixelCanvas(64, 64);
            glow.FillRadial(0, 0, 64, 64, 32, 32, 32, new Color32(255, 255, 255, 255), new Color32(255, 255, 255, 0));
            _glow = Spr(Tex(glow));

            var ellipse = new PixelCanvas(128, 32);
            ellipse.FillEllipse(64, 16, 63.5, 15.5, new Color32(255, 255, 255, 255));
            _ellipse = Spr(Tex(ellipse));

            // Phase 3 projectiles and hazards, drawn as small pixel sprites.
            _feather = Spr(Tex(Grid(new[] { "..bbbbbbbbbb..", "wwwwwwwwwwwwwk", "..bbbbbbbbbb..", "...cccccccc..." },
                ('b', "#5a8ad8"), ('w', "#f4f6ff"), ('c', "#9fc4ff"), ('k', "#2a3a6a")), FilterMode.Point));
            _ticket = Spr(Tex(Grid(new[] { "kkkkkkkkkkkk", "kpppppppppk.", "kpddpdpddpk.", "kpppppppppk.", "kpdddpddppk.", "kpppppppppk.", "kkkkkkkkkkkk" },
                ('k', "#9a7a40"), ('p', "#efe6cc"), ('d', "#3a2a1a")), FilterMode.Point));
            _seed = Spr(Tex(Grid(new[] { ".gg.", "gllg", "gggg", ".gg." }, ('g', "#2a5a1a"), ('l', "#7fbf4a")), FilterMode.Point));
            _koi = Spr(Tex(Grid(new[] { "o.....oooo....", "oo..oowwoooo..", "ooooowoooooke.", "oo..oooowoo...", "o.....oooo...." },
                ('o', "#ff8a3a"), ('w', "#ffffff"), ('k', "#1a1414"), ('e', "#ffb070")), FilterMode.Point));
            _bus = Spr(Tex(Grid(new[]
            {
                "llllllllllllllllllllllllllllllll",
                "lyyyybbbbbbbbbbbbbbbbbbbbbbbbbbl",
                "lynnybbbbbbbbbbbbbbbbbbbbbbbbbbl",
                "lyyyybwwwbwwwbwwwbwwwbwwwbwwwwbl",
                "lbbbbbwwwbwwwbwwwbwwwbwwwbwwwwbl",
                "lbbbbbwwwbwwwbwwwbwwwbwwwbwwwwbl",
                "lbbbbbbbbbbbbbbbbbbbbbbbbbbddbbh",
                "lbbbbbbbbbbbbbbbbbbbbbbbbbbddbbh",
                "lssssssssssssssssssssssssssddssl",
                "llkkkllllllllllllllllllllkkklll.",
                "..kkk....................kkk....",
            }, ('l', "#1a3a7a"), ('b', "#2a6ad0"), ('w', "#bfe8ff"), ('y', "#ffb347"), ('n', "#1a1414"),
               ('d', "#1a3a7a"), ('h', "#ffe08a"), ('s', "#d8dce8"), ('k', "#0a0a10")), FilterMode.Point));
            var ring = new PixelCanvas(128, 128);
            ring.FillCircle(64, 64, 63.5, new Color32(255, 255, 255, 255));
            for (int y = 0; y < 128; y++)
                for (int x = 0; x < 128; x++)
                    if (Mathf.Sqrt((x + 0.5f - 64) * (x + 0.5f - 64) + (y + 0.5f - 64) * (y + 0.5f - 64)) < 57) ring.Pixels[y * 128 + x] = default;
            _ring = Spr(Tex(ring));
        }

        static PixelCanvas Grid(string[] rows, params (char key, string color)[] pal)
        {
            var c = new PixelCanvas(rows[0].Length, rows.Length);
            for (int y = 0; y < rows.Length; y++)
                for (int x = 0; x < rows[y].Length; x++)
                    foreach (var (key, color) in pal)
                        if (rows[y][x] == key) c.Px(x, y, color);
            return c;
        }

        void BuildCamera()
        {
            var go = new GameObject("BattleCamera");
            go.transform.SetParent(transform, false);
            go.transform.position = new Vector3(0, 0, -10);
            _cam = go.AddComponent<Camera>();
            _cam.orthographic = true;
            _cam.orthographicSize = T.arenaHeight / 2 / PxPerUnit;
            _cam.clearFlags = CameraClearFlags.SolidColor;
            _cam.backgroundColor = PixelCanvas.Col("#05040a");
            _cam.depth = 0;
            _cam.nearClipPlane = 0.1f;
            _cam.farClipPlane = 100;

            // The arena camera only covers the letterboxed rect; this one clears the rest.
            var clearGo = new GameObject("ScreenClearCamera");
            clearGo.transform.SetParent(transform, false);
            var clear = clearGo.AddComponent<Camera>();
            clear.clearFlags = CameraClearFlags.SolidColor;
            clear.backgroundColor = _cam.backgroundColor;
            clear.cullingMask = 0;
            clear.depth = -1;
            clear.orthographic = true;
        }

        Vector3 W(float x, float y) => new Vector3((x - T.arenaWidth / 2) / PxPerUnit, (T.arenaHeight / 2 - y) / PxPerUnit, 0);

        SpriteRenderer Make(string name, Sprite sprite, Material mat, int order, Color color, Transform parent = null)
        {
            var go = new GameObject(name);
            go.transform.SetParent(parent ? parent : _world, false);
            var sr = go.AddComponent<SpriteRenderer>();
            sr.sprite = sprite;
            sr.sharedMaterial = mat;
            sr.sortingOrder = order;
            sr.color = color;
            return sr;
        }

        /// <summary>Places a renderer as a logical-pixel rect (top-left x, y, w, h).</summary>
        void Rect(SpriteRenderer sr, float x, float y, float w, float h)
        {
            sr.transform.localPosition = W(x + w / 2, y + h / 2);
            Size(sr, w, h);
        }

        static void Size(SpriteRenderer sr, float wPx, float hPx)
        {
            var b = sr.sprite.bounds.size;
            sr.transform.localScale = new Vector3(wPx / PxPerUnit / b.x, hPx / PxPerUnit / b.y, 1);
        }

        void BuildScene()
        {
            _world = new GameObject("Arena").transform;
            _world.SetParent(transform, false);
            float AW = T.arenaWidth, AH = T.arenaHeight;

            // sky
            var sky = new PixelCanvas(1, 128);
            sky.FillVertical(0, 0, 1, 128, (0, PixelCanvas.Col("#0a0716")), (0.6f, PixelCanvas.Col("#120a22")), (1, PixelCanvas.Col("#0c1410")));
            var skyTex = sky.ToTexture(null, FilterMode.Bilinear);
            var skySr = Make("Sky", Sprite.Create(skyTex, new UnityEngine.Rect(0, 0, 1, 128), new Vector2(0.5f, 0.5f), 1), _alpha, 0, Color.white);
            Rect(skySr, 0, 0, AW, AH);
            _forest.Add(skySr);

            // A boss's painted arena replaces the forest when its image exists.
            _arenaArt = Make("ArenaArt", _white, _alpha, 0, Color.white);
            _arenaArt.enabled = false;

            // moon halo + moon
            float mx = AW * 0.8f, my = AH * 0.18f;
            var halo = Make("MoonHalo", _glow, _alpha, 1, new Color(180 / 255f, 170 / 255f, 210 / 255f, 0.35f));
            Rect(halo, mx - 90, my - 90, 180, 180);
            _forest.Add(halo);
            var moon = Make("Moon", _circle, _alpha, 2, PixelCanvas.Col("#c9c2dd"));
            Rect(moon, mx - 26, my - 26, 52, 52);
            _forest.Add(moon);

            // ground
            var ground = Make("Ground", _white, _alpha, 3, PixelCanvas.Col("#0e1a12"));
            Rect(ground, 0, AH * 0.82f, AW, AH * 0.18f);
            _forest.Add(ground);

            // trees are placed in Start() of each fight (random, like _spawnTrees)
            // fog bands: 3 rows of ellipses, screen-blended
            for (int i = 0; i < 3; i++)
                for (int x = -60; x < AW; x += 90)
                {
                    var f = Make($"Fog{i}", _ellipse, _screen, 20, new Color(120 / 255f, 130 / 255f, 150 / 255f, 0.05f + i * 0.02f));
                    Size(f, 120, 18);
                    _fog.Add(f);
                }

            // vignette: transparent at 0.3·AH from the centre → 60% black at 0.8·AH
            var vg = new PixelCanvas(160, 90);
            for (int y = 0; y < 90; y++)
                for (int x = 0; x < 160; x++)
                {
                    float dx = (x + 0.5f - 80) * AW / 160, dy = (y + 0.5f - 45) * AH / 90;
                    float d = Mathf.Sqrt(dx * dx + dy * dy);
                    float t = Mathf.Clamp01((d - AH * 0.3f) / (AH * 0.5f));
                    vg.Pixels[y * 160 + x] = new Color32(0, 0, 0, (byte)(t * 0.6f * 255));
                }
            var vgTex = vg.ToTexture(null, FilterMode.Bilinear);
            var vgSr = Make("Vignette", Sprite.Create(vgTex, new UnityEngine.Rect(0, 0, 160, 90), new Vector2(0.5f, 0.5f), 160), _alpha, 30, Color.white);
            Rect(vgSr, 0, 0, AW, AH);

            // fighters (images are set per fight)
            _playerSr = Make("Player", _white, _alpha, 50, Color.white);
            _playerShadow = Make("PlayerShadow", _ellipse, _alpha, 51, new Color(0, 0, 0, 0.35f));
        }

        /// <summary>
        /// The boss's painted arena (StreamingAssets/assets/arena-&lt;bossId&gt;.png or .jpg),
        /// cover-fitted to the 16:9 arena; null when there is none.
        /// </summary>
        Sprite ArenaArt(string bossId)
        {
            string name = "arena-" + bossId;
            var tex = ArtLibrary.Load(name);
            if (tex == null) return null;
            var r = LoomModel.CoverRect(tex.width, tex.height, T.arenaWidth / T.arenaHeight);
            return ArtLibrary.Crop(name, new UnityEngine.Rect(r.x / tex.width, r.y / tex.height, r.width / tex.width, r.height / tex.height));
        }

        public bool ShowingArenaArt => _arenaArt != null && _arenaArt.enabled;

        void SetBackdrop(BossDef boss)
        {
            var art = ArenaArt(boss.id);
            _arenaArt.enabled = art != null;
            if (art != null)
            {
                _arenaArt.sprite = art;
                Rect(_arenaArt, 0, 0, T.arenaWidth, T.arenaHeight);
            }
            foreach (var r in _forest) r.enabled = art == null;
            // The forest's drifting fog bands read as grey stripes over a painting, so they go too.
            foreach (var f in _fog) f.enabled = art == null;
            ClearTrees();
            if (art == null) SpawnTrees();
        }

        void ClearTrees()
        {
            foreach (var t in _trees)
            {
                Destroy(t.r.gameObject);
                foreach (var part in t.parts) Destroy(part.gameObject);
            }
            _trees.Clear();
        }

        void SpawnTrees()
        {
            ClearTrees();
            float AW = T.arenaWidth, AH = T.arenaHeight;
            float R() => UnityEngine.Random.value;
            var black = PixelCanvas.Col("#05060a");
            for (int i = 0; i < 22; i++)
            {
                float edge = R();
                float x = edge < 0.5f ? R() * AW : (R() < 0.5f ? R() * 120 : AW - R() * 120);
                float y = 60 + R() * (AH - 120);
                float s = 0.6f + R() * 0.9f, sway = R() * 7;
                // trunk + 4 pairs of branches as flat rects (the 3px branch thickness doesn't scale)
                var trunk = Make("Tree", _white, _alpha, 10, black);
                var parts = new Transform[8];
                for (int b = 0; b < 8; b++) parts[b] = Make("Branch", _white, _alpha, 10, black, trunk.transform.parent).transform;
                _trees.Add((trunk, x, y, s, sway, parts));
            }
        }

        void PlaceTrees(float time)
        {
            foreach (var (trunk, x, y, s, swayPhase, parts) in _trees)
            {
                float sway = Mathf.Sin(time * 0.6f + swayPhase) * 3;
                Rect(trunk, x - 3 * s + sway, y - 70 * s, 6 * s, 80 * s);
                for (int b = 0; b < 4; b++)
                {
                    PlaceRect(parts[b * 2], x - 18 * s + sway, y - 60 * s + b * 16 * s, 16 * s, 3);
                    PlaceRect(parts[b * 2 + 1], x + 2 * s + sway, y - 54 * s + b * 16 * s, 16 * s, 3);
                }
            }
        }

        void PlaceRect(Transform t, float x, float y, float w, float h)
        {
            t.localPosition = W(x + w / 2, y + h / 2);
            var b = _white.bounds.size;
            t.localScale = new Vector3(w / PxPerUnit / b.x, h / PxPerUnit / b.y, 1);
        }

        void BuildOverlay(VisualElement layer)
        {
            _root = El("battle-root hidden", "boss-battle");
            _hud = El("bb-hud");
            _hud.pickingMode = PickingMode.Ignore;
            _bossFill = Bar(_hud, T.arenaWidth / 2 - 220, 16, 440, 16, "#b03a3a", out _bossLabel);
            _playerFill = Bar(_hud, 20, T.arenaHeight - 28, 240, 14, "#3aa05a", out var you);
            you.text = "YOU";
            _root.Add(_hud);

            // Marin's battle items: three slots above the health bar, keys 1–3.
            for (int i = 0; i < 3; i++)
            {
                var slot = El("bb-item");
                slot.userData = new UnityEngine.Rect(20 + i * 40, T.arenaHeight - 66, 34, 34);
                slot.name = "bb-item-" + (i + 1);
                var icon = El("bb-item-icon");
                slot.Add(icon);
                var key = Lbl((i + 1).ToString(), "bb-item-key");
                slot.Add(key);
                var count = Lbl("0", "bb-item-count");
                slot.Add(count);
                _hud.Add(slot);
                _itemSlots.Add((slot, count));
            }

            var stageRow = El("bb-stage-row");
            stageRow.pickingMode = PickingMode.Ignore;
            _stageBanner = Lbl("", "bb-stage-banner hidden");
            stageRow.Add(_stageBanner);
            _root.Add(stageRow);

            _flee = Btn("Flee", "bb-flee", Stop, "back", 10);
            _flee.name = "bb-flee";
            _root.Add(_flee);
            var hintRow = El("bb-hint-row");
            hintRow.pickingMode = PickingMode.Ignore;
            _hint = Lbl(BaseHint, "bb-hint");
            hintRow.Add(_hint);
            _root.Add(hintRow);

            _result = El("bb-result hidden", "bb-result");
            _resultTitle = Lbl("VICTORY", "bb-result-title");
            _resultSub = Lbl("", "bb-result-sub");
            _result.Add(_resultTitle);
            _result.Add(_resultSub);
            _stars = El("bb-stars");
            _result.Add(_stars);
            _resultReward = Lbl("", "bb-result-reward");
            _resultReward.name = "bb-reward";
            _result.Add(_resultReward);
            _resultTime = Lbl("", "bb-result-time");
            _result.Add(_resultTime);
            var btns = El("bb-result-btns");
            var again = Btn("Fight again", "menu-btn", () => { if (_run != null) BeginGauntlet(_char); else Begin(_boss, _char); });
            again.name = "bb-again";
            var ret = Btn("Return to the path", "menu-btn primary", Stop);
            ret.name = "bb-return";
            btns.Add(again);
            btns.Add(ret);
            _result.Add(btns);
            _root.Add(_result);
            layer.Add(_root);
            _root.RegisterCallback<GeometryChangedEvent>(_ => LayoutArena());
        }

        const string BaseHint = "WASD / arrows to move · aim & click (or Space) to shoot";

        // A health bar in arena coordinates: dark frame, inner track, coloured fill, centred label.
        VisualElement Bar(VisualElement host, float x, float y, float w, float h, string color, out Label label)
        {
            var frame = El("bb-bar");
            frame.userData = new UnityEngine.Rect(x - 2, y - 2, w + 4, h + 4);
            var inner = El("bb-bar-inner");
            inner.style.left = 2; inner.style.top = 2; inner.style.right = 2; inner.style.bottom = 2;
            var fill = El("bb-bar-fill");
            fill.style.backgroundColor = (Color)PixelCanvas.Col(color);
            fill.style.width = Length.Percent(100);
            inner.Add(fill);
            label = Lbl("", "bb-bar-label");
            inner.Add(label);
            frame.Add(inner);
            host.Add(frame);
            return fill;
        }

        /// <summary>The arena's rect in panel units: width min(98vw, 94vh·16/9), centred.</summary>
        public UnityEngine.Rect ArenaPanelRect()
        {
            float vw = _root.resolvedStyle.width, vh = _root.resolvedStyle.height;
            if (float.IsNaN(vw) || vw <= 0) return default;
            float w = Mathf.Min(0.98f * vw, 0.94f * vh * 16f / 9f), h = w * 9f / 16f;
            return new UnityEngine.Rect((vw - w) / 2, (vh - h) / 2, w, h);
        }

        void LayoutArena()
        {
            var r = ArenaPanelRect();
            if (r.width <= 0) return;
            _hud.style.left = r.x; _hud.style.top = r.y; _hud.style.width = r.width; _hud.style.height = r.height;
            float k = r.width / T.arenaWidth;
            foreach (var child in _hud.Children())
            {
                if (!(child.userData is UnityEngine.Rect a)) continue;
                child.style.left = a.x * k; child.style.top = a.y * k;
                child.style.width = a.width * k; child.style.height = a.height * k;
                child.Q<Label>().style.fontSize = 11 * k;   // bold 11px monospace on the canvas
            }
            // Camera viewport in normalised screen space (panel and screen share the same proportions).
            float vw = _root.resolvedStyle.width, vh = _root.resolvedStyle.height;
            _cam.rect = new UnityEngine.Rect(r.x / vw, 1 - (r.y + r.height) / vh, r.width / vw, r.height / vh);
        }

        // =====================================================================
        // Start / stop
        // =====================================================================
        public void Begin(BossDef boss, CharacterDef ch)
        {
            _run = null;
            BeginFight(boss, ch);
        }

        /// <summary>Starts the Gauntlet: every boss in order, health carried between stages.</summary>
        public void BeginGauntlet(CharacterDef ch)
        {
            _run = new GauntletRun();
            BeginFight(_ctx.Db.Boss(_ctx.Db.gauntlet.order[0]), ch);
        }

        void BeginFight(BossDef boss, CharacterDef ch)
        {
            _boss = boss;
            _char = ch;
            _nextStageAt = -1;
            LastReward = null;
            Show(_stageBanner, false);
            _kit = ch != null ? _ctx.Db.ChampionKit(ch.id) : null;
            Sim = new BattleSim(T, boss, null, _kit, _ctx.Db.BossKit(boss.id));
            Sim.OnSfx += s => _ctx.Audio.Sfx(s);
            Sim.OnEnd += End;
            var defs = _ctx.Db.battleItems;
            Sim.LoadItems(defs, defs.ConvertAll(d => _ctx.Hub.SatchelCount(d.id)));
            Sim.OnItemUsed += slot =>
            {
                _ctx.Hub.SetSatchel(defs[slot].id, Sim.ItemCounts[slot]);   // used items are gone even if you flee
                _ctx.SaveHub();
            };
            if (_run != null && _run.stage > 0)
                Sim.Player.hp = Mathf.Min(Sim.Player.maxHp, _run.carryHp + _ctx.Db.gauntlet.healBetween);
            for (int i = 0; i < _itemSlots.Count; i++)
            {
                var icon = _itemSlots[i].slot.Q(className: "bb-item-icon");
                icon.Clear();
                if (i < defs.Count) icon.Add(Icon(defs[i].icon, 20));
            }
            UiKit.Show(_root, true);
            UiKit.Show(_result, false);
            _world.gameObject.SetActive(true);
            _cam.enabled = true;

            // Boss art: the main figure cropped from the sheet; a coloured circle with red eyes if missing.
            _bossSprite = ArtLibrary.Crop(boss.img, boss.portraitRect);
            _bossHasArt = _bossSprite != null;
            BuildKitHud();

            // The player walks as the pixel hero in the champion's colours, facing the way they move.
            _hero = HeroSprites.For(ch?.id);
            _playerSr.sprite = _hero.Frame(new Vector2(0, -1), false, 0);    // facing the boss

            SetBackdrop(boss);
            _ctx.Audio.PlayMusic("caves");   // tense bed
            LayoutArena();
            Render();
        }

        HeroSprites _hero;

        public void Stop()
        {
            _run = null;
            _nextStageAt = -1;
            Sim = null;
            UiKit.Show(_root, false);
            _world.gameObject.SetActive(false);
            _cam.enabled = false;
            _ctx.Audio.PlayMusic(HubController.MusicMenu);
            _onExit?.Invoke();   // back to the path map
        }

        void End(bool win)
        {
            var hub = _ctx.Hub;
            float hpRatio = Mathf.Max(0, Sim.Player.hp) / Sim.Player.maxHp;
            Progression.Reward? reward = null;
            string title = win ? "VICTORY" : "DEFEATED";
            string sub;

            if (_run != null)
            {
                var order = _ctx.Db.gauntlet.order;
                _run.totalTime += Sim.EndTime;
                if (win) _onBossDefeated?.Invoke(_boss.id);
                if (win && _run.stage < order.Count - 1)
                {
                    // On to the next boss after a short breath.
                    _run.carryHp = Sim.Player.hp;
                    _run.stage++;
                    var next = _ctx.Db.Boss(order[_run.stage]);
                    _stageBanner.text = $"STAGE {_run.stage + 1} / {order.Count}  —  {next.name.ToUpperInvariant()}";
                    Show(_stageBanner, true);
                    _nextStageAt = Time.unscaledTime + 1.6f;
                    return;
                }
                if (win)
                {
                    var g = _ctx.Db.gauntlet;
                    reward = Progression.RecordWin(hub, Progression.GauntletId, g.first, g.repeat, g.par, hpRatio, _run.totalTime);
                    title = "GAUNTLET CONQUERED";
                    sub = "All four have fallen, one after another.";
                }
                else sub = $"You fell at stage {_run.stage + 1} of {order.Count}, to {_boss.name}.";
            }
            else if (win)
            {
                var b = _ctx.Db.Bounty(_boss.id);
                if (b != null) reward = Progression.RecordWin(hub, _boss.id, b.first, b.repeat, b.par, hpRatio, Sim.EndTime);
                sub = $"{_boss.name} has fallen.";
                _onBossDefeated?.Invoke(_boss.id);
            }
            else sub = $"{_boss.name} bested you. Steel yourself and try again.";

            if (reward.HasValue) _ctx.SaveHub();
            LastReward = reward;
            _resultTitle.text = title;
            _resultTitle.EnableInClassList("win", win);
            _resultTitle.EnableInClassList("lose", !win);
            _resultSub.text = sub;
            _stars.Clear();
            if (reward.HasValue)
            {
                var r = reward.Value;
                for (int i = 0; i < 3; i++) _stars.Add(Icon(i < r.stars ? "star" : "star-empty", 30, "bb-star"));
                _resultReward.text = $"+{r.gold} gold" + (r.firstWin ? "  ·  first victory bounty" : "") + (r.newBestStars && !r.firstWin ? "  ·  new best rating" : "");
                float par = _run != null ? _ctx.Db.gauntlet.par : _ctx.Db.Bounty(_boss.id)?.par ?? 0;
                _resultTime.text = $"Time {Progression.Clock(r.time)}  ·  best {Progression.Clock(r.bestTime)}  ·  par {Progression.Clock(par)}" + (r.newBestTime && !r.firstWin ? "  ·  new record!" : "");
            }
            else { _resultReward.text = ""; _resultTime.text = ""; }
            Show(_resultReward, reward.HasValue);
            Show(_resultTime, reward.HasValue);
            UiKit.Show(_result, true);
        }

        public bool ResultShowing => _result != null && !_result.ClassListContains("hidden");
        public string ResultTitle => _resultTitle?.text;

        // =====================================================================
        // Loop
        // =====================================================================
        void Update()
        {
            if (!Active) return;
            if (_nextStageAt >= 0 && Time.unscaledTime >= _nextStageAt)
            {
                var order = _ctx.Db.gauntlet.order;
                BeginFight(_ctx.Db.Boss(order[_run.stage]), _char);
                return;
            }
            Sim.Step(Time.deltaTime, ScriptedInput ?? ReadInput());
            Render();
        }

        /// <summary>Skips the pause between Gauntlet stages (tests and the autopilot).</summary>
        public void SkipStageTransition() { if (_nextStageAt >= 0) _nextStageAt = 0; }

        BattleSim.Inputs ReadInput()
        {
            var inp = new BattleSim.Inputs();
            var kb = Keyboard.current;
            var mouse = Mouse.current;
            if (kb != null)
            {
                if (kb.wKey.isPressed || kb.upArrowKey.isPressed) inp.move.y -= 1;
                if (kb.sKey.isPressed || kb.downArrowKey.isPressed) inp.move.y += 1;
                if (kb.aKey.isPressed || kb.leftArrowKey.isPressed) inp.move.x -= 1;
                if (kb.dKey.isPressed || kb.rightArrowKey.isPressed) inp.move.x += 1;
                inp.shoot |= kb.spaceKey.isPressed;
                inp.special |= kb.qKey.wasPressedThisFrame;
                if (kb.digit1Key.wasPressedThisFrame) inp.itemSlot = 1;
                else if (kb.digit2Key.wasPressedThisFrame) inp.itemSlot = 2;
                else if (kb.digit3Key.wasPressedThisFrame) inp.itemSlot = 3;
            }
            if (mouse != null)
            {
                var sp = mouse.position.ReadValue();
                var wp = _cam.ScreenToWorldPoint(new Vector3(sp.x, sp.y, 10));
                inp.aim = new Vector2(wp.x * PxPerUnit + T.arenaWidth / 2, T.arenaHeight / 2 - wp.y * PxPerUnit);
                // Clicking the Flee button must not also fire a shot.
                bool overButton = PointerOverButton(sp);
                inp.shoot |= mouse.leftButton.isPressed && !overButton;
                inp.special |= mouse.rightButton.wasPressedThisFrame && !overButton;
            }
            return inp;
        }

        bool PointerOverButton(Vector2 screenPos)
        {
            var panel = _root.panel;
            if (panel == null) return false;
            var p = RuntimePanelUtils.ScreenToPanel(panel, new Vector2(screenPos.x, Screen.height - screenPos.y));
            for (var e = panel.Pick(p); e != null; e = e.parent)
                if (e is ClickEl) return true;
            return false;
        }

        // Champion ability HUD: the special's cooldown (or meter) plus any extra meters, under the arena's bottom-right.
        void BuildKitHud()
        {
            foreach (var b in _kitBars) b.RemoveFromHierarchy();
            _kitBars.Clear();
            _kitMeters.Clear();
            var sim = Sim;
            if (_kit == null || _kit.special == "none") { _hint.text = BaseHint; return; }
            _hint.text = $"{BaseHint} · right-click (or Q): {_kit.ability}";
            float x = T.arenaWidth - 260, y = T.arenaHeight - 28;
            void Meter(string color, Func<float> value, Func<string> text)
            {
                var fill = Bar(_hud, x, y, 240, 14, color, out var label);
                _kitBars.Add(fill.parent.parent);
                _kitMeters.Add((fill, label, value, text));
                y -= 22;
            }
            switch (_kit.special)
            {
                case "coinring":
                    Meter("#e0b030", () => sim.CoinPower / _kit.P("powerMax", 100),
                        () => sim.CoinPower >= _kit.P("powerMax", 100) ? $"{_kit.ability.ToUpperInvariant()} READY" : "COIN POWER");
                    break;
                case "teleport":
                    Meter("#e07ad0", () => sim.Dust / _kit.P("dustMax", 50), () => $"DUST {Mathf.FloorToInt(sim.Dust)} · {_kit.ability.ToUpperInvariant()}");
                    break;
                default:
                    Meter("#7a5ad0", () => _kit.cooldown > 0 ? 1 - Mathf.Clamp01(sim.SpecialCooldown / _kit.cooldown) : 1,
                        () => sim.SpecialCooldown > 0 ? _kit.ability.ToUpperInvariant() : $"{_kit.ability.ToUpperInvariant()} READY");
                    break;
            }
            if (_kit.special == "drink")
            {
                Meter("#d8a838", () => sim.Drunk / 100f, () => "DRUNK LEVEL");
                Meter("#9ab0a0", () => sim.Nicotine / Mathf.Max(1, _kit.P("nicotineStart", 100)), () => "NICOTINE STATUS");
            }
            LayoutArena();
        }

        SpriteRenderer D(Sprite sprite, Material mat, int order, float x, float y, float w, float h, float rotDeg = 0, Color? color = null, float brightness = 1)
        {
            if (_drawn == _draws.Count) _draws.Add(Make("Draw", sprite, mat, order, Color.white));
            var r = _draws[_drawn++];
            r.enabled = true;
            r.sprite = sprite;
            r.sharedMaterial = mat;
            r.sortingOrder = order;
            r.color = color ?? Color.white;
            r.transform.localPosition = W(x, y);
            r.transform.localRotation = Quaternion.Euler(0, 0, rotDeg);
            Size(r, w, h);
            _mpb.SetFloat("_Brightness", brightness);
            r.SetPropertyBlock(_mpb);
            return r;
        }

        static float Deg(float vx, float vy) => -Mathf.Atan2(vy, vx) * Mathf.Rad2Deg;   // canvas y points down

        void Render()
        {
            if (Sim == null) return;
            float t = Sim.Time;
            PlaceTrees(t);
            _drawn = 0;

            // fog drift: each row slides right and wraps (off = (t·(10+6i)) % (AW+120) - 60)
            int idx = 0;
            for (int i = 0; i < 3; i++)
            {
                float y = T.arenaHeight * 0.55f + i * 30;
                float off = (t * (10 + i * 6)) % (T.arenaWidth + 120) - 60;
                for (int x = -60; x < T.arenaWidth; x += 90)
                    _fog[idx++].transform.localPosition = W(x + off, y);
            }

            var p = Sim.Player;
            var gold = PixelCanvas.Col("#ffd24a");

            // Ground effects: honey puddles, dropped coins, the bus lane warning.
            foreach (var pd in Sim.Puddles)
                D(_ellipse, _alpha, 34, pd.x, pd.y, pd.r * 2.2f, pd.r * 1.2f, 0, new Color(0.93f, 0.66f, 0.12f, Mathf.Min(0.5f, pd.life * 0.5f)));
            foreach (var c in Sim.Coins)
            {
                float bob = Mathf.Sin(t * 6 + c.x) * 2;
                D(_glow, _additive, 35, c.x, c.y + bob, 22, 22, 0, new Color(1, 0.8f, 0.2f, 0.6f));
                D(_circle, _alpha, 36, c.x, c.y + bob, 9, 9, 0, gold);
            }
            var bus = Sim.ActiveBus;
            if (bus != null && !bus.driving)
                D(_white, _alpha, 37, T.arenaWidth / 2, bus.y, T.arenaWidth, bus.h, 0, new Color(1, 0.2f, 0.15f, 0.12f + 0.12f * Mathf.Abs(Mathf.Sin(t * 14))));

            // Boss bodies (+ shadows drawn over their feet, as on the canvas)
            var bossColor = PixelCanvas.Col(_boss.color ?? "#9a5fd0");
            foreach (var u in Sim.Units)
            {
                float size = _bossHasArt ? u.r * T.bossSpriteScale : u.r * 2;
                D(_bossHasArt ? _bossSprite : _circle, _alpha, 40, u.x, u.y, size, size, 0, _bossHasArt ? Color.white : (Color)bossColor, u.hurt > 0 ? 2f : 1f);
                if (!_bossHasArt)
                {
                    float k = u.r / T.bossRadius;
                    D(_white, _alpha, 41, u.x - 6 * k, u.y - 4 * k, 4 * k, 4 * k, 0, PixelCanvas.Col("#ff5a4a"));
                    D(_white, _alpha, 41, u.x + 6 * k, u.y - 4 * k, 4 * k, 4 * k, 0, PixelCanvas.Col("#ff5a4a"));
                }
                D(_ellipse, _alpha, 42, u.x, u.y + u.r, u.r * 1.6f, u.r * 0.6f, 0, new Color(0, 0, 0, 0.35f));
            }

            // Player (blinks while invulnerable; rolls, jumps, flickers while dashing/blinking)
            float alpha = p.hurt > 0 && Mathf.FloorToInt(p.hurt * 12) % 2 == 1 ? 0.4f : 1f;
            if (Sim.DashT > 0 || Sim.TeleportT > 0) alpha = 0.55f;
            _playerSr.color = new Color(1, 1, 1, alpha);
            float lift = 0, grow = 1;
            if (Sim.JumpT > 0 && _kit != null)
            {
                float k = Mathf.Sin(Mathf.PI * (1 - Sim.JumpT / Mathf.Max(0.01f, _kit.P("duration", 0.6f))));
                lift = 22 * k; grow = 1 + 0.3f * k;
            }
            _playerSr.sprite = _hero.Frame(Sim.Moving ? Sim.MoveDir : Sim.FacingAim, Sim.Moving, Sim.Time);
            _playerSr.transform.localPosition = W(p.x, p.y - lift);
            _playerSr.transform.localRotation = Quaternion.Euler(0, 0, Sim.RollT > 0 ? Sim.RollT * 900 : 0);
            Size(_playerSr, HeroSprites.W * T.heroSpriteScale * grow, HeroSprites.H * T.heroSpriteScale * grow);
            _playerShadow.transform.localPosition = W(p.x, p.y + 16);
            Size(_playerShadow, 28 / grow, 10 / grow);

            // Swiftwing Elixir glow and Warding Charm orbs
            if (Sim.SwiftT > 0) D(_glow, _additive, 49, p.x, p.y + 8, 70, 30, 0, new Color(0.5f, 1f, 0.6f, 0.45f));
            for (int i = 0; i < Sim.WardHits; i++)
            {
                float a = t * 3 + i * Mathf.PI * 2 / Mathf.Max(1, Sim.WardHits);
                D(_glow, _additive, 56, p.x + Mathf.Cos(a) * 26, p.y + Mathf.Sin(a) * 26, 16, 16, 0, new Color(0.55f, 0.8f, 1f, 0.9f));
            }

            // Atanas' calm aura and Nikolay's mug-shield
            if (Sim.AuraT > 0)
            {
                float k = 1 - Mathf.Repeat(t * 1.2f, 1);
                D(_ring, _additive, 55, p.x, p.y, 60 + 50 * (1 - k), 60 + 50 * (1 - k), 0, new Color(0.3f, 0.95f, 0.85f, 0.6f * k));
            }
            if (Sim.ShieldT > 0)
            {
                float aim = Mathf.Atan2(Sim.FacingAim.y, Sim.FacingAim.x), arc = _kit?.P("arc", 2.1f) ?? 2.1f;
                for (int i = -3; i <= 3; i++)
                {
                    float a = aim + i * arc / 6;
                    D(_white, _alpha, 55, p.x + Mathf.Cos(a) * 30, p.y + Mathf.Sin(a) * 30, 5, 13, Deg(Mathf.Cos(a), Mathf.Sin(a)), PixelCanvas.Col("#c8843a"));
                }
            }

            // Projectiles
            var playerGlow = PixelCanvas.Col("#9fe0ff");
            var bossGlow = PixelCanvas.Col(_boss.color ?? "#ff6a3a");
            foreach (var s in Sim.PlayerShots) DrawShot(s, playerGlow, t);
            foreach (var s in Sim.BossShots) DrawShot(s, bossGlow, t);

            // Bus №9 on top of everything on the ground
            if (bus != null && bus.driving) D(_bus, _alpha, 70, bus.x, bus.y, bus.w, bus.h);

            for (int i = _drawn; i < _draws.Count; i++) _draws[i].enabled = false;

            // HUD bars: one health bar covers every boss body
            _bossFill.style.width = Length.Percent(Sim.BossHpRatio * 100);
            _playerFill.style.width = Length.Percent(Mathf.Max(0, p.hp / p.maxHp) * 100);
            _bossLabel.text = _boss.name.ToUpperInvariant() + (Sim.Units.Count > 1 ? $"  ×{Sim.Units.Count}" : "");
            for (int i = 0; i < _itemSlots.Count; i++)
            {
                int n = i < Sim.ItemCounts.Count ? Sim.ItemCounts[i] : 0;
                _itemSlots[i].count.text = n.ToString();
                _itemSlots[i].slot.EnableInClassList("empty", n == 0);
            }
            foreach (var (fill, label, value, text) in _kitMeters)
            {
                fill.style.width = Length.Percent(Mathf.Clamp01(value()) * 100);
                label.text = text();
            }
        }

        void DrawShot(BattleSim.Shot s, Color glow, float t)
        {
            switch (s.kind)
            {
                case BattleSim.ShotKind.Feather:
                    D(_feather, _alpha, 60, s.x, s.y, 16, 5, Deg(s.vx, s.vy));
                    break;
                case BattleSim.ShotKind.Ticket:
                    D(_ticket, _alpha, 60, s.x, s.y, 13, 8, t * s.spin * Mathf.Rad2Deg);
                    break;
                case BattleSim.ShotKind.Seed:
                    D(_seed, _alpha, 60, s.x, s.y, 7, 7, Deg(s.vx, s.vy));
                    break;
                case BattleSim.ShotKind.Coin:
                    D(_glow, _additive, 59, s.x, s.y, s.r * 4, s.r * 4, 0, new Color(1, 0.8f, 0.25f, 0.8f));
                    D(_circle, _alpha, 60, s.x, s.y, s.r * 1.8f, s.r * 1.8f, 0, PixelCanvas.Col("#ffd24a"));
                    break;
                case BattleSim.ShotKind.Koi:
                    D(_koi, _alpha, 60, s.x, s.y, 18, 7, Deg(s.vx, s.vy));
                    break;
                case BattleSim.ShotKind.Sparkle:
                    D(_glow, _additive, 60, s.x, s.y, s.r * 4.4f, s.r * 4.4f, 0, PixelCanvas.Col("#ff9ae8"));
                    D(_white, _additive, 61, s.x, s.y, 2, 2, t * 400, Color.white);
                    break;
                default:
                    // additive glow (radius r·2.2) + a 2×2 white core
                    D(_glow, _additive, 60, s.x, s.y, s.r * 4.4f, s.r * 4.4f, 0, glow);
                    D(_white, _additive, 61, s.x, s.y, 2, 2, 0, Color.white);
                    break;
            }
        }
    }
}
