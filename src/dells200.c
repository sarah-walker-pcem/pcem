#include "ibm.h"
#include "io.h"
#include "mem.h"

#include "dells200.h"

static struct
{
        uint8_t regs[3];
} dells200;

static uint8_t dells200_in(uint16_t addr, void *p)
{
        uint8_t ret = 0xff;
        
        switch (addr)
        {
                case 0xa4:
                ret = dells200.regs[0];
                break;
                case 0xa5:
                ret = dells200.regs[1];
                break;
                case 0xab:
                ret = dells200.regs[2];
                break;
        }
        
//        pclog("dells200_in: addr=%04x ret=%02x %04x(%08x):%08x\n", addr, ret, CS,cs,cpu_state.pc);
        
        return ret;
}

static void dells200_out(uint16_t addr, uint8_t val, void *p)
{
//        pclog("dells200_out: addr=%04x val=%02x %04x(%08x):%08x\n", addr, val, CS,cs,cpu_state.pc);

        switch (addr)
        {
                case 0xa4:
                dells200.regs[0] = val;
                mem_a20_alt = val & 0x40;
                mem_a20_recalc();
                break;
                case 0xa5:
                dells200.regs[1] = val;
                if (val & 1)
                        mem_mapping_set_addr(&ram_low_mapping, 0, 0x40000);
                else
                        mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                if (val & 1)
                        mem_mapping_disable(&ram_high_mapping);
                else
                        mem_mapping_enable(&ram_high_mapping);
                break;
                case 0xab:
                dells200.regs[2] = val;
                break;
        }
}

void dells200_chipset_init()
{
        memset(&dells200, 0, sizeof(dells200));
        
        io_sethandler(0x00a4, 0x0002, dells200_in,NULL,NULL, dells200_out,NULL,NULL, NULL);
        io_sethandler(0x00ab, 0x0001, dells200_in,NULL,NULL, dells200_out,NULL,NULL, NULL);
}
