#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_x86_defs.h"
#include "codegen_ir_defs.h"

#define STACK_ARG0 (0)
#define STACK_ARG1 (4)
#define STACK_ARG2 (8)
#define STACK_ARG3 (12)

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
static void host_x86_CALL(codeblock_t *block, void *p)
{
        codegen_addbyte(block, 0xe8); /*CALL*/
        codegen_addlong(block, (uintptr_t)p - (uintptr_t)&block->data[block_pos + 4]);
}

static void host_x86_JNZ(codeblock_t *block, void *p)
{
        codegen_addbyte2(block, 0x0f, 0x85); /*JNZ*/
        codegen_addlong(block, (uintptr_t)p - (uintptr_t)&block->data[block_pos + 4]);
}

static void host_x86_MOV8_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0xc6, 0x45, offset); /*MOVB offset[EBP], imm_data*/
                codegen_addbyte(block, imm_data);
        }
        else
        {
                codegen_addbyte2(block, 0xc6, 0x05); /*MOVB p, imm_data*/
                codegen_addlong(block, (uint32_t)p);
                codegen_addbyte(block, imm_data);
        }
}
static void host_x86_MOV32_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0xc7, 0x45, offset); /*MOV offset[EBP], imm_data*/
                codegen_addlong(block, imm_data);
        }
        else
        {
                codegen_addbyte2(block, 0xc7, 0x05); /*MOV p, imm_data*/
                codegen_addlong(block, (uint32_t)p);
                codegen_addlong(block, imm_data);
        }
}

static void host_x86_MOV8_ABS_REG(codeblock_t *block, void *p, int src_reg)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0x88, 0x45 | (src_reg << 3), offset); /*MOVB offset[EBP], src_reg*/
        }
        else
        {
                codegen_addbyte(block, 0x88); /*MOVB [p], src_reg*/
                codegen_addbyte(block, 0x05 | (src_reg << 3));
                codegen_addlong(block, (uint32_t)p);
        }
}
static void host_x86_MOV32_ABS_REG(codeblock_t *block, void *p, int src_reg)
{
        int offset = (uintptr_t)p - (((uintptr_t)&cpu_state) + 128);

        if (offset >= -128 && offset < 127)
        {
                codegen_addbyte3(block, 0x89, 0x45 | (src_reg << 3), offset); /*MOV offset[EBP], src_reg*/
        }
        else
        {
                codegen_addbyte(block, 0x89); /*MOV [p], src_reg*/
                codegen_addbyte(block, 0x05 | (src_reg << 3));
                codegen_addlong(block, (uint32_t)p);
        }
}

static void host_x86_MOV32_REG_IMM(codeblock_t *block, int dst_reg, uint32_t imm_data)
{
        if (!imm_data)
        {
                codegen_addbyte2(block, 0x31, 0xc0 | dst_reg | (dst_reg << 3)); /*XOR dst_reg, dst_reg*/
        }
        else
        {
                codegen_addbyte(block, 0xb8 + dst_reg); /*MOV reg, imm_data*/
                codegen_addlong(block, imm_data);
        }
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

#define MODRM_MOD_REG(rm, reg) (0xc0 | reg | (rm << 3))

static void host_x86_TEST32_REG(codeblock_t *block, int src_host_reg, int dst_host_reg)
{
        codegen_addbyte2(block, 0x85, MODRM_MOD_REG(dst_host_reg, src_host_reg)); /*TEST dst_host_reg, src_host_reg*/
}

/*void codegen_debug()
{
        pclog(" %04x:%04x : %08x %08x %08x %08x\n", CS, cpu_state.pc, AX, BX, CX, DX);
}*/

static int codegen_CALL_INSTRUCTION_FUNC(codeblock_t *block, uop_t *uop)
{
        host_x86_CALL(block, uop->p);
        host_x86_TEST32_REG(block, REG_EAX, REG_EAX);
        host_x86_JNZ(block, &block->data[BLOCK_EXIT_OFFSET]);
//        host_x86_CALL(block, codegen_debug);

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

static int codegen_MOV_IMM(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_REG_IMM(block, uop->dest_reg_a_real, uop->imm_data);
        return 0;
}
static int codegen_MOV_PTR(codeblock_t *block, uop_t *uop)
{
        host_x86_MOV32_REG_IMM(block, uop->dest_reg_a_real, (uint32_t)uop->p);
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

const uOpFn uop_handlers[UOP_MAX] =
{
        [UOP_CALL_INSTRUCTION_FUNC & UOP_MASK] = codegen_CALL_INSTRUCTION_FUNC,

        [UOP_LOAD_FUNC_ARG_0_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG0_IMM,
        [UOP_LOAD_FUNC_ARG_1_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG1_IMM,
        [UOP_LOAD_FUNC_ARG_2_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG2_IMM,
        [UOP_LOAD_FUNC_ARG_3_IMM & UOP_MASK] = codegen_LOAD_FUNC_ARG3_IMM,

        [UOP_STORE_P_IMM & UOP_MASK] = codegen_STORE_PTR_IMM,
        [UOP_STORE_P_IMM_8 & UOP_MASK] = codegen_STORE_PTR_IMM_8,
        
        [UOP_MOV_PTR & UOP_MASK] = codegen_MOV_PTR,
        [UOP_MOV_IMM & UOP_MASK] = codegen_MOV_IMM
};

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
        host_x86_MOV32_ABS_REG(block, p, host_reg);
}

#endif
