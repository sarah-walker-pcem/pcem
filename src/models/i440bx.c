#include <string.h>

#include "ibm.h"
#include "io.h"
#include "keyboard_at.h"
#include "mem.h"
#include "pci.h"
#include "x86.h"

#include "i440bx.h"

static uint8_t card_i440bx[256];

static void i440bx_map(uint32_t addr, uint32_t size, int state)
{
        switch (state & 3)
        {
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

void i440bx_write(int func, int addr, uint8_t val, void *priv)
{
        if (func)
                return;

        if (addr >= 0x10 && addr < 0x4f)
                return;

        switch (addr)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                case 0x0c: case 0x0e:
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
                val = 0x02;
                break;

                case 0x59: /*PAM0*/
                if ((card_i440bx[0x59] ^ val) & 0xf0)
                {
                        i440bx_map(0xf0000, 0x10000, val >> 4);
                }
                pclog("i440bx_write : PAM0 write %02X\n", val);
//                if (val == 0x10)
//                        output = 3;
                break;
                case 0x5a: /*PAM1*/
                if ((card_i440bx[0x5a] ^ val) & 0x0f)
                        i440bx_map(0xc0000, 0x04000, val & 0xf);
                if ((card_i440bx[0x5a] ^ val) & 0xf0)
                        i440bx_map(0xc4000, 0x04000, val >> 4);
                break;
                case 0x5b: /*PAM2*/
                if ((card_i440bx[0x5b] ^ val) & 0x0f)
                        i440bx_map(0xc8000, 0x04000, val & 0xf);
                if ((card_i440bx[0x5b] ^ val) & 0xf0)
                        i440bx_map(0xcc000, 0x04000, val >> 4);
                break;
                case 0x5c: /*PAM3*/
                if ((card_i440bx[0x5c] ^ val) & 0x0f)
                        i440bx_map(0xd0000, 0x04000, val & 0xf);
                if ((card_i440bx[0x5c] ^ val) & 0xf0)
                        i440bx_map(0xd4000, 0x04000, val >> 4);
                break;
                case 0x5d: /*PAM4*/
                if ((card_i440bx[0x5d] ^ val) & 0x0f)
                        i440bx_map(0xd8000, 0x04000, val & 0xf);
                if ((card_i440bx[0x5d] ^ val) & 0xf0)
                        i440bx_map(0xdc000, 0x04000, val >> 4);
                break;
                case 0x5e: /*PAM5*/
                if ((card_i440bx[0x5e] ^ val) & 0x0f)
                        i440bx_map(0xe0000, 0x04000, val & 0xf);
                if ((card_i440bx[0x5e] ^ val) & 0xf0)
                        i440bx_map(0xe4000, 0x04000, val >> 4);
                pclog("i440bx_write : PAM5 write %02X\n", val);
                break;
                case 0x5f: /*PAM6*/
                if ((card_i440bx[0x5f] ^ val) & 0x0f)
                        i440bx_map(0xe8000, 0x04000, val & 0xf);
                if ((card_i440bx[0x5f] ^ val) & 0xf0)
                        i440bx_map(0xec000, 0x04000, val >> 4);
                pclog("i440bx_write : PAM6 write %02X\n", val);
                break;
                
                case 0x72: /*SMRAM*/
//                pclog("Write SMRAM %02x was %02x\n", val,
                val = (val & 0x78) | 2; /*SMRAM always at A0000-BFFFF*/
                val |= (card_i440bx[0x72] & 0x10); /*D_LCK can not be cleared by software*/
                if (val & 0x10) /*D_LCK locks D_OPEN and G_SMRAME*/
                {
                        val &= ~0x48; /*D_OPEN is forced to 0, G_SMRAME is read only*/
                        val |= (card_i440bx[0x72] & 0x08);
                }
                if ((card_i440bx[0x72] ^ val) & 0x40)
                {
                        if (val & 0x40) /*SMRAM enabled*/
                        {
                                pclog("Enable SMRAM\n");
                                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        }
                        else
                        {
                                pclog("Disable SMRAM\n");
                                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                        }
                }
                break;
        }

        card_i440bx[addr] = val;
}

uint8_t i440bx_read(int func, int addr, void *priv)
{
        if (func)
                return 0xff;

        return card_i440bx[addr];
}

static void i440bx_smram_enable(void)
{
//        pclog("i440bx_smram_enable: 440BX 0x72=%02x\n", card_i440bx[0x72]);
        if (card_i440bx[0x72] & 8)
                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
}
static void i440bx_smram_disable(void)
{
//        pclog("i440bx_smram_disable: 440BX 0x72=%02x\n", card_i440bx[0x72]);
        if (card_i440bx[0x72] & 8)
                mem_set_mem_state(0xa0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
}

void i440bx_init()
{
        pci_add_specific(0, i440bx_read, i440bx_write, NULL);

        memset(card_i440bx, 0, 256);
        card_i440bx[0x00] = 0x86; card_i440bx[0x01] = 0x80; /*Intel*/
        card_i440bx[0x02] = 0x92; card_i440bx[0x03] = 0x71; /*82441FX*/
        card_i440bx[0x04] = 0x06; card_i440bx[0x05] = 0x00;
        card_i440bx[0x06] = 0x00; card_i440bx[0x07] = 0x02;
        card_i440bx[0x08] = 0x02; /*B1 stepping*/
        card_i440bx[0x09] = 0x00; card_i440bx[0x0a] = 0x00; card_i440bx[0x0b] = 0x06;
        card_i440bx[0x51] = 0x20; /*66 MHz bus*/
        card_i440bx[0x60] = card_i440bx[0x61] = card_i440bx[0x62] = card_i440bx[0x63] = 0x01;
        card_i440bx[0x64] = card_i440bx[0x65] = card_i440bx[0x66] = card_i440bx[0x67] = 0x01;
        card_i440bx[0x72] = 0x02;
        card_i440bx[0x73] = 0x38;
        card_i440bx[0x7a] = 0x02; /*AGP disabled*/

        smram_enable = i440bx_smram_enable;
        smram_disable = i440bx_smram_disable;
}

void i440bx_reset()
{
        i440bx_write(0, 0x59, 0xf, NULL); /*Should reset all PCI devices, but just set PAM0 to point to ROM for now*/
        i440bx_write(0, 0x5a, 0x0, NULL);
        i440bx_write(0, 0x5b, 0x0, NULL);
        i440bx_write(0, 0x5c, 0x0, NULL);
        i440bx_write(0, 0x5d, 0x0, NULL);
        i440bx_write(0, 0x5e, 0x0, NULL);
        i440bx_write(0, 0x5f, 0x0, NULL);
}
