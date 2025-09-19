#ifndef _VID_QUADCOLOR_H_
#define _VID_QUADCOLOR_H_
typedef struct quadcolor_t {
        mem_mapping_t mapping;
        mem_mapping_t mapping_2;

        int crtcreg;
        uint8_t crtc[32];

        uint8_t cgastat;

        uint8_t cgamode, cgacol;

        uint8_t quadcolor_ctrl;
        uint8_t quadcolor_2_oe;
        uint16_t page_offset;

        int fontbase;
        int linepos, displine;
        int sc, vc;
        int cgadispon;
        int con, coff, cursoron, cgablink;
        int vsynctime, vadj;
        uint16_t ma, maback;
        int oddeven;
        int qc2idx;
        uint8_t qc2mask;

        uint64_t dispontime, dispofftime;
        pc_timer_t timer;

        int firstline, lastline;

        int drawcursor;

        uint8_t *vram;
        uint8_t *vram_2;

        uint8_t charbuffer[256];

        int revision;
        int composite;
        int has_2nd_charset;
        int has_quadcolor_2;
} quadcolor_t;

void quadcolor_init(quadcolor_t *quadcolor);
void quadcolor_out(uint16_t addr, uint8_t val, void *p);
uint8_t quadcolor_in(uint16_t addr, void *p);
void quadcolor_write(uint32_t addr, uint8_t val, void *p);
uint8_t quadcolor_read(uint32_t addr, void *p);
void quadcolor_recalctimings(quadcolor_t *quadcolor);
void quadcolor_poll(void *p);

extern device_t quadcolor_device;

#endif /* _VID_QUADCOLOR_H_ */
