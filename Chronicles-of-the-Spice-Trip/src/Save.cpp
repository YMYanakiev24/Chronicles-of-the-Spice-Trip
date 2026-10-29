// ============================================================================
//  Save.cpp — binary save files.
// ============================================================================
#include "Save.h"
#include <cstring>
#include <filesystem>
#include <type_traits>

namespace save {

    namespace {
        constexpr unsigned RUN_MAGIC = 0x53504333;       // "SPC3": champions and boss fights
        constexpr unsigned PROFILE_MAGIC = 0x53505031;   // "SPP1"
        constexpr unsigned SETTINGS_MAGIC = 0x53505331;  // "SPS1"

        std::string g_dir;

        template <typename T>
        bool writePod(const char* file, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            return SaveFileData(path(file).c_str(), (void*)&value, (int)sizeof value);
        }

        template <typename T>
        bool readPod(const char* file, T& value, unsigned magic) {
            static_assert(std::is_trivially_copyable_v<T>);
            std::string p = path(file);
            if (!FileExists(p.c_str())) return false;
            int size = 0;
            unsigned char* data = LoadFileData(p.c_str(), &size);
            if (!data) return false;
            bool ok = size == (int)sizeof(T) && *(const unsigned*)data == magic;
            if (ok) memcpy(&value, data, sizeof(T));
            UnloadFileData(data);
            return ok;
        }

        // Settings has no magic field of its own, so it is wrapped.
        struct SettingsFile { unsigned magic; Settings settings; };
    }

    void setDir(const std::string& dir) {
        g_dir = dir;
        if (!g_dir.empty()) std::filesystem::create_directories(g_dir);
    }

    std::string path(const char* file) { return g_dir.empty() ? file : g_dir + "/" + file; }

    bool writeRun(const RunSave& run) {
        static RunSave copy;               // a Dungeon is large; keep it off the stack
        copy = run; copy.magic = RUN_MAGIC;
        return writePod("savegame.dat", copy);
    }

    bool readRun(RunSave& run) { return readPod("savegame.dat", run, RUN_MAGIC); }

    bool hasRun() {
        static RunSave probe;
        return readRun(probe);
    }

    bool writeProfile(const Profile& p) {
        Profile copy = p; copy.magic = PROFILE_MAGIC;
        return writePod("profile.dat", copy);
    }

    Profile readProfile() {
        Profile p;
        if (!readPod("profile.dat", p, PROFILE_MAGIC)) p = Profile{};
        return p;
    }

    bool writeSettings(const Settings& s) { return writePod("settings.dat", SettingsFile{ SETTINGS_MAGIC, s }); }

    Settings readSettings() {
        SettingsFile f{};
        return readPod("settings.dat", f, SETTINGS_MAGIC) ? f.settings : Settings{};
    }

} // namespace save
