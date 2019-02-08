#include "ibm.h"
#include "x86.h"
#include "386_common.h"
#include "x86_flags.h"
#include "codegen.h"

x86seg gdt, ldt, idt, tr;

uint32_t cr2, cr3, cr4;
uint32_t dr[8];

uint32_t use32;
int stack32;
int optype;

int trap;

uint32_t rmdat;

uint32_t *eal_r, *eal_w;

int nmi_enable = 1;

int cpl_override=0;

int fpucount=0;

uint16_t cpu_cur_status = 0;

uint32_t pccache;
uint8_t *pccache2;

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
                                writememw(ss,ESP-2,cpu_state.flags);
                                writememw(ss,ESP-4,CS);
                                writememw(ss,ESP-6,cpu_state.pc);
                                ESP-=6;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),cpu_state.flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                                SP-=6;
                        }

                        cpu_state.flags &= ~I_FLAG;
                        cpu_state.flags &= ~T_FLAG;
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
                                writememw(ss,ESP-2,cpu_state.flags);
                                writememw(ss,ESP-4,CS);
                                writememw(ss,ESP-6,cpu_state.pc);
                                ESP-=6;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),cpu_state.flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),cpu_state.pc);
                                SP-=6;
                        }

                        cpu_state.flags &= ~I_FLAG;
                        cpu_state.flags &= ~T_FLAG;
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

        writememw(ss,((SP-2)&0xFFFF),cpu_state.flags); if (cpu_state.abrt) {pclog("abrt5\n"); return 1; }
        writememw(ss,((SP-4)&0xFFFF),CS);
        writememw(ss,((SP-6)&0xFFFF),cpu_state.pc); if (cpu_state.abrt) {pclog("abrt6\n"); return 1; }
        SP-=6;

        cpu_state.eflags &= ~VIF_FLAG;
        cpu_state.flags &= ~T_FLAG;
        cpu_state.pc = new_pc;
        loadcs(new_cs);

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
        d = readmembl(tr.base + t + (port >> 3));
//        d=readmemb(tr.base,t+(port>>3));
        cpl_override = 0;
//      pclog("%02X %02X\n",d,d&(1<<(port&7)));
        return d&(1<<(port&7));
}

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
