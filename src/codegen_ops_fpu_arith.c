#include "ibm.h"

#include "x86.h"
#include "x86_flags.h"
#include "386_common.h"
#include "x87.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_fpu_arith.h"

uint32_t ropFADD(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FADD(ir, IREG_ST(0), IREG_ST(0), IREG_ST(src_reg));
        uop_MOV_IMM(ir, IREG_tag(0), TAG_VALID);
        
        return op_pc;
}
uint32_t ropFADDr(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FADD(ir, IREG_ST(dest_reg), IREG_ST(dest_reg), IREG_ST(0));
        uop_MOV_IMM(ir, IREG_tag(dest_reg), TAG_VALID);

        return op_pc;
}

uint32_t ropFDIV(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FDIV(ir, IREG_ST(0), IREG_ST(0), IREG_ST(src_reg));
        uop_MOV_IMM(ir, IREG_tag(0), TAG_VALID);

        return op_pc;
}
uint32_t ropFDIVr(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FDIV(ir, IREG_ST(dest_reg), IREG_ST(dest_reg), IREG_ST(0));
        uop_MOV_IMM(ir, IREG_tag(dest_reg), TAG_VALID);

        return op_pc;
}
uint32_t ropFDIVR(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FDIV(ir, IREG_ST(0), IREG_ST(src_reg), IREG_ST(0));
        uop_MOV_IMM(ir, IREG_tag(0), TAG_VALID);

        return op_pc;
}
uint32_t ropFDIVRr(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FDIV(ir, IREG_ST(dest_reg), IREG_ST(0), IREG_ST(dest_reg));
        uop_MOV_IMM(ir, IREG_tag(dest_reg), TAG_VALID);

        return op_pc;
}

uint32_t ropFMUL(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FMUL(ir, IREG_ST(0), IREG_ST(0), IREG_ST(src_reg));
        uop_MOV_IMM(ir, IREG_tag(0), TAG_VALID);

        return op_pc;
}
uint32_t ropFMULr(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FMUL(ir, IREG_ST(dest_reg), IREG_ST(dest_reg), IREG_ST(0));
        uop_MOV_IMM(ir, IREG_tag(dest_reg), TAG_VALID);

        return op_pc;
}

uint32_t ropFSUB(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FSUB(ir, IREG_ST(0), IREG_ST(0), IREG_ST(src_reg));
        uop_MOV_IMM(ir, IREG_tag(0), TAG_VALID);

        return op_pc;
}
uint32_t ropFSUBr(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FSUB(ir, IREG_ST(dest_reg), IREG_ST(dest_reg), IREG_ST(0));
        uop_MOV_IMM(ir, IREG_tag(dest_reg), TAG_VALID);

        return op_pc;
}
uint32_t ropFSUBR(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FSUB(ir, IREG_ST(0), IREG_ST(src_reg), IREG_ST(0));
        uop_MOV_IMM(ir, IREG_tag(0), TAG_VALID);

        return op_pc;
}
uint32_t ropFSUBRr(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = fetchdat & 7;

        uop_FP_ENTER(ir);
        uop_FSUB(ir, IREG_ST(dest_reg), IREG_ST(0), IREG_ST(dest_reg));
        uop_MOV_IMM(ir, IREG_tag(dest_reg), TAG_VALID);

        return op_pc;
}
