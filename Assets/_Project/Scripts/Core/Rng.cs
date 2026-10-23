/*
 * Rng.cs
 * ----------------------------------------------------------------------------
 * Bit-exact ports of the web game's makeRNG (mulberry32) and hashStr (FNV-1a),
 * so procedural sprites seeded with them come out pixel-identical.
 * ----------------------------------------------------------------------------
 */

namespace SpiceTrip.Core
{
    public sealed class Mulberry32
    {
        int _a;

        public Mulberry32(uint seed) { _a = unchecked((int)seed); }
        public Mulberry32(long seed) : this(unchecked((uint)seed)) { }

        /// <summary>Next value in [0, 1), identical to the JavaScript generator.</summary>
        public double Next()
        {
            unchecked
            {
                _a = _a + 0x6D2B79F5;
                uint ua = (uint)_a;
                int t = (int)(ua ^ (ua >> 15)) * (1 | _a);
                uint ut = (uint)t;
                t = (t + (int)(ut ^ (ut >> 7)) * (61 | t)) ^ t;
                ut = (uint)t;
                return (ut ^ (ut >> 14)) / 4294967296.0;
            }
        }
    }

    public static class Hash
    {
        /// <summary>FNV-1a over UTF-16 code units, as PixelArt.js hashStr.</summary>
        public static uint Str(string s)
        {
            unchecked
            {
                int h = (int)2166136261;
                foreach (char c in s) { h ^= c; h *= 16777619; }
                return (uint)h;
            }
        }

        /// <summary>FNV-1a over raw bytes (used to fingerprint sprites in tests).</summary>
        public static uint Bytes(byte[] data)
        {
            unchecked
            {
                int h = (int)2166136261;
                foreach (byte b in data) { h ^= b; h *= 16777619; }
                return (uint)h;
            }
        }
    }
}
