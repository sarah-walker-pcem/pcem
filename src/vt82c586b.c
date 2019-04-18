#include <string.h>

#include "ibm.h"
#include "ide.h"
#include "ide_sff8038i.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "x86.h"

#include "vt82c586b.h"

static struct
{
        uint8_t pci_isa_bridge_regs[256];
        uint8_t ide_regs[256];
        uint8_t usb_regs[256];
        uint8_t power_regs[256];
        
        uint8_t sysctrl;
        
        sff_busmaster_t busmaster[2];
        
        struct
        {
                uint16_t io_base;
        } usb;
        struct
        {
                uint16_t io_base;
        } power;
} vt82c586b;

#define ACPI_TIMER_FREQ 3579545

#define ACPI_IO_ENABLE   (1 << 7)
#define ACPI_TIMER_32BIT (1 << 3)

static uint8_t vt82c586b_read(int func, int addr, void *priv)
{
        switch (func)
        {
                case 0:
                return vt82c586b.pci_isa_bridge_regs[addr];

                case 1:
                return vt82c586b.ide_regs[addr];

                case 2:
                return vt82c586b.usb_regs[addr];

                case 3:
                return vt82c586b.power_regs[addr];

                default:
                return 0xff;
        }
}

static void vt82c586b_pci_isa_bridge_write(int addr, uint8_t val)
{
        /*Read-only addresses*/
        if ((addr < 4) || (addr == 5) || (addr == 6)
                        || (addr >= 8 && addr < 0x40)
                        || (addr == 0x49)
                        || (addr == 0x4b)
                        || (addr >= 0x51 && addr < 0x54)
                        || (addr >= 0x5d && addr < 0x60)
                        || (addr >= 0x68 && addr < 0x6a)
                        || (addr >= 0x71))
                return;

        switch (addr)
        {
                case 4:
                vt82c586b.pci_isa_bridge_regs[4] = (val & 8) | 7;
                break;
                case 6:
                vt82c586b.pci_isa_bridge_regs[6] &= ~(val & 0xb0);
                break;
                
                case 0x47:
                if ((val & 0x81) == 0x81)
                        resetx86();
                vt82c586b.pci_isa_bridge_regs[0x47] = val & 0xfe;
                break;
                
                case 0x55:
                if (!(val & 0xf0))
                        pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTD, val >> 4);
                vt82c586b.pci_isa_bridge_regs[0x55] = val;
                break;
                case 0x56:
                if (!(val & 0xf0))
                        pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTA, val >> 4);
                if (!(val & 0x0f))
                        pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTB, val & 0xf);
                vt82c586b.pci_isa_bridge_regs[0x56] = val;
                break;
                case 0x57:
                if (!(val & 0xf0))
                        pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
                else
                        pci_set_irq_routing(PCI_INTC, val >> 4);
                vt82c586b.pci_isa_bridge_regs[0x57] = val;
                break;

                default:
                vt82c586b.pci_isa_bridge_regs[addr] = val;
                break;
        }
}
static void vt82c586b_ide_write(int addr, uint8_t val)
{
        /*Read-only addresses*/
        if ((addr < 4) || (addr == 5) || (addr == 8)
                        || (addr >= 0xa && addr < 0x0d)
                        || (addr >= 0x0e && addr < 0x20)
                        || (addr >= 0x22 && addr < 0x3c)
                        || (addr >= 0x3d && addr < 0x40)
                        || (addr >= 0x54 && addr < 0x60)
                        || (addr >= 0x52 && addr < 0x68)
                        || (addr >= 0x62))
                return;

        switch (addr)
        {
                case 4:
                vt82c586b.ide_regs[4] = val & 0x85;
                break;
                case 6:
                vt82c586b.ide_regs[6] &= ~(val & 0xb0);
                break;

                case 9:
                vt82c586b.ide_regs[9] = (vt82c586b.pci_isa_bridge_regs[9] & ~0x70) | 0x8a;
                break;
                
                case 0x20:
                vt82c586b.ide_regs[0x20] = (val & ~0xf) | 1;
                break;

                default:
                vt82c586b.ide_regs[addr] = val;
                break;
        }

        if (addr == 4 || (addr & ~3) == 0x20) /*Bus master base address*/
        {
                uint16_t base = (vt82c586b.ide_regs[0x20] & 0xf0) | (vt82c586b.ide_regs[0x21] << 8);
                io_removehandler(0, 0x10000, sff_bus_master_read, NULL, NULL, sff_bus_master_write, NULL, NULL,  vt82c586b.busmaster);
                if (vt82c586b.ide_regs[0x04] & 1)
                        io_sethandler(base, 0x10, sff_bus_master_read, NULL, NULL, sff_bus_master_write, NULL, NULL,  vt82c586b.busmaster);
        }
        if (addr == 4 || addr == 0x40)
        {
                ide_pri_disable();
                ide_sec_disable();
                if (vt82c586b.ide_regs[0x04] & 1)
                {
                        if (vt82c586b.ide_regs[0x40] & 0x02)
                                ide_pri_enable();
                        if (vt82c586b.ide_regs[0x40] & 0x01)
                                ide_sec_enable();
                }
        }
}

static uint8_t usb_reg_read(uint16_t addr, void *p)
{
//        pclog("usb_reg_read: addr=%04x\n", addr);
        switch (addr & 0x1f)
        {
                case 0x10: case 0x11: case 0x12: case 0x13: /*Port status*/
                return 0;
        }
        return 0xff;
}
static void usb_reg_write(uint16_t addr, uint8_t val, void *p)
{
//        pclog("usb_reg_write: addr=%04x val=%02x\n", addr, val);
}
static void usb_update_io_mapping()
{
        io_removehandler(vt82c586b.usb.io_base, 0x20, usb_reg_read, NULL, NULL, usb_reg_write, NULL, NULL, NULL);
        vt82c586b.usb.io_base = (vt82c586b.usb_regs[0x20] & ~0x1f) | (vt82c586b.usb_regs[0x21] << 8);
        if (vt82c586b.usb_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO)
                io_sethandler(vt82c586b.usb.io_base, 0x20, usb_reg_read, NULL, NULL, usb_reg_write, NULL, NULL, NULL);
}
static void vt82c586b_usb_write(int addr, uint8_t val)
{
        /*Read-only addresses*/
        if ((addr < 4) || (addr == 5) || (addr == 6)
                        || (addr >= 8 && addr < 0xd)
                        || (addr >= 0xe && addr < 0x20)
                        || (addr >= 0x22 && addr < 0x3c)
                        || (addr >= 0x3e && addr < 0x40)
                        || (addr >= 0x42 && addr < 0x44)
                        || (addr >= 0x46 && addr < 0xc0)
                        || (addr >= 0xc2))
                return;

        switch (addr)
        {
                case 4:
                vt82c586b.usb_regs[4] = val & 0x97;
                usb_update_io_mapping();
                break;
                case 7:
                vt82c586b.usb_regs[7] = val & 0x7f;
                break;

                case 0x20:
                vt82c586b.usb_regs[0x20] = (val & ~0x1f) | 1;
                usb_update_io_mapping();
                break;
                case 0x21:
                vt82c586b.usb_regs[0x21] = val;
                usb_update_io_mapping();
                break;

                default:
                vt82c586b.usb_regs[addr] = val;
                break;
        }
//        pclog("  USB write %02x %02x\n", addr, val);
}

static uint8_t power_reg_read(uint16_t addr, void *p)
{
        uint32_t timer;

        switch (addr & 0xff)
        {
                case 0x08: case 0x09: case 0x0a: case 0x0b: /*ACPI timer*/
                timer = (tsc * ACPI_TIMER_FREQ) / cpu_get_speed();
                if (!(vt82c586b.power_regs[0x41] & ACPI_TIMER_32BIT))
                        timer &= 0x00ffffff;
                return (timer >> (8 * (addr & 3))) & 0xff;
        }
//        pclog("power_reg_read: addr=%04x\n", addr);
        return 0xff;
}
static void power_reg_write(uint16_t addr, uint8_t val, void *p)
{
//        pclog("power_reg_write: addr=%04x val=%02x\n", addr, val);
}
static void vt82c586b_power_write(int addr, uint8_t val)
{
        /*Read-only addresses*/
        if ((addr < 0xd) || (addr >= 0xe && addr < 0x40)
                        || (addr == 0x43)
                        || (addr == 0x48)
                        || (addr >= 0x4a && addr < 0x50)
                        || (addr >= 0x54))
                return;

        switch (addr)
        {
                case 0x41:
                io_removehandler(vt82c586b.power.io_base, 0x0100, power_reg_read, NULL, NULL, power_reg_write, NULL, NULL, NULL);
                vt82c586b.power_regs[addr] = val;
                if (vt82c586b.power_regs[0x41] & ACPI_IO_ENABLE)
                        io_sethandler(vt82c586b.power.io_base, 0x0100, power_reg_read, NULL, NULL, power_reg_write, NULL, NULL, NULL);
                break;

                case 0x49:
                io_removehandler(vt82c586b.power.io_base, 0x0100, power_reg_read, NULL, NULL, power_reg_write, NULL, NULL, NULL);
                vt82c586b.power_regs[addr] = val;
                vt82c586b.power.io_base = val << 8;
                if (vt82c586b.power_regs[0x41] & ACPI_IO_ENABLE)
                        io_sethandler(vt82c586b.power.io_base, 0x0100, power_reg_read, NULL, NULL, power_reg_write, NULL, NULL, NULL);
                break;
                
                default:
                vt82c586b.power_regs[addr] = val;
                break;
        }
}

static void vt82c586b_write(int func, int addr, uint8_t val, void *priv)
{
        switch (func)
        {
                case 0:
                vt82c586b_pci_isa_bridge_write(addr, val);
                break;

                case 1:
                vt82c586b_ide_write(addr, val);
                break;

                case 2:
                vt82c586b_usb_write(addr, val);
                break;

                case 3:
                vt82c586b_power_write(addr, val);
                break;
        }
}

uint8_t vt82c586b_read_sysctrl(uint16_t port, void *p)
{
        return vt82c586b.sysctrl;
}

void vt82c586b_write_sysctrl(uint16_t port, uint8_t val, void *p)
{
        if ((mem_a20_alt ^ val) & 2)
        {
                mem_a20_alt = val & 2;
                mem_a20_recalc();
        }
        if ((~vt82c586b.sysctrl & val) & 1)
        {
                softresetx86();
                cpu_set_edx();
        }
        vt82c586b.sysctrl = val;
}

void vt82c586b_init(int card, int pci_a, int pci_b, int pci_c, int pci_d)
{
        pci_add_specific(card, vt82c586b_read, vt82c586b_write, NULL);
        io_sethandler(0x0092, 0x0001, vt82c586b_read_sysctrl, NULL, NULL, vt82c586b_write_sysctrl, NULL, NULL,  NULL);

        memset(&vt82c586b, 0, sizeof(vt82c586b));
        vt82c586b.pci_isa_bridge_regs[0x00] = 0x06; vt82c586b.pci_isa_bridge_regs[0x01] = 0x11; /*VIA*/
        vt82c586b.pci_isa_bridge_regs[0x02] = 0x86; vt82c586b.pci_isa_bridge_regs[0x03] = 0x05; /*VT82C586B*/
        vt82c586b.pci_isa_bridge_regs[0x04] = 0x0f; vt82c586b.pci_isa_bridge_regs[0x05] = 0x00;
        vt82c586b.pci_isa_bridge_regs[0x06] = 0x00; vt82c586b.pci_isa_bridge_regs[0x07] = 0x02;
        vt82c586b.pci_isa_bridge_regs[0x0a] = 0x01;
        vt82c586b.pci_isa_bridge_regs[0x0b] = 0x06;
        vt82c586b.pci_isa_bridge_regs[0x0e] = 0x80;

        /*ISA bus control*/
        vt82c586b.pci_isa_bridge_regs[0x48] = 0x01;
        vt82c586b.pci_isa_bridge_regs[0x4a] = 0x04;
        vt82c586b.pci_isa_bridge_regs[0x4f] = 0x03;

        /*Plug and Play control*/
        vt82c586b.pci_isa_bridge_regs[0x50] = 0x24;
        vt82c586b.pci_isa_bridge_regs[0x59] = 0x04;

        vt82c586b.ide_regs[0x00] = 0x06; vt82c586b.ide_regs[0x01] = 0x11; /*VIA*/
        vt82c586b.ide_regs[0x02] = 0x71; vt82c586b.ide_regs[0x03] = 0x05;
        vt82c586b.ide_regs[0x04] = 0x80; vt82c586b.ide_regs[0x05] = 0x00;
        vt82c586b.ide_regs[0x06] = 0x80; vt82c586b.ide_regs[0x07] = 0x02;
        vt82c586b.ide_regs[0x09] = 0x85;
        vt82c586b.ide_regs[0x0a] = 0x01;
        vt82c586b.ide_regs[0x0b] = 0x01;

        vt82c586b.ide_regs[0x10] = 0xf1; vt82c586b.ide_regs[0x11] = 0x01;
        vt82c586b.ide_regs[0x14] = 0xf5; vt82c586b.ide_regs[0x15] = 0x03;
        vt82c586b.ide_regs[0x18] = 0x71; vt82c586b.ide_regs[0x19] = 0x01;
        vt82c586b.ide_regs[0x1c] = 0x75; vt82c586b.ide_regs[0x1d] = 0x03;
        vt82c586b.ide_regs[0x20] = 0x01; vt82c586b.ide_regs[0x21] = 0xcc;
        vt82c586b.ide_regs[0x3c] = 0x0e;

        vt82c586b.ide_regs[0x40] = 0x08;
        vt82c586b.ide_regs[0x41] = 0x02;
        vt82c586b.ide_regs[0x42] = 0x09;
        vt82c586b.ide_regs[0x43] = 0x3a;
        vt82c586b.ide_regs[0x44] = 0x68;
        vt82c586b.ide_regs[0x46] = 0xc0;
        vt82c586b.ide_regs[0x48] = 0xa8;
        vt82c586b.ide_regs[0x49] = 0xa8;
        vt82c586b.ide_regs[0x4a] = 0xa8;
        vt82c586b.ide_regs[0x4b] = 0xa8;
        vt82c586b.ide_regs[0x4c] = 0xff;
        vt82c586b.ide_regs[0x4e] = 0xff;
        vt82c586b.ide_regs[0x4f] = 0xff;
        vt82c586b.ide_regs[0x50] = 0x03;
        vt82c586b.ide_regs[0x51] = 0x03;
        vt82c586b.ide_regs[0x52] = 0x03;
        vt82c586b.ide_regs[0x53] = 0x03;
        vt82c586b.ide_regs[0x61] = 0x02;
        vt82c586b.ide_regs[0x68] = 0x02;
        
        vt82c586b.usb_regs[0x00] = 0x06; vt82c586b.usb_regs[0x01] = 0x11; /*VIA*/
        vt82c586b.usb_regs[0x02] = 0x38; vt82c586b.usb_regs[0x03] = 0x30;
        vt82c586b.usb_regs[0x04] = 0x00; vt82c586b.usb_regs[0x05] = 0x00;
        vt82c586b.usb_regs[0x06] = 0x00; vt82c586b.usb_regs[0x07] = 0x02;
        vt82c586b.usb_regs[0x0a] = 0x03;
        vt82c586b.usb_regs[0x0b] = 0x0c;
        vt82c586b.usb_regs[0x0d] = 0x16;
        vt82c586b.usb_regs[0x20] = 0x01;
        vt82c586b.usb_regs[0x21] = 0x03;
        vt82c586b.usb_regs[0x3d] = 0x04;

        vt82c586b.usb_regs[0x60] = 0x10;
        vt82c586b.usb_regs[0xc1] = 0x20;

        vt82c586b.power_regs[0x00] = 0x06; vt82c586b.power_regs[0x01] = 0x11; /*VIA*/
        vt82c586b.power_regs[0x02] = 0x40; vt82c586b.power_regs[0x03] = 0x30;
        vt82c586b.power_regs[0x04] = 0x00; vt82c586b.power_regs[0x05] = 0x00;
        vt82c586b.power_regs[0x06] = 0x80; vt82c586b.power_regs[0x07] = 0x02;
        vt82c586b.power_regs[0x08] = 0x10; /*Production version (3041)*/
        vt82c586b.power_regs[0x48] = 0x01;

        if (pci_a)
                pci_set_card_routing(pci_a, PCI_INTA);
        if (pci_b)
                pci_set_card_routing(pci_b, PCI_INTB);
        if (pci_c)
                pci_set_card_routing(pci_c, PCI_INTC);
        if (pci_d)
                pci_set_card_routing(pci_d, PCI_INTD);

        sff_bus_master_init(vt82c586b.busmaster);
        
        ide_pri_disable();
        ide_sec_disable();
}
