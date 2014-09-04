#include "allegro-main.h"
#include "plat-joystick.h"

joystick_t joystick_state[2];
int joysticks_present;

void joystick_init()
{
        install_joystick(JOY_TYPE_AUTODETECT);
        joysticks_present = MIN(num_joysticks, 2);
}
void joystick_close()
{
}
void joystick_poll()
{
        int c;
        
        poll_joystick();

        for (c = 0; c < MIN(num_joysticks, 2); c++)
        {                
                joystick_state[c].x = joy[c].stick[0].axis[0].pos * 256;
                joystick_state[c].y = joy[c].stick[0].axis[1].pos * 256;
                joystick_state[c].b[0] = joy[c].button[0].b;
                joystick_state[c].b[1] = joy[c].button[1].b;
                joystick_state[c].b[2] = joy[c].button[2].b;
                joystick_state[c].b[3] = joy[c].button[3].b;
        }
}
