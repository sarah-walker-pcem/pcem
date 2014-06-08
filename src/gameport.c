#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "plat-joystick.h"
#include "timer.h"

#include "gameport.h"

typedef struct gameport_axis_t
{
        int count;
        int axis_nr;
        struct gameport_t *gameport;
} gameport_axis_t;
        
typedef struct gameport_t
{
        uint8_t state;
        
        gameport_axis_t axis[4];
} gameport_t;

static int gameport_time(int axis)
{
        axis += 32768;
        axis = (axis * 100) / 65; /*Axis now in ohms*/
        axis = (axis * 11) / 1000;
        return TIMER_USEC * (axis + 24); /*max = 11.115 ms*/
}

void gameport_write(uint16_t addr, uint8_t val, void *p)
{
        gameport_t *gameport = (gameport_t *)p;

        gameport->state |= 0x0f;
//        pclog("gameport_write : joysticks_present=%i\n", joysticks_present);
        if (joysticks_present)
        {
                gameport->axis[0].count = gameport_time(joystick_state[0].x);
                gameport->axis[1].count = gameport_time(joystick_state[0].y);
//                pclog("gameport_write: axis[0]=%i,%i axis[1]=%i,%i\n", joystick_state[0].x, gameport->axis[0].count, joystick_state[0].y, gameport->axis[1].count);
        }
        if (joysticks_present >= 2)
        {
                gameport->axis[2].count = gameport_time(joystick_state[1].x);
                gameport->axis[3].count = gameport_time(joystick_state[1].y);
//                pclog("gameport_write: axis[2]=%i,%i axis[3]=%i,%i\n", joystick_state[1].x, gameport->axis[2].count, joystick_state[1].y, gameport->axis[3].count);
        }
}

uint8_t gameport_read(uint16_t addr, void *p)
{
        gameport_t *gameport = (gameport_t *)p;
        uint8_t ret;
        
        ret = gameport->state | 0xf0;
        
        if (joysticks_present)
        {
                if (joystick_state[0].b[0])
                        ret &= ~0x10;
                if (joystick_state[0].b[1])
                        ret &= ~0x20;
                if (joystick_state[0].b[2])
                        ret &= ~0x40;
                if (joystick_state[0].b[3])
                        ret &= ~0x80;
        }
        if (joysticks_present >= 2)
        {
                if (joystick_state[1].b[0])
                        ret &= ~0x40;
                if (joystick_state[1].b[1])
                        ret &= ~0x80;
        }

//        pclog("gameport_read: ret=%02x %08x:%08x\n", ret, cs, pc);
        return ret;
}

void gameport_timer_over(void *p)
{
        gameport_axis_t *axis = (gameport_axis_t *)p;
        gameport_t *gameport = axis->gameport;
        
        gameport->state &= ~(1 << axis->axis_nr);
        axis->count = 0;
        
//        pclog("gameport_timer_over : axis_nr=%i\n", axis->axis_nr);
}

void *gameport_init()
{
        gameport_t *gameport = malloc(sizeof(gameport_t));
        
        memset(gameport, 0, sizeof(gameport_t));
        
        gameport->axis[0].gameport = gameport;
        gameport->axis[1].gameport = gameport;
        gameport->axis[2].gameport = gameport;
        gameport->axis[3].gameport = gameport;

        gameport->axis[0].axis_nr = 0;
        gameport->axis[1].axis_nr = 1;
        gameport->axis[2].axis_nr = 2;
        gameport->axis[3].axis_nr = 3;
        
        timer_add(gameport_timer_over, &gameport->axis[0].count, &gameport->axis[0].count, &gameport->axis[0]);
        timer_add(gameport_timer_over, &gameport->axis[1].count, &gameport->axis[1].count, &gameport->axis[1]);
        timer_add(gameport_timer_over, &gameport->axis[2].count, &gameport->axis[2].count, &gameport->axis[2]);
        timer_add(gameport_timer_over, &gameport->axis[3].count, &gameport->axis[3].count, &gameport->axis[3]);
        
        io_sethandler(0x0200, 0x0008, gameport_read, NULL, NULL, gameport_write, NULL, NULL, gameport);
        
        return gameport;
}

void gameport_close(void *p)
{
        gameport_t *gameport = (gameport_t *)p;

        free(gameport);
}

device_t gameport_device =
{
        "Game port",
        0,
        gameport_init,
        gameport_close,
        NULL,
        NULL,
        NULL,
        NULL
};
