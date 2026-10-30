// ============================================================================
//  Input.h — every key and mouse read goes through here.
//  Normally these forward to raylib. The autopilot switches them to scripted
//  input so it can play the game with the same code paths a player uses.
// ============================================================================
#pragma once

#include "raylib.h"

namespace in {

    bool    down(int key);              // held this frame
    bool    pressed(int key);           // went down this frame
    int     nextKey();                  // queued key press (for rebinding), 0 if none
    bool    mouseDown(int button);
    bool    mousePressed(int button);
    Vector2 mouse();                    // screen position

    // --- on-screen controls (Touch.cpp), rebuilt every frame ---
    void clearVirtual();
    void virtualHold(int key, bool on);
    void virtualTap(int key);
    void virtualMouse(int button, bool on);
    void setCaptured(bool on);          // fingers belong to the controls, not the raw mouse

    // --- scripted input (autopilot) ---
    void setScripted(bool on);
    bool scripted();
    void hold(int key, bool on);        // keep a key held until released
    void tap(int key);                  // press a key for exactly one frame
    void holdMouse(int button, bool on);
    void click(int button);             // press a mouse button for one frame
    void moveMouse(Vector2 p);
    void releaseAll();
    void endFrame();                    // clears this frame's taps

} // namespace in
