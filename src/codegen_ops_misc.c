#include "ibm.h"

#include "x86.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_helpers.h"
#include "codegen_ops_misc.h"

uint32_t ropFF_16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg, sp_reg;
        
        if ((fetchdat & 0x38) != 0x10 && (fetchdat & 0x38) != 0x20 && (fetchdat & 0x38) != 0x30)
                return 0;
                
        if ((fetchdat & 0xc0) == 0xc0)
                src_reg = IREG_16(fetchdat & 7);
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_temp0_W, ireg_seg_base(target_seg), IREG_eaaddr);
                src_reg = IREG_temp0_W;
        }
        
        switch (fetchdat & 0x38)
        {
                case 0x10: /*CALL*/
                if ((fetchdat & 0xc0) == 0xc0)
                        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                sp_reg = LOAD_SP_WITH_OFFSET(ir, -2);
                uop_MEM_STORE_IMM_16(ir, IREG_SS_base, sp_reg, op_pc + 1);
                SUB_SP(ir, 2);
                uop_MOVZX(ir, IREG_pc, src_reg);
                return -1;
                
                case 0x20: /*JMP*/
                uop_MOVZX(ir, IREG_pc, src_reg);
                return -1;

                case 0x30: /*PUSH*/
                if ((fetchdat & 0xc0) == 0xc0)
                        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                sp_reg = LOAD_SP_WITH_OFFSET(ir, -2);
                uop_MEM_STORE_REG(ir, IREG_SS_base, sp_reg, src_reg);
                SUB_SP(ir, 2);
                return -1;
        }
        return 0;
}

uint32_t ropFF_32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg, sp_reg;

        if ((fetchdat & 0x38) != 0x10 && (fetchdat & 0x38) != 0x20 && (fetchdat & 0x38) != 0x30)
                return 0;

        if ((fetchdat & 0xc0) == 0xc0)
                src_reg = IREG_32(fetchdat & 7);
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_temp0, ireg_seg_base(target_seg), IREG_eaaddr);
                src_reg = IREG_temp0;
        }

        switch (fetchdat & 0x38)
        {
                case 0x10: /*CALL*/
                if ((fetchdat & 0xc0) == 0xc0)
                        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                sp_reg = LOAD_SP_WITH_OFFSET(ir, -4);
                uop_MEM_STORE_IMM_32(ir, IREG_SS_base, sp_reg, op_pc + 1);
                SUB_SP(ir, 4);
                uop_MOV(ir, IREG_pc, src_reg);
                return -1;

                case 0x20: /*JMP*/
                uop_MOV(ir, IREG_pc, src_reg);
                return -1;

                case 0x30: /*PUSH*/
                if ((fetchdat & 0xc0) == 0xc0)
                        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                sp_reg = LOAD_SP_WITH_OFFSET(ir, -4);
                uop_MEM_STORE_REG(ir, IREG_SS_base, sp_reg, src_reg);
                SUB_SP(ir, 4);
                return -1;
        }
        return 0;
}
