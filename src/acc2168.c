#include "ibm.h"
#include "acc2168.h"
#include "cpu.h"
#include "io.h"
#include "mem.h"
#include "x86.h"

static struct
{
        int reg_idx;
        uint8_t regs[256];
        uint8_t port_62;
} acc2168;

static void acc2168_shadow_recalc()
{
        if (acc2168.regs[0x02] & 8)
        {
                switch (acc2168.regs[0x02] & 0x30)
                {
                        case 0x00:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                        break;
                        case 0x10:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        break;
                        case 0x20:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                        case 0x30:
                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                }
        }
        else
                mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

        if (acc2168.regs[0x02] & 4)
        {
                switch (acc2168.regs[0x02] & 0x30)
                {
                        case 0x00:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                        break;
                        case 0x10:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        break;
                        case 0x20:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                        case 0x30:
                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                        break;
                }
        }
        else
                mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
}

static void acc2168_write(uint16_t addr, uint8_t val, void *p)
{
//        pclog("acc2168_write: addr=%04x val=%02x\n", addr, val);
        if (!(addr & 1))
                acc2168.reg_idx = val;
        else
        {
                acc2168.regs[acc2168.reg_idx] = val;
//                pclog("acc2168_write: idx=%02x val=%02x %04x:%04x\n", acc2168.reg_idx, val, CS,cpu_state.pc);

                switch (acc2168.reg_idx)
                {
                        case 0x02:
                        acc2168_shadow_recalc();
                        break;
                }
        }
}

static uint8_t acc2168_read(uint16_t addr, void *p)
{
        if (!(addr & 1))
                return acc2168.reg_idx;

//        pclog("acc2168_read: idx=%02x val=%02x\n", acc2168.reg_idx, acc2168.regs[acc2168.reg_idx]);
        return acc2168.regs[acc2168.reg_idx];
}

static uint8_t port78_read(uint16_t addr, void *p)
{
        return 0;
}

static uint8_t port92_read(uint16_t addr, void *p)
{
        return acc2168.port_62 | 0xfc;
}

static void port92_write(uint16_t addr, uint8_t val, void *p)
{
        if ((mem_a20_alt ^ val) & 2)
        {
                mem_a20_alt = val & 2;
                mem_a20_recalc();
        }
        if ((~acc2168.port_62 & val) & 1)
        {
                softresetx86();
                cpu_set_edx();
        }
        acc2168.port_62 = val | 0xfC;
}

void acc2168_init()
{
        memset(&acc2168, 0, sizeof(acc2168));
        io_sethandler(0x00f2, 0x0002, acc2168_read, NULL, NULL, acc2168_write, NULL, NULL,  NULL);
        io_sethandler(0x0078, 0x0001, port78_read, NULL, NULL, NULL, NULL, NULL,  NULL);
        io_sethandler(0x0092, 0x0001, port92_read, NULL, NULL, port92_write, NULL, NULL,  NULL);
}
