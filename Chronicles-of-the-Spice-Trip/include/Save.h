// ============================================================================
//  Save.h — the run save, the profile (records) and the settings file.
//  Each is a plain struct written to disk with a magic number in front, so a
//  file from an older version is simply ignored.
// ============================================================================
#pragma once

#include "Entities.h"
#include "Rooms.h"
#include "Settings.h"
#include <string>

// Everything needed to resume a run. All members are trivially copyable.
struct RunSave {
    unsigned magic = 0;
    Player   player;
    Dungeon  dungeon;
    int      floor = 1, kills = 0;
    float    runTime = 0;
};

// Records kept between runs.
struct Profile {
    unsigned magic = 0;
    int      champion = 0;          // last champion picked
    int      deepestFloor = 0;
    int      runsCompleted = 0;     // times the Cinder-Maw fell
    unsigned bossesBeaten = 0;      // bit per BOSSES entry
    float    bestRush = 0;          // best Boss Rush time, 0 = none yet
};

namespace save {

    void setDir(const std::string& dir);     // where the files live ("" = working directory)
    std::string path(const char* file);

    bool writeRun(const RunSave& run);
    bool readRun(RunSave& run);
    bool hasRun();

    bool writeProfile(const Profile& p);
    Profile readProfile();

    bool writeSettings(const Settings& s);
    Settings readSettings();

    bool writeTouchLayout(const TouchLayout& t);
    TouchLayout readTouchLayout();

} // namespace save
