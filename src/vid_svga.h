/*Vertical timings*/
extern int svga_vtotal, svga_dispend, svga_vsyncstart, svga_split;
/*Horizontal timings*/
extern int svga_hdisp, svga_htotal, svga_rowoffset;
/*Flags - svga_lowres = 1/2 clock in 256+ colour modes, svga_interlace = interlace mode enabled*/
extern int svga_lowres, svga_interlace;

/*Status of display on*/
extern int svga_hdisp_on;

extern double svga_clock;
extern uint32_t svga_ma;
extern void (*svga_recalctimings_ex)();

extern uint8_t svga_miscout;

extern int  svga_init();
extern void svga_poll();
extern void svga_recalctimings();

typedef struct
{
        int ena;
        int x, y;
        int xoff, yoff;
        uint32_t addr;
} SVGA_HWCURSOR;

extern SVGA_HWCURSOR svga_hwcursor, svga_hwcursor_latch;

//extern int      svga_hwcursor_x,    svga_hwcursor_y;
//extern int      svga_hwcursor_xoff, svga_hwcursor_yoff;
//extern uint32_t /*svga_hwcursor_addr, */svga_hwcursor_addr_cur;
//extern int      svga_hwcursor_ena;
extern int      svga_hwcursor_on;
extern void   (*svga_hwcursor_draw)(int displine);

uint8_t  svga_read(uint32_t addr);
uint16_t svga_readw(uint32_t addr);
uint32_t svga_readl(uint32_t addr);
void     svga_write(uint32_t addr, uint8_t val);
void     svga_writew(uint32_t addr, uint16_t val);
void     svga_writel(uint32_t addr, uint32_t val);
uint8_t  svga_read_linear(uint32_t addr);
uint16_t svga_readw_linear(uint32_t addr);
uint32_t svga_readl_linear(uint32_t addr);
void     svga_write_linear(uint32_t addr, uint8_t val);
void     svga_writew_linear(uint32_t addr, uint16_t val);
void     svga_writel_linear(uint32_t addr, uint32_t val);

void svga_doblit(int y1, int y2);

extern uint32_t svga_vram_limit;
