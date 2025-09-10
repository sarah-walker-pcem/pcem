
#include <stdlib.h>
#include <math.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "timer.h"
#include "video.h"
#include "vid_v6355.h"
#include "dosbox/vid_cga_comp.h"

/* Emulation of the Yamaha V6355 chipset. This is a CGA clone that was 
 * probably designed primarily for laptops, where the primary display was
 * a fixed-resolution LCD panel and there was the option of connecting to
 * an external CGA monitor or PAL/SECAM television.
 *
 * Consequently, unlike a real CGA, it doesn't implement the first ten 6845
 * registers; instead, a small number of fixed resolutions can be selected
 * using the V6355's own registers at 0x3DD / 0x3DF. Width is either 512 or 640
 * pixels; height is 64, 192, 200 or 204 pixels.
 *
 * Other features include:
 * - MDA attribute support
 * - Palette support - mapping from RGBI colour to 9-bit rrrgggbbb colours 
 *  (when output is to composite)
 * - Hardware mouse pointer support
 *
 * Outline of the V6355's extra registers, accessed through ports 0x3DD (index)
 * and 0x3DE (data):
 *
 * 0x00-0x1F: Mouse pointer AND mask
 * 0x20-0x3F: Mouse pointer XOR mask
 * 0x40-0x5F: Palette for composite output. 0r,gb,0r,gb,0r,gb etc.
 * 0x60-0x61: Mouse pointer X (big-endian, in 320x200 coordinates)
 * 0x62:      Not used (would be high byte of mouse pointer Y)
 * 0x63:      Mouse pointer Y
 * 0x64:      Mouse pointer visibility & vertical adjustment
 * 0x65:      Screen height & width, display type, RAM type
 * 0x66:      LCD adjust, MDA attribute emulation
 * 0x67:      Horizontal adjustment, other configuration
 * 0x68:      Mouse pointer colour
 * 0x69:      Control data register (not well documented)
 *
 * Currently unimplemented:
 * > Display type (PAL/SECAM @50Hz vs NTSC @60Hz)
 * > MDA monitor support
 * > LCD panel support 
 * > Horizontal / vertical position adjustments
 * > 160x200x16 and 640x200x16 video modes. Documentation suggests that these 
 *   should be selected by setting bit 6 of the CGA control register, but 
 *   that doesn't work on my real hardware, so I can't test and therefore
 *   can't replicate
 * > Palette support on composite output. Composite_Process() does not 
 *   appear to have any support for an arbitrary palette.
 */

#define COMPOSITE_OLD 0
#define COMPOSITE_NEW 1

static uint8_t crtcmask[32] = {0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f, 0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
                               0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


static uint8_t mdamap[256][2][2];


/* Default values for palette registers */
static uint8_t defpalette[32] = 
{
        0x00, 0x00,     /* Black */
        0x00, 0x04,     /* Blue */
        0x00, 0x40,     /* Green */
        0x00, 0x44,     /* Cyan */
        0x04, 0x00,     /* Red */
        0x04, 0x04,     /* Magenta */
        0x04, 0x40,     /* Yellow */
        0x04, 0x44,     /* Light grey */
        0x01, 0x11,     /* Dark grey */
        0x00, 0x06,     /* Bright blue */
        0x00, 0x60,     /* Bright green */
        0x00, 0x66,     /* Bright cyan */
        0x06, 0x00,     /* Bright red */
        0x06, 0x06,     /* Bright magenta */
        0x06, 0x60,     /* Bright yellow */
        0x07, 0x77,     /* Bright white */
};

void v6355_recalctimings(v6355_t *v6355);



void v6355_out(uint16_t addr, uint8_t val, void *p) {
        v6355_t *v6355 = (v6355_t *)p;
        uint8_t old;
        //        pclog("CGA_OUT %04X %02X\n", addr, val);
        switch (addr) {
        case 0x3d0:
        case 0x3d2:
        case 0x3d4:
        case 0x3d6:
                v6355->crtcreg = val & 31;
                return;
        case 0x3d1:
        case 0x3d3:
        case 0x3d5:
        case 0x3d7:
                old = v6355->crtc[v6355->crtcreg];
                v6355->crtc[v6355->crtcreg] = val & crtcmask[v6355->crtcreg];
                if (old != val) {
                        if (v6355->crtcreg < 0xe || v6355->crtcreg > 0x10) {
                                fullchange = changeframecount;
                                v6355_recalctimings(v6355);
                        }
                }
                return;
        case 0x3D8:
                if (((v6355->cgamode ^ val) & 5) != 0) {
                        v6355->cgamode = val;
                        update_cga16_color(v6355->cgamode);
                }
                v6355->cgamode = val;
                return;
        case 0x3D9:
                v6355->cgacol = val;
                return;
	case 0x3DD:
		v6355->v6355reg = val;
		return;
	case 0x3DE:
		v6355->v6355data[v6355->v6355reg] = val;

		/* Writes in the 0x40-0x5F range update the palette */
		if (v6355->v6355reg >= 0x40 && v6355->v6355reg < 0x60)
		{
			int r = (v6355->v6355data[v6355->v6355reg & 0xFE]) & 7;
			int g = (v6355->v6355data[v6355->v6355reg | 0x01] >> 4) & 7;
			int b = (v6355->v6355data[v6355->v6355reg | 0x01]) & 7;
			v6355->v6355pal[(v6355->v6355reg - 0x40) / 2] = 
				makecol(r * 0xFF / 7, g * 0xFF / 7, b * 0xFF / 7);
		}

		/* Register autoincrements after a write. */
		v6355->v6355reg = (v6355->v6355reg + 1) % sizeof(v6355->v6355data);
		return;
	case 0x3DF:
		/* Supposedly used for memory paging in 16-colour mode, but
		 * I've found no documentation to explain how */
		return;
        }
}

uint8_t v6355_in(uint16_t addr, void *p) {
        v6355_t *v6355 = (v6355_t *)p;
        //        pclog("CGA_IN %04X\n", addr);
        switch (addr) {
        case 0x3D4:
                return v6355->crtcreg;
        case 0x3D5:
                return v6355->crtc[v6355->crtcreg];
        case 0x3DA:
                return v6355->cgastat;
        }
        return 0xFF;
}


void v6355_write(uint32_t addr, uint8_t val, void *p) {
        v6355_t *v6355 = (v6355_t *)p;
        //        pclog("CGA_WRITE %04X %02X\n", addr, val);
        v6355->vram[addr & 0x3fff] = val;
        egawrites++;
        cycles -= 4;
}

uint8_t v6355_read(uint32_t addr, void *p) {
        v6355_t *v6355 = (v6355_t *)p;
        cycles -= 4;
        egareads++;
        //        pclog("CGA_READ %04X\n", addr);
        return v6355->vram[addr & 0x3fff];
}

/* Get width of display area (always 512px or 640px) */
unsigned v6355_width(v6355_t *v6355)
{
	return (v6355->v6355data[0x65] & 4) ? 512 : 640;
}

/* Get height of display area (192px, 200px, 204px or 64px) */
unsigned v6355_height(v6355_t *v6355)
{
	static const unsigned heights[] = { 192, 200, 204, 64 };

	return heights[v6355->v6355data[0x65] & 3];
}





/* Timings on a V6355 are largely fixed */
void v6355_recalctimings(v6355_t *v6355) 
{
        double disptime;
        double _dispontime, _dispofftime;

	unsigned w = v6355_width(v6355);
	unsigned h = v6355_height(v6355);
	unsigned xadj = v6355->v6355data[0x67] & 0x1F;	/* Horizontal adjust */

        pclog("Recalc - %i %i %i\n", w + 32, w, v6355->cgamode & 1);
        disptime = w + 33;
        _dispontime = w;
        _dispofftime = disptime - _dispontime;
        //        printf("%i %f %f %f  %i %i\n",cgamode&1,disptime,dispontime,dispofftime,crtc[0],crtc[1]);
        _dispontime *= CGACONST;
        _dispofftime *= CGACONST;
        //        printf("Timings - on %f off %f frame %f second
        //        %f\n",dispontime,dispofftime,(dispontime+dispofftime)*262.0,(dispontime+dispofftime)*262.0*59.92);
        v6355->dispontime = (uint64_t)_dispontime;
        v6355->dispofftime = (uint64_t)_dispofftime;
}


/* Overlay the pointer on a line of the display. pixel[] is an array of 640
 * IBGR values containing the pixels to draw onto */
static void v6355_pointer(v6355_t *v6355, uint8_t *pixel)
{
	int c, pxc;
	int y = v6355->displine - v6355->firstline;

	/* The pointer coordinates are on a 336x216 grid, with (16,16) being
	 * the top left-hand corner of the visible area */
	int pointer_x = (v6355->v6355data[0x60] << 8) | (v6355->v6355data[0x61]);
	int pointer_y = v6355->v6355data[0x63];
	uint8_t px, mc, mand, mxor, mflags;

	/* Mouse drawing options */
	mflags = v6355->v6355data[0x64];

	/* If the pointer is blinking and not currently shown, don't draw it */
	if ((v6355->cgablink & 8) && (mflags & 1)) return;

	/* If this line doesn't intersect the pointer, nothing to do */
	if (y < (pointer_y - 16) || y >= pointer_y) return;

	y -= (pointer_y - 16);

	/* Get mouse AND and XOR masks */
	mand = v6355->v6355data[0x68] & 0x0F;
	mxor = (v6355->v6355data[0x68] >> 4) & 0x0F;

	/* Draw up to 16 double-width pixels */
	for (c = 0; c < 32; c++)
	{
		mc = 0x80 >> ((c & 0x0E) >> 1);
		pxc = c + 2 * (pointer_x - 16);
		if (pxc < 0 || pxc >= 640) continue;	/* X clipping */

		if (mflags & 2)	{ /* Apply AND mask? */
			if (v6355->v6355data[y * 2 + c / 16] & mc) pixel[pxc] &= mand;
		}
		if (mflags & 4)	{ /* Apply XOR mask? */
			if (v6355->v6355data[y * 2 + c / 16 + 32] & mc) pixel[pxc] ^= mxor;
		}
	}	
}


/* Convert attribute byte to CGA colours */
static void v6355_map_attrs(v6355_t *v6355, uint8_t chr, uint8_t attr, uint8_t *cols)
{
	if (v6355->v6355data[0x66] & 0x40) {	
		/* MDA-style attributes */
		int blink = (v6355->cgamode & 0x20) && (v6355->cgablink & 8) && (attr & 0x80);

		cols[1] = mdamap[attr][blink][1];
		cols[0] = mdamap[attr][blink][0];
	} else {
		/* CGA attributes (blinking enabled) */
                if (v6355->cgamode & 0x20) {
			cols[1] = attr & 15;
			cols[0] = (attr >> 4) & 7;
			if ((v6355->cgablink & 8) && (attr & 0x80) && !v6355->drawcursor)
				cols[1] = cols[0];
		} else {
		/* CGA attributes (blinking disabled) */
			cols[1] = attr & 15;
			cols[0] = attr >> 4;
		}
	}
}

/* Render a line as 640 pixels in 80-column text mode */
static void v6355_line_text80(v6355_t *v6355, uint8_t *pixel, uint16_t ca)
{
	int x, c;
	uint8_t chr, attr;
	unsigned w = v6355_width(v6355) / 8;
	uint8_t cols[2];

	for (x = 0; x < w; x++) {
		if (v6355->cgamode & 8) {
			chr  = v6355->charbuffer[x << 1];
			attr = v6355->charbuffer[(x << 1) + 1];
		}
		else	chr = attr = 0;

		v6355_map_attrs(v6355, chr, attr, cols);

		for (c = 0; c < 8; c++) {
			uint8_t data = fontdat[chr + v6355->fontbase][v6355->sc & 7];
			/* Underline attribute if enabled */
			if ((v6355->v6355data[0x66] & 0x80) && ((attr & 7) == 1) && ((v6355->sc & 7) == 7)) data = 0xFF;

			pixel[(x << 3) + c] = cols[(data & (1 << (c ^ 7))) ? 1 : 0];
		}
	}
}


/* Render a line as 640 pixels in 40-column text mode */
static void v6355_line_text40(v6355_t *v6355, uint8_t *pixel, uint16_t ca)
{
	int x, c;
	uint8_t chr, attr;
	unsigned w = v6355_width(v6355) / 16;
	uint8_t cols[2];

	for (x = 0; x < w; x++) {
		if (v6355->cgamode & 8) {
			chr  = v6355->vram[((v6355->ma + x) << 1) & 0x3FFF];
			attr = v6355->vram[(((v6355->ma + x) << 1) + 1) & 0x3FFF];
		}
		else	chr = attr = 0;

		v6355_map_attrs(v6355, chr, attr, cols);
	
		for (c = 0; c < 8; c++) {
			uint8_t data = fontdat[chr + v6355->fontbase][v6355->sc & 7];
			/* Underline attribute if enabled */
			if ((v6355->v6355data[0x66] & 0x80) && ((attr & 7) == 1) && ((v6355->sc & 7) == 7)) data = 0xFF;

			pixel[(x << 4) + (c << 1)] = pixel[(x << 4) + (c << 1) + 1] = 
				cols[(data & (1 << (c ^ 7))) ? 1 : 0];
		}
	}
}

/* Render a line as 640 pixels in 320-pixel graphics mode */
static void v6355_line_graphics320(v6355_t *v6355, uint8_t *pixel)
{
	int x, c;
	uint8_t cols[4];
	uint8_t intensity;
	uint16_t dat;
	unsigned width = v6355_width(v6355) / 16;

	cols[0] = v6355->cgacol & 15;

	intensity = (v6355->cgacol & 16) ? 8 : 0;
	if (v6355->cgamode & 4) {
		cols[1] = intensity | 3;
		cols[2] = intensity | 4;
		cols[3] = intensity | 7;
	} else if (v6355->cgacol & 32) {
		cols[1] = intensity | 3;
		cols[2] = intensity | 5;
		cols[3] = intensity | 7;
	} else {
		cols[1] = intensity | 2;
		cols[2] = intensity | 4;
		cols[3] = intensity | 6;
	}
	for (x = 0; x < width; x++) {
		if (v6355->cgamode & 8)
			dat = (v6355->vram[((v6355->ma << 1) & 0x1fff) + ((v6355->sc & 1) * 0x2000)] << 8) | 
			v6355->vram[((v6355->ma << 1) & 0x1fff) + ((v6355->sc & 1) * 0x2000) + 1];
		else 
			dat = 0;
		v6355->ma++;
		for (c = 0; c < 8; c++) {
			pixel[(x << 4) + (c << 1)] = pixel[(x << 4) + (c << 1) + 1] = cols[dat >> 14];
			dat <<= 2;
		}	
	}
}



/* Render a line as 640 pixels in 640-pixel graphics mode */
static void v6355_line_graphics640(v6355_t *v6355, uint8_t *pixel)
{
	int x, c;
	uint8_t cols[2];
	uint16_t dat;
	unsigned width = v6355_width(v6355) / 16;

	cols[0] = 0;
	cols[1] = v6355->cgacol & 15;

	for (x = 0; x < width; x++) {
		if (v6355->cgamode & 8) 
			dat = (v6355->vram[((v6355->ma << 1) & 0x1fff) + ((v6355->sc & 1) * 0x2000)] << 8) | 
				v6355->vram[((v6355->ma << 1) & 0x1fff) + ((v6355->sc & 1) * 0x2000) + 1];
		else
			dat = 0;
		v6355->ma++;
		for (c = 0; c < 16; c++) {
			pixel[(x << 4) + c] = cols[dat >> 15];
                        dat <<= 1;
		}
	}
}




void v6355_poll(void *p) {
        v6355_t *v6355 = (v6355_t *)p;
        uint16_t ca = (v6355->crtc[15] | (v6355->crtc[14] << 8)) & 0x3fff;
        int drawcursor;
        int x, c;
        int oldvc;
        uint8_t chr, attr;
        uint16_t dat;
        uint32_t cols[4];
        int col;
        int oldsc;
	uint8_t pixel[640];

	int width = v6355_width(v6355);
	int height = v6355_height(v6355);
/* Simulated CRTC height registers */
	int crtc4, crtc5, crtc6, crtc7, crtc8, crtc9;

	if (!(v6355->cgamode & 2)) {
		/* Text mode values */
		crtc4 = (height + 54) / 8;
		crtc5 = 6;
		crtc6 = height / 8;
		crtc7 = (height + 24) / 8;
		crtc8 = 2;
		crtc9 = 7;
	} else {
		/* Graphics mode values */
		crtc4 = (height + 54) / 2;
		crtc5 = 6;
		crtc6 = height / 2;
		crtc7 = (height + 24) / 2;
		crtc8 = 2;
		crtc9 = 1;
	}
        if (!v6355->linepos) {
                timer_advance_u64(&v6355->timer, v6355->dispofftime);
                v6355->cgastat |= 1;
                v6355->linepos = 1;
                oldsc = v6355->sc;
                if ((crtc8 & 3) == 3)
                        v6355->sc = ((v6355->sc << 1) + v6355->oddeven) & 7;
                if (v6355->cgadispon) {
                        if (v6355->displine < v6355->firstline) {
                                v6355->firstline = v6355->displine;
                                video_wait_for_buffer();
                                //                                printf("Firstline %i\n",firstline);
                        }
                        v6355->lastline = v6355->displine;

/* Draw border */
                        cols[0] = ((v6355->cgamode & 0x12) == 0x12) ? 0 : (v6355->cgacol & 15);
                        for (c = 0; c < 8; c++) {
                                ((uint32_t *)buffer32->line[v6355->displine])[c] = cols[0];
                                ((uint32_t *)buffer32->line[v6355->displine])[c + width + 8] = cols[0];
                        }
/* Render screen data. */
                        if (v6355->cgamode & 1) {
/* High-res text */
				v6355_line_text80(v6355, pixel, ca);
				v6355_pointer(v6355, pixel);
				for (x = 0; x < (width / 8); x++) {
                                        drawcursor = ((v6355->ma == ca) && v6355->con && v6355->cursoron);
                                        if (drawcursor) {
                                                for (c = 0; c < 8; c++)
                                                        ((uint32_t *)buffer32->line[v6355->displine])[(x << 3) + c + 8] = pixel[(x << 3) + c] ^ 0xffffff;
                                        } else {
                                                for (c = 0; c < 8; c++)
                                                        ((uint32_t *)buffer32->line[v6355->displine])[(x << 3) + c + 8] = pixel[(x << 3) + c];
                                        }
                                        v6355->ma++;
				}
                        } else if (!(v6355->cgamode & 2)) {
/* Low-res text */
				v6355_line_text40(v6355, pixel, ca);
				v6355_pointer(v6355, pixel);
                                for (x = 0; x < (width / 16); x++) {
                                        drawcursor = ((v6355->ma == ca) && v6355->con && v6355->cursoron);
                                        if (drawcursor) {
                                                for (c = 0; c < 16; c++)
                                                        ((uint32_t *)buffer32->line[v6355->displine])[(x << 4) + c + 8] = pixel[(x << 4) + c] ^ 0xffffff;

                                        } else {
                                                for (c = 0; c < 16; c++)
                                                        ((uint32_t *)buffer32->line[v6355->displine])[(x << 4) + c + 8] = pixel[(x << 4) + c];
                                        }
                                        v6355->ma++;
                                }
                        } else if (!(v6355->cgamode & 16)) {
/* Low-res graphics 
 * XXX There should be a branch for 160x200x16 graphics somewhere around here */
				v6355_line_graphics320(v6355, pixel);
				v6355_pointer(v6355, pixel);
                                for (x = 0; x < (width / 16); x++) {
					for (c = 0; c < 16; c++)
                                                        ((uint32_t *)buffer32->line[v6355->displine])[(x << 4) + c + 8] = pixel[(x << 4) + c];
				}
                        } else {
/* High-res graphics 
 * XXX There should be a branch for 640x200x16 graphics somewhere around here */
				v6355_line_graphics640(v6355, pixel);
				v6355_pointer(v6355, pixel);
                                for (x = 0; x < (width / 16); x++) {
					for (c = 0; c < 16; c++)
                                                        ((uint32_t *)buffer32->line[v6355->displine])[(x << 4) + c + 8] = pixel[(x << 4) + c];
				}
                        }
                } else {
                        cols[0] = ((v6355->cgamode & 0x12) == 0x12) ? 0 : (v6355->cgacol & 15);
                        hline(buffer32, 0, v6355->displine, width + 16, cols[0]);
                }

		x = width + 16;

/* Now render the 640 pixels to the display buffer */
		switch (v6355->display_type) {
/* XXX DISPLAY_COMPOSITE can't use the V6355's palette registers */
			case DISPLAY_COMPOSITE:
                        for (c = 0; c < x; c++)
				buffer32->line[v6355->displine][c] = ((uint32_t *)buffer32->line[v6355->displine])[c] & 0xf;

				Composite_Process(v6355->cgamode, 0, x >> 2, buffer32->line[v6355->displine]);
				break;
			case DISPLAY_TRUECOLOUR:
/* DISPLAY_TRUECOLOUR is a fictitious display that behaves like RGB except it
 * takes account of the V6355's palette registers */
                        	for (c = 0; c < x; c++)
                               		((uint32_t *)buffer32->line[v6355->displine])[c] =
                                        	v6355->v6355pal[((uint32_t *)buffer32->line[v6355->displine])[c] & 0xf];
				break;

			default:
                        	for (c = 0; c < x; c++)
                               		((uint32_t *)buffer32->line[v6355->displine])[c] =
                                        	cgapal[((uint32_t *)buffer32->line[v6355->displine])[c] & 0xf];
				break;
                }

                v6355->sc = oldsc;
                if (v6355->vc == crtc7 && !v6355->sc)
                        v6355->cgastat |= 8;
                v6355->displine++;
                if (v6355->displine >= 360)
                        v6355->displine = 0;
        } else {
                timer_advance_u64(&v6355->timer, v6355->dispontime);
                v6355->linepos = 0;
                if (v6355->vsynctime) {
                        v6355->vsynctime--;
                        if (!v6355->vsynctime)
                                v6355->cgastat &= ~8;
                }
                if (v6355->sc == (v6355->crtc[11] & 31) || ((crtc8 & 3) == 3 && v6355->sc == ((v6355->crtc[11] & 31) >> 1))) {
                        v6355->con = 0;
                        v6355->coff = 1;
                }
                if ((crtc8 & 3) == 3 && v6355->sc == (crtc9 >> 1))
                        v6355->maback = v6355->ma;
                if (v6355->vadj) {
                        v6355->sc++;
                        v6355->sc &= 31;
                        v6355->ma = v6355->maback;
                        v6355->vadj--;
                        if (!v6355->vadj) {
                                v6355->cgadispon = 1;
                                v6355->ma = v6355->maback = (v6355->crtc[13] | (v6355->crtc[12] << 8)) & 0x3fff;
                                v6355->sc = 0;
                        }
                } else if (v6355->sc == crtc9) {
                        v6355->maback = v6355->ma;
                        v6355->sc = 0;
                        oldvc = v6355->vc;
                        v6355->vc++;
                        v6355->vc &= 127;

                        if (v6355->vc == crtc6)
                                v6355->cgadispon = 0;

                        if (oldvc == crtc4) {
                                v6355->vc = 0;
                                v6355->vadj = crtc5;
                                if (!v6355->vadj)
                                        v6355->cgadispon = 1;
                                if (!v6355->vadj)
                                        v6355->ma = v6355->maback = (v6355->crtc[13] | (v6355->crtc[12] << 8)) & 0x3fff;
                                if ((v6355->crtc[10] & 0x60) == 0x20)
                                        v6355->cursoron = 0;
                                else
                                        v6355->cursoron = v6355->cgablink & 8;
                        }

                        if (v6355->vc == crtc7) {
                                v6355->cgadispon = 0;
                                v6355->displine = 0;
                                v6355->vsynctime = 16;
                                if (crtc7) {
					x = width + 16;
                                        v6355->lastline++;
                                        if (x != xsize || (v6355->lastline - v6355->firstline) != ysize) {
                                                xsize = x;
                                                ysize = v6355->lastline - v6355->firstline;
                                                if (xsize < 64)
                                                        xsize = 656;
                                                if (ysize < 32)
                                                        ysize = 200;
                                                updatewindowsize(xsize, (ysize << 1) + 16);
                                        }

                                        video_blit_memtoscreen(0, v6355->firstline - 4, 0, (v6355->lastline - v6355->firstline) + 8,
                                                               xsize, (v6355->lastline - v6355->firstline) + 8);
                                        frames++;

                                        video_res_x = xsize - 16;
                                        video_res_y = ysize;
                                        if (v6355->cgamode & 1) {
                                                video_res_x /= 8;
                                                video_res_y /= crtc9 + 1;
                                                video_bpp = 0;
                                        } else if (!(v6355->cgamode & 2)) {
                                                video_res_x /= 16;
                                                video_res_y /= crtc9 + 1;
                                                video_bpp = 0;
                                        } else if (!(v6355->cgamode & 16)) {
                                                video_res_x /= 2;
                                                video_bpp = 2;
                                        } else {
                                                video_bpp = 1;
                                        }
                                }
                                v6355->firstline = 1000;
                                v6355->lastline = 0;
                                v6355->cgablink++;
                                v6355->oddeven ^= 1;
                        }
                } else {
                        v6355->sc++;
                        v6355->sc &= 31;
                        v6355->ma = v6355->maback;
                }
                if (v6355->cgadispon)
                        v6355->cgastat &= ~1;
                if ((v6355->sc == (v6355->crtc[10] & 31) || ((crtc8 & 3) == 3 && v6355->sc == ((v6355->crtc[10] & 31) >> 1))))
                        v6355->con = 1;
                if (v6355->cgadispon && (v6355->cgamode & 1)) {
                        for (x = 0; x < ((width / 8) * 2); x++)
                                v6355->charbuffer[x] = v6355->vram[(((v6355->ma << 1) + x) & 0x3fff)];
                }
        }
}



void v6355_init(v6355_t *v6355) {
        timer_add(&v6355->timer, v6355_poll, v6355, 1);
        v6355->display_type = DISPLAY_RGB;
}

void *v6355_standalone_init() {
        int n, c, contrast;
        v6355_t *v6355 = malloc(sizeof(v6355_t));
        memset(v6355, 0, sizeof(v6355_t));

	/* Initialise the palette registers to default values */
	memcpy(v6355->v6355data + 0x40, defpalette, 0x20);
	for (n = 0; n < 16; n++) {
		int r = (v6355->v6355data[0x40 + 2 * n]) & 7;
		int g = (v6355->v6355data[0x41 + 2 * n] >> 4) & 7;
		int b = (v6355->v6355data[0x41 + 2 * n]) & 7;
		v6355->v6355pal[n] = 
			makecol((r * 0xFF) / 7, (g * 0xFF) / 7, (b * 0xFF) / 7);
	}

	v6355->v6355data[0x65] = 0x01;	/* Default to 200 lines */

	/* Set up CGA -> MDA attribute mapping */
        for (c = 0; c < 256; c++) {
                mdamap[c][0][0] = mdamap[c][1][0] = mdamap[c][1][1] = 0;
                if (c & 8)
                        mdamap[c][0][1] = 0xf;
                else
                        mdamap[c][0][1] = 0x7;
        }
        mdamap[0x70][0][1] = 0;
        mdamap[0x70][0][0] = mdamap[0x70][1][0] = mdamap[0x70][1][1] = 0xf;
        mdamap[0xF0][0][1] = 0;
        mdamap[0xF0][0][0] = mdamap[0xF0][1][0] = mdamap[0xF0][1][1] = 0xf;
        mdamap[0x78][0][1] = 7;
        mdamap[0x78][0][0] = mdamap[0x78][1][0] = mdamap[0x78][1][1] = 0xf;
        mdamap[0xF8][0][1] = 7;
        mdamap[0xF8][0][0] = mdamap[0xF8][1][0] = mdamap[0xF8][1][1] = 0xf;
        mdamap[0x00][0][1] = mdamap[0x00][1][1] = 0;
        mdamap[0x08][0][1] = mdamap[0x08][1][1] = 0;
        mdamap[0x80][0][1] = mdamap[0x80][1][1] = 0;
        mdamap[0x88][0][1] = mdamap[0x88][1][1] = 0;



        v6355->display_type = device_get_config_int("display_type");
        v6355->revision = device_get_config_int("composite_type");
        contrast = device_get_config_int("contrast");

        v6355->vram = malloc(0x4000);

        cga_comp_init(v6355->revision);

        timer_add(&v6355->timer, v6355_poll, v6355, 1);
        mem_mapping_add(&v6355->mapping, 0xb8000, 0x08000, v6355_read, NULL, NULL, v6355_write, NULL, NULL, NULL, MEM_MAPPING_EXTERNAL, v6355);
        io_sethandler(0x03d0, 0x0010, v6355_in, NULL, NULL, v6355_out, NULL, NULL, v6355);

        cgapal_rebuild(v6355->display_type, contrast);

        return v6355;
}

void v6355_close(void *p) {
        v6355_t *v6355 = (v6355_t *)p;

        free(v6355->vram);
        free(v6355);
}

void v6355_speed_changed(void *p) {
        v6355_t *v6355 = (v6355_t *)p;

        v6355_recalctimings(v6355);
}

device_config_t v6355_config[] = {
        {.name = "display_type",
         .description = "Display type",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "RGB", .value = DISPLAY_RGB},
                       {.description = "RGB (no brown)", .value = DISPLAY_RGB_NO_BROWN},
                       {.description = "Truecolour", .value = DISPLAY_TRUECOLOUR},
                       {.description = "Green Monochrome", .value = DISPLAY_GREEN},
                       {.description = "Amber Monochrome", .value = DISPLAY_AMBER},
                       {.description = "White Monochrome", .value = DISPLAY_WHITE},
                       {.description = "Composite", .value = DISPLAY_COMPOSITE},
                       {.description = ""}},
         .default_int = DISPLAY_RGB},
        {.name = "composite_type",
         .description = "Composite type",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "Old", .value = COMPOSITE_OLD},
                       {.description = "New", .value = COMPOSITE_NEW},
                       {.description = ""}},
         .default_int = COMPOSITE_OLD},
        {.name = "contrast", .description = "Alternate monochrome contrast", .type = CONFIG_BINARY, .default_int = 0},
        {.type = -1}};

device_t v6355d_device = {"Yamaha V6355D", 0, v6355_standalone_init, v6355_close, NULL, v6355_speed_changed, NULL, NULL, v6355_config};
