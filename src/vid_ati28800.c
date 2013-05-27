/*ATI 28800 emulation (VGA Charger)*/
#include "ibm.h"
#include "io.h"
#include "video.h"
#include "vid_ati_eeprom.h"
#include "vid_svga.h"
#include "vid_svga_render.h"

static uint8_t ati_regs[256];
static int ati_index;

void ati28800_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;
        
        pclog("ati28800_out : %04X %02X  %04X:%04X\n", addr, val, CS,pc);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x1ce:
                ati_index = val;
                break;
                case 0x1cf:
                ati_regs[ati_index] = val;
                switch (ati_index)
                {
                        case 0xb2:
                        case 0xbe:
                        if (ati_regs[0xbe] & 8) /*Read/write bank mode*/
                        {
                                svgarbank = ((ati_regs[0xb2] >> 5) & 7) * 0x10000;
                                svgawbank = ((ati_regs[0xb2] >> 1) & 7) * 0x10000;
                        }
                        else                    /*Single bank mode*/
                                svgarbank = svgawbank = ((ati_regs[0xb2] >> 1) & 7) * 0x10000;
                        break;
                        case 0xb3:
                        ati_eeprom_write(val & 8, val & 2, val & 1);
                        break;
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

uint8_t ati28800_in(uint16_t addr, void *priv)
{
        uint8_t temp;

        if (addr != 0x3da) pclog("ati28800_in : %04X ", addr);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
             
        switch (addr)
        {
                case 0x1ce:
                temp = ati_index;
                break;
                case 0x1cf:
                switch (ati_index)
                {
                        case 0xb7:
                        temp = ati_regs[ati_index] & ~8;
                        if (ati_eeprom_read())
                                temp |= 8;
                        break;
                        
                        default:
                        temp = ati_regs[ati_index];
                        break;
                }
                break;

                case 0x3c2:
                if ((vgapal[0].r + vgapal[0].g + vgapal[0].b) >= 0x50)
                        temp = 0;
                else
                        temp = 0x10;
                break;
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

void ati28800_recalctimings()
{
        pclog("ati28800_recalctimings\n");
        if (!scrblank && (ati_regs[0xb0] & 0x20)) /*Extended 256 colour modes*/
        {
                pclog("8bpp_highres\n");
                svga_render = svga_render_8bpp_highres;
                svga_rowoffset <<= 1;
                svga_ma <<= 1;
        }
}               

int ati28800_init()
{
        svga_recalctimings_ex = ati28800_recalctimings;
        svga_vram_limit = 1 << 19; /*512kb*/
        vrammask = 0x7ffff;
        svgawbank = svgarbank = 0;
        bpp = 8;
        svga_miscout = 1;
        
        io_sethandler(0x01ce, 0x0002, ati28800_in, NULL, NULL, ati28800_out, NULL, NULL,  NULL);
        
        ati_eeprom_load("ati28800.nvr", 0);
        
        return svga_init();
}

GFXCARD vid_ati28800 =
{
        ati28800_init,
        /*IO at 3Cx/3Dx*/
        ati28800_out,
        ati28800_in,
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
