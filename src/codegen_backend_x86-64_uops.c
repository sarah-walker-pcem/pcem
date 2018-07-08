#ifdef __amd64__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_x86-64_defs.h"
#include "codegen_ir_defs.h"

#define STACK_ARG0 (0)
#define STACK_ARG1 (4)
#define STACK_ARG2 (8)
#define STACK_ARG3 (12)

#define RM_OP_ADD 0x00
#define RM_OP_OR  0x08
#define RM_OP_AND 0x20
#define RM_OP_SUB 0x28
#define RM_OP_XOR 0x30

static inline void codegen_addbyte(codeblock_t *block, uint8_t val)
{
        block->data[block_pos++] = val;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addbyte over! %i\n", block_pos);
//                CPU_BLOCK_END();
        }
}
static inline void codegen_addbyte2(codeblock_t *block, uint8_t vala, uint8_t valb)
{
        block->data[block_pos++] = vala;
        block->data[block_pos++] = valb;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addbyte2 over! %i\n", block_pos);
                CPU_BLOCK_END();
        }
}
static inline void codegen_addbyte3(codeblock_t *block, uint8_t vala, uint8_t valb, uint8_t valc)
{
        block->data[block_pos++] = vala;
        block->data[block_pos++] = valb;
        block->data[block_pos++] = valc;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addbyte3 over! %i\n", block_pos);
                CPU_BLOCK_END();
        }
}
static inline void codegen_addbyte4(codeblock_t *block, uint8_t vala, uint8_t valb, uint8_t valc, uint8_t vald)
{
        block->data[block_pos++] = vala;
        block->data[block_pos++] = valb;
        block->data[block_pos++] = valc;
        block->data[block_pos++] = vald;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addbyte4 over! %i\n", block_pos);
                CPU_BLOCK_END();
        }
}

static inline void codegen_addword(codeblock_t *block, uint16_t val)
{
        *(uint16_t *)&block->data[block_pos] = val;
        block_pos += 2;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addword over! %i\n", block_pos);
                CPU_BLOCK_END();
        }
}

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

static inline void codegen_addquad(codeblock_t *block, uint64_t val)
{
        *(uint64_t *)&block->data[block_pos] = val;
        block_pos += 8;
        if (block_pos >= BLOCK_MAX)
        {
                fatal("codegen_addquad over! %i\n", block_pos);
                CPU_BLOCK_END();
        }
}

static int is_imm8(uint32_t imm_data)
{
        if (imm_data <= 0x7f || imm_data >= 0xffffff80)
                return 1;
        return 0;
}

static inline void call(codeblock_t *block, uintptr_t func)
{
	uintptr_t diff = func - (uintptr_t)&block->data[block_pos + 5];

	if (diff >= -0x80000000 && diff < 0x7fffffff)
	{
	        codegen_addbyte(block, 0xE8); /*CALL*/
	        codegen_addlong(block, (uint32_t)diff);
	}
	else
	{
		codegen_addbyte2(block, 0x48, 0xb8); /*MOV RAX, func*/
		codegen_addquad(block, func);
		codegen_addbyte2(block, 0xff, 0xd0); /*CALL RAX*/
	}
}

static void host_x86_ADD32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data)
{
        if (dst_reg != src_reg || (dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_ADD32_REG_IMM - dst_reg != src_reg\n");

        if (is_imm8(imm_data))
        {
                codegen_addbyte3(block, 0x83, 0xc0 | RM_OP_ADD | (dst_reg & 7), imm_data & 0xff); /*ADD dst_reg, imm_data*/
        }
        else
        {
                if (dst_reg == REG_EAX)
                {
                        codegen_addbyte(block, 0x05); /*ADD EAX, imm_data*/
                        codegen_addlong(block, imm_data);
                }
                else
                {
                        codegen_addbyte2(block, 0x81, 0xc0 | RM_OP_ADD | (dst_reg & 7)); /*ADD dst_reg, imm_data*/
                        codegen_addlong(block, imm_data);
                }
        }
}
static void host_x86_ADD32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b)
{
        if (dst_reg != src_reg_a || (dst_reg & 8) || (src_reg_b & 8))
                fatal("host_x86_ADD32_REG_IMM - dst_reg != src_reg_a\n");

        codegen_addbyte2(block, 0x01, 0xc0 | (dst_reg & 7) | ((src_reg_b & 7) << 3)); /*ADD dst_reg, src_reg_b*/
}

static void host_x86_AND32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data)
{
        if (dst_reg != src_reg || (dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_AND32_REG_IMM - dst_reg != src_reg\n");

        if (is_imm8(imm_data))
        {
                codegen_addbyte3(block, 0x83, 0xc0 | RM_OP_AND | (dst_reg & 7), imm_data & 0xff); /*AND dst_reg, imm_data*/
        }
        else
        {
                if (dst_reg == REG_EAX)
                {
                        codegen_addbyte(block, 0x25); /*AND EAX, imm_data*/
                        codegen_addlong(block, imm_data);
                }
                else
                {
                        codegen_addbyte2(block, 0x81, 0xc0 | RM_OP_AND | (dst_reg & 7)); /*AND dst_reg, imm_data*/
                        codegen_addlong(block, imm_data);
                }
        }
}
static void host_x86_AND32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b)
{
        if (dst_reg != src_reg_a || (dst_reg & 8) || (src_reg_b & 8))
                fatal("host_x86_AND32_REG_IMM - dst_reg != src_reg_a\n");

        codegen_addbyte2(block, 0x21, 0xc0 | (dst_reg & 7) | ((src_reg_b & 7) << 3)); /*AND dst_reg, src_reg_b*/
}

static void host_x86_CALL(codeblock_t *block, void *p)
{
        call(block, (uintptr_t)p);
}

static void host_x86_JNZ(codeblock_t *block, void *p)
{
        codegen_addbyte2(block, 0x0f, 0x85); /*JNZ*/
        codegen_addlong(block, (uintptr_t)p - (uintptr_t)&block->data[block_pos + 4]);
}

static void host_x86_LEA_REG_REG_SHIFT(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b, int shift)
{
        if ((dst_reg & 8) || (src_reg_a & 8) || (src_reg_b & 8))
                fatal("host_x86_LEA_REG_REG_SHIFT - bad reg\n");

        codegen_addbyte3(block, 0x8d, 0x04 | ((dst_reg & 7) << 3),  /*LEA dst_reg, [Rsrc_reg_a + Rsrc_reg_b * (1 << shift)]*/
                         (shift << 6) | ((src_reg_b & 7) << 3) | (src_reg_a & 7));
}

static void host_x86_MOV8_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0xc6, 0x45, offset); /*MOVB offset[RBP], imm_data*/
                codegen_addbyte(block, imm_data);
        }
        else
        {
                if ((uintptr_t)p >> 32)
                        fatal("host_x86_MOV8_ABS_IMM - out of range %p\n", p);
                codegen_addbyte3(block, 0xc6, 0x04, 0x25); /*MOVB p, imm_data*/
                codegen_addlong(block, (uint32_t)(uintptr_t)p);
                codegen_addbyte(block, imm_data);
        }
}
static void host_x86_MOV32_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0xc7, 0x45, offset); /*MOV offset[RBP], imm_data*/
                codegen_addlong(block, imm_data);
        }
        else
        {
                if ((uintptr_t)p >> 32)
                        fatal("host_x86_MOV32_ABS_IMM - out of range %p\n", p);
                codegen_addbyte3(block, 0xc7, 0x04, 0x25); /*MOV p, imm_data*/
                codegen_addlong(block, (uint32_t)(uintptr_t)p);
                codegen_addlong(block, imm_data);
        }
}

static void host_x86_MOV8_ABS_REG(codeblock_t *block, void *p, int src_reg)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (src_reg & 8)
                fatal("host_x86_MOV8_ABS_REG - bad reg\n");

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0x88, 0x45 | ((src_reg & 7) << 3), offset); /*MOVB offset[RBP], src_reg*/
        }
        else
        {
                if ((uintptr_t)p >> 32)
                        fatal("host_x86_MOV8_ABS_REG - out of range %p\n", p);
                codegen_addbyte(block, 0x88); /*MOVB [p], src_reg*/
                codegen_addbyte(block, 0x05 | ((src_reg & 7) << 3));
                codegen_addlong(block, (uint32_t)(uintptr_t)p);
        }
}
static void host_x86_MOV32_ABS_REG(codeblock_t *block, void *p, int src_reg)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (src_reg & 8)
                fatal("host_x86_MOV32_ABS_REG - bad reg\n");

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0x89, 0x45 | ((src_reg & 7) << 3), offset); /*MOV offset[RBP], src_reg*/
        }
        else
        {
                if ((uintptr_t)p >> 32)
                        fatal("host_x86_MOV32_ABS_REG - out of range %p\n", p);
                codegen_addbyte(block, 0x89); /*MOV [p], src_reg*/
                codegen_addbyte(block, 0x05 | ((src_reg & 7) << 3));
                codegen_addlong(block, (uint32_t)(uintptr_t)p);
        }
}
static void host_x86_MOV64_ABS_REG(codeblock_t *block, void *p, int src_reg)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (src_reg & 8)
                fatal("host_x86_MOV64_ABS_REG - bad reg\n");

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte4(block, 0x48, 0x89, 0x45 | ((src_reg & 7) << 3), offset); /*MOV offset[RBP], src_reg*/
        }
        else
        {
                if ((uintptr_t)p >> 32)
                        fatal("host_x86_MOV64_ABS_REG - out of range %p\n", p);
                codegen_addbyte4(block, 0x48, 0x89, 0x04 | ((src_reg & 7) << 3), 0x25); /*MOV [p], src_reg*/
                codegen_addlong(block, (uint32_t)(uintptr_t)p);
        }
}

static void host_x86_MOV32_REG_ABS(codeblock_t *block, int dst_reg, void *p)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (dst_reg & 8)
                fatal("host_x86_MOV32_REG_ABS reg & 8\n");

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0x8b, 0x45 | ((dst_reg & 7) << 3), offset); /*MOV offset[RBP], src_reg*/
        }
        else
        {
                fatal("host_x86_MOV32_REG_ABS - out of range\n");
                codegen_addbyte(block, 0x8b); /*MOV [p], src_reg*/
                codegen_addbyte(block, 0x05 | ((dst_reg & 7) << 3));
                codegen_addlong(block, (uint32_t)(uintptr_t)p);
        }
}

static void host_x86_MOV32_REG_IMM(codeblock_t *block, int reg, uint32_t imm_data)
{
        if (reg & 8)
                fatal("host_x86_MOV32_REG_IMM reg & 8\n");
        codegen_addbyte(block, 0xb8 | (reg & 7)); /*MOV reg, imm_data*/
        codegen_addlong(block, imm_data);
}

static void host_x86_MOV64_REG_IMM(codeblock_t *block, int reg, uint64_t imm_data)
{
        if (reg & 8)
                fatal("host_x86_MOV64_REG_IMM reg & 8\n");
        codegen_addbyte2(block, 0x48, 0xb8 | (reg & 7)); /*MOVQ reg, imm_data*/
        codegen_addquad(block, imm_data);
}

static void host_x86_MOV32_REG_REG(codeblock_t *block, int dst_reg, int src_reg)
{
        if ((dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_MOV32_REG_REG - bad reg\n");

        codegen_addbyte2(block, 0x89, 0xc0 | (dst_reg & 7) | ((src_reg & 7) << 3));
}

static void host_x86_MOV32_STACK_IMM(codeblock_t *block, int32_t offset, uint32_t imm_data)
{
        if (!offset)
        {
                codegen_addbyte3(block, 0xc7, 0x04, 0x24); /*MOV [ESP], imm_data*/
                codegen_addlong(block, imm_data);
        }
        else if (offset >= -80 || offset < 0x80)
        {
                codegen_addbyte4(block, 0xc7, 0x44, 0x24, offset & 0xff); /*MOV offset[ESP], imm_data*/
                codegen_addlong(block, imm_data);
        }
        else
        {
                codegen_addbyte3(block, 0xc7, 0x84, 0x24); /*MOV offset[ESP], imm_data*/
                codegen_addlong(block, offset);
                codegen_addlong(block, imm_data);
        }
}

static void host_x86_OR32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data)
{
        if (dst_reg != src_reg || (dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_OR32_REG_IMM - dst_reg != src_reg\n");

        if (is_imm8(imm_data))
        {
                codegen_addbyte3(block, 0x83, 0xc0 | RM_OP_OR | (dst_reg & 7), imm_data & 0xff); /*OR dst_reg, imm_data*/
        }
        else
        {
                if (dst_reg == REG_EAX)
                {
                        codegen_addbyte(block, 0x0d); /*OR EAX, imm_data*/
                        codegen_addlong(block, imm_data);
                }
                else
                {
                        codegen_addbyte2(block, 0x81, 0xc0 | RM_OP_OR | (dst_reg & 7)); /*OR dst_reg, imm_data*/
                        codegen_addlong(block, imm_data);
                }
        }
}
static void host_x86_OR32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b)
{
        if (dst_reg != src_reg_a || (dst_reg & 8) || (src_reg_b & 8))
                fatal("host_x86_OR32_REG_IMM - dst_reg != src_reg_a\n");

        codegen_addbyte2(block, 0x09, 0xc0 | (dst_reg & 7) | ((src_reg_b & 7) << 3)); /*OR dst_reg, src_reg_b*/
}

static void host_x86_SUB32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data)
{
        if (dst_reg != src_reg || (dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_SUB32_REG_IMM - dst_reg != src_reg\n");

        if (is_imm8(imm_data))
        {
                codegen_addbyte3(block, 0x83, 0xc0 | RM_OP_SUB | (dst_reg & 7), imm_data & 0xff); /*SUB dst_reg, imm_data*/
        }
        else
        {
                if (dst_reg == REG_EAX)
                {
                        codegen_addbyte(block, 0x2d); /*SUB EAX, imm_data*/
                        codegen_addlong(block, imm_data);
                }
                else
                {
                        codegen_addbyte2(block, 0x81, 0xc0 | RM_OP_SUB | (dst_reg & 7)); /*SUB dst_reg, imm_data*/
                        codegen_addlong(block, imm_data);
                }
        }
}
static void host_x86_SUB32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b)
{
        if (dst_reg != src_reg_a || (dst_reg & 8) || (src_reg_b & 8))
                fatal("host_x86_ADD32_REG_IMM - dst_reg != src_reg_a\n");

        codegen_addbyte2(block, 0x29, 0xc0 | (dst_reg & 7) | ((src_reg_b & 7) << 3)); /*SUB dst_reg, src_reg_b*/
}

#define MODRM_MOD_REG(rm, reg) (0xc0 | reg | (rm << 3))

static void host_x86_TEST32_REG(codeblock_t *block, int src_reg, int dst_reg)
{
        if ((dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_TEST32_REG - bad reg\n");
        codegen_addbyte2(block, 0x85, MODRM_MOD_REG(dst_reg, src_reg)); /*TEST dst_host_reg, src_host_reg*/
}

static void host_x86_XOR32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data)
{
        if (dst_reg != src_reg || (dst_reg & 8) || (src_reg & 8))
                fatal("host_x86_XOR32_REG_IMM - dst_reg != src_reg\n");

        if (is_imm8(imm_data))
        {
                codegen_addbyte3(block, 0x83, 0xc0 | RM_OP_XOR | (dst_reg & 7), imm_data & 0xff); /*XOR dst_reg, imm_data*/
        }
        else
        {
                if (dst_reg == REG_EAX)
                {
                        codegen_addbyte(block, 0x35); /*XOR EAX, imm_data*/
                        codegen_addlong(block, imm_data);
                }
                else
                {
                        codegen_addbyte2(block, 0x81, 0xc0 | RM_OP_XOR | (dst_reg & 7)); /*XOR dst_reg, imm_data*/
                        codegen_addlong(block, imm_data);
                }
        }
}
static void host_x86_XOR32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b)
{
        if (dst_reg != src_reg_a || (dst_reg & 8) || (src_reg_b & 8))
                fatal("host_x86_XOR32_REG_IMM - dst_reg != src_reg_a\n");

        codegen_addbyte2(block, 0x31, 0xc0 | (dst_reg & 7) | ((src_reg_b & 7) << 3)); /*XOR dst_reg, src_reg_b*/
}


static int codegen_ADD(codeblock_t *block, uop_t *uop)
{
        host_x86_ADD32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);
        return 0;
}

static int codegen_ADD_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_ADD32_REG_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
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
        host_x86_AND32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);
        return 0;
}

static int codegen_AND_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_AND32_REG_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
        return 0;
}

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
        host_x86_CALL(block, uop->p);
        host_x86_TEST32_REG(block, REG_EAX, REG_EAX);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);

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

static int codegen_MOV(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real);
        return 0;
}
static int codegen_MOV_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_REG_IMM(block, uop->dest_reg_a_real, uop->imm_data);
        return 0;
}
static int codegen_MOV_PTR(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV64_REG_IMM(block, uop->dest_reg_a_real, (uint64_t)uop->p);
        return 0;
}

static int codegen_OR(codeblock_t *block, uop_t *uop)
{
        host_x86_OR32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);
        return 0;
}
static int codegen_OR_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_OR32_REG_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
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
        if (uop->dest_reg_a_real != uop->src_reg_a_real)
                host_x86_MOV32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real);

        host_x86_SUB32_REG_REG(block, uop->dest_reg_a_real, uop->dest_reg_a_real, uop->src_reg_b_real);
        return 0;
}
static int codegen_SUB_IMM(codeblock_t *block, uop_t *uop)
{
        if (uop->dest_reg_a_real != uop->src_reg_a_real)
                host_x86_MOV32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real);

        host_x86_SUB32_REG_IMM(block, uop->dest_reg_a_real, uop->dest_reg_a_real, uop->imm_data);
        return 0;
}

static int codegen_XOR(codeblock_t *block, uop_t *uop)
{
        host_x86_XOR32_REG_REG(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->src_reg_b_real);
        return 0;
}
static int codegen_XOR_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_XOR32_REG_IMM(block, uop->dest_reg_a_real, uop->src_reg_a_real, uop->imm_data);
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
