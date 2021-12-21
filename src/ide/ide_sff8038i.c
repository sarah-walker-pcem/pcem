/*Emulation of SFF-8038i IDE bus mastering interface.
  Used by PIIX and VT82C586B IDE controllers*/
#include "ibm.h"
#include "ide.h"
#include "ide_sff8038i.h"
#include "mem.h"

static void sff_bus_master_next_addr(sff_busmaster_t *busmaster, int channel)
{
        busmaster[channel].addr = (*(uint32_t *)(&ram[busmaster[channel].ptr_cur])) & ~1;
        busmaster[channel].count = (*(uint32_t *)(&ram[busmaster[channel].ptr_cur + 4])) & 0xfffe;
        if (!busmaster[channel].count)
                busmaster[channel].count = 0x10000;
        busmaster[channel].eot = (*(uint32_t *)(&ram[busmaster[channel].ptr_cur + 4])) >> 31;
        busmaster[channel].ptr_cur += 8;
//        pclog("New DMA settings on channel %i - Addr %08X Count %04X EOT %i\n", channel, piix_busmaster[channel].addr, piix_busmaster[channel].count, piix_busmaster[channel].eot);
}

void sff_bus_master_write(uint16_t port, uint8_t val, void *p)
{
        sff_busmaster_t *busmaster = (sff_busmaster_t *)p;
        int channel = (port & 8) ? 1 : 0;
//        pclog("PIIX Bus Master write %04X %02X %04x:%08x\n", port, val, CS, pc);
        switch (port & 7)
        {
                case 0:
                if ((val & 1) && !(busmaster[channel].command & 1)) /*Start*/
                {
                        busmaster[channel].ptr_cur = busmaster[channel].ptr;
                        sff_bus_master_next_addr(busmaster, channel);
                        busmaster[channel].status |= 1;
                }
                if (!(val & 1) && (busmaster[channel].command & 1)) /*Stop*/
                        busmaster[channel].status &= ~1;

                busmaster[channel].command = val;
                break;
                case 2:
                busmaster[channel].status = (val & 0x60) | ((busmaster[channel].status & ~val) & 6) | (busmaster[channel].status & 1);
                break;
                case 4:
                busmaster[channel].ptr = (busmaster[channel].ptr & 0xffffff00) | val;
                break;
                case 5:
                busmaster[channel].ptr = (busmaster[channel].ptr & 0xffff00ff) | (val << 8);
                break;
                case 6:
                busmaster[channel].ptr = (busmaster[channel].ptr & 0xff00ffff) | (val << 16);
                break;
                case 7:
                busmaster[channel].ptr = (busmaster[channel].ptr & 0x00ffffff) | (val << 24);
                break;
        }
}

uint8_t sff_bus_master_read(uint16_t port, void *p)
{
        sff_busmaster_t *busmaster = (sff_busmaster_t *)p;
        int channel = (port & 8) ? 1 : 0;
//        pclog("PIIX Bus Master read %04X %04x:%08x\n", port, CS, pc);
        switch (port & 7)
        {
                case 0:
                return busmaster[channel].command;
                case 2:
                return busmaster[channel].status;
                case 4:
                return busmaster[channel].ptr;
                case 5:
                return busmaster[channel].ptr >> 8;
                case 6:
                return busmaster[channel].ptr >> 16;
                case 7:
                return busmaster[channel].ptr >> 24;
        }
        return 0xff;
}

static int sff_bus_master_data_read(int channel, uint8_t *data, int size, void *p)
{
        sff_busmaster_t *busmaster = (sff_busmaster_t *)p;
        int transferred = 0;

        if (!(busmaster[channel].status & 1))
                return 1;                                    /*DMA disabled*/

        while (transferred < size)
        {
                if (busmaster[channel].count < (size - transferred) && busmaster[channel].eot)
                        fatal("DMA on channel %i - Read count less than size! Addr %08X Count %04X EOT %i size %i\n", channel, busmaster[channel].addr, busmaster[channel].count, busmaster[channel].eot, size);

                mem_invalidate_range(busmaster[channel].addr, busmaster[channel].addr+(size-1));

                if (busmaster[channel].count < (size - transferred))
                {
//                        pclog("Transferring smaller - %i bytes\n", piix_busmaster[channel].count);
                        if (busmaster[channel].addr < (mem_size * 1024))
                        {
                                int count = busmaster[channel].count;
                                if ((busmaster[channel].addr + count) > (mem_size * 1024))
                                        count = (mem_size * 1024) - busmaster[channel].addr;
                                memcpy(&ram[busmaster[channel].addr], data + transferred, count);
                        }
                        transferred += busmaster[channel].count;
                        busmaster[channel].addr += busmaster[channel].count;
                        busmaster[channel].count = 0;
                }
                else
                {
//                        pclog("Transferring larger - %i bytes\n", size - transferred);
                        if (busmaster[channel].addr < (mem_size * 1024))
                        {
                                int count = size - transferred;
                                if ((busmaster[channel].addr + count) > (mem_size * 1024))
                                        count = (mem_size * 1024) - busmaster[channel].addr;
                                memcpy(&ram[busmaster[channel].addr], data + transferred, count);
                        }
                        busmaster[channel].addr += (size - transferred);
                        busmaster[channel].count -= (size - transferred);
                        transferred += (size - transferred);
                }

//                pclog("DMA on channel %i - Addr %08X Count %04X EOT %i\n", channel, piix_busmaster[channel].addr, piix_busmaster[channel].count, piix_busmaster[channel].eot);

                if (!busmaster[channel].count)
                {
//                        pclog("DMA on channel %i - block over\n", channel);
                        if (busmaster[channel].eot) /*End of transfer?*/
                        {
//                                pclog("DMA on channel %i - transfer over\n", channel);
                                busmaster[channel].status &= ~1;
                        }
                        else
                                sff_bus_master_next_addr(busmaster, channel);
                }
        }
        return 0;
}
static int sff_bus_master_data_write(int channel, uint8_t *data, int size, void *p)
{
        sff_busmaster_t *busmaster = (sff_busmaster_t *)p;
        int transferred = 0;

        if (!(busmaster[channel].status & 1))
                return 1;                                    /*DMA disabled*/

        while (transferred < size)
        {
                if (busmaster[channel].count < (size - transferred) && busmaster[channel].eot)
                        fatal("DMA on channel %i - Write count less than size! Addr %08X Count %04X EOT %i size %i\n", channel, busmaster[channel].addr, busmaster[channel].count, busmaster[channel].eot, size);

                if (busmaster[channel].count < (size - transferred))
                {
//                        pclog("Transferring smaller - %i bytes\n", piix_busmaster[channel].count);
                        if (busmaster[channel].addr < (mem_size * 1024))
                        {
                                int count = busmaster[channel].count;
                                if ((busmaster[channel].addr + count) > (mem_size * 1024))
                                        count = (mem_size * 1024) - busmaster[channel].addr;
                                memcpy(data + transferred, &ram[busmaster[channel].addr], count);
                        }
                        transferred += busmaster[channel].count;
                        busmaster[channel].addr += busmaster[channel].count;
                        busmaster[channel].count = 0;
                }
                else
                {
//                        pclog("Transferring larger - %i bytes\n", size - transferred);
                        if (busmaster[channel].addr < (mem_size * 1024))
                        {
                                int count = size - transferred;
                                if ((busmaster[channel].addr + count) > (mem_size * 1024))
                                        count = (mem_size * 1024) - busmaster[channel].addr;
                                memcpy(data + transferred, &ram[busmaster[channel].addr], count);
                        }
                        busmaster[channel].addr += (size - transferred);
                        busmaster[channel].count -= (size - transferred);
                        transferred += (size - transferred);
                }

//                pclog("DMA on channel %i - Addr %08X Count %04X EOT %i\n", channel, piix_busmaster[channel].addr, piix_busmaster[channel].count, piix_busmaster[channel].eot);

                if (!busmaster[channel].count)
                {
//                        pclog("DMA on channel %i - block over\n", channel);
                        if (busmaster[channel].eot) /*End of transfer?*/
                        {
//                                pclog("DMA on channel %i - transfer over\n", channel);
                                busmaster[channel].status &= ~1;
                        }
                        else
                                sff_bus_master_next_addr(busmaster, channel);
                }
        }
        return 0;
}

static void sff_bus_master_set_irq(int channel, void *p)
{
        sff_busmaster_t *busmaster = (sff_busmaster_t *)p;
        
        busmaster[channel].status |= 4;
}

void sff_bus_master_init(sff_busmaster_t *busmaster)
{
        ide_set_bus_master(sff_bus_master_data_read, sff_bus_master_data_write, sff_bus_master_set_irq, busmaster);
}
