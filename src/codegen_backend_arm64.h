#include "codegen_backend_arm64_defs.h"

#define BLOCK_SIZE 0x1000
#define BLOCK_MASK 0x0fff
#define BLOCK_START 64

#define HASH_SIZE 0x20000
#define HASH_MASK 0x1ffff

#define HASH(l) ((l) & 0x1ffff)

/*Hack until better memory management written*/
/*#define BLOCK_EXIT_OFFSET 0x7f0*/

#define BLOCK_GPF_OFFSET 0
#define BLOCK_EXIT_OFFSET 32

#define ARM_LITERAL_POOL_OFFSET 0xf000

/*#define BLOCK_MAX 1720*/
#define BLOCK_MAX 65208


void host_arm64_BLR(codeblock_t *block, int addr_reg);
void host_arm64_CBNZ(codeblock_t *block, int reg, uintptr_t dest);
void host_arm64_MOVK_IMM(codeblock_t *block, int reg, uint32_t imm_data);
void host_arm64_MOVZ_IMM(codeblock_t *block, int reg, uint32_t imm_data);
void host_arm64_LDP_POSTIDX_X(codeblock_t *block, int src_reg1, int src_reg2, int base_reg, int offset);
void host_arm64_LDR_LITERAL_W(codeblock_t *block, int dest_reg, int literal_offset);
void host_arm64_LDR_LITERAL_X(codeblock_t *block, int dest_reg, int literal_offset);
void host_arm64_NOP(codeblock_t *block);
void host_arm64_RET(codeblock_t *block, int reg);
void host_arm64_STP_PREIDX_X(codeblock_t *block, int src_reg1, int src_reg2, int base_reg, int offset);
void host_arm64_STR_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_STRB_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset);

void host_arm64_call(codeblock_t *block, void *dst_addr);
void host_arm64_mov_imm(codeblock_t *block, int reg, uint32_t imm_data);

void codegen_reset_literal_pool(codeblock_t *block);
int add_literal(codeblock_t *block, uint32_t data);
int add_literal_q(codeblock_t *block, uint64_t data);
