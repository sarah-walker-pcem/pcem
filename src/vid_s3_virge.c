/*S3 ViRGE emulation

  The SVGA core is largely the same as older S3 chips, but the blitter is totally different*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "rom.h"
#include "video.h"
#include "vid_s3_virge.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
//#include "vid_sdac_ramdac.h"

typedef struct virge_t
{
        mem_mapping_t   linear_mapping;
        mem_mapping_t     mmio_mapping;
        mem_mapping_t new_mmio_mapping;
        
        rom_t bios_rom;
        
        svga_t svga;
        
        uint8_t bank;
        uint8_t ma_ext;
        int width;
        int bpp;

        uint8_t virge_id, virge_id_high, virge_id_low, virge_rev;

        uint32_t linear_base, linear_size;

        uint8_t pci_regs[256];
        
        struct
        {
                uint32_t src_base;
                uint32_t dest_base;
                int clip_l, clip_r, clip_t, clip_b;
                int dest_str, src_str;
                uint32_t mono_pat_0;
                uint32_t mono_pat_1;
                uint32_t pat_bg_clr;
                uint32_t pat_fg_clr;
                uint32_t src_bg_clr;
                uint32_t src_fg_clr;
                uint32_t cmd_set;
                int r_width, r_height;
                int rsrc_x, rsrc_y;
                int rdest_x, rdest_y;
                
                int lxend0, lxend1;
                int32_t ldx;
                uint32_t lxstart, lystart;
                int lycnt;
                int line_dir;
                
                int src_x, src_y;
                int dest_x, dest_y;
                int w, h;
                uint8_t rop;
                
                int data_left_count;
                uint32_t data_left;
                
                uint32_t pattern_8[8*8];
                uint32_t pattern_16[8*8];
                uint32_t pattern_32[8*8];
        } s3d;
        
        struct
        {
                uint32_t pri_ctrl;
                uint32_t chroma_ctrl;
                uint32_t sec_ctrl;
                uint32_t chroma_upper_bound;
                uint32_t sec_filter;
                uint32_t blend_ctrl;
                uint32_t pri_fb0, pri_fb1;
                uint32_t pri_stride;
                uint32_t buffer_ctrl;
                uint32_t sec_fb0, sec_fb1;
                uint32_t sec_stride;
                uint32_t overlay_ctrl;
                uint32_t k1_vert_scale;
                uint32_t k2_vert_scale;
                uint32_t dda_vert_accumulator;
                uint32_t fifo_ctrl;
                uint32_t pri_start;
                uint32_t pri_size;
                uint32_t sec_start;
                uint32_t sec_size;
                
                int pri_x, pri_y, pri_w, pri_h;
                int sec_x, sec_y, sec_w, sec_h;
        } streams;
} virge_t;

static void s3_virge_recalctimings(svga_t *svga);
static void s3_virge_updatemapping(virge_t *virge);

static void s3_virge_bitblt(virge_t *virge, int count, uint32_t cpu_dat);

static uint8_t  s3_virge_mmio_read(uint32_t addr, void *p);
static uint16_t s3_virge_mmio_read_w(uint32_t addr, void *p);
static uint32_t s3_virge_mmio_read_l(uint32_t addr, void *p);
static void     s3_virge_mmio_write(uint32_t addr, uint8_t val, void *p);
static void     s3_virge_mmio_write_w(uint32_t addr, uint16_t val, void *p);
static void     s3_virge_mmio_write_l(uint32_t addr, uint32_t val, void *p);

enum
{
        CMD_SET_AE = 1,
        CMD_SET_HC = (1 << 1),
        
        CMD_SET_FORMAT_MASK = (7 << 2),
        CMD_SET_FORMAT_8 = (0 << 2),
        CMD_SET_FORMAT_16 = (1 << 2),
        CMD_SET_FORMAT_24 = (2 << 2),
        
        CMD_SET_MS = (1 << 6),
        CMD_SET_IDS = (1 << 7),
        CMD_SET_MP = (1 << 8),
        CMD_SET_TP = (1 << 9),
        
        CMD_SET_ITA_MASK = (3 << 10),
        CMD_SET_ITA_BYTE = (0 << 10),
        CMD_SET_ITA_WORD = (1 << 10),
        CMD_SET_ITA_DWORD = (2 << 10),

        CMD_SET_XP = (1 << 25),
        CMD_SET_YP = (1 << 26),
        
        CMD_SET_COMMAND_MASK = (15 << 27)
};

enum
{
        CMD_SET_COMMAND_BITBLT = (0 << 27),
        CMD_SET_COMMAND_RECTFILL = (2 << 27),
        CMD_SET_COMMAND_LINE = (3 << 27),
        CMD_SET_COMMAND_NOP = (15 << 27)
};

static void s3_virge_out(uint16_t addr, uint8_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
        uint8_t old;

        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;
       
//        pclog("S3 out %04X %02X %04X:%08X  %04X %04X %i\n", addr, val, CS, pc, ES, BX, ins);

        switch (addr)
        {
                case 0x3c5:
                if (svga->seqaddr >= 0x10)
                {
                        svga->seqregs[svga->seqaddr & 0x1f]=val;
                        s3_virge_recalctimings(svga);
                        return;
                }
                if (svga->seqaddr == 4) /*Chain-4 - update banking*/
                {
                        if (val & 8) svga->write_bank = svga->read_bank = virge->bank << 16;
                        else         svga->write_bank = svga->read_bank = virge->bank << 14;
                }
                break;
                
                //case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Write RAMDAC %04X %02X %04X:%04X\n", addr, val, CS, pc);
                //sdac_ramdac_out(addr,val);
                //return;

                case 0x3d4:
                svga->crtcreg = val & 0x7f;
                return;
                case 0x3d5:
//                pclog("Write CRTC R%02X %02X\n", svga->crtcreg, val);
                if (svga->crtcreg <= 7 && svga->crtc[0x11] & 0x80) 
                        return;
                if (svga->crtcreg >= 0x20 && svga->crtcreg != 0x38 && (svga->crtc[0x38] & 0xcc) != 0x48) 
                        return;
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                switch (svga->crtcreg)
                {
                        case 0x31:
                        virge->ma_ext = (virge->ma_ext & 0x1c) | ((val & 0x30) >> 4);
                        svga->vrammask = (val & 8) ? 0x3fffff : 0x3ffff;
                        break;
                        
                        case 0x50:
                        switch (svga->crtc[0x50] & 0xc1)
                        {
                                case 0x00: virge->width = (svga->crtc[0x31] & 2) ? 2048 : 1024; break;
                                case 0x01: virge->width = 1152; break;
                                case 0x40: virge->width = 640;  break;
                                case 0x80: virge->width = 800;  break;
                                case 0x81: virge->width = 1600; break;
                                case 0xc0: virge->width = 1280; break;
                        }
                        virge->bpp = (svga->crtc[0x50] >> 4) & 3;
                        break;
                        case 0x69:
                        virge->ma_ext = val & 0x1f;
                        break;
                        
                        case 0x35:
                        virge->bank = (virge->bank & 0x70) | (val & 0xf);
//                        pclog("CRTC write R35 %02X\n", val);
                        if (svga->chain4) svga->write_bank = svga->read_bank = virge->bank << 16;
                        else              svga->write_bank = svga->read_bank = virge->bank << 14;
                        break;
                        case 0x51:
                        virge->bank = (virge->bank & 0x4f) | ((val & 0xc) << 2);
                        if (svga->chain4) svga->write_bank = svga->read_bank = virge->bank << 16;
                        else              svga->write_bank = svga->read_bank = virge->bank << 14;
                        virge->ma_ext = (virge->ma_ext & ~0xc) | ((val & 3) << 2);
                        break;
                        case 0x6a:
                        virge->bank = val;
//                        pclog("CRTC write R6a %02X\n", val);
                        if (svga->chain4) svga->write_bank = svga->read_bank = virge->bank << 16;
                        else              svga->write_bank = svga->read_bank = virge->bank << 14;
                        break;
                        
                        case 0x3a:
                        if (val & 0x10) svga->gdcreg[5] |= 0x40; /*Horrible cheat*/
                        break;
                        
                        case 0x45:
                        svga->hwcursor.ena = val & 1;
                        break;
                        case 0x46: case 0x47: case 0x48: case 0x49:
                        case 0x4c: case 0x4d: case 0x4e: case 0x4f:
                        svga->hwcursor.x = ((svga->crtc[0x46] << 8) | svga->crtc[0x47]) & 0x7ff;
                        svga->hwcursor.y = ((svga->crtc[0x48] << 8) | svga->crtc[0x49]) & 0x7ff;
                        svga->hwcursor.xoff = svga->crtc[0x4e] & 63;
                        svga->hwcursor.yoff = svga->crtc[0x4f] & 63;
                        svga->hwcursor.addr = ((((svga->crtc[0x4c] << 8) | svga->crtc[0x4d]) & 0xfff) * 1024) + (svga->hwcursor.yoff * 16);
                        break;

                        case 0x53:
                        case 0x58: case 0x59: case 0x5a:
                        s3_virge_updatemapping(virge);
                        break;
                        
                        case 0x67:
                        switch (val >> 4)
                        {
                                case 3:  svga->bpp = 15; break;
                                case 5:  svga->bpp = 16; break;
                                case 7:  svga->bpp = 24; break;
                                case 13: svga->bpp = 32; break;
                                default: svga->bpp = 8;  break;
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

static uint8_t s3_virge_in(uint16_t addr, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
        
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

//        if (addr != 0x3da) pclog("S3 in %04X %04X:%08X\n", addr, CS, pc);
        switch (addr)
        {
                //case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Read RAMDAC %04X  %04X:%04X\n", addr, CS, pc);
                //return sdac_ramdac_in(addr);

                case 0x3c5:
                if (svga->seqaddr >= 0x10)
                   return svga->seqregs[svga->seqaddr & 0x1f];
                break;

                case 0x3D4:
                return svga->crtcreg;
                case 0x3D5:
//                pclog("Read CRTC R%02X %04X:%04X (%02x)\n", svga->crtcreg, CS, pc, svga->crtc[svga->crtcreg]);
                switch (svga->crtcreg)
                {
                        case 0x2d: return virge->virge_id_high; /*Extended chip ID*/
                        case 0x2e: return virge->virge_id_low;  /*New chip ID*/
                        case 0x2f: return virge->virge_rev;
                        case 0x30: return virge->virge_id;      /*Chip ID*/
                        case 0x31: return (svga->crtc[0x31] & 0xcf) | ((virge->ma_ext & 3) << 4);
                        case 0x35: return (svga->crtc[0x35] & 0xf0) | (virge->bank & 0xf);
                        case 0x36: return (svga->crtc[0x36] & 0xfc) | 2; /*PCI bus*/
                        case 0x51: return (svga->crtc[0x51] & 0xf0) | ((virge->bank >> 2) & 0xc) | ((virge->ma_ext >> 2) & 3);
                        case 0x69: return virge->ma_ext;
                        case 0x6a: return virge->bank;
                }
                return svga->crtc[svga->crtcreg];
        }
        return svga_in(addr, svga);
}

static void s3_virge_recalctimings(svga_t *svga)
{
        virge_t *virge = (virge_t *)svga->p;

        if (svga->crtc[0x5d] & 0x01) svga->htotal      += 0x100;
        if (svga->crtc[0x5d] & 0x02) svga->hdisp       += 0x100;
        if (svga->crtc[0x5e] & 0x01) svga->vtotal      += 0x400;
        if (svga->crtc[0x5e] & 0x02) svga->dispend     += 0x400;
        if (svga->crtc[0x5e] & 0x04) svga->vblankstart += 0x400;
        if (svga->crtc[0x5e] & 0x10) svga->vsyncstart  += 0x400;
        if (svga->crtc[0x5e] & 0x40) svga->split       += 0x400;
        svga->interlace = svga->crtc[0x42] & 0x20;

        if ((svga->crtc[0x67] & 0xc) != 0xc) /*VGA mode*/
        {
                svga->ma_latch |= (virge->ma_ext << 16);

                if (svga->crtc[0x51] & 0x30)      svga->rowoffset += (svga->crtc[0x51] & 0x30) << 4;
                else if (svga->crtc[0x43] & 0x04) svga->rowoffset += 0x100;
                if (!svga->rowoffset) svga->rowoffset = 256;

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
                                break;
                        }
                }
        
//                pclog("svga->rowoffset = %i bpp=%i\n", svga->rowoffset, svga->bpp);
                if (svga->bpp == 15 || svga->bpp == 16)
                {
                        svga->htotal >>= 1;
                        svga->hdisp >>= 1;
                }
                if (svga->bpp == 24)
                {
                        svga->rowoffset = (svga->rowoffset * 3) / 4; /*Hack*/
                }
        }
        else /*Streams mode*/
        {
                if (virge->streams.buffer_ctrl & 1)
                        svga->ma_latch = virge->streams.pri_fb1 >> 2;
                else
                        svga->ma_latch = virge->streams.pri_fb0 >> 2;
                
                svga->rowoffset = virge->streams.pri_stride >> 3;

                switch ((virge->streams.pri_ctrl >> 24) & 0x7)
                {
                        case 0: /*RGB-8 (CLUT)*/
                        svga->render = svga_render_8bpp_highres; 
                        break;
                        case 3: /*KRGB-16 (1.5.5.5)*/ 
                        svga->render = svga_render_15bpp_highres; 
                        break;
                        case 5: /*RGB-16 (5.6.5)*/ 
                        svga->render = svga_render_16bpp_highres; 
                        break;
                        case 6: /*RGB-24 (8.8.8)*/ 
                        svga->render = svga_render_24bpp_highres; 
                        break;
                        case 7: /*XRGB-32 (X.8.8.8)*/
                        svga->render = svga_render_32bpp_highres; 
                        break;
                }
        }

        if (((svga->miscout >> 2) & 3) == 3)
        {
                int n = svga->seqregs[0x12] & 0x1f;
                int r = (svga->seqregs[0x12] >> 5) & 3;
                int m = svga->seqregs[0x13] & 0x7f;
                double freq = (((double)m + 2) / (((double)n + 2) * (double)(1 << r))) * 14318184.0;

                svga->clock = cpuclock / freq;
        }
}

static void s3_virge_updatemapping(virge_t *virge)
{
        svga_t *svga = &virge->svga;

        if (!(virge->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM))
        {
//                pclog("Update mapping - PCI disabled\n");
                mem_mapping_disable(&svga->mapping);
                mem_mapping_disable(&virge->linear_mapping);
                mem_mapping_disable(&virge->mmio_mapping);
                mem_mapping_disable(&virge->new_mmio_mapping);
                return;
        }

        pclog("Update mapping - bank %02X ", svga->gdcreg[6] & 0xc);        
        switch (svga->gdcreg[6] & 0xc) /*Banked framebuffer*/
        {
                case 0x0: /*128k at A0000*/
                mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x20000);
                svga->banked_mask = 0xffff;
                break;
                case 0x4: /*64k at A0000*/
                mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
                svga->banked_mask = 0xffff;
                break;
                case 0x8: /*32k at B0000*/
                mem_mapping_set_addr(&svga->mapping, 0xb0000, 0x08000);
                svga->banked_mask = 0x7fff;
                break;
                case 0xC: /*32k at B8000*/
                mem_mapping_set_addr(&svga->mapping, 0xb8000, 0x08000);
                svga->banked_mask = 0x7fff;
                break;
        }
        
        virge->linear_base = (svga->crtc[0x5a] << 16) | (svga->crtc[0x59] << 24);
        
        pclog("Linear framebuffer %02X ", svga->crtc[0x58] & 0x10);
        if (svga->crtc[0x58] & 0x10) /*Linear framebuffer*/
        {
                switch (svga->crtc[0x58] & 3)
                {
                        case 0: /*64k*/
                        virge->linear_size = 0x10000;
                        break;
                        case 1: /*1mb*/
                        virge->linear_size = 0x100000;
                        break;
                        case 2: /*2mb*/
                        virge->linear_size = 0x200000;
                        break;
                        case 3: /*8mb*/
                        virge->linear_size = 0x400000;
                        break;
                }
                virge->linear_base &= ~(virge->linear_size - 1);
//                pclog("%08X %08X  %02X %02X %02X\n", linear_base, linear_size, crtc[0x58], crtc[0x59], crtc[0x5a]);
                pclog("Linear framebuffer at %08X size %08X\n", virge->linear_base, virge->linear_size);
                if (virge->linear_base == 0xa0000)
                {
                        mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
                        mem_mapping_disable(&virge->linear_mapping);
                }
                else
                        mem_mapping_set_addr(&virge->linear_mapping, virge->linear_base, virge->linear_size);
        }
        else
                mem_mapping_disable(&virge->linear_mapping);
        
        pclog("Memory mapped IO %02X\n", svga->crtc[0x53] & 0x18);
        if (svga->crtc[0x53] & 0x10) /*Old MMIO*/
        {
                if (svga->crtc[0x53] & 0x20)
                        mem_mapping_set_addr(&virge->mmio_mapping, 0xb8000, 0x8000);
                else
                        mem_mapping_set_addr(&virge->mmio_mapping, 0xa0000, 0x10000);
        }
        else
                mem_mapping_disable(&virge->mmio_mapping);

        if (svga->crtc[0x53] & 0x08) /*New MMIO*/
                mem_mapping_set_addr(&virge->new_mmio_mapping, virge->linear_base + 0x1000000, 0x10000);
        else
                mem_mapping_disable(&virge->new_mmio_mapping);

}


static uint8_t s3_virge_mmio_read(uint32_t addr, void *p)
{
//        pclog("New MMIO readb %08X\n", addr);
        switch (addr & 0xffff)
        {
                case 0x83b0: case 0x83b1: case 0x83b2: case 0x83b3:
                case 0x83b4: case 0x83b5: case 0x83b6: case 0x83b7:
                case 0x83b8: case 0x83b9: case 0x83ba: case 0x83bb:
                case 0x83bc: case 0x83bd: case 0x83be: case 0x83bf:
                case 0x83c0: case 0x83c1: case 0x83c2: case 0x83c3:
                case 0x83c4: case 0x83c5: case 0x83c6: case 0x83c7:
                case 0x83c8: case 0x83c9: case 0x83ca: case 0x83cb:
                case 0x83cc: case 0x83cd: case 0x83ce: case 0x83cf:
                case 0x83d0: case 0x83d1: case 0x83d2: case 0x83d3:
                case 0x83d4: case 0x83d5: case 0x83d6: case 0x83d7:
                case 0x83d8: case 0x83d9: case 0x83da: case 0x83db:
                case 0x83dc: case 0x83dd: case 0x83de: case 0x83df:
                return s3_virge_in(addr & 0x3ff, p);
        }
        return 0xff;
}
static uint16_t s3_virge_mmio_read_w(uint32_t addr, void *p)
{
//        pclog("New MMIO readw %08X\n", addr);
        switch (addr & 0xfffe)
        {
                default:
                return s3_virge_mmio_read(addr, p) | (s3_virge_mmio_read(addr + 1, p) << 8);
        }
        return 0xffff;
}
static uint32_t s3_virge_mmio_read_l(uint32_t addr, void *p)
{
        virge_t *virge = (virge_t *)p;
        uint32_t ret = 0xffffffff;
//        pclog("New MMIO readl %08X\n", addr);
        switch (addr & 0xfffc)
        {
                case 0x8180:
                ret = virge->streams.pri_ctrl;
                break;
                case 0x8184:
                ret = virge->streams.chroma_ctrl;
                break;
                case 0x8190:
                ret = virge->streams.sec_ctrl;
                break;
                case 0x8194:
                ret = virge->streams.chroma_upper_bound;
                break;
                case 0x8198:
                ret = virge->streams.sec_filter;
                break;
                case 0x81a0:
                ret = virge->streams.blend_ctrl;
                break;
                case 0x81c0:
                ret = virge->streams.pri_fb0;
                break;
                case 0x81c4:
                ret = virge->streams.pri_fb1;
                break;
                case 0x81c8:
                ret = virge->streams.pri_stride;
                break;
                case 0x81cc:
                ret = virge->streams.buffer_ctrl;
                break;
                case 0x81d0:
                ret = virge->streams.sec_fb0;
                break;
                case 0x81d4:
                ret = virge->streams.sec_fb1;
                break;
                case 0x81d8:
                ret = virge->streams.sec_stride;
                break;
                case 0x81dc:
                ret = virge->streams.overlay_ctrl;
                break;
                case 0x81e0:
                ret = virge->streams.k1_vert_scale;
                break;
                case 0x81e4:
                ret = virge->streams.k2_vert_scale;
                break;
                case 0x81e8:
                ret = virge->streams.dda_vert_accumulator;
                break;
                case 0x81ec:
                ret = virge->streams.fifo_ctrl;
                break;
                case 0x81f0:
                ret = virge->streams.pri_start;
                break;
                case 0x81f4:
                ret = virge->streams.pri_size;
                break;
                case 0x81f8:
                ret = virge->streams.sec_start;
                break;
                case 0x81fc:
                ret = virge->streams.sec_size;
                break;
                
                case 0x8504:
                ret = (0x1f << 8) | (1 << 13);
                break;
                case 0xa4d4:
                ret = virge->s3d.src_base;
                break;
                case 0xa4d8:
                ret = virge->s3d.dest_base;
                break;
                case 0xa4dc:
                ret = (virge->s3d.clip_l << 16) | virge->s3d.clip_r;
                break;
                case 0xa4e0:
                ret = (virge->s3d.clip_t << 16) | virge->s3d.clip_b;
                break;
                case 0xa4e4:
                ret = (virge->s3d.dest_str << 16) | virge->s3d.src_str;
                break;
                case 0xa4e8:
                ret = virge->s3d.mono_pat_0;
                break;
                case 0xa4ec:
                ret = virge->s3d.mono_pat_1;
                break;
                case 0xa4f0:
                ret = virge->s3d.pat_bg_clr;
                break;
                case 0xa4f4:
                ret = virge->s3d.pat_fg_clr;
                break;
                case 0xa4f8:
                ret = virge->s3d.src_bg_clr;
                break;
                case 0xa4fc:
                ret = virge->s3d.src_fg_clr;
                break;
                case 0xa500:
                ret = virge->s3d.cmd_set;
                break;
                case 0xa504:
                ret = (virge->s3d.r_width << 16) | virge->s3d.r_height;
                break;
                case 0xa508:
                ret = (virge->s3d.rsrc_x << 16) | virge->s3d.rsrc_y;
                break;
                case 0xa50c:
                ret = (virge->s3d.rdest_x << 16) | virge->s3d.rdest_y;
                break;
                
                default:
                return s3_virge_mmio_read_w(addr, p) | (s3_virge_mmio_read_w(addr + 2, p) << 16);
        }
        return ret;
}
static void s3_virge_mmio_write(uint32_t addr, uint8_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
        
//        pclog("New MMIO writeb %08X %02X\n", addr, val);
        
        if ((addr & 0xfffc) < 0x8000)
                s3_virge_bitblt(virge, 8, val);
        else switch (addr & 0xffff)
        {
                case 0x83b0: case 0x83b1: case 0x83b2: case 0x83b3:
                case 0x83b4: case 0x83b5: case 0x83b6: case 0x83b7:
                case 0x83b8: case 0x83b9: case 0x83ba: case 0x83bb:
                case 0x83bc: case 0x83bd: case 0x83be: case 0x83bf:
                case 0x83c0: case 0x83c1: case 0x83c2: case 0x83c3:
                case 0x83c4: case 0x83c5: case 0x83c6: case 0x83c7:
                case 0x83c8: case 0x83c9: case 0x83ca: case 0x83cb:
                case 0x83cc: case 0x83cd: case 0x83ce: case 0x83cf:
                case 0x83d0: case 0x83d1: case 0x83d2: case 0x83d3:
                case 0x83d4: case 0x83d5: case 0x83d6: case 0x83d7:
                case 0x83d8: case 0x83d9: case 0x83da: case 0x83db:
                case 0x83dc: case 0x83dd: case 0x83de: case 0x83df:
                s3_virge_out(addr & 0x3ff, val, p);
                break;
        }

                
}
static void s3_virge_mmio_write_w(uint32_t addr, uint16_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
//        pclog("New MMIO writew %08X %04X\n", addr, val);
        if ((addr & 0xfffc) < 0x8000)
        {
                if (virge->s3d.cmd_set & CMD_SET_MS)
                        s3_virge_bitblt(virge, 16, ((val >> 8) | (val << 8)) << 16);
                else
                        s3_virge_bitblt(virge, 16, val);
        } 
        else switch (addr & 0xfffe)
        {
                case 0x83d4:
                s3_virge_mmio_write(addr, val, p);
                s3_virge_mmio_write(addr + 1, val >> 8, p);
                break;
        }
}
static void s3_virge_mmio_write_l(uint32_t addr, uint32_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
//        if ((addr & 0xfffc) >= 0x8000)
//                pclog("New MMIO writel %08X %08X\n", addr, val);

        if ((addr & 0xfffc) < 0x8000)
        {
                if (virge->s3d.cmd_set & CMD_SET_MS)
                        s3_virge_bitblt(virge, 32, ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24));
                else
                        s3_virge_bitblt(virge, 32, val);
        }
        else switch (addr & 0xfffc)
        {
                case 0x8180:
                virge->streams.pri_ctrl = val;
                s3_virge_recalctimings(svga);
                svga->fullchange = changeframecount;
                break;
                case 0x8184:
                virge->streams.chroma_ctrl = val;
                break;
                case 0x8190:
                virge->streams.sec_ctrl = val;
                break;
                case 0x8194:
                virge->streams.chroma_upper_bound = val;
                break;
                case 0x8198:
                virge->streams.sec_filter = val;
                break;
                case 0x81a0:
                virge->streams.blend_ctrl = val;
                break;
                case 0x81c0:
                virge->streams.pri_fb0 = val & 0x3fffff;
                s3_virge_recalctimings(svga);
                svga->fullchange = changeframecount;
                break;
                case 0x81c4:
                virge->streams.pri_fb1 = val & 0x3fffff;
                s3_virge_recalctimings(svga);
                svga->fullchange = changeframecount;
                break;
                case 0x81c8:
                virge->streams.pri_stride = val & 0xfff;
                s3_virge_recalctimings(svga);
                svga->fullchange = changeframecount;
                break;
                case 0x81cc:
                virge->streams.buffer_ctrl = val;
                s3_virge_recalctimings(svga);
                break;
                case 0x81d0:
                virge->streams.sec_fb0 = val;
                break;
                case 0x81d4:
                virge->streams.sec_fb1 = val;
                break;
                case 0x81d8:
                virge->streams.sec_stride = val;
                break;
                case 0x81dc:
                virge->streams.overlay_ctrl = val;
                break;
                case 0x81e0:
                virge->streams.k1_vert_scale = val;
                break;
                case 0x81e4:
                virge->streams.k2_vert_scale = val;
                break;
                case 0x81e8:
                virge->streams.dda_vert_accumulator = val;
                break;
                case 0x81ec:
                virge->streams.fifo_ctrl = val;
                break;
                case 0x81f0:
                virge->streams.pri_start = val;
                virge->streams.pri_x = (val >> 16) & 0x7ff;
                virge->streams.pri_y = val & 0x7ff;                
                s3_virge_recalctimings(svga);
                svga->fullchange = changeframecount;
                break;
                case 0x81f4:
                virge->streams.pri_size = val;
                virge->streams.pri_w = (val >> 16) & 0x7ff;
                virge->streams.pri_h = val & 0x7ff;                
                s3_virge_recalctimings(svga);
                svga->fullchange = changeframecount;
                break;
                case 0x81f8:
                virge->streams.sec_start = val;
                virge->streams.sec_x = (val >> 16) & 0x7ff;
                virge->streams.sec_y = val & 0x7ff;                
                break;
                case 0x81fc:
                virge->streams.sec_size = val;
                virge->streams.sec_w = (val >> 16) & 0x7ff;
                virge->streams.sec_h = val & 0x7ff;                
                break;
                
                case 0xa000: case 0xa004: case 0xa008: case 0xa00c:
                case 0xa010: case 0xa014: case 0xa018: case 0xa01c:
                case 0xa020: case 0xa024: case 0xa028: case 0xa02c:
                case 0xa030: case 0xa034: case 0xa038: case 0xa03c:
                case 0xa040: case 0xa044: case 0xa048: case 0xa04c:
                case 0xa050: case 0xa054: case 0xa058: case 0xa05c:
                case 0xa060: case 0xa064: case 0xa068: case 0xa06c:
                case 0xa070: case 0xa074: case 0xa078: case 0xa07c:
                case 0xa080: case 0xa084: case 0xa088: case 0xa08c:
                case 0xa090: case 0xa094: case 0xa098: case 0xa09c:
                case 0xa0a0: case 0xa0a4: case 0xa0a8: case 0xa0ac:
                case 0xa0b0: case 0xa0b4: case 0xa0b8: case 0xa0bc:
                case 0xa0c0: case 0xa0c4: case 0xa0c8: case 0xa0cc:
                case 0xa0d0: case 0xa0d4: case 0xa0d8: case 0xa0dc:
                case 0xa0e0: case 0xa0e4: case 0xa0e8: case 0xa0ec:
                case 0xa0f0: case 0xa0f4: case 0xa0f8: case 0xa0fc:
                case 0xa100: case 0xa104: case 0xa108: case 0xa10c:
                case 0xa110: case 0xa114: case 0xa118: case 0xa11c:
                case 0xa120: case 0xa124: case 0xa128: case 0xa12c:
                case 0xa130: case 0xa134: case 0xa138: case 0xa13c:
                case 0xa140: case 0xa144: case 0xa148: case 0xa14c:
                case 0xa150: case 0xa154: case 0xa158: case 0xa15c:
                case 0xa160: case 0xa164: case 0xa168: case 0xa16c:
                case 0xa170: case 0xa174: case 0xa178: case 0xa17c:
                case 0xa180: case 0xa184: case 0xa188: case 0xa18c:
                case 0xa190: case 0xa194: case 0xa198: case 0xa19c:
                case 0xa1a0: case 0xa1a4: case 0xa1a8: case 0xa1ac:
                case 0xa1b0: case 0xa1b4: case 0xa1b8: case 0xa1bc:
                case 0xa1c0: case 0xa1c4: case 0xa1c8: case 0xa1cc:
                case 0xa1d0: case 0xa1d4: case 0xa1d8: case 0xa1dc:
                case 0xa1e0: case 0xa1e4: case 0xa1e8: case 0xa1ec:
                case 0xa1f0: case 0xa1f4: case 0xa1f8: case 0xa1fc:
                {
                        int x = addr & 4;
                        int y = (addr >> 3) & 7;
                        virge->s3d.pattern_8[y*8 + x]     = val & 0xff;
                        virge->s3d.pattern_8[y*8 + x + 1] = val >> 8;
                        virge->s3d.pattern_8[y*8 + x + 2] = val >> 16;
                        virge->s3d.pattern_8[y*8 + x + 3] = val >> 24;
                        
                        x = (addr >> 1) & 6;
                        y = (addr >> 4) & 7;
                        virge->s3d.pattern_16[y*8 + x]     = val & 0xffff;
                        virge->s3d.pattern_16[y*8 + x + 1] = val >> 16;

                        x = (addr >> 2) & 7;
                        y = (addr >> 5) & 7;
                        virge->s3d.pattern_32[y*8 + x] = val & 0xffffff;
                }
                break;


                case 0xa4d4: case 0xa8d4:
                virge->s3d.src_base = val & 0x3ffff8;
                break;
                case 0xa4d8: case 0xa8d8:
                virge->s3d.dest_base = val & 0x3ffff8;
                break;
                case 0xa4dc: case 0xa8dc:
                virge->s3d.clip_l = (val >> 16) & 0x7ff;
                virge->s3d.clip_r = val & 0x7ff;
                break;
                case 0xa4e0: case 0xa8e0:
                virge->s3d.clip_t = (val >> 16) & 0x7ff;
                virge->s3d.clip_b = val & 0x7ff;
                break;
                case 0xa4e4: case 0xa8e4:
                virge->s3d.dest_str = (val >> 16) & 0xff8;
                virge->s3d.src_str = val & 0xff8;
                break;
                case 0xa4e8:
                virge->s3d.mono_pat_0 = val;
                break;
                case 0xa4ec:
                virge->s3d.mono_pat_1 = val;
                break;
                case 0xa4f0:
                virge->s3d.pat_bg_clr = val;
                break;
                case 0xa4f4: case 0xa8f4:
                virge->s3d.pat_fg_clr = val;
                break;
                case 0xa4f8:
                virge->s3d.src_bg_clr = val;
                break;
                case 0xa4fc:
                virge->s3d.src_fg_clr = val;
                break;
                case 0xa500: case 0xa900:
                virge->s3d.cmd_set = val;
                if (!(val & CMD_SET_AE))
                        s3_virge_bitblt(virge, -1, 0);
                break;
                case 0xa504:
                virge->s3d.r_width = (val >> 16) & 0x7ff;
                virge->s3d.r_height = val & 0x7ff;
                break;
                case 0xa508:
                virge->s3d.rsrc_x = (val >> 16) & 0x7ff;
                virge->s3d.rsrc_y = val & 0x7ff;
                break;
                case 0xa50c:
                virge->s3d.rdest_x = (val >> 16) & 0x7ff;
                virge->s3d.rdest_y = val & 0x7ff;
                if (virge->s3d.cmd_set & CMD_SET_AE)
                        s3_virge_bitblt(virge, -1, 0);
                break;
                case 0xa96c:
                virge->s3d.lxend0 = (val >> 16) & 0x7ff;
                virge->s3d.lxend1 = val & 0x7ff;
                break;
                case 0xa970:
                virge->s3d.ldx = (int32_t)val;
                break;
                case 0xa974:
                virge->s3d.lxstart = val;
                break;
                case 0xa978:
                virge->s3d.lystart = val & 0x7ff;
                break;
                case 0xa97c:
                virge->s3d.lycnt = val & 0x7ff;
                virge->s3d.line_dir = val >> 31;
                if (virge->s3d.cmd_set & CMD_SET_AE)
                        s3_virge_bitblt(virge, -1, 0);
                break;
        }
}

#define READ(addr, val)                                                                         \
        do                                                                                      \
        {                                                                                       \
                switch (bpp)                                                                    \
                {                                                                               \
                        case 0: /*8 bpp*/                                                       \
                        val = vram[addr & 0x3fffff];                                            \
                        break;                                                                  \
                        case 1: /*16 bpp*/                                                      \
                        val = *(uint16_t *)&vram[addr & 0x3fffff];                              \
                        break;                                                                  \
                        case 2: /*24 bpp*/                                                      \
                        val = (*(uint32_t *)&vram[addr & 0x3fffff]) & 0xffffff;                 \
                        break;                                                                  \
                }                                                                               \
        } while (0)

#define CLIP(x, y)                                              \
        do                                                      \
        {                                                       \
                if ((virge->s3d.cmd_set & CMD_SET_HC) &&        \
                    (x < virge->s3d.clip_l ||                   \
                     x > virge->s3d.clip_r ||                   \
                     y < virge->s3d.clip_t ||                   \
                     y > virge->s3d.clip_b))                    \
                        update = 0;                             \
        } while (0)

#define MIX()                                                   \
        do                                                      \
        {                                                       \
                int c;                                          \
                for (c = 0; c < 24; c++)                        \
                {                                               \
                        int d = (dest & (1 << c)) ? 1 : 0;      \
                        if (source & (1 << c))  d |= 2;         \
                        if (pattern & (1 << c)) d |= 4;         \
                        if (virge->s3d.rop & (1 << d)) out |= (1 << c);    \
                }                                               \
        } while (0)

#define WRITE(addr, val)                                                                        \
        do                                                                                      \
        {                                                                                       \
                switch (bpp)                                                                    \
                {                                                                               \
                        case 0: /*8 bpp*/                                                       \
                        vram[addr & 0x3fffff] = val;                                            \
                        virge->svga.changedvram[(addr & 0x3fffff) >> 12] = changeframecount;    \
                        break;                                                                  \
                        case 1: /*16 bpp*/                                                      \
                        *(uint16_t *)&vram[addr & 0x3fffff] = val;                              \
                        virge->svga.changedvram[(addr & 0x3fffff) >> 12] = changeframecount;    \
                        break;                                                                  \
                        case 2: /*24 bpp*/                                                      \
                        *(uint32_t *)&vram[addr & 0x3fffff] = (val & 0xffffff) |                \
                                                              (vram[(addr + 3) & 0x3fffff] << 24);  \
                        virge->svga.changedvram[(addr & 0x3fffff) >> 12] = changeframecount;    \
                        break;                                                                  \
                }                                                                               \
        } while (0)

static void s3_virge_bitblt(virge_t *virge, int count, uint32_t cpu_dat)
{
        int cpu_input = (count != -1);
        uint8_t *vram = virge->svga.vram;
        uint32_t mono_pattern[64];
        int count_mask;
        int x_inc = (virge->s3d.cmd_set & CMD_SET_XP) ? 1 : -1;
        int y_inc = (virge->s3d.cmd_set & CMD_SET_YP) ? 1 : -1;
        int bpp;
        int x_mul;
        int cpu_dat_shift;
        uint32_t *pattern_data;
        
        switch (virge->s3d.cmd_set & CMD_SET_FORMAT_MASK)
        {
                case CMD_SET_FORMAT_8:
                bpp = 0;
                x_mul = 1;
                cpu_dat_shift = 8;
                pattern_data = virge->s3d.pattern_8;
                break;
                case CMD_SET_FORMAT_16:
                bpp = 1;
                x_mul = 2;
                cpu_dat_shift = 16;
                pattern_data = virge->s3d.pattern_16;
                break;
                case CMD_SET_FORMAT_24:
                default:
                bpp = 2;
                x_mul = 3;
                cpu_dat_shift = 24;
                pattern_data = virge->s3d.pattern_32;
                break;
        }
        if (virge->s3d.cmd_set & CMD_SET_MP)
                pattern_data = mono_pattern;
        
        switch (virge->s3d.cmd_set & CMD_SET_ITA_MASK)
        {
                case CMD_SET_ITA_BYTE:
                count_mask = ~0x7;
                break;
                case CMD_SET_ITA_WORD:
                count_mask = ~0xf;
                break;
                case CMD_SET_ITA_DWORD:
                default:
                count_mask = ~0x1f;
                break;
        }
        if (virge->s3d.cmd_set & CMD_SET_MP)
        {
                int x, y;
                for (y = 0; y < 4; y++)
                {
                        for (x = 0; x < 8; x++)
                        {
                                if (virge->s3d.mono_pat_0 & (1 << (x + y*8)))
                                        mono_pattern[y*8 + x] = virge->s3d.pat_fg_clr;
                                else
                                        mono_pattern[y*8 + x] = virge->s3d.pat_bg_clr;
                                if (virge->s3d.mono_pat_1 & (1 << (x + y*8)))
                                        mono_pattern[(y+4)*8 + x] = virge->s3d.pat_fg_clr;
                                else
                                        mono_pattern[(y+4)*8 + x] = virge->s3d.pat_bg_clr;
                        }
                }
        }
        switch (virge->s3d.cmd_set & CMD_SET_COMMAND_MASK)
        {
                case CMD_SET_COMMAND_NOP:
                break;
                
                case CMD_SET_COMMAND_BITBLT:
                if (count == -1)
                {
                        virge->s3d.src_x = virge->s3d.rsrc_x;
                        virge->s3d.src_y = virge->s3d.rsrc_y;
                        virge->s3d.dest_x = virge->s3d.rdest_x;
                        virge->s3d.dest_y = virge->s3d.rdest_y;
                        virge->s3d.w = virge->s3d.r_width;
                        virge->s3d.h = virge->s3d.r_height;
                        virge->s3d.rop = (virge->s3d.cmd_set >> 17) & 0xff;
                        virge->s3d.data_left_count = 0;
                        
  /*                      pclog("BitBlt start %i,%i %i,%i %02X\n", virge->s3d.dest_x,
                                                                 virge->s3d.dest_y,
                                                                 virge->s3d.w,
                                                                 virge->s3d.h,
                                                                 virge->s3d.rop);*/
                        
                        if (virge->s3d.cmd_set & CMD_SET_IDS)
                                return;
                }
                if (!virge->s3d.h)
                        return;
                while (count)
                {
                        uint32_t src_addr = virge->s3d.src_base + (virge->s3d.src_x * x_mul) + (virge->s3d.src_y * virge->s3d.src_str);
                        uint32_t dest_addr = virge->s3d.dest_base + (virge->s3d.dest_x * x_mul) + (virge->s3d.dest_y * virge->s3d.dest_str);
                        uint32_t source, dest, pattern;
                        uint32_t out = 0;
                        int update = 1;

                        switch (virge->s3d.cmd_set & (CMD_SET_MS | CMD_SET_IDS))
                        {
                                case 0:
                                case CMD_SET_MS:
                                READ(src_addr, source);
                                if ((virge->s3d.cmd_set & CMD_SET_TP) && source == virge->s3d.src_fg_clr)
                                        update = 0;
                                break;
                                case CMD_SET_IDS:
                                if (virge->s3d.data_left_count)
                                {
                                        /*Handle shifting for 24-bit data*/
                                        source = virge->s3d.data_left;
                                        source |= ((cpu_dat << virge->s3d.data_left_count) & ~0xff000000);
                                        cpu_dat >>= (cpu_dat_shift - virge->s3d.data_left_count);
                                        count -= (cpu_dat_shift - virge->s3d.data_left_count);
                                        virge->s3d.data_left_count = 0;
                                        if (count < cpu_dat_shift)
                                        {
                                                virge->s3d.data_left = cpu_dat;
                                                virge->s3d.data_left_count = count;
                                                count = 0;
                                        }
                                }
                                else
                                {
                                        source = cpu_dat;
                                        cpu_dat >>= cpu_dat_shift;
                                        count -= cpu_dat_shift;
                                        if (count < cpu_dat_shift)
                                        {
                                                virge->s3d.data_left = cpu_dat;
                                                virge->s3d.data_left_count = count;
                                                count = 0;
                                        }
                                }
                                if ((virge->s3d.cmd_set & CMD_SET_TP) && source == virge->s3d.src_fg_clr)
                                        update = 0;
                                break;
                                case CMD_SET_IDS | CMD_SET_MS:
                                source = (cpu_dat & (1 << 31)) ? virge->s3d.src_fg_clr : virge->s3d.src_bg_clr;
                                if ((virge->s3d.cmd_set & CMD_SET_TP) && !(cpu_dat & (1 << 31)))
                                        update = 0;
                                cpu_dat <<= 1;
                                count--;
                                break;
                        }

                        CLIP(virge->s3d.dest_x, virge->s3d.dest_y);

                        if (update)
                        {
                                READ(dest_addr, dest);
                                pattern = pattern_data[(virge->s3d.dest_y & 7)*8 + (virge->s3d.dest_x & 7)];
                                MIX();

                                WRITE(dest_addr, out);
                        }
                
                        virge->s3d.src_x += x_inc;
                        virge->s3d.dest_x += x_inc;
                        if (!virge->s3d.w)
                        {
                                virge->s3d.src_x = virge->s3d.rsrc_x;
                                virge->s3d.dest_x = virge->s3d.rdest_x;
                                virge->s3d.w = virge->s3d.r_width;

                                virge->s3d.src_y += y_inc;
                                virge->s3d.dest_y += y_inc;
                                virge->s3d.h--;
                                
                                switch (virge->s3d.cmd_set & (CMD_SET_MS | CMD_SET_IDS))
                                {
                                        case CMD_SET_IDS:
                                        cpu_dat >>= (count - (count & count_mask));
                                        count &= count_mask;
                                        virge->s3d.data_left_count = 0;
                                        break;

                                        case CMD_SET_IDS | CMD_SET_MS:
                                        cpu_dat <<= (count - (count & count_mask));
                                        count &= count_mask;
                                        break;
                                }
                                if (!virge->s3d.h)
                                {
                                        return;
                                }
                        }
                        else
                                virge->s3d.w--;                        
                }
                break;
                
                case CMD_SET_COMMAND_RECTFILL:
                /*No source, pattern = pat_fg_clr*/
                if (count == -1)
                {
                        virge->s3d.src_x = virge->s3d.rsrc_x;
                        virge->s3d.src_y = virge->s3d.rsrc_y;
                        virge->s3d.dest_x = virge->s3d.rdest_x;
                        virge->s3d.dest_y = virge->s3d.rdest_y;
                        virge->s3d.w = virge->s3d.r_width;
                        virge->s3d.h = virge->s3d.r_height;
                        virge->s3d.rop = (virge->s3d.cmd_set >> 17) & 0xff;
                        
/*                        pclog("RctFll start %i,%i %i,%i %02X\n", virge->s3d.dest_x,
                                                                 virge->s3d.dest_y,
                                                                 virge->s3d.w,
                                                                 virge->s3d.h,
                                                                 virge->s3d.rop);*/
                }

                while (count)
                {
                        uint32_t dest_addr = virge->s3d.dest_base + (virge->s3d.dest_x * x_mul) + (virge->s3d.dest_y * virge->s3d.dest_str);
                        uint32_t source = 0, dest, pattern = virge->s3d.pat_fg_clr;
                        uint32_t out = 0;
                        int update = 1;

                        CLIP(virge->s3d.dest_x, virge->s3d.dest_y);

                        if (update)
                        {
                                READ(dest_addr, dest);

                                MIX();

                                WRITE(dest_addr, out);
                        }

                        virge->s3d.src_x += x_inc;
                        virge->s3d.dest_x += x_inc;
                        if (!virge->s3d.w)
                        {
                                virge->s3d.src_x = virge->s3d.rsrc_x;
                                virge->s3d.dest_x = virge->s3d.rdest_x;
                                virge->s3d.w = virge->s3d.r_width;

                                virge->s3d.src_y += y_inc;
                                virge->s3d.dest_y += y_inc;
                                virge->s3d.h--;
                                if (!virge->s3d.h)
                                {
                                        return;
                                }
                        }
                        else
                                virge->s3d.w--;                        
                        count--;
                }
                break;
                
                case CMD_SET_COMMAND_LINE:
                if (count == -1)
                {
                        virge->s3d.dest_x = virge->s3d.lxstart;
                        virge->s3d.dest_y = virge->s3d.lystart;
                        virge->s3d.h = virge->s3d.lycnt;
                        virge->s3d.rop = (virge->s3d.cmd_set >> 17) & 0xff;
                        if (virge->s3d.ldx >= 0)
                                virge->s3d.dest_x -= virge->s3d.ldx / 2;
                        else
                                virge->s3d.dest_x += virge->s3d.ldx / 2;
                        //virge->s3d.dest_dest_x = virge->s3d.dest_x + virge->s3d.ldx;
                }
                while (virge->s3d.h)
                {
                        int x = virge->s3d.dest_x >> 20;
                        int new_x = (virge->s3d.dest_x + virge->s3d.ldx) >> 20;
                        
                        do
                        {
                                uint32_t dest_addr = virge->s3d.dest_base + (x * x_mul) + (virge->s3d.dest_y * virge->s3d.dest_str);
                                uint32_t source = 0, dest, pattern;
                                uint32_t out = 0;
                                int update = 1;

                                CLIP(x, virge->s3d.dest_y);

                                if (update)
                                {
                                        READ(dest_addr, dest);
                                        pattern = virge->s3d.pat_fg_clr;

                                        MIX();

                                        WRITE(dest_addr, out);
                                }
                                
                                if (x < new_x)
                                        x++;
                                else if (x > new_x)
                                        x--;
                        } while (x != new_x);

                        virge->s3d.dest_x += virge->s3d.ldx;
                        virge->s3d.dest_y--;
                        virge->s3d.h--;
                }
                break;

                default:
                fatal("s3_virge_bitblt : blit command %i %08x\n", (virge->s3d.cmd_set >> 27) & 0xf, virge->s3d.cmd_set);
        }
}


static void s3_virge_hwcursor_draw(svga_t *svga, int displine)
{
        int x;
        uint16_t dat[2];
        int xx;
        int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
        
//        pclog("HWcursor %i %i\n", svga->hwcursor_latch.x, svga->hwcursor_latch.y);
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
//                                pclog("Plot %i, %i (%i %i) %04X %04X\n", offset, displine, x+xx, svga->hwcursor_on, dat[0], dat[1]);
                        }
                           
                        offset++;
                        dat[0] <<= 1;
                        dat[1] <<= 1;
                }
                svga->hwcursor_latch.addr += 4;
        }
}

static uint8_t s3_virge_pci_read(int func, int addr, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
        uint8_t ret = 0;
//        pclog("S3 PCI read %08X  ", addr);
        switch (addr)
        {
                case 0x00: ret = 0x33; break; /*'S3'*/
                case 0x01: ret = 0x53; break;
                
                case 0x02: ret = virge->virge_id_low; break;
                case 0x03: ret = virge->virge_id_high; break;

                case 0x04: ret = virge->pci_regs[0x04] & 0x27; break;
                
                case 0x07: ret = virge->pci_regs[0x07] & 0x36; break;
                                
                case 0x08: ret = 0; break; /*Revision ID*/
                case 0x09: ret = 0; break; /*Programming interface*/
                
                case 0x0a: ret = 0x00; break; /*Supports VGA interface*/
                case 0x0b: ret = 0x03; /*output = 3; */break;

                case 0x0d: ret = virge->pci_regs[0x0d] & 0xf8; break;
                                
                case 0x10: ret = 0x00; break;/*Linear frame buffer address*/
                case 0x11: ret = 0x00; break;
                case 0x12: ret = 0x00; break;
                case 0x13: ret = svga->crtc[0x59] & 0xfc; break;

                case 0x30: ret = virge->pci_regs[0x30] & 0x01; break; /*BIOS ROM address*/
                case 0x31: ret = 0x00; break;
                case 0x32: ret = virge->pci_regs[0x32]; break;
                case 0x33: ret = virge->pci_regs[0x33]; break;

                case 0x3c: ret = virge->pci_regs[0x3c]; break;
                                
                case 0x3d: ret = 0x01; break; /*INTA*/
                
                case 0x3e: ret = 0x04; break;
                case 0x3f: ret = 0xff; break;
                
        }
//        pclog("%02X\n", ret);
        return ret;
}

static void s3_virge_pci_write(int func, int addr, uint8_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
//        pclog("S3 PCI write %08X %02X %04X:%08X\n", addr, val, CS, pc);
        switch (addr)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                case 0x3d: case 0x3e: case 0x3f:
                return;
                
                case PCI_REG_COMMAND:
                if (val & PCI_COMMAND_IO)
                {
                        io_removehandler(0x03c0, 0x0020, s3_virge_in, NULL, NULL, s3_virge_out, NULL, NULL, virge);
                        io_sethandler(0x03c0, 0x0020, s3_virge_in, NULL, NULL, s3_virge_out, NULL, NULL, virge);
                }
                else
                        io_removehandler(0x03c0, 0x0020, s3_virge_in, NULL, NULL, s3_virge_out, NULL, NULL, virge);
                virge->pci_regs[PCI_REG_COMMAND] = val & 0x27;
                return;
                case 0x07:
                virge->pci_regs[0x07] = val & 0x3e;
                return;
                case 0x0d: 
                virge->pci_regs[0x0d] = val & 0xf8;
                return;
                
                case 0x13: 
                svga->crtc[0x59] = val & 0xfc; 
                s3_virge_updatemapping(virge); 
                return;

                case 0x30: case 0x32: case 0x33:
                virge->pci_regs[addr] = val;
                if (virge->pci_regs[0x30] & 0x01)
                {
                        uint32_t addr = (virge->pci_regs[0x32] << 16) | (virge->pci_regs[0x33] << 24);
                        pclog("Virge bios_rom enabled at %08x\n", addr);
                        mem_mapping_set_addr(&virge->bios_rom.mapping, addr, 0x8000);
                        mem_mapping_enable(&virge->bios_rom.mapping);
                }
                else
                {
                        pclog("Virge bios_rom disabled\n");
                        mem_mapping_disable(&virge->bios_rom.mapping);
                }
                return;
                case 0x3c: 
                virge->pci_regs[0x3c] = val;
                return;
        }
}

static void *s3_virge_init()
{
        virge_t *virge = malloc(sizeof(virge_t));
        memset(virge, 0, sizeof(virge_t));
        
        svga_init(&virge->svga, virge, 1 << 22, /*4mb*/
                   s3_virge_recalctimings,
                   s3_virge_in, s3_virge_out,
                   s3_virge_hwcursor_draw);

        rom_init(&virge->bios_rom, "roms/s3virge.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        if (PCI)
                mem_mapping_disable(&virge->bios_rom.mapping);

        mem_mapping_add(&virge->mmio_mapping,     0, 0, s3_virge_mmio_read,
                                                        s3_virge_mmio_read_w,
                                                        s3_virge_mmio_read_l,
                                                        s3_virge_mmio_write,
                                                        s3_virge_mmio_write_w,
                                                        s3_virge_mmio_write_l,
                                                        NULL,
                                                        0,
                                                        virge);
        mem_mapping_add(&virge->new_mmio_mapping, 0, 0, s3_virge_mmio_read,
                                                        s3_virge_mmio_read_w,
                                                        s3_virge_mmio_read_l,
                                                        s3_virge_mmio_write,
                                                        s3_virge_mmio_write_w,
                                                        s3_virge_mmio_write_l,
                                                        NULL,
                                                        0,
                                                        virge);
        mem_mapping_add(&virge->linear_mapping,   0, 0, svga_read_linear,
                                                        svga_readw_linear,
                                                        svga_readl_linear,
                                                        svga_write_linear,
                                                        svga_writew_linear,
                                                        svga_writel_linear,
                                                        NULL,
                                                        0,
                                                        &virge->svga);

        io_sethandler(0x03c0, 0x0020, s3_virge_in, NULL, NULL, s3_virge_out, NULL, NULL, virge);

        virge->pci_regs[4] = 3;
        virge->pci_regs[5] = 0;        
        virge->pci_regs[6] = 0;
        virge->pci_regs[7] = 2;
        virge->pci_regs[0x32] = 0x0c;
        virge->pci_regs[0x3d] = 1; 
        virge->pci_regs[0x3e] = 4;
        virge->pci_regs[0x3f] = 0xff;
        
        virge->virge_id_high = 0x56;
        virge->virge_id_low = 0x31;
        virge->virge_rev = 0;
        virge->virge_id = 0xe1;

        virge->svga.crtc[0x36] = 2 | (0 << 2) | (1 << 4);
        virge->svga.crtc[0x37] = 1;// | (7 << 5);
        virge->svga.crtc[0x53] = 1 << 3;
        virge->svga.crtc[0x59] = 0x70;
        
        pci_add(s3_virge_pci_read, s3_virge_pci_write, virge);
 
        return virge;
}

static void s3_virge_close(void *p)
{
        virge_t *virge = (virge_t *)p;

        svga_close(&virge->svga);
        
        free(virge);
}

static int s3_virge_available()
{
        return rom_present("roms/s3virge.bin");
}

static void s3_virge_speed_changed(void *p)
{
        virge_t *virge = (virge_t *)p;
        
        svga_recalctimings(&virge->svga);
}

static void s3_virge_force_redraw(void *p)
{
        virge_t *virge = (virge_t *)p;

        virge->svga.fullchange = changeframecount;
}

static int s3_virge_add_status_info(char *s, int max_len, void *p)
{
        virge_t *virge = (virge_t *)p;
        
        return svga_add_status_info(s, max_len, &virge->svga);
}

device_t s3_virge_device =
{
        "Diamond Stealth 3D 2000 (S3 VIRGE)",
        DEVICE_NOT_WORKING,
        s3_virge_init,
        s3_virge_close,
        s3_virge_available,
        s3_virge_speed_changed,
        s3_virge_force_redraw,
        s3_virge_add_status_info
};
