#include <stdlib.h>
#include "ibm.h"
#include "mouse.h"
#include "pic.h"
#include "serial.h"
#include "timer.h"

typedef struct mouse_msystems_t
{
        int mousepos, mousedelay;
        int oldb;
        SERIAL *serial;
} mouse_msystems_t;

static void mouse_msystems_poll(int x, int y, int z, int b, void *p)
{
        mouse_msystems_t *mouse = (mouse_msystems_t *)p;
        SERIAL *serial = mouse->serial;        
        uint8_t mousedat[5];

        if (!x && !y && b == mouse->oldb)
                return;

        mouse->oldb = b;
        y = -y;
        if (x > 127)
                x = 127;
        if (y > 127)
                y = 127;
        if (x < -128)
                x = -128;
        if (y < -128)
                y = -128;

        /*Use Mouse Systems format*/
        mousedat[0] = 0x87;
        if (b & 1)
                mousedat[0] &= ~0x04;
        if (b & 2)
                mousedat[0] &= ~0x01;
        if (b & 4)
                mousedat[0] &= ~0x02;

        mousedat[1] = x;
        mousedat[2] = y;
        mousedat[3] = 0;
        mousedat[4] = 0;
        
        if (!(serial->mctrl & 0x10))
        {
//                pclog("Serial data %02X %02X %02X\n", mousedat[0], mousedat[1], mousedat[2]);
                serial_write_fifo(mouse->serial, mousedat[0]);
                serial_write_fifo(mouse->serial, mousedat[1]);
                serial_write_fifo(mouse->serial, mousedat[2]);
                serial_write_fifo(mouse->serial, mousedat[3]);
                serial_write_fifo(mouse->serial, mousedat[4]);
        }
}

static void *mouse_msystems_init()
{
        mouse_msystems_t *mouse = (mouse_msystems_t *)malloc(sizeof(mouse_msystems_t));
        memset(mouse, 0, sizeof(mouse_msystems_t));

        mouse->serial = &serial1;
        serial1.rcr_callback = NULL;
        serial1.rcr_callback_p = NULL;
        
        return mouse;
}

static void mouse_msystems_close(void *p)
{
        mouse_msystems_t *mouse = (mouse_msystems_t *)p;
        
        free(mouse);
        
        serial1.rcr_callback = NULL;
}

mouse_t mouse_serial_msystems =
{
        "Mouse Systems 3-button mouse (serial)",
        mouse_msystems_init,
        mouse_msystems_close,
        mouse_msystems_poll,
        MOUSE_TYPE_SERIAL | MOUSE_TYPE_3BUTTON
};
