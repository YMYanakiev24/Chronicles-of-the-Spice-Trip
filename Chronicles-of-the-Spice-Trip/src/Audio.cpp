// ============================================================================
//  Audio.cpp — implementation of the runtime synthesizer.
// ============================================================================
#include "Audio.h"
#include "Common.h"
#include <vector>
#include <cmath>
#include <cstdint>

namespace au {

    // Single-sample oscillator (phase in [0,1)).
    static float osc(WaveType t, float phase) {
        switch (t) {
        case SINE:   return sinf(2.0f * PI * phase);
        case SQUARE: return phase < 0.5f ? 1.0f : -1.0f;
        case SAW:    return 2.0f * phase - 1.0f;
        case TRI:    return 4.0f * fabsf(phase - 0.5f) - 1.0f;
        }
        return 0;
    }

    // RBJ biquad (lowpass / highpass / bandpass) for the noise SFX.
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set(int type, float freq, float q, float sr) {   // 0 lp, 1 hp, 2 bp
            float w0 = 2.0f * PI * freq / sr;
            float c = cosf(w0), s = sinf(w0);
            float alpha = s / (2.0f * q);
            float a0;
            if (type == 0) { float b1n = 1 - c; b0 = b1n / 2; b1 = b1n; b2 = b1n / 2; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; }
            else if (type == 1) { float b1n = 1 + c; b0 = b1n / 2; b1 = -(b1n); b2 = b1n / 2; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; }
            else { b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; }
            b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        }
        float proc(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };

    // Render a pitched tone (optional exponential freq sweep) additively.
    static void renderTone(std::vector<float>& buf, float freq, float freq2,
        WaveType type, float dur, float vol, float startT = 0) {
        int n = (int)(dur * SR);
        int off = (int)(startT * SR);
        if ((int)buf.size() < off + n) buf.resize(off + n, 0.0f);
        float k = logf(vol / 0.0008f) / dur;
        float phase = 0;
        for (int i = 0; i < n; ++i) {
            float tt = (float)i / SR;
            float f = freq2 > 0 ? freq * powf(freq2 / freq, tt / dur) : freq;
            phase += f / SR; if (phase >= 1) phase -= 1;
            float env = (tt < 0.01f) ? (tt / 0.01f) * vol : vol * expf(-k * (tt - 0.01f));
            buf[off + i] += osc(type, phase) * env;
        }
    }

    // Render a filtered noise burst additively.
    static void renderNoise(std::vector<float>& buf, float dur, float vol,
        float freq, float q, int ftype, float startT = 0) {
        int n = (int)(dur * SR);
        int off = (int)(startT * SR);
        if ((int)buf.size() < off + n) buf.resize(off + n, 0.0f);
        Biquad bq; bq.set(ftype, freq, q < 0.3f ? 0.3f : q, (float)SR);
        float k = logf(vol / 0.0008f) / dur;
        for (int i = 0; i < n; ++i) {
            float tt = (float)i / SR;
            float env = vol * expf(-k * tt);
            buf[off + i] += bq.proc(frand(-1, 1)) * env;
        }
    }

    static Sound toSound(const std::vector<float>& buf) {
        std::vector<int16_t> pcm(buf.size());
        for (size_t i = 0; i < buf.size(); ++i) {
            float v = buf[i]; if (v > 1) v = 1; if (v < -1) v = -1;
            pcm[i] = (int16_t)(v * 30000);
        }
        Wave w{};
        w.frameCount = (unsigned int)pcm.size();
        w.sampleRate = SR; w.sampleSize = 16; w.channels = 1;
        w.data = (void*)pcm.data();
        return LoadSoundFromWave(w);   // raylib copies the data
    }

    std::map<std::string, Sound> buildSfx() {
        std::map<std::string, Sound> m;
        auto make = [&](const std::string& name, std::vector<float> b) { m[name] = toSound(b); };
        std::vector<float> b;

        b.clear(); renderTone(b, 320, 120, SAW, 0.35f, 0.30f); renderNoise(b, 0.40f, 0.25f, 800, 0.7f, 0); make("fire", b);
        b.clear(); renderTone(b, 520, 760, TRI, 0.30f, 0.25f); make("nature", b);
        b.clear(); renderNoise(b, 0.25f, 0.40f, 3000, 0.5f, 1); renderTone(b, 1200, 200, SQUARE, 0.20f, 0.20f); make("lightning", b);
        b.clear(); renderTone(b, 440, 880, SINE, 0.50f, 0.30f); renderTone(b, 660, 990, SINE, 0.60f, 0.20f); make("heal", b);
        b.clear(); renderTone(b, 200, 500, SINE, 0.50f, 0.30f); make("shield", b);
        b.clear(); renderTone(b, 80, 400, SAW, 0.80f, 0.40f); renderNoise(b, 0.90f, 0.30f, 600, 0.4f, 0); make("ultimate", b);
        b.clear(); renderNoise(b, 0.15f, 0.35f, 400, 1.0f, 2); renderTone(b, 180, 80, SQUARE, 0.12f, 0.20f); make("hit", b);
        b.clear(); renderTone(b, 260, 110, SAW, 0.25f, 0.35f); make("hurt", b);
        b.clear(); renderTone(b, 660, 990, TRI, 0.15f, 0.30f); make("pickup", b);
        b.clear(); renderTone(b, 523, 0, SINE, 0.18f, 0.30f); renderTone(b, 784, 0, SINE, 0.30f, 0.30f, 0.14f); make("quest", b);
        b.clear(); { float f[4] = { 523,659,784,1047 }; for (int i = 0; i < 4; ++i) renderTone(b, f[i], 0, TRI, 0.25f, 0.30f, i * 0.11f); } make("levelup", b);
        b.clear(); renderTone(b, 600, 0, SINE, 0.06f, 0.18f); make("ui", b);
        b.clear(); renderTone(b, 300, 60, SAW, 1.20f, 0.40f); make("death", b);
        return m;
    }

    // --- Music --------------------------------------------------------------
    void Music::init() { stream = LoadAudioStream(SR, 16, 1); PlayAudioStream(stream); }

    void Music::setMood(const std::string& mood) {
        active = true;
        if (mood == "menu") { root = 130.81f; droneType = SINE; arpType = SINE; rate = 2.6f; }
        else { root = 130.81f; droneType = SINE; arpType = TRI; rate = 2.8f; } // mist
    }

    void Music::triggerPluck() {
        int note = scale[GetRandomValue(0, 7)];
        int oct = (frand(0, 1) < 0.4f) ? 12 : 0;
        float f = root * powf(2.0f, (note + oct) / 12.0f);
        for (auto& p : plucks) if (!p.on) { p.on = true; p.phase = 0; p.t = 0; p.freq = f; p.type = arpType; return; }
        plucks[0] = Pluck{ 0, f, 0, 1.7f, arpType, true };
    }

    void Music::update() {
        if (!active) return;
        while (IsAudioStreamProcessed(stream)) {
            const int N = 1024;
            static int16_t out[N];
            for (int i = 0; i < N; ++i) {
                float dt = 1.0f / SR;
                lfo += dt * 0.08f * 2 * PI;
                float trem = 0.85f + 0.15f * sinf(lfo);
                float f0 = root, f1 = root * powf(2, 7 / 12.0f), f2 = root * 0.5f;
                dphase[0] += f0 / SR; if (dphase[0] >= 1) dphase[0] -= 1;
                dphase[1] += f1 / SR; if (dphase[1] >= 1) dphase[1] -= 1;
                dphase[2] += f2 / SR; if (dphase[2] >= 1) dphase[2] -= 1;
                float s = osc(droneType, dphase[0]) * 0.07f
                    + osc(droneType, dphase[1]) * 0.07f
                    + osc(droneType, dphase[2]) * 0.12f;
                s *= trem;
                arpTimer -= dt;
                if (arpTimer <= 0) { triggerPluck(); arpTimer = rate + frand(0, 1.5f); }
                for (auto& p : plucks) {
                    if (!p.on) continue;
                    p.phase += p.freq / SR; if (p.phase >= 1) p.phase -= 1;
                    p.t += dt;
                    s += osc(p.type, p.phase) * (0.06f * expf(-p.t * 2.5f));
                    if (p.t > p.dur) p.on = false;
                }
                s *= vol;
                if (s > 1) s = 1; if (s < -1) s = -1;
                out[i] = (int16_t)(s * 30000);
            }
            UpdateAudioStream(stream, out, N);
        }
    }

    void Music::unload() { UnloadAudioStream(stream); }

} // namespace au
