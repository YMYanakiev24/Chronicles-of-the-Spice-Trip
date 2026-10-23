/*
 * SynthDsp.cs
 * ----------------------------------------------------------------------------
 * The sound recipes from the web game's AudioManager.js, rebuilt as plain
 * sample math so they can be rendered into AudioClips (SFX) or streamed
 * (music). Oscillators, Web Audio-style exponential ramps and RBJ biquad
 * filters cover everything the original Web Audio graph did.
 * ----------------------------------------------------------------------------
 */

using System;

namespace SpiceTrip.Core
{
    public enum Wave { Sine, Square, Sawtooth, Triangle }
    public enum FilterKind { Lowpass, Highpass, Bandpass }

    public static class SynthDsp
    {
        public static Wave ParseWave(string s) => s switch
        {
            "square" => Wave.Square,
            "sawtooth" => Wave.Sawtooth,
            "triangle" => Wave.Triangle,
            _ => Wave.Sine,
        };

        /// <summary>One sample of a naive oscillator at phase (cycles).</summary>
        public static float Osc(Wave w, double phase)
        {
            double p = phase - Math.Floor(phase);
            switch (w)
            {
                case Wave.Square: return p < 0.5 ? 1f : -1f;
                case Wave.Sawtooth: return (float)(2 * p - 1);
                case Wave.Triangle: return (float)(p < 0.5 ? 4 * p - 1 : 3 - 4 * p);
                default: return (float)Math.Sin(2 * Math.PI * p);
            }
        }

        /// <summary>Web Audio exponentialRampToValueAtTime between two values over a duration.</summary>
        public static double ExpRamp(double from, double to, double t, double dur)
        {
            if (dur <= 0 || t >= dur) return to;
            if (t <= 0) return from;
            return from * Math.Pow(to / from, t / dur);
        }

        public struct Biquad
        {
            double b0, b1, b2, a1, a2, x1, x2, y1, y2;

            /// <summary>RBJ cookbook coefficients. q is linear (convert Web Audio's dB Q first for lowpass/highpass).</summary>
            public static Biquad Make(FilterKind kind, double freq, double q, int sampleRate)
            {
                double w0 = 2 * Math.PI * Math.Min(freq, sampleRate * 0.49) / sampleRate;
                double cos = Math.Cos(w0), alpha = Math.Sin(w0) / (2 * Math.Max(q, 1e-4));
                double b0, b1, b2, a0 = 1 + alpha, a1 = -2 * cos, a2 = 1 - alpha;
                switch (kind)
                {
                    case FilterKind.Highpass: b0 = (1 + cos) / 2; b1 = -(1 + cos); b2 = (1 + cos) / 2; break;
                    case FilterKind.Bandpass: b0 = alpha; b1 = 0; b2 = -alpha; break;
                    default: b0 = (1 - cos) / 2; b1 = 1 - cos; b2 = (1 - cos) / 2; break;
                }
                return new Biquad { b0 = b0 / a0, b1 = b1 / a0, b2 = b2 / a0, a1 = a1 / a0, a2 = a2 / a0 };
            }

            public float Process(float x)
            {
                double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                return (float)y;
            }
        }

        /// <summary>Web Audio lowpass/highpass Q is a resonance in dB; the RBJ formulas want linear Q.</summary>
        public static double DbQ(double db) => Math.Pow(10, db / 20.0);

        // --- the generic one-shots from AudioManager.js ------------------------------

        /// <summary>_tone: oscillator with a 10 ms exponential attack, exponential decay and optional pitch sweep.</summary>
        public static void Tone(float[] buf, int sr, double start, double freq, double freq2, Wave type, double dur, double vol)
        {
            int i0 = (int)(start * sr), n = (int)((dur + 0.02) * sr);
            double phase = 0;
            for (int i = 0; i < n && i0 + i < buf.Length; i++)
            {
                double t = (double)i / sr;
                if (t >= dur) break;
                double f = freq2 > 0 ? ExpRamp(freq, Math.Max(20, freq2), t, dur) : freq;
                double g = t < 0.01 ? ExpRamp(0.0001, vol, t, 0.01) : ExpRamp(vol, 0.0001, t - 0.01, dur - 0.01);
                buf[i0 + i] += (float)(Osc(type, phase) * g);
                phase += f / sr;
            }
        }

        /// <summary>_noise: a filtered white-noise burst with an exponential decay.</summary>
        public static void Noise(float[] buf, int sr, double start, double dur, double vol, double freq, double q, FilterKind kind, Random rng)
        {
            int i0 = (int)(start * sr), n = (int)(dur * sr);
            double linQ = kind == FilterKind.Bandpass ? q : DbQ(q);
            var filt = Biquad.Make(kind, freq, linQ, sr);
            for (int i = 0; i < n && i0 + i < buf.Length; i++)
            {
                double t = (double)i / sr;
                double g = ExpRamp(vol, 0.0001, t, dur);
                buf[i0 + i] += (float)(filt.Process((float)(rng.NextDouble() * 2 - 1)) * g);
            }
        }

        /// <summary>Total length in seconds of a named effect (its longest voice plus any delay).</summary>
        public static double SfxLength(string name) => name switch
        {
            "fire" => 0.4,
            "nature" => 0.32,
            "lightning" => 0.25,
            "heal" => 0.62,
            "shield" => 0.52,
            "ultimate" => 0.9,
            "hit" => 0.15,
            "hurt" => 0.27,
            "pickup" => 0.17,
            "quest" => 0.14 + 0.32,
            "levelup" => 3 * 0.11 + 0.27,
            "ui" => 0.08,
            "death" => 1.22,
            "whistle" => 0.42,
            _ => 0,
        };

        public static readonly string[] SfxNames =
            { "fire", "nature", "lightning", "heal", "shield", "ultimate", "hit", "hurt", "pickup", "quest", "levelup", "ui", "death", "whistle" };

        /// <summary>Renders one of the named effects; each is the same sound-design recipe as sfx(name) in JS.</summary>
        public static float[] RenderSfx(string name, int sr, int seed = 1)
        {
            var buf = new float[(int)Math.Ceiling(SfxLength(name) * sr) + 1];
            var rng = new Random(seed);
            switch (name)
            {
                case "fire":
                    Tone(buf, sr, 0, 320, 120, Wave.Sawtooth, 0.35, 0.3);
                    Noise(buf, sr, 0, 0.4, 0.25, 800, 0.7, FilterKind.Bandpass, rng);
                    break;
                case "nature":
                    Tone(buf, sr, 0, 520, 760, Wave.Triangle, 0.3, 0.25);
                    break;
                case "lightning":
                    Noise(buf, sr, 0, 0.25, 0.4, 3000, 0.5, FilterKind.Highpass, rng);
                    Tone(buf, sr, 0, 1200, 200, Wave.Square, 0.2, 0.2);
                    break;
                case "heal":
                    Tone(buf, sr, 0, 440, 880, Wave.Sine, 0.5, 0.3);
                    Tone(buf, sr, 0, 660, 990, Wave.Sine, 0.6, 0.2);
                    break;
                case "shield":
                    Tone(buf, sr, 0, 200, 500, Wave.Sine, 0.5, 0.3);
                    break;
                case "ultimate":
                    Tone(buf, sr, 0, 80, 400, Wave.Sawtooth, 0.8, 0.4);
                    Noise(buf, sr, 0, 0.9, 0.3, 600, 0.4, FilterKind.Bandpass, rng);
                    break;
                case "hit":
                    Noise(buf, sr, 0, 0.15, 0.35, 400, 1, FilterKind.Bandpass, rng);
                    Tone(buf, sr, 0, 180, 80, Wave.Square, 0.12, 0.2);
                    break;
                case "hurt":
                    Tone(buf, sr, 0, 260, 110, Wave.Sawtooth, 0.25, 0.35);
                    break;
                case "pickup":
                    Tone(buf, sr, 0, 660, 990, Wave.Triangle, 0.15, 0.3);
                    break;
                case "quest":
                    Tone(buf, sr, 0, 523, 0, Wave.Sine, 0.18, 0.3);
                    Tone(buf, sr, 0.14, 784, 0, Wave.Sine, 0.3, 0.3);
                    break;
                case "levelup":
                    double[] notes = { 523, 659, 784, 1047 };
                    for (int i = 0; i < notes.Length; i++) Tone(buf, sr, i * 0.11, notes[i], 0, Wave.Triangle, 0.25, 0.3);
                    break;
                case "ui":
                    Tone(buf, sr, 0, 600, 0, Wave.Sine, 0.06, 0.18);
                    break;
                case "death":
                    Tone(buf, sr, 0, 300, 60, Wave.Sawtooth, 1.2, 0.4);
                    break;
                case "whistle":   // Phase 3: Vlado's conductor whistle (two sharp blasts)
                    Tone(buf, sr, 0, 1900, 2050, Wave.Sine, 0.14, 0.22);
                    Tone(buf, sr, 0.2, 1900, 2150, Wave.Sine, 0.2, 0.22);
                    break;
            }
            return buf;
        }

        // --- music -----------------------------------------------------------------------

        public sealed class Mood
        {
            public double root;
            public Wave type, arp;
            public double rate;
        }

        /// <summary>The dark modal beds from playMusic(); lower roots mean more dread.</summary>
        public static Mood MoodFor(string mood)
        {
            switch (mood)
            {
                case "temple": return new Mood { root = 146.83, type = Wave.Sine, arp = Wave.Triangle, rate = 2.6 };
                case "ruins": return new Mood { root = 123.47, type = Wave.Sawtooth, arp = Wave.Sine, rate = 3.0 };
                case "caves": return new Mood { root = 97.999, type = Wave.Sawtooth, arp = Wave.Sine, rate = 3.6 };
                case "citadel": return new Mood { root = 138.59, type = Wave.Triangle, arp = Wave.Square, rate = 2.7 };
                case "godreach": return new Mood { root = 164.81, type = Wave.Triangle, arp = Wave.Triangle, rate = 2.2 };
                case "menu": return new Mood { root = 130.81, type = Wave.Sine, arp = Wave.Sine, rate = 2.6 };
                default: return new Mood { root = 130.81, type = Wave.Sine, arp = Wave.Triangle, rate = 2.8 }; // mist
            }
        }

        // A gentle, mysterious scale (natural minor / aeolian) in semitone steps.
        public static readonly int[] Scale = { 0, 2, 3, 5, 7, 8, 10, 12 };

        /// <summary>
        /// One mood's bed: two detuned drones + a sub with a breathing LFO on each
        /// gain, plus a slow random arpeggio of low-passed plucks. Fades in over
        /// 4 s and out over 1.5 s, as the Web Audio gain ramps did.
        /// </summary>
        public sealed class MusicVoice
        {
            readonly Mood _m;
            readonly int _sr;
            readonly Random _rng;
            readonly double[] _phase = new double[3];
            readonly double[] _freq = new double[3];
            readonly double[] _target = { 0.07, 0.07, 0.12 };
            readonly double[] _lfoHz = new double[3];
            double _t;
            double _nextArp = 1.2;
            double _fadeOutAt = -1;
            readonly System.Collections.Generic.List<Pluck> _plucks = new System.Collections.Generic.List<Pluck>();

            sealed class Pluck
            {
                public double freq, phase, t;
                public Wave type;
                public Biquad filt;
            }

            public MusicVoice(Mood m, int sampleRate, int seed)
            {
                _m = m; _sr = sampleRate; _rng = new Random(seed);
                int[] semis = { 0, 7, -12 };
                for (int i = 0; i < 3; i++)
                {
                    double detuneCents = (i - 1) * 6;
                    _freq[i] = m.root * Math.Pow(2, semis[i] / 12.0) * Math.Pow(2, detuneCents / 1200.0);
                    _lfoHz[i] = 0.08 + i * 0.03;
                }
            }

            public bool Finished => _fadeOutAt >= 0 && _t > _fadeOutAt + 1.6 && _plucks.Count == 0;
            public void FadeOut() { if (_fadeOutAt < 0) _fadeOutAt = _t; }

            public float Next()
            {
                double fade = _fadeOutAt < 0 ? 1 : Math.Max(0, 1 - (_t - _fadeOutAt) / 1.5);
                double s = 0;
                for (int i = 0; i < 3; i++)
                {
                    double g = Math.Min(1, _t / 4) * _target[i] + 0.03 * Math.Sin(2 * Math.PI * _lfoHz[i] * _t);
                    s += Osc(_m.type, _phase[i]) * g * fade;
                    _phase[i] += _freq[i] / _sr;
                }

                if (_fadeOutAt < 0 && _t >= _nextArp)
                {
                    int note = Scale[_rng.Next(Scale.Length)];
                    int oct = _rng.NextDouble() < 0.4 ? 12 : 0;
                    _plucks.Add(new Pluck
                    {
                        freq = _m.root * Math.Pow(2, (note + oct) / 12.0),
                        type = _m.arp,
                        filt = Biquad.Make(FilterKind.Lowpass, 1800, DbQ(1), _sr),
                    });
                    _nextArp = _t + _m.rate + _rng.NextDouble() * 1.5;
                }

                for (int i = _plucks.Count - 1; i >= 0; i--)
                {
                    var p = _plucks[i];
                    double g = p.t < 0.02 ? 0.06 * p.t / 0.02 : ExpRamp(0.06, 0.0001, p.t - 0.02, 1.58);
                    s += p.filt.Process(Osc(p.type, p.phase)) * g;
                    p.phase += p.freq / _sr;
                    p.t += 1.0 / _sr;
                    if (p.t >= 1.7) _plucks.RemoveAt(i);
                }

                _t += 1.0 / _sr;
                return (float)s;
            }
        }
    }
}
