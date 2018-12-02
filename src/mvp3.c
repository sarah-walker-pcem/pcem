#include <string.h>

#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pci.h"

#include "mvp3.h"

static struct
{
        uint8_t host_bridge_regs[256];
        uint8_t pci_bridge_regs[256];
} mvp3;

static void mvp3_map(uint32_t addr, uint32_t size, int state)
{
        pclog("mvp3_map: addr=%08x size=%08x state=%i\n", addr, size, state);
        switch (state & 3)
        {
                case 0:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 1:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                break;
                case 2:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 3:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                break;
        }
        flushmmucache_nopc();
}

static uint8_t mvp3_read(int func, int addr, void *priv)
{
        switch (func)
        {
                case 0:
                return mvp3.host_bridge_regs[addr];

                case 1:
                return mvp3.pci_bridge_regs[addr];
                
                default:
                return 0xff;
        }
}


static void mvp3_host_bridge_write(int addr, uint8_t val)
{
        /*Read-only addresses*/
        if ((addr < 4) || (addr >= 5 && addr < 7)
                        || (addr >= 8 && addr < 0xd)
                        || (addr >= 0xe && addr < 0x12)
                        || (addr >= 0x14 && addr < 0x50)
                        || (addr >= 0x79 && addr < 0x7e)
                        || (addr >= 0x85 && addr < 0x88)
                        || (addr >= 0x8c && addr < 0xa8)
                        || (addr >= 0xad && addr < 0xfd))
                return;

        switch (addr)
        {
                case 0x04: /*Command*/
                mvp3.host_bridge_regs[0x04] = (mvp3.host_bridge_regs[0x04] & ~0x40) | (val & 0x40);
                break;
                case 0x07: /*Status*/
                mvp3.host_bridge_regs[0x07] &= ~(val & 0xb0);
                break;
                
                case 0x12: /*Graphics Aperture Base*/
                mvp3.host_bridge_regs[0x12] = (val & 0xf0);
                break;
                case 0x13: /*Graphics Aperture Base*/
                mvp3.host_bridge_regs[0x13] = val;
                break;

                case 0x61: /*Shadow RAM Control 1*/
                if ((mvp3.host_bridge_regs[0x61] ^ val) & 0x03)
                        mvp3_map(0xc0000, 0x04000, val & 0x03);
                if ((mvp3.host_bridge_regs[0x61] ^ val) & 0x0c)
                        mvp3_map(0xc4000, 0x04000, (val & 0x0c) >> 2);
                if ((mvp3.host_bridge_regs[0x61] ^ val) & 0x30)
                        mvp3_map(0xc8000, 0x04000, (val & 0x30) >> 4);
                if ((mvp3.host_bridge_regs[0x61] ^ val) & 0xc0)
                        mvp3_map(0xcc000, 0x04000, (val & 0xc0) >> 6);
                mvp3.host_bridge_regs[0x61] = val;
                return;
                case 0x62: /*Shadow RAM Control 2*/
                if ((mvp3.host_bridge_regs[0x62] ^ val) & 0x03)
                        mvp3_map(0xd0000, 0x04000, val & 0x03);
                if ((mvp3.host_bridge_regs[0x62] ^ val) & 0x0c)
                        mvp3_map(0xd4000, 0x04000, (val & 0x0c) >> 2);
                if ((mvp3.host_bridge_regs[0x62] ^ val) & 0x30)
                        mvp3_map(0xd8000, 0x04000, (val & 0x30) >> 4);
                if ((mvp3.host_bridge_regs[0x62] ^ val) & 0xc0)
                        mvp3_map(0xdc000, 0x04000, (val & 0xc0) >> 6);
                mvp3.host_bridge_regs[0x62] = val;
                return;
                case 0x63: /*Shadow RAM Control 3*/
                if ((mvp3.host_bridge_regs[0x63] ^ val) & 0x30)
                        mvp3_map(0xf0000, 0x10000, (val & 0x30) >> 4);
                if ((mvp3.host_bridge_regs[0x63] ^ val) & 0xc0)
                        mvp3_map(0xe0000, 0x10000, (val & 0xc0) >> 6);
                mvp3.host_bridge_regs[0x63] = val;
//                if (val == 0xb0)
//                        output = 3;
                return;

                default:
                mvp3.host_bridge_regs[addr] = val;
                break;
        }
}
static void mvp3_pci_bridge_write(int addr, uint8_t val)
{
        /*Read-only addresses*/
        if ((addr < 4) || (addr >= 5 && addr < 7)
                        || (addr >= 8 && addr < 0x18)
                        || (addr == 0x1b)
                        || (addr >= 0x1e && addr < 0x20)
                        || (addr >= 0x28 && addr < 0x3e)
                        || (addr >= 0x43))
                return;
                
        switch (addr)
        {
                case 0x04: /*Command*/
                mvp3.pci_bridge_regs[0x04] = (mvp3.pci_bridge_regs[0x04] & ~0x47) | (val & 0x47);
                break;
                case 0x07: /*Status*/
                mvp3.pci_bridge_regs[0x07] &= ~(val & 0x30);
                break;

                case 0x20: /*Memory Base*/
                mvp3.pci_bridge_regs[0x20] = val & 0xf0;
                break;
                case 0x22: /*Memory Limit*/
                mvp3.pci_bridge_regs[0x22] = val & 0xf0;
                break;
                case 0x24: /*Prefetchable Memory Base*/
                mvp3.pci_bridge_regs[0x24] = val & 0xf0;
                break;
                case 0x26: /*Prefetchable Memory Limit*/
                mvp3.pci_bridge_regs[0x26] = val & 0xf0;
                break;

                default:
                mvp3.pci_bridge_regs[addr] = val;
                break;
        }
}

static void mvp3_write(int func, int addr, uint8_t val, void *priv)
{
        switch (func)
        {
                case 0:
                mvp3_host_bridge_write(addr, val);
                break;
                
                case 1:
                mvp3_pci_bridge_write(addr, val);
                break;
        }
}

void mvp3_init()
{
        pci_add_specific(0, mvp3_read, mvp3_write, NULL);

        memset(&mvp3, 0, sizeof(mvp3));
        mvp3.host_bridge_regs[0x00] = 0x06; mvp3.host_bridge_regs[0x01] = 0x11; /*VIA*/
        mvp3.host_bridge_regs[0x02] = 0x98; mvp3.host_bridge_regs[0x03] = 0x05; /*VT82C598MVP*/
        mvp3.host_bridge_regs[0x04] = 0x06; mvp3.host_bridge_regs[0x05] = 0x00;
        mvp3.host_bridge_regs[0x06] = 0x90; mvp3.host_bridge_regs[0x07] = 0x02;
        mvp3.host_bridge_regs[0x0b] = 0x06;
        mvp3.host_bridge_regs[0x10] = 0x08;
        mvp3.host_bridge_regs[0x34] = 0xa0;

        mvp3.host_bridge_regs[0x5a] = 0x01;
        mvp3.host_bridge_regs[0x5b] = 0x01;
        mvp3.host_bridge_regs[0x5c] = 0x01;
        mvp3.host_bridge_regs[0x5d] = 0x01;
        mvp3.host_bridge_regs[0x5e] = 0x01;
        mvp3.host_bridge_regs[0x5f] = 0x01;

        mvp3.host_bridge_regs[0x64] = 0xec;
        mvp3.host_bridge_regs[0x65] = 0xec;
        mvp3.host_bridge_regs[0x66] = 0xec;
        mvp3.host_bridge_regs[0x6b] = 0x01;

        mvp3.host_bridge_regs[0xa0] = 0x02;
        mvp3.host_bridge_regs[0xa2] = 0x10;
        mvp3.host_bridge_regs[0xa4] = 0x03;
        mvp3.host_bridge_regs[0xa5] = 0x02;
        mvp3.host_bridge_regs[0xa6] = 0x00;
        mvp3.host_bridge_regs[0xa7] = 0x07;

        mvp3.pci_bridge_regs[0x00] = 0x06; mvp3.pci_bridge_regs[0x01] = 0x11; /*VIA*/
        mvp3.pci_bridge_regs[0x02] = 0x98; mvp3.pci_bridge_regs[0x03] = 0x85; /*VT82C598MVP*/
        mvp3.pci_bridge_regs[0x04] = 0x07; mvp3.pci_bridge_regs[0x05] = 0x00;
        mvp3.pci_bridge_regs[0x06] = 0x20; mvp3.pci_bridge_regs[0x07] = 0x02;
        mvp3.pci_bridge_regs[0x0a] = 0x04;
        mvp3.pci_bridge_regs[0x0b] = 0x06;
        mvp3.pci_bridge_regs[0x0e] = 0x01;
        
        mvp3.pci_bridge_regs[0x1c] = 0xf0;

        mvp3.pci_bridge_regs[0x20] = 0xf0;
        mvp3.pci_bridge_regs[0x21] = 0xff;

        mvp3.pci_bridge_regs[0x24] = 0xf0;
        mvp3.pci_bridge_regs[0x25] = 0xff;
}
