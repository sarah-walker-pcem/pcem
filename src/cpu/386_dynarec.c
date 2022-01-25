#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(__APPLE__) && defined(__aarch64__)
#include <pthread.h>
#endif
#include "ibm.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "mem.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "cpu.h"
#include "fdc.h"
#include "nmi.h"
#include "pic.h"
#include "timer.h"

#include "386_common.h"

#define CPU_BLOCK_END() cpu_block_end = 1

int inrecomp = 0;
int cpu_recomp_blocks, cpu_recomp_full_ins, cpu_new_blocks;
int cpu_recomp_blocks_latched, cpu_recomp_ins_latched, cpu_recomp_full_ins_latched, cpu_new_blocks_latched;

int cpu_block_end = 0;

static inline void fetch_ea_32_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = cpu_state.ea_seg->base;
        if (cpu_rm == 4)
        {
                uint8_t sib = rmdat >> 8;

                switch (cpu_mod)
                {
                case 0:
                        cpu_state.eaaddr = cpu_state.regs[sib & 7].l;
                        cpu_state.pc++;
                        break;
                case 1:
                        cpu_state.pc++;
                        cpu_state.eaaddr = ((uint32_t)(int8_t)getbyte()) + cpu_state.regs[sib & 7].l;
//                        cpu_state.pc++; 
                        break;
                case 2:
                        cpu_state.eaaddr = (fastreadl(cs + cpu_state.pc + 1)) + cpu_state.regs[sib & 7].l;
                        cpu_state.pc += 5;
                        break;
                }
                /*SIB byte present*/
                if ((sib & 7) == 5 && !cpu_mod)
                        cpu_state.eaaddr = getlong();
                else if ((sib & 6) == 4 && !cpu_state.ssegs)
                {
                        easeg = ss;
                        cpu_state.ea_seg = &cpu_state.seg_ss;
                }
                if (((sib >> 3) & 7) != 4)
                        cpu_state.eaaddr += cpu_state.regs[(sib >> 3) & 7].l << (sib >> 6);
        }
        else
        {
                cpu_state.eaaddr = cpu_state.regs[cpu_rm].l;
                if (cpu_mod)
                {
                        if (cpu_rm == 5 && !cpu_state.ssegs)
                        {
                                easeg = ss;
                                cpu_state.ea_seg = &cpu_state.seg_ss;
                        }
                        if (cpu_mod == 1)
                        {
                                cpu_state.eaaddr += ((uint32_t)(int8_t)(rmdat >> 8));
                                cpu_state.pc++;
                        }
                        else
                        {
                                cpu_state.eaaddr += getlong();
                        }
                }
                else if (cpu_rm == 5)
                {
                        cpu_state.eaaddr = getlong();
                }
        }
        if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
        {
                uint32_t addr = easeg + cpu_state.eaaddr;
                if (readlookup2[addr >> 12] != -1)
                        eal_r = (uint32_t*)(readlookup2[addr >> 12] + addr);
                if (writelookup2[addr >> 12] != -1)
                        eal_w = (uint32_t*)(writelookup2[addr >> 12] + addr);
        }
}

static inline void fetch_ea_16_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = cpu_state.ea_seg->base;
        if (!cpu_mod && cpu_rm == 6)
        {
                cpu_state.eaaddr = getword();
        }
        else
        {
                switch (cpu_mod)
                {
                case 0:
                        cpu_state.eaaddr = 0;
                        break;
                case 1:
                        cpu_state.eaaddr = (uint16_t)(int8_t)(rmdat >> 8);
                        cpu_state.pc++;
                        break;
                case 2:
                        cpu_state.eaaddr = getword();
                        break;
                }
                cpu_state.eaaddr += (*mod1add[0][cpu_rm]) + (*mod1add[1][cpu_rm]);
                if (mod1seg[cpu_rm] == &ss && !cpu_state.ssegs)
                {
                        easeg = ss;
                        cpu_state.ea_seg = &cpu_state.seg_ss;
                }
                cpu_state.eaaddr &= 0xFFFF;
        }
        if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
        {
                uint32_t addr = easeg + cpu_state.eaaddr;
                if (readlookup2[addr >> 12] != -1)
                        eal_r = (uint32_t*)(readlookup2[addr >> 12] + addr);
                if (writelookup2[addr >> 12] != -1)
                        eal_w = (uint32_t*)(writelookup2[addr >> 12] + addr);
        }
}

#define fetch_ea_16(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_16_long(rmdat); if (cpu_state.abrt) return 1; }
#define fetch_ea_32(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_32_long(rmdat); } if (cpu_state.abrt) return 1

#include "x86_flags.h"

/*Prefetch emulation is a fairly simplistic model:
  - All instruction bytes must be fetched before it starts.
  - Cycles used for non-instruction memory accesses are counted and subtracted
    from the total cycles taken
  - Any remaining cycles are used to refill the prefetch queue.

  Note that this is only used for 286 / 386 systems. It is disabled when the
  internal cache on 486+ CPUs is enabled.
*/
static int prefetch_bytes = 0;
static int prefetch_prefixes = 0;

static void prefetch_run(int instr_cycles, int bytes, int modrm, int reads, int reads_l, int writes, int writes_l, int ea32)
{
        int mem_cycles = reads * cpu_cycles_read + reads_l * cpu_cycles_read_l + writes * cpu_cycles_write + writes_l * cpu_cycles_write_l;

        if (instr_cycles < mem_cycles)
                instr_cycles = mem_cycles;

        prefetch_bytes -= prefetch_prefixes;
        prefetch_bytes -= bytes;
        if (modrm != -1)
        {
                if (ea32)
                {
                        if ((modrm & 7) == 4)
                        {
                                if ((modrm & 0x700) == 0x500)
                                        prefetch_bytes -= 5;
                                else if ((modrm & 0xc0) == 0x40)
                                        prefetch_bytes -= 2;
                                else if ((modrm & 0xc0) == 0x80)
                                        prefetch_bytes -= 5;
                        }
                        else
                        {
                                if ((modrm & 0xc7) == 0x05)
                                        prefetch_bytes -= 4;
                                else if ((modrm & 0xc0) == 0x40)
                                        prefetch_bytes--;
                                else if ((modrm & 0xc0) == 0x80)
                                        prefetch_bytes -= 4;
                        }
                }
                else
                {
                        if ((modrm & 0xc7) == 0x06)
                                prefetch_bytes -= 2;
                        else if ((modrm & 0xc0) != 0xc0)
                                prefetch_bytes -= ((modrm & 0xc0) >> 6);
                }
        }

        /* Fill up prefetch queue */
        while (prefetch_bytes < 0)
        {
                prefetch_bytes += cpu_prefetch_width;
                cycles -= cpu_prefetch_cycles;
        }

        /* Subtract cycles used for memory access by instruction */
        instr_cycles -= mem_cycles;

        while (instr_cycles >= cpu_prefetch_cycles)
        {
                prefetch_bytes += cpu_prefetch_width;
                instr_cycles -= cpu_prefetch_cycles;
        }

        prefetch_prefixes = 0;
        if (prefetch_bytes > 16)
                prefetch_bytes = 16;
}

static void prefetch_flush()
{
        prefetch_bytes = 0;
}

#define PREFETCH_RUN(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32) \
        do { if (cpu_prefetch_cycles) prefetch_run(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32); } while (0)

#define PREFETCH_PREFIX() do { if (cpu_prefetch_cycles) prefetch_prefixes++; } while (0)
#define PREFETCH_FLUSH() prefetch_flush()

#define OP_TABLE(name) ops_ ## name
#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "386_ops.h"

#define CACHE_ON() (!(cr0 & (1 << 30)) && !(cpu_state.flags & T_FLAG))
//#define CACHE_ON() 0

int cpu_end_block_after_ins = 0;

static inline void exec_interpreter(void)
{
        cpu_block_end = 0;
        x86_was_reset = 0;
//        if (output) pclog("Interpret block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  %04x\n", CS, pc, AX, BX, CX, DX, SI, DI, SP);
        while (!cpu_block_end)
        {
                cpu_state.oldpc = cpu_state.pc;
                cpu_state.op32 = use32;

                cpu_state.ea_seg = &cpu_state.seg_ds;
                cpu_state.ssegs = 0;

                fetchdat = fastreadl(cs + cpu_state.pc);

                if (!cpu_state.abrt)
                {
                        uint8_t opcode = fetchdat & 0xFF;
                        fetchdat >>= 8;
                        trap = cpu_state.flags & T_FLAG;

//                        if (output == 3)
//                                pclog("int %04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X %f  %02X%02X %02X%02X\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, pit.c[0], ram[0x8f13f], ram[0x8f13e], ram[0x8f141], ram[0x8f140]);

                        cpu_state.pc++;
                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
                }

                if (((cs + cpu_state.pc) >> 12) != pccache)
                        CPU_BLOCK_END();

                if (cpu_state.abrt)
                        CPU_BLOCK_END();
                if (cpu_state.smi_pending)
                        CPU_BLOCK_END();
                if (trap)
                        CPU_BLOCK_END();
                if (nmi && nmi_enable && nmi_mask)
                        CPU_BLOCK_END();
                if (cpu_end_block_after_ins)
                {
                        cpu_end_block_after_ins--;
                        if (!cpu_end_block_after_ins)
                                CPU_BLOCK_END();
                }

                ins++;
                insc++;
        }

        if (trap)
        {
                trap = 0;
                cpu_state.oldpc = cpu_state.pc;
                x86_int(1);
        }

        cpu_end_block_after_ins = 0;
}

/* TODO: Look in deeper for why, but this is to fix crashing. Somehow a function is being optimized out, causing
 *       a SEGFAULT
 */

static inline void __attribute__((optimize("O0"))) exec_recompiler(void)
{
        uint32_t phys_addr = get_phys(cs + cpu_state.pc);
        int hash = HASH(phys_addr);
        codeblock_t* block = &codeblock[codeblock_hash[hash]];
        int valid_block = 0;

        if (!cpu_state.abrt)
        {
                page_t* page = &pages[phys_addr >> 12];

                /*Block must match current CS, PC, code segment size,
                  and physical address. The physical address check will
                  also catch any page faults at this stage*/
                valid_block = (block->pc == cs + cpu_state.pc) && (block->_cs == cs) &&
                              (block->phys == phys_addr) && !((block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) &&
                              ((block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                if (!valid_block)
                {
                        uint64_t mask = (uint64_t)1 << ((phys_addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                        int byte_offset = (phys_addr >> PAGE_BYTE_MASK_SHIFT) & PAGE_BYTE_MASK_OFFSET_MASK;
                        uint64_t byte_mask = 1ull << (PAGE_BYTE_MASK_MASK & 0x3f);

                        if ((page->code_present_mask & mask) || (page->byte_code_present_mask[byte_offset] & byte_mask))
                        {
                                /*Walk page tree to see if we find the correct block*/
                                codeblock_t* new_block = codeblock_tree_find(phys_addr, cs);
                                if (new_block)
                                {
                                        valid_block = (new_block->pc == cs + cpu_state.pc) && (new_block->_cs == cs) &&
                                                      (new_block->phys == phys_addr) && !((new_block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) &&
                                                      ((new_block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                                        if (valid_block)
                                        {
                                                block = new_block;
                                                codeblock_hash[hash] = get_block_nr(block);
                                        }
                                }
                        }
                }

                if (valid_block && (block->page_mask & *block->dirty_mask))
                {
                        codegen_check_flush(page, page->dirty_mask, phys_addr);
                        if (block->pc == BLOCK_PC_INVALID)
                                valid_block = 0;
                        else if (block->flags & CODEBLOCK_IN_DIRTY_LIST)
                                block->flags &= ~CODEBLOCK_WAS_RECOMPILED;
                }
                if (valid_block && block->page_mask2)
                {
                        /*We don't want the second page to cause a page
                          fault at this stage - that would break any
                          code crossing a page boundary where the first
                          page is present but the second isn't. Instead
                          allow the first page to be interpreted and for
                          the page fault to occur when the page boundary
                          is actually crossed.*/
                        uint32_t phys_addr_2 = get_phys_noabrt(block->pc + ((block->flags & CODEBLOCK_BYTE_MASK) ? 0x40 : 0x400));
                        page_t* page_2 = &pages[phys_addr_2 >> 12];
                        if ((block->phys_2 ^ phys_addr_2) & ~0xfff)
                                valid_block = 0;
                        else if (block->page_mask2 & *block->dirty_mask2)
                        {
                                codegen_check_flush(page_2, page_2->dirty_mask, phys_addr_2);
                                if (block->pc == BLOCK_PC_INVALID)
                                        valid_block = 0;
                                else if (block->flags & CODEBLOCK_IN_DIRTY_LIST)
                                        block->flags &= ~CODEBLOCK_WAS_RECOMPILED;
                        }
                }
                if (valid_block && (block->flags & CODEBLOCK_IN_DIRTY_LIST))
                {
                        block->flags &= ~CODEBLOCK_WAS_RECOMPILED;
                        if (block->flags & CODEBLOCK_BYTE_MASK)
                                block->flags |= CODEBLOCK_NO_IMMEDIATES;
                        else
                                block->flags |= CODEBLOCK_BYTE_MASK;
                }
                if (valid_block && (block->flags & CODEBLOCK_WAS_RECOMPILED) && (block->flags & CODEBLOCK_STATIC_TOP) && block->TOP != (cpu_state.TOP & 7))
                {
                        /*FPU top-of-stack does not match the value this block was compiled
                          with, re-compile using dynamic top-of-stack*/
                        block->flags &= ~(CODEBLOCK_STATIC_TOP | CODEBLOCK_WAS_RECOMPILED);
                }
        }

        if (valid_block && (block->flags & CODEBLOCK_WAS_RECOMPILED))
        {
                void (* code)() = (void*)&block->data[BLOCK_START]; // FIX: This seems to get optimized out, I tried making volatile but still segfaulted

//                if (output) pclog("Run block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  ESP=%08x %04x  %08x %08x  %016llx %08x\n", CS, pc, AX, BX, CX, DX, SI, DI, ESP, BP, get_phys(cs+pc), block->phys, block->page_mask, block->endpc);

                inrecomp = 1;
                code();
                inrecomp = 0;

                cpu_recomp_blocks++;
        }
        else if (valid_block && !cpu_state.abrt)
        {
                uint32_t start_pc = cs + cpu_state.pc;
                const int max_block_size = (block->flags & CODEBLOCK_BYTE_MASK) ? ((128 - 25) - (start_pc & 0x3f)) : 1000;

                cpu_block_end = 0;
                x86_was_reset = 0;

                cpu_new_blocks++;

#if defined(__APPLE__) && defined(__aarch64__)
                pthread_jit_write_protect_np(0);
#endif
                codegen_block_start_recompile(block);
                codegen_in_recompile = 1;

//                if (output) pclog("Recompile block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  ESP=%04x %04x  %02x%02x:%02x%02x %02x%02x:%02x%02x %02x%02x:%02x%02x\n", CS, pc, AX, BX, CX, DX, SI, DI, ESP, BP, ram[0x116330+0x6df4+0xa+3], ram[0x116330+0x6df4+0xa+2], ram[0x116330+0x6df4+0xa+1], ram[0x116330+0x6df4+0xa+0], ram[0x11d136+3],ram[0x11d136+2],ram[0x11d136+1],ram[0x11d136+0], ram[(0x119abe)+0x3],ram[(0x119abe)+0x2],ram[(0x119abe)+0x1],ram[(0x119abe)+0x0]);
                while (!cpu_block_end)
                {
                        cpu_state.oldpc = cpu_state.pc;
                        cpu_state.op32 = use32;

                        cpu_state.ea_seg = &cpu_state.seg_ds;
                        cpu_state.ssegs = 0;

                        fetchdat = fastreadl(cs + cpu_state.pc);

                        if (!cpu_state.abrt)
                        {
                                uint8_t opcode = fetchdat & 0xFF;
                                fetchdat >>= 8;

//                                if (output == 3)
//                                        pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X  %08x %08x\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, cs+pc, pccache);

                                cpu_state.pc++;

                                codegen_generate_call(opcode, x86_opcodes[(opcode | cpu_state.op32) & 0x3ff], fetchdat, cpu_state.pc, cpu_state.pc - 1);

                                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                                if (x86_was_reset)
                                        break;
                        }

                        /*Cap source code at 4000 bytes per block; this
                          will prevent any block from spanning more than
                          2 pages. In practice this limit will never be
                          hit, as host block size is only 2kB*/
                        if (((cs + cpu_state.pc) - start_pc) >= max_block_size)
                                CPU_BLOCK_END();
                        if (cpu_state.flags & T_FLAG)
                                CPU_BLOCK_END();
                        if (cpu_state.smi_pending)
                                CPU_BLOCK_END();
                        if (nmi && nmi_enable && nmi_mask)
                                CPU_BLOCK_END();

                        if (cpu_end_block_after_ins)
                        {
                                cpu_end_block_after_ins--;
                                if (!cpu_end_block_after_ins)
                                        CPU_BLOCK_END();
                        }

                        if (cpu_state.abrt)
                        {
                                if (!(cpu_state.abrt & ABRT_EXPECTED))
                                        codegen_block_remove();
                                CPU_BLOCK_END();
                        }

                        ins++;
                        insc++;
                }
                cpu_end_block_after_ins = 0;

                if ((!cpu_state.abrt || (cpu_state.abrt & ABRT_EXPECTED)) && !x86_was_reset)
                        codegen_block_end_recompile(block);

                if (x86_was_reset)
                        codegen_reset();

                codegen_in_recompile = 0;
#if defined(__APPLE__) && defined(__aarch64__)
                pthread_jit_write_protect_np(1);
#endif
        }
        else if (!cpu_state.abrt)
        {
                /*Mark block but do not recompile*/
                uint32_t start_pc = cs + cpu_state.pc;
                const int max_block_size = (block->flags & CODEBLOCK_BYTE_MASK) ? ((128 - 25) - (start_pc & 0x3f)) : 1000;

                cpu_block_end = 0;
                x86_was_reset = 0;

                codegen_block_init(phys_addr);

//                if (output) pclog("Recompile block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  ESP=%04x %04x  %02x%02x:%02x%02x %02x%02x:%02x%02x %02x%02x:%02x%02x\n", CS, pc, AX, BX, CX, DX, SI, DI, ESP, BP, ram[0x116330+0x6df4+0xa+3], ram[0x116330+0x6df4+0xa+2], ram[0x116330+0x6df4+0xa+1], ram[0x116330+0x6df4+0xa+0], ram[0x11d136+3],ram[0x11d136+2],ram[0x11d136+1],ram[0x11d136+0], ram[(0x119abe)+0x3],ram[(0x119abe)+0x2],ram[(0x119abe)+0x1],ram[(0x119abe)+0x0]);
                while (!cpu_block_end)
                {
                        cpu_state.oldpc = cpu_state.pc;
                        cpu_state.op32 = use32;

                        cpu_state.ea_seg = &cpu_state.seg_ds;
                        cpu_state.ssegs = 0;

                        codegen_endpc = (cs + cpu_state.pc) + 8;
                        fetchdat = fastreadl(cs + cpu_state.pc);

                        if (!cpu_state.abrt)
                        {
                                uint8_t opcode = fetchdat & 0xFF;
                                fetchdat >>= 8;

//                                if (output == 3)
//                                        pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X  %08x %08x\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, cs+pc, pccache);

                                cpu_state.pc++;

                                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                                if (x86_was_reset)
                                        break;
                        }

                        /*Cap source code at 4000 bytes per block; this
                          will prevent any block from spanning more than
                          2 pages. In practice this limit will never be
                          hit, as host block size is only 2kB*/
                        if (((cs + cpu_state.pc) - start_pc) >= max_block_size)
                                CPU_BLOCK_END();
                        if (cpu_state.flags & T_FLAG)
                                CPU_BLOCK_END();
                        if (cpu_state.smi_pending)
                                CPU_BLOCK_END();
                        if (nmi && nmi_enable && nmi_mask)
                                CPU_BLOCK_END();

                        if (cpu_end_block_after_ins)
                        {
                                cpu_end_block_after_ins--;
                                if (!cpu_end_block_after_ins)
                                        CPU_BLOCK_END();
                        }

                        if (cpu_state.abrt)
                        {
                                if (!(cpu_state.abrt & ABRT_EXPECTED))
                                        codegen_block_remove();
                                CPU_BLOCK_END();
                        }

                        ins++;
                        insc++;
                }
                cpu_end_block_after_ins = 0;

//                if (!cpu_state.abrt && !x86_was_reset)
                if ((!cpu_state.abrt || (cpu_state.abrt & ABRT_EXPECTED)) && !x86_was_reset)
                        codegen_block_end();

                if (x86_was_reset)
                        codegen_reset();
        }
        else
                cpu_state.oldpc = cpu_state.pc;

}

int cycles_main = 0;
void exec386_dynarec(int cycs)
{
        uint8_t temp;
        int tempi;
        int cycdiff;
        int oldcyc;
        int cyc_period = cycs / 2000; /*5us*/

        cycles_main += cycs;
        while (cycles_main > 0)
        {
                int cycles_start;

                cycles += cyc_period;
                cycles_start = cycles;

                while (cycles > 0)
                {
                        oldcyc = cycles;
//                        if (output && CACHE_ON()) pclog("Block %04x:%04x %04x:%08x\n", CS, pc, SS,ESP);
                        if (!CACHE_ON()) /*Interpret block*/
                                exec_interpreter();
                        else
                                exec_recompiler();

                        if (cpu_state.abrt)
                        {
                                flags_rebuild();
                                tempi = cpu_state.abrt & ABRT_MASK;
                                cpu_state.abrt = 0;
                                x86_doabrt(tempi);
                                if (cpu_state.abrt)
                                {
                                        cpu_state.abrt = 0;
                                        cpu_state.pc = cpu_state.oldpc;
                                        pclog("Double fault %i\n", ins);
                                        pmodeint(8, 0);
                                        if (cpu_state.abrt)
                                        {
                                                cpu_state.abrt = 0;
                                                softresetx86();
                                                cpu_set_edx();
                                                pclog("Triple fault - reset\n");
                                        }
                                }
                        }

                        if (cpu_state.smi_pending)
                        {
                                cpu_state.smi_pending = 0;
                                x86_smi_enter();
                        }
                        else if (nmi && nmi_enable && nmi_mask)
                        {
                                cpu_state.oldpc = cpu_state.pc;
//                                pclog("NMI\n");
                                x86_int(2);
                                nmi_enable = 0;
                                if (nmi_auto_clear)
                                {
                                        nmi_auto_clear = 0;
                                        nmi = 0;
                                }
                        }
                        else if ((cpu_state.flags & I_FLAG) && pic_intpending)
                        {
                                temp = picinterrupt();
                                if (temp != 0xFF)
                                {
                                        cpu_state.oldpc = cpu_state.pc;
                                        x86_int(temp);
//                                        pclog("IRQ %02X %04X:%04X %04X:%04X\n", temp, SS, SP, CS, pc);
                                }
                        }

                        cycdiff = oldcyc - cycles;
                        tsc += cycdiff;
                }

                if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint32_t)tsc))
                        timer_process();
                cycles_main -= (cycles_start - cycles);
        }
}
