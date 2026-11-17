// ============================================================================
//  Chronicles of the Spice Trip — raylib edition
// ----------------------------------------------------------------------------
//  Entry point. The game itself lives in Game.h; see README.md for the layout.
//
//    --autopilot <dir>   play through every system with scripted input, save
//                        screenshots and a PASS/FAIL log in <dir>, then quit
//    --savedir <dir>     keep saves somewhere other than the working directory
//    --touch             show the phone controls (the mouse plays the fingers)
//
//  In a browser (PLATFORM_WEB) the page runs the frames: see web/ and PlatformWeb.cpp.
// ============================================================================
#include "Game.h"
#include "Autopilot.h"
#include "Input.h"
#include "Platform.h"
#include "Touch.h"
#include <cstring>
#include <filesystem>
#include <string>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
// A browser runs the frames itself, one per screen refresh, after main has returned.
static Game* g_webGame = nullptr;
static void webFrame() {
    platform::fitWindow();
    g_webGame->frame();
}
#endif

static int run(int argc, char** argv) {
    std::string autopilotDir, saveDir;
    for (int i = 1; i + 1 < argc; ++i) {
        if (!strcmp(argv[i], "--autopilot")) autopilotDir = argv[++i];
        else if (!strcmp(argv[i], "--savedir")) saveDir = argv[++i];
    }
    for (int i = 1; i < argc; ++i) if (!strcmp(argv[i], "--touch")) touch::setEnabled(true);
#ifdef PLATFORM_ANDROID
    touch::setEnabled(true);
    saveDir = platform::dataDir();
    int windowW, windowH;
    platform::windowSize(windowW, windowH);
#elif defined(PLATFORM_WEB)
    if (platform::touchScreen()) touch::setEnabled(true);
    saveDir = "/save";                                  // kept in the browser's storage (web/pre.js)
    int windowW, windowH;
    platform::windowSize(windowW, windowH);
#else
    int windowW = SCREEN_W, windowH = SCREEN_H;
#endif
    bool autopilot = !autopilotDir.empty();
    if (autopilot) {
        // The autopilot checks first-time things, so it always starts from an empty save of its own.
        if (saveDir.empty()) saveDir = autopilotDir + "/save";
        std::filesystem::remove_all(saveDir);
        std::filesystem::create_directories(autopilotDir);
    }

    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
#ifdef PLATFORM_WEB
    flags &= ~FLAG_WINDOW_RESIZABLE;                    // the page sizes the canvas (platform::fitWindow)
#endif
    if (autopilot) flags |= FLAG_WINDOW_UNFOCUSED;      // don't steal focus while it plays
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(windowW, windowH, "Chronicles of the Spice Trip");
    SetWindowMinSize(960, 540);
#ifndef PLATFORM_WEB
    SetTargetFPS(60);                                   // a browser paces the frames to the screen
#endif
    SetExitKey(KEY_NULL);                               // Esc is handled by the game
    InitAudioDevice();
    if (autopilot) SetMasterVolume(0);
    save::setDir(saveDir);

#ifdef PLATFORM_WEB
    // main returns before the game is played, so the game can't live on its stack.
    Game& game = *new Game;
    game.init(false);
    g_webGame = &game;
    emscripten_set_main_loop(webFrame, 0, 1);           // doesn't come back
    return 0;
#else
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
#endif
}

#ifdef PLATFORM_WEB
int main() { return run(0, nullptr); }              // a page has no command line
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
