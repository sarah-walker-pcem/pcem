#include "ibm.h"
#include "fdc.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"

#include "pc87306.h"

typedef struct pc87306_t
{
        int first_read;
        int cur_addr;
        
        uint8_t regs[32];
        
        uint8_t gpio_1, gpio_2;
} pc87306_t;

static pc87306_t pc87306_global;

#define REG_FER  0x00
#define REG_FAR  0x01
#define REG_GPBA 0x0f
#define REG_SCF0 0x12
#define REG_PNP0 0x1b

#define FER_LPT_ENA  (1 << 0)
#define FER_COM1_ENA (1 << 1)
#define FER_COM2_ENA (1 << 2)
#define FER_FDC_ENA  (1 << 3)

static uint16_t get_com_addr(pc87306_t *pc87306, int reg)
{
        switch (reg)
        {
                case 0:
                return 0x3f8;
                case 1:
                return 0x2f8;
                case 2:
                switch (pc87306->regs[REG_FAR] >> 6)
                {
                        case 0:
                        return 0x3e8;
                        case 1:
                        return 0x338;
                        case 2:
                        return 0x2e8;
                        case 3:
                        return 0x220;
                }
                break;
                case 3:
                switch (pc87306->regs[REG_FAR] >> 6)
                {
                        case 0:
                        return 0x2e8;
                        case 1:
                        return 0x238;
                        case 2:
                        return 0x2e0;
                        case 3:
                        return 0x228;
                }
                break;
        }
        fatal("get_com_addr invalid\n");
        return 0xff;
}

static uint8_t pc87306_gpio_read(uint16_t port, void *p)
{
        pc87306_t *pc87306 = (pc87306_t *)p;
        uint8_t temp;

        if (!(port & 1))
                temp = pc87306->gpio_1;
        else
                temp = pc87306->gpio_2;

//        pclog("Read GPIO %04x %02x\n", port, temp);
        return temp;
}

static void pc87306_gpio_write(uint16_t port, uint8_t val, void *p)
{
        pc87306_t *pc87306 = (pc87306_t *)p;

//        pclog("Write GPIO %04x %02x\n", port, val);
        if (!(port & 1))
                pc87306->gpio_1 = val;
}

static uint8_t pc87306_read(uint16_t port, void *p)
{
        pc87306_t *pc87306 = (pc87306_t *)p;
        
        if (!(port & 1))
        {
                if (pc87306->first_read)
                {
                        pc87306->first_read = 0;
                        return 0x88; /*ID*/
                }
                return pc87306->cur_addr;
        }
        else
        {
                pc87306->regs[8] = 0x70;
//                pclog("PC87306 read %02x %02x %04x:%04x\n", pc87306->cur_addr, pc87306->regs[pc87306->cur_addr], CS,cpu_state.pc);
//                if (pc87306->cur_addr == 0)
//                        output = 3;
                return pc87306->regs[pc87306->cur_addr];
        }
}

static void pc87306_write(uint16_t port, uint8_t val, void *p)
{
        pc87306_t *pc87306 = (pc87306_t *)p;
//pclog("pc87306_write: port=%04x val=%02x %04x:%04x\n", port, val, CS,cpu_state.pc);
        if (!(port & 1))
                pc87306->cur_addr = val & 0x1f;
        else
        {
                uint16_t old_gpio_addr = pc87306_global.regs[REG_GPBA] << 2;

                pc87306->regs[pc87306->cur_addr] = val;

//                pclog("PC87306 write %02x %02x\n", pc87306->cur_addr, val);

                fdc_remove();
                if (pc87306->regs[REG_FER] & FER_FDC_ENA)
                        fdc_add();
                serial1_remove();
                if (pc87306->regs[REG_FER] & FER_COM1_ENA)
                {
                        uint16_t addr = get_com_addr(pc87306, (pc87306->regs[REG_FAR] >> 2) & 3);

                        switch ((pc87306->regs[REG_FAR] >> 2) & 3)
                        {
                                case 0: case 2:
                                serial1_set(addr, 4);
                                break;
                                case 1: case 3:
                                serial1_set(addr, 3);
                                break;
                        }
                }
                serial2_remove();
                if (pc87306->regs[REG_FER] & FER_COM2_ENA)
                {
                        uint16_t addr = get_com_addr(pc87306, (pc87306->regs[REG_FAR] >> 4) & 3);

                        switch ((pc87306->regs[REG_FAR] >> 4) & 3)
                        {
                                case 0: case 2:
                                serial2_set(addr, 4);
                                break;
                                case 1: case 3:
                                serial2_set(addr, 3);
                                break;
                        }
                }
                lpt1_remove();
                lpt2_remove();
                if (pc87306->regs[REG_FER] & FER_LPT_ENA)
                {
                        int reg;
                        
                        reg = pc87306->regs[REG_FAR] & 3;
                        reg |= (pc87306->regs[REG_PNP0] & 0x30) >> 2;
                        
                        switch (reg)
                        {
                                case 0x0: case 0x8:
                                case 0x4: case 0x5: case 0x6: case 0x7:
                                lpt1_init(0x378);
                                break;
                                case 0x1: case 0x9:
                                lpt1_init(0x3bc);
                                break;
                                case 0x2: case 0xa:
                                case 0xc: case 0xd: case 0xe: case 0xf:
                                lpt1_init(0x278);
                                break;
                        }
                }
                io_removehandler(old_gpio_addr, 0x0002, pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, &pc87306_global);
                io_sethandler(pc87306_global.regs[REG_GPBA] << 2, 0x0002, pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, &pc87306_global);
        }
}

void pc87306_init(uint16_t port)
{
        memset(&pc87306_global, 0, sizeof(pc87306_t));
        
        pc87306_global.first_read = 1;        
        io_sethandler(port, 0x0002, pc87306_read, NULL, NULL, pc87306_write, NULL, NULL, &pc87306_global);
        pc87306_global.regs[REG_FER] = 0;
        pc87306_global.regs[REG_FAR] = 0x10;
        pc87306_global.regs[REG_GPBA] = 0x78 >> 2;
        pc87306_global.regs[REG_SCF0] = 0x30;
        pc87306_global.gpio_2 = 0xff; // this is a mask only, output will be defined by zappa_brdconfig and endeavor_brdconfig in intel.c
        io_sethandler(0x0078, 0x0002, pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, &pc87306_global);
}
