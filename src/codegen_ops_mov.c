#include "ibm.h"

#include "x86.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_mov.h"

uint32_t ropMOV_rb_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint8_t imm = fastreadb(cs + op_pc);

        uop_MOV_IMM(ir, IREG_8(opcode & 7), imm);

        return op_pc + 1;
}
uint32_t ropMOV_rw_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint16_t imm = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_16(opcode & 7), imm);

        return op_pc + 2;
}
uint32_t ropMOV_rl_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_MOV_IMM(ir, IREG_32(opcode & 7), fetchdat);

        return op_pc + 4;
}



uint32_t ropMOV_b_r(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = (fetchdat >> 3) & 7;
                uop_MOV(ir, IREG_8(dest_reg), IREG_8(src_reg));
        }
        else
                return 0;

        return op_pc + 1;
}
uint32_t ropMOV_w_r(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = (fetchdat >> 3) & 7;
                uop_MOV(ir, IREG_16(dest_reg), IREG_16(src_reg));
        }
        else
                return 0;

        return op_pc + 1;
}
uint32_t ropMOV_l_r(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = (fetchdat >> 3) & 7;
                uop_MOV(ir, IREG_32(dest_reg), IREG_32(src_reg));
        }
        else
                return 0;

        return op_pc + 1;
}
uint32_t ropMOV_r_b(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = fetchdat & 7;
                uop_MOV(ir, IREG_8(dest_reg), IREG_8(src_reg));
        }
        else
                return 0;

        return op_pc + 1;
}
uint32_t ropMOV_r_w(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = fetchdat & 7;
                uop_MOV(ir, IREG_16(dest_reg), IREG_16(src_reg));
        }
        else
                return 0;

        return op_pc + 1;
}
uint32_t ropMOV_r_l(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = fetchdat & 7;
                uop_MOV(ir, IREG_32(dest_reg), IREG_32(src_reg));
        }
        else
                return 0;

        return op_pc + 1;
}
