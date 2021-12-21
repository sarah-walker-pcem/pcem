#include "ibm.h"
#include "codegen.h"
#include "codegen_accumulate.h"
#include "codegen_ir.h"

static struct
{
        int count;
        int dest_reg;
} acc_regs[] =
{
        [ACCREG_ins]    = {0, IREG_ins},
        [ACCREG_cycles] = {0, IREG_cycles},
};

void codegen_accumulate(int acc_reg, int delta)
{
        if (acc_reg == ACCREG_cycles)
        {
//                pclog("cycles %04x:%04x %i\n", CS, cpu_state.pc, delta);
//                if (CS == 0x1000 && cpu_state.pc == 0x863)
//                        fatal("Here\n");
        }
        acc_regs[acc_reg].count += delta;
}

void codegen_accumulate_flush(ir_data_t *ir)
{
        int c;
        
        for (c = 0; c < ACCREG_COUNT; c++)
        {
                if (acc_regs[c].count)
                {
//                        if (c == ACCREG_cycles)
//                                pclog("accumulate %i  %i %i\n", c, acc_regs[c].dest_reg, acc_regs[c].count);
                        uop_ADD_IMM(ir, acc_regs[c].dest_reg, acc_regs[c].dest_reg, acc_regs[c].count);
                }

                acc_regs[c].count = 0;
        }
}

void codegen_accumulate_reset()
{
        int c;

        for (c = 0; c < ACCREG_COUNT; c++)
                acc_regs[c].count = 0;
}
