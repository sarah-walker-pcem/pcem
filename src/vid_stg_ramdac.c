/*STG1702 true colour RAMDAC emulation*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_stg_ramdac.h"

struct
{
        int magic_count;
        uint8_t command;
        int index;
        uint8_t regs[256];
} stg_ramdac;

int stg_state_read[2][8]={{1,2,3,4,0,0,0,0}, {1,2,3,4,5,6,7,7}};
int stg_state_write[8]={0,0,0,0,0,6,7,7};

void stg_ramdac_out(uint16_t addr, uint8_t val, void *priv)
{
        int didwrite;
        //if (CS!=0xC000) pclog("OUT RAMDAC %04X %02X %i %04X:%04X\n",addr,val,stg_ramdac.magic_count,CS,pc);
        switch (addr)
        {
                case 0x3C6:
                switch (stg_ramdac.magic_count)
                {
                        case 0: case 1: case 2: case 3: break;
                        case 4: stg_ramdac.command=val; /*pclog("Write RAMDAC command %02X\n",val);*/ break;
                        case 5: stg_ramdac.index=(stg_ramdac.index&0xFF00)|val; break;
                        case 6: stg_ramdac.index=(stg_ramdac.index&0xFF)|(val<<8); break;
                        case 7:
                                pclog("Write RAMDAC reg %02X %02X\n", stg_ramdac.index, val);
                        if (stg_ramdac.index<0x100) stg_ramdac.regs[stg_ramdac.index]=val;
                        stg_ramdac.index++;
                        break;
                }
                didwrite = (stg_ramdac.magic_count>=4);
                stg_ramdac.magic_count=stg_state_write[stg_ramdac.magic_count&7];
                if (stg_ramdac.command&8)
                {
                        switch (stg_ramdac.regs[3])
                        {
                                case 0: case 5: case 7: bpp=8; break;
                                case 1: case 2: case 8: bpp=15; break;
                                case 3: case 6: bpp=16; break;
                                case 4: case 9: bpp=24; break;
                                default: bpp=8; break;
                        }
                }
                else
                {
                        switch (stg_ramdac.command>>5)
                        {
                                case 0: bpp=8;  break;
                                case 5: bpp=15; break;
                                case 6: bpp=16; break;
                                case 7: bpp=24; break;
                                default: bpp=8; break;
                        }
                }
                if (didwrite) return;
                break;
                case 0x3C7: case 0x3C8: case 0x3C9:
                stg_ramdac.magic_count=0;
                break;
        }
        svga_out(addr, val, NULL);
}

uint8_t stg_ramdac_in(uint16_t addr, void *priv)
{
        uint8_t temp;
        //if (CS!=0xC000) pclog("IN RAMDAC %04X %04X:%04X\n",addr,CS,pc);
        switch (addr)
        {
                case 0x3C6:
                switch (stg_ramdac.magic_count)
                {
                        case 0: case 1: case 2: case 3: temp=0xFF; break;
                        case 4: temp=stg_ramdac.command; break;
                        case 5: temp=stg_ramdac.index&0xFF; break;
                        case 6: temp=stg_ramdac.index>>8; break;
                        case 7:
//                                pclog("Read RAMDAC index %04X\n",stg_ramdac.index);
                        switch (stg_ramdac.index)
                        {
                                case 0: temp=0x44; break;
                                case 1: temp=0x02; break;
                                default:
                                if (stg_ramdac.index<0x100) temp=stg_ramdac.regs[stg_ramdac.index];
                                else                        temp=0xFF;
                                break;
                        }
                        stg_ramdac.index++;
                        break;
                }
                stg_ramdac.magic_count=stg_state_read[(stg_ramdac.command&0x10)?1:0][stg_ramdac.magic_count&7];
                return temp;
                case 0x3C8: case 0x3C9:
                stg_ramdac.magic_count=0;
                break;
        }
        return svga_in(addr, NULL);
}
