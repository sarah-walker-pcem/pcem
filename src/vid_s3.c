/*S3 emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "video.h"
#include "vid_s3.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_sdac_ramdac.h"

typedef struct s3_t
{
        mem_mapping_t linear_mapping;
        mem_mapping_t mmio_mapping;
        
        svga_t svga;
        sdac_ramdac_t ramdac;

        uint8_t bank;
        uint8_t ma_ext;
        int width;
        int bpp;

        uint8_t id, id_ext;

        uint32_t linear_base, linear_size;

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
        } accel;
} s3_t;

void s3_updatemapping();

void s3_accel_write(uint32_t addr, uint8_t val, void *p);
void s3_accel_write_w(uint32_t addr, uint16_t val, void *p);
void s3_accel_write_l(uint32_t addr, uint32_t val, void *p);
uint8_t s3_accel_read(uint32_t addr, void *p);


void s3_out(uint16_t addr, uint8_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
        svga_t *svga = &s3->svga;
        uint8_t old;

        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;
        
//        pclog("S3 out %04X %02X\n", addr, val);

        switch (addr)
        {
                case 0x3c5:
                if (svga->seqaddr == 4) /*Chain-4 - update banking*/
                {
                        if (val & 8) svga->write_bank = svga->read_bank = s3->bank << 16;
                        else         svga->write_bank = svga->read_bank = s3->bank << 14;
                }
                break;
                
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Write RAMDAC %04X %02X %04X:%04X\n", addr, val, CS, pc);
                sdac_ramdac_out(addr, val, &s3->ramdac, svga);
                return;

                case 0x3D4:
                svga->crtcreg = val & 0x7f;
                return;
                case 0x3D5:
//                        if (crtcreg == 0x67) pclog("Write CRTC R%02X %02X\n", crtcreg, val);
                if (svga->crtcreg <= 7 && svga->crtc[0x11] & 0x80) return;
                if (svga->crtcreg >= 0x20 && svga->crtcreg != 0x38 && (svga->crtc[0x38] & 0xcc) != 0x48) return;
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                switch (svga->crtcreg)
                {
                        case 0x31:
                        s3->ma_ext = (s3->ma_ext & 0x1c) | ((val & 0x30) >> 4);
                        svga->vrammask = /*(val & 8) ? */0x3fffff/* : 0x3ffff*/;
                        break;
                        
                        case 0x50:
                        switch (svga->crtc[0x50] & 0xc1)
                        {
                                case 0x00: s3->width = (svga->crtc[0x31] & 2) ? 2048 : 1024; break;
                                case 0x01: s3->width = 1152; break;
                                case 0x40: s3->width = 640;  break;
                                case 0x80: s3->width = 800;  break;
                                case 0x81: s3->width = 1600; break;
                                case 0xc0: s3->width = 1280; break;
                        }
                        s3->bpp = (svga->crtc[0x50] >> 4) & 3;
                        break;
                        case 0x69:
                        s3->ma_ext = val & 0x1f;
                        break;
                        
                        case 0x35:
                        s3->bank = (s3->bank & 0x70) | (val & 0xf);
//                        pclog("CRTC write R35 %02X\n", val);
                        if (svga->chain4) svga->write_bank = svga->read_bank = s3->bank << 16;
                        else              svga->write_bank = svga->read_bank = s3->bank << 14;
                        break;
                        case 0x51:
                        s3->bank = (s3->bank & 0x4f) | ((val & 0xc) << 2);
//                        pclog("CRTC write R51 %02X\n", val);
                        if (svga->chain4) svga->write_bank = svga->read_bank = s3->bank << 16;
                        else              svga->write_bank = svga->read_bank = s3->bank << 14;
                        s3->ma_ext = (s3->ma_ext & ~0xc) | ((val & 3) << 2);
                        break;
                        case 0x6a:
                        s3->bank = val;
//                        pclog("CRTC write R6a %02X\n", val);
                        if (svga->chain4) svga->write_bank = svga->read_bank = s3->bank << 16;
                        else              svga->write_bank = svga->read_bank = s3->bank << 14;
                        break;
                        
                        case 0x3a:
                        if (val & 0x10) 
                                svga->gdcreg[5] |= 0x40; /*Horrible cheat*/
                        break;
                        
                        case 0x45:
                        svga->hwcursor.ena = val & 1;
                        break;
                        case 0x48:
                        svga->hwcursor.x = ((svga->crtc[0x46] << 8) | svga->crtc[0x47]) & 0x7ff;
                        if (svga->bpp == 32) svga->hwcursor.x >>= 1;
                        svga->hwcursor.y = ((svga->crtc[0x48] << 8) | svga->crtc[0x49]) & 0x7ff;
                        svga->hwcursor.xoff = svga->crtc[0x4e] & 63;
                        svga->hwcursor.yoff = svga->crtc[0x4f] & 63;
                        svga->hwcursor.addr = ((((svga->crtc[0x4c] << 8) | svga->crtc[0x4d]) & 0xfff) * 1024) + (svga->hwcursor.yoff * 16);
                        if (gfxcard == GFX_N9_9FX && svga->bpp == 32) /*Trio64*/
                                svga->hwcursor.x <<= 1;
                        break;

                        case 0x53:
                        case 0x58: case 0x59: case 0x5a:
                        s3_updatemapping(s3);
                        break;
                        
                        case 0x67:
                        if (gfxcard == GFX_N9_9FX) /*Trio64*/
                        {
                                switch (val >> 4)
                                {
                                        case 3:  svga->bpp = 15; break;
                                        case 5:  svga->bpp = 16; break;
                                        case 7:  svga->bpp = 24; break;
                                        case 13: svga->bpp = 32; break;
                                        default: svga->bpp = 8;  break;
                                }
                        }
                        break;
                        //case 0x55: case 0x43:
//                                pclog("Write CRTC R%02X %02X\n", crtcreg, val);
                }
                if (old != val)
                {
                        if (svga->crtcreg < 0xe || svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;
        }
        svga_out(addr, val, svga);
}

uint8_t s3_in(uint16_t addr, void *p)
{
        s3_t *s3 = (s3_t *)p;
        svga_t *svga = &s3->svga;

        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

//        if (addr != 0x3da) pclog("S3 in %04X\n", addr);
        switch (addr)
        {
                case 0x3c6: case 0x3c7: case 0x3c8: case 0x3c9:
//                pclog("Read RAMDAC %04X  %04X:%04X\n", addr, CS, pc);
                return sdac_ramdac_in(addr, &s3->ramdac, svga);

                case 0x3d4:
                return svga->crtcreg;
                case 0x3d5:
//                pclog("Read CRTC R%02X %04X:%04X\n", crtcreg, CS, pc);
                switch (svga->crtcreg)
                {
                        case 0x2d: return 0x88;       /*Extended chip ID*/
                        case 0x2e: return s3->id_ext; /*New chip ID*/
                        case 0x30: return s3->id;     /*Chip ID*/
                        case 0x31: return (svga->crtc[0x31] & 0xcf) | ((s3->ma_ext & 3) << 4);
                        case 0x35: return (svga->crtc[0x35] & 0xf0) | (s3->bank & 0xf);
                        case 0x51: return (svga->crtc[0x51] & 0xf0) | ((s3->bank >> 2) & 0xc) | ((s3->ma_ext >> 2) & 3);
                        case 0x69: return s3->ma_ext;
                        case 0x6a: return s3->bank;
                }
                return svga->crtc[svga->crtcreg];
        }
        return svga_in(addr, svga);
}

void s3_recalctimings(svga_t *svga)
{
        s3_t *s3 = (s3_t *)svga->p;
        svga->hdisp = svga->hdisp_old;

//        pclog("%i %i\n", svga->hdisp, svga->hdisp_time);
//        pclog("recalctimings\n");
        svga->ma_latch |= (s3->ma_ext << 16);
//        pclog("SVGA_MA %08X\n", svga_ma);
        if (svga->crtc[0x5d] & 0x01) svga->htotal     += 0x100;
        if (svga->crtc[0x5d] & 0x02) 
        {
                svga->hdisp_time += 0x100;
                svga->hdisp += 0x100 * ((svga->seqregs[1] & 8) ? 16 : 8);
        }
        if (svga->crtc[0x5e] & 0x01) svga->vtotal     += 0x400;
        if (svga->crtc[0x5e] & 0x02) svga->dispend    += 0x400;
        if (svga->crtc[0x5e] & 0x10) svga->vsyncstart += 0x400;
        if (svga->crtc[0x5e] & 0x40) svga->split      += 0x400;
        if (svga->crtc[0x51] & 0x30)      svga->rowoffset  += (svga->crtc[0x51] & 0x30) << 4;
        else if (svga->crtc[0x43] & 0x04) svga->rowoffset  += 0x100;
        if (!svga->rowoffset) svga->rowoffset = 256;
        svga->interlace = svga->crtc[0x42] & 0x20;
        svga->clock = cpuclock / sdac_getclock((svga->miscout >> 2) & 3, &s3->ramdac);
//        pclog("SVGA_CLOCK = %f  %02X  %f\n", svga_clock, svga_miscout, cpuclock);
        if (svga->bpp > 8) svga->clock /= 2;

        svga->lowres = !((svga->gdcreg[5] & 0x40) && (svga->crtc[0x3a] & 0x10));
        if ((svga->gdcreg[5] & 0x40) && (svga->crtc[0x3a] & 0x10))
        {
                switch (svga->bpp)
                {
                        case 8: 
                        svga->render = svga_render_8bpp_highres; 
                        break;
                        case 15: 
                        svga->render = svga_render_15bpp_highres; 
                        break;
                        case 16: 
                        svga->render = svga_render_16bpp_highres; 
                        break;
                        case 24: 
                        svga->render = svga_render_24bpp_highres; 
                        break;
                        case 32:
                        svga->render = svga_render_32bpp_highres; 
                        if (gfxcard == GFX_N9_9FX) /*Trio64*/
                                svga->hdisp *= 4;
                        break;
                }
        }

       // if (svga->bpp == 32) svga->hdisp *= 3;
//        pclog("svga->hdisp %i %02X %i\n", svga->hdisp, svga->crtc[0x5d], svga->hdisp_time);
}

void s3_updatemapping(s3_t *s3)
{
        svga_t *svga = &s3->svga;
        
//        video_write_a000_w = video_write_a000_l = NULL;


//        pclog("Update mapping - bank %02X ", svga->gdcreg[6] & 0xc);
        switch (svga->gdcreg[6] & 0xc) /*Banked framebuffer*/
        {
                case 0x0: /*128k at A0000*/
                mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x20000);
                break;
                case 0x4: /*64k at A0000*/
                mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
                break;
                case 0x8: /*32k at B0000*/
                mem_mapping_set_addr(&svga->mapping, 0xb0000, 0x08000);
                break;
                case 0xC: /*32k at B8000*/
                mem_mapping_set_addr(&svga->mapping, 0xb8000, 0x08000);
                break;
        }
        
//        pclog("Linear framebuffer %02X ", svga->crtc[0x58] & 0x10);
        if (svga->crtc[0x58] & 0x10) /*Linear framebuffer*/
        {
                mem_mapping_disable(&svga->mapping);
                
                s3->linear_base = (svga->crtc[0x5a] << 16) | (svga->crtc[0x59] << 24);
                switch (svga->crtc[0x58] & 3)
                {
                        case 0: /*64k*/
                        s3->linear_size = 0x10000;
                        break;
                        case 1: /*1mb*/
                        s3->linear_size = 0x100000;
                        break;
                        case 2: /*2mb*/
                        s3->linear_size = 0x200000;
                        break;
                        case 3: /*8mb*/
                        s3->linear_size = 0x800000;
                        break;
                }
                s3->linear_base &= ~(s3->linear_size - 1);
//                pclog("%08X %08X  %02X %02X %02X\n", linear_base, linear_size, crtc[0x58], crtc[0x59], crtc[0x5a]);
//                pclog("Linear framebuffer at %08X size %08X\n", s3->linear_base, s3->linear_size);
                if (s3->linear_base == 0xa0000)
                {
                        mem_mapping_disable(&s3->linear_mapping);
                        mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
//                        mem_mapping_set_addr(&s3->linear_mapping, 0xa0000, 0x10000);
                }
                else
                        mem_mapping_set_addr(&s3->linear_mapping, s3->linear_base, s3->linear_size);
        }
        else
                mem_mapping_disable(&s3->linear_mapping);
        
//        pclog("Memory mapped IO %02X\n", svga->crtc[0x53] & 0x10);
        if (svga->crtc[0x53] & 0x10) /*Memory mapped IO*/
                mem_mapping_enable(&s3->mmio_mapping);
        else
                mem_mapping_disable(&s3->mmio_mapping);
}


void s3_accel_start(int count, int cpu_input, uint32_t mix_dat, uint32_t cpu_dat, s3_t *s3);

void s3_accel_out(uint16_t port, uint8_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
//        pclog("Accel out %04X %02X\n", port, val);
        switch (port)
        {
                case 0x42e8:
                break;
                case 0x42e9:
                s3->accel.subsys_cntl = val;
                break;
                case 0x46e8:
                s3->accel.setup_md = val;
                break;
                case 0x4ae8:
                s3->accel.advfunc_cntl = val;
                break;
                
                case 0x82e8:
                s3->accel.cur_y = (s3->accel.cur_y & 0xf00) | val;
                break;
                case 0x82e9:
                s3->accel.cur_y = (s3->accel.cur_y & 0xff) | ((val & 0x1f) << 8);
                break;
                
                case 0x86e8:
                s3->accel.cur_x = (s3->accel.cur_x & 0xf00) | val;
                break;
                case 0x86e9:
                s3->accel.cur_x = (s3->accel.cur_x & 0xff) | ((val & 0x1f) << 8);
                break;
                
                case 0x8ae8:
                s3->accel.desty_axstp = (s3->accel.desty_axstp & 0x3f00) | val;
                break;
                case 0x8ae9:
                s3->accel.desty_axstp = (s3->accel.desty_axstp & 0xff) | ((val & 0x3f) << 8);
                if (val & 0x20)
                   s3->accel.desty_axstp |= ~0x3fff;
                break;
                
                case 0x8ee8:
                s3->accel.destx_distp = (s3->accel.destx_distp & 0x3f00) | val;
                break;
                case 0x8ee9:
                s3->accel.destx_distp = (s3->accel.destx_distp & 0xff) | ((val & 0x3f) << 8);
                if (val & 0x20)
                   s3->accel.destx_distp |= ~0x3fff;
                break;
                
                case 0x92e8:
                s3->accel.err_term = (s3->accel.err_term & 0x3f00) | val;
                break;
                case 0x92e9:
                s3->accel.err_term = (s3->accel.err_term & 0xff) | ((val & 0x3f) << 8);
                if (val & 0x20)
                   s3->accel.err_term |= ~0x3fff;
                break;

                case 0x96e8:
                s3->accel.maj_axis_pcnt = (s3->accel.maj_axis_pcnt & 0x3f00) | val;
                break;
                case 0x96e9:
                s3->accel.maj_axis_pcnt = (s3->accel.maj_axis_pcnt & 0xff) | ((val & 0x0f) << 8);
                if (val & 0x08)
                   s3->accel.maj_axis_pcnt |= ~0x0fff;
                break;

                case 0x9ae8:
                s3->accel.cmd = (s3->accel.cmd & 0xff00) | val;
                break;
                case 0x9ae9:
                s3->accel.cmd = (s3->accel.cmd & 0xff) | (val << 8);
                s3_accel_start(-1, 0, 0xffffffff, 0, s3);
                s3->accel.pix_trans_count = 0;
                break;

                case 0x9ee8:
                s3->accel.short_stroke = (s3->accel.short_stroke & 0xff00) | val;
                break;
                case 0x9ee9:
                s3->accel.short_stroke = (s3->accel.short_stroke & 0xff) | (val << 8);
                break;

                case 0xa2e8:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.bkgd_color = (s3->accel.bkgd_color & ~0x00ff0000) | (val << 16);
                else
                   s3->accel.bkgd_color = (s3->accel.bkgd_color & ~0x000000ff) | val;
                break;
                case 0xa2e9:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.bkgd_color = (s3->accel.bkgd_color & ~0xff000000) | (val << 24);
                else
                   s3->accel.bkgd_color = (s3->accel.bkgd_color & ~0x0000ff00) | (val << 8);
                s3->accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xa6e8:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.frgd_color = (s3->accel.frgd_color & ~0x00ff0000) | (val << 16);
                else
                   s3->accel.frgd_color = (s3->accel.frgd_color & ~0x000000ff) | val;
//                pclog("Foreground colour now %08X %i\n", s3->accel.frgd_color, s3->bpp);
                break;
                case 0xa6e9:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.frgd_color = (s3->accel.frgd_color & ~0xff000000) | (val << 24);
                else
                   s3->accel.frgd_color = (s3->accel.frgd_color & ~0x0000ff00) | (val << 8);
                s3->accel.multifunc[0xe] ^= 0x10;
//                pclog("Foreground colour now %08X\n", s3->accel.frgd_color);
                break;

                case 0xaae8:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.wrt_mask = (s3->accel.wrt_mask & ~0x00ff0000) | (val << 16);
                else
                   s3->accel.wrt_mask = (s3->accel.wrt_mask & ~0x000000ff) | val;
                break;
                case 0xaae9:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.wrt_mask = (s3->accel.wrt_mask & ~0xff000000) | (val << 24);
                else
                   s3->accel.wrt_mask = (s3->accel.wrt_mask & ~0x0000ff00) | (val << 8);
                s3->accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xaee8:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.rd_mask = (s3->accel.rd_mask & ~0x00ff0000) | (val << 16);
                else
                   s3->accel.rd_mask = (s3->accel.rd_mask & ~0x000000ff) | val;
                break;
                case 0xaee9:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.rd_mask = (s3->accel.rd_mask & ~0xff000000) | (val << 24);
                else
                   s3->accel.rd_mask = (s3->accel.rd_mask & ~0x0000ff00) | (val << 8);
                s3->accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xb2e8:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.color_cmp = (s3->accel.color_cmp & ~0x00ff0000) | (val << 16);
                else
                   s3->accel.color_cmp = (s3->accel.color_cmp & ~0x000000ff) | val;
                break;
                case 0xb2e9:
                if (s3->bpp == 3 && s3->accel.multifunc[0xe] & 0x10)
                   s3->accel.color_cmp = (s3->accel.color_cmp & ~0xff000000) | (val << 24);
                else
                   s3->accel.color_cmp = (s3->accel.color_cmp & ~0x0000ff00) | (val << 8);
                s3->accel.multifunc[0xe] ^= 0x10;
                break;

                case 0xb6e8:
                s3->accel.bkgd_mix = val;
                break;

                case 0xbae8:
                s3->accel.frgd_mix = val;
                break;
                
                case 0xbee8:
                s3->accel.multifunc_cntl = (s3->accel.multifunc_cntl & 0xff00) | val;
                break;
                case 0xbee9:
                s3->accel.multifunc_cntl = (s3->accel.multifunc_cntl & 0xff) | (val << 8);
                s3->accel.multifunc[s3->accel.multifunc_cntl >> 12] = s3->accel.multifunc_cntl & 0xfff;
                break;

                case 0xe2e8:
                s3->accel.pix_trans[0] = val;
                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80 && !(s3->accel.cmd & 0x600) && (s3->accel.cmd & 0x100))
                        s3_accel_start(8, 1, s3->accel.pix_trans[0], 0, s3);
                else if (!(s3->accel.cmd & 0x600) && (s3->accel.cmd & 0x100))
                        s3_accel_start(1, 1, 0xffffffff, s3->accel.pix_trans[0], s3);
                break;
                case 0xe2e9:
                s3->accel.pix_trans[1] = val;
                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80 && (s3->accel.cmd & 0x600) == 0x200 && (s3->accel.cmd & 0x100))
                {
                        if (s3->accel.cmd & 0x1000) s3_accel_start(16, 1, s3->accel.pix_trans[1] | (s3->accel.pix_trans[0] << 8), 0, s3);
                        else                        s3_accel_start(16, 1, s3->accel.pix_trans[0] | (s3->accel.pix_trans[1] << 8), 0, s3);
                }
                else if ((s3->accel.cmd & 0x600) == 0x200 && (s3->accel.cmd & 0x100))
                {
                        if (s3->accel.cmd & 0x1000) s3_accel_start(2, 1, 0xffffffff, s3->accel.pix_trans[1] | (s3->accel.pix_trans[0] << 8), s3);
                        else                        s3_accel_start(2, 1, 0xffffffff, s3->accel.pix_trans[0] | (s3->accel.pix_trans[1] << 8), s3);
                }
                break;
                case 0xe2ea:
                s3->accel.pix_trans[2] = val;
                break;
                case 0xe2eb:
                s3->accel.pix_trans[3] = val;
                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80 && (s3->accel.cmd & 0x600) == 0x400 && (s3->accel.cmd & 0x100))
                        s3_accel_start(32, 1, s3->accel.pix_trans[0] | (s3->accel.pix_trans[1] << 8) | (s3->accel.pix_trans[2] << 16) | (s3->accel.pix_trans[3] << 24), 0, s3);
                else if ((s3->accel.cmd & 0x600) == 0x400 && (s3->accel.cmd & 0x100))
                        s3_accel_start(4, 1, 0xffffffff, s3->accel.pix_trans[0] | (s3->accel.pix_trans[1] << 8) | (s3->accel.pix_trans[2] << 16) | (s3->accel.pix_trans[3] << 24), s3);
                break;
        }
}

void s3_accel_out_w(uint16_t port, uint16_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
//        pclog("Accel out w %04X %04X\n", port, val);
        if (s3->accel.cmd & 0x100)
        {
                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80)
                {
                        if (s3->accel.cmd & 0x1000)
                           val = (val >> 8) | (val << 8);
                        s3_accel_start(16, 1, val | (val << 16), 0, s3);
                }
                else
                   s3_accel_start(2, 1, 0xffffffff, val | (val << 16), s3);
        }
}

void s3_accel_out_l(uint16_t port, uint32_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
//        pclog("Accel out l %04X %08X\n", port, val);
        if (s3->accel.cmd & 0x100)
        {
                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80)
                {
                        if (s3->accel.cmd & 0x1000)
                                val = ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
                        if ((s3->accel.cmd & 0x600) == 0x400)
                                s3_accel_start(32, 1, val, 0, s3);
                        else if ((s3->accel.cmd & 0x600) == 0x200)
                        {
                                s3_accel_start(16, 1, val >> 16, 0, s3);
                                s3_accel_start(16, 1, val, 0, s3);
                        }
                        else if (!(s3->accel.cmd & 0x600))
                        {
                                s3_accel_start(8, 1, val >> 24, 0, s3);
                                s3_accel_start(8, 1, val >> 16, 0, s3);
                                s3_accel_start(8, 1, val >> 8,  0, s3);
                                s3_accel_start(8, 1, val,       0, s3);
                        }
                }
                else
                {
                        if ((s3->accel.cmd & 0x600) == 0x400)
                                s3_accel_start(4, 1, 0xffffffff, val, s3);
                        else if ((s3->accel.cmd & 0x600) == 0x200)
                        {
                                s3_accel_start(2, 1, 0xffffffff, val >> 16, s3);
                                s3_accel_start(2, 1, 0xffffffff, val, s3);
                        }
                        else if (!(s3->accel.cmd & 0x600))
                        {
                                s3_accel_start(1, 1, 0xffffffff, val >> 24, s3);
                                s3_accel_start(1, 1, 0xffffffff, val >> 16, s3);
                                s3_accel_start(1, 1, 0xffffffff, val >> 8, s3);
                                s3_accel_start(1, 1, 0xffffffff, val, s3);
                        }
                }
        }
}

uint8_t s3_accel_in(uint16_t port, void *p)
{
        s3_t *s3 = (s3_t *)p;
        int temp;
//        pclog("Accel in  %04X\n", port);
        switch (port)
        {
                case 0x42e8:
                return 0;
                case 0x42e9:
                return 0;

                case 0x82e8:
                return s3->accel.cur_y & 0xff;
                case 0x82e9:
                return s3->accel.cur_y  >> 8;

                case 0x86e8:
                return s3->accel.cur_x & 0xff;
                case 0x86e9:
                return s3->accel.cur_x  >> 8;

                case 0x8ae8:
                return s3->accel.desty_axstp & 0xff;
                case 0x8ae9:
                return s3->accel.desty_axstp >> 8;

                case 0x8ee8:
                return s3->accel.destx_distp & 0xff;
                case 0x8ee9:
                return s3->accel.destx_distp >> 8;

                case 0x92e8:
                return s3->accel.err_term & 0xff;
                case 0x92e9:
                return s3->accel.err_term >> 8;

                case 0x96e8:
                return s3->accel.maj_axis_pcnt & 0xff;
                case 0x96e9:
                return s3->accel.maj_axis_pcnt >> 8;

                case 0x9ae8:
                return 0;
                case 0x9ae9:
                return 0;

                case 0xa2e8:
                return s3->accel.bkgd_color & 0xff;
                case 0xa2e9:
                return s3->accel.bkgd_color >> 8;

                case 0xa6e8:
                return s3->accel.frgd_color & 0xff;
                case 0xa6e9:
                return s3->accel.frgd_color >> 8;

                case 0xaae8:
                return s3->accel.wrt_mask & 0xff;
                case 0xaae9:
                return s3->accel.wrt_mask >> 8;

                case 0xaee8:
                return s3->accel.rd_mask & 0xff;
                case 0xaee9:
                return s3->accel.rd_mask >> 8;

                case 0xb2e8:
                return s3->accel.color_cmp & 0xff;
                case 0xb2e9:
                return s3->accel.color_cmp >> 8;

                case 0xb6e8:
                return s3->accel.bkgd_mix;

                case 0xbae8:
                return s3->accel.frgd_mix;

                case 0xbee8:
                temp = s3->accel.multifunc[0xf] & 0xf;
                switch (temp)
                {
                        case 0x0: return s3->accel.multifunc[0x0] & 0xff;
                        case 0x1: return s3->accel.multifunc[0x1] & 0xff;
                        case 0x2: return s3->accel.multifunc[0x2] & 0xff;
                        case 0x3: return s3->accel.multifunc[0x3] & 0xff;
                        case 0x4: return s3->accel.multifunc[0x4] & 0xff;
                        case 0x5: return s3->accel.multifunc[0xa] & 0xff;
                        case 0x6: return s3->accel.multifunc[0xe] & 0xff;
                        case 0x7: return s3->accel.cmd            & 0xff;
                        case 0x8: return s3->accel.subsys_cntl    & 0xff;
                        case 0x9: return s3->accel.setup_md       & 0xff;
                        case 0xa: return s3->accel.multifunc[0xd] & 0xff;
                }
                return 0xff;
                case 0xbee9:
                temp = s3->accel.multifunc[0xf] & 0xf;
                s3->accel.multifunc[0xf]++;
                switch (temp)
                {
                        case 0x0: return  s3->accel.multifunc[0x0] >> 8;
                        case 0x1: return  s3->accel.multifunc[0x1] >> 8;
                        case 0x2: return  s3->accel.multifunc[0x2] >> 8;
                        case 0x3: return  s3->accel.multifunc[0x3] >> 8;
                        case 0x4: return  s3->accel.multifunc[0x4] >> 8;
                        case 0x5: return  s3->accel.multifunc[0xa] >> 8;
                        case 0x6: return  s3->accel.multifunc[0xe] >> 8;
                        case 0x7: return  s3->accel.cmd            >> 8;
                        case 0x8: return (s3->accel.subsys_cntl    >> 8) & ~0xe000;
                        case 0x9: return (s3->accel.setup_md       >> 8) & ~0xf000;
                        case 0xa: return  s3->accel.multifunc[0xd] >> 8;
                }
                return 0xff;

                case 0xe2e8: case 0xe2e9: case 0xe2ea: case 0xe2eb: /*PIX_TRANS*/
                break;
        }
        return 0;
}

void s3_accel_write(uint32_t addr, uint8_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
//        pclog("Write S3 accel %08X %02X\n", addr, val);
        if (addr & 0x8000)
           s3_accel_out(addr & 0xffff, val, p);
        else
        {
                if (s3->accel.cmd & 0x100)
                {
                        if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80)
                                s3_accel_start(8, 1, val | (val << 8) | (val << 16) | (val << 24), 0, s3);
                        else
                                s3_accel_start(1, 1, 0xffffffff, val | (val << 8) | (val << 16) | (val << 24), s3);
                }
        }
}

void s3_accel_write_w(uint32_t addr, uint16_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
//        pclog("Write S3 accel w %08X %04X\n", addr, val);
        if (addr & 0x8000)
        {
                s3_accel_out( addr & 0xffff,      val, p);
                s3_accel_out((addr & 0xffff) + 1, val >> 8, p);
        }
        else
        {
                if (s3->accel.cmd & 0x100)
                {
                        if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80)
                        {
                                if (s3->accel.cmd & 0x1000)
                                        val = (val >> 8) | (val << 8);
                                s3_accel_start(16, 1, val | (val << 16), 0, s3);
                        }
                        else
                           s3_accel_start(2, 1, 0xffffffff, val | (val << 16), s3);
                }
        }
}

void s3_accel_write_l(uint32_t addr, uint32_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
//        pclog("Write S3 accel l %08X %08X\n", addr, val);
        if (addr & 0x8000)
        {
                s3_accel_out( addr & 0xffff,      val,       p);
                s3_accel_out((addr & 0xffff) + 1, val >> 8,  p);
                s3_accel_out((addr & 0xffff) + 2, val >> 16, p);
                s3_accel_out((addr & 0xffff) + 3, val >> 24, p);
        }
        else
        {
                if (s3->accel.cmd & 0x100)
                {
                        if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80)
                        {
                                if (s3->accel.cmd & 0x1000)
                                        val = ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24);
                                if ((s3->accel.cmd & 0x600) == 0x400)
                                        s3_accel_start(32, 1, val, 0, s3);
                                else if ((s3->accel.cmd & 0x600) == 0x200)
                                {
                                        s3_accel_start(16, 1, val >> 16, 0, s3);
                                        s3_accel_start(16, 1, val, 0,       s3);
                                }
                                else if (!(s3->accel.cmd & 0x600))
                                {
                                        s3_accel_start(8, 1, val >> 24, 0, s3);
                                        s3_accel_start(8, 1, val >> 16, 0, s3);
                                        s3_accel_start(8, 1, val >> 8,  0, s3);
                                        s3_accel_start(8, 1, val,       0, s3);
                                }
                        }
                        else
                        {
                                if ((s3->accel.cmd & 0x600) == 0x400)
                                        s3_accel_start(4, 1, 0xffffffff, val, s3);
                                else if ((s3->accel.cmd & 0x600) == 0x200)
                                {
                                        s3_accel_start(2, 1, 0xffffffff, val >> 16, s3);
                                        s3_accel_start(2, 1, 0xffffffff, val,       s3);
                                }
                                else if (!(s3->accel.cmd & 0x600))
                                {
                                        s3_accel_start(1, 1, 0xffffffff, val >> 24, s3);
                                        s3_accel_start(1, 1, 0xffffffff, val >> 16, s3);
                                        s3_accel_start(1, 1, 0xffffffff, val >> 8,  s3);
                                        s3_accel_start(1, 1, 0xffffffff, val,       s3);
                                }
                        }
                }
        }
}

uint8_t s3_accel_read(uint32_t addr, void *p)
{
        if (addr & 0x8000)
           return s3_accel_in(addr & 0xffff, p);
        return 0;
}

#define READ(addr, dat) if (s3->bpp == 0)      dat = svga->vram[  (addr) & 0x3fffff]; \
                        else if (s3->bpp == 1) dat = vram_w[(addr) & 0x1fffff]; \
                        else                   dat = vram_l[(addr) & 0x0fffff];

#define MIX     switch ((mix_dat & mix_mask) ? (s3->accel.frgd_mix & 0xf) : (s3->accel.bkgd_mix & 0xf))   \
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


#define WRITE(addr)     if (s3->bpp == 0)                                                                                \
                        {                                                                                               \
                                svga->vram[(addr) & 0x3fffff] = dest_dat;                              \
                                svga->changedvram[((addr) & 0x3fffff) >> 10] = changeframecount;       \
                        }                                                                                               \
                        else if (s3->bpp == 1)                                                                           \
                        {                                                                                               \
                                vram_w[(addr) & 0x1fffff] = dest_dat;                            \
                                svga->changedvram[((addr) & 0x1fffff) >> 9] = changeframecount;        \
                        }                                                                                               \
                        else                                                                                            \
                        {                                                                                               \
                                vram_l[(addr) & 0xfffff] = dest_dat;                             \
                                svga->changedvram[((addr) & 0xfffff) >> 8] = changeframecount;         \
                        }

void s3_accel_start(int count, int cpu_input, uint32_t mix_dat, uint32_t cpu_dat, s3_t *s3)
{
        svga_t *svga = &s3->svga;
        uint32_t src_dat, dest_dat;
        int frgd_mix, bkgd_mix;
        int clip_t = s3->accel.multifunc[1] & 0xfff;
        int clip_l = s3->accel.multifunc[2] & 0xfff;
        int clip_b = s3->accel.multifunc[3] & 0xfff;
        int clip_r = s3->accel.multifunc[4] & 0xfff;
        int vram_mask = (s3->accel.multifunc[0xa] & 0xc0) == 0xc0;
        uint32_t mix_mask;
        uint16_t *vram_w = (uint16_t *)svga->vram;
        uint32_t *vram_l = (uint32_t *)svga->vram;
        uint32_t compare = s3->accel.color_cmp;
        int compare_mode = (s3->accel.multifunc[0xe] >> 7) & 3;
//return;
//        if (!cpu_input) pclog("Start S3 command %i  %i, %i  %i, %i (clip %i, %i to %i, %i  %i)\n", s3->accel.cmd >> 13, s3->accel.cur_x, s3->accel.cur_y, s3->accel.maj_axis_pcnt & 0xfff, s3->accel.multifunc[0]  & 0xfff, clip_l, clip_t, clip_r, clip_b, s3->accel.multifunc[0xe] & 0x20);
//        else            pclog("      S3 command %i, %i, %08x %08x\n", s3->accel.cmd >> 13, count, mix_dat, cpu_dat);

        if (!cpu_input) s3->accel.dat_count = 0;
        if (cpu_input && (s3->accel.multifunc[0xa] & 0xc0) != 0x80)
        {
                if (s3->bpp == 3 && count == 2)
                {
                        if (s3->accel.dat_count)
                        {
                                cpu_dat = (cpu_dat & 0xffff) | (s3->accel.dat_buf << 16);
                                count = 4;
                                s3->accel.dat_count = 0;
                        }
                        else
                        {
                                s3->accel.dat_buf = cpu_dat & 0xffff;
                                s3->accel.dat_count = 1;
                        }
                }
                if (s3->bpp == 1) count >>= 1;
                if (s3->bpp == 3) count >>= 2;
        }
        
        switch (s3->accel.cmd & 0x600)
        {
                case 0x000: mix_mask = 0x80; break;
                case 0x200: mix_mask = 0x8000; break;
                case 0x400: mix_mask = 0x80000000; break;
                case 0x600: mix_mask = 0x80000000; break;
        }

        if (s3->bpp == 0) compare &=   0xff;
        if (s3->bpp == 1) compare &= 0xffff;
        switch (s3->accel.cmd >> 13)
        {
                case 1: /*Draw line*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3->accel.cx   = s3->accel.cur_x;
                        if (s3->accel.cur_x & 0x1000) s3->accel.cx |= ~0xfff;
                        s3->accel.cy   = s3->accel.cur_y;
                        if (s3->accel.cur_y & 0x1000) s3->accel.cy |= ~0xfff;
                        
                        s3->accel.sy = s3->accel.maj_axis_pcnt;
                }
                if ((s3->accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
                if (s3->accel.cmd & 8) /*Radial*/
                {
                        while (count-- && s3->accel.sy >= 0)
                        {
                                if (s3->accel.cx >= clip_l && s3->accel.cx <= clip_r &&
                                    s3->accel.cy >= clip_t && s3->accel.cy <= clip_b)
                                {
                                        switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                        {
                                                case 0: src_dat = s3->accel.bkgd_color; break;
                                                case 1: src_dat = s3->accel.frgd_color; break;
                                                case 2: src_dat = cpu_dat; break;
                                                case 3: src_dat = 0; break;
                                        }

                                        if ((compare_mode == 2 && src_dat != compare) ||
                                            (compare_mode == 3 && src_dat == compare) ||
                                             compare_mode < 2)
                                        {
                                                READ((s3->accel.cy * s3->width) + s3->accel.cx, dest_dat);
                                        
                                                MIX

                                                WRITE((s3->accel.cy * s3->width) + s3->accel.cx);
                                        }
                                }

                                mix_dat <<= 1;
                                mix_dat |= 1;
                                if (s3->bpp == 0) cpu_dat >>= 8;
                                else             cpu_dat >>= 16;

                                switch (s3->accel.cmd & 0xe0)
                                {
                                        case 0x00: s3->accel.cx++;                break;
                                        case 0x20: s3->accel.cx++; s3->accel.cy--; break;
                                        case 0x40:                s3->accel.cy--; break;
                                        case 0x60: s3->accel.cx--; s3->accel.cy--; break;
                                        case 0x80: s3->accel.cx--;                break;
                                        case 0xa0: s3->accel.cx--; s3->accel.cy++; break;
                                        case 0xc0:                s3->accel.cy++; break;
                                        case 0xe0: s3->accel.cx++; s3->accel.cy++; break;
                                }
                                s3->accel.sy--;
                        }
                }
                else /*Bresenham*/
                {
                        while (count-- && s3->accel.sy >= 0)
                        {
                                if (s3->accel.cx >= clip_l && s3->accel.cx <= clip_r &&
                                    s3->accel.cy >= clip_t && s3->accel.cy <= clip_b)
                                {
                                        switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                        {
                                                case 0: src_dat = s3->accel.bkgd_color; break;
                                                case 1: src_dat = s3->accel.frgd_color; break;
                                                case 2: src_dat = cpu_dat; break;
                                                case 3: src_dat = 0; break;
                                        }

                                        if ((compare_mode == 2 && src_dat != compare) ||
                                            (compare_mode == 3 && src_dat == compare) ||
                                             compare_mode < 2)
                                        {
                                                READ((s3->accel.cy * s3->width) + s3->accel.cx, dest_dat);

//                                        pclog("Line : %04i, %04i (%06X) - %02X (%02X %04X %05X) %02X (%02X %02X)  ", s3->accel.cx, s3->accel.cy, s3->accel.dest + s3->accel.cx, src_dat, vram[s3->accel.src + s3->accel.cx], mix_dat & mix_mask, s3->accel.src + s3->accel.cx, dest_dat, s3->accel.frgd_color, s3->accel.bkgd_color);
                                        
                                                MIX

//                                        pclog("%02X\n", dest_dat);
                                        
                                                WRITE((s3->accel.cy * s3->width) + s3->accel.cx);
                                        }
                                }

                                mix_dat <<= 1;
                                mix_dat |= 1;
                                if (s3->bpp == 0) cpu_dat >>= 8;
                                else             cpu_dat >>= 16;

//                                pclog("%i, %i - %i %i  %i %i\n", s3->accel.cx, s3->accel.cy, s3->accel.err_term, s3->accel.maj_axis_pcnt, s3->accel.desty_axstp, s3->accel.destx_distp);

                                if (s3->accel.err_term >= s3->accel.maj_axis_pcnt)
                                {
                                        s3->accel.err_term += s3->accel.destx_distp;
                                        /*Step minor axis*/
                                        switch (s3->accel.cmd & 0xe0)
                                        {
                                                case 0x00: s3->accel.cy--; break;
                                                case 0x20: s3->accel.cy--; break;
                                                case 0x40: s3->accel.cx--; break;
                                                case 0x60: s3->accel.cx++; break;
                                                case 0x80: s3->accel.cy++; break;
                                                case 0xa0: s3->accel.cy++; break;
                                                case 0xc0: s3->accel.cx--; break;
                                                case 0xe0: s3->accel.cx++; break;
                                        }
                                }
                                else
                                   s3->accel.err_term += s3->accel.desty_axstp;

                                /*Step major axis*/
                                switch (s3->accel.cmd & 0xe0)
                                {
                                        case 0x00: s3->accel.cx--; break;
                                        case 0x20: s3->accel.cx++; break;
                                        case 0x40: s3->accel.cy--; break;
                                        case 0x60: s3->accel.cy--; break;
                                        case 0x80: s3->accel.cx--; break;
                                        case 0xa0: s3->accel.cx++; break;
                                        case 0xc0: s3->accel.cy++; break;
                                        case 0xe0: s3->accel.cy++; break;
                                }
                                s3->accel.sy--;
                        }
                }
                break;
                
                case 2: /*Rectangle fill*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3->accel.sx   = s3->accel.maj_axis_pcnt & 0xfff;
                        s3->accel.sy   = s3->accel.multifunc[0]  & 0xfff;
                        s3->accel.cx   = s3->accel.cur_x;
                        if (s3->accel.cur_x & 0x1000) s3->accel.cx |= ~0xfff;
                        s3->accel.cy   = s3->accel.cur_y;
                        if (s3->accel.cur_y & 0x1000) s3->accel.cy |= ~0xfff;
                        
                        s3->accel.dest = s3->accel.cy * s3->width;

//                        pclog("Dest %08X  (%i, %i) %04X %04X\n", s3->accel.dest, s3->accel.cx, s3->accel.cy, s3->accel.cur_x, s3->accel.cur_x & 0x1000);
                }
                if ((s3->accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
//                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80 && !cpu_input) /*Mix data from CPU*/
//                   return;

                frgd_mix = (s3->accel.frgd_mix >> 5) & 3;
                bkgd_mix = (s3->accel.bkgd_mix >> 5) & 3;
                
                while (count-- && s3->accel.sy >= 0)
                {
                        if (s3->accel.cx >= clip_l && s3->accel.cx <= clip_r &&
                            s3->accel.cy >= clip_t && s3->accel.cy <= clip_b)
                        {
                                switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                {
                                        case 0: src_dat = s3->accel.bkgd_color; break;
                                        case 1: src_dat = s3->accel.frgd_color; break;
                                        case 2: src_dat = cpu_dat; break;
                                        case 3: src_dat = 0; break;
                                }

                                if ((compare_mode == 2 && src_dat != compare) ||
                                    (compare_mode == 3 && src_dat == compare) ||
                                     compare_mode < 2)
                                {
                                        READ(s3->accel.dest + s3->accel.cx, dest_dat);
                                
//                                if (CS != 0xc000) pclog("Write %05X  %02X %02X  %04X (%02X %02X)  ", s3->accel.dest + s3->accel.cx, src_dat, dest_dat, mix_dat, s3->accel.frgd_mix, s3->accel.bkgd_mix);

                                        MIX
                                
//                                if (CS != 0xc000) pclog("%02X\n", dest_dat);
                                
                                        WRITE(s3->accel.dest + s3->accel.cx);
                                }
                        }
                
                        mix_dat <<= 1;
                        mix_dat |= 1;
                        if (s3->bpp == 0) cpu_dat >>= 8;
                        else             cpu_dat >>= 16;
                        
                        if (s3->accel.cmd & 0x20) s3->accel.cx++;
                        else                     s3->accel.cx--;
                        s3->accel.sx--;
                        if (s3->accel.sx < 0)
                        {
                                if (s3->accel.cmd & 0x20) s3->accel.cx   -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                else                     s3->accel.cx   += (s3->accel.maj_axis_pcnt & 0xfff) + 1;
//                                s3->accel.dest -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                s3->accel.sx    = s3->accel.maj_axis_pcnt & 0xfff;
                                
//                                s3->accel.dest  += s3_width;
                                if (s3->accel.cmd & 0x80) s3->accel.cy++;
                                else                     s3->accel.cy--;
                                
                                s3->accel.dest = s3->accel.cy * s3->width;
                                s3->accel.sy--;

                                if (cpu_input/* && (s3->accel.multifunc[0xa] & 0xc0) == 0x80*/) return;
                                if (s3->accel.sy < 0)
                                {
                                        return;
                                }
                        }
                }
                break;

                case 6: /*BitBlt*/
                if (!cpu_input) /*!cpu_input is trigger to start operation*/
                {
                        s3->accel.sx   = s3->accel.maj_axis_pcnt & 0xfff;
                        s3->accel.sy   = s3->accel.multifunc[0]  & 0xfff;

                        s3->accel.dx   = s3->accel.destx_distp & 0xfff;
                        if (s3->accel.destx_distp & 0x1000) s3->accel.dx |= ~0xfff;
                        s3->accel.dy   = s3->accel.desty_axstp & 0xfff;
                        if (s3->accel.desty_axstp & 0x1000) s3->accel.dy |= ~0xfff;

                        s3->accel.cx   = s3->accel.cur_x & 0xfff;
                        if (s3->accel.cur_x & 0x1000) s3->accel.cx |= ~0xfff;
                        s3->accel.cy   = s3->accel.cur_y & 0xfff;
                        if (s3->accel.cur_y & 0x1000) s3->accel.cy |= ~0xfff;

                        s3->accel.src  = s3->accel.cy * s3->width;
                        s3->accel.dest = s3->accel.dy * s3->width;
                        
//                        pclog("Source %08X Dest %08X  (%i, %i) - (%i, %i)\n", s3->accel.src, s3->accel.dest, s3->accel.cx, s3->accel.cy, s3->accel.dx, s3->accel.dy);
                }
                if ((s3->accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
//                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80 && !cpu_input) /*Mix data from CPU*/
//                  return;

                if (s3->accel.sy < 0)
                   return;

                frgd_mix = (s3->accel.frgd_mix >> 5) & 3;
                bkgd_mix = (s3->accel.bkgd_mix >> 5) & 3;
                
                if (!cpu_input && frgd_mix == 3 && !vram_mask && !compare_mode &&
                    (s3->accel.cmd & 0xa0) == 0xa0 && (s3->accel.frgd_mix & 0xf) == 7) 
                {
                        while (1)
                        {
                                if (s3->accel.dx >= clip_l && s3->accel.dx <= clip_r &&
                                    s3->accel.dy >= clip_t && s3->accel.dy <= clip_b)
                                {
                                        READ(s3->accel.src + s3->accel.cx, src_dat);
        
                                        dest_dat = src_dat;
                                        
                                        WRITE(s3->accel.dest + s3->accel.dx);
                                }
        
                                s3->accel.cx++;
                                s3->accel.dx++;
                                s3->accel.sx--;
                                if (s3->accel.sx < 0)
                                {
                                        s3->accel.cx -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                        s3->accel.dx -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                        s3->accel.sx  =  s3->accel.maj_axis_pcnt & 0xfff;
        
                                        s3->accel.cy++;
                                        s3->accel.dy++;
        
                                        s3->accel.src  = s3->accel.cy * s3->width;
                                        s3->accel.dest = s3->accel.dy * s3->width;
        
                                        s3->accel.sy--;
        
                                        if (s3->accel.sy < 0)
                                           return;
                                }
                        }
                }
                else
                {                     
                        while (count-- && s3->accel.sy >= 0)
                        {
                                if (s3->accel.dx >= clip_l && s3->accel.dx <= clip_r &&
                                    s3->accel.dy >= clip_t && s3->accel.dy <= clip_b)
                                {
                                        if (vram_mask)
                                        {
                                                READ(s3->accel.src + s3->accel.cx, mix_dat)
                                                mix_dat = mix_dat ? mix_mask : 0;
                                        }
                                        switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                        {
                                                case 0: src_dat = s3->accel.bkgd_color;                  break;
                                                case 1: src_dat = s3->accel.frgd_color;                  break;
                                                case 2: src_dat = cpu_dat;                              break;
                                                case 3: READ(s3->accel.src + s3->accel.cx, src_dat);      break;
                                        }

                                        if ((compare_mode == 2 && src_dat != compare) ||
                                            (compare_mode == 3 && src_dat == compare) ||
                                             compare_mode < 2)
                                        {
                                                READ(s3->accel.dest + s3->accel.dx, dest_dat);
                                                                                
//                                pclog("BitBlt : %04i, %04i (%06X) - %02X (%02X %04X %05X) %02X   ", s3->accel.dx, s3->accel.dy, s3->accel.dest + s3->accel.dx, src_dat, vram[s3->accel.src + s3->accel.cx], mix_dat, s3->accel.src + s3->accel.cx, dest_dat);
                                
                                                MIX

//                                pclog("%02X\n", dest_dat);

                                                WRITE(s3->accel.dest + s3->accel.dx);
                                        }
                                }

                                mix_dat <<= 1;
                                mix_dat |= 1;
                                if (s3->bpp == 0) cpu_dat >>= 8;
                                else             cpu_dat >>= 16;

                                if (s3->accel.cmd & 0x20)
                                {
                                        s3->accel.cx++;
                                        s3->accel.dx++;
                                }
                                else
                                {
                                        s3->accel.cx--;
                                        s3->accel.dx--;
                                }
                                s3->accel.sx--;
                                if (s3->accel.sx < 0)
                                {
                                        if (s3->accel.cmd & 0x20)
                                        {
                                                s3->accel.cx -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                                s3->accel.dx -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                        }
                                        else
                                        {
                                                s3->accel.cx += (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                                s3->accel.dx += (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                        }
                                        s3->accel.sx    =  s3->accel.maj_axis_pcnt & 0xfff;

                                        if (s3->accel.cmd & 0x80)
                                        {
                                                s3->accel.cy++;
                                                s3->accel.dy++;
                                        }
                                        else
                                        {
                                                s3->accel.cy--;
                                                s3->accel.dy--;
                                        }

                                        s3->accel.src  = s3->accel.cy * s3->width;
                                        s3->accel.dest = s3->accel.dy * s3->width;

                                        s3->accel.sy--;

                                        if (cpu_input/* && (s3->accel.multifunc[0xa] & 0xc0) == 0x80*/) return;
                                        if (s3->accel.sy < 0)
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
                        s3->accel.sx   = s3->accel.maj_axis_pcnt & 0xfff;
                        s3->accel.sy   = s3->accel.multifunc[0]  & 0xfff;

                        s3->accel.dx   = s3->accel.destx_distp & 0xfff;
                        if (s3->accel.destx_distp & 0x1000) s3->accel.dx |= ~0xfff;
                        s3->accel.dy   = s3->accel.desty_axstp & 0xfff;
                        if (s3->accel.desty_axstp & 0x1000) s3->accel.dy |= ~0xfff;

                        s3->accel.cx   = s3->accel.cur_x & 0xfff;
                        if (s3->accel.cur_x & 0x1000) s3->accel.cx |= ~0xfff;
                        s3->accel.cy   = s3->accel.cur_y & 0xfff;
                        if (s3->accel.cur_y & 0x1000) s3->accel.cy |= ~0xfff;
                        
                        /*Align source with destination*/
//                        s3->accel.cx = (s3->accel.cx & ~7) | (s3->accel.dx & 7);
//                        s3->accel.cy = (s3->accel.cy & ~7) | (s3->accel.dy & 7);

                        s3->accel.pattern  = (s3->accel.cy * s3->width) + s3->accel.cx;
                        s3->accel.dest     = s3->accel.dy * s3->width;
                        
                        s3->accel.cx = s3->accel.dx & 7;
                        s3->accel.cy = s3->accel.dy & 7;
                        
                        s3->accel.src  = s3->accel.pattern + (s3->accel.cy * s3->width);

//                        pclog("Source %08X Dest %08X  (%i, %i) - (%i, %i)\n", s3->accel.src, s3->accel.dest, s3->accel.cx, s3->accel.cy, s3->accel.dx, s3->accel.dy);
//                        dumpregs();
//                        exit(-1);
                }
                if ((s3->accel.cmd & 0x100) && !cpu_input) return; /*Wait for data from CPU*/
//                if ((s3->accel.multifunc[0xa] & 0xc0) == 0x80 && !cpu_input) /*Mix data from CPU*/
//                   return;

                frgd_mix = (s3->accel.frgd_mix >> 5) & 3;
                bkgd_mix = (s3->accel.bkgd_mix >> 5) & 3;

                while (count-- && s3->accel.sy >= 0)
                {
                        if (s3->accel.dx >= clip_l && s3->accel.dx <= clip_r &&
                            s3->accel.dy >= clip_t && s3->accel.dy <= clip_b)
                        {
                                if (vram_mask)
                                {
                                        READ(s3->accel.src + s3->accel.cx, mix_dat)
                                        mix_dat = mix_dat ? mix_mask : 0;
                                }
                                switch ((mix_dat & mix_mask) ? frgd_mix : bkgd_mix)
                                {
                                        case 0: src_dat = s3->accel.bkgd_color;                  break;
                                        case 1: src_dat = s3->accel.frgd_color;                  break;
                                        case 2: src_dat = cpu_dat;                              break;
                                        case 3: READ(s3->accel.src + s3->accel.cx, src_dat);      break;
                                }

                                if ((compare_mode == 2 && src_dat != compare) ||
                                    (compare_mode == 3 && src_dat == compare) ||
                                     compare_mode < 2)
                                {
                                        READ(s3->accel.dest + s3->accel.dx, dest_dat);
                                
//                                pclog("Pattern fill : %04i, %04i (%06X) - %02X (%02X %04X %05X) %02X   ", s3->accel.dx, s3->accel.dy, s3->accel.dest + s3->accel.dx, src_dat, vram[s3->accel.src + s3->accel.cx], mix_dat, s3->accel.src + s3->accel.cx, dest_dat);

                                        MIX

//                                pclog("%02X\n", dest_dat);

                                        WRITE(s3->accel.dest + s3->accel.dx);
                                }
                        }

                        mix_dat <<= 1;
                        mix_dat |= 1;
                        if (s3->bpp == 0) cpu_dat >>= 8;
                        else             cpu_dat >>= 16;

                        if (s3->accel.cmd & 0x20)
                        {
                                s3->accel.cx = ((s3->accel.cx + 1) & 7) | (s3->accel.cx & ~7);
                                s3->accel.dx++;
                        }
                        else
                        {
                                s3->accel.cx = ((s3->accel.cx - 1) & 7) | (s3->accel.cx & ~7);
                                s3->accel.dx--;
                        }
                        s3->accel.sx--;
                        if (s3->accel.sx < 0)
                        {
                                if (s3->accel.cmd & 0x20)
                                {
                                        s3->accel.cx = ((s3->accel.cx - ((s3->accel.maj_axis_pcnt & 0xfff) + 1)) & 7) | (s3->accel.cx & ~7);
                                        s3->accel.dx -= (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                }
                                else
                                {
                                        s3->accel.cx = ((s3->accel.cx + ((s3->accel.maj_axis_pcnt & 0xfff) + 1)) & 7) | (s3->accel.cx & ~7);
                                        s3->accel.dx += (s3->accel.maj_axis_pcnt & 0xfff) + 1;
                                }
                                s3->accel.sx    =  s3->accel.maj_axis_pcnt & 0xfff;

                                if (s3->accel.cmd & 0x80)
                                {
                                        s3->accel.cy = ((s3->accel.cy + 1) & 7) | (s3->accel.cy & ~7);
                                        s3->accel.dy++;
                                }
                                else
                                {
                                        s3->accel.cy = ((s3->accel.cy - 1) & 7) | (s3->accel.cy & ~7);
                                        s3->accel.dy--;
                                }

                                s3->accel.src  = s3->accel.pattern + (s3->accel.cy * s3->width);
                                s3->accel.dest = s3->accel.dy * s3->width;

                                s3->accel.sy--;

                                if (cpu_input/* && (s3->accel.multifunc[0xa] & 0xc0) == 0x80*/) return;
                                if (s3->accel.sy < 0)
                                {
                                        return;
                                }
                        }
                }
                break;
        }
}

void s3_hwcursor_draw(svga_t *svga, int displine)
{
        int x;
        uint16_t dat[2];
        int xx;
        int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
        
        pclog("HWcursor %i %i\n", svga->hwcursor_latch.x, svga->hwcursor_latch.y);
        for (x = 0; x < 64; x += 16)
        {
                dat[0] = (svga->vram[svga->hwcursor_latch.addr]     << 8) | svga->vram[svga->hwcursor_latch.addr + 1];
                dat[1] = (svga->vram[svga->hwcursor_latch.addr + 2] << 8) | svga->vram[svga->hwcursor_latch.addr + 3];
                for (xx = 0; xx < 16; xx++)
                {
                        if (offset >= svga->hwcursor_latch.x)
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
                svga->hwcursor_latch.addr += 4;
        }
}


uint8_t s3_pci_read(int func, int addr, void *p)
{
        s3_t *s3 = (s3_t *)p;
        svga_t *svga = &s3->svga;
//        pclog("S3 PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x33; /*'S3'*/
                case 0x01: return 0x53;
                
                case 0x02: return s3->id_ext;
                case 0x03: return 0x88;
                
                case 0x04: return 0x03; /*Respond to IO and memory accesses*/

                case 0x07: return 1 << 1; /*Medium DEVSEL timing*/
                
                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x0a: return 0x01; /*Supports VGA interface*/
                case 0x0b: return 0x03;
                
                case 0x10: return 0x00; /*Linear frame buffer address*/
                case 0x11: return 0x00;
                case 0x12: return svga->crtc[0x5a] & 0x80;
                case 0x13: return svga->crtc[0x59];

                case 0x30: return 0x01; /*BIOS ROM address*/
                case 0x31: return 0x00;
                case 0x32: return 0x0C;
                case 0x33: return 0x00;
        }
        return 0;
}

void s3_pci_write(int func, int addr, uint8_t val, void *p)
{
        s3_t *s3 = (s3_t *)p;
        svga_t *svga = &s3->svga;
        switch (addr)
        {
                case 0x12: 
                svga->crtc[0x5a] = val & 0x80; 
                s3_updatemapping(s3); 
                break;
                case 0x13: 
                svga->crtc[0x59] = val;        
                s3_updatemapping(s3); 
                break;                
        }
}

static void *s3_init()
{
        s3_t *s3 = malloc(sizeof(s3_t));
        svga_t *svga = &s3->svga;
        memset(s3, 0, sizeof(s3_t));
        
        mem_mapping_add(&s3->linear_mapping, 0,       0,       svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear, &s3->svga);
        mem_mapping_add(&s3->mmio_mapping,   0xa0000, 0x10000, s3_accel_read, NULL, NULL, s3_accel_write, s3_accel_write_w, s3_accel_write_l, s3);
        mem_mapping_disable(&s3->mmio_mapping);

        svga_init(&s3->svga, s3, 1 << 22, /*4mb - 864 supports 8mb but buggy VESA driver reports 0mb*/
                   s3_recalctimings,
                   s3_in, s3_out,
                   s3_hwcursor_draw);

        io_sethandler(0x03c0, 0x0020, s3_in, NULL, NULL, s3_out, NULL, NULL, s3);

        svga->crtc[0x36] = 1 | (2 << 2) | (1 << 4) | (4 << 5);
        svga->crtc[0x37] = 1 | (7 << 5);
        
        io_sethandler(0x42e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x46e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x4ae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x82e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x86e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x8ae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x8ee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x92e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x96e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x9ae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0x9ee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xa2e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xa6e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xaae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xaee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xb2e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xb6e8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xbae8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xbee8, 0x0002, s3_accel_in, NULL, NULL, s3_accel_out, NULL, NULL,  s3);
        io_sethandler(0xe2e8, 0x0004, s3_accel_in, NULL, NULL, s3_accel_out, s3_accel_out_w, s3_accel_out_l,  s3);

        pci_add(s3_pci_read, s3_pci_write, s3);
 
        return s3;
}

void *s3_bahamas64_init()
{
        s3_t *s3 = s3_init();

        s3->id = 0xc1; /*Vision864P*/
        s3->id_ext = 0xc1; /*Trio64*/
        
        return s3;
}

void *s3_9fx_init()
{
        s3_t *s3 = s3_init();

        s3->id = 0xe1;
        s3->id_ext = 0x11; /*Trio64*/
        
        return s3;
}

void s3_close(void *p)
{
        s3_t *s3 = (s3_t *)p;

        svga_close(&s3->svga);
        
        free(s3);
}

void s3_speed_changed(void *p)
{
        s3_t *s3 = (s3_t *)p;
        
        svga_recalctimings(&s3->svga);
}

void s3_force_redraw(void *p)
{
        s3_t *s3 = (s3_t *)p;

        s3->svga.fullchange = changeframecount;
}

device_t s3_bahamas64_device =
{
        "Paradise Bahamas 64 (S3 Vision864)",
        s3_bahamas64_init,
        s3_close,
        s3_speed_changed,
        s3_force_redraw,
        svga_add_status_info
};

device_t s3_9fx_device =
{
        "Number 9 9FX (S3 Trio64)",
        s3_9fx_init,
        s3_close,
        s3_speed_changed,
        s3_force_redraw,
        svga_add_status_info
};
