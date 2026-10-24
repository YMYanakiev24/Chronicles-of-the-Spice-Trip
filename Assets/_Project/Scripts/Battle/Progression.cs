/*
 * Progression.cs
 * ----------------------------------------------------------------------------
 * What a boss victory is worth: a gold bounty paid into Marin's wallet (a big
 * one for the first win, a smaller one after), a star rating, and the best
 * time. Records are kept per boss in hub.json, plus one for the Gauntlet.
 *
 *   ★     win
 *   ★★    win with at least half your health left
 *   ★★★   ...and within the boss's par time
 * ----------------------------------------------------------------------------
 */

using SpiceTrip.Core;
using UnityEngine;

namespace SpiceTrip.Battle
{
    public static class Progression
    {
        public const string GauntletId = "gauntlet";

        public struct Reward
        {
            public int gold, stars;
            public bool firstWin, newBestTime, newBestStars;
            public float time, bestTime;
        }

        public static int Stars(float hpRatio, float time, float par) =>
            1 + (hpRatio >= 0.5f ? 1 : 0) + (hpRatio >= 0.5f && par > 0 && time <= par ? 1 : 0);

        /// <summary>Pays the bounty into the hub wallet and updates the record. The caller saves the hub.</summary>
        public static Reward RecordWin(HubData hub, string recordId, int firstGold, int repeatGold, float par, float hpRatio, float time)
        {
            var rec = hub.Record(recordId);
            var r = new Reward { stars = Stars(hpRatio, time, par), time = time, firstWin = rec.wins == 0 };
            r.gold = r.firstWin ? firstGold : repeatGold;
            hub.gold += r.gold;
            rec.wins++;
            r.newBestStars = r.stars > rec.bestStars;
            if (r.newBestStars) rec.bestStars = r.stars;
            r.newBestTime = rec.bestTime <= 0 || time < rec.bestTime;
            if (r.newBestTime) rec.bestTime = time;
            r.bestTime = rec.bestTime;
            return r;
        }

        public static string Clock(float seconds)
        {
            int s = Mathf.Max(0, Mathf.RoundToInt(seconds));
            return $"{s / 60}:{s % 60:00}";
        }
    }
}
