#include <SDL2/SDL.h>
#include <string.h>
#include "plat-mouse.h"

int mouse_buttons;
static int mouse_x = 0, mouse_y = 0, mouse_z = 0;

int mouse[3];

void mouse_init()
{
        memset(mouse, 0, sizeof(mouse));
}

void mouse_close()
{
}

void mouse_wheel_update(int dir)
{
        mouse[2] += dir;
}

void mouse_poll_host()
{
        if (mousecapture)
        {
                uint32_t mb = SDL_GetRelativeMouseState(&mouse[0], &mouse[1]);
                mouse_buttons = 0;
                if (mb & SDL_BUTTON(SDL_BUTTON_LEFT))
                {
                        mouse_buttons |= 1;
                }
                if (mb & SDL_BUTTON(SDL_BUTTON_RIGHT))
                {
                        mouse_buttons |= 2;
                }
                if (mb & SDL_BUTTON(SDL_BUTTON_MIDDLE))
                {
                        mouse_buttons |= 4;
                }
                mouse_x += mouse[0];
                mouse_y += mouse[1];
                mouse_z += mouse[2];
                mouse[2] = 0;
        }
        else
        {
                mouse_x = mouse_y = mouse_z = mouse_buttons = 0;
        }
}

void mouse_get_mickeys(int *x, int *y, int *z)
{
        *x = mouse_x;
        *y = mouse_y;
        *z = mouse_z;
        mouse_x = mouse_y = mouse_z = 0;
}

