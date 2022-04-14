#include "ibm.h"
#include "acc3221.h"
#include "fdc.h"
#include "ide.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"

static struct
{
        int reg_idx;
        uint8_t regs[256];
} acc3221;

#define REG_BE_LPT1_DISABLE (1 << 4)

#define REG_DB_SERIAL1_DISABLE (1 << 4)
#define REG_DB_SERIAL2_DISABLE (1 << 1)

#define REG_DE_SIRQ3_SOURCE  (3 << 6)
#define REG_DE_SIRQ3_SERIAL1 (1 << 6)
#define REG_DE_SIRQ3_SERIAL2 (3 << 6)

#define REG_DE_SIRQ4_SOURCE  (3 << 4)
#define REG_DE_SIRQ4_SERIAL1 (1 << 4)
#define REG_DE_SIRQ4_SERIAL2 (3 << 4)

#define REG_FB_FDC_DISABLE (1 << 1)

#define REG_FE_IDE_DISABLE (1 << 1)

static void acc3221_write(uint16_t addr, uint8_t val, void *p)
{
        if (!(addr & 1))
                acc3221.reg_idx = val;
        else
        {
                acc3221.regs[acc3221.reg_idx] = val;
//                pclog("Write ACC2036 R%02x %02x %04x:%04x %i\n", acc2036.reg_idx, val, CS,cpu_state.pc, ins);

                if (acc3221.reg_idx >= 0xbc)
                {
//                        pclog("Write ACC3221 R%02x %02x %04x:%04x %i\n", acc3221.reg_idx, val, CS,cpu_state.pc, ins);
                        fdc_remove();
                        if (!(acc3221.regs[0xfb] & REG_FB_FDC_DISABLE))
                                fdc_add();

                        serial1_remove();
                        if (!(acc3221.regs[0xdb] & REG_DB_SERIAL1_DISABLE))
                        {
                                uint16_t addr = (acc3221.regs[0xdc] & 0x7f) << 2;

                                if ((acc3221.regs[0xde] & REG_DE_SIRQ3_SOURCE) == REG_DE_SIRQ3_SERIAL1)
                                {
//                                        pclog("COM1 addr %03x IRQ 3\n", addr);
                                        serial1_set(addr, 3);
                                }
                                else if ((acc3221.regs[0xde] & REG_DE_SIRQ4_SOURCE) == REG_DE_SIRQ4_SERIAL1)
                                {
//                                        pclog("COM1 addr %03x IRQ 4\n", addr);
                                        serial1_set(addr, 4);
                                }
//                                else
//                                        pclog("COM1 addr %03x no IRQ\n", addr);
                        }
//                        else
//                                pclog("COM1 disabled\n");

                        serial2_remove();
                        if (!(acc3221.regs[0xdb] & REG_DB_SERIAL2_DISABLE))
                        {
                                uint16_t addr = (acc3221.regs[0xdd] & 0x7f) << 2;

                                if ((acc3221.regs[0xde] & REG_DE_SIRQ3_SOURCE) == REG_DE_SIRQ3_SERIAL2)
                                {
//                                        pclog("COM2 addr %03x IRQ 3\n", addr);
                                        serial2_set(addr, 3);
                                }
                                else if ((acc3221.regs[0xde] & REG_DE_SIRQ4_SOURCE) == REG_DE_SIRQ4_SERIAL2)
                                {
//                                        pclog("COM2 addr %03x IRQ 4\n", addr);
                                        serial2_set(addr, 4);
                                }
//                                else
//                                        pclog("COM2 addr %03x no IRQ\n", addr);
                        }
//                        else
//                                pclog("COM2 disabled\n");

                        lpt1_remove();
                        lpt2_remove();
                        if (!(acc3221.regs[0xbe] & REG_BE_LPT1_DISABLE))
                        {
//                                pclog("LPT1 addr %03x\n", acc3221.regs[0xbf] << 2);
                                lpt1_init(acc3221.regs[0xbf] << 2);
                        }
//                        else
//                                pclog("LPT1 disabled\n");

                        ide_pri_disable();
                        if (!(acc3221.regs[0xfe] & REG_FE_IDE_DISABLE))
                                ide_pri_enable();
                }
        }
}

static uint8_t acc3221_read(uint16_t addr, void *p)
{
        if (!(addr & 1))
                return acc3221.reg_idx;

        if (acc3221.reg_idx < 0xbc)
                return 0xff;
                
        return acc3221.regs[acc3221.reg_idx];
}

void acc3221_init()
{
        memset(&acc3221, 0, sizeof(acc3221));
        io_sethandler(0x00f2, 0x0002, acc3221_read, NULL, NULL, acc3221_write, NULL, NULL,  NULL);
}
