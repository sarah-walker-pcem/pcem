#include <string.h>
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "cpu.h"

#include "ali1429.h"

static int ali1429_index;
static uint8_t ali1429_regs[256];

void ali1429_write(uint16_t port, uint8_t val, void *priv)
{
//        return;
        if (!(port & 1)) 
                ali1429_index = val;
        else
        {
                ali1429_regs[ali1429_index] = val;
//                pclog("ALI1429 write %02X %02X %04X:%04X %i\n",ali1429_index,val,CS,pc,ins);
                switch (ali1429_index)
                {
                        case 0x13:
                        if (!(val & 0xc0)) 
                        {
                                shadowbios = 0;
                                if (!shadowbios_write)
                                        mem_bios_set_state(0xf0000, 0x10000, 0, 0);
                                else
                                        mem_bios_set_state(0xf0000, 0x10000, 0, 1);
                                flushmmucache();
                        }                                        
                        break;
                        case 0x14:
                        shadowbios = val & 1;
                        shadowbios_write = val & 2;
                        switch (val & 3)
                        {
                                case 0: 
                                mem_bios_set_state(0xf0000, 0x10000, 0, 0);
                                break;
                                case 1: 
                                mem_bios_set_state(0xf0000, 0x10000, 1, 0);
                                break;
                                case 2:
                                mem_bios_set_state(0xf0000, 0x10000, 0, 1);
                                break;
                                case 3:
                                mem_bios_set_state(0xf0000, 0x10000, 1, 1);
                                break;
                        }                                                                                                
                        flushmmucache();
                        break;
                }
        }
}

uint8_t ali1429_read(uint16_t port, void *priv)
{
        if (!(port & 1)) 
                return ali1429_index;
        if ((ali1429_index >= 0xc0 || ali1429_index == 0x20) && cpu_iscyrix)
                return 0xff; /*Don't conflict with Cyrix config registers*/
        return ali1429_regs[ali1429_index];
}


void ali1429_reset()
{
        memset(ali1429_regs, 0xff, 256);
}

void ali1429_init()
{
        io_sethandler(0x0022, 0x0002, ali1429_read, NULL, NULL, ali1429_write, NULL, NULL, NULL);
}
