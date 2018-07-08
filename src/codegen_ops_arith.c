#include "ibm.h"

#include "x86.h"
#include "x86_flags.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_arith.h"

uint32_t ropADD_EAX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_MOV(ir, IREG_flags_op1, IREG_EAX);
        uop_ADD_IMM(ir, IREG_EAX, IREG_EAX, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op2, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD32);
        uop_MOV(ir, IREG_flags_res, IREG_EAX);

        codegen_flags_changed = 1;
        return op_pc + 4;
}
uint32_t ropADD_l_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
        uop_MOV(ir, IREG_flags_op2, IREG_32(src_reg));
        uop_ADD(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropADD_l_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
        uop_MOV(ir, IREG_flags_op2, IREG_32(src_reg));
        uop_ADD(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}

uint32_t ropCMP_EAX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_MOV(ir, IREG_flags_op1, IREG_EAX);
        uop_SUB_IMM(ir, IREG_flags_res, IREG_EAX, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op2, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);

        codegen_flags_changed = 1;
        return op_pc + 4;
}
uint32_t ropCMP_l_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
        uop_MOV(ir, IREG_flags_op2, IREG_32(src_reg));
        uop_SUB(ir, IREG_flags_res, IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropCMP_l_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
        uop_MOV(ir, IREG_flags_op2, IREG_32(src_reg));
        uop_SUB(ir, IREG_flags_res, IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);

        codegen_flags_changed = 1;
        return op_pc + 1;
}

uint32_t ropSUB_EAX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_MOV(ir, IREG_flags_op1, IREG_EAX);
        uop_SUB_IMM(ir, IREG_EAX, IREG_EAX, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op2, fetchdat);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
        uop_MOV(ir, IREG_flags_res, IREG_EAX);

        codegen_flags_changed = 1;
        return op_pc + 4;
}
uint32_t ropSUB_l_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
        uop_MOV(ir, IREG_flags_op2, IREG_32(src_reg));
        uop_SUB(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropSUB_l_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
        uop_MOV(ir, IREG_flags_op2, IREG_32(src_reg));
        uop_SUB(ir, IREG_32(dest_reg), IREG_32(dest_reg), IREG_32(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
        uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}

uint32_t rop81_l(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;
        uint32_t imm;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                imm = fastreadl(cs + op_pc + 1);

                uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
                uop_ADD_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x08: /*OR*/
                imm = fastreadl(cs + op_pc + 1);

                uop_OR_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x20: /*AND*/
                imm = fastreadl(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x28: /*SUB*/
                imm = fastreadl(cs + op_pc + 1);

                uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
                uop_SUB_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x30: /*XOR*/
                imm = fastreadl(cs + op_pc + 1);

                uop_XOR_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x38: /*CMP*/
                imm = fastreadl(cs + op_pc + 1);

                uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
                uop_SUB_IMM(ir, IREG_flags_res, IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
                break;

                default:
                return 0;
        }

        codegen_flags_changed = 1;
        return op_pc + 5;
}
uint32_t rop83_l(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;
        uint32_t imm;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                imm = (int32_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
                uop_ADD_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x08: /*OR*/
                imm = (int32_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_OR_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x20: /*AND*/
                imm = (int32_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x28: /*SUB*/
                imm = (int32_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
                uop_SUB_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;

                case 0x30: /*XOR*/
                imm = (int32_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_XOR_IMM(ir, IREG_32(dest_reg), IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);
                uop_MOV(ir, IREG_flags_res, IREG_32(dest_reg));
                break;
                
                case 0x38: /*CMP*/
                imm = (int32_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_MOV(ir, IREG_flags_op1, IREG_32(dest_reg));
                uop_SUB_IMM(ir, IREG_flags_res, IREG_32(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
                break;

                default:
                return 0;
        }

        codegen_flags_changed = 1;
        return op_pc + 2;
}
