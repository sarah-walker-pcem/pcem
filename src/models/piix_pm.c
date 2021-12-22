#include "ibm.h"
#include "io.h"
#include "piix.h"
#include "piix_pm.h"
#include "x86.h"

#define DEVSTS_TRP_STS_DEV13 (1 << 29)

#define DEVCTL_TRP_EN_DEV13 (1 << 25)

#define GLBSTS_APM_STS (1 << 5)

static uint8_t dev13_read(uint16_t addr, void *p)
{
        piix_t *piix = (piix_t *)p;

//        pclog("dev13_read: addr=%04x  %08x\n", addr, piix.pm.devctl);
        if (piix->pm.devctl & DEVCTL_TRP_EN_DEV13)
        {
                piix->pm.devsts |= DEVSTS_TRP_STS_DEV13;
                x86_smi_trigger();
        }
        return 0xff;
}
static void dev13_write(uint16_t addr, uint8_t val, void *p)
{
        piix_t *piix = (piix_t *)p;

//        pclog("dev13_write: addr=%04x val=%02x  %08x\n", addr, val, piix.pm.devctl);
        if (piix->pm.devctl & DEVCTL_TRP_EN_DEV13)
        {
                piix->pm.devsts |= DEVSTS_TRP_STS_DEV13;
                x86_smi_trigger();
        }
}

static void dev13_recalc(piix_t *piix)
{
        if (piix->pm.dev13_enable)
                io_removehandler(piix->pm.dev13_base, piix->pm.dev13_size,
                                dev13_read, NULL, NULL,
                                dev13_write, NULL, NULL, piix);
        piix->pm.dev13_base = piix->card_pm[0x70] | (piix->card_pm[0x71] << 8);
        piix->pm.dev13_size = (piix->card_pm[0x72] & 0xf) + 1;
        piix->pm.dev13_enable = piix->card_pm[0x72] & 0x10;
        if (piix->pm.dev13_enable)
                io_sethandler(piix->pm.dev13_base, piix->pm.dev13_size,
                                dev13_read, NULL, NULL,
                                dev13_write, NULL, NULL, piix);
}

static void piix_pm_write(uint16_t port, uint8_t val, void *p)
{
        piix_t *piix = (piix_t *)p;

//        pclog("piix_pm_write: port=%04x val=%02x\n", port, val);
        switch (port & 0x3f)
        {
                case 0x00:
                piix->pm.pmsts &= ~(val & 0x31);
                break;
                case 0x01:
                piix->pm.pmsts &= ~((val & 0x8d) << 8);
                break;

                case 0x02:
                piix->pm.pmen = (piix->pm.pmen & ~0xff) | (val & 0x21);
                break;
                case 0x03:
                piix->pm.pmen = (piix->pm.pmen & ~0xff00) | ((val & 0x05) << 8);
                break;

                case 0x04:
                piix->pm.pmcntrl = (piix->pm.pmcntrl & ~0xff) | (val & 0x03);
                break;
                case 0x05:
                pclog("PM reg 5 write %02x\n", val);
                if (val & 0x20)
                {
                        pclog("PIIX transition to power state %i\n", (val >> 2) & 7);
                        if (val == 0x20)
                        {
                                pclog("Power off\n");
                                stop_emulation_now();
                        }
                }
                piix->pm.pmcntrl = (piix->pm.pmcntrl & ~0xff00) | ((val & 0x1c) << 8);
                break;

                case 0x0c: case 0x0d:
                piix->pm.gpsts &= ~(val << ((port & 1) * 8));
                break;

                case 0x0e:
                piix->pm.pmen = (piix->pm.pmen & ~0xff) | (val & 0x01);
                break;
                case 0x0f:
                piix->pm.pmen = (piix->pm.pmen & ~0xff00) | ((val & 0x0f) << 8);
                break;

                case 0x10:
                piix->pm.pcntrl = (piix->pm.pcntrl & ~0xff) | (val & 0x1e);
                break;
                case 0x11:
                piix->pm.pcntrl = (piix->pm.pcntrl & ~0xff00) | ((val & 0x3e) << 8);
                break;

                case 0x18:
                piix->pm.glbsts &= ~(val & 0x25);
                break;
                case 0x19:
                piix->pm.glbsts &= ~((val & 0x0d) << 8);
                break;

                case 0x1c: case 0x1d: case 0x1e: case 0x1f:
                piix->pm.devsts &= ~(val << ((port & 3) * 8));
                break;

                case 0x20:
                piix->pm.glben = (piix->pm.glben & ~0xff) | (val & 0x01);
                break;
                case 0x21:
                piix->pm.glben = (piix->pm.glben & ~0xff00) | ((val & 0x0f) << 8);
                break;

                case 0x28:
                piix->pm.glbctl = (piix->pm.glbctl & ~0xff) | (val & 0x05);
                break;
                case 0x29:
                piix->pm.glbctl = (piix->pm.glbctl & ~0xff00) | ((val & 0xff) << 8);
                break;
                case 0x2a:
                piix->pm.glbctl = (piix->pm.glbctl & ~0xff0000) | ((val & 0x01) << 16);
                break;
                case 0x2b:
                piix->pm.glbctl = (piix->pm.glbctl & ~0xff000000) | ((val & 0x07) << 24);
                break;

                case 0x2c:
                piix->pm.devctl = (piix->pm.devctl & ~0xff) | val;
                break;
                case 0x2d:
                piix->pm.devctl = (piix->pm.devctl & ~0xff00) | (val << 8);
                break;
                case 0x2e:
                piix->pm.devctl = (piix->pm.devctl & ~0xff0000) | (val << 16);
                break;
                case 0x2f:
                piix->pm.devctl = (piix->pm.devctl & ~0xff000000) | ((val & 0x0f) << 24);
                break;

                case 0x34:
                piix->pm.gporeg = (piix->pm.gporeg & ~0xff) | val;
                break;
                case 0x35:
                piix->pm.gporeg = (piix->pm.gporeg & ~0xff00) | (val << 8);
                break;
                case 0x36:
                piix->pm.gporeg = (piix->pm.gporeg & ~0xff0000) | (val << 16);
                break;
                case 0x37:
                piix->pm.gporeg = (piix->pm.gporeg & ~0xff000000) | (val << 24);
                break;
        }
}
static uint8_t piix_pm_read(uint16_t port, void *p)
{
        piix_t *piix = (piix_t *)p;
        uint8_t ret = 0xff;

        switch (port & 0x3f)
        {
                case 0x00: case 0x01:
                ret = piix->pm.pmsts >> ((port & 1) * 8);
                break;

                case 0x02: case 0x03:
                ret = piix->pm.pmen >> ((port & 1) * 8);
                break;

                case 0x04: case 0x05:
                ret = piix->pm.pmcntrl >> ((port & 1) * 8);
                break;

                case 0x08: case 0x09: case 0x0a: case 0x0b:
                {
                        uint64_t timer = tsc - piix->pm.timer_offset;
                        uint32_t pm_timer = (timer * 3579545) / cpu_get_speed();

                        ret = pm_timer >> ((port & 3) * 8);
                        break;
                }

                case 0x0c: case 0x0d:
                ret = piix->pm.gpsts >> ((port & 1) * 8);
                break;

                case 0x0e: case 0x0f:
                ret = piix->pm.pmen >> ((port & 1) * 8);
                break;

                case 0x10: case 0x11: case 0x12: case 0x13:
                ret = piix->pm.pcntrl >> ((port & 3) * 8);
                break;

                case 0x14:
                ret = 0;
                break;
                case 0x15:
                ret = 0;
                break;

                case 0x18: case 0x19:
                ret = piix->pm.glbsts >> ((port & 1) * 8);
                break;

                case 0x1c: case 0x1d: case 0x1e: case 0x1f:
                ret = piix->pm.devsts >> ((port & 3) * 8);
                break;

                case 0x20: case 0x21:
                ret = piix->pm.glben >> ((port & 1) * 8);
                break;

                case 0x28: case 0x29: case 0x2a: case 0x2b:
                ret = piix->pm.glbctl >> ((port & 3) * 8);
                break;

                case 0x2c: case 0x2d: case 0x2e: case 0x2f:
                ret = piix->pm.devctl >> ((port & 3) * 8);
                break;

                case 0x30:
                ret = 0x00;
                break;
                case 0x31:
                ret = 0x00;
                break;
                case 0x32:
                /*GA686BX - bit 1 = low battery*/
                ret = 0x00;
                break;

                case 0x34: case 0x35: case 0x36: case 0x37:
                ret = piix->pm.gporeg >> ((port & 3) * 8);
                break;

//                default:
//                fatal("piix_pm_read: unknown read %04x\n", port);
        }

//        pclog("piix_pm_read: port=%04x val=%02x\n", port, ret);
        return ret;
}

static void piix_smbus_write(uint16_t port, uint8_t val, void *p)
{
        piix_t *piix = (piix_t *)p;
//        pclog("piix_smbus_write: port=%04x val=%02x\n", port, val);

        switch (port & 0xf)
        {
                case 0x2:
                piix->smbus.host_ctrl = (val & 0x5f) | (piix->smbus.host_ctrl & 0x40);
                break;
        }
}
static uint8_t piix_smbus_read(uint16_t port, void *p)
{
        piix_t *piix = (piix_t *)p;
        uint8_t ret = 0xff;

        switch (port & 0x0f)
        {
                case 0x0:
                ret = 0;
                break;

                case 0x2:
                ret = piix->smbus.host_ctrl;
                break;

                case 0x5:
                ret = 0;
                break;

//                default:
//                fatal("piix_smbus_read: unknown read %04x\n", port);
        }

        return ret;
}

static uint8_t piix4_apm_read(uint16_t port, void *p)
{
        piix_t *piix = (piix_t *)p;
        uint8_t ret = 0xff;

        switch (port)
        {
                case 0xb2:
                ret = piix->pm.apmc;
                break;

                case 0xb3:
                ret = piix->pm.apms;
                break;
        }
//        pclog("piix_apm_read: port=%04x ret=%02x\n", port, ret);
        return ret;
}
static void piix4_apm_write(uint16_t port, uint8_t val, void *p)
{
        piix_t *piix = (piix_t *)p;

//        pclog("piix_apm_write: port=%04x val=%02x\n", port, val);

        switch (port)
        {
                case 0xb2:
                piix->pm.apmc = val;
                if (piix->card_pm[0x5b] & (1 << 1))
                {
                        piix->pm.glbsts |= GLBSTS_APM_STS;
//                        pclog("APMC write causes SMI\n");
                        x86_smi_trigger();
                }
                break;

                case 0xb3:
                piix->pm.apms = val;
                break;
        }
}

static void pm_clear_io(piix_t *piix)
{
        io_removehandler(piix->pmba, 0x0040, piix_pm_read, NULL, NULL, piix_pm_write, NULL, NULL,  piix);
        io_removehandler(piix->smbba, 0x0010, piix_smbus_read, NULL, NULL, piix_smbus_write, NULL, NULL,  piix);
}
static void pm_set_io(piix_t *piix)
{
//        pclog("pm_set_io: %02x %04x\n", piix.card_pm[0x04], piix.smbba);
        if ((piix->card_pm[0x04] & 0x01) && piix->pmba)
        {
//                pclog("PMBA=%04x\n", piix.pmba);
                io_sethandler(piix->pmba, 0x0040, piix_pm_read, NULL, NULL, piix_pm_write, NULL, NULL,  piix);
        }
        if ((piix->card_pm[0x04] & 0x01) && piix->smbba)
        {
//                pclog("SMBBA=%04x\n", piix.smbba);
                io_sethandler(piix->smbba, 0x0010, piix_smbus_read, NULL, NULL, piix_smbus_write, NULL, NULL,  piix);
        }
}

void piix_pm_pci_write(int addr, uint8_t val, void *p)
{
        piix_t *piix = (piix_t *)p;

        switch (addr)
        {
                case 0x04:
                pm_clear_io(piix);
                piix->card_pm[0x04] = val & 1;
                pm_set_io(piix);
                break;
                case 0x40:
                pm_clear_io(piix);
                piix->pmba = (piix->pmba & 0xff00) | (val & 0xc0);
                pm_set_io(piix);
                break;
                case 0x41:
                pm_clear_io(piix);
                piix->pmba = (piix->pmba & 0xc0) | (val << 8);
                pm_set_io(piix);
                break;
                case 0x58: case 0x59: case 0x5b: case 0x5c:
                piix->card_pm[addr] = val;
                break;
                case 0x70: case 0x71: case 0x72:
                piix->card_pm[addr] = val;
                dev13_recalc(piix);
                break;
                case 0x90:
                pm_clear_io(piix);
                piix->smbba = (piix->smbba & 0xff00) | (val & 0xf0);
                pm_set_io(piix);
                break;
                case 0x91:
                pm_clear_io(piix);
                piix->smbba = (piix->smbba & 0xf0) | (val << 8);
                pm_set_io(piix);
                break;
        }
}
uint8_t piix_pm_pci_read(int addr, void *p)
{
        piix_t *piix = (piix_t *)p;

        return piix->card_pm[addr];
}

void piix_pm_init(piix_t *piix)
{
        memset(&piix->card_pm, 0, sizeof(piix->card_pm));
        piix->card_pm[0x00] = 0x86; piix->card_pm[0x01] = 0x80; /*Intel*/
        piix->card_pm[0x02] = 0x13; piix->card_pm[0x03] = 0x71; /*82371EB (PIIX4)*/
        piix->card_pm[0x06] = 0x80; piix->card_pm[0x07] = 0x02;
        piix->card_pm[0x09] = 0x00; piix->card_pm[0x0a] = 0x80; piix->card_pm[0x0b] = 0x06; /*Bridge device*/
        piix->card_pm[0x3d] = 0x01; /*IRQA*/
        piix->card_pm[0x40] = 0x01;
        piix->card_pm[0x90] = 0x01;
        piix->pm.timer_offset = 0;
        piix->pm.gporeg = 0x7fffbfff;

        io_sethandler(0x00b2, 0x0002, piix4_apm_read, NULL, NULL, piix4_apm_write, NULL, NULL, piix);
}
