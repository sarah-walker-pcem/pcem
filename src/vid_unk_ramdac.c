/*It is unknown exactly what RAMDAC this is
  It is possibly a Sierra 1502x
  It's addressed by the TLIVESA1 driver for ET4000*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_unk_ramdac.h"

static int unk_state=0;
static uint8_t unk_ctrl;

void unk_ramdac_out(uint16_t addr, uint8_t val)
{
        //pclog("OUT RAMDAC %04X %02X\n",addr,val);
        switch (addr)
        {
                case 0x3C6:
                if (unk_state == 4)
                {
                        unk_state = 0;
                        unk_ctrl = val;
                        switch ((val&1)|((val&0xE0)>>4))
                        {
                                case 0: case 1: case 2: case 3:
                                bpp = 8;
                                break;
                                case 6: case 7:
                                bpp = 24;
                                break;
                                case 8: case 9: case 0xA: case 0xB:
                                bpp = 15;
                                break;
                                case 0xC: case 0xD: case 0xE: case 0xF:
                                bpp = 16;
                                break;
                        }
                        return;
                }
                unk_state = 0;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                unk_state = 0;
                break;
        }
        svga_out(addr,val);
}

uint8_t unk_ramdac_in(uint16_t addr)
{
        //pclog("IN RAMDAC %04X\n",addr);
        switch (addr)
        {
                case 0x3C6:
                if (unk_state == 4)
                {
                        unk_state = 0;
                        return unk_ctrl;
                }
                unk_state++;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                unk_state = 0;
                break;
        }
        return svga_in(addr);
}
