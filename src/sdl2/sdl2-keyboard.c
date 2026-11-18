#include "plat-keyboard.h"
#include "plat-mouse.h"
#include <string.h>

uint8_t pcem_key[272];
int rawinputkey[272];

void keyboard_init() { memset(pcem_key, 0, sizeof(pcem_key)); }

void keyboard_close() {}

static uint8_t hotkey_mask[272];
static const int hotkey_keys[] = {0x1d, 0x9d, 0x38, 0xb8, 0xd1, 0x51, 0xc9, 0x49, 0xcf, 0x4f, 0xdb, 0xdc};

static int key_down(int c) { return rawinputkey[c] > 0; }

static int hotkey_active() {
        int ctrl = key_down(0x1d) || key_down(0x9d);
        int alt = key_down(0x38) || key_down(0xb8);

        if (ctrl && alt && (key_down(0xd1) || key_down(0x51) || key_down(0xc9) || key_down(0x49)))
                return 1;
        if (!mousecapture)
                return 0;
#ifdef __APPLE__
        return ctrl && (key_down(0xdb) || key_down(0xdc));
#else
        return ctrl && (key_down(0xcf) || key_down(0x4f));
#endif
}

void keyboard_poll_host() {
        int c;

        if (hotkey_active()) {
                for (c = 0; c < (int)(sizeof(hotkey_keys) / sizeof(hotkey_keys[0])); ++c)
                        if (key_down(hotkey_keys[c]))
                                hotkey_mask[hotkey_keys[c]] = 1;
        }

        for (c = 0; c < 272; ++c) {
                if (!key_down(c))
                        hotkey_mask[c] = 0;
                pcem_key[c] = key_down(c) && !hotkey_mask[c];
        }
}
