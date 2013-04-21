/*S3 emulation*/
#include "ibm.h"
#include "mem.h"
#include "pci.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_sdac_ramdac.h"

void s3_updatemapping();

void s3_accel_write(uint32_t addr, uint8_t val);
void s3_accel_write_w(uint32_t addr, uint16_t val);
void s3_accel_write_l(uint32_t addr, uint32_t val);
uint8_t s3_accel_read(uint32_t addr);

static uint8_t s3_bank;
static uint8_t s3_ma_ext;
static int s3_width = 1024;
static int s3_bpp = 0;

static uint8_t s3_id, s3_id_ext;

void s3_out(uint16_t addr, uint8_t val)
{
        uint8_t old;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
//        pclog("S3 out %04X %02X\n", addr, val);

        switch (addr)
        {
                case 0x3c5:
                if (seqaddr == 4) /*Chain-4 - update banking*/
                {
                        if (val & 8) svgawbank = svgarbank = s3_bank << 16;
                        else         svgawbank = svgarbank = s3_bank << 14;
                }
                break;
                
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Write RAMDAC %04X %02X %04X:%04X\n", addr, val, CS, pc);
                sdac_ramdac_out(addr,val);
                return;

                case 0x3D4:
                crtcreg=val&0x7f;
                return;
                case 0x3D5:
//                        if (crtcreg == 0x67) pclog("Write CRTC R%02X %02X\n", crtcreg, val);
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                if (crtcreg >= 0x20 && crtcreg != 0x38 && (crtc[0x38] & 0xcc) != 0x48) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;
                switch (crtcreg)
                {
                        case 0x31:
                        s3_ma_ext = (s3_ma_ext & 0x1c) | ((val & 0x30) >> 4);
                        vrammask = (val & 8) ? 0x3fffff : 0x3ffff;
                        break;
                        
                        case 0x50:
                        switch (crtc[0x50] & 0xc1)
                        {
                                case 0x00: s3_width = (crtc[0x31] & 2) ? 2048 : 1024; break;
                                case 0x01: s3_width = 1152; break;
                                case 0x40: s3_width = 640;  break;
                                case 0x80: s3_width = 800;  break;
                                case 0x81: s3_width = 1600; break;
                                case 0xc0: s3_width = 1280; break;
                        }
                        s3_bpp = (crtc[0x50] >> 4) & 3;
                        break;
                        case 0x69:
                        s3_ma_ext = val & 0x1f;
                        break;
                        
                        case 0x35:
                        s3_bank = (s3_bank & 0x70) | (val & 0xf);
//                        pclog("CRTC write R35 %02X\n", val);
                        if (chain4) svgawbank = svgarbank = s3_bank << 16;
                        else        svgawbank = svgarbank = s3_bank << 14;
                        break;
                        case 0x51:
                        s3_bank = (s3_bank & 0x4f) | ((val & 0xc) << 2);
                        if (chain4) svgawbank = svgarbank = s3_bank << 16;
                        else        svgawbank = svgarbank = s3_bank << 14;
                        s3_ma_ext = (s3_ma_ext & ~0xc) | ((val & 3) << 2);
                        break;
                        case 0x6a:
                        s3_bank = val;
//                        pclog("CRTC write R6a %02X\n", val);
                        if (chain4) svgawbank = svgarbank = s3_bank << 16;
                        else        svgawbank = svgarbank = s3_bank << 14;
                        break;
                        
                        case 0x3a:
                        if (val & 0x10) gdcreg[5] |= 0x40; /*Horrible cheat*/
                        break;
                        
                        case 0x45:
                        svga_hwcursor.ena = val & 1;
                        break;
                        case 0x48:
                        svga_hwcursor.x = ((crtc[0x46] << 8) | crtc[0x47]) & 0x7ff;
                        if (bpp == 32) svga_hwcursor.x >>= 1;
                        svga_hwcursor.y = ((crtc[0x48] << 8) | crtc[0x49]) & 0x7ff;
                        svga_hwcursor.xoff = crtc[0x4e] & 63;
                        svga_hwcursor.yoff = crtc[0x4f] & 63;
                        svga_hwcursor.addr = ((((crtc[0x4c] << 8) | crtc[0x4d]) & 0xfff) * 1024) + (svga_hwcursor.yoff * 16);
                        break;

                        case 0x53:
                        case 0x58: case 0x59: case 0x5a:
                        s3_updatemapping();
                        break;
                        
                        case 0x67:
                        if (gfxcard == GFX_N9_9FX) /*Trio64*/
                        {
                                switch (val >> 4)
                                {
                                        case 3:  bpp = 15; break;
                                        case 5:  bpp = 16; break;
                                        case 7:  bpp = 24; break;
                                        case 13: bpp = 32; break;
                                        default: bpp = 8;  break;
                                }
                        }
                        break;
                        //case 0x55: case 0x43:
//                                pclog("Write CRTC R%02X %02X\n", crtcreg, val);
                }
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                svga_recalctimings();
                        }
                }
                break;
        }
        svga_out(addr,val);
}

uint8_t s3_in(uint16_t addr)
{
        uint8_t temp;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

//        if (addr != 0x3da) pclog("S3 in %04X\n", addr);
        switch (addr)
        {
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Read RAMDAC %04X  %04X:%04X\n", addr, CS, pc);
                return sdac_ramdac_in(addr);

                case 0x3D4:
                return crtcreg;
                case 0x3D5:
//                pclog("Read CRTC R%02X %04X:%04X\n", crtcreg, CS, pc);
                switch (crtcreg)
                {
                        case 0x2d: return 0x88;      /*Extended chip ID*/
                        case 0x2e: return s3_id_ext; /*New chip ID*/
                        case 0x30: return s3_id;     /*Chip ID*/
                        case 0x31: return (crtc[0x31] & 0xcf) | ((s3_ma_ext & 3) << 4);
                        case 0x35: return (crtc[0x35] & 0xf0) | (s3_bank & 0xf);
                        case 0x51: return (crtc[0x51] & 0xf0) | ((s3_bank >> 2) & 0xc) | ((s3_ma_ext >> 2) & 3);
                        case 0x69: return s3_ma_ext;
                        case 0x6a: return s3_bank;
                }
                return crtc[crtcreg];
        }
        return svga_in(addr);
}

void s3_recalctimings()
{
//        pclog("recalctimings\n");
        svga_ma |= (s3_ma_ext << 16);
//        pclog("SVGA_MA %08X\n", svga_ma);
        if (gdcreg[5] & 0x40) svga_lowres = !(crtc[0x3a] & 0x10);
        if (crtc[0x5d] & 0x01) svga_htotal     += 0x100;
        if (crtc[0x5d] & 0x02) svga_hdisp      += 0x100;
        if (crtc[0x5e] & 0x01) svga_vtotal     += 0x400;
        if (crtc[0x5e] & 0x02) svga_dispend    += 0x400;
        if (crtc[0x5e] & 0x10) svga_vsyncstart += 0x400;
        if (crtc[0x5e] & 0x40) svga_split      += 0x400;
        if (crtc[0x51] & 0x30)      svga_rowoffset  += (crtc[0x51] & 0x30) << 4;
        else if (crtc[0x43] & 0x04) svga_rowoffset  += 0x100;
        if (!svga_rowoffset) svga_rowoffset = 256;
        svga_interlace = crtc[0x42] & 0x20;
        svga_clock = cpuclock / sdac_getclock((svga_miscout >> 2) & 3);
//        pclog("SVGA_CLOCK = %f  %02X  %f\n", svga_clock, svga_miscout, cpuclock);
        if (bpp > 8) svga_clock /= 2;
}

static uint32_t s3_linear_base = 0, s3_linear_size = 0;
void s3_updatemapping()
{
        mem_removehandler(s3_linear_base, s3_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear);
        
//        video_write_a000_w = video_write_a000_l = NULL;

        mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
//        pclog("Update mapping - bank %02X ", gdcreg[6] & 0xc);        
        switch (gdcreg[6] & 0xc) /*Banked framebuffer*/
        {
                case 0x0: /*128k at A0000*/
                mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                break;
                case 0x4: /*64k at A0000*/
                mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                break;
                case 0x8: /*32k at B0000*/
                mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                break;
                case 0xC: /*32k at B8000*/
                mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                break;
        }
        
//        pclog("Linear framebuffer %02X ", crtc[0x58] & 0x10);
        if (crtc[0x58] & 0x10) /*Linear framebuffer*/
        {
                s3_linear_base = (crtc[0x5a] << 16) | (crtc[0x59] << 24);
                switch (crtc[0x58] & 3)
                {
                        case 0: /*64k*/
                        s3_linear_size = 0x10000;
                        break;
                        case 1: /*1mb*/
                        s3_linear_size = 0x100000;
                        break;
                        case 2: /*2mb*/
                        s3_linear_size = 0x200000;
                        break;
                        case 3: /*8mb*/
                        s3_linear_size = 0x800000;
                        break;
                }
                s3_linear_base &= ~(s3_linear_size - 1);
//                pclog("%08X %08X  %02X %02X %02X\n", linear_base, linear_size, crtc[0x58], crtc[0x59], crtc[0x5a]);
//                pclog("Linear framebuffer at %08X size %08X\n", linear_base, linear_size);
                if (s3_linear_base == 0xa0000)
                   mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                else
                   mem_sethandler(s3_linear_base, s3_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear);
        }
        
//        pclog("Memory mapped IO %02X\n", crtc[0x53] & 0x10);
        if (crtc[0x53] & 0x10) /*Memory mapped IO*/
        {
                mem_sethandler(0xa0000, 0x10000, s3_accel_read, NULL, NULL, s3_accel_write, s3_accel_write_w, s3_accel_write_l);
/*                video_write_a000   = s3_accel_write;
                video_write_a000_w = s3_accel_write_w;
                video_write_a000_l = s3_accel_write_l;
                video_read_a000    = s3_accel_read;*/
        }
}


struct
{
        uint8_t subsys_cntl;
        uint8_t setup_md;
        uint8_t advfunc_cntl;
        uint16_t cur_y;
        uint16_t cur_x;
         int16_t desty_axstp;
         int16_t destx_distp;
         int16_t err_term;
         int16_t maj_axis_pcnt;
        uint16_t cmd;
        uint16_t short_stroke;
        uint32_t bkgd_color;
        uint32_t frgd_color;
        uint32_t wrt_mask;
        uint32_t rd_mask;
        uint32_t color_cmp;
        uint8_t bkgd_mix;
        uint8_t frgd_mix;
        uint16_t multifunc_cntl;
        uint16_t multifunc[16];
        uint8_t pix_trans[4];
        
        int cx, cy;
        int sx, sy;
        int dx, dy;
        uint32_t src, dest, pattern;
        int pix_trans_count;
        
        uint32_t dat_buf;
        int dat_count;
} s3_accel;

void s3_accel_start(int count, int cpu_input, uint32_t mix_dat, uint32_t cpu_dat);

void s3_accel_out(uint16_t port, uint8_t val)
{
//        pclog("Accel out %04X %02X\n", port, val);
        switch (port)
        {
                case 0x42e8:
                break;
                case 0x42e9:
                s3_accel.subsys_cntl = val;
                break;
                case 0x46e8:
                s3_accel.setup_md = val;
                break;
                case 0x4ae8:
                s3_accel.advfunc_cntl = val;
                break;
                
                case 0x82e8:
                s3_accel.cur_y = (s3_accel.cur_y & 0xf00) | val;
                break;
                case 0x82e9:
                s3_accel.cur_y = (s3_accel.cur_y & 0xff) | ((val & 0x1f) << 8);
                break;
                
                case 0x86e8:
                s3_accel.cur_x = (s3_accel.cur_x & 0xf00) | val;
                break;
                case 0x86e9:
                s3_accel.cur_x = (s3_accel.cur_x & 0xff) | ((val & 0x1f) << 8);
                break;
                
                case 0x8ae8:
                s3_accel.desty_axstp = (s3_accel.desty_axstp & 0x3f00) | val;
                break;
                case 0x8ae9:
                s3_accel.desty_axstp = (s3_accel.desty_axstp & 0xff) | ((val & 0x3f) << 8);
                if (val & 0x20)
                   s3_accel.desty_axstp |= ~0x3fff;
                break;
                
                case 0x8ee8:
                s3_accel.destx_distp = (s3_accel.destx_distp & 0x3f00) | val;
                break;
                case 0x8ee9:
                s3_accel.destx_distp = (s3_accel.destx_distp & 0xff) | ((val & 0x3f) << 8);
                if (val & 0x20)
                   s3_accel.destx_distp |= ~0x3fff;
                break;
                
                case 0x92e8:
                s3_accel.err_term = (s3_accel.err_term & 0x3f00) | val;
                break;
                case 0x92e9:
                s3_accel.err_term = (s3_accel.err_term & 0xff) | ((val & 0x3f) << 8);
                if (val & 0x20)
                   s3_accel.err_term |= ~0x3fff;
                break;

                case 0x96e8:
                s3_accel.maj_axis_pcnt = (s3_accel.maj_axis_pcnt & 0x3f00) | val;
                break;
                case 0x96e9:
                s3_accel.maj_axis_pcnt = (s3_accel.maj_axis_pcnt & 0xff) | ((val & 0x0f) << 8);
                if (val & 0x08)
                   s3_accel.maj_axis_pcnt |= ~0x0fff;
                break;

                case 0x9ae8:
                s3_accel.cmd = (s3_accel.cmd & 0xff00) | val;
                break;
                case 0x9ae9:
                s3_accel.cmd = (s3_accel.cmd & 0xff) | (val << 8);
                s3_accel_start(-1, 0, 0xffffffff, 0);
                s3_accel.pix_trans_count = 0;
                break;

                case 0x9ee8:
                s3_accel.short_stroke = (s3_accel.short_stroke & 0xff00) | val;
                break;
                case 0x9ee9:
                s3_accel.short_stroke = (s3_accel.short_stroke & 0xff) | (val << 8);
                break;

                case 0xa2e8:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.bkgd_color = (s3_accel.bkgd_color & ~0x00ff0000) | (val << 16);
                else
                   s3_accel.bkgd_color = (s3_accel.bkgd_color & ~0x000000ff) | val;
                break;
                case 0xa2e9:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.bkgd_color = (s3_accel.bkgd_color & ~0xff000000) | (val << 24);
                else
                   s3_accel.bkgd_color = (s3_accel.bkgd_color & ~0x0000ff00) | (val << 8);
                s3_accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xa6e8:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.frgd_color = (s3_accel.frgd_color & ~0x00ff0000) | (val << 16);
                else
                   s3_accel.frgd_color = (s3_accel.frgd_color & ~0x000000ff) | val;
//                pclog("Foreground colour now %08X %i\n", s3_accel.frgd_color, s3_bpp);
                break;
                case 0xa6e9:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.frgd_color = (s3_accel.frgd_color & ~0xff000000) | (val << 24);
                else
                   s3_accel.frgd_color = (s3_accel.frgd_color & ~0x0000ff00) | (val << 8);
                s3_accel.multifunc[0xe] ^= 0x10;
//                pclog("Foreground colour now %08X\n", s3_accel.frgd_color);
                break;

                case 0xaae8:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.wrt_mask = (s3_accel.wrt_mask & ~0x00ff0000) | (val << 16);
                else
                   s3_accel.wrt_mask = (s3_accel.wrt_mask & ~0x000000ff) | val;
                break;
                case 0xaae9:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.wrt_mask = (s3_accel.wrt_mask & ~0xff000000) | (val << 24);
                else
                   s3_accel.wrt_mask = (s3_accel.wrt_mask & ~0x0000ff00) | (val << 8);
                s3_accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xaee8:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.rd_mask = (s3_accel.rd_mask & ~0x00ff0000) | (val << 16);
                else
                   s3_accel.rd_mask = (s3_accel.rd_mask & ~0x000000ff) | val;
                break;
                case 0xaee9:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.rd_mask = (s3_accel.rd_mask & ~0xff000000) | (val << 24);
                else
                   s3_accel.rd_mask = (s3_accel.rd_mask & ~0x0000ff00) | (val << 8);
                s3_accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xb2e8:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.color_cmp = (s3_accel.color_cmp & ~0x00ff0000) | (val << 16);
                else
                   s3_accel.color_cmp = (s3_accel.color_cmp & ~0x000000ff) | val;
                break;
                case 0xb2e9:
                if (s3_bpp == 3 && s3_accel.multifunc[0xe] & 0x10)
                   s3_accel.color_cmp = (s3_accel.color_cmp & ~0xff000000) | (val << 24);
                else
                   s3_accel.color_cmp = (s3_accel.color_cmp & ~0x0000ff00) | (val << 8);
                s3_accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xb6e8:
                s3_accel.bkgd_mix = val;
                break;

                case 0xbae8:
                s3_accel.frgd_mix = val;
                break;
                
                case 0xbee8:
                s3_accel.multifunc_cntl = (s3_accel.multifunc_cntl & 0xff00) | val;
                break;
                case 0xbee9:
                s3_accel.multifunc_cntl = (s3_accel.multifunc_cntl & 0xff) | (val << 8);
                s3_accel.multifunc[s3_accel.multifunc_cntl >> 12] = s3_accel.multifunc_cntl & 0xfff;
                break;

                case 0xe2e8:
                s3_accel.pix_trans[0] = val;
                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80 && !(s3_accel.cmd & 0x600) && (s3_accel.cmd & 0x100))
                   s3_accel_start(8, 1, s3_accel.pix_trans[0], 0);
                else if (!(s3_accel.cmd & 0x600) && (s3_accel.cmd & 0x100))
                   s3_accel_start(1, 1, 0xffffffff, s3_accel.pix_trans[0]);
                break;
                case 0xe2e9:
                s3_accel.pix_trans[1] = val;
                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80 && (s3_accel.cmd & 0x600) == 0x200 && (s3_accel.cmd & 0x100))
                {
                        if (s3_accel.cmd & 0x1000) s3_accel_start(16, 1, s3_accel.pix_trans[1] | (s3_accel.pix_trans[0] << 8), 0);
                        else                       s3_accel_start(16, 1, s3_accel.pix_trans[0] | (s3_accel.pix_trans[1] << 8), 0);
                }
                else if ((s3_accel.cmd & 0x600) == 0x200 && (s3_accel.cmd & 0x100))
                {
                        if (s3_accel.cmd & 0x1000) s3_accel_start(2, 1, 0xffffffff, s3_accel.pix_trans[1] | (s3_accel.pix_trans[0] << 8));
                        else                       s3_accel_start(2, 1, 0xffffffff, s3_accel.pix_trans[0] | (s3_accel.pix_trans[1] << 8));
                }
                break;
                case 0xe2ea:
                s3_accel.pix_trans[2] = val;
                break;
                case 0xe2eb:
                s3_accel.pix_trans[3] = val;
                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80 && (s3_accel.cmd & 0x600) == 0x400 && (s3_accel.cmd & 0x100))
                   s3_accel_start(32, 1, s3_accel.pix_trans[0] | (s3_accel.pix_trans[1] << 8) | (s3_accel.pix_trans[2] << 16) | (s3_accel.pix_trans[3] << 24), 0);
                else if ((s3_accel.cmd & 0x600) == 0x400 && (s3_accel.cmd & 0x100))
                   s3_accel_start(4, 1, 0xffffffff, s3_accel.pix_trans[0] | (s3_accel.pix_trans[1] << 8) | (s3_accel.pix_trans[2] << 16) | (s3_accel.pix_trans[3] << 24));
                break;
        }
}

void s3_accel_out_w(uint16_t port, uint16_t val)
{
//        pclog("Accel out w %04X %04X\n", port, val);
        if (s3_accel.cmd & 0x100)
        {
                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80)
                {
                        if (s3_accel.cmd & 0x1000)
                           val = (val >> 8) | (val << 8);
                        s3_accel_start(16, 1, val | (val << 16), 0);
                }
                else
                   s3_accel_start(2, 1, 0xffffffff, val | (val << 16));
        }
}

void s3_accel_out_l(uint16_t port, uint32_t val)
{
//        pclog("Accel out l %04X %08X\n", port, val);
        if (s3_accel.cmd & 0x100)
        {
                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80)
                {
                        if (s3_accel.cmd & 0x1000)
                           val = ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
                        if ((s3_accel.cmd & 0x600) == 0x400)
                           s3_accel_start(32, 1, val, 0);
                        else if ((s3_accel.cmd & 0x600) == 0x200)
                        {
                                s3_accel_start(16, 1, val >> 16, 0);
                                s3_accel_start(16, 1, val, 0);
                        }
                        else if (!(s3_accel.cmd & 0x600))
                        {
                                s3_accel_start(8, 1, val >> 24, 0);
                                s3_accel_start(8, 1, val >> 16, 0);
                                s3_accel_start(8, 1, val >> 8,  0);
                                s3_accel_start(8, 1, val,       0);
                        }
                }
                else
                {
                        if ((s3_accel.cmd & 0x600) == 0x400)
                           s3_accel_start(4, 1, 0xffffffff, val);
                        else if ((s3_accel.cmd & 0x600) == 0x200)
                        {
                                s3_accel_start(2, 1, 0xffffffff, val >> 16);
                                s3_accel_start(2, 1, 0xffffffff, val);
                        }
                        else if (!(s3_accel.cmd & 0x600))
                        {
                                s3_accel_start(1, 1, 0xffffffff, val >> 24);
                                s3_accel_start(1, 1, 0xffffffff, val >> 16);
                                s3_accel_start(1, 1, 0xffffffff, val >> 8);
                                s3_accel_start(1, 1, 0xffffffff, val);
                        }
                }
        }
}

uint8_t s3_accel_in(uint16_t port)
{
        int temp;
//        pclog("Accel in  %04X\n", port);
        switch (port)
        {
                case 0x42e8:
                return 0;
                case 0x42e9:
                return 0;

                case 0x82e8:
                return s3_accel.cur_y & 0xff;
                case 0x82e9:
                return s3_accel.cur_y  >> 8;

                case 0x86e8:
                return s3_accel.cur_x & 0xff;
                case 0x86e9:
                return s3_accel.cur_x  >> 8;

                case 0x8ae8:
                return s3_accel.desty_axstp & 0xff;
                case 0x8ae9:
                return s3_accel.desty_axstp >> 8;

                case 0x8ee8:
                return s3_accel.destx_distp & 0xff;
                case 0x8ee9:
                return s3_accel.destx_distp >> 8;

                case 0x92e8:
                return s3_accel.err_term & 0xff;
                case 0x92e9:
                return s3_accel.err_term >> 8;

                case 0x96e8:
                return s3_accel.maj_axis_pcnt & 0xff;
                case 0x96e9:
                return s3_accel.maj_axis_pcnt >> 8;

                case 0x9ae8:
                return 0;
                case 0x9ae9:
                return 0;

                case 0xa2e8:
                return s3_accel.bkgd_color & 0xff;
                case 0xa2e9:
                return s3_accel.bkgd_color >> 8;

                case 0xa6e8:
                return s3_accel.frgd_color & 0xff;
                case 0xa6e9:
                return s3_accel.frgd_color >> 8;

                case 0xaae8:
                return s3_accel.wrt_mask & 0xff;
                case 0xaae9:
                return s3_accel.wrt_mask >> 8;

                case 0xaee8:
                return s3_accel.rd_mask & 0xff;
                case 0xaee9:
                return s3_accel.rd_mask >> 8;

                case 0xb2e8:
                return s3_accel.color_cmp & 0xff;
                case 0xb2e9:
                return s3_accel.color_cmp >> 8;

                case 0xb6e8:
                return s3_accel.bkgd_mix;

                case 0xbae8:
                return s3_accel.frgd_mix;

                case 0xbee8:
                temp = s3_accel.multifunc[0xf] & 0xf;
                switch (temp)
                {
                        case 0x0: return s3_accel.multifunc[0x0] & 0xff;
                        case 0x1: return s3_accel.multifunc[0x1] & 0xff;
                        case 0x2: return s3_accel.multifunc[0x2] & 0xff;
                        case 0x3: return s3_accel.multifunc[0x3] & 0xff;
                        case 0x4: return s3_accel.multifunc[0x4] & 0xff;
                        case 0x5: return s3_accel.multifunc[0xa] & 0xff;
                        case 0x6: return s3_accel.multifunc[0xe] & 0xff;
                        case 0x7: return s3_accel.cmd            & 0xff;
                        case 0x8: return s3_accel.subsys_cntl    & 0xff;
                        case 0x9: return s3_accel.setup_md       & 0xff;
                        case 0xa: return s3_accel.multifunc[0xd] & 0xff;
                }
                return 0xff;
                case 0xbee9:
                temp = s3_accel.multifunc[0xf] & 0xf;
                s3_accel.multifunc[0xf]++;
                switch (temp)
                {
                        case 0x0: return  s3_accel.multifunc[0x0] >> 8;
                        case 0x1: return  s3_accel.multifunc[0x1] >> 8;
                        case 0x2: return  s3_accel.multifunc[0x2] >> 8;
                        case 0x3: return  s3_accel.multifunc[0x3] >> 8;
                        case 0x4: return  s3_accel.multifunc[0x4] >> 8;
                        case 0x5: return  s3_accel.multifunc[0xa] >> 8;
                        case 0x6: return  s3_accel.multifunc[0xe] >> 8;
                        case 0x7: return  s3_accel.cmd            >> 8;
                        case 0x8: return (s3_accel.subsys_cntl    >> 8) & ~0xe000;
                        case 0x9: return (s3_accel.setup_md       >> 8) & ~0xf000;
                        case 0xa: return  s3_accel.multifunc[0xd] >> 8;
                }
                return 0xff;

                case 0xe2e8: case 0xe2e9: case 0xe2ea: case 0xe2eb: /*PIX_TRANS*/
                break;
        }
        return 0;
}

void s3_accel_write(uint32_t addr, uint8_t val)
{
//        pclog("Write S3 accel %08X %02X\n", addr, val);
        if (addr & 0x8000)
           s3_accel_out(addr & 0xffff, val);
        else
        {
                if (s3_accel.cmd & 0x100)
                {
                        if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80)
                           s3_accel_start(8, 1, val | (val << 8) | (val << 16) | (val << 24), 0);
                        else
                           s3_accel_start(1, 1, 0xffffffff, val | (val << 8) | (val << 16) | (val << 24));
                }
        }
}

void s3_accel_write_w(uint32_t addr, uint16_t val)
{
//        pclog("Write S3 accel w %08X %04X\n", addr, val);
        if (addr & 0x8000)
        {
                s3_accel_out( addr & 0xffff,      val);
                s3_accel_out((addr & 0xffff) + 1, val >> 8);
        }
        else
        {
                if (s3_accel.cmd & 0x100)
                {
                        if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80)
                        {
                                if (s3_accel.cmd & 0x1000)
                                   val = (val >> 8) | (val << 8);
                                s3_accel_start(16, 1, val | (val << 16), 0);
                        }
                        else
                           s3_accel_start(2, 1, 0xffffffff, val | (val << 16));
                }
        }
}

void s3_accel_write_l(uint32_t addr, uint32_t val)
{
//        pclog("Write S3 accel l %08X %08X\n", addr, val);
        if (addr & 0x8000)
        {
                s3_accel_out( addr & 0xffff,      val);
                s3_accel_out((addr & 0xffff) + 1, val >> 8);
                s3_accel_out((addr & 0xffff) + 2, val >> 16);
                s3_accel_out((addr & 0xffff) + 3, val >> 24);
        }
        else
        {
                if (s3_accel.cmd & 0x100)
                {
                        if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80)
                        {
                                if (s3_accel.cmd & 0x1000)
                                   val = ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
                                if ((s3_accel.cmd & 0x600) == 0x400)
                                   s3_accel_start(32, 1, val, 0);
                                else if ((s3_accel.cmd & 0x600) == 0x200)
                                {
                                        s3_accel_start(16, 1, val >> 16, 0);
                                        s3_accel_start(16, 1, val, 0);
                                }
                                else if (!(s3_accel.cmd & 0x600))
                                {
                                        s3_accel_start(8, 1, val >> 24, 0);
                                        s3_accel_start(8, 1, val >> 16, 0);
                                        s3_accel_start(8, 1, val >> 8,  0);
                                        s3_accel_start(8, 1, val,       0);
                                }
                        }
                        else
                        {
                                if ((s3_accel.cmd & 0x600) == 0x400)
                                   s3_accel_start(4, 1, 0xffffffff, val);
                                else if ((s3_accel.cmd & 0x600) == 0x200)
                                {
                                        s3_accel_start(2, 1, 0xffffffff, val >> 16);
                                        s3_accel_start(2, 1, 0xffffffff, val);
                                }
                                else if (!(s3_accel.cmd & 0x600))
                                {
                                        s3_accel_start(1, 1, 0xffffffff, val >> 24);
                                        s3_accel_start(1, 1, 0xffffffff, val >> 16);
                                        s3_accel_start(1, 1, 0xffffffff, val >> 8);
                                        s3_accel_start(1, 1, 0xffffffff, val);
                                }
                        }
                }
        }
}

uint8_t s3_accel_read(uint32_t addr)
{
//        pclog("Read S3 accel %08X\n", addr);
        if (addr & 0x8000)
           return s3_accel_in(addr & 0xffff);
        return 0;
}

#define READ(addr, dat) if (s3_bpp == 0)      dat = vram[  (addr) & 0x3fffff]; \
                        else if (s3_bpp == 1) dat = vram_w[(addr) & 0x1fffff]; \
                        else                  dat = vram_l[(addr) & 0x0fffff];

#define MIX     switch ((mix_dat & mix_mask) ? (s3_accel.frgd_mix & 0xf) : (s3_accel.bkgd_mix & 0xf))   \
                {                                                                                       \
                        case 0x0: dest_dat =             ~dest_dat;  break;                             \
                        case 0x1: dest_dat =  0;                     break;                             \
                        case 0x2: dest_dat =  1;                     break;                             \
                        case 0x3: dest_dat =              dest_dat;  break;                             \
                        case 0x4: dest_dat =  ~src_dat;              break;                             \
                        case 0x5: dest_dat =   src_dat ^  dest_dat;  break;                             \
                        case 0x6: dest_dat = ~(src_dat ^  dest_dat); break;                             \
                        case 0x7: dest_dat =   src_dat;              break;                             \
                        case 0x8: dest_dat = ~(src_dat &  dest_dat); break;                             \
                        case 0x9: dest_dat =  ~src_dat |  dest_dat;  break;                             \
                        case 0xa: dest_dat =   src_dat | ~dest_dat;  break;                             \
                        case 0xb: dest_dat =   src_dat |  dest_dat;  break;                             \
                        case 0xc: dest_dat =   src_dat &  dest_dat;  break;                             \
                        case 0xd: dest_dat =   src_dat & ~dest_dat;  break;                             \
                        case 0xe: dest_dat =  ~src_dat &  dest_dat;  break;                             \
                        case 0xf: dest_dat = ~(src_dat |  dest_dat); break;                             \
                }


#define WRITE(addr)     if (s3_bpp == 0)                                                                                \
                        {                                                                                               \
                                vram[(addr) & 0x3fffff] = dest_dat;                              \
                                changedvram[((addr) & 0x3fffff) >> 10] = changeframecount;       \
                        }                                                                                               \
                        else if (s3_bpp == 1)                                                                           \
                        {                                                                                               \
                                vram_w[(addr) & 0x1fffff] = dest_dat;                            \
                                changedvram[((addr) & 0x1fffff) >> 9] = changeframecount;        \
                        }                                                                                               \
                        else                                                                                            \
                        {                                                                                               \
                                vram_l[(addr) & 0xfffff] = dest_dat;                             \
                                changedvram[((addr) & 0xfffff) >> 8] = changeframecount;         \
                        }

void s3_accel_start(int count, int cpu_input, uint32_t mix_dat, uint32_t cpu_dat)
{
        uint32_t dest, destbak;
        int sx, sy, cx, cy;
        int x, y;
        uint32_t src_dat, dest_dat;
        int frgd_mix, bkgd_mix;
        int mix;
        int clip_t = s3_accel.multifunc[1] & 0xfff;
        int clip_l = s3_accel.multifunc[2] & 0xfff;
        int clip_b = s3_accel.multifunc[3] & 0xfff;
        int clip_r = s3_accel.multifunc[4] & 0xfff;
        int vram_mask = (s3_accel.multifunc[0xa] & 0xc0) == 0xc0;
        uint32_t mix_mask;
        uint16_t *vram_w = (uint16_t *)vram;
        uint32_t *vram_l = (uint32_t *)vram;
        uint32_t compare = s3_accel.color_cmp;
        int compare_mode = (s3_accel.multifunc[0xe] >> 7) & 3;
//return;
//        if (!cpu_input) pclog("Start S3 command %i  %i, %i  %i, %i (clip %i, %i to %i, %i  %i)\n", s3_accel.cmd >> 13, s3_accel.cur_x, s3_accel.cur_y, s3_accel.maj_axis_pcnt & 0xfff, s3_accel.multifunc[0]  & 0xfff, clip_l, clip_t, clip_r, clip_b, s3_accel.multifunc[0xe] & 0x20);
//        else            pclog("      S3 command %i, %i, %08x %08x\n", s3_accel.cmd >> 13, count, mix_dat, cpu_dat);

        if (!cpu_input) s3_accel.dat_count = 0;
        if (cpu_input && (s3_accel.multifunc[0xa] & 0xc0) != 0x80)
        {
                if (s3_bpp == 3 && count == 2)
                {
                        if (s3_accel.dat_count)
                        {
                                cpu_dat = (cpu_dat & 0xffff) | (s3_accel.dat_buf << 16);
                                count = 4;
                                s3_accel.dat_count = 0;
                        }
                        else
                        {
                                s3_accel.dat_buf = cpu_dat & 0xffff;
                                s3_accel.dat_count = 1;
                        }
                }
                if (s3_bpp == 1) count >>= 1;
                if (s3_bpp == 3) count >>= 2;
        }
        
        switch (s3_accel.cmd & 0x600)
        {
                case 0x000: mix_mask = 0x80; break;
                case 0x200: mix_mask = 0x8000; break;
                case 0x400: mix_mask = 0x80000000; break;
                case 0x600: mix_mask = 0x80000000; break;
        }

        if (s3_bpp == 0) compare &=   0xff;
        if (s3_bpp == 1) compare &= 0xffff;
        switch (s3_accel.cmd >> 13)
        {
                case 1: /*Draw line*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3_accel.cx   = s3_accel.cur_x;
                        if (s3_accel.cur_x & 0x1000) s3_accel.cx |= ~0xfff;
                        s3_accel.cy   = s3_accel.cur_y;
                        if (s3_accel.cur_y & 0x1000) s3_accel.cy |= ~0xfff;
                        
                        s3_accel.sy = s3_accel.maj_axis_pcnt;
                }
                if ((s3_accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
                if (s3_accel.cmd & 8) /*Radial*/
                {
                        while (count-- && s3_accel.sy >= 0)
                        {
                                if (s3_accel.cx >= clip_l && s3_accel.cx <= clip_r &&
                                    s3_accel.cy >= clip_t && s3_accel.cy <= clip_b)
                                {
                                        switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                        {
                                                case 0: src_dat = s3_accel.bkgd_color; break;
                                                case 1: src_dat = s3_accel.frgd_color; break;
                                                case 2: src_dat = cpu_dat; break;
                                                case 3: src_dat = 0; break;
                                        }

                                        if ((compare_mode == 2 && src_dat != compare) ||
                                            (compare_mode == 3 && src_dat == compare) ||
                                             compare_mode < 2)
                                        {
                                                READ((s3_accel.cy * s3_width) + s3_accel.cx, dest_dat);
                                        
                                                MIX

                                                WRITE((s3_accel.cy * s3_width) + s3_accel.cx);
                                        }
                                }

                                mix_dat <<= 1;
                                mix_dat |= 1;
                                if (s3_bpp == 0) cpu_dat >>= 8;
                                else             cpu_dat >>= 16;

                                switch (s3_accel.cmd & 0xe0)
                                {
                                        case 0x00: s3_accel.cx++;                break;
                                        case 0x20: s3_accel.cx++; s3_accel.cy--; break;
                                        case 0x40:                s3_accel.cy--; break;
                                        case 0x60: s3_accel.cx--; s3_accel.cy--; break;
                                        case 0x80: s3_accel.cx--;                break;
                                        case 0xa0: s3_accel.cx--; s3_accel.cy++; break;
                                        case 0xc0:                s3_accel.cy++; break;
                                        case 0xe0: s3_accel.cx++; s3_accel.cy++; break;
                                }
                                s3_accel.sy--;
                        }
                }
                else /*Bresenham*/
                {
                        while (count-- && s3_accel.sy >= 0)
                        {
                                if (s3_accel.cx >= clip_l && s3_accel.cx <= clip_r &&
                                    s3_accel.cy >= clip_t && s3_accel.cy <= clip_b)
                                {
                                        switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                        {
                                                case 0: src_dat = s3_accel.bkgd_color; break;
                                                case 1: src_dat = s3_accel.frgd_color; break;
                                                case 2: src_dat = cpu_dat; break;
                                                case 3: src_dat = 0; break;
                                        }

                                        if ((compare_mode == 2 && src_dat != compare) ||
                                            (compare_mode == 3 && src_dat == compare) ||
                                             compare_mode < 2)
                                        {
                                                READ((s3_accel.cy * s3_width) + s3_accel.cx, dest_dat);

//                                        pclog("Line : %04i, %04i (%06X) - %02X (%02X %04X %05X) %02X (%02X %02X)  ", s3_accel.cx, s3_accel.cy, s3_accel.dest + s3_accel.cx, src_dat, vram[s3_accel.src + s3_accel.cx], mix_dat & mix_mask, s3_accel.src + s3_accel.cx, dest_dat, s3_accel.frgd_color, s3_accel.bkgd_color);
                                        
                                                MIX

//                                        pclog("%02X\n", dest_dat);
                                        
                                                WRITE((s3_accel.cy * s3_width) + s3_accel.cx);
                                        }
                                }

                                mix_dat <<= 1;
                                mix_dat |= 1;
                                if (s3_bpp == 0) cpu_dat >>= 8;
                                else             cpu_dat >>= 16;

//                                pclog("%i, %i - %i %i  %i %i\n", s3_accel.cx, s3_accel.cy, s3_accel.err_term, s3_accel.maj_axis_pcnt, s3_accel.desty_axstp, s3_accel.destx_distp);

                                if (s3_accel.err_term >= s3_accel.maj_axis_pcnt)
                                {
                                        s3_accel.err_term += s3_accel.destx_distp;
                                        /*Step minor axis*/
                                        switch (s3_accel.cmd & 0xe0)
                                        {
                                                case 0x00: s3_accel.cy--; break;
                                                case 0x20: s3_accel.cy--; break;
                                                case 0x40: s3_accel.cx--; break;
                                                case 0x60: s3_accel.cx++; break;
                                                case 0x80: s3_accel.cy++; break;
                                                case 0xa0: s3_accel.cy++; break;
                                                case 0xc0: s3_accel.cx--; break;
                                                case 0xe0: s3_accel.cx++; break;
                                        }
                                }
                                else
                                   s3_accel.err_term += s3_accel.desty_axstp;

                                /*Step major axis*/
                                switch (s3_accel.cmd & 0xe0)
                                {
                                        case 0x00: s3_accel.cx--; break;
                                        case 0x20: s3_accel.cx++; break;
                                        case 0x40: s3_accel.cy--; break;
                                        case 0x60: s3_accel.cy--; break;
                                        case 0x80: s3_accel.cx--; break;
                                        case 0xa0: s3_accel.cx++; break;
                                        case 0xc0: s3_accel.cy++; break;
                                        case 0xe0: s3_accel.cy++; break;
                                }
                                s3_accel.sy--;
                        }
                }
                break;
                
                case 2: /*Rectangle fill*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3_accel.sx   = s3_accel.maj_axis_pcnt & 0xfff;
                        s3_accel.sy   = s3_accel.multifunc[0]  & 0xfff;
                        s3_accel.cx   = s3_accel.cur_x;
                        if (s3_accel.cur_x & 0x1000) s3_accel.cx |= ~0xfff;
                        s3_accel.cy   = s3_accel.cur_y;
                        if (s3_accel.cur_y & 0x1000) s3_accel.cy |= ~0xfff;
                        
                        s3_accel.dest = s3_accel.cy * s3_width;

//                        pclog("Dest %08X  (%i, %i) %04X %04X\n", s3_accel.dest, s3_accel.cx, s3_accel.cy, s3_accel.cur_x, s3_accel.cur_x & 0x1000);
                }
                if ((s3_accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
//                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80 && !cpu_input) /*Mix data from CPU*/
//                   return;

                frgd_mix = (s3_accel.frgd_mix >> 5) & 3;
                bkgd_mix = (s3_accel.bkgd_mix >> 5) & 3;
                
                while (count-- && s3_accel.sy >= 0)
                {
                        if (s3_accel.cx >= clip_l && s3_accel.cx <= clip_r &&
                            s3_accel.cy >= clip_t && s3_accel.cy <= clip_b)
                        {
                                switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                {
                                        case 0: src_dat = s3_accel.bkgd_color; break;
                                        case 1: src_dat = s3_accel.frgd_color; break;
                                        case 2: src_dat = cpu_dat; break;
                                        case 3: src_dat = 0; break;
                                }

                                if ((compare_mode == 2 && src_dat != compare) ||
                                    (compare_mode == 3 && src_dat == compare) ||
                                     compare_mode < 2)
                                {
                                        READ(s3_accel.dest + s3_accel.cx, dest_dat);
                                
//                                if (CS != 0xc000) pclog("Write %05X  %02X %02X  %04X (%02X %02X)  ", s3_accel.dest + s3_accel.cx, src_dat, dest_dat, mix_dat, s3_accel.frgd_mix, s3_accel.bkgd_mix);

                                        MIX
                                
//                                if (CS != 0xc000) pclog("%02X\n", dest_dat);
                                
                                        WRITE(s3_accel.dest + s3_accel.cx);
                                }
                        }
                
                        mix_dat <<= 1;
                        mix_dat |= 1;
                        if (s3_bpp == 0) cpu_dat >>= 8;
                        else             cpu_dat >>= 16;
                        
                        if (s3_accel.cmd & 0x20) s3_accel.cx++;
                        else                     s3_accel.cx--;
                        s3_accel.sx--;
                        if (s3_accel.sx < 0)
                        {
                                if (s3_accel.cmd & 0x20) s3_accel.cx   -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                else                     s3_accel.cx   += (s3_accel.maj_axis_pcnt & 0xfff) + 1;
//                                s3_accel.dest -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                s3_accel.sx    = s3_accel.maj_axis_pcnt & 0xfff;
                                
//                                s3_accel.dest  += s3_width;
                                if (s3_accel.cmd & 0x80) s3_accel.cy++;
                                else                     s3_accel.cy--;
                                
                                s3_accel.dest = s3_accel.cy * s3_width;
                                s3_accel.sy--;

                                if (cpu_input/* && (s3_accel.multifunc[0xa] & 0xc0) == 0x80*/) return;
                                if (s3_accel.sy < 0)
                                {
                                        return;
                                }
                        }
                }
                break;

                case 6: /*BitBlt*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3_accel.sx   = s3_accel.maj_axis_pcnt & 0xfff;
                        s3_accel.sy   = s3_accel.multifunc[0]  & 0xfff;

                        s3_accel.dx   = s3_accel.destx_distp & 0xfff;
                        if (s3_accel.destx_distp & 0x1000) s3_accel.dx |= ~0xfff;
                        s3_accel.dy   = s3_accel.desty_axstp & 0xfff;
                        if (s3_accel.desty_axstp & 0x1000) s3_accel.dy |= ~0xfff;

                        s3_accel.cx   = s3_accel.cur_x & 0xfff;
                        if (s3_accel.cur_x & 0x1000) s3_accel.cx |= ~0xfff;
                        s3_accel.cy   = s3_accel.cur_y & 0xfff;
                        if (s3_accel.cur_y & 0x1000) s3_accel.cy |= ~0xfff;

                        s3_accel.src  = s3_accel.cy * s3_width;
                        s3_accel.dest = s3_accel.dy * s3_width;
                        
//                        pclog("Source %08X Dest %08X  (%i, %i) - (%i, %i)\n", s3_accel.src, s3_accel.dest, s3_accel.cx, s3_accel.cy, s3_accel.dx, s3_accel.dy);
                }
                if ((s3_accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
//                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80 && !cpu_input) /*Mix data from CPU*/
//                  return;

                if (s3_accel.sy < 0)
                   return;

                frgd_mix = (s3_accel.frgd_mix >> 5) & 3;
                bkgd_mix = (s3_accel.bkgd_mix >> 5) & 3;
                
                if (!cpu_input && frgd_mix == 3 && !vram_mask && !compare_mode &&
                    (s3_accel.cmd & 0xa0) == 0xa0 && (s3_accel.frgd_mix & 0xf) == 7) 
                {
                        while (1)
                        {
                                if (s3_accel.dx >= clip_l && s3_accel.dx <= clip_r &&
                                    s3_accel.dy >= clip_t && s3_accel.dy <= clip_b)
                                {
                                        READ(s3_accel.src + s3_accel.cx, src_dat);
        
                                        dest_dat = src_dat;
                                        
                                        WRITE(s3_accel.dest + s3_accel.dx);
                                }
        
                                s3_accel.cx++;
                                s3_accel.dx++;
                                s3_accel.sx--;
                                if (s3_accel.sx < 0)
                                {
                                        s3_accel.cx -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                        s3_accel.dx -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                        s3_accel.sx  =  s3_accel.maj_axis_pcnt & 0xfff;
        
                                        s3_accel.cy++;
                                        s3_accel.dy++;
        
                                        s3_accel.src  = s3_accel.cy * s3_width;
                                        s3_accel.dest = s3_accel.dy * s3_width;
        
                                        s3_accel.sy--;
        
                                        if (s3_accel.sy < 0)
                                           return;
                                }
                        }
                }
                else
                {                     
                        while (count-- && s3_accel.sy >= 0)
                        {
                                if (s3_accel.dx >= clip_l && s3_accel.dx <= clip_r &&
                                    s3_accel.dy >= clip_t && s3_accel.dy <= clip_b)
                                {
                                        if (vram_mask)
                                        {
                                                READ(s3_accel.src + s3_accel.cx, mix_dat)
                                                mix_dat = mix_dat ? mix_mask : 0;
                                        }
                                        switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                        {
                                                case 0: src_dat = s3_accel.bkgd_color;                  break;
                                                case 1: src_dat = s3_accel.frgd_color;                  break;
                                                case 2: src_dat = cpu_dat;                              break;
                                                case 3: READ(s3_accel.src + s3_accel.cx, src_dat);      break;
                                        }

                                        if ((compare_mode == 2 && src_dat != compare) ||
                                            (compare_mode == 3 && src_dat == compare) ||
                                             compare_mode < 2)
                                        {
                                                READ(s3_accel.dest + s3_accel.dx, dest_dat);
                                                                                
//                                pclog("BitBlt : %04i, %04i (%06X) - %02X (%02X %04X %05X) %02X   ", s3_accel.dx, s3_accel.dy, s3_accel.dest + s3_accel.dx, src_dat, vram[s3_accel.src + s3_accel.cx], mix_dat, s3_accel.src + s3_accel.cx, dest_dat);
                                
                                                MIX

//                                pclog("%02X\n", dest_dat);

                                                WRITE(s3_accel.dest + s3_accel.dx);
                                        }
                                }

                                mix_dat <<= 1;
                                mix_dat |= 1;
                                if (s3_bpp == 0) cpu_dat >>= 8;
                                else             cpu_dat >>= 16;

                                if (s3_accel.cmd & 0x20)
                                {
                                        s3_accel.cx++;
                                        s3_accel.dx++;
                                }
                                else
                                {
                                        s3_accel.cx--;
                                        s3_accel.dx--;
                                }
                                s3_accel.sx--;
                                if (s3_accel.sx < 0)
                                {
                                        if (s3_accel.cmd & 0x20)
                                        {
                                                s3_accel.cx -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                                s3_accel.dx -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                        }
                                        else
                                        {
                                                s3_accel.cx += (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                                s3_accel.dx += (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                        }
                                        s3_accel.sx    =  s3_accel.maj_axis_pcnt & 0xfff;

                                        if (s3_accel.cmd & 0x80)
                                        {
                                                s3_accel.cy++;
                                                s3_accel.dy++;
                                        }
                                        else
                                        {
                                                s3_accel.cy--;
                                                s3_accel.dy--;
                                        }

                                        s3_accel.src  = s3_accel.cy * s3_width;
                                        s3_accel.dest = s3_accel.dy * s3_width;

                                        s3_accel.sy--;

                                        if (cpu_input/* && (s3_accel.multifunc[0xa] & 0xc0) == 0x80*/) return;
                                        if (s3_accel.sy < 0)
                                        {
                                                return;
                                        }
                                }
                        }
                }
                break;

                case 7: /*Pattern fill - BitBlt but with source limited to 8x8*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3_accel.sx   = s3_accel.maj_axis_pcnt & 0xfff;
                        s3_accel.sy   = s3_accel.multifunc[0]  & 0xfff;

                        s3_accel.dx   = s3_accel.destx_distp & 0xfff;
                        if (s3_accel.destx_distp & 0x1000) s3_accel.dx |= ~0xfff;
                        s3_accel.dy   = s3_accel.desty_axstp & 0xfff;
                        if (s3_accel.desty_axstp & 0x1000) s3_accel.dy |= ~0xfff;

                        s3_accel.cx   = s3_accel.cur_x & 0xfff;
                        if (s3_accel.cur_x & 0x1000) s3_accel.cx |= ~0xfff;
                        s3_accel.cy   = s3_accel.cur_y & 0xfff;
                        if (s3_accel.cur_y & 0x1000) s3_accel.cy |= ~0xfff;
                        
                        /*Align source with destination*/
//                        s3_accel.cx = (s3_accel.cx & ~7) | (s3_accel.dx & 7);
//                        s3_accel.cy = (s3_accel.cy & ~7) | (s3_accel.dy & 7);

                        s3_accel.pattern  = (s3_accel.cy * s3_width) + s3_accel.cx;
                        s3_accel.dest     = s3_accel.dy * s3_width;
                        
                        s3_accel.cx = s3_accel.dx & 7;
                        s3_accel.cy = s3_accel.dy & 7;
                        
                        s3_accel.src  = s3_accel.pattern + (s3_accel.cy * s3_width);

//                        pclog("Source %08X Dest %08X  (%i, %i) - (%i, %i)\n", s3_accel.src, s3_accel.dest, s3_accel.cx, s3_accel.cy, s3_accel.dx, s3_accel.dy);
//                        dumpregs();
//                        exit(-1);
                }
                if ((s3_accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
//                if ((s3_accel.multifunc[0xa] & 0xc0) == 0x80 && !cpu_input) /*Mix data from CPU*/
//                   return;

                frgd_mix = (s3_accel.frgd_mix >> 5) & 3;
                bkgd_mix = (s3_accel.bkgd_mix >> 5) & 3;

                while (count-- && s3_accel.sy >= 0)
                {
                        if (s3_accel.dx >= clip_l && s3_accel.dx <= clip_r &&
                            s3_accel.dy >= clip_t && s3_accel.dy <= clip_b)
                        {
                                if (vram_mask)
                                {
                                        READ(s3_accel.src + s3_accel.cx, mix_dat)
                                        mix_dat = mix_dat ? mix_mask : 0;
                                }
                                switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                {
                                        case 0: src_dat = s3_accel.bkgd_color;                  break;
                                        case 1: src_dat = s3_accel.frgd_color;                  break;
                                        case 2: src_dat = cpu_dat;                              break;
                                        case 3: READ(s3_accel.src + s3_accel.cx, src_dat);      break;
                                }

                                if ((compare_mode == 2 && src_dat != compare) ||
                                    (compare_mode == 3 && src_dat == compare) ||
                                     compare_mode < 2)
                                {
                                        READ(s3_accel.dest + s3_accel.dx, dest_dat);
                                
//                                pclog("Pattern fill : %04i, %04i (%06X) - %02X (%02X %04X %05X) %02X   ", s3_accel.dx, s3_accel.dy, s3_accel.dest + s3_accel.dx, src_dat, vram[s3_accel.src + s3_accel.cx], mix_dat, s3_accel.src + s3_accel.cx, dest_dat);

                                        MIX

//                                pclog("%02X\n", dest_dat);

                                        WRITE(s3_accel.dest + s3_accel.dx);
                                }
                        }

                        mix_dat <<= 1;
                        mix_dat |= 1;
                        if (s3_bpp == 0) cpu_dat >>= 8;
                        else             cpu_dat >>= 16;

                        if (s3_accel.cmd & 0x20)
                        {
                                s3_accel.cx = ((s3_accel.cx + 1) & 7) | (s3_accel.cx & ~7);
                                s3_accel.dx++;
                        }
                        else
                        {
                                s3_accel.cx = ((s3_accel.cx - 1) & 7) | (s3_accel.cx & ~7);
                                s3_accel.dx--;
                        }
                        s3_accel.sx--;
                        if (s3_accel.sx < 0)
                        {
                                if (s3_accel.cmd & 0x20)
                                {
                                        s3_accel.cx = ((s3_accel.cx - ((s3_accel.maj_axis_pcnt & 0xfff) + 1)) & 7) | (s3_accel.cx & ~7);
                                        s3_accel.dx -= (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                }
                                else
                                {
                                        s3_accel.cx = ((s3_accel.cx + ((s3_accel.maj_axis_pcnt & 0xfff) + 1)) & 7) | (s3_accel.cx & ~7);
                                        s3_accel.dx += (s3_accel.maj_axis_pcnt & 0xfff) + 1;
                                }
                                s3_accel.sx    =  s3_accel.maj_axis_pcnt & 0xfff;

                                if (s3_accel.cmd & 0x80)
                                {
                                        s3_accel.cy = ((s3_accel.cy + 1) & 7) | (s3_accel.cy & ~7);
                                        s3_accel.dy++;
                                }
                                else
                                {
                                        s3_accel.cy = ((s3_accel.cy - 1) & 7) | (s3_accel.cy & ~7);
                                        s3_accel.dy--;
                                }

                                s3_accel.src  = s3_accel.pattern + (s3_accel.cy * s3_width);
                                s3_accel.dest = s3_accel.dy * s3_width;

                                s3_accel.sy--;

                                if (cpu_input/* && (s3_accel.multifunc[0xa] & 0xc0) == 0x80*/) return;
                                if (s3_accel.sy < 0)
                                {
                                        return;
                                }
                        }
                }
                break;
        }
}

void s3_hwcursor_draw(int displine)
{
        int x;
        uint16_t dat[2];
        int xx;
        int offset = svga_hwcursor_latch.x - svga_hwcursor_latch.xoff;
        
        for (x = 0; x < 64; x += 16)
        {
                dat[0] = (vram[svga_hwcursor_latch.addr]     << 8) | vram[svga_hwcursor_latch.addr + 1];
                dat[1] = (vram[svga_hwcursor_latch.addr + 2] << 8) | vram[svga_hwcursor_latch.addr + 3];
                for (xx = 0; xx < 16; xx++)
                {
                        if (offset >= svga_hwcursor_latch.x)
                        {
                                if (!(dat[0] & 0x8000))
                                   ((uint32_t *)buffer32->line[displine])[offset + 32]  = (dat[1] & 0x8000) ? 0xffffff : 0;
                                else if (dat[1] & 0x8000)
                                   ((uint32_t *)buffer32->line[displine])[offset + 32] ^= 0xffffff;
//                                pclog("Plot %i, %i (%i %i) %04X %04X\n", offset, displine, x+xx, svga_hwcursor_on, dat[0], dat[1]);
                        }
                           
                        offset++;
                        dat[0] <<= 1;
                        dat[1] <<= 1;
                }
                svga_hwcursor_latch.addr += 4;
        }
}


uint8_t s3_pci_read(int func, int addr)
{
        pclog("S3 PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x33; /*'S3'*/
                case 0x01: return 0x53;
                
                case 0x02: return s3_id_ext;
                case 0x03: return 0x88;
                
                case 0x04: return 0x03; /*Respond to IO and memory accesses*/

                case 0x07: return 1 << 1; /*Medium DEVSEL timing*/
                
                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x0a: return 0x01; /*Supports VGA interface*/
                case 0x0b: return 0x03;
                
                case 0x10: return 0x00; /*Linear frame buffer address*/
                case 0x11: return 0x00;
                case 0x12: return crtc[0x5a] & 0x80;
                case 0x13: return crtc[0x59];

                case 0x30: return 0x01; /*BIOS ROM address*/
                case 0x31: return 0x00;
                case 0x32: return 0x0C;
                case 0x33: return 0x00;
        }
        return 0;
}

void s3_pci_write(int func, int addr, uint8_t val)
{
        switch (addr)
        {
                case 0x12: crtc[0x5a] = val & 0x80; s3_updatemapping(); break;
                case 0x13: crtc[0x59] = val;        s3_updatemapping(); break;                
        }
}

int s3_init()
{
        if (gfxcard == GFX_BAHAMAS64)
        {
                s3_id = 0xc1; /*Vision864P*/
                s3_id_ext = 0xc1; /*Trio64*/
        }
        if (gfxcard == GFX_N9_9FX)
        {
                s3_id = 0xe1;
                s3_id_ext = 0x11; /*Trio64*/
        }
        if (gfxcard == GFX_STEALTH64)
        {
                s3_id = 0xe1; /*Trio64*/
                s3_id_ext = 0x11;
        }
        svga_recalctimings_ex = s3_recalctimings;
        svga_hwcursor_draw    = s3_hwcursor_draw;

        crtc[0x36] = 1 | (2 << 2) | (1 << 4) | (4 << 5);
        crtc[0x37] = 1 | (7 << 5);
        
        vrammask = 0x3fffff;

        io_sethandler(0x42e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x46e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x4ae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x82e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x86e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x8ae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x8ee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x92e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x96e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x9ae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0x9ee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xa2e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xa6e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xaae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xaee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xb2e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xb6e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xbae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xbee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL);
        io_sethandler(0xe2e8, 0x0004, s3_accel_in, NULL, NULL, s3_accel_out, s3_accel_out_w, s3_accel_out_l);

        pci_add(s3_pci_read, s3_pci_write);
 
        svga_vram_limit = 4 << 20; /*4mb - 864 supports 8mb but buggy VESA driver reports 0mb*/
        return svga_init();
}

GFXCARD vid_s3 =
{
        s3_init,
        /*IO at 3Cx/3Dx*/
        s3_out,
        s3_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        svga_poll,
        svga_recalctimings,

        svga_write,
        video_write_null,
        video_write_null,

        svga_read,
        video_read_null,
        video_read_null
};

