/*Cirrus Logic CL-GD5429 emulation*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_unk_ramdac.h"

struct
{
        uint32_t bank[2];

        struct
        {
                uint16_t bg_col, fg_col;                
                uint16_t width, height;
                uint16_t dst_pitch, src_pitch;               
                uint32_t dst_addr, src_addr;
                uint8_t mask, mode, rop;
                
                uint32_t dst_addr_backup, src_addr_backup;
                uint16_t width_backup, height_internal;
                int x_count;
        } blt;

} gd5429;

void gd5429_write(uint32_t addr, uint8_t val, void *priv);
uint8_t gd5429_read(uint32_t addr, void *priv);

void gd5429_mmio_write(uint32_t addr, uint8_t val, void *priv);
uint8_t gd5429_mmio_read(uint32_t addr, void *priv);

void gd5429_blt_write_w(uint32_t addr, uint16_t val, void *priv);
void gd5429_blt_write_l(uint32_t addr, uint32_t val, void *priv);

void gd5429_recalc_banking();
void gd5429_recalc_mapping();

void gd5429_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        pclog("gd5429 out %04X %02X\n", addr, val);
                
        switch (addr)
        {
                case 0x3c4:
                seqaddr = val;
                break;
                case 0x3c5:
                if (seqaddr > 5)
                {
                        seqregs[seqaddr & 0x1f] = val;
                        switch (seqaddr & 0x1f)
                        {
                                case 0x10: case 0x30: case 0x50: case 0x70:
                                case 0x90: case 0xb0: case 0xd0: case 0xf0:
                                svga_hwcursor.x = (val << 3) | ((seqaddr >> 5) & 7);
                                pclog("svga_hwcursor.x = %i\n", svga_hwcursor.x);
                                break;
                                case 0x11: case 0x31: case 0x51: case 0x71:
                                case 0x91: case 0xb1: case 0xd1: case 0xf1:
                                svga_hwcursor.y = (val << 3) | ((seqaddr >> 5) & 7);
                                pclog("svga_hwcursor.y = %i\n", svga_hwcursor.y);
                                break;
                                case 0x12:
                                svga_hwcursor.ena = val & 1;
                                pclog("svga_hwcursor.ena = %i\n", svga_hwcursor.ena);
                                break;                               
                                case 0x13:
                                svga_hwcursor.addr = 0x1fc000 + ((val & 0x3f) * 256);
                                pclog("svga_hwcursor.addr = %x\n", svga_hwcursor.addr);
                                break;                                
                                
                                case 0x17:
                                gd5429_recalc_mapping();
                                break;
                        }
                        return;
                }
                break;

                case 0x3cf:
                if (gdcaddr == 5)
                {
                        gdcreg[5] = val;
                        if (gdcreg[0xb] & 0x04)
                                writemode = gdcreg[5] & 7;
                        else
                                writemode = gdcreg[5] & 3;
                        readmode = val & 8;
                        pclog("writemode = %i\n", writemode);
                        return;
                }
                if (gdcaddr == 6)
                {
                        if ((gdcreg[6] & 0xc) != (val & 0xc))
                        {
                                gdcreg[6] = val;
                                gd5429_recalc_mapping();
                        }
                        gdcreg[6] = val;
                        return;
                }
                if (gdcaddr > 8)
                {
                        gdcreg[gdcaddr & 0x3f] = val;
                        switch (gdcaddr)
                        {
                                case 0x09: case 0x0a: case 0x0b:
                                gd5429_recalc_banking();
                                if (gdcreg[0xb] & 0x04)
                                        writemode = gdcreg[5] & 7;
                                else
                                        writemode = gdcreg[5] & 3;
                                break;
                        }                        
                        return;
                }
                break;
                
                case 0x3D4:
                crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;

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

uint8_t gd5429_in(uint16_t addr, void *priv)
{
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
        if (addr != 0x3da) pclog("IN gd5429 %04X\n", addr);
        
        switch (addr)
        {
                case 0x3c5:
                if (seqaddr > 5)
                {
                        switch (seqaddr)
                        {
                                case 6:
                                return ((seqregs[6] & 0x17) == 0x12) ? 0x12 : 0x0f;
                        }
                        return seqregs[seqaddr & 0x3f];
                }
                break;

                case 0x3cf:
                if (gdcaddr > 8)
                {
                        return gdcreg[seqaddr & 0x3f];
                }
                break;

                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                switch (crtcreg)
                {
                        case 0x27: /*ID*/
                        return 0x9c; /*GD5429*/
                }
                return crtc[crtcreg];
        }
        return svga_in(addr, NULL);
}

void gd5429_recalc_banking()
{
        if (gdcreg[0xb] & 0x20)
                gd5429.bank[0] = (gdcreg[0x09] & 0x7f) << 14;
        else
                gd5429.bank[0] = gdcreg[0x09] << 12;
                                
        if (gdcreg[0xb] & 0x01)
        {
                if (gdcreg[0xb] & 0x20)
                        gd5429.bank[1] = (gdcreg[0x0a] & 0x7f) << 14;
                else
                        gd5429.bank[1] = gdcreg[0x0a] << 12;
        }
        else
                gd5429.bank[1] = gd5429.bank[0] + 0x8000;
}

void gd5429_recalc_mapping()
{
        mem_removehandler(0xa0000, 0x20000, gd5429_read, NULL, NULL, gd5429_write, NULL, NULL,  NULL);
        mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,  NULL);
        mem_removehandler(0xb8000, 0x00100, gd5429_mmio_read, NULL, NULL, gd5429_mmio_write, NULL, NULL,  NULL);
        pclog("Write mapping %02X %i\n", gdcreg[6], seqregs[0x17] & 0x04);
        switch (gdcreg[6] & 0x0C)
        {
                case 0x0: /*128k at A0000*/
                mem_sethandler(0xa0000, 0x10000, gd5429_read, NULL, NULL, gd5429_write, NULL, NULL,  NULL);
                break;
                case 0x4: /*64k at A0000*/
                mem_sethandler(0xa0000, 0x10000, gd5429_read, NULL, NULL, gd5429_write, NULL, NULL,  NULL);
                if (seqregs[0x17] & 0x04)
                        mem_sethandler(0xb8000, 0x00100, gd5429_mmio_read, NULL, NULL, gd5429_mmio_write, NULL, NULL,  NULL);
                break;
                case 0x8: /*32k at B0000*/
                mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,  NULL);
                break;
                case 0xC: /*32k at B8000*/
                mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,  NULL);
                break;
        }
}
        
void gd5429_recalctimings()
{
        if (seqregs[7] & 0x01)
        {
                svga_render = svga_render_8bpp_highres;
        }
        
        svga_ma |= ((crtc[0x1b] & 0x01) << 16) | ((crtc[0x1b] & 0xc) << 15);
        pclog("MA now %05X %02X\n", svga_ma, crtc[0x1b]);
}

void gd5429_hwcursor_draw(int displine)
{
        int x;
        uint8_t dat[2];
        int xx;
        int offset = svga_hwcursor_latch.x - svga_hwcursor_latch.xoff;
        
        pclog("HWcursor %i %i  %i %i  %x %02X %02X\n", svga_hwcursor_latch.x, svga_hwcursor_latch.y,  offset, displine, svga_hwcursor_latch.addr, vram[svga_hwcursor_latch.addr], vram[svga_hwcursor_latch.addr + 0x80]);
        for (x = 0; x < 32; x += 8)
        {
                dat[0] = vram[svga_hwcursor_latch.addr];
                dat[1] = vram[svga_hwcursor_latch.addr + 0x80];
                for (xx = 0; xx < 8; xx++)
                {
                        if (offset >= svga_hwcursor_latch.x)
                        {
                                if (dat[1] & 0x80)
                                        ((uint32_t *)buffer32->line[displine])[offset + 32] = 0;
                                if (dat[0] & 0x80)
                                        ((uint32_t *)buffer32->line[displine])[offset + 32] ^= 0xffffff;
                        }
                           
                        offset++;
                        dat[0] <<= 1;
                        dat[1] <<= 1;
                }
                svga_hwcursor_latch.addr++;
        }
}

int gd5429_init()
{
        svga_recalctimings_ex = gd5429_recalctimings;
        svga_hwcursor_draw    = gd5429_hwcursor_draw;
        svga_vram_limit = 2 << 20; /*2mb*/
        vrammask = 0x1fffff;

        svga_hwcursor.yoff = 32;
        svga_hwcursor.xoff = 0;

        memset(&gd5429, 0, sizeof(gd5429));
        
        gd5429.bank[1] = 0x8000;
        
        return svga_init();
}

static uint8_t la, lb, lc, ld;

uint8_t gd5429_read_linear(uint32_t addr, void *priv);
void gd5429_write_linear(uint32_t addr, uint8_t val, void *priv);

void gd5429_write(uint32_t addr, uint8_t val, void *priv)
{
//        pclog("gd5429_write : %05X %02X  ", addr, val);
        addr = (addr & 0x7fff) + gd5429.bank[(addr >> 15) & 1];
//        pclog("%08X\n", addr);
        gd5429_write_linear(addr, val, priv);
}

uint8_t gd5429_read(uint32_t addr, void *priv)
{
        uint8_t ret;
//        pclog("gd5429_read : %05X ", addr);
        addr = (addr & 0x7fff) + gd5429.bank[(addr >> 15) & 1];
        ret = gd5429_read_linear(addr, priv);
//        pclog("%08X %02X\n", addr, ret);  
        return ret;      
}

void gd5429_write_linear(uint32_t addr, uint8_t val, void *priv)
{
        uint8_t vala,valb,valc,vald,wm=writemask;
        int writemask2 = writemask;

        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

        egawrites++;
        
//        if (svga_output) pclog("Write LFB %08X %02X ", addr, val);
        if (!(gdcreg[6]&1)) fullchange=2;
        if (chain4 && (writemode < 4))
        {
                writemask2=1<<(addr&3);
                addr&=~3;
        }
        else
        {
                addr<<=2;
        }
        addr &= 0x7fffff;
//        if (svga_output) pclog("%08X\n", addr);
        changedvram[addr>>10]=changeframecount;
        
        switch (writemode)
        {
                case 4:
                pclog("Writemode 4 : %X ", addr);
                addr <<= 1;
                changedvram[addr>>10]=changeframecount;
                pclog("%X %X\n", addr, val);
                if (val & 0x80)
                        vram[addr + 0] = gdcreg[1];
                if (val & 0x40)
                        vram[addr + 1] = gdcreg[1];
                if (val & 0x20)
                        vram[addr + 2] = gdcreg[1];
                if (val & 0x10)
                        vram[addr + 3] = gdcreg[1];
                if (val & 0x08)
                        vram[addr + 4] = gdcreg[1];
                if (val & 0x04)
                        vram[addr + 5] = gdcreg[1];
                if (val & 0x02)
                        vram[addr + 6] = gdcreg[1];
                if (val & 0x01)
                        vram[addr + 7] = gdcreg[1];
                break;
                        
                case 5:
                pclog("Writemode 5 : %X ", addr);
                addr <<= 1;
                changedvram[addr>>10]=changeframecount;
                pclog("%X %X\n", addr, val);
                vram[addr + 0] = (val & 0x80) ? gdcreg[1] : gdcreg[0];
                vram[addr + 1] = (val & 0x40) ? gdcreg[1] : gdcreg[0];
                vram[addr + 2] = (val & 0x20) ? gdcreg[1] : gdcreg[0];
                vram[addr + 3] = (val & 0x10) ? gdcreg[1] : gdcreg[0];
                vram[addr + 4] = (val & 0x08) ? gdcreg[1] : gdcreg[0];
                vram[addr + 5] = (val & 0x04) ? gdcreg[1] : gdcreg[0];
                vram[addr + 6] = (val & 0x02) ? gdcreg[1] : gdcreg[0];
                vram[addr + 7] = (val & 0x01) ? gdcreg[1] : gdcreg[0];
                break;
                
                case 1:
                if (writemask2&1) vram[addr]=la;
                if (writemask2&2) vram[addr|0x1]=lb;
                if (writemask2&4) vram[addr|0x2]=lc;
                if (writemask2&8) vram[addr|0x3]=ld;
                break;
                case 0:
                if (gdcreg[3]&7) val=svga_rotate[gdcreg[3]&7][val];
                if (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1])
                {
                        if (writemask2&1) vram[addr]=val;
                        if (writemask2&2) vram[addr|0x1]=val;
                        if (writemask2&4) vram[addr|0x2]=val;
                        if (writemask2&8) vram[addr|0x3]=val;
                }
                else
                {
                        if (gdcreg[1]&1) vala=(gdcreg[0]&1)?0xFF:0;
                        else             vala=val;
                        if (gdcreg[1]&2) valb=(gdcreg[0]&2)?0xFF:0;
                        else             valb=val;
                        if (gdcreg[1]&4) valc=(gdcreg[0]&4)?0xFF:0;
                        else             valc=val;
                        if (gdcreg[1]&8) vald=(gdcreg[0]&8)?0xFF:0;
                        else             vald=val;

                        switch (gdcreg[3]&0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                                if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                break;
                        }
//                        pclog("- %02X %02X %02X %02X   %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                }
                break;
                case 2:
                if (!(gdcreg[3]&0x18) && !gdcreg[1])
                {
                        if (writemask2&1) vram[addr]=(((val&1)?0xFF:0)&gdcreg[8])|(la&~gdcreg[8]);
                        if (writemask2&2) vram[addr|0x1]=(((val&2)?0xFF:0)&gdcreg[8])|(lb&~gdcreg[8]);
                        if (writemask2&4) vram[addr|0x2]=(((val&4)?0xFF:0)&gdcreg[8])|(lc&~gdcreg[8]);
                        if (writemask2&8) vram[addr|0x3]=(((val&8)?0xFF:0)&gdcreg[8])|(ld&~gdcreg[8]);
                }
                else
                {
                        vala=((val&1)?0xFF:0);
                        valb=((val&2)?0xFF:0);
                        valc=((val&4)?0xFF:0);
                        vald=((val&8)?0xFF:0);
                        switch (gdcreg[3]&0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                                if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                break;
                        }
                }
                break;
                case 3:
                if (gdcreg[3]&7) val=svga_rotate[gdcreg[3]&7][val];
                wm=gdcreg[8];
                gdcreg[8]&=val;

                vala=(gdcreg[0]&1)?0xFF:0;
                valb=(gdcreg[0]&2)?0xFF:0;
                valc=(gdcreg[0]&4)?0xFF:0;
                vald=(gdcreg[0]&8)?0xFF:0;
                switch (gdcreg[3]&0x18)
                {
                        case 0: /*Set*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                        break;
                        case 8: /*AND*/
                        if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                        if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                        if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                        if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                        break;
                        case 0x10: /*OR*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                        break;
                        case 0x18: /*XOR*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                        break;
                }
                gdcreg[8]=wm;
                break;
        }
}

uint8_t gd5429_read_linear(uint32_t addr, void *priv)
{
        uint8_t temp,temp2,temp3,temp4;
  
        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

        egareads++;
        
        if (chain4) 
        { 
                addr &= 0x7fffff;
                if (addr >= svga_vram_limit)
                   return 0xff;
                return vram[addr & 0x7fffff]; 
        }
        else        addr<<=2;

        addr &= 0x7fffff;
        
        if (addr >= svga_vram_limit)
           return 0xff;

        la=vram[addr];
        lb=vram[addr|0x1];
        lc=vram[addr|0x2];
        ld=vram[addr|0x3];
        if (readmode)
        {
                temp= (colournocare&1) ?0xFF:0;
                temp&=la;
                temp^=(colourcompare&1)?0xFF:0;
                temp2= (colournocare&2) ?0xFF:0;
                temp2&=lb;
                temp2^=(colourcompare&2)?0xFF:0;
                temp3= (colournocare&4) ?0xFF:0;
                temp3&=lc;
                temp3^=(colourcompare&4)?0xFF:0;
                temp4= (colournocare&8) ?0xFF:0;
                temp4&=ld;
                temp4^=(colourcompare&8)?0xFF:0;
                return ~(temp|temp2|temp3|temp4);
        }
//printf("Read %02X %04X %04X\n",vram[addr|readplane],addr,readplane);
        return vram[addr|readplane];
}

void gd5429_start_blit(uint32_t cpu_dat, int count)
{
        pclog("gd5429_start_blit %i\n", count);
        if (count == -1)
        {
                gd5429.blt.dst_addr_backup = gd5429.blt.dst_addr;
                gd5429.blt.src_addr_backup = gd5429.blt.src_addr;
                gd5429.blt.width_backup    = gd5429.blt.width;
                gd5429.blt.height_internal = gd5429.blt.height;
                gd5429.blt.x_count         = gd5429.blt.mask & 7;
                pclog("gd5429_start_blit : size %i, %i\n", gd5429.blt.width, gd5429.blt.height);
                
                if (gd5429.blt.mode & 0x04)
                {
//                        pclog("blt.mode & 0x04\n");
                        mem_removehandler(0xa0000, 0x10000, gd5429_read, NULL, NULL, gd5429_write, NULL, NULL,  NULL);
                        mem_sethandler(0xa0000, 0x10000, NULL, NULL, NULL, NULL, gd5429_blt_write_w, gd5429_blt_write_l,  NULL);
                        return;
                }
                else
                {
                        mem_removehandler(0xa0000, 0x10000, NULL, NULL, NULL, NULL, gd5429_blt_write_w, gd5429_blt_write_l,  NULL);
                        gd5429_recalc_mapping();
                }                
        }
        
        while (count)
        {
                uint8_t src, dst;
                int mask;
                
                if (gd5429.blt.mode & 0x04)
                {
                        if (gd5429.blt.mode & 0x80)
                        {
                                src = (cpu_dat & 0x80) ? gd5429.blt.fg_col : gd5429.blt.bg_col;
                                mask = cpu_dat & 0x80;
                                cpu_dat <<= 1;
                                count--;
                        }
                        else
                        {
                                src = cpu_dat & 0xff;
                                cpu_dat >>= 8;
                                count -= 8;
                                mask = 1;
                        }
                }
                else
                {
                        switch (gd5429.blt.mode & 0xc0)
                        {
                                case 0x00:
                                src = vram[gd5429.blt.src_addr & vrammask];
                                gd5429.blt.src_addr += ((gd5429.blt.mode & 0x01) ? -1 : 1);
                                mask = 1;
                                break;
                                case 0x40:
                                src = vram[(gd5429.blt.src_addr & (vrammask & ~7)) | (gd5429.blt.dst_addr & 7)];
                                mask = 1;
                                break;
                                case 0x80:
                                mask = vram[gd5429.blt.src_addr & vrammask] & (0x80 >> gd5429.blt.x_count);
                                src = mask ? gd5429.blt.fg_col : gd5429.blt.bg_col;
                                gd5429.blt.x_count++;
                                if (gd5429.blt.x_count == 8)
                                {
                                        gd5429.blt.x_count = 0;
                                        gd5429.blt.src_addr++;
                                }
                                break;
                                case 0xc0:
                                mask = vram[gd5429.blt.src_addr & vrammask] & (0x80 >> (gd5429.blt.dst_addr & 7));
                                src = mask ? gd5429.blt.fg_col : gd5429.blt.bg_col;
                                break;
                        }
                        count--;                        
                }
                dst = vram[gd5429.blt.dst_addr & vrammask];
                changedvram[(gd5429.blt.dst_addr & vrammask) >> 10] = changeframecount;
               
                pclog("Blit %i,%i %06X %06X  %06X %02X %02X  %02X %02X ", gd5429.blt.width, gd5429.blt.height_internal, gd5429.blt.src_addr, gd5429.blt.dst_addr, gd5429.blt.src_addr & vrammask, vram[gd5429.blt.src_addr & vrammask], 0x80 >> (gd5429.blt.dst_addr & 7), src, dst);
                switch (gd5429.blt.rop)
                {
                        case 0x00: dst = 0;             break;
                        case 0x05: dst =   src &  dst;  break;
                        case 0x06: dst =   dst;         break;
                        case 0x09: dst =   src & ~dst;  break;
                        case 0x0b: dst = ~ dst;         break;
                        case 0x0d: dst =   src;         break;
                        case 0x0e: dst = 0xff;          break;
                        case 0x50: dst = ~ src &  dst;  break;
                        case 0x59: dst =   src ^  dst;  break;
                        case 0x6d: dst =   src |  dst;  break;
                        case 0x90: dst = ~(src |  dst); break;
                        case 0x95: dst = ~(src ^  dst); break;
                        case 0xad: dst =   src | ~dst;  break;
                        case 0xd0: dst =  ~src;         break;
                        case 0xd6: dst =  ~src |  dst;  break;
                        case 0xda: dst = ~(src &  dst); break;                       
                }
                pclog("%02X  %02X\n", dst, mask);
                
                if ((gd5429.blt.width_backup - gd5429.blt.width) >= (gd5429.blt.mask & 7) &&
                    !((gd5429.blt.mode & 0x08) && !mask))
                        vram[gd5429.blt.dst_addr & vrammask] = dst;
                
                gd5429.blt.dst_addr += ((gd5429.blt.mode & 0x01) ? -1 : 1);
                
                gd5429.blt.width--;
                
                if (gd5429.blt.width == 0xffff)
                {
                        gd5429.blt.width = gd5429.blt.width_backup;

                        gd5429.blt.dst_addr = gd5429.blt.dst_addr_backup = gd5429.blt.dst_addr_backup + ((gd5429.blt.mode & 0x01) ? -gd5429.blt.dst_pitch : gd5429.blt.dst_pitch);                        
                        
                        switch (gd5429.blt.mode & 0xc0)
                        {
                                case 0x00:
                                gd5429.blt.src_addr = gd5429.blt.src_addr_backup = gd5429.blt.src_addr_backup + ((gd5429.blt.mode & 0x01) ? -gd5429.blt.src_pitch : gd5429.blt.src_pitch);
                                break;
                                case 0x40:
                                gd5429.blt.src_addr = ((gd5429.blt.src_addr + ((gd5429.blt.mode & 0x01) ? -8 : 8)) & 0x38) | (gd5429.blt.src_addr & ~0x38);
                                break;
                                case 0x80:
                                if (gd5429.blt.x_count != 0)
                                {
                                        gd5429.blt.x_count = 0;
                                        gd5429.blt.src_addr++;
                                }
                                break;
                                case 0xc0:
                                gd5429.blt.src_addr = ((gd5429.blt.src_addr + ((gd5429.blt.mode & 0x01) ? -1 : 1)) & 7) | (gd5429.blt.src_addr & ~7);
                                break;
                        }
                        
                        gd5429.blt.height_internal--;
                        if (gd5429.blt.height_internal == 0xffff)
                        {
                                if (gd5429.blt.mode & 0x04)
                                {
                                        mem_removehandler(0xa0000, 0x10000, NULL, NULL, NULL, NULL, gd5429_blt_write_w, gd5429_blt_write_l,  NULL);
                                        gd5429_recalc_mapping();
                                }
                                return;
                        }
                                
                        if (gd5429.blt.mode & 0x04)
                                return;
                }                        
        }
}

void gd5429_mmio_write(uint32_t addr, uint8_t val, void *priv)
{
        pclog("MMIO write %08X %02X\n", addr, val);
        switch (addr & 0xff)
        {
                case 0x00:
                gd5429.blt.bg_col = (gd5429.blt.bg_col & 0xff00) | val;
                break;
                case 0x01:
                gd5429.blt.bg_col = (gd5429.blt.bg_col & 0x00ff) | (val << 8);
                break;

                case 0x04:
                gd5429.blt.fg_col = (gd5429.blt.fg_col & 0xff00) | val;
                break;
                case 0x05:
                gd5429.blt.fg_col = (gd5429.blt.fg_col & 0x00ff) | (val << 8);
                break;

                case 0x08:
                gd5429.blt.width = (gd5429.blt.width & 0xff00) | val;
                break;
                case 0x09:
                gd5429.blt.width = (gd5429.blt.width & 0x00ff) | (val << 8);
                break;
                case 0x0a:
                gd5429.blt.height = (gd5429.blt.height & 0xff00) | val;
                break;
                case 0x0b:
                gd5429.blt.height = (gd5429.blt.height & 0x00ff) | (val << 8);
                break;
                case 0x0c:
                gd5429.blt.dst_pitch = (gd5429.blt.dst_pitch & 0xff00) | val;
                break;
                case 0x0d:
                gd5429.blt.dst_pitch = (gd5429.blt.dst_pitch & 0x00ff) | (val << 8);
                break;
                case 0x0e:
                gd5429.blt.src_pitch = (gd5429.blt.src_pitch & 0xff00) | val;
                break;
                case 0x0f:
                gd5429.blt.src_pitch = (gd5429.blt.src_pitch & 0x00ff) | (val << 8);
                break;
                
                case 0x10:
                gd5429.blt.dst_addr = (gd5429.blt.dst_addr & 0xffff00) | val;
                break;
                case 0x11:
                gd5429.blt.dst_addr = (gd5429.blt.dst_addr & 0xff00ff) | (val << 8);
                break;
                case 0x12:
                gd5429.blt.dst_addr = (gd5429.blt.dst_addr & 0x00ffff) | (val << 16);
                break;

                case 0x14:
                gd5429.blt.src_addr = (gd5429.blt.src_addr & 0xffff00) | val;
                break;
                case 0x15:
                gd5429.blt.src_addr = (gd5429.blt.src_addr & 0xff00ff) | (val << 8);
                break;
                case 0x16:
                gd5429.blt.src_addr = (gd5429.blt.src_addr & 0x00ffff) | (val << 16);
                break;

                case 0x17:
                gd5429.blt.mask = val;
                break;
                case 0x18:
                gd5429.blt.mode = val;
                break;
                
                case 0x1a:
                gd5429.blt.rop = val;
                break;
                
                case 0x40:
                if (val & 0x02)
                        gd5429_start_blit(0, -1);
                break;
        }
}

uint8_t gd5429_mmio_read(uint32_t addr, void *priv)
{
        pclog("MMIO read %08X\n", addr);
        switch (addr & 0xff)
        {
                case 0x40: /*BLT status*/
                return 0;
        }
        return 0xff; /*All other registers read-only*/
}

void gd5429_blt_write_w(uint32_t addr, uint16_t val, void *priv)
{
        pclog("gd5429_blt_write_w %08X %08X\n", addr, val);
        gd5429_start_blit(val, 16);
}

void gd5429_blt_write_l(uint32_t addr, uint32_t val, void *priv)
{
        pclog("gd5429_blt_write_l %08X %08X  %04X %04X\n", addr, val,  ((val >> 8) & 0x00ff) | ((val << 8) & 0xff00), ((val >> 24) & 0x00ff) | ((val >> 8) & 0xff00));
        if ((gd5429.blt.mode & 0x84) == 0x84)
        {
                gd5429_start_blit( val        & 0xff, 8);
                gd5429_start_blit((val >> 8)  & 0xff, 8);
                gd5429_start_blit((val >> 16) & 0xff, 8);
                gd5429_start_blit((val >> 24) & 0xff, 8);
        }
        else
                gd5429_start_blit(val, 32);
}

GFXCARD vid_gd5429 =
{
        gd5429_init,
        /*IO at 3Cx/3Dx*/
        gd5429_out,
        gd5429_in,
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
