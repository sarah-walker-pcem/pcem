extern int egareads,egawrites;

extern int bpp;
extern uint8_t svgaseg,svgaseg2;

extern uint8_t crtcreg;
extern uint8_t crtc[128];
extern uint8_t crtcmask[128];
extern uint8_t gdcreg[16];
extern int gdcaddr;
extern uint8_t attrregs[32];
extern int attraddr,attrff;
extern uint8_t seqregs[32];
extern int seqaddr;
extern int svgaon;

extern uint8_t cgamode,cgacol,cgastat;
extern int egapal[16];

extern int scrblank;

extern uint8_t writemask,charset;
extern int charseta,charsetb;
extern uint8_t colourcompare,colournocare;
extern int readplane,readmode,writemode;

extern int vidclock;
extern int vres;

extern uint32_t *pallook,pallook16[256],pallook64[256],pallook256[256];

extern int fullchange;
extern int changeframecount;

extern int firstline,lastline;
extern int ega_hdisp,ega_rowoffset,ega_split,ega_dispend,ega_vsyncstart,ega_vtotal;
extern float dispontime,dispofftime,disptime;

extern void (*video_out)     (uint16_t addr, uint8_t val);
extern void (*video_mono_out)(uint16_t addr, uint8_t val);
extern uint8_t (*video_in)     (uint16_t addr);
extern uint8_t (*video_mono_in)(uint16_t addr);

extern void (*video_write_a000)(uint32_t addr, uint8_t val);
extern void (*video_write_b000)(uint32_t addr, uint8_t val);
extern void (*video_write_b800)(uint32_t addr, uint8_t val);

extern uint8_t (*video_read_a000)(uint32_t addr);
extern uint8_t (*video_read_b000)(uint32_t addr);
extern uint8_t (*video_read_b800)(uint32_t addr);

extern void (*video_write_a000_w)(uint32_t addr, uint16_t val);
extern void (*video_write_a000_l)(uint32_t addr, uint32_t val);

extern void    video_out_null(uint16_t addr, uint8_t val);
extern uint8_t video_in_null(uint16_t addr);

extern void    video_write_null(uint32_t addr, uint8_t val);
extern uint8_t video_read_null (uint32_t addr);


extern uint8_t tridentoldctrl2,tridentnewctrl2;
extern int rowdbl;

typedef struct
{
        int w, h;
        uint8_t *dat;
        uint8_t *line[0];
} BITMAP;

extern BITMAP *buffer,*buffer32,*vbuf;

extern BITMAP *screen;

extern int wx,wy;

extern uint8_t fontdat[256][8];
extern uint8_t fontdatm[256][16];


extern int xsize,ysize;

extern int dacread,dacwrite,dacpos;

typedef struct
{
        uint8_t r, g, b;
} RGB;
        
typedef RGB PALETTE[256];
extern PALETTE vgapal;
extern int palchange;


extern uint32_t vrammask;

typedef struct
{
        int     (*init)();
        void    (*out)(uint16_t addr, uint8_t val);
        uint8_t (*in)(uint16_t addr);
        void    (*mono_out)(uint16_t addr, uint8_t val);
        uint8_t (*mono_in)(uint16_t addr);
        void    (*poll)();
        void    (*recalctimings)();
        void    (*write_a000)(uint32_t addr, uint8_t val);
        void    (*write_b000)(uint32_t addr, uint8_t val);
        void    (*write_b800)(uint32_t addr, uint8_t val);
        uint8_t (*read_a000)(uint32_t addr);
        uint8_t (*read_b000)(uint32_t addr);
        uint8_t (*read_b800)(uint32_t addr);
} GFXCARD;

extern GFXCARD vid_cga;
extern GFXCARD vid_mda;
extern GFXCARD vid_hercules;
extern GFXCARD vid_pc1512;
extern GFXCARD vid_pc1640;
extern GFXCARD vid_pc200;
extern GFXCARD vid_m24;
extern GFXCARD vid_paradise;
extern GFXCARD vid_tandy;
extern GFXCARD vid_ega;
extern GFXCARD vid_oti067;
extern GFXCARD vid_tvga;
extern GFXCARD vid_et4000;
extern GFXCARD vid_et4000w32p;
extern GFXCARD vid_s3;


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
