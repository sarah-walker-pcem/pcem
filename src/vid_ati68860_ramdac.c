/*ATI 68860 RAMDAC emulation (for Mach64)*/
/*
ATI 68860/68880 Truecolor DACs:
REG08 (R/W):
bit 0-?  Always 2 ??

REG0A (R/W):
bit 0-?  Always 1Dh ??

REG0B (R/W):  (GMR ?)
bit 0-7  Mode. 82h: 4bpp, 83h: 8bpp, A0h: 15bpp, A1h: 16bpp, C0h: 24bpp,
          E3h: 32bpp  (80h for VGA modes ?)

REG0C (R/W):  Device Setup Register A
bit   0  Controls 6/8bit DAC. 0: 8bit DAC/LUT, 1: 6bit DAC/LUT
    2-3  Depends on Video memory (= VRAM width ?) . 1: Less than 1Mb, 2: 1Mb,
           3: > 1Mb
    5-6  Always set ?
      7  If set can remove "snow" in some cases (A860_Delay_L ?) ??
*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_ati68860_ramdac.h"

static uint8_t ramdac_regs[16];
void ati68860_ramdac_out(uint16_t addr, uint8_t val, void *priv)
{
//        pclog("ati68860_out : addr %04X val %02X  %04X:%04X\n", addr, val, CS,pc);
        switch (addr)
        {
                case 0: 
                svga_out(0x3c8, val, NULL); 
                break;
                case 1: 
                svga_out(0x3c9, val, NULL); 
                break;
                case 2: 
                svga_out(0x3c6, val, NULL); 
                break;
                case 3: 
                svga_out(0x3c7, val, NULL); 
                break;
                default:
                ramdac_regs[addr & 0xf] = val;
                break;
        }
}

uint8_t ati68860_ramdac_in(uint16_t addr, void *priv)
{
        uint8_t ret = 0;
        switch (addr)
        {
                case 0:
                ret = svga_in(0x3c8, NULL);
                break;
                case 1:
                ret = svga_in(0x3c9, NULL);
                break;
                case 2:
                ret = svga_in(0x3c6, NULL);
                break;
                case 3:
                ret = svga_in(0x3c7, NULL);
                break;
                case 4: case 8:
                ret = 2; 
                break;
                case 6: case 0xa:
                ret = 0x1d;
                break;
                case 0xf:
                ret = 0xd0;
                break;
                
                default:
                ret = ramdac_regs[addr & 0xf];
                break;
        }
//        pclog("ati68860_in  : addr %04X ret %02X  %04X:%04X\n", addr, ret, CS,pc);
        return ret;
}
