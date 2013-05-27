/*S3 ViRGE emulation

  The SVGA core is largely the same as older S3 chips, but the blitter is totally different*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
//#include "vid_sdac_ramdac.h"

void s3_virge_updatemapping();

static uint8_t s3_bank;
static uint8_t s3_ma_ext;
static int s3_width = 1024;
static int s3_bpp = 0;

static uint8_t s3_virge_id, s3_virge_id_high, s3_virge_id_low, s3_virge_rev;

uint8_t  s3_virge_mmio_read(uint32_t addr, void *priv);
uint16_t s3_virge_mmio_read_w(uint32_t addr, void *priv);
uint32_t s3_virge_mmio_read_l(uint32_t addr, void *priv);
void     s3_virge_mmio_write(uint32_t addr, uint8_t val, void *priv);
void     s3_virge_mmio_write_w(uint32_t addr, uint16_t val, void *priv);
void     s3_virge_mmio_write_l(uint32_t addr, uint32_t val, void *priv);

void s3_virge_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
        pclog("S3 out %04X %02X %04X:%08X  %04X %04X %i\n", addr, val, CS, pc, ES, BX, ins);

        switch (addr)
        {
                case 0x3c5:
                if (seqaddr >= 0x10)
                {
                        seqregs[seqaddr&0x1F]=val;
                        return;
                }
                if (seqaddr == 4) /*Chain-4 - update banking*/
                {
                        if (val & 8) svgawbank = svgarbank = s3_bank << 16;
                        else         svgawbank = svgarbank = s3_bank << 14;
                }
                break;
                
                //case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Write RAMDAC %04X %02X %04X:%04X\n", addr, val, CS, pc);
                //sdac_ramdac_out(addr,val);
                //return;

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
                        s3_virge_updatemapping();
                        break;
                        
                        case 0x67:
                        switch (val >> 4)
                        {
                                case 3:  bpp = 15; break;
                                case 5:  bpp = 16; break;
                                case 7:  bpp = 24; break;
                                case 13: bpp = 32; break;
                                default: bpp = 8;  break;
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
        svga_out(addr, val, NULL);
}

uint8_t s3_virge_in(uint16_t addr, void *priv)
{
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        if (addr != 0x3da) pclog("S3 in %04X %04X:%08X\n", addr, CS, pc);
        switch (addr)
        {
                //case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
//                pclog("Read RAMDAC %04X  %04X:%04X\n", addr, CS, pc);
                //return sdac_ramdac_in(addr);

                case 0x3c5:
                if (seqaddr >= 0x10)
                   return seqregs[seqaddr&0x1F];
                break;

                case 0x3D4:
                return crtcreg;
                case 0x3D5:
//                pclog("Read CRTC R%02X %04X:%04X\n", crtcreg, CS, pc);
                switch (crtcreg)
                {
                        case 0x2d: return s3_virge_id_high; /*Extended chip ID*/
                        case 0x2e: return s3_virge_id_low;  /*New chip ID*/
                        case 0x2f: return s3_virge_rev;
                        case 0x30: return s3_virge_id;      /*Chip ID*/
                        case 0x31: return (crtc[0x31] & 0xcf) | ((s3_ma_ext & 3) << 4);
                        case 0x35: return (crtc[0x35] & 0xf0) | (s3_bank & 0xf);
                        case 0x51: return (crtc[0x51] & 0xf0) | ((s3_bank >> 2) & 0xc) | ((s3_ma_ext >> 2) & 3);
                        case 0x69: return s3_ma_ext;
                        case 0x6a: return s3_bank;
                }
                return crtc[crtcreg];
        }
        return svga_in(addr, NULL);
}

void s3_virge_recalctimings()
{
//        pclog("recalctimings\n");
        svga_ma |= (s3_ma_ext << 16);
//        pclog("SVGA_MA %08X\n", svga_ma);
//        if (gdcreg[5] & 0x40) svga_lowres = !(crtc[0x3a] & 0x10);
        if ((gdcreg[5] & 0x40) && (crtc[0x3a] & 0x10))
        {
                switch (bpp)
                {
                        case 8: 
                        svga_render = svga_render_8bpp_highres; 
                        break;
                        case 15: 
                        svga_render = svga_render_15bpp_highres; 
                        break;
                        case 16: 
                        svga_render = svga_render_16bpp_highres; 
                        break;
                        case 24: 
                        svga_render = svga_render_24bpp_highres; 
                        break;
                        case 32: 
                        svga_render = svga_render_32bpp_highres; 
                        break;
                }
        }
        
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
        if (bpp == 32)
        {
                svga_htotal <<= 2;
                svga_hdisp <<= 2;
        }
        //svga_clock = cpuclock / sdac_getclock((svga_miscout >> 2) & 3);
//        pclog("SVGA_CLOCK = %f  %02X  %f\n", svga_clock, svga_miscout, cpuclock);
        //if (bpp > 8) svga_clock /= 2;
}

static uint32_t s3_linear_base = 0, s3_linear_size = 0;
void s3_virge_updatemapping()
{
        mem_removehandler(s3_linear_base, s3_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear, NULL);
        
//        video_write_a000_w = video_write_a000_l = NULL;

        mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
        mem_removehandler(0xa0000, 0x20000, s3_virge_mmio_read, s3_virge_mmio_read_w, s3_virge_mmio_read_l, s3_virge_mmio_write, s3_virge_mmio_write_w, s3_virge_mmio_write_l, NULL);
        pclog("Update mapping - bank %02X ", gdcreg[6] & 0xc);        
        switch (gdcreg[6] & 0xc) /*Banked framebuffer*/
        {
                case 0x0: /*128k at A0000*/
                mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                break;
                case 0x4: /*64k at A0000*/
                mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                break;
                case 0x8: /*32k at B0000*/
                mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                break;
                case 0xC: /*32k at B8000*/
                mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                break;
        }
        
        pclog("Linear framebuffer %02X ", crtc[0x58] & 0x10);
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
                        s3_linear_size = 0x400000;
                        break;
                }
                s3_linear_base &= ~(s3_linear_size - 1);
//                pclog("%08X %08X  %02X %02X %02X\n", linear_base, linear_size, crtc[0x58], crtc[0x59], crtc[0x5a]);
                pclog("Linear framebuffer at %08X size %08X\n", s3_linear_base, s3_linear_size);
                if (s3_linear_base == 0xa0000)
                   mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                else
                   mem_sethandler(s3_linear_base, s3_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear, NULL);
        }
        
        pclog("Memory mapped IO %02X\n", crtc[0x53] & 0x18);
        output = 0;
        if ((crtc[0x53] & 0x18) == 0x10) /*Memory mapped IO*/
        {
                output = 3;
                if (crtc[0x53] & 0x20)
                   mem_sethandler(0xb8000, 0x8000, s3_virge_mmio_read, s3_virge_mmio_read_w, s3_virge_mmio_read_l, s3_virge_mmio_write, s3_virge_mmio_write_w, s3_virge_mmio_write_l, NULL);
                else
                   mem_sethandler(0xa8000, 0x8000, s3_virge_mmio_read, s3_virge_mmio_read_w, s3_virge_mmio_read_l, s3_virge_mmio_write, s3_virge_mmio_write_w, s3_virge_mmio_write_l, NULL);
        }

}


uint8_t s3_virge_mmio_read(uint32_t addr, void *priv)
{
        pclog("MMIO readb %08X\n", addr);
        return 0xff;
}
uint16_t s3_virge_mmio_read_w(uint32_t addr, void *priv)
{
        pclog("MMIO readw %08X\n", addr);
        return 0xffff;
}
uint32_t s3_virge_mmio_read_l(uint32_t addr, void *priv)
{
        pclog("MMIO readl %08X\n", addr);
        return 0xffffffff;
}
void s3_virge_mmio_write(uint32_t addr, uint8_t val, void *priv)
{
        pclog("MMIO writeb %08X %02X\n", addr, val);
}
void     s3_virge_mmio_write_w(uint32_t addr, uint16_t val, void *priv)
{
        pclog("MMIO writew %08X %04X\n", addr, val);
}
void     s3_virge_mmio_write_l(uint32_t addr, uint32_t val, void *priv)
{
        pclog("MMIO writel %08X %08X\n", addr, val);
}



void s3_virge_hwcursor_draw(int displine)
{
        int x;
        uint16_t dat[2];
        int xx;
        int offset = svga_hwcursor_latch.x - svga_hwcursor_latch.xoff;
        
        pclog("HWcursor %i %i\n", svga_hwcursor_latch.x, svga_hwcursor_latch.y);
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


static uint8_t s3_virge_pci_regs[256];

uint8_t s3_virge_pci_read(int func, int addr, void *priv)
{
//        pclog("S3 PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x33; /*'S3'*/
                case 0x01: return 0x53;
                
                case 0x02: return s3_virge_id_low;
                case 0x03: return s3_virge_id_high;
                
                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x0a: return 0x00; /*Supports VGA interface*/
                case 0x0b: return 0x03;
                
                case 0x10: return 0x00; /*Linear frame buffer address*/
                case 0x11: return 0x00;
                case 0x12: return 0x00;
                case 0x13: return crtc[0x59] & 0xfc;

                case 0x30: return 0x01; /*BIOS ROM address*/
                case 0x31: return 0x00;
                case 0x32: return 0x0C;
                case 0x33: return 0x00;
                
        }
        return s3_virge_pci_regs[addr];
}

void s3_virge_pci_write(int func, int addr, uint8_t val, void *priv)
{
        switch (addr)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                case 0x3d: case 0x3e: case 0x3f:
                break;
                
                case 0x13: crtc[0x59] = val & 0xfc; s3_virge_updatemapping(); break;                
        }
        s3_virge_pci_regs[addr] = val;
}

int s3_virge_init()
{
        s3_virge_pci_regs[4] = 3;
        s3_virge_pci_regs[5] = 0;        
        s3_virge_pci_regs[6] = 0;
        s3_virge_pci_regs[7] = 2;
        s3_virge_pci_regs[0x3d] = 1; 
        s3_virge_pci_regs[0x3e] = 4;
        s3_virge_pci_regs[0x3f] = 0xff;
        
        s3_virge_id_high = 0x56;
        s3_virge_id_low = 0x31;
        s3_virge_rev = 0;
        s3_virge_id = 0xe1;

        svga_recalctimings_ex = s3_virge_recalctimings;
        svga_hwcursor_draw    = s3_virge_hwcursor_draw;

        crtc[0x36] = 1 | (2 << 2) | (1 << 4) | (4 << 5);
        crtc[0x37] = 1 | (7 << 5);
        
        vrammask = 0x3fffff;

        pci_add(s3_virge_pci_read, s3_virge_pci_write, NULL);
 
        svga_vram_limit = 4 << 20; /*4mb*/
        return svga_init();
}

GFXCARD vid_s3_virge =
{
        s3_virge_init,
        /*IO at 3Cx/3Dx*/
        s3_virge_out,
        s3_virge_in,
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

