#include "ibm.h"
#include "acc2036.h"
#include "io.h"
#include "mem.h"

static struct
{
        int reg_idx;
        uint8_t regs[256];
} acc2036;

static void acc2036_shadow_recalc()
{
        if (acc2036.regs[0x2c] & 8)
        {
                switch (acc2036.regs[0x22] & 3)
                {
                        case 0:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                        case 1:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                        case 2:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                        break;
                        case 3:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        break;
                }
        }
        else
                mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

        if (acc2036.regs[0x2c] & 4)
        {
                switch (acc2036.regs[0x22] & 3)
                {
                        case 0:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                        case 1:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                        case 2:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                        break;
                        case 3:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        break;
                }
        }
        else
                mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
}

static void acc2036_write(uint16_t addr, uint8_t val, void *p)
{
        if (!(addr & 1))
                acc2036.reg_idx = val;
        else
        {
                acc2036.regs[acc2036.reg_idx] = val;
//                pclog("Write ACC2036 R%02x %02x %04x:%04x %i\n", acc2036.reg_idx, val, CS,cpu_state.pc, ins);
                
                switch (acc2036.reg_idx)
                {
                        case 0x22:
                        case 0x2c:
                        acc2036_shadow_recalc();
                        break;
                        
                        case 0x23:
                        mem_mapping_disable(&ram_remapped_mapping);
                        if (val & 0x10)
                        {
                                if (acc2036.regs[0x2c] & 0xc)
                                        mem_remap_top_256k();
                                else
                                        mem_remap_top_384k();
                        }
                        break;
                }
        }
}

static uint8_t acc2036_read(uint16_t addr, void *p)
{
        if (!(addr & 1))
                return acc2036.reg_idx;

        if (acc2036.reg_idx >= 0xbc)
                return 0xff;
                
        return acc2036.regs[acc2036.reg_idx];
}

void acc2036_init()
{
        memset(&acc2036, 0, sizeof(acc2036));
        io_sethandler(0x00f2, 0x0002, acc2036_read, NULL, NULL, acc2036_write, NULL, NULL,  NULL);
}
