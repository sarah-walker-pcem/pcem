#include "ibm.h"
#include "cpu.h"
#include "386.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "x86_flags.h"
#include "mem.h"
#include "nmi.h"
#include "pic.h"
#include "codegen.h"

#define CPU_BLOCK_END() cpu_block_end = 1

#include "386_common.h"


extern uint16_t *mod1add[2][8];
extern uint32_t *mod1seg[8];

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


#define PREFETCH_RUN(instr_cycles, bytes, modrm, reads, read_ls, writes, write_ls, ea32)
#define PREFETCH_PREFIX()
#define PREFETCH_FLUSH()

#define OP_TABLE(name) dynarec_ops_ ## name
/*Temporary*/
#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "386_ops.h"
