/*
 * Tuning.cs
 * ----------------------------------------------------------------------------
 * Numbers that were inline literals in the web game's BossBattle.js and the
 * Enchanted Loom in Scenes.js. Kept in logical canvas pixels and seconds,
 * exactly as written in the JavaScript, and converted to world units in one
 * place (BossBattleView).
 * ----------------------------------------------------------------------------
 */

using System;

namespace SpiceTrip.Data
{
    [Serializable]
    public class BattleTuning
    {
        public float arenaWidth = 800, arenaHeight = 450;
        public float maxDt = 0.05f;

        // Player
        public float playerStartYFromBottom = 70;
        public float playerRadius = 14;
        public int playerHp = 100;
        public float playerSpeed = 220;
        public float playerEdge = 16;
        public float playerFireCooldown = 0.22f;
        public float playerShotSpeed = 380, playerShotRadius = 5, playerShotLife = 1.4f;
        public int playerShotDamage = 8;
        public float playerSpriteSize = 56;     // champion art, logical px
        public float heroSpriteScale = 2.2f;     // procedural hero

        // Boss
        public float bossStartY = 90;
        public float bossRadius = 32;
        public float bossSpeed = 95;
        public float bossFirstShot = 1.2f;
        public float bossMoveMin = 1.4f, bossMoveJitter = 1.2f;
        public float bossWaypointMarginX = 120, bossWaypointMinY = 60, bossWaypointRangeY = 0.4f;
        public float bossClampEdge = 40, bossClampMaxY = 0.55f;
        public float bossFireInterval = 1.1f;
        public int bossSpreadCount = 3;
        public float bossSpreadAngle = 0.22f;
        public float bossShotSpeed = 240, bossShotRadius = 7, bossShotLife = 3, bossShotYOffset = 10;
        public int bossShotDamage = 8;
        public float bossSpriteScale = 2.6f;

        // Hits
        public float playerHitInvuln = 0.6f;
        public int contactDamage = 12;
        public float contactInvuln = 0.8f;
        public float bossHurtFlash = 0.1f;
    }

    [Serializable]
    public class LoomTuning
    {
        public int canvasSize = 512;
        public int[] gridOptions = { 4, 8, 12, 16 };
        public int defaultGrid = 8;
        public int minPixelSize = 48;
        public int pixelsPerCell = 8;
        public float tearVx = 260;         // vx = (rand-0.5) * tearVx
        public float tearVyMin = 120, tearVyRange = 120;   // vy = -min - rand*range
        public float tearSpin = 8;          // vr = (rand-0.5) * tearSpin
        public float gravity = 520;
        public float fadePerSecond = 0.7f;
    }
}
