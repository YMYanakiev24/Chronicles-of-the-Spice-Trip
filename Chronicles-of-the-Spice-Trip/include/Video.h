// ============================================================================
//  Video.h — plays an MPEG-1 video (.mpg, MPEG-1 video + MP2 audio) into a
//  texture, with its sound. Decoding is pl_mpeg (vendor/pl_mpeg, MIT).
//
//  Make one from any video with ffmpeg:
//    ffmpeg -i in.mp4 -c:v mpeg1video -q:v 2 -c:a mp2 -b:a 192k -f mpeg out.mpg
// ============================================================================
#pragma once

#include "raylib.h"
#include <vector>

typedef struct plm_t plm_t;

struct VideoPlayer {
    bool  open(const char* file);       // a file under assets/; false if it can't be played
    void  update(float dt);             // advance the clock, show the frames that are due
    void  draw(Rectangle area) const;   // as large as fits in `area`, centred, black bars round it
    void  close();
    bool  playing() const { return plm != nullptr; }
    bool  finished() const;             // played to the end
    int   framesShown = 0;              // frames put on screen so far
    float time = 0, duration = 0;       // seconds

private:
    plm_t* plm = nullptr;
    void* pending = nullptr;            // a decoded plm_frame_t, waiting for its time
    bool  ended = false;
    unsigned char* data = nullptr;      // the whole file (pl_mpeg reads it in place)
    std::vector<unsigned char> rgba;
    Texture2D tex{};
    Sound sound{};
    bool  hasSound = false, started = false;
};
