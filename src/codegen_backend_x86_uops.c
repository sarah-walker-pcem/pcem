#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

#include "ibm.h"
#include "x86.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_x86_defs.h"
#include "codegen_backend_x86_ops.h"
#include "codegen_ir_defs.h"

#define STACK_ARG0 (0)
#define STACK_ARG1 (4)
#define STACK_ARG2 (8)
#define STACK_ARG3 (12)

/*void codegen_debug()
{
        pclog(" %04x:%04x : %08x %08x %08x %08x\n", CS, cpu_state.pc, AX, BX, CX, DX);
}*/

#define HOST_REG_IS_L(reg)  (IREG_GET_SIZE(reg) == IREG_SIZE_L)
#define HOST_REG_IS_W(reg)  (IREG_GET_SIZE(reg) == IREG_SIZE_W)
#define HOST_REG_IS_B(reg)  (IREG_GET_SIZE(reg) == IREG_SIZE_B  && IREG_GET_REG(reg) < 4)
#define HOST_REG_IS_BH(reg) (IREG_GET_SIZE(reg) == IREG_SIZE_BH && IREG_GET_REG(reg) < 4)

#define HOST_REG_GET(reg) ((IREG_GET_SIZE(reg) == IREG_SIZE_BH) ? (IREG_GET_REG((reg) & 3) | 4) : (IREG_GET_REG(reg) & 7))

#define REG_IS_L(size) (size == IREG_SIZE_L)
#define REG_IS_W(size) (size == IREG_SIZE_W)
#define REG_IS_B(size) (size == IREG_SIZE_B || size == IREG_SIZE_BH)
#define REG_IS_BH(size) (size == IREG_SIZE_BH)
#define REG_IS_D(size) (size == IREG_SIZE_D)
#define REG_IS_Q(size) (size == IREG_SIZE_Q)

static int codegen_ADD(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_LEA_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
                else
                        host_x86_ADD32_REG_REG(block, dest_reg, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg_a);
                host_x86_ADD16_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg_a);
                host_x86_ADD8_REG_REG(block, dest_reg, dest_reg, src_reg_b);
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
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_LEA_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
                else
                        host_x86_ADD32_REG_IMM(block, dest_reg, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_ADD16_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_ADD8_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
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
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg_a);
                host_x86_AND32_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg_a);
                host_x86_AND16_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg_a);
                host_x86_AND8_REG_REG(block, dest_reg, dest_reg, src_reg_b);
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
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_AND32_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_AND16_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_AND8_REG_IMM(block, dest_reg, dest_reg, uop->imm_data);
        }
        else
                fatal("AND_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_CALL_FUNC(codeblock_t *block, uop_t *uop)
{
        host_x86_CALL(block, uop->p);

        return 0;
}

static int codegen_CALL_FUNC_RESULT(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

        if (!REG_IS_L(dest_size))
                fatal("CALL_FUNC_RESULT %02x\n", uop->dest_reg_a_real);
        host_x86_CALL(block, uop->p);
        host_x86_MOV32_REG_REG(block, dest_reg, REG_EAX);

        return 0;
}

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
        host_x86_CALL(block, uop->p);
        host_x86_TEST32_REG(block, REG_EAX, REG_EAX);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);
//        host_x86_CALL(block, codegen_debug);

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

static int codegen_CMP_IMM_JNZ_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_x86_CMP32_REG_IMM(block, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(src_size))
        {
                host_x86_CMP16_REG_IMM(block, src_reg, uop->imm_data);
        }
        else
                fatal("CMP_IMM_JNZ_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNZ_long(block);

        return 0;
}
static int codegen_CMP_IMM_JZ_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_x86_CMP32_REG_IMM(block, src_reg, uop->imm_data);
        }
        else if (REG_IS_W(src_size))
        {
                host_x86_CMP16_REG_IMM(block, src_reg, uop->imm_data);
        }
        else
                fatal("CMP_IMM_JZ_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JZ_long(block);
        
        return 0;
}

static int codegen_CMP_JNB_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JNB_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNB_long(block);

        return 0;
}
static int codegen_CMP_JNBE_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JNBE_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNBE_long(block);

        return 0;
}
static int codegen_CMP_JNL_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JNL_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNL_long(block);

        return 0;
}
static int codegen_CMP_JNLE_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JNLE_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNLE_long(block);

        return 0;
}
static int codegen_CMP_JNO_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JNO_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNO_long(block);

        return 0;
}
static int codegen_CMP_JNZ_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JNZ_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNZ_long(block);

        return 0;
}
static int codegen_CMP_JB_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JB_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JB_long(block);

        return 0;
}
static int codegen_CMP_JBE_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JBE_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JBE_long(block);

        return 0;
}
static int codegen_CMP_JL_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JL_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JL_long(block);

        return 0;
}
static int codegen_CMP_JLE_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JLE_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JLE_long(block);

        return 0;
}
static int codegen_CMP_JO_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JO_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JO_long(block);

        return 0;
}
static int codegen_CMP_JZ_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                host_x86_CMP32_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                host_x86_CMP16_REG_REG(block, src_reg_a, src_reg_b);
        }
        else if (REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                host_x86_CMP8_REG_REG(block, src_reg_a, src_reg_b);
        }
        else
                fatal("CMP_JZ_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JZ_long(block);

        return 0;
}

static int codegen_FADD(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_D(dest_size) && REG_IS_D(src_size_a) && REG_IS_D(src_size_b) && dest_reg == src_reg_a)
        {
                host_x86_ADDSD_XREG_XREG(block, dest_reg, src_reg_b);
        }
        else
                fatal("codegen_FADD %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_FDIV(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_D(dest_size) && REG_IS_D(src_size_a) && REG_IS_D(src_size_b) && dest_reg == src_reg_a)
        {
                host_x86_DIVSD_XREG_XREG(block, dest_reg, src_reg_b);
        }
        else if (REG_IS_D(dest_size) && REG_IS_D(src_size_a) && REG_IS_D(src_size_b))
        {
                host_x86_MOVQ_XREG_XREG(block, REG_XMM_TEMP, src_reg_a);
                host_x86_DIVSD_XREG_XREG(block, REG_XMM_TEMP, src_reg_b);
                host_x86_MOVQ_XREG_XREG(block, dest_reg, REG_XMM_TEMP);
        }
        else
                fatal("codegen_FDIV %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_FMUL(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_D(dest_size) && REG_IS_D(src_size_a) && REG_IS_D(src_size_b) && dest_reg == src_reg_a)
        {
                host_x86_MULSD_XREG_XREG(block, dest_reg, src_reg_b);
        }
        else
                fatal("codegen_FMUL %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_FSUB(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_D(dest_size) && REG_IS_D(src_size_a) && REG_IS_D(src_size_b) && dest_reg == src_reg_a)
        {
                host_x86_SUBSD_XREG_XREG(block, dest_reg, src_reg_b);
        }
        else if (REG_IS_D(dest_size) && REG_IS_D(src_size_a) && REG_IS_D(src_size_b))
        {
                host_x86_MOVQ_XREG_XREG(block, REG_XMM_TEMP, src_reg_a);
                host_x86_SUBSD_XREG_XREG(block, REG_XMM_TEMP, src_reg_b);
                host_x86_MOVQ_XREG_XREG(block, dest_reg, REG_XMM_TEMP);
        }
        else
                fatal("codegen_FSUB %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);
        
        return 0;
}

static int codegen_FP_ENTER(codeblock_t *block, uop_t *uop)
{
        uint8_t *branch_offset;
        
        host_x86_MOV32_REG_ABS(block, REG_ECX, &cr0);
        host_x86_TEST32_REG_IMM(block, REG_ECX, 0xc);
        branch_offset = host_x86_JZ_short(block);
        host_x86_MOV32_ABS_IMM(block, &cpu_state.oldpc, uop->imm_data);
        host_x86_MOV32_STACK_IMM(block, STACK_ARG0, 7);
        host_x86_CALL(block, x86_int);
        host_x86_JMP(block, &block->data[BLOCK_EXIT_OFFSET]);
        *branch_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)branch_offset) - 1;

        return 0;
}

static int codegen_JMP(codeblock_t *block, uop_t *uop)
{
        host_x86_JMP(block, uop->p);

        return 0;
}

static int codegen_LOAD_FUNC_ARG0(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_W(src_size))
        {
                host_x86_MOV16_STACK_REG(block, STACK_ARG0, src_reg);
        }
        else
                fatal("codegen_LOAD_FUNC_ARG0 %02x\n", uop->src_reg_a_real);

        return 0;
}
static int codegen_LOAD_FUNC_ARG1(codeblock_t *block, uop_t *uop)
{
        fatal("codegen_LOAD_FUNC_ARG1 %02x\n", uop->src_reg_a_real);
        return 0;
}
static int codegen_LOAD_FUNC_ARG2(codeblock_t *block, uop_t *uop)
{
        fatal("codegen_LOAD_FUNC_ARG2 %02x\n", uop->src_reg_a_real);
        return 0;
}
static int codegen_LOAD_FUNC_ARG3(codeblock_t *block, uop_t *uop)
{
        fatal("codegen_LOAD_FUNC_ARG3 %02x\n", uop->src_reg_a_real);
        return 0;
}

static int codegen_LOAD_FUNC_ARG0_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_STACK_IMM(block, STACK_ARG0, uop->imm_data);
        return 0;
}
static int codegen_LOAD_FUNC_ARG1_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_STACK_IMM(block, STACK_ARG1, uop->imm_data);
        return 0;
}
static int codegen_LOAD_FUNC_ARG2_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_STACK_IMM(block, STACK_ARG2, uop->imm_data);
        return 0;
}
static int codegen_LOAD_FUNC_ARG3_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_STACK_IMM(block, STACK_ARG3, uop->imm_data);
        return 0;
}

static int codegen_LOAD_SEG(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (!REG_IS_W(src_size))
                fatal("LOAD_SEG %02x %p\n", uop->src_reg_a_real, uop->p);
        host_x86_MOV16_STACK_REG(block, STACK_ARG0, src_reg);
        host_x86_MOV32_STACK_IMM(block, STACK_ARG1, (uint32_t)uop->p);
        host_x86_CALL(block, loadseg);
        host_x86_TEST32_REG(block, REG_EAX, REG_EAX);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

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
        if (uop->imm_data)
                host_x86_ADD32_REG_IMM(block, REG_ESI, REG_ESI, uop->imm_data);
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
static int codegen_MEM_LOAD_SINGLE(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

        if (!REG_IS_D(dest_size))
                fatal("MEM_LOAD_SINGLE - %02x\n", uop->dest_reg_a_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (uop->imm_data)
                host_x86_ADD32_REG_IMM(block, REG_ESI, REG_ESI, uop->imm_data);
        host_x86_CALL(block, codegen_mem_load_single);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);
        host_x86_MOVQ_XREG_XREG(block, dest_reg, REG_XMM_TEMP);

        return 0;
}

static int codegen_MEM_LOAD_DOUBLE(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

        if (!REG_IS_D(dest_size))
                fatal("MEM_LOAD_DOUBLE - %02x\n", uop->dest_reg_a_real);
                
        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (uop->imm_data)
                host_x86_ADD32_REG_IMM(block, REG_ESI, REG_ESI, uop->imm_data);
        host_x86_CALL(block, codegen_mem_load_double);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);
        host_x86_MOVQ_XREG_XREG(block, dest_reg, REG_XMM_TEMP);
        
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

static int codegen_MEM_STORE_REG(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real), src_reg = HOST_REG_GET(uop->src_reg_c_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_c_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (uop->imm_data)
                host_x86_ADD32_REG_IMM(block, REG_ESI, REG_ESI, uop->imm_data);
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

static int codegen_MEM_STORE_SINGLE(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real), src_reg = HOST_REG_GET(uop->src_reg_c_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_c_real);

        if (!REG_IS_D(src_size))
                fatal("MEM_STORE_SINGLE - %02x\n", uop->src_reg_b_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (uop->imm_data)
                host_x86_ADD32_REG_IMM(block, REG_ESI, REG_ESI, uop->imm_data);
        host_x86_CVTSD2SS_XREG_XREG(block, REG_XMM_TEMP, src_reg);
        host_x86_CALL(block, codegen_mem_store_single);
        host_x86_TEST32_REG(block, REG_ESI, REG_ESI);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_DOUBLE(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real), src_reg = HOST_REG_GET(uop->src_reg_c_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_c_real);

        if (!REG_IS_D(src_size))
                fatal("MEM_STORE_DOUBLE - %02x\n", uop->src_reg_b_real);

        host_x86_LEA_REG_REG(block, REG_ESI, seg_reg, addr_reg);
        if (uop->imm_data)
                host_x86_ADD32_REG_IMM(block, REG_ESI, REG_ESI, uop->imm_data);
        host_x86_MOVQ_XREG_XREG(block, REG_XMM_TEMP, src_reg);
        host_x86_CALL(block, codegen_mem_store_double);
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
        else if (REG_IS_D(dest_size) && REG_IS_D(src_size))
        {
                host_x86_MOVQ_XREG_XREG(block, dest_reg, src_reg);
        }
        else if (REG_IS_Q(dest_size) && REG_IS_Q(src_size))
        {
                host_x86_MOVQ_XREG_XREG(block, dest_reg, src_reg);
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
        host_x86_MOV32_REG_IMM(block, uop->dest_reg_a_real, (uint32_t)uop->p);
        return 0;
}
static int codegen_MOVSX(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_W(src_size))
        {
                host_x86_MOVSX_REG_32_16(block, dest_reg, src_reg);
        }
        else if (REG_IS_L(dest_size) && REG_IS_B(src_size))
        {
                host_x86_MOVSX_REG_32_8(block, dest_reg, src_reg);
        }
        else if (REG_IS_W(dest_size) && REG_IS_B(src_size))
        {
                host_x86_MOVSX_REG_16_8(block, dest_reg, src_reg);
        }
        else
                fatal("MOVSX %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);
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
        else if (REG_IS_W(dest_size) && REG_IS_B(src_size))
        {
                host_x86_MOVZX_REG_16_8(block, dest_reg, src_reg);
        }
        else
                fatal("MOVZX %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);
        return 0;
}

static int codegen_MOV_DOUBLE_INT(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_D(dest_size) && REG_IS_L(src_size))
        {
                host_x86_CVTSI2SD_XREG_REG(block, dest_reg, src_reg);
        }
        else if (REG_IS_D(dest_size) && REG_IS_W(src_size))
        {
                host_x86_MOVSX_REG_32_16(block, REG_ECX, src_reg);
                host_x86_CVTSI2SD_XREG_REG(block, dest_reg, REG_ECX);
        }
        else
                fatal("MOV_DOUBLE_INT %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_OR(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg_a);
                host_x86_OR32_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg_a);
                host_x86_OR16_REG_REG(block, dest_reg, dest_reg, src_reg_b);
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

static int codegen_SAR(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real), shift_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        host_x86_MOV32_REG_REG(block, REG_ECX, shift_reg);
        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SAR32_CL(block, dest_reg);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SAR16_CL(block, dest_reg);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SAR8_CL(block, dest_reg);
        }
        else
                fatal("SAR %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_SAR_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SAR32_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SAR16_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SAR8_IMM(block, dest_reg, uop->imm_data);
        }
        else
                fatal("SAR_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_SHL(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real), shift_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        host_x86_MOV32_REG_REG(block, REG_ECX, shift_reg);
        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SHL32_CL(block, dest_reg);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SHL16_CL(block, dest_reg);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SHL8_CL(block, dest_reg);
        }
        else
                fatal("SHL %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_SHL_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SHL32_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SHL16_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SHL8_IMM(block, dest_reg, uop->imm_data);
        }
        else
                fatal("SHL_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_SHR(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real), shift_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        host_x86_MOV32_REG_REG(block, REG_ECX, shift_reg);
        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SHR32_CL(block, dest_reg);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SHR16_CL(block, dest_reg);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SHR8_CL(block, dest_reg);
        }
        else
                fatal("SHR %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_SHR_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg);
                host_x86_SHR32_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg);
                host_x86_SHR16_IMM(block, dest_reg, uop->imm_data);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV8_REG_REG(block, dest_reg, src_reg);
                host_x86_SHR8_IMM(block, dest_reg, uop->imm_data);
        }
        else
                fatal("SHR_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_STORE_PTR_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_ABS_IMM(block, uop->p, uop->imm_data);
        return 0;
}
static int codegen_STORE_PTR_IMM_8(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV8_ABS_IMM(block, uop->p, uop->imm_data);
        return 0;
}

static int codegen_SUB(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

        if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV32_REG_REG(block, dest_reg, src_reg_a);
                host_x86_SUB32_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
                        host_x86_MOV16_REG_REG(block, dest_reg, src_reg_a);
                host_x86_SUB16_REG_REG(block, dest_reg, dest_reg, src_reg_b);
        }
        else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
        {
                if (uop->dest_reg_a_real != uop->src_reg_a_real)
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

static int codegen_TEST_JNS_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_x86_TEST32_REG(block, src_reg, src_reg);
        }
        else if (REG_IS_W(src_size))
        {
                host_x86_TEST16_REG(block, src_reg, src_reg);
        }
        else if (REG_IS_B(src_size))
        {
                host_x86_TEST8_REG(block, src_reg, src_reg);
        }
        else
                fatal("TEST_JNS_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JNS_long(block);

        return 0;
}
static int codegen_TEST_JS_DEST(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_x86_TEST32_REG(block, src_reg, src_reg);
        }
        else if (REG_IS_W(src_size))
        {
                host_x86_TEST16_REG(block, src_reg, src_reg);
        }
        else if (REG_IS_B(src_size))
        {
                host_x86_TEST8_REG(block, src_reg, src_reg);
        }
        else
                fatal("TEST_JS_DEST %02x\n", uop->src_reg_a_real);

        uop->p = host_x86_JS_long(block);

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
                fatal("OR %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

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
        [UOP_CALL_FUNC & UOP_MASK] = codegen_CALL_FUNC,
        [UOP_CALL_FUNC_RESULT & UOP_MASK] = codegen_CALL_FUNC_RESULT,
        [UOP_CALL_INSTRUCTION_FUNC & UOP_MASK] = codegen_CALL_INSTRUCTION_FUNC,

        [UOP_JMP & UOP_MASK] = codegen_JMP,

        [UOP_LOAD_SEG & UOP_MASK] = codegen_LOAD_SEG,
        
        [UOP_LOAD_FUNC_ARG_0 & UOP_MASK] = codegen_LOAD_FUNC_ARG0,
        [UOP_LOAD_FUNC_ARG_1 & UOP_MASK] = codegen_LOAD_FUNC_ARG1,
        [UOP_LOAD_FUNC_ARG_2 & UOP_MASK] = codegen_LOAD_FUNC_ARG2,
        [UOP_LOAD_FUNC_ARG_3 & UOP_MASK] = codegen_LOAD_FUNC_ARG3,

        [UOP_LOAD_FUNC_ARG_0_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG0_IMM,
        [UOP_LOAD_FUNC_ARG_1_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG1_IMM,
        [UOP_LOAD_FUNC_ARG_2_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG2_IMM,
        [UOP_LOAD_FUNC_ARG_3_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG3_IMM,

        [UOP_STORE_P_IMM & UOP_MASK] = codegen_STORE_PTR_IMM,
        [UOP_STORE_P_IMM_8 & UOP_MASK] = codegen_STORE_PTR_IMM_8,
        
        [UOP_MEM_LOAD_ABS & UOP_MASK]    = codegen_MEM_LOAD_ABS,
        [UOP_MEM_LOAD_REG & UOP_MASK]    = codegen_MEM_LOAD_REG,
        [UOP_MEM_LOAD_SINGLE & UOP_MASK] = codegen_MEM_LOAD_SINGLE,
        [UOP_MEM_LOAD_DOUBLE & UOP_MASK] = codegen_MEM_LOAD_DOUBLE,
        
        [UOP_MEM_STORE_ABS & UOP_MASK] = codegen_MEM_STORE_ABS,
        [UOP_MEM_STORE_REG & UOP_MASK] = codegen_MEM_STORE_REG,
        [UOP_MEM_STORE_IMM_8 & UOP_MASK] = codegen_MEM_STORE_IMM_8,
        [UOP_MEM_STORE_IMM_16 & UOP_MASK] = codegen_MEM_STORE_IMM_16,
        [UOP_MEM_STORE_IMM_32 & UOP_MASK] = codegen_MEM_STORE_IMM_32,
        [UOP_MEM_STORE_SINGLE & UOP_MASK] = codegen_MEM_STORE_SINGLE,
        [UOP_MEM_STORE_DOUBLE & UOP_MASK] = codegen_MEM_STORE_DOUBLE,
        
        [UOP_MOV            & UOP_MASK] = codegen_MOV,
        [UOP_MOV_PTR        & UOP_MASK] = codegen_MOV_PTR,
        [UOP_MOV_IMM        & UOP_MASK] = codegen_MOV_IMM,
        [UOP_MOVSX          & UOP_MASK] = codegen_MOVSX,
        [UOP_MOVZX          & UOP_MASK] = codegen_MOVZX,
        [UOP_MOV_DOUBLE_INT & UOP_MASK] = codegen_MOV_DOUBLE_INT,
        
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

        [UOP_SAR     & UOP_MASK] = codegen_SAR,
        [UOP_SAR_IMM & UOP_MASK] = codegen_SAR_IMM,
        [UOP_SHL     & UOP_MASK] = codegen_SHL,
        [UOP_SHL_IMM & UOP_MASK] = codegen_SHL_IMM,
        [UOP_SHR     & UOP_MASK] = codegen_SHR,
        [UOP_SHR_IMM & UOP_MASK] = codegen_SHR_IMM,

        [UOP_CMP_IMM_JZ & UOP_MASK] = codegen_CMP_IMM_JZ,

        [UOP_CMP_JNB_DEST  & UOP_MASK] = codegen_CMP_JNB_DEST,
        [UOP_CMP_JNBE_DEST & UOP_MASK] = codegen_CMP_JNBE_DEST,
        [UOP_CMP_JNL_DEST  & UOP_MASK] = codegen_CMP_JNL_DEST,
        [UOP_CMP_JNLE_DEST & UOP_MASK] = codegen_CMP_JNLE_DEST,
        [UOP_CMP_JNO_DEST  & UOP_MASK] = codegen_CMP_JNO_DEST,
        [UOP_CMP_JNZ_DEST  & UOP_MASK] = codegen_CMP_JNZ_DEST,
        [UOP_CMP_JB_DEST   & UOP_MASK] = codegen_CMP_JB_DEST,
        [UOP_CMP_JBE_DEST  & UOP_MASK] = codegen_CMP_JBE_DEST,
        [UOP_CMP_JL_DEST   & UOP_MASK] = codegen_CMP_JL_DEST,
        [UOP_CMP_JLE_DEST  & UOP_MASK] = codegen_CMP_JLE_DEST,
        [UOP_CMP_JO_DEST   & UOP_MASK] = codegen_CMP_JO_DEST,
        [UOP_CMP_JZ_DEST   & UOP_MASK] = codegen_CMP_JZ_DEST,

        [UOP_CMP_IMM_JNZ_DEST & UOP_MASK] = codegen_CMP_IMM_JNZ_DEST,
        [UOP_CMP_IMM_JZ_DEST  & UOP_MASK] = codegen_CMP_IMM_JZ_DEST,

        [UOP_TEST_JNS_DEST & UOP_MASK] = codegen_TEST_JNS_DEST,
        [UOP_TEST_JS_DEST & UOP_MASK] = codegen_TEST_JS_DEST,
        
        [UOP_FP_ENTER & UOP_MASK] = codegen_FP_ENTER,
        
        [UOP_FADD & UOP_MASK] = codegen_FADD,
        [UOP_FDIV & UOP_MASK] = codegen_FDIV,
        [UOP_FMUL & UOP_MASK] = codegen_FMUL,
        [UOP_FSUB & UOP_MASK] = codegen_FSUB
};

void codegen_direct_read_8(codeblock_t *block, int host_reg, void *p)
{
        host_x86_MOV8_REG_ABS(block, host_reg, p);
}
void codegen_direct_read_16(codeblock_t *block, int host_reg, void *p)
{
        host_x86_MOV16_REG_ABS(block, host_reg, p);
}
void codegen_direct_read_32(codeblock_t *block, int host_reg, void *p)
{
        host_x86_MOV32_REG_ABS(block, host_reg, p);
}
void codegen_direct_read_64(codeblock_t *block, int host_reg, void *p)
{
        host_x86_MOVQ_XREG_ABS(block, host_reg, p);
}
void codegen_direct_read_double(codeblock_t *block, int host_reg, void *p)
{
        host_x86_MOVQ_XREG_ABS(block, host_reg, p);
}
void codegen_direct_read_st_8(codeblock_t *block, int host_reg, void *base, int reg_idx)
{
        int offset = (uintptr_t)base - (((uintptr_t)&cpu_state) + 128);

        host_x86_MOV32_REG_BASE_OFFSET(block, REG_ECX, REG_ESP, IREG_TOP_diff_stack_offset);
        host_x86_ADD32_REG_IMM(block, REG_ECX, REG_ECX, reg_idx);
        host_x86_AND32_REG_IMM(block, REG_ECX, REG_ECX, 7);
        host_x86_MOV8_REG_ABS_REG_REG_SHIFT(block, host_reg, offset, REG_EBP, REG_ECX, 0);
}
void codegen_direct_read_st_64(codeblock_t *block, int host_reg, void *base, int reg_idx)
{
        int offset = (uintptr_t)base - (((uintptr_t)&cpu_state) + 128);

        host_x86_MOV32_REG_BASE_OFFSET(block, REG_ECX, REG_ESP, IREG_TOP_diff_stack_offset);
        host_x86_ADD32_REG_IMM(block, REG_ECX, REG_ECX, reg_idx);
        host_x86_AND32_REG_IMM(block, REG_ECX, REG_ECX, 7);
        host_x86_MOVQ_XREG_ABS_REG_REG_SHIFT(block, host_reg, offset, REG_EBP, REG_ECX, 3);
}
void codegen_direct_read_st_double(codeblock_t *block, int host_reg, void *base, int reg_idx)
{
        int offset = (uintptr_t)base - (((uintptr_t)&cpu_state) + 128);

        host_x86_MOV32_REG_BASE_OFFSET(block, REG_ECX, REG_ESP, IREG_TOP_diff_stack_offset);
        host_x86_ADD32_REG_IMM(block, REG_ECX, REG_ECX, reg_idx);
        host_x86_AND32_REG_IMM(block, REG_ECX, REG_ECX, 7);
        host_x86_MOVQ_XREG_ABS_REG_REG_SHIFT(block, host_reg, offset, REG_EBP, REG_ECX, 3);
}

void codegen_direct_write_8(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV8_ABS_REG(block, p, host_reg);
}
void codegen_direct_write_16(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV16_ABS_REG(block, p, host_reg);
}
void codegen_direct_write_32(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV32_ABS_REG(block, p, host_reg);
}
void codegen_direct_write_64(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOVQ_ABS_XREG(block, p, host_reg);
}
void codegen_direct_write_double(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOVQ_ABS_XREG(block, p, host_reg);
}
void codegen_direct_write_st_8(codeblock_t *block, void *base, int reg_idx, int host_reg)
{
        int offset = (uintptr_t)base - (((uintptr_t)&cpu_state) + 128);

        host_x86_MOV32_REG_BASE_OFFSET(block, REG_ECX, REG_ESP, IREG_TOP_diff_stack_offset);
        host_x86_ADD32_REG_IMM(block, REG_ECX, REG_ECX, reg_idx);
        host_x86_AND32_REG_IMM(block, REG_ECX, REG_ECX, 7);
        host_x86_MOV8_ABS_REG_REG_SHIFT_REG(block, offset, REG_EBP, REG_ECX, 0, host_reg);
}
void codegen_direct_write_st_64(codeblock_t *block, void *base, int reg_idx, int host_reg)
{
        int offset = (uintptr_t)base - (((uintptr_t)&cpu_state) + 128);

        host_x86_MOV32_REG_BASE_OFFSET(block, REG_ECX, REG_ESP, IREG_TOP_diff_stack_offset);
        host_x86_ADD32_REG_IMM(block, REG_ECX, REG_ECX, reg_idx);
        host_x86_AND32_REG_IMM(block, REG_ECX, REG_ECX, 7);
        host_x86_MOVQ_ABS_REG_REG_SHIFT_XREG(block, offset, REG_EBP, REG_ECX, 3, host_reg);
}
void codegen_direct_write_st_double(codeblock_t *block, void *base, int reg_idx, int host_reg)
{
        int offset = (uintptr_t)base - (((uintptr_t)&cpu_state) + 128);

        host_x86_MOV32_REG_BASE_OFFSET(block, REG_ECX, REG_ESP, IREG_TOP_diff_stack_offset);
        host_x86_ADD32_REG_IMM(block, REG_ECX, REG_ECX, reg_idx);
        host_x86_AND32_REG_IMM(block, REG_ECX, REG_ECX, 7);
        host_x86_MOVQ_ABS_REG_REG_SHIFT_XREG(block, offset, REG_EBP, REG_ECX, 3, host_reg);
}

void codegen_direct_write_ptr(codeblock_t *block, void *p, int host_reg)
{
        host_x86_MOV32_ABS_REG(block, p, host_reg);
}

void codegen_direct_read_16_stack(codeblock_t *block, int host_reg, int stack_offset)
{
        host_x86_MOV16_REG_BASE_OFFSET(block, host_reg, REG_ESP, stack_offset);
}
void codegen_direct_read_32_stack(codeblock_t *block, int host_reg, int stack_offset)
{
        host_x86_MOV32_REG_BASE_OFFSET(block, host_reg, REG_ESP, stack_offset);
}
void codegen_direct_read_64_stack(codeblock_t *block, int host_reg, int stack_offset)
{
        host_x86_MOVQ_XREG_BASE_OFFSET(block, host_reg, REG_ESP, stack_offset);
}
void codegen_direct_read_double_stack(codeblock_t *block, int host_reg, int stack_offset)
{
        host_x86_MOVQ_XREG_BASE_OFFSET(block, host_reg, REG_ESP, stack_offset);
}

void codegen_direct_write_32_stack(codeblock_t *block, int stack_offset, int host_reg)
{
        host_x86_MOV32_BASE_OFFSET_REG(block, REG_ESP, stack_offset, host_reg);
}
void codegen_direct_write_64_stack(codeblock_t *block, int stack_offset, int host_reg)
{
        host_x86_MOVQ_BASE_OFFSET_XREG(block, REG_ESP, stack_offset, host_reg);
}
void codegen_direct_write_double_stack(codeblock_t *block, int stack_offset, int host_reg)
{
        host_x86_MOVQ_BASE_OFFSET_XREG(block, REG_ESP, stack_offset, host_reg);
}

void codegen_set_jump_dest(codeblock_t *block, void *p)
{
        *(uint32_t *)p = (uintptr_t)&block->data[block_pos] - ((uintptr_t)p + 4);
}

#endif
