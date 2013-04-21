#include "ibm.h"
#include "io.h"
#include "mem.h"

#include "um8881f.h"

static uint8_t card_16[256];
static uint8_t card_18[256];

void um8881f_write(int func, int addr, uint8_t val)
{
        if (addr == 0x54)
        {
                if ((card_16[0x54] ^ val) & 0x01)
                {
                        if (val & 1)
                           mem_sethandler(0xe0000, 0x10000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml);
                        else
                           mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   NULL,          NULL,           NULL          );
                }
                flushmmucache_nopc();
        }
        if (addr == 0x55)
        {
                if ((card_16[0x55] ^ val) & 0xc0)
                {
                        switch (val & 0xc0)
                        {
                                case 0x00: mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml); break;
                                case 0x40: mem_sethandler(0xf0000, 0x10000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   NULL,          NULL,           NULL          ); break;
                                case 0x80: mem_sethandler(0xf0000, 0x10000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml); break;
                                case 0xc0: mem_sethandler(0xf0000, 0x10000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    NULL,          NULL,           NULL          ); break;
                        }
                        shadowbios = val & 0x80;
                        shadowbios_write = !(val & 0x40);
                        flushmmucache_nopc();
                }
        }
        if (addr >= 4) 
           card_16[addr] = val;
}

uint8_t um8881f_read(int func, int addr)
{
        return card_16[addr];
}
 
void um8886f_write(int func, int addr, uint8_t val)
{
        if (addr >= 4) 
           card_18[addr] = val;
}

uint8_t um8886f_read(int func, int addr)
{
        return card_18[addr];
}
 
 
     
void um8881f_init()
{
        pci_add_specific(16, um8881f_read, um8881f_write);
        pci_add_specific(18, um8886f_read, um8886f_write);
        
        card_16[0] = card_18[0] = 0x60; /*UMC*/
        card_16[1] = card_18[1] = 0x10;
        card_16[2] = 0x81; card_16[3] = 0x88; /*UM8881 Host - PCI bridge*/
        card_18[2] = 0x86; card_18[3] = 0x88; /*UM8886 PCI - ISA bridge*/
}
