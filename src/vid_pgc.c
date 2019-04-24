/*PGC emulation*/
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "timer.h"
#include "thread.h"
#include "video.h"
#include "vid_pgc.h"
#include <assert.h>

/* This implements just enough of the Professional Graphics Controller to
 * act as a basis for the Vermont Microsystems IM-1024. 
 *
 * PGC features implemented include:
 * > The CGA-compatible display modes
 * > Switching to and from native mode
 * > Communicating with the host PC
 * 
 * Numerous features are implemented partially or not at all, such as:
 * > 2D drawing
 * > 3D drawing
 * > Command lists
 * Some of these are marked TODO.
 *
 * The PGC has two display modes: CGA (in which it appears in the normal CGA 
 * memory and I/O ranges) and native (in which all functions are accessed
 * through reads and writes to 1k of memory at 0xC6000). The PGC's 8088
 * processor monitors this buffer and executes instructions left there
 * for it. We simulate this behaviour with a separate thread.
 */

#define PGC_CGA_WIDTH 640
#define PGC_CGA_HEIGHT 400

static int pgc_parse_command(pgc_core_t *pgc, const pgc_command_t **pcmd);
int	pgc_clist_byte(pgc_core_t *pgc, uint8_t *val);




static const char *pgc_err_msgs[] = 
{
	"Range   \r",
	"Integer \r",
	"Memory  \r",
	"Overflow\r",
	"Digit   \r",
	"Opcode  \r",
	"Running \r",
	"Stack   \r",
	"Too long\r",
	"Area    \r",
	"Missing \r" 
};

#define HWORD(u) ((u) >> 16)
#define LWORD(u) ((u) & 0xFFFF)


/* Initial palettes */
uint32_t init_palette[6][256] = 
{
#include "pgc_palettes.h"
};



/* When idle, the PGC drawing thread sleeps. pgc_wake() awakens it - but
 * not immediately. Like the Voodoo, it has a short delay so that writes 
 * can be batched */
#define WAKE_DELAY (TIMER_USEC * 500)
void pgc_wake(pgc_core_t *pgc)
{
	if (!timer_is_enabled(&pgc->wake_timer))
	{
		timer_set_delay_u64(&pgc->wake_timer, WAKE_DELAY);
	}
}


/* When the wake timer expires, that's when the drawing thread is actually
 * woken */
static void pgc_wake_timer(void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;

	PGCLOG(("pgc: Waking up\n"));
	thread_set_event(pgc->pgc_wake_thread);
}

/* This is called by the drawing thread when it's waiting for the host
 * to put more input in its input FIFO, or drain output from its output
 * FIFO. */
void pgc_sleep(pgc_core_t *pgc)
{

	PGCLOG(("pgc: Sleeping on %d %d %d 0x%02x 0x%02x\n",
			pgc->waiting_input_fifo,
			pgc->waiting_output_fifo,
			pgc->waiting_error_fifo,
			pgc->mapram[0x300], pgc->mapram[0x301])); 
	/* Race condition: If host wrote to the PGC during the pclog() that
	 * won't be noticed */
	if (pgc->waiting_input_fifo && pgc->mapram[0x300] != pgc->mapram[0x301])
	{
		pgc->waiting_input_fifo = 0;
		return;
	}
	/* Same if they read */
	if (pgc->waiting_output_fifo && pgc->mapram[0x302] != (uint8_t)(pgc->mapram[0x303] - 1))	
	{
		pgc->waiting_output_fifo = 0;
		return;
	}
	thread_wait_event(pgc->pgc_wake_thread, -1);
	thread_reset_event(pgc->pgc_wake_thread);

}

/* Switch between CGA mode (DISPLAY 1) and native mode (DISPLAY 0) */
void pgc_setdisplay(pgc_core_t *pgc, int cga)
{
	PGCLOG(("pgc_setdisplay(%d): cga_selected=%d cga_enabled=%d\n",
			cga, pgc->cga_selected, pgc->cga_enabled));
	if (pgc->cga_selected != (pgc->cga_enabled && cga))
	{
		pgc->cga_selected = (pgc->cga_enabled && cga);

		if (pgc->cga_selected)
		{
			mem_mapping_enable(&pgc->cga_mapping);
			pgc->screenw = PGC_CGA_WIDTH;
			pgc->screenh = PGC_CGA_HEIGHT;
		}
		else	
		{
			mem_mapping_disable(&pgc->cga_mapping);
			pgc->screenw = pgc->visw;
			pgc->screenh = pgc->vish;
		}
		pgc_recalctimings(pgc);
	}
}





/* Convert coordinates based on the current window / viewport to raster 
 * coordinates. */
void pgc_dto_raster(pgc_core_t *pgc, double *x, double *y)
{
/*	double x0 = *x, y0 = *y; */

	*x += (pgc->vp_x1 - pgc->win_x1);
	*y += (pgc->vp_y1 - pgc->win_y1);

/* 	PGCLOG(("Coords to raster: (%f, %f) -> (%f, %f)\n", x0, y0, *x, *y)); */
}


/* Overloads that take ints */
void pgc_sto_raster(pgc_core_t *pgc, int16_t *x, int16_t *y)
{
	double xd = *x, yd = *y;

	pgc_dto_raster(pgc, &xd, &yd);
	*x = (int16_t)xd;
	*y = (int16_t)yd;
}

void pgc_ito_raster(pgc_core_t *pgc, int32_t *x, int32_t *y)
{
	double xd = *x, yd = *y;

	pgc_dto_raster(pgc, &xd, &yd);
	*x = (int32_t)xd;
	*y = (int32_t)yd;
}


/* Add a byte to a command list. We allow command lists to be 
 * arbitrarily large */
int pgc_commandlist_append(pgc_commandlist_t *list, uint8_t v)
{
	if (list->listmax == 0 || list->list == NULL)
	{
		list->list = malloc(4096);
		if (!list->list) 
		{
			PGCLOG(("Out of memory initialising command list\n"));
			return 0;
		}
		list->listmax = 4096;
	}
	while (list->wrptr >= list->listmax)
	{
		uint8_t *buf = realloc(list->list, 2 * list->listmax);
		if (!buf) 
		{
			PGCLOG(("Out of memory growing command list\n"));
			return 0;
		}
		list->list = buf;
		list->listmax *= 2;
	}
	list->list[list->wrptr++] = v;
	return 1;
}

/* Beginning of a command list. Parse commands up to the next CLEND,
 * storing them (in hex form) in the named command list. */
static void hndl_clbeg(pgc_core_t *pgc)
{
	uint8_t param;
	pgc_commandlist_t cl;
	const pgc_command_t *cmd;

	memset(&cl, 0, sizeof(cl));
	if (!pgc_param_byte(pgc, &param)) return;

	PGCLOG(("CLBEG(%d)\n", param));
	while (1)
	{
		if (!pgc_parse_command(pgc, &cmd))
		{
			/* PGC has been reset */
			return;	
		} 
		if (!cmd)
		{
			pgc_error(pgc, PGC_ERROR_OPCODE);
			return;		
		}
		else if (pgc->hex_command == 0x71)	/* CLEND */
		{
			pgc->clist[param] = cl;
			return;
		}
		else
		{
			if (!pgc_commandlist_append(&cl, pgc->hex_command))
			{
				pgc_error(pgc, PGC_ERROR_OVERFLOW);
				return;
			}		
			if (cmd->parser)
			{
				if (!(*cmd->parser)(pgc, &cl, cmd->p))
				{
					return;
				}
			}
		}
	}
}

static void hndl_clend(pgc_core_t *pgc)
{
	/* Shouldn't happen outside a CLBEG */
}

/* Execute a command list. If one was already executing, remember it so 
 * we can return to it afterwards. */
static void hndl_clrun(pgc_core_t *pgc)
{
	uint8_t param;
	pgc_commandlist_t *clprev = pgc->clcur;

	if (!pgc_param_byte(pgc, &param)) return;

	pgc->clcur = &pgc->clist[param];
	pgc->clcur->rdptr = 0;
	pgc->clcur->repeat = 1;
	pgc->clcur->chain = clprev;
}

/* Execute a command list multiple times. */
static void hndl_cloop(pgc_core_t *pgc)
{
	uint8_t param;
	int16_t repeat;
	pgc_commandlist_t *clprev = pgc->clcur;

	if (!pgc_param_byte(pgc, &param)) return;
	if (!pgc_param_word(pgc, &repeat)) return;

	pgc->clcur = &pgc->clist[param];
	pgc->clcur->rdptr = 0;
	pgc->clcur->repeat = repeat;
	pgc->clcur->chain = clprev;
}


/* Read back a command list */
static void hndl_clread(pgc_core_t *pgc)
{
	uint8_t param;
	int n;

	if (!pgc_param_byte(pgc, &param)) return;

	for (n = 0; n < pgc->clist[param].wrptr; n++)
	{
		if (!pgc_result_byte(pgc, pgc->clist[param].list[n]))
			return;
	}
}


/* Delete a command list */
static void hndl_cldel(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc_param_byte(pgc, &param)) return;
	memset(&pgc->clist[param], 0, sizeof(pgc_commandlist_t));
}



/* Clear the screen to a specified colour */
static void hndl_clears(pgc_core_t *pgc)
{
	uint8_t param;
	int y;

	if (!pgc_param_byte(pgc, &param)) return;

	for (y = 0; y < pgc->screenh; y++)
		memset(pgc->vram + y * pgc->maxw, param, pgc->screenw);
}

/* Select drawing colour */
static void hndl_color(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc_param_byte(pgc, &param)) return;

	pgc->colour = param;
	PGCLOG(("COLOR(%d)\n", param));
}


/* Set drawing mode. 
 * 
 * 0 => Draw
 * 1 => Invert
 * 2 => XOR (IM-1024)
 * 3 => AND (IM-1024)
 *
 */
static void hndl_linfun(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc_param_byte(pgc, &param)) return;

	/* TODO: Not range-checked. Strictly speaking we should limit to 0-1
	 * for the PGC and 0-3 for the IM-1024. */
	pgc->draw_mode = param;
	PGCLOG(("LINFUN(%d)\n", param));
}

/* Set the line drawing pattern */
static void hndl_linpat(pgc_core_t *pgc)
{
	uint16_t param;

	if (!pgc_param_word(pgc, (int16_t *)&param)) return;

	pgc->line_pattern = param;
	PGCLOG(("LINPAT(0x%04x)\n", param));
}

/* Set the polygon fill mode (0=hollow, 1=filled, 2=fast fill) */
static void hndl_prmfil(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc_param_byte(pgc, &param)) return;

	PGCLOG(("PRMFIL(%d)\n", param));
	if (param < 3) 
	{
		pgc->fill_mode = param;
	}
	else
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
	}
}


/* Set the 2D drawing position */
static void hndl_move(pgc_core_t *pgc)
{
	int32_t x = 0, y = 0;

	if (!pgc_param_coord(pgc, &x)) return;
	if (!pgc_param_coord(pgc, &y)) return;

	pgc->x = x;
	pgc->y = y;
	PGCLOG(("MOVE %x.%04x,%x.%04x\n", HWORD(x), LWORD(x), HWORD(y), LWORD(y)));
}

/* Set the 3D drawing position */
static void hndl_move3(pgc_core_t *pgc)
{
	int32_t x = 0, y = 0, z = 0;

	if (!pgc_param_coord(pgc, &x)) return;
	if (!pgc_param_coord(pgc, &y)) return;
	if (!pgc_param_coord(pgc, &z)) return;

	pgc->x = x;
	pgc->y = y;
	pgc->z = z;
}

/* Relative move (2D) */
static void hndl_mover(pgc_core_t *pgc)
{
	int32_t x = 0, y = 0;

	if (!pgc_param_coord(pgc, &x)) return;
	if (!pgc_param_coord(pgc, &y)) return;

	pgc->x += x;
	pgc->y += y;
}

/* Relative move (3D) */
static void hndl_mover3(pgc_core_t *pgc)
{
	int32_t x = 0, y = 0, z = 0;

	if (!pgc_param_coord(pgc, &x)) return;
	if (!pgc_param_coord(pgc, &y)) return;
	if (!pgc_param_coord(pgc, &z)) return;

	pgc->x += x;
	pgc->y += y;
	pgc->z += z;
}



/* Draw a line (using PGC fixed-point coordinates) */
uint16_t pgc_draw_line(pgc_core_t *pgc, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t linemask)
{

	PGCLOG(("pgc_draw_line: (%d,%d) to (%d,%d)\n", x0 >> 16, y0 >> 16,
					x1 >> 16, y1 >> 16));
	/* Convert from PGC fixed-point to device coordinates */
	x0 >>= 16;
	x1 >>= 16;
	y0 >>= 16;
	y1 >>= 16;

	pgc_ito_raster(pgc, &x0, &y0);
	pgc_ito_raster(pgc, &x1, &y1);

	return pgc_draw_line_r(pgc, x0, y0, x1, y1, linemask);
}

/* Draw a line (using raster coordinates)
   Bresenham's Algorithm from <https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C> 
 *
 * The line pattern mask to use is passed in. The return value is the line
 * pattern mask rotated by the number of points drawn.
 */
uint16_t pgc_draw_line_r(pgc_core_t *pgc, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t linemask)
{
	int32_t dx, dy, sx, sy, err, e2;

	dx = abs(x1 - x0);
	dy = abs(y1 - y0);
	sx = (x0 < x1) ? 1 : -1;
	sy = (y0 < y1) ? 1 : -1;
	err = (dx > dy ? dx : -dy) / 2;

	for(;;)	
	{
		if (linemask & 0x8000) 
		{
			pgc_plot(pgc, x0, y0);
			linemask = (linemask << 1) | 1;
		}
		else
		{
			linemask = (linemask << 1);
		}
		if (x0 == x1 && y0 == y1) break;
		e2 = err;
		if (e2 > -dx) { err -= dy; x0 += sx; }
		if (e2 <  dy) { err += dx; y0 += sy; }
	}
	return linemask;
}

/* Draw a horizontal line in the current fill pattern 
 * (using raster coordinates) */
void pgc_fill_line_r(pgc_core_t *pgc, int32_t x0, int32_t x1, int32_t y0)
{
	int32_t x;
	int32_t mask = 0x8000 >> (x0 & 0x0F);

	if (x0 > x1) { x = x1; x1 = x0; x0 = x; }

	for (x = x0; x <= x1; x++)
	{
		if (pgc->fill_pattern[y0 & 0x0F] & mask)
			pgc_plot(pgc, x, y0);
		mask = mask >> 1;
		if (mask == 0) mask = 0x8000; 
	} 
}

/* For sorting polygon nodes */
static int compare_double(const void *a, const void *b)
{
	const double *da = (const double *)a;
	const double *db = (const double *)b;

	if (*da > *db) return 1;
	if (*da < *db) return -1;
	return 0;
}

/* Draw a filled polygon (using PGC fixed-point coordinates) */
void pgc_fill_polygon(pgc_core_t *pgc, 
	unsigned corners, int32_t *x, int32_t *y)
{
	double *nodex;
	double *dx;
	double *dy;
	unsigned n, nodes, i, j;
	double ymin, ymax, ypos;

	PGCLOG(("pgc_fill_polygon(%d corners)\n", corners));

	if (corners < 2) return;	/* Degenerate polygon */
	nodex = malloc(corners * sizeof(double));
	dx    = malloc(corners * sizeof(double));
	dy    = malloc(corners * sizeof(double));
	if (!nodex || !dx || !dy) return;

	ymin = ymax = y[0] / 65536.0;
	for (n = 0; n < corners; n++)
	{
	/* Convert from PGC fixed-point to native floating-point */
		dx[n] = x[n] / 65536.0;
		dy[n] = y[n] / 65536.0;
	
		if (dy[n] < ymin) ymin = dy[n];
		if (dy[n] > ymax) ymax = dy[n];
	}
	/* Polygon fill. Based on <http://alienryderflex.com/polygon_fill/> */
	/* For each row, work out where the polygon lines intersect with 
	 * that row. */		
	for (ypos = ymin; ypos <= ymax; ypos++)
	{
		nodes = 0;
		j = corners - 1;
		for (i = 0; i < corners; i++)
		{
			if ((dy[i] < ypos && dy[j] >= ypos)
			||  (dy[j] < ypos && dy[i] >= ypos)) /* Line crosses */
			{
				nodex[nodes++] = dx[i] + (ypos-dy[i])/(dy[j]-dy[i]) * (dx[j] - dx[i]);
			}
			j = i;
		}

		/* Sort the intersections */
		if (nodes) qsort(nodex, nodes, sizeof(double), compare_double);
/*
		PGCLOG(("pgc_fill_polygon ypos=%f nodes=%d  ", ypos, nodes));
		for (i = 0; i < nodes; i++)
		{
			PGCLOG(("%f;", nodex[i]));
		}
		PGCLOG(("\n")); 
*/
		/* And fill between them */
		for (i = 0; i < nodes; i += 2)
		{
			int16_t x1 = nodex[i], x2 = nodex[i + 1],
				y1 = ypos, y2 = ypos;
			pgc_sto_raster(pgc, &x1, &y1);
			pgc_sto_raster(pgc, &x2, &y2);
/*			PGCLOG(("pgc_fill_polygon raster %d,%d to %d,%d\n", 
				x1, y1, x2, y2)); */
			pgc_fill_line_r(pgc, x1, x2, y1);
		}
	}
	free(nodex); 
	free(dx);
	free(dy);
}

	

/* Draw a filled ellipse (using PGC fixed-point coordinates) */
void pgc_draw_ellipse(pgc_core_t *pgc, int32_t x, int32_t y)
{
	/* Convert from PGC fixed-point to native floating-point */
	double h = y / 65536.0;
	double w = x / 65536.0;
	double y0 = pgc->y / 65536.0;
	double x0 = pgc->x / 65536.0;
	double ypos = 0.0, xpos = 0.0;
	double x1;
	double xlast = 0.0;
	int16_t linemask = pgc->line_pattern;

	pgc_dto_raster(pgc, &x0, &y0);
	PGCLOG(("Ellipse: Colour=%d Drawmode=%d fill=%d\n", pgc->colour, 
		pgc->draw_mode, pgc->fill_mode));
	for (ypos = 0; ypos <= h; ypos++)
	{
		if (ypos == 0)
		{
			if (pgc->fill_mode)
			{
				pgc_fill_line_r(pgc, x0 - w, x0 + w, y0);
			}
			if (linemask & 0x8000)
			{
				pgc_plot(pgc, x0 + w, y0);
				pgc_plot(pgc, x0 - w, y0);
				linemask = (linemask << 1) | 1;
			}
			else
			{
				linemask = linemask << 1;
			}
			xlast = w;
		}
		else
		{
			x1 = sqrt((h * h) - (ypos * ypos)) * w / h;

			if (pgc->fill_mode)
			{
				pgc_fill_line_r(pgc, x0 - x1, x0 + x1, y0 + ypos);
				pgc_fill_line_r(pgc, x0 - x1, x0 + x1, y0 - ypos);
			}

			/* Draw border */
			for (xpos = xlast; xpos >= x1; xpos--)
			{
				if (linemask & 0x8000)
				{
					pgc_plot(pgc, x0 + xpos, y0 + ypos);
					pgc_plot(pgc, x0 - xpos, y0 + ypos);
					pgc_plot(pgc, x0 + xpos, y0 - ypos);
					pgc_plot(pgc, x0 - xpos, y0 - ypos);
					linemask = (linemask << 1) | 1;
				}
				else
				{
					linemask = linemask << 1;
				}
				
			}
			xlast = x1;
		}
	}
}

/* Handle the ELIPSE (sic) command */
static void hndl_ellipse(pgc_core_t *pgc)
{
	int32_t x = 0, y = 0;

	if (!pgc_param_coord(pgc, &x)) return;
	if (!pgc_param_coord(pgc, &y)) return;

	pgc_draw_ellipse(pgc, x, y);
}




/* Handle the POLY command */
static void hndl_poly(pgc_core_t *pgc)
{
	uint8_t count;
	int32_t x[256];
	int32_t y[256];
	int32_t n;

	if (!pgc_param_byte(pgc, &count)) return;

	for (n = 0; n < count; n++)
	{
		if (!pgc_param_coord(pgc, &x[n])) return;
		if (!pgc_param_coord(pgc, &y[n])) return;
	}
	PGCLOG(("POLY (%d)\n", count));
}

/* Parse but don't execute a POLY command (for adding to a command list) */
static int parse_poly(pgc_core_t *pgc, pgc_commandlist_t *cl, int c)
{
	uint8_t count;

	PGCLOG(("parse_poly\n"));
	if (!pgc_param_byte(pgc, &count)) return 0;
	PGCLOG(("parse_poly: count=%02x\n", count));

	if (!pgc_commandlist_append(cl, count))
	{
		pgc_error(pgc, PGC_ERROR_OVERFLOW);
		return 0;	
	}
	PGCLOG(("parse_poly: parse %d coords\n", 2 * count));
	return pgc_parse_coords(pgc, cl, 2 * count);
}

	
/* Parse but don't execute a command with a fixed number of byte parameters */
int pgc_parse_bytes(pgc_core_t *pgc, pgc_commandlist_t *cl, int count)
{
	uint8_t *param = malloc(count);
	int n;

	if (!param)
	{
		pgc_error(pgc, PGC_ERROR_OVERFLOW);
		return 0;	
	}
	for (n = 0; n < count; n++)
	{
		if (!pgc_param_byte(pgc, &param[n])) 
		{
			free(param);
			return 0;
		}
	}
	for (n = 0; n < count; n++)
	{
		if (!pgc_commandlist_append(cl, param[n]))
		{
			pgc_error(pgc, PGC_ERROR_OVERFLOW);
			free(param);
			return 0;	
		}
	}
	free(param);
	return 1;
}


/* Parse but don't execute a command with a fixed number of word parameters */
int pgc_parse_words(pgc_core_t *pgc, pgc_commandlist_t *cl, int count)
{
	int16_t *param = malloc(count * sizeof(int16_t));
	int n;

	if (!param)
	{
		pgc_error(pgc, PGC_ERROR_OVERFLOW);
		return 0;	
	}
	for (n = 0; n < count; n++)
	{
		if (!pgc_param_word(pgc, &param[n])) return 0;
	}
	for (n = 0; n < count; n++)
	{
		if (!pgc_commandlist_append(cl, param[n] & 0xFF) ||
		    !pgc_commandlist_append(cl, param[n] >> 8))
		{
			pgc_error(pgc, PGC_ERROR_OVERFLOW);
			free(param);
			return 0;	
		}
	}
	return 1;
}



/* Parse but don't execute a command with a fixed number of coord parameters */
int pgc_parse_coords(pgc_core_t *pgc, pgc_commandlist_t *cl, int count)
{
	int32_t *param = malloc(count * sizeof(int32_t));
	int n;

	if (!param)
	{
		pgc_error(pgc, PGC_ERROR_OVERFLOW);
		return 0;	
	}
	for (n = 0; n < count; n++)
	{
		if (!pgc_param_coord(pgc, &param[n])) return 0;
	}
/* Here's how the real PGC serialises coords:
 *
 * 100.5 -> 64 00 00 80  ie 0064.8000 
 * 100.3 -> 64 00 CD 4C  ie 0064.4CCD
 * 
 */
	for (n = 0; n < count; n++)
	{
		/* Serialise integer part */
		if (!pgc_commandlist_append(cl, (param[n] >> 16) & 0xFF) ||
		    !pgc_commandlist_append(cl, (param[n] >> 24) & 0xFF) ||
		/* Serialise fraction part */
		    !pgc_commandlist_append(cl, (param[n]      ) & 0xFF) ||
		    !pgc_commandlist_append(cl, (param[n] >>  8) & 0xFF))
		{
			pgc_error(pgc, PGC_ERROR_OVERFLOW);
			free(param);
			return 0;	
		}
	}
	return 1;
}



/* Handle the DISPLAY command */
static void hndl_display(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc_param_byte(pgc, &param)) return;

	PGCLOG(("DISPLAY(%d)\n", param));
	if (param > 1) 
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
	}
	else
	{
		pgc_setdisplay(pgc, param);
	}
}


/* Handle the IMAGEW command (memory to screen blit) */
static void hndl_imagew(pgc_core_t *pgc)
{
	int16_t row, col1, col2;
	uint8_t v1, v2;

	if (!pgc_param_word(pgc, &row)) return;
	if (!pgc_param_word(pgc, &col1)) return;
	if (!pgc_param_word(pgc, &col2)) return;

	if (row >= pgc->screenh || col1 >= pgc->maxw || col2 >= pgc->maxw)
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
		return;
	}

	/* In ASCII mode, what is written is a stream of bytes */
	if (pgc->ascii_mode)
	{
		while (col1 <= col2)
		{
			if (!pgc_param_byte(pgc, &v1)) return;
			pgc_write_pixel(pgc, col1, row, v1);
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
					pgc_write_pixel(pgc, col1, row, v2);
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
					pgc_write_pixel(pgc, col1, row, v2);
					++col1;
					--v1;
				}
			}	
		}
	}
}


/* Select one of the built-in palettes */
static void pgc_init_lut(pgc_core_t *pgc, int param)
{
	if (param >= 0 && param < 6)
	{
		memcpy(pgc->palette, init_palette[param], sizeof(pgc->palette));
	}
	else if (param == 0xFF)
	{
		memcpy(pgc->palette, pgc->userpal, sizeof(pgc->palette));
	}
	else
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
	}
}

/* Save the current palette */
static void hndl_lutsav(pgc_core_t *pgc)
{
	memcpy(pgc->userpal, pgc->palette, sizeof(pgc->palette));
}


/* Handle LUTINT (select palette) */
static void hndl_lutint(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc_param_byte(pgc, &param)) return;

	pgc_init_lut(pgc, param);
}


/* Handle LUTRD (read palette register) */
static void hndl_lutrd(pgc_core_t *pgc)
{
	uint8_t param;
	uint32_t col;

	if (!pgc_param_byte(pgc, &param)) return;

	col = pgc->palette[param];

	pgc_result_byte(pgc, (col >> 20) & 0x0F);	
	pgc_result_byte(pgc, (col >> 12) & 0x0F);	
	pgc_result_byte(pgc, (col >>  4) & 0x0F);	
}

/* Handle LUT (write palette register) */
static void hndl_lut(pgc_core_t *pgc)
{
	uint8_t param[4];
	int n;

	for (n = 0; n < 4; n++)
	{
		if (!pgc_param_byte(pgc, &param[n])) return;
		if (n > 0 && param[n] > 15)
		{
			pgc_error(pgc, PGC_ERROR_RANGE);
			param[n] &= 0x0F;
		}
	}
	pgc->palette[param[0]] = makecol((param[1] * 0x11), 
					 (param[2] * 0x11), 
					 (param[3] * 0x11));
}

/* LUT8RD and LUT8 are extensions implemented by several PGC clones, so
 * here are functions that implement them even though they aren't 
 * used by the PGC */
void pgc_hndl_lut8rd(pgc_core_t *pgc)
{
	uint8_t param;
	uint32_t col;

	if (!pgc_param_byte(pgc, &param)) return;

	col = pgc->palette[param];

	pgc_result_byte(pgc, (col >> 16) & 0xFF);	
	pgc_result_byte(pgc, (col >>  8) & 0xFF);	
	pgc_result_byte(pgc, col & 0xFF);
}

void pgc_hndl_lut8(pgc_core_t *pgc)
{
	uint8_t param[4];
	int n;

	for (n = 0; n < 4; n++)
	{
		if (!pgc_param_byte(pgc, &param[n])) return;
	}
	pgc->palette[param[0]] = makecol((param[1]), (param[2]), (param[3]));
}


/* Handle AREAPT (set 16x16 fill pattern) */
static void hndl_areapt(pgc_core_t *pgc)
{
	int16_t pattern[16];
	int n;

	for (n = 0; n < 16; n++)
	{
		if (!pgc_param_word(pgc, &pattern[n])) return;
	}
	memcpy(pgc->fill_pattern, pattern, sizeof(pgc->fill_pattern));
	PGCLOG(("AREAPT(%04x %04x %04x %04x...)\n",
		pattern[0] & 0xFFFF, pattern[1] & 0xFFFF, 
		pattern[2] & 0xFFFF, pattern[3] & 0xFFFF));
}


/* Handle CA (select ASCII mode) */
static void hndl_ca(pgc_core_t *pgc)
{
	pgc->ascii_mode = 1;
}

/* Handle CX (select hex mode) */
static void hndl_cx(pgc_core_t *pgc)
{
	pgc->ascii_mode = 0;
}

/* CA and CX remain valid in hex mode; they are handled as command 0x43 ('C') 
 * with a one-byte parameter */
static void hndl_c(pgc_core_t *pgc)
{
	uint8_t param;

	if (!pgc->inputbyte(pgc, &param)) return;

	if (param == 'A') pgc->ascii_mode = 1;
	if (param == 'X') pgc->ascii_mode = 0;
}

/* RESETF resets the PGC */
static void hndl_resetf(pgc_core_t *pgc)
{
	pgc_reset(pgc);
}



/* TJUST sets text justify settings */
static void hndl_tjust(pgc_core_t *pgc)
{
	uint8_t param[2];

	if (!pgc->inputbyte(pgc, &param[0])) return;
	if (!pgc->inputbyte(pgc, &param[1])) return;

	if (param[0] >= 1 && param[0] <= 3 &&
	    param[1] >= 1 && param[1] <= 3)
	{
		pgc->tjust_h = param[0];
		pgc->tjust_v = param[1];
	}
	else
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
	}
}

/* TSIZE controls text horizontal spacing */
static void hndl_tsize(pgc_core_t *pgc)
{
	int32_t param = 0;

	if (!pgc_param_coord(pgc, &param)) return;

	pgc->tsize = param;
	PGCLOG(("TSIZE %d\n", param));
}


/* VWPORT sets up the viewport (roughly, the clip rectangle) in raster 
 * coordinates, measured from the bottom left of the screen */
static void hndl_vwport(pgc_core_t *pgc)
{
	int16_t x1, x2, y1, y2;

	if (!pgc_param_word(pgc, &x1)) return;
	if (!pgc_param_word(pgc, &x2)) return;
	if (!pgc_param_word(pgc, &y1)) return;
	if (!pgc_param_word(pgc, &y2)) return;

	PGCLOG(("VWPORT %d,%d,%d,%d\n", x1,x2,y1,y2));
	pgc->vp_x1 = x1;
	pgc->vp_x2 = x2;
	pgc->vp_y1 = y1;
	pgc->vp_y2 = y2;
}

/* WINDOW defines the coordinate system in use */
static void hndl_window(pgc_core_t *pgc)
{
	int16_t x1, x2, y1, y2;

	if (!pgc_param_word(pgc, &x1)) return;
	if (!pgc_param_word(pgc, &x2)) return;
	if (!pgc_param_word(pgc, &y1)) return;
	if (!pgc_param_word(pgc, &y2)) return;

	PGCLOG(("WINDOW %d,%d,%d,%d\n", x1,x2,y1,y2));
	pgc->win_x1 = x1;
	pgc->win_x2 = x2;
	pgc->win_y1 = y1;
	pgc->win_y2 = y2;
}



/* The list of commands implemented by this mini-PGC. In order to support
 * the original PGC and clones, we support two lists; core commands (listed
 * below) and subclass commands (listed in the clone).
 * 
 * Each row has five parameters:
 * 	ASCII-mode command
 * 	Hex-mode command
 * 	Function that executes this command
 * 	Function that parses this command when building a command list
 * 	Parameter for the parse function
 *
 * TODO: This list omits numerous commands present in a genuine PGC
 *       (ARC, AREA, AREABC, BUFFER, CIRCLE etc etc).
 * TODO: Some commands don't have a parse function (for example, IMAGEW) 
 *
 * The following ASCII entries have special meaning:
 * ~~~~~~  command is valid only in hex mode
 * ******  end of subclass command list, now process core command list
 * @@@@@@  end of core command list
 *
 */
static const pgc_command_t pgc_core_commands[] =
{
	{ "AREAPT", 0xE7, hndl_areapt, pgc_parse_words, 16 },
	{ "AP",     0xE7, hndl_areapt, pgc_parse_words, 16 },
	{ "~~~~~~", 0x43, hndl_c },	/* Handle CA / CX in hex mode */
	{ "CA",     0xD2, hndl_ca },		
	{ "CLBEG",  0x70, hndl_clbeg },
	{ "CB",     0x70, hndl_clbeg },
	{ "CLDEL",  0x74, hndl_cldel, pgc_parse_bytes, 1 },
	{ "CD",     0x74, hndl_cldel, pgc_parse_bytes, 1 },
	{ "CLEND",  0x71, hndl_clend },
	{ "CLRUN",  0x72, hndl_clrun, pgc_parse_bytes, 1 },
	{ "CR",     0x72, hndl_clrun, pgc_parse_bytes, 1 },
	{ "CLRD",   0x75, hndl_clread, pgc_parse_bytes, 1 },
	{ "CRD",    0x75, hndl_clread, pgc_parse_bytes, 1 },
	{ "CLOOP",  0x73, hndl_cloop },
	{ "CL",     0x73, hndl_cloop },
	{ "CLEARS", 0x0F, hndl_clears, pgc_parse_bytes, 1 },
	{ "CLS",    0x0F, hndl_clears, pgc_parse_bytes, 1 },
	{ "COLOR",  0x06, hndl_color, pgc_parse_bytes, 1 },
	{ "C",      0x06, hndl_color, pgc_parse_bytes, 1 },
	{ "CX",     0xD1, hndl_cx },
	{ "DISPLA", 0xD0, hndl_display, pgc_parse_bytes, 1 },
	{ "DI",     0xD0, hndl_display, pgc_parse_bytes, 1 },
	{ "ELIPSE", 0x39, hndl_ellipse, pgc_parse_coords, 2 },
	{ "EL",     0x39, hndl_ellipse, pgc_parse_coords, 2 },
	{ "IMAGEW", 0xD9, hndl_imagew },
	{ "IW",     0xD9, hndl_imagew },
	{ "LINFUN", 0xEB, hndl_linfun, pgc_parse_bytes, 1 },
	{ "LF",     0xEB, hndl_linfun, pgc_parse_bytes, 1 },
	{ "LINPAT", 0xEA, hndl_linpat, pgc_parse_words, 1 },
	{ "LP",     0xEA, hndl_linpat, pgc_parse_words, 1 },
	{ "LUTINT", 0xEC, hndl_lutint, pgc_parse_bytes, 1 },
	{ "LI",     0xEC, hndl_lutint, pgc_parse_bytes, 1 },
	{ "LUTRD",  0x50, hndl_lutrd, pgc_parse_bytes, 1 },
	{ "LUTSAV", 0xED, hndl_lutsav, NULL, 0 },
	{ "LUT",    0xEE, hndl_lut, pgc_parse_bytes, 4 },
	{ "MOVE",   0x10, hndl_move, pgc_parse_coords, 2 },
	{ "M",      0x10, hndl_move, pgc_parse_coords, 2 },
	{ "MOVE3",  0x12, hndl_move3, pgc_parse_coords, 3 },
	{ "M3",     0x12, hndl_move3, pgc_parse_coords, 3 },
	{ "MOVER",  0x11, hndl_mover, pgc_parse_coords, 2 },
	{ "MR",     0x11, hndl_mover, pgc_parse_coords, 2 },
	{ "MOVER3", 0x13, hndl_mover3, pgc_parse_coords, 3 },
	{ "MR3",    0x13, hndl_mover3, pgc_parse_coords, 3 },
	{ "PRMFIL", 0xE9, hndl_prmfil, pgc_parse_bytes, 1 },
	{ "PF",     0xE9, hndl_prmfil, pgc_parse_bytes, 1 },
	{ "POLY",   0x30, hndl_poly,   parse_poly },
	{ "P",      0x30, hndl_poly,   parse_poly },
	{ "RESETF", 0x04, hndl_resetf, NULL, 0 },
	{ "RF",     0x04, hndl_resetf, NULL, 0 },
	{ "TJUST",  0x85, hndl_tjust, pgc_parse_bytes, 2 },
	{ "TJ",     0x85, hndl_tjust, pgc_parse_bytes, 2 },
	{ "TSIZE",  0x81, hndl_tsize, pgc_parse_coords, 1 },
	{ "TS",     0x81, hndl_tsize, pgc_parse_coords, 1 },
	{ "VWPORT", 0xB2, hndl_vwport, pgc_parse_words, 4 },
	{ "VWP",    0xB2, hndl_vwport, pgc_parse_words, 4 },
	{ "WINDOW", 0xB3, hndl_window, pgc_parse_words, 4 },
	{ "WI",     0xB3, hndl_window, pgc_parse_words, 4 }, 

	{ "@@@@@@", 0x00, NULL }
};


/* Writes to CGA registers are copied into the transfer memory buffer */
void pgc_out(uint16_t addr, uint8_t val, void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;

	switch(addr)
	{
		case 0x3D0: case 0x3D2: case 0x3D4: case 0x3D6:
			    pgc->mapram[0x3D0] = val; break;
		case 0x3D1: case 0x3D3: case 0x3D5: case 0x3D7:
			    if (pgc->mapram[0x3D0] < 18)
			    {
			        pgc->mapram[0x3E0 + pgc->mapram[0x3D0]] = val; 
			    }
			    break;
		case 0x3D8: pgc->mapram[0x3D8] = val; break;
		case 0x3D9: pgc->mapram[0x3D9] = val; break;
	}
	
}

/* Read back the CGA registers */
uint8_t pgc_in(uint16_t addr, void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;

	switch(addr)
	{
		case 0x3D0: case 0x3D2: case 0x3D4: case 0x3D6:
			    return pgc->mapram[0x3D0];
		case 0x3D1: case 0x3D3: case 0x3D5: case 0x3D7:
			    if (pgc->mapram[0x3D0] < 18)
			    {
			        return pgc->mapram[0x3E0 + pgc->mapram[0x3D0]];
			    }
			    return 0xFF;

		case 0x3D8: return pgc->mapram[0x3D8];
		case 0x3D9: return pgc->mapram[0x3D9];
		case 0x3DA: return pgc->mapram[0x3DA];
	}
	return 0xFF;
}

/* Memory write to the transfer buffer */
void pgc_write(uint32_t addr, uint8_t val, void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;

	/* It seems variable whether the PGC maps 1k or 2k at 0xC6000. 
	 * Map 2k here in case a clone requires it */
	if (addr >= 0xC6000 && addr < 0xC6800)
	{
		addr &= 0x7FF;

		/* If one of the FIFOs has been updated, this may cause
		 * the drawing thread to be woken */

		if (pgc->mapram[addr] != val)
		{
			pgc->mapram[addr] = val;
			switch (addr)
			{
				case 0x300: /* Input write pointer */
				if (pgc->waiting_input_fifo &&
					pgc->mapram[0x300] != pgc->mapram[0x301])
				{
					pgc->waiting_input_fifo = 0;
					pgc_wake(pgc);
				}
				break;

				case 0x303:	/* Output read pointer */
				if (pgc->waiting_output_fifo &&
					pgc->mapram[0x302] != (uint8_t)(pgc->mapram[0x303] - 1))	
				{
					pgc->waiting_output_fifo = 0;
					pgc_wake(pgc);
				}
				break;
			
				case 0x305:	/* Error read pointer */
				if (pgc->waiting_error_fifo &&
					pgc->mapram[0x304] != (uint8_t)(pgc->mapram[0x305] - 1))
				{
					pgc->waiting_error_fifo = 0;
					pgc_wake(pgc);
				}
				break;

				case 0x306:	/* Cold start flag */
				/* XXX This should be in IM-1024 specific code */
				pgc->mapram[0x306] = 0;
				break;

				case 0x030C:	/* Display type */
				pgc_setdisplay(p, pgc->mapram[0x30C]);
				pgc->mapram[0x30D] = pgc->mapram[0x30C];
				break;


				case 0x3FF:	/* Reboot the PGC 
						(handled on core thread) */
				pgc_wake(pgc);
				break;
			} // end switch (addr)
		}
	}
	if (addr >= 0xB8000 && addr < 0xBC000 && pgc->cga_selected)
	{
		addr &= 0x3FFF;
		pgc->cga_vram[addr] = val;
	}
}


uint8_t pgc_read(uint32_t addr, void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;

	if (addr >= 0xC6000 && addr < 0xC6800)
	{
		addr &= 0x7FF;

		return pgc->mapram[addr];
	}
	if (addr >= 0xB8000 && addr < 0xBC000 && pgc->cga_selected)
	{
		addr &= 0x3FFF;
		return pgc->cga_vram[addr];
	}
	return 0xFF;
}

/* Called by the drawing thread to read the next byte from the input 
 * buffer. If no byte available will sleep until one is. Returns 0 if 
 * a PGC reset has been triggered by a write to 0xC63FF */
int pgc_input_byte(pgc_core_t *pgc, uint8_t *result)
{
	/* If input buffer empty, wait for it to fill */
	while (pgc->mapram[0x300] == pgc->mapram[0x301])	
	{
		pgc->waiting_input_fifo = 1;
		pgc_sleep(pgc);	
	}
	if (pgc->mapram[0x3FF])	/* Reset triggered */
	{
		pgc_reset(pgc);
		return 0;
	}

	*result = pgc->mapram[pgc->mapram[0x301]];
	++pgc->mapram[0x301];
	return 1;
}

/* Called by the drawing thread to write a byte to the output buffer.
 * If buffer is full will sleep until it is not. Returns 0 if
 * a PGC reset has been triggered by a write to 0xC63FF */
int pgc_output_byte(pgc_core_t *pgc, uint8_t val)
{
	/* If output buffer full, wait for it to empty */
	while (pgc->mapram[0x302] == (uint8_t)(pgc->mapram[0x303] - 1))	
	{
		PGCLOG(("Output buffer state: %02x %02x  Sleeping\n",
			pgc->mapram[0x302], pgc->mapram[0x303]));
		pgc->waiting_output_fifo = 1;
		pgc_sleep(pgc);	
	}
	if (pgc->mapram[0x3FF])	/* Reset triggered */
	{
		pgc_reset(pgc);
		return 0;
	}
	pgc->mapram[0x100 + pgc->mapram[0x302]] = val;
	++pgc->mapram[0x302];
/*
	PGCLOG(("Output %02x: new state: %02x %02x\n", val,
		pgc->mapram[0x302], pgc->mapram[0x303])); */
	return 1;
}

/* Helper to write an entire string to the output buffer */
int pgc_output_string(pgc_core_t *pgc, const char *s)
{
	while (*s)
	{
		if (!pgc_output_byte(pgc, *s)) return 0;
		++s;
	}
	return 1;
}

/* As pgc_output_byte, for the error buffer */
int pgc_error_byte(pgc_core_t *pgc, uint8_t val)
{
	/* If error buffer full, wait for it to empty */
	while (pgc->mapram[0x304] == pgc->mapram[0x305] - 1)	
	{
		pgc->waiting_error_fifo = 1;
		pgc_sleep(pgc);	
	}
	if (pgc->mapram[0x3FF])	/* Reset triggered */
	{
		pgc_reset(pgc);
		return 0;
	}
	pgc->mapram[0x200 + pgc->mapram[0x304]] = val;
	++pgc->mapram[0x304];
	return 1;
}

/* As pgc_output_string, for the error buffer */
int pgc_error_string(pgc_core_t *pgc, const char *s)
{
	while (*s)
	{
		if (!pgc_error_byte(pgc, *s)) return 0;
		++s;
	}
	return 1;
}


/* Report an error, either in ASCII or in hex */
int pgc_error(pgc_core_t *pgc, int err)
{
	if (pgc->mapram[0x307])	/* Errors enabled? */
	{
		if (pgc->ascii_mode)
		{
			if (err >= PGC_ERROR_RANGE && err <= PGC_ERROR_MISSING)
				return pgc_error_string(pgc, pgc_err_msgs[err]);
			return pgc_error_string(pgc, "Unknown error\r");
		}
		else
		{
			return pgc_error_byte(pgc, err);
		}
	}
	return 1;
}


static inline int is_whitespace(char ch)
{
	return (ch != 0 && strchr(" \r\n\t,;()+-", ch) != NULL);
}


/* Read a byte and interpret as ASCII: ignore control characters other than
 * CR, LF or tab. */
int pgc_input_char(pgc_core_t *pgc, char *result)
{
	uint8_t ch;

	while (1)
	{
		if (!pgc->inputbyte(pgc, &ch)) return 0;

		ch &= 0x7F;
		if (ch == '\r' || ch == '\n' || ch == '\t' || ch >= ' ')
		{
			*result = toupper(ch);
			return 1;
		}
	}
}


/* Parameter passed is not a number: abort */
static int err_digit(pgc_core_t *pgc)
{
	uint8_t asc;

	do	/* Swallow everything until the next separator */
	{
		if (!pgc->inputbyte(pgc, &asc)) return 0;
	}	
	while (!is_whitespace(asc));	
	pgc_error(pgc, PGC_ERROR_DIGIT);
	return 0;
}


typedef enum parse_state_t
{
	PS_MAIN,
	PS_FRACTION,
	PS_EXPONENT
} parse_state_t;

/* Read in a PGC coordinate, either as hex (4 bytes) or ASCII (xxxx.yyyyEeee)
 * Returns 0 if PGC reset detected while the value is being read */
int pgc_param_coord(pgc_core_t *pgc, int32_t *value)
{
	uint8_t asc;
	int sign = 1;
	int esign = 1;
	int n;
	uint16_t dp = 1;
	uint16_t integer = 0;
	uint16_t frac = 0;
	uint16_t exponent = 0;
	uint32_t res;
	parse_state_t state = PS_MAIN;
	uint8_t encoded[4];

	/* If there is a command list running, pull the bytes out of that
	 * command list */
	if (pgc->clcur) 
	{
		for (n = 0; n < 4; n++)
		{
			if (!pgc_clist_byte(pgc, &encoded[n])) return 0;
		}
		integer = (((int16_t)encoded[1]) << 8) | encoded[0];
		frac    = (((int16_t)encoded[3]) << 8) | encoded[2];

		*value = (((int32_t)integer) << 16) | frac;
		return 1;
	}
	/* If in hex mode, read in the encoded integer and fraction parts 
	 * from the hex stream */
	if (!pgc->ascii_mode)
	{
		for (n = 0; n < 4; n++)
		{
			if (!pgc->inputbyte(pgc, &encoded[n])) return 0;
		}
		integer = (((int16_t)encoded[1]) << 8) | encoded[0];
		frac    = (((int16_t)encoded[3]) << 8) | encoded[2];

		*value = (((int32_t)integer) << 16) | frac;
		return 1;
	}
	/* Parsing an ASCII value */

	/* Skip separators */
	do
	{
		if (!pgc->inputbyte(pgc, &asc)) return 0;
		if (asc == '-') sign = -1;
	}	
	while (is_whitespace(asc));	

	/* There had better be a digit next */
	if (!isdigit(asc)) 	
	{
		pgc_error(pgc, PGC_ERROR_MISSING);
		return 0;
	}
	do
	{
		switch (asc)
		{
/* Decimal point is acceptable in 'main' state (start of fraction)
 * not otherwise */
			case '.':
				if (state == PS_MAIN)
				{
					if (!pgc->inputbyte(pgc, &asc)) return 0;
					state = PS_FRACTION;
					continue;
				}
				else
				{
					pgc_error(pgc, PGC_ERROR_MISSING);
					return err_digit(pgc);
				}
				break;
			/* Scientific notation */
			case 'd': case 'D': case 'e': case 'E':
				esign = 1;	
				if (!pgc->inputbyte(pgc, &asc)) return 0;
				if (asc == '-') 
				{
					sign = -1;
					if (!pgc->inputbyte(pgc, &asc)) return 0;
				}
				state = PS_EXPONENT;
				continue;
			/* Should be a number or a separator */
			default:
				if (is_whitespace(asc)) break;
				if (!isdigit(asc))
				{
					pgc_error(pgc, PGC_ERROR_MISSING);
					return err_digit(pgc);
				}
				asc -= '0';	/* asc is digit */
				switch (state)
				{
					case PS_MAIN: 
						integer = (integer * 10)+asc;
						if (integer & 0x8000)	/* Overflow */
						{
							pgc_error(pgc, PGC_ERROR_RANGE);
							integer = 0x7FFF;	
						}
						break;
					case PS_FRACTION:
						frac = (frac * 10)+asc;
						dp *= 10;
						break;
					case PS_EXPONENT:
						exponent = (exponent * 10)+asc;
						break;
				}
			
		}
		if (!pgc->inputbyte(pgc, &asc)) return 0;
	}	
	while (!is_whitespace(asc));	


	res = (frac << 16) / dp;
	PGCLOG(("integer=%u frac=%u exponent=%u dp=%d res=0x%08x\n",
		integer, frac, exponent, dp, res));

	res = (res & 0xFFFF) | (integer << 16);
	if (exponent)
	{
		for (n = 0; n < exponent; n++)
		{
			if (esign > 0) res *= 10;
			else	       res /= 10;
		}
	}
	*value = sign*res;	

	return 1;
}

/* Pull the next byte from the current command list */
int pgc_clist_byte(pgc_core_t *pgc, uint8_t *val)
{
	if (pgc->clcur == NULL) return 0;

	if (pgc->clcur->rdptr < pgc->clcur->wrptr)
	{
		*val = pgc->clcur->list[pgc->clcur->rdptr++];
	}
	else 
	{
		*val = 0;
	}
	/* If we've reached the end, reset to the beginning and
	 * (if repeating) run the repeat */
	if (pgc->clcur->rdptr >= pgc->clcur->wrptr)
	{
		pgc->clcur->rdptr = 0;
		--pgc->clcur->repeat;
		if (pgc->clcur->repeat == 0)
		{
			pgc->clcur = pgc->clcur->chain;
		}
	}
	return 1;
}


/* Read in a byte, either as hex (1 byte) or ASCII (decimal).
 * Returns 0 if PGC reset detected while the value is being read */
int pgc_param_byte(pgc_core_t *pgc, uint8_t *val)
{
	int32_t c;

	if (pgc->clcur) return pgc_clist_byte(pgc, val);

	if (!pgc->ascii_mode) return pgc->inputbyte(pgc, val);

	if (!pgc_param_coord(pgc, &c)) return 0;
	c = (c >> 16);	/* Drop fractional part */

	if (c > 255)
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
		return 0;
	}
	*val = (uint8_t)c;
	return 1;
}

/* Read in a word, either as hex (2 bytes) or ASCII (decimal).
 * Returns 0 if PGC reset detected while the value is being read */
int pgc_param_word(pgc_core_t *pgc, int16_t *val)
{
	int32_t c;

	if (pgc->clcur) 
	{
		uint8_t lo, hi;

		if (!pgc_clist_byte(pgc, &lo)) return 0;
		if (!pgc_clist_byte(pgc, &hi)) return 0;
		*val = (((int16_t)hi) << 8) | lo;
		return 1;
	}

	if (!pgc->ascii_mode)
	{
		uint8_t lo, hi;

		if (!pgc->inputbyte(pgc, &lo)) return 0;
		if (!pgc->inputbyte(pgc, &hi)) return 0;
		*val = (((int16_t)hi) << 8) | lo;
		return 1;
	}

	if (!pgc_param_coord(pgc, &c)) return 0;

	c = (c >> 16);
	if (c > 0x7FFF || c < -0x7FFF)
	{
		pgc_error(pgc, PGC_ERROR_RANGE);
		return 0;
	}
	*val = (int16_t)c;
	return 1;
}


/* Output a byte, either as hex or ASCII depending on the mode */
int pgc_result_byte(pgc_core_t *pgc, uint8_t val)
{
	char buf[20];

	if (!pgc->ascii_mode) return pgc_output_byte(pgc, val);

	if (pgc->result_count) 
	{
		if (!pgc_output_byte(pgc, ',')) return 0;
	}
	sprintf(buf, "%d", val);
	++pgc->result_count;
	return pgc_output_string(pgc, buf);	
}


/* Output a word, either as hex or ASCII depending on the mode */
int pgc_result_word(pgc_core_t *pgc, int16_t val)
{
	char buf[20];

	if (!pgc->ascii_mode) 
	{
		if (!pgc_output_byte(pgc, val & 0xFF)) return 0;
		return pgc_output_byte(pgc, val >> 8);
	}

	if (pgc->result_count) 
	{
		if (!pgc_output_byte(pgc, ',')) return 0;
	}
	sprintf(buf, "%d", val);
	++pgc->result_count;
	return pgc_output_string(pgc, buf);	
}


/* Write a screen pixel (x and y are raster coordinates, ink is the
 * value to write) */
void pgc_write_pixel(pgc_core_t *pgc, uint16_t x, uint16_t y, uint8_t ink)
{
	uint8_t *vram;

	/* Suppress out-of-range writes; clip to viewport */
	if (x < pgc->vp_x1 || x > pgc->vp_x2 || x >= pgc->maxw || 
	    y < pgc->vp_y1 || y > pgc->vp_y2 || y >= pgc->maxh) 
	{
		PGCLOG(("pgc_write_pixel clipped: (%d,%d) "
			"vp_x1=%d vp_y1=%d vp_x2=%d vp_y2=%d "
			"ink=0x%02x\n", x, y, 
				pgc->vp_x1, pgc->vp_y1, 
				pgc->vp_x2, pgc->vp_y2, ink));
		return;
	}
	vram = pgc_vram_addr(pgc, x, y);
	if (vram) *vram = ink;
}

/* Read a screen pixel (x and y are raster coordinates) */
uint8_t pgc_read_pixel(pgc_core_t *pgc, uint16_t x, uint16_t y)
{
	uint8_t *vram;

	/* Suppress out-of-range reads */
	if (x >= pgc->maxw || y >= pgc->maxh) 
	{
		return 0;
	}
	vram = pgc_vram_addr(pgc, x, y);
	if (vram) return *vram;
	return 0;
}




/* Plot a point in the current colour and draw mode. Raster coordinates. */
void pgc_plot(pgc_core_t *pgc, uint16_t x, uint16_t y)
{
	uint8_t *vram;

	/* Only allow plotting within the current viewport. */
	if (x < pgc->vp_x1 || x > pgc->vp_x2 || x >= pgc->maxw || 
	    y < pgc->vp_y1 || y > pgc->vp_y2 || y >= pgc->maxh) 
	{
		PGCLOG(("pgc_plot clipped: (%d,%d) %d <= x <= %d; %d <= y <= %d; "
			"mode=%d ink=0x%02x\n", x, y, 
			pgc->vp_x1, pgc->vp_x2, pgc->vp_y1, pgc->vp_y2,
			pgc->draw_mode, pgc->colour));
		return;
	}
	vram = pgc_vram_addr(pgc, x, y);
	if (!vram) return;

	/* TODO: Does not implement the PGC plane mask (set by MASK) */
	switch (pgc->draw_mode)
	{
		default:
		case 0: *vram = pgc->colour; break;	/* Write */
		case 1: *vram ^= 0xFF; break;		/* Invert */
		case 2: *vram ^= pgc->colour; break;	/* XOR colour */
		case 3: *vram &= pgc->colour; break;	/* AND */
	}
}


/* Given raster coordinates, find the matching address in PGC video RAM */
uint8_t *pgc_vram_addr(pgc_core_t *pgc, int16_t x, int16_t y)
{
	int offset;

/* We work from the bottom left-hand corner */
	if (y < 0 || y >= pgc->maxh || x < 0 || x >= pgc->maxw) return NULL;

	offset = (pgc->maxh - 1 -y) * (pgc->maxw) + x;
//	PGCLOG(("pgc_vram_addr x=%d y=%d offset=%d\n", x, y, offset);

	if (offset < 0 || offset >= (pgc->maxw * pgc->maxh))
	{
		return NULL;
	}
	return &pgc->vram[offset];
}



/* Read in the next command, either as hex (1 byte) or ASCII (up to 6 
 * characters) */
int pgc_read_command(pgc_core_t *pgc)
{
	if (pgc->clcur)
	{
		return pgc_clist_byte(pgc, &pgc->hex_command);
	}
	else if (pgc->ascii_mode)
	{
		char ch;	
		int count  = 0;

		while (count < 7)
		{
			if (!pgc_input_char(pgc, &ch)) return 0;
			if (is_whitespace(ch))
			{
				/* Pad to 6 characters */
				while (count < 6)
					pgc->asc_command[count++] = ' ';
				pgc->asc_command[6] = 0;
				return 1;
			}
			pgc->asc_command[count++] = toupper(ch);
		}
		return 1;
	}
	else
	{	
		return pgc->inputbyte(pgc, &pgc->hex_command);
	}
}


/* Read in the next command and parse it */
static int pgc_parse_command(pgc_core_t *pgc, const pgc_command_t **pcmd)
{
	const pgc_command_t *cmd;
	char match[7];

	*pcmd = NULL;
	pgc->hex_command = 0;
	memset(pgc->asc_command, ' ', 6);
	pgc->asc_command[6] = 0;
	if (!pgc_read_command(pgc))
	{
		/* PGC has been reset */
		return 0;
	}
/* Scan the list of valid commands. pgc->pgc_commands may be a subclass
 * list (terminated with '*') or the core list (terminated with '@') */
	for (cmd = pgc->pgc_commands; cmd->ascii[0] != '@'; cmd++)
	{
		/* End of subclass command list, chain to core */
		if (cmd->ascii[0] == '*') cmd = pgc_core_commands;

		/* If in ASCII mode match on the ASCII command */
		if (pgc->ascii_mode && !pgc->clcur)
		{
			sprintf(match, "%-6.6s", cmd->ascii);		
			if (!strncmp(match, pgc->asc_command, 6))
			{
				*pcmd = cmd;
				pgc->hex_command = cmd->hex;
				break;
			}
		}
		else	/* Otherwise match on the hex command */
		{
			if (cmd->hex == pgc->hex_command)
			{
				sprintf(pgc->asc_command, "%-6.6s",
					cmd->ascii);
				*pcmd = cmd;
				break;
			}
		}
	}
	return 1;
}


/* The PGC drawing thread main loop. Read in commands and execute them ad
 * infinitum */
void pgc_core_thread(void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;
	const pgc_command_t *cmd;

	PGCLOG(("pgc_core_thread begins"));
	while (1)
	{
		if (!pgc_parse_command(pgc, &cmd))
		{
			/* PGC has been reset */
			continue;
		} 
		PGCLOG(("PGC command: [%02x] '%s' found=%d\n", 
			pgc->hex_command, pgc->asc_command, (cmd != NULL)));

		if (cmd)
		{
			pgc->result_count = 0;
			(*cmd->handler)(pgc);
		}
		else
		{
			pgc_error(pgc, PGC_ERROR_OPCODE);
		}
	}
}

void pgc_recalctimings(pgc_core_t *pgc)
{
	double disptime, _dispontime, _dispofftime;
	double pixel_clock = (cpuclock * (double)(1ull << 32)) / (pgc->cga_selected ? 25175000.0 : pgc->native_pixel_clock);

	/* Use a fixed 640 columns, like the T3100e */
        disptime = pgc->screenw + 11;
        _dispontime = pgc->screenw * pixel_clock;
        _dispofftime = (disptime - pgc->screenw) * pixel_clock;
        pgc->dispontime  = (uint64_t)_dispontime;
        pgc->dispofftime = (uint64_t)_dispofftime;
}

/* Draw the display in CGA (640x400) text mode */
void pgc_cga_text(pgc_core_t *pgc, int w)
{
	int x, c;
	uint8_t chr, attr;
        int drawcursor = 0;
	uint32_t cols[2];
	int pitch = (pgc->mapram[0x3E9] + 1) * 2;
	uint16_t sc = (pgc->displine & 0x0F) % pitch;
        uint16_t ma = (pgc->mapram[0x3ED] | (pgc->mapram[0x3EC] << 8)) & 0x3fff;
        uint16_t ca = (pgc->mapram[0x3EF] | (pgc->mapram[0x3EE] << 8)) & 0x3fff;
	uint8_t *addr;
	int cw = (w == 80) ? 8 : 16;

	addr = &pgc->cga_vram[((ma << 1) + (((pgc->displine / pitch)*w)) * 2) & 0x3ffe];
	ma += (pgc->displine / pitch) * w;

	for (x = 0; x < w; x++)
	{
		chr  = addr[0];
		attr = addr[1];
		addr += 2;
 
		/* Cursor enabled? */           
		if (ma == ca && (pgc->cgablink & 8) && 
			(pgc->mapram[0x3EA] & 0x60) != 0x20)
		{
    			drawcursor = ((pgc->mapram[0x3EA] & 0x1F) <= (sc >> 1)) 
				&&   ((pgc->mapram[0x3EB] & 0x1F) >= (sc >> 1));
		}
		else	drawcursor = 0;
                if (pgc->mapram[0x3D8] & 0x20)
		{
			cols[1] = attr & 15;
			cols[0] = (attr >> 4) & 7;
                        if ((pgc->cgablink & 8) && (attr & 0x80) && !drawcursor) 
				cols[1] = cols[0];
		}
		else
		{ 
			cols[1] = attr & 15; 
			cols[0] = attr >> 4;
		}
		if (drawcursor) 
		{ 
			for (c = 0; c < cw; c++)
			{ 
				if (w == 80) 
					((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 3) + c] = cols[(fontdatm[chr + pgc->fontbase][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ 0xffffff;
				else
                                        ((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 4) + c] = cols[(fontdatm[chr + pgc->fontbase][sc] & (1 << (c ^ 7))) ? 1 : 0] ^ 0xffffff;
			}
		}
		else
		{ 
			for (c = 0; c < cw; c++) 
			{
				if (w == 80)
					((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 3) + c] = cols[(fontdatm[chr + pgc->fontbase][sc] & (1 << (c ^ 7))) ? 1 : 0];
				else
                                        ((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 4) + c] = cols[(fontdatm[chr + pgc->fontbase][sc] & (1 << ((c >> 1) ^ 7))) ? 1 : 0];
			}
		}
		ma++;
	}
}


/* Draw the display in CGA (320x200) graphics mode */
void pgc_cga_gfx40(pgc_core_t *pgc)
{
	int x, c;
	uint32_t cols[4];
	int	col;
        uint16_t ma = (pgc->mapram[0x3ED] | (pgc->mapram[0x3EC] << 8)) & 0x3fff;
	uint8_t *addr;
        uint16_t dat;

	cols[0] = pgc->mapram[0x3D9] & 15; 
	col = (pgc->mapram[0x3D9] & 16) ? 8 : 0; 

	if (pgc->mapram[0x3D8] & 4) 
	{ 
		cols[1] = col | 3; 
		cols[2] = col | 4; 
		cols[3] = col | 7;
	}
	else if (pgc->mapram[0x3D9] & 32) 
	{ 
		cols[1] = col | 3; 
		cols[2] = col | 5; 
		cols[3] = col | 7; 
	} 
	else 
	{ 
		cols[1] = col | 2; 
		cols[2] = col | 4; 
		cols[3] = col | 6; 
	}
	for (x = 0; x < 40; x++)
	{
		addr = &pgc->cga_vram[(ma + 2 * x + 80 * (pgc->displine >> 2) + 0x2000 * ((pgc->displine >> 1) & 1)) & 0x3FFF];
		dat = (addr[0] << 8) | addr[1];
		pgc->ma++;
		for (c = 0; c < 8; c++) 
		{ 
			((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 4) + (c << 1)] =
			((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 4) + (c << 1) + 1] = cols[dat >> 14];
			dat <<= 2; 
		}
	}
}


/* Draw the display in CGA (640x200) graphics mode */
void pgc_cga_gfx80(pgc_core_t *pgc)
{
	int x, c;
	uint32_t cols[2];
        uint16_t ma = (pgc->mapram[0x3ED] | (pgc->mapram[0x3EC] << 8)) & 0x3fff;
	uint8_t *addr;
        uint16_t dat;

	cols[0] = 0;
	cols[1] = pgc->mapram[0x3D9] & 15; 

	for (x = 0; x < 40; x++)
	{
		addr = &pgc->cga_vram[(ma + 2 * x + 80 * (pgc->displine >> 2) + 0x2000 * ((pgc->displine >> 1) & 1)) & 0x3FFF];
		dat = (addr[0] << 8) | addr[1];
		pgc->ma++;
		for (c = 0; c < 16; c++)
		{ 
			((uint32_t *)buffer32->line[pgc->displine & 2047])[(x << 4) + c] = cols[dat >> 15];
			dat <<= 1;
		}
	}
}



/* Draw the screen in CGA mode. Based on the simplified WY700 renderer 
 * rather than the original CGA, since the PGC doesn't have a real 6845 */
void pgc_cga_poll(pgc_core_t *pgc)
{
        int c;
	uint32_t cols[2];

        if (!pgc->linepos)
        {
                timer_advance_u64(&pgc->timer, pgc->dispofftime);
                pgc->mapram[0x3DA] |= 1;
                pgc->linepos = 1;
                if (pgc->cgadispon)
                {
                        if (pgc->displine == 0)
                        {
                                video_wait_for_buffer();
                        }
                       
                        if ((pgc->mapram[0x3D8] & 0x12) == 0x12)
			{
				pgc_cga_gfx80(pgc);	
			}
			else if (pgc->mapram[0x3D8] & 0x02)
			{
				pgc_cga_gfx40(pgc);
			}
                        else if (pgc->mapram[0x3D8] & 0x01)
			{
				pgc_cga_text(pgc, 80);
			}
			else 
			{
				pgc_cga_text(pgc, 40);
			}
                }
                else
                {
                        cols[0] = ((pgc->mapram[0x3D8] & 0x12) == 0x12) ? 0 : (pgc->mapram[0x3D9] & 15);
                        hline(buffer32, 0, pgc->displine & 2047, PGC_CGA_WIDTH, cols[0]);
                }

		for (c = 0; c < PGC_CGA_WIDTH; c++)
			((uint32_t *)buffer32->line[pgc->displine & 2047])[c] = cgapal[((uint32_t *)buffer32->line[pgc->displine & 2047])[c] & 0xf];

		pgc->displine++;
		if (pgc->displine == PGC_CGA_HEIGHT)
		{
                	pgc->mapram[0x3DA] |= 8;
                	pgc->cgadispon = 0;
		}
		if (pgc->displine == PGC_CGA_HEIGHT + 32)
		{
                	pgc->mapram[0x3DA] &= ~8;
                	pgc->cgadispon = 1;
			pgc->displine = 0;
		}
        }
        else
        {
		if (pgc->cgadispon)
		{
			pgc->mapram[0x3DA] &= ~1;
		}
                timer_advance_u64(&pgc->timer, pgc->dispontime);
                pgc->linepos = 0;

		if (pgc->displine == PGC_CGA_HEIGHT)
		{
			if (PGC_CGA_WIDTH != xsize || PGC_CGA_HEIGHT != ysize)
                        {
                                xsize = PGC_CGA_WIDTH;
                                ysize = PGC_CGA_HEIGHT;
                                updatewindowsize(xsize, ysize);
                        } 
			video_blit_memtoscreen(0, 0, 0, ysize, xsize, ysize);
			frames++;

			/* We have a fixed 640x400 screen for
			 * CGA modes */
			video_res_x = PGC_CGA_WIDTH; 
			video_res_y = PGC_CGA_HEIGHT; 
			switch (pgc->mapram[0x3D8] & 0x12)
			{
				case 0x12: video_bpp = 1; break;
				case 0x02: video_bpp = 2; break;
				default:   video_bpp = 0; break;
			}
                        pgc->cgablink++;
                }
        }
}



/* Draw the screen in CGA or native mode. */
void pgc_poll(void *p)
{
	pgc_core_t *pgc = (pgc_core_t *)p;
	int x, y;

	if (pgc->cga_selected)
	{
		pgc_cga_poll(pgc);
		return;
	}
/* Not CGA, so must be native mode */
        if (!pgc->linepos)
        {
                timer_advance_u64(&pgc->timer, pgc->dispofftime);
                pgc->mapram[0x3DA] |= 1;
                pgc->linepos = 1;
                if (pgc->cgadispon && pgc->displine < pgc->maxh)
                {
			if (pgc->displine == 0)
			{
				video_wait_for_buffer();
			}
			/* Don't know why pan needs to be multiplied by -2, but
			 * the IM1024 driver uses PAN -112 for an offset of 
			 * 224. */
			y = pgc->displine - 2 * pgc->pan_y;
			for (x = 0; x < pgc->screenw; x++)
			{
				if (x + pgc->pan_x < pgc->maxw)
				{
					((uint32_t *)buffer32->line[pgc->displine & 2047])[x] = pgc->palette[pgc->vram[y * pgc->maxw + x]];
				}
				else
				{
					((uint32_t *)buffer32->line[pgc->displine & 2047])[x] = pgc->palette[0];
				}
			}
		}
                else
                {
                        hline(buffer32, 0, pgc->displine & 2047, pgc->screenw, pgc->palette[0]);
                }
		pgc->displine++;
		if (pgc->displine == pgc->screenh)
		{
                	pgc->mapram[0x3DA] |= 8;
                	pgc->cgadispon = 0;
		}
		if (pgc->displine == pgc->screenh + 32)
		{
                	pgc->mapram[0x3DA] &= ~8;
                	pgc->cgadispon = 1;
			pgc->displine = 0;
		}
        }
        else
        {
		if (pgc->cgadispon)
		{
			pgc->mapram[0x3DA] &= ~1;
		}
                timer_advance_u64(&pgc->timer, pgc->dispontime);
                pgc->linepos = 0;

		if (pgc->displine == pgc->screenh)
		{
			if (pgc->screenw != xsize || pgc->screenh != ysize)
                        {
                                xsize = pgc->screenw;
                                ysize = pgc->screenh;
                                updatewindowsize(xsize, ysize);
                        } 
			video_blit_memtoscreen(0, 0, 0, ysize, xsize, ysize);
			frames++;

			video_res_x = pgc->screenw;
			video_res_y = pgc->screenh;
			video_bpp = 8;
                        pgc->cgablink++;
                }
        }
}


/* Initialise RAM and registers to default values */
void pgc_reset(pgc_core_t *pgc)
{
	int n;

	memset(pgc->mapram, 0, sizeof(pgc->mapram));
/* There is no point in emulating the 'CGA disable' jumper as this is only
 * appropriate for a dual-head system, not emulated by PCEM */
	pgc->mapram[0x30B] = pgc->cga_enabled = 1;
	pgc->mapram[0x3F8] = 0x03;	/* Minor version */
	pgc->mapram[0x3F9] = 0x01;	/* Minor version */
	pgc->mapram[0x3FB] = 0xA5;	/* } */
	pgc->mapram[0x3FC] = 0x5A;	/* PGC self-test passed */
	pgc->mapram[0x3FD] = 0x55;	/* } */
	pgc->mapram[0x3FE] = 0x5A;	/* } */

	pgc->ascii_mode = 1;	/* Start off in ASCII mode */
	pgc->line_pattern = 0xFFFF;
	memset(pgc->fill_pattern, 0xFF, sizeof(pgc->fill_pattern));
	pgc->colour = 0xFF;
	pgc->tjust_h = 1;
	pgc->tjust_v = 1;

	/* Reset panning */
	pgc->pan_x = 0;
	pgc->pan_y = 0;

	/* Reset clipping */
	pgc->vp_x1 = 0;
	pgc->vp_y1 = 0;
	pgc->vp_x2 = pgc->visw - 1;
	pgc->vp_y2 = pgc->vish - 1;

	/* Empty command lists */
	for (n = 0; n < 256; n++)
	{
		pgc->clist[n].wrptr = 0;
		pgc->clist[n].rdptr = 0;
		pgc->clist[n].repeat = 0;
		pgc->clist[n].chain = 0;
	}
	pgc->clcur = NULL;
	/* Select CGA display */
	pgc->cga_selected = -1;
	pgc_setdisplay(pgc, pgc->cga_enabled);
	pgc->mapram[0x30C] = pgc->cga_enabled;
	pgc->mapram[0x30D] = pgc->cga_enabled;
	/* Default palette is 0 */
	pgc_init_lut(pgc, 0);
	hndl_lutsav(pgc);
}


/* Initialisation code common to the PGC and its subclasses. 
 *
 * Pass the 'input byte' function in since this is overridden in 
 * the IM-1024, and needs to be set before the drawing thread is 
 * launched */
void pgc_core_init(pgc_core_t *pgc, int maxw, int maxh, int visw, int vish,
	int (*inpbyte)(struct pgc_core_t *pgc, uint8_t *result), double native_pixel_clock)
{
	int n;

	mem_mapping_add(&pgc->mapping, 0xC6000, 0x800, 
		pgc_read, NULL, NULL, 
		pgc_write, NULL, NULL, NULL, MEM_MAPPING_EXTERNAL, pgc);
	mem_mapping_add(&pgc->cga_mapping, 0xB8000, 0x8000, 
		pgc_read, NULL, NULL, 
		pgc_write, NULL, NULL, NULL, MEM_MAPPING_EXTERNAL, pgc);
	io_sethandler(0x3D0, 0x0010, pgc_in, NULL, NULL, pgc_out, NULL, NULL,
		pgc);

	pgc->maxw = maxw;
	pgc->maxh = maxh;
	pgc->visw = visw;
	pgc->vish = vish;
	pgc->vram = malloc(maxw * maxh);
	pgc->cga_vram = malloc(0x4000);
	pgc->clist = calloc(256, sizeof(pgc_commandlist_t));
	pgc->clcur = NULL;
	pgc->native_pixel_clock = native_pixel_clock;

	memset(pgc->vram, 0, maxw * maxh);
	memset(pgc->cga_vram, 0, 0x4000);
	/* Empty command lists */
	for (n = 0; n < 256; n++)
	{
		pgc->clist[n].list = NULL;
		pgc->clist[n].listmax = 0;
		pgc->clist[n].wrptr = 0;
		pgc->clist[n].rdptr = 0;
		pgc->clist[n].repeat = 0;
		pgc->clist[n].chain = NULL;
	}

	pgc_reset(pgc);
	pgc->inputbyte = inpbyte;
	pgc->pgc_commands = pgc_core_commands;
	pgc->pgc_wake_thread = thread_create_event();
	pgc->pgc_thread = thread_create(pgc_core_thread, pgc);

        timer_add(&pgc->timer, pgc_poll, (void *)pgc, 1);

        timer_add(&pgc->wake_timer, pgc_wake_timer, (void *)pgc, 0);
}


/* Initialisation code specific to the PGC */
void *pgc_standalone_init()
{
	pgc_core_t *pgc = malloc(sizeof(pgc_core_t));
	memset(pgc, 0, sizeof(pgc_core_t));

	/* Framebuffer and screen are both 640x480 */
	pgc_core_init(pgc, 640, 480, 640, 480, pgc_input_byte, 25175000.0);
	return pgc;
}


void pgc_close(void *p)
{
        pgc_core_t *pgc = (pgc_core_t *)p;
        
        thread_kill(pgc->pgc_thread);
        thread_destroy_event(pgc->pgc_wake_thread);

	if (pgc->cga_vram)
	{
        	free(pgc->cga_vram);
	}
	if (pgc->vram)
	{
        	free(pgc->vram);
	}
        free(pgc);
}

void pgc_speed_changed(void *p)
{
        pgc_core_t *pgc = (pgc_core_t *)p;

        pgc_recalctimings(pgc);
}


device_config_t pgc_config[] =
{
	{
		.type = -1
	}
};

device_t pgc_device =
{
        "PGC",
        0,
        pgc_standalone_init,
        pgc_close,
        NULL,
        pgc_speed_changed,
        NULL,
        NULL,
        pgc_config
};

