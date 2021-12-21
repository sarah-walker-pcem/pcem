/*Plantronics ColorPlus emulation*/
#include <stdlib.h>
#include <math.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "timer.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_colorplus.h"

typedef struct colorplus_t
{
	cga_t cga;
        uint8_t control;
} colorplus_t;

/* Bits in the colorplus control register: */
#define COLORPLUS_PLANE_SWAP	0x40	/* Swap planes at 0000h and 4000h */
#define COLORPLUS_640x200_MODE	0x20	/* 640x200x4 mode active */
#define COLORPLUS_320x200_MODE	0x10	/* 320x200x16 mode active */
#define COLORPLUS_EITHER_MODE   0x30	/* Either mode active */

/* Bits in the CGA graphics mode register */
#define CGA_GRAPHICS_MODE       0x02	/* CGA graphics mode selected? */

void colorplus_out(uint16_t addr, uint8_t val, void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;

//        pclog("COLORPLUS_OUT %04X %02X\n", addr, val);
        if (addr == 0x3DD)
        {
                colorplus->control = val & 0x70;
	}
        else
        {
                cga_out(addr, val, &colorplus->cga);
	}
}

uint8_t colorplus_in(uint16_t addr, void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;

        return cga_in(addr, &colorplus->cga);
}

void colorplus_write(uint32_t addr, uint8_t val, void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;

//        pclog("COLORPLUS_WRITE %04X %02X\n", addr, val);
	if ((colorplus->control & COLORPLUS_PLANE_SWAP) &&
	    (colorplus->control & COLORPLUS_EITHER_MODE) &&
	    (colorplus->cga.cgamode & CGA_GRAPHICS_MODE))
	{
		addr ^= 0x4000;
	}
	else if (!(colorplus->control & COLORPLUS_EITHER_MODE))
	{
		addr &= 0x3FFF;
	}
        colorplus->cga.vram[addr & 0x7fff] = val;
        egawrites++;
        cycles -= 4;
}

uint8_t colorplus_read(uint32_t addr, void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;

	if ((colorplus->control & COLORPLUS_PLANE_SWAP) &&
	    (colorplus->control & COLORPLUS_EITHER_MODE) &&
	    (colorplus->cga.cgamode & CGA_GRAPHICS_MODE))
	{
		addr ^= 0x4000;
	}	
	else if (!(colorplus->control & COLORPLUS_EITHER_MODE))
	{
		addr &= 0x3FFF;
	}
        cycles -= 4;        
        egareads++;
//        pclog("COLORPLUS_READ %04X\n", addr);
        return colorplus->cga.vram[addr & 0x7fff];
}

void colorplus_recalctimings(colorplus_t *colorplus)
{
	cga_recalctimings(&colorplus->cga);
}

void colorplus_poll(void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;
        int x, c;
        int oldvc;
        uint16_t dat0, dat1;
        int cols[4];
        int col;
        int oldsc;
	static const int cols16[16] = { 0x0,0x2,0x4,0x6,
					0x8,0xA,0xC,0xE,
					0x1,0x3,0x5,0x7,
					0x9,0xB,0xD,0xF };
	uint8_t *plane0 = colorplus->cga.vram;
	uint8_t *plane1 = colorplus->cga.vram + 0x4000;

	/* If one of the extra modes is not selected, drop down to the CGA
	 * drawing code. */
	if (!((colorplus->control & COLORPLUS_EITHER_MODE) &&
	      (colorplus->cga.cgamode & CGA_GRAPHICS_MODE)))
	{
		cga_poll(&colorplus->cga);
		return;
	}

        if (!colorplus->cga.linepos)
        {
                timer_advance_u64(&colorplus->cga.timer, colorplus->cga.dispofftime);
                colorplus->cga.cgastat |= 1;
                colorplus->cga.linepos = 1;
                oldsc = colorplus->cga.sc;
                if ((colorplus->cga.crtc[8] & 3) == 3) 
                   colorplus->cga.sc = ((colorplus->cga.sc << 1) + colorplus->cga.oddeven) & 7;
                if (colorplus->cga.cgadispon)
                {
                        if (colorplus->cga.displine < colorplus->cga.firstline)
                        {
                                colorplus->cga.firstline = colorplus->cga.displine;
                                video_wait_for_buffer();
//                                printf("Firstline %i\n",firstline);
                        }
                        colorplus->cga.lastline = colorplus->cga.displine;
			/* Left / right border */
                        for (c = 0; c < 8; c++)
                        {
                                ((uint32_t *)buffer32->line[colorplus->cga.displine])[c] = 
                                ((uint32_t *)buffer32->line[colorplus->cga.displine])[c + (colorplus->cga.crtc[1] << 4) + 8] = cgapal[colorplus->cga.cgacol & 15];
                        }
			if (colorplus->control & COLORPLUS_320x200_MODE)
			{
                                for (x = 0; x < colorplus->cga.crtc[1]; x++)
                                {
                                        dat0 = (plane0[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000)] << 8) |
                                                plane0[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000) + 1];
                                        dat1 = (plane1[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000)] << 8) |
                                                plane1[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000) + 1];
                                        colorplus->cga.ma++;
                                        for (c = 0; c < 8; c++)
                                        {
                                                ((uint32_t *)buffer32->line[colorplus->cga.displine])[(x << 4) + (c << 1) + 8] =
                                                ((uint32_t *)buffer32->line[colorplus->cga.displine])[(x << 4) + (c << 1) + 1 + 8] = 
                                                  cgapal[cols16[(dat0 >> 14) | ((dat1 >> 14) << 2)]];
                                                dat0 <<= 2;
                                                dat1 <<= 2;
                                        }
                                }
			}
			else if (colorplus->control & COLORPLUS_640x200_MODE)
                        {
                                cols[0] = cgapal[colorplus->cga.cgacol & 15];
                                col = (colorplus->cga.cgacol & 16) ? 8 : 0;
                                if (colorplus->cga.cgamode & 4)
                                {
                                        cols[1] = cgapal[col | 3];
                                        cols[2] = cgapal[col | 4];
                                        cols[3] = cgapal[col | 7];
                                }
                                else if (colorplus->cga.cgacol & 32)
                                {
                                        cols[1] = cgapal[col | 3];
                                        cols[2] = cgapal[col | 5];
                                        cols[3] = cgapal[col | 7];
                                }
                                else
                                {
                                        cols[1] = cgapal[col | 2];
                                        cols[2] = cgapal[col | 4];
                                        cols[3] = cgapal[col | 6];
                                }
                                for (x = 0; x < colorplus->cga.crtc[1]; x++)
                                {
                                        dat0 = (plane0[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000)] << 8) |
                                                plane0[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000) + 1];
                                        dat1 = (plane1[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000)] << 8) |
                                                plane1[((colorplus->cga.ma << 1) & 0x1fff) + ((colorplus->cga.sc & 1) * 0x2000) + 1];
                                        colorplus->cga.ma++;
                                        for (c = 0; c < 16; c++)
                                        {
                                                ((uint32_t *)buffer32->line[colorplus->cga.displine])[(x << 4) + c + 8] =
                                                  cols[(dat0 >> 15) | ((dat1 >> 15) << 1)];
                                                dat0 <<= 1;
                                                dat1 <<= 1;
                                        }
                                }
                        }
                }
                else	/* Top / bottom border */
                {
                        cols[0] = cgapal[colorplus->cga.cgacol & 15];
                        hline(buffer32, 0, colorplus->cga.displine, (colorplus->cga.crtc[1] << 4) + 16, cols[0]);
                }

                x = (colorplus->cga.crtc[1] << 4) + 16;

                colorplus->cga.sc = oldsc;
                if (colorplus->cga.vc == colorplus->cga.crtc[7] && !colorplus->cga.sc)
                   colorplus->cga.cgastat |= 8;
                colorplus->cga.displine++;
                if (colorplus->cga.displine >= 360) 
                        colorplus->cga.displine = 0;
        }
        else
        {
                timer_advance_u64(&colorplus->cga.timer, colorplus->cga.dispontime);
                colorplus->cga.linepos = 0;
                if (colorplus->cga.vsynctime)
                {
                        colorplus->cga.vsynctime--;
                        if (!colorplus->cga.vsynctime)
                           colorplus->cga.cgastat &= ~8;
                }
                if (colorplus->cga.sc == (colorplus->cga.crtc[11] & 31) || ((colorplus->cga.crtc[8] & 3) == 3 && colorplus->cga.sc == ((colorplus->cga.crtc[11] & 31) >> 1))) 
                { 
                        colorplus->cga.con = 0; 
                        colorplus->cga.coff = 1; 
                }
                if ((colorplus->cga.crtc[8] & 3) == 3 && colorplus->cga.sc == (colorplus->cga.crtc[9] >> 1))
                   colorplus->cga.maback = colorplus->cga.ma;
                if (colorplus->cga.vadj)
                {
                        colorplus->cga.sc++;
                        colorplus->cga.sc &= 31;
                        colorplus->cga.ma = colorplus->cga.maback;
                        colorplus->cga.vadj--;
                        if (!colorplus->cga.vadj)
                        {
                                colorplus->cga.cgadispon = 1;
                                colorplus->cga.ma = colorplus->cga.maback = (colorplus->cga.crtc[13] | (colorplus->cga.crtc[12] << 8)) & 0x3fff;
                                colorplus->cga.sc = 0;
                        }
                }
                else if (colorplus->cga.sc == colorplus->cga.crtc[9])
                {
                        colorplus->cga.maback = colorplus->cga.ma;
                        colorplus->cga.sc = 0;
                        oldvc = colorplus->cga.vc;
                        colorplus->cga.vc++;
                        colorplus->cga.vc &= 127;

                        if (colorplus->cga.vc == colorplus->cga.crtc[6]) 
                                colorplus->cga.cgadispon = 0;

                        if (oldvc == colorplus->cga.crtc[4])
                        {
                                colorplus->cga.vc = 0;
                                colorplus->cga.vadj = colorplus->cga.crtc[5];
                                if (!colorplus->cga.vadj) colorplus->cga.cgadispon = 1;
                                if (!colorplus->cga.vadj) colorplus->cga.ma = colorplus->cga.maback = (colorplus->cga.crtc[13] | (colorplus->cga.crtc[12] << 8)) & 0x3fff;
                                if ((colorplus->cga.crtc[10] & 0x60) == 0x20) colorplus->cga.cursoron = 0;
                                else                                colorplus->cga.cursoron = colorplus->cga.cgablink & 8;
                        }

                        if (colorplus->cga.vc == colorplus->cga.crtc[7])
                        {
                                colorplus->cga.cgadispon = 0;
                                colorplus->cga.displine = 0;
                                colorplus->cga.vsynctime = 16;
                                if (colorplus->cga.crtc[7])
                                {
                                        if (colorplus->cga.cgamode & 1) x = (colorplus->cga.crtc[1] << 3) + 16;
                                        else                  x = (colorplus->cga.crtc[1] << 4) + 16;
                                        colorplus->cga.lastline++;
                                        if (x != xsize || (colorplus->cga.lastline - colorplus->cga.firstline) != ysize)
                                        {
                                                xsize = x;
                                                ysize = colorplus->cga.lastline - colorplus->cga.firstline;
                                                if (xsize < 64) xsize = 656;
                                                if (ysize < 32) ysize = 200;
                                                updatewindowsize(xsize, (ysize << 1) + 16);
                                        }
                                        
                                        video_blit_memtoscreen(0, colorplus->cga.firstline - 4, 0, (colorplus->cga.lastline - colorplus->cga.firstline) + 8, xsize, (colorplus->cga.lastline - colorplus->cga.firstline) + 8);
                                        frames++;

                                        video_res_x = xsize - 16;
                                        video_res_y = ysize;
                                        if (colorplus->cga.cgamode & 1)
                                        {
                                                video_res_x /= 8;
                                                video_res_y /= colorplus->cga.crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(colorplus->cga.cgamode & 2))
                                        {
                                                video_res_x /= 16;
                                                video_res_y /= colorplus->cga.crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(colorplus->cga.cgamode & 16))
                                        {
                                                video_res_x /= 2;
                                                video_bpp = 2;
                                        }
                                        else
                                        {
                                                video_bpp = 1;
                                        }
                                }
                                colorplus->cga.firstline = 1000;
                                colorplus->cga.lastline = 0;
                                colorplus->cga.cgablink++;
                                colorplus->cga.oddeven ^= 1;
                        }
                }
                else
                {
                        colorplus->cga.sc++;
                        colorplus->cga.sc &= 31;
                        colorplus->cga.ma = colorplus->cga.maback;
                }
                if (colorplus->cga.cgadispon)
                        colorplus->cga.cgastat &= ~1;
                if ((colorplus->cga.sc == (colorplus->cga.crtc[10] & 31) || ((colorplus->cga.crtc[8] & 3) == 3 && colorplus->cga.sc == ((colorplus->cga.crtc[10] & 31) >> 1)))) 
                        colorplus->cga.con = 1;
                if (colorplus->cga.cgadispon && (colorplus->cga.cgamode & 1))
                {
                        for (x = 0; x < (colorplus->cga.crtc[1] << 1); x++)
                            colorplus->cga.charbuffer[x] = colorplus->cga.vram[(((colorplus->cga.ma << 1) + x) & 0x3fff)];
                }
        }
}

void *colorplus_init()
{
        int display_type, contrast;
        colorplus_t *colorplus = malloc(sizeof(colorplus_t));
        memset(colorplus, 0, sizeof(colorplus_t));

        colorplus->cga.vram = malloc(0x8000);
        colorplus->cga.composite = colorplus->cga.snow_enabled = 0;

        display_type = device_get_config_int("display_type");
        contrast = device_get_config_int("contrast");
                
        timer_add(&colorplus->cga.timer, colorplus_poll, colorplus, 1);
        mem_mapping_add(&colorplus->cga.mapping, 0xb8000, 0x08000, colorplus_read, NULL, NULL, colorplus_write, NULL, NULL,  NULL, 0, colorplus);
        io_sethandler(0x03d0, 0x0010, colorplus_in, NULL, NULL, colorplus_out, NULL, NULL, colorplus);
        
        cgapal_rebuild(display_type, contrast);
		
        return colorplus;
}

void colorplus_close(void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;

        free(colorplus->cga.vram);
        free(colorplus);
}

void colorplus_speed_changed(void *p)
{
        colorplus_t *colorplus = (colorplus_t *)p;
        
        cga_recalctimings(&colorplus->cga);
}

device_config_t colorplus_config[] =
{
        {
                .name = "display_type",
                .description = "Display type",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "RGB",
                                .value = DISPLAY_RGB
                        },
                        {
                                .description = "RGB (no brown)",
                                .value = DISPLAY_RGB_NO_BROWN
                        },
                        {
                                .description = "Green Monochrome",
                                .value = DISPLAY_GREEN
                        },
                        {
                                .description = "Amber Monochrome",
                                .value = DISPLAY_AMBER
                        },
                        {
                                .description = "White Monochrome",
                                .value = DISPLAY_WHITE
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = DISPLAY_RGB
        },
        {
                .name = "contrast",
                .description = "Alternate monochrome contrast",
                .type = CONFIG_BINARY,
                .default_int = 0
        },
        {
                .type = -1
        }
};

device_t colorplus_device =
{
        "Colorplus",
        0,
        colorplus_init,
        colorplus_close,
        NULL,
        colorplus_speed_changed,
        NULL,
        NULL,
        colorplus_config
};
