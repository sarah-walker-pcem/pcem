#include "ibm.h"

#include "fdc.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"
#include "pc87307.h"

typedef struct pc87307_t
{
        int cur_reg;
        int cur_device;
        
        uint8_t superio_cfg1, superio_cfg2;
        
        struct
        {
                int enable;
                uint16_t addr;
                int irq;
                int dma;
        } dev[9];
} pc87307_t;

static pc87307_t pc87307_global;

#define DEV_KBC   0
#define DEV_MOUSE 1
#define DEV_RTC   2
#define DEV_FDC   3
#define DEV_LPT   4
#define DEV_COM2  5
#define DEV_COM1  6
#define DEV_GPIO  7
#define DEV_POWER 8

#define REG_DEVICE 0x07
#define REG_SID    0x20
#define REG_SUPERIO_CFG1 0x21
#define REG_SUPERIO_CFG2 0x22
#define REG_ENABLE 0x30
#define REG_ADDRHI 0x60
#define REG_ADDRLO 0x61
#define REG_IRQ    0x70
#define REG_DMA    0x74

void pc87307_write(uint16_t port, uint8_t val, void *p)
{
        pc87307_t *pc87307 = (pc87307_t *)p;

        if (port == 0x2e)
                pc87307->cur_reg = val;
        else
        {
                if (pc87307->cur_reg == REG_DEVICE)
                        pc87307->cur_device = val & 0xf;
                else
                {
//                        pclog("Write pc87307 %02x [%02x] %02x\n", pc87307->cur_reg, pc87307->cur_device, val);
                        switch (pc87307->cur_reg)
                        {
                                case REG_ENABLE:
                                if (pc87307->cur_device <= DEV_POWER)
                                        pc87307->dev[pc87307->cur_device].enable = val;
                                break;
                                case REG_ADDRLO:
                                if (pc87307->cur_device <= DEV_POWER)
                                        pc87307->dev[pc87307->cur_device].addr = (pc87307->dev[pc87307->cur_device].addr & 0xff00) | val;
                                break;
                                case REG_ADDRHI:
                                if (pc87307->cur_device <= DEV_POWER)
                                        pc87307->dev[pc87307->cur_device].addr = (pc87307->dev[pc87307->cur_device].addr & 0x00ff) | (val << 8);
                                break;
                                case REG_IRQ:
                                if (pc87307->cur_device <= DEV_POWER)
                                        pc87307->dev[pc87307->cur_device].irq = val;
                                break;
                                case REG_DMA:
                                if (pc87307->cur_device <= DEV_POWER)
                                        pc87307->dev[pc87307->cur_device].dma = val;
                                break;
                        }

                        switch (pc87307->cur_device)
                        {
                                case DEV_FDC:
                                fdc_remove();
                                if (pc87307->dev[DEV_FDC].enable & 1)
                                        fdc_add();
                                break;
                                case DEV_COM1:
                                serial1_remove();
                                if (pc87307->dev[DEV_COM1].enable & 1)
                                        serial1_set(pc87307->dev[DEV_COM1].addr, pc87307->dev[DEV_COM1].irq);
                                break;
                                case DEV_COM2:
                                serial2_remove();
                                if (pc87307->dev[DEV_COM2].enable & 1)
                                        serial2_set(pc87307->dev[DEV_COM2].addr, pc87307->dev[DEV_COM2].irq);
                                break;
                                case DEV_LPT:
                                lpt1_remove();
                                lpt2_remove();
                                if (pc87307->dev[DEV_LPT].enable & 1)
                                        lpt1_init(pc87307->dev[DEV_LPT].addr);
                                break;
                        }
                }
        }
}

uint8_t pc87307_read(uint16_t port, void *p)
{
        pc87307_t *pc87307 = (pc87307_t *)p;

//        pclog("Read pc87307 %02x\n", pc87307->cur_reg);

        switch (pc87307->cur_reg)
        {
                case REG_DEVICE:
                return pc87307->cur_device;
                case REG_SID:
                return 0x20; /*PC87307*/
                case REG_SUPERIO_CFG1:
                return pc87307->superio_cfg1;
                case REG_SUPERIO_CFG2:
                return pc87307->superio_cfg2;
                case REG_ENABLE:
                return pc87307->dev[pc87307->cur_device].enable;
                case REG_ADDRLO:
                return pc87307->dev[pc87307->cur_device].addr & 0xff;
                case REG_ADDRHI:
                return pc87307->dev[pc87307->cur_device].addr >> 8;
                case REG_IRQ:
                return pc87307->dev[pc87307->cur_device].irq;
                case REG_DMA:
                return pc87307->dev[pc87307->cur_device].dma;
        }

        return 0xff;
}

void pc87307_init(uint16_t base)
{
        memset(&pc87307_global, 0, sizeof(pc87307_t));
        
        pc87307_global.superio_cfg1 = 0x04;
        pc87307_global.superio_cfg2 = 0x03;

        io_sethandler(base, 0x0002, pc87307_read, NULL, NULL, pc87307_write, NULL, NULL,  &pc87307_global);
}
