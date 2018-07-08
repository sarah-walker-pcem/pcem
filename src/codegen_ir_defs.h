#ifndef _CODEGEN_IR_DEFS_
#define _CODEGEN_IR_DEFS_

#include "codegen_reg.h"

#define UOP_REG(reg, size, version) ((reg) | (size) | (version << 8))

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
#define UOP_MOV_PTR               (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_POINTER | 0x20)
#define UOP_MOV_IMM               (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x21)
#define UOP_MOV                   (UOP_TYPE_PARAMS_REGS    | 0x22)
#define UOP_ADD                   (UOP_TYPE_PARAMS_REGS    | 0x30)
#define UOP_ADD_IMM               (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x31)
#define UOP_AND                   (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x32)
#define UOP_AND_IMM               (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x33)
#define UOP_ADD_LSHIFT            (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x34)
#define UOP_OR                    (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x35)
#define UOP_OR_IMM                (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x36)
#define UOP_SUB                   (UOP_TYPE_PARAMS_REGS    | 0x37)
#define UOP_SUB_IMM               (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x38)
#define UOP_XOR                   (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x39)
#define UOP_XOR_IMM               (UOP_TYPE_PARAMS_REGS | UOP_TYPE_PARAMS_IMM | 0x3a)

#define UOP_MAX 0x3b

#define UOP_MASK 0xffff

typedef struct uop_t
{
        uint32_t type;
        ir_reg_t dest_reg_a;
        ir_reg_t src_reg_a;
        ir_reg_t src_reg_b;
        uint32_t imm_data;
        void *p;
        ir_host_reg_t dest_reg_a_real;
        ir_host_reg_t src_reg_a_real, src_reg_b_real;
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
        
        uop->dest_reg_a = invalid_ir_reg;
        uop->src_reg_a = invalid_ir_reg;
        uop->src_reg_b = invalid_ir_reg;
//        uop->src_reg_c = invalid_ir_reg;

        return uop;
}

static inline void uop_gen_reg_src1(uint32_t uop_type, ir_data_t *ir, int arg, int src_reg_a)
{
        uop_t *uop = uop_alloc(ir);
        
        uop->type = uop_type;
        uop->src_reg_a = codegen_reg_read(src_reg_a);
}

static inline void uop_gen_reg_dst_imm(uint32_t uop_type, ir_data_t *ir, int dest_reg, uint32_t imm)
{
        uop_t *uop = uop_alloc(ir);

        uop->type = uop_type;
        uop->dest_reg_a = codegen_reg_write(dest_reg);
        uop->imm_data = imm;
}

static inline void uop_gen_reg_dst_pointer(uint32_t uop_type, ir_data_t *ir, int dest_reg, void *p)
{
        uop_t *uop = uop_alloc(ir);

        uop->type = uop_type;
        uop->dest_reg_a = codegen_reg_write(dest_reg);
        uop->p = p;
}

static inline void uop_gen_reg_dst_src1(uint32_t uop_type, ir_data_t *ir, int dest_reg, int src_reg)
{
        uop_t *uop = uop_alloc(ir);

        uop->type = uop_type;
        uop->src_reg_a = codegen_reg_read(src_reg);
        uop->dest_reg_a = codegen_reg_write(dest_reg);
}

static inline void uop_gen_reg_dst_src2(uint32_t uop_type, ir_data_t *ir, int dest_reg, int src_reg_a, int src_reg_b)
{
        uop_t *uop = uop_alloc(ir);

        uop->type = uop_type;
        uop->src_reg_a = codegen_reg_read(src_reg_a);
        uop->src_reg_b = codegen_reg_read(src_reg_b);
        uop->dest_reg_a = codegen_reg_write(dest_reg);
}

static inline void uop_gen_reg_dst_src2_imm(uint32_t uop_type, ir_data_t *ir, int dest_reg, int src_reg_a, int src_reg_b, uint32_t imm)
{
        uop_t *uop = uop_alloc(ir);

        uop->type = uop_type;
        uop->src_reg_a = codegen_reg_read(src_reg_a);
        uop->src_reg_b = codegen_reg_read(src_reg_b);
        uop->dest_reg_a = codegen_reg_write(dest_reg);
        uop->imm_data = imm;
}

static inline void uop_gen_reg_dst_src_imm(uint32_t uop_type, ir_data_t *ir, int dest_reg, int src_reg, uint32_t imm)
{
        uop_t *uop = uop_alloc(ir);

        uop->type = uop_type;
        uop->src_reg_a = codegen_reg_read(src_reg);
        uop->dest_reg_a = codegen_reg_write(dest_reg);
        uop->imm_data = imm;
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

#define uop_ADD(ir, dst_reg, src_reg_a, src_reg_b) uop_gen_reg_dst_src2(UOP_ADD, ir, dst_reg, src_reg_a, src_reg_b)
#define uop_ADD_IMM(ir, dst_reg, src_reg, imm)     uop_gen_reg_dst_src_imm(UOP_ADD_IMM, ir, dst_reg, src_reg, imm)
#define uop_ADD_LSHIFT(ir, dst_reg, src_reg_a, src_reg_b, shift) uop_gen_reg_dst_src2_imm(UOP_ADD_LSHIFT, ir, dst_reg, src_reg_a, src_reg_b, shift)
#define uop_AND(ir, dst_reg, src_reg_a, src_reg_b) uop_gen_reg_dst_src2(UOP_AND, ir, dst_reg, src_reg_a, src_reg_b)
#define uop_AND_IMM(ir, dst_reg, src_reg, imm)     uop_gen_reg_dst_src_imm(UOP_AND_IMM, ir, dst_reg, src_reg, imm)
#define uop_OR(ir, dst_reg, src_reg_a, src_reg_b)  uop_gen_reg_dst_src2(UOP_OR, ir, dst_reg, src_reg_a, src_reg_b)
#define uop_OR_IMM(ir, dst_reg, src_reg, imm)      uop_gen_reg_dst_src_imm(UOP_OR_IMM, ir, dst_reg, src_reg, imm)
#define uop_SUB(ir, dst_reg, src_reg_a, src_reg_b) uop_gen_reg_dst_src2(UOP_SUB, ir, dst_reg, src_reg_a, src_reg_b)
#define uop_SUB_IMM(ir, dst_reg, src_reg, imm)     uop_gen_reg_dst_src_imm(UOP_SUB_IMM, ir, dst_reg, src_reg, imm)
#define uop_XOR(ir, dst_reg, src_reg_a, src_reg_b) uop_gen_reg_dst_src2(UOP_XOR, ir, dst_reg, src_reg_a, src_reg_b)
#define uop_XOR_IMM(ir, dst_reg, src_reg, imm)     uop_gen_reg_dst_src_imm(UOP_XOR_IMM, ir, dst_reg, src_reg, imm)

#define uop_CALL_FUNC(ir, p)             uop_gen_pointer(UOP_CALL_FUNC, ir, p)
#define uop_CALL_INSTRUCTION_FUNC(ir, p) uop_gen_pointer(UOP_CALL_INSTRUCTION_FUNC, ir, p)

#define uop_MOV(ir, dst_reg, src_reg)    uop_gen_reg_dst_src1(UOP_MOV, ir, dst_reg, src_reg)
#define uop_MOV_IMM(ir, reg, imm)        uop_gen_reg_dst_imm(UOP_MOV_IMM, ir, reg, imm)
#define uop_MOV_PTR(ir, reg, p)          uop_gen_reg_dst_pointer(UOP_MOV_PTR, ir, reg, p)

#define uop_STORE_PTR_IMM(ir, p, imm)    uop_gen_pointer_imm(UOP_STORE_P_IMM, ir, p, imm)
#define uop_STORE_PTR_IMM_8(ir, p, imm)  uop_gen_pointer_imm(UOP_STORE_P_IMM_8, ir, p, imm)


void codegen_direct_read_32(codeblock_t *block, int host_reg, void *p);

void codegen_direct_write_8(codeblock_t *block, void *p, int host_reg);
void codegen_direct_write_32(codeblock_t *block, void *p, int host_reg);
void codegen_direct_write_ptr(codeblock_t *block, void *p, int host_reg);

#endif
