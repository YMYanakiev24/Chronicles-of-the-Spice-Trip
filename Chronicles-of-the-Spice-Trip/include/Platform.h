// ============================================================================
//  Platform.h — the few things Android does differently (PlatformAndroid.cpp).
// ============================================================================
#pragma once

#include <string>

namespace platform {
#ifdef PLATFORM_ANDROID
    std::string dataDir();                       // private, writable storage for saves
    bool assetExists(const std::string& file);   // assets are packed inside the APK
    void windowSize(int& w, int& h);             // 720 lines tall, as wide as the phone's shape
    void vibrate(int ms);                        // a short buzz
#else
    inline void vibrate(int) {}                  // desktops don't buzz
#endif
}
