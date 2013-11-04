/*S3 ViRGE emulation

  The SVGA core is largely the same as older S3 chips, but the blitter is totally different*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "video.h"
#include "vid_s3_virge.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
//#include "vid_sdac_ramdac.h"

typedef struct virge_t
{
        mem_mapping_t linear_mapping;
        mem_mapping_t   mmio_mapping;
        
        svga_t svga;
        
        uint8_t bank;
        uint8_t ma_ext;
        int width;
        int bpp;

        uint8_t virge_id, virge_id_high, virge_id_low, virge_rev;

        uint32_t linear_base, linear_size;

        uint8_t pci_regs[256];
} virge_t;

void s3_virge_updatemapping(virge_t *virge);

uint8_t  s3_virge_mmio_read(uint32_t addr, void *p);
uint16_t s3_virge_mmio_read_w(uint32_t addr, void *p);
uint32_t s3_virge_mmio_read_l(uint32_t addr, void *p);
void     s3_virge_mmio_write(uint32_t addr, uint8_t val, void *p);
void     s3_virge_mmio_write_w(uint32_t addr, uint16_t val, void *p);
void     s3_virge_mmio_write_l(uint32_t addr, uint32_t val, void *p);

void s3_virge_out(uint16_t addr, uint8_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
        uint8_t old;

        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;
        
        pclog("S3 out %04X %02X %04X:%08X  %04X %04X %i\n", addr, val, CS, pc, ES, BX, ins);

        switch (addr)
        {
                case 0x3c5:
                if (svga->seqaddr >= 0x10)
                {
                        svga->seqregs[svga->seqaddr & 0x1f]=val;
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
//                        if (crtcreg == 0x67) pclog("Write CRTC R%02X %02X\n", crtcreg, val);
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
                        case 0x48:
                        svga->hwcursor.x = ((svga->crtc[0x46] << 8) | svga->crtc[0x47]) & 0x7ff;
                        if (svga->bpp == 32) svga->hwcursor.x >>= 1;
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

uint8_t s3_virge_in(uint16_t addr, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
        
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

        if (addr != 0x3da) pclog("S3 in %04X %04X:%08X\n", addr, CS, pc);
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
//                pclog("Read CRTC R%02X %04X:%04X\n", crtcreg, CS, pc);
                switch (svga->crtcreg)
                {
                        case 0x2d: return virge->virge_id_high; /*Extended chip ID*/
                        case 0x2e: return virge->virge_id_low;  /*New chip ID*/
                        case 0x2f: return virge->virge_rev;
                        case 0x30: return virge->virge_id;      /*Chip ID*/
                        case 0x31: return (svga->crtc[0x31] & 0xcf) | ((virge->ma_ext & 3) << 4);
                        case 0x35: return (svga->crtc[0x35] & 0xf0) | (virge->bank & 0xf);
                        case 0x51: return (svga->crtc[0x51] & 0xf0) | ((virge->bank >> 2) & 0xc) | ((virge->ma_ext >> 2) & 3);
                        case 0x69: return virge->ma_ext;
                        case 0x6a: return virge->bank;
                }
                return svga->crtc[svga->crtcreg];
        }
        return svga_in(addr, svga);
}

void s3_virge_recalctimings(svga_t *svga)
{
        virge_t *virge = (virge_t *)svga->p;
//        pclog("recalctimings\n");
        svga->ma_latch |= (virge->ma_ext << 16);
//        pclog("SVGA_MA %08X\n", svga_ma);
//        if (gdcreg[5] & 0x40) svga_lowres = !(crtc[0x3a] & 0x10);
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
        
        if (svga->crtc[0x5d] & 0x01) svga->htotal     += 0x100;
        if (svga->crtc[0x5d] & 0x02) svga->hdisp      += 0x100;
        if (svga->crtc[0x5e] & 0x01) svga->vtotal     += 0x400;
        if (svga->crtc[0x5e] & 0x02) svga->dispend    += 0x400;
        if (svga->crtc[0x5e] & 0x10) svga->vsyncstart += 0x400;
        if (svga->crtc[0x5e] & 0x40) svga->split      += 0x400;
        if (svga->crtc[0x51] & 0x30)      svga->rowoffset += (svga->crtc[0x51] & 0x30) << 4;
        else if (svga->crtc[0x43] & 0x04) svga->rowoffset += 0x100;
        if (!svga->rowoffset) svga->rowoffset = 256;
        svga->interlace = svga->crtc[0x42] & 0x20;
        if (svga->bpp == 32)
        {
                svga->htotal <<= 2;
                svga->hdisp <<= 2;
        }
        //svga_clock = cpuclock / sdac_getclock((svga_miscout >> 2) & 3);
//        pclog("SVGA_CLOCK = %f  %02X  %f\n", svga_clock, svga_miscout, cpuclock);
        //if (bpp > 8) svga_clock /= 2;
}

void s3_virge_updatemapping(virge_t *virge)
{
        svga_t *svga = &virge->svga;
        
        pclog("Update mapping - bank %02X ", svga->gdcreg[6] & 0xc);        
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
        
        pclog("Linear framebuffer %02X ", svga->crtc[0x58] & 0x10);
        if (svga->crtc[0x58] & 0x10) /*Linear framebuffer*/
        {
                virge->linear_base = (svga->crtc[0x5a] << 16) | (svga->crtc[0x59] << 24);
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
                        mem_mapping_set_addr(&svga->mapping, virge->linear_base, virge->linear_size);
        }
        else
                mem_mapping_disable(&virge->linear_mapping);
        
        pclog("Memory mapped IO %02X\n", svga->crtc[0x53] & 0x18);
        if ((svga->crtc[0x53] & 0x18) == 0x10) /*Memory mapped IO*/
        {
                if (svga->crtc[0x53] & 0x20)
                        mem_mapping_set_addr(&virge->mmio_mapping, 0xb8000, 0x8000);
                else
                        mem_mapping_set_addr(&virge->mmio_mapping, 0xa8000, 0x8000);
        }
        else
                mem_mapping_disable(&virge->mmio_mapping);
}


uint8_t s3_virge_mmio_read(uint32_t addr, void *p)
{
        pclog("MMIO readb %08X\n", addr);
        return 0xff;
}
uint16_t s3_virge_mmio_read_w(uint32_t addr, void *p)
{
        pclog("MMIO readw %08X\n", addr);
        return 0xffff;
}
uint32_t s3_virge_mmio_read_l(uint32_t addr, void *p)
{
        pclog("MMIO readl %08X\n", addr);
        return 0xffffffff;
}
void s3_virge_mmio_write(uint32_t addr, uint8_t val, void *p)
{
        pclog("MMIO writeb %08X %02X\n", addr, val);
}
void     s3_virge_mmio_write_w(uint32_t addr, uint16_t val, void *p)
{
        pclog("MMIO writew %08X %04X\n", addr, val);
}
void     s3_virge_mmio_write_l(uint32_t addr, uint32_t val, void *p)
{
        pclog("MMIO writel %08X %08X\n", addr, val);
}



void s3_virge_hwcursor_draw(svga_t *svga, int displine)
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
//                                pclog("Plot %i, %i (%i %i) %04X %04X\n", offset, displine, x+xx, svga->hwcursor_on, dat[0], dat[1]);
                        }
                           
                        offset++;
                        dat[0] <<= 1;
                        dat[1] <<= 1;
                }
                svga->hwcursor_latch.addr += 4;
        }
}


uint8_t s3_virge_pci_read(int func, int addr, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;
//        pclog("S3 PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x33; /*'S3'*/
                case 0x01: return 0x53;
                
                case 0x02: return virge->virge_id_low;
                case 0x03: return virge->virge_id_high;
                
                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x0a: return 0x00; /*Supports VGA interface*/
                case 0x0b: return 0x03;
                
                case 0x10: return 0x00; /*Linear frame buffer address*/
                case 0x11: return 0x00;
                case 0x12: return 0x00;
                case 0x13: return svga->crtc[0x59] & 0xfc;

                case 0x30: return 0x01; /*BIOS ROM address*/
                case 0x31: return 0x00;
                case 0x32: return 0x0C;
                case 0x33: return 0x00;
                
        }
        return virge->pci_regs[addr];
}

void s3_virge_pci_write(int func, int addr, uint8_t val, void *p)
{
        virge_t *virge = (virge_t *)p;
        svga_t *svga = &virge->svga;

        switch (addr)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                case 0x3d: case 0x3e: case 0x3f:
                break;
                
                case 0x13: 
                svga->crtc[0x59] = val & 0xfc; 
                s3_virge_updatemapping(virge); 
                break;
        }
        virge->pci_regs[addr] = val;
}

void *s3_virge_init()
{
        virge_t *virge = malloc(sizeof(virge_t));
        memset(virge, 0, sizeof(virge_t));

        svga_init(&virge->svga, virge, 1 << 22, /*4mb*/
                   s3_virge_recalctimings,
                   s3_virge_in, s3_virge_out,
                   s3_virge_hwcursor_draw);

        mem_mapping_add(&virge->mmio_mapping,   0, 0, s3_virge_mmio_read, s3_virge_mmio_read_w, s3_virge_mmio_read_l, s3_virge_mmio_write, s3_virge_mmio_write_w, s3_virge_mmio_write_l, virge);
        mem_mapping_add(&virge->linear_mapping, 0, 0, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear, &virge->svga);

        io_sethandler(0x03c0, 0x0020, s3_virge_in, NULL, NULL, s3_virge_out, NULL, NULL, virge);

        virge->pci_regs[4] = 3;
        virge->pci_regs[5] = 0;        
        virge->pci_regs[6] = 0;
        virge->pci_regs[7] = 2;
        virge->pci_regs[0x3d] = 1; 
        virge->pci_regs[0x3e] = 4;
        virge->pci_regs[0x3f] = 0xff;
        
        virge->virge_id_high = 0x56;
        virge->virge_id_low = 0x31;
        virge->virge_rev = 0;
        virge->virge_id = 0xe1;

        virge->svga.crtc[0x36] = 1 | (2 << 2) | (1 << 4) | (4 << 5);
        virge->svga.crtc[0x37] = 1 | (7 << 5);
        
        pci_add(s3_virge_pci_read, s3_virge_pci_write, virge);
 
        return virge;
}

void s3_virge_close(void *p)
{
        virge_t *virge = (virge_t *)p;

        svga_close(&virge->svga);
        
        free(virge);
}

void s3_virge_speed_changed(void *p)
{
        virge_t *virge = (virge_t *)p;
        
        svga_recalctimings(&virge->svga);
}

void s3_virge_force_redraw(void *p)
{
        virge_t *virge = (virge_t *)p;

        virge->svga.fullchange = changeframecount;
}

device_t s3_virge_device =
{
        "Diamond Stealth 3D 2000 (S3 VIRGE)",
        s3_virge_init,
        s3_virge_close,
        NULL,
        s3_virge_speed_changed,
        s3_virge_force_redraw,
        svga_add_status_info
};
