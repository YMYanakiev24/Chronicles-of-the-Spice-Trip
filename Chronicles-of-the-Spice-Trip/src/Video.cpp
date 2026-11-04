// ============================================================================
//  Video.cpp — the MPEG-1 player (see Video.h).
//
//  The sound is decoded up front into one Sound and simply played; the
//  pictures are decoded as they fall due against the same clock, so the two
//  stay together. A video this short makes that the simple, safe way.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstdio>
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "Video.h"
#include "Render.h"
#include <cmath>
#include <string>

bool VideoPlayer::open(const char* file) {
    close();
    std::string path = findAsset(file);
    if (path.empty()) return false;
    int size = 0;
    data = LoadFileData(path.c_str(), &size);
    if (!data || size <= 0) { close(); return false; }

    // the sound, all of it, into a Sound
    if (plm_t* a = plm_create_with_memory(data, (size_t)size, 0)) {
        plm_set_video_enabled(a, 0);
        if (plm_get_num_audio_streams(a) > 0) {
            plm_set_audio_enabled(a, 1);
            std::vector<short> pcm;
            while (plm_samples_t* s = plm_decode_audio(a))
                for (int i = 0; i < PLM_AUDIO_SAMPLES_PER_FRAME * 2; ++i)
                    pcm.push_back((short)(fmaxf(-1.0f, fminf(1.0f, s->interleaved[i])) * 32767));
            if (!pcm.empty()) {
                Wave w{};
                w.frameCount = (unsigned)(pcm.size() / 2); w.sampleRate = (unsigned)plm_get_samplerate(a);
                w.sampleSize = 16; w.channels = 2; w.data = pcm.data();
                sound = LoadSoundFromWave(w);       // copies the samples
                hasSound = true;
            }
        }
        plm_destroy(a);
    }

    // the pictures
    plm = plm_create_with_memory(data, (size_t)size, 0);
    if (!plm || plm_get_num_video_streams(plm) == 0 || plm_get_width(plm) <= 0) { close(); return false; }
    plm_set_audio_enabled(plm, 0);
    plm_set_loop(plm, 0);
    int w = plm_get_width(plm), h = plm_get_height(plm);
    duration = (float)plm_get_duration(plm);
    rgba.assign((size_t)w * h * 4, 0);
    for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    Image blank{ rgba.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    tex = LoadTextureFromImage(blank);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    time = 0; framesShown = 0; ended = false; started = false; pending = nullptr;
    return true;
}

void VideoPlayer::update(float dt) {
    if (!plm) return;
    if (!started) { started = true; if (hasSound) PlaySound(sound); }
    else time += dt;
    bool fresh = false;
    while (!ended) {
        if (!pending && !(pending = plm_decode_video(plm))) { ended = true; break; }
        plm_frame_t* f = (plm_frame_t*)pending;
        if (f->time > time) break;
        plm_frame_to_rgba(f, rgba.data(), tex.width * 4);
        pending = nullptr;
        fresh = true;
        framesShown++;
    }
    if (fresh) UpdateTexture(tex, rgba.data());
}

void VideoPlayer::draw(Rectangle area) const {
    DrawRectangleRec(area, BLACK);
    if (!tex.id) return;
    // Cover the whole area: the video is scaled until no bars are left, and whatever
    // reaches past a screen of another shape is trimmed evenly off both sides.
    float sc = fmaxf(area.width / tex.width, area.height / tex.height);
    float sw = area.width / sc, sh = area.height / sc;
    DrawTexturePro(tex, { (tex.width - sw) / 2, (tex.height - sh) / 2, sw, sh }, area, { 0, 0 }, 0, WHITE);
}

bool VideoPlayer::finished() const {
    return !plm || (ended && time >= duration);
}

void VideoPlayer::close() {
    if (hasSound) { StopSound(sound); UnloadSound(sound); }
    if (tex.id) UnloadTexture(tex);
    if (plm) plm_destroy(plm);
    if (data) UnloadFileData(data);
    plm = nullptr; pending = nullptr; data = nullptr; tex = Texture2D{}; sound = Sound{};
    hasSound = started = ended = false;
    rgba.clear(); rgba.shrink_to_fit();
}
