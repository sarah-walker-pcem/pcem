#ifdef __aarch64__

#include <stdlib.h>
#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm64_defs.h"
#include "x86.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <windows.h>
#endif

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
	host_arm64_mov_imm(block, REG_ARG0, 0);
	host_arm64_mov_imm(block, REG_ARG1, 0);
	host_arm64_call(block, x86gpf);
	while (block_pos != BLOCK_EXIT_OFFSET)
		host_arm64_NOP(block);

        block_pos = BLOCK_EXIT_OFFSET; /*Exit code*/
	host_arm64_LDP_POSTIDX_X(block, REG_X19, REG_X20, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X21, REG_X22, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X23, REG_X24, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X25, REG_X26, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X27, REG_X28, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X29, REG_X30, REG_SP, 16);
	host_arm64_RET(block, REG_X30);

	while (block_pos != BLOCK_START)
		host_arm64_NOP(block);

	/*Entry code*/

	host_arm64_STP_PREIDX_X(block, REG_X29, REG_X30, REG_SP, -16);
	host_arm64_STP_PREIDX_X(block, REG_X27, REG_X28, REG_SP, -16);
	host_arm64_STP_PREIDX_X(block, REG_X25, REG_X26, REG_SP, -16);
	host_arm64_STP_PREIDX_X(block, REG_X23, REG_X24, REG_SP, -16);
	host_arm64_STP_PREIDX_X(block, REG_X21, REG_X22, REG_SP, -16);
	host_arm64_STP_PREIDX_X(block, REG_X19, REG_X20, REG_SP, -16);

	offset = add_literal_q(block, (uintptr_t)&cpu_state);
	host_arm64_LDR_LITERAL_X(block, REG_CPUSTATE, offset);
}

void codegen_backend_epilogue(codeblock_t *block)
{
	host_arm64_LDP_POSTIDX_X(block, REG_X19, REG_X20, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X21, REG_X22, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X23, REG_X24, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X25, REG_X26, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X27, REG_X28, REG_SP, 16);
	host_arm64_LDP_POSTIDX_X(block, REG_X29, REG_X30, REG_SP, 16);
	host_arm64_RET(block, REG_X30);

        if (block_pos > ARM_LITERAL_POOL_OFFSET)
                fatal("Over limit!\n");



	__clear_cache(&block->data[0], &block->data[block_pos]);
}

#endif
