// ============================================================================
//  Autopilot.h вЂ” plays the game with scripted input, checks every system,
//  saves screenshots and writes autopilot.log. Run with --autopilot <dir>.
// ============================================================================
#pragma once

#include "raylib.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct Game;

class Autopilot {
public:
    // coop: "" for the whole game alone; "host", or "join" with the host's address, for one side of a
    // co-op game (run a host and a joiner at once: they play a run together and check each other).
    void start(Game& g, const std::string& dir, const std::string& coop = "", const std::string& address = "");
    void step(Game& g);                 // call before each frame
    int  failures() const { return fails; }

private:
    // A step runs every frame until it returns true (or times out).
    struct Step { std::string name; float timeout; std::function<bool(Game&, float)> run; };
    std::vector<Step> steps;
    size_t current = 0;
    float  stepT = 0;
    bool   keepAlive = false;           // top the player up so long checks can't end in death
    int    passes = 0, fails = 0, shotCount = 0;
    float  markF = 0;                   // scratch values a step remembers between frames
    float  markX = 0, markY = 0;
    bool   markB = false, markC = false;
    int    markWeapon = 0;
    Vector2 markP{};
    std::string dir;
    FILE*  log = nullptr;

    void build();
    void buildCoopHost();
    void buildCoopJoin(const std::string& address);
    void buildLook();
    void check(const char* what, bool ok);
    void shot(Game& g, const char* name);
    void finish(Game& g);
};
