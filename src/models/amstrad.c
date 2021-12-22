#include <stdlib.h>
#include "ibm.h"
#include "io.h"
#include "keyboard.h"
#include "lpt.h"
#include "mouse.h"
#include "video.h"
#include "fdc.h"
#include "x86.h"

#include "amstrad.h"

static uint8_t amstrad_dead;
int amstrad_latch = 0;
static uint8_t amstrad_language;

uint8_t amstrad_read(uint16_t port, void *priv)
{
        uint8_t temp;
        
        pclog("amstrad_read : %04X\n",port);
        switch (port)
        {
                case 0x378:
                return lpt1_read(port, NULL);
                case 0x379:
                return lpt1_read(port, NULL) | (amstrad_language & 7);
                case 0x37a:
                temp = lpt1_read(port, NULL) & 0x1f;
                if (romset == ROM_PC1512) return temp | 0x20;
                if (romset == ROM_PC200 || romset == ROM_PPC512)
                {
                        if (fdc_discchange_read())
                                temp |= 0x20;
/* DIP switches 4,5: Initial video mode */
                        if (video_is_cga())
                                temp |= 0x80;	/* CGA 80 */
                        else if (video_is_mda())
                                temp |= 0xc0;	/* MDA */
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
                if (romset == ROM_PC200 || romset == ROM_PPC512)
		{
/* Read DIP switches / internal display adapter status. This code is only
 * reached if the IDA is disabled; if it is enabled this read will be
 * handled by the video card */
		}
                return 0x20; /* Disable IDA */
                case 0xdead:
                return amstrad_dead;
        }
        return 0xff;
}

void amstrad_write(uint16_t port, uint8_t val, void *priv)
{
        switch (port)
        {
                case 0x66:
                softresetx86();
                break;
                
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
        io_sethandler(0x0066, 0x0001, NULL,               NULL, NULL, amstrad_write,       NULL, NULL,  NULL);
        io_sethandler(0x0378, 0x0003, amstrad_read,       NULL, NULL, amstrad_write,       NULL, NULL,  NULL);
        io_sethandler(0xdead, 0x0001, amstrad_read,       NULL, NULL, amstrad_write,       NULL, NULL,  NULL);
        if ((romset == ROM_PC200 || romset == ROM_PPC512) && gfxcard != GFX_BUILTIN)        
                io_sethandler(0x03de, 0x0001, amstrad_read,       NULL, NULL, amstrad_write,       NULL, NULL,  NULL);

	
}

static void *ams1512_init()
{
	amstrad_language = device_get_config_int("language");
	return &amstrad_language;
}

device_config_t ams1512_config[] = 
{
	{
                .name = "language",
                .description = "BIOS language",
                .type = CONFIG_SELECTION,
		.selection =
		{
			{
				.description = "English",
				.value = 7
			},
			{
				.description = "German",
				.value = 6
			},
			{
				.description = "French",
				.value = 5
			},
			{
				.description = "Spanish",
				.value = 4
			},
			{
				.description = "Danish",
				.value = 3
			},
			{
				.description = "Swedish",
				.value = 2
			},
			{
				.description = "Italian",
				.value = 1
			},
			{
				.description = "Diagnostic mode",
				.value = 0
			},
			{
				.description = ""
			}
		},
                .default_int = 7
	},
        {
                .type = -1
        }
};
	

device_config_t ams2086_config[] = 
{
	{
                .name = "language",
                .description = "BIOS language",
                .type = CONFIG_SELECTION,
		.selection =
		{
			{
				.description = "English",
				.value = 7
			},
			{
				.description = "Diagnostic mode",
				.value = 0
			},
			{
				.description = ""
			}
		},
                .default_int = 7
	},
        {
                .type = -1
        }
};
		


device_config_t ams3086_config[] = 
{
	{
                .name = "language",
                .description = "BIOS language",
                .type = CONFIG_SELECTION,
		.selection =
		{
			{
				.description = "English",
				.value = 7
			},
			{
				.description = "Diagnostic mode",
				.value = 3
			},
			{
				.description = ""
			}
		},
                .default_int = 7
	},
        {
                .type = -1
        }
};
	

	

device_t ams1512_device =
{
        "Amstrad PC1512 (BIOS)",
        0,
        ams1512_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        ams1512_config
};



device_t ams2086_device =
{
        "Amstrad PC2086 (BIOS)",
        0,
        ams1512_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        ams2086_config
};


device_t ams3086_device =
{
        "Amstrad PC3086 (BIOS)",
        0,
        ams1512_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        ams3086_config
};
