/*Trident TKD8001 RAMDAC emulation*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_tkd8001_ramdac.h"

static int tkd8001_state=0;
static uint8_t tkd8001_ctrl;

void tkd8001_ramdac_out(uint16_t addr, uint8_t val)
{
//        pclog("OUT RAMDAC %04X %02X %04X:%04X\n",addr,val,CS,pc);
        switch (addr)
        {
                case 0x3C6:
                if (tkd8001_state == 4)
                {
                        tkd8001_state = 0;
                        tkd8001_ctrl = val;
                        switch (val>>5)
                        {
                                case 0: case 1: case 2: case 3:
                                bpp = 8;
                                break;
                                case 5:
                                bpp = 15;
                                break;
                                case 6:
                                bpp = 24;
                                break;
                                case 7:
                                bpp = 16;
                                break;
                        }
                        return;
                }
               // tkd8001_state = 0;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                tkd8001_state = 0;
                break;
        }
        svga_out(addr,val);
}

uint8_t tkd8001_ramdac_in(uint16_t addr)
{
//        pclog("IN RAMDAC %04X %04X:%04X\n",addr,CS,pc);
        switch (addr)
        {
                case 0x3C6:
                if (tkd8001_state == 4)
                {
                        //tkd8001_state = 0;
                        return tkd8001_ctrl;
                }
                tkd8001_state++;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                tkd8001_state = 0;
                break;
        }
        return svga_in(addr);
}
