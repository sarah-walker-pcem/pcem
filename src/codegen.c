#include "ibm.h"
#include "x86_ops.h"
#include "mem.h"
#include "codegen.h"
#include "x86.h"

#include "386_common.h"

#include "codegen_backend.h"
#include "codegen_ir.h"

codeblock_t *codeblock;
codeblock_t **codeblock_hash;

void (*codegen_timing_start)();
void (*codegen_timing_prefix)(uint8_t prefix, uint32_t fetchdat);
void (*codegen_timing_opcode)(uint8_t opcode, uint32_t fetchdat, int op_32);
void (*codegen_timing_block_start)();
void (*codegen_timing_block_end)();

void codegen_timing_set(codegen_timing_t *timing)
{
        codegen_timing_start = timing->start;
        codegen_timing_prefix = timing->prefix;
        codegen_timing_opcode = timing->opcode;
        codegen_timing_block_start = timing->block_start;
        codegen_timing_block_end = timing->block_end;
}

int codegen_in_recompile;

static int last_op_ssegs;
static x86seg *last_op_ea_seg;
static uint32_t last_op_32;

void codegen_generate_reset()
{
        last_op_ssegs = -1;
        last_op_ea_seg = NULL;
        last_op_32 = -1;
}

void codegen_generate_call(uint8_t opcode, OpFn op, uint32_t fetchdat, uint32_t new_pc, uint32_t old_pc)
{
        codeblock_t *block = &codeblock[block_current];
        ir_data_t *ir = codegen_get_ir_data();
        uint32_t op_pc = new_pc;
        OpFn *op_table = x86_dynarec_opcodes;
        int opcode_shift = 0;
        int opcode_mask = 0x3ff;
        uint32_t op_32 = use32;
        x86seg *op_ea_seg = &_ds;
        int op_ssegs = 0;
        int over = 0;

        op_ea_seg = &_ds;

        while (!over)
        {
                switch (opcode)
                {
                        case 0x0f:
                        op_table = x86_dynarec_opcodes_0f;
//                        recomp_op_table = recomp_opcodes_0f;
                        over = 1;
                        break;

                        case 0x26: /*ES:*/
                        op_ea_seg = &_es;
                        op_ssegs = 1;
                        break;
                        case 0x2e: /*CS:*/
                        op_ea_seg = &_cs;
                        op_ssegs = 1;
                        break;
                        case 0x36: /*SS:*/
                        op_ea_seg = &_ss;
                        op_ssegs = 1;
                        break;
                        case 0x3e: /*DS:*/
                        op_ea_seg = &_ds;
                        op_ssegs = 1;
                        break;
                        case 0x64: /*FS:*/
                        op_ea_seg = &_fs;
                        op_ssegs = 1;
                        break;
                        case 0x65: /*GS:*/
                        op_ea_seg = &_gs;
                        op_ssegs = 1;
                        break;

                        case 0x66: /*Data size select*/
                        op_32 = ((use32 & 0x100) ^ 0x100) | (op_32 & 0x200);
                        break;
                        case 0x67: /*Address size select*/
                        op_32 = ((use32 & 0x200) ^ 0x200) | (op_32 & 0x100);
                        break;

                        case 0xf0: /*LOCK*/
                        break;

                        case 0xf2: /*REPNE*/
                        op_table = x86_dynarec_opcodes_REPNE;
//                        recomp_op_table = recomp_opcodes_REPNE;
                        break;
                        case 0xf3: /*REPE*/
                        op_table = x86_dynarec_opcodes_REPE;
//                        recomp_op_table = recomp_opcodes_REPE;
                        break;

                        default:
                        goto generate_call;
                }
                fetchdat = fastreadl(cs + op_pc);
//                codegen_timing_prefix(opcode, fetchdat);
                if (cpu_state.abrt)
                        return;
                opcode = fetchdat & 0xff;
//                if (!pc_off)
                        fetchdat >>= 8;

                op_pc++;
        }

generate_call:
        //pclog("%04x:%08x : %02x\n", CS, new_pc, opcode);
        op = op_table[((opcode >> opcode_shift) | op_32) & opcode_mask];

        uop_MOV_IMM(ir, IREG_pc, op_pc);
        uop_MOV_IMM(ir, IREG_oldpc, old_pc);
        if (op_32 != last_op_32)
                uop_MOV_IMM(ir, IREG_op32, op_32);
/*Lazy ea_seg/ssegs updating isn't safe until EA calculation is implemented*/
//        if (op_ea_seg != last_op_ea_seg)
                uop_MOV_PTR(ir, IREG_ea_seg, (void *)op_ea_seg);
//        if (op_ssegs != last_op_ssegs)
                uop_MOV_IMM(ir, IREG_ssegs, op_ssegs);
        uop_LOAD_FUNC_ARG_IMM(ir, 0, fetchdat);
        uop_CALL_INSTRUCTION_FUNC(ir, op);

        last_op_32 = op_32;
        last_op_ea_seg = op_ea_seg;
        last_op_ssegs = op_ssegs;
        //codegen_block_ins++;
        
        block->ins++;

	if (block->ins >= 50)
		CPU_BLOCK_END();

        codegen_endpc = (cs + cpu_state.pc) + 8;
}
