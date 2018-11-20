#include "plat-keyboard.h"
#include <string.h>

uint8_t pcem_key[272];
int rawinputkey[272];

void keyboard_init()
{
        memset(pcem_key, 0, sizeof(pcem_key));
}

void keyboard_close()
{
}

void keyboard_poll_host()
{
        int c;

        for (c = 0; c < 272; ++c)
                pcem_key[c] = rawinputkey[c] > 0;
}

