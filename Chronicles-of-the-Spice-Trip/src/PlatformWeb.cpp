// ============================================================================
//  PlatformWeb.cpp — the browser build: canvas size, fingers, saves, buzz.
//  Built with Emscripten by web/CMakeLists.txt; the page around it is
//  web/shell.html and the save folder's setup is web/pre.js.
// ============================================================================
#ifdef PLATFORM_WEB
#include "Platform.h"
#include <emscripten/emscripten.h>

// The page (web/shell.html) works out the canvas size from the browser window and keeps it
// in Module.spiceW / spiceH. Without the page, the desktop's 1280 x 720.
EM_JS(int, web_target_w, (), { return Module.spiceW | 0 || 1280; });
EM_JS(int, web_target_h, (), { return Module.spiceH | 0 || 720; });

// Resizes the canvas and tells raylib. This goes round GLFW's own resizing (SetWindowSize),
// which takes a size equal to the screen's as a request for fullscreen, and in fullscreen
// draws at the screen's size instead of the size asked for.
EM_JS(void, web_set_canvas_size, (int w, int h), {
    var c = Module.canvas;
    c.width = w;
    c.height = h;
    if (typeof GLFW !== 'undefined' && GLFW.active) {
        GLFW.active.width = w;
        GLFW.active.height = h;
        GLFW.onWindowSizeChanged();          // raylib's WindowSizeCallback: screen size, viewport
        GLFW.onFramebufferSizeChanged();
    }
});

// A phone or tablet: its main pointer is a finger. ?touch=1 or ?touch=0 in the address overrides it.
EM_JS(int, web_touch_screen, (), {
    var q = new URLSearchParams(location.search).get('touch');
    if (q === '1') return 1;
    if (q === '0') return 0;
    return (matchMedia('(pointer: coarse)').matches || matchMedia('(hover: none)').matches) ? 1 : 0;
});

// raylib's own touch handling loses track of which fingers are still down when one of
// several is lifted, so the fingers on the canvas are followed here, straight from the browser.
EM_JS(void, web_watch_fingers, (), {
    var canvas = Module.canvas;
    Module.spiceFingers = [];
    function update(e) {
        var r = canvas.getBoundingClientRect();
        var sx = canvas.width / (r.width || 1), sy = canvas.height / (r.height || 1);
        var list = [];
        for (var i = 0; i < e.touches.length; ++i) {
            var t = e.touches[i];
            if (t.target !== canvas) continue;
            list.push([t.identifier, (t.clientX - r.left) * sx, (t.clientY - r.top) * sy]);
        }
        Module.spiceFingers = list;
        if (list.length) Module.spiceSawFinger = true;
    }
    ['touchstart', 'touchmove', 'touchend', 'touchcancel'].forEach(function(type) {
        canvas.addEventListener(type, update, { passive: true });
    });
    window.addEventListener('blur', function() { Module.spiceFingers = []; });
});
EM_JS(int, web_saw_finger, (), { return Module.spiceSawFinger ? 1 : 0; });
EM_JS(int, web_finger_count, (), { return Module.spiceFingers ? Module.spiceFingers.length : 0; });
EM_JS(int, web_finger_id, (int i), { return Module.spiceFingers[i][0]; });
EM_JS(float, web_finger_x, (int i), { return Module.spiceFingers[i][1]; });
EM_JS(float, web_finger_y, (int i), { return Module.spiceFingers[i][2]; });

EM_JS(void, web_persist, (), { if (Module.spicePersist) Module.spicePersist(); });
EM_JS(void, web_vibrate, (int ms), { if (navigator.vibrate) navigator.vibrate(ms); });
EM_JS(int, web_is_fullscreen, (), { return document.fullscreenElement || document.webkitFullscreenElement ? 1 : 0; });
EM_JS(void, web_set_fullscreen, (int on), {
    var el = document.documentElement;
    if (on) {
        var go = el.requestFullscreen || el.webkitRequestFullscreen;
        if (go) { var p = go.call(el); if (p && p.catch) p.catch(function() {}); }
    }
    else if (document.fullscreenElement || document.webkitFullscreenElement) {
        var leave = document.exitFullscreen || document.webkitExitFullscreen;
        if (leave) leave.call(document);
    }
});

namespace platform {

    void windowSize(int& w, int& h) {
        static bool watching = false;
        if (!watching) { web_watch_fingers(); watching = true; }
        w = web_target_w();
        h = web_target_h();
    }

    void fitWindow() {
        int w = web_target_w(), h = web_target_h();
        if (w != GetScreenWidth() || h != GetScreenHeight()) web_set_canvas_size(w, h);
    }

    bool sawFinger() { return web_saw_finger() != 0; }

    bool touchScreen() {
        static int touch = -1;
        if (touch < 0) touch = web_touch_screen();
        return touch != 0;
    }

    void touches(std::vector<Vector2>& pos, std::vector<int>& ids) {
        int n = web_finger_count();
        for (int i = 0; i < n; ++i) {
            pos.push_back({ web_finger_x(i), web_finger_y(i) });
            ids.push_back(web_finger_id(i));
        }
    }

    void persist() { web_persist(); }
    void vibrate(int ms) { web_vibrate(ms); }
    bool browserFullscreen() { return web_is_fullscreen() != 0; }
    void setBrowserFullscreen(bool on) { web_set_fullscreen(on ? 1 : 0); }

} // namespace platform
#endif
