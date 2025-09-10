#ifndef _VID_V6355_H_
#define _VID_V6355_H_
typedef struct v6355_t {
        mem_mapping_t mapping;

        int crtcreg;
        uint8_t crtc[32];

/* The V6355 has its own set of registers, as well as the emulated MC6845 */
	int v6355reg;
	uint8_t v6355data[106];

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
	int display_type;

        uint64_t dispontime, dispofftime;
        pc_timer_t timer;

        int firstline, lastline;

        int drawcursor;

        uint8_t *vram;

        uint8_t charbuffer[256];
	uint32_t v6355pal[16];

        int revision;
} v6355_t;

void v6355_init(v6355_t *v6355);
void v6355_out(uint16_t addr, uint8_t val, void *p);
uint8_t v6355_in(uint16_t addr, void *p);
void v6355_write(uint32_t addr, uint8_t val, void *p);
uint8_t v6355_read(uint32_t addr, void *p);
void v6355_recalctimings(v6355_t *v6355);
void v6355_poll(void *p);

extern device_t v6355d_device;

#endif /* _VID_V6355_H_ */
