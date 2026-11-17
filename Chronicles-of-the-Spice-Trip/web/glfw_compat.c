// raylib 5.5 asks the browser's GLFW to report changes in the screen's pixel scale (a GLFW 3.3
// call). The Emscripten that builds this (Ubuntu's 3.1.6) has GLFW 3.2 without it. The game
// doesn't use high-DPI mode, so the call may do nothing.
#include <stddef.h>

void* glfwSetWindowContentScaleCallback(void* window, void* callback) {
    (void)window; (void)callback;
    return NULL;
}
