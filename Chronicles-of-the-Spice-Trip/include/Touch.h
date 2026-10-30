// ============================================================================
//  Touch.h — on-screen stick and buttons for phones.
//  Fingers are turned into the same keys and mouse buttons the desktop game
//  reads through in::, so the gameplay code doesn't know the difference.
//  Run the desktop build with --touch to try it with the mouse.
// ============================================================================
#pragma once

#include "Settings.h"

namespace touch {

    void setEnabled(bool on);
    bool enabled();

    void update(bool playing, const Keybinds& keys);    // once per frame, before the game reads input
    void draw(const Keybinds& keys);

    void zone(Rectangle r, int key);    // tapping r presses key (HUD slots register themselves each frame)
    Vector2 stickDir();                 // normalised direction of the stick, {0,0} when idle

} // namespace touch
