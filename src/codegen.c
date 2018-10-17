#include "ibm.h"
#include "x86_ops.h"
#include "mem.h"
#include "codegen.h"
#include "x86.h"

#include "386_common.h"

#include "codegen_accumulate.h"
#include "codegen_backend.h"
#include "codegen_ir.h"
#include "codegen_ops.h"

int has_ea;

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
        has_ea = 0;
}

void codegen_check_seg_read(codeblock_t *block, ir_data_t *ir, x86seg *seg)
{
        /*Segments always valid in real/V86 mode*/
        if (!(cr0 & 1) || (eflags & VM_FLAG))
                return;
        /*CS and SS must always be valid*/
        if (seg == &cpu_state.seg_cs || seg == &cpu_state.seg_ss)
                return;
        if (seg->checked)
                return;
        if (seg == &cpu_state.seg_ds && codegen_flat_ds && !(cpu_cur_status & CPU_STATUS_NOTFLATDS))
                return;

        uop_CMP_IMM_JZ(ir, ireg_seg_base(seg), (uint32_t)-1, &block->data[BLOCK_GPF_OFFSET]);

        seg->checked = 1;
}
void codegen_check_seg_write(codeblock_t *block, ir_data_t *ir, x86seg *seg)
{
        /*Segments always valid in real/V86 mode*/
        if (!(cr0 & 1) || (eflags & VM_FLAG))
                return;
        /*CS and SS must always be valid*/
        if (seg == &cpu_state.seg_cs || seg == &cpu_state.seg_ss)
                return;
        if (seg->checked)
                return;
        if (seg == &cpu_state.seg_ds && codegen_flat_ds && !(cpu_cur_status & CPU_STATUS_NOTFLATDS))
                return;

        uop_CMP_IMM_JZ(ir, ireg_seg_base(seg), (uint32_t)-1, &block->data[BLOCK_GPF_OFFSET]);

        seg->checked = 1;
}

static x86seg *codegen_generate_ea_16_long(ir_data_t *ir, x86seg *op_ea_seg, uint32_t fetchdat, int op_ssegs, uint32_t *op_pc)
{
//        pclog("codegen - mod=%i rm=%i reg=%i fetchdat=%08x\n", cpu_mod, cpu_rm, cpu_reg, fetchdat);
        if (!cpu_mod && cpu_rm == 6)
        {
                uint16_t addr = (fetchdat >> 8) & 0xffff;
                uop_MOV_IMM(ir, IREG_eaaddr, addr);
                (*op_pc) += 2;
        }
        else
        {
                int base_reg, index_reg, offset;

                switch (cpu_rm & 7)
                {
                        case 0: case 1: case 7:
                        base_reg = IREG_EBX;
                        break;
                        case 2: case 3: case 6:
                        base_reg = IREG_EBP;
                        break;
                        case 4:
                        base_reg = IREG_ESI;
                        break;
                        case 5:
                        base_reg = IREG_EDI;
                        break;
                }
                uop_MOV(ir, IREG_eaaddr, base_reg);

                if (!(cpu_rm & 4))
                {
                        if (!(cpu_rm & 1))
                                index_reg = IREG_ESI;
                        else
                                index_reg = IREG_EDI;

                        uop_ADD(ir, IREG_eaaddr, IREG_eaaddr, index_reg);
                }

                switch (cpu_mod)
                {
                        case 1:
                        offset = (int)(int8_t)((fetchdat >> 8) & 0xff);
                        uop_ADD_IMM(ir, IREG_eaaddr, IREG_eaaddr, offset);
                        (*op_pc)++;
                        break;
                        case 2:
                        offset = (fetchdat >> 8) & 0xffff;
                        uop_ADD_IMM(ir, IREG_eaaddr, IREG_eaaddr, offset);
                        (*op_pc) += 2;
                        break;
                }

                uop_AND_IMM(ir, IREG_eaaddr, IREG_eaaddr, 0xffff);

                if (mod1seg[cpu_rm] == &ss && !op_ssegs)
                {
                        op_ea_seg = &cpu_state.seg_ss;
                }
        }
        
        return op_ea_seg;
}

static x86seg *codegen_generate_ea_32_long(ir_data_t *ir, x86seg *op_ea_seg, uint32_t fetchdat, int op_ssegs, uint32_t *op_pc, int stack_offset)
{
        uint32_t new_eaaddr;

        if (cpu_rm == 4)
        {
                uint8_t sib = fetchdat >> 8;
                (*op_pc)++;

                switch (cpu_mod)
                {
                        case 0:
                        if ((sib & 7) == 5)
                        {
                                new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                                uop_MOV_IMM(ir, IREG_eaaddr, new_eaaddr);
                                (*op_pc) += 4;
                        }
                        else
                        {
                                uop_MOV(ir, IREG_eaaddr, sib & 7);
                        }
                        break;
                        case 1:
                        new_eaaddr = (uint32_t)(int8_t)((fetchdat >> 16) & 0xff);
                        uop_MOV_IMM(ir, IREG_eaaddr, new_eaaddr);
                        uop_ADD(ir, IREG_eaaddr, IREG_eaaddr, sib & 7);
                        (*op_pc)++;
                        break;
                        case 2:
                        new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                        uop_MOV_IMM(ir, IREG_eaaddr, new_eaaddr);
                        uop_ADD(ir, IREG_eaaddr, IREG_eaaddr, sib & 7);
                        (*op_pc) += 4;
                        break;
                }
                if (stack_offset && (sib & 7) == 4 && (cpu_mod || (sib & 7) != 5)) /*ESP*/
                {
                        uop_ADD_IMM(ir, IREG_eaaddr, IREG_eaaddr, stack_offset);
//                        addbyte(0x05);
//                        addlong(stack_offset);
                }
                if (((sib & 7) == 4 || (cpu_mod && (sib & 7) == 5)) && !op_ssegs)
                        op_ea_seg = &cpu_state.seg_ss;
                if (((sib >> 3) & 7) != 4)
                {
                        switch (sib >> 6)
                        {
                                case 0:
                                uop_ADD(ir, IREG_eaaddr, IREG_eaaddr, (sib >> 3) & 7);
                                break;
                                case 1:
                                uop_ADD_LSHIFT(ir, IREG_eaaddr, IREG_eaaddr, (sib >> 3) & 7, 1);
                                break;
                                case 2:
                                uop_ADD_LSHIFT(ir, IREG_eaaddr, IREG_eaaddr, (sib >> 3) & 7, 2);
                                break;
                                case 3:
                                uop_ADD_LSHIFT(ir, IREG_eaaddr, IREG_eaaddr, (sib >> 3) & 7, 3);
                                break;
                        }
                }
        }
        else
        {
                if (!cpu_mod && cpu_rm == 5)
                {
                        new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                        uop_MOV_IMM(ir, IREG_eaaddr, new_eaaddr);
                        (*op_pc) += 4;
                        return op_ea_seg;
                }
                uop_MOV(ir, IREG_eaaddr, cpu_rm);
                if (cpu_mod)
                {
                        if (cpu_rm == 5 && !op_ssegs)
                                op_ea_seg = &cpu_state.seg_ss;
                        if (cpu_mod == 1)
                        {
                                uop_ADD_IMM(ir, IREG_eaaddr, IREG_eaaddr, (uint32_t)(int8_t)(fetchdat >> 8));
                                (*op_pc)++;
                        }
                        else
                        {
                                new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                                uop_ADD_IMM(ir, IREG_eaaddr, IREG_eaaddr, new_eaaddr);
                                (*op_pc) += 4;
                        }
                }
        }
        return op_ea_seg;
}

x86seg *codegen_generate_ea(ir_data_t *ir, x86seg *op_ea_seg, uint32_t fetchdat, int op_ssegs, uint32_t *op_pc, uint32_t op_32, int stack_offset)
{
        cpu_mod = (fetchdat >> 6) & 3;
        cpu_reg = (fetchdat >> 3) & 7;
        cpu_rm = fetchdat & 7;

        if ((fetchdat & 0xc0) == 0xc0)
                return NULL;
        if (op_32 & 0x200)
                return codegen_generate_ea_32_long(ir, op_ea_seg, fetchdat, op_ssegs, op_pc, stack_offset);

        return codegen_generate_ea_16_long(ir, op_ea_seg, fetchdat, op_ssegs, op_pc);
}

static uint8_t opcode_modrm[256] =
{
        1, 1, 1, 1,  0, 0, 0, 0,  1, 1, 1, 1,  0, 0, 0, 0,  /*00*/
        1, 1, 1, 1,  0, 0, 0, 0,  1, 1, 1, 1,  0, 0, 0, 0,  /*10*/
        1, 1, 1, 1,  0, 0, 0, 0,  1, 1, 1, 1,  0, 0, 0, 0,  /*20*/
        1, 1, 1, 1,  0, 0, 0, 0,  1, 1, 1, 1,  0, 0, 0, 0,  /*30*/

        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*40*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*50*/
        0, 0, 1, 1,  0, 0, 0, 0,  0, 1, 0, 1,  0, 0, 0, 0,  /*60*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*70*/

        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*80*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*90*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*a0*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*b0*/

        1, 1, 0, 0,  1, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  /*c0*/
        1, 1, 1, 1,  0, 0, 0, 0,  1, 1, 1, 1,  1, 1, 1, 1,  /*d0*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*e0*/
        0, 0, 0, 0,  0, 0, 1, 1,  0, 0, 0, 0,  0, 0, 1, 1,  /*f0*/
};
static uint8_t opcode_0f_modrm[256] =
{
        1, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /*00*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /*10*/
        1, 1, 1, 1,  1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /*20*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /*30*/

        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, /*40*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /*50*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 0, 1, 1, /*60*/
        0, 1, 1, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 1, 1, /*70*/

        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /*80*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, /*90*/
        0, 0, 0, 1,  1, 1, 0, 0,  0, 0, 0, 1,  1, 1, 0, 1, /*a0*/
        1, 1, 1, 1,  1, 1, 1, 1,  0, 0, 1, 1,  1, 1, 1, 1, /*b0*/

        1, 1, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 0, /*c0*/
        0, 1, 1, 1,  0, 1, 0, 0,  1, 1, 0, 1,  1, 1, 0, 1, /*d0*/
        0, 1, 1, 0,  0, 1, 0, 0,  1, 1, 0, 1,  1, 1, 0, 1, /*e0*/
        0, 1, 1, 1,  0, 1, 0, 0,  1, 1, 1, 0,  1, 1, 1, 0  /*f0*/
};

void codegen_generate_call(uint8_t opcode, OpFn op, uint32_t fetchdat, uint32_t new_pc, uint32_t old_pc)
{
        codeblock_t *block = &codeblock[block_current];
        ir_data_t *ir = codegen_get_ir_data();
        uint32_t op_pc = new_pc;
        OpFn *op_table = x86_dynarec_opcodes;
        RecompOpFn *recomp_op_table = recomp_opcodes;
        int opcode_shift = 0;
        int opcode_mask = 0x3ff;
        uint32_t op_32 = use32;
        int over = 0;
        int test_modrm = 1;
        int pc_off = 0;
        
        op_ea_seg = &cpu_state.seg_ds;
        op_ssegs = 0;

        codegen_timing_start();

        while (!over)
        {
                switch (opcode)
                {
                        case 0x0f:
                        op_table = x86_dynarec_opcodes_0f;
                        recomp_op_table = recomp_opcodes_0f;
                        over = 1;
                        break;

                        case 0x26: /*ES:*/
                        op_ea_seg = &cpu_state.seg_es;
                        op_ssegs = 1;
                        break;
                        case 0x2e: /*CS:*/
                        op_ea_seg = &cpu_state.seg_cs;
                        op_ssegs = 1;
                        break;
                        case 0x36: /*SS:*/
                        op_ea_seg = &cpu_state.seg_ss;
                        op_ssegs = 1;
                        break;
                        case 0x3e: /*DS:*/
                        op_ea_seg = &cpu_state.seg_ds;
                        op_ssegs = 1;
                        break;
                        case 0x64: /*FS:*/
                        op_ea_seg = &cpu_state.seg_fs;
                        op_ssegs = 1;
                        break;
                        case 0x65: /*GS:*/
                        op_ea_seg = &cpu_state.seg_gs;
                        op_ssegs = 1;
                        break;

                        case 0x66: /*Data size select*/
                        op_32 = ((use32 & 0x100) ^ 0x100) | (op_32 & 0x200);
                        break;
                        case 0x67: /*Address size select*/
                        op_32 = ((use32 & 0x200) ^ 0x200) | (op_32 & 0x100);
                        break;

                        case 0xd8:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_d8_a32 : x86_dynarec_opcodes_d8_a16;
                        recomp_op_table = recomp_opcodes_d8;
                        opcode_shift = 3;
                        opcode_mask = 0x1f;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xd9:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_d9_a32 : x86_dynarec_opcodes_d9_a16;
                        recomp_op_table = recomp_opcodes_d9;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xda:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_da_a32 : x86_dynarec_opcodes_da_a16;
                        recomp_op_table = NULL;//recomp_opcodes_da;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xdb:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_db_a32 : x86_dynarec_opcodes_db_a16;
                        recomp_op_table = recomp_opcodes_db;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xdc:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_dc_a32 : x86_dynarec_opcodes_dc_a16;
                        recomp_op_table = recomp_opcodes_dc;
                        opcode_shift = 3;
                        opcode_mask = 0x1f;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xdd:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_dd_a32 : x86_dynarec_opcodes_dd_a16;
                        recomp_op_table = recomp_opcodes_dd;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xde:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_de_a32 : x86_dynarec_opcodes_de_a16;
                        recomp_op_table = recomp_opcodes_de;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;
                        case 0xdf:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_df_a32 : x86_dynarec_opcodes_df_a16;
                        recomp_op_table = recomp_opcodes_df;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        test_modrm = 0;
                        block->flags |= CODEBLOCK_HAS_FPU;
                        break;

                        case 0xf0: /*LOCK*/
                        break;

                        case 0xf2: /*REPNE*/
                        op_table = x86_dynarec_opcodes_REPNE;
                        recomp_op_table = NULL;//recomp_opcodes_REPNE;
                        break;
                        case 0xf3: /*REPE*/
                        op_table = x86_dynarec_opcodes_REPE;
                        recomp_op_table = NULL;//recomp_opcodes_REPE;
                        break;

                        default:
                        goto generate_call;
                }
                fetchdat = fastreadl(cs + op_pc);
                codegen_timing_prefix(opcode, fetchdat);
                if (cpu_state.abrt)
                        return;
                opcode = fetchdat & 0xff;
                if (!pc_off)
                        fetchdat >>= 8;

                op_pc++;
        }

generate_call:
        codegen_timing_opcode(opcode, fetchdat, op_32);

        codegen_accumulate(ACCREG_ins, 1);
        codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);
        codegen_block_cycles = 0;

        if ((op_table == x86_dynarec_opcodes &&
              ((opcode & 0xf0) == 0x70 || (opcode & 0xfc) == 0xe0 || opcode == 0xc2 ||
              (opcode & 0xfe) == 0xca || (opcode & 0xfc) == 0xcc || (opcode & 0xfc) == 0xe8 ||
              (opcode == 0xff && ((fetchdat & 0x38) >= 0x10 && (fetchdat & 0x38) < 0x30)))) ||
            (op_table == x86_dynarec_opcodes_0f && ((opcode & 0xf0) == 0x80)))
                codegen_accumulate_flush(ir);
//        pclog("%04x:%08x : %02x\n", CS, new_pc, opcode);
        if (recomp_op_table && recomp_op_table[(opcode | op_32) & 0x1ff])
        {
                uint32_t new_pc = recomp_op_table[(opcode | op_32) & 0x1ff](block, ir, opcode, fetchdat, op_32, op_pc);
                if (new_pc)
                {
                        if (new_pc != -1)
                                uop_MOV_IMM(ir, IREG_pc, new_pc);

                        codegen_endpc = (cs + cpu_state.pc) + 8;

                        return;
                }
        }
        
        if ((op_table == x86_dynarec_opcodes_REPNE || op_table == x86_dynarec_opcodes_REPE) && !op_table[opcode | op_32])
        {
                op_table = x86_dynarec_opcodes;
                recomp_op_table = recomp_opcodes;
        }

        op = op_table[((opcode >> opcode_shift) | op_32) & opcode_mask];

        if (!test_modrm ||
                (op_table == x86_dynarec_opcodes && opcode_modrm[opcode]) ||
                (op_table == x86_dynarec_opcodes_0f && opcode_0f_modrm[opcode]))
        {
                int stack_offset = 0;

                if (op_table == x86_dynarec_opcodes && opcode == 0x8f) /*POP*/
                        stack_offset = (op_32 & 0x100) ? 4 : 2;

                cpu_mod = (fetchdat >> 6) & 3;
                cpu_reg = (fetchdat >> 3) & 7;
                cpu_rm = fetchdat & 7;

                uop_MOV_IMM(ir, IREG_rm_mod_reg, cpu_rm | (cpu_mod << 8) | (cpu_reg << 16));

                op_pc += pc_off;
                if (cpu_mod != 3 && !(op_32 & 0x200))
                {
                        op_ea_seg = codegen_generate_ea_16_long(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc);
//                        has_ea = 1;
                }
                if (cpu_mod != 3 &&  (op_32 & 0x200))
                {
                        op_ea_seg = codegen_generate_ea_32_long(ir, op_ea_seg, fetchdat, op_ssegs, &op_pc, stack_offset);
//                        has_ea = 1;
                }
//                        op_ea_seg = codegen_generate_ea_32_long(op_ea_seg, fetchdat, op_ssegs, &op_pc, stack_offset);
                op_pc -= pc_off;
        }


        uop_MOV_IMM(ir, IREG_pc, op_pc+pc_off);
        uop_MOV_IMM(ir, IREG_oldpc, old_pc);
        if (op_32 != last_op_32)
                uop_MOV_IMM(ir, IREG_op32, op_32);
        if (op_ea_seg != last_op_ea_seg)
                uop_MOV_PTR(ir, IREG_ea_seg, (void *)op_ea_seg);
        if (op_ssegs != last_op_ssegs)
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
        
//        if (has_ea)
//                fatal("Has EA\n");
}
