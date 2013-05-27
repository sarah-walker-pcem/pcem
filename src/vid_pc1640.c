/*PC1640 video emulation.
  Mostly standard EGA, but with CGA & Hercules emulation*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_ega.h"

static int pc1640_cga=1;

void pc1640_out(uint16_t addr, uint8_t val, void *priv)
{
        switch (addr)
        {
                case 0x3DB:
                pc1640_cga=val&0x40;
                mem_removehandler(0xa0000, 0x20000, ega_read, NULL, NULL, ega_write, NULL, NULL,  NULL);
                mem_removehandler(0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL,  NULL);
                if (pc1640_cga)
                   mem_sethandler(0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL,  NULL);
                else
                {                
                        switch (gdcreg[6] & 0xC)
                        {
                                case 0x0: /*128k at A0000*/
                                mem_sethandler(0xa0000, 0x20000, ega_read, NULL, NULL, ega_write, NULL, NULL,  NULL);
                                break;
                                case 0x4: /*64k at A0000*/
                                mem_sethandler(0xa0000, 0x10000, ega_read, NULL, NULL, ega_write, NULL, NULL,  NULL);
                                break;
                                case 0x8: /*32k at B0000*/
                                mem_sethandler(0xb0000, 0x08000, ega_read, NULL, NULL, ega_write, NULL, NULL,  NULL);
                                break;
                                case 0xC: /*32k at B8000*/
                                mem_sethandler(0xb8000, 0x08000, ega_read, NULL, NULL, ega_write, NULL, NULL,  NULL);
                                break;
                        }
                }                
                pclog("3DB write %02X\n", val);
                return;
        }
        if (pc1640_cga) cga_out(addr, val, NULL);
        else            ega_out(addr, val, NULL);
}

uint8_t pc1640_in(uint16_t addr, void *priv)
{
        switch (addr)
        {
        }
        if (pc1640_cga) return cga_in(addr, NULL);
        else            return ega_in(addr, NULL);
}

void pc1640_recalctimings()
{
        if (pc1640_cga) cga_recalctimings();
        else            ega_recalctimings();
}

void pc1640_poll()
{
        if (pc1640_cga) cga_poll();
        else            ega_poll();
}

int pc1640_init()
{
        int r = ega_init();
        
        pc1640_cga = 1;
        
        mem_sethandler(0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL,  NULL);
        
        return r;
}

GFXCARD vid_pc1640 =
{
        pc1640_init,
        /*IO at 3Cx/3Dx*/
        pc1640_out,
        pc1640_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        pc1640_poll,
        pc1640_recalctimings,

        ega_write,
        video_write_null,
        video_write_null,

        ega_read,
        video_read_null,
        video_read_null
};
