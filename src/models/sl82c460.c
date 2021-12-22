#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "sl82c460.h"

static struct
{
        int reg_idx;
        uint8_t regs[256];
} sl82c460;

static void sl82c460_shadow_set(uint32_t base, uint32_t size, int state)
{
        switch (state)
        {
                case 0:
                mem_set_mem_state(base, size, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 1:
                mem_set_mem_state(base, size, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                break;
                case 2:
                mem_set_mem_state(base, size, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 3:
                mem_set_mem_state(base, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                break;
        }
        
}

static void sl82c460_shadow_recalc()
{
        sl82c460_shadow_set(0xc0000, 0x4000, sl82c460.regs[0x18] & 3);
        sl82c460_shadow_set(0xc4000, 0x4000, (sl82c460.regs[0x18] >> 2) & 3);
        sl82c460_shadow_set(0xc8000, 0x4000, (sl82c460.regs[0x18] >> 4) & 3);
        sl82c460_shadow_set(0xcc000, 0x4000, (sl82c460.regs[0x18] >> 6) & 3);
        sl82c460_shadow_set(0xd0000, 0x4000, sl82c460.regs[0x19] & 3);
        sl82c460_shadow_set(0xd4000, 0x4000, (sl82c460.regs[0x19] >> 2) & 3);
        sl82c460_shadow_set(0xd8000, 0x4000, (sl82c460.regs[0x19] >> 4) & 3);
        sl82c460_shadow_set(0xdc000, 0x4000, (sl82c460.regs[0x19] >> 6) & 3);
        sl82c460_shadow_set(0xe0000, 0x10000, (sl82c460.regs[0x1b] >> 4) & 3);
        sl82c460_shadow_set(0xf0000, 0x10000, (sl82c460.regs[0x1b] >> 2) & 3);
}

static void sl82c460_write(uint16_t addr, uint8_t val, void *p)
{
        if (!(addr & 1))
                sl82c460.reg_idx = val;
        else
        {
//                pclog("SL82C460 write %02x %02x\n", sl82c460.reg_idx, val);
                sl82c460.regs[sl82c460.reg_idx] = val;
                
                switch (sl82c460.reg_idx)
                {
                        case 0x18: case 0x19: case 0x1b:
                        sl82c460_shadow_recalc();
                        break;
                }
        }
}

static uint8_t sl82c460_read(uint16_t addr, void *p)
{
        if (!(addr & 1))
                return sl82c460.reg_idx;

        return sl82c460.regs[sl82c460.reg_idx];
}

void sl82c460_init()
{
        memset(&sl82c460, 0, sizeof(sl82c460));
        io_sethandler(0x00a8, 0x0002, sl82c460_read, NULL, NULL, sl82c460_write, NULL, NULL, NULL);
}
