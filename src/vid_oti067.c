/*Oak OTI067 emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "video.h"
#include "vid_oti067.h"
#include "vid_svga.h"

typedef struct oti067_t
{
        svga_t svga;
        
        int index;
        uint8_t regs[32];
} oti067_t;

void oti067_out(uint16_t addr, uint8_t val, void *p)
{
        oti067_t *oti067 = (oti067_t *)p;
        svga_t *svga = &oti067->svga;
        uint8_t old;

//        pclog("oti067_out : %04X %02X  %02X %i\n", addr, val, ram[0x489], ins);
                
        if ((((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga->miscout & 1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x3D4:
                svga->crtcreg = val & 31;
                return;
                case 0x3D5:
                if (svga->crtcreg <= 7 && svga->crtc[0x11] & 0x80) return;
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                if (old != val)
                {
                        if (svga->crtcreg < 0xE || svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;

                case 0x3DE: 
                oti067->index = val & 0x1f; 
                return;
                case 0x3DF:
                oti067->regs[oti067->index] = val;
                switch (oti067->index)
                {
                        case 0xD:
                        svga->vrammask = (val & 0xc) ? 0x7ffff : 0x3ffff;
                        break;
                        case 0x11:
                        svga->read_bank = (val & 0xf) * 65536;
                        svga->write_bank = (val >> 4) * 65536;
                        break;
                }
                return;
        }
        svga_out(addr, val, svga);
}

uint8_t oti067_in(uint16_t addr, void *p)
{
        oti067_t *oti067 = (oti067_t *)p;
        svga_t *svga = &oti067->svga;
        uint8_t temp;
        
//        if (addr != 0x3da && addr != 0x3ba) pclog("oti067_in : %04X ", addr);
        
        if ((((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga->miscout & 1)) addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3D4:
                temp = svga->crtcreg;
                break;
                case 0x3D5:
                temp = svga->crtc[svga->crtcreg];
                break;
                
                case 0x3DE: 
                temp = oti067->index | (2 << 5);
                break;               
                case 0x3DF: 
                if (oti067->index==0x10)     temp = 0x18;
                else if (oti067->index==0xD) temp = oti067->regs[oti067->index]|0xC0;
                else                         temp = oti067->regs[oti067->index];
                break;

                default:
                temp = svga_in(addr, svga);
                break;
        }
//        if (addr != 0x3da && addr != 0x3ba) pclog("%02X  %04X:%04X\n", temp, CS,pc);        
        return temp;
}

void oti067_recalctimings(svga_t *svga)
{
        oti067_t *oti067 = (oti067_t *)svga->p;
        
        if (oti067->regs[0x14] & 0x08) svga->ma_latch |= 0x10000;
        if (oti067->regs[0x0d] & 0x0c) svga->rowoffset <<= 1;
        svga->interlace = oti067->regs[0x14] & 0x80;
}

void *oti067_init()
{
        oti067_t *oti067 = malloc(sizeof(oti067_t));
        memset(oti067, 0, sizeof(oti067_t));
        
        svga_init(&oti067->svga, oti067, 1 << 19, /*512kb*/
                   oti067_recalctimings,
                   oti067_in, oti067_out,
                   NULL);

        io_sethandler(0x03c0, 0x0020, oti067_in, NULL, NULL, oti067_out, NULL, NULL, oti067);

        oti067->svga.miscout = 1;
        return oti067;
}

void oti067_close(void *p)
{
        oti067_t *oti067 = (oti067_t *)p;

        svga_close(&oti067->svga);
        
        free(oti067);
}

void oti067_speed_changed(void *p)
{
        oti067_t *oti067 = (oti067_t *)p;
        
        svga_recalctimings(&oti067->svga);
}
        
void oti067_force_redraw(void *p)
{
        oti067_t *oti067 = (oti067_t *)p;

        oti067->svga.fullchange = changeframecount;
}

device_t oti067_device =
{
        "Oak OTI-067",
        oti067_init,
        oti067_close,
        NULL,
        oti067_speed_changed,
        oti067_force_redraw,
        svga_add_status_info
};
