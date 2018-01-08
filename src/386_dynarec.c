#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "386.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "mem.h"
#include "codegen.h"
#include "cpu.h"
#include "fdc.h"
#include "nmi.h"
#include "pic.h"
#include "timer.h"

#include "386_common.h"

#define CPU_BLOCK_END() cpu_block_end = 1

uint32_t cpu_cur_status = 0;

int cpu_reps, cpu_reps_latched;
int cpu_notreps, cpu_notreps_latched;

int inrecomp = 0;
int cpu_recomp_blocks, cpu_recomp_full_ins, cpu_new_blocks;
int cpu_recomp_blocks_latched, cpu_recomp_ins_latched, cpu_recomp_full_ins_latched, cpu_new_blocks_latched;

int cpu_block_end = 0;

int nmi_enable = 1;

int inscounts[256];
uint32_t oldpc2;

int trap;



int cpl_override=0;

int has_fpu;
int fpucount=0;
uint16_t rds;
uint16_t ea_rseg;

int is486;
int cgate32;


uint8_t romext[32768];
uint8_t *ram,*rom;

uint32_t rmdat32;
uint32_t backupregs[16];
int oddeven=0;
int inttype;


uint32_t oldcs2;
uint32_t oldecx;

uint32_t *eal_r, *eal_w;

uint16_t *mod1add[2][8];
uint32_t *mod1seg[8];

static inline void fetch_ea_32_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = cpu_state.ea_seg->base;
        ea_rseg = cpu_state.ea_seg->seg;
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
                        ea_rseg = SS;
                        cpu_state.ea_seg = &_ss;
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
                                ea_rseg = SS;
                                cpu_state.ea_seg = &_ss;
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
                if ( readlookup2[addr >> 12] != -1)
                   eal_r = (uint32_t *)(readlookup2[addr >> 12] + addr);
                if (writelookup2[addr >> 12] != -1)
                   eal_w = (uint32_t *)(writelookup2[addr >> 12] + addr);
        }
}

static inline void fetch_ea_16_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = cpu_state.ea_seg->base;
        ea_rseg = cpu_state.ea_seg->seg;
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
                        cpu_state.eaaddr = (uint16_t)(int8_t)(rmdat >> 8); cpu_state.pc++;
                        break;
                        case 2:
                        cpu_state.eaaddr = getword();
                        break;
                }
                cpu_state.eaaddr += (*mod1add[0][cpu_rm]) + (*mod1add[1][cpu_rm]);
                if (mod1seg[cpu_rm] == &ss && !cpu_state.ssegs)
                {
                        easeg = ss;
                        ea_rseg = SS;
                        cpu_state.ea_seg = &_ss;
                }
                cpu_state.eaaddr &= 0xFFFF;
        }
        if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
        {
                uint32_t addr = easeg + cpu_state.eaaddr;
                if ( readlookup2[addr >> 12] != -1)
                   eal_r = (uint32_t *)(readlookup2[addr >> 12] + addr);
                if (writelookup2[addr >> 12] != -1)
                   eal_w = (uint32_t *)(writelookup2[addr >> 12] + addr);
        }
}

#define fetch_ea_16(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_16_long(rmdat); if (cpu_state.abrt) return 1; } 
#define fetch_ea_32(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_32_long(rmdat); } if (cpu_state.abrt) return 1

#include "x86_flags.h"

void x86_int(int num)
{
        uint32_t addr;
//        pclog("x86_int %02x %04x:%04x\n", num, CS,pc);
        flags_rebuild();
        cpu_state.pc=cpu_state.oldpc;
        if (msw&1)
        {
                pmodeint(num,0);
        }
        else
        {
                addr = (num << 2) + idt.base;

                if ((num << 2) + 3 > idt.limit)
                {
                        if (idt.limit < 35)
                        {
                                cpu_state.abrt = 0;
                                softresetx86();
                                cpu_set_edx();
                                pclog("Triple fault in real mode - reset\n");
                        }
                        else
                                x86_int(8);
                }
                else
                {
                        if (stack32)
                        {
                                writememw(ss,ESP-2,flags);
                                writememw(ss,ESP-4,CS);
                                writememw(ss,ESP-6,cpu_state.pc);
                                ESP-=6;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                                SP-=6;
                        }

                        flags&=~I_FLAG;
                        flags&=~T_FLAG;
                        oxpc=cpu_state.pc;
                        cpu_state.pc=readmemw(0,addr);
                        loadcs(readmemw(0,addr+2));
                }
        }
        cycles-=70;
        CPU_BLOCK_END();
}

void x86_int_sw(int num)
{
        uint32_t addr;
//        pclog("x86_int_sw %02x %04x:%04x\n", num, CS,pc);
//        pclog("x86_int\n");
        flags_rebuild();
        cycles -= timing_int;
        if (msw&1)
        {
                pmodeint(num,1);
        }
        else
        {
                addr = (num << 2) + idt.base;

                if ((num << 2) + 3 > idt.limit)
                {
                        x86_int(13);
                }
                else
                {
                        if (stack32)
                        {
                                writememw(ss,ESP-2,flags);
                                writememw(ss,ESP-4,CS);
                                writememw(ss,ESP-6,cpu_state.pc);
                                ESP-=6;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                                SP-=6;
                        }

                        flags&=~I_FLAG;
                        flags&=~T_FLAG;
                        oxpc=cpu_state.pc;
                        cpu_state.pc=readmemw(0,addr);
                        loadcs(readmemw(0,addr+2));
                        cycles -= timing_int_rm;
                }
        }
        trap = 0;
        CPU_BLOCK_END();
}

int x86_int_sw_rm(int num)
{
        uint32_t addr;
        uint16_t new_pc, new_cs;
        
        flags_rebuild();
        cycles -= timing_int;

        addr = num << 2;
        new_pc = readmemw(0, addr);
        new_cs = readmemw(0, addr + 2);

        if (cpu_state.abrt) return 1;

        writememw(ss,((SP-2)&0xFFFF),flags); if (cpu_state.abrt) {pclog("abrt5\n"); return 1; }
        writememw(ss,((SP-4)&0xFFFF),CS);
        writememw(ss,((SP-6)&0xFFFF),cpu_state.pc); if (cpu_state.abrt) {pclog("abrt6\n"); return 1; }
        SP-=6;

        eflags &= ~VIF_FLAG;
        flags &= ~T_FLAG;
        cpu_state.pc = new_pc;
        loadcs(new_cs);
        oxpc=cpu_state.pc;

        cycles -= timing_int_rm;
        trap = 0;
        CPU_BLOCK_END();
        
        return 0;
}

void x86illegal()
{
//        pclog("x86 illegal %04X %08X %04X:%08X %02X\n",msw,cr0,CS,pc,opcode);
        
//        if (output)
//        {
//                dumpregs();
//                exit(-1);
//        }
        x86_int(6);
}

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
        int mem_cycles = reads*cpu_cycles_read + reads_l*cpu_cycles_read_l + writes*cpu_cycles_write + writes_l*cpu_cycles_write_l;

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
}

static void prefetch_flush()
{
        prefetch_bytes = 0;
}

#define PREFETCH_RUN(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32) \
        do { if (cpu_prefetch_cycles) prefetch_run(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32); } while (0)

#define PREFETCH_PREFIX() do { if (cpu_prefetch_cycles) prefetch_prefixes++; } while (0)
#define PREFETCH_FLUSH() prefetch_flush()


int checkio(int port)
{
        uint16_t t;
        uint8_t d;
        cpl_override = 1;
        t = readmemw(tr.base, 0x66);
        cpl_override = 0;
//        pclog("CheckIO 1 %08X  %04x %02x\n",tr.base, eflags, _cs.access);
        if (cpu_state.abrt) return 0;
//        pclog("CheckIO %04X %01X %01X %02X %04X %04X %08X ",CS,CPL,IOPL,port,t,t+(port>>3),tr.base+t+(port>>3));
        if ((t+(port>>3))>tr.limit) return 1;
        cpl_override = 1;
        d = readmemb386l(0, tr.base + t + (port >> 3));
//        d=readmemb(tr.base,t+(port>>3));
        cpl_override = 0;
//      pclog("%02X %02X\n",d,d&(1<<(port&7)));
        return d&(1<<(port&7));
}

int xout=0;


#define divexcp() { \
                pclog("Divide exception at %04X(%06X):%04X\n",CS,cs,cpu_state.pc); \
                x86_int(0); \
}

int divl(uint32_t val)
{
        if (val==0) 
        {
                divexcp();
                return 1;
        }
        uint64_t num=(((uint64_t)EDX)<<32)|EAX;
        uint64_t quo=num/val;
        uint32_t rem=num%val;
        uint32_t quo32=(uint32_t)(quo&0xFFFFFFFF);
        if (quo!=(uint64_t)quo32) 
        {
                divexcp();
                return 1;
        }
        EDX=rem;
        EAX=quo32;
        return 0;
}
int idivl(int32_t val)
{
        if (val==0) 
        {       
                divexcp();
                return 1;
        }
        int64_t num=(((uint64_t)EDX)<<32)|EAX;
        int64_t quo=num/val;
        int32_t rem=num%val;
        int32_t quo32=(int32_t)(quo&0xFFFFFFFF);
        if (quo!=(int64_t)quo32) 
        {
                divexcp();
                return 1;
        }
        EDX=rem;
        EAX=quo32;
        return 0;
}


void cpu_386_flags_extract()
{
        flags_extract();
}
void cpu_386_flags_rebuild()
{
        flags_rebuild();
}

int oldi;

uint32_t testr[9];
int dontprint=0;

#define OP_TABLE(name) ops_ ## name
#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "386_ops.h"


#define CACHE_ON() (!(cr0 & (1 << 30)) /*&& (cr0 & 1)*/ && !(flags & T_FLAG))
//#define CACHE_ON() 0

static int cycles_main = 0;
void exec386_dynarec(int cycs)
{
        uint8_t temp;
        uint32_t addr;
        int tempi;
        int cycdiff;
        int oldcyc;
        int cyc_period = cycs / 2000; /*5us*/
//output = 3;
        cycles_main += cycs;
        while (cycles_main > 0)
        {
                int cycles_start;
                
                cycles += cyc_period;
                cycles_start = cycles;

                timer_start_period(cycles << TIMER_SHIFT);
//        output=3;
        while (cycles>0)
        {
                oldcs = CS;
                cpu_state.oldpc = cpu_state.pc;
                oldcpl = CPL;
                cpu_state.op32 = use32;


                cycdiff=0;
                oldcyc=cycles;
//                if (output && CACHE_ON()) pclog("Block %04x:%04x %04x:%08x\n", CS, pc, SS,ESP);
                if (!CACHE_ON()) /*Interpret block*/
                {
                        cpu_block_end = 0;
                        x86_was_reset = 0;
//                        if (output) pclog("Interpret block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  %04x\n", CS, pc, AX, BX, CX, DX, SI, DI, SP);
                        while (!cpu_block_end)
                        {
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                oldcpl=CPL;
                                cpu_state.op32 = use32;

                                cpu_state.ea_seg = &_ds;
                                cpu_state.ssegs = 0;
                
                                fetchdat = fastreadl(cs + cpu_state.pc);
//                                if (!fetchdat)
//                                        fatal("Dead with cache off\n");
                                if (!cpu_state.abrt)
                                {               
                                        trap = flags & T_FLAG;
                                        opcode = fetchdat & 0xFF;
                                        fetchdat >>= 8;

//                                        if (output == 3)
//                                                pclog("int %04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X %f  %02X%02X %02X%02X\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, pit.c[0], ram[0x8f13f], ram[0x8f13e], ram[0x8f141], ram[0x8f140]);

                                        cpu_state.pc++;
                                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
                                }

                                if (!use32) cpu_state.pc &= 0xffff;

                                if (((cs + cpu_state.pc) >> 12) != pccache)
                                        CPU_BLOCK_END();

/*                                if (ssegs)
                                {
                                        ds=oldds;
                                        ss=oldss;
                                        ssegs=0;
                                }*/
                                if (cpu_state.abrt)
                                        CPU_BLOCK_END();
                                if (trap)
                                        CPU_BLOCK_END();
                                if (nmi && nmi_enable && nmi_mask)
                                        CPU_BLOCK_END();
                                        
                                ins++;
                                insc++;
                                
/*                                if ((cs + pc) == 4)
                                        fatal("4\n");*/
/*                                if (ins >= 141400000)
                                        output = 3;*/
                        }
                }
                else
                {
                uint32_t phys_addr = get_phys(cs+cpu_state.pc);
                int hash = HASH(phys_addr);
                codeblock_t *block = codeblock_hash[hash];
                int valid_block = 0;
                trap = 0;

                if (block && !cpu_state.abrt)
                {
                        page_t *page = &pages[phys_addr >> 12];

                        /*Block must match current CS, PC, code segment size,
                          and physical address. The physical address check will
                          also catch any page faults at this stage*/
                        valid_block = (block->pc == cs + cpu_state.pc) && (block->_cs == cs) &&
                                      (block->phys == phys_addr) && !((block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) &&
                                      ((block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                        if (!valid_block)
                        {
                                uint64_t mask = (uint64_t)1 << ((phys_addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                                
                                if (page->code_present_mask[(phys_addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] & mask)
                                {
                                        /*Walk page tree to see if we find the correct block*/
                                        codeblock_t *new_block = codeblock_tree_find(phys_addr, cs);
                                        if (new_block)
                                        {
                                                valid_block = (new_block->pc == cs + cpu_state.pc) && (new_block->_cs == cs) &&
                                                                (new_block->phys == phys_addr) && !((new_block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) &&
                                                                ((new_block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                                                if (valid_block)
                                                        block = new_block;
                                        }
                                }
                        }

                        if (valid_block && (block->page_mask & *block->dirty_mask))
                        {
                                codegen_check_flush(page, page->dirty_mask[(phys_addr >> 10) & 3], phys_addr);
                                page->dirty_mask[(phys_addr >> 10) & 3] = 0;
                                if (block->pc == BLOCK_PC_INVALID)
                                        valid_block = 0;
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
                                uint32_t phys_addr_2 = get_phys_noabrt(block->endpc);
                                page_t *page_2 = &pages[phys_addr_2 >> 12];
                                if ((block->phys_2 ^ phys_addr_2) & ~0xfff)
                                        valid_block = 0;
                                else if (block->page_mask2 & *block->dirty_mask2)
                                {
                                        codegen_check_flush(page_2, page_2->dirty_mask[(phys_addr_2 >> 10) & 3], phys_addr_2);
                                        page_2->dirty_mask[(phys_addr_2 >> 10) & 3] = 0;
                                        if (block->pc == BLOCK_PC_INVALID)
                                                valid_block = 0;
                                }
                        }
                        if (valid_block && block->was_recompiled && (block->flags & CODEBLOCK_STATIC_TOP) && block->TOP != cpu_state.TOP)
                        {
                                /*FPU top-of-stack does not match the value this block was compiled
                                  with, re-compile using dynamic top-of-stack*/
                                block->flags &= ~CODEBLOCK_STATIC_TOP;
                                block->was_recompiled = 0;
                        }
                }

                if (valid_block && block->was_recompiled)
                {
                        void (*code)() = (void *)&block->data[BLOCK_START];

                        codeblock_hash[hash] = block;
//                        if (output) pclog("Run block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  ESP=%08x %04x  %08x %08x  %016llx %08x\n", CS, pc, AX, BX, CX, DX, SI, DI, ESP, BP, get_phys(cs+pc), block->phys, block->page_mask, block->endpc);

inrecomp=1;
                        code();
inrecomp=0;
                        if (!use32) cpu_state.pc &= 0xffff;
//                        cpu_recomp_ins += block->ins;
                        cpu_recomp_blocks++;
/*                        ins += codeblock_ins[index];
                        insc += codeblock_ins[index];*/
/*                        pclog("Exit block now %04X:%04X\n", CS, pc);*/
                }
                else if (valid_block && !cpu_state.abrt)
                {
                        uint32_t start_pc = cpu_state.pc;
                        
//                        pclog("Hash %08x %i\n", codeblock_hash_pc[HASH(cs + pc)], codeblock_page_dirty[(cs + pc) >> 12]);
                        cpu_block_end = 0;
                        x86_was_reset = 0;

                        cpu_new_blocks++;
                        
                        codegen_block_start_recompile(block);
                        codegen_in_recompile = 1;

//                        if (output) pclog("Recompile block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  ESP=%04x %04x  %02x%02x:%02x%02x %02x%02x:%02x%02x %02x%02x:%02x%02x\n", CS, pc, AX, BX, CX, DX, SI, DI, ESP, BP, ram[0x116330+0x6df4+0xa+3], ram[0x116330+0x6df4+0xa+2], ram[0x116330+0x6df4+0xa+1], ram[0x116330+0x6df4+0xa+0], ram[0x11d136+3],ram[0x11d136+2],ram[0x11d136+1],ram[0x11d136+0], ram[(0x119abe)+0x3],ram[(0x119abe)+0x2],ram[(0x119abe)+0x1],ram[(0x119abe)+0x0]);
                        while (!cpu_block_end)
                        {
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                oldcpl=CPL;
                                cpu_state.op32 = use32;

                                cpu_state.ea_seg = &_ds;
                                cpu_state.ssegs = 0;
                
                                fetchdat = fastreadl(cs + cpu_state.pc);
//                                if (fetchdat == 0xffffffff)
//                                        fatal("Dead ffffffff\n");
//                                if (!fetchdat)
//                                        fatal("Dead\n");
                                if (!cpu_state.abrt)
                                {               
                                        trap = flags & T_FLAG;
                                        opcode = fetchdat & 0xFF;
                                        fetchdat >>= 8;

//                                        if (output == 3)
//                                                pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X  %08x %08x\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, cs+pc, pccache);

                                        cpu_state.pc++;
                                                
                                        codegen_generate_call(opcode, x86_opcodes[(opcode | cpu_state.op32) & 0x3ff], fetchdat, cpu_state.pc, cpu_state.pc-1);

                                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                                        if (x86_was_reset)
                                                break;
                                }

                                if (!use32) cpu_state.pc &= 0xffff;

                                /*Cap source code at 4000 bytes per block; this
                                  will prevent any block from spanning more than
                                  2 pages. In practice this limit will never be
                                  hit, as host block size is only 2kB*/
                                if ((cpu_state.pc - start_pc) > 1000)
                                        CPU_BLOCK_END();
                                        
                                if (trap)
                                        CPU_BLOCK_END();

                                if (nmi && nmi_enable && nmi_mask)
                                        CPU_BLOCK_END();

                                if (cpu_state.abrt)
                                {
                                        codegen_block_remove();
                                        CPU_BLOCK_END();
                                }

                                ins++;
                                insc++;
                        }
                        
                        if (!cpu_state.abrt && !x86_was_reset)
                                codegen_block_end_recompile(block);
                        
                        if (x86_was_reset)
                                codegen_reset();

                        codegen_in_recompile = 0;
//                        output &= ~2;
                }
                else if (!cpu_state.abrt)
                {
                        /*Mark block but do not recompile*/
                        uint32_t start_pc = cpu_state.pc;

//                        pclog("Hash %08x %i\n", codeblock_hash_pc[HASH(cs + pc)], codeblock_page_dirty[(cs + pc) >> 12]);
                        cpu_block_end = 0;
                        x86_was_reset = 0;

//                        cpu_new_blocks++;
                        
                        codegen_block_init(phys_addr);

//                        if (output) pclog("Recompile block at %04x:%04x  %04x %04x %04x %04x  %04x %04x  ESP=%04x %04x  %02x%02x:%02x%02x %02x%02x:%02x%02x %02x%02x:%02x%02x\n", CS, pc, AX, BX, CX, DX, SI, DI, ESP, BP, ram[0x116330+0x6df4+0xa+3], ram[0x116330+0x6df4+0xa+2], ram[0x116330+0x6df4+0xa+1], ram[0x116330+0x6df4+0xa+0], ram[0x11d136+3],ram[0x11d136+2],ram[0x11d136+1],ram[0x11d136+0], ram[(0x119abe)+0x3],ram[(0x119abe)+0x2],ram[(0x119abe)+0x1],ram[(0x119abe)+0x0]);
                        while (!cpu_block_end)
                        {
                                oldcs=CS;
                                cpu_state.oldpc = cpu_state.pc;
                                oldcpl=CPL;
                                cpu_state.op32 = use32;

                                cpu_state.ea_seg = &_ds;
                                cpu_state.ssegs = 0;
                
                                codegen_endpc = (cs + cpu_state.pc) + 8;
                                fetchdat = fastreadl(cs + cpu_state.pc);

                                if (!cpu_state.abrt)
                                {               
                                        trap = flags & T_FLAG;
                                        opcode = fetchdat & 0xFF;
                                        fetchdat >>= 8;

//                                        if (output == 3)
//                                                pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X  %08x %08x\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, cs+pc, pccache);

                                        cpu_state.pc++;
                                                
                                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                                        if (x86_was_reset)
                                                break;
                                }

                                if (!use32) cpu_state.pc &= 0xffff;

                                /*Cap source code at 4000 bytes per block; this
                                  will prevent any block from spanning more than
                                  2 pages. In practice this limit will never be
                                  hit, as host block size is only 2kB*/
                                if ((cpu_state.pc - start_pc) > 1000)
                                        CPU_BLOCK_END();
                                        
                                if (trap)
                                        CPU_BLOCK_END();

                                if (nmi && nmi_enable && nmi_mask)
                                        CPU_BLOCK_END();

                                if (cpu_state.abrt)
                                {
                                        codegen_block_remove();
                                        CPU_BLOCK_END();
                                }

                                ins++;
                                insc++;
                        }
                        
                        if (!cpu_state.abrt && !x86_was_reset)
                                codegen_block_end();
                        
                        if (x86_was_reset)
                                codegen_reset();

//                        output &= ~2;
                }
//                        if (output && (SP & 1))
//                                fatal("odd SP\n");
                }

                cycdiff=oldcyc-cycles;
                tsc += cycdiff;
                
//                timer_end_period(cycles);
                
                if (cpu_state.abrt)
                {
                        flags_rebuild();
                        tempi = cpu_state.abrt;
                        cpu_state.abrt = 0;
                        x86_doabrt(tempi);
                        if (cpu_state.abrt)
                        {
                                cpu_state.abrt = 0;
                                CS = oldcs;
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
                
                if (trap)
                {

                        flags_rebuild();
//                        oldpc=pc;
//                        oldcs=CS;
                        if (msw&1)
                        {
                                pmodeint(1,0);
                        }
                        else
                        {
                                writememw(ss,(SP-2)&0xFFFF,flags);
                                writememw(ss,(SP-4)&0xFFFF,CS);
                                writememw(ss,(SP-6)&0xFFFF,cpu_state.pc);
                                SP-=6;
                                addr = (1 << 2) + idt.base;
                                flags&=~I_FLAG;
                                flags&=~T_FLAG;
                                cpu_state.pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                        }
                }
                else if (nmi && nmi_enable && nmi_mask)
                {
                        cpu_state.oldpc = cpu_state.pc;
                        oldcs = CS;
//                        pclog("NMI\n");
                        x86_int(2);
                        nmi_enable = 0;
                        if (nmi_auto_clear)
                        {
                                nmi_auto_clear = 0;
                                nmi = 0;
                        }
                }
                else if ((flags&I_FLAG) && pic_intpending)
                {
                        temp=picinterrupt();
                        if (temp!=0xFF)
                        {
//                                pclog("IRQ %02X %04X:%04X %04X:%04X\n", temp, SS, SP, CS, pc);
                                CPU_BLOCK_END();
                                flags_rebuild();
                                if (msw&1)
                                {
                                        pmodeint(temp,0);
                                }
                                else
                                {
                                        writememw(ss,(SP-2)&0xFFFF,flags);
                                        writememw(ss,(SP-4)&0xFFFF,CS);
                                        writememw(ss,(SP-6)&0xFFFF,cpu_state.pc);
                                        SP-=6;
                                        addr=temp<<2;
                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=cpu_state.pc;
                                        cpu_state.pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                }
                        }
                }
        }
                timer_end_period(cycles << TIMER_SHIFT);
                cycles_main -= (cycles_start - cycles);
        }
}
