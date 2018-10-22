#ifdef __aarch64__

#include <stdlib.h>
#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_backend_arm64_defs.h"
#include "codegen_backend_arm64_ops.h"
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
void *codegen_mem_store_single;
void *codegen_mem_store_double;

int codegen_host_reg_list[CODEGEN_HOST_REGS] =
{
        REG_X19,
        REG_X20,
        REG_X21,
	REG_X22,
	REG_X23,
        REG_X24,
        REG_X25,
	REG_X26,
	REG_X27,
	REG_X28
};

int codegen_host_fp_reg_list[CODEGEN_HOST_FP_REGS] =
{
        REG_V8,
        REG_V9,
        REG_V10,
	REG_V11,
	REG_V12,
        REG_V13,
        REG_V14,
	REG_V15
};

static void build_load_routine(codeblock_t *block, int size, int is_float)
{
        uint32_t *branch_offset;
        uint8_t *misaligned_offset;
	int offset;

        /*In - W0 = address
          Out - W0 = data, W1 = abrt*/
	/*MOV W1, W0, LSR #12
	  MOV X2, #readlookup2
	  LDR X1, [X2, X1, LSL #3]
	  CMP X1, #-1
	  BEQ +
	  LDRB W0, [X1, X0]
	  MOV W1, #0
	  RET
	* STP X29, X30, [SP, #-16]
	  BL readmemb386l
	  LDRB R1, cpu_state.abrt
	  LDP X29, X30, [SP, #-16]
	  RET
	*/
	host_arm64_MOV_REG_LSR(block, REG_W1, REG_W0, 12);
	offset = add_literal_q(block, (uintptr_t)readlookup2);
	host_arm64_LDR_LITERAL_X(block, REG_X2, offset);
	host_arm64_LDRX_REG_LSL3(block, REG_X1, REG_X2, REG_X1);
	if (size != 1)
	{
		host_arm64_TST_IMM(block, REG_W0, size-1);
		misaligned_offset = host_arm64_BNE_(block);
	}
	host_arm64_CMPX_IMM(block, REG_X1, -1);
	branch_offset = host_arm64_BEQ_(block);
	if (size == 1 && !is_float)
		host_arm64_LDRB_REG(block, REG_W0, REG_W1, REG_W0);
	else if (size == 2 && !is_float)
		host_arm64_LDRH_REG(block, REG_W0, REG_W1, REG_W0);
	else if (size == 4 && !is_float)
		host_arm64_LDR_REG(block, REG_W0, REG_W1, REG_W0);
	else if (size == 4 && is_float)
		host_arm64_LDR_REG_F32(block, REG_V_TEMP, REG_W1, REG_W0);
	else if (size == 8)
		host_arm64_LDR_REG_F64(block, REG_V_TEMP, REG_W1, REG_W0);
	host_arm64_MOVZ_IMM(block, REG_W1, 0);
	host_arm64_RET(block, REG_X30);

	host_arm64_branch_set_offset(branch_offset, &block->data[block_pos]);
	if (size != 1)
		host_arm64_branch_set_offset(misaligned_offset, &block->data[block_pos]);
	host_arm64_STP_PREIDX_X(block, REG_X29, REG_X30, REG_SP, -16);
	if (size == 1)
		host_arm64_call(block, (uintptr_t)readmemb386l);
	else if (size == 2)
		host_arm64_call(block, (uintptr_t)readmemwl);
	else if (size == 4)
		host_arm64_call(block, (uintptr_t)readmemll);
	else if (size == 8)
		host_arm64_call(block, (uintptr_t)readmemql);
	else
		fatal("build_load_routine - unknown size %i\n", size);
	codegen_direct_read_8(block, REG_W1, &cpu_state.abrt);
	if (size == 4 && is_float)
		host_arm64_FMOV_S_W(block, REG_V_TEMP, REG_W0);
	else if (size == 8)
		host_arm64_FMOV_D_Q(block, REG_V_TEMP, REG_X0);
	host_arm64_LDP_POSTIDX_X(block, REG_X29, REG_X30, REG_SP, 16);
	host_arm64_RET(block, REG_X30);

        block_pos = (block_pos + 63) & ~63;
}

static void build_store_routine(codeblock_t *block, int size, int is_float)
{
        uint8_t *branch_offset;
        uint8_t *misaligned_offset;
	int offset;

        /*In - R0 = address, R1 = data
          Out - R1 = abrt*/
	/*MOV W2, W0, LSR #12
	  MOV X3, #writelookup2
	  LDR X2, [X3, X2, LSL #3]
	  CMP X2, #-1
	  BEQ +
	  STRB W1, [X2, X0]
	  MOV W1, #0
	  RET
	* STP X29, X30, [SP, #-16]
	  BL writememb386l
	  LDRB R1, cpu_state.abrt
	  LDP X29, X30, [SP, #-16]
	  RET
	*/
	host_arm64_MOV_REG_LSR(block, REG_W2, REG_W0, 12);
	offset = add_literal_q(block, (uintptr_t)writelookup2);
	host_arm64_LDR_LITERAL_X(block, REG_X3, offset);
	host_arm64_LDRX_REG_LSL3(block, REG_X2, REG_X3, REG_X2);
	if (size != 1)
	{
		host_arm64_TST_IMM(block, REG_W0, size-1);
		misaligned_offset = host_arm64_BNE_(block);
	}
	host_arm64_CMPX_IMM(block, REG_X2, -1);
	branch_offset = host_arm64_BEQ_(block);
	if (size == 1 && !is_float)
		host_arm64_STRB_REG(block, REG_X1, REG_X2, REG_X0);
	else if (size == 2 && !is_float)
		host_arm64_STRH_REG(block, REG_X1, REG_X2, REG_X0);
	else if (size == 4 && !is_float)
		host_arm64_STR_REG(block, REG_X1, REG_X2, REG_X0);
	else if (size == 4 && is_float)
		host_arm64_STR_REG_F32(block, REG_V_TEMP, REG_X2, REG_X0);
	else if (size == 8 && is_float)
		host_arm64_STR_REG_F64(block, REG_V_TEMP, REG_X2, REG_X0);
	host_arm64_MOVZ_IMM(block, REG_X1, 0);
	host_arm64_RET(block, REG_X30);

	host_arm64_branch_set_offset(branch_offset, &block->data[block_pos]);
	if (size != 1)
		host_arm64_branch_set_offset(misaligned_offset, &block->data[block_pos]);
	host_arm64_STP_PREIDX_X(block, REG_X29, REG_X30, REG_SP, -16);
	if (size == 4 && is_float)
		host_arm64_FMOV_W_S(block, REG_W1, REG_V_TEMP);
	else if (size == 8 && is_float)
		host_arm64_FMOV_Q_D(block, REG_X1, REG_V_TEMP);
	if (size == 1)
		host_arm64_call(block, (uintptr_t)writememb386l);
	else if (size == 2)
		host_arm64_call(block, (uintptr_t)writememwl);
	else if (size == 4)
		host_arm64_call(block, (uintptr_t)writememll);
	else if (size == 8)
		host_arm64_call(block, (uintptr_t)writememql);
	else
		fatal("build_store_routine - unknown size %i\n", size);
	codegen_direct_read_8(block, REG_W1, &cpu_state.abrt);
	host_arm64_LDP_POSTIDX_X(block, REG_X29, REG_X30, REG_SP, 16);
	host_arm64_RET(block, REG_X30);

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

        block_current = BLOCK_SIZE;
        block_pos = 0;
	codegen_reset_literal_pool(&codeblock[block_current]);
        build_loadstore_routines(&codeblock[block_current]);
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
	host_arm64_LDP_POSTIDX_X(block, REG_X19, REG_X20, REG_SP, 64);
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
	host_arm64_STP_PREIDX_X(block, REG_X19, REG_X20, REG_SP, -64);

	offset = add_literal_q(block, (uintptr_t)&cpu_state);
	host_arm64_LDR_LITERAL_X(block, REG_CPUSTATE, offset);

        if (block->flags & CODEBLOCK_HAS_FPU)
        {
		host_arm64_LDR_IMM_W(block, REG_TEMP, REG_CPUSTATE, (uintptr_t)&cpu_state.TOP - (uintptr_t)&cpu_state);
		host_arm64_SUB_IMM(block, REG_TEMP, REG_TEMP, block->TOP);
		host_arm64_STR_IMM_W(block, REG_TEMP, REG_SP, IREG_TOP_diff_stack_offset);
        }
}

void codegen_backend_epilogue(codeblock_t *block)
{
	host_arm64_LDP_POSTIDX_X(block, REG_X19, REG_X20, REG_SP, 64);
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
