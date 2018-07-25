void host_x86_ADD8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_ADD16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_ADD32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);
void host_x86_ADD64_REG_IMM(codeblock_t *block, int dst_reg, uint64_t imm_data);

void host_x86_ADD8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_ADD16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_ADD32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_AND8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_AND16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_AND32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);

void host_x86_AND8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_AND16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_AND32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_CALL(codeblock_t *block, void *p);

void host_x86_CMP32_REG_IMM(codeblock_t *block, int dst_reg, uint32_t imm_data);
void host_x86_CMP64_REG_IMM(codeblock_t *block, int dst_reg, uint64_t imm_data);

void host_x86_JNZ(codeblock_t *block, void *p);
void host_x86_JZ(codeblock_t *block, void *p);

uint8_t *host_x86_JNZ_short(codeblock_t *block);
uint8_t *host_x86_JZ_short(codeblock_t *block);

void host_x86_LEA_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t offset);
void host_x86_LEA_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_LEA_REG_REG_SHIFT(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b, int shift);

void host_x86_MOV8_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data);
void host_x86_MOV32_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data);

void host_x86_MOV8_ABS_REG(codeblock_t *block, void *p, int src_reg);
void host_x86_MOV32_ABS_REG(codeblock_t *block, void *p, int src_reg);
void host_x86_MOV64_ABS_REG(codeblock_t *block, void *p, int src_reg);

void host_x86_MOV8_BASE_INDEX_REG(codeblock_t *block, int dst_reg, int base_reg, int index_reg);
void host_x86_MOV16_BASE_INDEX_REG(codeblock_t *block, int dst_reg, int base_reg, int index_reg);
void host_x86_MOV32_BASE_INDEX_REG(codeblock_t *block, int dst_reg, int base_reg, int index_reg);

void host_x86_MOV32_REG_ABS(codeblock_t *block, int dst_reg, void *p);

void host_x86_MOV32_REG_BASE_INDEX(codeblock_t *block, int dst_reg, int base_reg, int index_reg);

void host_x86_MOV64_REG_BASE_INDEX_SHIFT(codeblock_t *block, int dst_reg, int base_reg, int index_reg, int scale);

void host_x86_MOV8_REG_IMM(codeblock_t *block, int reg, uint16_t imm_data);
void host_x86_MOV16_REG_IMM(codeblock_t *block, int reg, uint16_t imm_data);
void host_x86_MOV32_REG_IMM(codeblock_t *block, int reg, uint32_t imm_data);

void host_x86_MOV64_REG_IMM(codeblock_t *block, int reg, uint64_t imm_data);

void host_x86_MOV8_REG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOV16_REG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOV32_REG_REG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOV32_STACK_IMM(codeblock_t *block, int32_t offset, uint32_t imm_data);

void host_x86_MOVZX_BASE_INDEX_32_8(codeblock_t *block, int dst_reg, int base_reg, int index_reg);
void host_x86_MOVZX_BASE_INDEX_32_16(codeblock_t *block, int dst_reg, int base_reg, int index_reg);

void host_x86_MOVZX_REG_32_8(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVZX_REG_32_16(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOVZX_REG_ABS_32_8(codeblock_t *block, int dst_reg, void *p);

void host_x86_OR8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_OR16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_OR32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);

void host_x86_OR8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_OR16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_OR32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_POP(codeblock_t *block, int src_reg);

void host_x86_PUSH(codeblock_t *block, int src_reg);

void host_x86_RET(codeblock_t *block);

void host_x86_SHR32_IMM(codeblock_t *block, int dst_reg, int shift);

void host_x86_SUB8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_SUB16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_SUB32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);
void host_x86_SUB64_REG_IMM(codeblock_t *block, int dst_reg, uint64_t imm_data);

void host_x86_SUB8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_SUB16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_SUB32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_TEST32_REG(codeblock_t *block, int src_reg, int dst_reg);
void host_x86_TEST32_REG_IMM(codeblock_t *block, int dst_reg, uint32_t imm_data);

void host_x86_XOR8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_XOR16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_XOR32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);

void host_x86_XOR8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_XOR16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_XOR32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
