#include <string.h>

#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pci.h"

#include "i430vx.h"

static uint8_t card_i430vx[256];

static void i430vx_map(uint32_t addr, uint32_t size, int state)
{
        switch (state & 3)
        {
                case 0x0: mem_sethandler(addr, size, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   NULL,          NULL,           NULL          , NULL); break; /*DRAM disabled, accesses directed to PCI bus*/
                case 0x1: mem_sethandler(addr, size, mem_read_ram,    mem_read_ramw,    mem_read_raml,    NULL,          NULL,           NULL          , NULL); break; /*Read only, DRAM write protected, non-cacheable*/
                case 0x2: mem_sethandler(addr, size, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml, NULL); break; /*Write only*/
                case 0x3: mem_sethandler(addr, size, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml, NULL); break; /*Read/write, non-cacheable*/
                /*Below are redundant*/
//                case 0x5: mem_sethandler(addr, size, mem_read_ram,    mem_read_ramw,    mem_read_raml,    NULL,          NULL,           NULL          ); break; /*Read only, DRAM write protected, cacheable*/
//                case 0x7: mem_sethandler(addr, size, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml); break; /*Read/write, non-cacheable*/                
        }
        flushmmucache_nopc();        
}
void i430vx_write(int func, int addr, uint8_t val, void *priv)
{
        if (func)
           return;
           
        switch (addr)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                case 0x0e:
                return;
                
                case 0x59: /*PAM0*/
                if ((card_i430vx[0x59] ^ val) & 0xf0)
                {
                        i430vx_map(0xf0000, 0x10000, val >> 4);
                        shadowbios = (val & 0x10);
                }
                pclog("i430vx_write : PAM0 write %02X\n", val);
                break;
                case 0x5e: /*PAM5*/
                if ((card_i430vx[0x5e] ^ val) & 0x0f)
                   i430vx_map(0xe0000, 0x04000, val & 0xf);
                if ((card_i430vx[0x5e] ^ val) & 0xf0)
                   i430vx_map(0xe4000, 0x04000, val >> 4);
                pclog("i430vx_write : PAM5 write %02X\n", val);
                break;
                case 0x5f: /*PAM6*/
                if ((card_i430vx[0x5f] ^ val) & 0x0f)
                   i430vx_map(0xe8000, 0x04000, val & 0xf);
                if ((card_i430vx[0x5f] ^ val) & 0xf0)
                   i430vx_map(0xec000, 0x04000, val >> 4);
                pclog("i430vx_write : PAM6 write %02X\n", val);
                break;
        }
                
        card_i430vx[addr] = val;
}

uint8_t i430vx_read(int func, int addr, void *priv)
{
        if (func)
           return 0xff;

        return card_i430vx[addr];
}
 
    
void i430vx_init()
{
        pci_add_specific(0, i430vx_read, i430vx_write, NULL);
        
        memset(card_i430vx, 0, 256);
        card_i430vx[0x00] = 0x86; card_i430vx[0x01] = 0x80; /*Intel*/
        card_i430vx[0x02] = 0x30; card_i430vx[0x03] = 0x70; /*82437VX*/
        card_i430vx[0x04] = 0x06; card_i430vx[0x05] = 0x00;
        card_i430vx[0x06] = 0x00; card_i430vx[0x07] = 0x02;
        card_i430vx[0x08] = 0x00; /*A0 stepping*/
        card_i430vx[0x09] = 0x00; card_i430vx[0x0a] = 0x00; card_i430vx[0x0b] = 0x06;
        card_i430vx[0x52] = 0x42; /*256kb PLB cache*/
        card_i430vx[0x53] = 0x14;
        card_i430vx[0x56] = 0x52; /*DRAM control*/
        card_i430vx[0x57] = 0x01;
        card_i430vx[0x60] = card_i430vx[0x61] = card_i430vx[0x62] = card_i430vx[0x63] = card_i430vx[0x64] = 0x02;
        card_i430vx[0x67] = 0x11;
        card_i430vx[0x69] = 0x03;
        card_i430vx[0x70] = 0x20;
        card_i430vx[0x72] = 0x02;
        card_i430vx[0x74] = 0x0e;
        card_i430vx[0x78] = 0x23;
}
