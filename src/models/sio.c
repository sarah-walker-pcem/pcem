#include <string.h>

#include "ibm.h"
#include "ide.h"
#include "io.h"
#include "mem.h"
#include "pci.h"

#include "sio.h"

typedef struct sio_t {
        struct {
                uint8_t apmc, apms;
        } pm;
} sio_t;
static sio_t sio;
static uint8_t card_sio[256];
// static int pci_cards_ids[4];

#define REG_SMIEN 0xa2
#define REG_SMIREQ 0xaa

#define SMIEN_APMC (1 << 7)
#define SMIREQ_RAPMC (1 << 7)

void sio_write(int func, int addr, uint8_t val, void *priv) {
        //        pclog("sio_write: func=%d addr=%02x val=%02x %04x:%08x\n", func, addr, val, CS, cpu_state.pc);
        if (func > 0)
                return;

        if (addr >= 0x0f && addr < 0x4c)
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
        case 0x0e:
                return;

        case 0x04: /*Command register*/
                val &= 0x08;
                val |= 0x07;
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

        case 0x60:
                //                pclog("IRQ routing %02x %02x\n", addr, val);
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTA, val & 0xf);
                break;
        case 0x61:
                //                pclog("IRQ routing %02x %02x\n", addr, val);
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTC, val & 0xf);
                break;
        case 0x62:
                //                pclog("IRQ routing %02x %02x\n", addr, val);
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTB, val & 0xf);
                break;
        case 0x63:
                //                pclog("IRQ routing %02x %02x\n", addr, val);
                if (val & 0x80)
                        pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTD, val & 0xf);
                break;

        case REG_SMIREQ:
                card_sio[addr] &= val;
                return;
        case REG_SMIREQ + 1:
                card_sio[addr] &= val;
                return;
        }
        card_sio[addr] = val;
}

uint8_t sio_read(int func, int addr, void *priv) {
        //        pclog("sio_read: func=%d addr=%02x %04x:%08x\n", func, addr, CS, pc);
        if (func > 0)
                return 0xff;

        return card_sio[addr];
}

static uint8_t sio_apm_read(uint16_t port, void *p) {
        sio_t *sio = (sio_t *)p;
        uint8_t ret = 0xff;

        switch (port) {
        case 0xb2:
                ret = sio->pm.apmc;
                break;

        case 0xb3:
                ret = sio->pm.apms;
                break;
        }
        //        pclog("sio_apm_read: port=%04x ret=%02x\n", port, ret);
        return ret;
}

static void sio_apm_write(uint16_t port, uint8_t val, void *p) {
        sio_t *sio = (sio_t *)p;

        //        pclog("sio_apm_write: port=%04x val=%02x\n", port, val);

        switch (port) {
        case 0xb2:
                sio->pm.apmc = val;
                if (card_sio[REG_SMIEN] & SMIEN_APMC) {
                        card_sio[REG_SMIREQ] |= SMIREQ_RAPMC;
                        //                        pclog("APMC write causes SMI\n");
                        x86_smi_trigger();
                }
                break;

        case 0xb3:
                sio->pm.apms = val;
                break;
        }
}

void sio_init(int card, int pci_a, int pci_b, int pci_c, int pci_d) {
        memset(&sio, 0, sizeof(sio_t));

        pci_add_specific(card, sio_read, sio_write, NULL);

        memset(card_sio, 0, 256);
        card_sio[0x00] = 0x86;
        card_sio[0x01] = 0x80; /*Intel*/
        card_sio[0x02] = 0x84;
        card_sio[0x03] = 0x04; /*82378IB (SIO)*/
        card_sio[0x04] = 0x07;
        card_sio[0x05] = 0x00;
        card_sio[0x06] = 0x00;
        card_sio[0x07] = 0x02;
        card_sio[0x08] = 0x03; /*A0 stepping*/

        card_sio[0x40] = 0x20;
        card_sio[0x41] = 0x00;
        card_sio[0x42] = 0x04;
        card_sio[0x43] = 0x00;
        card_sio[0x44] = 0x20;
        card_sio[0x45] = 0x10;
        card_sio[0x46] = 0x0f;
        card_sio[0x47] = 0x00;
        card_sio[0x48] = 0x01;
        card_sio[0x49] = 0x10;
        card_sio[0x4a] = 0x10;
        card_sio[0x4b] = 0x0f;
        card_sio[0x4c] = 0x56;
        card_sio[0x4d] = 0x40;
        card_sio[0x4e] = 0x07;
        card_sio[0x4f] = 0x4f;
        card_sio[0x54] = 0x00;
        card_sio[0x55] = 0x00;
        card_sio[0x56] = 0x00;
        card_sio[0x60] = 0x80;
        card_sio[0x61] = 0x80;
        card_sio[0x62] = 0x80;
        card_sio[0x63] = 0x80;
        card_sio[0x80] = 0x78;
        card_sio[0x81] = 0x00;
        card_sio[0xa0] = 0x08;
        card_sio[0xa8] = 0x0f;

        if (pci_a)
                pci_set_card_routing(pci_a, PCI_INTA);
        if (pci_b)
                pci_set_card_routing(pci_b, PCI_INTB);
        if (pci_c)
                pci_set_card_routing(pci_c, PCI_INTC);
        if (pci_d)
                pci_set_card_routing(pci_d, PCI_INTD);

        io_sethandler(0x00b2, 0x0002, sio_apm_read, NULL, NULL, sio_apm_write, NULL, NULL, &sio);
}
