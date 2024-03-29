#ifndef _VID_EGA_H_
#define _VID_EGA_H_
typedef struct ega_t {
        mem_mapping_t mapping;

        rom_t bios_rom;

        uint8_t crtcreg;
        uint8_t crtc[32];
        uint8_t gdcreg[16];
        int gdcaddr;
        uint8_t attrregs[32];
        int attraddr, attrff;
        uint8_t seqregs[64];
        int seqaddr;

        uint8_t miscout;
        int vidclock;

        uint8_t la, lb, lc, ld;

        uint8_t stat;

        int fast;
        uint8_t colourcompare, colournocare;
        int readmode, writemode, readplane;
        int chain4, chain2_read, chain2_write;
        uint8_t writemask;
        uint32_t charseta, charsetb;

        uint8_t egapal[16];
        uint32_t *pallook;

        int vtotal, dispend, vsyncstart, split;
        int hdisp, htotal, hdisp_time, rowoffset;
        int lowres, interlace;
        int linedbl, rowcount;
        double clock;
        uint32_t ma_latch;

        int vres;

        uint64_t dispontime, dispofftime;
        pc_timer_t timer;

        uint8_t scrblank;

        int dispon;
        int hdisp_on;

        uint32_t ma, maback, ca;
        int vc;
        int sc;
        int linepos, vslines, linecountff, oddeven;
        int con, cursoron, blink;
        int scrollcache;

        int firstline, lastline;
        int firstline_draw, lastline_draw;
        int displine;

        uint8_t *vram;
        int vrammask;
        uint32_t vram_limit;

        int video_res_x, video_res_y, video_bpp;
        int frames;
} ega_t;

void *ega_standalone_init();
void ega_out(uint16_t addr, uint8_t val, void *p);
uint8_t ega_in(uint16_t addr, void *p);
void ega_poll(void *p);
void ega_recalctimings(struct ega_t *ega);
void ega_write(uint32_t addr, uint8_t val, void *p);
uint8_t ega_read(uint32_t addr, void *p);
void ega_init(ega_t *ega, int monitor_type, int is_mono);

extern device_t ega_device;

#endif /* _VID_EGA_H_ */
