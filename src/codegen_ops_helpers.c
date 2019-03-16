#include "ibm.h"
#include "x86.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir_defs.h"
#include "codegen_reg.h"
#include "codegen_ops_helpers.h"

void LOAD_IMMEDIATE_FROM_RAM_16_unaligned(codeblock_t *block, ir_data_t *ir, int dest_reg, uint32_t addr)
{
        /*Word access that crosses two pages. Perform reads from both pages, shift and combine*/
        uop_MOVZX_REG_PTR_8(ir, IREG_temp3_W, get_ram_ptr(addr));
        uop_MOVZX_REG_PTR_8(ir, dest_reg, get_ram_ptr(addr+1));
        uop_SHL_IMM(ir, IREG_temp3_W, IREG_temp3_W, 8);
        uop_OR(ir, dest_reg, dest_reg, IREG_temp3_W);
}

void LOAD_IMMEDIATE_FROM_RAM_32_unaligned(codeblock_t *block, ir_data_t *ir, int dest_reg, uint32_t addr)
{
        /*Dword access that crosses two pages. Perform reads from both pages, shift and combine*/
        uop_MOV_REG_PTR(ir, dest_reg, get_ram_ptr(addr & ~3));
        uop_MOV_REG_PTR(ir, IREG_temp3, get_ram_ptr((addr + 4) & ~3));
        uop_SHR_IMM(ir, dest_reg, dest_reg, (addr & 3) * 8);
        uop_SHL_IMM(ir, IREG_temp3, IREG_temp3, (4 - (addr & 3)) * 8);
        uop_OR(ir, dest_reg, dest_reg, IREG_temp3);
}
