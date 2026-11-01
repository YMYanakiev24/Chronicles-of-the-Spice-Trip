// ============================================================================
//  Save.cpp — binary save files.
// ============================================================================
#include "Save.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <vector>

namespace save {

    namespace {
        constexpr unsigned RUN_MAGIC = 0x53504336;       // "SPC6": five weapons, packs, permanent vapes
        constexpr unsigned PROFILE_MAGIC = 0x53505031;   // "SPP1"
        constexpr unsigned SETTINGS_MAGIC = 0x53505331;  // "SPS1"
        constexpr unsigned TOUCH_MAGIC = 0x53505431;     // "SPT1"

        std::string g_dir;

        template <typename T>
        bool writePod(const char* file, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            std::ofstream out(path(file), std::ios::binary);
            out.write((const char*)&value, sizeof value);
            return out.good();
        }

        template <typename T>
        bool readPod(const char* file, T& value, unsigned magic) {
            static_assert(std::is_trivially_copyable_v<T>);
            std::ifstream in(path(file), std::ios::binary | std::ios::ate);
            if (!in || (size_t)in.tellg() != sizeof(T)) return false;
            std::vector<char> data(sizeof(T));
            in.seekg(0);
            in.read(data.data(), sizeof(T));
            bool ok = in.good() && *(const unsigned*)data.data() == magic;
            if (ok) memcpy((void*)&value, data.data(), sizeof(T));
            return ok;
        }

        // Settings has no magic field of its own, so it is wrapped.
        struct SettingsFile { unsigned magic; Settings settings; };
        struct TouchFile { unsigned magic; TouchLayout layout; };
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

    bool writeTouchLayout(const TouchLayout& t) { return writePod("touch.dat", TouchFile{ TOUCH_MAGIC, t }); }

    TouchLayout readTouchLayout() {
        TouchFile f{};
        return readPod("touch.dat", f, TOUCH_MAGIC) ? f.layout : TouchLayout{};
    }

} // namespace save
