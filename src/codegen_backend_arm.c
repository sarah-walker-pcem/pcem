#ifdef __ARM_EABI__

#include <stdlib.h>
#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm_defs.h"
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
        REG_R4,
        REG_R5,
        REG_R6,
};

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
	if (!codeblock)
		fatal("codeblock failed to alloc - %i\n", (BLOCK_SIZE+1) * sizeof(codeblock_t));
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

/*R11 - literal pool
  R10 - cpu_state*/
void codegen_backend_prologue(codeblock_t *block)
{
	int offset;

	codegen_reset_literal_pool(block);

        block_pos = 0;

        block_pos = BLOCK_GPF_OFFSET;
	host_arm_MOV_IMM(block, REG_R0, 0);
	host_arm_MOV_IMM(block, REG_R1, 0);
	host_arm_call(block, x86gpf);
	while (block_pos != BLOCK_EXIT_OFFSET)
		host_arm_nop(block);

        block_pos = BLOCK_EXIT_OFFSET; /*Exit code*/
	host_arm_ADD_IMM(block, REG_SP, REG_SP, 0x10);
	host_arm_LDMIA_WB(block, REG_SP, REG_MASK_LOCAL | REG_MASK_PC);
	while (block_pos != BLOCK_START)
		host_arm_nop(block);

	/*Entry code*/

	host_arm_STMDB_WB(block, REG_SP, REG_MASK_LOCAL | REG_MASK_LR);
	host_arm_SUB_IMM(block, REG_SP, REG_SP, 0x10);
	host_arm_ADD_IMM(block, REG_LITERAL, REG_PC, ARM_LITERAL_POOL_OFFSET);
	host_arm_SUB_IMM(block, REG_LITERAL, REG_LITERAL, 16 + BLOCK_START);
	offset = add_literal(block, (uintptr_t)&cpu_state);
	host_arm_LDR_IMM(block, REG_CPUSTATE, REG_R11, offset);
}

void codegen_backend_epilogue(codeblock_t *block)
{
	host_arm_ADD_IMM(block, REG_SP, REG_SP, 0x10);
	host_arm_LDMIA_WB(block, REG_SP, REG_MASK_LOCAL | REG_MASK_PC);

        if (block_pos > ARM_LITERAL_POOL_OFFSET)
                fatal("Over limit!\n");



	__clear_cache(&block->data[0], &block->data[block_pos]);
}

#endif
