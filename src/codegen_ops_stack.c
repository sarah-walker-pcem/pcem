#include "ibm.h"

#include "x86.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_helpers.h"
#include "codegen_ops_misc.h"

uint32_t ropPUSH_r16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int sp_reg;

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        sp_reg = LOAD_SP_WITH_OFFSET(ir, -2);
        uop_MEM_STORE_REG(ir, IREG_SS_base, sp_reg, IREG_16(opcode & 7));
        SUB_SP(ir, 2);

        return op_pc;
}
uint32_t ropPUSH_r32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int sp_reg;

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        sp_reg = LOAD_SP_WITH_OFFSET(ir, -4);
        uop_MEM_STORE_REG(ir, IREG_SS_base, sp_reg, IREG_32(opcode & 7));
        SUB_SP(ir, 4);

        return op_pc;
}

uint32_t ropPOP_r16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);

        if (stack32)
                uop_MEM_LOAD_REG(ir, IREG_16(opcode & 7), IREG_SS_base, IREG_ESP);
        else
        {
                uop_MOVZX(ir, IREG_eaaddr, IREG_SP);
                uop_MEM_LOAD_REG(ir, IREG_16(opcode & 7), IREG_SS_base, IREG_eaaddr);
        }
        if ((opcode & 7) != REG_SP)
                ADD_SP(ir, 2);

        return op_pc;
}
uint32_t ropPOP_r32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);

        if (stack32)
                uop_MEM_LOAD_REG(ir, IREG_32(opcode & 7), IREG_SS_base, IREG_ESP);
        else
        {
                uop_MOVZX(ir, IREG_eaaddr, IREG_SP);
                uop_MEM_LOAD_REG(ir, IREG_32(opcode & 7), IREG_SS_base, IREG_eaaddr);
        }
        if ((opcode & 7) != REG_ESP)
                ADD_SP(ir, 4);

        return op_pc;
}

uint32_t ropPOP_W(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);

        if ((fetchdat & 0xc0) == 0xc0)
        {
                if (stack32)
                        uop_MEM_LOAD_REG(ir, IREG_16(fetchdat & 7), IREG_SS_base, IREG_ESP);
                else
                {
                        uop_MOVZX(ir, IREG_eaaddr, IREG_SP);
                        uop_MEM_LOAD_REG(ir, IREG_16(fetchdat & 7), IREG_SS_base, IREG_eaaddr);
                }
        }
        else
        {
                x86seg *target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 2);
                codegen_check_seg_write(block, ir, target_seg);
                
                if (stack32)
                        uop_MEM_LOAD_REG(ir, IREG_temp0_W, IREG_SS_base, IREG_ESP);
                else
                {
                        uop_MOVZX(ir, IREG_temp0, IREG_SP);
                        uop_MEM_LOAD_REG(ir, IREG_temp0_W, IREG_SS_base, IREG_temp0);
                }

                uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_temp0_W);
        }

        if ((fetchdat & 0xc7) != (0xc0 | REG_SP))
                ADD_SP(ir, 2);

        return op_pc + 1;
}
uint32_t ropPOP_L(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);

        if ((fetchdat & 0xc0) == 0xc0)
        {
                if (stack32)
                        uop_MEM_LOAD_REG(ir, IREG_32(fetchdat & 7), IREG_SS_base, IREG_ESP);
                else
                {
                        uop_MOVZX(ir, IREG_eaaddr, IREG_SP);
                        uop_MEM_LOAD_REG(ir, IREG_32(fetchdat & 7), IREG_SS_base, IREG_eaaddr);
                }
        }
        else
        {
                x86seg *target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 4);
                codegen_check_seg_write(block, ir, target_seg);

                if (stack32)
                        uop_MEM_LOAD_REG(ir, IREG_temp0, IREG_SS_base, IREG_ESP);
                else
                {
                        uop_MOVZX(ir, IREG_temp0, IREG_SP);
                        uop_MEM_LOAD_REG(ir, IREG_temp0, IREG_SS_base, IREG_temp0);
                }

                uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_temp0);
        }

        if ((fetchdat & 0xc7) != (0xc0 | REG_ESP))
                ADD_SP(ir, 4);

        return op_pc + 1;
}

#define ROP_PUSH_SEG(seg) \
uint32_t ropPUSH_ ## seg ## _16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)   \
{                                                                                                                                       \
        int sp_reg;                                                                                                                     \
                                                                                                                                        \
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);                                                                                   \
        sp_reg = LOAD_SP_WITH_OFFSET(ir, -2);                                                                                           \
        uop_MEM_STORE_REG(ir, IREG_SS_base, sp_reg, IREG_ ## seg ## _seg_W);                                                            \
        SUB_SP(ir, 2);                                                                                                                  \
                                                                                                                                        \
        return op_pc;                                                                                                                   \
}                                                                                                                                       \
uint32_t ropPUSH_ ## seg ## _32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)   \
{                                                                                                                                       \
        int sp_reg;                                                                                                                     \
                                                                                                                                        \
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);                                                                                   \
        sp_reg = LOAD_SP_WITH_OFFSET(ir, -4);                                                                                           \
        uop_MOVZX(ir, IREG_temp0, IREG_ ## seg ## _seg_W);                                                                              \
        uop_MEM_STORE_REG(ir, IREG_SS_base, sp_reg, IREG_temp0);                                                                        \
        SUB_SP(ir, 4);                                                                                                                  \
                                                                                                                                        \
        return op_pc;                                                                                                                   \
}

#define ROP_POP_SEG(seg, rseg) \
uint32_t ropPOP_ ## seg ## _16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)    \
{                                                                                                                                       \
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);                                                                                   \
                                                                                                                                        \
        if (stack32)                                                                                                                    \
                uop_MEM_LOAD_REG(ir, IREG_temp0_W, IREG_SS_base, IREG_ESP);                                                             \
        else                                                                                                                            \
        {                                                                                                                               \
                uop_MOVZX(ir, IREG_eaaddr, IREG_SP);                                                                                    \
                uop_MEM_LOAD_REG(ir, IREG_temp0_W, IREG_SS_base, IREG_eaaddr);                                                          \
        }                                                                                                                               \
        uop_LOAD_SEG(ir, &rseg, IREG_temp0_W);                                                                                          \
        ADD_SP(ir, 2);                                                                                                                  \
                                                                                                                                        \
        return op_pc;                                                                                                                   \
}                                                                                                                                       \
uint32_t ropPOP_ ## seg ## _32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)    \
{                                                                                                                                       \
        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);                                                                                   \
                                                                                                                                        \
        if (stack32)                                                                                                                    \
                uop_MEM_LOAD_REG(ir, IREG_temp0_W, IREG_SS_base, IREG_ESP);                                                             \
        else                                                                                                                            \
        {                                                                                                                               \
                uop_MOVZX(ir, IREG_eaaddr, IREG_SP);                                                                                    \
                uop_MEM_LOAD_REG(ir, IREG_temp0_W, IREG_SS_base, IREG_eaaddr);                                                          \
        }                                                                                                                               \
        uop_LOAD_SEG(ir, &rseg, IREG_temp0_W);                                                                                          \
        ADD_SP(ir, 4);                                                                                                                  \
                                                                                                                                        \
        return op_pc;                                                                                                                   \
}


ROP_PUSH_SEG(CS)
ROP_PUSH_SEG(DS)
ROP_PUSH_SEG(ES)
ROP_PUSH_SEG(FS)
ROP_PUSH_SEG(GS)
ROP_PUSH_SEG(SS)
ROP_POP_SEG(DS, cpu_state.seg_ds)
ROP_POP_SEG(ES, cpu_state.seg_es)
ROP_POP_SEG(FS, cpu_state.seg_fs)
ROP_POP_SEG(GS, cpu_state.seg_gs)
