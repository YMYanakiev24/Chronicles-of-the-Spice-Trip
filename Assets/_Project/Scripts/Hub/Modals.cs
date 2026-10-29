/*
 * Modals.cs
 * ----------------------------------------------------------------------------
 * The two pop-ups of the Citadel hub (Scenes.js):
 *   LoomModal            Marin's Enchanted Loom — load an image, weave it into
 *                        a perforated tapestry, tear the shards off
 *   CharacterSelectModal the Changing Room — choose the champion you fight as
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using System.IO;
using SpiceTrip.Art;
using SpiceTrip.Core;
using SpiceTrip.Data;
using UnityEngine;
using UnityEngine.UIElements;
using static SpiceTrip.Hub.UiKit;

namespace SpiceTrip.Hub
{
    public class LoomModal
    {
        public readonly VisualElement Root;
        public readonly LoomModel Model;
        readonly GameContext _ctx;
        readonly LoomView _view;
        readonly Label _empty, _torn;
        readonly DropdownField _grid;
        readonly List<int> _options;
        Texture2D _image, _tapestry;

        public bool IsOpen => !Root.ClassListContains("hidden");
        public Texture2D Tapestry => _tapestry;

        public LoomModal(GameContext ctx, Func<string> pickImage)
        {
            _ctx = ctx;
            Model = new LoomModel(ctx.Db.loom);
            Root = El("modal hidden", "tapestry-modal");
            var box = El("modal-box tapestry-box");
            Root.Add(box);
            box.Add(Btn(null, "tap-close", Close, "close", 9));
            box.Add(Lbl("Marin's Enchanted Loom", "modal-title"));
            box.Add(Lbl("Weave any image into a tapestry of rune-shards — then tear them free, one by one.", "tap-sub"));

            var controls = El("tap-controls");
            controls.Add(Btn("Upload image", "tap-upload", () =>
            {
                var path = pickImage?.Invoke();
                if (!string.IsNullOrEmpty(path)) LoadImageFile(path);
            }, "camera", 14));

            var sel = El("tap-sel");
            sel.Add(Lbl("Shards", "tap-sel-label"));
            _options = new List<int>(ctx.Db.loom.gridOptions);
            var labels = _options.ConvertAll(n => $"{n}×{n}");
            _grid = new DropdownField(labels, _options.IndexOf(ctx.Db.loom.defaultGrid));
            _grid.AddToClassList("tap-dropdown");
            _grid.RegisterValueChangedCallback(_ => Build());
            sel.Add(_grid);
            controls.Add(sel);

            controls.Add(Btn("Tear All", "tap-btn", TearAll));
            controls.Add(Btn("Re-weave", "tap-btn", Build));
            var count = El("tap-count");
            count.Add(Lbl("Shards torn: ", "tap-sel-label"));
            _torn = Lbl("0", "tap-count-value");
            count.Add(_torn);
            controls.Add(count);
            box.Add(controls);

            var stage = El("tap-stage");
            _view = new LoomView { Model = Model };
            SetBackground(_view, CheckerTexture());
            _view.OnCanvasClick = OnCanvasClick;
            stage.Add(_view);
            _empty = Lbl("Upload an image to weave your tapestry", "tap-empty");
            _empty.pickingMode = PickingMode.Ignore;
            stage.Add(_empty);
            box.Add(stage);
            box.Add(Lbl("Click a shard to tear it free.", "tap-foot"));

            Root.RegisterCallback<GeometryChangedEvent>(_ =>
            {
                // .tapestry-box: width min(640px, 94vw); canvas min(512px, 84vw). The web box
                // scrolled when taller than 92vh; here the canvas shrinks to fit instead.
                float vw = Root.resolvedStyle.width, vh = Root.resolvedStyle.height;
                box.style.width = Mathf.Min(640, 0.94f * vw);
                float side = Mathf.Max(160, Mathf.Min(Mathf.Min(512, 0.84f * vw), vh - 300));
                _view.style.width = side; _view.style.height = side;
            });
        }

        public int SelectedGrid => _options[Mathf.Clamp(_grid.index, 0, _options.Count - 1)];

        public void SetGrid(int cols)
        {
            int i = _options.IndexOf(cols);
            if (i >= 0) _grid.index = i;
        }

        public void Open()
        {
            UiKit.Show(Root, true);
            _ctx.Audio.Sfx("ui");
        }

        public void Close()
        {
            UiKit.Show(Root, false);
            _ctx.Audio.Sfx("ui");
        }

        /// <summary>Loads an image file from disk and weaves it.</summary>
        public bool LoadImageFile(string path)
        {
            try
            {
                var tex = new Texture2D(2, 2, TextureFormat.RGBA32, false);
                if (!tex.LoadImage(File.ReadAllBytes(path))) { ArtLibrary.SafeDestroy(tex); return false; }
                SetImage(tex);
                return true;
            }
            catch (Exception e)
            {
                Debug.LogWarning($"Loom could not read {path}: {e.Message}");
                return false;
            }
        }

        public void SetImage(Texture2D readable)
        {
            if (_image != null && _image != readable) ArtLibrary.SafeDestroy(_image);
            _image = readable;
            Build();
        }

        // Pixelate the source image, lay out the perforated grid.
        void Build()
        {
            if (_image == null) return;
            _empty.style.display = DisplayStyle.None;
            int cols = SelectedGrid;
            Model.Build(cols);
            UpdateTorn();

            // Pixelate: cover-fit into a small square with nearest sampling; shown upscaled crisp.
            int small = Model.PixelSizeFor(cols);
            var src = LoomModel.CoverRect(_image.width, _image.height);
            var px = new Color32[small * small];
            var srcPx = _image.GetPixels32();
            for (int y = 0; y < small; y++)
                for (int x = 0; x < small; x++)
                {
                    // canvas row y (top-down) samples texture row from the top of the cover rect
                    int sx = Mathf.Clamp((int)(src.x + (x + 0.5f) * src.width / small), 0, _image.width - 1);
                    int syTop = Mathf.Clamp((int)(src.y + (y + 0.5f) * src.height / small), 0, _image.height - 1);
                    int sy = _image.height - 1 - syTop;
                    px[(small - 1 - y) * small + x] = srcPx[sy * _image.width + sx];
                }
            if (_tapestry == null || _tapestry.width != small)
            {
                if (_tapestry != null) ArtLibrary.SafeDestroy(_tapestry);
                _tapestry = new Texture2D(small, small, TextureFormat.RGBA32, false)
                {
                    filterMode = FilterMode.Point,
                    wrapMode = TextureWrapMode.Clamp,
                    name = "loom-tapestry",
                };
            }
            _tapestry.SetPixels32(px);
            _tapestry.Apply(false);
            _view.Source = _tapestry;
            _view.MarkDirtyRepaint();
        }

        void OnCanvasClick(Vector2 p)
        {
            var cell = Model.HitTest(p.x, p.y);
            if (cell == null) return;
            Model.Tear(cell);
            UpdateTorn();
            _ctx.Audio.Sfx("hit");
        }

        public void TearAll()
        {
            if (Model.TearAll() > 0) _ctx.Audio.Sfx("lightning");
            UpdateTorn();
        }

        public void TearAt(Vector2 canvasPoint) => OnCanvasClick(canvasPoint);

        void UpdateTorn() => _torn.text = Model.TornCount.ToString();

        public void Tick(float dt)
        {
            if (!IsOpen || Model.Cells.Count == 0) return;
            Model.Step(dt);
            _view.MarkDirtyRepaint();
        }
    }

    public class CharacterSelectModal
    {
        public readonly VisualElement Root;
        readonly GameContext _ctx;
        readonly VisualElement _grid, _box;
        readonly Action<CharacterDef> _onSelect;
        string _selected;

        public bool IsOpen => !Root.ClassListContains("hidden");

        public CharacterSelectModal(GameContext ctx, Action<CharacterDef> onSelect)
        {
            _ctx = ctx;
            _onSelect = onSelect;
            Root = El("modal hidden", "char-modal");
            _box = El("modal-box char-box");
            Root.Add(_box);
            _box.Add(Btn(null, "tap-close", () => { ctx.Audio.Sfx("ui"); Close(); }, "close", 9));
            _box.Add(Lbl("Choose Your Champion", "modal-title"));
            _box.Add(Lbl("Pick who you fight as. Drop more art into /assets and add them later.", "tap-sub"));
            var scroll = new ScrollView(ScrollViewMode.Vertical);
            _grid = El("char-grid", "char-grid");
            scroll.Add(_grid);
            _box.Add(scroll);

            Root.RegisterCallback<GeometryChangedEvent>(_ =>
            {
                // .char-box: width min(880px, 94vw); max-height 90vh
                _box.style.width = Mathf.Min(880, 0.94f * Root.resolvedStyle.width);
                _box.style.maxHeight = 0.9f * Root.resolvedStyle.height;
                Build(_selected);
            });
        }

        public void Open(string selectedId)
        {
            Build(selectedId);
            UiKit.Show(Root, true);
        }

        public void Close() => UiKit.Show(Root, false);

        // grid-template-columns: repeat(auto-fill, minmax(130px, 1fr)); gap 12px
        void Build(string selectedId)
        {
            _selected = selectedId;
            _grid.Clear();
            float inner = _box.resolvedStyle.width - 48 - 6 - 20;   // box padding + borders + scrollbar
            if (float.IsNaN(inner) || inner <= 0) inner = 800;
            int cols = Mathf.Max(1, Mathf.FloorToInt((inner + 12) / (130 + 12)));
            float cardW = (inner - (cols - 1) * 12) / cols - 1;
            int i = 0;
            foreach (var ch in _ctx.Db.characters)
            {
                var c = ch;
                var card = new ClickEl(() => { if (!c.empty) _onSelect(c); }) { name = "char-" + ch.id };
                card.AddToClassList("char-card");
                if (ch.empty) card.AddToClassList("empty");
                if (ch.id == selectedId) card.AddToClassList("selected");
                card.style.width = cardW;
                card.style.flexShrink = 0;
                card.style.marginRight = (++i % cols == 0) ? 0 : 12;
                var thumb = El("char-thumb");
                float side = cardW - 20;
                thumb.style.width = side; thumb.style.height = side;
                if (ch.empty) thumb.Add(Lbl("?", "char-soon"));
                else if (!string.IsNullOrEmpty(ch.img))
                {
                    var tex = ArtLibrary.Load(ch.img);
                    if (tex == null) thumb.Add(Lbl("?", "char-soon"));
                    else if (ch.portraitCard) SetBackground(thumb, ArtLibrary.Crop(ch.img, ch.portraitRect));
                    else SetBackground(thumb, tex);
                }
                else { thumb.AddToClassList("char-thumb-figure"); SetBackground(thumb, HeroSprites.For(ch.id).Frame("down", 0)); }
                thumb.pickingMode = PickingMode.Ignore;
                card.Add(thumb);
                // ...and the figure they walk as, from every side
                if (!ch.empty) card.Add(Turnaround(ch.id, (side - 8) / 4 * HeroSprites.H / HeroSprites.W, "char-turn"));
                var name = Lbl(ch.name, "char-name");
                name.pickingMode = PickingMode.Ignore;
                card.Add(name);
                _grid.Add(card);
            }
        }
    }
}
