#include <stdlib.h>
#include "ibm.h"
#include "io.h"
#include "keyboard.h"
#include "lpt.h"
#include "mouse.h"
#include "video.h"

#include "amstrad.h"

static uint8_t amstrad_dead;
int amstrad_latch = 0;

uint8_t amstrad_read(uint16_t port, void *priv)
{
        uint8_t temp;
        
        pclog("amstrad_read : %04X\n",port);
        switch (port)
        {
                case 0x378:
                return lpt1_read(port, NULL);
                case 0x379:
                return lpt1_read(port, NULL) | 7;
                case 0x37a:
                temp = lpt1_read(port, NULL) & 0x1f;
                if (romset == ROM_PC1512) return temp | 0x20;
                if (romset == ROM_PC200)
                {
                        if (video_is_cga())
                                temp |= 0x80;
                        else if (video_is_mda())
                                temp |= 0xc0;
                        return temp;
                }
                if (romset == ROM_PC1640)
                {
                        if (video_is_cga())
                                temp |= 0x80;                        
                        else if (video_is_mda())
                                temp |= 0xc0;

                        switch (amstrad_latch)
                        {
                                case AMSTRAD_NOLATCH:
                                temp &= ~0x20;
                                break;
                                case AMSTRAD_SW9:
                                temp &= ~0x20;
                                break;
                                case AMSTRAD_SW10:
                                temp |= 0x20;
                                break;
                        }
                }
                return temp;
                case 0x3de:
                return 0x20; /*PC200 - use external video. If internal video is being used
                               then this port is handled in vid_pc200.c*/
                case 0xdead:
                return amstrad_dead;
        }
        return 0xff;
}

void amstrad_write(uint16_t port, uint8_t val, void *priv)
{
        switch (port)
        {
                case 0x378:
                case 0x379:
                case 0x37a:
                lpt1_write(port, val, NULL);
                break;
                
                case 0xdead:
                amstrad_dead = val;
                break;
        }
}

static uint8_t mousex, mousey;
static void amstrad_mouse_write(uint16_t addr, uint8_t val, void *p)
{
//        pclog("Write mouse %04X %02X %04X:%04X\n", addr, val, CS, pc);
        if (addr == 0x78)
                mousex = 0;
        else
                mousey = 0;
}

static uint8_t amstrad_mouse_read(uint16_t addr, void *p)
{
//        printf("Read mouse %04X %04X:%04X %02X\n", addr, CS, pc, (addr == 0x78) ? mousex : mousey);
        if (addr == 0x78)
                return mousex;
        return mousey;
}

typedef struct mouse_amstrad_t
{
        int oldb;
} mouse_amstrad_t;

static void mouse_amstrad_poll(int x, int y, int z, int b, void *p)
{
        mouse_amstrad_t *mouse = (mouse_amstrad_t *)p;
        
        mousex += x;
        mousey -= y;

        if ((b & 1) && !(mouse->oldb & 1))
                keyboard_send(0x7e);
        if ((b & 2) && !(mouse->oldb & 2))
                keyboard_send(0x7d);
        if (!(b & 1) && (mouse->oldb & 1))
                keyboard_send(0xfe);
        if (!(b & 2) && (mouse->oldb & 2))
                keyboard_send(0xfd);
        
        mouse->oldb = b;
}

static void *mouse_amstrad_init()
{
        mouse_amstrad_t *mouse = (mouse_amstrad_t *)malloc(sizeof(mouse_amstrad_t));
        memset(mouse, 0, sizeof(mouse_amstrad_t));
                
        return mouse;
}

static void mouse_amstrad_close(void *p)
{
        mouse_amstrad_t *mouse = (mouse_amstrad_t *)p;
        
        free(mouse);
}

mouse_t mouse_amstrad =
{
        "Amstrad mouse",
        mouse_amstrad_init,
        mouse_amstrad_close,
        mouse_amstrad_poll,
        MOUSE_TYPE_AMSTRAD
};

void amstrad_init()
{
        lpt2_remove_ams();
        
        io_sethandler(0x0078, 0x0001, amstrad_mouse_read, NULL, NULL, amstrad_mouse_write, NULL, NULL,  NULL);
        io_sethandler(0x007a, 0x0001, amstrad_mouse_read, NULL, NULL, amstrad_mouse_write, NULL, NULL,  NULL);
        io_sethandler(0x0378, 0x0003, amstrad_read,       NULL, NULL, amstrad_write,       NULL, NULL,  NULL);
        io_sethandler(0xdead, 0x0001, amstrad_read,       NULL, NULL, amstrad_write,       NULL, NULL,  NULL);
        if (romset == ROM_PC200 && gfxcard != GFX_BUILTIN)        
                io_sethandler(0x03de, 0x0001, amstrad_read,       NULL, NULL, amstrad_write,       NULL, NULL,  NULL);
}
