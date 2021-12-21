#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "thread.h"
#include "video.h"
#include "vid_pgc.h"
#include "vid_im1024.h"

/* This implements just enough of the Vermont Microsystems IM-1024 to 
 * support the Windows 1.03 driver. Functions are partially implemented
 * or hardwired to the behaviour expected by the Windows driver.
 *
 * One major difference seems to be that in hex mode, coordinates are
 * passed as 2-byte integer words rather than 4-byte fixed-point fractions.
 * I don't know what triggers this, so for now it's always on.
 */ 

extern uint8_t fontdat12x18[256][36];
 
typedef struct im1024_t
{
	pgc_core_t pgc;
	unsigned char fontx[256];
	unsigned char fonty[256];
	unsigned char font[256][128];

	unsigned char *fifo;
	unsigned fifo_len, fifo_wrptr, fifo_rdptr;
} im1024_t;

/* As well as the usual PGC ring buffer at 0xC6000, the IM1024 appears to 
 * have an alternate method of passing commands. This is enabled by setting 
 * 0xC6330 to 1, and then:
 * 
 * CX = count to write
 * SI -> bytes to write
 * 
 * Set pending bytes to 0
 * Read [C6331]. This gives number of bytes that can be written: 
 *               0xFF => 0, 0xFE => 1, 0xFD => 2 etc.
 * Write that number of bytes to C6000.
 * If there are more to come, go back to reading [C6331].
 * As far as I can see, at least one byte is always written; there's no 
 * provision to pause if the queue is full.
 *
 * I am implementing this by holding a FIFO of unlimited depth in the 
 * IM1024 to receive the data.
 *
 */

static void fifo_write(im1024_t *im1024, unsigned char val)
{
/*	PGCLOG(("fifo_write: %02x [rd=%04x wr=%04x]\n", val,
			im1024->fifo_rdptr, im1024->fifo_wrptr)); */
	if (((im1024->fifo_wrptr + 1) % im1024->fifo_len) == im1024->fifo_rdptr)
	{
/* FIFO is full. Double its size. */
		unsigned char *buf;

		PGCLOG(("fifo_resize: %d to %d\n", 
			im1024->fifo_len, 2 * im1024->fifo_len));

		buf  = realloc(im1024->fifo, 2 * im1024->fifo_len);
		if (!buf) return;
/* Move the [0..wrptr] range to the newly-allocated area [len..len+wrptr] */
		memmove(buf + im1024->fifo_len, buf, im1024->fifo_wrptr);
		im1024->fifo = buf;
		im1024->fifo_wrptr += im1024->fifo_len;
		im1024->fifo_len *= 2;
	}
/* Append to the queue */	
	im1024->fifo[im1024->fifo_wrptr] = val;
	++im1024->fifo_wrptr;

/* Wrap if end of buffer reached */
	if (im1024->fifo_wrptr >= im1024->fifo_len)
	{
		im1024->fifo_wrptr = 0;
	}
}


static int fifo_read(im1024_t *im1024)
{
	uint8_t result;

	if (im1024->fifo_wrptr == im1024->fifo_rdptr)
	{
		return -1;	/* FIFO empty */
	}
	result = im1024->fifo[im1024->fifo_rdptr];
	++im1024->fifo_rdptr;
	if (im1024->fifo_rdptr >= im1024->fifo_len)
	{
		im1024->fifo_rdptr = 0;
	}
/*
	PGCLOG(("fifo_read: %02x\n", result));
*/
	return result;

}

/* Where a normal PGC would just read from the ring buffer at 0xC6300, the
 * IM-1024 can read either from this or from its internal FIFO. The internal
 * FIFO has priority. */
int im1024_input_byte(pgc_core_t *pgc, uint8_t *result)
{
	im1024_t *im1024 = (im1024_t *)pgc;

	/* If input buffer empty, wait for it to fill */
	while ((im1024->fifo_wrptr == im1024->fifo_rdptr) &&
	       (pgc->mapram[0x300] == pgc->mapram[0x301]))
	{
		pgc->waiting_input_fifo = 1;
		pgc_sleep(pgc);	
	}
	if (pgc->mapram[0x3FF])	/* Reset triggered */
	{
		pgc_reset(pgc);
		return 0;
	}
	if (im1024->fifo_wrptr == im1024->fifo_rdptr)
	{
		*result = pgc->mapram[pgc->mapram[0x301]];
		++pgc->mapram[0x301];
	}
	else
	{
		*result = fifo_read(im1024);
	}
	return 1;
}

/* Macros to disable clipping and save clip state */
#define PUSHCLIP { \
	uint16_t vp_x1, vp_x2, vp_y1, vp_y2; \
	vp_x1 = pgc->vp_x1; \
	vp_y1 = pgc->vp_y1; \
	vp_x2 = pgc->vp_x2; \
	vp_y2 = pgc->vp_y2; \
	pgc->vp_x1 = 0; \
	pgc->vp_y1 = 0; \
	pgc->vp_x2 = pgc->maxw - 1; \
	pgc->vp_y2 = pgc->maxh - 1; \

/* And to restore clip state */	
#define POPCLIP \
	pgc->vp_x1 = vp_x1; \
	pgc->vp_y1 = vp_y1; \
	pgc->vp_x2 = vp_x2; \
	pgc->vp_y2 = vp_y2; \
	}	


/* Override memory read to return FIFO space */
uint8_t im1024_read(uint32_t addr, void *p)
{
	im1024_t *im1024 = (im1024_t *)p;

	if (addr == 0xC6331 && im1024->pgc.mapram[0x330] == 1)
	{
		return 0x80;	/* Hardcode that there are 128 bytes
				 * free */
	}
	return pgc_read(addr, &im1024->pgc);
}

/* Override memory write to handle writes to the FIFO */
void im1024_write(uint32_t addr, uint8_t val, void *p)
{
	im1024_t *im1024 = (im1024_t *)p;

	/* If we are in 'fast' input mode, send all writes to the internal
	 * FIFO */
	if (addr >= 0xC6000 && addr < 0xC6100 && im1024->pgc.mapram[0x330] == 1)
	{
		fifo_write(im1024, val);
/*
		PGCLOG(("im1024_write(%02x)\n", val));
*/
		if (im1024->pgc.waiting_input_fifo)
		{
			im1024->pgc.waiting_input_fifo = 0;
			pgc_wake(&im1024->pgc);
		}
		return;
	}
	pgc_write(addr, val, &im1024->pgc);
}



/* I don't know what the IMGSIZ command does, only that the Windows driver
 * issues it. So just parse and ignore it */
void hndl_imgsiz(pgc_core_t *pgc)
{
	int16_t w,h;
	uint8_t a,b;
//	im1024_t *im1024 = (im1024_t *)pgc;

	if (!pgc_param_word(pgc, &w)) return;
	if (!pgc_param_word(pgc, &h)) return;
	if (!pgc_param_byte(pgc, &a)) return;
	if (!pgc_param_byte(pgc, &b)) return;

	PGCLOG(("IMGSIZ %d,%d,%d,%d\n", w,h,a,b));
}


/* I don't know what the IPREC command does, only that the Windows driver
 * issues it. So just parse and ignore it */
void hndl_iprec(pgc_core_t *pgc)
{
	uint8_t param;
//	im1024_t *im1024 = (im1024_t *)pgc;

	if (!pgc_param_byte(pgc, &param)) return;

	PGCLOG(("IPREC %d\n", param));
}

/* I think PAN controls which part of the 1024x1024 framebuffer is displayed
 * in the 1024x800 visible screen. */
void hndl_pan(pgc_core_t *pgc)
{
	int16_t x,y;

	if (!pgc_param_word(pgc, &x)) return;
	if (!pgc_param_word(pgc, &y)) return;

	PGCLOG(("PAN %d,%d\n", x, y));

	pgc->pan_x = x;
	pgc->pan_y = y;
}

/* PLINE draws a non-filled polyline at a fixed position */
void hndl_pline(pgc_core_t *pgc)
{
	int16_t x[257];
	int16_t y[257];
	uint8_t count;
	unsigned n;
	uint16_t linemask = pgc->line_pattern;

	if (!pgc_param_byte(pgc, &count)) return;

	PGCLOG(("PLINE<IM1024> (%d)  ", count));
	for (n = 0; n < count; n++)
	{
		if (!pgc_param_word(pgc, &x[n])) return;
		if (!pgc_param_word(pgc, &y[n])) return;
		PGCLOG(("    (%d,%d)\n", x[n], y[n]));
	}
	for (n = 1; n < count; n++)
	{
		linemask = pgc_draw_line(pgc, x[n - 1] << 16, y[n - 1] << 16,
			x[n] << 16, y[n] << 16, linemask);
	}
}


/* Blit a single row of pixels from one location to another. To avoid 
 * difficulties if the two overlap, read both rows into memory, process them
 * there, and write the result back. */
void blkmov_row(pgc_core_t *pgc, int16_t x0, int16_t x1, int16_t x2, 
			int16_t sy, int16_t ty)
{
	int16_t x;
	uint8_t src[1024];
	uint8_t dst[1024];

	for (x = x0; x <= x1; x++)
	{
		src[x - x0] = pgc_read_pixel(pgc, x, sy);
		dst[x - x0] = pgc_read_pixel(pgc, x - x0 + x2, ty);
	}
	for (x = x0; x <= x1; x++)
	{
		switch (pgc->draw_mode)
		{
			default:
			case 0: pgc_write_pixel(pgc, (x - x0 + x2), ty, src[x - x0]); break;
			case 1: pgc_write_pixel(pgc, (x - x0 + x2), ty, dst[x - x0] ^ 0xFF); break;
			case 2: pgc_write_pixel(pgc, (x - x0 + x2), ty, src[x - x0] ^ dst[x - x0]); break;
			case 3: pgc_write_pixel(pgc, (x - x0 + x2), ty, src[x - x0] & dst[x - x0]); break;
		}
	}

}


/* BLKMOV blits a rectangular area from one location to another, with no
 * clipping. */
void hndl_blkmov(pgc_core_t *pgc)
{
	int16_t x0,y0;
	int16_t x1,y1;
	int16_t x2,y2;
	int16_t y;
//	im1024_t *im1024 = (im1024_t *)pgc;

	if (!pgc_param_word(pgc, &x0)) return;
	if (!pgc_param_word(pgc, &y0)) return;
	if (!pgc_param_word(pgc, &x1)) return;
	if (!pgc_param_word(pgc, &y1)) return;
	if (!pgc_param_word(pgc, &x2)) return;
	if (!pgc_param_word(pgc, &y2)) return;

	PGCLOG(("BLKMOV %d,%d,%d,%d,%d,%d\n", x0,y0,x1,y1,x2,y2));

	/* Disable clipping */
	PUSHCLIP

	/* Either go down from the top, or up from the bottom, depending
	 * whether areas might overlap */
	if (y2 <= y0) 
	{
		for (y = y0; y <= y1; y++)
		{
			blkmov_row(pgc, x0, x1, x2, y, y - y0 + y2);
		}
	}
	else
	{
		for (y = y1; y >= y0; y--)
		{
			blkmov_row(pgc, x0, x1, x2, y, y - y0 + y2);
		}
	}
	/* Restore clipping */
	POPCLIP
}


/* This overrides the PGC ELIPSE command to parse its parameters as words
 * rather than coordinates */
static void hndl_ellipse(pgc_core_t *pgc)
{
	int16_t x, y;

	if (!pgc_param_word(pgc, &x)) return;
	if (!pgc_param_word(pgc, &y)) return;

	PGCLOG(("ELLIPSE<IM1024> %d,%d @ %d,%d\n", x,y,
		pgc->x >> 16, pgc->y >> 16));
	pgc_draw_ellipse(pgc, x << 16, y << 16);
}



/* This overrides the PGC MOVE command to parse its parameters as words
 * rather than coordinates */
static void hndl_move(pgc_core_t *pgc)
{
	int16_t x, y;

	if (!pgc_param_word(pgc, &x)) return;
	if (!pgc_param_word(pgc, &y)) return;

	pgc->x = x << 16;
	pgc->y = y << 16;
	PGCLOG(("MOVE<IM1024> %d,%d\n", x,y));
}

/* This overrides the PGC DRAW command to parse its parameters as words
 * rather than coordinates */
static void hndl_draw(pgc_core_t *pgc)
{
	int16_t x, y;

	if (!pgc_param_word(pgc, &x)) return;
	if (!pgc_param_word(pgc, &y)) return;

	PGCLOG(("DRAW<IM1024> %d,%d to %d,%d\n", 
		pgc->x >> 16, pgc->y >> 16, x, y));
	pgc_draw_line(pgc, pgc->x, pgc->y, x << 16, y << 16, pgc->line_pattern);
	pgc->x = x << 16;
	pgc->y = y << 16;
}




/* This overrides the PGC POLY command to parse its parameters as words
 * rather than coordinates */
static void hndl_poly(pgc_core_t *pgc)
{
	int32_t *x, *y;
	int32_t n;
	int16_t xw, yw, mask;
	unsigned realcount = 0;
	int parsing = 1;
	int as = 256;

	x = malloc(as * sizeof(int32_t));
	y = malloc(as * sizeof(int32_t));

	if (!x || !y) 
	{
		PGCLOG(("hndl_poly: malloc failed\n"));
		return;
	}
	while (parsing)	
	{
		uint8_t count;
		if (!pgc_param_byte(pgc, &count)) return;

		if (count + realcount >= as)
		{
			int32_t *nx, *ny;
		
			nx = realloc(x, 2 * as * sizeof(int32_t));	
			ny = realloc(y, 2 * as * sizeof(int32_t));	
			if (!x || !y) 
			{
				PGCLOG(("hndl_poly: realloc failed\n"));
				break;
			}
			x = nx;
			y = ny;
			as *= 2;
		}

		for (n = 0; n < count; n++)
		{
			if (!pgc_param_word(pgc, &xw)) return;
			if (!pgc_param_word(pgc, &yw)) return;
	
			/* Skip degenerate line segments */
			if (realcount > 0 && 
				(xw << 16) == x[realcount - 1] && 
				(yw << 16) == y[realcount - 1])
			{
				continue;
			}
			x[realcount] = xw << 16;
			y[realcount] = yw << 16;
			++realcount;
		}
/* If we're in a command list, peek ahead to see if the next command is
 * also POLY. If so, that's a continuation of this polygon! */
		parsing = 0;
		if (pgc->clcur && (pgc->clcur->rdptr+1) < pgc->clcur->wrptr &&
		    pgc->clcur->list[pgc->clcur->rdptr] == 0x30)
		{
			PGCLOG(("hndl_poly: POLY continues!\n"));
			parsing = 1;
			/* Swallow the POLY */
			++pgc->clcur->rdptr;
		}

	}

	PGCLOG(("POLY<IM1024> (%d) fill_mode=%d\n", realcount, pgc->fill_mode));
	for (n = 0; n < realcount; n++)
	{
		PGCLOG(("     (%d,%d)\n", x[n] >> 16, y[n] >> 16));
	}
	if (pgc->fill_mode)
	{
		pgc_fill_polygon(pgc, realcount, x, y);
	}
	/* Now draw borders */
	mask = pgc->line_pattern;
	for (n = 1; n < realcount; n++)
	{
		mask = pgc_draw_line(pgc, x[n - 1], y[n - 1], x[n], y[n], mask);
	}
	pgc_draw_line(pgc, x[realcount - 1], y[realcount - 1], x[0], y[0], mask);
	free(y);
	free(x);
}


static int parse_poly(pgc_core_t *pgc, pgc_commandlist_t *cl, int c)
{
	uint8_t count;

	PGCLOG(("parse_poly<IM1024>\n"));
	if (!pgc_param_byte(pgc, &count)) return 0;

	PGCLOG(("parse_poly<IM1024>: count=%02x\n", count));
	if (!pgc_commandlist_append(cl, count))
	{
		pgc_error(pgc, PGC_ERROR_OVERFLOW);
		return 0;	
	}
	PGCLOG(("parse_poly<IM1024>: parse %d words\n", 2 * count));
	return pgc_parse_words(pgc, cl, count * 2);
}



/* This overrides the PGC RECT command to parse its parameters as words
 * rather than coordinates */
static void hndl_rect(pgc_core_t *pgc)
{
	int16_t x0, y0, x1, y1, p, q;

	x0 = pgc->x >> 16;
	y0 = pgc->y >> 16;

	if (!pgc_param_word(pgc, &x1)) return;
	if (!pgc_param_word(pgc, &y1)) return;

	/* Convert to raster coords */
	pgc_sto_raster(pgc, &x0, &y0);
	pgc_sto_raster(pgc, &x1, &y1);

	if (x0 > x1) { p = x0; x0 = x1; x1 = p; }
	if (y0 > y1) { q = y0; y0 = y1; y1 = q; }

	PGCLOG(("RECT<IM1024> (%d,%d) -> (%d,%d)\n", x0, y0, x1, y1));

	if (pgc->fill_mode) 
	{
		for (p = y0; p <= y1; p++)
		{
			pgc_fill_line_r(pgc, x0, x1, p);
		}
	}
	else	/* Outline: 4 lines */
	{
		p = pgc->line_pattern;
		p = pgc_draw_line_r(pgc, x0, y0, x1, y0, p);		
		p = pgc_draw_line_r(pgc, x1, y0, x1, y1, p);		
		p = pgc_draw_line_r(pgc, x1, y1, x0, y1, p);		
		p = pgc_draw_line_r(pgc, x0, y1, x0, y0, p);		
	}
}


/* TODO: Text drawing should probably be implemented in vid_pgc.c rather
 * than vid_im1024.c */
static void hndl_tdefin(pgc_core_t *pgc)
{
	unsigned char ch, bt;
	unsigned char rows, cols;
	unsigned len, n;
//	unsigned x = 0;
	im1024_t * im1024 = (im1024_t *)pgc;

	if (!pgc_param_byte(pgc, &ch)) return;
	if (!pgc_param_byte(pgc, &cols)) return;
	if (!pgc_param_byte(pgc, &rows)) return;

	PGCLOG(("TDEFIN<IM1024> (%d,%d,%d) 0x%02x 0x%02x\n", ch, rows, cols,
		pgc->mapram[0x300], pgc->mapram[0x301]));

	len = ((cols + 7) / 8) * rows;
	for (n = 0; n < len; n++)
	{
//		char buf[10];
//		unsigned char mask;

		if (!pgc_param_byte(pgc, &bt)) return;

//		buf[0] = 0;	
//		for (mask = 0x80; mask != 0; mask >>= 1)
//		{
//			if (bt & mask) strcat(buf, "#");	
//			else	       strcat(buf, "-");	
//			++x;
//			if (x == cols) { strcat(buf, "\n"); x = 0; }
//		}	
//		PGCLOG((buf));

		if (n < sizeof(im1024->font[ch])) im1024->font[ch][n] = bt;
	}
	im1024->fontx[ch] = cols;
	im1024->fonty[ch] = rows;
}

static void hndl_tsize(pgc_core_t *pgc)
{
	int16_t size;

	if (!pgc_param_word(pgc, &size)) return;
	PGCLOG(("TSIZE<IM1024>(%d)\n", size));

	pgc->tsize = size << 16;
}



static void hndl_twrite(pgc_core_t *pgc)
{
	unsigned char count;
	unsigned char mask;
	unsigned char *row;
	int x, y, wb, n;
	im1024_t * im1024 = (im1024_t *)pgc;
	int16_t x0 = pgc->x >> 16;
	int16_t y0 = pgc->y >> 16;
	unsigned char buf[256];
/*	unsigned char rbuf[256];*/

	if (!pgc_param_byte(pgc, &count)) return;

	for (n = 0; n < count; n++)
	{
		if (!pgc_param_byte(pgc, &buf[n])) return;
	}
	buf[count] = 0;
/*	for (n = 0; n <= count; n++)
	{
		if (isprint(buf[n]) || 0 == buf[n])
		{
			rbuf[n] = buf[n];
		}
		else	rbuf[n] = '?';
	}*/
	pgc_sto_raster(pgc, &x0, &y0);
	PGCLOG(("TWRITE<IM1024> (%d,%-*.*s) x0=%d y0=%d\n", count, count, count, rbuf, x0, y0));

	for (n = 0; n < count; n++)
	{
		wb = (im1024->fontx[buf[n]] + 7) / 8;
		PGCLOG(("ch=0x%02x w=%d h=%d wb=%d\n", 
				buf[n], im1024->fontx[buf[n]], 
				im1024->fonty[buf[n]], wb));
		for (y = 0; y < im1024->fonty[buf[n]]; y++)
		{
			mask = 0x80;
			row = &im1024->font[buf[n]][y * wb];
			for (x = 0; x < im1024->fontx[buf[n]]; x++)
			{
/*				rbuf[x] = (row[0] & mask) ? '#' : '-';*/
				if (row[0] & mask) pgc_plot(pgc, x + x0, y0 - y);
				mask = mask >> 1;
				if (mask == 0) { mask = 0x80; ++row; }
			}
/*			rbuf[x++] = '\n';
			rbuf[x++] = 0;*/
//			PGCLOG((rbuf);
		}
		x0 += im1024->fontx[buf[n]];
	}
}



static void hndl_txt88(pgc_core_t *pgc)
{
	unsigned char count;
	unsigned char mask;
	unsigned char *row;
	int x, y, /*wb, */n;
	int16_t x0 = pgc->x >> 16;
	int16_t y0 = pgc->y >> 16;
	unsigned char buf[256];
/*	unsigned char rbuf[256];*/

	if (!pgc_param_byte(pgc, &count)) return;

	for (n = 0; n < count; n++)
	{
		if (!pgc_param_byte(pgc, &buf[n])) return;
	}
	buf[count] = 0;
/*	for (n = 0; n <= count; n++)
	{
		if (isprint(buf[n]) || 0 == buf[n])
		{
			rbuf[n] = buf[n];
		}
		else	rbuf[n] = '?';
	}*/
	pgc_sto_raster(pgc, &x0, &y0);
	PGCLOG(("TXT88<IM1024> (%d,%-*.*s) x0=%d y0=%d\n", count, count, count, rbuf, x0, y0));

	for (n = 0; n < count; n++)
	{
/*		wb = 2;*/
		PGCLOG(("ch=0x%02x w=%d h=%d wb=%d\n", buf[n], 12, 18, wb));
		for (y = 0; y < 18; y++)
		{
			mask = 0x80;
			row = &fontdat12x18[buf[n]][y * 2];
			for (x = 0; x < 12; x++)
			{
/*				rbuf[x] = (row[0] & mask) ? '#' : '-';*/
				if (row[0] & mask) pgc_plot(pgc, x + x0, y0 - y);
				mask = mask >> 1;
				if (mask == 0) { mask = 0x80; ++row; }
			}
/*			rbuf[x++] = '\n';
			rbuf[x++] = 0;*/
//			PGCLOG((rbuf);
		}
		x0 += 12;
	}
}




static void hndl_imagew(pgc_core_t *pgc)
{
	int16_t row1, col1, col2;
	uint8_t v1, v2;
	int16_t vp_x1, vp_y1, vp_x2, vp_y2;

	if (!pgc_param_word(pgc, &row1)) return;
	if (!pgc_param_word(pgc, &col1)) return;
	if (!pgc_param_word(pgc, &col2)) return;

	/* IMAGEW already uses raster coordinates so there is no need to
	 * convert it */
	PGCLOG(("IMAGEW<IM1024> (row=%d,col1=%d,col2=%d)\n", row1, col1, col2));

	vp_x1 = pgc->vp_x1;
	vp_y1 = pgc->vp_y1;
	vp_x2 = pgc->vp_x2;
	vp_y2 = pgc->vp_y2;
	/* Disable clipping */
	pgc->vp_x1 = 0;
	pgc->vp_y1 = 0;
	pgc->vp_x2 = pgc->maxw - 1;
	pgc->vp_y2 = pgc->maxh - 1;

	/* In ASCII mode, what is written is a stream of bytes */
	if (pgc->ascii_mode)
	{
		while (col1 <= col2)
		{
			if (!pgc_param_byte(pgc, &v1)) return;
			pgc_write_pixel(pgc, col1, row1, v1);
			++col1;
		}
		return;
	}
	else	/* In hex mode, it's RLE compressed */
	{
		while (col1 <= col2)
		{
			if (!pgc_param_byte(pgc, &v1)) return;

			if (v1 & 0x80)	/* Literal run */
			{
				v1 -= 0x7F;
				while (col1 <= col2 && v1 != 0)	
				{	
					if (!pgc_param_byte(pgc, &v2)) return;
					pgc_write_pixel(pgc, col1, row1, v2);
					++col1;
					--v1;
				}
			}
			else	/* Repeated run */
			{
				if (!pgc_param_byte(pgc, &v2)) return;
		
				++v1;	
				while (col1 <= col2 && v1 != 0)	
				{	
					pgc_write_pixel(pgc, col1, row1, v2);
					++col1;
					--v1;
				}
			}	
		}
	}
	/* Restore clipping */
	pgc->vp_x1 = vp_x1;
	pgc->vp_y1 = vp_y1;
	pgc->vp_x2 = vp_x2;
	pgc->vp_y2 = vp_y2;
}

/* I have called this command DOT - I don't know its proper name. Draws a 
 * single pixel at the current location */
static void hndl_dot(pgc_core_t *pgc)
{
	int16_t x = pgc->x >> 16, y = pgc->y >> 16;

	pgc_sto_raster(pgc, &x, &y);	

	PGCLOG(("Dot @ %d,%d ink=%d mode=%d\n", x, y, pgc->colour, pgc->draw_mode));
	pgc_plot(pgc, x, y);
}

/* This command (which I have called IMAGEX, since I don't know its real 
 * name) is a screen-to-memory blit. It reads a rectangle of bytes, rather
 * than the single row read by IMAGER, and does not attempt to compress 
 * the result */
static void hndl_imagex(pgc_core_t *pgc)
{
	int16_t x0, x1, y0, y1;
	int16_t p,q;

	if (!pgc_param_word(pgc, &x0)) return;
	if (!pgc_param_word(pgc, &y0)) return;
	if (!pgc_param_word(pgc, &x1)) return;
	if (!pgc_param_word(pgc, &y1)) return;

	/* IMAGEX already uses raster coordinates so don't convert */
	PGCLOG(("IMAGEX<IM1024> (%d,%d,%d,%d)\n", x0,y0,x1,y1));

	for (p = y0; p <= y1; p++)
	{
		for (q = x0; q <= x1; q++)
		{
			if (!pgc_result_byte(pgc, pgc_read_pixel(pgc, q, p)))
				return;
		}
	}
}

/* Commands implemented by the IM-1024.
 *
 * TODO: A lot of commands need commandlist parsers.
 * TODO: The IM-1024 has a lot more commands that are not included here
 *       (BLINK, BUTRD, COPROC, RBAND etc) because the Windows 1.03 driver
 *       does not use them.
 */
static const pgc_command_t im1024_commands[] =
{
	{ "BLKMOV", 0xDF, hndl_blkmov,  pgc_parse_words, 6 },
	{ "DRAW",   0x28, hndl_draw,    pgc_parse_words, 2 },
	{ "D",      0x28, hndl_draw,    pgc_parse_words, 2 },
	{ "DOT",    0x08, hndl_dot },
	{ "ELIPSE", 0x39, hndl_ellipse, pgc_parse_words, 2 },
	{ "EL",     0x39, hndl_ellipse, pgc_parse_words, 2 },
	{ "IMAGEW", 0xD9, hndl_imagew },
	{ "IW",     0xD9, hndl_imagew },
	{ "IMAGEX", 0xDA, hndl_imagex },
	{ "TXT88",  0x88, hndl_txt88 },
	{ "TWRITE", 0x8B, hndl_twrite },
	{ "TDEFIN", 0x84, hndl_tdefin },
	{ "TD",     0x84, hndl_tdefin },
	{ "TSIZE",  0x81, hndl_tsize },
	{ "TS",     0x81, hndl_tsize },
	{ "IPREC",  0xE4, hndl_iprec },
	{ "IMGSIZ", 0x4E, hndl_imgsiz },
	{ "LUT8",   0xE6, pgc_hndl_lut8 },
	{ "L8",     0xE6, pgc_hndl_lut8 },
	{ "LUT8RD", 0x53, pgc_hndl_lut8rd },
	{ "L8RD",   0x53, pgc_hndl_lut8rd },
	{ "PAN",    0xB7, hndl_pan },
	{ "POLY",   0x30, hndl_poly, parse_poly },
	{ "P",      0x30, hndl_poly, parse_poly },
	{ "PLINE",  0x36, hndl_pline },
	{ "PL",     0x37, hndl_pline },
	{ "MOVE",   0x10, hndl_move, pgc_parse_words, 2 },
	{ "M",      0x10, hndl_move, pgc_parse_words, 2 },
	{ "RECT",   0x34, hndl_rect },
	{ "R",      0x34, hndl_rect },
	{ "******", 0x00, NULL }
};


void *im1024_init()
{
	im1024_t *im1024 = malloc(sizeof(im1024_t));
	memset(im1024, 0, sizeof(im1024_t));

	im1024->fifo = malloc(4096);
	im1024->fifo_len = 4096;
	im1024->fifo_wrptr = 0;
	im1024->fifo_rdptr = 0;

	/* 1024x1024 framebuffer with 1024x800 visible
           Pixel clock is a guess */
	pgc_core_init(&im1024->pgc, 1024, 1024,  1024, 800, im1024_input_byte, 65000000.0);
	
	mem_mapping_set_handler(&im1024->pgc.mapping, 
		im1024_read,  NULL, NULL,
		im1024_write, NULL, NULL);

	im1024->pgc.pgc_commands = im1024_commands;
	return im1024;
}

void im1024_close(void *p)
{
	im1024_t *im1024 = (im1024_t *)p;
        
	pgc_close(&im1024->pgc);
}

void im1024_speed_changed(void *p)
{
	im1024_t *im1024 = (im1024_t *)p;
        
        pgc_speed_changed(&im1024->pgc);
}

device_t im1024_device =
{
        "Image Manager 1024",
        0,
        im1024_init,
        im1024_close,
        NULL,
        im1024_speed_changed,
        NULL,
        NULL,
        NULL
};
