/*ET4000 emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_unk_ramdac.h"

#include "vid_et4000.h"

typedef struct et4000_t
{
        svga_t svga;
        unk_ramdac_t ramdac;
        
        uint8_t banking;
} et4000_t;

void et4000_out(uint16_t addr, uint8_t val, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        svga_t *svga = &et4000->svga;
                
        uint8_t old;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

        pclog("ET4000 out %04X %02X\n", addr, val);
                
        switch (addr)
        {
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                unk_ramdac_out(addr, val, &et4000->ramdac, svga);
                return;
                
                case 0x3CD: /*Banking*/
                svga->write_bank = (val & 0xf) * 0x10000;
                svga->read_bank = ((val >> 4) & 0xf) * 0x10000;
                et4000->banking = val;
                pclog("Banking write %08X %08X %02X\n", svga->write_bank, svga->read_bank, val);
                return;
                case 0x3D4:
                svga->crtcreg = val & 0x3f;
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
        }
        svga_out(addr, val, svga);
}

uint8_t et4000_in(uint16_t addr, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        svga_t *svga = &et4000->svga;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) 
                addr ^= 0x60;
        
        if (addr != 0x3da) pclog("IN ET4000 %04X\n", addr);
        
        switch (addr)
        {
                case 0x3C5:
                if ((svga->seqaddr & 0xf) == 7) return svga->seqregs[svga->seqaddr & 0xf] | 4;
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                return unk_ramdac_in(addr, &et4000->ramdac, svga);
                
                case 0x3CD: /*Banking*/
                return et4000->banking;
                case 0x3D4:
                return svga->crtcreg;
                case 0x3D5:
                return svga->crtc[svga->crtcreg];
        }
        return svga_in(addr, svga);
}

void et4000_recalctimings(svga_t *svga)
{
        et4000_t *et4000 = (et4000_t *)svga->p;

        svga->ma_latch |= (svga->crtc[0x33]&3)<<16;
        if (svga->crtc[0x35] & 2)    svga->vtotal += 0x400;
        if (svga->crtc[0x35] & 4)    svga->dispend += 0x400;
        if (svga->crtc[0x35] & 8)    svga->vsyncstart += 0x400;
        if (svga->crtc[0x35] & 0x10) svga->split += 0x400;
        if (!svga->rowoffset)        svga->rowoffset = 0x100;
        if (svga->crtc[0x3f] & 1)    svga->htotal += 256;
        if (svga->attrregs[0x16] & 0x20) svga->hdisp <<= 1;

//        pclog("Rowoffset %i\n",svga_rowoffset);

        switch (((svga->miscout >> 2) & 3) | ((svga->crtc[0x34] << 1) & 4))
        {
                case 0: case 1: break;
                case 3: svga->clock = cpuclock / 40000000.0; break;
                case 5: svga->clock = cpuclock / 65000000.0; break;
                default: svga->clock = cpuclock / 36000000.0; break;
        }

}

void *et4000_init()
{
        et4000_t *et4000 = malloc(sizeof(et4000_t));
        memset(et4000, 0, sizeof(et4000_t));
        
        io_sethandler(0x03c0, 0x0020, et4000_in, NULL, NULL, et4000_out, NULL, NULL, et4000);

        svga_init(&et4000->svga, et4000, 1 << 20, /*1mb*/
                   et4000_recalctimings,
                   et4000_in, et4000_out,
                   NULL);
        
        return et4000;
}

void et4000_close(void *p)
{
        et4000_t *et4000 = (et4000_t *)p;

        svga_close(&et4000->svga);
        
        free(et4000);
}

void et4000_speed_changed(void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        
        svga_recalctimings(&et4000->svga);
}

void et4000_force_redraw(void *p)
{
        et4000_t *et4000 = (et4000_t *)p;

        et4000->svga.fullchange = changeframecount;
}

device_t et4000_device =
{
        "Tseng Labs ET4000AX",
        et4000_init,
        et4000_close,
        et4000_speed_changed,
        et4000_force_redraw,
        svga_add_status_info
};
