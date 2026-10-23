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
        SpriteRenderer _bossSr, _bossShadow, _playerSr, _playerShadow;
        SpriteRenderer _bossEyeL, _bossEyeR;
        readonly List<(SpriteRenderer r, float x, float y, float s, float sway, Transform[] parts)> _trees = new List<(SpriteRenderer, float, float, float, float, Transform[])>();
        readonly List<SpriteRenderer> _fog = new List<SpriteRenderer>();
        readonly List<SpriteRenderer> _shotPool = new List<SpriteRenderer>();
        readonly List<SpriteRenderer> _corePool = new List<SpriteRenderer>();
        bool _bossHasArt;

        // UI overlay
        VisualElement _root, _hud, _result, _bossFill, _playerFill;
        Label _bossLabel, _resultTitle, _resultSub;
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

            // moon halo + moon
            float mx = AW * 0.8f, my = AH * 0.18f;
            var halo = Make("MoonHalo", _glow, _alpha, 1, new Color(180 / 255f, 170 / 255f, 210 / 255f, 0.35f));
            Rect(halo, mx - 90, my - 90, 180, 180);
            var moon = Make("Moon", _circle, _alpha, 2, PixelCanvas.Col("#c9c2dd"));
            Rect(moon, mx - 26, my - 26, 52, 52);

            // ground
            var ground = Make("Ground", _white, _alpha, 3, PixelCanvas.Col("#0e1a12"));
            Rect(ground, 0, AH * 0.82f, AW, AH * 0.18f);

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
            _bossSr = Make("Boss", _circle, _alpha, 40, Color.white);
            _bossEyeL = Make("BossEyeL", _white, _alpha, 41, PixelCanvas.Col("#ff5a4a"));
            _bossEyeR = Make("BossEyeR", _white, _alpha, 41, PixelCanvas.Col("#ff5a4a"));
            _bossShadow = Make("BossShadow", _ellipse, _alpha, 42, new Color(0, 0, 0, 0.35f));
            _playerSr = Make("Player", _white, _alpha, 50, Color.white);
            _playerShadow = Make("PlayerShadow", _ellipse, _alpha, 51, new Color(0, 0, 0, 0.35f));
        }

        void SpawnTrees()
        {
            foreach (var t in _trees)
            {
                Destroy(t.r.gameObject);
                foreach (var part in t.parts) Destroy(part.gameObject);
            }
            _trees.Clear();
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

            _flee = Btn("Flee", "bb-flee", Stop, "back", 10);
            _flee.name = "bb-flee";
            _root.Add(_flee);
            var hintRow = El("bb-hint-row");
            hintRow.pickingMode = PickingMode.Ignore;
            hintRow.Add(Lbl("WASD / arrows to move · aim & click (or Space) to shoot", "bb-hint"));
            _root.Add(hintRow);

            _result = El("bb-result hidden", "bb-result");
            _resultTitle = Lbl("VICTORY", "bb-result-title");
            _resultSub = Lbl("", "bb-result-sub");
            _result.Add(_resultTitle);
            _result.Add(_resultSub);
            var btns = El("bb-result-btns");
            var again = Btn("Fight again", "menu-btn", () => Begin(_boss, _char));
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
            _boss = boss;
            _char = ch;
            Sim = new BattleSim(T, boss);
            Sim.OnSfx += s => _ctx.Audio.Sfx(s);
            Sim.OnEnd += End;
            UiKit.Show(_root, true);
            UiKit.Show(_result, false);
            _world.gameObject.SetActive(true);
            _cam.enabled = true;

            // Boss art: the main figure cropped from the sheet; a coloured circle with red eyes if missing.
            var bossSprite = ArtLibrary.Crop(boss.img, boss.portraitRect);
            _bossHasArt = bossSprite != null;
            _bossSr.sprite = _bossHasArt ? bossSprite : _circle;
            _bossSr.color = _bossHasArt ? Color.white : PixelCanvas.Col(boss.color ?? "#9a5fd0");
            _bossEyeL.enabled = _bossEyeR.enabled = !_bossHasArt;

            // Player art: the champion's crop, or the procedural hero facing up.
            Sprite playerSprite = null;
            if (ch != null && !string.IsNullOrEmpty(ch.img)) playerSprite = ArtLibrary.Crop(ch.img, ch.portraitRect);
            if (playerSprite == null)
            {
                var heroTex = PixelArt.Hero()["up"][0].ToTexture();
                playerSprite = Sprite.Create(heroTex, new UnityEngine.Rect(0, 0, heroTex.width, heroTex.height), new Vector2(0.5f, 0.5f), PxPerUnit);
                _playerIsHero = true;
            }
            else _playerIsHero = false;
            _playerSr.sprite = playerSprite;

            SpawnTrees();
            _ctx.Audio.PlayMusic("caves");   // tense bed
            LayoutArena();
            Render();
        }

        bool _playerIsHero;

        public void Stop()
        {
            Sim = null;
            UiKit.Show(_root, false);
            _world.gameObject.SetActive(false);
            _cam.enabled = false;
            _ctx.Audio.PlayMusic(HubController.MusicMenu);
            _onExit?.Invoke();   // back to the path map
        }

        void End(bool win)
        {
            _resultTitle.text = win ? "VICTORY" : "DEFEATED";
            _resultTitle.EnableInClassList("win", win);
            _resultTitle.EnableInClassList("lose", !win);
            _resultSub.text = win
                ? $"{_boss.name} has fallen."
                : $"{_boss.name} bested you. Steel yourself and try again.";
            UiKit.Show(_result, true);
            if (win) _onBossDefeated?.Invoke(_boss.id);
        }

        public bool ResultShowing => _result != null && !_result.ClassListContains("hidden");
        public string ResultTitle => _resultTitle?.text;

        // =====================================================================
        // Loop
        // =====================================================================
        void Update()
        {
            if (!Active) return;
            Sim.Step(Time.deltaTime, ScriptedInput ?? ReadInput());
            Render();
        }

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
            }
            if (mouse != null)
            {
                var sp = mouse.position.ReadValue();
                var wp = _cam.ScreenToWorldPoint(new Vector3(sp.x, sp.y, 10));
                inp.aim = new Vector2(wp.x * PxPerUnit + T.arenaWidth / 2, T.arenaHeight / 2 - wp.y * PxPerUnit);
                // Clicking the Flee button must not also fire a shot.
                inp.shoot |= mouse.leftButton.isPressed && !PointerOverButton(sp);
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

        void Render()
        {
            if (Sim == null) return;
            float t = Sim.Time;
            PlaceTrees(t);

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
            var b = Sim.Boss;

            // Boss (+ shadow drawn over its feet, as on the canvas)
            float bs = _bossHasArt ? b.r * T.bossSpriteScale : b.r * 2;
            _bossSr.transform.localPosition = W(b.x, b.y);
            Size(_bossSr, bs, bs);
            _mpb.SetFloat("_Brightness", b.hurt > 0 ? 2f : 1f);
            _bossSr.SetPropertyBlock(_mpb);
            if (!_bossHasArt)
            {
                Rect(_bossEyeL, b.x - 8, b.y - 6, 4, 4);
                Rect(_bossEyeR, b.x + 4, b.y - 6, 4, 4);
            }
            _bossShadow.transform.localPosition = W(b.x, b.y + b.r);
            Size(_bossShadow, b.r * 1.6f, b.r * 0.6f);

            // Player (blinks while invulnerable)
            float alpha = p.hurt > 0 && Mathf.FloorToInt(p.hurt * 12) % 2 == 1 ? 0.4f : 1f;
            _playerSr.color = new Color(1, 1, 1, alpha);
            _playerSr.transform.localPosition = W(p.x, p.y);
            if (_playerIsHero) Size(_playerSr, PixelArt.HeroW * T.heroSpriteScale, PixelArt.HeroH * T.heroSpriteScale);
            else Size(_playerSr, T.playerSpriteSize, T.playerSpriteSize);
            _playerShadow.transform.localPosition = W(p.x, p.y + 16);
            Size(_playerShadow, 28, 10);

            // Projectiles: additive glow (radius r·2.2) + a 2×2 white core
            int n = 0;
            var playerGlow = PixelCanvas.Col("#9fe0ff");
            var bossGlow = PixelCanvas.Col(_boss.color ?? "#ff6a3a");
            foreach (var s in Sim.PlayerShots) DrawShot(n++, s, playerGlow);
            foreach (var s in Sim.BossShots) DrawShot(n++, s, bossGlow);
            for (int i = n; i < _shotPool.Count; i++) { _shotPool[i].enabled = false; _corePool[i].enabled = false; }

            // HUD bars
            _bossFill.style.width = Length.Percent(Mathf.Max(0, b.hp / b.maxHp) * 100);
            _playerFill.style.width = Length.Percent(Mathf.Max(0, p.hp / p.maxHp) * 100);
            _bossLabel.text = _boss.name.ToUpperInvariant();
        }

        void DrawShot(int i, BattleSim.Shot s, Color col)
        {
            while (_shotPool.Count <= i)
            {
                _shotPool.Add(Make("Shot", _glow, _additive, 60, Color.white));
                _corePool.Add(Make("ShotCore", _white, _additive, 61, Color.white));
            }
            var g = _shotPool[i];
            g.enabled = true;
            g.color = col;
            g.transform.localPosition = W(s.x, s.y);
            Size(g, s.r * 4.4f, s.r * 4.4f);
            var c = _corePool[i];
            c.enabled = true;
            Rect(c, s.x - 1, s.y - 1, 2, 2);
        }
    }
}
