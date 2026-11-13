#include <string.h>
#include "ibm.h"
#include "mouse.h"
#include "amstrad.h"
#include "mouse_msystems.h"
#include "mouse_ps2.h"
#include "mouse_serial.h"
#include "keyboard_olim24.h"

PCEM_API extern mouse_t *mouse_list[MOUSE_MAX];

void mouse_init_builtin() {
        pcem_add_mouse(&mouse_serial_microsoft);
        pcem_add_mouse(&mouse_serial_msystems);
        pcem_add_mouse(&mouse_ps2_2_button);
        pcem_add_mouse(&mouse_intellimouse);
        pcem_add_mouse(&mouse_amstrad);
        pcem_add_mouse(&mouse_olim24);
}

static mouse_t *cur_mouse;
static void *mouse_p;
int mouse_type = 0;

static mouse_t *mouse_get(int mouse) {
        if (mouse < 0 || mouse >= MOUSE_MAX || !mouse_list[mouse])
                return mouse_list[0];
        return mouse_list[mouse];
}

void mouse_emu_init() {
        cur_mouse = mouse_get(mouse_type);
        mouse_p = cur_mouse->init();
}

void mouse_emu_close() {
        if (cur_mouse)
                cur_mouse->close(mouse_p);
        cur_mouse = NULL;
}

void mouse_poll(int x, int y, int z, int b) {
        if (cur_mouse)
                cur_mouse->poll(x, y, z, b, mouse_p);
}

char *mouse_get_name(int mouse) {
        if (mouse < 0 || mouse >= MOUSE_MAX || !mouse_list[mouse])
                return NULL;
        return mouse_list[mouse]->name;
}
int mouse_get_from_name(char *s) {
        int c;

        for (c = 0; c < MOUSE_MAX && mouse_list[c]; c++) {
                if (!strcmp(mouse_list[c]->name, s))
                        return c;
        }
        return -1;
}

int mouse_get_type(int mouse) { return mouse_get(mouse)->type; }
