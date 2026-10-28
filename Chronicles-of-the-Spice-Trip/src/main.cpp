// ============================================================================
//  Chronicles of the Spice Trip — raylib edition
// ----------------------------------------------------------------------------
//  Entry point. The game itself lives in Game.h; see README.md for the layout.
//
//    --autopilot <dir>   play through every system with scripted input, save
//                        screenshots and a PASS/FAIL log in <dir>, then quit
//    --savedir <dir>     keep saves somewhere other than the working directory
// ============================================================================
#include "Game.h"
#include "Autopilot.h"
#include "Input.h"
#include <cstring>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    std::string autopilotDir, saveDir;
    for (int i = 1; i + 1 < argc; ++i) {
        if (!strcmp(argv[i], "--autopilot")) autopilotDir = argv[++i];
        else if (!strcmp(argv[i], "--savedir")) saveDir = argv[++i];
    }
    bool autopilot = !autopilotDir.empty();
    if (autopilot) {
        // The autopilot checks first-time things, so it always starts from an empty save of its own.
        if (saveDir.empty()) saveDir = autopilotDir + "/save";
        std::filesystem::remove_all(saveDir);
        std::filesystem::create_directories(autopilotDir);
    }

    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
    if (autopilot) flags |= FLAG_WINDOW_UNFOCUSED;      // don't steal focus while it plays
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(SCREEN_W, SCREEN_H, "Chronicles of the Spice Trip");
    SetWindowMinSize(960, 540);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);                               // Esc is handled by the game
    InitAudioDevice();
    if (autopilot) SetMasterVolume(0);
    save::setDir(saveDir);

    Game game;
    game.init(autopilot);
    Autopilot pilot;
    if (autopilot) pilot.start(game, autopilotDir);

    while (!WindowShouldClose() && !game.quit) {
        if (autopilot) pilot.step(game);                // this frame's scripted input
        game.frame();
    }

    game.shutdown();
    CloseAudioDevice();
    CloseWindow();
    return autopilot ? pilot.failures() : 0;
}
