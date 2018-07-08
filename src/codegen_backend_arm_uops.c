#ifdef __ARM_EABI__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm_defs.h"
#include "codegen_ir_defs.h"

static inline void codegen_addlong(codeblock_t *block, uint32_t val)
{
        *(uint32_t *)&block->data[block_pos] = val;
        block_pos += 4;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addlong over! %i\n", block_pos);
                CPU_BLOCK_END();
        }
}

#define Rm(x) (x)
#define Rd(x) ((x) << 12)
#define Rn(x) ((x) << 16)

#define DATA_OFFSET_UP   (1 << 23)
#define DATA_OFFSET_DOWN (0 << 23)

#define COND_SHIFT 28
#define COND_NE (0x1 << COND_SHIFT)
#define COND_AL (0xe << COND_SHIFT)

#define OPCODE_SHIFT 20
#define OPCODE_ADD_IMM  (0x28 << OPCODE_SHIFT)
#define OPCODE_ADD_REG  (0x08 << OPCODE_SHIFT)
#define OPCODE_AND_IMM  (0x20 << OPCODE_SHIFT)
#define OPCODE_AND_REG  (0x00 << OPCODE_SHIFT)
#define OPCODE_B        (0xa0 << OPCODE_SHIFT)
#define OPCODE_BIC_IMM  (0x3c << OPCODE_SHIFT)
#define OPCODE_BIC_REG  (0x1c << OPCODE_SHIFT)
#define OPCODE_EOR_IMM  (0x22 << OPCODE_SHIFT)
#define OPCODE_EOR_REG  (0x02 << OPCODE_SHIFT)
#define OPCODE_LDMIA_WB (0x8b << OPCODE_SHIFT)
#define OPCODE_LDR_IMM  (0x51 << OPCODE_SHIFT)
#define OPCODE_MOV_IMM  (0x3a << OPCODE_SHIFT)
#define OPCODE_MOV_REG  (0x1a << OPCODE_SHIFT)
#define OPCODE_ORR_IMM  (0x38 << OPCODE_SHIFT)
#define OPCODE_ORR_REG  (0x18 << OPCODE_SHIFT)
#define OPCODE_STMDB_WB (0x92 << OPCODE_SHIFT)
#define OPCODE_STR_IMM  (0x50 << OPCODE_SHIFT)
#define OPCODE_STRB_IMM (0x54 << OPCODE_SHIFT)
#define OPCODE_SUB_IMM  (0x24 << OPCODE_SHIFT)
#define OPCODE_SUB_REG  (0x04 << OPCODE_SHIFT)
#define OPCODE_TST_REG  (0x11 << OPCODE_SHIFT)

#define OPCODE_BLX 0xe12fff30

#define B_OFFSET(x) (((x) >> 2) & 0xffffff)

#define SHIFT_TYPE_SHIFT 5
#define SHIFT_TYPE_LSL (0 << SHIFT_TYPE_SHIFT)
#define SHIFT_TYPE_LSR (1 << SHIFT_TYPE_SHIFT)
#define SHIFT_TYPE_ASR (2 << SHIFT_TYPE_SHIFT)
#define SHIFT_TYPE_ROR (3 << SHIFT_TYPE_SHIFT)

#define SHIFT_TYPE_IMM (0 << 4)
#define SHIFT_TYPE_REG (1 << 4)

#define SHIFT_IMM_SHIFT 7
#define SHIFT_LSL_IMM(x) (SHIFT_TYPE_LSL | SHIFT_TYPE_IMM | ((x) << SHIFT_IMM_SHIFT))

static int literal_offset = 0;
void codegen_reset_literal_pool(codeblock_t *block)
{
	literal_offset = 0;
}

int add_literal(codeblock_t *block, uint32_t data)
{
	int c;

	if (literal_offset >= 4096)
		fatal("add_literal - literal pool full\n");

	/*Search for pre-existing value*/
	for (c = 0; c < literal_offset; c += 4)
	{
		if (*(uint32_t *)&block->data[ARM_LITERAL_POOL_OFFSET + c] == data)
			return c;
	}

	*(uint32_t *)&block->data[ARM_LITERAL_POOL_OFFSET + literal_offset] = data;

	literal_offset += 4;

	return literal_offset - 4;
}


static inline uint32_t arm_data_offset(int offset)
{
	if (offset < -0xffc || offset > 0xffc)
		fatal("arm_data_offset out of range - %i\n", offset);

	if (offset >= 0)
		return offset | DATA_OFFSET_UP;
	return (-offset) | DATA_OFFSET_DOWN;
}

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

void host_arm_ADD_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_AND_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_EOR_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_ORR_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_SUB_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_ADD_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm)
{
	uint32_t arm_imm;

	if ((int32_t)imm < 0 && imm != 0x80000000)
	{
		host_arm_SUB_IMM(block, dst_reg, src_reg, -(int32_t)imm);
	}
	else if (get_arm_imm(imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_ADD_IMM | Rd(dst_reg) | Rn(src_reg) | arm_imm);
	}
	else
	{
		int offset = add_literal(block, imm);
		host_arm_LDR_IMM(block, REG_TEMP, REG_LITERAL, offset);
		host_arm_ADD_REG_LSL(block, dst_reg, src_reg, REG_TEMP, 0);
	}
}

void host_arm_ADD_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift)
{
	codegen_addlong(block, COND_AL | OPCODE_ADD_REG | Rd(dst_reg) | Rn(src_reg_n) | Rm(src_reg_m) | SHIFT_LSL_IMM(shift));
}

void host_arm_AND_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm)
{
	uint32_t arm_imm;

	if (get_arm_imm(imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_AND_IMM | Rd(dst_reg) | Rn(src_reg) | arm_imm);
	}
	else if (get_arm_imm(~imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_BIC_IMM | Rd(dst_reg) | Rn(src_reg) | arm_imm);
	}
	else
	{
		int offset = add_literal(block, imm);
		host_arm_LDR_IMM(block, REG_TEMP, REG_LITERAL, offset);
		host_arm_AND_REG_LSL(block, dst_reg, src_reg, REG_TEMP, 0);
	}
}

void host_arm_AND_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift)
{
	codegen_addlong(block, COND_AL | OPCODE_AND_REG | Rd(dst_reg) | Rn(src_reg_n) | Rm(src_reg_m) | SHIFT_LSL_IMM(shift));
}

void host_arm_BLX(codeblock_t *block, int addr_reg)
{
	codegen_addlong(block, OPCODE_BLX | Rm(addr_reg));
}

void host_arm_BNE(codeblock_t *block, uintptr_t dest_addr)
{
	uint32_t offset = (dest_addr - (uintptr_t)&block->data[block_pos]) - 8;

	if ((offset & 0xfe000000) && (offset & 0xfe000000) != 0xfe000000)
		fatal("host_arm_BNE - out of range %08x %i\n", offset, offset);

	codegen_addlong(block, COND_NE | OPCODE_B | B_OFFSET(offset));
}

void host_arm_EOR_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm)
{
	uint32_t arm_imm;

	if (get_arm_imm(imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_EOR_IMM | Rd(dst_reg) | Rn(src_reg) | arm_imm);
	}
	else
	{
		int offset = add_literal(block, imm);
		host_arm_LDR_IMM(block, REG_TEMP, REG_LITERAL, offset);
		host_arm_EOR_REG_LSL(block, dst_reg, src_reg, REG_TEMP, 0);
	}
}

void host_arm_EOR_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift)
{
	codegen_addlong(block, COND_AL | OPCODE_EOR_REG | Rd(dst_reg) | Rn(src_reg_n) | Rm(src_reg_m) | SHIFT_LSL_IMM(shift));
}

void host_arm_LDMIA_WB(codeblock_t *block, int addr_reg, uint32_t reg_mask)
{
	codegen_addlong(block, COND_AL | OPCODE_LDMIA_WB | Rn(addr_reg) | reg_mask);
}

void host_arm_LDR_IMM(codeblock_t *block, int dst_reg, int addr_reg, int offset)
{
	codegen_addlong(block, COND_AL | OPCODE_LDR_IMM | Rn(addr_reg) | Rd(dst_reg) | arm_data_offset(offset));
}

void host_arm_MOV_IMM(codeblock_t *block, int dst_reg, uint32_t imm)
{
	uint32_t arm_imm;

	if (get_arm_imm(imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_MOV_IMM | Rd(dst_reg) | arm_imm);
	}
	else
	{
		int offset = add_literal(block, imm);
		host_arm_LDR_IMM(block, dst_reg, REG_LITERAL, offset);
	}
}

void host_arm_MOV_REG_LSL(codeblock_t *block, int dst_reg, int src_reg, int shift)
{
	codegen_addlong(block, COND_AL | OPCODE_MOV_REG | Rd(dst_reg) | Rm(src_reg) | SHIFT_LSL_IMM(shift));
}

void host_arm_ORR_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm)
{
	uint32_t arm_imm;

	if (get_arm_imm(imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_ORR_IMM | Rd(dst_reg) | Rn(src_reg) | arm_imm);
	}
	else
	{
		int offset = add_literal(block, imm);
		host_arm_LDR_IMM(block, REG_TEMP, REG_LITERAL, offset);
		host_arm_ORR_REG_LSL(block, dst_reg, src_reg, REG_TEMP, 0);
	}
}

void host_arm_ORR_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift)
{
	codegen_addlong(block, COND_AL | OPCODE_ORR_REG | Rd(dst_reg) | Rn(src_reg_n) | Rm(src_reg_m) | SHIFT_LSL_IMM(shift));
}

void host_arm_STMDB_WB(codeblock_t *block, int addr_reg, uint32_t reg_mask)
{
	codegen_addlong(block, COND_AL | OPCODE_STMDB_WB | Rn(addr_reg) | reg_mask);
}

void host_arm_STR_IMM(codeblock_t *block, int src_reg, int addr_reg, int offset)
{
	codegen_addlong(block, COND_AL | OPCODE_STR_IMM | Rn(addr_reg) | Rd(src_reg) | arm_data_offset(offset));
}

void host_arm_STRB_IMM(codeblock_t *block, int src_reg, int addr_reg, int offset)
{
	codegen_addlong(block, COND_AL | OPCODE_STRB_IMM | Rn(addr_reg) | Rd(src_reg) | arm_data_offset(offset));
}

void host_arm_SUB_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm)
{
	uint32_t arm_imm;

	if ((int32_t)imm < 0 && imm != 0x80000000)
	{
		host_arm_ADD_IMM(block, dst_reg, src_reg, -(int32_t)imm);
	}
	else if (get_arm_imm(imm, &arm_imm))
	{
		codegen_addlong(block, COND_AL | OPCODE_SUB_IMM | Rd(dst_reg) | Rn(src_reg) | arm_imm);
	}
	else
	{
		int offset = add_literal(block, imm);
		host_arm_LDR_IMM(block, REG_TEMP, REG_LITERAL, offset);
		host_arm_SUB_REG_LSL(block, dst_reg, src_reg, REG_TEMP, 0);
	}
}

void host_arm_SUB_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift)
{
	codegen_addlong(block, COND_AL | OPCODE_SUB_REG | Rd(dst_reg) | Rn(src_reg_n) | Rm(src_reg_m) | SHIFT_LSL_IMM(shift));
}

void host_arm_TST_REG(codeblock_t *block, int src_reg1, int src_reg2)
{
	codegen_addlong(block, COND_AL | OPCODE_TST_REG | Rn(src_reg1) | Rm(src_reg2));
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

static int codegen_ADD(codeblock_t *block, uop_t *uop)
{
	host_arm_ADD_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, 0);
        return 0;
}
static int codegen_ADD_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm_ADD_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
        return 0;
}
static int codegen_ADD_LSHIFT(codeblock_t *block, uop_t *uop)
{
	host_arm_ADD_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, uop->imm_data);
        return 0;
}

static int codegen_AND(codeblock_t *block, uop_t *uop)
{
	host_arm_AND_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, 0);
        return 0;
}
static int codegen_AND_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm_AND_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
        return 0;
}

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
	host_arm_call(block, uop->p);
	host_arm_TST_REG(block, REG_R0, REG_R0);
	host_arm_BNE(block, (uintptr_t)&block->data[BLOCK_EXIT_OFFSET]);

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

static int codegen_MOV(codeblock_t *block, uop_t *uop)
{
	host_arm_MOV_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, 0);

        return 0;
}

static int codegen_MOV_IMM(codeblock_t *block, uop_t *uop)
{
	uint32_t arm_imm;

	if (get_arm_imm(uop->imm_data, &arm_imm))
		host_arm_MOV_IMM(block, uop->dest_reg_a_real, uop->imm_data);
	else
	{
		int offset = add_literal(block, uop->imm_data);
		host_arm_LDR_IMM(block, uop->dest_reg_a_real, REG_LITERAL, offset);
	}

        return 0;
}
static int codegen_MOV_PTR(codeblock_t *block, uop_t *uop)
{
	int offset = add_literal(block, (uintptr_t)uop->p);
	host_arm_LDR_IMM(block, uop->dest_reg_a_real, REG_LITERAL, offset);

        return 0;
}

static int codegen_OR(codeblock_t *block, uop_t *uop)
{
	host_arm_ORR_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, 0);
        return 0;
}
static int codegen_OR_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm_ORR_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
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
	host_arm_SUB_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, 0);
        return 0;
}
static int codegen_SUB_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm_SUB_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
        return 0;
}

static int codegen_XOR(codeblock_t *block, uop_t *uop)
{
	host_arm_EOR_REG_LSL(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real, 0);
        return 0;
}
static int codegen_XOR_IMM(codeblock_t *block, uop_t *uop)
{
	host_arm_EOR_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
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

        [UOP_MOV     & UOP_MASK] = codegen_MOV,
        [UOP_MOV_PTR & UOP_MASK] = codegen_MOV_PTR,
        [UOP_MOV_IMM & UOP_MASK] = codegen_MOV_IMM,
        
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
        [UOP_XOR_IMM & UOP_MASK] = codegen_XOR_IMM
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

#endif
