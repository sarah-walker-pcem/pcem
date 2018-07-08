#include "ibm.h"

#include "x86.h"
#include "x86_flags.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_logic.h"

uint32_t ropAND_EAX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_AND_IMM(ir, IREG_EAX, IREG_EAX, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_EAX);

        codegen_flags_changed = 1;
        return op_pc + 4;
}
uint32_t ropAND_l_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_AND(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropAND_l_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_AND(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}

uint32_t ropOR_EAX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_OR_IMM(ir, IREG_EAX, IREG_EAX, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_EAX);

        codegen_flags_changed = 1;
        return op_pc + 4;
}
uint32_t ropOR_l_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_OR(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropOR_l_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_OR(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}

uint32_t ropXOR_EAX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_XOR_IMM(ir, IREG_EAX, IREG_EAX, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_EAX);

        codegen_flags_changed = 1;
        return op_pc + 4;
}
uint32_t ropXOR_l_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_XOR(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropXOR_l_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_XOR(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
