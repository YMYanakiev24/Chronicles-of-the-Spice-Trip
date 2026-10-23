/*
 * RpgInterfaces.cs
 * ----------------------------------------------------------------------------
 * The seams between the Chronicle rules and Unity. The web game's gameplay
 * code called the UI manager, the input manager and the canvas directly; here
 * those calls go through three small interfaces so the rules run unchanged in
 * tests (with null implementations) and in the game (RpgHud / RpgView).
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Art;
using UnityEngine;

namespace SpiceTrip.Rpg
{
    /// <summary>Everything the rules ask of the interface (UI.js methods called from gameplay).</summary>
    public interface IRpgUi
    {
        void Toast(string msg, string type = "info", float dur = 2.4f);
        void BigBanner(string text);
        void RefreshHUD();
        void RefreshQuestTracker();
        void DamageFlash();
        void Shake(float amount);
        void SpawnDamageNumber(float wx, float wy, int amount, bool crit);
        void ShowInteract(string richText);
        void HideInteract();
        void ShowHint(string richText, float dur = 6);
        void ShowDialogue(Npc npc, string richText, IReadOnlyList<DialogueChoice> choices);
        void HideDialogue();
        void ShowDeath();
        void HideDeath();
        void SetRegionName(string name);
        bool IsBlocking { get; }
    }

    public struct DialogueChoice
    {
        public string label;
        public bool primary;
        public Action action;
    }

    /// <summary>One frame of player intent (Input.js polled state, already mapped to actions).</summary>
    public struct RpgInput
    {
        public float moveX, moveY;        // axis; y is +1 for "up" like Input.moveAxis()
        public bool run;                  // Shift held
        public bool rollPressed;          // Space this frame
        public Vector2 aimWorld;          // cursor in world pixels
        public int castSlot;              // 0-5 pressed this frame, -1 none
        public bool meleePressed;         // left click this frame
        public bool castFirstPressed;     // right click this frame (slot 1)
        public bool interactPressed;      // E this frame

        public static RpgInput None => new RpgInput { castSlot = -1 };
    }

    public enum Blend { Alpha, Additive, Screen }

    /// <summary>
    /// Immediate-mode drawing in world pixels (y down), in paint order — the
    /// subset of the 2D canvas API the original draw() methods used.
    /// </summary>
    public interface IWorldPainter
    {
        void Sprite(PixelCanvas sprite, float x, float y, float alpha = 1, Blend blend = Blend.Alpha);
        void SpriteRotated(PixelCanvas sprite, float centreX, float centreY, float radians, float alpha = 1);
        void Rect(float x, float y, float w, float h, Color color, Blend blend = Blend.Alpha);
        void Line(float x1, float y1, float x2, float y2, float width, Color color, Blend blend = Blend.Alpha);
        void Arc(float cx, float cy, float r, float a0, float a1, float width, Color color, Blend blend = Blend.Alpha);
        void Glow(float cx, float cy, float r, Color color, Blend blend = Blend.Additive);
        void Glyph(string glyph, float cx, float baselineY, Color color);
    }

    public static class Css
    {
        public static Color C(string css) => PixelCanvas.ParseColor(css) ?? Color.white;
        public static Color A(string css, float alpha) { var c = C(css); c.a = alpha; return c; }
    }
}
