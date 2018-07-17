#ifdef __aarch64__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm64_defs.h"
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

#define Rt(x)   (x)
#define Rd(x)   (x)
#define Rn(x)  ((x) << 5)
#define Rt2(x) ((x) << 10)
#define Rm(x)  ((x) << 16)

#define shift_imm6(x) ((x) << 10)

#define DATA_OFFSET_UP   (1 << 23)
#define DATA_OFFSET_DOWN (0 << 23)

#define COND_SHIFT 28
#define COND_NE (0x1 << COND_SHIFT)
#define COND_AL (0xe << COND_SHIFT)

#define OPCODE_SHIFT 24
#define OPCODE_ADD_IMM       (0x11 << OPCODE_SHIFT)
#define OPCODE_CBNZ          (0xb5 << OPCODE_SHIFT)
#define OPCODE_LDR_LITERAL_W (0x18 << OPCODE_SHIFT)
#define OPCODE_LDR_LITERAL_X (0x58 << OPCODE_SHIFT)
#define OPCODE_SUB_IMM       (0x51 << OPCODE_SHIFT)

#define OPCODE_AND_IMM       (0x024 << 23)
#define OPCODE_EOR_IMM       (0x0a4 << 23)
#define OPCODE_MOVK_W        (0x0e5 << 23)
#define OPCODE_MOVZ_W        (0x0a5 << 23)
#define OPCODE_ORR_IMM       (0x064 << 23)

#define OPCODE_BFI           (0x0cc << 22)
#define OPCODE_LDR_IMM_W     (0x2e5 << 22)
#define OPCODE_LDP_POSTIDX_X (0x2a3 << 22)
#define OPCODE_STP_PREIDX_X  (0x2a6 << 22)
#define OPCODE_STR_IMM_W     (0x2e4 << 22)
#define OPCODE_STR_IMM_Q     (0x3e4 << 22)
#define OPCODE_STRB_IMM      (0x0e4 << 22)
#define OPCODE_UBFX          (0x14c << 22)

#define OPCODE_ADD_LSL       (0x058 << 21)
#define OPCODE_ADD_LSR       (0x05a << 21)
#define OPCODE_AND_ASR       (0x054 << 21)
#define OPCODE_AND_LSL       (0x050 << 21)
#define OPCODE_AND_ROR       (0x056 << 21)
#define OPCODE_EOR_LSL       (0x250 << 21)
#define OPCODE_ORR_LSL       (0x150 << 21)
#define OPCODE_ORR_LSR       (0x152 << 21)
#define OPCODE_SUB_LSL       (0x258 << 21)
#define OPCODE_SUB_LSR       (0x25a << 21)

#define OPCODE_BLR           (0xd63f0000)
#define OPCODE_NOP           (0xd503201f)
#define OPCODE_RET           (0xd65f0000)

#define DATPROC_SHIFT(sh) (sh << 10)
#define DATPROC_IMM_SHIFT(sh) (sh << 22)
#define MOV_WIDE_HW(hw) (hw << 21)

#define IMM7_X(imm_data)  (((imm_data >> 3) & 0x7f) << 15)
#define IMM12(imm_data) ((imm_data) << 10)
#define IMM16(imm_data) ((imm_data) << 5)

#define IMMN(immn) ((immn) << 22)
#define IMMR(immr) ((immr) << 16)
#define IMMS(imms) ((imms) << 10)

#define OFFSET19(offset) (((offset >> 2) << 5) & 0x00ffffe0)

#define OFFSET12_B(offset)    (offset << 10)
#define OFFSET12_W(offset) ((offset >> 2) << 10)
#define OFFSET12_Q(offset) ((offset >> 3) << 10)

static int literal_offset = 0;
void codegen_reset_literal_pool(codeblock_t *block)
{
	literal_offset = 0;
}

/*Returns true if offset fits into 19 bits*/
static int offset_is_19bit(int offset)
{
	if (offset >= (1 << (18+2)))
		return 0;
	if (offset < -(1 << (18+2)))
		return 0;
	return 1;
}

#define in_range7_x(offset) (((offset) >= -0x200) && ((offset) < (0x200)) && !((offset) & 7))
#define in_range12_b(offset) (((offset) >= 0) && ((offset) < 0x1000))
#define in_range12_w(offset) (((offset) >= 0) && ((offset) < 0x4000) && !((offset) & 3))
#define in_range12_q(offset) (((offset) >= 0) && ((offset) < 0x8000) && !((offset) & 7))

int add_literal(codeblock_t *block, uint32_t data)
{
	if (literal_offset >= 4096)
		fatal("add_literal - literal pool full\n");

	*(uint32_t *)&block->data[ARM_LITERAL_POOL_OFFSET + literal_offset] = data;

	literal_offset += 4;

	return literal_offset - 4;
}

int add_literal_q(codeblock_t *block, uint64_t data)
{
	if (literal_offset & 4)
		literal_offset += 4;

	if (literal_offset >= 4096-4)
		fatal("add_literal - literal pool full\n");

	*(uint64_t *)&block->data[ARM_LITERAL_POOL_OFFSET + literal_offset] = data;

	literal_offset += 8;

	return literal_offset - 8;
}

static inline int imm_is_imm16(uint32_t imm_data)
{
	if (!(imm_data & 0xffff0000) || !(imm_data & 0x0000ffff))
		return 1;
	return 0;
}
static inline int imm_is_imm12(uint32_t imm_data)
{
	if (!(imm_data & 0xfffff000) || !(imm_data & 0xff000fff))
		return 1;
	return 0;
}

void host_arm64_MOV_REG(codeblock_t *block, int dst_reg, int src_m_reg, int shift);
void host_arm64_SUB_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);

void host_arm64_ADD_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data)
{
	if (!imm_data)
		host_arm64_MOV_REG(block, dst_reg, src_n_reg, 0);
	else if ((int32_t)imm_data < 0 && imm_data != 0x80000000)
	{
		host_arm64_SUB_IMM(block, dst_reg, src_n_reg, -(int32_t)imm_data);
	}
	else if (!(imm_data & 0xff000000))
	{
		if (imm_data & 0xfff)
		{
			codegen_addlong(block, OPCODE_ADD_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMM12(imm_data & 0xfff) | DATPROC_IMM_SHIFT(0));
			if (imm_data & 0xfff000)
				codegen_addlong(block, OPCODE_ADD_IMM | Rd(dst_reg) | Rn(dst_reg) | IMM12((imm_data >> 12) & 0xfff) | DATPROC_IMM_SHIFT(1));
		}
		else if (imm_data & 0xfff000)
			codegen_addlong(block, OPCODE_ADD_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMM12((imm_data >> 12) & 0xfff) | DATPROC_IMM_SHIFT(1));
	}
	else
	{
		host_arm64_MOVZ_IMM(block, REG_W16, imm_data & 0xffff);
		host_arm64_MOVK_IMM(block, REG_W16, imm_data & 0xffff0000);
		codegen_addlong(block, OPCODE_ADD_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(REG_W16) | DATPROC_SHIFT(0));
	}
}
void host_arm64_ADD_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_ADD_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}
void host_arm64_ADD_REG_LSR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_ADD_LSR | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}
//12001c00  S=0 R=7
void host_arm64_AND_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data)
{
	if (imm_data == 0xff) /*Quick hack until proper immediate generation is written */
	{
		codegen_addlong(block, OPCODE_AND_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(0) | IMMS(0x07));
	}
	else if (imm_data == 0xffff) /*Quick hack until proper immediate generation is written */
	{
		codegen_addlong(block, OPCODE_AND_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(0) | IMMS(0x0f));
	}
	else if (imm_data == 0xffffff00)
	{
		codegen_addlong(block, OPCODE_AND_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(24) | IMMS(0x17));
	}
	else if (imm_data == 0xffff00ff)
	{
		codegen_addlong(block, OPCODE_AND_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(16) | IMMS(0x17));
	}
	else if (imm_data == 0x0000ff00)
	{
		codegen_addlong(block, OPCODE_AND_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(24) | IMMS(0x07));
	}
	else
	{
		host_arm64_mov_imm(block, REG_W16, imm_data);
		codegen_addlong(block, OPCODE_AND_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(REG_W16) | DATPROC_SHIFT(0));
	}
}
void host_arm64_AND_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_AND_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}
void host_arm64_AND_REG_ASR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_AND_ASR | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}
void host_arm64_AND_REG_ROR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_AND_ROR | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}

void host_arm64_BFI(codeblock_t *block, int dst_reg, int src_reg, int lsb, int width)
{
	codegen_addlong(block, OPCODE_BFI | Rd(dst_reg) | Rn(src_reg) | IMMN(0) | IMMR((32 - lsb) & 31) | IMMS((width-1) & 31));
}

void host_arm64_BLR(codeblock_t *block, int addr_reg)
{
	codegen_addlong(block, OPCODE_BLR | Rn(addr_reg));
}

void host_arm64_CBNZ(codeblock_t *block, int reg, uintptr_t dest)
{
	int offset = dest - (uintptr_t)&block->data[block_pos];
	if (!offset_is_19bit(offset))
		fatal("host_arm64_CBNZ - offset out of range %x\n", offset);
	codegen_addlong(block, OPCODE_CBNZ | OFFSET19(offset) | Rt(reg));
}

void host_arm64_EOR_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data)
{
	if (imm_data == 0xffff) /*Quick hack until proper immediate generation is written */
	{
		codegen_addlong(block, OPCODE_EOR_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(0) | IMMS(0x0f));
	}
	else
	{
		host_arm64_mov_imm(block, REG_W16, imm_data);
		codegen_addlong(block, OPCODE_EOR_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(REG_W16) | DATPROC_SHIFT(0));
	}
}
void host_arm64_EOR_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_EOR_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}

void host_arm64_LDP_POSTIDX_X(codeblock_t *block, int src_reg1, int src_reg2, int base_reg, int offset)
{
	if (!in_range7_x(offset))
		fatal("host_arm64_LDP_POSTIDX out of range7 %i\n", offset);
	codegen_addlong(block, OPCODE_LDP_POSTIDX_X | IMM7_X(offset) | Rn(base_reg) | Rt(src_reg1) | Rt2(src_reg2));
}

void host_arm64_LDR_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset)
{
	if (!in_range12_w(offset))
		fatal("host_arm64_LDR_IMM_W out of range12 %i\n", offset);
	codegen_addlong(block, OPCODE_LDR_IMM_W | OFFSET12_W(offset) | Rn(base_reg) | Rt(dest_reg));
}

void host_arm64_LDR_LITERAL_W(codeblock_t *block, int dest_reg, int literal_offset)
{
	int offset = (ARM_LITERAL_POOL_OFFSET + literal_offset) - block_pos;

	if (!offset_is_19bit(offset))
		fatal("host_arm64_CBNZ - offset out of range %x\n", offset);

	codegen_addlong(block, OPCODE_LDR_LITERAL_W | OFFSET19(offset) | Rt(dest_reg));
}
void host_arm64_LDR_LITERAL_X(codeblock_t *block, int dest_reg, int literal_offset)
{
	int offset = (ARM_LITERAL_POOL_OFFSET + literal_offset) - block_pos;

	if (!offset_is_19bit(offset))
		fatal("host_arm64_CBNZ - offset out of range %x\n", offset);

	codegen_addlong(block, OPCODE_LDR_LITERAL_X | OFFSET19(offset) | Rt(dest_reg));
}

void host_arm64_MOV_REG(codeblock_t *block, int dst_reg, int src_m_reg, int shift)
{
	if (dst_reg != src_m_reg)
		codegen_addlong(block, OPCODE_ORR_LSL | Rd(dst_reg) | Rn(REG_WZR) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}

void host_arm64_MOV_REG_LSR(codeblock_t *block, int dst_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_ORR_LSR | Rd(dst_reg) | Rn(REG_WZR) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}

void host_arm64_MOVZ_IMM(codeblock_t *block, int reg, uint32_t imm_data)
{
	int hw;

	if (!imm_is_imm16(imm_data))
		fatal("MOVZ_IMM - imm not representable %08x\n", imm_data);

	hw = (imm_data & 0xffff0000) ? 1 : 0;
	if (hw)
		imm_data >>= 16;

	codegen_addlong(block, OPCODE_MOVZ_W | MOV_WIDE_HW(hw) | IMM16(imm_data) | Rd(reg));
}

void host_arm64_MOVK_IMM(codeblock_t *block, int reg, uint32_t imm_data)
{
	int hw;

	if (!imm_is_imm16(imm_data))
		fatal("MOVK_IMM - imm not representable %08x\n", imm_data);

	hw = (imm_data & 0xffff0000) ? 1 : 0;
	if (hw)
		imm_data >>= 16;

	codegen_addlong(block, OPCODE_MOVK_W | MOV_WIDE_HW(hw) | IMM16(imm_data) | Rd(reg));
}

void host_arm64_NOP(codeblock_t *block)
{
	codegen_addlong(block, OPCODE_NOP);
}

void host_arm64_ORR_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data)
{
	if (imm_data == 0xffff) /*Quick hack until proper immediate generation is written */
	{
		codegen_addlong(block, OPCODE_ORR_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(0) | IMMS(0x0f));
	}
	else if (imm_data == 0xffff0000)
	{
		codegen_addlong(block, OPCODE_ORR_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(16) | IMMS(0x0f));
	}
	else if (imm_data == 0xffffff00)
	{
		codegen_addlong(block, OPCODE_ORR_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(24) | IMMS(0x17));
	}
	else if (imm_data == 0xffff00ff)
	{
		codegen_addlong(block, OPCODE_ORR_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMMN(0) | IMMR(16) | IMMS(0x17));
	}
	else
	{
		host_arm64_mov_imm(block, REG_W16, imm_data);
		codegen_addlong(block, OPCODE_ORR_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(REG_W16) | DATPROC_SHIFT(0));
	}
}
void host_arm64_ORR_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_ORR_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}

void host_arm64_RET(codeblock_t *block, int reg)
{
	codegen_addlong(block, OPCODE_RET | Rn(reg));
}

void host_arm64_STP_PREIDX_X(codeblock_t *block, int src_reg1, int src_reg2, int base_reg, int offset)
{
	if (!in_range7_x(offset))
		fatal("host_arm64_STP_PREIDX out of range7 %i\n", offset);
	codegen_addlong(block, OPCODE_STP_PREIDX_X | IMM7_X(offset) | Rn(base_reg) | Rt(src_reg1) | Rt2(src_reg2));
}

void host_arm64_STR_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset)
{
	if (!in_range12_w(offset))
		fatal("host_arm64_STR_IMM_W out of range12 %i\n", offset);
	codegen_addlong(block, OPCODE_STR_IMM_W | OFFSET12_W(offset) | Rn(base_reg) | Rt(dest_reg));
}
void host_arm64_STR_IMM_Q(codeblock_t *block, int dest_reg, int base_reg, int offset)
{
	if (!in_range12_q(offset))
		fatal("host_arm64_STR_IMM_W out of range12 %i\n", offset);
	codegen_addlong(block, OPCODE_STR_IMM_Q | OFFSET12_Q(offset) | Rn(base_reg) | Rt(dest_reg));
}
void host_arm64_STRB_IMM(codeblock_t *block, int dest_reg, int base_reg, int offset)
{
	if (!in_range12_b(offset))
		fatal("host_arm64_STRB_IMM out of range12 %i\n", offset);
	codegen_addlong(block, OPCODE_STRB_IMM | OFFSET12_B(offset) | Rn(base_reg) | Rt(dest_reg));
}

void host_arm64_SUB_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data)
{
	if (!imm_data)
		host_arm64_MOV_REG(block, dst_reg, src_n_reg, 0);
	else if ((int32_t)imm_data < 0 && imm_data != 0x80000000)
	{
		host_arm64_ADD_IMM(block, dst_reg, src_n_reg, -(int32_t)imm_data);
	}
	else if (!(imm_data & 0xff000000))
	{
		if (imm_data & 0xfff)
		{
			codegen_addlong(block, OPCODE_SUB_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMM12(imm_data & 0xfff) | DATPROC_IMM_SHIFT(0));
			if (imm_data & 0xfff000)
				codegen_addlong(block, OPCODE_SUB_IMM | Rd(dst_reg) | Rn(dst_reg) | IMM12((imm_data >> 12) & 0xfff) | DATPROC_IMM_SHIFT(1));
		}
		else if (imm_data & 0xfff000)
			codegen_addlong(block, OPCODE_SUB_IMM | Rd(dst_reg) | Rn(src_n_reg) | IMM12((imm_data >> 12) & 0xfff) | DATPROC_IMM_SHIFT(1));
	}
	else
	{
		host_arm64_MOVZ_IMM(block, REG_W16, imm_data & 0xffff);
		host_arm64_MOVK_IMM(block, REG_W16, imm_data & 0xffff0000);
		codegen_addlong(block, OPCODE_SUB_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(REG_W16) | DATPROC_SHIFT(0));
	}
}
void host_arm64_SUB_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_SUB_LSL | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}
void host_arm64_SUB_REG_LSR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift)
{
	codegen_addlong(block, OPCODE_SUB_LSR | Rd(dst_reg) | Rn(src_n_reg) | Rm(src_m_reg) | DATPROC_SHIFT(shift));
}

void host_arm64_UBFX(codeblock_t *block, int dst_reg, int src_reg, int lsb, int width)
{
	codegen_addlong(block, OPCODE_UBFX | Rd(dst_reg) | Rn(src_reg) | IMMN(0) | IMMR(lsb) | IMMS((lsb+width-1) & 31));
}

void host_arm64_call(codeblock_t *block, void *dst_addr)
{
	int offset = add_literal_q(block, (uintptr_t)dst_addr);
	host_arm64_LDR_LITERAL_X(block, REG_X16, offset);
	host_arm64_BLR(block, REG_X16);
}

void host_arm64_mov_imm(codeblock_t *block, int reg, uint32_t imm_data)
{
	if (imm_is_imm16(imm_data))
		host_arm64_MOVZ_IMM(block, reg, imm_data);
	else
	{
		host_arm64_MOVZ_IMM(block, reg, imm_data & 0xffff);
		host_arm64_MOVK_IMM(block, reg, imm_data & 0xffff0000);
	}
}

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
	else if (REG_IS_W(dest_size) && REG_IS_W(src_size) && dest_reg == src_reg)
	{
		host_arm64_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 16);
	}
	else if (REG_IS_B(dest_size) && REG_IS_B(src_size) && dest_reg == src_reg)
	{
		host_arm64_ADD_IMM(block, REG_TEMP, src_reg, uop->imm_data);
		host_arm64_BFI(block, dest_reg, REG_TEMP, 0, 8);
	}
	else if (REG_IS_BH(dest_size) && REG_IS_BH(src_size) && dest_reg == src_reg)
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

        [UOP_LOAD_FUNC_ARG_0_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG0_IMM,
        [UOP_LOAD_FUNC_ARG_1_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG1_IMM,
        [UOP_LOAD_FUNC_ARG_2_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG2_IMM,
        [UOP_LOAD_FUNC_ARG_3_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG3_IMM,

        [UOP_STORE_P_IMM & UOP_MASK] = codegen_STORE_PTR_IMM,
        [UOP_STORE_P_IMM_8 & UOP_MASK] = codegen_STORE_PTR_IMM_8,

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
        [UOP_XOR_IMM & UOP_MASK] = codegen_XOR_IMM
};

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

#endif
