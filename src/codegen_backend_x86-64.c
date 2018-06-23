#ifdef __amd64__

#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "x86.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <windows.h>
#endif

int codegen_host_reg_list[CODEGEN_HOST_REGS] =
{
        REG_EAX,
        REG_EBX,
        REG_EDX
};

static void *mem_abrt_rout;

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
        addbyte(0x28);
        addbyte(0x48); /*MOVL RBP, &cpu_state*/
        addbyte(0xBD);
        addquad(((uintptr_t)&cpu_state) + 128);
}

void codegen_backend_epilogue(codeblock_t *block)
{
        addbyte(0x48); /*ADDL $40,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x28);
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
        addbyte(0x28);
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
