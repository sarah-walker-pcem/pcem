#ifndef _CODEGEN_REG_H_
#define _CODEGEN_REG_H_

#define IREG_SIZE_L  (0 << 6)
#define IREG_SIZE_W  (1 << 6)
#define IREG_SIZE_B  (2 << 6)
#define IREG_SIZE_BH (3 << 6)

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

	/*Temporary registers are stored on the stack, and are not guaranteed to
          be preserved across uOPs. They will not be written back if they will
          not be read again.*/
	IREG_temp0 = 20,
	IREG_temp1 = 21,
	IREG_temp2 = 22,
	IREG_temp3 = 23,

	IREG_COUNT,
	
	IREG_INVALID = 255
};

extern uint8_t reg_last_version[IREG_COUNT];
extern uint8_t reg_version_refcount[IREG_COUNT][256];

typedef struct
{
        uint8_t reg;
        uint8_t version;
} ir_reg_t;

extern ir_reg_t invalid_ir_reg;

typedef uint8_t ir_host_reg_t;

static inline ir_reg_t codegen_reg_read(int reg)
{
        ir_reg_t ireg;
        
        if (reg == IREG_INVALID)
                fatal("codegen_reg_read - IREG_INVALID\n");
        
        ireg.reg = reg;
        ireg.version = reg_last_version[reg];
        reg_version_refcount[ireg.reg][ireg.version]++;
        if (!reg_version_refcount[ireg.reg][ireg.version])
                fatal("codegen_reg_read - refcount overflow\n");
        
        return ireg;
}

static inline ir_reg_t codegen_reg_write(int reg)
{
        ir_reg_t ireg;

        if (reg == IREG_INVALID)
                fatal("codegen_reg_write - IREG_INVALID\n");

        ireg.reg = reg;
        ireg.version = reg_last_version[reg] + 1;
        reg_last_version[reg]++;
        if (!reg_last_version[reg])
                fatal("codegen_reg_write - version overflow\n");
        reg_version_refcount[reg][ireg.version] = 0;

        return ireg;
}

struct ir_data_t;

void codegen_reg_reset();
void codegen_reg_flush(struct ir_data_t *ir, codeblock_t *block);

ir_host_reg_t codegen_reg_alloc_write_reg(codeblock_t *block, ir_reg_t ir_reg);

#endif
