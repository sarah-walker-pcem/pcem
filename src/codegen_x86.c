#define CHECKSUM

#ifdef DYNAREC
#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

#include <stdlib.h>
#include "ibm.h"
#include "cpu.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"

#include "codegen.h"

#include "386_common.h"

#define BLOCK_EXIT_OFFSET 0x1ff0

#define CPU_BLOCK_END() cpu_block_end = 1




codeblock_t *codeblock;
codeblock_t **codeblock_hash;

typedef struct page_t
{
        codeblock_t *block;
} page_t;

page_t *pages;

uint8_t *codeblock_page_dirty;

static int block_current = 0;
static int block_num;
static int block_pos;

int cpu_recomp_flushes, cpu_recomp_flushes_latched;
int cpu_recomp_evicted, cpu_recomp_evicted_latched;
int cpu_recomp_reuse, cpu_recomp_reuse_latched;
int cpu_recomp_removed, cpu_recomp_removed_latched;

static uint32_t codegen_endpc;

int codegen_block_cycles;

static uint32_t last_op32;
static x86seg *last_ea_seg;
static int last_ssegs;

static inline void addbyte(uint8_t val)
{
//        pclog("Addbyte %02X at %i %i\n", val, block_pos, cpu_block_end);
        codeblock[block_current].data[block_pos++] = val;
        if (block_pos >= 8000)
        {
                CPU_BLOCK_END();
        }
}

static inline void addlong(uint32_t val)
{
//        pclog("Addlong %08X at %i %i\n", val, block_pos, cpu_block_end);
        *(uint32_t *)&codeblock[block_current].data[block_pos] = val;
        block_pos += 4;
        if (block_pos >= 8000)
        {
                CPU_BLOCK_END();
        }
}


void codegen_init()
{
        int c;
        
        codeblock = malloc(BLOCK_SIZE * sizeof(codeblock_t));
        codeblock_hash = malloc(HASH_SIZE * sizeof(codeblock_t *));
        pages = malloc(((256 * 1024 * 1024) >> 12) * sizeof(page_t));

        memset(codeblock, 0, sizeof(BLOCK_SIZE * sizeof(codeblock_t)));
        memset(codeblock_hash, 0, HASH_SIZE * sizeof(codeblock_t *));
        memset(pages, 0, ((256 * 1024 * 1024) >> 12) * sizeof(page_t));
        
        codeblock_page_dirty = malloc(1 << 20);
        memset(codeblock_page_dirty, 1, 1 << 20);
        
        pclog("Codegen is %p\n", (void *)pages[0xfab12 >> 12].block);
}

void dump_block()
{
        codeblock_t *block = pages[0x119000 >> 12].block;

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
        pclog("dump_block done\n");
}

int codegen_check_dirty(uint32_t phys_addr)
{
#ifdef CHECKSUM
        int has_evicted = 0;
        int dirty = 0;
        codeblock_t *block = pages[phys_addr >> 12].block;
//pclog("codegen_check_dirty %08x %08x\n", phys_addr, pages[phys_addr >> 12].block);
//        if ((phys_addr & ~0xfff) == 0x119000) pclog("dirty start\n");
        while (block)
        {
                uint32_t start_pc = (block->pc & 0xffc) | (block->phys & ~0xfff);
                uint32_t end_pc = (block->endpc & 0xffc) | (block->phys & ~0xfff);
                uint32_t checksum = 0;
//                if ((phys_addr & ~0xfff) == 0x119000) pclog("  %08x\n", block->pc);
                if (!block->pc)
                        fatal("!PC\n");

//                if ((phys_addr & ~0xfff) == 0x119000) dump_block();

//                if ((phys_addr & ~0xfff) == 0x119000) pclog(" %08x-%08x  %08x-%08x\n", start_pc, end_pc,  block->pc, block->endpc);
                if (end_pc > start_pc)
                {
                        for (; start_pc <= end_pc; start_pc += 4)
                                checksum += *(uint32_t *)&ram[start_pc];
                }
                else
                        checksum = ~block->checksum;
        
                if (checksum != block->checksum)
                {
//                        if ((phys_addr & ~0xfff) == 0x119000) pclog("   evict %08x %08x %p\n", checksum, block->checksum, (void *)block->prev);
                        if (block->pc == (cs+pc))
                                dirty = 1;

                        block->pc = 0;//xffffffff;

                        if (block->prev)
                        {
                                block->prev->next = block->next;
                                if (block->next)
                                        block->next->prev = block->prev;
                        }
                        else
                        {
                                pages[block->phys >> 12].block = block->next;
                                if (block->next)
                                        block->next->prev = NULL;
                        }
                        
                        has_evicted = 1;

                        codeblock_hash[HASH(block->phys)] = NULL;
                }
                        
                block = block->next;
        }
//        if ((phys_addr & ~0xfff) == 0x8f000) pclog("dirty end\n");
        if (has_evicted)
        {
                cpu_recomp_evicted++;

        }

        mem_flush_write_page(phys_addr, cs+pc);
                        
        codeblock_page_dirty[phys_addr >> 12] = 0;
        return dirty;
#else
        return 1;
#endif
}

void codegen_block_init(uint32_t phys_addr)
{
        codeblock_t *block;
        int has_evicted = 0;
//        pclog("block_init %08x:%08x hash=%04x %08x\n", cs, pc, HASH(cs + pc), phys_addr);
//if (phys_addr > 0x100000)
//        dump_block();
        if (codeblock_page_dirty[phys_addr >> 12])
        {
                codeblock_t *last_block = NULL;
                block = pages[phys_addr >> 12].block;
//                pages[phys_addr >> 12].block = NULL;
//                if ((phys_addr & ~0xfff) == 0x119000) pclog("Flush %08x\n", phys_addr);
//pclog("init start\n");
                while (block)
                {
                        codeblock_t *old_block = block;
                        codeblock_t *next_block = block->next;
                        
                        uint32_t start_pc = (block->pc & 0xffc) | (block->phys & ~0xfff);
                        uint32_t end_pc = (block->endpc & 0xffc) | (block->phys & ~0xfff);
                        uint32_t checksum = 0;

                        if (!block->pc)
                                fatal("!PC on init\n");

//if ((phys_addr & ~0xfff) == 0x119000) dump_block();
                        
//                        if ((phys_addr & ~0xfff) == 0x119000) pclog(" %p : %08x-%08x  %08x-%08x\n", (void *)block, start_pc, end_pc,  block->pc, block->endpc);
                        if (end_pc > start_pc)
                        {
                                for (; start_pc <= end_pc; start_pc += 4)
                                        checksum += *(uint32_t *)&ram[start_pc];
                        }
                        else
                                checksum = ~block->checksum;
                                
                        if (checksum != block->checksum)
                        {
                                if ((phys_addr & ~0xfff) == 0x119000) pclog("  evict %08x  %08x %08x  %p %p\n", block->pc, checksum, block->checksum, (void *)block->prev, (void *)last_block);
                                block->pc = 0;//xffffffff;

                                if (block->prev)
                                {
                                        block->prev->next = block->next;
                                        if (block->next)
                                                block->next->prev = block->prev;
                                }
                                else
                                {
                                        pages[block->phys >> 12].block = block->next;
                                        if (block->next)
                                                block->next->prev = NULL;
                                }

                                codeblock_hash[HASH(block->phys)] = NULL;

                                block->next = NULL;
                        }

                        block = next_block;
                        last_block = old_block;
                }
//if ((phys_addr & ~0xfff) == 0x119000) pclog("init end\n");
                codeblock_page_dirty[phys_addr >> 12] = 0;
                mem_flush_write_page(phys_addr, cs+pc);
        }
        
/*        if (has_evicted)
                cpu_recomp_evicted++;*/
//        pclog("Build %08x\n",cs+pc);
 
        block_current = (block_current + 1) & BLOCK_MASK;
        block = &codeblock[block_current];
        if (block->pc != 0)//xffffffff)
        {
//                if ((phys_addr & ~0xfff) == 0x119000) pclog("Block reuse - %04x old_pc=%08x new_pc=%08x\n", block_current, block->pc, cs+pc);
//                if ((phys_addr & ~0xfff) == 0x119000) dump_block();
                /*Block currently used - throw out contents*/
                if (block->prev)
                {
                        block->prev->next = block->next;
                        if (block->next)
                                block->next->prev = block->prev;
                }
                else
                {
                        pages[block->phys >> 12].block = block->next;
                        if (block->next)
                                block->next->prev = NULL;
                }
                codeblock_hash[HASH(block->phys)] = NULL;
                cpu_recomp_reuse++;
//                if ((phys_addr & ~0xfff) == 0x119000) pclog("Block reuse end\n");
//                if ((phys_addr & ~0xfff) == 0x119000) dump_block();
        }
        block_num = HASH(phys_addr);
        codeblock_hash[block_num] = &codeblock[block_current];
        block->ins = 0;
        block->pc = cs + pc;
        block->_cs = cs;
        block->pnt = block_current;
        block->phys = phys_addr;
        block->use32 = use32;
        block->next = block->prev = NULL;
        
        block_pos = 0;
        
        block_pos = BLOCK_EXIT_OFFSET; /*Exit code*/
        addbyte(0x83); /*ADDL $8,%esp*/
        addbyte(0xC4);
        addbyte(0x08);
        addbyte(0xC3); /*RET*/
        cpu_block_end = 0;
        block_pos = 0; /*Entry code*/
        addbyte(0x83); /*SUBL $8,%esp*/
        addbyte(0xEC);
        addbyte(0x08);

//        pclog("New block %i for %08X   %03x\n", block_current, cs+pc, block_num);

        last_op32 = -1;
        last_ea_seg = NULL;
        last_ssegs = -1;
        
        codegen_block_cycles = 0;
}

void codegen_block_remove()
{
        codeblock_t *block = &codeblock[block_current];
//if ((block->phys & ~0xfff) == 0x119000)  pclog("codegen_block_remove %08x\n", block->pc);
        codeblock_hash[block_num] = NULL;
        block->pc = 0;//xffffffff;
        cpu_recomp_removed++;
//        pclog("Remove block %i\n", block_num);
}

void codegen_block_end()
{
        codeblock_t *block = &codeblock[block_current];
        codeblock_t *block_prev = pages[block->phys >> 12].block;
        uint32_t start_pc = (block->pc & 0xffc) | (block->phys & ~0xfff);
        uint32_t end_pc = ((codegen_endpc + 3) & 0xffc) | (block->phys & ~0xfff);
        uint32_t checksum = 0;

        block->endpc = end_pc;

//if ((block->phys & ~0xfff) == 0x119000) pclog("codegen_block_end start\n");
//if ((block->phys & ~0xfff) == 0x119000) dump_block();
        if (block_prev)
        {
                block->next = block_prev;
                block_prev->prev = block;
                pages[block->phys >> 12].block = block;
        }
        else
        {
                block->next = NULL;
                pages[block->phys >> 12].block = block;
        }

//if ((block->phys & ~0xfff) == 0x119000) pclog("codegen_block_end end\n");
//if ((block->phys & ~0xfff) == 0x119000) dump_block();

#ifdef CHECKSUM        
        if (end_pc > start_pc)
        {
//                pclog("block_end %08x-%08x  %08x-%08x\n", start_pc, end_pc,  block->pc, block->endpc);
                for (; start_pc <= end_pc; start_pc += 4)
                        checksum += *(uint32_t *)&ram[start_pc];
//                pclog(" %08x\n", checksum);
        }
#endif
        
        block->checksum = checksum;
//        pclog("Checksum for %08x - %08x = %08x\n", codeblock_hash_pc[block_num] & ~3, end_pc, checksum);

        addbyte(0x81); /*SUB $codegen_block_cycles, cyclcs*/
        addbyte(0x2d);
        addlong((uint32_t)&cycles);
        addlong((uint32_t)codegen_block_cycles);

        addbyte(0x83); /*ADDL $8,%esp*/
        addbyte(0xC4);
        addbyte(0x08);
        addbyte(0xC3); /*RET*/
//        pclog("End block %i\n", block_num);
}

void codegen_flush()
{
        return;
}

void codegen_dirty(uint32_t addr)
{
        codeblock_page_dirty[addr >> 12] = 1;
//        pclog("codegen_dirty : %08x  %04x:%04x\n", addr, CS, pc);
//        if ((addr >> 12) == 0x152)
//                pclog("Dirty : %08x %08x\n", addr, cs+pc);
/*        int c;
        int offset = HASH(addr & ~0xfff);
//        pclog("codegen_dirty %08X\n", addr);
        for (c = 0; c < 0x1000; c++)
        {
                if (!((codeblock_hash_pc[(c + offset) & HASH_MASK] ^ addr) & ~0xfff))
                {
//                        pclog("Throwing out %08X\n", codeblock_hash_pc[(c + offset) & HASH_MASK]);
                        codeblock_hash_pc[(c + offset) & HASH_MASK] = 0xffffffff;
                }
        }*/
}

static int opcode_needs_tempc[256] =
{
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1,  /*00*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*10*/
        0, 0, 0, 0,  0, 0, 1, 1,  0, 0, 0, 0,  0, 0, 1, 1,  /*20*/
        0, 0, 0, 0,  0, 0, 1, 1,  0, 0, 0, 0,  0, 0, 1, 1,  /*30*/

        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*40*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*50*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*60*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*70*/

        1, 1, 1, 1,  1, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  /*80*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*90*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*a0*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*b0*/

        1, 1, 1, 1,  1, 1, 0, 0,  1, 1, 1, 1,  1, 1, 1, 1,  /*c0*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*d0*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*e0*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*f0*/
};

static int opcode_conditional_jump[256] = 
{
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1,  /*00*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*10*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*20*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*30*/

        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*40*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*50*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*60*/
        1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  /*70*/
        
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*80*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*90*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*a0*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*b0*/
        
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*c0*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*d0*/
        1, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*e0*/
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /*f0*/
};

void codegen_debug()
{
        if (output)
        {
                pclog("At %04x(%08x):%04x  %04x(%08x):%04x  EAX=%08x BX=%04x BP=%04x EDX=%08x\n", CS, cs, pc, SS, ss, ESP,  EAX, BX,BP,  EDX);
        }
}

void codegen_generate_call(uint8_t opcode, OpFn op, uint32_t fetchdat, uint32_t new_pc, uint32_t old_pc)
{
        codeblock_t *block = &codeblock[block_current];
        uint32_t op_32 = use32;
        uint32_t op_pc = new_pc;
        OpFn *op_table = x86_dynarec_opcodes;
        x86seg *op_ea_seg = &_ds;
        int op_ssegs = 0;
        int opcode_shift = 0;
        int opcode_mask = 0x3ff;
        int over = 0;
        int pc_off = 0;

        codegen_timing_start();

        while (!over)
        {
                switch (opcode)
                {
                        case 0x0f:
                        op_table = x86_dynarec_opcodes_0f;
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
                        
                        case 0xd8:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_d8_a32 : x86_dynarec_opcodes_d8_a16;
                        opcode_shift = 3;
                        opcode_mask = 0x1f;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xd9:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_d9_a32 : x86_dynarec_opcodes_d9_a16;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xda:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_da_a32 : x86_dynarec_opcodes_da_a16;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xdb:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_db_a32 : x86_dynarec_opcodes_db_a16;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xdc:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_dc_a32 : x86_dynarec_opcodes_dc_a16;
                        opcode_shift = 3;
                        opcode_mask = 0x1f;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xdd:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_dd_a32 : x86_dynarec_opcodes_dd_a16;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xde:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_de_a32 : x86_dynarec_opcodes_de_a16;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        break;
                        case 0xdf:
                        op_table = (op_32 & 0x200) ? x86_dynarec_opcodes_df_a32 : x86_dynarec_opcodes_df_a16;
                        opcode_mask = 0xff;
                        over = 1;
                        pc_off = -1;
                        break;
                        
                        case 0xf0: /*LOCK*/
                        break;

                        default:
                        goto generate_call;
                }
                codegen_timing_prefix(opcode);
                fetchdat = fastreadl(cs + op_pc);
                if (abrt)
                        return;
                opcode = fetchdat & 0xff;
                if (!pc_off)
                        fetchdat >>= 8;
                
                op_pc++;
        }
        
generate_call:
        codegen_timing_opcode(opcode, fetchdat, op_32);

        op = op_table[((opcode >> opcode_shift) | op_32) & opcode_mask];
//        if (output)
//                pclog("Generate call at %08X %02X %08X %02X  %08X %08X %08X %08X %08X  %02X %02X %02X %02X\n", &codeblock[block_current][block_pos], opcode, new_pc, ram[old_pc], EAX, EBX, ECX, EDX, ESI, ram[0x7bd2+6],ram[0x7bd2+7],ram[0x7bd2+8],ram[0x7bd2+9]);
        if (opcode_needs_tempc[opcode])
        {
                addbyte(0xa1);  /*MOVL (flags), %eax*/
                addlong((uint32_t)&flags);
                addbyte(0x83);  /*ANDL $1, %eax*/
                addbyte(0xe0);
                addbyte(0x01);
                addbyte(0xa3);  /*MOVL %eax, (tempc)*/
                addlong((uint32_t)&tempc);
        }
        if (op_ssegs != last_ssegs)
        {
                last_ssegs = op_ssegs;
                addbyte(0xC7); /*MOVL $0,(ssegs)*/
                addbyte(0x05);
                addlong((uint32_t)&ssegs);
                addlong(op_ssegs);
        }
//        if (op_ea_seg != last_ea_seg)
//        {
//                last_ea_seg = op_ea_seg;
                addbyte(0xC7); /*MOVL $&_ds,(ea_seg)*/
                addbyte(0x05);
                addlong((uint32_t)&ea_seg);
                addlong((uint32_t)op_ea_seg);
//        }
        addbyte(0xC7); /*MOVL $new_pc,(pc)*/
        addbyte(0x05);
        addlong((uint32_t)&pc);
        addlong(op_pc + pc_off);
        addbyte(0xC7); /*MOVL $old_pc,(oldpc)*/
        addbyte(0x05);
        addlong((uint32_t)&oldpc);
        addlong(old_pc);
        if (op_32 != last_op32)
        {
                last_op32 = op_32;
                addbyte(0xC7); /*MOVL $use32,(op32)*/
                addbyte(0x05);
                addlong((uint32_t)&op32);
                addlong(op_32);
        }

        addbyte(0xC7); /*MOVL $fetchdat,(%esp)*/
        addbyte(0x04);
        addbyte(0x24);
        addlong(fetchdat);
  
        addbyte(0x83); /*ADD $1,ins*/
        addbyte(0x05);
        addlong((uint32_t)&cpu_recomp_ins);
        addbyte(1);

        addbyte(0xE8); /*CALL*/
        addlong(((uint8_t *)op - (uint8_t *)(&block->data[block_pos + 4])));

        if (codegen_block_cycles &&
            ((op_table == x86_dynarec_opcodes && ((opcode & 0xf0) == 0x70)) ||
            (op_table == x86_dynarec_opcodes && ((opcode & 0xfc) == 0xe0)) ||
            (op_table == x86_dynarec_opcodes_0f && ((opcode & 0xf0) == 0x80))))
        {
                /*Opcode is likely to cause block to exit, update cycle count*/
                addbyte(0x81); /*SUB $codegen_block_cycles, cyclcs*/
                addbyte(0x2d);
                addlong((uint32_t)&cycles);
                addlong((uint32_t)codegen_block_cycles);
                codegen_block_cycles = 0;
        }
        
        block->ins++;

        addbyte(0x09); /*OR %eax, %eax*/
        addbyte(0xc0);
        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
        addlong((uint32_t)&block->data[BLOCK_EXIT_OFFSET] - (uint32_t)(&block->data[block_pos + 4]));

//        addbyte(0xE8); /*CALL*/
//        addlong(((uint8_t *)codegen_debug - (uint8_t *)(&block->data[block_pos + 4])));

        codegen_endpc = (cs + pc) + 8;
}

void codegen_check_abrt()
{
        codeblock_t *block = &codeblock[block_current];
//        pclog("Generate check abrt at %08X\n", &codeblock[block_current][block_pos]);
        addbyte(0xf7); addbyte(0x05); /*TESTL $-1, (abrt)*/
        addlong((uint32_t)&abrt); addlong(0xffffffff);
        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
        addlong((uint32_t)&block->data[BLOCK_EXIT_OFFSET] - (uint32_t)(&block->data[block_pos + 4]));
}

#endif
#endif
