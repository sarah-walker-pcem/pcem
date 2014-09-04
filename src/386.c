#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "x86.h"
#include "x87.h"
#include "mem.h"
#include "cpu.h"
#include "fdc.h"
#include "timer.h"


x86seg *ea_seg;

int nmi_enable = 1;

int inscounts[256];
uint32_t oldpc2;

int trap;

#define check_io_perm(port) if (!IOPLp || (eflags&VM_FLAG)) \
                        { \
                                int tempi = checkio(port); \
                                if (abrt) return 0; \
                                if (tempi) \
                                { \
                                        x86gpf(NULL,0); \
                                        return 0; \
                                } \
                        }

#define checkio_perm(port) if (!IOPLp || (eflags&VM_FLAG)) \
                        { \
                                tempi = checkio(port); \
                                if (abrt) break; \
                                if (tempi) \
                                { \
                                        x86gpf(NULL,0); \
                                        break; \
                                } \
                        }

#define CHECK_READ(seg, low, high)  \
        if ((low < (seg)->limit_low) || (high > (seg)->limit_high))       \
        {                                       \
                x86gpf("Limit check", 0);       \
                return 0;                       \
        }

#define CHECK_WRITE(seg, low, high)  \
        if ((low < (seg)->limit_low) || (high > (seg)->limit_high))       \
        {                                       \
                x86gpf("Limit check", 0);       \
                return 0;                       \
        }

#define CHECK_WRITE_REP(seg, low, high)  \
        if ((low < (seg)->limit_low) || (high > (seg)->limit_high))       \
        {                                       \
                x86gpf("Limit check", 0);       \
                break;                       \
        }


int cpl_override=0;

int has_fpu;
int fpucount=0;
int times;
uint16_t rds;
uint16_t ea_rseg;

int is486;
int cgate32;

#undef readmemb
#undef writememb


#define readmemb(s,a) ((readlookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF)?readmemb386l(s,a): *(uint8_t *)(readlookup2[((s)+(a))>>12] + (s) + (a)) )

#define writememb(s,a,v) if (writelookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF) writememb386l(s,a,v); else *(uint8_t *)(writelookup2[((s) + (a)) >> 12] + (s) + (a)) = v

#define writememw(s,a,v) if (writelookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFE) writememwl(s,a,v); else *(uint16_t *)(writelookup2[((s) + (a)) >> 12] + (s) + (a)) = v
#define writememl(s,a,v) if (writelookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFC) writememll(s,a,v); else *(uint32_t *)(writelookup2[((s) + (a)) >> 12] + (s) + (a)) = v


uint8_t romext[32768];
uint8_t *ram,*rom;
uint16_t biosmask;

uint32_t rmdat32;
#define rmdat rmdat32
#define fetchdat rmdat32
uint32_t backupregs[16];
int oddeven=0;
int inttype,abrt;


uint32_t oldcs2;
uint32_t oldecx;
uint32_t op32;

static inline uint8_t fastreadb(uint32_t a)
{
        if ((a >> 12) == pccache) 
                return *((uint8_t *)&pccache2[a]);
        pccache2 = getpccache(a);
        if (abrt)
                return;
        pccache = a >> 12;
        return *((uint8_t *)&pccache2[a]);
}

static inline uint16_t fastreadw(uint32_t a)
{
        uint32_t t;
        if ((a&0xFFF)>0xFFE)
        {
                t=readmemb(0,a);
                t|=(readmemb(0,a+1)<<8);
                return t;
        }
        if ((a>>12)==pccache) return *((uint16_t *)&pccache2[a]);
        pccache2=getpccache(a);
        if (abrt)
                return;
        pccache=a>>12;
        return *((uint16_t *)&pccache2[a]);
}

static inline uint32_t fastreadl(uint32_t a)
{
        uint8_t *t;
        uint32_t val;
        if ((a&0xFFF)<0xFFD)
        {
                if ((a>>12)!=pccache)
                {
                        t = getpccache(a);
                        if (abrt) return 0;
                        pccache2 = t;
                        pccache=a>>12;
                        //return *((uint32_t *)&pccache2[a]);
                }
                return *((uint32_t *)&pccache2[a]);
        }
        val  =readmemb(0,a);
        val |=(readmemb(0,a+1)<<8);
        val |=(readmemb(0,a+2)<<16);
        val |=(readmemb(0,a+3)<<24);
        return val;
}


static inline uint8_t getbyte()
{
        pc++;
        return fastreadb(cs + (pc - 1));
}

static inline uint16_t getword()
{
        pc+=2;
        return fastreadw(cs+(pc-2));
}

static inline uint32_t getlong()
{
        pc+=4;
        return fastreadl(cs+(pc-4));
}

uint32_t *eal_r, *eal_w;
static inline uint8_t geteab()
{
        if (mod==3)
           return (rm&4)?regs[rm&3].b.h:regs[rm&3].b.l;
        if (eal_r) return *(uint8_t *)eal_r;
        return readmemb(easeg,eaaddr);
}

static inline uint16_t geteaw()
{
        if (mod==3)
           return regs[rm].w;
//        cycles-=3;
        if (eal_r) return *(uint16_t *)eal_r;
        return readmemw(easeg,eaaddr);
}

static inline uint32_t geteal()
{
        if (mod==3)
           return regs[rm].l;
//        cycles-=3;
        if (eal_r) return *eal_r;
        return readmeml(easeg,eaaddr);
}

static inline uint8_t geteab_mem()
{
        if (eal_r) return *(uint8_t *)eal_r;
        return readmemb(easeg,eaaddr);
}
static inline uint16_t geteaw_mem()
{
        if (eal_r) return *(uint16_t *)eal_r;
        return readmemw(easeg,eaaddr);
}
static inline uint32_t geteal_mem()
{
        if (eal_r) return *eal_r;
        return readmeml(easeg,eaaddr);
}

#define seteab(v) if (mod!=3) { if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,eaaddr,v); } else if (rm&4) regs[rm&3].b.h=v; else regs[rm].b.l=v
#define seteaw(v) if (mod!=3) { if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,eaaddr,v);    } else regs[rm].w=v
#define seteal(v) if (mod!=3) { if (eal_w) *eal_w=v;             else writememll(easeg,eaaddr,v);    } else regs[rm].l=v

#define seteab_mem(v) if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,eaaddr,v);
#define seteaw_mem(v) if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,eaaddr,v);
#define seteal_mem(v) if (eal_w) *eal_w=v;             else writememll(easeg,eaaddr,v);

uint16_t *mod1add[2][8];
uint32_t *mod1seg[8];

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

#define fetch_ea_16(rmdat)              pc++; mod=(rmdat >> 6) & 3; reg=(rmdat >> 3) & 7; rm = rmdat & 7; if (mod != 3) { fetch_ea_16_long(rmdat); if (abrt) return 0; } 
#define fetch_ea_32(rmdat)              pc++; mod=(rmdat >> 6) & 3; reg=(rmdat >> 3) & 7; rm = rmdat & 7; if (mod != 3) { fetch_ea_32_long(rmdat); } if (abrt) return 0

#include "x86_flags.h"

void x86_int(int num)
{
        uint32_t addr;
//        pclog("x86_int\n");
        flags_rebuild();
        pc=oldpc;
        if (msw&1)
        {
                pmodeint(num,0);
        }
        else
        {
                if (stack32)
                {
                        writememw(ss,ESP-2,flags);
                        writememw(ss,ESP-4,CS);
                        writememw(ss,ESP-6,pc);
                        ESP-=6;
                }
                else
                {
                        writememw(ss,((SP-2)&0xFFFF),flags);
                        writememw(ss,((SP-4)&0xFFFF),CS);
                        writememw(ss,((SP-6)&0xFFFF),pc);
                        SP-=6;
                }
                addr = (num << 2) + idt.base;

                flags&=~I_FLAG;
                flags&=~T_FLAG;
                oxpc=pc;
                pc=readmemw(0,addr);
                loadcs(readmemw(0,addr+2));
        }
        cycles-=70;
}

void x86_int_sw(int num)
{
        uint32_t addr;
//        pclog("x86_int\n");
        flags_rebuild();
        if (msw&1)
        {
                pmodeint(num,1);
        }
        else
        {
                if (stack32)
                {
                        writememw(ss,ESP-2,flags);
                        writememw(ss,ESP-4,CS);
                        writememw(ss,ESP-6,pc);
                        ESP-=6;
                }
                else
                {
                        writememw(ss,((SP-2)&0xFFFF),flags);
                        writememw(ss,((SP-4)&0xFFFF),CS);
                        writememw(ss,((SP-6)&0xFFFF),pc);
                        SP-=6;
                }
                addr = (num << 2) + idt.base;

                flags&=~I_FLAG;
                flags&=~T_FLAG;
                oxpc=pc;
                pc=readmemw(0,addr);
                loadcs(readmemw(0,addr+2));
        }
        cycles-=70;
        trap = 0;
}

void x86illegal()
{
        uint16_t addr;
        pclog("x86 illegal %04X %08X %04X:%08X %02X\n",msw,cr0,CS,pc,opcode);
//        if (output)
//        {
//                dumpregs();
//                exit(-1);
//        }
        x86_int(6);
}


#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG))\
                { \
                        x86_int(6); \
                        break; \
                }

void rep386(int fv)
{
        uint8_t temp;
        uint32_t c;//=CX;
        uint8_t temp2;
        uint16_t tempw,tempw2,of;
        uint32_t ipc=oldpc;//pc-1;
        uint32_t oldds;
        uint32_t rep32=op32;
        uint32_t templ,templ2;
        int tempz;
        int tempi;
        
        flags_rebuild();
        of = flags;
//        if (output) pclog("REP32 %04X %04X  ",use32,rep32);
        startrep:
        temp=opcode2=readmemb(cs,pc); pc++;
//        if (firstrepcycle && temp==0xA5) pclog("REP MOVSW %06X:%04X %06X:%04X\n",ds,SI,es,DI);
//        if (output) pclog("REP %02X %04X\n",temp,ipc);
        c=(rep32&0x200)?ECX:CX;
/*        if (rep32 && (msw&1))
        {
                if (temp!=0x67 && temp!=0x66 && (rep32|temp)!=0x1AB && (rep32|temp)!=0x3AB) pclog("32-bit REP %03X %08X %04X:%06X\n",temp|rep32,c,CS,pc);
        }*/
        switch (temp|rep32)
        {
                case 0xC3: case 0x1C3: case 0x2C3: case 0x3C3:
                pc--;
                break;
                case 0x08:
                pc=ipc+1;
                break;
                case 0x26: case 0x126: case 0x226: case 0x326: /*ES:*/
                ea_seg = &_es;
                goto startrep;
                break;
                case 0x2E: case 0x12E: case 0x22E: case 0x32E: /*CS:*/
                ea_seg = &_cs;
                goto startrep;
                case 0x36: case 0x136: case 0x236: case 0x336: /*SS:*/
                ea_seg = &_ss;
                goto startrep;
                case 0x3E: case 0x13E: case 0x23E: case 0x33E: /*DS:*/
                ea_seg = &_ds;
                goto startrep;
                case 0x64: case 0x164: case 0x264: case 0x364: /*FS:*/
                ea_seg = &_fs;
                goto startrep;
                case 0x65: case 0x165: case 0x265: case 0x365: /*GS:*/
                ea_seg = &_gs;
                goto startrep;
                case 0x66: case 0x166: case 0x266: case 0x366: /*Data size prefix*/
                rep32^=0x100;
                goto startrep;
                case 0x67: case 0x167: case 0x267: case 0x367:  /*Address size prefix*/
                rep32^=0x200;
                goto startrep;
                case 0x6C: case 0x16C: /*REP INSB*/
                if (c>0)
                {
                        checkio_perm(DX);
                        temp2=inb(DX);
                        writememb(es,DI,temp2);
                        if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        c--;
                        cycles-=15;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x26C: case 0x36C: /*REP INSB*/
                if (c>0)
                {
                        checkio_perm(DX);
                        temp2=inb(DX);
                        writememb(es,EDI,temp2);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        c--;
                        cycles-=15;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x6D: /*REP INSW*/
                if (c>0)
                {
                        tempw=inw(DX);
                        writememw(es,DI,tempw);
                        if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        c--;
                        cycles-=15;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x16D: /*REP INSL*/
                if (c>0)
                {
                        templ=inl(DX);
                        writememl(es,DI,templ);
                        if (abrt) break;
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        c--;
                        cycles-=15;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x26D: /*REP INSW*/
                if (c>0)
                {
                        tempw=inw(DX);
                        writememw(es,EDI,tempw);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        c--;
                        cycles-=15;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x36D: /*REP INSL*/
                if (c>0)
                {
                        templ=inl(DX);
                        writememl(es,EDI,templ);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        c--;
                        cycles-=15;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x6E: case 0x16E: /*REP OUTSB*/
                if (c>0)
                {
                        temp2 = readmemb(ea_seg->base, SI);
                        if (abrt) break;
                        checkio_perm(DX);
                        outb(DX,temp2);
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x26E: case 0x36E: /*REP OUTSB*/
                if (c>0)
                {
                        temp2 = readmemb(ea_seg->base, ESI);
                        if (abrt) break;
                        checkio_perm(DX);
                        outb(DX,temp2);
                        if (flags&D_FLAG) ESI--;
                        else              ESI++;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x6F: /*REP OUTSW*/
                if (c>0)
                {
                        tempw = readmemw(ea_seg->base, SI);
                        if (abrt) break;
//                        pclog("OUTSW %04X -> %04X\n",SI,tempw);
                        outw(DX,tempw);
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x16F: /*REP OUTSL*/
                if (c > 0)
                {
                        templ = readmeml(ea_seg->base, SI);
                        if (abrt) break;
                        outl(DX, templ);
                        if (flags & D_FLAG) SI -= 4;
                        else                SI += 4;
                        c--;
                        cycles -= 14;
                }
                if (c > 0) { firstrepcycle = 0; pc = ipc; }
                else firstrepcycle = 1;
                break;
                case 0x26F: /*REP OUTSW*/
                if (c>0)
                {
                        tempw = readmemw(ea_seg->base, ESI);
                        if (abrt) break;
                        outw(DX,tempw);
                        if (flags&D_FLAG) ESI-=2;
                        else              ESI+=2;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x36F: /*REP OUTSL*/
                if (c > 0)
                {
                        templ = readmeml(ea_seg->base, ESI);
                        if (abrt) break;
                        outl(DX, templ);
                        if (flags & D_FLAG) ESI -= 4;
                        else                ESI += 4;
                        c--;
                        cycles -= 14;
                }
                if (c > 0) { firstrepcycle = 0; pc = ipc; }
                else firstrepcycle = 1;
                break;
                case 0x90: case 0x190: /*REP NOP*/
		case 0x290: case 0x390:
                break;
                case 0xA4: case 0x1A4: /*REP MOVSB*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, DI, DI);
                        temp2 = readmemb(ea_seg->base, SI); if (abrt) break;
                        writememb(es,DI,temp2); if (abrt) break;
//                        if (output==3) pclog("MOVSB %08X:%04X -> %08X:%04X %02X\n",ds,SI,es,DI,temp2);
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x2A4: case 0x3A4: /*REP MOVSB*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, EDI, EDI);
                        temp2 = readmemb(ea_seg->base, ESI); if (abrt) break;
                        writememb(es,EDI,temp2); if (abrt) break;
                        if (flags&D_FLAG) { EDI--; ESI--; }
                        else              { EDI++; ESI++; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0xA5: /*REP MOVSW*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, DI, DI+1);
                        tempw = readmemw(ea_seg->base, SI); if (abrt) break;
                        writememw(es,DI,tempw); if (abrt) break;
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x1A5: /*REP MOVSL*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, DI, DI+3);
                        templ = readmeml(ea_seg->base, SI); if (abrt) break;
//                        pclog("MOVSD %08X from %08X to %08X (%04X:%08X)\n", templ, ds+SI, es+DI, CS, pc);
                        writememl(es,DI,templ); if (abrt) break;
                        if (flags&D_FLAG) { DI-=4; SI-=4; }
                        else              { DI+=4; SI+=4; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x2A5: /*REP MOVSW*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, EDI, EDI+1);
                        tempw = readmemw(ea_seg->base, ESI); if (abrt) break;
                        writememw(es,EDI,tempw); if (abrt) break;
//                        if (output) pclog("Written %04X from %08X to %08X %i  %08X %04X %08X %04X\n",tempw,ds+ESI,es+EDI,c,ds,ES,es,ES);
                        if (flags&D_FLAG) { EDI-=2; ESI-=2; }
                        else              { EDI+=2; ESI+=2; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x3A5: /*REP MOVSL*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, EDI, EDI+3);
                        templ = readmeml(ea_seg->base, ESI); if (abrt) break;
//                        if ((EDI&0xFFFF0000)==0xA0000) cycles-=12;
                        writememl(es,EDI,templ); if (abrt) break;
//                        if (output) pclog("Load %08X from %08X to %08X  %04X %08X  %04X %08X\n",templ,ESI,EDI,DS,ds,ES,es);
                        if (flags&D_FLAG) { EDI-=4; ESI-=4; }
                        else              { EDI+=4; ESI+=4; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0xA6: case 0x1A6: /*REP CMPSB*/
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        temp = readmemb(ea_seg->base, SI);
                        temp2=readmemb(es,DI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub8(temp,temp2);
                        tempz = (ZF_SET()) ? 1 : 0;
                }
                if ((c>0) && (fv==tempz)) { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x2A6: case 0x3A6: /*REP CMPSB*/
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        temp = readmemb(ea_seg->base, ESI);
                        temp2=readmemb(es,EDI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { EDI--; ESI--; }
                        else              { EDI++; ESI++; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub8(temp,temp2);
                        tempz = (ZF_SET()) ? 1 : 0;
                }
                if ((c>0) && (fv==tempz)) { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0xA7: /*REP CMPSW*/
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
//                        pclog("CMPSW %05X %05X  %08X %08X   ", ds+SI, es+DI, readlookup2[(ds+SI)>>12], readlookup2[(es+DI)>>12]);
                        tempw = readmemw(ea_seg->base, SI);
                        tempw2=readmemw(es,DI);
//                        pclog("%04X %04X %02X\n", tempw, tempw2, ram[8]);

                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub16(tempw,tempw2);
                        tempz = (ZF_SET()) ? 1 : 0;
                }
                if ((c>0) && (fv==tempz)) { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x1A7: /*REP CMPSL*/
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        templ = readmeml(ea_seg->base, SI);
                        templ2=readmeml(es,DI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { DI-=4; SI-=4; }
                        else              { DI+=4; SI+=4; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub32(templ,templ2);
                        tempz = (ZF_SET()) ? 1 : 0;
                }
                if ((c>0) && (fv==tempz)) { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x2A7: /*REP CMPSW*/
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        tempw = readmemw(ea_seg->base, ESI);
                        tempw2=readmemw(es,EDI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { EDI-=2; ESI-=2; }
                        else              { EDI+=2; ESI+=2; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub16(tempw,tempw2);
                        tempz = (ZF_SET()) ? 1 : 0;
                }
                if ((c>0) && (fv==tempz)) { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x3A7: /*REP CMPSL*/
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        templ = readmeml(ea_seg->base, ESI);
                        templ2=readmeml(es,EDI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { EDI-=4; ESI-=4; }
                        else              { EDI+=4; ESI+=4; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub32(templ,templ2);
                        tempz = (ZF_SET()) ? 1 : 0;
                }
                if ((c>0) && (fv==tempz)) { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;

                case 0xAA: case 0x1AA: /*REP STOSB*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, DI, DI);
                        writememb(es,DI,AL);
                        if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x2AA: case 0x3AA: /*REP STOSB*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, EDI, EDI);
                        writememb(es,EDI,AL);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0xAB: /*REP STOSW*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, DI, DI+1);
                        writememw(es,DI,AX);
                        if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x2AB: /*REP STOSW*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, EDI, EDI+1);
                        writememw(es,EDI,AX);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x1AB: /*REP STOSL*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, DI, DI+3);
                        writememl(es,DI,EAX);
                        if (abrt) break;
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x3AB: /*REP STOSL*/
                if (c>0)
                {
                        CHECK_WRITE_REP(&_es, EDI, EDI+3);
                        writememl(es,EDI,EAX);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0xAC: case 0x1AC: /*REP LODSB*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSB %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AL = readmemb(ea_seg->base, SI);
                        if (abrt) break;
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x2AC: case 0x3AC: /*REP LODSB*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSB %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AL = readmemb(ea_seg->base, ESI);
                        if (abrt) break;
                        if (flags&D_FLAG) ESI--;
                        else              ESI++;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0xAD: /*REP LODSW*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSW %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AX = readmemw(ea_seg->base, SI);
                        if (abrt) break;
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x1AD: /*REP LODSL*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSL %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        EAX = readmeml(ea_seg->base, SI);
                        if (abrt) break;
                        if (flags&D_FLAG) SI-=4;
                        else              SI+=4;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x2AD: /*REP LODSW*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSW %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AX = readmemw(ea_seg->base, ESI);
                        if (abrt) break;
                        if (flags&D_FLAG) ESI-=2;
                        else              ESI+=2;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0x3AD: /*REP LODSL*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSL %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        EAX = readmeml(ea_seg->base, ESI);
                        if (abrt) break;
                        if (flags&D_FLAG) ESI-=4;
                        else              ESI+=4;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; }
                else firstrepcycle=1;
                break;
                case 0xAE: case 0x1AE: /*REP SCASB*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASB %04X(%06X):%06X\n",CS,cs,pc);
//                tempz=(fv)?1:0;
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        temp2=readmemb(es,DI);
                        if (abrt) { flags=of; break; }
                        setsub8(AL,temp2);
                        tempz = (ZF_SET()) ? 1 : 0;                        
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==tempz))  { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x2AE: case 0x3AE: /*REP SCASB*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASB %04X(%06X):%06X\n",CS,cs,pc);
//                tempz=(fv)?1:0;
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        temp2=readmemb(es,EDI);
                        if (abrt) { flags=of; break; }
//                        if (output) pclog("Compare %02X,%02X\n",temp2,AL);
                        setsub8(AL,temp2);
                        tempz = (ZF_SET()) ? 1 : 0;
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==tempz))  { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0xAF: /*REP SCASW*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASW %04X(%06X):%06X\n",CS,cs,pc);
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        tempw=readmemw(es,DI);
                        if (abrt) { flags=of; break; }
                        setsub16(AX,tempw);
                        tempz = (ZF_SET()) ? 1 : 0;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==tempz))  { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x1AF: /*REP SCASL*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASL %04X(%06X):%06X\n",CS,cs,pc);
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        templ=readmeml(es,DI);
                        if (abrt) { flags=of; break; }
                        setsub32(EAX,templ);
                        tempz = (ZF_SET()) ? 1 : 0;
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==tempz))  { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x2AF: /*REP SCASW*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASW %04X(%06X):%06X\n",CS,cs,pc);
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        tempw=readmemw(es,EDI);
                        if (abrt) { flags=of; break; }
                        setsub16(AX,tempw);
                        tempz = (ZF_SET()) ? 1 : 0;
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==tempz))  { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;
                case 0x3AF: /*REP SCASL*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASL %04X(%06X):%06X\n",CS,cs,pc);
                tempz = (fv) ? 1 : 0;
                if ((c>0) && (fv==tempz))
                {
                        templ=readmeml(es,EDI);
                        if (abrt) { flags=of; break; }
                        setsub32(EAX,templ);
                        tempz = (ZF_SET()) ? 1 : 0;
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==tempz))  { pc=ipc; firstrepcycle=0; }
                else firstrepcycle=1;
                break;


                default:
                        pc=ipc;
                        cycles-=20;
                pclog("Bad REP %02X %i\n", temp, rep32 >> 8);
                x86illegal();
        }
        if (rep32&0x200) ECX=c;
        else             CX=c;
//        if (output) pclog("%03X %03X\n",rep32,use32);
}

int checkio(int port)
{
        uint16_t t;
        uint8_t d;
        cpl_override = 1;
        t = readmemw(tr.base, 0x66);
        cpl_override = 0;
//        pclog("CheckIO 1 %08X\n",tr.base);
        if (abrt) return 0;
//        pclog("CheckIO %04X %01X %01X %02X %04X %04X %08X ",CS,CPL,IOPL,port,t,t+(port>>3),tr.base+t+(port>>3));
        if ((t+(port>>3))>tr.limit) return 1;
        cpl_override = 1;
        d = readmemb386l(0, tr.base + t + (port >> 3));
//        d=readmemb(tr.base,t+(port>>3));
        cpl_override = 0;
//      pclog("%02X %02X\n",d,d&(1<<(port&7)));
        return d&(1<<(port&7));
}

#define getbytef() ((uint8_t)(fetchdat)); pc++
#define getwordf() ((uint16_t)(fetchdat)); pc+=2
#define getbyte2f() ((uint8_t)(fetchdat>>8)); pc++
#define getword2f() ((uint16_t)(fetchdat>>8)); pc+=2
int xout=0;


#define divexcp() { \
                pclog("Divide exception at %04X(%06X):%04X\n",CS,cs,pc); \
                x86_int(0); \
}

void divl(uint32_t val)
{
        if (val==0) 
        {
                divexcp();
                return;
        }
        uint64_t num=(((uint64_t)EDX)<<32)|EAX;
        uint64_t quo=num/val;
        uint32_t rem=num%val;
        uint32_t quo32=(uint32_t)(quo&0xFFFFFFFF);
        if (quo!=(uint64_t)quo32) 
        {
                divexcp();
                return;
        }
        EDX=rem;
        EAX=quo32;
}
void idivl(int32_t val)
{
        if (val==0) 
        {       
                divexcp();
                return;
        }
        int64_t num=(((uint64_t)EDX)<<32)|EAX;
        int64_t quo=num/val;
        int32_t rem=num%val;
        int32_t quo32=(int32_t)(quo&0xFFFFFFFF);
        if (quo!=(int64_t)quo32) 
        {
                divexcp();
                return;
        }
        EDX=rem;
        EAX=quo32;
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

#undef NOTRM
#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG))\
                { \
                        x86_int(6); \
                        return 0; \
                }

#include "386_ops.h"

#undef NOTRM
#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG))\
                { \
                        x86_int(6); \
                        break; \
                }

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
                cycdiff=0;
                oldcyc=cycles;
                timer_start_period(cycles);
//                pclog("%i %02X\n", ins, ram[8]);
                while (cycdiff<100)
                {
            /*            testr[0]=EAX; testr[1]=EBX; testr[2]=ECX; testr[3]=EDX;
                        testr[4]=ESI; testr[5]=EDI; testr[6]=EBP; testr[7]=ESP;*/
/*                        testr[8]=flags;*/
//                oldcs2=oldcs;
//                oldpc2=oldpc;
opcode_realstart:
                oldcs=CS;
                oldpc=pc;
                oldcpl=CPL;
                op32=use32;
                
dontprint=0;

                ea_seg = &_ds;
                ssegs = 0;
                
opcodestart:
                fetchdat = fastreadl(cs + pc);

                if (!abrt)
                {               
                        tempc = CF_SET();
                        trap = flags & T_FLAG;
                        opcode = fetchdat & 0xFF;
                        fetchdat >>= 8;

                        if (output == 3)
                        {
                                pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X %f  %02X%02X %02X%02X %02X%02X  %02X\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,0, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, pit.c[0], ram[0xB270+0x3F5], ram[0xB270+0x3F4], ram[0xB270+0x3F7], ram[0xB270+0x3F6], ram[0xB270+0x3F9], ram[0xB270+0x3F8], ram[0x4430+0x0D49]);
                        }
                        pc++;
                        if (x86_opcodes[(opcode | op32) & 0x3ff](fetchdat))
                                goto opcodestart;
                }

                if (!use32) pc &= 0xffff;

                if (abrt)
                {
                        flags_rebuild();
//                        pclog("Abort\n");
//                        if (CS == 0x228) pclog("Abort at %04X:%04X - %i %i %i\n",CS,pc,notpresent,nullseg,abrt);
/*                        if (testr[0]!=EAX) pclog("EAX corrupted %08X\n",pc);
                        if (testr[1]!=EBX) pclog("EBX corrupted %08X\n",pc);
                        if (testr[2]!=ECX) pclog("ECX corrupted %08X\n",pc);
                        if (testr[3]!=EDX) pclog("EDX corrupted %08X\n",pc);
                        if (testr[4]!=ESI) pclog("ESI corrupted %08X\n",pc);
                        if (testr[5]!=EDI) pclog("EDI corrupted %08X\n",pc);
                        if (testr[6]!=EBP) pclog("EBP corrupted %08X\n",pc);
                        if (testr[7]!=ESP) pclog("ESP corrupted %08X\n",pc);*/
/*                        if (testr[8]!=flags) pclog("FLAGS corrupted %08X\n",pc);*/
                        tempi = abrt;
                        abrt = 0;
                        x86_doabrt(tempi);
                        if (abrt)
                        {
                                abrt = 0;
                                CS = oldcs;
                                pc = oldpc;
                                pclog("Double fault %i\n", ins);
                                pmodeint(8, 0);
                                if (abrt)
                                {
                                        abrt = 0;
                                        softresetx86();
                                        pclog("Triple fault - reset\n");
                                }
                        }
                }
                cycdiff=oldcyc-cycles;

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
                                writememw(ss,(SP-6)&0xFFFF,pc);
                                SP-=6;
                                addr = (1 << 2) + idt.base;
                                flags&=~I_FLAG;
                                flags&=~T_FLAG;
                                pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                        }
                }
                else if (nmi && nmi_enable)
                {
                        oldpc = pc;
                        oldcs = CS;
//                        pclog("NMI\n");
                        x86_int(2);
                        nmi_enable = 0;
                }
                else if ((flags&I_FLAG) && pic_intpending)
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
                                        writememw(ss,(SP-2)&0xFFFF,flags);
                                        writememw(ss,(SP-4)&0xFFFF,CS);
                                        writememw(ss,(SP-6)&0xFFFF,pc);
                                        SP-=6;
                                        addr = (temp << 2) + idt.base;
                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
                                        pc=readmemw(0,addr);
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
                
                tsc += cycdiff;
                
                timer_end_period(cycles);
        }
}
