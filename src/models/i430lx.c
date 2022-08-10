#include <string.h>

#include "ibm.h"
#include "ide.h"
#include "io.h"
#include "keyboard_at.h"
#include "mem.h"
#include "pci.h"
#include "sio.h"
#include "x86.h"

#include "i430lx.h"

static uint8_t card_i430lx[256];

static void i430lx_map(uint32_t addr, uint32_t size, int state) {
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

void i430lx_write(int func, int addr, uint8_t val, void *priv) {
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
                val &= 0x42;
                val |= 0x04;
                break;
        case 0x05:
                val &= 0x01;
                break;

        case 0x06: /*Status*/
                val = 0;
                break;
        case 0x07:
                val = 0x02;
                break;

        case 0x59: /*PAM0*/
                if ((card_i430lx[0x59] ^ val) & 0xf0) {
                        i430lx_map(0xf0000, 0x10000, val >> 4);
                }
                pclog("i430lx_write : PAM0 write %02X\n", val);
                break;
        case 0x5a: /*PAM1*/
                if ((card_i430lx[0x5a] ^ val) & 0x0f)
                        i430lx_map(0xc0000, 0x04000, val & 0xf);
                if ((card_i430lx[0x5a] ^ val) & 0xf0)
                        i430lx_map(0xc4000, 0x04000, val >> 4);
                break;
        case 0x5b: /*PAM2*/
                if ((card_i430lx[0x5b] ^ val) & 0x0f)
                        i430lx_map(0xc8000, 0x04000, val & 0xf);
                if ((card_i430lx[0x5b] ^ val) & 0xf0)
                        i430lx_map(0xcc000, 0x04000, val >> 4);
                break;
        case 0x5c: /*PAM3*/
                if ((card_i430lx[0x5c] ^ val) & 0x0f)
                        i430lx_map(0xd0000, 0x04000, val & 0xf);
                if ((card_i430lx[0x5c] ^ val) & 0xf0)
                        i430lx_map(0xd4000, 0x04000, val >> 4);
                break;
        case 0x5d: /*PAM4*/
                if ((card_i430lx[0x5d] ^ val) & 0x0f)
                        i430lx_map(0xd8000, 0x04000, val & 0xf);
                if ((card_i430lx[0x5d] ^ val) & 0xf0)
                        i430lx_map(0xdc000, 0x04000, val >> 4);
                break;
        case 0x5e: /*PAM5*/
                if ((card_i430lx[0x5e] ^ val) & 0x0f)
                        i430lx_map(0xe0000, 0x04000, val & 0xf);
                if ((card_i430lx[0x5e] ^ val) & 0xf0)
                        i430lx_map(0xe4000, 0x04000, val >> 4);
                pclog("i430lx_write : PAM5 write %02X\n", val);
                break;
        case 0x5f: /*PAM6*/
                if ((card_i430lx[0x5f] ^ val) & 0x0f)
                        i430lx_map(0xe8000, 0x04000, val & 0xf);
                if ((card_i430lx[0x5f] ^ val) & 0xf0)
                        i430lx_map(0xec000, 0x04000, val >> 4);
                pclog("i430lx_write : PAM6 write %02X\n", val);
                break;

        case 0x72: /*SMRAM*/
                pclog("Write SMRAM %02x\n", val);
                val = (val & 0x38) | 2;            /*SMRAM always at A0000-BFFFF*/
                val |= (card_i430lx[0x72] & 0x08); /*D_LCK can not be cleared by software*/
                if (val & 0x08)                    /*D_LCK locks D_OPEN*/
                {
                        val &= ~0x20; /*D_OPEN is forced to 0*/
                }
                val |= (card_i430lx[0x72] & 0x08);
                if ((card_i430lx[0x72] ^ val) & 0x20) {
                        if (val & 0x20) /*SMRAM enabled*/
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

        card_i430lx[addr] = val;
}

uint8_t i430lx_read(int func, int addr, void *priv) {
        if (func)
                return 0xff;

        return card_i430lx[addr];
}

static uint8_t trc = 0;

uint8_t i430lx_trc_read(uint16_t port, void *p) { return trc; }

void i430lx_trc_write(uint16_t port, uint8_t val, void *p) {
        if ((val & 4) && !(trc & 4)) {
                if (val & 2) /*Hard reset*/
                {
                        i430lx_write(0, 0x59, 0xf,
                                     NULL);    /*Should reset all PCI devices, but just set PAM0 to point to ROM for now*/
                        card_i430lx[0x72] = 0; /*Also clear SMRAM register so BIOS will relocate SMBASE*/
                        keyboard_at_reset();   /*Reset keyboard controller to reset system flag*/
                        ide_reset_devices();
                        resetx86();
                } else
                        softresetx86();
        }

        trc = val;
}

static void i430lx_smram_enable(void) { mem_set_mem_state(0xa0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL); }
static void i430lx_smram_disable(void) { mem_set_mem_state(0xa0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL); }

void i430lx_init() {
        pci_add_specific(0, i430lx_read, i430lx_write, NULL);

        memset(card_i430lx, 0, 256);
        card_i430lx[0x00] = 0x86;
        card_i430lx[0x01] = 0x80; /*Intel*/
        card_i430lx[0x02] = 0xa3;
        card_i430lx[0x03] = 0x04; /*82434LX*/
        card_i430lx[0x04] = 0x06;
        card_i430lx[0x05] = 0x00;
        card_i430lx[0x06] = 0x00;
        card_i430lx[0x07] = 0x02;
        card_i430lx[0x08] = 0x03; /*A3 stepping*/
        card_i430lx[0x09] = 0x00;
        card_i430lx[0x0a] = 0x00;
        card_i430lx[0x0b] = 0x06;
        card_i430lx[0x50] = 0x80;
        card_i430lx[0x52] = 0x40; /*256kb PLB cache*/
                                  //        card_i430lx[0x53] = 0x14;
                                  //        card_i430lx[0x56] = 0x52; /*DRAM control*/
        card_i430lx[0x57] = 0x31;
        card_i430lx[0x60] = card_i430lx[0x61] = card_i430lx[0x62] = card_i430lx[0x63] = card_i430lx[0x64] = 0x02;
        //        card_i430lx[0x67] = 0x11;
        //        card_i430lx[0x69] = 0x03;
        //        card_i430lx[0x70] = 0x20;
        //        card_i430lx[0x72] = 0x02;
        //        card_i430lx[0x74] = 0x0e;
        //        card_i430lx[0x78] = 0x23;

        io_sethandler(0x0cf9, 0x0001, i430lx_trc_read, NULL, NULL, i430lx_trc_write, NULL, NULL, NULL);

        smram_enable = i430lx_smram_enable;
        smram_disable = i430lx_smram_disable;
}
