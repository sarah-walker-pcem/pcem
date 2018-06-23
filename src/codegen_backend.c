#include <stdlib.h>
#include "ibm.h"
#include "cpu.h"
#include "x86.h"
#include "x86_flags.h"
#include "x86_ops.h"
#include "x87.h"
#include "mem.h"

#include "386_common.h"

#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_ir.h"
#include "codegen_reg.h"

int codegen_flat_ds, codegen_flat_ss;
int mmx_ebx_ecx_loaded;
int codegen_flags_changed = 0;
int codegen_fpu_entered = 0;
int codegen_mmx_entered = 0;
int codegen_fpu_loaded_iq[8];
x86seg *op_ea_seg;
int op_ssegs;
uint32_t op_old_pc;

uint32_t recomp_page = -1;

int block_current = 0;
static int block_num;
int block_pos;

int cpu_recomp_flushes, cpu_recomp_flushes_latched;
int cpu_recomp_evicted, cpu_recomp_evicted_latched;
int cpu_recomp_reuse, cpu_recomp_reuse_latched;
int cpu_recomp_removed, cpu_recomp_removed_latched;

uint32_t codegen_endpc;

int codegen_block_cycles;
static int codegen_block_ins;
static int codegen_block_full_ins;

static uint32_t last_op32;
static x86seg *last_ea_seg;
static int last_ssegs;


void codegen_init()
{
        codegen_backend_init();
}

void codegen_reset()
{
        int c;
        
        memset(codeblock, 0, BLOCK_SIZE * sizeof(codeblock_t));
        memset(codeblock_hash, 0, HASH_SIZE * sizeof(codeblock_t *));
        mem_reset_page_blocks();
        
        for (c = 0; c < BLOCK_SIZE; c++)
                codeblock[c].pc = BLOCK_PC_INVALID;
}

void dump_block()
{
/*        codeblock_t *block = pages[0x119000 >> 12].block;

        pclog("dump_block:\n");
        while (block)
        {
                uint32_t start_pc = (block->pc & 0xffc) | (block->phys & ~0xfff);
                uint32_t end_pc = (block->endpc & 0xffc) | (block->phys & ~0xfff);
                pclog(" %p : %08x-%08x  %08x-%08x %p %p\n", (void *)block, start_pc, end_pc,  block->pc, block->endpc, (void *)block->prev, (void *)block->next);
                if (!block->pc)
                        fatal("Dead PC=0\n");
                
                block = block->next;
        }
        pclog("dump_block done\n");*/
}

static void add_to_block_list(codeblock_t *block)
{
        codeblock_t *block_prev = pages[block->phys >> 12].block[(block->phys >> 10) & 3];

        if (!block->page_mask)
                fatal("add_to_block_list - mask = 0\n");

        if (block_prev)
        {
                block->next = block_prev;
                block_prev->prev = block;
                pages[block->phys >> 12].block[(block->phys >> 10) & 3] = block;
        }
        else
        {
                block->next = NULL;
                pages[block->phys >> 12].block[(block->phys >> 10) & 3] = block;
        }

        if (block->next)
        {
                if (block->next->pc == BLOCK_PC_INVALID)
                        fatal("block->next->pc=BLOCK_PC_INVALID %p %p %x %x\n", (void *)block->next, (void *)codeblock, block_current, block_pos);
        }
        
        if (block->page_mask2)
        {
                block_prev = pages[block->phys_2 >> 12].block_2[(block->phys_2 >> 10) & 3];

                if (block_prev)
                {
                        block->next_2 = block_prev;
                        block_prev->prev_2 = block;
                        pages[block->phys_2 >> 12].block_2[(block->phys_2 >> 10) & 3] = block;
                }
                else
                {
                        block->next_2 = NULL;
                        pages[block->phys_2 >> 12].block_2[(block->phys_2 >> 10) & 3] = block;
                }
        }
}

static void remove_from_block_list(codeblock_t *block, uint32_t pc)
{
        if (!block->page_mask)
                return;

        if (block->prev)
        {
                block->prev->next = block->next;
                if (block->next)
                        block->next->prev = block->prev;
        }
        else
        {
                pages[block->phys >> 12].block[(block->phys >> 10) & 3] = block->next;
                if (block->next)
                        block->next->prev = NULL;
                else
                        mem_flush_write_page(block->phys, 0);
        }
        if (!block->page_mask2)
        {
                if (block->prev_2 || block->next_2)
                        fatal("Invalid block_2\n");
                return;
        }

        if (block->prev_2)
        {
                block->prev_2->next_2 = block->next_2;
                if (block->next_2)
                        block->next_2->prev_2 = block->prev_2;
        }
        else
        {
//                pclog(" pages.block_2=%p 3 %p %p\n", (void *)block->next_2, (void *)block, (void *)pages[block->phys_2 >> 12].block_2);
                pages[block->phys_2 >> 12].block_2[(block->phys_2 >> 10) & 3] = block->next_2;
                if (block->next_2)
                        block->next_2->prev_2 = NULL;
                else
                        mem_flush_write_page(block->phys_2, 0);
        }
}

static void delete_block(codeblock_t *block)
{
        uint32_t old_pc = block->pc;

        if (block == codeblock_hash[HASH(block->phys)])
                codeblock_hash[HASH(block->phys)] = NULL;

        if (block->pc == BLOCK_PC_INVALID)
                fatal("Deleting deleted block\n");
        block->pc = BLOCK_PC_INVALID;

        codeblock_tree_delete(block);
        remove_from_block_list(block, old_pc);
}

void codegen_check_flush(page_t *page, uint64_t mask, uint32_t phys_addr)
{
        struct codeblock_t *block = page->block[(phys_addr >> 10) & 3];

        while (block)
        {
                if (mask & block->page_mask)
                {
                        delete_block(block);
                        cpu_recomp_evicted++;
                }
                if (block == block->next)
                        fatal("Broken 1\n");
                block = block->next;
        }

        block = page->block_2[(phys_addr >> 10) & 3];
        
        while (block)
        {
                if (mask & block->page_mask2)
                {
                        delete_block(block);
                        cpu_recomp_evicted++;
                }
                if (block == block->next_2)
                        fatal("Broken 2\n");
                block = block->next_2;
        }
}

void codegen_block_init(uint32_t phys_addr)
{
        codeblock_t *block;
        page_t *page = &pages[phys_addr >> 12];
        
        if (!page->block[(phys_addr >> 10) & 3])
                mem_flush_write_page(phys_addr, cs+cpu_state.pc);

        block_current = (block_current + 1) & BLOCK_MASK;
        block = &codeblock[block_current];

//        if (block->pc == 0xb00b4ff5)
//                pclog("Init target block\n");
        if (block->pc != BLOCK_PC_INVALID)
        {
//                pclog("Reuse block : was %08x now %08x\n", block->pc, cs+pc);
                delete_block(block);
                cpu_recomp_reuse++;
        }
        block_num = HASH(phys_addr);
        codeblock_hash[block_num] = &codeblock[block_current];

        block->ins = 0;
        block->pc = cs + cpu_state.pc;
        block->_cs = cs;
        block->pnt = block_current;
        block->phys = phys_addr;
        block->dirty_mask = &page->dirty_mask[(phys_addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK];
        block->dirty_mask2 = NULL;
        block->next = block->prev = NULL;
        block->next_2 = block->prev_2 = NULL;
        block->page_mask = 0;
        block->flags = CODEBLOCK_STATIC_TOP;
        block->status = cpu_cur_status;
        
        block->was_recompiled = 0;

        recomp_page = block->phys & ~0xfff;
        
        codeblock_tree_add(block);
}

static ir_data_t *ir_data;

ir_data_t *codegen_get_ir_data()
{
        return ir_data;
}

void codegen_block_start_recompile(codeblock_t *block)
{
        page_t *page = &pages[block->phys >> 12];
        
        if (!page->block[(block->phys >> 10) & 3])
                mem_flush_write_page(block->phys, cs+cpu_state.pc);

        block_num = HASH(block->phys);
        block_current = block->pnt;

        if (block->pc != cs + cpu_state.pc || block->was_recompiled)
                fatal("Recompile to used block!\n");

        block->status = cpu_cur_status;

        cpu_block_end = 0;

//        pclog("New block %i for %08X   %03x\n", block_current, cs+pc, block_num);

        last_op32 = -1;
        last_ea_seg = NULL;
        last_ssegs = -1;
        
        codegen_block_cycles = 0;
        codegen_timing_block_start();
        
        codegen_block_ins = 0;
        codegen_block_full_ins = 0;

        recomp_page = block->phys & ~0xfff;
        
        codegen_flags_changed = 0;
        codegen_fpu_entered = 0;
        codegen_mmx_entered = 0;

        codegen_fpu_loaded_iq[0] = codegen_fpu_loaded_iq[1] = codegen_fpu_loaded_iq[2] = codegen_fpu_loaded_iq[3] =
        codegen_fpu_loaded_iq[4] = codegen_fpu_loaded_iq[5] = codegen_fpu_loaded_iq[6] = codegen_fpu_loaded_iq[7] = 0;
        
        _ds.checked = _es.checked = _fs.checked = _gs.checked = (cr0 & 1) ? 0 : 1;

        block->TOP = cpu_state.TOP;
        block->was_recompiled = 1;

        codegen_flat_ds = !(cpu_cur_status & CPU_STATUS_NOTFLATDS);
        codegen_flat_ss = !(cpu_cur_status & CPU_STATUS_NOTFLATSS);       
        
        ir_data = codegen_ir_init();
        codegen_reg_reset();
        codegen_generate_reset();
}


void codegen_block_remove()
{
        codeblock_t *block = &codeblock[block_current];

        delete_block(block);
        cpu_recomp_removed++;

        recomp_page = -1;
}

void codegen_block_generate_end_mask()
{
        codeblock_t *block = &codeblock[block_current];
        uint32_t start_pc;
        uint32_t end_pc;

        block->endpc = codegen_endpc;

        block->page_mask = 0;
        start_pc = (block->pc & 0x3ff) & ~15;
        if ((block->pc ^ block->endpc) & ~0x3ff)
                end_pc = 0x3ff & ~15;
        else
                end_pc = (block->endpc & 0x3ff) & ~15;
        if (end_pc < start_pc)
                end_pc = 0x3ff;
        start_pc >>= PAGE_MASK_SHIFT;
        end_pc >>= PAGE_MASK_SHIFT;
        
//        pclog("block_end: %08x %08x\n", start_pc, end_pc);
        for (; start_pc <= end_pc; start_pc++)
        {                
                block->page_mask |= ((uint64_t)1 << start_pc);
//                pclog("  %08x %llx\n", start_pc, block->page_mask);
        }
        
        pages[block->phys >> 12].code_present_mask[(block->phys >> 10) & 3] |= block->page_mask;

        block->phys_2 = -1;
        block->page_mask2 = 0;
        block->next_2 = block->prev_2 = NULL;
        if ((block->pc ^ block->endpc) & ~0x3ff)
        {
                block->phys_2 = get_phys_noabrt(block->endpc);
                if (block->phys_2 != -1)
                {
                        page_t *page_2 = &pages[block->phys_2 >> 12];

                        start_pc = 0;
                        end_pc = (block->endpc & 0x3ff) >> PAGE_MASK_SHIFT;
                        for (; start_pc <= end_pc; start_pc++)
                                block->page_mask2 |= ((uint64_t)1 << start_pc);
                        page_2->code_present_mask[(block->phys_2 >> 10) & 3] |= block->page_mask2;
                
                        if (!pages[block->phys_2 >> 12].block_2[(block->phys_2 >> 10) & 3])
                                mem_flush_write_page(block->phys_2, block->endpc);

                        if (!block->page_mask2)
                                fatal("!page_mask2\n");
                        if (block->next_2)
                        {
//                        pclog("  next_2->pc=%08x\n", block->next_2->pc);
                                if (block->next_2->pc == BLOCK_PC_INVALID)
                                        fatal("block->next_2->pc=BLOCK_PC_INVALID %p\n", (void *)block->next_2);
                        }

                        block->dirty_mask2 = &page_2->dirty_mask[(block->phys_2 >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK];
                }
        }

//        pclog("block_end: %08x %08x %016llx\n", block->pc, block->endpc, block->page_mask);
        recomp_page = -1;
}

void codegen_block_end()
{
        codeblock_t *block = &codeblock[block_current];

        codegen_block_generate_end_mask();
        add_to_block_list(block);
}

void codegen_block_end_recompile(codeblock_t *block)
{
        codegen_timing_block_end();

        remove_from_block_list(block, block->pc);
        block->next = block->prev = NULL;
        block->next_2 = block->prev_2 = NULL;
        codegen_block_generate_end_mask();
        add_to_block_list(block);
//        pclog("End block %i\n", block_num);

        if (!(block->flags & CODEBLOCK_HAS_FPU))
                block->flags &= ~CODEBLOCK_STATIC_TOP;

        codegen_ir_compile(ir_data, block);
}

void codegen_flush()
{
        return;
}
