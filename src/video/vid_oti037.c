/*Oak OTI037 emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "video.h"
#include "vid_oti037.h"
#include "vid_svga.h"

typedef struct oti037_t
{
        svga_t svga;

        rom_t bios_rom;

        int index;
        uint8_t regs[32];

        uint8_t enable_register;
} oti037_t;

void oti037_out(uint16_t addr, uint8_t val, void *p)
{
        oti037_t *oti037 = (oti037_t *)p;
        svga_t *svga = &oti037->svga;
        uint8_t old;
        uint8_t idx;

        if (!(oti037->enable_register & 1) && addr != 0x3C3)
                return;

        if ((((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga->miscout & 1)) addr ^= 0x60; // mono / color addr selection

//        pclog("oti037_out : %04X %02X  %02X %i ", addr, val, ram[0x489], ins);
//        pclog("  %04X:%04X\n", CS,cpu_state.pc);        
        switch (addr)
        {
                case 0x3C3:
                oti037->enable_register = val & 1;
                return;
                case 0x3D4:
                svga->crtcreg = val /*& 31*/; // fixme: bios wants to set the test bit?
                return;
                case 0x3D5:
                if (((svga->crtcreg & 31) < 7) && (svga->crtc[0x11] & 0x80))
                        return;
                if (((svga->crtcreg & 31) == 7) && (svga->crtc[0x11] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                old = svga->crtc[svga->crtcreg & 31];
                svga->crtc[svga->crtcreg & 31] = val;
                if (old != val)
                {
                        if ((svga->crtcreg & 31) < 0xE || (svga->crtcreg & 31) > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;

                case 0x3DE: 
                oti037->index = val /*& 0x1f*/; 
                return;
                case 0x3DF:
                idx = oti037->index & 0x1f;
                oti037->regs[idx] = val;
                switch (idx)
                {
                        case 0xD:
                        if (val & 0x80)
                                mem_mapping_disable(&svga->mapping);
                        else
                                mem_mapping_enable(&svga->mapping);
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

uint8_t oti037_in(uint16_t addr, void *p)
{
        oti037_t *oti037 = (oti037_t *)p;
        svga_t *svga = &oti037->svga;
        uint8_t temp;

        if (!(oti037->enable_register & 1) && addr != 0x3C3)
                return 0xff;

        if ((((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga->miscout & 1)) addr ^= 0x60;  // mono / color addr selection

//        if (addr != 0x3da && addr != 0x3ba) pclog("oti037_in : %04X ", addr);

        switch (addr)
        {
                case 0x3C3:
                temp = oti037->enable_register;
                break;
                case 0x3D4:
                temp = svga->crtcreg;
                break;
                case 0x3D5:
                temp = svga->crtc[svga->crtcreg & 31];
                break;

                case 0x3DA:
                svga->attrff = 0;
                
                /*The OTI-037C BIOS waits for bits 0 and 3 in 0x3da to go low, then reads 0x3da again
                  and expects the diagnostic bits to equal the current border colour. As I understand
                  it, the 0x3da active enable status does not include the border time, so this may be
                  an area where OTI-037C is not entirely VGA compatible.*/
                svga->cgastat &= ~0x30;
                /* copy color diagnostic info from the overscan color register */
                switch (svga->attrregs[0x12] & 0x30)
                {
                        case 0x00: /* P0 and P2 */
                        if (svga->attrregs[0x11] & 0x01)
                                svga->cgastat |= 0x10;
                        if (svga->attrregs[0x11] & 0x04)
                                svga->cgastat |= 0x20;
                        break;
                        case 0x10: /* P4 and P5 */
                        if (svga->attrregs[0x11] & 0x10)
                                svga->cgastat |= 0x10;
                        if (svga->attrregs[0x11] & 0x20)
                                svga->cgastat |= 0x20;
                        break;
                        case 0x20: /* P1 and P3 */
                        if (svga->attrregs[0x11] & 0x02)
                                svga->cgastat |= 0x10;
                        if (svga->attrregs[0x11] & 0x08)
                                svga->cgastat |= 0x20;
                        break;
                        case 0x30: /* P6 and P7 */
                        if (svga->attrregs[0x11] & 0x40)
                                svga->cgastat |= 0x10;
                        if (svga->attrregs[0x11] & 0x80)
                                svga->cgastat |= 0x20;
                        break;
                }
                return svga->cgastat;


                case 0x3DE: 
                temp = oti037->index;
                break;
                case 0x3DF: 
                if ((oti037->index & 0x1f)==0x10)     temp = 0x18;
                else                         temp = oti037->regs[oti037->index & 0x1f];
                break;

                default:
                temp = svga_in(addr, svga);
                break;
        }
//        if (addr != 0x3da && addr != 0x3ba) pclog("%02X  %04X:%04X\n", temp, CS,cpu_state.pc);        
        return temp;
}

void oti037_recalctimings(svga_t *svga)
{
        oti037_t *oti037 = (oti037_t *)svga->p;

        if (oti037->regs[0x14] & 0x08) svga->ma_latch |= 0x10000;
        if (oti037->regs[0x0d] & 0x0c) svga->rowoffset <<= 1;
        svga->interlace = oti037->regs[0x14] & 0x80;
}

void *oti037_common_init(char *bios_fn, int vram_size)
{
        oti037_t *oti037 = malloc(sizeof(oti037_t));
        memset(oti037, 0, sizeof(oti037_t));

        rom_init(&oti037->bios_rom, bios_fn, 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

        svga_init(&oti037->svga, oti037, vram_size << 10,
                   oti037_recalctimings,
                   oti037_in, oti037_out,
                   NULL,
                   NULL);

        io_sethandler(0x03c0, 0x0020, oti037_in, NULL, NULL, oti037_out, NULL, NULL, oti037);

        oti037->svga.miscout = 1;
        oti037->regs[0] = 0x08; // fixme: bios wants to read this at index 0? this index is undocumented
        return oti037;
}

void *oti037_init()
{
        return oti037_common_init("oti037/bios.bin", 256);
}

static int oti037_available()
{
        return rom_present("oti037/bios.bin");
}

void oti037_close(void *p)
{
        oti037_t *oti037 = (oti037_t *)p;

        svga_close(&oti037->svga);

        free(oti037);
}

void oti037_speed_changed(void *p)
{
        oti037_t *oti037 = (oti037_t *)p;

        svga_recalctimings(&oti037->svga);
}

void oti037_force_redraw(void *p)
{
        oti037_t *oti037 = (oti037_t *)p;

        oti037->svga.fullchange = changeframecount;
}

void oti037_add_status_info(char *s, int max_len, void *p)
{
        oti037_t *oti037 = (oti037_t *)p;

        svga_add_status_info(s, max_len, &oti037->svga);
}

device_t oti037_device =
{
        "Oak OTI-037",
        0,
        oti037_init,
        oti037_close,
        oti037_available,
        oti037_speed_changed,
        oti037_force_redraw,
        oti037_add_status_info
};
