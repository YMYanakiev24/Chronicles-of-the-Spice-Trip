/*
 * LoomView.cs
 * ----------------------------------------------------------------------------
 * Draws the Loom tapestry inside the UI: one textured quad per shard (rotated
 * and faded when torn) plus the dashed perforation grid over the intact
 * tapestry. Coordinates are the original 512×512 canvas, scaled to the
 * element's size.
 * ----------------------------------------------------------------------------
 */

using System;
using UnityEngine;
using UnityEngine.UIElements;

namespace SpiceTrip.Hub
{
    public class LoomView : VisualElement
    {
        public LoomModel Model;
        public Texture2D Source;        // the pixelated tapestry, canvasSize²
        public Action<Vector2> OnCanvasClick;

        static readonly Color Perforation = new Color(1f, 240 / 255f, 200 / 255f, 0.35f);

        public LoomView()
        {
            AddToClassList("tap-canvas");
            style.overflow = Overflow.Hidden;   // the web canvas clipped shards that fell off it
            generateVisualContent += Draw;
            RegisterCallback<PointerDownEvent>(e =>
            {
                if (e.button != 0 || Model == null) return;
                float s = Model.T.canvasSize / Mathf.Max(1, contentRect.width);
                OnCanvasClick?.Invoke(new Vector2(e.localPosition.x * s, e.localPosition.y * s));
            });
        }

        void Draw(MeshGenerationContext mgc)
        {
            if (Model == null || Source == null || Model.Cells.Count == 0) return;
            float size = Model.T.canvasSize;
            float k = contentRect.width / size;
            float cell = Model.CellSize;

            foreach (var c in Model.Cells)
            {
                if (c.torn && c.alpha <= 0) continue;
                // Source UVs: canvas rows run top-down, texture rows bottom-up.
                float u0 = c.gx / size, u1 = (c.gx + cell) / size;
                float v1 = 1 - c.gy / size, v0 = 1 - (c.gy + cell) / size;
                Vector2 centre;
                float rot = 0, alpha = 1;
                if (!c.torn) centre = new Vector2(c.gx + cell / 2, c.gy + cell / 2);
                else { centre = new Vector2(c.x + cell / 2, c.y + cell / 2); rot = c.rot; alpha = Mathf.Max(0, c.alpha); }
                Quad(mgc, centre * k, cell * k, rot, new Color(1, 1, 1, alpha), u0, u1, v0, v1);
            }

            // Perforation overlay over the intact shards.
            var p = mgc.painter2D;
            p.strokeColor = Perforation;
            p.lineWidth = Mathf.Max(1, k);
            p.SetDashPattern(4f * k, 4f * k);
            for (int i = 1; i < Model.Cols; i++)
            {
                p.BeginPath(); p.MoveTo(new Vector2(i * cell * k, 0)); p.LineTo(new Vector2(i * cell * k, size * k)); p.Stroke();
                p.BeginPath(); p.MoveTo(new Vector2(0, i * cell * k)); p.LineTo(new Vector2(size * k, i * cell * k)); p.Stroke();
            }
            p.SetDashPattern(ReadOnlySpan<float>.Empty);
        }

        void Quad(MeshGenerationContext mgc, Vector2 centre, float side, float rot, Color tint, float u0, float u1, float v0, float v1)
        {
            var mesh = mgc.Allocate(4, 6, Source);
            float h = side / 2, cos = Mathf.Cos(rot), sin = Mathf.Sin(rot);
            Vector3 P(float x, float y) => new Vector3(centre.x + x * cos - y * sin, centre.y + x * sin + y * cos, Vertex.nearZ);
            Vector2 UV(float u, float v) => new Vector2(u, v);   // the renderer remaps into atlases itself
            mesh.SetNextVertex(new Vertex { position = P(-h, -h), tint = tint, uv = UV(u0, v1) });
            mesh.SetNextVertex(new Vertex { position = P(h, -h), tint = tint, uv = UV(u1, v1) });
            mesh.SetNextVertex(new Vertex { position = P(h, h), tint = tint, uv = UV(u1, v0) });
            mesh.SetNextVertex(new Vertex { position = P(-h, h), tint = tint, uv = UV(u0, v0) });
            mesh.SetNextIndex(0); mesh.SetNextIndex(1); mesh.SetNextIndex(2);
            mesh.SetNextIndex(0); mesh.SetNextIndex(2); mesh.SetNextIndex(3);
        }
    }
}
