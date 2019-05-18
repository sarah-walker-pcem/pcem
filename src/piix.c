/*PRD format :
        
        word 0 - base address
        word 1 - bits 1 - 15 = byte count, bit 31 = end of transfer
*/
#include <string.h>

#include "ibm.h"
#include "ide.h"
#include "ide_sff8038i.h"
#include "keyboard_at.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "pic.h"

#include "piix.h"

static uint8_t card_piix[256], card_piix_ide[256];
static sff_busmaster_t piix_busmaster[2];
static uint8_t piix_rc = 0;
static void (*piix_nb_reset)();

void piix_write(int func, int addr, uint8_t val, void *priv)
{
//        pclog("piix_write: func=%d addr=%02x val=%02x %04x:%08x\n", func, addr, val, CS, pc);
        if (func > 1)
           return;
        
        if (func == 1) /*IDE*/
        {
                switch (addr)
                {
                        case 0x04:
                        card_piix_ide[0x04] = (card_piix_ide[0x04] & ~5) | (val & 5);
                        break;
                        case 0x07:
                        card_piix_ide[0x07] = (card_piix_ide[0x07] & ~0x38) | (val & 0x38);
                        break;
                        case 0x0d:
                        card_piix_ide[0x0d] = val;
                        break;
                        
                        case 0x20:
                        card_piix_ide[0x20] = (val & ~0x0f) | 1;
                        break;
                        case 0x21:
                        card_piix_ide[0x21] = val;
                        break;
                        
                        case 0x40:
                        card_piix_ide[0x40] = val;
                        break;
                        case 0x41:
                        card_piix_ide[0x41] = val;
                        break;
                        case 0x42:
                        card_piix_ide[0x42] = val;
                        break;
                        case 0x43:
                        card_piix_ide[0x43] = val;
                        break;
                }
                if (addr == 4 || (addr & ~3) == 0x20) /*Bus master base address*/                
                {
                        uint16_t base = (card_piix_ide[0x20] & 0xf0) | (card_piix_ide[0x21] << 8);
                        io_removehandler(0, 0x10000, sff_bus_master_read, NULL, NULL, sff_bus_master_write, NULL, NULL,  piix_busmaster);
                        if (card_piix_ide[0x04] & 1)
                                io_sethandler(base, 0x10, sff_bus_master_read, NULL, NULL, sff_bus_master_write, NULL, NULL,  piix_busmaster);
                }
                if (addr == 4 || addr == 0x41 || addr == 0x43)
                {
                        ide_pri_disable();
                        ide_sec_disable();
                        if (card_piix_ide[0x04] & 1)
                        {
                                if (card_piix_ide[0x41] & 0x80)
                                        ide_pri_enable();
                                if (card_piix_ide[0x43] & 0x80)
                                        ide_sec_enable();
                        }
                }
//                pclog("PIIX write %02X %02X\n", addr, val);
        }
        else
        {
                if (addr >= 0x0f && addr < 0x4c)
                        return;

                switch (addr)
                {
                        case 0x00: case 0x01: case 0x02: case 0x03:
                        case 0x08: case 0x09: case 0x0a: case 0x0b:
                        case 0x0e:
                        return;
                        
                        case 0x60:
//                        pclog("IRQ routing %02x %02x\n", addr, val);
                        if (val & 0x80)
                                pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
                        else
                                pci_set_irq_routing(PCI_INTA, val & 0xf);
                        break;
                        case 0x61:
//                        pclog("IRQ routing %02x %02x\n", addr, val);
                        if (val & 0x80)
                                pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
                        else
                                pci_set_irq_routing(PCI_INTB, val & 0xf);
                        break;
                        case 0x62:
//                        pclog("IRQ routing %02x %02x\n", addr, val);
                        if (val & 0x80)
                                pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
                        else
                                pci_set_irq_routing(PCI_INTC, val & 0xf);
                        break;
                        case 0x63:
//                        pclog("IRQ routing %02x %02x\n", addr, val);
                        if (val & 0x80)
                                pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
                        else
                                pci_set_irq_routing(PCI_INTD, val & 0xf);
                        break;
                }
                card_piix[addr] = val;
        }
}

uint8_t piix_read(int func, int addr, void *priv)
{
//        pclog("piix_read: func=%d addr=%02x %04x:%08x\n", func, addr, CS, pc);
        if (func > 1)
           return 0xff;

        if (func == 1) /*IDE*/
        {
//                pclog("PIIX IDE read %02X %02X\n", addr, card_piix_ide[addr]);                
                return card_piix_ide[addr];
        }
        else
           return card_piix[addr];
}


uint8_t piix_rc_read(uint16_t port, void *p)
{
        return piix_rc;
}

void piix_rc_write(uint16_t port, uint8_t val, void *p)
{
        if (val & 4)
        {
                if (val & 2) /*Hard reset*/
                {
                        piix_nb_reset();
                        keyboard_at_reset(); /*Reset keyboard controller to reset system flag*/
                        ide_reset_devices();
                }
                resetx86();
        }

        piix_rc = val & ~4;
}

void piix_init(int card, int pci_a, int pci_b, int pci_c, int pci_d, void (*nb_reset)())
{
        pci_add_specific(card, piix_read, piix_write, NULL);
        
        memset(card_piix, 0, 256);
        card_piix[0x00] = 0x86; card_piix[0x01] = 0x80; /*Intel*/
        card_piix[0x02] = 0x2e; card_piix[0x03] = 0x12; /*82371FB (PIIX)*/
        card_piix[0x04] = 0x07; card_piix[0x05] = 0x00;
        card_piix[0x06] = 0x80; card_piix[0x07] = 0x02;
        card_piix[0x08] = 0x00; /*A0 stepping*/
        card_piix[0x09] = 0x00; card_piix[0x0a] = 0x01; card_piix[0x0b] = 0x06;
        card_piix[0x0e] = 0x80; /*Multi-function device*/
        card_piix[0x4c] = 0x4d;
        card_piix[0x4e] = 0x03;
        card_piix[0x60] = card_piix[0x61] = card_piix[0x62] = card_piix[0x63] = 0x80;
        card_piix[0x69] = 0x02;
        card_piix[0x70] = card_piix[0x71] = 0x80;
        card_piix[0x76] = card_piix[0x77] = 0x0c;
        card_piix[0x78] = 0x02; card_piix[0x79] = 0x00;
        card_piix[0xa0] = 0x08;
        card_piix[0xa2] = card_piix[0xa3] = 0x00;
        card_piix[0xa4] = card_piix[0xa5] = card_piix[0xa6] = card_piix[0xa7] = 0x00;
        card_piix[0xa8] = 0x0f;
        card_piix[0xaa] = card_piix[0xab] = 0x00;
        card_piix[0xac] = 0x00;
        card_piix[0xae] = 0x00;

        card_piix_ide[0x00] = 0x86; card_piix_ide[0x01] = 0x80; /*Intel*/
        card_piix_ide[0x02] = 0x30; card_piix_ide[0x03] = 0x12; /*82371FB (PIIX)*/
        card_piix_ide[0x04] = 0x02; card_piix_ide[0x05] = 0x00;
        card_piix_ide[0x06] = 0x80; card_piix_ide[0x07] = 0x02;
        card_piix_ide[0x08] = 0x00;
        card_piix_ide[0x09] = 0x80; card_piix_ide[0x0a] = 0x01; card_piix_ide[0x0b] = 0x01;
        card_piix_ide[0x0d] = 0x00;
        card_piix_ide[0x0e] = 0x00;
        card_piix_ide[0x20] = 0x01; card_piix_ide[0x21] = card_piix_ide[0x22] = card_piix_ide[0x23] = 0x00; /*Bus master interface base address*/
        card_piix_ide[0x40] = card_piix_ide[0x41] = 0x00;
        card_piix_ide[0x42] = card_piix_ide[0x43] = 0x00;

        sff_bus_master_init(piix_busmaster);
        
        pci_set_card_routing(pci_a, PCI_INTA);
        pci_set_card_routing(pci_b, PCI_INTB);
        pci_set_card_routing(pci_c, PCI_INTC);
        pci_set_card_routing(pci_d, PCI_INTD);
        
        ide_pri_disable();
        ide_sec_disable();
        
        pic_init_elcrx();

        io_sethandler(0x0cf9, 0x0001, piix_rc_read, NULL, NULL, piix_rc_write, NULL, NULL, NULL);
        piix_nb_reset = nb_reset;
}
