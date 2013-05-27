/*ET4000/W32p emulation (Diamond Stealth 32)*/
/*Known bugs :

  - Accelerator doesn't work in planar modes
*/

#include "ibm.h"
#include "io.h"
#include "pci.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_icd2061.h"
#include "vid_stg_ramdac.h"
#include "mem.h"

int et4k_b8000;

void et4000w32p_recalcmapping();

uint8_t et4000w32p_mmu_read(uint32_t addr, void *priv);
void et4000w32p_mmu_write(uint32_t addr, uint8_t val, void *priv);

static int et4000w32p_index;
static uint8_t et4000w32p_regs[256];
static uint32_t et4000w32p_linearbase, et4000w32p_linearbase_old;

void et4000w32p_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;

//        if (!(addr==0x3D4 && (val&~1)==0xE) && !(addr==0x3D5 && (crtcreg&~1)==0xE)) pclog("ET4000W32p out %04X %02X  %04X:%04X  ",addr,val,CS,pc);
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
//        if (!(addr==0x3D4 && (val&~1)==0xE) && !(addr==0x3D5 && (crtcreg&~1)==0xE)) pclog("%04X\n",addr);
        
        switch (addr)
        {
                case 0x3c2:
                icd2061_write((val >> 2) & 3);
                break;
                
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                stg_ramdac_out(addr, val, NULL);
                return;
                
                case 0x3CB: /*Banking extension*/
                svgawbank=(svgawbank&0xFFFFF)|((val&1)<<20);
                svgarbank=(svgarbank&0xFFFFF)|((val&0x10)<<16);
                svgaseg2=val;
                return;
                case 0x3CD: /*Banking*/
                svgawbank=(svgawbank&0x100000)|((val&0xF)*65536);
                svgarbank=(svgarbank&0x100000)|(((val>>4)&0xF)*65536);
                svgaseg=val;
                return;
                case 0x3CF:
                switch (gdcaddr&15)
                {
                        case 6:
                        gdcreg[gdcaddr&15]=val;
                        //et4k_b8000=((crtc[0x36]&0x38)==0x28) && ((gdcreg[6]&0xC)==4);
                        et4000w32p_recalcmapping();
                        return;
                }
                break;
                case 0x3D4:
                crtcreg=val&63;
                return;
                case 0x3D5:
//                pclog("Write CRTC R%02X %02X\n", crtcreg, val);
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;
                et4k_b8000=((crtc[0x36]&0x38)==0x28) && ((gdcreg[6]&0xC)==4);
//                if (crtcreg!=0xE && crtcreg!=0xF) pclog("CRTC R%02X = %02X\n",crtcreg,val);
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                svga_recalctimings();
                        }
                }
                if (crtcreg == 0x30)
                {
                        et4000w32p_linearbase = val * 0x400000;
//                        pclog("Linear base now at %08X %02X\n", et4000w32p_linearbase, val);
                        et4000w32p_recalcmapping();
                }
                if (crtcreg == 0x36) et4000w32p_recalcmapping();
                break;
                case 0x3D8:
                if (val==0xA0) svgaon=1;
                if (val==0x29) svgaon=0;
                break;

                case 0x210A: case 0x211A: case 0x212A: case 0x213A:
                case 0x214A: case 0x215A: case 0x216A: case 0x217A:
                et4000w32p_index=val;
                return;
                case 0x210B: case 0x211B: case 0x212B: case 0x213B:
                case 0x214B: case 0x215B: case 0x216B: case 0x217B:
                et4000w32p_regs[et4000w32p_index] = val;
                svga_hwcursor.x     = et4000w32p_regs[0xE0] | ((et4000w32p_regs[0xE1] & 7) << 8);
                svga_hwcursor.y     = et4000w32p_regs[0xE4] | ((et4000w32p_regs[0xE5] & 7) << 8);
                svga_hwcursor.addr  = (et4000w32p_regs[0xE8] | (et4000w32p_regs[0xE9] << 8) | ((et4000w32p_regs[0xEA] & 7) << 16)) << 2;
                svga_hwcursor.addr += (et4000w32p_regs[0xE6] & 63) * 16;
                svga_hwcursor.ena   = et4000w32p_regs[0xF7] & 0x80;
                svga_hwcursor.xoff  = et4000w32p_regs[0xE2] & 63;
                svga_hwcursor.yoff  = et4000w32p_regs[0xE6] & 63;
//                pclog("HWCURSOR X %i Y %i\n",svga_hwcursor_x,svga_hwcursor_y);
                return;

        }
        svga_out(addr, val, NULL);
}

uint8_t et4000w32p_in(uint16_t addr, void *priv)
{
        uint8_t temp;
//        if (addr==0x3DA) pclog("In 3DA %04X(%06X):%04X\n",CS,cs,pc);
        
//        pclog("ET4000W32p in  %04X  %04X:%04X  ",addr,CS,pc);

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
//        pclog("%04X\n",addr);
        
        switch (addr)
        {
                case 0x3C5:
                if ((seqaddr&0xF)==7) return seqregs[seqaddr&0xF]|4;
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                return stg_ramdac_in(addr, NULL);

                case 0x3CB:
                return svgaseg2;
                case 0x3CD:
                return svgaseg;
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
//                pclog("Read CRTC R%02X %02X\n", crtcreg, crtc[crtcreg]);
                return crtc[crtcreg];
                
                case 0x3DA:
                attrff=0;
                cgastat^=0x30;
                temp = cgastat & 0x39;
                if (svga_hdisp_on) temp |= 2;
                if (!(cgastat & 8)) temp |= 0x80;
//                pclog("3DA in %02X\n",temp);
                return temp;

                case 0x210A: case 0x211A: case 0x212A: case 0x213A:
                case 0x214A: case 0x215A: case 0x216A: case 0x217A:
                return et4000w32p_index;
                case 0x210B: case 0x211B: case 0x212B: case 0x213B:
                case 0x214B: case 0x215B: case 0x216B: case 0x217B:
                if (et4000w32p_index==0xEC) return (et4000w32p_regs[0xEC]&0xF)|0x60; /*ET4000/W32p rev D*/
                if (et4000w32p_index == 0xEF) 
                {
                        if (PCI) return et4000w32p_regs[0xEF] | 0xe0;       /*PCI*/
                        else     return et4000w32p_regs[0xEF] | 0x60;       /*VESA local bus*/
                }
                return et4000w32p_regs[et4000w32p_index];
        }
        return svga_in(addr, NULL);
}

void et4000w32p_recalctimings()
{
//        pclog("Recalc %08X  ",svga_ma);
        svga_ma|=(crtc[0x33]&0x7)<<16;
//        pclog("SVGA_MA %08X %i\n", svga_ma, (svga_miscout >> 2) & 3);
        if (crtc[0x35]&2)    svga_vtotal+=0x400;
        if (crtc[0x35]&4)    svga_dispend+=0x400;
        if (crtc[0x35]&8)    svga_vsyncstart+=0x400;
        if (crtc[0x35]&0x10) svga_split+=0x400;
        if (crtc[0x3F]&0x80) svga_rowoffset+=0x100;
        if (crtc[0x3F]&1)    svga_htotal+=256;
        if (attrregs[0x16]&0x20) svga_hdisp<<=1;
        
        switch ((svga_miscout >> 2) & 3)
        {
                case 0: case 1: break;
                case 2: case 3: svga_clock = cpuclock/icd2061_getfreq(2); break;
/*                default: 
                        pclog("Unknown clock %i\n", ((svga_miscout >> 2) & 3) | ((crtc[0x34] << 1) & 4) | ((crtc[0x31] & 0xc0) >> 3));
                        svga_clock = cpuclock/36000000.0; break;*/
        }
        
//        pclog("Recalctimings - %02X %02X %02X\n", crtc[6], crtc[7], crtc[0x35]);
}

int et4000w32p_getclock()
{
        return ((svga_miscout >> 2) & 3) | ((crtc[0x34] << 1) & 4) | ((crtc[0x31] & 0xc0) >> 3);
}


void et4000w32p_recalcmapping()
{
        int map;
        
        mem_removehandler(et4000w32p_linearbase_old, 0x200000,    svga_read_linear, svga_readw_linear, svga_readl_linear,    svga_write_linear, svga_writew_linear, svga_writel_linear,         NULL);
        mem_removehandler(                  0xa0000,  0x20000,           svga_read,        svga_readw,        svga_readl,           svga_write,        svga_writew,        svga_writel,         NULL);
        mem_removehandler(                  0xb0000,  0x10000, et4000w32p_mmu_read,              NULL,              NULL, et4000w32p_mmu_write,               NULL,               NULL,         NULL);    
        if (crtc[0x36] & 0x10) /*Linear frame buffer*/
        {
                mem_sethandler(et4000w32p_linearbase, 0x200000, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,      NULL);
        }
        else
        {
                map = (gdcreg[6] & 0xC) >> 2;
                if (crtc[0x36] & 0x20) map |= 4;
                if (crtc[0x36] & 0x08) map |= 8;
                switch (map)
                {
                        case 0x0: case 0x4: case 0x8: case 0xC: /*128k at A0000*/
                        mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        break;
                        case 0x1: /*64k at A0000*/
                        mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        break;
                        case 0x2: /*32k at B0000*/
                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        break;
                        case 0x3: /*32k at B8000*/
                        mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        break;
                        case 0x5: case 0x9: case 0xD: /*64k at A0000, MMU at B8000*/
                        mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        mem_sethandler(0xb8000, 0x08000, et4000w32p_mmu_read, NULL, NULL, et4000w32p_mmu_write, NULL, NULL,       NULL);                        
                        break;
                        case 0x6: case 0xA: case 0xE: /*32k at B0000, MMU at A8000*/
                        mem_sethandler(0xa8000, 0x08000, et4000w32p_mmu_read, NULL, NULL, et4000w32p_mmu_write, NULL, NULL,       NULL);
                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        break;
                        case 0x7: case 0xB: case 0xF: /*32k at B8000, MMU at A8000*/
                        mem_sethandler(0xa8000, 0x08000, et4000w32p_mmu_read, NULL, NULL, et4000w32p_mmu_write, NULL, NULL,       NULL);
                        mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,       NULL);
                        break;
                }
//                pclog("ET4K map %02X\n", map);
        }
        et4000w32p_linearbase_old = et4000w32p_linearbase;
}

/*Accelerator*/
struct
{
        struct
        {
                uint32_t pattern_addr,source_addr,dest_addr,mix_addr;
                uint16_t pattern_off,source_off,dest_off,mix_off;
                uint8_t pixel_depth,xy_dir;
                uint8_t pattern_wrap,source_wrap;
                uint16_t count_x,count_y;
                uint8_t ctrl_routing,ctrl_reload;
                uint8_t rop_fg,rop_bg;
                uint16_t pos_x,pos_y;
                uint16_t error;
                uint16_t dmin,dmaj;
        } queued,internal;
        uint32_t pattern_addr,source_addr,dest_addr,mix_addr;
        uint32_t pattern_back,source_back,dest_back,mix_back;
        int pattern_x,source_x;
        int pattern_x_back,source_x_back;
        int pattern_y,source_y;
        uint8_t status;
        uint64_t cpu_dat;
        int cpu_dat_pos;
        int pix_pos;
} acl;

#define ACL_WRST 1
#define ACL_RDST 2
#define ACL_XYST 4
#define ACL_SSO  8

struct
{
        uint32_t base[3];
        uint8_t ctrl;
} mmu;

void et4000w32_reset()
{
        acl.status=0;
}

void et4000w32_blit_start();
void et4000w32_blit(int count, uint32_t mix, uint32_t sdat, int cpu_input);

void et4000w32p_mmu_write(uint32_t addr, uint8_t val, void *priv)
{
        int bank;
//        pclog("ET4K write %08X %02X %02X %04X(%08X):%08X\n",addr,val,acl.status,acl.internal.ctrl_routing,CS,cs,pc);
        switch (addr&0x6000)
        {
                case 0x0000: /*MMU 0*/
                case 0x2000: /*MMU 1*/
                case 0x4000: /*MMU 2*/
                bank=(addr>>13)&3;
                if (mmu.ctrl&(1<<bank))
                {
                        if (!(acl.status&ACL_XYST)) return;
                        if (acl.internal.ctrl_routing&3)
                        {
                                if ((acl.internal.ctrl_routing&3)==2)
                                {
                                        if (acl.mix_addr&7)
                                           et4000w32_blit(8-(acl.mix_addr&7), val>>(acl.mix_addr&7), 0, 1);
                                        else
                                           et4000w32_blit(8, val, 0, 1);
                                }
                                else if ((acl.internal.ctrl_routing&3)==1)
                                   et4000w32_blit(1, ~0, val, 2);
//                                else
//                                   pclog("Bad ET4K routing %i\n",acl.internal.ctrl_routing&7);
                        }
                }
                else
                {
                        vram[(addr&0x1FFF)+mmu.base[bank]]=val;
                        changedvram[((addr&0x1FFF)+mmu.base[bank])>>10]=changeframecount;
                }
                break;
                case 0x6000:
                switch (addr&0x7FFF)
                {
                        case 0x7F00: mmu.base[0]=(mmu.base[0]&0xFFFFFF00)|val;       break;
                        case 0x7F01: mmu.base[0]=(mmu.base[0]&0xFFFF00FF)|(val<<8);  break;
                        case 0x7F02: mmu.base[0]=(mmu.base[0]&0xFF00FFFF)|(val<<16); break;
                        case 0x7F03: mmu.base[0]=(mmu.base[0]&0x00FFFFFF)|(val<<24); break;
                        case 0x7F04: mmu.base[1]=(mmu.base[1]&0xFFFFFF00)|val;       break;
                        case 0x7F05: mmu.base[1]=(mmu.base[1]&0xFFFF00FF)|(val<<8);  break;
                        case 0x7F06: mmu.base[1]=(mmu.base[1]&0xFF00FFFF)|(val<<16); break;
                        case 0x7F07: mmu.base[1]=(mmu.base[1]&0x00FFFFFF)|(val<<24); break;
                        case 0x7F08: mmu.base[2]=(mmu.base[2]&0xFFFFFF00)|val;       break;
                        case 0x7F09: mmu.base[2]=(mmu.base[2]&0xFFFF00FF)|(val<<8);  break;
                        case 0x7F0A: mmu.base[2]=(mmu.base[2]&0xFF00FFFF)|(val<<16); break;
                        case 0x7F0B: mmu.base[2]=(mmu.base[2]&0x00FFFFFF)|(val<<24); break;
                        case 0x7F13: mmu.ctrl=val; break;

                        case 0x7F80: acl.queued.pattern_addr=(acl.queued.pattern_addr&0xFFFFFF00)|val;       break;
                        case 0x7F81: acl.queued.pattern_addr=(acl.queued.pattern_addr&0xFFFF00FF)|(val<<8);  break;
                        case 0x7F82: acl.queued.pattern_addr=(acl.queued.pattern_addr&0xFF00FFFF)|(val<<16); break;
                        case 0x7F83: acl.queued.pattern_addr=(acl.queued.pattern_addr&0x00FFFFFF)|(val<<24); break;
                        case 0x7F84: acl.queued.source_addr =(acl.queued.source_addr &0xFFFFFF00)|val;       break;
                        case 0x7F85: acl.queued.source_addr =(acl.queued.source_addr &0xFFFF00FF)|(val<<8);  break;
                        case 0x7F86: acl.queued.source_addr =(acl.queued.source_addr &0xFF00FFFF)|(val<<16); break;
                        case 0x7F87: acl.queued.source_addr =(acl.queued.source_addr &0x00FFFFFF)|(val<<24); break;
                        case 0x7F88: acl.queued.pattern_off=(acl.queued.pattern_off&0xFF00)|val;      break;
                        case 0x7F89: acl.queued.pattern_off=(acl.queued.pattern_off&0x00FF)|(val<<8); break;
                        case 0x7F8A: acl.queued.source_off =(acl.queued.source_off &0xFF00)|val;      break;
                        case 0x7F8B: acl.queued.source_off =(acl.queued.source_off &0x00FF)|(val<<8); break;
                        case 0x7F8C: acl.queued.dest_off   =(acl.queued.dest_off   &0xFF00)|val;      break;
                        case 0x7F8D: acl.queued.dest_off   =(acl.queued.dest_off   &0x00FF)|(val<<8); break;
                        case 0x7F8E: acl.queued.pixel_depth=val; break;
                        case 0x7F8F: acl.queued.xy_dir=val; break;
                        case 0x7F90: acl.queued.pattern_wrap=val; break;
                        case 0x7F92: acl.queued.source_wrap=val; break;
                        case 0x7F98: acl.queued.count_x    =(acl.queued.count_x    &0xFF00)|val;      break;
                        case 0x7F99: acl.queued.count_x    =(acl.queued.count_x    &0x00FF)|(val<<8); break;
                        case 0x7F9A: acl.queued.count_y    =(acl.queued.count_y    &0xFF00)|val;      break;
                        case 0x7F9B: acl.queued.count_y    =(acl.queued.count_y    &0x00FF)|(val<<8); break;
                        case 0x7F9C: acl.queued.ctrl_routing=val; break;
                        case 0x7F9D: acl.queued.ctrl_reload =val; break;
                        case 0x7F9E: acl.queued.rop_bg      =val; break;
                        case 0x7F9F: acl.queued.rop_fg      =val; break;
                        case 0x7FA0: acl.queued.dest_addr   =(acl.queued.dest_addr   &0xFFFFFF00)|val;       break;
                        case 0x7FA1: acl.queued.dest_addr   =(acl.queued.dest_addr   &0xFFFF00FF)|(val<<8);  break;
                        case 0x7FA2: acl.queued.dest_addr   =(acl.queued.dest_addr   &0xFF00FFFF)|(val<<16); break;
                        case 0x7FA3: acl.queued.dest_addr   =(acl.queued.dest_addr   &0x00FFFFFF)|(val<<24);
                        acl.internal=acl.queued;
                        et4000w32_blit_start();
                        if (!(acl.queued.ctrl_routing&0x43))
                        {
                                et4000w32_blit(0xFFFFFF, ~0, 0, 0);
                        }
                        if ((acl.queued.ctrl_routing&0x40) && !(acl.internal.ctrl_routing&3))
                           et4000w32_blit(4, ~0, 0, 0);
                        break;
                        case 0x7FA4: acl.queued.mix_addr=(acl.queued.mix_addr&0xFFFFFF00)|val;       break;
                        case 0x7FA5: acl.queued.mix_addr=(acl.queued.mix_addr&0xFFFF00FF)|(val<<8);  break;
                        case 0x7FA6: acl.queued.mix_addr=(acl.queued.mix_addr&0xFF00FFFF)|(val<<16); break;
                        case 0x7FA7: acl.queued.mix_addr=(acl.queued.mix_addr&0x00FFFFFF)|(val<<24); break;
                        case 0x7FA8: acl.queued.mix_off    =(acl.queued.mix_off    &0xFF00)|val;      break;
                        case 0x7FA9: acl.queued.mix_off    =(acl.queued.mix_off    &0x00FF)|(val<<8); break;
                        case 0x7FAA: acl.queued.error      =(acl.queued.error      &0xFF00)|val;      break;
                        case 0x7FAB: acl.queued.error      =(acl.queued.error      &0x00FF)|(val<<8); break;
                        case 0x7FAC: acl.queued.dmin       =(acl.queued.dmin       &0xFF00)|val;      break;
                        case 0x7FAD: acl.queued.dmin       =(acl.queued.dmin       &0x00FF)|(val<<8); break;
                        case 0x7FAE: acl.queued.dmaj       =(acl.queued.dmaj       &0xFF00)|val;      break;
                        case 0x7FAF: acl.queued.dmaj       =(acl.queued.dmaj       &0x00FF)|(val<<8); break;
                }
                break;
        }
}

uint8_t et4000w32p_mmu_read(uint32_t addr, void *priv)
{
        int bank;
        uint8_t temp;
//        pclog("ET4K read %08X %04X(%08X):%08X\n",addr,CS,cs,pc);
        switch (addr&0x6000)
        {
                case 0x0000: /*MMU 0*/
                case 0x2000: /*MMU 1*/
                case 0x4000: /*MMU 2*/
                bank=(addr>>13)&3;
                if (mmu.ctrl&(1<<bank))
                {
                        temp=0xFF;
                        if (acl.cpu_dat_pos)
                        {
                                acl.cpu_dat_pos--;
                                temp=acl.cpu_dat&0xFF;
                                acl.cpu_dat>>=8;
                        }
                        if ((acl.queued.ctrl_routing&0x40) && !acl.cpu_dat_pos && !(acl.internal.ctrl_routing&3))
                           et4000w32_blit(4, ~0, 0, 0);
                        /*???*/
                        return temp;
                }
                return vram[(addr&0x1FFF)+mmu.base[bank]];
                case 0x6000:
                switch (addr&0x7FFF)
                {
                        case 0x7F00: return mmu.base[0];
                        case 0x7F01: return mmu.base[0]>>8;
                        case 0x7F02: return mmu.base[0]>>16;
                        case 0x7F03: return mmu.base[0]>>24;
                        case 0x7F04: return mmu.base[1];
                        case 0x7F05: return mmu.base[1]>>8;
                        case 0x7F06: return mmu.base[1]>>16;
                        case 0x7F07: return mmu.base[1]>>24;
                        case 0x7F08: return mmu.base[2];
                        case 0x7F09: return mmu.base[2]>>8;
                        case 0x7F0A: return mmu.base[2]>>16;
                        case 0x7F0B: return mmu.base[2]>>24;
                        case 0x7F13: return mmu.ctrl;

                        case 0x7F36:
//                                pclog("Read ACL status %02X\n",acl.status);
//                        if (acl.internal.pos_x!=acl.internal.count_x || acl.internal.pos_y!=acl.internal.count_y) return acl.status | ACL_XYST;
                        return acl.status;
                        case 0x7F80: return acl.internal.pattern_addr;
                        case 0x7F81: return acl.internal.pattern_addr>>8;
                        case 0x7F82: return acl.internal.pattern_addr>>16;
                        case 0x7F83: return acl.internal.pattern_addr>>24;
                        case 0x7F84: return acl.internal.source_addr;
                        case 0x7F85: return acl.internal.source_addr>>8;
                        case 0x7F86: return acl.internal.source_addr>>16;
                        case 0x7F87: return acl.internal.source_addr>>24;
                        case 0x7F88: return acl.internal.pattern_off;
                        case 0x7F89: return acl.internal.pattern_off>>8;
                        case 0x7F8A: return acl.internal.source_off;
                        case 0x7F8B: return acl.internal.source_off>>8;
                        case 0x7F8C: return acl.internal.dest_off;
                        case 0x7F8D: return acl.internal.dest_off>>8;
                        case 0x7F8E: return acl.internal.pixel_depth;
                        case 0x7F8F: return acl.internal.xy_dir;
                        case 0x7F90: return acl.internal.pattern_wrap;
                        case 0x7F92: return acl.internal.source_wrap;
                        case 0x7F98: return acl.internal.count_x;
                        case 0x7F99: return acl.internal.count_x>>8;
                        case 0x7F9A: return acl.internal.count_y;
                        case 0x7F9B: return acl.internal.count_y>>8;
                        case 0x7F9C: return acl.internal.ctrl_routing;
                        case 0x7F9D: return acl.internal.ctrl_reload;
                        case 0x7F9E: return acl.internal.rop_bg;
                        case 0x7F9F: return acl.internal.rop_fg;
                        case 0x7FA0: return acl.internal.dest_addr;
                        case 0x7FA1: return acl.internal.dest_addr>>8;
                        case 0x7FA2: return acl.internal.dest_addr>>16;
                        case 0x7FA3: return acl.internal.dest_addr>>24;
                }
                return 0xFF;
        }
        return 0xff;
}

int et4000w32_max_x[8]={0,0,4,8,16,32,64,0x70000000};
int et4000w32_wrap_x[8]={0,0,3,7,15,31,63,0xFFFFFFFF};
int et4000w32_wrap_y[8]={1,2,4,8,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

int bltout=0;
void et4000w32_blit_start()
{
//        if (acl.queued.xy_dir&0x80)
//           pclog("Blit - %02X %08X (%i,%i) %08X (%i,%i) %08X (%i,%i) %i  %i %i  %02X %02X  %02X\n",acl.queued.xy_dir,acl.internal.pattern_addr,(acl.internal.pattern_addr/3)%640,(acl.internal.pattern_addr/3)/640,acl.internal.source_addr,(acl.internal.source_addr/3)%640,(acl.internal.source_addr/3)/640,acl.internal.dest_addr,(acl.internal.dest_addr/3)%640,(acl.internal.dest_addr/3)/640,acl.internal.xy_dir,acl.internal.count_x,acl.internal.count_y,acl.internal.rop_fg,acl.internal.rop_bg, acl.internal.ctrl_routing);
//           bltout=1;
//        bltout=(acl.internal.count_x==1541);
        if (!(acl.queued.xy_dir&0x20))
           acl.internal.error = acl.internal.dmaj/2;
        acl.pattern_addr=acl.internal.pattern_addr;
        acl.source_addr =acl.internal.source_addr;
        acl.mix_addr    =acl.internal.mix_addr;
        acl.mix_back    =acl.mix_addr;
        acl.dest_addr   =acl.internal.dest_addr;
        acl.dest_back   =acl.dest_addr;
        acl.internal.pos_x=acl.internal.pos_y=0;
        acl.pattern_x=acl.source_x=acl.pattern_y=acl.source_y=0;
        acl.status = ACL_XYST;
        if ((!(acl.internal.ctrl_routing&7) || (acl.internal.ctrl_routing&4)) && !(acl.internal.ctrl_routing&0x40)) acl.status |= ACL_SSO;
        
        if (et4000w32_wrap_x[acl.internal.pattern_wrap&7])
        {
                acl.pattern_x=acl.pattern_addr&et4000w32_wrap_x[acl.internal.pattern_wrap&7];
                acl.pattern_addr&=~et4000w32_wrap_x[acl.internal.pattern_wrap&7];
        }
        acl.pattern_back=acl.pattern_addr;
        if (!(acl.internal.pattern_wrap&0x40))
        {
                acl.pattern_y=(acl.pattern_addr/(et4000w32_wrap_x[acl.internal.pattern_wrap&7]+1))&(et4000w32_wrap_y[(acl.internal.pattern_wrap>>4)&7]-1);
                acl.pattern_back&=~(((et4000w32_wrap_x[acl.internal.pattern_wrap&7]+1)*et4000w32_wrap_y[(acl.internal.pattern_wrap>>4)&7])-1);
        }
        acl.pattern_x_back=acl.pattern_x;
        
        if (et4000w32_wrap_x[acl.internal.source_wrap&7])
        {
                acl.source_x=acl.source_addr&et4000w32_wrap_x[acl.internal.source_wrap&7];
                acl.source_addr&=~et4000w32_wrap_x[acl.internal.source_wrap&7];
        }
        acl.source_back=acl.source_addr;
        if (!(acl.internal.source_wrap&0x40))
        {
                acl.source_y=(acl.source_addr/(et4000w32_wrap_x[acl.internal.source_wrap&7]+1))&(et4000w32_wrap_y[(acl.internal.source_wrap>>4)&7]-1);
                acl.source_back&=~(((et4000w32_wrap_x[acl.internal.source_wrap&7]+1)*et4000w32_wrap_y[(acl.internal.source_wrap>>4)&7])-1);
        }
        acl.source_x_back=acl.source_x;

        et4000w32_max_x[2]=((acl.internal.pixel_depth&0x30)==0x20)?3:4;
        
        acl.internal.count_x += (acl.internal.pixel_depth>>4)&3;
        acl.cpu_dat_pos=0;
        acl.cpu_dat=0;
        
        acl.pix_pos=0;
}

void et4000w32_incx(int c)
{
        acl.dest_addr+=c;
        acl.pattern_x+=c;
        acl.source_x +=c;
        acl.mix_addr +=c;
        if (acl.pattern_x>=et4000w32_max_x[acl.internal.pattern_wrap&7])
           acl.pattern_x -=et4000w32_max_x[acl.internal.pattern_wrap&7];
        if (acl.source_x >=et4000w32_max_x[acl.internal.source_wrap &7])
           acl.source_x  -=et4000w32_max_x[acl.internal.source_wrap &7];
}
void et4000w32_decx(int c)
{
        acl.dest_addr-=c;
        acl.pattern_x-=c;
        acl.source_x -=c;
        acl.mix_addr -=c;
        if (acl.pattern_x<0)
           acl.pattern_x +=et4000w32_max_x[acl.internal.pattern_wrap&7];
        if (acl.source_x <0)
           acl.source_x  +=et4000w32_max_x[acl.internal.source_wrap &7];
}
void et4000w32_incy()
{
        acl.pattern_addr+=acl.internal.pattern_off+1;
        acl.source_addr +=acl.internal.source_off +1;
        acl.mix_addr    +=acl.internal.mix_off    +1;
        acl.dest_addr   +=acl.internal.dest_off   +1;
        acl.pattern_y++;
        if (acl.pattern_y == et4000w32_wrap_y[(acl.internal.pattern_wrap>>4)&7])
        {
                acl.pattern_y = 0;
                acl.pattern_addr = acl.pattern_back;
        }
        acl.source_y++;
        if (acl.source_y == et4000w32_wrap_y[(acl.internal.source_wrap >>4)&7])
        {
                acl.source_y = 0;
                acl.source_addr = acl.source_back;
        }
}
void et4000w32_decy()
{
        acl.pattern_addr-=acl.internal.pattern_off+1;
        acl.source_addr -=acl.internal.source_off +1;
        acl.mix_addr    -=acl.internal.mix_off    +1;
        acl.dest_addr   -=acl.internal.dest_off   +1;
        acl.pattern_y--;
        if (acl.pattern_y<0 && !(acl.internal.pattern_wrap&0x40))
        {
                acl.pattern_y=et4000w32_wrap_y[(acl.internal.pattern_wrap>>4)&7]-1;
                acl.pattern_addr=acl.pattern_back+(et4000w32_wrap_x[acl.internal.pattern_wrap&7]*(et4000w32_wrap_y[(acl.internal.pattern_wrap>>4)&7]-1));
        }
        acl.source_y--;
        if (acl.source_y<0 && !(acl.internal.source_wrap&0x40))
        {
                acl.source_y =et4000w32_wrap_y[(acl.internal.source_wrap >>4)&7]-1;
                acl.source_addr =acl.source_back +(et4000w32_wrap_x[acl.internal.source_wrap&7] *(et4000w32_wrap_y[(acl.internal.source_wrap>>4)&7]-1));;
        }
}

void et4000w32_blit(int count, uint32_t mix, uint32_t sdat, int cpu_input)
{
        int c,d;
        uint8_t pattern,source,dest,out;
        uint8_t rop;
        int mixdat;

        if (!(acl.status & ACL_XYST)) return;
//        if (count>400) pclog("New blit - %i,%i %06X (%i,%i) %06X %06X\n",acl.internal.count_x,acl.internal.count_y,acl.dest_addr,acl.dest_addr%640,acl.dest_addr/640,acl.source_addr,acl.pattern_addr);
        //pclog("Blit exec - %i %i %i\n",count,acl.internal.pos_x,acl.internal.pos_y);
        if (acl.internal.xy_dir&0x80) /*Line draw*/
        {
                while (count--)
                {
                        if (bltout) pclog("%i,%i : ",acl.internal.pos_x,acl.internal.pos_y);
                        pattern=vram[(acl.pattern_addr+acl.pattern_x)&0x1FFFFF];
                        source =vram[(acl.source_addr +acl.source_x) &0x1FFFFF];
                        if (bltout) pclog("%06X %06X ",(acl.pattern_addr+acl.pattern_x)&0x1FFFFF,(acl.source_addr +acl.source_x) &0x1FFFFF);
                        if (cpu_input==2)
                        {
                                source=sdat&0xFF;
                                sdat>>=8;
                        }
                        dest=vram[acl.dest_addr   &0x1FFFFF];
                        out=0;
                        if (bltout) pclog("%06X   ",acl.dest_addr);
                        if ((acl.internal.ctrl_routing&0xA)==8)
                        {
                                mixdat = vram[(acl.mix_addr>>3)&0x1FFFFF] & (1<<(acl.mix_addr&7));
                                if (bltout) pclog("%06X %02X  ",acl.mix_addr,vram[(acl.mix_addr>>3)&0x1FFFFF]);
                        }
                        else
                        {
                                mixdat = mix & 1;
                                mix>>=1; mix|=0x80000000;
                        }
                        acl.mix_addr++;
                        rop = (mixdat) ? acl.internal.rop_fg:acl.internal.rop_bg;
                        for (c=0;c<8;c++)
                        {
                                d=(dest & (1<<c)) ? 1:0;
                                if (source & (1<<c))  d|=2;
                                if (pattern & (1<<c)) d|=4;
                                if (rop & (1<<d)) out|=(1<<c);
                        }
                        if (bltout) pclog("%06X = %02X\n",acl.dest_addr&0x1FFFFF,out);
                        if (!(acl.internal.ctrl_routing&0x40))
                        {
                                vram[acl.dest_addr&0x1FFFFF]=out;
                                changedvram[(acl.dest_addr&0x1FFFFF)>>10]=changeframecount;
                        }
                        else
                        {
                                acl.cpu_dat|=((uint64_t)out<<(acl.cpu_dat_pos*8));
                                acl.cpu_dat_pos++;
                        }
                        
//                        pclog("%i %i\n",acl.pix_pos,(acl.internal.pixel_depth>>4)&3);
                        acl.pix_pos++;
                        acl.internal.pos_x++;
                        if (acl.pix_pos<=((acl.internal.pixel_depth>>4)&3))
                        {
                                if (acl.internal.xy_dir&1) et4000w32_decx(1);
                                else                       et4000w32_incx(1);
                        }
                        else
                        {
                                if (acl.internal.xy_dir&1) et4000w32_incx((acl.internal.pixel_depth>>4)&3);
                                else                       et4000w32_decx((acl.internal.pixel_depth>>4)&3);
                                acl.pix_pos=0;
                                /*Next pixel*/
                                switch (acl.internal.xy_dir&7)
                                {
                                        case 0: case 1: /*Y+*/
                                        et4000w32_incy();
                                        acl.internal.pos_y++;
                                        acl.internal.pos_x-=((acl.internal.pixel_depth>>4)&3)+1;
                                        break;
                                        case 2: case 3: /*Y-*/
                                        et4000w32_decy();
                                        acl.internal.pos_y++;
                                        acl.internal.pos_x-=((acl.internal.pixel_depth>>4)&3)+1;
                                        break;
                                        case 4: case 6: /*X+*/
                                        et4000w32_incx(((acl.internal.pixel_depth>>4)&3)+1);
                                        //acl.internal.pos_x++;
                                        break;
                                        case 5: case 7: /*X-*/
                                        et4000w32_decx(((acl.internal.pixel_depth>>4)&3)+1);
                                        //acl.internal.pos_x++;
                                        break;
                                }
                                acl.internal.error+=acl.internal.dmin;
                                if (acl.internal.error > acl.internal.dmaj)
                                {
                                        acl.internal.error-=acl.internal.dmaj;
                                        switch (acl.internal.xy_dir&7)
                                        {
                                                case 0: case 2: /*X+*/
                                                et4000w32_incx(((acl.internal.pixel_depth>>4)&3)+1);
                                                acl.internal.pos_x++;
                                                break;
                                                case 1: case 3: /*X-*/
                                                et4000w32_decx(((acl.internal.pixel_depth>>4)&3)+1);
                                                acl.internal.pos_x++;
                                                break;
                                                case 4: case 5: /*Y+*/
                                                et4000w32_incy();
                                                acl.internal.pos_y++;
                                                break;
                                                case 6: case 7: /*Y-*/
                                                et4000w32_decy();
                                                acl.internal.pos_y++;
                                                break;
                                        }
                                }
                                if (acl.internal.pos_x > acl.internal.count_x ||
                                    acl.internal.pos_y > acl.internal.count_y)
                                {
                                        acl.status = 0;
//                                        pclog("Blit line over\n");
                                        return;
                                }
                        }
                }
        }
        else
        {
                while (count--)
                {
                        if (bltout) pclog("%i,%i : ",acl.internal.pos_x,acl.internal.pos_y);
                        
                        pattern=vram[(acl.pattern_addr+acl.pattern_x)&0x1FFFFF];
                        source =vram[(acl.source_addr +acl.source_x) &0x1FFFFF];
                        if (bltout) pclog("%i %06X %06X %02X %02X  ",acl.pattern_y,(acl.pattern_addr+acl.pattern_x)&0x1FFFFF,(acl.source_addr +acl.source_x) &0x1FFFFF,pattern,source);

                        if (cpu_input==2)
                        {
                                source=sdat&0xFF;
                                sdat>>=8;
                        }
                        dest=vram[acl.dest_addr   &0x1FFFFF];
                        out=0;
                        if (bltout) pclog("%06X %02X  %i %08X %08X  ",dest,acl.dest_addr,mix&1,mix,acl.mix_addr);
                        if ((acl.internal.ctrl_routing&0xA)==8)
                        {
                                mixdat = vram[(acl.mix_addr>>3)&0x1FFFFF] & (1<<(acl.mix_addr&7));
                                if (bltout) pclog("%06X %02X  ",acl.mix_addr,vram[(acl.mix_addr>>3)&0x1FFFFF]);
                        }
                        else
                        {
                                mixdat = mix & 1;
                                mix>>=1; mix|=0x80000000;
                        }

                        rop = (mixdat) ? acl.internal.rop_fg:acl.internal.rop_bg;
                        for (c=0;c<8;c++)
                        {
                                d=(dest & (1<<c)) ? 1:0;
                                if (source & (1<<c))  d|=2;
                                if (pattern & (1<<c)) d|=4;
                                if (rop & (1<<d)) out|=(1<<c);
                        }
                        if (bltout) pclog("%06X = %02X\n",acl.dest_addr&0x1FFFFF,out);
                        if (!(acl.internal.ctrl_routing&0x40))
                        {
                                vram[acl.dest_addr&0x1FFFFF]=out;
                                changedvram[(acl.dest_addr&0x1FFFFF)>>10]=changeframecount;
                        }
                        else
                        {
                                acl.cpu_dat|=((uint64_t)out<<(acl.cpu_dat_pos*8));
                                acl.cpu_dat_pos++;
                        }

                        if (acl.internal.xy_dir&1) et4000w32_decx(1);
                        else                       et4000w32_incx(1);

                        acl.internal.pos_x++;
                        if (acl.internal.pos_x>acl.internal.count_x)
                        {
                                if (acl.internal.xy_dir&2)
                                {
                                        et4000w32_decy();
                                        acl.mix_back =acl.mix_addr =acl.mix_back -(acl.internal.mix_off +1);
                                        acl.dest_back=acl.dest_addr=acl.dest_back-(acl.internal.dest_off+1);
                                }
                                else
                                {
                                        et4000w32_incy();
                                        acl.mix_back =acl.mix_addr =acl.mix_back +acl.internal.mix_off +1;
                                        acl.dest_back=acl.dest_addr=acl.dest_back+acl.internal.dest_off+1;
                                }

                                acl.pattern_x = acl.pattern_x_back;
                                acl.source_x  = acl.source_x_back;

                                acl.internal.pos_y++;
                                acl.internal.pos_x=0;
                                if (acl.internal.pos_y>acl.internal.count_y)
                                {
                                        acl.status = 0;
//                                        pclog("Blit over\n");
                                        return;
                                }
                                if (cpu_input) return;
                                if (acl.internal.ctrl_routing&0x40)
                                {
                                        if (acl.cpu_dat_pos&3) acl.cpu_dat_pos+=4-(acl.cpu_dat_pos&3);
                                        return;
                                }
                        }
                }
        }
}


void et4000w32p_cursor_draw(int displine)
{
        int x, offset;
        uint8_t dat;
        offset = svga_hwcursor_latch.xoff;
        for (x = 0; x < 64 - svga_hwcursor_latch.xoff; x += 4)
        {
                dat = vram[svga_hwcursor_latch.addr + (offset >> 2)];
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 32]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 32] ^= 0xFFFFFF;
                dat >>= 2;
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 33]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 33] ^= 0xFFFFFF;
                dat >>= 2;
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 34]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 34] ^= 0xFFFFFF;
                dat >>= 2;
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 35]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 35] ^= 0xFFFFFF;
                dat >>= 2;
                offset += 4;
        }
        svga_hwcursor_latch.addr += 16;
}

uint8_t et4000w32p_pci_read(int func, int addr, void *priv)
{
        pclog("ET4000 PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x0c; /*Tseng Labs*/
                case 0x01: return 0x10;
                
                case 0x02: return 0x06; /*ET4000W32p Rev D*/
                case 0x03: return 0x32;
                
                case 0x04: return 0x03; /*Respond to IO and memory accesses*/

                case 0x07: return 1 << 1; /*Medium DEVSEL timing*/
                
                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x0a: return 0x01; /*Supports VGA interface, XGA compatible*/
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

void et4000w32p_pci_write(int func, int addr, uint8_t val, void *priv)
{
        switch (addr)
        {
                case 0x13: et4000w32p_linearbase = val << 24; et4000w32p_recalcmapping(); break;
        }
}

int et4000w32p_init()
{
        svga_recalctimings_ex = et4000w32p_recalctimings;
        svga_hwcursor_draw    = et4000w32p_cursor_draw;
        
        io_sethandler(0x210A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x211A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x212A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x213A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x214A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x215A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x216A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        io_sethandler(0x217A, 0x0002, et4000w32p_in, NULL, NULL, et4000w32p_out, NULL, NULL, NULL);
        
        pci_add(et4000w32p_pci_read, et4000w32p_pci_write, NULL);
        
        svga_vram_limit = 2 << 20; /*2mb - chip supports 4mb but can't map both 4mb linear frame buffer and accelerator registers*/
        
        vrammask = 0x1fffff;

        return svga_init();
}

GFXCARD vid_et4000w32p =
{
        et4000w32p_init,
        /*IO at 3Cx/3Dx*/
        et4000w32p_out,
        et4000w32p_in,
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


