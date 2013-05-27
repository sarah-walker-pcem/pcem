/*Paradise VGA emulation

  PC2086, PC3086 use PVGA1A
  MegaPC uses W90C11A
  */
#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_unk_ramdac.h"

void    paradise_write(uint32_t addr, uint8_t val, void *priv);
uint8_t paradise_read(uint32_t addr, void *priv);
void paradise_remap();

enum
{
        PVGA1A = 0,
        WD90C11
};

static int paradise_type;

static uint32_t paradise_bank_r[4], paradise_bank_w[4];

void paradise_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
//        output = 3;
        pclog("Paradise out %04X %02X %04X:%04X\n", addr, val, CS, pc);
        switch (addr)
        {
                case 0x3c5:
                if (seqaddr > 7)                        
                {
                        if (paradise_type < WD90C11 || seqregs[6] != 0x48) 
                           return;
                        seqregs[seqaddr & 0x1f] = val;
                        if (seqaddr == 0x11)
                           paradise_remap();
                        return;
                }
                break;

                case 0x3cf:
                if (gdcaddr >= 0x9 && gdcaddr < 0xf)
                {
                        if ((gdcreg[0xf] & 7) != 5)
                           return;
                }
                if (gdcaddr == 6)
                {
                        if ((gdcreg[6] & 0xc) != (val & 0xc))
                        {
                                mem_removehandler(0xa0000, 0x20000, paradise_read, NULL, NULL, paradise_write, NULL, NULL,  NULL);
//                                pclog("Write mapping %02X\n", val);
                                switch (val&0xC)
                                {
                                        case 0x0: /*128k at A0000*/
                                        mem_sethandler(0xa0000, 0x20000, paradise_read, NULL, NULL, paradise_write, NULL, NULL,  NULL);
                                        break;
                                        case 0x4: /*64k at A0000*/
                                        mem_sethandler(0xa0000, 0x10000, paradise_read, NULL, NULL, paradise_write, NULL, NULL,  NULL);
                                        break;
                                        case 0x8: /*32k at B0000*/
                                        mem_sethandler(0xb0000, 0x08000, paradise_read, NULL, NULL, paradise_write, NULL, NULL,  NULL);
                                        break;
                                        case 0xC: /*32k at B8000*/
                                        mem_sethandler(0xb8000, 0x08000, paradise_read, NULL, NULL, paradise_write, NULL, NULL,  NULL);
                                        break;
                                }
                        }
                        gdcreg[6] = val;
                        paradise_remap();
                        return;
                }
                if (gdcaddr == 0x9 || gdcaddr == 0xa)
                {
                        gdcreg[gdcaddr] = val;
                        paradise_remap();
                        return;
                }
                if (gdcaddr == 0xe)
                {
                        gdcreg[0xe] = val;
                        paradise_remap();
                        return;
                }
                break;
                
                case 0x3D4:
                if (paradise_type == PVGA1A)
                   crtcreg = val & 0x1f;
                else
                   crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if (crtcreg <= 7 && crtc[0x11] & 0x80) 
                   return;
                if (crtcreg > 0x29 && (crtc[0x29] & 7) != 5)
                   return;
                if (crtcreg >= 0x31 && crtcreg <= 0x37)
                   return;
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

uint8_t paradise_in(uint16_t addr, void *priv)
{
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
//        if (addr != 0x3da) pclog("Paradise in %04X\n", addr);
        switch (addr)
        {
                case 0x3c5:
                if (seqaddr > 7)
                {
                        if (paradise_type < WD90C11 || seqregs[6] != 0x48) 
                           return 0xff;
                        if (seqaddr > 0x12) 
                           return 0xff;
                        return seqregs[seqaddr & 0x1f];
                }
                break;
                        
                case 0x3cf:
                if (gdcaddr >= 0x9 && gdcaddr < 0xf)
                {
                        if (gdcreg[0xf] & 0x10)
                           return 0xff;
                        switch (gdcaddr)
                        {
                                case 0xf:
                                return (gdcreg[0xf] & 0x17) | 0x80;
                        }
                }
                break;

                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                if (crtcreg > 0x29 && crtcreg < 0x30 && (crtc[0x29] & 0x88) != 0x80)
                   return 0xff;
                return crtc[crtcreg];
        }
        return svga_in(addr, NULL);
}

void paradise_remap()
{
        if (seqregs[0x11] & 0x80)
        {
//                pclog("Remap 1\n");
                paradise_bank_r[0] = paradise_bank_r[2] =  (gdcreg[0x9] & 0x7f) << 12;
                paradise_bank_r[1] = paradise_bank_r[3] = ((gdcreg[0x9] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
                paradise_bank_w[0] = paradise_bank_w[2] =  (gdcreg[0xa] & 0x7f) << 12;
                paradise_bank_w[1] = paradise_bank_w[3] = ((gdcreg[0xa] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
        }
        else if (gdcreg[0xe] & 0x08)
        {
                if (gdcreg[0x6] & 0xc)
                {
//                pclog("Remap 2\n");                        
                        paradise_bank_r[0] = paradise_bank_r[2] =  (gdcreg[0xa] & 0x7f) << 12;
                        paradise_bank_w[0] = paradise_bank_w[2] =  (gdcreg[0xa] & 0x7f) << 12;
                        paradise_bank_r[1] = paradise_bank_r[3] = ((gdcreg[0x9] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
                        paradise_bank_w[1] = paradise_bank_w[3] = ((gdcreg[0x9] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
                }
                else
                {
//                pclog("Remap 3\n");
                        paradise_bank_r[0] = paradise_bank_w[0] =  (gdcreg[0xa] & 0x7f) << 12;
                        paradise_bank_r[1] = paradise_bank_w[1] = ((gdcreg[0xa] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
                        paradise_bank_r[2] = paradise_bank_w[2] =  (gdcreg[0x9] & 0x7f) << 12;
                        paradise_bank_r[3] = paradise_bank_w[3] = ((gdcreg[0x9] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
                }
        }
        else
        {
  //              pclog("Remap 4\n");
                paradise_bank_r[0] = paradise_bank_r[2] =  (gdcreg[0x9] & 0x7f) << 12;
                paradise_bank_r[1] = paradise_bank_r[3] = ((gdcreg[0x9] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
                paradise_bank_w[0] = paradise_bank_w[2] =  (gdcreg[0x9] & 0x7f) << 12;
                paradise_bank_w[1] = paradise_bank_w[3] = ((gdcreg[0x9] & 0x7f) << 12) + ((gdcreg[6] & 0x08) ? 0 : 0x8000);
        }
//        pclog("Remap - %04X %04X\n", paradise_bank_r[0], paradise_bank_w[0]);
}

void paradise_recalctimings()
{
        svga_lowres = !(gdcreg[0xe] & 0x01);
}

#define egacycles 1
#define egacycles2 1
void paradise_write(uint32_t addr, uint8_t val, void *priv)
{
//        pclog("paradise_write : %05X %02X  ", addr, val);
        addr = (addr & 0x7fff) + paradise_bank_w[(addr >> 15) & 3];
//        pclog("%08X\n", addr);
        svga_write_linear(addr, val, priv);
}

uint8_t paradise_read(uint32_t addr, void *priv)
{
//        pclog("paradise_read : %05X ", addr);
        addr = (addr & 0x7fff) + paradise_bank_r[(addr >> 15) & 3];
//        pclog("%08X\n", addr);
        return svga_read_linear(addr, priv);
}

int paradise_init()
{
        if (romset == ROM_PC2086 || romset == ROM_PC3086)
        {
                paradise_type = PVGA1A;               
                vrammask = 0x3ffff;
                pclog("Init PVGA1A\n");
                svga_vram_limit = 1 << 18; /*256kb*/
        }
        if (romset == ROM_MEGAPC)
        {
                paradise_type = WD90C11;
                vrammask = 0x7ffff;
                crtc[0x36] = '1';
                crtc[0x37] = '1';
                pclog("Init WD90C11\n");
                svga_vram_limit = 1 << 19; /*512kb*/
        }
        crtc[0x31] = 'W';
        crtc[0x32] = 'D';
        crtc[0x33] = '9';
        crtc[0x34] = '0';
        crtc[0x35] = 'C';                                
        svga_recalctimings_ex = paradise_recalctimings;
        return svga_init();
}

GFXCARD vid_paradise =
{
        paradise_init,
        /*IO at 3Cx/3Dx*/
        paradise_out,
        paradise_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        svga_poll,
        svga_recalctimings,

        paradise_write,
        video_write_null,
        video_write_null,

        paradise_read,
        video_read_null,
        video_read_null
};
