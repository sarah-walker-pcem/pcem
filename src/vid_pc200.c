/*PC200 video emulation.
  CGA with some NMI stuff. But we don't need that as it's only used for TV and
  LCD displays, and we're emulating a CRT*/
#include "ibm.h"
#include "video.h"
#include "vid_cga.h"

uint8_t pc200_3dd, pc200_3de, pc200_3df;

void pc200_out(uint16_t addr, uint8_t val)
{
        uint8_t old;
        switch (addr)
        {
                case 0x3D5:
                if (!(pc200_3de&0x40) && crtcreg<=11)
                {
                        if (pc200_3de&0x80) nmi=1;
                        pc200_3dd=0x20|(crtcreg&0x1F);
                        pc200_3df=val;
                        return;
                }
                old=crtc[crtcreg];
                crtc[crtcreg]=val&crtcmask[crtcreg];
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                cga_recalctimings();
                        }
                }
                return;
                
                case 0x3D8:
                old = cgamode;
                cgamode=val;
                if ((cgamode ^ old) & 3)
                   cga_recalctimings();
                pc200_3dd|=0x80;
                if (pc200_3de&0x80)
                   nmi=1;
                return;
                
                case 0x3DE:
                pc200_3de=val;
                pc200_3dd=0x1F;
                if (val&0x80) pc200_3dd|=0x40;
                return;
        }
        cga_out(addr,val);
}

uint8_t pc200_in(uint16_t addr)
{
        uint8_t temp;
        switch (addr)
        {
                case 0x3D8:
                return cgamode;
                
                case 0x3DD:
                temp = pc200_3dd;
                pc200_3dd &= 0x1F;
                return temp;
                
                case 0x3DE:
                return (pc200_3de&0xC7)| 0x18; /*External CGA*/
                
                case 0x3DF:
                return pc200_3df;
        }
        return cga_in(addr);
}

int pc200_init()
{
        mem_sethandler(0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL);
        return cga_init();
}

GFXCARD vid_pc200 =
{
        pc200_init,
        /*IO at 3Cx/3Dx*/
        pc200_out,
        pc200_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        cga_poll,
        cga_recalctimings,

        video_write_null,
        video_write_null,
        cga_write,

        video_read_null,
        video_read_null,
        cga_read
};
