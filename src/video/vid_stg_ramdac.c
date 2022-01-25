/*STG1702 true colour RAMDAC emulation*/
#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_stg_ramdac.h"

static int stg_state_read[2][8] = {{ 1, 2, 3, 4, 0, 0, 0, 0 }, { 1, 2, 3, 4, 5, 6, 7, 7 }};
static int stg_state_write[8] = { 0, 0, 0, 0, 0, 6, 7, 7 };

void stg_ramdac_out(uint16_t addr, uint8_t val, stg_ramdac_t* ramdac, svga_t* svga)
{
        int didwrite;
        //if (CS!=0xC000) pclog("OUT RAMDAC %04X %02X %i %04X:%04X\n",addr,val,stg_ramdac.magic_count,CS,pc);
        switch (addr)
        {
        case 0x3c6:
                switch (ramdac->magic_count)
                {
                case 0:
                case 1:
                case 2:
                case 3:
                        break;
                case 4:
                        ramdac->command = val;
                        /*pclog("Write RAMDAC command %02X\n",val);*/
                        break;
                case 5:
                        ramdac->index = (ramdac->index & 0xff00) | val;
                        break;
                case 6:
                        ramdac->index = (ramdac->index & 0xff) | (val << 8);
                        break;
                case 7:
                        pclog("Write RAMDAC reg %02X %02X\n", ramdac->index, val);
                        if (ramdac->index < 0x100)
                                ramdac->regs[ramdac->index] = val;
                        ramdac->index++;
                        break;
                }
                didwrite = (ramdac->magic_count >= 4);
                ramdac->magic_count = stg_state_write[ramdac->magic_count & 7];
                if (ramdac->command & 8)
                {
                        switch (ramdac->regs[3])
                        {
                        case 0:
                        case 5:
                        case 7:
                                svga->bpp = 8;
                                break;
                        case 1:
                        case 2:
                        case 8:
                                svga->bpp = 15;
                                break;
                        case 3:
                        case 6:
                                svga->bpp = 16;
                                break;
                        case 4:
                        case 9:
                                svga->bpp = 24;
                                break;
                        default:
                                svga->bpp = 8;
                                break;
                        }
                }
                else
                {
                        switch (ramdac->command >> 5)
                        {
                        case 0:
                                svga->bpp = 8;
                                break;
                        case 5:
                                svga->bpp = 15;
                                break;
                        case 6:
                                svga->bpp = 16;
                                break;
                        case 7:
                                svga->bpp = 24;
                                break;
                        default:
                                svga->bpp = 8;
                                break;
                        }
                }
                svga_recalctimings(svga);
                if (didwrite) return;
                break;
        case 0x3c7:
        case 0x3c8:
        case 0x3c9:
                ramdac->magic_count = 0;
                break;
        }
        svga_out(addr, val, svga);
}

uint8_t stg_ramdac_in(uint16_t addr, stg_ramdac_t* ramdac, svga_t* svga)
{
        uint8_t temp = 0xff;
        //if (CS!=0xC000) pclog("IN RAMDAC %04X %04X:%04X\n",addr,CS,pc);
        switch (addr)
        {
        case 0x3c6:
                switch (ramdac->magic_count)
                {
                case 0:
                case 1:
                case 2:
                case 3:
                        temp = 0xff;
                        break;
                case 4:
                        temp = ramdac->command;
                        break;
                case 5:
                        temp = ramdac->index & 0xff;
                        break;
                case 6:
                        temp = ramdac->index >> 8;
                        break;
                case 7:
//                                pclog("Read RAMDAC index %04X\n",stg_ramdac.index);
                        switch (ramdac->index)
                        {
                        case 0:
                                temp = 0x44;
                                break;
                        case 1:
                                temp = 0x02;
                                break;
                        default:
                                if (ramdac->index < 0x100) temp = ramdac->regs[ramdac->index];
                                else temp = 0xff;
                                break;
                        }
                        ramdac->index++;
                        break;
                }
                ramdac->magic_count = stg_state_read[(ramdac->command & 0x10) ? 1 : 0][ramdac->magic_count & 7];
                return temp;
        case 0x3c8:
        case 0x3c9:
                ramdac->magic_count = 0;
                break;
        }
        return svga_in(addr, svga);
}
