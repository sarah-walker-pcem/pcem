#include "ibm.h"

#include "fdc.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"
#include "w83977tf.h"

typedef struct winbond_t
{
        int cur_reg;
        int cur_device;
        int index;

        uint8_t unlock_key;
        uint8_t key[2];
        int enable_regs;

        struct
        {
                int enable;
                uint16_t addr;
                int irq;
                int dma;
        } dev[11];
} winbond_t;

static winbond_t winbond_global;

#define DEV_FDC         0x0
#define DEV_LPT         0x1
#define DEV_COM1        0x2
#define DEV_COM2        0x3
#define DEV_KBC         0x5
#define DEV_GPIO        0x7
#define DEV_GPIO_II     0x8
#define DEV_GPIO_III    0x9
#define DEV_ACPI        0xa

#define REG_DEVICE      0x07
#define REG_DEVICE_ID   0x20
#define REG_DEVICE_REV  0x21
#define REG_ENABLE      0x30
#define REG_ADDRHI      0x60
#define REG_ADDRLO      0x61
#define REG_IRQ         0x70
#define REG_DMA         0x74

void winbond_write_reg(winbond_t *winbond, int index, uint8_t val)
{
        if (index == REG_DEVICE)
                winbond->cur_device = val & 0xf;
        else
        {
//                pclog("Write winbond %02x [%02x] %02x\n", winbond->cur_reg, winbond->cur_device, val);
                switch (index)
                {
                        case REG_ENABLE:
                        if (winbond->cur_device <= DEV_ACPI)
                                winbond->dev[winbond->cur_device].enable = val;
                        break;
                        case REG_ADDRLO:
                        if (winbond->cur_device <= DEV_ACPI)
                                winbond->dev[winbond->cur_device].addr = (winbond->dev[winbond->cur_device].addr & 0xff00) | val;
                        break;
                        case REG_ADDRHI:
                        if (winbond->cur_device <= DEV_ACPI)
                                winbond->dev[winbond->cur_device].addr = (winbond->dev[winbond->cur_device].addr & 0x00ff) | (val << 8);
                        break;
                        case REG_IRQ:
                        if (winbond->cur_device <= DEV_ACPI)
                                winbond->dev[winbond->cur_device].irq = val;
                        break;
                        case REG_DMA:
                        if (winbond->cur_device <= DEV_ACPI)
                                winbond->dev[winbond->cur_device].dma = val;
                        break;
                }

                switch (winbond->cur_device)
                {
                        case DEV_FDC:
                        fdc_remove();
                        if (winbond->dev[DEV_FDC].enable & 1)
                                fdc_add();
                        break;
                        case DEV_COM1:
                        serial1_remove();
                        if (winbond->dev[DEV_COM1].enable & 1)
                                serial1_set(winbond->dev[DEV_COM1].addr, winbond->dev[DEV_COM1].irq);
                        break;
                        case DEV_COM2:
                        serial2_remove();
                        if (winbond->dev[DEV_COM2].enable & 1)
                                serial2_set(winbond->dev[DEV_COM2].addr, winbond->dev[DEV_COM2].irq);
                        break;
                        case DEV_LPT:
                        lpt1_remove();
                        lpt2_remove();
                        if (winbond->dev[DEV_LPT].enable & 1)
                                lpt1_init(winbond->dev[DEV_LPT].addr);
                        break;
                }
        }
}

uint8_t winbond_read_reg(winbond_t *winbond, int index)
{
//        pclog("Read winbond %02x\n", winbond->cur_reg);

        switch (index)
        {
                case REG_DEVICE:
                return winbond->cur_device;
                case REG_DEVICE_ID:
                return 0x97;
                case REG_DEVICE_REV:
                return 0x73;
                case REG_ENABLE:
                return winbond->dev[winbond->cur_device].enable;
                case REG_ADDRLO:
                return winbond->dev[winbond->cur_device].addr & 0xff;
                case REG_ADDRHI:
                return winbond->dev[winbond->cur_device].addr >> 8;
                case REG_IRQ:
                return winbond->dev[winbond->cur_device].irq;
                case REG_DMA:
                return winbond->dev[winbond->cur_device].dma;
        }

        return 0xff;
}

static void winbond_write_3fx(uint16_t port, uint8_t val, void *p)
{
        winbond_t *winbond = (winbond_t *)p;

//        pclog("winbond_write: port=%04x val=%02x\n", port, val);

        switch (port)
        {
                case 0x3f0: /*Enable/index register*/
                if (val == 0xaa)
                {
                        fdc_3f1_enable(1);
                        winbond->enable_regs = 0;
                }
                else if (!winbond->enable_regs)
                {
                        winbond->key[1] = winbond->key[0];
                        winbond->key[0] = val;
                        winbond->enable_regs = (winbond->key[0] == winbond->unlock_key && winbond->key[1] == winbond->unlock_key);
                        /*The FDC conflicts with the 3fx config registers, so disable
                          it when enabling config registers*/
                        if (winbond->enable_regs)
                                fdc_3f1_enable(0);
                }
                else
                        winbond->index = val;
                break;
                case 0x3f1: /*Data*/
                if (winbond->enable_regs)
                        winbond_write_reg(winbond, winbond->index, val);
                break;
        }
}

static uint8_t winbond_read_3fx(uint16_t port, void *p)
{
        winbond_t *winbond = (winbond_t *)p;
        uint8_t ret = 0xff;

        switch (port)
        {
                case 0x3f0: /*Index*/
                if (winbond->enable_regs)
                        ret = winbond->index;
                break;

                case 0x3f1: /*Data*/
                if (winbond->enable_regs)
                        ret = winbond_read_reg(winbond, winbond->index);
                break;
        }

//        pclog("winbond_read: port=%04x val=%02x\n", port, ret);

        return ret;
}

void w83977tf_init(uint16_t base, uint8_t key)
{
        memset(&winbond_global, 0, sizeof(winbond_t));

        winbond_global.unlock_key = key;
        io_sethandler(base, 0x0002, winbond_read_3fx, NULL, NULL, winbond_write_3fx, NULL, NULL,  &winbond_global);
}
