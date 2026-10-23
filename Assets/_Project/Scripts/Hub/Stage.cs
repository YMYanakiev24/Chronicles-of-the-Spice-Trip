/*
 * Stage.cs
 * ----------------------------------------------------------------------------
 * The 16:9 stage every full-screen scene is drawn on (.scene-stage in the web
 * CSS): width = min(100vw, 100vh·16/9), letterboxed and centred. Hotspots are
 * percentages of the stage, so they stay over the painted doors and bosses at
 * any window size.
 * ----------------------------------------------------------------------------
 */

using UnityEngine;
using UnityEngine.UIElements;

namespace SpiceTrip.Hub
{
    public class Stage : VisualElement
    {
        public Stage()
        {
            AddToClassList("scene-stage");
            RegisterCallback<AttachToPanelEvent>(_ => Fit());
        }

        /// <summary>Call after the parent's size changes.</summary>
        public void Fit()
        {
            var p = parent;
            if (p == null) return;
            var r = Fit(p.resolvedStyle.width, p.resolvedStyle.height);
            style.left = r.x; style.top = r.y; style.width = r.width; style.height = r.height;
        }

        public static Rect Fit(float w, float h, float aspect = 16f / 9f)
        {
            if (float.IsNaN(w) || float.IsNaN(h) || w <= 0 || h <= 0) return new Rect(0, 0, 0, 0);
            float sw = Mathf.Min(w, h * aspect), sh = Mathf.Min(h, w / aspect);
            return new Rect((w - sw) / 2, (h - sh) / 2, sw, sh);
        }
    }
}
