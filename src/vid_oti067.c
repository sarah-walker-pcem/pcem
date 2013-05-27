/*Oak OTI067 emulation*/
#include "ibm.h"
#include "io.h"
#include "video.h"
#include "vid_svga.h"

static int oti067_index;
static uint8_t oti067_regs[32];

void oti067_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;

//        pclog("oti067_out : %04X %02X  %02X %i\n", addr, val, ram[0x489], ins);
                
        if ((((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga_miscout&1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x3D4:
                crtcreg=val&31;
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

                case 0x3DE: oti067_index=val&0x1F; return;
                case 0x3DF:
                oti067_regs[oti067_index]=val;
                switch (oti067_index)
                {
                        case 0xD:
                        vrammask=(val&0xC)?0x7FFFF:0x3FFFF;
                        break;
                        case 0x11:
                        svgarbank=(val&0xF)*65536;
                        svgawbank=(val>>4)*65536;
                        break;
                }
                return;
        }
        svga_out(addr, val, NULL);
}

uint8_t oti067_in(uint16_t addr, void *priv)
{
        uint8_t temp;
        
//        if (addr != 0x3da && addr != 0x3ba) pclog("oti067_in : %04X ", addr);
        
        if ((((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga_miscout&1)) addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3D4:
                temp = crtcreg;
                break;
                case 0x3D5:
                temp = crtc[crtcreg];
                break;
                
                case 0x3DE: 
                temp = oti067_index|(2<<5);
                break;               
                case 0x3DF: 
                if (oti067_index==0x10) temp = 0x18;
                else if (oti067_index==0xD) temp = oti067_regs[oti067_index]|0xC0;
                else                            temp = oti067_regs[oti067_index];
                break;

                default:
                temp = svga_in(addr, NULL);
                break;
        }
//        if (addr != 0x3da && addr != 0x3ba) pclog("%02X  %04X:%04X\n", temp, CS,pc);        
        return temp;
}

void oti067_recalctimings()
{
        if (oti067_regs[0x14]&0x08) svga_ma|=0x10000;
        if (oti067_regs[0x0D]&0x0C) svga_rowoffset<<=1;
        svga_interlace = oti067_regs[0x14]&0x80;
}

int oti067_init()
{
        svga_recalctimings_ex = oti067_recalctimings;
        svga_vram_limit = 1 << 19; /*512kb*/
        vrammask = 0x7ffff;
        bpp = 8;
        svga_miscout = 1;
        return svga_init();
}

GFXCARD vid_oti067 =
{
        oti067_init,
        /*IO at 3Cx/3Dx*/
        oti067_out,
        oti067_in,
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
