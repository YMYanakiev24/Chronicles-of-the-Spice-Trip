// ============================================================================
//  Save.cpp — binary save files.
// ============================================================================
#include "Save.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <vector>

namespace save {

    namespace {
        constexpr unsigned RUN_MAGIC = 0x53504337;       // "SPC7": difficulty, hardcore, special rooms, the belt, seven weapons
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
        // grows: the struct gained fields at its end since older files were written; those keep their defaults
        bool readPod(const char* file, T& value, unsigned magic, bool grows = false) {
            static_assert(std::is_trivially_copyable_v<T>);
            std::ifstream in(path(file), std::ios::binary | std::ios::ate);
            size_t size = in ? (size_t)in.tellg() : 0;
            if (!in || (grows ? size < sizeof(unsigned) || size > sizeof(T) : size != sizeof(T))) return false;
            std::vector<char> data(size);
            in.seekg(0);
            in.read(data.data(), size);
            bool ok = in.good() && *(const unsigned*)data.data() == magic;
            if (ok) memcpy((void*)&value, data.data(), size);
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

    bool deleteRun() { return std::remove(path("savegame.dat").c_str()) == 0; }

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
        if (!readPod("profile.dat", p, PROFILE_MAGIC, true)) p = Profile{};
        return p;
    }

    bool writeSettings(const Settings& s) { return writePod("settings.dat", SettingsFile{ SETTINGS_MAGIC, s }); }

    Settings readSettings() {
        SettingsFile f{};
        return readPod("settings.dat", f, SETTINGS_MAGIC, true) ? f.settings : Settings{};
    }

    bool writeTouchLayout(const TouchLayout& t) { return writePod("touch.dat", TouchFile{ TOUCH_MAGIC, t }); }

    TouchLayout readTouchLayout() {
        TouchFile f{};
        if (readPod("touch.dat", f, TOUCH_MAGIC)) return f.layout;
        // a layout from before the movable hotbar: six controls, the bar where it always was
        struct OldTouch { unsigned magic; bool custom; float x[6], y[6], r[6]; } old{};
        if (!readPod("touch.dat", old, TOUCH_MAGIC)) return TouchLayout{};
        TouchLayout t;
        t.custom = old.custom;
        for (int i = 0; i < 6; ++i) { t.x[i] = old.x[i]; t.y[i] = old.y[i]; t.r[i] = old.r[i]; }
        t.x[TC_BAR] = -1;
        return t;
    }

} // namespace save
