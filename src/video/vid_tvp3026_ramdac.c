#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_tvp3026_ramdac.h"

#define REG_DCC         0x09
#define REG_CURSOR_DATA 0x0b
#define REG_CURSOR_XL   0x0c
#define REG_CURSOR_XH   0x0d
#define REG_CURSOR_YL   0x0e
#define REG_CURSOR_YH   0x0f

#define IREG_ICC   0x06
#define REG_MSC    0x1e

#define DCC_MODE_SEL_MASK (0x03)
#define DCC_MODE_SEL_3_COLOUR (0x01)
#define DCC_MODE_SEL_XGA      (0x02)
#define DCC_MODE_SEL_XWINDOWS (0x03)
#define CCR_ADDR_SHIFT (2)
#define CCR_ADDR_MASK (3 << CCR_ADDR_SHIFT)

#define MSC_PAL_DEPTH (1 << 3)

/*Update SVGA hwcursor variables with RAMDAC cursor. Should only be called when
  ramdac->cursor_enabled is set, otherwise the host SVGA board controls the cursor*/
static void update_cursor(tvp3026_ramdac_t* ramdac, svga_t* svga)
{
//        pclog("update_cursor: %02x\n", ramdac->regs[REG_DCC]);
        if (ramdac->cursor_control & DCC_MODE_SEL_MASK)
        {
                svga->hwcursor.ena = 1;
                if (ramdac->cursor_x >= 64)
                {
                        svga->hwcursor.xoff = 0;
                        svga->hwcursor.x = ramdac->cursor_x - 64;
                }
                else
                {
                        svga->hwcursor.xoff = 64 - ramdac->cursor_x;
                        svga->hwcursor.x = 0;
                }
                if (ramdac->cursor_y >= 64)
                {
                        svga->hwcursor.yoff = 0;
                        svga->hwcursor.addr = 0;
                        svga->hwcursor.y = ramdac->cursor_y - 64;
                }
                else
                {
                        svga->hwcursor.yoff = 64 - ramdac->cursor_y;
                        svga->hwcursor.addr = svga->hwcursor.yoff * 8;
                        svga->hwcursor.y = 0;
                }
                ramdac->cursor_mode = ramdac->cursor_control & DCC_MODE_SEL_MASK;
//                pclog("     x=%i y=%i\n", svga->hwcursor.x, svga->hwcursor.y);
        }
        else
                svga->hwcursor.ena = 0;
}

void tvp3026_ramdac_out(uint16_t addr, uint8_t val, tvp3026_ramdac_t* ramdac, svga_t* svga)
{
//        pclog("tvp3026_ramdac_out: addr=%04x val=%02x\n", addr, val);
        switch (addr)
        {
        case 0x0:
                ramdac->reg_idx = val;
        case 0x1:
                svga_out(addr + 0x3c8, val, svga);
                break;
        case 0x2:
        case 0x3:
                svga_out(addr + 0x3c4, val, svga);
                break;

        case 0x4:
                ramdac->cursor_pal_write = val;
                ramdac->cursor_pal_pos = 0;
                break;
        case 0x5:
                svga->fullchange = changeframecount;
                switch (ramdac->cursor_pal_pos)
                {
                case 0:
                        ramdac->cursor_pal_r = val;
                        ramdac->cursor_pal_pos++;
                        break;
                case 1:
                        ramdac->cursor_pal_g = val;
                        ramdac->cursor_pal_pos++;
                        break;
                case 2:
                        ramdac->cursor_pal[ramdac->cursor_pal_write].r = ramdac->cursor_pal_r;
                        ramdac->cursor_pal[ramdac->cursor_pal_write].g = ramdac->cursor_pal_g;
                        ramdac->cursor_pal[ramdac->cursor_pal_write].b = val;
                        ramdac->cursor_pal_look[ramdac->cursor_pal_write] = makecol32(ramdac->cursor_pal[ramdac->cursor_pal_write].r, ramdac->cursor_pal[ramdac->cursor_pal_write].g, ramdac->cursor_pal[ramdac->cursor_pal_write].b);
                        ramdac->cursor_pal_pos = 0;
                        ramdac->cursor_pal_write = (ramdac->cursor_pal_write + 1) & 3;
                        break;
                }
                break;
        case 0x7:
                ramdac->cursor_pal_read = val;
                ramdac->cursor_pal_pos = 0;
                break;

        case REG_DCC:
                ramdac->cursor_control = (ramdac->cursor_control & ~DCC_MODE_SEL_MASK) | (val & DCC_MODE_SEL_MASK);
                update_cursor(ramdac, svga);
                break;

        case REG_CURSOR_DATA:
                ramdac->cursor_data[svga->dac_write + (((ramdac->cursor_control & CCR_ADDR_MASK) >> CCR_ADDR_SHIFT) << 8)] = val;
                svga->dac_write = (svga->dac_write + 1) & 0xff;
                if (!svga->dac_write)
                        ramdac->cursor_control = (ramdac->cursor_control & ~CCR_ADDR_MASK) | ((ramdac->cursor_control + (1 << CCR_ADDR_SHIFT)) & CCR_ADDR_MASK);
                break;

        case REG_CURSOR_XL:
                ramdac->cursor_x = val | (ramdac->cursor_x & 0xf00);
                update_cursor(ramdac, svga);
                break;
        case REG_CURSOR_XH:
                ramdac->cursor_x = ((val & 0xf) << 8) | (ramdac->cursor_x & 0xff);
                update_cursor(ramdac, svga);
                break;
        case REG_CURSOR_YL:
                ramdac->cursor_y = val | (ramdac->cursor_y & 0xf00);
                update_cursor(ramdac, svga);
                break;
        case REG_CURSOR_YH:
                ramdac->cursor_y = ((val & 0xf) << 8) | (ramdac->cursor_y & 0xff);
                update_cursor(ramdac, svga);
                break;

        case 0xa:
                switch (ramdac->reg_idx)
                {
                case IREG_ICC:
                        ramdac->cursor_control = val;
                        update_cursor(ramdac, svga);
                        break;

                case 0x18: /*True color control*/
                        /*bit 7 = pseudo-color mode
                          bit 6 = true-color mode
                          bit 4 = 24-bit colour mode
                          bit 3 = alternate packing on 24-bit formats*/
                        if (val & 0x80)
                                svga->bpp = 8;
                        else
                                switch (val & 0x1f)
                                {
                                case 0x04: /*ORGB 1555*/
                                        svga->bpp = 15;
                                        break;
                                case 0x05: /*RGB 565*/
                                        svga->bpp = 16;
                                        break;
                                case 0x06: /*ORGB 8888*/
                                        svga->bpp = 32;
                                        break;
                                case 0x16:
                                case 0x1e: /*RGB 888*/
                                        svga->bpp = 24;
                                        break;

                                case 0x01: /*RGBO 4444 - not implemented*/
                                case 0x03: /*RGB 664 - not implemented*/
                                case 0x07: /*BGRO 8888 - not implemented*/
                                case 0x17: /*BGR 888 - not implemented*/
                                case 0x1f: /*BGR 888 - not implemented*/
                                        break;
                                }
                        svga_recalctimings(svga);
//                        pclog("bpp=%i\n", svga->bpp);
                        break;

                case REG_MSC:
                        svga_set_ramdac_type(svga, (val & MSC_PAL_DEPTH) ? RAMDAC_8BIT : RAMDAC_6BIT);
                        break;

                case 0x2d: /*Pixel clock PLL data*/
                        switch (ramdac->regs[0x2c] & 3)
                        {
                        case 0:
                                ramdac->pix.n = val;
                                break;
                        case 1:
                                ramdac->pix.m = val;
                                break;
                        case 2:
                                ramdac->pix.p = val;
                                break;
                        }
                        ramdac->regs[0x2c] = ((ramdac->regs[0x2c] + 1) & 3) | (ramdac->regs[0x2c] & 0xfc);
                        break;

                case 0x2e: /*Memory clock PLL data*/
                        switch ((ramdac->regs[0x2c] >> 2) & 3)
                        {
                        case 0:
                                ramdac->mem.n = val;
                                break;
                        case 1:
                                ramdac->mem.m = val;
                                break;
                        case 2:
                                ramdac->mem.p = val;
                                break;
                        }
                        ramdac->regs[0x2c] = ((ramdac->regs[0x2c] + 4) & 0x0c) | (ramdac->regs[0x2c] & 0xf3);
                        break;

                case 0x2f: /*Loop clock PLL data*/
                        switch ((ramdac->regs[0x2c] >> 4) & 3)
                        {
                        case 0:
                                ramdac->loop.n = val;
                                break;
                        case 1:
                                ramdac->loop.m = val;
                                break;
                        case 2:
                                ramdac->loop.p = val;
                                break;
                        }
                        ramdac->regs[0x2c] = ((ramdac->regs[0x2c] + 0x10) & 0x30) | (ramdac->regs[0x2c] & 0xcf);
                        break;
                }

                switch (ramdac->reg_idx)
                {
                case 0x06:
                case 0x0f:
                case 0x18:
                case 0x19:
                case 0x1a:
                case 0x1c:
                case 0x1d:
                case 0x1e:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                case 0x2d:
                case 0x2e:
                case 0x2f:
                case 0x30:
                case 0x31:
                case 0x32:
                case 0x33:
                case 0x34:
                case 0x35:
                case 0x36:
                case 0x37:
                case 0x38:
                case 0x39:
                case 0x3a:
                case 0x3e:
                case 0xff:
                        ramdac->regs[ramdac->reg_idx] = val;
                        break;
                }
//                pclog("RAMDAC[%02x]=%02x\n", ramdac->reg_idx, val);
                break;
        }
}

uint8_t tvp3026_ramdac_in(uint16_t addr, tvp3026_ramdac_t* ramdac, svga_t* svga)
{
        uint8_t ret = 0xff;

        switch (addr)
        {
        case 0x0:
        case 0x1:
                ret = svga_in(addr + 0x3c8, svga);
                break;
        case 0x2:
                ret = 0xff;
                break;
        case 0x3:
                ret = svga_in(addr + 0x3c4, svga);
                break;

        case 0x4:
                ret = ramdac->cursor_pal_write;
                break;
        case 0x5:
                switch (ramdac->cursor_pal_pos)
                {
                case 0:
                        ramdac->cursor_pal_pos++;
                        return ramdac->cursor_pal[ramdac->cursor_pal_read].r;
                case 1:
                        ramdac->cursor_pal_pos++;
                        return ramdac->cursor_pal[ramdac->cursor_pal_read].g;
                case 2:
                        ramdac->cursor_pal_pos = 0;
                        ramdac->cursor_pal_read = (ramdac->cursor_pal_read + 1) & 3;
                        return ramdac->cursor_pal[(ramdac->cursor_pal_read - 1) & 3].b;
                }
                break;
        case 0x7:
                ret = ramdac->cursor_pal_read;
                break;

        case 0xa:
                switch (ramdac->reg_idx)
                {
                case 0x2d: /*Pixel clock PLL data*/
                        switch (ramdac->regs[0x2c] & 3)
                        {
                        case 0:
                                ret = ramdac->pix.n;
                                break;
                        case 1:
                                ret = ramdac->pix.m;
                                break;
                        case 2:
                                ret = ramdac->pix.p;
                                break;
                        case 3:
                                ret = 0x40; /*PLL locked to frequency*/
                                break;
                        }
                        break;

                case 0x2e: /*Memory clock PLL data*/
                        switch ((ramdac->regs[0x2c] >> 2) & 3)
                        {
                        case 0:
                                ret = ramdac->mem.n;
                                break;
                        case 1:
                                ret = ramdac->mem.m;
                                break;
                        case 2:
                                ret = ramdac->mem.p;
                                break;
                        case 3:
                                ret = 0x40; /*PLL locked to frequency*/
                                break;
                        }
                        break;

                case 0x2f: /*Loop clock PLL data*/
                        switch ((ramdac->regs[0x2c] >> 4) & 3)
                        {
                        case 0:
                                ret = ramdac->loop.n;
                                break;
                        case 1:
                                ret = ramdac->loop.m;
                                break;
                        case 2:
                                ret = ramdac->loop.p;
                                break;
                        case 3:
                                ret = 0x40; /*PLL locked to frequency*/
                                break;
                        }
                        break;

                default:
                        ret = ramdac->regs[ramdac->reg_idx];
                        break;
                }
//                pclog("Read RAMDAC[%02x]=%02x\n", ramdac->reg_idx, ret);
                break;
        }
//        pclog("tvp3026_ramdac_in: addr=%04x ret=%02x\n", addr, ret);
        return ret;
}

float tvp3026_getclock(tvp3026_ramdac_t* ramdac)
{
/*Fvco = 8 x Fref x (65 - M) / (65 - N)*/
/*Fpll = Fvco / 2^P*/
        int n = ramdac->pix.n & 0x3f;
        int m = ramdac->pix.m & 0x3f;
        int p = ramdac->pix.p & 0x03;
        double f_vco = 8.0 * 14318184 * (double)(65 - m) / (double)(65 - n);
        double f_pll = f_vco / (double)(1 << p);

//        pclog("f_pll=%g %i\n", f_pll, (int)f_pll);
        return f_pll;
}

void tvp3026_set_cursor_enable(tvp3026_ramdac_t* ramdac, svga_t* svga, int ena)
{
        ramdac->cursor_ena = ena;
        if (ena)
                update_cursor(ramdac, svga);
}

void tvp3026_hwcursor_draw(tvp3026_ramdac_t* ramdac, svga_t* svga, int displine)
{
        int offset = (svga->hwcursor_latch.x + 32) - svga->hwcursor.xoff;
        int x;
        uint8_t* cursor_data = &ramdac->cursor_data[svga->hwcursor_latch.addr];

        for (x = 0; x < 64; x++)
        {
                if ((offset + x) >= 32)
                {
                        int p0 = cursor_data[x >> 3] & (0x80 >> (x & 7));
                        int p1 = cursor_data[(x >> 3) + 512] & (0x80 >> (x & 7));

                        switch (ramdac->cursor_mode)
                        {
                        case DCC_MODE_SEL_3_COLOUR:
                                if (p0 || p1)
                                {
                                        int col = (p0 ? 1 : 0) | (p1 ? 2 : 0);
                                        ((uint32_t*)buffer32->line[displine])[x + offset] = ramdac->cursor_pal_look[col];
                                }
                                break;

                        case DCC_MODE_SEL_XGA:
                                if (!p1)
                                        ((uint32_t*)buffer32->line[displine])[x + offset] = ramdac->cursor_pal_look[p0 ? 2 : 1];
                                else if (p0)
                                        ((uint32_t*)buffer32->line[displine])[x + offset] ^= 0xffffff;
                                break;

                        case DCC_MODE_SEL_XWINDOWS:
                                if (p1)
                                        ((uint32_t*)buffer32->line[displine])[x + offset] = ramdac->cursor_pal_look[p0 ? 2 : 1];
                                break;
                        }
                }
        }
        svga->hwcursor_latch.addr += 8;
}

void tvp3026_init(tvp3026_ramdac_t* ramdac)
{
        ramdac->regs[0x0f] = 0x06; /*Latch control*/
        ramdac->regs[0x18] = 0x80; /*True color control*/
        ramdac->regs[0x19] = 0x80; /*Multiplex control*/
        ramdac->regs[0x1a] = 0x80; /*Clock selection*/
        ramdac->regs[0x39] = 0x18; /*MCLK / Loop clock control*/
        ramdac->regs[0x3f] = 0x26; /*ID*/
}
