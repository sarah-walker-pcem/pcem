void host_x86_ADD8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_ADD16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_ADD32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);
void host_x86_ADD64_REG_IMM(codeblock_t *block, int dst_reg, uint64_t imm_data);

void host_x86_ADD8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_ADD16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_ADD32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_ADDPS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_ADDSD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_AND8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_AND16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_AND32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);

void host_x86_AND8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_AND16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_AND32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_CALL(codeblock_t *block, void *p);

void host_x86_CMP16_REG_IMM(codeblock_t *block, int dst_reg, uint16_t imm_data);
void host_x86_CMP32_REG_IMM(codeblock_t *block, int dst_reg, uint32_t imm_data);
void host_x86_CMP64_REG_IMM(codeblock_t *block, int dst_reg, uint64_t imm_data);

void host_x86_CMP8_REG_REG(codeblock_t *block, int src_reg_a, int src_reg_b);
void host_x86_CMP16_REG_REG(codeblock_t *block, int src_reg_a, int src_reg_b);
void host_x86_CMP32_REG_REG(codeblock_t *block, int src_reg_a, int src_reg_b);

#define CMPPS_EQ  0
#define CMPPS_NLT 5
#define CMPPS_NLE 6
void host_x86_CMPPS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg, int type);

void host_x86_COMISD_XREG_XREG(codeblock_t *block, int src_reg_a, int src_reg_b);

void host_x86_CVTDQ2PS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTPS2DQ_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_CVTSD2SI_REG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSD2SI_REG64_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSD2SS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSI2SD_XREG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSI2SD_XREG_REG64(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSI2SS_XREG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSS2SD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_CVTSS2SD_XREG_BASE_INDEX(codeblock_t *block, int dst_reg, int base_reg, int idx_reg);

void host_x86_DIVSD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_DIVSS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_JMP(codeblock_t *block, void *p);

void host_x86_JNZ(codeblock_t *block, void *p);
void host_x86_JZ(codeblock_t *block, void *p);

uint8_t *host_x86_JNZ_short(codeblock_t *block);
uint8_t *host_x86_JS_short(codeblock_t *block);
uint8_t *host_x86_JZ_short(codeblock_t *block);

uint32_t *host_x86_JNB_long(codeblock_t *block);
uint32_t *host_x86_JNBE_long(codeblock_t *block);
uint32_t *host_x86_JNL_long(codeblock_t *block);
uint32_t *host_x86_JNLE_long(codeblock_t *block);
uint32_t *host_x86_JNO_long(codeblock_t *block);
uint32_t *host_x86_JNS_long(codeblock_t *block);
uint32_t *host_x86_JNZ_long(codeblock_t *block);
uint32_t *host_x86_JB_long(codeblock_t *block);
uint32_t *host_x86_JBE_long(codeblock_t *block);
uint32_t *host_x86_JL_long(codeblock_t *block);
uint32_t *host_x86_JLE_long(codeblock_t *block);
uint32_t *host_x86_JO_long(codeblock_t *block);
uint32_t *host_x86_JS_long(codeblock_t *block);
uint32_t *host_x86_JZ_long(codeblock_t *block);

void host_x86_LAHF(codeblock_t *block);

void host_x86_LDMXCSR(codeblock_t *block, void *p);

void host_x86_LEA_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t offset);
void host_x86_LEA_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_LEA_REG_REG_SHIFT(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b, int shift);

void host_x86_MAXSD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOV8_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data);
void host_x86_MOV32_ABS_IMM(codeblock_t *block, void *p, uint32_t imm_data);

void host_x86_MOV8_ABS_REG(codeblock_t *block, void *p, int src_reg);
void host_x86_MOV16_ABS_REG(codeblock_t *block, void *p, int src_reg);
void host_x86_MOV32_ABS_REG(codeblock_t *block, void *p, int src_reg);
void host_x86_MOV64_ABS_REG(codeblock_t *block, void *p, int src_reg);

void host_x86_MOV8_ABS_REG_REG_SHIFT_REG(codeblock_t *block, uint32_t addr, int base_reg, int index_reg, int shift, int src_reg);

void host_x86_MOV8_BASE_INDEX_REG(codeblock_t *block, int dst_reg, int base_reg, int index_reg);
void host_x86_MOV16_BASE_INDEX_REG(codeblock_t *block, int dst_reg, int base_reg, int index_reg);
void host_x86_MOV32_BASE_INDEX_REG(codeblock_t *block, int dst_reg, int base_reg, int index_reg);

void host_x86_MOV32_BASE_OFFSET_REG(codeblock_t *block, int base_reg, int offset, int src_reg);

void host_x86_MOV8_REG_ABS(codeblock_t *block, int dst_reg, void *p);
void host_x86_MOV16_REG_ABS(codeblock_t *block, int dst_reg, void *p);
void host_x86_MOV32_REG_ABS(codeblock_t *block, int dst_reg, void *p);
void host_x86_MOV64_REG_ABS(codeblock_t *block, int dst_reg, void *p);

void host_x86_MOV8_REG_ABS_REG_REG_SHIFT(codeblock_t *block, int dst_reg, uint32_t addr, int base_reg, int index_reg, int shift);

void host_x86_MOV32_REG_BASE_INDEX(codeblock_t *block, int dst_reg, int base_reg, int index_reg);

void host_x86_MOV64_REG_BASE_INDEX_SHIFT(codeblock_t *block, int dst_reg, int base_reg, int index_reg, int scale);

void host_x86_MOV16_REG_BASE_OFFSET(codeblock_t *block, int dst_reg, int base_reg, int offset);
void host_x86_MOV32_REG_BASE_OFFSET(codeblock_t *block, int dst_reg, int base_reg, int offset);

void host_x86_MOV8_REG_IMM(codeblock_t *block, int reg, uint16_t imm_data);
void host_x86_MOV16_REG_IMM(codeblock_t *block, int reg, uint16_t imm_data);
void host_x86_MOV32_REG_IMM(codeblock_t *block, int reg, uint32_t imm_data);

void host_x86_MOV64_REG_IMM(codeblock_t *block, int reg, uint64_t imm_data);

void host_x86_MOV8_REG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOV16_REG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOV32_REG_REG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOV32_STACK_IMM(codeblock_t *block, int32_t offset, uint32_t imm_data);

void host_x86_MOVD_BASE_INDEX_XREG(codeblock_t *block, int base_reg, int idx_reg, int src_reg);
void host_x86_MOVD_REG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVD_XREG_BASE_INDEX(codeblock_t *block, int dst_reg, int base_reg, int idx_reg);
void host_x86_MOVD_XREG_REG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOVQ_ABS_XREG(codeblock_t *block, void *p, int src_reg);
void host_x86_MOVQ_ABS_REG_REG_SHIFT_XREG(codeblock_t *block, uint32_t addr, int src_reg_a, int src_reg_b, int shift, int src_reg);
void host_x86_MOVQ_BASE_INDEX_XREG(codeblock_t *block, int base_reg, int idx_reg, int src_reg);
void host_x86_MOVQ_BASE_OFFSET_XREG(codeblock_t *block, int base_reg, int offset, int src_reg);
void host_x86_MOVQ_XREG_ABS(codeblock_t *block, int dst_reg, void *p);
void host_x86_MOVQ_XREG_ABS_REG_REG_SHIFT(codeblock_t *block, int dst_reg, uint32_t addr, int src_reg_a, int src_reg_b, int shift);
void host_x86_MOVQ_XREG_BASE_INDEX(codeblock_t *block, int dst_reg, int base_reg, int idx_reg);
void host_x86_MOVQ_XREG_BASE_OFFSET(codeblock_t *block, int dst_reg, int base_reg, int offset);

void host_x86_MOVQ_REG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVQ_XREG_REG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVQ_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOVSX_REG_16_8(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVSX_REG_32_8(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVSX_REG_32_16(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOVZX_BASE_INDEX_32_8(codeblock_t *block, int dst_reg, int base_reg, int index_reg);
void host_x86_MOVZX_BASE_INDEX_32_16(codeblock_t *block, int dst_reg, int base_reg, int index_reg);

void host_x86_MOVZX_REG_16_8(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVZX_REG_32_8(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MOVZX_REG_32_16(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MOVZX_REG_ABS_32_8(codeblock_t *block, int dst_reg, void *p);

void host_x86_MAXPS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MINPS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_MULSD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_MULSS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_NOP(codeblock_t *block);

void host_x86_OR8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_OR16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_OR32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);

void host_x86_OR8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_OR16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_OR32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_PACKSSWB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PACKSSDW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PACKUSWB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_PADDB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PADDW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PADDD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PADDSB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PADDSW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PADDUSB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PADDUSW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_PAND_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PANDN_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_POR_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PXOR_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_PCMPEQB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PCMPEQW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PCMPEQD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PCMPGTB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PCMPGTW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PCMPGTD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_PMADDWD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PMULHW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PMULLW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_PSLLW_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSLLD_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSLLQ_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSRAW_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSRAD_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSRAQ_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSRLW_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSRLD_XREG_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_PSRLQ_XREG_IMM(codeblock_t *block, int dst_reg, int shift);

void host_x86_PSUBB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PSUBW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PSUBD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PSUBSB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PSUBSW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PSUBUSB_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PSUBUSW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_PUNPCKHBW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PUNPCKHWD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PUNPCKHDQ_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PUNPCKLBW_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PUNPCKLWD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_PUNPCKLDQ_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_POP(codeblock_t *block, int src_reg);

void host_x86_PUSH(codeblock_t *block, int src_reg);

void host_x86_RET(codeblock_t *block);

void host_x86_SAR8_CL(codeblock_t *block, int dst_reg);
void host_x86_SAR16_CL(codeblock_t *block, int dst_reg);
void host_x86_SAR32_CL(codeblock_t *block, int dst_reg);

void host_x86_SAR8_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_SAR16_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_SAR32_IMM(codeblock_t *block, int dst_reg, int shift);

void host_x86_SHL8_CL(codeblock_t *block, int dst_reg);
void host_x86_SHL16_CL(codeblock_t *block, int dst_reg);
void host_x86_SHL32_CL(codeblock_t *block, int dst_reg);

void host_x86_SHL8_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_SHL16_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_SHL32_IMM(codeblock_t *block, int dst_reg, int shift);

void host_x86_SHR8_CL(codeblock_t *block, int dst_reg);
void host_x86_SHR16_CL(codeblock_t *block, int dst_reg);
void host_x86_SHR32_CL(codeblock_t *block, int dst_reg);

void host_x86_SHR8_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_SHR16_IMM(codeblock_t *block, int dst_reg, int shift);
void host_x86_SHR32_IMM(codeblock_t *block, int dst_reg, int shift);

void host_x86_SQRTSD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_SQRTSS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_SUB8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_SUB16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_SUB32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);
void host_x86_SUB64_REG_IMM(codeblock_t *block, int dst_reg, uint64_t imm_data);

void host_x86_SUB8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_SUB16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_SUB32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);

void host_x86_SUBPS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);
void host_x86_SUBSD_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_TEST8_REG(codeblock_t *block, int src_host_reg, int dst_host_reg);
void host_x86_TEST16_REG(codeblock_t *block, int src_host_reg, int dst_host_reg);
void host_x86_TEST32_REG(codeblock_t *block, int src_reg, int dst_reg);
void host_x86_TEST32_REG_IMM(codeblock_t *block, int dst_reg, uint32_t imm_data);

void host_x86_UNPCKLPS_XREG_XREG(codeblock_t *block, int dst_reg, int src_reg);

void host_x86_XOR8_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint8_t imm_data);
void host_x86_XOR16_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint16_t imm_data);
void host_x86_XOR32_REG_IMM(codeblock_t *block, int dst_reg, int src_reg, uint32_t imm_data);

void host_x86_XOR8_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_XOR16_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
void host_x86_XOR32_REG_REG(codeblock_t *block, int dst_reg, int src_reg_a, int src_reg_b);
