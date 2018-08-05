#ifdef __aarch64__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm64_defs.h"
#include "codegen_backend_arm64_ops.h"
#include "codegen_ir_defs.h"

#define HOST_REG_GET(reg) (IREG_GET_REG(reg) & 0x1f)

#define REG_IS_L(size) (size == IREG_SIZE_L)
#define REG_IS_W(size) (size == IREG_SIZE_W)
#define REG_IS_B(size) (size == IREG_SIZE_B)
#define REG_IS_BH(size) (size == IREG_SIZE_BH)

static int codegen_ADD(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
	{
		host_arm64_ADD_REG(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
	{
		host_arm64_ADD_REG(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm64_ADD_REG(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm64_ADD_REG_LSR(block, REG_TEMP, src_reg_a, src_reg_b, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm64_ADD_REG_LSR(block, REG_TEMP, src_reg_b, src_reg_a, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_a, 0x0000ff00);
		host_arm64_ADD_REG(block, REG_TEMP, REG_TEMP, src_reg_b, 0);
		host_arm64_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
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
		host_arm64_ADD_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm64_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm64_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data << 8);
		host_arm64_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else
		fatal("ADD_IMM %x %x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_ADD_LSHIFT(codeblock_t *block, uop_t *uop)
{
	host_arm64_ADD_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, uop->imm_data);
        return 0;
}

static int codegen_AND(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
	{
		host_arm64_AND_REG(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_ORR_IMM(block, REG_TEMP, src_reg_b, 0xffff0000);
		host_arm64_AND_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_ORR_IMM(block, REG_TEMP, src_reg_b, 0xffffff00);
		host_arm64_AND_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_ORR_IMM(block, REG_TEMP, src_reg_b, 0xffff00ff);
		host_arm64_AND_REG_ASR(block, dest_reg, src_reg_a, REG_TEMP, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_ORR_IMM(block, REG_TEMP, src_reg_b, 0xffffff00);
		host_arm64_AND_REG_ROR(block, dest_reg, src_reg_a, REG_TEMP, 24);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_ORR_IMM(block, REG_TEMP, src_reg_b, 0xffff00ff);
		host_arm64_AND_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
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
		host_arm64_AND_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm64_AND_IMM(block, dest_reg, src_reg, uop->imm_data | 0xffff0000);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm64_AND_IMM(block, dest_reg, src_reg, uop->imm_data | 0xffffff00);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_AND_IMM(block, dest_reg, src_reg, (uop->imm_data << 8) | 0xffff00ff);
	}
	else
		fatal("AND_IMM %x %x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
	host_arm64_call(block, uop->p);
	host_arm64_CBNZ(block, REG_X0, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_CMP_IMM_JZ(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_arm64_CMP_IMM(block, src_reg, uop->imm_data);
        }
        else
                fatal("CMP_IMM_JZ %02x\n", uop->src_reg_a_real);
        host_arm64_BEQ(block, uop->p);

        return 0;
}

static int codegen_LOAD_FUNC_ARG0_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm64_mov_imm(block, REG_ARG0, uop->imm_data);

        return 0;
}
static int codegen_LOAD_FUNC_ARG1_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm64_mov_imm(block, REG_ARG1, uop->imm_data);

        return 0;
}
static int codegen_LOAD_FUNC_ARG2_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm64_mov_imm(block, REG_ARG2, uop->imm_data);

        return 0;
}
static int codegen_LOAD_FUNC_ARG3_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm64_mov_imm(block, REG_ARG3, uop->imm_data);

        return 0;
}

static int codegen_LOAD_SEG(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);
	int offset = add_literal_q(block, (uintptr_t)uop->p);

        if (!REG_IS_W(src_size))
                fatal("LOAD_SEG %02x %p\n", uop->src_reg_a_real, uop->p);

	host_arm64_LDR_LITERAL_X(block, REG_ARG1, offset);
	host_arm64_AND_IMM(block, REG_ARG0, src_reg, 0xffff);
	host_arm64_call(block, (void *)loadseg);
	host_arm64_CBNZ(block, REG_X0, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MEM_LOAD_ABS(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

	host_arm64_ADD_IMM(block, REG_X0, seg_reg, uop->imm_data);
        if (REG_IS_B(dest_size) || REG_IS_BH(dest_size))
        {
                host_arm64_call(block, codegen_mem_load_byte);
        }
        else if (REG_IS_W(dest_size))
        {
                host_arm64_call(block, codegen_mem_load_word);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm64_call(block, codegen_mem_load_long);
        }
        else
                fatal("MEM_LOAD_ABS - %02x\n", uop->dest_reg_a_real);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);
        if (REG_IS_B(dest_size))
        {
		host_arm64_BFI(block, dest_reg, REG_X0, 0, 8);
        }
	else if (REG_IS_BH(dest_size))
        {
		host_arm64_BFI(block, dest_reg, REG_X0, 8, 8);
        }
        else if (REG_IS_W(dest_size))
        {
		host_arm64_BFI(block, dest_reg, REG_X0, 0, 16);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm64_MOV_REG(block, dest_reg, REG_X0, 0);
        }

        return 0;
}
static int codegen_MEM_LOAD_REG(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

	host_arm64_ADD_REG(block, REG_X0, seg_reg, addr_reg, 0);
        if (REG_IS_B(dest_size) || REG_IS_BH(dest_size))
        {
                host_arm64_call(block, codegen_mem_load_byte);
        }
        else if (REG_IS_W(dest_size))
        {
                host_arm64_call(block, codegen_mem_load_word);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm64_call(block, codegen_mem_load_long);
        }
        else
                fatal("MEM_LOAD_ABS - %02x\n", uop->dest_reg_a_real);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);
        if (REG_IS_B(dest_size))
        {
		host_arm64_BFI(block, dest_reg, REG_X0, 0, 8);
        }
	else if (REG_IS_BH(dest_size))
        {
		host_arm64_BFI(block, dest_reg, REG_X0, 8, 8);
        }
        else if (REG_IS_W(dest_size))
        {
		host_arm64_BFI(block, dest_reg, REG_X0, 0, 16);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm64_MOV_REG(block, dest_reg, REG_X0, 0);
        }

        return 0;
}

static int codegen_MEM_STORE_ABS(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_b_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_b_real);

	host_arm64_ADD_IMM(block, REG_W0, seg_reg, uop->imm_data);
        if (REG_IS_B(src_size))
        {
		host_arm64_AND_IMM(block, REG_W1, src_reg, 0xff);
                host_arm64_call(block, codegen_mem_store_byte);
        }
	else if (REG_IS_BH(src_size))
	{
		host_arm64_UBFX(block, REG_W1, src_reg, 8, 8);
                host_arm64_call(block, codegen_mem_store_byte);
	}
        else if (REG_IS_W(src_size))
        {
		host_arm64_AND_IMM(block, REG_W1, src_reg, 0xffff);
                host_arm64_call(block, codegen_mem_store_word);
        }
        else if (REG_IS_L(src_size))
        {
		host_arm64_MOV_REG(block, REG_W1, src_reg, 0);
                host_arm64_call(block, codegen_mem_store_long);
        }
        else
                fatal("MEM_STORE_ABS - %02x\n", uop->dest_reg_a_real);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_REG(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real), src_reg = HOST_REG_GET(uop->src_reg_c_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_c_real);

	host_arm64_ADD_REG(block, REG_W0, seg_reg, addr_reg, 0);
        if (REG_IS_B(src_size))
        {
		host_arm64_AND_IMM(block, REG_W1, src_reg, 0xff);
                host_arm64_call(block, codegen_mem_store_byte);
        }
	else if (REG_IS_BH(src_size))
	{
		host_arm64_UBFX(block, REG_W1, src_reg, 8, 8);
                host_arm64_call(block, codegen_mem_store_byte);
	}
        else if (REG_IS_W(src_size))
        {
		host_arm64_AND_IMM(block, REG_W1, src_reg, 0xffff);
                host_arm64_call(block, codegen_mem_store_word);
        }
        else if (REG_IS_L(src_size))
        {
		host_arm64_MOV_REG(block, REG_W1, src_reg, 0);
                host_arm64_call(block, codegen_mem_store_long);
        }
        else
                fatal("MEM_STORE_REG - %02x\n", uop->dest_reg_a_real);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MEM_STORE_IMM_8(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

	host_arm64_ADD_REG(block, REG_W0, seg_reg, addr_reg, 0);
	host_arm64_mov_imm(block, REG_W1, uop->imm_data);
        host_arm64_call(block, codegen_mem_store_byte);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_IMM_16(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

	host_arm64_ADD_REG(block, REG_W0, seg_reg, addr_reg, 0);
	host_arm64_mov_imm(block, REG_W1, uop->imm_data);
        host_arm64_call(block, codegen_mem_store_word);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_IMM_32(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

	host_arm64_ADD_REG(block, REG_W0, seg_reg, addr_reg, 0);
	host_arm64_mov_imm(block, REG_W1, uop->imm_data);
        host_arm64_call(block, codegen_mem_store_long);
	host_arm64_CBNZ(block, REG_X1, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MOV(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size))
	{
		host_arm64_MOV_REG(block, dest_reg, src_reg, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm64_BFI(block, dest_reg, src_reg, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm64_BFI(block, dest_reg, src_reg, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_MOV_REG_LSR(block, REG_TEMP, src_reg, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_B(src_size))
	{
		host_arm64_BFI(block, dest_reg, src_reg, 8, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_MOV_REG_LSR(block, REG_TEMP, src_reg, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else
		fatal("MOV %x %x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}
static int codegen_MOV_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

	if (REG_IS_L(dest_size))
	{
		host_arm64_mov_imm(block, dest_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size))
	{
		host_arm64_MOVK_IMM(block, dest_reg, uop->imm_data);
	}
	else if (REG_IS_B(dest_size))
	{
		host_arm64_MOVZ_IMM(block, REG_TEMP, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size))
	{
		host_arm64_MOVZ_IMM(block, REG_TEMP, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else
		fatal("MOV_IMM %x\n", uop->dest_reg_a_real);

        return 0;
}
static int codegen_MOV_PTR(codeblock_t *block, uop_t *uop)
{
	int offset = add_literal_q(block, (uintptr_t)uop->p);
	host_arm64_LDR_LITERAL_X(block, uop->dest_reg_a_real, offset);

        return 0;
}

static int codegen_MOVZX(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

	if (REG_IS_L(dest_size) && REG_IS_B(src_size))
	{
		host_arm64_AND_IMM(block, dest_reg, src_reg, 0xff);
	}
	else if (REG_IS_L(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_UBFX(block, dest_reg, src_reg, 8, 8);
	}
	else if (REG_IS_L(dest_size) && REG_IS_W(src_size))
	{
		host_arm64_AND_IMM(block, dest_reg, src_reg, 0xffff);
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
		host_arm64_ORR_REG(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_b, 0xffff);
		host_arm64_ORR_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_b, 0xff);
		host_arm64_ORR_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_UBFX(block, REG_TEMP, src_reg_b, 8, 8);
		host_arm64_ORR_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_b, 0xff);
		host_arm64_ORR_REG(block, dest_reg, src_reg_a, REG_TEMP, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_UBFX(block, REG_TEMP, src_reg_b, 8, 8);
		host_arm64_ORR_REG(block, dest_reg, src_reg_a, REG_TEMP, 8);
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
		host_arm64_ORR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size) && dest_reg == src_reg)
	{
		host_arm64_ORR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size) && dest_reg == src_reg)
	{
		host_arm64_ORR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && dest_reg == src_reg)
	{
		host_arm64_ORR_IMM(block, dest_reg, src_reg, uop->imm_data << 8);
	}
	else
		fatal("OR_IMM %x %x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_STORE_PTR_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm64_mov_imm(block, REG_W16, uop->imm_data);

	if (in_range12_w((uintptr_t)uop->p - (uintptr_t)&cpu_state))
		host_arm64_STR_IMM_W(block, REG_W16, REG_CPUSTATE, (uintptr_t)uop->p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_STORE_PTR_IMM - not in range\n");

        return 0;
}
static int codegen_STORE_PTR_IMM_8(codeblock_t *block, uop_t *uop)
{
	host_arm64_mov_imm(block, REG_W16, uop->imm_data);

	if (in_range12_b((uintptr_t)uop->p - (uintptr_t)&cpu_state))
		host_arm64_STRB_IMM(block, REG_W16, REG_CPUSTATE, (uintptr_t)uop->p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_STORE_PTR_IMM - not in range\n");

        return 0;
}

static int codegen_SUB(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
	{
		host_arm64_SUB_REG(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
	{
		host_arm64_SUB_REG(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm64_SUB_REG(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm64_SUB_REG_LSR(block, REG_TEMP, src_reg_a, src_reg_b, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm64_SUB_REG(block, REG_TEMP, src_reg_a, src_reg_b, 8);
		host_arm64_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm64_MOV_REG_LSR(block, REG_TEMP, src_reg_a, 8);
		host_arm64_SUB_REG_LSR(block, REG_TEMP, REG_TEMP, src_reg_b, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm64_SUB_REG(block, REG_TEMP, src_reg_a, src_reg_b, 8);
		host_arm64_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm64_MOV_REG_LSR(block, REG_TEMP, src_reg_a, 8);
		host_arm64_SUB_REG_LSR(block, REG_TEMP, REG_TEMP, src_reg_b, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
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
		host_arm64_SUB_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm64_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm64_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data << 8);
		host_arm64_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size))
	{
		host_arm64_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data << 8);
		host_arm64_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else
		fatal("SUB_IMM %x %x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_XOR(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
	{
		host_arm64_EOR_REG(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_b, 0xffff);
		host_arm64_EOR_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_b, 0xff);
		host_arm64_EOR_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_UBFX(block, REG_TEMP, src_reg_b, 8, 8);
		host_arm64_EOR_REG(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_AND_IMM(block, REG_TEMP, src_reg_b, 0xff);
		host_arm64_EOR_REG(block, dest_reg, src_reg_a, REG_TEMP, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm64_UBFX(block, REG_TEMP, src_reg_b, 8, 8);
		host_arm64_EOR_REG(block, dest_reg, src_reg_a, REG_TEMP, 8);
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
		host_arm64_EOR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size) && dest_reg == src_reg)
	{
		host_arm64_EOR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size) && dest_reg == src_reg)
	{
		host_arm64_EOR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && dest_reg == src_reg)
	{
		host_arm64_EOR_IMM(block, dest_reg, src_reg, uop->imm_data << 8);
	}
	else
		fatal("XOR_IMM %x %x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

const uOpFn uop_handlers[UOP_MAX] =
{
        [UOP_CALL_INSTRUCTION_FUNC & UOP_MASK] = codegen_CALL_INSTRUCTION_FUNC,
        [UOP_LOAD_SEG & UOP_MASK] = codegen_LOAD_SEG,

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

void codegen_direct_read_8(codeblock_t *block, int host_reg, void *p)
{
	if (in_range12_b((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_LDRB_IMM_W(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_read_8 - not in range\n");
}
void codegen_direct_read_16(codeblock_t *block, int host_reg, void *p)
{
	if (in_range12_h((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_LDRH_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_read_16 - not in range\n");
}
void codegen_direct_read_32(codeblock_t *block, int host_reg, void *p)
{
	if (in_range12_w((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_LDR_IMM_W(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_read_32 - not in range\n");
}

void codegen_direct_write_8(codeblock_t *block, void *p, int host_reg)
{
	if (in_range12_b((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_STRB_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_8 - not in range\n");
}
void codegen_direct_write_16(codeblock_t *block, void *p, int host_reg)
{
	if (in_range12_h((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_STRH_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_16 - not in range\n");
}
void codegen_direct_write_32(codeblock_t *block, void *p, int host_reg)
{
	if (in_range12_w((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_STR_IMM_W(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_32 - not in range\n");
}

void codegen_direct_write_ptr(codeblock_t *block, void *p, int host_reg)
{
	if (in_range12_q((uintptr_t)p - (uintptr_t)&cpu_state))
		host_arm64_STR_IMM_Q(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_ptr - not in range\n");
}

void codegen_direct_read_16_stack(codeblock_t *block, int host_reg, int stack_offset)
{
	if (in_range12_h(stack_offset))
		host_arm64_LDRH_IMM(block, host_reg, REG_SP, stack_offset);
	else
		fatal("codegen_direct_read_32_stack - not in range\n");
}
void codegen_direct_read_32_stack(codeblock_t *block, int host_reg, int stack_offset)
{
	if (in_range12_w(stack_offset))
		host_arm64_LDR_IMM_W(block, host_reg, REG_SP, stack_offset);
	else
		fatal("codegen_direct_read_32_stack - not in range\n");
}

void codegen_direct_write_32_stack(codeblock_t *block, int stack_offset, int host_reg)
{
	if (in_range12_w(stack_offset))
		host_arm64_STR_IMM_W(block, host_reg, REG_SP, stack_offset);
	else
		fatal("codegen_direct_write_32_stack - not in range\n");
}

#endif
