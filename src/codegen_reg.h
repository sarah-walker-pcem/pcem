#ifndef _CODEGEN_REG_H_
#define _CODEGEN_REG_H_

#define IREG_REG_MASK 0x3f
#define IREG_SIZE_SHIFT 6
#define IREG_SIZE_MASK (3 << IREG_SIZE_SHIFT)

#define IREG_GET_REG(reg)  ((reg) & IREG_REG_MASK)
#define IREG_GET_SIZE(reg) ((reg) & IREG_SIZE_MASK)

#define IREG_SIZE_L  (0 << IREG_SIZE_SHIFT)
#define IREG_SIZE_W  (1 << IREG_SIZE_SHIFT)
#define IREG_SIZE_B  (2 << IREG_SIZE_SHIFT)
#define IREG_SIZE_BH (3 << IREG_SIZE_SHIFT)

enum
{
	IREG_EAX = 0,
	IREG_ECX = 1,
	IREG_EDX = 2,
	IREG_EBX = 3,
	IREG_ESP = 4,
	IREG_EBP = 5,
	IREG_ESI = 6,
	IREG_EDI = 7,

	IREG_flags_op = 8,
	IREG_flags_res = 9,
	IREG_flags_op1 = 10,
	IREG_flags_op2 = 11,

	IREG_pc = 12,
	IREG_oldpc = 13,

	IREG_eaaddr = 14,
	IREG_ea_seg = 15,
	IREG_op32   = 16,
	IREG_ssegs  = 17,
	
	IREG_rm_mod_reg = 18,
	
	IREG_ins = 19,
	IREG_cycles = 20,

        IREG_CS_base = 21,
        IREG_DS_base = 22,
        IREG_ES_base = 23,
        IREG_FS_base = 24,
        IREG_GS_base = 25,
        IREG_SS_base = 26,

        IREG_CS_seg = 27,
        IREG_DS_seg = 28,
        IREG_ES_seg = 29,
        IREG_FS_seg = 30,
        IREG_GS_seg = 31,
        IREG_SS_seg = 32,

	/*Temporary registers are stored on the stack, and are not guaranteed to
          be preserved across uOPs. They will not be written back if they will
          not be read again.*/
	IREG_temp0 = 33,
	IREG_temp1 = 34,
	IREG_temp2 = 35,
	IREG_temp3 = 36,

	IREG_COUNT,
	
	IREG_INVALID = 63,
	
	IREG_AX = IREG_EAX + IREG_SIZE_W,
	IREG_CX = IREG_ECX + IREG_SIZE_W,
	IREG_DX = IREG_EDX + IREG_SIZE_W,
	IREG_BX = IREG_EBX + IREG_SIZE_W,
	IREG_SP = IREG_ESP + IREG_SIZE_W,
	IREG_BP = IREG_EBP + IREG_SIZE_W,
	IREG_SI = IREG_ESI + IREG_SIZE_W,
	IREG_DI = IREG_EDI + IREG_SIZE_W,

	IREG_AL = IREG_EAX + IREG_SIZE_B,
	IREG_CL = IREG_ECX + IREG_SIZE_B,
	IREG_DL = IREG_EDX + IREG_SIZE_B,
	IREG_BL = IREG_EBX + IREG_SIZE_B,

	IREG_AH = IREG_EAX + IREG_SIZE_BH,
	IREG_CH = IREG_ECX + IREG_SIZE_BH,
	IREG_DH = IREG_EDX + IREG_SIZE_BH,
	IREG_BH = IREG_EBX + IREG_SIZE_BH,
	
	IREG_flags_res_W = IREG_flags_res + IREG_SIZE_W,
	IREG_flags_op1_W = IREG_flags_op1 + IREG_SIZE_W,
	IREG_flags_op2_W = IREG_flags_op2 + IREG_SIZE_W,

	IREG_flags_res_B = IREG_flags_res + IREG_SIZE_B,
	IREG_flags_op1_B = IREG_flags_op1 + IREG_SIZE_B,
	IREG_flags_op2_B = IREG_flags_op2 + IREG_SIZE_B,

	IREG_temp0_W = IREG_temp0 + IREG_SIZE_W,
	IREG_temp1_W = IREG_temp1 + IREG_SIZE_W,
	IREG_temp2_W = IREG_temp2 + IREG_SIZE_W,
	
	IREG_temp0_B = IREG_temp0 + IREG_SIZE_B,
	IREG_temp1_B = IREG_temp1 + IREG_SIZE_B,
	
	IREG_eaaddr_W = IREG_eaaddr + IREG_SIZE_W,
	
        IREG_CS_seg_W = IREG_CS_seg + IREG_SIZE_W,
        IREG_DS_seg_W = IREG_DS_seg + IREG_SIZE_W,
        IREG_ES_seg_W = IREG_ES_seg + IREG_SIZE_W,
        IREG_FS_seg_W = IREG_FS_seg + IREG_SIZE_W,
        IREG_GS_seg_W = IREG_GS_seg + IREG_SIZE_W,
        IREG_SS_seg_W = IREG_SS_seg + IREG_SIZE_W
};

#define IREG_8(reg)  (((reg) & 4) ? (((reg) & 3) + IREG_AH) : ((reg) + IREG_AL))
#define IREG_16(reg) ((reg) + IREG_AX)
#define IREG_32(reg) ((reg) + IREG_EAX)

static inline int ireg_seg_base(x86seg *seg)
{
        if (seg == &cpu_state.seg_cs)
                return IREG_CS_base;
        if (seg == &cpu_state.seg_ds)
                return IREG_DS_base;
        if (seg == &cpu_state.seg_es)
                return IREG_ES_base;
        if (seg == &cpu_state.seg_fs)
                return IREG_FS_base;
        if (seg == &cpu_state.seg_gs)
                return IREG_GS_base;
        if (seg == &cpu_state.seg_ss)
                return IREG_SS_base;
        fatal("ireg_seg_base : unknown segment\n");
        return 0;
}

extern uint8_t reg_last_version[IREG_COUNT];
extern uint8_t reg_version_refcount[IREG_COUNT][256];

typedef struct
{
        uint8_t reg;
        uint8_t version;
} ir_reg_t;

extern ir_reg_t invalid_ir_reg;

typedef uint8_t ir_host_reg_t;

#define REG_VERSION_MAX 250

static inline ir_reg_t codegen_reg_read(int reg)
{
        ir_reg_t ireg;
        
        if (IREG_GET_REG(reg) == IREG_INVALID)
                fatal("codegen_reg_read - IREG_INVALID\n");
        
        ireg.reg = reg;
        ireg.version = reg_last_version[IREG_GET_REG(reg)];
        reg_version_refcount[IREG_GET_REG(ireg.reg)][ireg.version]++;
        if (!reg_version_refcount[IREG_GET_REG(ireg.reg)][ireg.version])
                fatal("codegen_reg_read - refcount overflow\n");
        else if (reg_version_refcount[IREG_GET_REG(ireg.reg)][ireg.version] > REG_VERSION_MAX)
                CPU_BLOCK_END();
        
        return ireg;
}

static inline ir_reg_t codegen_reg_write(int reg)
{
        ir_reg_t ireg;

        if (IREG_GET_REG(reg) == IREG_INVALID)
                fatal("codegen_reg_write - IREG_INVALID\n");

        ireg.reg = reg;
        ireg.version = reg_last_version[IREG_GET_REG(reg)] + 1;
        reg_last_version[IREG_GET_REG(reg)]++;
        if (!reg_last_version[IREG_GET_REG(reg)])
                fatal("codegen_reg_write - version overflow\n");
        else if (reg_last_version[IREG_GET_REG(reg)] > REG_VERSION_MAX)
                CPU_BLOCK_END();
        reg_version_refcount[IREG_GET_REG(reg)][ireg.version] = 0;

        return ireg;
}

struct ir_data_t;

void codegen_reg_reset();
/*Write back all dirty registers*/
void codegen_reg_flush(struct ir_data_t *ir, codeblock_t *block);
/*Write back and evict all registers*/
void codegen_reg_flush_invalidate(struct ir_data_t *ir, codeblock_t *block);

/*Register ir_reg usage for this uOP. This ensures that required registers aren't evicted*/
void codegen_reg_alloc_register(ir_reg_t dest_reg_a, ir_reg_t src_reg_a, ir_reg_t src_reg_b, ir_reg_t src_reg_c);

ir_host_reg_t codegen_reg_alloc_read_reg(codeblock_t *block, ir_reg_t ir_reg, int *host_reg_idx);
ir_host_reg_t codegen_reg_alloc_write_reg(codeblock_t *block, ir_reg_t ir_reg);

#endif
