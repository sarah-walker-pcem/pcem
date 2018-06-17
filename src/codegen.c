#include "ibm.h"
#include "x86_ops.h"
#include "mem.h"
#include "codegen.h"
#include "x86.h"

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


void codegen_generate_call(uint8_t opcode, OpFn op, uint32_t fetchdat, uint32_t new_pc, uint32_t old_pc)
{
        codeblock_t *block = &codeblock[block_current];
        ir_data_t *ir = codegen_get_ir_data();
        uint32_t op_pc = new_pc;
        OpFn *op_table = x86_dynarec_opcodes;
        int opcode_shift = 0;
        int opcode_mask = 0x3ff;
        uint32_t op_32 = use32;


        //pclog("%04x:%08x : %02x\n", CS, new_pc, opcode);
        op = op_table[((opcode >> opcode_shift) | op_32) & opcode_mask];

        uop_STORE_PTR_IMM(ir, &cpu_state.pc, op_pc);
        uop_STORE_PTR_IMM(ir, &cpu_state.oldpc, old_pc);
        uop_STORE_PTR_IMM(ir, &cpu_state.op32, op_32);
        uop_STORE_PTR_IMM(ir, &cpu_state.ea_seg, (uintptr_t)&_ds);
        uop_STORE_PTR_IMM_8(ir, &cpu_state.ssegs, 0);
        uop_LOAD_FUNC_ARG_IMM(ir, 0, fetchdat);
        uop_CALL_INSTRUCTION_FUNC(ir, op);

        //codegen_block_ins++;
        
        block->ins++;

	if (block->ins >= 50)
		CPU_BLOCK_END();

//        addbyte(0xE8); /*CALL*/
//        addlong(((uint8_t *)codegen_debug - (uint8_t *)(&block->data[block_pos + 4])));

        codegen_endpc = (cs + cpu_state.pc) + 8;
}
