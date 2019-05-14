#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "x86.h"
#include "x87.h"
#include "mem.h"
#include "cpu.h"
#include "fdc.h"
#include "pic.h"
#include "timer.h"
#include "nmi.h"

#include "386_common.h"
#include "codegen.h"

#undef CPU_BLOCK_END
#define CPU_BLOCK_END()

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
//                        pc++; 
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
                        cpu_state.ea_seg = &cpu_state.seg_ss;
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

#define fetch_ea_16(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_16_long(rmdat); if (cpu_state.abrt) return 0; } 
#define fetch_ea_32(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_32_long(rmdat); } if (cpu_state.abrt) return 0

#include "x86_flags.h"

#define getbytef() ((uint8_t)(fetchdat)); cpu_state.pc++
#define getwordf() ((uint16_t)(fetchdat)); cpu_state.pc+=2
#define getbyte2f() ((uint8_t)(fetchdat>>8)); cpu_state.pc++
#define getword2f() ((uint16_t)(fetchdat>>8)); cpu_state.pc+=2


#define OP_TABLE(name) ops_ ## name

#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "x86_ops.h"

void exec386(int cycs)
{
        uint8_t temp;
        uint32_t addr;
        int tempi;
        int cycdiff;
        int oldcyc;

        cycles+=cycs;
//        output=3;
        while (cycles>0)
        {
                int cycle_period = (timer_target - (uint32_t)tsc) + 1;
                
                x86_was_reset = 0;
                cycdiff=0;
                oldcyc=cycles;
//                pclog("%i %02X\n", ins, ram[8]);
                while (cycdiff < cycle_period)
                {
                        int ins_cycles = cycles;
                        
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

                        if (output == 3)
                        {
                                pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X\n",CS,cs,cpu_state.pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,cpu_state.flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask);
                        }
                        cpu_state.pc++;
                        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
                        if (x86_was_reset)
                                break;
                }

                if (cpu_state.abrt)
                {
                        flags_rebuild();
//                        pclog("Abort\n");
//                        if (CS == 0x228) pclog("Abort at %04X:%04X - %i %i %i\n",CS,pc,notpresent,nullseg,abrt);
                        tempi = cpu_state.abrt;
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
                ins_cycles -= cycles;
                tsc += ins_cycles;
                
                cycdiff=oldcyc-cycles;

                if (trap)
                {
                        flags_rebuild();
//                        oldpc=pc;
                        if (msw&1)
                        {
                                pmodeint(1,0);
                        }
                        else
                        {
                                writememw(ss,(SP-2)&0xFFFF,cpu_state.flags);
                                writememw(ss,(SP-4)&0xFFFF,CS);
                                writememw(ss,(SP-6)&0xFFFF,cpu_state.pc);
                                SP-=6;
                                addr = (1 << 2) + idt.base;
                                cpu_state.flags &= ~I_FLAG;
                                cpu_state.flags &= ~T_FLAG;
                                cpu_state.pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                        }
                }
                else if (nmi && nmi_enable && nmi_mask)
                {
                        cpu_state.oldpc = cpu_state.pc;
//                        pclog("NMI\n");
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
                        temp=picinterrupt();
                        if (temp!=0xFF)
                        {
//                                if (temp == 0x54) pclog("Take int 54\n");
//                                if (output) output=3;
//                                if (temp == 0xd) pclog("Hardware int %02X %i %04X(%08X):%08X\n",temp,ins, CS,cs,pc);
//                                if (temp==0x54) output=3;
                                flags_rebuild();
                                if (msw&1)
                                {
                                        pmodeint(temp,0);
                                }
                                else
                                {
                                        writememw(ss,(SP-2)&0xFFFF,cpu_state.flags);
                                        writememw(ss,(SP-4)&0xFFFF,CS);
                                        writememw(ss,(SP-6)&0xFFFF,cpu_state.pc);
                                        SP-=6;
                                        addr = (temp << 2) + idt.base;
                                        cpu_state.flags &= ~I_FLAG;
                                        cpu_state.flags &= ~T_FLAG;
                                        cpu_state.pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
//                                        if (temp==0x76) pclog("INT to %04X:%04X\n",CS,pc);
                                }
//                                pclog("Now at %04X(%08X):%08X\n", CS, cs, pc);
                        }
                }

                ins++;
                insc++;

                if (timetolive)
                {
                        timetolive--;
                        if (!timetolive)
                                fatal("Life expired\n");
                }
                }
                
		if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint32_t)tsc))
			timer_process();
        }
}
