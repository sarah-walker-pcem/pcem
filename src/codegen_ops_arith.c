#include "ibm.h"

#include "x86.h"
#include "x86_flags.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_arith.h"

uint32_t ropADD_AL_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint8_t imm_data = fastreadb(cs + op_pc);

        uop_MOVZX(ir, IREG_flags_op1, IREG_AL);
        uop_ADD_IMM(ir, IREG_AL, IREG_AL, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op2, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD8);
        uop_MOVZX(ir, IREG_flags_res, IREG_AL);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropADD_AX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint16_t imm_data = fastreadw(cs + op_pc);

        uop_MOVZX(ir, IREG_flags_op1, IREG_AX);
        uop_ADD_IMM(ir, IREG_AX, IREG_AX, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op2, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD16);
        uop_MOVZX(ir, IREG_flags_res, IREG_AX);

        codegen_flags_changed = 1;
        return op_pc + 2;
}
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
uint32_t ropADD_b_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_8(src_reg));
        uop_ADD(ir, IREG_8(dest_reg), IREG_8(dest_reg), IREG_8(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD8);
        uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropADD_b_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_8(src_reg));
        uop_ADD(ir, IREG_8(dest_reg), IREG_8(dest_reg), IREG_8(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD8);
        uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropADD_w_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_16(src_reg));
        uop_ADD(ir, IREG_16(dest_reg), IREG_16(dest_reg), IREG_16(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD16);
        uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropADD_w_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_16(src_reg));
        uop_ADD(ir, IREG_16(dest_reg), IREG_16(dest_reg), IREG_16(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD16);
        uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
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

uint32_t ropCMP_AL_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint8_t imm_data = fastreadb(cs + op_pc);

        uop_MOVZX(ir, IREG_flags_op1, IREG_AL);
        uop_SUB_IMM(ir, IREG_flags_res_B, IREG_AL, imm_data);
        uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_B);
        uop_MOV_IMM(ir, IREG_flags_op2, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropCMP_AX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint16_t imm_data = fastreadw(cs + op_pc);

        uop_MOVZX(ir, IREG_flags_op1, IREG_AX);
        uop_SUB_IMM(ir, IREG_flags_res_W, IREG_AX, imm_data);
        uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_W);
        uop_MOV_IMM(ir, IREG_flags_op2, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);

        codegen_flags_changed = 1;
        return op_pc + 2;
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
uint32_t ropCMP_b_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_8(src_reg));
        uop_SUB(ir, IREG_flags_res_B, IREG_8(dest_reg), IREG_8(src_reg));
        uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_B);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropCMP_b_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_8(src_reg));
        uop_SUB(ir, IREG_flags_res_B, IREG_8(dest_reg), IREG_8(src_reg));
        uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_B);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropCMP_w_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_16(src_reg));
        uop_SUB(ir, IREG_flags_res_W, IREG_16(dest_reg), IREG_16(src_reg));
        uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_W);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropCMP_w_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_16(src_reg));
        uop_SUB(ir, IREG_flags_res_W, IREG_16(dest_reg), IREG_16(src_reg));
        uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_W);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);

        codegen_flags_changed = 1;
        return op_pc + 1;
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

uint32_t ropSUB_AL_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint8_t imm_data = fastreadb(cs + op_pc);

        uop_MOVZX(ir, IREG_flags_op1, IREG_AL);
        uop_SUB_IMM(ir, IREG_AL, IREG_AL, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op2, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);
        uop_MOVZX(ir, IREG_flags_res, IREG_AL);

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropSUB_AX_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint16_t imm_data = fastreadw(cs + op_pc);

        uop_MOVZX(ir, IREG_flags_op1, IREG_AX);
        uop_SUB_IMM(ir, IREG_AX, IREG_AX, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op2, imm_data);
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
        uop_MOVZX(ir, IREG_flags_res, IREG_AX);

        codegen_flags_changed = 1;
        return op_pc + 2;
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
uint32_t ropSUB_b_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_8(src_reg));
        uop_SUB(ir, IREG_8(dest_reg), IREG_8(dest_reg), IREG_8(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);
        uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropSUB_b_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_8(src_reg));
        uop_SUB(ir, IREG_8(dest_reg), IREG_8(dest_reg), IREG_8(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);
        uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropSUB_w_rm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7, dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_16(src_reg));
        uop_SUB(ir, IREG_16(dest_reg), IREG_16(dest_reg), IREG_16(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
        uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
}
uint32_t ropSUB_w_rmw(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7, dest_reg = fetchdat & 7;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
        uop_MOVZX(ir, IREG_flags_op2, IREG_16(src_reg));
        uop_SUB(ir, IREG_16(dest_reg), IREG_16(dest_reg), IREG_16(src_reg));
        uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
        uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));

        codegen_flags_changed = 1;
        return op_pc + 1;
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

uint32_t rop80(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;
        uint8_t imm;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                imm = fastreadb(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
                uop_ADD_IMM(ir, IREG_8(dest_reg), IREG_8(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD8);
                uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));
                break;

                case 0x08: /*OR*/
                imm = fastreadb(cs + op_pc + 1);

                uop_OR_IMM(ir, IREG_8(dest_reg), IREG_8(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN8);
                uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));
                break;

                case 0x20: /*AND*/
                imm = fastreadb(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_8(dest_reg), IREG_8(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN8);
                uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));
                break;

                case 0x28: /*SUB*/
                imm = fastreadb(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
                uop_SUB_IMM(ir, IREG_8(dest_reg), IREG_8(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);
                uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));
                break;

                case 0x30: /*XOR*/
                imm = fastreadb(cs + op_pc + 1);

                uop_XOR_IMM(ir, IREG_8(dest_reg), IREG_8(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN8);
                uop_MOVZX(ir, IREG_flags_res, IREG_8(dest_reg));
                break;

                case 0x38: /*CMP*/
                imm = fastreadb(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_8(dest_reg));
                uop_SUB_IMM(ir, IREG_flags_res_B, IREG_8(dest_reg), imm);
                uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_B);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);
                break;

                default:
                return 0;
        }

        codegen_flags_changed = 1;
        return op_pc + 2;
}
uint32_t rop81_w(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;
        uint16_t imm;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                imm = fastreadw(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
                uop_ADD_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x08: /*OR*/
                imm = fastreadw(cs + op_pc + 1);

                uop_OR_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x20: /*AND*/
                imm = fastreadw(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x28: /*SUB*/
                imm = fastreadw(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
                uop_SUB_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x30: /*XOR*/
                imm = fastreadw(cs + op_pc + 1);

                uop_XOR_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x38: /*CMP*/
                imm = fastreadw(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
                uop_SUB_IMM(ir, IREG_flags_res_W, IREG_16(dest_reg), imm);
                uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_W);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
                break;

                default:
                return 0;
        }

        codegen_flags_changed = 1;
        return op_pc + 3;
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

uint32_t rop83_w(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;
        uint16_t imm;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                imm = (int16_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
                uop_ADD_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ADD16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x08: /*OR*/
                imm = (int16_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_OR_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x20: /*AND*/
                imm = (int16_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x28: /*SUB*/
                imm = (int16_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
                uop_SUB_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x30: /*XOR*/
                imm = (int16_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_XOR_IMM(ir, IREG_16(dest_reg), IREG_16(dest_reg), imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);
                uop_MOVZX(ir, IREG_flags_res, IREG_16(dest_reg));
                break;

                case 0x38: /*CMP*/
                imm = (int16_t)(int8_t)fastreadb(cs + op_pc + 1);

                uop_MOVZX(ir, IREG_flags_op1, IREG_16(dest_reg));
                uop_SUB_IMM(ir, IREG_flags_res_W, IREG_16(dest_reg), imm);
                uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_W);
                uop_MOV_IMM(ir, IREG_flags_op2, imm);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
                break;

                default:
                return 0;
        }

        codegen_flags_changed = 1;
        return op_pc + 2;
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
