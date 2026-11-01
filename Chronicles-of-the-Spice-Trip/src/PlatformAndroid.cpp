// ============================================================================
//  PlatformAndroid.cpp — saves, packed assets and the screen shape on Android.
// ============================================================================
#ifdef PLATFORM_ANDROID
#include "Platform.h"
#include <algorithm>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>

extern "C" struct android_app* GetAndroidApp(void);     // from raylib's rcore_android.c

namespace platform {

    std::string dataDir() { return GetAndroidApp()->activity->internalDataPath; }

    bool assetExists(const std::string& file) {
        AAsset* a = AAssetManager_open(GetAndroidApp()->activity->assetManager, file.c_str(), AASSET_MODE_UNKNOWN);
        if (a) AAsset_close(a);
        return a != nullptr;
    }

    void windowSize(int& w, int& h) {
        // The UI is laid out for about 720p; raylib renders that and the phone scales it up.
        AConfiguration* c = GetAndroidApp()->config;
        int a = AConfiguration_getScreenWidthDp(c), b = AConfiguration_getScreenHeightDp(c);
        int longSide = std::max(a, b), shortSide = std::min(a, b);
        h = 720;
        w = shortSide > 0 ? std::max(960, longSide * 720 / shortSide) : 1600;
    }

} // namespace platform
#endif
