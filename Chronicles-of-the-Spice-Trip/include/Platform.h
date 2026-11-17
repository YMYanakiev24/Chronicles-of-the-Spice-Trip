// ============================================================================
//  Platform.h — the few things phones and browsers do differently
//  (PlatformAndroid.cpp, PlatformWeb.cpp).
// ============================================================================
#pragma once

#include "raylib.h"
#include <string>
#include <vector>

namespace platform {
#ifdef PLATFORM_ANDROID
    std::string dataDir();                       // private, writable storage for saves
    bool assetExists(const std::string& file);   // assets are packed inside the APK
    void windowSize(int& w, int& h);             // 720 lines tall, as wide as the phone's shape
    void vibrate(int ms);                        // a short buzz
    inline void persist() {}                     // files are on disk already
#elif defined(PLATFORM_WEB)
    // The browser build (iPhone, Android and desktop browsers). The page is web/shell.html.
    void windowSize(int& w, int& h);             // the size the page wants the canvas drawn at
    void fitWindow();                            // once a frame: follow the page when it's resized or turned
    bool touchScreen();                          // a phone or tablet: show the touch controls
    void touches(std::vector<Vector2>& pos, std::vector<int>& ids);   // fingers on the canvas, in screen pixels
    bool sawFinger();                            // has a finger touched the canvas yet?
    void persist();                              // copy the save folder into the browser's storage
    void vibrate(int ms);                        // a short buzz, where the browser can
    bool browserFullscreen();
    void setBrowserFullscreen(bool on);
#else
    inline void vibrate(int) {}                  // desktops don't buzz
    inline void persist() {}                     // files are on disk already
#endif
}
