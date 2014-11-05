#ifdef DYNAREC
#include "ibm.h"
#include "cpu.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "x86_flags.h"
#include "codegen.h"

#define CPU_BLOCK_END() cpu_block_end = 1

#include "386_common.h"


extern uint16_t *mod1add[2][8];
extern uint32_t *mod1seg[8];

/*Copy-and-pasted from 386_dynarec.c, is not in a header file because it will
  eventually be different for the dynarec ops*/
static inline void fetch_ea_32_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = ea_seg->base;
        ea_rseg = ea_seg->seg;
        if (rm == 4)
        {
                uint8_t sib = rmdat >> 8;
                
                switch (mod)
                {
                        case 0: 
                        eaaddr = regs[sib & 7].l; 
                        pc++; 
                        break;
                        case 1: 
                        pc++;
                        eaaddr = ((uint32_t)(int8_t)getbyte()) + regs[sib & 7].l; 
//                        pc++; 
                        break;
                        case 2: 
                        eaaddr = (fastreadl(cs + pc + 1)) + regs[sib & 7].l; 
                        pc += 5; 
                        break;
                }
                /*SIB byte present*/
                if ((sib & 7) == 5 && !mod) 
                        eaaddr = getlong();
                else if ((sib & 6) == 4 && !ssegs)
                {
                        easeg = ss;
                        ea_rseg = SS;
                        ea_seg = &_ss;
                }
                if (((sib >> 3) & 7) != 4) 
                        eaaddr += regs[(sib >> 3) & 7].l << (sib >> 6);
        }
        else
        {
                eaaddr = regs[rm].l;
                if (mod) 
                {
                        if (rm == 5 && !ssegs)
                        {
                                easeg = ss;
                                ea_rseg = SS;
                                ea_seg = &_ss;
                        }
                        if (mod == 1) 
                        { 
                                eaaddr += ((uint32_t)(int8_t)(rmdat >> 8)); 
                                pc++; 
                        }
                        else          
                        {
                                eaaddr += getlong(); 
                        }
                }
                else if (rm == 5) 
                {
                        eaaddr = getlong();
                }
        }
        if (easeg != 0xFFFFFFFF && ((easeg + eaaddr) & 0xFFF) <= 0xFFC)
        {
                if ( readlookup2[(easeg + eaaddr) >> 12] != -1)
                   eal_r = (uint32_t *)(readlookup2[(easeg + eaaddr) >> 12] + easeg + eaaddr);
                if (writelookup2[(easeg + eaaddr) >> 12] != -1)
                   eal_w = (uint32_t *)(writelookup2[(easeg + eaaddr) >> 12] + easeg + eaaddr);
        }
}

static inline void fetch_ea_16_long(uint32_t rmdat)
{
        eal_r = eal_w = NULL;
        easeg = ea_seg->base;
        ea_rseg = ea_seg->seg;
        if (!mod && rm == 6) 
        { 
                eaaddr = getword();
        }
        else
        {
                switch (mod)
                {
                        case 0:
                        eaaddr = 0;
                        break;
                        case 1:
                        eaaddr = (uint16_t)(int8_t)(rmdat >> 8); pc++;
                        break;
                        case 2:
                        eaaddr = getword();
                        break;
                }
                eaaddr += (*mod1add[0][rm]) + (*mod1add[1][rm]);
                if (mod1seg[rm] == &ss && !ssegs)
                {
                        easeg = ss;
                        ea_rseg = SS;
                        ea_seg = &_ss;
                }
                eaaddr &= 0xFFFF;
        }
        if (easeg != 0xFFFFFFFF && ((easeg + eaaddr) & 0xFFF) <= 0xFFC)
        {
                if ( readlookup2[(easeg + eaaddr) >> 12] != -1)
                   eal_r = (uint32_t *)(readlookup2[(easeg + eaaddr) >> 12] + easeg + eaaddr);
                if (writelookup2[(easeg + eaaddr) >> 12] != -1)
                   eal_w = (uint32_t *)(writelookup2[(easeg + eaaddr) >> 12] + easeg + eaaddr);
        }
}

#define fetch_ea_16(rmdat)              pc++; mod=(rmdat >> 6) & 3; reg=(rmdat >> 3) & 7; rm = rmdat & 7; if (mod != 3) { fetch_ea_16_long(rmdat); if (abrt) return 1; } 
#define fetch_ea_32(rmdat)              pc++; mod=(rmdat >> 6) & 3; reg=(rmdat >> 3) & 7; rm = rmdat & 7; if (mod != 3) { fetch_ea_32_long(rmdat); } if (abrt) return 1

#define OP_TABLE(name) dynarec_ops_ ## name

#include "386_ops.h"
#endif
