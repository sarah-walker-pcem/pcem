#ifndef _VID_TVP3026_RAMDAC_H_
#define _VID_TVP3026_RAMDAC_H_

typedef struct tvp3026_ramdac_t
{
        int reg_idx;
        uint16_t regs[256];
        int cursor_ena, cursor_mode;
        uint8_t cursor_control;
        int cursor_x, cursor_y;
        uint8_t cursor_data[1024];
        struct
        {
                uint8_t m, n, p;
        } pix, mem, loop;
        
        int cursor_pal_read, cursor_pal_write, cursor_pal_pos;
        int cursor_pal_r, cursor_pal_g;
        RGB cursor_pal[4];
        uint32_t cursor_pal_look[4];
} tvp3026_ramdac_t;

void tvp3026_init(tvp3026_ramdac_t *ramdac);

void tvp3026_ramdac_out(uint16_t addr, uint8_t val, tvp3026_ramdac_t *ramdac, svga_t *svga);
uint8_t tvp3026_ramdac_in(uint16_t addr, tvp3026_ramdac_t *ramdac, svga_t *svga);

void tvp3026_set_cursor_enable(tvp3026_ramdac_t *ramdac, svga_t *svga, int ena);
void tvp3026_hwcursor_draw(tvp3026_ramdac_t *ramdac, svga_t *svga, int displine);

float tvp3026_getclock(tvp3026_ramdac_t *ramdac);

#endif /* _VID_TVP3026_RAMDAC_H_ */
