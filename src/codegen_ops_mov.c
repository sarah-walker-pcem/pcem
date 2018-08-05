#include "ibm.h"

#include "x86.h"
#include "386_common.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_mov.h"

uint32_t ropMOV_rb_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint8_t imm = fastreadb(cs + op_pc);

        uop_MOV_IMM(ir, IREG_8(opcode & 7), imm);

        return op_pc + 1;
}
uint32_t ropMOV_rw_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint16_t imm = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_16(opcode & 7), imm);

        return op_pc + 2;
}
uint32_t ropMOV_rl_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        fetchdat = fastreadl(cs + op_pc);

        uop_MOV_IMM(ir, IREG_32(opcode & 7), fetchdat);

        return op_pc + 4;
}



uint32_t ropMOV_b_r(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int dest_reg = fetchdat & 7;
                uop_MOV(ir, IREG_8(dest_reg), IREG_8(src_reg));
        }
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_write(block, ir, target_seg);
                uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_8(src_reg));
        }

        return op_pc + 1;
}
uint32_t ropMOV_w_r(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int dest_reg = fetchdat & 7;
                uop_MOV(ir, IREG_16(dest_reg), IREG_16(src_reg));
        }
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_write(block, ir, target_seg);
                uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_16(src_reg));
        }

        return op_pc + 1;
}
uint32_t ropMOV_l_r(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int src_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int dest_reg = fetchdat & 7;
                uop_MOV(ir, IREG_32(dest_reg), IREG_32(src_reg));
        }
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_write(block, ir, target_seg);
                uop_MEM_STORE_REG(ir, ireg_seg_base(target_seg), IREG_eaaddr, IREG_32(src_reg));
        }

        return op_pc + 1;
}
uint32_t ropMOV_r_b(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = fetchdat & 7;
                uop_MOV(ir, IREG_8(dest_reg), IREG_8(src_reg));
        }
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_8(dest_reg), ireg_seg_base(target_seg), IREG_eaaddr);
        }

        return op_pc + 1;
}
uint32_t ropMOV_r_w(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = fetchdat & 7;
                uop_MOV(ir, IREG_16(dest_reg), IREG_16(src_reg));
        }
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_16(dest_reg), ireg_seg_base(target_seg), IREG_eaaddr);
        }

        return op_pc + 1;
}
uint32_t ropMOV_r_l(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        int dest_reg = (fetchdat >> 3) & 7;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int src_reg = fetchdat & 7;
                uop_MOV(ir, IREG_32(dest_reg), IREG_32(src_reg));
        }
        else
        {
                x86seg *target_seg;

                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_read(block, ir, target_seg);
                uop_MEM_LOAD_REG(ir, IREG_32(dest_reg), ireg_seg_base(target_seg), IREG_eaaddr);
        }

        return op_pc + 1;
}

uint32_t ropMOV_AL_abs(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        codegen_check_seg_read(block, ir, op_ea_seg);
        uop_MEM_LOAD_ABS(ir, IREG_AL, ireg_seg_base(op_ea_seg), addr);

        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
uint32_t ropMOV_AX_abs(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        codegen_check_seg_read(block, ir, op_ea_seg);
        uop_MEM_LOAD_ABS(ir, IREG_AX, ireg_seg_base(op_ea_seg), addr);

        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
uint32_t ropMOV_EAX_abs(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        codegen_check_seg_read(block, ir, op_ea_seg);
        uop_MEM_LOAD_ABS(ir, IREG_EAX, ireg_seg_base(op_ea_seg), addr);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}

uint32_t ropMOV_abs_AL(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        codegen_check_seg_write(block, ir, op_ea_seg);
        uop_MEM_STORE_ABS(ir, ireg_seg_base(op_ea_seg), addr, IREG_AL);

        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
uint32_t ropMOV_abs_AX(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        codegen_check_seg_write(block, ir, op_ea_seg);
        uop_MEM_STORE_ABS(ir, ireg_seg_base(op_ea_seg), addr, IREG_AX);

        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
uint32_t ropMOV_abs_EAX(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
        codegen_check_seg_write(block, ir, op_ea_seg);
        uop_MEM_STORE_ABS(ir, ireg_seg_base(op_ea_seg), addr, IREG_EAX);

        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}

uint32_t ropMOV_b_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        x86seg *target_seg;
        uint8_t imm;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int dest_reg = fetchdat & 7;

                imm = fastreadb(cs + op_pc + 1);
                uop_MOV_IMM(ir, IREG_8(dest_reg), imm);
        }
        else
        {
                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_write(block, ir, target_seg);
                imm = fastreadb(cs + op_pc + 1);
                uop_MEM_STORE_IMM_8(ir, ireg_seg_base(target_seg), IREG_eaaddr, imm);
        }

        return op_pc + 2;
}
uint32_t ropMOV_w_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        x86seg *target_seg;
        uint16_t imm;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int dest_reg = fetchdat & 7;

                imm = fastreadw(cs + op_pc + 1);
                uop_MOV_IMM(ir, IREG_16(dest_reg), imm);
        }
        else
        {
                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_write(block, ir, target_seg);
                imm = fastreadw(cs + op_pc + 1);
                uop_MEM_STORE_IMM_16(ir, ireg_seg_base(target_seg), IREG_eaaddr, imm);
        }

        return op_pc + 3;
}
uint32_t ropMOV_l_imm(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc)
{
        x86seg *target_seg;
        uint32_t imm;

        if ((fetchdat & 0xc0) == 0xc0)
        {
                int dest_reg = fetchdat & 7;

                imm = fastreadl(cs + op_pc + 1);
                uop_MOV_IMM(ir, IREG_32(dest_reg), imm);
        }
        else
        {
                uop_MOV_IMM(ir, IREG_oldpc, cpu_state.oldpc);
                target_seg = codegen_generate_ea(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32, 0);
                codegen_check_seg_write(block, ir, target_seg);
                imm = fastreadl(cs + op_pc + 1);
                uop_MEM_STORE_IMM_32(ir, ireg_seg_base(target_seg), IREG_eaaddr, imm);
        }

        return op_pc + 5;
}
