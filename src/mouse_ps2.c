#include "ibm.h"
#include "keyboard_at.h"
#include "mouse.h"
#include "mouse_ps2.h"
#include "plat-mouse.h"

int mouse_scan = 0;

enum
{
        MOUSE_STREAM,
        MOUSE_REMOTE,
        MOUSE_ECHO
};

#define MOUSE_ENABLE 0x20
#define MOUSE_SCALE  0x10

typedef struct mouse_ps2_t
{
        int mode;
        
        uint8_t flags;
        uint8_t resolution;
        uint8_t sample_rate;
        
        uint8_t command;
        
        int cd;
        
        int x, y, b;
} mouse_ps2_t;

void mouse_ps2_write(uint8_t val, void *p)
{
        mouse_ps2_t *mouse = (mouse_ps2_t *)p;
        
        if (mouse->cd)
        {
                mouse->cd = 0;
                switch (mouse->command)
                {
                        case 0xe8: /*Set mouse resolution*/
                        mouse->resolution = val;
                        keyboard_at_adddata_mouse(0xfa);
                        break;
                        
                        case 0xf3: /*Set sample rate*/
                        mouse->sample_rate = val;
                        keyboard_at_adddata_mouse(0xfa);
                        break;
                        
//                        default:
//                        fatal("mouse_ps2 : Bad data write %02X for command %02X\n", val, mouse->command);
                }
        }
        else
        {
                uint8_t temp;
                
                mouse->command = val;
                switch (mouse->command)
                {
                        case 0xe6: /*Set scaling to 1:1*/
                        mouse->flags &= ~MOUSE_SCALE;
                        keyboard_at_adddata_mouse(0xfa);
                        break;

                        case 0xe7: /*Set scaling to 2:1*/
                        mouse->flags |= MOUSE_SCALE;
                        keyboard_at_adddata_mouse(0xfa);
                        break;
                        
                        case 0xe8: /*Set mouse resolution*/
                        mouse->cd = 1;
                        keyboard_at_adddata_mouse(0xfa);
                        break;
                        
                        case 0xe9: /*Status request*/
                        keyboard_at_adddata_mouse(0xfa);
                        temp = mouse->flags;
                        if (mouse_buttons & 1)
                                temp |= 1;
                        if (mouse_buttons & 2)
                                temp |= 2;
                        if (mouse_buttons & 4)
                                temp |= 3;
                        keyboard_at_adddata_mouse(temp);
                        keyboard_at_adddata_mouse(mouse->resolution);
                        keyboard_at_adddata_mouse(mouse->sample_rate);
                        break;
                        
                        case 0xf2: /*Read ID*/
                        keyboard_at_adddata_mouse(0xfa);
                        keyboard_at_adddata_mouse(0x00);
                        break;
                        
                        case 0xf3: /*Set sample rate*/
                        mouse->cd = 1;
                        keyboard_at_adddata_mouse(0xfa);
                        break;

                        case 0xf4: /*Enable*/
                        mouse->flags |= MOUSE_ENABLE;
                        keyboard_at_adddata_mouse(0xfa);
                        break;
                                                
                        case 0xf5: /*Disable*/
                        mouse->flags &= ~MOUSE_ENABLE;
                        keyboard_at_adddata_mouse(0xfa);
                        break;
                        
                        case 0xff: /*Reset*/
                        mouse->mode  = MOUSE_STREAM;
                        mouse->flags = 0;
                        keyboard_at_adddata_mouse(0xfa);
                        keyboard_at_adddata_mouse(0xaa);
                        keyboard_at_adddata_mouse(0x00);
                        break;
                        
//                        default:
//                        fatal("mouse_ps2 : Bad command %02X\n", val, mouse->command);
                }
        }
}

void mouse_ps2_poll(int x, int y, int b, void *p)
{
        mouse_ps2_t *mouse = (mouse_ps2_t *)p;
        uint8_t packet[3] = {0x08, 0, 0};
        
        if (!x && !y && b == mouse->b)
                return;        

        if (!mouse_scan)
                return;
                
        mouse->x += x;
        mouse->y -= y;        
        if (mouse->mode == MOUSE_STREAM && (mouse->flags & MOUSE_ENABLE) &&
            ((mouse_queue_end - mouse_queue_start) & 0xf) < 13)
        {
                mouse->b = b;
               // pclog("Send packet : %i %i\n", ps2_x, ps2_y);
                if (mouse->x > 255)
                   mouse->x = 255;
                if (mouse->x < -256)
                   mouse->x = -256;
                if (mouse->y > 255)
                   mouse->y = 255;
                if (mouse->y < -256)
                   mouse->y = -256;
                if (mouse->x < 0)
                   packet[0] |= 0x10;
                if (mouse->y < 0)
                   packet[0] |= 0x20;
                if (mouse_buttons & 1)
                   packet[0] |= 1;
                if (mouse_buttons & 2)
                   packet[0] |= 2;
                if (mouse_buttons & 4)
                   packet[0] |= 4;
                packet[1] = mouse->x & 0xff;
                packet[2] = mouse->y & 0xff;
                
                mouse->x = mouse->y = 0;
                
                keyboard_at_adddata_mouse(packet[0]);
                keyboard_at_adddata_mouse(packet[1]);
                keyboard_at_adddata_mouse(packet[2]);
        }
}

void *mouse_ps2_init()
{
        mouse_ps2_t *mouse = (mouse_ps2_t *)malloc(sizeof(mouse_ps2_t));
        memset(mouse, 0, sizeof(mouse_ps2_t));
        
//        mouse_poll  = mouse_ps2_poll;
//        mouse_write = mouse_ps2_write;
        mouse->cd = 0;
        mouse->flags = 0;
        mouse->mode = MOUSE_STREAM;
        
        keyboard_at_set_mouse(mouse_ps2_write, mouse);
        
        return mouse;
}

void mouse_ps2_close(void *p)
{
        mouse_ps2_t *mouse = (mouse_ps2_t *)p;
        
        free(mouse);
}

mouse_t mouse_ps2_2_button =
{
        "2-button mouse (PS/2)",
        mouse_ps2_init,
        mouse_ps2_close,
        mouse_ps2_poll,
        MOUSE_TYPE_PS2
};
