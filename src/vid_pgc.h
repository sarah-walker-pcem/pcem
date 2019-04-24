#include "timer.h"

struct pgc_core_t;

typedef struct pgc_commandlist_t
{
	uint8_t *list;
	uint32_t listmax;
	uint32_t wrptr;
	uint32_t rdptr;
	uint32_t repeat;
	struct pgc_commandlist_t *chain;
} pgc_commandlist_t;

int pgc_commandlist_append(pgc_commandlist_t *list, uint8_t v);

typedef struct pgc_command_t
{
	char ascii[6];
	uint8_t hex;
	void (*handler)(struct pgc_core_t *pgc);
	int  (*parser) (struct pgc_core_t *pgc, pgc_commandlist_t *cl, 
			int p);
	int p;	
} pgc_command_t;


typedef struct pgc_core_t
{
        mem_mapping_t mapping;
        mem_mapping_t cga_mapping;
     
	pgc_commandlist_t *clist;
	pgc_commandlist_t *clcur;
	uint8_t mapram[2048];	/* Host <--> PGC communication buffer */
        uint8_t *cga_vram;
        uint8_t *vram;
	char    asc_command[7];
	uint8_t hex_command;
	uint32_t palette[256];
	uint32_t userpal[256];
	uint32_t maxw, maxh;	/* Maximum framebuffer size */
	uint32_t visw, vish;	/* Maximum screen size */
	uint32_t screenw, screenh;
	int16_t pan_x, pan_y;
	uint16_t win_x1, win_x2, win_y1, win_y2;
	uint16_t vp_x1, vp_x2, vp_y1, vp_y2;
	int16_t fill_pattern[16];
	int16_t line_pattern;
	uint8_t draw_mode;
	uint8_t fill_mode;
	uint8_t colour;
	uint8_t tjust_h; /* Horizontal alignment 1=left 2=centre 3=right*/
	uint8_t tjust_v; /* Vertical alignment 1=bottom 2=centre 3=top*/
	int32_t tsize;	 /* Horizontal spacing */

	int32_t x, y, z;/* Drawing position */

	const pgc_command_t *pgc_commands;
	thread_t *pgc_thread;
	event_t  *pgc_wake_thread;
        pc_timer_t wake_timer;
//	int	wake_timer;

	int	waiting_input_fifo;
	int	waiting_output_fifo;
	int	waiting_error_fifo;
	int	cga_enabled;
	int	cga_selected;
	int	ascii_mode;
	int	result_count;
 
	int fontbase;
        int linepos, displine;
        int vc;
        int cgadispon;
        int con, coff, cursoron, cgablink;
        int vsynctime, vadj;
        uint16_t ma, maback;
        int oddeven;

        uint64_t dispontime, dispofftime;
        pc_timer_t timer;
//        int vidtime;
        double native_pixel_clock;
        
        int drawcursor;
        
	int (*inputbyte)(struct pgc_core_t *pgc, uint8_t *result); 

} pgc_core_t;

void    pgc_init(pgc_core_t *pgc);
void    pgc_out(uint16_t addr, uint8_t val, void *p);
uint8_t pgc_in(uint16_t addr, void *p);
void    pgc_write(uint32_t addr, uint8_t val, void *p);
uint8_t pgc_read(uint32_t addr, void *p);
void    pgc_recalctimings(pgc_core_t *pgc);
void    pgc_poll(void *p);
void	pgc_reset(pgc_core_t *pgc);
void	pgc_wake(pgc_core_t *pgc);
/* Plot a pixel (raster coordinates, current ink) */
void	pgc_plot(pgc_core_t *pgc, uint16_t x, uint16_t y);
void	pgc_write_pixel(pgc_core_t *pgc, uint16_t x, uint16_t y, uint8_t ink);
uint8_t pgc_read_pixel(pgc_core_t *pgc, uint16_t x, uint16_t y);
uint8_t *pgc_vram_addr(pgc_core_t *pgc, int16_t x, int16_t y);
/* Draw line (window coordinates) */
uint16_t pgc_draw_line(pgc_core_t *pgc, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t linemask);
/* Draw line (raster coordinates) */
uint16_t pgc_draw_line_r(pgc_core_t *pgc, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t linemask);
void    pgc_draw_ellipse(pgc_core_t *pgc, int32_t x, int32_t y);
void	pgc_fill_polygon(pgc_core_t *pgc, unsigned corners, int32_t *x, int32_t *y);
/* Horizontal line in fill pattern (raster coordinates) */
void    pgc_fill_line_r(pgc_core_t *pgc, int32_t x0, int32_t x1, int32_t y);

/* Convert to raster coordinates */
void	pgc_sto_raster(pgc_core_t *pgc, int16_t *x, int16_t *y);
void	pgc_ito_raster(pgc_core_t *pgc, int32_t *x, int32_t *y);
void	pgc_dto_raster(pgc_core_t *pgc, double *x, double *y);

/* Read/write to the PGC's FIFOs */
int	pgc_input_byte(pgc_core_t *pgc, uint8_t *val);
int	pgc_output_byte(pgc_core_t *pgc, uint8_t val);
int	pgc_output_string(pgc_core_t *pgc, const char *val);
int	pgc_error_byte(pgc_core_t *pgc, uint8_t val);
int	pgc_error_string(pgc_core_t *pgc, const char *val);

int	pgc_param_byte(pgc_core_t *pgc, uint8_t *val);
int	pgc_param_word(pgc_core_t *pgc, int16_t *val);
int	pgc_param_coord(pgc_core_t *pgc, int32_t *val);
int	pgc_result_byte(pgc_core_t *pgc, uint8_t val);
int	pgc_result_word(pgc_core_t *pgc, int16_t val);
int	pgc_result_coord(pgc_core_t *pgc, int32_t val);

void pgc_sleep(pgc_core_t *pgc);
int pgc_error(pgc_core_t *pgc, int err);
void pgc_core_init(pgc_core_t *pgc, int maxw, int maxh, int visw, int vish,
	int (*inpbyte)(struct pgc_core_t *pgc, uint8_t *result), double native_pixel_clock);
void pgc_speed_changed(void *p);
void pgc_close(void *p);

void pgc_hndl_lut8(pgc_core_t *pgc);
void pgc_hndl_lut8rd(pgc_core_t *pgc);

int pgc_parse_bytes(pgc_core_t *pgc, pgc_commandlist_t *cl, int p);
int pgc_parse_words(pgc_core_t *pgc, pgc_commandlist_t *cl, int p);
int pgc_parse_coords(pgc_core_t *pgc, pgc_commandlist_t *cl, int p);

extern device_t pgc_device;

#define PGC_ERROR_RANGE    0x01
#define PGC_ERROR_INTEGER  0x02
#define PGC_ERROR_MEMORY   0x03
#define PGC_ERROR_OVERFLOW 0x04
#define PGC_ERROR_DIGIT    0x05
#define PGC_ERROR_OPCODE   0x06
#define PGC_ERROR_RUNNING  0x07
#define PGC_ERROR_STACK    0x08
#define PGC_ERROR_TOOLONG  0x09
#define PGC_ERROR_AREA     0x0A
#define PGC_ERROR_MISSING  0x0B

/* #define PGCLOG(x) pclog x */
#define PGCLOG(x) 
