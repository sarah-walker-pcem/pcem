#ifdef __ARM_EABI__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm_defs.h"
#include "codegen_backend_arm_ops.h"
#include "codegen_ir_defs.h"

static inline int get_arm_imm(uint32_t imm_data, uint32_t *arm_imm)
{
	int shift = 0;
//pclog("get_arm_imm - imm_data=%08x\n", imm_data);
	if (!(imm_data & 0xffff))
	{
		shift += 16;
		imm_data >>= 16;
	}
	if (!(imm_data & 0xff))
	{
		shift += 8;
		imm_data >>= 8;
	}
	if (!(imm_data & 0xf))
	{
		shift += 4;
		imm_data >>= 4;
	}
	if (!(imm_data & 0x3))
	{
		shift += 2;
		imm_data >>= 2;
	}
	if (imm_data > 0xff) /*Note - should handle rotation round the word*/
		return 0;
//pclog("   imm_data=%02x shift=%i\n", imm_data, shift);
	*arm_imm = imm_data | ((((32 - shift) >> 1) & 15) << 8);
	return 1;
}

static inline int in_range(void *addr, void *base)
{
	int diff = (uintptr_t)addr - (uintptr_t)base;

	if (diff < -4095 || diff > 4095)
		return 0;
	return 1;
}

void host_arm_call(codeblock_t *block, void *dst_addr)
{
	int offset = add_literal(block, (uintptr_t)dst_addr);
	host_arm_LDR_IMM(block, REG_R3, REG_LITERAL, offset);
	host_arm_BLX(block, REG_R3);
}

void host_arm_nop(codeblock_t *block)
{
	host_arm_MOV_REG_LSL(block, REG_R0, REG_R0, 0);
}

#define HOST_REG_GET(reg) (IREG_GET_REG(reg) & 0xf)

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
		host_arm_ADD_REG_LSL(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
	{
		host_arm_ADD_REG(block, REG_TEMP, src_reg_a, src_reg_b);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm_ADD_REG(block, REG_TEMP, src_reg_a, src_reg_b);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 8);
		host_arm_UADD8(block, dest_reg, src_reg_a, REG_TEMP);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 0);
		host_arm_MOV_REG_LSL(block, REG_TEMP, REG_TEMP, 8);
		host_arm_UADD8(block, dest_reg, src_reg_a, REG_TEMP);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_AND_IMM(block, REG_TEMP, src_reg_b, 0x0000ff00);
		host_arm_UADD8(block, dest_reg, src_reg_a, REG_TEMP);
	}
	else
                fatal("ADD %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;
}
static int codegen_ADD_IMM(codeblock_t *block, uop_t *uop)
{
//	host_arm_ADD_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
//        return 0;

        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size))
	{
		host_arm_ADD_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && src_reg == dest_reg)
	{
		host_arm_MOV_IMM(block, REG_TEMP, uop->imm_data << 8);
		host_arm_UADD8(block, dest_reg, src_reg, REG_TEMP);
	}
	else
                fatal("ADD_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;

}
static int codegen_ADD_LSHIFT(codeblock_t *block, uop_t *uop)
{
	host_arm_ADD_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, uop->imm_data);
        return 0;
}

static int codegen_AND(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg_a = HOST_REG_GET(uop->src_reg_a_real), src_reg_b = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size_a = IREG_GET_SIZE(uop->src_reg_a_real), src_size_b = IREG_GET_SIZE(uop->src_reg_b_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size_a) && REG_IS_L(src_size_b))
	{
		host_arm_AND_REG_LSL(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_MVN_REG_LSL(block, REG_TEMP, src_reg_b, 16);
		host_arm_BIC_REG_LSR(block, dest_reg, src_reg_a, REG_TEMP, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_MVN_REG_LSL(block, REG_TEMP, src_reg_b, 24);
		host_arm_BIC_REG_LSR(block, dest_reg, src_reg_a, REG_TEMP, 24);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_MVN_REG_LSL(block, REG_TEMP, src_reg_b, 16);
		host_arm_BIC_REG_LSR(block, dest_reg, src_reg_a, REG_TEMP, 24);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_MVN_REG_LSL(block, REG_TEMP, src_reg_b, 8);
		host_arm_AND_IMM(block, REG_TEMP, REG_TEMP, 0x0000ff00);
		host_arm_BIC_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_MVN_REG_LSL(block, REG_TEMP, src_reg_b, 0);
		host_arm_AND_IMM(block, REG_TEMP, REG_TEMP, 0x0000ff00);
		host_arm_BIC_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
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
		host_arm_AND_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size) && dest_reg == src_reg)
	{
		host_arm_AND_IMM(block, dest_reg, src_reg, uop->imm_data | 0xffff0000);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size) && dest_reg == src_reg)
	{
		host_arm_AND_IMM(block, dest_reg, src_reg, uop->imm_data | 0xffffff00);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && dest_reg == src_reg)
	{
		host_arm_AND_IMM(block, dest_reg, src_reg, (uop->imm_data << 8) | 0xffff00ff);
	}
	else
                fatal("AND_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
	host_arm_call(block, uop->p);
	host_arm_TST_REG(block, REG_R0, REG_R0);
	host_arm_BNE(block, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_CMP_IMM_JZ(codeblock_t *block, uop_t *uop)
{
        int src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_a_real);

        if (REG_IS_L(src_size))
        {
                host_arm_CMP_IMM(block, src_reg, uop->imm_data);
        }
        else
                fatal("CMP_IMM_JZ %02x\n", uop->src_reg_a_real);
        host_arm_BEQ(block, uop->p);

        return 0;
}

static int codegen_LOAD_FUNC_ARG0_IMM(codeblock_t *block, uop_t *uop)
{
	uint32_t arm_imm;

	if (get_arm_imm(uop->imm_data, &arm_imm))
		host_arm_MOV_IMM(block, REG_ARG0, uop->imm_data);
	else
	{
		int offset = add_literal(block, uop->imm_data);
		host_arm_LDR_IMM(block, REG_ARG0, REG_LITERAL, offset);
	}
        return 0;
}
static int codegen_LOAD_FUNC_ARG1_IMM(codeblock_t *block, uop_t *uop)
{
	uint32_t arm_imm;

	if (get_arm_imm(uop->imm_data, &arm_imm))
		host_arm_MOV_IMM(block, REG_ARG1, uop->imm_data);
	else
	{
		int offset = add_literal(block, uop->imm_data);
		host_arm_LDR_IMM(block, REG_ARG1, REG_LITERAL, offset);
	}
        return 0;
}
static int codegen_LOAD_FUNC_ARG2_IMM(codeblock_t *block, uop_t *uop)
{
	uint32_t arm_imm;

	if (get_arm_imm(uop->imm_data, &arm_imm))
		host_arm_MOV_IMM(block, REG_ARG2, uop->imm_data);
	else
	{
		int offset = add_literal(block, uop->imm_data);
		host_arm_LDR_IMM(block, REG_ARG2, REG_LITERAL, offset);
	}
        return 0;
}
static int codegen_LOAD_FUNC_ARG3_IMM(codeblock_t *block, uop_t *uop)
{
	uint32_t arm_imm;

	if (get_arm_imm(uop->imm_data, &arm_imm))
		host_arm_MOV_IMM(block, REG_ARG3, uop->imm_data);
	else
	{
		int offset = add_literal(block, uop->imm_data);
		host_arm_LDR_IMM(block, REG_ARG3, REG_LITERAL, offset);
	}
        return 0;
}

static int codegen_MEM_LOAD_ABS(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

	host_arm_ADD_IMM(block, REG_R0, seg_reg, uop->imm_data);
        if (REG_IS_B(dest_size) || REG_IS_BH(dest_size))
        {
                host_arm_BL(block, codegen_mem_load_byte);
        }
        else if (REG_IS_W(dest_size))
        {
                host_arm_BL(block, codegen_mem_load_word);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm_BL(block, codegen_mem_load_long);
        }
        else
                fatal("MEM_LOAD_ABS - %02x\n", uop->dest_reg_a_real);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);
        if (REG_IS_B(dest_size))
        {
		host_arm_BFI(block, dest_reg, REG_R0, 0, 8);
        }
	else if (REG_IS_BH(dest_size))
        {
		host_arm_BFI(block, dest_reg, REG_R0, 8, 8);
        }
        else if (REG_IS_W(dest_size))
        {
		host_arm_BFI(block, dest_reg, REG_R0, 0, 16);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm_MOV_REG(block, dest_reg, REG_R0);
        }

        return 0;
}

static int codegen_MEM_LOAD_REG(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real);

	host_arm_ADD_REG(block, REG_R0, seg_reg, addr_reg);
        if (REG_IS_B(dest_size) || REG_IS_BH(dest_size))
        {
                host_arm_BL(block, codegen_mem_load_byte);
        }
        else if (REG_IS_W(dest_size))
        {
                host_arm_BL(block, codegen_mem_load_word);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm_BL(block, codegen_mem_load_long);
        }
        else
                fatal("MEM_LOAD_REG - %02x\n", uop->dest_reg_a_real);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);
        if (REG_IS_B(dest_size))
        {
		host_arm_BFI(block, dest_reg, REG_R0, 0, 8);
        }
	else if (REG_IS_BH(dest_size))
        {
		host_arm_BFI(block, dest_reg, REG_R0, 8, 8);
        }
        else if (REG_IS_W(dest_size))
        {
		host_arm_BFI(block, dest_reg, REG_R0, 0, 16);
        }
        else if (REG_IS_L(dest_size))
        {
                host_arm_MOV_REG(block, dest_reg, REG_R0);
        }

        return 0;
}

static int codegen_MEM_STORE_ABS(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_b_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_b_real);

	host_arm_ADD_IMM(block, REG_R0, seg_reg, uop->imm_data);
        if (REG_IS_B(src_size))
        {
                host_arm_MOV_REG(block, REG_R1, src_reg);
                host_arm_BL(block, codegen_mem_store_byte);
        }
        else if (REG_IS_W(src_size))
        {
                host_arm_MOV_REG(block, REG_R1, src_reg);
                host_arm_BL(block, codegen_mem_store_word);
        }
        else if (REG_IS_L(src_size))
        {
                host_arm_MOV_REG(block, REG_R1, src_reg);
                host_arm_BL(block, codegen_mem_store_long);
        }
        else
                fatal("MEM_STORE_ABS - %02x\n", uop->src_reg_b_real);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MEM_STORE_REG(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real), src_reg = HOST_REG_GET(uop->src_reg_c_real);
        int src_size = IREG_GET_SIZE(uop->src_reg_c_real);

	host_arm_ADD_REG(block, REG_R0, seg_reg, addr_reg);
        if (REG_IS_B(src_size))
        {
                host_arm_MOV_REG(block, REG_R1, src_reg);
                host_arm_BL(block, codegen_mem_store_byte);
        }
        else if (REG_IS_BH(src_size))
        {
                host_arm_MOV_REG_LSR(block, REG_R1, src_reg, 8);
                host_arm_BL(block, codegen_mem_store_byte);
        }
        else if (REG_IS_W(src_size))
        {
                host_arm_MOV_REG(block, REG_R1, src_reg);
                host_arm_BL(block, codegen_mem_store_word);
        }
        else if (REG_IS_L(src_size))
        {
                host_arm_MOV_REG(block, REG_R1, src_reg);
                host_arm_BL(block, codegen_mem_store_long);
        }
        else
                fatal("MEM_STORE_REG - %02x\n", uop->src_reg_c_real);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MEM_STORE_IMM_8(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

	host_arm_ADD_REG(block, REG_R0, seg_reg, addr_reg);
        host_arm_MOV_IMM(block, REG_R1, uop->imm_data);
        host_arm_BL(block, codegen_mem_store_byte);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_IMM_16(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

	host_arm_ADD_REG(block, REG_R0, seg_reg, addr_reg);
        host_arm_MOV_IMM(block, REG_R1, uop->imm_data);
        host_arm_BL(block, codegen_mem_store_word);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}
static int codegen_MEM_STORE_IMM_32(codeblock_t *block, uop_t *uop)
{
        int seg_reg = HOST_REG_GET(uop->src_reg_a_real), addr_reg = HOST_REG_GET(uop->src_reg_b_real);

	host_arm_ADD_REG(block, REG_R0, seg_reg, addr_reg);
        host_arm_MOV_IMM(block, REG_R1, uop->imm_data);
        host_arm_BL(block, codegen_mem_store_long);
        host_arm_TST_REG(block, REG_R1, REG_R1);
        host_arm_BNE(block, &block->data[BLOCK_EXIT_OFFSET]);

        return 0;
}

static int codegen_MOV(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size))
	{
		host_arm_MOV_REG_LSL(block, dest_reg, src_reg, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm_BFI(block, dest_reg, src_reg, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm_BFI(block, dest_reg, src_reg, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_B(src_size))
	{
		host_arm_BFI(block, dest_reg, src_reg, 8, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size))
	{
		host_arm_MOV_REG_LSR(block, REG_TEMP, src_reg, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size))
	{
		host_arm_MOV_REG_LSR(block, REG_TEMP, src_reg, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 8, 8);
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
		uint32_t arm_imm;

		if (get_arm_imm(uop->imm_data, &arm_imm))
			host_arm_MOV_IMM(block, dest_reg, uop->imm_data);
		else
		{
			int offset = add_literal(block, uop->imm_data);
			host_arm_LDR_IMM(block, dest_reg, REG_LITERAL, offset);
		}
	}
	else if (REG_IS_W(dest_size))
	{
		host_arm_MOVW_IMM(block, REG_TEMP, uop->imm_data);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size))
	{
		host_arm_AND_IMM(block, dest_reg, dest_reg, ~0x000000ff);
		host_arm_ORR_IMM(block, dest_reg, dest_reg, uop->imm_data);
	}
	else if (REG_IS_BH(dest_size))
	{
		host_arm_AND_IMM(block, dest_reg, dest_reg, ~0x0000ff00);
		host_arm_ORR_IMM(block, dest_reg, dest_reg, uop->imm_data << 8);
	}
	else
                fatal("MOV_IMM %02x\n", uop->dest_reg_a_real);

        return 0;
}
static int codegen_MOV_PTR(codeblock_t *block, uop_t *uop)
{
	int offset = add_literal(block, (uintptr_t)uop->p);
	host_arm_LDR_IMM(block, uop->dest_reg_a_real, REG_LITERAL, offset);

        return 0;
}

static int codegen_MOVZX(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

	if (REG_IS_L(dest_size) && REG_IS_B(src_size))
	{
		host_arm_UXTB(block, dest_reg, src_reg, 0);
	}
	else if (REG_IS_L(dest_size) && REG_IS_BH(src_size))
	{
		host_arm_UXTB(block, dest_reg, src_reg, 8);
	}
	else if (REG_IS_L(dest_size) && REG_IS_W(src_size))
	{
		host_arm_UXTH(block, dest_reg, src_reg, 0);
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
		host_arm_ORR_REG_LSL(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTH(block, REG_TEMP, src_reg_b, 0);
		host_arm_ORR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 0);
		host_arm_ORR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 8);
		host_arm_ORR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 0);
		host_arm_ORR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 8);
		host_arm_ORR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 8);
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
		host_arm_ORR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size) && dest_reg == src_reg)
	{
		host_arm_ORR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size) && dest_reg == src_reg)
	{
		host_arm_ORR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && dest_reg == src_reg)
	{
		host_arm_ORR_IMM(block, dest_reg, src_reg, uop->imm_data << 8);
	}
	else
                fatal("OR_IMM %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real);

        return 0;
}

static int codegen_STORE_PTR_IMM(codeblock_t *block, uop_t *uop)
{
	uint32_t arm_imm;

	if (get_arm_imm(uop->imm_data, &arm_imm))
		host_arm_MOV_IMM(block, REG_R0, uop->imm_data);
	else
	{
		int offset = add_literal(block, uop->imm_data);
		host_arm_LDR_IMM(block, REG_R0, REG_LITERAL, offset);
	}
	if (in_range(uop->p, &cpu_state))
		host_arm_STR_IMM(block, REG_R0, REG_CPUSTATE, (uintptr_t)uop->p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_STORE_PTR_IMM - not in range\n");

        return 0;
}
static int codegen_STORE_PTR_IMM_8(codeblock_t *block, uop_t *uop)
{
	host_arm_MOV_IMM(block, REG_R0, uop->imm_data);
	if (in_range(uop->p, &cpu_state))
		host_arm_STRB_IMM(block, REG_R0, REG_CPUSTATE, (uintptr_t)uop->p - (uintptr_t)&cpu_state);
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
		host_arm_SUB_REG_LSL(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b))
	{
		host_arm_SUB_REG_LSL(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm_SUB_REG_LSL(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm_SUB_REG_LSR(block, REG_TEMP, src_reg_a, src_reg_b, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm_RSB_REG_LSR(block, REG_TEMP, src_reg_b, src_reg_a, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm_SUB_REG_LSL(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b))
	{
		host_arm_RSB_REG_LSR(block, REG_TEMP, src_reg_b, src_reg_a, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b))
	{
		host_arm_SUB_REG_LSL(block, REG_TEMP, src_reg_a, src_reg_b, 0);
		host_arm_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 8, 8);
	}
	else
                fatal("SUB %02x %02x %02x\n", uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);

        return 0;

//	host_arm_SUB_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, 0);
//        return 0;
}
static int codegen_SUB_IMM(codeblock_t *block, uop_t *uop)
{
        int dest_reg = HOST_REG_GET(uop->dest_reg_a_real), src_reg = HOST_REG_GET(uop->src_reg_a_real);
        int dest_size = IREG_GET_SIZE(uop->dest_reg_a_real), src_size = IREG_GET_SIZE(uop->src_reg_a_real);

	if (REG_IS_L(dest_size) && REG_IS_L(src_size))
	{
		host_arm_SUB_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size))
	{
		host_arm_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size))
	{
		host_arm_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_B(dest_size) && REG_IS_BH(src_size))
	{
		host_arm_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data << 8);
		host_arm_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size))
	{
		host_arm_SUB_IMM(block, REG_TEMP, src_reg, uop->imm_data << 8);
		host_arm_MOV_REG_LSR(block, REG_TEMP, REG_TEMP, 8);
		host_arm_BFI(block, dest_reg, REG_TEMP, 8, 8);
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
		host_arm_EOR_REG_LSL(block, dest_reg, src_reg_a, src_reg_b, 0);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size_a) && REG_IS_W(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTH(block, REG_TEMP, src_reg_b, 0);
		host_arm_EOR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 0);
		host_arm_EOR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 8);
		host_arm_EOR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 0);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_B(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 0);
		host_arm_EOR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size_a) && REG_IS_BH(src_size_b) && dest_reg == src_reg_a)
	{
		host_arm_UXTB(block, REG_TEMP, src_reg_b, 8);
		host_arm_EOR_REG_LSL(block, dest_reg, src_reg_a, REG_TEMP, 8);
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
		host_arm_EOR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size) && dest_reg == src_reg)
	{
		host_arm_EOR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size) && dest_reg == src_reg)
	{
		host_arm_EOR_IMM(block, dest_reg, src_reg, uop->imm_data);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && dest_reg == src_reg)
	{
		host_arm_EOR_IMM(block, dest_reg, src_reg, uop->imm_data << 8);
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
	if (in_range(p, &cpu_state))
		host_arm_LDR_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_read_32 - not in range\n");
}

void codegen_direct_write_8(codeblock_t *block, void *p, int host_reg)
{
	if (in_range(p, &cpu_state))
		host_arm_STRB_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_8 - not in range\n");
}

void codegen_direct_write_32(codeblock_t *block, void *p, int host_reg)
{
	if (in_range(p, &cpu_state))
		host_arm_STR_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_32 - not in range\n");
}

void codegen_direct_write_ptr(codeblock_t *block, void *p, int host_reg)
{
	if (in_range(p, &cpu_state))
		host_arm_STR_IMM(block, host_reg, REG_CPUSTATE, (uintptr_t)p - (uintptr_t)&cpu_state);
	else
		fatal("codegen_direct_write_ptr - not in range\n");
}

void codegen_direct_read_32_stack(codeblock_t *block, int host_reg, int stack_offset)
{
	if (stack_offset >= 0 && stack_offset < 4096)
		host_arm_LDR_IMM(block, host_reg, REG_SP, stack_offset);
	else
		fatal("codegen_direct_read_32 - not in range\n");
}

void codegen_direct_write_32_stack(codeblock_t *block, int stack_offset, int host_reg)
{
	if (stack_offset >= 0 && stack_offset < 4096)
		host_arm_STR_IMM(block, host_reg, REG_SP, stack_offset);
	else
		fatal("codegen_direct_write_32 - not in range\n");
}

#endif
