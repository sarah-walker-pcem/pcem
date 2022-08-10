#include <string.h>

#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pci.h"

#include "i430vx.h"

static uint8_t card_i430vx[256];

static void i430vx_map(uint32_t addr, uint32_t size, int state) {
        switch (state & 3) {
        case 0:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
        case 1:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                break;
        case 2:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                break;
        case 3:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                break;
        }
        flushmmucache_nopc();
}

void i430vx_write(int func, int addr, uint8_t val, void *priv) {
        if (func)
                return;

        if (addr >= 0x10 && addr < 0x4f)
                return;

        switch (addr) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0e:
                return;

        case 0x04: /*Command register*/
                val &= 0x02;
                val |= 0x04;
                break;
        case 0x05:
                val = 0;
                break;

        case 0x06: /*Status*/
                val = 0;
                break;
        case 0x07:
                val &= 0x80;
                val |= 0x02;
                break;

        case 0x59: /*PAM0*/
                if ((card_i430vx[0x59] ^ val) & 0xf0) {
                        i430vx_map(0xf0000, 0x10000, val >> 4);
                }
                pclog("i430vx_write : PAM0 write %02X\n", val);
                break;
        case 0x5a: /*PAM1*/
                if ((card_i430vx[0x5a] ^ val) & 0x0f)
                        i430vx_map(0xc0000, 0x04000, val & 0xf);
                if ((card_i430vx[0x5a] ^ val) & 0xf0)
                        i430vx_map(0xc4000, 0x04000, val >> 4);
                break;
        case 0x5b: /*PAM2*/
                if ((card_i430vx[0x5b] ^ val) & 0x0f)
                        i430vx_map(0xc8000, 0x04000, val & 0xf);
                if ((card_i430vx[0x5b] ^ val) & 0xf0)
                        i430vx_map(0xcc000, 0x04000, val >> 4);
                break;
        case 0x5c: /*PAM3*/
                if ((card_i430vx[0x5c] ^ val) & 0x0f)
                        i430vx_map(0xd0000, 0x04000, val & 0xf);
                if ((card_i430vx[0x5c] ^ val) & 0xf0)
                        i430vx_map(0xd4000, 0x04000, val >> 4);
                break;
        case 0x5d: /*PAM4*/
                if ((card_i430vx[0x5d] ^ val) & 0x0f)
                        i430vx_map(0xd8000, 0x04000, val & 0xf);
                if ((card_i430vx[0x5d] ^ val) & 0xf0)
                        i430vx_map(0xdc000, 0x04000, val >> 4);
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

        case 0x72: /*SMRAM*/
                pclog("Write SMRAM %02x\n", val);
                val = (val & 0x78) | 2;            /*SMRAM always at A0000-BFFFF*/
                val |= (card_i430vx[0x72] & 0x10); /*D_LCK can not be cleared by software*/
                if (val & 0x10)                    /*D_LCK locks D_OPEN and G_SMRAME*/
                {
                        val &= ~0x48; /*D_OPEN is forced to 0, G_SMRAME is read only*/
                        val |= (card_i430vx[0x72] & 0x08);
                }
                if ((card_i430vx[0x72] ^ val) & 0x40) {
                        if (val & 0x40) /*SMRAM enabled*/
                        {
                                pclog("Enable SMRAM\n");
                                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        } else {
                                pclog("Disable SMRAM\n");
                                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                        }
                }
                break;
        }

        card_i430vx[addr] = val;
}

uint8_t i430vx_read(int func, int addr, void *priv) {
        if (func)
                return 0xff;

        return card_i430vx[addr];
}

static void i430vx_smram_enable(void) {
        if (card_i430vx[0x72] & 8)
                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
}
static void i430vx_smram_disable(void) {
        if (card_i430vx[0x72] & 8)
                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
}

void i430vx_init() {
        pci_add_specific(0, i430vx_read, i430vx_write, NULL);

        memset(card_i430vx, 0, 256);
        card_i430vx[0x00] = 0x86;
        card_i430vx[0x01] = 0x80; /*Intel*/
        card_i430vx[0x02] = 0x30;
        card_i430vx[0x03] = 0x70; /*82437VX*/
        card_i430vx[0x04] = 0x06;
        card_i430vx[0x05] = 0x00;
        card_i430vx[0x06] = 0x00;
        card_i430vx[0x07] = 0x02;
        card_i430vx[0x08] = 0x00; /*A0 stepping*/
        card_i430vx[0x09] = 0x00;
        card_i430vx[0x0a] = 0x00;
        card_i430vx[0x0b] = 0x06;
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

        smram_enable = i430vx_smram_enable;
        smram_disable = i430vx_smram_disable;
}

void i430vx_reset() {
        i430vx_write(0, 0x59, 0xf, NULL); /*Should reset all PCI devices, but just set PAM0 to point to ROM for now*/
}
