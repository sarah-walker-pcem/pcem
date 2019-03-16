#include "ibm.h"

#include "x86.h"
#include "386_common.h"
#include "x86_flags.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_helpers.h"
#include "codegen_ops_mov.h"

static int NF_SET_01()
{
        return NF_SET() ? 1 : 0;
}
static int VF_SET_01()
{
        return VF_SET() ? 1 : 0;
}

static void ropJO_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8: case FLAGS_ZN16: case FLAGS_ZN32:
                /*Overflow is always zero*/
                return;

                case FLAGS_SUB8: case FLAGS_DEC8:
                jump_uop = uop_CMP_JNO_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;

                case FLAGS_SUB16: case FLAGS_DEC16:
                jump_uop = uop_CMP_JNO_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;

                case FLAGS_SUB32: case FLAGS_DEC32:
                jump_uop = uop_CMP_JNO_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, VF_SET);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNO_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8: case FLAGS_ZN16: case FLAGS_ZN32:
                /*Overflow is always zero*/
                uop_MOV_IMM(ir, IREG_pc, dest_addr);
                uop_JMP(ir, codegen_exit_rout);
                return;

                case FLAGS_SUB8: case FLAGS_DEC8:
                jump_uop = uop_CMP_JO_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;

                case FLAGS_SUB16: case FLAGS_DEC16:
                jump_uop = uop_CMP_JO_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;

                case FLAGS_SUB32: case FLAGS_DEC32:
                jump_uop = uop_CMP_JO_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, VF_SET);
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}

static void ropJB_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8: case FLAGS_ZN16: case FLAGS_ZN32:
                /*Carry is always zero*/
                return;

                case FLAGS_SUB8:
                jump_uop = uop_CMP_JNB_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;

                case FLAGS_SUB16:
                jump_uop = uop_CMP_JNB_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;

                case FLAGS_SUB32:
                jump_uop = uop_CMP_JNB_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, CF_SET);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNB_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8: case FLAGS_ZN16: case FLAGS_ZN32:
                /*Carry is always zero*/
                uop_MOV_IMM(ir, IREG_pc, dest_addr);
                uop_JMP(ir, codegen_exit_rout);
                return;

                case FLAGS_SUB8:
                jump_uop = uop_CMP_JB_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;

                case FLAGS_SUB16:
                jump_uop = uop_CMP_JB_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;

                case FLAGS_SUB32:
                jump_uop = uop_CMP_JB_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, CF_SET);
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}

static void ropJE_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        if (!codegen_flags_changed || (cpu_state.flags_op == FLAGS_UNKNOWN))
        {
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
        }
        else
        {
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_flags_res, 0);
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
void ropJNE_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        if (!codegen_flags_changed || (cpu_state.flags_op == FLAGS_UNKNOWN))
        {
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
        }
        else
        {
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_flags_res, 0);
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}

static void ropJBE_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop, jump_uop2 = -1;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8: case FLAGS_ZN16: case FLAGS_ZN32:
                /*Carry is always zero, so test zero only*/
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_flags_res, 0);
                break;

                case FLAGS_SUB8:
                jump_uop = uop_CMP_JNBE_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;
                case FLAGS_SUB16:
                jump_uop = uop_CMP_JNBE_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;
                case FLAGS_SUB32:
                jump_uop = uop_CMP_JNBE_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, CF_SET);
                jump_uop2 = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        if (jump_uop2 != -1)
                uop_set_jump_dest(ir, jump_uop2);
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNBE_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop, jump_uop2 = -1;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8: case FLAGS_ZN16: case FLAGS_ZN32:
                /*Carry is always zero, so test zero only*/
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_flags_res, 0);
                break;

                case FLAGS_SUB8:
                jump_uop = uop_CMP_JBE_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;
                case FLAGS_SUB16:
                jump_uop = uop_CMP_JBE_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;
                case FLAGS_SUB32:
                jump_uop = uop_CMP_JBE_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, CF_SET);
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop2 = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
        if (jump_uop2 != -1)
                uop_set_jump_dest(ir, jump_uop2);
}

static void ropJS_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8:
                case FLAGS_ADD8:
                case FLAGS_SUB8:
                case FLAGS_SHL8:
                case FLAGS_SHR8:
                case FLAGS_SAR8:
                case FLAGS_INC8:
                case FLAGS_DEC8:
                jump_uop = uop_TEST_JNS_DEST(ir, IREG_flags_res_B);
                break;

                case FLAGS_ZN16:
                case FLAGS_ADD16:
                case FLAGS_SUB16:
                case FLAGS_SHL16:
                case FLAGS_SHR16:
                case FLAGS_SAR16:
                case FLAGS_INC16:
                case FLAGS_DEC16:
                jump_uop = uop_TEST_JNS_DEST(ir, IREG_flags_res_W);
                break;

                case FLAGS_ZN32:
                case FLAGS_ADD32:
                case FLAGS_SUB32:
                case FLAGS_SHL32:
                case FLAGS_SHR32:
                case FLAGS_SAR32:
                case FLAGS_INC32:
                case FLAGS_DEC32:
                jump_uop = uop_TEST_JNS_DEST(ir, IREG_flags_res);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, NF_SET);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNS_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8:
                case FLAGS_ADD8:
                case FLAGS_SUB8:
                case FLAGS_SHL8:
                case FLAGS_SHR8:
                case FLAGS_SAR8:
                case FLAGS_INC8:
                case FLAGS_DEC8:
                jump_uop = uop_TEST_JS_DEST(ir, IREG_flags_res_B);
                break;

                case FLAGS_ZN16:
                case FLAGS_ADD16:
                case FLAGS_SUB16:
                case FLAGS_SHL16:
                case FLAGS_SHR16:
                case FLAGS_SAR16:
                case FLAGS_INC16:
                case FLAGS_DEC16:
                jump_uop = uop_TEST_JS_DEST(ir, IREG_flags_res_W);
                break;

                case FLAGS_ZN32:
                case FLAGS_ADD32:
                case FLAGS_SUB32:
                case FLAGS_SHL32:
                case FLAGS_SHR32:
                case FLAGS_SAR32:
                case FLAGS_INC32:
                case FLAGS_DEC32:
                jump_uop = uop_TEST_JS_DEST(ir, IREG_flags_res);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, NF_SET);
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}

static void ropJP_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        uop_CALL_FUNC_RESULT(ir, IREG_temp0, PF_SET);
        jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNP_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        uop_CALL_FUNC_RESULT(ir, IREG_temp0, PF_SET);
        jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}

static void ropJL_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8:
                /*V flag is always clear. Condition is true if N is set*/
                jump_uop = uop_TEST_JNS_DEST(ir, IREG_flags_res_B);
                break;
                case FLAGS_ZN16:
                jump_uop = uop_TEST_JNS_DEST(ir, IREG_flags_res_W);
                break;
                case FLAGS_ZN32:
                jump_uop = uop_TEST_JNS_DEST(ir, IREG_flags_res);
                break;

                case FLAGS_SUB8: case FLAGS_DEC8:
                jump_uop = uop_CMP_JNL_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;
                case FLAGS_SUB16: case FLAGS_DEC16:
                jump_uop = uop_CMP_JNL_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;
                case FLAGS_SUB32: case FLAGS_DEC32:
                jump_uop = uop_CMP_JNL_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, NF_SET_01);
                uop_CALL_FUNC_RESULT(ir, IREG_temp1, VF_SET_01);
                jump_uop = uop_CMP_JZ_DEST(ir, IREG_temp0, IREG_temp1);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNL_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_ZN8:
                /*V flag is always clear. Condition is true if N is set*/
                jump_uop = uop_TEST_JS_DEST(ir, IREG_flags_res_B);
                break;
                case FLAGS_ZN16:
                jump_uop = uop_TEST_JS_DEST(ir, IREG_flags_res_W);
                break;
                case FLAGS_ZN32:
                jump_uop = uop_TEST_JS_DEST(ir, IREG_flags_res);
                break;

                case FLAGS_SUB8: case FLAGS_DEC8:
                jump_uop = uop_CMP_JL_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;
                case FLAGS_SUB16: case FLAGS_DEC16:
                jump_uop = uop_CMP_JL_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;
                case FLAGS_SUB32: case FLAGS_DEC32:
                jump_uop = uop_CMP_JL_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, NF_SET_01);
                uop_CALL_FUNC_RESULT(ir, IREG_temp1, VF_SET_01);
                jump_uop = uop_CMP_JNZ_DEST(ir, IREG_temp0, IREG_temp1);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}

static void ropJLE_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop, jump_uop2 = -1;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_SUB8: case FLAGS_DEC8:
                jump_uop = uop_CMP_JNLE_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;
                case FLAGS_SUB16: case FLAGS_DEC16:
                jump_uop = uop_CMP_JNLE_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;
                case FLAGS_SUB32: case FLAGS_DEC32:
                jump_uop = uop_CMP_JNLE_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop2 = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, NF_SET_01);
                uop_CALL_FUNC_RESULT(ir, IREG_temp1, VF_SET_01);
                jump_uop = uop_CMP_JZ_DEST(ir, IREG_temp0, IREG_temp1);
                break;
        }
        if (jump_uop2 != -1)
                uop_set_jump_dest(ir, jump_uop2);
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
}
static void ropJNLE_common(codeblock_t *block, ir_data_t *ir, uint32_t dest_addr)
{
        int jump_uop, jump_uop2 = -1;

        switch (codegen_flags_changed ? cpu_state.flags_op : FLAGS_UNKNOWN)
        {
                case FLAGS_SUB8: case FLAGS_DEC8:
                jump_uop = uop_CMP_JLE_DEST(ir, IREG_flags_op1_B, IREG_flags_op2_B);
                break;
                case FLAGS_SUB16: case FLAGS_DEC16:
                jump_uop = uop_CMP_JLE_DEST(ir, IREG_flags_op1_W, IREG_flags_op2_W);
                break;
                case FLAGS_SUB32: case FLAGS_DEC32:
                jump_uop = uop_CMP_JLE_DEST(ir, IREG_flags_op1, IREG_flags_op2);
                break;

                case FLAGS_UNKNOWN:
                default:
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop2 = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, NF_SET_01);
                uop_CALL_FUNC_RESULT(ir, IREG_temp1, VF_SET_01);
                jump_uop = uop_CMP_JNZ_DEST(ir, IREG_temp0, IREG_temp1);
                break;
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
        if (jump_uop2 != -1)
                uop_set_jump_dest(ir, jump_uop2);
}

#define ropJ(cond)                                                                                                                      \
uint32_t ropJ ## cond ## _8(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)       \
{                                                                                                                                       \
	uint32_t offset = (int32_t)(int8_t)fastreadb(cs + op_pc);                                                                       \
	uint32_t dest_addr = op_pc + 1 + offset;                                                                                        \
                                                                                                                                        \
	if (!(op_32 & 0x100))                                                                                                           \
                dest_addr &= 0xffff;                                                                                                    \
	ropJ ## cond ## _common(block, ir, dest_addr);                                                                                  \
                                                                                                                                        \
        codegen_mark_code_present(block, cs+op_pc, 1);                                                                                  \
	return op_pc+1;                                                                                                                 \
}                                                                                                                                       \
uint32_t ropJ ## cond ## _16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)      \
{                                                                                                                                       \
	uint32_t offset = (int32_t)(int16_t)fastreadw(cs + op_pc);                                                                      \
	uint32_t dest_addr = (op_pc + 2 + offset) & 0xffff;                                                                             \
                                                                                                                                        \
        ropJ ## cond ## _common(block, ir, dest_addr);                                                                                  \
                                                                                                                                        \
        codegen_mark_code_present(block, cs+op_pc, 2);                                                                                  \
	return op_pc+2;                                                                                                                 \
}                                                                                                                                       \
uint32_t ropJ ## cond ## _32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)      \
{                                                                                                                                       \
	uint32_t offset = fastreadl(cs + op_pc);                                                                                        \
	uint32_t dest_addr = op_pc + 4 + offset;                                                                                        \
                                                                                                                                        \
        ropJ ## cond ## _common(block, ir, dest_addr);                                                                                  \
                                                                                                                                        \
        codegen_mark_code_present(block, cs+op_pc, 4);                                                                                  \
	return op_pc+4;                                                                                                                 \
}

ropJ(O)
ropJ(NO)
ropJ(B)
ropJ(NB)
ropJ(E)
ropJ(NE)
ropJ(BE)
ropJ(NBE)
ropJ(S)
ropJ(NS)
ropJ(P)
ropJ(NP)
ropJ(L)
ropJ(NL)
ropJ(LE)
ropJ(NLE)


uint32_t ropJCXZ(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t offset = (int32_t)(int8_t)fastreadb(cs + op_pc);
        uint32_t dest_addr = op_pc + 1 + offset;
        int jump_uop;

	if (!(op_32 & 0x100))
                dest_addr &= 0xffff;

        if (op_32 & 0x200)
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_ECX, 0);
        else
                jump_uop = uop_CMP_IMM_JNZ_DEST(ir, IREG_CX, 0);
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);

        codegen_mark_code_present(block, cs+op_pc, 1);
        return op_pc+1;
}

uint32_t ropLOOP(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t offset = (int32_t)(int8_t)fastreadb(cs + op_pc);
        uint32_t dest_addr = op_pc + 1 + offset;
        int jump_uop;

	if (!(op_32 & 0x100))
                dest_addr &= 0xffff;

        if (op_32 & 0x200)
        {
                uop_SUB_IMM(ir, IREG_ECX, IREG_ECX, 1);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_ECX, 0);
        }
        else
        {
                uop_SUB_IMM(ir, IREG_CX, IREG_CX, 1);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_CX, 0);
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);

        codegen_mark_code_present(block, cs+op_pc, 1);
        return op_pc+1;
}

uint32_t ropLOOPE(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t offset = (int32_t)(int8_t)fastreadb(cs + op_pc);
        uint32_t dest_addr = op_pc + 1 + offset;
        int jump_uop, jump_uop2;

	if (!(op_32 & 0x100))
                dest_addr &= 0xffff;

        if (op_32 & 0x200)
        {
                uop_SUB_IMM(ir, IREG_ECX, IREG_ECX, 1);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_ECX, 0);
        }
        else
        {
                uop_SUB_IMM(ir, IREG_CX, IREG_CX, 1);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_CX, 0);
        }
        if (!codegen_flags_changed || (cpu_state.flags_op == FLAGS_UNKNOWN))
        {
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop2 = uop_CMP_IMM_JZ_DEST(ir, IREG_temp0, 0);
        }
        else
        {
                jump_uop2 = uop_CMP_IMM_JNZ_DEST(ir, IREG_flags_res, 0);
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
        uop_set_jump_dest(ir, jump_uop2);

        codegen_mark_code_present(block, cs+op_pc, 1);
        return op_pc+1;
}
uint32_t ropLOOPNE(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t offset = (int32_t)(int8_t)fastreadb(cs + op_pc);
        uint32_t dest_addr = op_pc + 1 + offset;
        int jump_uop, jump_uop2;

	if (!(op_32 & 0x100))
                dest_addr &= 0xffff;

        if (op_32 & 0x200)
        {
                uop_SUB_IMM(ir, IREG_ECX, IREG_ECX, 1);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_ECX, 0);
        }
        else
        {
                uop_SUB_IMM(ir, IREG_CX, IREG_CX, 1);
                jump_uop = uop_CMP_IMM_JZ_DEST(ir, IREG_CX, 0);
        }
        if (!codegen_flags_changed || (cpu_state.flags_op == FLAGS_UNKNOWN))
        {
                uop_CALL_FUNC_RESULT(ir, IREG_temp0, ZF_SET);
                jump_uop2 = uop_CMP_IMM_JNZ_DEST(ir, IREG_temp0, 0);
        }
        else
        {
                jump_uop2 = uop_CMP_IMM_JZ_DEST(ir, IREG_flags_res, 0);
        }
        uop_MOV_IMM(ir, IREG_pc, dest_addr);
        uop_JMP(ir, codegen_exit_rout);
        uop_set_jump_dest(ir, jump_uop);
        uop_set_jump_dest(ir, jump_uop2);

        codegen_mark_code_present(block, cs+op_pc, 1);
        return op_pc+1;
}
