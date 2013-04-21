/*Oak OTI067 emulation*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"

static int oti067_index;
static uint8_t oti067_regs[32];

void oti067_out(uint16_t addr, uint8_t val)
{
        uint8_t old;
        
        if ((addr&0xFFF0) == 0x3B0) addr |= 0x20;

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
        svga_out(addr,val);
}

uint8_t oti067_in(uint16_t addr)
{
        uint8_t temp;
        
        if ((addr&0xFFF0) == 0x3B0) addr |= 0x20;
        
        switch (addr)
        {
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
                case 0x3DE: return oti067_index|(2<<5);
                case 0x3DF: 
                if (oti067_index==0x10) return 0x40;
                if (oti067_index==0xD) return oti067_regs[oti067_index]|0xC0;
                return oti067_regs[oti067_index];
        }
        return svga_in(addr);
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
