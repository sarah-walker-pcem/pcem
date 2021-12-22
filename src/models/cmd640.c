/*CMD640 emulation
  Currently only legacy mode emulated*/
#include "ibm.h"
#include "cmd640.h"
#include "ide.h"
#include "pci.h"

static uint8_t card_cmd640[256];

void cmd640_write(int func, int addr, uint8_t val, void *priv)
{
//        pclog("cmd640_write: func=%d addr=%02x val=%02x %04x:%08x\n", func, addr, val, CS, cpu_state.pc);
        if (func > 0)
                return;
        
        switch (addr)
        {
                case 0x04:
                ide_pri_disable();
                ide_sec_disable();
                if (val & 0x01)
                {
                        ide_pri_enable();
                        ide_sec_enable();
                }
                card_cmd640[0x04] = val & 0x41;
                break;
                case 0x07:
                card_cmd640[0x07] = val & 0x86;
                break;
                case 0x3c:
                card_cmd640[0x3c] = val;
                break;
        }
}

static uint8_t cmd640_read(int func, int addr, void *p)
{
//        pclog("cmd640_read: func=%d addr=%02x val=%02x %04x:%08x\n", func, addr, card_cmd640[addr], CS, cpu_state.pc);
        if (func > 0)
                return 0xff;

        return card_cmd640[addr];
}

void cmd640b_init(int card)
{
        pci_add_specific(card, cmd640_read, cmd640_write, NULL);
        
        memset(card_cmd640, 0, 256);
        card_cmd640[0x00] = 0x95; card_cmd640[0x01] = 0x10; /**/
        card_cmd640[0x02] = 0x40; card_cmd640[0x03] = 0x06; /*CMD640*/
        card_cmd640[0x04] = 0x01; card_cmd640[0x05] = 0x00;
        card_cmd640[0x06] = 0x00; card_cmd640[0x07] = 0x02;
        card_cmd640[0x08] = 0x00; /*Revision*/
        card_cmd640[0x09] = 0x00; /*Programming interface*/
        card_cmd640[0x0a] = 0x01; /*Sub class - IDE controller*/
        card_cmd640[0x0b] = 0x01; /*Base class - Mass storage controller*/
        

        card_cmd640[0x3c] = 0x0e; /*Interrupt line*/
        card_cmd640[0x3d] = 1;

        card_cmd640[0x50] = 0x02; /*Revision = 2*/
        card_cmd640[0x51] = 0x80; /*2nd port disabled, drive 1 read ahead disabled*/
        card_cmd640[0x57] = 0x0c; /*Drive 2/3 read ahead disabled*/
        card_cmd640[0x59] = 0x40; /*Burst length*/

        ide_pri_disable();
        ide_sec_disable();
}
