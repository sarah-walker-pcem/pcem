/*87C716 'SDAC' true colour RAMDAC emulation*/
/*Misidentifies as AT&T 21C504*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_sdac_ramdac.h"

struct
{
        int magic_count;
        uint8_t command;
        int windex, rindex;
        uint16_t regs[256];
        int reg_ff;
        int rs2;
} sdac_ramdac;

void sdac_ramdac_out(uint16_t addr, uint8_t val)
{
        int didwrite;
//        /*if (CS!=0xC000) */pclog("OUT RAMDAC %04X %02X %i %04X:%04X  %i\n",addr,val,sdac_ramdac.magic_count,CS,pc, sdac_ramdac.rs2);
        switch (addr)
        {
                case 0x3C6:
                if (val == 0xff)
                {
                        sdac_ramdac.rs2 = 0;
                        sdac_ramdac.magic_count = 0;
                        break;
                }
                if (sdac_ramdac.magic_count < 4) break;
                if (sdac_ramdac.magic_count == 4)
                {
                        sdac_ramdac.command = val;
//                        pclog("RAMDAC command reg now %02X\n", val);
                        switch (val >> 4)
                        {
                                case 2: case 3: case 0xa: bpp = 15; break;
                                case 4: case 0xe: bpp = 24; break;
                                case 5: case 6: case 0xc: bpp = 16; break;
                                case 7: bpp = 32; break;

                                case 0: case 1: default: bpp = 8; break;
                        }
                }
                //sdac_ramdac.magic_count = 0;
                break;
                
                case 0x3C7:
                sdac_ramdac.magic_count = 0;
                if (sdac_ramdac.rs2)
                   sdac_ramdac.rindex = val;
                break;
                case 0x3C8:
                sdac_ramdac.magic_count = 0;
                if (sdac_ramdac.rs2)
                   sdac_ramdac.windex = val;
                break;
                case 0x3C9:
                sdac_ramdac.magic_count = 0;
                if (sdac_ramdac.rs2)
                {
                        if (!sdac_ramdac.reg_ff) sdac_ramdac.regs[sdac_ramdac.windex] = (sdac_ramdac.regs[sdac_ramdac.windex] & 0xff00) | val;
                        else                    sdac_ramdac.regs[sdac_ramdac.windex] = (sdac_ramdac.regs[sdac_ramdac.windex] & 0x00ff) | (val << 8);
                        sdac_ramdac.reg_ff = !sdac_ramdac.reg_ff;
//                        pclog("RAMDAC reg %02X now %04X\n", sdac_ramdac.windex, sdac_ramdac.regs[sdac_ramdac.windex]);
                        if (!sdac_ramdac.reg_ff) sdac_ramdac.windex++;
                }
                break;
        }
        svga_out(addr,val);
}

uint8_t sdac_ramdac_in(uint16_t addr)
{
        uint8_t temp;
//        /*if (CS!=0xC000) */pclog("IN RAMDAC %04X %04X:%04X %i\n",addr,CS,pc, sdac_ramdac.rs2);
        switch (addr)
        {
                case 0x3C6:
                sdac_ramdac.reg_ff = 0;
                if (sdac_ramdac.magic_count < 5)
                   sdac_ramdac.magic_count++;
                if (sdac_ramdac.magic_count == 4)
                {
                        temp = 0x70; /*SDAC ID*/
                        sdac_ramdac.rs2 = 1;
                }
                if (sdac_ramdac.magic_count == 5)
                {
                        temp = sdac_ramdac.command;
                        sdac_ramdac.magic_count = 0;
                }
                return temp;
                case 0x3C7:
//                if (sdac_ramdac.magic_count < 4)
//                {
                        sdac_ramdac.magic_count=0;
//                        break;
//                }
                if (sdac_ramdac.rs2) return sdac_ramdac.rindex;
                break;
                case 0x3C8:
//                if (sdac_ramdac.magic_count < 4)
//                {
                        sdac_ramdac.magic_count=0;
//                        break;
//                }
                if (sdac_ramdac.rs2) return sdac_ramdac.windex;
                break;
                case 0x3C9:
//                if (sdac_ramdac.magic_count < 4)
//                {
                        sdac_ramdac.magic_count=0;
//                        break;
//                }
                if (sdac_ramdac.rs2)
                {
                        if (!sdac_ramdac.reg_ff) temp = sdac_ramdac.regs[sdac_ramdac.rindex] & 0xff;
                        else                     temp = sdac_ramdac.regs[sdac_ramdac.rindex] >> 8;
                        sdac_ramdac.reg_ff = !sdac_ramdac.reg_ff;
                        if (!sdac_ramdac.reg_ff)
                        {
                                sdac_ramdac.rindex++;
                                sdac_ramdac.magic_count = 0;
                        }
                        return temp;
                }
                break;
        }
        return svga_in(addr);
}

float sdac_getclock(int clock)
{
        float t;
        int m, n1, n2;
//        pclog("SDAC_Getclock %i %04X\n", clock, sdac_ramdac.regs[clock]);
        if (clock == 0) return 25175000.0;
        if (clock == 1) return 28322000.0;
        clock ^= 1; /*Clocks 2 and 3 seem to be reversed*/
        m  =  (sdac_ramdac.regs[clock] & 0x7f) + 2;
        n1 = ((sdac_ramdac.regs[clock] >>  8) & 0x1f) + 2;
        n2 = ((sdac_ramdac.regs[clock] >> 13) & 0x07);
        t = (14318184.0 * ((float)m / (float)n1)) / (float)(1 << n2);
//        pclog("SDAC clock %i %i %i %f %04X  %f %i\n", m, n1, n2, t, sdac_ramdac.regs[2], 14318184.0 * ((float)m / (float)n1), 1 << n2);
        return t;
}
