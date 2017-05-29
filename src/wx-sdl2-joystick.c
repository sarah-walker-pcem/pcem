#include "plat-joystick.h"

int joysticks_present;
joystick_t joystick_state[MAX_JOYSTICKS];

void joystick_init()
{
        joysticks_present = 0;
}
void joystick_close()
{
}
void joystick_poll()
{
}
