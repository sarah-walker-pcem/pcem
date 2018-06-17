#ifndef _CODEGEN_IR_DEFS_
#define _CODEGEN_IR_DEFS_

#define IREG_L  (0 << 6)
#define IREG_W  (1 << 6)
#define IREG_B  (2 << 6)
#define IREG_BH (3 << 6)

enum
{
	ireg_EAX = 0,
	ireg_ECX = 1,
	ireg_EDX = 2,
	ireg_EBX = 3,
	ireg_ESP = 4,
	ireg_EBP = 5,
	ireg_ESI = 6,
	ireg_EDI = 7,

	ireg_flags_op = 8,
	ireg_flags_reg = 9,
	ireg_flags_op1 = 10,
	ireg_flags_op2 = 11,

	ireg_flags_pc = 12,
	ireg_flags_oldpc = 13,

	ireg_eaaddr = 14,
	
	/*Temporary registers are stored on the stack, and are not guaranteed to
          be preserved across uOPs. They will not be written back if they will
          not be read again.*/
	ireg_temp0 = 16,
	ireg_temp1 = 17,
	ireg_temp2 = 18,
	ireg_temp3 = 19
};

#define UOP_REG(reg, size, version) ((reg) | (size) | (version << 8))

#define UOP_REG_UNUSED 0xffff

/*uOP is a barrier. All previous uOPs must have completed before this one executes.
  All registers must have been written back or discarded*/
#define UOP_TYPE_BARRIER (1 << 31)

/*uOP uses source and dest registers*/
#define UOP_TYPE_PARAMS_REGS    (1 << 28)
/*uOP uses pointer*/
#define UOP_TYPE_PARAMS_POINTER (1 << 29)
/*uOP uses immediate data*/
#define UOP_TYPE_PARAMS_IMM     (1 << 30)



#define UOP_LOAD_FUNC_ARG_0       (UOP_TYPE_PARAMS_REGS    | 0x00)
#define UOP_LOAD_FUNC_ARG_1       (UOP_TYPE_PARAMS_REGS    | 0x01)
#define UOP_LOAD_FUNC_ARG_2       (UOP_TYPE_PARAMS_REGS    | 0x02)
#define UOP_LOAD_FUNC_ARG_3       (UOP_TYPE_PARAMS_REGS    | 0x03)
#define UOP_LOAD_FUNC_ARG_0_IMM   (UOP_TYPE_PARAMS_IMM     | 0x08)
#define UOP_LOAD_FUNC_ARG_1_IMM   (UOP_TYPE_PARAMS_IMM     | 0x09)
#define UOP_LOAD_FUNC_ARG_2_IMM   (UOP_TYPE_PARAMS_IMM     | 0x0a)
#define UOP_LOAD_FUNC_ARG_3_IMM   (UOP_TYPE_PARAMS_IMM     | 0x0b)
#define UOP_CALL_FUNC             (UOP_TYPE_PARAMS_POINTER | 0x10 | UOP_TYPE_BARRIER)
#define UOP_CALL_INSTRUCTION_FUNC (UOP_TYPE_PARAMS_POINTER | 0x11 | UOP_TYPE_BARRIER)
#define UOP_STORE_P_IMM           (UOP_TYPE_PARAMS_IMM     | 0x12)
#define UOP_STORE_P_IMM_8         (UOP_TYPE_PARAMS_IMM     | 0x13)

#define UOP_MAX 0x14

#define UOP_MASK 0xffff

typedef struct uop_t
{
        uint32_t type;
        uint16_t dest_reg_a;
        uint16_t src_reg_a;
        uint16_t src_reg_b;
        uint16_t src_reg_c;
        uint32_t imm_data;
        void *p;
} uop_t;

#define UOP_NR_MAX 4096

typedef struct ir_data_t
{
        uop_t uops[UOP_NR_MAX];
        int wr_pos;
} ir_data_t;

static inline uop_t *uop_alloc(ir_data_t *ir)
{
        uop_t *uop;
        
        if (ir->wr_pos >= UOP_NR_MAX)
                fatal("Exceeded uOP max\n");
        
        uop = &ir->uops[ir->wr_pos++];
        
        uop->dest_reg_a = UOP_REG_UNUSED;
        uop->src_reg_a = UOP_REG_UNUSED;
        uop->src_reg_b = UOP_REG_UNUSED;
        uop->src_reg_c = UOP_REG_UNUSED;

        return uop;
}

static inline void uop_gen_reg_src1(uint32_t uop_type, ir_data_t *ir, int arg, uint16_t src_reg_a)
{
        uop_t *uop = uop_alloc(ir);
        
        uop->type = uop_type;
        uop->src_reg_a = src_reg_a;
}

static inline void uop_gen_imm(uint32_t uop_type, ir_data_t *ir, uint32_t imm)
{
        uop_t *uop = uop_alloc(ir);
        
        uop->type = uop_type;
        uop->imm_data = imm;
}

static inline void uop_gen_pointer(uint32_t uop_type, ir_data_t *ir, void *p)
{
        uop_t *uop = uop_alloc(ir);
        
        uop->type = uop_type;
        uop->p = p;
}

static inline void uop_gen_pointer_imm(uint32_t uop_type, ir_data_t *ir, void *p, uint32_t imm)
{
        uop_t *uop = uop_alloc(ir);
        
        uop->type = uop_type;
        uop->p = p;
        uop->imm_data = imm;
}

#define uop_LOAD_FUNC_ARG_REG(ir, arg, reg)  uop_gen_reg_src1(UOP_LOAD_FUNC_ARG_0 + arg, ir, reg)

#define uop_LOAD_FUNC_ARG_IMM(ir, arg, imm)  uop_gen_imm(UOP_LOAD_FUNC_ARG_0_IMM + arg, ir, imm)

#define uop_CALL_FUNC(ir, p)             uop_gen_pointer(UOP_CALL_FUNC, ir, p)
#define uop_CALL_INSTRUCTION_FUNC(ir, p) uop_gen_pointer(UOP_CALL_INSTRUCTION_FUNC, ir, p)

#define uop_STORE_PTR_IMM(ir, p, imm)    uop_gen_pointer_imm(UOP_STORE_P_IMM, ir, p, imm)
#define uop_STORE_PTR_IMM_8(ir, p, imm)  uop_gen_pointer_imm(UOP_STORE_P_IMM_8, ir, p, imm)

#endif
