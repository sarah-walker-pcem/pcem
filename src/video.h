extern int egareads,egawrites;

extern int fullchange;
extern int changeframecount;

typedef struct
{
        int w, h;
        uint8_t *dat;
        uint8_t *line[0];
} BITMAP;

extern BITMAP *buffer, *buffer32;

extern BITMAP *screen;

BITMAP *create_bitmap(int w, int h);

extern uint8_t fontdat[256][8];
extern uint8_t fontdatm[256][16];

extern int xsize,ysize;

typedef struct
{
        uint8_t r, g, b;
} RGB;
        
typedef RGB PALETTE[256];

extern float cpuclock;

extern int emu_fps, frames;

#define makecol(r, g, b)    ((b) | ((g) << 8) | ((r) << 16))
#define makecol32(r, g, b)  ((b) | ((g) << 8) | ((r) << 16))

extern int readflash;

extern void (*video_recalctimings)();

extern void (*video_blit_memtoscreen)(int x, int y, int y1, int y2, int w, int h);
extern void (*video_blit_memtoscreen_8)(int x, int y, int w, int h);

extern int video_timing_b, video_timing_w, video_timing_l;
extern int video_speed;

extern int video_res_x, video_res_y, video_bpp;
