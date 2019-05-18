#include "ibm.h"

#include "fdc.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"
#include "w83877tf.h"

enum
{
        CHIP_W83877F,
        CHIP_W83877TF
};

typedef struct w83877tf_t
{
        int enable_regs;
        uint8_t key[2];
        uint8_t unlock_key;
        int index;
        uint8_t regs[256];
        int chip;
} w83877tf_t;

static w83877tf_t w83877tf_global;

static void w83877tf_write_25x(uint16_t port, uint8_t val, void *p);
static uint8_t w83877tf_read_25x(uint16_t port, void *p);
static void w83877tf_write_3fx(uint16_t port, uint8_t val, void *p);
static uint8_t w83877tf_read_3fx(uint16_t port, void *p);

static void w83877tf_write_reg(w83877tf_t *w83877tf, int index, uint8_t val)
{
//        pclog("w83877tf_write: index=%02x val=%02x\n", index, val);
        switch (index)
        {
                case 0x0c:
                if (w83877tf->regs[0x16] & 1)
                        w83877tf->unlock_key = (val & (1 << 5)) ? 0x87 : 0x86;
                else
                        w83877tf->unlock_key = (val & (1 << 5)) ? 0x89 : 0x88;
                break;

                case 0x16:
                if (val == 2) /*Written by VA-503+ BIOS. Apparently should not change IO mapping*/
                        return;
                        
                io_removehandler(0x0250, 0x0003, w83877tf_read_25x, NULL, NULL, w83877tf_write_25x, NULL, NULL,  &w83877tf_global);
                io_removehandler(0x03f0, 0x0002, w83877tf_read_3fx, NULL, NULL, w83877tf_write_3fx, NULL, NULL,  &w83877tf_global);
                if (val & 0x01)
                {
                        io_sethandler(0x03f0, 0x0002, w83877tf_read_3fx, NULL, NULL, w83877tf_write_3fx, NULL, NULL,  &w83877tf_global);
                        w83877tf->unlock_key = (w83877tf->regs[0xc] & (1 << 5)) ? 0x87 : 0x86;
                }
                else
                {
                        io_sethandler(0x0250, 0x0003, w83877tf_read_25x, NULL, NULL, w83877tf_write_25x, NULL, NULL,  &w83877tf_global);
                        w83877tf->unlock_key = (w83877tf->regs[0xc] & (1 << 5)) ? 0x89 : 0x88;
                }

                if ((w83877tf->regs[0x16] & 4) && !(val & 4))
                {
                        w83877tf->regs[0x20] = 0;
                        w83877tf->regs[0x23] = 0;
                        w83877tf->regs[0x24] = 0;
                        w83877tf->regs[0x25] = 0;
                        w83877tf->regs[0x26] = 0;
                        w83877tf->regs[0x27] = 0;
                        w83877tf->regs[0x28] = 0;
                        w83877tf->regs[0x29] = 0;
                }
                break;
        }

        w83877tf->regs[index] = val;
        

        fdc_remove();
        if (w83877tf->regs[0x20] >= 0x40)
                fdc_add();

        lpt1_remove();
        lpt2_remove();
        if (w83877tf->regs[0x23] >= 0x40)
                lpt1_init(w83877tf->regs[0x23] << 2);

        serial1_remove();
        if (w83877tf->regs[0x24] >= 0x40)
                serial1_set((w83877tf->regs[0x24] & ~1) << 2, w83877tf->regs[0x28] >> 4);

        serial2_remove();
        if (w83877tf->regs[0x25] >= 0x40)
                serial2_set((w83877tf->regs[0x25] & ~1) << 2, w83877tf->regs[0x28] & 0xf);

//        pclog("LPT at %04x\n", w83877tf->regs[0x23] << 2);
//        pclog("Serial1 at %04x %i\n", (w83877tf->regs[0x24] & ~1) << 2, w83877tf->regs[0x28] >> 4);
//        pclog("Serial2 at %04x %i\n", (w83877tf->regs[0x25] & ~1) << 2, w83877tf->regs[0x28] & 0xf);
}

static uint8_t w83877tf_read_reg(w83877tf_t *w83877tf, int index)
{
        switch (w83877tf->index)
        {
                case 0x09:
                switch (w83877tf->chip)
                {
                        case CHIP_W83877F:
                        return 0x0a; /*W83877F ID*/
                        case CHIP_W83877TF:
                        return 0x0c; /*W83877TF ID*/
                }
                break;
        }
        return w83877tf->regs[index];
}

static void w83877tf_write_25x(uint16_t port, uint8_t val, void *p)
{
        w83877tf_t *w83877tf = (w83877tf_t *)p;

//        pclog("w83877tf_write: port=%04x val=%02x\n", port, val);

        switch (port)
        {
                case 0x250: /*Enable register*/
                w83877tf->enable_regs = (val == w83877tf->unlock_key) ? 1 : 0;
                break;
                case 0x251: /*Index*/
                if (w83877tf->enable_regs)
                        w83877tf->index = val;
                break;
                case 0x252: /*Data*/
                if (w83877tf->enable_regs)
                        w83877tf_write_reg(w83877tf, w83877tf->index, val);
                break;
        }
}

static uint8_t w83877tf_read_25x(uint16_t port, void *p)
{
        w83877tf_t *w83877tf = (w83877tf_t *)p;

        switch (port)
        {
                case 0x251: /*Index*/
                if (w83877tf->enable_regs)
                        return w83877tf->index;
                break;

                case 0x252: /*Data*/
                if (w83877tf->enable_regs)
                        return w83877tf_read_reg(w83877tf, w83877tf->index);
                break;
        }
        
        return 0xff;
}

static void w83877tf_write_3fx(uint16_t port, uint8_t val, void *p)
{
        w83877tf_t *w83877tf = (w83877tf_t *)p;

//        pclog("w83877tf_write: port=%04x val=%02x\n", port, val);

        switch (port)
        {
                case 0x3f0: /*Enable/index register*/
                if (val == 0xaa)
                {
                        fdc_3f1_enable(1);
                        w83877tf->enable_regs = 0;
                }
                else if (!w83877tf->enable_regs)
                {
                        w83877tf->key[1] = w83877tf->key[0];
                        w83877tf->key[0] = val;
                        w83877tf->enable_regs = (w83877tf->key[0] == w83877tf->unlock_key && w83877tf->key[1] == w83877tf->unlock_key);
                        /*The FDC conflicts with the 3fx config registers, so disable
                          it when enabling config registers*/
                        if (w83877tf->enable_regs)
                                fdc_3f1_enable(0);
                }
                else
                        w83877tf->index = val;
                break;
                case 0x3f1: /*Data*/
                if (w83877tf->enable_regs)
                        w83877tf_write_reg(w83877tf, w83877tf->index, val);
                break;
        }
}

static uint8_t w83877tf_read_3fx(uint16_t port, void *p)
{
        w83877tf_t *w83877tf = (w83877tf_t *)p;
        uint8_t ret = 0xff;

        switch (port)
        {
                case 0x3f0: /*Index*/
                if (w83877tf->enable_regs)
                        ret = w83877tf->index;
                break;

                case 0x3f1: /*Data*/
                if (w83877tf->enable_regs)
                        ret = w83877tf_read_reg(w83877tf, w83877tf->index);
                break;
        }

//        pclog("w83877tf_read: port=%04x val=%02x\n", port, ret);
        
        return ret;
}

static void w83877_common_init(int chip, uint16_t base, uint8_t key)
{
        memset(&w83877tf_global, 0, sizeof(w83877tf_t));

        if (base == 0x250)
                io_sethandler(0x0250, 0x0003, w83877tf_read_25x, NULL, NULL, w83877tf_write_25x, NULL, NULL,  &w83877tf_global);
        else
                io_sethandler(0x03f0, 0x0002, w83877tf_read_3fx, NULL, NULL, w83877tf_write_3fx, NULL, NULL,  &w83877tf_global);

        /*Defaults*/
        w83877tf_global.regs[0x03] = 0x30;
        w83877tf_global.regs[0x09] = 0x0c;
        w83877tf_global.regs[0x0b] = 0x0c;
        if (key == 0x88 || key == 0x86)
                w83877tf_global.regs[0x0c] = 0x08;
        else
                w83877tf_global.regs[0x0c] = 0x28;
        w83877tf_global.regs[0x0d] = 0xa3;
        if (base == 0x250)
                w83877tf_global.regs[0x16] = 0x04;
        else
                w83877tf_global.regs[0x16] = 0x05;
        w83877tf_global.regs[0x20] = 0xfc;
        w83877tf_global.regs[0x23] = 0xde;
        w83877tf_global.regs[0x24] = 0xfe;
        w83877tf_global.regs[0x25] = 0xbe;
        w83877tf_global.regs[0x26] = 0x23;
        w83877tf_global.regs[0x27] = 0x05;
        w83877tf_global.regs[0x28] = 0x43;
        w83877tf_global.regs[0x29] = 0x60;

        w83877tf_global.chip = chip;
        w83877tf_global.unlock_key = key;
}

void w83877f_init(uint16_t base, uint8_t key)
{
        w83877_common_init(CHIP_W83877F, base, key);
}
void w83877tf_init(uint16_t base, uint8_t key)
{
        w83877_common_init(CHIP_W83877TF, base, key);
}
