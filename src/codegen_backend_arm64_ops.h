void host_arm64_ADD_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);
void host_arm64_ADD_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);
void host_arm64_ADD_REG_LSR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);

void host_arm64_AND_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);
void host_arm64_AND_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);
void host_arm64_AND_REG_ASR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);
void host_arm64_AND_REG_ROR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);

void host_arm64_ANDS_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);

void host_arm64_ASR(codeblock_t *block, int dst_reg, int src_n_reg, int shift_reg);

void host_arm64_BFI(codeblock_t *block, int dst_reg, int src_reg, int lsb, int width);

void host_arm64_BLR(codeblock_t *block, int addr_reg);

void host_arm64_BEQ(codeblock_t *block, void *dest);

uint32_t *host_arm64_BEQ_(codeblock_t *block);
uint32_t *host_arm64_BNE_(codeblock_t *block);

void host_arm64_branch_set_offset(uint32_t *opcode, void *dest);

void host_arm64_CBNZ(codeblock_t *block, int reg, uintptr_t dest);

void host_arm64_CMN_IMM(codeblock_t *block, int src_n_reg, uint32_t imm_data);
void host_arm64_CMNX_IMM(codeblock_t *block, int src_n_reg, uint64_t imm_data);

void host_arm64_CMP_IMM(codeblock_t *block, int src_n_reg, uint32_t imm_data);
void host_arm64_CMPX_IMM(codeblock_t *block, int src_n_reg, uint64_t imm_data);

void host_arm64_EOR_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);
void host_arm64_EOR_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);

void host_arm64_LDP_POSTIDX_X(codeblock_t *block, int src_reg1, int src_reg2, int base_reg, int offset);

void host_arm64_LDR_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_LDR_LITERAL_W(codeblock_t *block, int dest_reg, int literal_offset);
void host_arm64_LDR_LITERAL_X(codeblock_t *block, int dest_reg, int literal_offset);
void host_arm64_LDR_REG(codeblock_t *block, int dest_reg, int base_reg, int offset_reg);

void host_arm64_LDRB_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_LDRB_REG(codeblock_t *block, int dest_reg, int base_reg, int offset_reg);

void host_arm64_LDRH_IMM(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_LDRH_REG(codeblock_t *block, int dest_reg, int base_reg, int offset_reg);

void host_arm64_LDRX_REG_LSL3(codeblock_t *block, int dest_reg, int base_reg, int offset_reg);

void host_arm64_LSL(codeblock_t *block, int dst_reg, int src_n_reg, int shift_reg);
void host_arm64_LSR(codeblock_t *block, int dst_reg, int src_n_reg, int shift_reg);

void host_arm64_MOV_REG_ASR(codeblock_t *block, int dst_reg, int src_m_reg, int shift);
void host_arm64_MOV_REG(codeblock_t *block, int dst_reg, int src_m_reg, int shift);
void host_arm64_MOV_REG_LSR(codeblock_t *block, int dst_reg, int src_m_reg, int shift);

void host_arm64_MOVX_REG(codeblock_t *block, int dst_reg, int src_m_reg, int shift);

void host_arm64_MOVZ_IMM(codeblock_t *block, int reg, uint32_t imm_data);
void host_arm64_MOVK_IMM(codeblock_t *block, int reg, uint32_t imm_data);

void host_arm64_NOP(codeblock_t *block);

void host_arm64_ORR_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);
void host_arm64_ORR_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);

void host_arm64_RET(codeblock_t *block, int reg);

void host_arm64_STP_PREIDX_X(codeblock_t *block, int src_reg1, int src_reg2, int base_reg, int offset);

void host_arm64_STR_IMM_W(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_STR_IMM_Q(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_STR_REG(codeblock_t *block, int src_reg, int base_reg, int offset_reg);

void host_arm64_STRB_IMM(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_STRB_REG(codeblock_t *block, int src_reg, int base_reg, int offset_reg);

void host_arm64_STRH_IMM(codeblock_t *block, int dest_reg, int base_reg, int offset);
void host_arm64_STRH_REG(codeblock_t *block, int src_reg, int base_reg, int offset_reg);

void host_arm64_SUB_IMM(codeblock_t *block, int dst_reg, int src_n_reg, uint32_t imm_data);
void host_arm64_SUB_REG(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);
void host_arm64_SUB_REG_LSR(codeblock_t *block, int dst_reg, int src_n_reg, int src_m_reg, int shift);

#define host_arm64_TST_IMM(block, src_n_reg, imm_data) host_arm64_ANDS_IMM(block, REG_XZR, src_n_reg, imm_data)

void host_arm64_UBFX(codeblock_t *block, int dst_reg, int src_reg, int lsb, int width);

void host_arm64_call(codeblock_t *block, void *dst_addr);
void host_arm64_mov_imm(codeblock_t *block, int reg, uint32_t imm_data);


#define in_range7_x(offset) (((offset) >= -0x200) && ((offset) < (0x200)) && !((offset) & 7))
#define in_range12_b(offset) (((offset) >= 0) && ((offset) < 0x1000))
#define in_range12_h(offset) (((offset) >= 0) && ((offset) < 0x2000) && !((offset) & 1))
#define in_range12_w(offset) (((offset) >= 0) && ((offset) < 0x4000) && !((offset) & 3))
#define in_range12_q(offset) (((offset) >= 0) && ((offset) < 0x8000) && !((offset) & 7))


void codegen_direct_read_8(codeblock_t *block, int host_reg, void *p);

int add_literal_q(codeblock_t *block, uint64_t data);