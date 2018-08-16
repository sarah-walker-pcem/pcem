void host_arm_ADD_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
#define host_arm_ADD_REG(block, dst_reg, src_reg_n, src_reg_m) host_arm_ADD_REG_LSL(block, dst_reg, src_reg_n, src_reg_m, 0)
void host_arm_ADD_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_ADD_REG_LSR(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_AND_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
void host_arm_AND_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_AND_REG_LSR(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_BFI(codeblock_t *block, int dst_reg, int src_reg, int lsb, int width);

void host_arm_BIC_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
void host_arm_BIC_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_BIC_REG_LSR(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_BL(codeblock_t *block, uintptr_t dest_addr);
void host_arm_BLX(codeblock_t *block, int addr_reg);
uint32_t *host_arm_BEQ_(codeblock_t *block);
uint32_t *host_arm_BNE_(codeblock_t *block);
void host_arm_BEQ(codeblock_t *block, uintptr_t dest_addr);
void host_arm_BNE(codeblock_t *block, uintptr_t dest_addr);

void host_arm_CMN_IMM(codeblock_t *block, int src_reg, uint32_t imm);
void host_arm_CMN_REG_LSL(codeblock_t *block, int src_reg_n, int src_reg_m, int shift);

void host_arm_CMP_IMM(codeblock_t *block, int src_reg, uint32_t imm);
void host_arm_CMP_REG_LSL(codeblock_t *block, int src_reg_n, int src_reg_m, int shift);

void host_arm_EOR_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
void host_arm_EOR_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_LDMIA_WB(codeblock_t *block, int addr_reg, uint32_t reg_mask);

void host_arm_LDR_IMM(codeblock_t *block, int dst_reg, int addr_reg, int offset);
void host_arm_LDR_IMM_POST(codeblock_t *block, int dst_reg, int addr_reg, int offset);
#define host_arm_LDR_REG(block, dst_reg, addr_reg, offset_reg) host_arm_LDR_REG_LSL(block, dst_reg, addr_reg, offset_reg, 0)
void host_arm_LDR_REG_LSL(codeblock_t *block, int dst_reg, int addr_reg, int offset_reg, int shift);

void host_arm_LDRB_ABS(codeblock_t *block, int dst, void *p);
void host_arm_LDRB_IMM(codeblock_t *block, int dst_reg, int addr_reg, int offset);
#define host_arm_LDRB_REG(block, dst_reg, addr_reg, offset_reg) host_arm_LDRB_REG_LSL(block, dst_reg, addr_reg, offset_reg, 0)
void host_arm_LDRB_REG_LSL(codeblock_t *block, int dst_reg, int addr_reg, int offset_reg, int shift);

void host_arm_LDRH_IMM(codeblock_t *block, int dst_reg, int addr_reg, int offset);
void host_arm_LDRH_REG(codeblock_t *block, int dst_reg, int addr_reg, int offset_reg);

void host_arm_MOV_IMM(codeblock_t *block, int dst_reg, uint32_t imm);
#define host_arm_MOV_REG(block, dst_reg, src_reg) host_arm_MOV_REG_LSL(block, dst_reg, src_reg, 0)
void host_arm_MOV_REG_ASR(codeblock_t *block, int dst_reg, int src_reg, int shift);
void host_arm_MOV_REG_ASR_REG(codeblock_t *block, int dst_reg, int src_reg, int shift_reg);
void host_arm_MOV_REG_LSL(codeblock_t *block, int dst_reg, int src_reg, int shift);
void host_arm_MOV_REG_LSL_REG(codeblock_t *block, int dst_reg, int src_reg, int shift_reg);
void host_arm_MOV_REG_LSR(codeblock_t *block, int dst_reg, int src_reg, int shift);
void host_arm_MOV_REG_LSR_REG(codeblock_t *block, int dst_reg, int src_reg, int shift_reg);
void host_arm_MOVT_IMM(codeblock_t *block, int dst_reg, uint16_t imm);
void host_arm_MOVW_IMM(codeblock_t *block, int dst_reg, uint16_t imm);

void host_arm_MVN_REG_LSL(codeblock_t *block, int dst_reg, int src_reg, int shift);

void host_arm_ORR_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
void host_arm_ORR_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_RSB_REG_LSR(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_STMDB_WB(codeblock_t *block, int addr_reg, uint32_t reg_mask);

void host_arm_STR_IMM(codeblock_t *block, int src_reg, int addr_reg, int offset);
void host_arm_STR_IMM_WB(codeblock_t *block, int src_reg, int addr_reg, int offset);
#define host_arm_STR_REG(block, src_reg, addr_reg, offset_reg) host_arm_STR_REG_LSL(block, src_reg, addr_reg, offset_reg, 0)
void host_arm_STR_REG_LSL(codeblock_t *block, int src_reg, int addr_reg, int offset_reg, int shift);

void host_arm_STRB_IMM(codeblock_t *block, int src_reg, int addr_reg, int offset);
#define host_arm_STRB_REG(block, src_reg, addr_reg, offset_reg) host_arm_STRB_REG_LSL(block, src_reg, addr_reg, offset_reg, 0)
void host_arm_STRB_REG_LSL(codeblock_t *block, int src_reg, int addr_reg, int offset_reg, int shift);

void host_arm_STRH_IMM(codeblock_t *block, int src_reg, int addr_reg, int offset);
void host_arm_STRH_REG(codeblock_t *block, int src_reg, int addr_reg, int offset_reg);

void host_arm_SUB_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm);
void host_arm_SUB_REG_LSL(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);
void host_arm_SUB_REG_LSR(codeblock_t *block, int dst_reg, int src_reg_n, int src_reg_m, int shift);

void host_arm_TST_IMM(codeblock_t *block, int src_reg1, uint32_t imm);
void host_arm_TST_REG(codeblock_t *block, int src_reg1, int src_reg2);

void host_arm_UADD8(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_arm_UADD16(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_arm_USUB8(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_arm_USUB16(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_arm_UXTB(codeblock_t *block, int dst_reg, int src_reg, int rotate);
void host_arm_UXTH(codeblock_t *block, int dst_reg, int src_reg, int rotate);
