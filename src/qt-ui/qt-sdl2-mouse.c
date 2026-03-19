#include <SDL2/SDL.h>
#include <string.h>
#include "plat-mouse.h"

extern int mousecapture;

int mouse_buttons = 0;
static int mouse_x = 0, mouse_y = 0, mouse_z = 0;

int mouse[3] = {0};

/* Accumulated relative motion from Qt mouse events */
static volatile int qt_mouse_dx = 0, qt_mouse_dy = 0;
static volatile int qt_mouse_buttons = 0;

void qt_mouse_motion(int dx, int dy) {
        qt_mouse_dx += dx;
        qt_mouse_dy += dy;
}

void qt_mouse_set_buttons(int buttons) {
        qt_mouse_buttons = buttons;
}

void mouse_init() { memset(mouse, 0, sizeof(mouse)); }

void mouse_close() {}

void mouse_wheel_update(int dir) { mouse[2] += dir; }

void mouse_poll_host() {
        if (mousecapture) {
                mouse[0] = qt_mouse_dx;
                mouse[1] = qt_mouse_dy;
                qt_mouse_dx = 0;
                qt_mouse_dy = 0;
                mouse_buttons = qt_mouse_buttons;
                mouse_x += mouse[0];
                mouse_y += mouse[1];
                mouse_z += mouse[2];
                mouse[2] = 0;
        } else {
                mouse_x = mouse_y = mouse_z = mouse_buttons = 0;
        }
}

void mouse_get_mickeys(int *x, int *y, int *z) {
        *x = mouse_x;
        *y = mouse_y;
        *z = mouse_z;
        mouse_x = mouse_y = mouse_z = 0;
}
