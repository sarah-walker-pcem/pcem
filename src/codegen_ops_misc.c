#include "ibm.h"

#include "x86.h"
#include "x86_flags.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_helpers.h"
#include "codegen_ops_misc.h"

uint32_t ropLEA_16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
                return 0;

        codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
        uop_MOV(ir, IREG_16(dest_reg), IREG_eaaddr_W);
        
        return op_pc + 1;
}
uint32_t ropLEA_32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;
        
        if ((fetchdat & 0xc0) == 0xc0)
                return 0;

        codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
        uop_MOV(ir, IREG_32(dest_reg), IREG_eaaddr);

        return op_pc + 1;
}

uint32_t ropF6(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        x86seg *target_seg = NULL;
        uint8_t imm_data;
        int reg;
        
        if (fetchdat & 0x20)
                return 0;

        if ((fetchdat & 0xc0) == 0xc0)
                reg = IREG_8(fetchdat & 7);
        else
        {
                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                if ((fetchdat & 0x30) == 0x10) /*NEG/NOT*/
                        codegen_check_seg_write(block, ir, target_seg);
                else
                        codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_temp0_B, ireg_seg_base(target_seg), IREG_eaaddr);
                reg = IREG_temp0_B;
        }

        switch (fetchdat & 0x38)
        {
                case 0x00: case 0x08: /*TEST*/
                imm_data = fastreadb(cs + op_pc + 1);
                
                uop_AND_IMM(ir, IREG_flags_res_B, reg, imm_data);
                uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_B);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN8);

                codegen_flags_changed = 1;
                return op_pc+2;
                
                case 0x10: /*NOT*/
                uop_XOR_IMM(ir, reg, reg, 0xff);
                if ((fetchdat & 0xc0) != 0xc0)
                        uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, reg);

                codegen_flags_changed = 1;
                return op_pc+1;

                case 0x18: /*NEG*/
                uop_MOV_IMM(ir, IREG_temp1_B, 0);

                if ((fetchdat & 0xc0) == 0xc0)
                {
                        uop_MOVZX(ir, IREG_flags_op2, reg);
                        uop_SUB(ir, IREG_temp1_B, IREG_temp1_B, reg);
                        uop_MOVZX(ir, IREG_flags_res, IREG_temp1_B);
                        uop_MOV(ir, reg, IREG_temp1_B);
                }
                else
                {
                        uop_SUB(ir, IREG_temp1_B, IREG_temp1_B, reg);
                        uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_temp1_B);
                        uop_MOVZX(ir, IREG_flags_op2, IREG_temp0_B);
                        uop_MOVZX(ir, IREG_flags_res, IREG_temp1_B);
                }
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB8);
                uop_MOV_IMM(ir, IREG_flags_op1, 0);

                codegen_flags_changed = 1;
                return op_pc+1;
        }
        return 0;
}
uint32_t ropF7_16(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        x86seg *target_seg = NULL;
        uint16_t imm_data;
        int reg;

        if (fetchdat & 0x20)
                return 0;

        if ((fetchdat & 0xc0) == 0xc0)
                reg = IREG_16(fetchdat & 7);
        else
        {
                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                if ((fetchdat & 0x30) == 0x10) /*NEG/NOT*/
                        codegen_check_seg_write(block, ir, target_seg);
                else
                        codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_temp0_W, ireg_seg_base(target_seg), IREG_eaaddr);
                reg = IREG_temp0_W;
        }

        switch (fetchdat & 0x38)
        {
                case 0x00: case 0x08: /*TEST*/
                imm_data = fastreadw(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_flags_res_W, reg, imm_data);
                uop_MOVZX(ir, IREG_flags_res, IREG_flags_res_W);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN16);

                codegen_flags_changed = 1;
                return op_pc+3;

                case 0x10: /*NOT*/
                uop_XOR_IMM(ir, reg, reg, 0xffff);
                if ((fetchdat & 0xc0) != 0xc0)
                        uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, reg);

                codegen_flags_changed = 1;
                return op_pc+1;

                case 0x18: /*NEG*/
                uop_MOV_IMM(ir, IREG_temp1_W, 0);

                if ((fetchdat & 0xc0) == 0xc0)
                {
                        uop_MOVZX(ir, IREG_flags_op2, reg);
                        uop_SUB(ir, IREG_temp1_W, IREG_temp1_W, reg);
                        uop_MOVZX(ir, IREG_flags_res, IREG_temp1_W);
                        uop_MOV(ir, reg, IREG_temp1_W);
                }
                else
                {
                        uop_SUB(ir, IREG_temp1_W, IREG_temp1_W, reg);
                        uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_temp1_W);
                        uop_MOVZX(ir, IREG_flags_op2, IREG_temp0_W);
                        uop_MOVZX(ir, IREG_flags_res, IREG_temp1_W);
                }
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB16);
                uop_MOV_IMM(ir, IREG_flags_op1, 0);

                codegen_flags_changed = 1;
                return op_pc+1;
        }
        return 0;
}
uint32_t ropF7_32(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        x86seg *target_seg = NULL;
        uint32_t imm_data;
        int reg;

        if (fetchdat & 0x20)
                return 0;

        if ((fetchdat & 0xc0) == 0xc0)
                reg = IREG_32(fetchdat & 7);
        else
        {
                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                if ((fetchdat & 0x30) == 0x10) /*NEG/NOT*/
                        codegen_check_seg_write(block, ir, target_seg);
                else
                        codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_temp0, ireg_seg_base(target_seg), IREG_eaaddr);
                reg = IREG_temp0;
        }

        switch (fetchdat & 0x38)
        {
                case 0x00: case 0x08: /*TEST*/
                imm_data = fastreadl(cs + op_pc + 1);

                uop_AND_IMM(ir, IREG_flags_res, reg, imm_data);
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_ZN32);

                codegen_flags_changed = 1;
                return op_pc+5;

                case 0x10: /*NOT*/
                uop_XOR_IMM(ir, reg, reg, 0xffffffff);
                if ((fetchdat & 0xc0) != 0xc0)
                        uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, reg);

                codegen_flags_changed = 1;
                return op_pc+1;

                case 0x18: /*NEG*/
                uop_MOV_IMM(ir, IREG_temp1, 0);

                if ((fetchdat & 0xc0) == 0xc0)
                {
                        uop_MOV(ir, IREG_flags_op2, reg);
                        uop_SUB(ir, IREG_temp1, IREG_temp1, reg);
                        uop_MOV(ir, IREG_flags_res, IREG_temp1);
                        uop_MOV(ir, reg, IREG_temp1);
                }
                else
                {
                        uop_SUB(ir, IREG_temp1, IREG_temp1, reg);
                        uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_temp1);
                        uop_MOV(ir, IREG_flags_op2, IREG_temp0);
                        uop_MOV(ir, IREG_flags_res, IREG_temp1);
                }
                uop_MOV_IMM(ir, IREG_flags_op, FLAGS_SUB32);
                uop_MOV_IMM(ir, IREG_flags_op1, 0);

                codegen_flags_changed = 1;
                return op_pc+1;
        }
        return 0;
}

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
                return op_pc+1;
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
                return op_pc+1;
        }
        return 0;
}

uint32_t ropNOP(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        return op_pc;
}
