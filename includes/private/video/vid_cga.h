#ifndef _VID_CGA_H_
#define _VID_CGA_H_
typedef struct cga_t {
        mem_mapping_t mapping;

        int crtcreg;
        uint8_t crtc[32];

        uint8_t cgastat;

        uint8_t cgamode, cgacol;

        int fontbase;
        int linepos, displine;
        int sc, vc;
        int cgadispon;
        int con, coff, cursoron, cgablink;
        int vsynctime, vadj;
        uint16_t ma, maback;
        int oddeven;

        uint64_t dispontime, dispofftime;
        pc_timer_t timer;

        int firstline, lastline;

        int drawcursor;

        uint8_t *vram;

        uint8_t charbuffer[256];

        int revision;
        int composite;
        int snow_enabled;
} cga_t;

void cga_init(cga_t *cga);
void cga_out(uint16_t addr, uint8_t val, void *p);
uint8_t cga_in(uint16_t addr, void *p);
void cga_write(uint32_t addr, uint8_t val, void *p);
uint8_t cga_read(uint32_t addr, void *p);
void cga_recalctimings(cga_t *cga);
void cga_poll(void *p);

extern device_t cga_device;

#endif /* _VID_CGA_H_ */
