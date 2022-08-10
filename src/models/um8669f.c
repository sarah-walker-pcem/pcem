/*um8669f :

  aa to 108 unlocks
  next 108 write is register select (Cx?)
  data read/write to 109
  55 to 108 locks

C1
bit 7 - enable PnP registers

PnP registers :

07 - device :
        0 = FDC
        1 = COM1
        2 = COM2
        3 = LPT1
        5 = Game port
30 - enable
60/61 - addr
70 - IRQ
74 - DMA*/

#include "ibm.h"

#include "fdc.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"
#include "um8669f.h"

typedef struct um8669f_t {
        int locked;
        int cur_reg_108;
        uint8_t regs_108[256];

        int cur_reg;
        int cur_device;
        struct {
                int enable;
                uint16_t addr;
                int irq;
                int dma;
        } dev[8];
} um8669f_t;

static um8669f_t um8669f_global;

#define DEV_FDC 0
#define DEV_COM1 1
#define DEV_COM2 2
#define DEV_LPT1 3
#define DEV_GAME 5

#define REG_DEVICE 0x07
#define REG_ENABLE 0x30
#define REG_ADDRHI 0x60
#define REG_ADDRLO 0x61
#define REG_IRQ 0x70
#define REG_DMA 0x74

void um8669f_pnp_write(uint16_t port, uint8_t val, void *p) {
        um8669f_t *um8669f = (um8669f_t *)p;

        if (port == 0x279)
                um8669f->cur_reg = val;
        else {
                if (um8669f->cur_reg == REG_DEVICE)
                        um8669f->cur_device = val & 7;
                else {
                        //                        pclog("Write UM8669F %02x [%02x] %02x\n", um8669f->cur_reg, um8669f->cur_device,
                        //                        val);
                        switch (um8669f->cur_reg) {
                        case REG_ENABLE:
                                um8669f->dev[um8669f->cur_device].enable = val;
                                break;
                        case REG_ADDRLO:
                                um8669f->dev[um8669f->cur_device].addr = (um8669f->dev[um8669f->cur_device].addr & 0xff00) | val;
                                break;
                        case REG_ADDRHI:
                                um8669f->dev[um8669f->cur_device].addr =
                                        (um8669f->dev[um8669f->cur_device].addr & 0x00ff) | (val << 8);
                                break;
                        case REG_IRQ:
                                um8669f->dev[um8669f->cur_device].irq = val;
                                break;
                        case REG_DMA:
                                um8669f->dev[um8669f->cur_device].dma = val;
                                break;
                        }

                        switch (um8669f->cur_device) {
                        case DEV_FDC:
                                fdc_remove();
                                if (um8669f->dev[DEV_FDC].enable & 1)
                                        fdc_add();
                                break;
                        case DEV_COM1:
                                serial1_remove();
                                if (um8669f->dev[DEV_COM1].enable & 1)
                                        serial1_set(um8669f->dev[DEV_COM1].addr, um8669f->dev[DEV_COM1].irq);
                                break;
                        case DEV_COM2:
                                serial2_remove();
                                if (um8669f->dev[DEV_COM2].enable & 1)
                                        serial2_set(um8669f->dev[DEV_COM2].addr, um8669f->dev[DEV_COM2].irq);
                                break;
                        case DEV_LPT1:
                                lpt1_remove();
                                lpt2_remove();
                                if (um8669f->dev[DEV_LPT1].enable & 1)
                                        lpt1_init(um8669f->dev[DEV_LPT1].addr);
                                break;
                        }
                }
        }
}

uint8_t um8669f_pnp_read(uint16_t port, void *p) {
        um8669f_t *um8669f = (um8669f_t *)p;

        //        pclog("Read UM8669F %02x\n", um8669f->cur_reg);

        switch (um8669f->cur_reg) {
        case REG_DEVICE:
                return um8669f->cur_device;
        case REG_ENABLE:
                return um8669f->dev[um8669f->cur_device].enable;
        case REG_ADDRLO:
                return um8669f->dev[um8669f->cur_device].addr & 0xff;
        case REG_ADDRHI:
                return um8669f->dev[um8669f->cur_device].addr >> 8;
        case REG_IRQ:
                return um8669f->dev[um8669f->cur_device].irq;
        case REG_DMA:
                return um8669f->dev[um8669f->cur_device].dma;
        }

        return 0xff;
}

void um8669f_write(uint16_t port, uint8_t val, void *p) {
        um8669f_t *um8669f = (um8669f_t *)p;

        if (um8669f->locked) {
                if (port == 0x108 && val == 0xaa)
                        um8669f->locked = 0;
        } else {
                if (port == 0x108) {
                        if (val == 0x55)
                                um8669f->locked = 1;
                        else
                                um8669f->cur_reg_108 = val;
                } else {
                        //                        pclog("Write UM8669f register %02x %02x %04x:%04x %i\n", um8669f_curreg, val,
                        //                        CS,cpu_state.pc, ins);
                        um8669f->regs_108[um8669f->cur_reg_108] = val;

                        io_removehandler(0x0279, 0x0001, NULL, NULL, NULL, um8669f_pnp_write, NULL, NULL, um8669f);
                        io_removehandler(0x0a79, 0x0001, NULL, NULL, NULL, um8669f_pnp_write, NULL, NULL, um8669f);
                        io_removehandler(0x03e3, 0x0001, um8669f_pnp_read, NULL, NULL, NULL, NULL, NULL, um8669f);
                        if (um8669f->regs_108[0xc1] & 0x80) {
                                io_sethandler(0x0279, 0x0001, NULL, NULL, NULL, um8669f_pnp_write, NULL, NULL, um8669f);
                                io_sethandler(0x0a79, 0x0001, NULL, NULL, NULL, um8669f_pnp_write, NULL, NULL, um8669f);
                                io_sethandler(0x03e3, 0x0001, um8669f_pnp_read, NULL, NULL, NULL, NULL, NULL, um8669f);
                        }
                }
        }
}

uint8_t um8669f_read(uint16_t port, void *p) {
        um8669f_t *um8669f = (um8669f_t *)p;

        //        pclog("um8669f_read : port=%04x reg %02X locked=%i  %02x\n", port, um8669f_curreg, um8669f_locked,
        //        um8669f_regs[um8669f_curreg]);
        if (um8669f->locked)
                return 0xff;

        if (port == 0x108)
                return um8669f->cur_reg_108; /*???*/
        else
                return um8669f->regs_108[um8669f->cur_reg_108];
}

void um8669f_init() {
        memset(&um8669f_global, 0, sizeof(um8669f_t));

        um8669f_global.locked = 1;

        io_sethandler(0x0108, 0x0002, um8669f_read, NULL, NULL, um8669f_write, NULL, NULL, &um8669f_global);
}
