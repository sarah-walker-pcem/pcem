#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_x86_defs.h"
#include "codegen_backend_x86_ops.h"
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

void *codegen_mem_store_byte;
void *codegen_mem_store_word;
void *codegen_mem_store_long;

int codegen_host_reg_list[CODEGEN_HOST_REGS] =
{
        REG_EAX,
        REG_EBX,
        REG_EDX
};

static void *mem_abrt_rout;

static void build_load_routine(codeblock_t *block, int size)
{
        uint8_t *branch_offset;
        uint8_t *misaligned_offset;
        
        /*In - ESI = address
          Out - ECX = data, ESI = abrt*/
        /*MOV ECX, ESI
          SHR ESI, 12
          MOV ESI, [readlookup2+ESI*4]
          CMP ESI, -1
          JNZ +
          MOVZX ECX, B[ESI+ECX]
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
        host_x86_MOV32_REG_ABS_INDEX_SHIFT(block, REG_ESI, readlookup2, REG_ESI, 2);
        if (size != 1)
        {
                host_x86_TEST32_REG_IMM(block, REG_ECX, size-1);
                misaligned_offset = host_x86_JNZ_short(block);
        }
        host_x86_CMP32_REG_IMM(block, REG_ESI, (uint32_t)-1);
        branch_offset = host_x86_JZ_short(block);
        if (size == 1)
                host_x86_MOVZX_BASE_INDEX_32_8(block, REG_ECX, REG_ESI, REG_ECX);
        else if (size == 2)
                host_x86_MOVZX_BASE_INDEX_32_16(block, REG_ECX, REG_ESI, REG_ECX);
        else if (size == 4)
                host_x86_MOV32_REG_BASE_INDEX(block, REG_ECX, REG_ESI, REG_ECX);
        else
                fatal("build_load_routine: size=%i\n", size);
        host_x86_XOR32_REG_REG(block, REG_ESI, REG_ESI, REG_ESI);
        host_x86_RET(block);
        
        *branch_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)branch_offset) - 1;
        if (size != 1)
                *misaligned_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)misaligned_offset) - 1;
        host_x86_PUSH(block, REG_EAX);
        host_x86_PUSH(block, REG_EDX);
        host_x86_PUSH(block, REG_ECX);
        if (size == 1)
                host_x86_CALL(block, (void *)readmemb386l);
        else if (size == 2)
                host_x86_CALL(block, (void *)readmemwl);
        else if (size == 4)
                host_x86_CALL(block, (void *)readmemll);
        host_x86_POP(block, REG_ECX);
        if (size == 1)
                host_x86_MOVZX_REG_32_8(block, REG_ECX, REG_EAX);
        else if (size == 2)
                host_x86_MOVZX_REG_32_16(block, REG_ECX, REG_EAX);
        else if (size == 4)
                host_x86_MOV32_REG_REG(block, REG_ECX, REG_EAX);
        host_x86_POP(block, REG_EDX);
        host_x86_POP(block, REG_EAX);
        host_x86_MOVZX_REG_ABS_32_8(block, REG_ESI, &cpu_state.abrt);
        host_x86_RET(block);
        block_pos = (block_pos + 63) & ~63;
}

static void build_store_routine(codeblock_t *block, int size)
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
          MOV [ESI+EDI], ECX
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
        host_x86_MOV32_REG_ABS_INDEX_SHIFT(block, REG_ESI, writelookup2, REG_ESI, 2);
        if (size != 1)
        {
                host_x86_TEST32_REG_IMM(block, REG_EDI, size-1);
                misaligned_offset = host_x86_JNZ_short(block);
        }
        host_x86_CMP32_REG_IMM(block, REG_ESI, (uint32_t)-1);
        branch_offset = host_x86_JZ_short(block);
        if (size == 1)
                host_x86_MOV8_BASE_INDEX_REG(block, REG_ESI, REG_EDI, REG_ECX);
        else if (size == 2)
                host_x86_MOV16_BASE_INDEX_REG(block, REG_ESI, REG_EDI, REG_ECX);
        else if (size == 4)
                host_x86_MOV32_BASE_INDEX_REG(block, REG_ESI, REG_EDI, REG_ECX);
        else
                fatal("build_store_routine: size=%i\n", size);
        host_x86_XOR32_REG_REG(block, REG_ESI, REG_ESI, REG_ESI);
        host_x86_RET(block);

        *branch_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)branch_offset) - 1;
        if (size != 1)
                *misaligned_offset = (uint8_t)((uintptr_t)&block->data[block_pos] - (uintptr_t)misaligned_offset) - 1;
        host_x86_PUSH(block, REG_EAX);
        host_x86_PUSH(block, REG_EDX);
        host_x86_PUSH(block, REG_ECX);
        host_x86_PUSH(block, REG_EDI);
        if (size == 1)
                host_x86_CALL(block, (void *)writememb386l);
        else if (size == 2)
                host_x86_CALL(block, (void *)writememwl);
        else if (size == 4)
                host_x86_CALL(block, (void *)writememll);
        host_x86_POP(block, REG_EDI);
        host_x86_POP(block, REG_ECX);
        host_x86_POP(block, REG_EDX);
        host_x86_POP(block, REG_EAX);
        host_x86_MOVZX_REG_ABS_32_8(block, REG_ESI, &cpu_state.abrt);
        host_x86_RET(block);
        block_pos = (block_pos + 63) & ~63;
}

static void build_loadstore_routines(codeblock_t *block)
{
        codegen_mem_load_byte = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 1);
        codegen_mem_load_word = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 2);
        codegen_mem_load_long = &codeblock[block_current].data[block_pos];
        build_load_routine(block, 4);

        codegen_mem_store_byte = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 1);
        codegen_mem_store_word = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 2);
        codegen_mem_store_long = &codeblock[block_current].data[block_pos];
        build_store_routine(block, 4);
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
        codeblock = VirtualAlloc(NULL, (BLOCK_SIZE+1) * sizeof(codeblock_t), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
        codeblock = malloc((BLOCK_SIZE+1) * sizeof(codeblock_t));
#endif
        codeblock_hash = malloc(HASH_SIZE * sizeof(codeblock_t *));

        memset(codeblock, 0, (BLOCK_SIZE+1) * sizeof(codeblock_t));
        memset(codeblock_hash, 0, HASH_SIZE * sizeof(codeblock_t *));

        for (c = 0; c < BLOCK_SIZE; c++)
                codeblock[c].pc = BLOCK_PC_INVALID;

#if defined(__linux__) || defined(__APPLE__)
	start = (void *)((long)codeblock & pagemask);
	len = (((BLOCK_SIZE+1) * sizeof(codeblock_t)) + pagesize) & pagemask;
	if (mprotect(start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
	{
		perror("mprotect");
		exit(-1);
	}
#endif
//        pclog("Codegen is %p\n", (void *)pages[0xfab12 >> 12].block);

        block_current = BLOCK_SIZE;
        block_pos = 0;
        mem_abrt_rout = &codeblock[block_current].data[block_pos];
        addbyte(0x83); /*ADDL $16+4,%esp*/
        addbyte(0xC4);
        addbyte(0x10+4);
        addbyte(0x5f); /*POP EDI*/
        addbyte(0x5e); /*POP ESI*/
        addbyte(0x5d); /*POP EBP*/
        addbyte(0x5b); /*POP EDX*/
        addbyte(0xC3); /*RET*/
        
        block_pos = 64;
        build_loadstore_routines(&codeblock[block_current]);
        //fatal("Here\n");

        asm(
                "fstcw %0\n"
                : "=m" (cpu_state.old_npxc)
        );
}

void codegen_backend_prologue(codeblock_t *block)
{
        block_pos = 0; /*Entry code*/
        addbyte(0x53); /*PUSH EBX*/
        addbyte(0x55); /*PUSH EBP*/
        addbyte(0x56); /*PUSH ESI*/
        addbyte(0x57); /*PUSH EDI*/
        addbyte(0x83); /*SUBL $32,%esp*/
        addbyte(0xEC);
        addbyte(0x20);
        addbyte(0xBD); /*MOVL EBP, &cpu_state*/
        addlong(((uintptr_t)&cpu_state) + 128);
}

void codegen_backend_epilogue(codeblock_t *block)
{
        addbyte(0x83); /*ADDL $32,%esp*/
        addbyte(0xC4);
        addbyte(0x20);
        addbyte(0x5f); /*POP EDI*/
        addbyte(0x5e); /*POP ESI*/
        addbyte(0x5d); /*POP EBP*/
        addbyte(0x5b); /*POP EDX*/
        addbyte(0xC3); /*RET*/

        if (block_pos > BLOCK_GPF_OFFSET)
                fatal("Over limit!\n");

        block_pos = BLOCK_GPF_OFFSET;
        addbyte(0xc7); /*MOV [ESP],0*/
        addbyte(0x04);
        addbyte(0x24);
        addlong(0);
        addbyte(0xc7); /*MOV [ESP+4],0*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addlong(0);
        addbyte(0xe8); /*CALL x86gpf*/
        addlong((uint32_t)x86gpf - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        block_pos = BLOCK_EXIT_OFFSET; /*Exit code*/
        addbyte(0x83); /*ADDL $32,%esp*/
        addbyte(0xC4);
        addbyte(0x20);
        addbyte(0x5f); /*POP EDI*/
        addbyte(0x5e); /*POP ESI*/
        addbyte(0x5d); /*POP EBP*/
        addbyte(0x5b); /*POP EDX*/
        addbyte(0xC3); /*RET*/
}

#endif
