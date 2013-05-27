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
        if (!(port&1)) ali1429_index=val;
        else
        {
                ali1429_regs[ali1429_index]=val;
//                pclog("ALI1429 write %02X %02X %04X:%04X %i\n",ali1429_index,val,CS,pc,ins);
                switch (ali1429_index)
                {
                        case 0x13:
/*                                if (val == 1)
                                { 
                                        times = 1;
                                        ins = 0;
                                        output = 3;
                                }*/
//                                pclog("write 13 %02X %i\n",val,shadowbios);
                                if (!(val&0xC0)) 
                                {
                                       shadowbios=0;
                                        if (!shadowbios_write)
                                           mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   NULL,          NULL,           NULL,           NULL);
                                        else
                                           mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml, NULL);
                                        flushmmucache();
                                }                                        
                                break;
                        case 0x14:
                        shadowbios=val&1;//((val&3)==1);
                        shadowbios_write=val&2;
                        switch (val & 3)
                        {
                                case 0: mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   NULL,          NULL,           NULL,              NULL); break;
                                case 1: mem_sethandler(0xf0000, 0x10000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    NULL,          NULL,           NULL,              NULL); break;
                                case 2: mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,    NULL); break;
                                case 3: mem_sethandler(0xf0000, 0x10000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,    NULL); break;
                        }                                                                                                
                        
//                        if (val==0x43) shadowbios=1;
//                        pclog("Shadow bios %i\n",shadowbios);
                        flushmmucache();
                        break;
                }
        }
}

uint8_t ali1429_read(uint16_t port, void *priv)
{
        if (!(port&1)) return ali1429_index;
        if ((ali1429_index >= 0xc0 || ali1429_index == 0x20) && cpu_iscyrix)
           return 0xff; /*Don't conflict with Cyrix config registers*/
        return ali1429_regs[ali1429_index];
}


void ali1429_reset()
{
        memset(ali1429_regs,0xFF,256);
}

void ali1429_init()
{
        io_sethandler(0x0022, 0x0002, ali1429_read, NULL, NULL, ali1429_write, NULL, NULL, NULL);
}
