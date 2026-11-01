// ============================================================================
//  Platform.h — the few things Android does differently (PlatformAndroid.cpp).
// ============================================================================
#pragma once

#ifdef PLATFORM_ANDROID
#include <string>

namespace platform {
    std::string dataDir();                       // private, writable storage for saves
    bool assetExists(const std::string& file);   // assets are packed inside the APK
    void windowSize(int& w, int& h);             // 720 lines tall, as wide as the phone's shape
}
#endif
