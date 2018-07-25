#ifdef __amd64__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_x86-64_defs.h"
#include "codegen_backend_x86-64_ops.h"
#include "codegen_ir_defs.h"

#define STACK_ARG0 (0)
#define STACK_ARG1 (4)
#define STACK_ARG2 (8)
#define STACK_ARG3 (12)

#define HOST_REG_GET(reg) ((IREG_GET_SIZE(reg) == IREG_SIZE_BH) ? (IREG_GET_REG((reg) & 3) | 4) : (IREG_GET_REG(reg) & 7))

#define REG_IS_L(size) (size == IREG_SIZE_L)
#define REG_IS_W(size) (size == IREG_SIZE_W)
#define REG_IS_B(size) (size == IREG_SIZE_B || size == IREG_SIZE_BH)
#define REG_IS_BH(size) (size == IREG_SIZE_BH)

static int codegen_ADD(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_ADD32_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_ADD16_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_ADD8_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else
                fatal("ADD %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}

static int codegen_ADD_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                host_x86_ADD32_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                host_x86_ADD16_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                host_x86_ADD8_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else
                fatal("ADD_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_ADD_LSHIFT(codeblock_t *block, uop_t *uop)
{
        if (!uop->imm_data)
                host_x86_ADD32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);
        else if (uop->imm_data < 4)
                host_x86_LEA_REG_REG_SHIFT(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, uop->imm_data);
        else
                fatal("codegen_ADD_LSHIFT - shift out of range %i\n", uop->imm_data);

        return 0;
}

static int codegen_AND(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_AND32_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_AND16_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_AND8_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else
                fatal("AND %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}

static int codegen_AND_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                host_x86_AND32_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                host_x86_AND16_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                host_x86_AND8_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else
                fatal("AND_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
        host_x86_CALL(block, uop->p);
        host_x86_TEST32_REG(block, REG_EAX, REG_EAX);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_CMP_IMM_JZ(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_x86_CMP32_REG_IMM(block, src_reg, uop->imm_data);
        }
        else
                fatal("CMP_IMM_JZ %02x\n", uop->src_reg_a_real);
        host_x86_JZ(block, uop->p);

        return 0;
}

static int codegen_LOAD_FUNC_ARG0_IMM(codeblock_t *block, uop_t *uop)
{
#if WIN64
        host_x86_MOV32_REG_IMM(block, REG_ECX, uop->imm_data);
#else
        host_x86_MOV32_REG_IMM(block, REG_EDI, uop->imm_data);
#endif
        return 0;
}
static int codegen_LOAD_FUNC_ARG1_IMM(codeblock_t *block, uop_t *uop)
{
#if WIN64
        host_x86_MOV32_REG_IMM(block, REG_EDX, uop->imm_data);
#else
        host_x86_MOV32_REG_IMM(block, REG_ESI, uop->imm_data);
#endif
        return 0;
}
static int codegen_LOAD_FUNC_ARG2_IMM(codeblock_t *block, uop_t *uop)
{
        fatal("codegen_LOAD_FUNC_ARG2_IMM\n");
        return 0;
}
static int codegen_LOAD_FUNC_ARG3_IMM(codeblock_t *block, uop_t *uop)
{
        fatal("codegen_LOAD_FUNC_ARG3_IMM\n");
        return 0;
}

static int codegen_MEM_LOAD_ABS(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

        host_x86_LEA_REG_IMM(block, REG_ESI, seg_reg, uop->imm_data);
        if (REG_IS_B(dest_size))
        {
                host_x86_CALL(block, codegen_mem_load_byte);
        }
        else if (REG_IS_W(dest_size))
        {
                host_x86_CALL(block, codegen_mem_load_word);
        }
        else if (REG_IS_L(dest_size))
        {
                host_x86_CALL(block, codegen_mem_load_long);
        }
        else
                fatal("MEM_LOAD_ABS - %02x\n", uop->dest_reg_a_real);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);
        if (REG_IS_B(dest_size))
        {
                host_x86_MOV8_REG_REG(block, dest_reg, REG_ECX);
        }
        else if (REG_IS_W(dest_size))
        {
                host_x86_MOV16_REG_REG(block, dest_reg, REG_ECX);
        }
        else if (REG_IS_L(dest_size))
        {
                host_x86_MOV32_REG_REG(block, dest_reg, REG_ECX);
        }

        return 0;
}
static int codegen_MEM_LOAD_REG(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (REG_IS_B(dest_size))
        {
                host_x86_CALL(block, codegen_mem_load_byte);
        }
        else if (REG_IS_W(dest_size))
        {
                host_x86_CALL(block, codegen_mem_load_word);
        }
        else if (REG_IS_L(dest_size))
        {
                host_x86_CALL(block, codegen_mem_load_long);
        }
        else
                fatal("MEM_LOAD_REG - %02x\n", uop->dest_reg_a_real);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);
        if (REG_IS_B(dest_size))
        {
                host_x86_MOV8_REG_REG(block, dest_reg, REG_ECX);
//                fatal("Here\n");
        }
        else if (REG_IS_W(dest_size))
        {
                host_x86_MOV16_REG_REG(block, dest_reg, REG_ECX);
        }
        else if (REG_IS_L(dest_size))
        {
                host_x86_MOV32_REG_REG(block, dest_reg, REG_ECX);
        }

        return 0;
}

static int codegen_MEM_STORE_ABS(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_b_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_b_real);

        host_x86_LEA_REG_IMM(block, REG_ESI, seg_reg, uop->imm_data);
        if (REG_IS_B(src_size))
        {
                host_x86_MOV8_REG_REG(block, REG_ECX, src_reg);
                host_x86_CALL(block, codegen_mem_store_byte);
        }
        else if (REG_IS_W(src_size))
        {
                host_x86_MOV16_REG_REG(block, REG_ECX, src_reg);
                host_x86_CALL(block, codegen_mem_store_word);
        }
        else if (REG_IS_L(src_size))
        {
                host_x86_MOV32_REG_REG(block, REG_ECX, src_reg);
                host_x86_CALL(block, codegen_mem_store_long);
        }
        else
                fatal("MEM_STORE_ABS - %02x\n", uop->src_reg_b_real);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MEM_STORE_IMM_8(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        host_x86_MOV8_REG_IMM(block, REG_ECX, uop->imm_data);
        host_x86_CALL(block, codegen_mem_store_byte);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_IMM_16(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        host_x86_MOV16_REG_IMM(block, REG_ECX, uop->imm_data);
        host_x86_CALL(block, codegen_mem_store_word);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_IMM_32(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        host_x86_MOV32_REG_IMM(block, REG_ECX, uop->imm_data);
        host_x86_CALL(block, codegen_mem_store_long);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MEM_STORE_REG(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real), src_reg = HOST_REG_GET(uop->src_reg_c_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_c_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (REG_IS_B(src_size))
        {
                host_x86_MOV8_REG_REG(block, REG_ECX, src_reg);
                host_x86_CALL(block, codegen_mem_store_byte);
        }
        else if (REG_IS_W(src_size))
        {
                host_x86_MOV16_REG_REG(block, REG_ECX, src_reg);
                host_x86_CALL(block, codegen_mem_store_word);
        }
        else if (REG_IS_L(src_size))
        {
                host_x86_MOV32_REG_REG(block, REG_ECX, src_reg);
                host_x86_CALL(block, codegen_mem_store_long);
        }
        else
                fatal("MEM_STORE_ABS - %02x\n", uop->src_reg_b_real);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MOV(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
        }
        else
                fatal("MOV %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_MOV_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

        if (REG_IS_L(dest_size))
        {
                host_x86_MOV32_REG_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size))
        {
                host_x86_MOV16_REG_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size))
        {
                host_x86_MOV8_REG_IMM(block, dest_reg, uop->imm_data);
        }
        else
                fatal("MOV_IMM %02x\n", uop->dest_reg_a_real);

        return 0;
}
static int codegen_MOV_PTR(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV64_REG_IMM(block, uop->dest_reg_a_real, (uint64_t)uop->p);
        return 0;
}
static int codegen_MOVZX(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_W(src_size))
        {
                host_x86_MOVZX_REG_32_16(block, dest_reg, src_reg);
        }
        else if (REG_IS_L(dest_size) && REG_IS_B(src_size))
        {
                host_x86_MOVZX_REG_32_8(block, dest_reg, src_reg);
        }
        else
                fatal("MOVZX %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);
        return 0;
}

static int codegen_OR(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_OR32_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_OR16_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_OR8_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else
                fatal("OR %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_OR_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                host_x86_OR32_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                host_x86_OR16_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                host_x86_OR8_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else
                fatal("OR_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_STORE_PTR_IMM(codeblock_t *block, uop_t *uop)
{
        if (((uintptr_t)uop->p) >> 32)
                fatal("STORE_PTR_IMM 64-bit addr\n");
        host_x86_MOV32_ABS_IMM(block, uop->p, uop->imm_data);
        return 0;
}
static int codegen_STORE_PTR_IMM_8(codeblock_t *block, uop_t *uop)
{
        if (((uintptr_t)uop->p) >> 32)
                fatal("STORE_PTR_IMM_8 64-bit addr\n");
        host_x86_MOV8_ABS_IMM(block, uop->p, uop->imm_data);
        return 0;
}

static int codegen_SUB(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                if (dest_reg != src_reg_a)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg_a);
                host_x86_SUB32_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                if (dest_reg != src_reg_a)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg_a);
                host_x86_SUB16_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                if (dest_reg != src_reg_a)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg_a);
                host_x86_SUB8_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else
                fatal("SUB %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_SUB_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (dest_reg != src_reg)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SUB32_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (dest_reg != src_reg)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SUB16_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (dest_reg != src_reg)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SUB8_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else
                fatal("SUB_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_XOR(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_XOR32_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_XOR16_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_XOR8_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else
                fatal("XOR %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_XOR_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                host_x86_XOR32_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                host_x86_XOR16_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                host_x86_XOR8_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else
                fatal("XOR_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

const uOpFn uop_handlers[UOP_MAX] =
{
        [UOP_CALL_INSTRUCTION_FUNC & UOP_MASK] = codegen_CALL_INSTRUCTION_FUNC,

        [UOP_LOAD_FUNC_ARG_0_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG0_IMM,
        [UOP_LOAD_FUNC_ARG_1_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG1_IMM,
        [UOP_LOAD_FUNC_ARG_2_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG2_IMM,
        [UOP_LOAD_FUNC_ARG_3_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG3_IMM,

        [UOP_STORE_P_IMM & UOP_MASK] = codegen_STORE_PTR_IMM,
        [UOP_STORE_P_IMM_8 & UOP_MASK] = codegen_STORE_PTR_IMM_8,

        [UOP_MEM_LOAD_ABS & UOP_MASK] = codegen_MEM_LOAD_ABS,
        [UOP_MEM_LOAD_REG & UOP_MASK] = codegen_MEM_LOAD_REG,

        [UOP_MEM_STORE_ABS & UOP_MASK] = codegen_MEM_STORE_ABS,
        [UOP_MEM_STORE_REG & UOP_MASK] = codegen_MEM_STORE_REG,
        [UOP_MEM_STORE_IMM_8 & UOP_MASK] = codegen_MEM_STORE_IMM_8,
        [UOP_MEM_STORE_IMM_16 & UOP_MASK] = codegen_MEM_STORE_IMM_16,
        [UOP_MEM_STORE_IMM_32 & UOP_MASK] = codegen_MEM_STORE_IMM_32,

        [UOP_MOV     & UOP_MASK] = codegen_MOV,
        [UOP_MOV_PTR & UOP_MASK] = codegen_MOV_PTR,
        [UOP_MOV_IMM & UOP_MASK] = codegen_MOV_IMM,
        [UOP_MOVZX   & UOP_MASK] = codegen_MOVZX,
        
        [UOP_ADD     & UOP_MASK] = codegen_ADD,
        [UOP_ADD_IMM & UOP_MASK] = codegen_ADD_IMM,
        [UOP_ADD_LSHIFT & UOP_MASK] = codegen_ADD_LSHIFT,
        [UOP_AND     & UOP_MASK] = codegen_AND,
        [UOP_AND_IMM & UOP_MASK] = codegen_AND_IMM,
        [UOP_OR      & UOP_MASK] = codegen_OR,
        [UOP_OR_IMM  & UOP_MASK] = codegen_OR_IMM,
        [UOP_SUB     & UOP_MASK] = codegen_SUB,
        [UOP_SUB_IMM & UOP_MASK] = codegen_SUB_IMM,
        [UOP_XOR     & UOP_MASK] = codegen_XOR,
        [UOP_XOR_IMM & UOP_MASK] = codegen_XOR_IMM,

        [UOP_CMP_IMM_JZ & UOP_MASK] = codegen_CMP_IMM_JZ
};

void codegen_direct_read_32(codeblock_t *block, int host_reg, void *p)
{
        host_x86_MOV32_REG_ABS(block, host_reg, p);
}

void codegen_direct_write_8(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV8_ABS_REG(block, p, host_reg);
}

void codegen_direct_write_32(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV32_ABS_REG(block, p, host_reg);
}

void codegen_direct_write_ptr(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV64_ABS_REG(block, p, host_reg);
}

#endif
