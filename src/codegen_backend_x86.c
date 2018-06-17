#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

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
        addbyte(0x83); /*SUBL $16,%esp*/
        addbyte(0xEC);
        addbyte(0x10);
        addbyte(0xBD); /*MOVL EBP, &cpu_state*/
        addlong(((uintptr_t)&cpu_state) + 128);
}

void codegen_backend_epilogue(codeblock_t *block)
{
        addbyte(0x83); /*ADDL $16,%esp*/
        addbyte(0xC4);
        addbyte(0x10);
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
        addbyte(0x83); /*ADDL $16,%esp*/
        addbyte(0xC4);
        addbyte(0x10);
        addbyte(0x5f); /*POP EDI*/
        addbyte(0x5e); /*POP ESI*/
        addbyte(0x5d); /*POP EBP*/
        addbyte(0x5b); /*POP EDX*/
        addbyte(0xC3); /*RET*/
}

#endif
