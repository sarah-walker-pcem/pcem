#ifdef __amd64__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_x86-64_defs.h"
#include "codegen_backend_x86-64_ops.h"
#include "codegen_reg.h"
#include "x86.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <windows.h>
#endif

void *codegen_mem_load_byte;
void *codegen_mem_load_word;
void *codegen_mem_load_long;
void *codegen_mem_load_quad;
void *codegen_mem_load_single;
void *codegen_mem_load_double;

void *codegen_mem_store_byte;
void *codegen_mem_store_word;
void *codegen_mem_store_long;
void *codegen_mem_store_quad;
void *codegen_mem_store_single;
void *codegen_mem_store_double;

int codegen_host_reg_list[CODEGEN_HOST_REGS] =
{
        REG_EAX,
        REG_EBX,
        REG_EDX
};

int codegen_host_fp_reg_list[CODEGEN_HOST_FP_REGS] =
{
        REG_XMM0,
        REG_XMM1,
        REG_XMM2,
        REG_XMM3,
        REG_XMM4,
        REG_XMM5,
        REG_XMM6
};

static void *mem_abrt_rout;

static void build_load_routine(codeblock_t *block, int size, int is_float)
{
        uint8_t *branch_offset;
        uint8_t *misaligned_offset;

        /*In - ESI = address
          Out - ECX = data, ESI = abrt*/
        /*MOV ECX, ESI
          SHR ESI, 12
          MOV RSI, [readlookup2+ESI*4]
          CMP ESI, -1
          JNZ +
          MOVZX ECX, B[RSI+RCX]
          XOR ESI,ESI
          RET
        * PUSH EAX
          PUSH EDX
          PUSH ECX
          CALL readmemb386l
          POP ECX
          POP EDX
          POP EAX
          MOVZX ECX, AL
          RET
        */
        host_x86_MOV32_REG_REG(block, REG_ECX, REG_ESI);
        host_x86_SHR32_IMM(block, REG_ESI, 12);
        host_x86_MOV64_REG_IMM(block, REG_RDI, (uint64_t)(uintptr_t)readlookup2);
        host_x86_MOV64_REG_BASE_INDEX_SHIFT(block, REG_RSI, REG_RDI, REG_RSI, 3);
        if (size != 1)
        {
                host_x86_TEST32_REG_IMM(block, REG_ECX, size-1);
                misaligned_offset = host_x86_JNZ_short(block);
        }
        host_x86_CMP64_REG_IMM(block, REG_RSI, (uint32_t)-1);
        branch_offset = host_x86_JZ_short(block);
        if (size == 1 && !is_float)
                host_x86_MOVZX_BASE_INDEX_32_8(block, REG_ECX, REG_RSI, REG_RCX);
        else if (size == 2 && !is_float)
                host_x86_MOVZX_BASE_INDEX_32_16(block, REG_ECX, REG_RSI, REG_RCX);
        else if (size == 4 && !is_float)
                host_x86_MOV32_REG_BASE_INDEX(block, REG_ECX, REG_RSI, REG_RCX);
        else if (size == 4 && is_float)
                host_x86_CVTSS2SD_XREG_BASE_INDEX(block, REG_XMM_TEMP, REG_RSI, REG_RCX);
        else if (size == 8)
                host_x86_MOVQ_XREG_BASE_INDEX(block, REG_XMM_TEMP, REG_RSI, REG_RCX);
        else
                fatal("build_load_routine: size=%i\n", size);
        host_x86_XOR32_REG_REG(block, REG_ESI, REG_ESI, REG_ESI);
        host_x86_RET(block);

        *branch_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)branch_offset) - 1;
        if (size != 1)
                *misaligned_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)misaligned_offset) - 1;
        host_x86_PUSH(block, REG_RAX);
        host_x86_PUSH(block, REG_RDX);
#if WIN64
        host_x86_SUB64_REG_IMM(block, REG_RSP, 0x20);
        //host_x86_MOV32_REG_REG(block, REG_ECX, uop->imm_data);
#else
        host_x86_MOV32_REG_REG(block, REG_EDI, REG_ECX);
#endif
        if (size == 1 && !is_float)
        {
                host_x86_CALL(block, (void *)readmemb386l);
                host_x86_MOVZX_REG_32_8(block, REG_ECX, REG_EAX);
        }
        else if (size == 2 && !is_float)
        {
                host_x86_CALL(block, (void *)readmemwl);
                host_x86_MOVZX_REG_32_16(block, REG_ECX, REG_EAX);
        }
        else if (size == 4 && !is_float)
        {
                host_x86_CALL(block, (void *)readmemll);
                host_x86_MOV32_REG_REG(block, REG_ECX, REG_EAX);
        }
        else if (size == 4 && is_float)
        {
                host_x86_CALL(block, (void *)readmemll);
                host_x86_MOVD_XREG_REG(block, REG_XMM_TEMP, REG_EAX);
                host_x86_CVTSS2SD_XREG_XREG(block, REG_XMM_TEMP, REG_XMM_TEMP);
        }
        else if (size == 8)
        {
                host_x86_CALL(block, (void *)readmemql);
                host_x86_MOVQ_XREG_REG(block, REG_XMM_TEMP, REG_RAX);
        }
#if WIN64
        host_x86_ADD64_REG_IMM(block, REG_RSP, 0x20);
#endif
        host_x86_POP(block, REG_RDX);
        host_x86_POP(block, REG_RAX);
        host_x86_MOVZX_REG_ABS_32_8(block, REG_ESI, &cpu_state.abrt);
        host_x86_RET(block);
        block_pos = (block_pos + 63) & ~63;
}

static void build_store_routine(codeblock_t *block, int size, int is_float)
{
        uint8_t *branch_offset;
        uint8_t *misaligned_offset;

        /*In - ECX = data, ESI = address
          Out - ESI = abrt
          Corrupts EDI*/
        /*MOV EDI, ESI
          SHR ESI, 12
          MOV ESI, [writelookup2+ESI*4]
          CMP ESI, -1
          JNZ +
          MOV [RSI+RDI], ECX
          XOR ESI,ESI
          RET
        * PUSH EAX
          PUSH EDX
          PUSH ECX
          CALL writememb386l
          POP ECX
          POP EDX
          POP EAX
          MOVZX ECX, AL
          RET
        */
        host_x86_MOV32_REG_REG(block, REG_EDI, REG_ESI);
        host_x86_SHR32_IMM(block, REG_ESI, 12);
        host_x86_MOV64_REG_IMM(block, REG_R8, (uint64_t)(uintptr_t)writelookup2);
        host_x86_MOV64_REG_BASE_INDEX_SHIFT(block, REG_RSI, REG_R8, REG_RSI, 3);
        if (size != 1)
        {
                host_x86_TEST32_REG_IMM(block, REG_EDI, size-1);
                misaligned_offset = host_x86_JNZ_short(block);
        }
        host_x86_CMP64_REG_IMM(block, REG_RSI, (uint32_t)-1);
        branch_offset = host_x86_JZ_short(block);
        if (size == 1 && !is_float)
                host_x86_MOV8_BASE_INDEX_REG(block, REG_RSI, REG_RDI, REG_ECX);
        else if (size == 2 && !is_float)
                host_x86_MOV16_BASE_INDEX_REG(block, REG_RSI, REG_RDI, REG_ECX);
        else if (size == 4 && !is_float)
                host_x86_MOV32_BASE_INDEX_REG(block, REG_RSI, REG_RDI, REG_ECX);
        else if (size == 4 && is_float)
                host_x86_MOVD_BASE_INDEX_XREG(block, REG_RSI, REG_RDI, REG_XMM_TEMP);
        else if (size == 8)
                host_x86_MOVQ_BASE_INDEX_XREG(block, REG_RSI, REG_RDI, REG_XMM_TEMP);
        else
                fatal("build_store_routine: size=%i\n", size);
        host_x86_XOR32_REG_REG(block, REG_ESI, REG_ESI, REG_ESI);
        host_x86_RET(block);

        *branch_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)branch_offset) - 1;
        if (size != 1)
                *misaligned_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)misaligned_offset) - 1;
        host_x86_PUSH(block, REG_RAX);
        host_x86_PUSH(block, REG_RDX);
#if WIN64
        host_x86_SUB64_REG_IMM(block, REG_RSP, 0x20);
        if (size == 4 && is_float)
                host_x86_MOVD_REG_XREG(block, REG_EDX, REG_XMM_TEMP); //data
        else if (size == 8)
                host_x86_MOVQ_REG_XREG(block, REG_RDX, REG_XMM_TEMP); //data
        else
                host_x86_MOV32_REG_REG(block, REG_EDX, REG_ECX); //data
        host_x86_MOV32_REG_REG(block, REG_ECX, REG_EDI); //address
#else
        //host_x86_MOV32_REG_REG(block, REG_EDI, REG_ECX);  //address
        host_x86_MOV32_REG_REG(block, REG_ESI, REG_ECX); //data
#endif
        if (size == 1)
                host_x86_CALL(block, (void *)writememb386l);
        else if (size == 2)
                host_x86_CALL(block, (void *)writememwl);
        else if (size == 4)
                host_x86_CALL(block, (void *)writememll);
        else if (size == 8)
                host_x86_CALL(block, (void *)writememql);
#if WIN64
        host_x86_ADD64_REG_IMM(block, REG_RSP, 0x20);
#endif
        host_x86_POP(block, REG_RDX);
        host_x86_POP(block, REG_RAX);
        host_x86_MOVZX_REG_ABS_32_8(block, REG_ESI, &cpu_state.abrt);
        host_x86_RET(block);
        block_pos = (block_pos + 63) & ~63;
}

static void build_loadstore_routines(codeblock_t *block)
{
        codegen_mem_load_byte = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 1, 0);
        codegen_mem_load_word = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 2, 0);
        codegen_mem_load_long = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 4, 0);
        codegen_mem_load_quad = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 8, 0);
        codegen_mem_load_single = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 4, 1);
        codegen_mem_load_double = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 8, 1);

        codegen_mem_store_byte = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 1, 0);
        codegen_mem_store_word = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 2, 0);
        codegen_mem_store_long = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 4, 0);
        codegen_mem_store_quad = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 8, 0);
        codegen_mem_store_single = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 4, 1);
        codegen_mem_store_double = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 8, 1);
}

void codegen_backend_init()
{
        int c;
#if defined(__linux__) || defined(__APPLE__)
	void *start;
	size_t len;
	long pagesize = sysconf(_SC_PAGESIZE);
	long pagemask = ~(pagesize - 1);
#endif

#if defined WIN32 || defined _WIN32 || defined _WIN32
        codeblock_data = VirtualAlloc(NULL, BLOCK_SIZE * BLOCK_DATA_SIZE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
        codeblock_data = malloc(BLOCK_SIZE * BLOCK_DATA_SIZE);
#endif
        codeblock = malloc(BLOCK_SIZE * sizeof(codeblock_t));
        codeblock_hash = malloc(HASH_SIZE * sizeof(codeblock_t *));

        memset(codeblock, 0, BLOCK_SIZE * sizeof(codeblock_t));
        memset(codeblock_hash, 0, HASH_SIZE * sizeof(codeblock_t *));

        for (c = 0; c < BLOCK_SIZE; c++)
        {
                codeblock[c].data = &codeblock_data[c * BLOCK_DATA_SIZE];
                codeblock[c].pc = BLOCK_PC_INVALID;
        }

#if defined(__linux__) || defined(__APPLE__)
	start = (void *)((long)codeblock & pagemask);
	len = ((BLOCK_SIZE * sizeof(codeblock_t)) + pagesize) & pagemask;
	if (mprotect(start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
	{
		perror("mprotect");
		exit(-1);
	}
#endif
//        pclog("Codegen is %p\n", (void *)pages[0xfab12 >> 12].block);

        block_current = 0;
        block_pos = 0;
        build_loadstore_routines(&codeblock[block_current]);
//        fatal("Here\n");

        asm(
                "stmxcsr %0\n"
                : "=m" (cpu_state.old_fp_control)
        );
}

void codegen_set_rounding_mode(int mode)
{
        cpu_state.new_fp_control = (cpu_state.old_fp_control & ~0x6000) | (mode << 13);
}

static inline void call(codeblock_t *block, uintptr_t func)
{
	uintptr_t diff = func - (uintptr_t)&block->data[block_pos + 5];

	if (diff >= -0x80000000 && diff < 0x7fffffff)
	{
	        addbyte(0xE8); /*CALL*/
	        addlong((uint32_t)diff);
	}
	else
	{
		addbyte(0x48); /*MOV RAX, func*/
		addbyte(0xb8);
		addquad(func);
		addbyte(0xff); /*CALL RAX*/
		addbyte(0xd0);
	}
}

void codegen_backend_prologue(codeblock_t *block)
{
        block_pos = 0; /*Entry code*/
        addbyte(0x53); /*PUSH RBX*/
        addbyte(0x55); /*PUSH RBP*/
        addbyte(0x56); /*PUSH RSI*/
        addbyte(0x57); /*PUSH RDI*/
        addbyte(0x41); /*PUSH R12*/
        addbyte(0x54);
        addbyte(0x41); /*PUSH R13*/
        addbyte(0x55);
        addbyte(0x41); /*PUSH R14*/
        addbyte(0x56);
        addbyte(0x41); /*PUSH R15*/
        addbyte(0x57);
        addbyte(0x48); /*SUBL $40,%rsp*/
        addbyte(0x83);
        addbyte(0xEC);
        addbyte(0x38);
        addbyte(0x48); /*MOVL RBP, &cpu_state*/
        addbyte(0xBD);
        addquad(((uintptr_t)&cpu_state) + 128);
        if (block->flags & CODEBLOCK_HAS_FPU)
        {
                host_x86_MOV32_REG_ABS(block, REG_EAX, &cpu_state.TOP);
                host_x86_SUB32_REG_IMM(block, REG_EAX, REG_EAX, block->TOP);
                host_x86_MOV32_BASE_OFFSET_REG(block, REG_ESP, IREG_TOP_diff_stack_offset, REG_EAX);
        }
}

void codegen_backend_epilogue(codeblock_t *block)
{
        addbyte(0x48); /*ADDL $40,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x38);
        addbyte(0x41); /*POP R15*/
        addbyte(0x5f);
        addbyte(0x41); /*POP R14*/
        addbyte(0x5e);
        addbyte(0x41); /*POP R13*/
        addbyte(0x5d);
        addbyte(0x41); /*POP R12*/
        addbyte(0x5c);
        addbyte(0x5f); /*POP RDI*/
        addbyte(0x5e); /*POP RSI*/
        addbyte(0x5d); /*POP RBP*/
        addbyte(0x5b); /*POP RDX*/
        addbyte(0xC3); /*RET*/

        if (block_pos > BLOCK_GPF_OFFSET)
                fatal("Over limit!\n");

        block_pos = BLOCK_GPF_OFFSET;
#if WIN64
        addbyte(0x48); /*XOR RCX, RCX*/
        addbyte(0x31);
        addbyte(0xc9);
        addbyte(0x31); /*XOR EDX, EDX*/
        addbyte(0xd2);
#else
        addbyte(0x48); /*XOR RDI, RDI*/
        addbyte(0x31);
        addbyte(0xff);
        addbyte(0x31); /*XOR ESI, ESI*/
        addbyte(0xf6);
#endif
	call(block, (uintptr_t)x86gpf);
	while (block_pos < BLOCK_EXIT_OFFSET)
	       addbyte(0x90); /*NOP*/
        block_pos = BLOCK_EXIT_OFFSET; /*Exit code*/
        addbyte(0x48); /*ADDL $40,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x38);
        addbyte(0x41); /*POP R15*/
        addbyte(0x5f);
        addbyte(0x41); /*POP R14*/
        addbyte(0x5e);
        addbyte(0x41); /*POP R13*/
        addbyte(0x5d);
        addbyte(0x41); /*POP R12*/
        addbyte(0x5c);
        addbyte(0x5f); /*POP RDI*/
        addbyte(0x5e); /*POP RSI*/
        addbyte(0x5d); /*POP RBP*/
        addbyte(0x5b); /*POP RDX*/
        addbyte(0xC3); /*RET*/
}

#endif
