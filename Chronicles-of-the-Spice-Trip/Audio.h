// ============================================================================
//  Audio.h — runtime sound synthesis (ports js/core/AudioManager.js).
//  All SFX and music are generated in code; there are no audio files.
// ============================================================================
#pragma once

#include "raylib.h"
#include <map>
#include <string>

namespace au {

    enum WaveType { SINE, SQUARE, SAW, TRI };

    // Build every named SFX recipe and return them keyed by name.
    std::map<std::string, Sound> buildSfx();

    // --- Evolving drone "music" (ports AudioManager.playMusic) --------------
    struct Pluck { float phase = 0, freq = 0, t = 0, dur = 1.7f; WaveType type = TRI; bool on = false; };

    struct Music {
        AudioStream stream{};
        bool  active = false;
        float root = 130.81f;
        WaveType droneType = SINE, arpType = TRI;
        float rate = 2.8f;
        float dphase[3] = { 0, 0, 0 };
        float lfo = 0;
        float arpTimer = 1.2f;
        Pluck plucks[4];
        float vol = 0.32f;
        int   scale[8] = { 0, 2, 3, 5, 7, 8, 10, 12 };   // aeolian, matches _scale

        void init();
        void setMood(const std::string& mood);
        void triggerPluck();
        void update();                                    // call once per frame
        void unload();
    };

} // namespace au
