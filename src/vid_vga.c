/*IBM VGA emulation*/
#include "ibm.h"
#include "io.h"
#include "video.h"
#include "vid_svga.h"

void vga_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;
        
        pclog("vga_out : %04X %02X  %04X:%04X  %02X  %i\n", addr, val, CS,pc, ram[0x489], ins);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x3D4:
                crtcreg = val & 0x1f;
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

uint8_t vga_in(uint16_t addr, void *priv)
{
        uint8_t temp;

        if (addr != 0x3da) pclog("vga_in : %04X ", addr);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
             
        switch (addr)
        {
                case 0x3D4:
                temp = crtcreg;
                break;
                case 0x3D5:
                temp = crtc[crtcreg];
                break;
                default:
                temp = svga_in(addr, NULL);
                break;
        }
        if (addr != 0x3da) pclog("%02X  %04X:%04X\n", temp, CS,pc);
        return temp;
}

int vga_init()
{
        svga_recalctimings_ex = NULL;
        svga_vram_limit = 1 << 18; /*256kb*/
        vrammask = 0x3ffff;
        svgawbank = svgarbank = 0;
        bpp = 8;
        svga_miscout = 1;
        return svga_init();
}

GFXCARD vid_vga =
{
        vga_init,
        /*IO at 3Cx/3Dx*/
        vga_out,
        vga_in,
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
