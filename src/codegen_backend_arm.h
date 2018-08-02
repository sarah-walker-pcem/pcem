#include "codegen_backend_arm_defs.h"

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


void host_arm_ADD_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
void host_arm_LDMIA_WB(codeblock_t *block, int addr_reg, uint32_t reg_mask);
void host_arm_LDR_IMM(codeblock_t *block, int dst_reg, int addr_reg, int offset);
void host_arm_MOV_IMM(codeblock_t *block, int dst_reg, uint32_t imm);
void host_arm_STMDB_WB(codeblock_t *block, int addr_reg, uint32_t reg_mask);
void host_arm_SUB_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);

void host_arm_call(codeblock_t *block, void *dst_addr);
void host_arm_nop(codeblock_t *block);

void codegen_reset_literal_pool(codeblock_t *block);
int add_literal(codeblock_t *block, uint32_t data);
