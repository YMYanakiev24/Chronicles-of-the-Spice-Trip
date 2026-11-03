// ============================================================================
//  PlatformAndroid.cpp — saves, packed assets and the screen shape on Android.
// ============================================================================
#ifdef PLATFORM_ANDROID
#include "Platform.h"
#include <algorithm>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <jni.h>

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

    // Through Java: Context.getSystemService(VIBRATOR_SERVICE).vibrate(ms). The game's thread
    // stays attached to the VM, and the Vibrator is looked up once.
    void vibrate(int ms) {
        static jobject vibrator = nullptr;
        static jmethodID vibrateId = nullptr;
        static bool failed = false;
        if (failed) return;
        ANativeActivity* act = GetAndroidApp()->activity;
        JNIEnv* env = nullptr;
        if (act->vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK && act->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            failed = true;
            return;
        }
        if (!vibrator) {
            jclass context = env->FindClass("android/content/Context");
            jfieldID field = context ? env->GetStaticFieldID(context, "VIBRATOR_SERVICE", "Ljava/lang/String;") : nullptr;
            jobject name = field ? env->GetStaticObjectField(context, field) : nullptr;
            jclass activity = env->GetObjectClass(act->clazz);
            jmethodID get = env->GetMethodID(activity, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
            jobject service = name && get ? env->CallObjectMethod(act->clazz, get, name) : nullptr;
            jclass vibratorClass = env->FindClass("android/os/Vibrator");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (!service || !vibratorClass) { failed = true; return; }
            vibrateId = env->GetMethodID(vibratorClass, "vibrate", "(J)V");
            if (env->ExceptionCheck() || !vibrateId) { env->ExceptionClear(); failed = true; return; }
            vibrator = env->NewGlobalRef(service);
        }
        env->CallVoidMethod(vibrator, vibrateId, (jlong)ms);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

} // namespace platform
#endif
