//61%
//Win 3.1 - Timer (vtdapi.386) IRQ handler 80020E04 stores to 800200F8

//11A2D
//3EF08 - SYS_FloatTime()

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "x86.h"
#include "x87.h"
#include "mem.h"
#include "cpu.h"
#include "fdc.h"
#include "video.h"

extern int resets;
int gpf = 0;
int ins2 = 0;

//                        pclog("IO perm fail on %04X\n",port); \


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


int cpl_override=0;

int has_fpu;
int fpucount=0;
int times;
uint16_t rds;
uint16_t ea_rseg;

int hitits=0;

uint32_t oldpc2;
int is486;
int cgate32;
uint8_t opcode2;
int incga;
int enters=0;
int ec;

uint32_t ten3688;
int oldv86=0,oldpmode=0,itson=0;
uint8_t old22;
#undef readmemb
#undef writememb

//#define readmemb(s,a) readmemb386l(s,a)
//#define writememb(s,a,v) writememb386l(s,a,v)

//#define is486 1

#define readmemb(s,a) ((readlookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF)?readmemb386l(s,a):ram[readlookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)])

#define writememb(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF) writememb386l(s,a,v); else ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]=v

#define writememw(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFE) writememwl(s,a,v); else *((uint16_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v
#define writememl(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFC) writememll(s,a,v); else *((uint32_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v


/*#undef readmemw
#undef readmeml

#define readmemb(s, a) readmemb386l(s, a)
#define readmemw(s, a) readmemwl(s, a)
#define readmeml(s, a) readmemll(s, a)*/

/*#define writememb(s,a,v) writememb386l(s, a, v)
#define writememw(s,a,v) writememwl(s, a, v)
#define writememl(s,a,v) writememll(s, a, v)*/
//#define readmemb(s,a)    readmemb((s)+(a))
//#define writememb(s,a,v) writememb((s)+(a),v)
uint32_t mmucache[0x100000];

uint8_t romext[32768];
uint8_t *ram,*rom,*vram,*vrom;
uint16_t biosmask;

/*uint16_t getwordx()
{
        pc+=2;
        return readmemw(cs,(pc-2));
}*/

#define getword() getwordx()


//#define readmemb(s,a) readmemb386(s,a)
//#define writememb(s,a,v) writememb386(s,a,v)

//uint32_t fetchdat;
uint32_t rmdat32;
#define rmdat rmdat32
#define fetchdat rmdat32
uint32_t backupregs[16];
int oddeven=0;
int inttype,abrt;


uint32_t oldcs2;
uint32_t oldecx;
uint32_t op32;

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

#define seteab(v) if (mod!=3) { if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,eaaddr,v); } else if (rm&4) regs[rm&3].b.h=v; else regs[rm].b.l=v
#define seteaw(v) if (mod!=3) { if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,eaaddr,v);    } else regs[rm].w=v
#define seteal(v) if (mod!=3) { if (eal_w) *eal_w=v;             else writememll(easeg,eaaddr,v);    } else regs[rm].l=v

static inline void fetcheal32sib()
{
        uint8_t sib;
        sib=rmdat>>8;// pc++;
        switch (mod)
        {
                case 0: eaaddr=regs[sib&7].l; pc++; break;
                case 1: eaaddr=((uint32_t)(int8_t)(rmdat>>16))+regs[sib&7].l; pc+=2; break;
                case 2: eaaddr=(fastreadl(cs+pc+1))+regs[sib&7].l; pc+=5; break;
        }
        /*SIB byte present*/
        if ((sib&7)==5 && !mod) eaaddr=getlong();
        else if ((sib&6)==4)
        {
                easeg=ss;
                ea_rseg=SS;
        }
        if (((sib>>3)&7)!=4) eaaddr+=regs[(sib>>3)&7].l<<(sib>>6);
}

static inline void fetcheal32nosib()
{
        if (rm==5)
        {
                easeg=ss;
                ea_rseg=SS;
        }
        if (mod==1) { eaaddr+=((uint32_t)(int8_t)(rmdat>>8)); pc++; }
        else        { eaaddr+=getlong(); }
}

uint16_t *mod1add[2][8];
uint32_t *mod1seg[8];

void fetchea32()
{
        eal_r = eal_w = NULL;
        if (op32&0x200)
        {
                /*rmdat=readmemw(cs,pc); */pc++;
                reg=(rmdat>>3)&7;
                mod=(rmdat>>6)&3;
                rm=rmdat&7;
                
                if (mod!=3)
                {
                        easeg=ds;
                        ea_rseg=DS;
                        if (rm==4)      fetcheal32sib();
                        else
                        {
                                eaaddr=regs[rm].l;
                                if (mod) fetcheal32nosib();
                                else if (rm==5) eaaddr=getlong();
                        }
                        if (easeg != 0xFFFFFFFF && ((easeg + eaaddr) & 0xFFF) <= 0xFFC)
                        {
                                if ( readlookup2[(easeg + eaaddr) >> 12] != 0xFFFFFFFF)
                                   eal_r = (uint32_t *)&ram[ readlookup2[(easeg + eaaddr) >> 12] + ((easeg + eaaddr) & 0xFFF)];
                                if (writelookup2[(easeg + eaaddr) >> 12] != 0xFFFFFFFF)
                                   eal_w = (uint32_t *)&ram[writelookup2[(easeg + eaaddr) >> 12] + ((easeg + eaaddr) & 0xFFF)];
                        }
                }
        }
        else
        {
                /*rmdat=readmemb(cs,pc); */pc++;
                reg=(rmdat>>3)&7;
                mod=(rmdat>>6)&3;
                rm=rmdat&7;
                if (mod!=3)
                {
                        if (!mod && rm==6) { eaaddr=(rmdat>>8)&0xFFFF; pc+=2; easeg=ds; ea_rseg=DS; }
                        else
                        {
                                switch (mod)
                                {
                                        case 0:
                                        eaaddr=0;
                                        break;
                                        case 1:
                                        eaaddr=(uint16_t)(int8_t)(rmdat>>8); pc++;
                                        break;
                                        case 2:
                                        eaaddr=getword();
                                        break;
                                }
                                eaaddr+=(*mod1add[0][rm])+(*mod1add[1][rm]);
                                easeg=*mod1seg[rm];
                                if (mod1seg[rm]==&ss) ea_rseg=SS;
                                else                  ea_rseg=DS;
                                eaaddr&=0xFFFF;
                        }
                        if (easeg != 0xFFFFFFFF && ((easeg + eaaddr) & 0xFFF) <= 0xFFC)
                        {
                                if ( readlookup2[(easeg + eaaddr) >> 12] != 0xFFFFFFFF)
                                   eal_r = (uint32_t *)&ram[ readlookup2[(easeg + eaaddr) >> 12] + ((easeg + eaaddr) & 0xFFF)];
                                if (writelookup2[(easeg + eaaddr) >> 12] != 0xFFFFFFFF)
                                   eal_w = (uint32_t *)&ram[writelookup2[(easeg + eaaddr) >> 12] + ((easeg + eaaddr) & 0xFFF)];
                        }
/*                        if (readlookup2[(easeg+eaaddr)>>12]!=0xFFFFFFFF && easeg!=0xFFFFFFFF && ((easeg+eaaddr)&0xFFF)<=0xFFC)
                           eal=(uint32_t *)&ram[readlookup2[(easeg+eaaddr)>>12]+((easeg+eaaddr)&0xFFF)];*/
                }
        }
}

#undef fetchea
#define fetchea() fetchea32(); if (abrt) break
#define fetchea2() rmdat=fastreadl(cs+pc); fetchea32(); if (abrt) break

#include "x86_flags.h"

void setadd32(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a+(uint32_t)b;
        flags&=~0x8D5;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if (c<a) flags|=C_FLAG;
        if (!((a^b)&0x80000000)&&((a^c)&0x80000000)) flags|=V_FLAG;
        if (((a&0xF)+(b&0xF))&0x10)      flags|=A_FLAG;
}
void setadd32nc(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a+(uint32_t)b;
        flags&=~0x8D4;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if (!((a^b)&0x80000000)&&((a^c)&0x80000000)) flags|=V_FLAG;
        if (((a&0xF)+(b&0xF))&0x10)      flags|=A_FLAG;
}
void setadc32(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a+(uint32_t)b+tempc;
        flags&=~0x8D5;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if ((c<a) || (c==a && tempc)) flags|=C_FLAG;
        if (!((a^b)&0x80000000)&&((a^c)&0x80000000)) flags|=V_FLAG;
        if (((a&0xF)+(b&0xF)+tempc)&0x10)      flags|=A_FLAG;
}
void setsub32(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a-(uint32_t)b;
        flags&=~0x8D5;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if (c>a) flags|=C_FLAG;
        if ((a^b)&(a^c)&0x80000000) flags|=V_FLAG;
        if (((a&0xF)-(b&0xF))&0x10) flags|=A_FLAG;
}
void setsub32nc(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a-(uint32_t)b;
        flags&=~0x8D4;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if ((a^b)&(a^c)&0x80000000) flags|=V_FLAG;
        if (((a&0xF)-(b&0xF))&0x10)      flags|=A_FLAG;
}
void setsbc32(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a-(((uint32_t)b)+tempc);
        flags&=~0x8D5;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if ((c>a) || (c==a && tempc)) flags|=C_FLAG;
        if ((a^b)&(a^c)&0x80000000) flags|=V_FLAG;
        if (((a&0xF)-((b&0xF)+tempc))&0x10)      flags|=A_FLAG;
}



void x86_int(int num)
{
        uint32_t addr;
                                pc=oldpc;
                                if (msw&1)
                                {
                                        pmodeint(num,0);
                                }
                                else
                                {
                                        if (ssegs) ss=oldss;
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
                                        addr=num<<2;

                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                }
                                cycles-=70;
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
        uint16_t tempw,tempw2,tempw3,of=flags;
        uint32_t ipc=oldpc;//pc-1;
        int changeds=0;
        uint32_t oldds;
        uint32_t rep32=op32;
        uint32_t templ,templ2;
        int tempz;
        int tempi;
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
                oldds=ds;
                ds=es;
                rds=ES;
                changeds=1;
                goto startrep;
                break;
                case 0x2E: case 0x12E: case 0x22E: case 0x32E: /*CS:*/
                oldds=ds;
                ds=cs;
                rds=CS;
                changeds=1;
                goto startrep;
                break;
                case 0x36: case 0x136: case 0x236: case 0x336: /*SS:*/
                oldds=ds;
                ds=ss;
                rds=SS;
                changeds=1;
                goto startrep;
                break;
                case 0x3E: case 0x13E: case 0x23E: case 0x33E: /*DS:*/
                oldds=ds;
                ds=ds;
                changeds=1;
                goto startrep;
                break;
                case 0x64: case 0x164: case 0x264: case 0x364: /*FS:*/
                oldds=ds;
                ds=fs;
                rds=FS;
                changeds=1;
                goto startrep;
                case 0x65: case 0x165: case 0x265: case 0x365: /*GS:*/
                oldds=ds;
                ds=gs;
                rds=GS;
                changeds=1;
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
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
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
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
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
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
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
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
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
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
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
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x6E: case 0x16E: /*REP OUTSB*/
                if (c>0)
                {
                        temp2=readmemb(ds,SI);
                        if (abrt) break;
                        checkio_perm(DX);
                        outb(DX,temp2);
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x26E: case 0x36E: /*REP OUTSB*/
                if (c>0)
                {
                        temp2=readmemb(ds,ESI);
                        if (abrt) break;
                        checkio_perm(DX);
                        outb(DX,temp2);
                        if (flags&D_FLAG) ESI--;
                        else              ESI++;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x6F: /*REP OUTSW*/
                if (c>0)
                {
                        tempw=readmemw(ds,SI);
                        if (abrt) break;
//                        pclog("OUTSW %04X -> %04X\n",SI,tempw);
                        outw(DX,tempw);
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x16F: /*REP OUTSL*/
                if (c > 0)
                {
                        templ = readmeml(ds, SI);
                        if (abrt) break;
                        outl(DX, templ);
                        if (flags & D_FLAG) SI -= 4;
                        else                SI += 4;
                        c--;
                        cycles -= 14;
                }
                if (c > 0) { firstrepcycle = 0; pc = ipc; if (ssegs) ssegs++; }
                else firstrepcycle = 1;
                break;
                case 0x26F: /*REP OUTSW*/
                if (c>0)
                {
                        tempw=readmemw(ds,ESI);
                        if (abrt) break;
                        outw(DX,tempw);
                        if (flags&D_FLAG) ESI-=2;
                        else              ESI+=2;
                        c--;
                        cycles-=14;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x36F: /*REP OUTSL*/
                if (c > 0)
                {
                        templ = readmeml(ds, ESI);
                        if (abrt) break;
                        outl(DX, templ);
                        if (flags & D_FLAG) ESI -= 4;
                        else                ESI += 4;
                        c--;
                        cycles -= 14;
                }
                if (c > 0) { firstrepcycle = 0; pc = ipc; if (ssegs) ssegs++; }
                else firstrepcycle = 1;
                break;
                case 0xA4: case 0x1A4: /*REP MOVSB*/
                if (c>0)
                {
                        temp2=readmemb(ds,SI);  if (abrt) break;
                        writememb(es,DI,temp2); if (abrt) break;
//                        if (output==3) pclog("MOVSB %08X:%04X -> %08X:%04X %02X\n",ds,SI,es,DI,temp2);
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2A4: case 0x3A4: /*REP MOVSB*/
                if (c>0)
                {
                        temp2=readmemb(ds,ESI);  if (abrt) break;
                        writememb(es,EDI,temp2); if (abrt) break;
                        if (flags&D_FLAG) { EDI--; ESI--; }
                        else              { EDI++; ESI++; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xA5: /*REP MOVSW*/
                if (c>0)
                {
                        tempw=readmemw(ds,SI);  if (abrt) break;
                        writememw(es,DI,tempw); if (abrt) break;
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x1A5: /*REP MOVSL*/
                if (c>0)
                {
                        templ=readmeml(ds,SI);  if (abrt) break;
//                        pclog("MOVSD %08X from %08X to %08X (%04X:%08X)\n", templ, ds+SI, es+DI, CS, pc);
                        writememl(es,DI,templ); if (abrt) break;
                        if (flags&D_FLAG) { DI-=4; SI-=4; }
                        else              { DI+=4; SI+=4; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2A5: /*REP MOVSW*/
                if (c>0)
                {
                        tempw=readmemw(ds,ESI);  if (abrt) break;
                        writememw(es,EDI,tempw); if (abrt) break;
//                        if (output) pclog("Written %04X from %08X to %08X %i  %08X %04X %08X %04X\n",tempw,ds+ESI,es+EDI,c,ds,ES,es,ES);
                        if (flags&D_FLAG) { EDI-=2; ESI-=2; }
                        else              { EDI+=2; ESI+=2; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x3A5: /*REP MOVSL*/
                if (c>0)
                {
                        templ=readmeml(ds,ESI); if (abrt) break;
//                        if ((EDI&0xFFFF0000)==0xA0000) cycles-=12;
                        writememl(es,EDI,templ); if (abrt) break;
//                        if (output) pclog("Load %08X from %08X to %08X  %04X %08X  %04X %08X\n",templ,ESI,EDI,DS,ds,ES,es);
                        if (flags&D_FLAG) { EDI-=4; ESI-=4; }
                        else              { EDI+=4; ESI+=4; }
                        c--;
                        cycles-=(is486)?3:4;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xA6: case 0x1A6: /*REP CMPSB*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        temp=readmemb(ds,SI);
                        temp2=readmemb(es,DI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub8(temp,temp2);
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0))) { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2A6: case 0x3A6: /*REP CMPSB*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        temp=readmemb(ds,ESI);
                        temp2=readmemb(es,EDI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { EDI--; ESI--; }
                        else              { EDI++; ESI++; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub8(temp,temp2);
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0))) { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xA7: /*REP CMPSW*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
//                        pclog("CMPSW %05X %05X  %08X %08X   ", ds+SI, es+DI, readlookup2[(ds+SI)>>12], readlookup2[(es+DI)>>12]);
                        tempw=readmemw(ds,SI);
                        tempw2=readmemw(es,DI);
//                        pclog("%04X %04X %02X\n", tempw, tempw2, ram[8]);

                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub16(tempw,tempw2);
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0))) { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x1A7: /*REP CMPSL*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        templ=readmeml(ds,SI);
                        templ2=readmeml(es,DI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { DI-=4; SI-=4; }
                        else              { DI+=4; SI+=4; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub32(templ,templ2);
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0))) { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2A7: /*REP CMPSW*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        tempw=readmemw(ds,ESI);
                        tempw2=readmemw(es,EDI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { EDI-=2; ESI-=2; }
                        else              { EDI+=2; ESI+=2; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub16(tempw,tempw2);
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0))) { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x3A7: /*REP CMPSL*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        templ=readmeml(ds,ESI);
                        templ2=readmeml(es,EDI);
                        if (abrt) { flags=of; break; }
                        if (flags&D_FLAG) { EDI-=4; ESI-=4; }
                        else              { EDI+=4; ESI+=4; }
                        c--;
                        cycles-=(is486)?7:9;
                        setsub32(templ,templ2);
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0))) { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;

                case 0xAA: case 0x1AA: /*REP STOSB*/
                if (c>0)
                {
                        writememb(es,DI,AL);
                        if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2AA: case 0x3AA: /*REP STOSB*/
                if (c>0)
                {
                        writememb(es,EDI,AL);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xAB: /*REP STOSW*/
                if (c>0)
                {
                        writememw(es,DI,AX);
                        if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2AB: /*REP STOSW*/
                if (c>0)
                {
                        writememw(es,EDI,AX);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x1AB: /*REP STOSL*/
                if (c>0)
                {
                        writememl(es,DI,EAX);
                        if (abrt) break;
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x3AB: /*REP STOSL*/
                if (c>0)
                {
                        writememl(es,EDI,EAX);
                        if (abrt) break;
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        c--;
                        cycles-=(is486)?4:5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xAC: case 0x1AC: /*REP LODSB*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSB %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AL=readmemb(ds,SI);
                        if (abrt) break;
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2AC: case 0x3AC: /*REP LODSB*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSB %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AL=readmemb(ds,ESI);
                        if (abrt) break;
                        if (flags&D_FLAG) ESI--;
                        else              ESI++;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xAD: /*REP LODSW*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSW %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AX=readmemw(ds,SI);
                        if (abrt) break;
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x1AD: /*REP LODSL*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSL %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        EAX=readmeml(ds,SI);
                        if (abrt) break;
                        if (flags&D_FLAG) SI-=4;
                        else              SI+=4;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2AD: /*REP LODSW*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSW %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        AX=readmemw(ds,ESI);
                        if (abrt) break;
                        if (flags&D_FLAG) ESI-=2;
                        else              ESI+=2;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x3AD: /*REP LODSL*/
//                if (ds==0xFFFFFFFF) pclog("Null selector REP LODSL %04X(%06X):%06X\n",CS,cs,pc);
                if (c>0)
                {
                        EAX=readmeml(ds,ESI);
                        if (abrt) break;
                        if (flags&D_FLAG) ESI-=4;
                        else              ESI+=4;
                        c--;
                        cycles-=5;
                }
                if (c>0) { firstrepcycle=0; pc=ipc; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xAE: case 0x1AE: /*REP SCASB*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASB %04X(%06X):%06X\n",CS,cs,pc);
//                tempz=(fv)?1:0;
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        temp2=readmemb(es,DI);
                        if (abrt) { flags=of; break; }
                        setsub8(AL,temp2);
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))  { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2AE: case 0x3AE: /*REP SCASB*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASB %04X(%06X):%06X\n",CS,cs,pc);
//                tempz=(fv)?1:0;
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
//                if (output) pclog("REP SCASB - %i %04X %i\n",fv,flags&Z_FLAG,c);
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        temp2=readmemb(es,EDI);
                        if (abrt) { flags=of; break; }
//                        if (output) pclog("Compare %02X,%02X\n",temp2,AL);
                        setsub8(AL,temp2);
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))  { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0xAF: /*REP SCASW*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASW %04X(%06X):%06X\n",CS,cs,pc);
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        tempw=readmemw(es,DI);
                        if (abrt) { flags=of; break; }
                        setsub16(AX,tempw);
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))  { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x1AF: /*REP SCASL*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASL %04X(%06X):%06X\n",CS,cs,pc);
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        templ=readmeml(es,DI);
                        if (abrt) { flags=of; break; }
                        setsub32(EAX,templ);
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))  { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x2AF: /*REP SCASW*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASW %04X(%06X):%06X\n",CS,cs,pc);
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        tempw=readmemw(es,EDI);
                        if (abrt) { flags=of; break; }
                        setsub16(AX,tempw);
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))  { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;
                case 0x3AF: /*REP SCASL*/
//                if (es==0xFFFFFFFF) pclog("Null selector REP SCASL %04X(%06X):%06X\n",CS,cs,pc);
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        templ=readmeml(es,EDI);
                        if (abrt) { flags=of; break; }
                        setsub32(EAX,templ);
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        c--;
                        cycles-=(is486)?5:8;
                }
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))  { pc=ipc; firstrepcycle=0; if (ssegs) ssegs++; }
                else firstrepcycle=1;
                break;


                default:
                        pc=ipc;
                        cycles-=20;
                x86illegal();
                pclog("Bad REP %02X %i\n",temp,rep32>>8);
        }
        if (rep32&0x200) ECX=c;
        else             CX=c;
        if (changeds) ds=oldds;
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
        d=readmemb(tr.base,t+(port>>3));
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
                pc=oldpc; \
                if (msw&1) pmodeint(0,0); \
                else \
                { \
                        writememw(ss,(SP-2)&0xFFFF,flags); \
                        writememw(ss,(SP-4)&0xFFFF,CS); \
                        writememw(ss,(SP-6)&0xFFFF,pc); \
                        SP-=6; \
                        flags&=~I_FLAG; \
                        oxpc=pc; \
                        pc=readmemw(0,0); \
                        loadcs(readmemw(0,2)); \
                } \
                return; \
}

void divl(uint32_t val)
{
        if (val==0) divexcp();
        uint64_t num=(((uint64_t)EDX)<<32)|EAX;
        uint64_t quo=num/val;
        uint32_t rem=num%val;
        uint32_t quo32=(uint32_t)(quo&0xFFFFFFFF);
        if (quo!=(uint64_t)quo32) divexcp();
        EDX=rem;
        EAX=quo32;
}
void idivl(int32_t val)
{
        if (val==0) divexcp();
        int64_t num=(((uint64_t)EDX)<<32)|EAX;
        int64_t quo=num/val;
        int32_t rem=num%val;
        int32_t quo32=(int32_t)(quo&0xFFFFFFFF);
        if (quo!=(int64_t)quo32) divexcp();
        EDX=rem;
        EAX=quo32;
}

int oldi;

uint32_t testr[9];
int dontprint=0;
void exec386(int cycs)
{
        uint64_t temp64;
        int64_t temp64i;
        uint8_t temp,temp2;
        uint16_t tempw,tempw2,tempw3,tempw4;
        int8_t offset;
        int8_t temps;
        int16_t temps16;
        volatile int tempws,tempws2;
        uint32_t templ,templ2,templ3,addr;
        int c,cycdiff;
        int oldcyc;
        int tempi;
        int cyctot=0;
        int trap;

        FILE *f;
        
        cycles+=cycs;
//        output=3;
        while (cycles>0)
        {
                cycdiff=0;
                oldcyc=cycles;
//                pclog("%i %02X\n", ins, ram[8]);
                while (cycdiff<100)
                {
/*                        testr[0]=EAX; testr[1]=EBX; testr[2]=ECX; testr[3]=EDX;
                        testr[4]=ESI; testr[5]=EDI; testr[6]=EBP; testr[7]=ESP;
                        testr[8]=flags;*/
//                oldcs2=oldcs;
//                oldpc2=oldpc;
                oldcs=CS;
                oldpc=pc;
                oldcpl=CPL;
                op32=use32;
                
dontprint=0;

                opcodestart:
                fetchdat=fastreadl(cs+pc);
                if (abrt) goto opcodeend;
                
                tempc=flags&C_FLAG;
                trap=flags&T_FLAG;
                opcode=fetchdat&0xFF;
                fetchdat>>=8;

                if (output==3 && !dontprint)
                {
                        if ((opcode!=0xF2 && opcode!=0xF3) || firstrepcycle)
                        {
                                if (!skipnextprint)
                                {
                                        pclog("%04X(%06X):%04X : %08X %08X %08X %08X %04X %04X %04X(%08X) %04X %04X %04X(%08X) %08X %08X %08X SP=%04X:%08X %02X %04X %i %08X  %08X %i %i %02X %02X %02X   %02X %02X %02X\n",CS,cs,pc,EAX,EBX,ECX,EDX,CS,DS,ES,es,FS,GS,SS,ss,EDI,ESI,EBP,SS,ESP,opcode,flags,ins,writelookup2, ldt.base, CPL, stack32, pic.pend, pic.mask, pic.mask2, pic2.pend, pic2.mask, readmode);
                                        //x87_print();
                                }
                                skipnextprint=0;
                        }
                }
                dontprint=1;
//#endif
                pc++;
                inhlt=0;
                switch ((opcode|op32)&0x3FF)
                {
                        case 0x00: case 0x100: case 0x200: case 0x300: /*ADD 8,reg*/
                        fetchea();
                        //if (!rmdat && output) pc--;
/*                        if (!rmdat && output)
                        {
                                fatal("Crashed\n");
//                                clear_keybuf();
//                                readkey();


                        }*/
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setadd8(temp, temp2);
                                setr8(rm, temp + temp2);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();     if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp + temp2); if (abrt) break;
                                setadd8(temp, temp2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x01: case 0x201: /*ADD 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setadd16(regs[rm].w, regs[reg].w);
                                regs[rm].w += regs[reg].w;
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw = geteaw();             if (abrt) break;
                                seteaw(tempw + regs[reg].w);  if (abrt) break;
                                setadd16(tempw, regs[reg].w);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x101: case 0x301: /*ADD 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setadd32(regs[rm].l, regs[reg].l);
                                regs[rm].l += regs[reg].l;
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ = geteal();             if (abrt) break;
                                seteal(templ + regs[reg].l);  if (abrt) break;
                                setadd32(templ, regs[reg].l);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x02: case 0x102: case 0x202: case 0x302: /*ADD reg,8*/
                        fetchea();
                        temp=geteab();        if (abrt) break;
                        setadd8(getr8(reg),temp);
                        setr8(reg,getr8(reg)+temp);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x03: case 0x203: /*ADD reg,16*/
                        fetchea();
                        tempw=geteaw();       if (abrt) break;
                        setadd16(regs[reg].w,tempw);
                        regs[reg].w+=tempw;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x103: case 0x303: /*ADD reg,32*/
                        fetchea();
                        templ=geteal();       if (abrt) break;
                        setadd32(regs[reg].l,templ);
                        regs[reg].l+=templ;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x04: case 0x104: case 0x204: case 0x304: /*ADD AL,#8*/
                        temp=getbytef();
                        setadd8(AL,temp);
                        AL+=temp;
                        cycles -= timing_rr;
                        break;
                        case 0x05: case 0x205: /*ADD AX,#16*/
                        tempw=getwordf();
                        setadd16(AX,tempw);
                        AX+=tempw;
                        cycles -= timing_rr;
                        break;
                        case 0x105: case 0x305: /*ADD EAX,#32*/
                        templ=getlong(); if (abrt) break;
                        setadd32(EAX,templ);
                        EAX+=templ;
                        cycles -= timing_rr;
                        break;

                        case 0x06: case 0x206: /*PUSH ES*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,ES);           if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),ES); if (abrt) break;
                                SP-=2;
                        }
                        cycles-=2;
                        break;
                        case 0x106: case 0x306: /*PUSH ES*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,ES);           if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),ES); if (abrt) break;
                                SP-=4;
                        }
                        cycles-=2;
                        break;
                        case 0x07: case 0x207: /*POP ES*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP);         if (abrt) break;
                                loadseg(tempw,&_es);            if (abrt) break;
                                ESP+=2;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);          if (abrt) break;
                                loadseg(tempw,&_es);            if (abrt) break;
                                SP+=2;
                        }
                        cycles-=(is486)?3:7;
                        break;
                        case 0x107: case 0x307: /*POP ES*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmeml(ss,ESP)&0xFFFF;  if (abrt) break;
                                loadseg(tempw,&_es);            if (abrt) break;
                                ESP+=4;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);          if (abrt) break;
                                loadseg(tempw,&_es);            if (abrt) break;
                                SP+=4;
                        }
                        cycles-=(is486)?3:7;
                        break;


                        case 0x08: case 0x108: case 0x208: case 0x308: /*OR 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm) | getr8(reg);
                                setr8(rm, temp);
                                setznp8(temp);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();          if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp | temp2);     if (abrt) break;
                                setznp8(temp | temp2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x09: case 0x209: /*OR 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].w |= regs[reg].w;
                                setznp16(regs[rm].w);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw = geteaw() | regs[reg].w; if (abrt) break;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x109: case 0x309: /*OR 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].l |= regs[reg].l;
                                setznp32(regs[rm].l);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ = geteal() | regs[reg].l; if (abrt) break;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x0A: case 0x10A: case 0x20A: case 0x30A: /*OR reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp|=getr8(reg);
                        setznp8(temp);
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x0B: case 0x20B: /*OR reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw|=regs[reg].w;
                        setznp16(tempw);
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x10B: case 0x30B: /*OR reg,32*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        templ|=regs[reg].l;
                        setznp32(templ);
                        regs[reg].l=templ;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x0C: case 0x10C: case 0x20C: case 0x30C: /*OR AL,#8*/
                        AL|=getbytef();
                        setznp8(AL);
                        cycles -= timing_rr;
                        break;
                        case 0x0D: case 0x20D: /*OR AX,#16*/
                        AX|=getwordf();
                        setznp16(AX);
                        cycles -= timing_rr;
                        break;
                        case 0x10D: case 0x30D: /*OR AX,#32*/
                        templ=getlong(); if (abrt) break;
                        EAX|=templ;
                        setznp32(EAX);
                        cycles -= timing_rr;
                        break;

                        case 0x0E: case 0x20E: /*PUSH CS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,CS);                 if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),CS);       if (abrt) break;
                                SP-=2;
                        }
                        cycles-=2;
                        break;
                        case 0x10E: case 0x30E: /*PUSH CS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,CS);                 if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),CS);       if (abrt) break;
                                SP-=4;
                        }
                        cycles-=2;
                        break;

                        case 0x0F: case 0x20F:
                        temp=fetchdat&0xFF/*readmemb(cs+pc)*/; pc++;
                        opcode2=temp;
//                        if (temp>5 && temp!=0x82 && temp!=0x85 && temp!=0x84 && temp!=0x87 && temp!=0x8D && temp!=0x8F && temp!=0x8C && temp!=0x20 && temp!=0x22) pclog("Using magic 386 0F instruction %02X!\n",temp);
                        switch (temp)
                        {
                                case 0:
                                if (!(cr0&1) || (eflags&VM_FLAG)) goto inv16;
                                fetchea2(); if (abrt) break;
                                switch (rmdat&0x38)
                                {
                                        case 0x00: /*SLDT*/
                                        NOTRM
                                        seteaw(ldt.seg);
                                        cycles-=4;
                                        break;
                                        case 0x08: /*STR*/
                                        NOTRM
                                        seteaw(tr.seg);
                                        cycles-=4;
                                        break;
                                        case 0x10: /*LLDT*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LLDT!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        NOTRM
                                        ldt.seg=geteaw();
//                                        pclog("Load LDT %04X ",ldt.seg);
                                        templ=(ldt.seg&~7)+gdt.base;
//                                        pclog("%06X ",gdt.base);
                                        templ3=readmemw(0,templ)+((readmemb(0,templ+6)&0xF)<<16);
                                        templ2=(readmemw(0,templ+2))|(readmemb(0,templ+4)<<16)|(readmemb(0,templ+7)<<24);
                                        if (abrt) break;
                                        ldt.limit=templ3;
                                        ldt.access=readmemb(0,templ+6);
                                        if (readmemb(0,templ+6)&0x80)
                                        {
                                                ldt.limit<<=12;
                                                ldt.limit|=0xFFF;
                                        }
                                        ldt.base=templ2;
//                                        /*if (output==3) */pclog("LLDT %04X %08X %04X  %08X %08X\n",ldt.seg,ldt.base,ldt.limit,readmeml(0,templ),readmeml(0,templ+4));

                                        cycles-=20;
                                        break;
                                        case 0x18: /*LTR*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LTR!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        NOTRM
                                        tr.seg=geteaw();
                                        templ=(tr.seg&~7)+gdt.base;
                                        templ3=readmemw(0,templ)+((readmemb(0,templ+6)&0xF)<<16);
                                        templ2=(readmemw(0,templ+2))|(readmemb(0,templ+4)<<16)|(readmemb(0,templ+7)<<24);
                                        temp=readmemb(0,templ+5);
                                        if (abrt) break;
                                        tr.limit=templ3;
                                        tr.access=readmemb(0,templ+6);
                                        if (readmemb(0,templ+6)&0x80)
                                        {
                                                tr.limit<<=12;
                                                tr.limit|=0xFFF;
                                        }
                                        tr.base=templ2;
//                                        pclog("TR base = %08X\n",templ2);
                                        tr.access=temp;
                                        cycles-=20;
                                        break;
                                        case 0x20: /*VERR*/
                                        NOTRM
                                        tempw=geteaw(); if (abrt) break;
                                        flags&=~Z_FLAG;
                                        if (!(tempw&0xFFFC)) break; /*Null selector*/
                                        cpl_override=1;
                                        tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                        cpl_override=0;
                                        if (abrt) break;
                                        if (!(tempw2&0x1000)) tempi=0;
                                        if ((tempw2&0xC00)!=0xC00) /*Exclude conforming code segments*/
                                        {
                                                tempw3=(tempw2>>13)&3; /*Check permissions*/
                                                if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                        }
                                        if ((tempw2&0x0800) && !(tempw2&0x0200)) tempi=0; /*Non-readable code*/
                                        if (tempi) flags|=Z_FLAG;
                                        cycles-=20;
                                        break;
                                        case 0x28: /*VERW*/
                                        NOTRM
                                        tempw=geteaw(); if (abrt) break;
                                        flags&=~Z_FLAG;
                                        if (!(tempw&0xFFFC)) break; /*Null selector*/
                                        cpl_override=1;
                                        tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                        cpl_override=0;
                                        if (abrt) break;
                                        if (!(tempw2&0x1000)) tempi=0;
                                        tempw3=(tempw2>>13)&3; /*Check permissions*/
                                        if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                        if (tempw2&0x0800) tempi=0; /*Code*/
                                        else if (!(tempw2&0x0200)) tempi=0; /*Read-only data*/
                                        if (tempi) flags|=Z_FLAG;
                                        cycles-=20;
                                        break;


                                        default:
                                        pclog("Bad 0F 00 opcode %02X\n",rmdat&0x38);
                                        pc-=3;
                                        x86illegal();
                                        break;
                                }
                                break;
                                case 1:
                                fetchea2(); if (abrt) break;
                                switch (rmdat&0x38)
                                {
                                        case 0x00: /*SGDT*/
                                        seteaw(gdt.limit);
                                        writememw(easeg,eaaddr+2,gdt.base);
                                        writememw(easeg,eaaddr+4,(gdt.base>>16)&0xFF);
                                        cycles-=7; //less seteaw time
                                        break;
                                        case 0x08: /*SIDT*/
                                        seteaw(idt.limit);
                                        writememw(easeg,eaaddr+2,idt.base);
                                        writememw(easeg,eaaddr+4,(idt.base>>16)&0xFF);
                                        cycles-=7;
                                        break;
                                        case 0x10: /*LGDT*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LGDT!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw();
                                        templ=readmeml(0,easeg+eaaddr+2)&0xFFFFFF;
                                        if (abrt) break;
                                        gdt.limit=tempw;
                                        gdt.base=templ;
                                        cycles-=11;
                                        break;
                                        case 0x18: /*LIDT*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LIDT!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw();
                                        templ=readmeml(0,easeg+eaaddr+2)&0xFFFFFF;
                                        if (abrt) break;
                                        idt.limit=tempw;
                                        idt.base=templ;
                                        cycles-=11;
                                        break;

                                        case 0x20: /*SMSW*/
                                        if (is486) seteaw(msw);
                                        else       seteaw(msw|0xFF00);
//                                        pclog("SMSW %04X:%04X  %04X %i\n",CS,pc,msw,is486);
                                        cycles-=2;
                                        break;
                                        case 0x30: /*LMSW*/
                                        if ((CPL || eflags&VM_FLAG) && (msw&1))
                                        {
                                                pclog("LMSW - ring not zero!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw(); if (abrt) break;
                                        if (msw&1) tempw|=1;
                                        msw=tempw;
                                        break;

                                        default:
                                        pclog("Bad 0F 01 opcode %02X\n",rmdat&0x38);
                                        pc-=3;
                                        x86illegal();
                                        break;
                                }
                                break;

                                case 2: /*LAR*/
                                NOTRM
                                fetchea2();
                                tempw=geteaw(); if (abrt) break;
//                                pclog("LAR seg %04X\n",tempw);
                                
                                if (!(tempw&0xFFFC)) { flags&=~Z_FLAG; break; } /*Null selector*/
                                tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                if (tempi)
                                {
                                        cpl_override=1;
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                        cpl_override=0;
                                        if (abrt) break;
                                }
                                flags&=~Z_FLAG;
//                                pclog("tempw2 %04X  %i  %04X %04X\n",tempw2,tempi,ldt.limit,gdt.limit);
                                if ((tempw2&0x1F00)==0x000) tempi=0;
                                if ((tempw2&0x1F00)==0x800) tempi=0;
                                if ((tempw2&0x1F00)==0xA00) tempi=0;
                                if ((tempw2&0x1F00)==0xD00) tempi=0;
                                if ((tempw2&0x1C00)<0x1C00) /*Exclude conforming code segments*/
                                {
                                        tempw3=(tempw2>>13)&3;
                                        if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                }
                                if (tempi)
                                {
                                        flags|=Z_FLAG;
                                        cpl_override=1;
                                        regs[reg].w=readmemb(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+5)<<8;
                                        cpl_override=0;
                                }
                                cycles-=11;
                                break;

                                case 3: /*LSL*/
                                NOTRM
                                fetchea2();
                                tempw=geteaw(); if (abrt) break;
                                flags&=~Z_FLAG;
                                if (!(tempw&0xFFFC)) break; /*Null selector*/
                                tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                cpl_override=1;
                                if (tempi)
                                {
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                }
                                cpl_override=0;
                                if (abrt) break;
                                if ((tempw2&0x1400)==0x400) tempi=0; /*Interrupt or trap or call gate*/
                                if ((tempw2&0x1F00)==0x000) tempi=0; /*Invalid*/
                                if ((tempw2&0x1F00)==0xA00) tempi=0; /*Invalid*/
                                if ((tempw2&0x1C00)!=0x1C00) /*Exclude conforming code segments*/
                                {
                                        tempw3=(tempw2>>13)&3;
                                        if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                }
                                if (tempi)
                                {
                                        flags|=Z_FLAG;
                                        cpl_override=1;
                                        regs[reg].w=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7));
                                        cpl_override=0;
                                }
                                cycles-=10;
                                break;

                                case 5: /*LOADALL*/
//                                pclog("At %04X:%04X  ",CS,pc);
                                flags=(readmemw(0,0x818)&0xFFD5)|2;
                                pc=readmemw(0,0x81A);
                                DS=readmemw(0,0x81E);
                                SS=readmemw(0,0x820);
                                CS=readmemw(0,0x822);
                                ES=readmemw(0,0x824);
                                DI=readmemw(0,0x826);
                                SI=readmemw(0,0x828);
                                BP=readmemw(0,0x82A);
                                SP=readmemw(0,0x82C);
                                BX=readmemw(0,0x82E);
                                DX=readmemw(0,0x830);
                                CX=readmemw(0,0x832);
                                AX=readmemw(0,0x834);
                                es=readmemw(0,0x836)|(readmemb(0,0x838)<<16);
                                cs=readmemw(0,0x83C)|(readmemb(0,0x83E)<<16);
                                ss=readmemw(0,0x842)|(readmemb(0,0x844)<<16);
                                ds=readmemw(0,0x848)|(readmemb(0,0x84A)<<16);
                                cycles-=195;
//                                pclog("LOADALL - %06X:%04X %06X:%04X %04X\n",ds,SI,es,DI,DX);
                                break;
                                
                                case 6: /*CLTS*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't CLTS\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                cr0&=~8;
                                cycles-=5;
                                break;
                                
                                case 8: /*INVD*/
                                if (!is486) goto inv16;
                                cycles-=1000;
                                break;
                                case 9: /*WBINVD*/
                                if (!is486) goto inv16;
                                cycles-=10000;
                                break;

                                case 0x20: /*MOV reg32,CRx*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load from CRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                switch (reg)
                                {
                                        case 0:
                                        regs[rm].l=cr0;//&~2;
                                        if (is486) regs[rm].l|=0x10; /*ET hardwired on 486*/
                                        break;
                                        case 2:
                                        regs[rm].l=cr2;
                                        break;
                                        case 3:
                                        regs[rm].l=cr3;
                                        break;
                                        default:
                                        pclog("Bad read of CR%i %i\n",rmdat&7,reg);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                cycles-=6;
                                break;
                                case 0x21: /*MOV reg32,DRx*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load from DRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                regs[rm].l=0;
                                cycles-=6;
                                break;
                                case 0x22: /*MOV CRx,reg32*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load CRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                switch (reg)
                                {
                                        case 0:
                                        //if ((cr0^regs[rm].l)>>31) flushmmucache();
                                        if ((regs[rm].l^cr0) & 0x80000001) flushmmucache();
                                        cr0=regs[rm].l;//&~2;
                                        if (cpu_16bitbus) cr0 |= 0x10;
//                                        if (cs<0xF0000 && cr0&1) output=3;
                                        if (cr0&0x80000000)
                                        {
                                        }
                                        else mmu_perm=4;
                                        break;
                                        case 2:
                                        cr2=regs[rm].l;
                                        break;
                                        case 3:
                                        cr3=regs[rm].l&~0xFFF;
//                                        pclog("Loading CR3 with %08X at %08X:%08X\n",cr3,cs,pc);
//                                        if (pc==0x204) output=3;
                                        flushmmucache();
                                        break;
                                        default:
                                        pclog("Bad load CR%i\n",reg);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                cycles-=10;
                                break;
                                case 0x23: /*MOV DRx,reg32*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load DRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                cycles-=6;
                                break;
                                case 0x24: /*MOV reg32,TRx*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load from TRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                regs[rm].l=0;
                                cycles-=6;
                                break;
                                case 0x26: /*MOV TRx,reg32*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load TRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                cycles-=6;
                                break;

                                case 0x32: x86illegal(); break;
                                
                                case 0x80: /*JO*/
                                tempw=getword2f();
                                if (flags&V_FLAG) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x81: /*JNO*/
                                tempw=getword2f();
                                if (!(flags&V_FLAG)) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x82: /*JB*/
                                tempw=getword2f();
                                if (flags&C_FLAG) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x83: /*JNB*/
                                tempw=getword2f();
                                if (!(flags&C_FLAG)) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x84: /*JE*/
                                tempw=getword2f();
                                if (flags&Z_FLAG) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x85: /*JNE*/
                                tempw=getword2f();
                                if (!(flags&Z_FLAG)) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x86: /*JBE*/
                                tempw=getword2f();
                                if (flags&(C_FLAG|Z_FLAG)) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x87: /*JNBE*/
                                tempw=getword2f();
                                if (!(flags&(C_FLAG|Z_FLAG))) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x88: /*JS*/
                                tempw=getword2f();
                                if (flags&N_FLAG) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x89: /*JNS*/
                                tempw=getword2f();
                                if (!(flags&N_FLAG)) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x8A: /*JP*/
                                tempw=getword2f();
                                if (flags&P_FLAG) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x8B: /*JNP*/
                                tempw=getword2f();
                                if (!(flags&P_FLAG)) { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x8C: /*JL*/
                                tempw=getword2f();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if (temp!=temp2)  { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x8D: /*JNL*/
                                tempw=getword2f();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if (temp==temp2)  { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x8E: /*JLE*/
                                tempw=getword2f();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if ((flags&Z_FLAG) || (temp!=temp2))  { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;
                                case 0x8F: /*JNLE*/
                                tempw=getword2f();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if (!((flags&Z_FLAG) || (temp!=temp2)))  { pc+=(int16_t)tempw; cycles-=timing_bt; }
                                cycles -= timing_bnt;
                                break;

                                case 0x90: /*SETO*/
                                fetchea2();
                                seteab((flags&V_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x91: /*SETNO*/
                                fetchea2();
                                seteab((flags&V_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x92: /*SETC*/
                                fetchea2();
                                seteab((flags&C_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x93: /*SETAE*/
                                fetchea2();
                                seteab((flags&C_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x94: /*SETZ*/
                                fetchea2();
                                seteab((flags&Z_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x95: /*SETNZ*/
                                fetchea2();
                                seteab((flags&Z_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x96: /*SETBE*/
                                fetchea2();
                                seteab((flags&(C_FLAG|Z_FLAG))?1:0);
                                cycles-=4;
                                break;
                                case 0x97: /*SETNBE*/
                                fetchea2();
                                seteab((flags&(C_FLAG|Z_FLAG))?0:1);
                                cycles-=4;
                                break;
                                case 0x98: /*SETS*/
                                fetchea2();
                                seteab((flags&N_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x99: /*SETNS*/
                                fetchea2();
                                seteab((flags&N_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x9A: /*SETP*/
                                fetchea2();
                                seteab((flags&P_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x9B: /*SETNP*/
                                fetchea2();
                                seteab((flags&P_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x9C: /*SETL*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab(temp^temp2);
                                cycles-=4;
                                break;
                                case 0x9D: /*SETGE*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab((temp^temp2)?0:1);
                                cycles-=4;
                                break;
                                case 0x9E: /*SETLE*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab(((temp^temp2) || (flags&Z_FLAG))?1:0);
                                cycles-=4;
                                break;
                                case 0x9F: /*SETNLE*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab(((temp^temp2) || (flags&Z_FLAG))?0:1);
                                cycles-=4;
                                break;

                                case 0xA0: /*PUSH FS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememw(ss,ESP-2,FS); if (abrt) break;
                                        ESP-=2;
                                }
                                else
                                {
                                        writememw(ss,((SP-2)&0xFFFF),FS); if (abrt) break;
                                        SP-=2;
                                }
                                cycles-=2;
                                break;
                                case 0xA1: /*POP FS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        tempw=readmemw(ss,ESP);         if (abrt) break;
                                        loadseg(tempw,&_fs);            if (abrt) break;
                                        ESP+=2;
                                }
                                else
                                {
                                        tempw=readmemw(ss,SP);          if (abrt) break;
                                        loadseg(tempw,&_fs);            if (abrt) break;
                                        SP+=2;
                                }
                                cycles-=(is486)?3:7;
                                break;
                                case 0xA3: /*BT r16*/
                                fetchea2();
                                eaaddr+=((regs[reg].w/16)*2); eal_r = 0;
                                tempw=geteaw(); if (abrt) break;
                                if (tempw&(1<<(regs[reg].w&15))) flags|=C_FLAG;
                                else                             flags&=~C_FLAG;
                                cycles-=3;
                                break;
                                case 0xA4: /*SHLD imm*/
                                fetchea2();
                                temp=readmemb(cs,pc)&31; pc++;
                                if (temp)
                                {
                                        tempw=geteaw(); if (abrt) break;
                                        tempc=((tempw<<(temp-1))&0x8000)?1:0;
                                        templ=(tempw<<16)|regs[reg].w;
                                        if (temp<=16) tempw=templ>>(16-temp);
                                        else          tempw=(templ<<temp)>>16;
                                        seteaw(tempw); if (abrt) break;
                                        setznp16(tempw);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;
                                case 0xA5: /*SHLD CL*/
                                fetchea2();
                                temp=CL&31;
                                if (temp)
                                {
                                        tempw=geteaw(); if (abrt) break;
                                        tempc=((tempw<<(temp-1))&0x8000)?1:0;
                                        templ=(tempw<<16)|regs[reg].w;
                                        if (temp<=16) tempw=templ>>(16-temp);
                                        else          tempw=(templ<<temp)>>16;
                                        seteaw(tempw); if (abrt) break;
                                        setznp16(tempw);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;
                                case 0xA8: /*PUSH GS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememw(ss,ESP-2,GS); if (abrt) break;
                                        ESP-=2;
                                }
                                else
                                {
                                        writememw(ss,((SP-2)&0xFFFF),GS); if (abrt) break;
                                        SP-=2;
                                }
                                cycles-=2;
                                break;
                                case 0xA9: /*POP GS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        tempw=readmemw(ss,ESP);         if (abrt) break;
                                        loadseg(tempw,&_gs);            if (abrt) break;
                                        ESP+=2;
                                }
                                else
                                {
                                        tempw=readmemw(ss,SP);          if (abrt) break;
                                        loadseg(tempw,&_gs);            if (abrt) break;
                                        SP+=2;
                                }
                                cycles-=(is486)?3:7;
                                break;
                                case 0xAB: /*BTS r16*/
                                fetchea2();
                                eaaddr+=((regs[reg].w/16)*2); eal_r = eal_w = 0;
                                tempw=geteaw(); if (abrt) break;
                                tempc=(tempw&(1<<(regs[reg].w&15)))?1:0;
                                tempw|=(1<<(regs[reg].w&15));
                                seteaw(tempw); if (abrt) break;
                                if (tempc) flags|=C_FLAG;
                                else       flags&=~C_FLAG;
                                cycles-=6;
                                break;
                                case 0xAC: /*SHRD imm*/
                                fetchea2();
                                temp=readmemb(cs,pc)&31; pc++;
                                if (temp)
                                {
                                        tempw=geteaw(); if (abrt) break;
                                        tempc=(tempw>>(temp-1))&1;
                                        templ=tempw|(regs[reg].w<<16);
                                        tempw=templ>>temp;
                                        seteaw(tempw); if (abrt) break;
                                        setznp16(tempw);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;
                                case 0xAD: /*SHRD CL*/
                                fetchea2();
                                temp=CL&31;
                                if (temp)
                                {
                                        tempw=geteaw(); if (abrt) break;
                                        tempc=(tempw>>(temp-1))&1;
                                        templ=tempw|(regs[reg].w<<16);
                                        tempw=templ>>temp;
                                        seteaw(tempw); if (abrt) break;
                                        setznp16(tempw);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;

                                case 0xAF: /*IMUL reg16,rm16*/
                                fetchea2();
                                templ=(int32_t)(int16_t)regs[reg].w*(int32_t)(int16_t)geteaw();
                                if (abrt) break;
                                regs[reg].w=templ&0xFFFF;
                                if ((templ>>16) && (templ>>16)!=0xFFFF) flags|=C_FLAG|V_FLAG;
                                else                                    flags&=~(C_FLAG|V_FLAG);
                                cycles-=18;
                                break;

                                case 0xB0: /*CMPXCHG rm8, reg8*/
                                if (!is486) goto inv16;
                                fetchea2();
                                temp = geteab();
                                setsub8(AL, temp);
                                if (AL == temp) seteab(getr8(reg));
                                else             AL = temp;
                                cycles -= (mod == 3) ? 6 : 10;
                                break;
                                case 0xB1: /*CMPXCHG rm16, reg16*/
                                if (!is486) goto inv16;
                                fetchea2();
                                tempw = geteaw();
                                setsub16(AX, tempw);
                                if (AX == tempw) seteaw(regs[reg].w);
                                else             AX = tempw;
                                cycles -= (mod == 3) ? 6 : 10;
                                break;
                                
                                case 0xB3: /*BTR r16*/
                                fetchea2();
                                eaaddr+=((regs[reg].w/16)*2); eal_r = eal_w = 0;
                                tempw=geteaw(); if (abrt) break;
                                tempc=tempw&(1<<(regs[reg].w&15));
                                tempw&=~(1<<(regs[reg].w&15));
                                seteaw(tempw); if (abrt) break;
                                if (tempc) flags|=C_FLAG;
                                else       flags&=~C_FLAG;
                                cycles-=6;
                                break;

                                case 0xB2: /*LSS*/
                                fetchea2();
                                tempw2=readmemw(easeg,eaaddr);
                                tempw=readmemw(easeg,(eaaddr+2)); if (abrt) break;
                                loadseg(tempw,&_ss); if (abrt) break;
                                regs[reg].w=tempw2;
                                oldss=ss;
                                cycles-=7;
                                break;
                                case 0xB4: /*LFS*/
                                fetchea2();
                                tempw2=readmemw(easeg,eaaddr);
                                tempw=readmemw(easeg,(eaaddr+2)); if (abrt) break;
                                loadseg(tempw,&_fs); if (abrt) break;
                                regs[reg].w=tempw2;
                                cycles-=7;
                                break;
                                case 0xB5: /*LGS*/
                                fetchea2();
                                tempw2=readmemw(easeg,eaaddr);
                                tempw=readmemw(easeg,(eaaddr+2)); if (abrt) break;
                                loadseg(tempw,&_gs); if (abrt) break;
                                regs[reg].w=tempw2;
                                cycles-=7;
                                break;

                                case 0xB6: /*MOVZX b*/
                                fetchea2();
                                tempw=geteab(); if (abrt) break;
                                regs[reg].w=tempw;
                                cycles-=3;
                                break;
                                case 0xB7: /*MOVZX w*/
                                fetchea2(); /*Slightly pointless?*/
                                tempw=geteaw(); if (abrt) break;
                                regs[reg].w=tempw;
                                cycles-=3;
                                break;
                                case 0xBE: /*MOVSX b*/
                                fetchea2();
                                tempw=geteab(); if (abrt) break;
                                if (tempw&0x80) tempw|=0xFF00;
                                regs[reg].w=tempw;
                                cycles-=3;
                                break;

                                case 0xBA: /*MORE?!?!?!*/
                                fetchea2();
                                switch (rmdat&0x38)
                                {
                                        case 0x20: /*BT w,imm*/
                                        tempw=geteaw();
                                        temp=readmemb(cs,pc); pc++; if (abrt) break;
                                        if (tempw&(1<<temp)) flags|=C_FLAG;
                                        else                 flags&=~C_FLAG;
                                        cycles-=6;
                                        break;
                                        case 0x28: /*BTS w,imm*/
                                        tempw=geteaw();
                                        temp=readmemb(cs,pc); pc++; if (abrt) break;
                                        tempc=(tempw&(1<<temp))?1:0;
                                        tempw|=(1<<temp);
                                        seteaw(tempw);  if (abrt) break;
                                        if (tempc) flags|=C_FLAG;
                                        else       flags&=~C_FLAG;
                                        cycles-=6;
                                        break;
                                        case 0x30: /*BTR w,imm*/
                                        tempw=geteaw();
                                        temp=readmemb(cs,pc); pc++; if (abrt) break;
                                        tempc=(tempw&(1<<temp))?1:0;
                                        tempw&=~(1<<temp);
                                        seteaw(tempw); if (abrt) break;
                                        if (tempc) flags|=C_FLAG;
                                        else       flags&=~C_FLAG;
                                        cycles-=6;
                                        break;
                                        case 0x38: /*BTC w,imm*/
                                        tempw=geteaw();
                                        temp=readmemb(cs,pc); pc++; if (abrt) break;
                                        tempc=(tempw&(1<<temp))?1:0;
                                        tempw^=(1<<temp);
                                        seteaw(tempw); if (abrt) break;
                                        if (tempc) flags|=C_FLAG;
                                        else       flags&=~C_FLAG;
                                        cycles-=6;
                                        break;

                                        default:
                                        pclog("Bad 0F BA opcode %02X\n",rmdat&0x38);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                break;

                                case 0xBC: /*BSF w*/
                                fetchea2();
                                tempw = geteaw(); if (abrt) break;
                                if (!tempw)
                                {
//                                        regs[reg].w=0;
                                        flags |= Z_FLAG;
                                }
                                else
                                {
                                        for (tempi = 0; tempi < 16; tempi++)
                                        {
                                                cycles -= (is486) ? 1 : 3;
                                                if (tempw & (1 << tempi))
                                                {
                                                        flags &= ~Z_FLAG;
                                                        regs[reg].w = tempi;
                                                        break;
                                                }
                                        }
                                }
                                cycles -= (is486) ? 6 : 10;
                                break;
                                case 0xBD: /*BSR w*/
                                fetchea2();
                                tempw = geteaw(); if (abrt) break;
                                if (!tempw)
                                {
//                                        regs[reg].w=0;
                                        flags |= Z_FLAG;
                                }
                                else
                                {
                                        for (tempi = 15; tempi >= 0; tempi--)
                                        {
                                                cycles -= 3;
                                                if (tempw & (1 << tempi))
                                                {
                                                        flags &= ~Z_FLAG;
                                                        regs[reg].w = tempi;
                                                        break;
                                                }
                                        }
                                }
                                cycles -= (is486) ? 6 : 10;
                                break;

                                case 0xA2: /*CPUID*/
                                if (CPUID)
                                {
                                        cpu_CPUID();
                                        cycles-=9;
                                        break;
                                }
                                goto inv16;

                                case 0xC0: /*XADD b*/
                                pclog("XADDb 16\n");
                                fetchea2();
                                temp=geteab(); if (abrt) break;
                                seteab(temp+getr8(reg)); if (abrt) break;
                                setadd8(temp,getr8(reg));
                                setr8(reg,temp);
                                break;
                                case 0xC1: /*XADD w*/
                                pclog("XADDw 16\n");
                                fetchea2();
                                tempw=geteaw(); if (abrt) break;
                                seteaw(tempw+regs[reg].w); if (abrt) break;
                                setadd16(tempw,regs[reg].w);
                                regs[reg].w=tempw;
                                break;

                                case 0xA6: /*XBTS/CMPXCHG486*/
                                pclog("CMPXCHG486\n");
//                                output=1;
                                case 0xFF: /*Invalid - Windows 3.1 syscall trap?*/
                                inv16:
                                pc-=2;
                                if (msw&1)
                                {
                                        pmodeint(6,0);
                                }
                                else
                                {
                                        if (ssegs) ss=oldss;
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
                                        addr=6<<2;
                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                }
                                cycles-=70;
                                break;

                                default:
                                pclog("Bad 16-bit 0F opcode %02X 386 %i\n",temp,ins);
                                pc=oldpc;
                                x86illegal();
                                break;
                        }
                        break;

                        case 0x10F: case 0x30F:
                        temp=fetchdat&0xFF/*readmemb(cs+pc)*/; pc++;
                        opcode2=temp;
                        switch (temp)
                        {
                                case 0:
                                if (!(cr0&1) || (eflags&VM_FLAG)) goto inv32;
                                fetchea2();
//                                pclog("32-bit op %02X\n",rmdat&0x38);
                                switch (rmdat&0x38)
                                {
                                        case 0x00: /*SLDT*/
                                        NOTRM
                                        seteaw(ldt.seg);
                                        cycles-=4;
                                        break;
                                        case 0x08: /*STR*/
                                        NOTRM
                                        seteaw(tr.seg);
                                        cycles-=4;
                                        break;
                                        case 0x10: /*LLDT*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LLDT32!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        NOTRM
                                        ldt.seg=geteaw(); if (abrt) break;
                                        templ=(ldt.seg&~7)+gdt.base;
                                        templ3=readmemw(0,templ)+((readmemb(0,templ+6)&0xF)<<16);
                                        templ2=(readmemw(0,templ+2))|(readmemb(0,templ+4)<<16)|(readmemb(0,templ+7)<<24);
                                        if (abrt) break;
                                        ldt.limit=templ3;
                                        ldt.base=templ2;
                                        ldt.access=readmemb(0,templ+6);
                                        if (readmemb(0,templ+6)&0x80)
                                        {
                                                ldt.limit<<=12;
                                                ldt.limit|=0xFFF;
                                        }
//                                        /*if (output==3) */pclog("LLDT32 %04X %08X %04X\n",ldt.seg,ldt.base,ldt.limit,readmeml(0,templ),readmeml(0,templ+4));
                                        cycles-=20;
                                        break;
                                        case 0x18: /*LTR*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LTR32!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        NOTRM
                                        tempw=geteaw(); if (abrt) break;
                                        tr.seg=tempw;
                                        templ=(tempw&~7)+gdt.base;
                                        templ3=readmemw(0,templ)+((readmemb(0,templ+6)&0xF)<<16);
                                        templ2=(readmemw(0,templ+2))|(readmemb(0,templ+4)<<16)|(readmemb(0,templ+7)<<24);
                                        temp=readmemb(0,templ+5);
                                        if (abrt) break;
                                        tr.seg=tempw;
                                        tr.limit=templ3;
                                        tr.access=readmemb(0,templ+6);
                                        if (readmemb(0,templ+6)&0x80)
                                        {
                                                tr.limit<<=12;
                                                tr.limit|=0xFFF;
                                        }
                                        tr.base=templ2;
//                                        pclog("TR base = %08X\n",templ2);
                                        tr.access=temp;
                                        cycles-=20;
                                        break;

                                        case 0x20: /*VERR*/
                                        NOTRM
                                        tempw=geteaw(); if (abrt) break;
                                        flags&=~Z_FLAG;
                                        if (!(tempw&0xFFFC)) break; /*Null selector*/
                                        cpl_override=1;
                                        tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                        cpl_override=0;
                                        if (abrt) break;
                                        if (!(tempw2&0x1000)) tempi=0;
                                        if ((tempw2&0xC00)!=0xC00) /*Exclude conforming code segments*/
                                        {
                                                tempw3=(tempw2>>13)&3; /*Check permissions*/
                                                if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                        }
                                        if ((tempw2&0x0800) && !(tempw2&0x0200)) tempi=0; /*Non-readable code*/
                                        if (tempi) flags|=Z_FLAG;
                                        cycles-=20;
                                        break;
                                        case 0x28: /*VERW*/
                                        NOTRM
                                        tempw=geteaw(); if (abrt) break;
                                        flags&=~Z_FLAG;
                                        if (!(tempw&0xFFFC)) break; /*Null selector*/
                                        cpl_override=1;
                                        tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                        cpl_override=0;
                                        if (abrt) break;
                                        if (!(tempw2&0x1000)) tempi=0;
                                        tempw3=(tempw2>>13)&3; /*Check permissions*/
                                        if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                        if (tempw2&0x0800) tempi=0; /*Code*/
                                        else if (!(tempw2&0x0200)) tempi=0; /*Read-only data*/
                                        if (tempi) flags|=Z_FLAG;
                                        cycles-=20;
                                        break;

                                        default:
                                        pclog("Bad 0F 00 opcode %02X\n",rmdat&0x38);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                break;
                                case 1:
                                fetchea2();
                                switch (rmdat&0x38)
                                {
                                        case 0x00: /*SGDT*/
                                        seteaw(gdt.limit);
                                        writememl(easeg,eaaddr+2,gdt.base);
                                        cycles-=7;
                                        break;
                                        case 0x08: /*SIDT*/
                                        seteaw(idt.limit);
                                        writememl(easeg,eaaddr+2,idt.base);
                                        cycles-=7;
                                        break;
                                        case 0x10: /*LGDT*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LGDT32!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw();
                                        templ=readmeml(easeg,eaaddr+2);
                                        if (abrt) break;
                                        gdt.limit=tempw;
                                        gdt.base=templ;
//                                        pclog("LGDT32 %08X %04X\n",gdt.base,gdt.limit);
                                        cycles-=11;
                                        break;
                                        case 0x18: /*LIDT*/
                                        if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                        {
                                                pclog("Invalid LIDT32!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw();
                                        templ=readmeml(easeg,eaaddr+2);
                                        idt.limit=tempw;
                                        idt.base=templ;
//                                        pclog("LIDT32 %08X %08X\n",idt.base,readmeml(0,easeg+eaaddr+2));
                                        cycles-=11;
                                        break;
                                        case 0x20: /*SMSW*/
                                        if (mod<3) seteaw(cr0);
                                        else       seteal(cr0); /*Apparently this is the case!*/
//                                        pclog("SMSW32 %04X(%06X):%04X\n",CS,cs,pc);
                                        cycles-=2;
                                        break;
                                        
                                        case 0x30: /*LMSW*/
                                        if ((CPL || eflags&VM_FLAG) && (msw&1))
                                        {
                                                pclog("LMSW - ring not zero!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw(); if (abrt) break;
                                        if (msw&1) tempw|=1;
                                        msw=tempw;
                                        break;

                                        case 0x38: /*INVLPG*/
                                        if (is486)
                                        {
                                                if ((CPL || eflags&VM_FLAG) && (cr0&1))
                                                {
                                                        pclog("Invalid INVLPG!\n");
                                                        x86gpf(NULL,0);
                                                        break;
                                                }
                                                if (output) pclog("INVLPG %08X + %08X\n", eaaddr, ds);
                                                mmu_invalidate(ds + eaaddr);
                                                cycles-=12;
                                                break;
                                        }


                                        default:
                                        pclog("Bad 0F 01 opcode %02X\n",rmdat&0x38);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                break;

                                case 2: /*LAR*/
                                NOTRM
                                fetchea2();
                                tempw=geteaw(); if (abrt) break;
//                                pclog("LAR seg %04X\n",tempw);
                                if (!(tempw&0xFFFC)) { flags&=~Z_FLAG; break; } /*Null selector*/
                                cpl_override=1;
                                tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                cpl_override=0;
                                if (abrt) break;
                                flags&=~Z_FLAG;
//                                pclog("tempw2 %04X  %i  %04X %04X\n",tempw2,tempi,ldt.limit,gdt.limit);
                                if ((tempw2&0x1F00)==0x000) tempi=0;
                                if ((tempw2&0x1F00)==0x800) tempi=0;
                                if ((tempw2&0x1F00)==0xA00) tempi=0;
                                if ((tempw2&0x1F00)==0xD00) tempi=0;
                                if ((tempw2&0x1C00)<0x1C00) /*Exclude conforming code segments*/
                                {
                                        tempw3=(tempw2>>13)&3;
                                        if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                }
                                if (tempi)
                                {
                                        flags|=Z_FLAG;
                                        cpl_override=1;
                                        regs[reg].l=readmeml(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4)&0xFFFF00;
                                        cpl_override=0;
                                }
                                cycles-=11;
                                break;

                                case 3: /*LSL*/
                                NOTRM
                                fetchea2();
                                tempw=geteaw(); if (abrt) break;
                                if (output) pclog("LSL %04X\n",tempw);
                                if (!(tempw&0xFFFC)) { flags&=~Z_FLAG; break; } /*Null selector*/
                                cpl_override=1;
                                tempi=(tempw&~7)<((tempw&4)?ldt.limit:gdt.limit);
                                if (output) pclog("In range? %i\n",tempi);
                                if (tempi)
                                {
                                        tempw2=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+4);
                                        if (output) pclog("segdat[2] = %04X\n",tempw2);
                                }
                                cpl_override=0;
                                if (abrt) break;
                                flags&=~Z_FLAG;
                                if ((tempw2&0x1400)==0x400) tempi=0; /*Interrupt or trap or call gate*/
                                if ((tempw2&0x1F00)==0x000) tempi=0; /*Invalid*/
                                if ((tempw2&0x1F00)==0xA00) tempi=0; /*Invalid*/
                                if ((tempw2&0x1C00)!=0x1C00) /*Exclude conforming code segments*/
                                {
                                        tempw3=(tempw2>>13)&3;
                                        if (tempw3<CPL || tempw3<(tempw&3)) tempi=0;
                                }
                                if (output) pclog("Final tempi %i\n",tempi);
                                if (tempi)
                                {
                                        flags|=Z_FLAG;
                                        cpl_override=1;
                                        regs[reg].l=readmemw(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7));
                                        regs[reg].l|=(readmemb(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+6)&0xF)<<16;
                                        if (readmemb(0,((tempw&4)?ldt.base:gdt.base)+(tempw&~7)+6)&0x80)
                                        {
                                                regs[reg].l<<=12;
                                                regs[reg].l|=0xFFF;
                                        }
                                        cpl_override=0;
                                }
                                cycles-=10;
                                break;

                                case 6: /*CLTS*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't CLTS\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                cr0&=~8;
                                cycles-=5;
                                break;

                                case 8: /*INVD*/
                                if (!is486) goto inv32;
                                cycles-=1000;
                                break;
                                case 9: /*WBINVD*/
                                if (!is486) goto inv32;
                                cycles-=10000;
                                break;

                                case 0x20: /*MOV reg32,CRx*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load from CRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                switch (reg)
                                {
                                        case 0:
                                        regs[rm].l=cr0;//&~2;
                                        if (is486) regs[rm].l|=0x10; /*ET hardwired on 486*/
                                        break;
                                        case 2:
                                        regs[rm].l=cr2;
                                        break;
                                        case 3:
                                        regs[rm].l=cr3;
                                        break;
                                        default:
                                        pclog("Bad read of CR%i %i\n",rmdat&7,reg);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                cycles-=6;
                                break;
                                case 0x21: /*MOV reg32,DRx*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load from DRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                regs[rm].l=0;
                                cycles-=6;
                                break;
                                case 0x22: /*MOV CRx,reg32*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load CRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                switch (reg)
                                {
                                        case 0:
                                        //if ((cr0^regs[rm].l)>>31) flushmmucache();
                                        if ((regs[rm].l^cr0) & 0x80000001) flushmmucache();
                                        cr0=regs[rm].l;//&~2;
                                        if (cpu_16bitbus) cr0 |= 0x10;
//                                        if (cs<0xF0000 && cr0&1) output=3;
                                        if (cr0&0x80000000)
                                        {
                                        }
                                        else mmu_perm=4;
                                        break;
                                        case 2:
                                        cr2=regs[rm].l;
                                        break;
                                        case 3:
                                        cr3=regs[rm].l&~0xFFF;
//                                        pclog("Loading CR3 with %08X at %08X:%08X\n",cr3,cs,pc);
                                        flushmmucache();
                                        break;
                                        default:
                                        pclog("Bad load CR%i\n",reg);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                cycles-=10;
                                break;
                                case 0x23: /*MOV DRx,reg32*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load DRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                cycles-=6;
                                break;
                                case 0x24: /*MOV reg32,TRx*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load from TRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                regs[rm].l=0;
                                cycles-=6;
                                break;
                                case 0x26: /*MOV TRx,reg32*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
                                        pclog("Can't load TRx\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                fetchea2();
                                cycles-=6;
                                break;

                                case 0x80: /*JO*/
                                templ=getlong(); if (abrt) break;
                                if (flags&V_FLAG) { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x81: /*JNO*/
                                templ=getlong(); if (abrt) break;
                                if (!(flags&V_FLAG)) { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x82: /*JB*/
                                templ=getlong(); if (abrt) break;
                                if (flags&C_FLAG) { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x83: /*JNB*/
                                templ=getlong(); if (abrt) break;
                                if (!(flags&C_FLAG)) { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x84: /*JE*/
                                templ=getlong(); if (abrt) break;
                                if (flags&Z_FLAG) pc+=templ;
                                cycles-=4;
                                break;
                                case 0x85: /*JNE*/
                                templ=getlong(); if (abrt) break;
                                if (!(flags&Z_FLAG)) pc+=templ;
                                cycles-=4;
                                break;
                                case 0x86: /*JBE*/
                                templ=getlong(); if (abrt) break;
                                if (flags&(C_FLAG|Z_FLAG)) { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x87: /*JNBE*/
                                templ=getlong(); if (abrt) break;
                                if (!(flags&(C_FLAG|Z_FLAG))) { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x88: /*JS*/
                                templ=getlong(); if (abrt) break;
                                if (flags&N_FLAG) pc+=templ;
                                cycles-=4;
                                break;
                                case 0x89: /*JNS*/
                                templ=getlong(); if (abrt) break;
                                if (!(flags&N_FLAG)) pc+=templ;
                                cycles-=4;
                                break;
                                case 0x8A: /*JP*/
                                templ=getlong(); if (abrt) break;
                                if (flags&P_FLAG) pc+=templ;
                                cycles-=4;
                                break;
                                case 0x8B: /*JNP*/
                                templ=getlong(); if (abrt) break;
                                if (!(flags&P_FLAG)) pc+=templ;
                                cycles-=4;
                                break;
                                case 0x8C: /*JL*/
                                templ=getlong(); if (abrt) break;
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if (temp!=temp2)  { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x8D: /*JNL*/
                                templ=getlong(); if (abrt) break;
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if (temp==temp2)  { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x8E: /*JLE*/
                                templ=getlong(); if (abrt) break;
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if ((flags&Z_FLAG) || (temp!=temp2))  { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;
                                case 0x8F: /*JNLE*/
                                templ=getlong(); if (abrt) break;
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                if (!((flags&Z_FLAG) || (temp!=temp2)))  { pc+=templ; cycles-=((is486)?2:4); }
                                cycles-=((is486)?1:3);
                                break;

                                case 0x90: /*SETO*/
                                fetchea2();
                                seteab((flags&V_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x91: /*SETNO*/
                                fetchea2();
                                seteab((flags&V_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x92: /*SETC*/
                                fetchea2();
                                seteab((flags&C_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x93: /*SETAE*/
                                fetchea2();
                                seteab((flags&C_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x94: /*SETZ*/
                                fetchea2();
                                seteab((flags&Z_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x95: /*SETNZ*/
                                fetchea2();
                                seteab((flags&Z_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x96: /*SETBE*/
                                fetchea2();
                                seteab((flags&(C_FLAG|Z_FLAG))?1:0);
                                cycles-=4;
                                break;
                                case 0x97: /*SETNBE*/
                                fetchea2();
                                seteab((flags&(C_FLAG|Z_FLAG))?0:1);
                                cycles-=4;
                                break;
                                case 0x98: /*SETS*/
                                fetchea2();
                                seteab((flags&N_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x99: /*SETNS*/
                                fetchea2();
                                seteab((flags&N_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x9A: /*SETP*/
                                fetchea2();
                                seteab((flags&P_FLAG)?1:0);
                                cycles-=4;
                                break;
                                case 0x9B: /*SETNP*/
                                fetchea2();
                                seteab((flags&P_FLAG)?0:1);
                                cycles-=4;
                                break;
                                case 0x9C: /*SETL*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab(temp^temp2);
                                cycles-=4;
                                break;
                                case 0x9D: /*SETGE*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab((temp^temp2)?0:1);
                                cycles-=4;
                                break;
                                case 0x9E: /*SETLE*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab(((temp^temp2) || (flags&Z_FLAG))?1:0);
                                cycles-=4;
                                break;
                                case 0x9F: /*SETNLE*/
                                fetchea2();
                                temp=(flags&N_FLAG)?1:0;
                                temp2=(flags&V_FLAG)?1:0;
                                seteab(((temp^temp2) || (flags&Z_FLAG))?0:1);
                                cycles-=4;
                                break;

                                case 0xA0: /*PUSH FS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememl(ss,ESP-4,FS); if (abrt) break;
                                        ESP-=4;
                                }
                                else
                                {
                                        writememl(ss,((SP-4)&0xFFFF),FS); if (abrt) break;
                                        SP-=4;
                                }
                                cycles-=2;
                                break;
                                case 0xA1: /*POP FS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        tempw=readmemw(ss,ESP);         if (abrt) break;
                                        loadseg(tempw,&_fs);            if (abrt) break;
                                        ESP+=4;
                                }
                                else
                                {
                                        tempw=readmemw(ss,SP);          if (abrt) break;
                                        loadseg(tempw,&_fs);            if (abrt) break;
                                        SP+=4;
                                }
                                cycles-=(is486)?3:7;
                                break;
                                case 0xA8: /*PUSH GS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememl(ss,ESP-4,GS); if (abrt) break;
                                        ESP-=4;
                                }
                                else
                                {
                                        writememl(ss,((SP-4)&0xFFFF),GS); if (abrt) break;
                                        SP-=4;
                                }
                                cycles-=2;
                                break;
                                case 0xA9: /*POP GS*/
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        tempw=readmemw(ss,ESP);         if (abrt) break;
                                        loadseg(tempw,&_gs);            if (abrt) break;
                                        ESP+=4;
                                }
                                else
                                {
                                        tempw=readmemw(ss,SP);          if (abrt) break;
                                        loadseg(tempw,&_gs);            if (abrt) break;
                                        SP+=4;
                                }
                                cycles-=(is486)?3:7;
                                break;

                                case 0xA3: /*BT r32*/
                                fetchea2();
                                eaaddr+=((regs[reg].l/32)*4); eal_r = 0;
                                templ=geteal(); if (abrt) break;
                                if (templ&(1<<(regs[reg].l&31))) flags|=C_FLAG;
                                else                             flags&=~C_FLAG;
                                cycles-=3;
                                break;
                                case 0xA4: /*SHLD imm*/
                                fetchea2();
                                temp=readmemb(cs,pc)&31; pc++;
                                if (temp)
                                {
                                        templ=geteal(); if (abrt) break;
                                        tempc=((templ<<(temp-1))&0x80000000)?1:0;
                                        templ=(templ<<temp)|(regs[reg].l>>(32-temp));
                                        seteal(templ); if (abrt) break;
                                        setznp32(templ);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;
                                case 0xA5: /*SHLD CL*/
                                fetchea2();
                                temp=CL&31;
                                if (temp)
                                {
                                        templ=geteal(); if (abrt) break;
                                        tempc=((templ<<(temp-1))&0x80000000)?1:0;
                                        templ=(templ<<temp)|(regs[reg].l>>(32-temp));
                                        seteal(templ); if (abrt) break;
                                        setznp32(templ);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;
                                case 0xAB: /*BTS r32*/
                                fetchea2();
                                eaaddr+=((regs[reg].l/32)*4); eal_r = eal_w = 0;
                                templ=geteal(); if (abrt) break;
                                tempc=(templ&(1<<(regs[reg].l&31)))?1:0;
                                templ|=(1<<(regs[reg].l&31));
                                seteal(templ); if (abrt) break;
                                if (tempc) flags|=C_FLAG;
                                else       flags&=~C_FLAG;
                                cycles-=6;
                                break;
                                case 0xAC: /*SHRD imm*/
                                fetchea2();
                                temp=readmemb(cs,pc)&31; pc++;
                                if (temp)
                                {
                                        templ=geteal(); if (abrt) break;
                                        tempc=(templ>>(temp-1))&1;
                                        templ=(templ>>temp)|(regs[reg].l<<(32-temp));
                                        seteal(templ); if (abrt) break;
                                        setznp32(templ);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;
                                case 0xAD: /*SHRD CL*/
                                fetchea2();
                                temp=CL&31;
                                if (temp)
                                {
                                        templ=geteal(); if (abrt) break;
                                        tempc=(templ>>(temp-1))&1;
                                        templ=(templ>>temp)|(regs[reg].l<<(32-temp));
                                        seteal(templ); if (abrt) break;
                                        setznp32(templ);
                                        if (tempc) flags|=C_FLAG;
                                }
                                cycles-=3;
                                break;

                                case 0xAF: /*IMUL reg32,rm32*/
                                fetchea2();
                                temp64=(int64_t)(int32_t)regs[reg].l*(int64_t)(int32_t)geteal();
                                if (abrt) break;
                                regs[reg].l=temp64&0xFFFFFFFF;
                                if ((temp64>>32) && (temp64>>32)!=0xFFFFFFFF) flags|=C_FLAG|V_FLAG;
                                else                                          flags&=~(C_FLAG|V_FLAG);
                                cycles-=30;
                                break;

                                case 0xB0: /*CMPXCHG rm8, reg8*/
                                if (!is486) goto inv32;
                                fetchea2();
                                temp = geteab();
                                setsub8(AL, temp);
                                if (AL == temp) seteab(getr8(reg));
                                else            AL = temp;
                                cycles -= (mod == 3) ? 6 : 10;
                                break;
                                case 0xB1: /*CMPXCHG rm32, reg32*/
                                if (!is486) goto inv32;
                                fetchea2();
                                templ = geteal();
                                setsub32(EAX, templ);
                                if (EAX == templ) seteal(regs[reg].l);
                                else              EAX = templ;
                                cycles -= (mod == 3) ? 6 : 10;
                                break;

                                case 0xB3: /*BTR r32*/
                                fetchea2();
                                eaaddr+=((regs[reg].l/32)*4); eal_r = eal_w = 0;
                                templ=geteal(); if (abrt) break;
                                tempc=(templ&(1<<(regs[reg].l&31)))?1:0;
                                templ&=~(1<<(regs[reg].l&31));
                                seteal(templ); if (abrt) break;
                                if (tempc) flags|=C_FLAG;
                                else       flags&=~C_FLAG;
                                cycles-=6;
                                break;

                                case 0xB2: /*LSS*/
                                fetchea2();
                                templ=readmeml(easeg,eaaddr);     
                                tempw=readmemw(easeg,(eaaddr+4)); if (abrt) break;
                                loadseg(tempw,&_ss);              if (abrt) break;
                                regs[reg].l=templ;
                                oldss=ss;
                                cycles-=7;
                                break;
                                case 0xB4: /*LFS*/
                                fetchea2();
                                templ=readmeml(easeg,eaaddr);
                                tempw=readmemw(easeg,(eaaddr+4)); if (abrt) break;
                                loadseg(tempw,&_fs);              if (abrt) break;
                                regs[reg].l=templ;
                                cycles-=7;
                                break;
                                case 0xB5: /*LGS*/
                                fetchea2();
                                templ=readmeml(easeg,eaaddr);
                                tempw=readmemw(easeg,(eaaddr+4)); if (abrt) break;
                                loadseg(tempw,&_gs);              if (abrt) break;
                                regs[reg].l=templ;
                                cycles-=7;
                                break;

                                case 0xB6: /*MOVZX b*/
                                fetchea2();
                                templ=geteab(); if (abrt) break;
                                regs[reg].l=templ;
                                cycles-=3;
                                break;
                                case 0xB7: /*MOVZX w*/
                                fetchea2();
                                templ=geteaw(); if (abrt) break;
                                regs[reg].l=templ;
                                cycles-=3;
//                                if (pc==0x30B) output=1;
                                break;

                                case 0xBA: /*MORE?!?!?!*/
                                fetchea2();
                                switch (rmdat&0x38)
                                {
                                        case 0x20: /*BT l,imm*/
                                        templ=geteal();
                                        temp=readmemb(cs,pc); pc++;
                                        if (abrt) break;
                                        if (templ&(1<<temp)) flags|=C_FLAG;
                                        else                 flags&=~C_FLAG;
                                        cycles-=6;
                                        break;
                                        case 0x28: /*BTS l,imm*/
                                        templ=geteal();
                                        temp=readmemb(cs,pc); pc++;
                                        if (abrt) break;
                                        tempc=(templ&(1<<temp))?1:0;
                                        templ|=(1<<temp);
                                        seteal(templ); if (abrt) break;
                                        if (tempc) flags|=C_FLAG;
                                        else       flags&=~C_FLAG;
                                        cycles-=6;
                                        break;
                                        case 0x30: /*BTR l,imm*/
                                        templ=geteal();
                                        temp=readmemb(cs,pc); pc++;
                                        if (abrt) break;
                                        tempc=(templ&(1<<temp))?1:0;
                                        templ&=~(1<<temp);
                                        seteal(templ); if (abrt) break;
                                        if (tempc) flags|=C_FLAG;
                                        else       flags&=~C_FLAG;
                                        cycles-=6;
                                        break;
                                        case 0x38: /*BTC l,imm*/
                                        templ=geteal();
                                        temp=readmemb(cs,pc); pc++;
                                        if (abrt) break;
                                        tempc=(templ&(1<<temp))?1:0;
                                        templ^=(1<<temp);
                                        seteal(templ); if (abrt) break;
                                        if (tempc) flags|=C_FLAG;
                                        else       flags&=~C_FLAG;
                                        cycles-=6;
                                        break;

                                        default:
                                        pclog("Bad 32-bit 0F BA opcode %02X\n",rmdat&0x38);
                                        pc=oldpc;
                                        x86illegal();
                                        break;
                                }
                                break;

                                case 0xBB: /*BTC r32*/
                                fetchea2();
                                eaaddr+=((regs[reg].l/32)*4); eal_r = eal_w = 0;
                                templ=geteal(); if (abrt) break;
                                tempc=(templ&(1<<(regs[reg].l&31)))?1:0;
                                templ^=(1<<(regs[reg].l&31));
                                seteal(templ);  if (abrt) break;
                                if (tempc) flags|=C_FLAG;
                                else       flags&=~C_FLAG;
                                cycles-=6;
                                break;

                                case 0xBC: /*BSF l*/
                                fetchea2();
                                templ=geteal(); if (abrt) break;
                                if (!templ)
                                {
//                                        regs[reg].l=0;
                                        flags|=Z_FLAG;
                                }
                                else
                                {
                                        for (tempi=0;tempi<32;tempi++)
                                        {
                                                cycles -= (is486) ? 1 : 3;
                                                if (templ&(1<<tempi))
                                                {
                                                        flags&=~Z_FLAG;
                                                        regs[reg].l=tempi;
                                                        break;
                                                }
                                        }
                                }
                                cycles-=(is486)?6:10;
                                break;
                                case 0xBD: /*BSR l*/
                                fetchea2();
                                templ=geteal(); if (abrt) break;
                                if (!templ)
                                {
//                                        regs[reg].l=0;
                                        flags|=Z_FLAG;
                                }
                                else
                                {
                                        for (tempi=31;tempi>=0;tempi--)
                                        {
                                                cycles -= 3;
                                                if (templ&(1<<tempi))
                                                {
                                                        flags&=~Z_FLAG;
                                                        regs[reg].l=tempi;
                                                        break;
                                                }
                                        }
                                }
                                cycles-=(is486)?6:10;
                                break;

                                case 0xBE: /*MOVSX b*/
                                fetchea2();
                                templ=geteab(); if (abrt) break;
                                if (templ&0x80) templ|=0xFFFFFF00;
                                regs[reg].l=templ;
                                cycles-=3;
                                break;
                                case 0xBF: /*MOVSX w*/
                                fetchea2();
                                templ=geteaw(); if (abrt) break;
                                if (templ&0x8000) templ|=0xFFFF0000;
                                regs[reg].l=templ;
                                cycles-=3;
                                break;

                                case 0xC0: /*XADD b*/
                                pclog("XADDb 32\n");
                                fetchea2();
                                temp=geteab(); if (abrt) break;
                                seteab(temp+getr8(reg)); if (abrt) break;
                                setadd8(temp,getr8(reg));
                                setr8(reg,temp);
                                break;
                                case 0xC1: /*XADD l*/
//                                pclog("XADDl 32\n");
                                fetchea2();
                                templ=geteal(); if (abrt) break;
                                templ2=regs[reg].l;
//                                pclog("EAL %08X REG %08X\n",templ,templ2);
                                seteal(templ+templ2); if (abrt) break;
                                setadd32(templ,templ2);
                                regs[reg].l=templ;
//                                pclog("EAL %08X REG %08X\n",templ+templ2,regs[reg].l);
                                break;

                                case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*BSWAP*/
                                case 0xCC: case 0xCD: case 0xCE: case 0xCF: /*486!!!*/
                                regs[temp&7].l = (regs[temp&7].l>>24)|((regs[temp&7].l>>8)&0xFF00)|((regs[temp&7].l<<8)&0xFF0000)|((regs[temp&7].l<<24)&0xFF000000);
                                cycles--;
                                break;

                                case 0xA2: /*CPUID*/
                                if (CPUID)
                                {
                                        cpu_CPUID();
                                        cycles-=9;
                                        break;
                                }
                                goto inv32;
                                
                                case 0xFF:
                                inv32:
                                pclog("INV32!\n");
                                default:
                                pclog("Bad 32-bit 0F opcode %02X 386\n",temp);
                                pc=oldpc;
                                x86illegal();
                                break;
                        }
                        break;

                        case 0x10: case 0x110: case 0x210: case 0x310: /*ADC 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setadc8(temp, temp2);
                                setr8(rm, temp + temp2 + tempc);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();               if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp + temp2 + tempc);   if (abrt) break;
                                setadc8(temp, temp2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x11: case 0x211: /*ADC 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setadc16(regs[rm].w, regs[reg].w);
                                regs[rm].w += regs[reg].w + tempc;
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw  = geteaw();              if (abrt) break;
                                tempw2 = regs[reg].w;
                                seteaw(tempw + tempw2 + tempc); if (abrt) break;
                                setadc16(tempw, tempw2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x111: case 0x311: /*ADC 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setadc32(regs[rm].l, regs[reg].l);
                                regs[rm].l += regs[reg].l + tempc;
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ  = geteal();              if (abrt) break;
                                templ2 = regs[reg].l;
                                seteal(templ + templ2 + tempc); if (abrt) break;
                                setadc32(templ, templ2);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x12: case 0x112: case 0x212: case 0x312: /*ADC reg,8*/
                        fetchea();
                        temp=geteab();                  if (abrt) break;
                        setadc8(getr8(reg),temp);
                        setr8(reg,getr8(reg)+temp+tempc);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x13: case 0x213: /*ADC reg,16*/
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        setadc16(regs[reg].w,tempw);
                        regs[reg].w+=tempw+tempc;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x113: case 0x313: /*ADC reg,32*/
                        fetchea();
                        templ=geteal();                 if (abrt) break;
                        setadc32(regs[reg].l,templ);
                        regs[reg].l+=templ+tempc;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x14: case 0x114: case 0x214: case 0x314: /*ADC AL,#8*/
                        tempw=getbytef();
                        setadc8(AL,tempw);
                        AL+=tempw+tempc;
                        cycles -= timing_rr;
                        break;
                        case 0x15: case 0x215: /*ADC AX,#16*/
                        tempw=getwordf();
                        setadc16(AX,tempw);
                        AX+=tempw+tempc;
                        cycles -= timing_rr;
                        break;
                        case 0x115: case 0x315: /*ADC EAX,#32*/
                        templ=getlong(); if (abrt) break;
                        setadc32(EAX,templ);
                        EAX+=templ+tempc;
                        cycles -= timing_rr;
                        break;

                        case 0x16: case 0x216: /*PUSH SS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,SS);           if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),SS); if (abrt) break;
                                SP-=2;
                        }
                        cycles-=2;
                        break;
                        case 0x116: case 0x316: /*PUSH SS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,SS);           if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),SS); if (abrt) break;
                                SP-=4;
                        }
                        cycles-=2;
                        break;
                        case 0x17: case 0x217: /*POP SS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP); if (abrt) break;
                                loadseg(tempw,&_ss); if (abrt) break;
                                ESP+=2;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);  if (abrt) break;
                                loadseg(tempw,&_ss); if (abrt) break;
                                SP+=2;
                        }
                        cycles-=(is486)?3:7;
                        break;
                        case 0x117: case 0x317: /*POP SS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP); if (abrt) break;
                                loadseg(tempw,&_ss); if (abrt) break;
                                ESP+=4;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);  if (abrt) break;
                                loadseg(tempw,&_ss); if (abrt) break;
                                SP+=4;
                        }
                        cycles-=(is486)?3:7;
                        break;

                        case 0x18: case 0x118: case 0x218: case 0x318: /*SBB 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setsbc8(temp, temp2);
                                setr8(rm, temp - (temp2 + tempc));
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();                  if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp - (temp2 + tempc));    if (abrt) break;
                                setsbc8(temp, temp2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x19: case 0x219: /*SBB 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setsbc16(regs[rm].w, regs[reg].w);
                                regs[rm].w -= (regs[reg].w + tempc);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw  = geteaw();                 if (abrt) break;
                                tempw2 = regs[reg].w;
                                seteaw(tempw - (tempw2 + tempc));  if (abrt) break;
                                setsbc16(tempw, tempw2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x119: case 0x319: /*SBB 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setsbc32(regs[rm].l, regs[reg].l);
                                regs[rm].l -= (regs[reg].l + tempc);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ  = geteal();                 if (abrt) break;
                                templ2 = regs[reg].l;
                                seteal(templ - (templ2 + tempc));  if (abrt) break;
                                setsbc32(templ, templ2);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x1A: case 0x11A: case 0x21A: case 0x31A: /*SBB reg,8*/
                        fetchea();
                        temp=geteab();                  if (abrt) break;
                        setsbc8(getr8(reg),temp);
                        setr8(reg,getr8(reg)-(temp+tempc));
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x1B: case 0x21B: /*SBB reg,16*/
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        tempw2=regs[reg].w;
                        setsbc16(tempw2,tempw);
                        tempw2-=(tempw+tempc);
                        regs[reg].w=tempw2;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x11B: case 0x31B: /*SBB reg,32*/
                        fetchea();
                        templ=geteal();                 if (abrt) break;
                        templ2=regs[reg].l;
                        setsbc32(templ2,templ);
                        templ2-=(templ+tempc);
                        regs[reg].l=templ2;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x1C: case 0x11C: case 0x21C: case 0x31C: /*SBB AL,#8*/
                        temp=getbytef();
                        setsbc8(AL,temp);
                        AL-=(temp+tempc);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x1D: case 0x21D: /*SBB AX,#16*/
                        tempw=getwordf();
                        setsbc16(AX,tempw);
                        AX-=(tempw+tempc);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x11D: case 0x31D: /*SBB AX,#32*/
                        templ=getlong(); if (abrt) break;
                        setsbc32(EAX,templ);
                        EAX-=(templ+tempc);
                        cycles-=(is486)?1:2;
                        break;

                        case 0x1E: case 0x21E: /*PUSH DS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,DS);                 if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),DS);       if (abrt) break;
                                SP-=2;
                        }
                        cycles-=2;
                        break;
                        case 0x11E: case 0x31E: /*PUSH DS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,DS);                 if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),DS);       if (abrt) break;
                                SP-=4;
                        }
                        cycles-=2;
                        break;
                        case 0x1F: case 0x21F: /*POP DS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP);                 if (abrt) break;
                                loadseg(tempw,&_ds); if (abrt) break;
                                ESP+=2;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);                  if (abrt) break;
                                loadseg(tempw,&_ds); if (abrt) break;
                                SP+=2;
                        }
                        cycles-=(is486)?3:7;
                        break;
                        case 0x11F: case 0x31F: /*POP DS*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP);                 if (abrt) break;
                                loadseg(tempw,&_ds); if (abrt) break;
                                ESP+=4;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);                  if (abrt) break;
                                loadseg(tempw,&_ds); if (abrt) break;
                                SP+=4;
                        }
                        cycles-=(is486)?3:7;
                        break;

                        case 0x20: case 0x120: case 0x220: case 0x320: /*AND 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm) & getr8(reg);
                                setr8(rm, temp);
                                setznp8(temp);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();    if (abrt) break;
                                temp &= getr8(reg);
                                seteab(temp);        if (abrt) break;
                                setznp8(temp);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x21: case 0x221: /*AND 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].w &= regs[reg].w;
                                setznp16(regs[rm].w);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw = geteaw() & regs[reg].w; if (abrt) break;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x121: case 0x321: /*AND 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].l &= regs[reg].l;
                                setznp32(regs[rm].l);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ = geteal() & regs[reg].l; if (abrt) break;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x22: case 0x122: case 0x222: case 0x322: /*AND reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp&=getr8(reg);
                        setznp8(temp);
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x23: case 0x223: /*AND reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw&=regs[reg].w;
                        setznp16(tempw);
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x123: case 0x323: /*AND reg,32*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        templ&=regs[reg].l;
                        setznp32(templ);
                        regs[reg].l=templ;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x24: case 0x124: case 0x224: case 0x324: /*AND AL,#8*/
                        AL&=getbytef();
                        setznp8(AL);
                        cycles -= timing_rr;
                        break;
                        case 0x25: case 0x225: /*AND AX,#16*/
                        AX&=getwordf();
                        setznp16(AX);
                        cycles -= timing_rr;
                        break;
                        case 0x125: case 0x325: /*AND EAX,#32*/
                        templ=getlong(); if (abrt) break;
                        EAX&=templ;
                        setznp32(EAX);
                        cycles -= timing_rr;
                        break;

                        case 0x26: case 0x126: case 0x226: case 0x326: /*ES:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=es;
                        rds=ES;
                        ssegs=2;
                        cycles-=4;
                        goto opcodestart;
//                        break;

                        case 0x27: case 0x127: case 0x227: case 0x327: /*DAA*/
                        if ((flags & A_FLAG) || ((AL & 0xF) > 9))
                        {
                                tempi = ((uint16_t)AL) + 6;
                                AL += 6;
                                flags |= A_FLAG;
                                if (tempi & 0x100) flags |= C_FLAG;
                        }
//                        else
//                           flags&=~A_FLAG;
                        if ((flags&C_FLAG) || (AL>0x9F))
                        {
                                AL+=0x60;
                                flags|=C_FLAG;
                        }
//                        else
//                           flags&=~C_FLAG;
                        tempw = flags & (C_FLAG | A_FLAG);
                        setznp8(AL);
                        flags |= tempw;
                        cycles-=4;
                        break;

                        case 0x28: case 0x128: case 0x228: case 0x328: /*SUB 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setsub8(temp, temp2);
                                setr8(rm, temp - temp2);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();     if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp - temp2); if (abrt) break;
                                setsub8(temp, temp2);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x29: case 0x229: /*SUB 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setsub16(regs[rm].w, regs[reg].w);
                                regs[rm].w -= regs[reg].w;
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw = geteaw();             if (abrt) break;
                                seteaw(tempw - regs[reg].w);  if (abrt) break;
                                setsub16(tempw, regs[reg].w);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x129: case 0x329: /*SUB 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setsub32(regs[rm].l, regs[reg].l);
                                regs[rm].l -= regs[reg].l;
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ = geteal();             if (abrt) break;
                                seteal(templ - regs[reg].l);  if (abrt) break;
                                setsub32(templ, regs[reg].l);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x2A: case 0x12A: case 0x22A: case 0x32A: /*SUB reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        setsub8(getr8(reg),temp);
                        setr8(reg,getr8(reg)-temp);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x2B: case 0x22B: /*SUB reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        setsub16(regs[reg].w,tempw);
                        regs[reg].w-=tempw;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x12B: case 0x32B: /*SUB reg,32*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        setsub32(regs[reg].l,templ);
                        regs[reg].l-=templ;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x2C: case 0x12C: case 0x22C: case 0x32C: /*SUB AL,#8*/
                        temp=getbytef();
                        setsub8(AL,temp);
                        AL-=temp;
                        cycles -= timing_rr;
                        break;
                        case 0x2D: case 0x22D: /*SUB AX,#16*/
                        tempw=getwordf();
                        setsub16(AX,tempw);
                        AX-=tempw;
                        cycles -= timing_rr;
                        break;
                        case 0x12D: case 0x32D: /*SUB EAX,#32*/
                        templ=getlong(); if (abrt) break;
                        setsub32(EAX,templ);
                        EAX-=templ;
                        cycles -= timing_rr;
                        break;

                        case 0x2E: case 0x12E: case 0x22E: case 0x32E: /*CS:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=cs;
                        rds=CS;
                        ssegs=2;
                        cycles-=4;
                        goto opcodestart;
                        case 0x2F: case 0x12F: case 0x22F: case 0x32F: /*DAS*/
                        if ((flags&A_FLAG)||((AL&0xF)>9))
                        {
                                tempi=((uint16_t)AL)-6;
                                AL-=6;
                                flags|=A_FLAG;
                                if (tempi&0x100) flags|=C_FLAG;
                        }
//                        else
//                           flags&=~A_FLAG;
                        if ((flags&C_FLAG)||(AL>0x9F))
                        {
                                AL-=0x60;
                                flags|=C_FLAG;
                        }
//                        else
//                           flags&=~C_FLAG;
                        tempw = flags & (C_FLAG | A_FLAG);
                        setznp8(AL);
                        flags |= tempw;
                        cycles-=4;
                        break;

                        case 0x30: case 0x130: case 0x230: case 0x330: /*XOR 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp = getr8(rm) ^ getr8(reg);
                                setr8(rm, temp);
                                setznp8(temp);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                temp  = geteab();    if (abrt) break;
                                temp ^= getr8(reg);
                                seteab(temp);        if (abrt) break;
                                setznp8(temp);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x31: case 0x231: /*XOR 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].w ^= regs[reg].w;
                                setznp16(regs[rm].w);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                tempw = geteaw() ^ regs[reg].w; if (abrt) break;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= timing_mr;
                        }
                        break;
                        case 0x131: case 0x331: /*XOR 32,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].l ^= regs[reg].l;
                                setznp32(regs[rm].l);
                                cycles -= timing_rr;
                        }
                        else
                        {
                                templ = geteal() ^ regs[reg].l; if (abrt) break;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= timing_mrl;
                        }
                        break;
                        case 0x32: case 0x132: case 0x232: case 0x332: /*XOR reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp^=getr8(reg);
                        setznp8(temp);
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x33: case 0x233: /*XOR reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw^=regs[reg].w;
                        setznp16(tempw);
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x133: case 0x333: /*XOR reg,32*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        templ^=regs[reg].l;
                        setznp32(templ);
                        regs[reg].l=templ;
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x34: case 0x134: case 0x234: case 0x334: /*XOR AL,#8*/
                        AL^=getbytef();
                        setznp8(AL);
                        cycles -= timing_rr;
                        break;
                        case 0x35: case 0x235: /*XOR AX,#16*/
                        AX^=getwordf();
                        setznp16(AX);
                        cycles -= timing_rr;
                        break;
                        case 0x135: case 0x335: /*XOR EAX,#32*/
                        templ=getlong(); if (abrt) break;
                        EAX^=templ;
                        setznp32(EAX);
                        cycles -= timing_rr;
                        break;

                        case 0x36: case 0x136: case 0x236: case 0x336: /*SS:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=ss;
                        rds=SS;
                        ssegs=2;
                        cycles-=4;
                        goto opcodestart;
//                        break;

                        case 0x37: case 0x137: case 0x237: case 0x337: /*AAA*/
                        if ((flags&A_FLAG)||((AL&0xF)>9))
                        {
                                AL+=6;
                                AH++;
                                flags|=(A_FLAG|C_FLAG);
                        }
                        else
                           flags&=~(A_FLAG|C_FLAG);
                        AL&=0xF;
                        cycles-=(is486)?3:4;
                        break;

                        case 0x38: case 0x138: case 0x238: case 0x338: /*CMP 8,reg*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        setsub8(temp,getr8(reg));
                        if (is486) cycles-=((mod==3)?1:2);
                        else       cycles-=((mod==3)?2:5);
                        break;
                        case 0x39: case 0x239: /*CMP 16,reg*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
//                        if (output) pclog("CMP %04X %04X\n",tempw,regs[reg].w);
                        setsub16(tempw,regs[reg].w);
                        if (is486) cycles-=((mod==3)?1:2);
                        else       cycles-=((mod==3)?2:5);
                        break;
                        case 0x139: case 0x339: /*CMP 32,reg*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        setsub32(templ,regs[reg].l);
                        if (is486) cycles-=((mod==3)?1:2);
                        else       cycles-=((mod==3)?2:5);
                        break;
                        case 0x3A: case 0x13A: case 0x23A: case 0x33A: /*CMP reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
//                        if (output) pclog("CMP %02X-%02X\n",getr8(reg),temp);
                        setsub8(getr8(reg),temp);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x3B: case 0x23B: /*CMP reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        setsub16(regs[reg].w,tempw);
                        cycles -= (mod == 3) ? timing_rr : timing_rm;
                        break;
                        case 0x13B: case 0x33B: /*CMP reg,32*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        setsub32(regs[reg].l,templ);
                        cycles -= (mod == 3) ? timing_rr : timing_rml;
                        break;
                        case 0x3C: case 0x13C: case 0x23C: case 0x33C: /*CMP AL,#8*/
                        temp=getbytef();
                        setsub8(AL,temp);
                        cycles -= timing_rr;
                        break;
                        case 0x3D: case 0x23D: /*CMP AX,#16*/
                        tempw=getwordf();
                        setsub16(AX,tempw);
                        cycles -= timing_rr;
                        break;
                        case 0x13D: case 0x33D: /*CMP EAX,#32*/
                        templ=getlong(); if (abrt) break;
                        setsub32(EAX,templ);
                        cycles -= timing_rr;
                        break;

                        case 0x3E: case 0x13E: case 0x23E: case 0x33E: /*DS:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=ds;
                        ssegs=2;
                        cycles-=4;
                        goto opcodestart;
//                        break;

                        case 0x3F: case 0x13F: case 0x23F: case 0x33F: /*AAS*/
                        if ((flags&A_FLAG)||((AL&0xF)>9))
                        {
                                AL-=6;
                                AH--;
                                flags|=(A_FLAG|C_FLAG);
                        }
                        else
                           flags&=~(A_FLAG|C_FLAG);
                        AL&=0xF;
                        cycles-=(is486)?3:4;
                        break;

                        case 0x40: case 0x41: case 0x42: case 0x43: /*INC r16*/
                        case 0x44: case 0x45: case 0x46: case 0x47:
                        case 0x240: case 0x241: case 0x242: case 0x243:
                        case 0x244: case 0x245: case 0x246: case 0x247:
                        setadd16nc(regs[opcode&7].w,1);
                        regs[opcode&7].w++;
                        cycles -= timing_rr;
                        break;
                        case 0x140: case 0x141: case 0x142: case 0x143: /*INC r32*/
                        case 0x144: case 0x145: case 0x146: case 0x147:
                        case 0x340: case 0x341: case 0x342: case 0x343:
                        case 0x344: case 0x345: case 0x346: case 0x347:
                        setadd32nc(regs[opcode&7].l,1);
                        regs[opcode&7].l++;
                        cycles -= timing_rr;
                        break;
                        case 0x48: case 0x49: case 0x4A: case 0x4B: /*DEC r16*/
                        case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                        case 0x248: case 0x249: case 0x24A: case 0x24B:
                        case 0x24C: case 0x24D: case 0x24E: case 0x24F:
                        setsub16nc(regs[opcode&7].w,1);
                        regs[opcode&7].w--;
                        cycles -= timing_rr;
                        break;
                        case 0x148: case 0x149: case 0x14A: case 0x14B: /*DEC r32*/
                        case 0x14C: case 0x14D: case 0x14E: case 0x14F:
                        case 0x348: case 0x349: case 0x34A: case 0x34B:
                        case 0x34C: case 0x34D: case 0x34E: case 0x34F:
                        setsub32nc(regs[opcode&7].l,1);
                        regs[opcode&7].l--;
                        cycles -= timing_rr;
                        break;

                        case 0x50: case 0x51: case 0x52: case 0x53: /*PUSH r16*/
                        case 0x54: case 0x55: case 0x56: case 0x57:
                        case 0x250: case 0x251: case 0x252: case 0x253:
                        case 0x254: case 0x255: case 0x256: case 0x257:
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,regs[opcode&7].w);           if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,(SP-2)&0xFFFF,regs[opcode&7].w);   if (abrt) break;
                                SP-=2;
                        }
//                        if (pc>=0x1A1BED && pc<0x1A1C00) pclog("PUSH %02X! %08X\n",opcode,ESP);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x150: case 0x151: case 0x152: case 0x153: /*PUSH r32*/
                        case 0x154: case 0x155: case 0x156: case 0x157:
                        case 0x350: case 0x351: case 0x352: case 0x353:
                        case 0x354: case 0x355: case 0x356: case 0x357:
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,regs[opcode&7].l);           if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,(SP-4)&0xFFFF,regs[opcode&7].l);   if (abrt) break;
                                SP-=4;
                        }
                        cycles-=(is486)?1:2;
                        break;
                        case 0x58: case 0x59: case 0x5A: case 0x5B: /*POP r16*/
                        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
                        case 0x258: case 0x259: case 0x25A: case 0x25B:
                        case 0x25C: case 0x25D: case 0x25E: case 0x25F:
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                ESP+=2;
                                tempw=readmemw(ss,ESP-2);            if (abrt) { ESP-=2; break; }
                        }
                        else
                        {
                                SP+=2;
                                tempw=readmemw(ss,(SP-2)&0xFFFF);    if (abrt) { SP-=2; break; }
                        }
                        regs[opcode&7].w=tempw;
                        cycles-=(is486)?1:4;
                        break;
                        case 0x158: case 0x159: case 0x15A: case 0x15B: /*POP r32*/
                        case 0x15C: case 0x15D: case 0x15E: case 0x15F:
                        case 0x358: case 0x359: case 0x35A: case 0x35B:
                        case 0x35C: case 0x35D: case 0x35E: case 0x35F:
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                ESP+=4;
                                templ=readmeml(ss,ESP-4);            if (abrt) { ESP-=4; break; }
                        }
                        else
                        {
                                SP+=4;
                                templ=readmeml(ss,(SP-4)&0xFFFF);    if (abrt) { SP-=4; break; }
                        }
                        regs[opcode&7].l=templ;
                        cycles-=(is486)?1:4;
                        break;

                        case 0x60: case 0x260: /*PUSHA*/
                        if (stack32)
                        {
                                writememw(ss,ESP-2,AX);
                                writememw(ss,ESP-4,CX);
                                writememw(ss,ESP-6,DX);
                                writememw(ss,ESP-8,BX);
                                writememw(ss,ESP-10,SP);
                                writememw(ss,ESP-12,BP);
                                writememw(ss,ESP-14,SI);
                                writememw(ss,ESP-16,DI);
                                if (!abrt) ESP-=16;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),AX);
                                writememw(ss,((SP-4)&0xFFFF),CX);
                                writememw(ss,((SP-6)&0xFFFF),DX);
                                writememw(ss,((SP-8)&0xFFFF),BX);
                                writememw(ss,((SP-10)&0xFFFF),SP);
                                writememw(ss,((SP-12)&0xFFFF),BP);
                                writememw(ss,((SP-14)&0xFFFF),SI);
                                writememw(ss,((SP-16)&0xFFFF),DI);
                                if (!abrt) SP-=16;
                        }
                        cycles-=(is486)?11:18;
                        break;
                        case 0x61: case 0x261: /*POPA*/
                        if (stack32)
                        {
                                DI=readmemw(ss,ESP);    if (abrt) break;
                                SI=readmemw(ss,ESP+2);  if (abrt) break;
                                BP=readmemw(ss,ESP+4);  if (abrt) break;
                                BX=readmemw(ss,ESP+8);  if (abrt) break;
                                DX=readmemw(ss,ESP+10); if (abrt) break;
                                CX=readmemw(ss,ESP+12); if (abrt) break;
                                AX=readmemw(ss,ESP+14); if (abrt) break;
                                ESP+=16;
                        }
                        else
                        {
                                DI=readmemw(ss,((SP)&0xFFFF));          if (abrt) break;
                                SI=readmemw(ss,((SP+2)&0xFFFF));        if (abrt) break;
                                BP=readmemw(ss,((SP+4)&0xFFFF));        if (abrt) break;
                                BX=readmemw(ss,((SP+8)&0xFFFF));        if (abrt) break;
                                DX=readmemw(ss,((SP+10)&0xFFFF));       if (abrt) break;
                                CX=readmemw(ss,((SP+12)&0xFFFF));       if (abrt) break;
                                AX=readmemw(ss,((SP+14)&0xFFFF));       if (abrt) break;
                                SP+=16;
                        }
                        cycles-=(is486)?9:24;
                        break;
                        case 0x160: case 0x360: /*PUSHA*/
                        if (stack32)
                        {
                                writememl(ss,ESP-4,EAX);
                                writememl(ss,ESP-8,ECX);
                                writememl(ss,ESP-12,EDX);
                                writememl(ss,ESP-16,EBX);
                                writememl(ss,ESP-20,ESP);
                                writememl(ss,ESP-24,EBP);
                                writememl(ss,ESP-28,ESI);
                                writememl(ss,ESP-32,EDI);
                                if (!abrt) ESP-=32;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),EAX);
                                writememl(ss,((SP-8)&0xFFFF),ECX);
                                writememl(ss,((SP-12)&0xFFFF),EDX);
                                writememl(ss,((SP-16)&0xFFFF),EBX);
                                writememl(ss,((SP-20)&0xFFFF),ESP);
                                writememl(ss,((SP-24)&0xFFFF),EBP);
                                writememl(ss,((SP-28)&0xFFFF),ESI);
                                writememl(ss,((SP-32)&0xFFFF),EDI);
                                if (!abrt) SP-=32;
                        }
                        cycles-=(is486)?11:18;
                        break;
                        case 0x161: case 0x361: /*POPA*/
                        if (stack32)
                        {
                                EDI=readmeml(ss,ESP);           if (abrt) break;
                                ESI=readmeml(ss,ESP+4);         if (abrt) break;
                                EBP=readmeml(ss,ESP+8);         if (abrt) break;
                                EBX=readmeml(ss,ESP+16);        if (abrt) break;
                                EDX=readmeml(ss,ESP+20);        if (abrt) break;
                                ECX=readmeml(ss,ESP+24);        if (abrt) break;
                                EAX=readmeml(ss,ESP+28);        if (abrt) break;
                                ESP+=32;
                        }
                        else
                        {
                                EDI=readmeml(ss,((SP)&0xFFFF));         if (abrt) break;
                                ESI=readmeml(ss,((SP+4)&0xFFFF));       if (abrt) break;
                                EBP=readmeml(ss,((SP+8)&0xFFFF));       if (abrt) break;
                                EBX=readmeml(ss,((SP+16)&0xFFFF));      if (abrt) break;
                                EDX=readmeml(ss,((SP+20)&0xFFFF));      if (abrt) break;
                                ECX=readmeml(ss,((SP+24)&0xFFFF));      if (abrt) break;
                                EAX=readmeml(ss,((SP+28)&0xFFFF));      if (abrt) break;
                                SP+=32;
                        }
                        cycles-=(is486)?9:24;
                        break;

                        case 0x62: case 0x262: /*BOUND*/
                        fetchea();
                        tempw=geteaw();
                        tempw2=readmemw(easeg,eaaddr+2); if (abrt) break;
                        if (((int16_t)regs[reg].w<(int16_t)tempw) || ((int16_t)regs[reg].w>(int16_t)tempw2))
                        {
                                x86_int(5);
                        }
                        cycles-=(is486)?7:10;
                        break;
                        case 0x162: case 0x362: /*BOUND*/
                        fetchea();
                        templ=geteal();
                        templ2=readmeml(easeg,eaaddr+4); if (abrt) break;
                        if (((int32_t)regs[reg].l<(int32_t)templ) || ((int32_t)regs[reg].l>(int32_t)templ2))
                        {
                                x86_int(5);
                        }
                        cycles-=(is486)?7:10;
                        break;

                        
                        case 0x63: case 0x163: case 0x263: case 0x363: /*ARPL*/
                        NOTRM
                        fetchea();
                        tempw=geteaw(); if (abrt) break;
                        if ((tempw&3)<(regs[reg].w&3))
                        {
                                tempw=(tempw&0xFFFC)|(regs[reg].w&3);
                                seteaw(tempw); if (abrt) break;
                                flags|=Z_FLAG;
                        }
                        else
                           flags&=~Z_FLAG;
                        cycles-=(is486)?9:20;
                        break;
                        case 0x64: case 0x164: case 0x264: case 0x364: /*FS:*/
                        oldss=ss;
                        oldds=ds;
                        rds=FS;
                        ds=ss=fs;
                        ssegs=2;
                        cycles-=4;
                        goto opcodestart;
                        case 0x65: case 0x165: case 0x265: case 0x365: /*GS:*/
                        oldss=ss;
                        oldds=ds;
                        rds=GS;
                        ds=ss=gs;
                        ssegs=2;
                        cycles-=4;
                        goto opcodestart;

                        case 0x66: case 0x166: case 0x266: case 0x366: /*Data size select*/
                        op32=((use32&0x100)^0x100)|(op32&0x200);
//                        op32^=0x100;
                        cycles-=2;
                        goto opcodestart;
                        case 0x67: case 0x167: case 0x267: case 0x367: /*Address size select*/
                        op32=((use32&0x200)^0x200)|(op32&0x100);
//                        op32^=0x200;
                        cycles-=2;
                        goto opcodestart;

                        case 0x68: case 0x268: /*PUSH #w*/
                        tempw=getword();
                        if (stack32)
                        {
                                writememw(ss,ESP-2,tempw);              if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),tempw);    if (abrt) break;
                                SP-=2;
                        }
                        cycles-=2;
                        break;
                        case 0x168: case 0x368: /*PUSH #l*/
                        templ=getlong();
                        if (stack32)
                        {
                                writememl(ss,ESP-4,templ);              if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),templ);    if (abrt) break;
                                SP-=4;
                        }
                        cycles-=2;
                        break;
                        case 0x69: case 0x269: /*IMUL r16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw2=getword();       if (abrt) break;
                        templ=((int)(int16_t)tempw)*((int)(int16_t)tempw2);
                        if ((templ>>16)!=0 && (templ>>16)!=0xFFFF) flags|=C_FLAG|V_FLAG;
                        else                                       flags&=~(C_FLAG|V_FLAG);
                        regs[reg].w=templ&0xFFFF;
                        cycles-=((mod==3)?14:17);
                        break;
                        case 0x169: case 0x369: /*IMUL r32*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        templ2=getlong();       if (abrt) break;
                        temp64=((int64_t)(int32_t)templ)*((int64_t)(int32_t)templ2);
                        if ((temp64>>32)!=0 && (temp64>>32)!=0xFFFFFFFF) flags|=C_FLAG|V_FLAG;
                        else                                             flags&=~(C_FLAG|V_FLAG);
                        regs[reg].l=temp64&0xFFFFFFFF;
                        cycles-=25;
                        break;
                        case 0x6A: case 0x26A:/*PUSH #eb*/
                        tempw=readmemb(cs,pc); pc++;
                        if (tempw&0x80) tempw|=0xFF00;
                        if (output) pclog("PUSH %04X %i\n",tempw,stack32);
                        if (stack32)
                        {
                                writememw(ss,ESP-2,tempw);              if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),tempw);    if (abrt) break;
                                SP-=2;
                        }
                        cycles-=2;
                        break;
                        case 0x16A: case 0x36A:/*PUSH #eb*/
                        templ=readmemb(cs,pc); pc++;
                        if (templ&0x80) templ|=0xFFFFFF00;
                        if (output) pclog("PUSH %08X %i\n",templ,stack32);
                        if (stack32)
                        {
                                writememl(ss,ESP-4,templ);              if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),templ);    if (abrt) break;
                                SP-=4;
                        }
                        cycles-=2;
                        break;
//                        #if 0
                        case 0x6B: case 0x26B: /*IMUL r8*/
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        tempw2=readmemb(cs,pc); pc++;   if (abrt) break;
                        if (tempw2&0x80) tempw2|=0xFF00;
                        templ=((int)(int16_t)tempw)*((int)(int16_t)tempw2);
//                        pclog("IMULr8  %08X %08X %08X\n",tempw,tempw2,templ);
                        if ((templ>>16)!=0 && ((templ>>16)&0xFFFF)!=0xFFFF) flags|=C_FLAG|V_FLAG;
                        else                                                flags&=~(C_FLAG|V_FLAG);
                        regs[reg].w=templ&0xFFFF;
                        cycles-=((mod==3)?14:17);
                        break;
                        case 0x16B: case 0x36B: /*IMUL r8*/
                        fetchea();
                        templ=geteal();                 if (abrt) break;
                        templ2=readmemb(cs,pc); pc++;   if (abrt) break;
                        if (templ2&0x80) templ2|=0xFFFFFF00;
                        temp64=((int64_t)(int32_t)templ)*((int64_t)(int32_t)templ2);
//                        pclog("IMULr8  %08X %08X %i\n",templ,templ2,temp64i>>32);
                        if ((temp64>>32)!=0 && (temp64>>32)!=0xFFFFFFFF) flags|=C_FLAG|V_FLAG;
                        else                                             flags&=~(C_FLAG|V_FLAG);
                        regs[reg].l=temp64&0xFFFFFFFF;
                        cycles-=20;
                        break;
//#endif
                        case 0x6C: case 0x16C: /*INSB*/
                        checkio_perm(DX);
                        temp=inb(DX);
                        writememb(es,DI,temp);          if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        cycles-=15;
                        break;
                        case 0x26C: case 0x36C: /*INSB*/
                        checkio_perm(DX);
                        temp=inb(DX);
                        writememb(es,EDI,temp);         if (abrt) break;
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        cycles-=15;
                        break;
                        case 0x6D: /*INSW*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        tempw=inw(DX);
                        writememw(es,DI,tempw);         if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        cycles-=15;
                        break;
                        case 0x16D: /*INSL*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        checkio_perm(DX+2);
                        checkio_perm(DX+3);
                        templ=inl(DX);
                        writememl(es,DI,templ);         if (abrt) break;
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        cycles-=15;
                        break;
                        case 0x26D: /*INSW*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        tempw=inw(DX);
                        writememw(es,EDI,tempw);         if (abrt) break;
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        cycles-=15;
                        break;
                        case 0x36D: /*INSL*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        checkio_perm(DX+2);
                        checkio_perm(DX+3);
                        templ=inl(DX);
                        writememl(es,EDI,templ);         if (abrt) break;
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        cycles-=15;
                        break;
                        case 0x6E: case 0x16E: /*OUTSB*/
                        temp=readmemb(ds,SI);           if (abrt) break;
                        checkio_perm(DX);
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        outb(DX,temp);
                        cycles-=14;
                        break;
                        case 0x26E: case 0x36E: /*OUTSB*/
                        checkio_perm(DX);
                        temp=readmemb(ds,ESI);          if (abrt) break;
                        if (flags&D_FLAG) ESI--;
                        else              ESI++;
                        outb(DX,temp);
                        cycles-=14;
                        break;
                        case 0x6F: /*OUTSW*/
                        tempw=readmemw(ds,SI);          if (abrt) break;
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        outw(DX,tempw);
//                        outb(DX+1,tempw>>8);
                        cycles-=14;
                        break;
                        case 0x16F: /*OUTSL*/
                        tempw=readmemw(ds,SI);         if (abrt) break;
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        checkio_perm(DX+2);
                        checkio_perm(DX+3);
                        if (flags&D_FLAG) SI-=4;
                        else              SI+=4;
                        outl(EDX,templ);
                        cycles-=14;
                        break;
                        case 0x26F: /*OUTSW*/
                        tempw=readmemw(ds,ESI);         if (abrt) break;
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        if (flags&D_FLAG) ESI-=2;
                        else              ESI+=2;
                        outw(DX,tempw);
                        cycles-=14;
                        break;
                        case 0x36F: /*OUTSL*/
                        tempw=readmemw(ds,ESI);         if (abrt) break;
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        checkio_perm(DX+2);
                        checkio_perm(DX+3);
                        if (flags&D_FLAG) ESI-=4;
                        else              ESI+=4;
                        outl(EDX,templ);
                        cycles-=14;
                        break;

                        case 0x70: case 0x170: case 0x270: case 0x370: /*JO*/
                        offset=(int8_t)getbytef();
                        if (flags&V_FLAG) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x71: case 0x171: case 0x271: case 0x371: /*JNO*/
                        offset=(int8_t)getbytef();
                        if (!(flags&V_FLAG)) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x72: case 0x172: case 0x272: case 0x372: /*JB*/
                        offset=(int8_t)getbytef();
                        if (flags&C_FLAG) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x73: case 0x173: case 0x273: case 0x373: /*JNB*/
                        offset=(int8_t)getbytef();
                        if (!(flags&C_FLAG)) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x74: case 0x174: case 0x274: case 0x374: /*JZ*/
                        offset=(int8_t)getbytef();
                        if (flags&Z_FLAG) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x75: case 0x175: case 0x275: case 0x375: /*JNZ*/
                        offset=(int8_t)getbytef();
                        if (!(flags&Z_FLAG)) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x76: case 0x176: case 0x276: case 0x376: /*JBE*/
                        offset=(int8_t)getbytef();
                        if (flags&(C_FLAG|Z_FLAG)) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x77: case 0x177: case 0x277: case 0x377: /*JNBE*/
                        offset=(int8_t)getbytef();
                        if (!(flags&(C_FLAG|Z_FLAG))) { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x78: case 0x178: case 0x278: case 0x378: /*JS*/
                        offset=(int8_t)getbytef();
                        if (flags&N_FLAG)  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x79: case 0x179: case 0x279: case 0x379: /*JNS*/
                        offset=(int8_t)getbytef();
                        if (!(flags&N_FLAG))  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x7A: case 0x17A: case 0x27A: case 0x37A: /*JP*/
                        offset=(int8_t)getbytef();
                        if (flags&P_FLAG)  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x7B: case 0x17B: case 0x27B: case 0x37B: /*JNP*/
                        offset=(int8_t)getbytef();
                        if (!(flags&P_FLAG))  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x7C: case 0x17C: case 0x27C: case 0x37C: /*JL*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if (temp!=temp2)  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x7D: case 0x17D: case 0x27D: case 0x37D: /*JNL*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if (temp==temp2)  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x7E: case 0x17E: case 0x27E: case 0x37E: /*JLE*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if ((flags&Z_FLAG) || (temp!=temp2))  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;
                        case 0x7F: case 0x17F: case 0x27F: case 0x37F: /*JNLE*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if (!((flags&Z_FLAG) || (temp!=temp2)))  { pc += offset; cycles -= timing_bt; }
                        cycles -= timing_bnt;
                        break;


                        case 0x80: case 0x180: case 0x280: case 0x380:
                        case 0x82: case 0x182: case 0x282: case 0x382:
                        fetchea();
                        temp=geteab();                  if (abrt) break;
                        temp2=readmemb(cs,pc); pc++;    if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD b,#8*/
                                seteab(temp+temp2);             if (abrt) break;
                                setadd8(temp,temp2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x08: /*OR b,#8*/
                                temp|=temp2;
                                seteab(temp);                   if (abrt) break;
                                setznp8(temp);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x10: /*ADC b,#8*/
                                seteab(temp+temp2+tempc);       if (abrt) break;
                                setadc8(temp,temp2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x18: /*SBB b,#8*/
                                seteab(temp-(temp2+tempc));     if (abrt) break;
                                setsbc8(temp,temp2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x20: /*AND b,#8*/
                                temp&=temp2;
                                seteab(temp);                   if (abrt) break;
                                setznp8(temp);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x28: /*SUB b,#8*/
                                seteab(temp-temp2);             if (abrt) break;
                                setsub8(temp,temp2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x30: /*XOR b,#8*/
                                temp^=temp2;
                                seteab(temp);                   if (abrt) break;
                                setznp8(temp);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x38: /*CMP b,#8*/
                                setsub8(temp,temp2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:7);
                                break;
                        }
                        break;

                        case 0x81: case 0x281:
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw2=getword();       if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD w,#16*/
                                seteaw(tempw+tempw2);   if (abrt) break;
                                setadd16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x08: /*OR w,#16*/
                                tempw|=tempw2;
                                seteaw(tempw);          if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x10: /*ADC w,#16*/
                                seteaw(tempw+tempw2+tempc); if (abrt) break;
                                setadc16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x20: /*AND w,#16*/
                                tempw&=tempw2;
                                seteaw(tempw);          if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x18: /*SBB w,#16*/
                                seteaw(tempw-(tempw2+tempc));   if (abrt) break;
                                setsbc16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x28: /*SUB w,#16*/
                                seteaw(tempw-tempw2);           if (abrt) break;
                                setsub16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x30: /*XOR w,#16*/
                                tempw^=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x38: /*CMP w,#16*/
                                setsub16(tempw,tempw2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:7);
                                break;
                        }
                        break;
                        case 0x181: case 0x381:
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        templ2=getlong();       if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD l,#32*/
                                seteal(templ+templ2);           if (abrt) break;
                                setadd32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x08: /*OR l,#32*/
                                templ|=templ2;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x10: /*ADC l,#32*/
                                seteal(templ+templ2+tempc);     if (abrt) break;
                                setadc32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x20: /*AND l,#32*/
                                templ&=templ2;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x18: /*SBB l,#32*/
                                seteal(templ-(templ2+tempc));   if (abrt) break;
                                setsbc32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x28: /*SUB l,#32*/
                                seteal(templ-templ2);           if (abrt) break;
                                setsub32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x30: /*XOR l,#32*/
                                templ^=templ2;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mrl;
                                break;
                                case 0x38: /*CMP l,#32*/
                                setsub32(templ,templ2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:7);
                                break;
                        }
                        break;

                        case 0x83: case 0x283:
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        tempw2=readmemb(cs,pc); pc++;   if (abrt) break;
                        if (tempw2&0x80) tempw2|=0xFF00;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD w,#8*/
                                seteaw(tempw+tempw2);           if (abrt) break;
                                setadd16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x08: /*OR w,#8*/
                                tempw|=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x10: /*ADC w,#8*/
                                seteaw(tempw+tempw2+tempc);     if (abrt) break;
                                setadc16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x18: /*SBB w,#8*/
                                seteaw(tempw-(tempw2+tempc));   if (abrt) break;
                                setsbc16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x20: /*AND w,#8*/
                                tempw&=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x28: /*SUB w,#8*/
                                seteaw(tempw-tempw2);           if (abrt) break;
                                setsub16(tempw,tempw2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x30: /*XOR w,#8*/
                                tempw^=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x38: /*CMP w,#8*/
                                setsub16(tempw,tempw2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:7);
                                break;
                        }
                        break;
                        case 0x183: case 0x383:
                        fetchea();
                        templ=geteal();                 if (abrt) break;
                        templ2=readmemb(cs,pc); pc++;   if (abrt) break;
                        if (templ2&0x80) templ2|=0xFFFFFF00;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD l,#32*/
                                seteal(templ+templ2);           if (abrt) break;
                                setadd32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x08: /*OR l,#32*/
                                templ|=templ2;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x10: /*ADC l,#32*/
                                seteal(templ+templ2+tempc);     if (abrt) break;
                                setadc32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x20: /*AND l,#32*/
                                templ&=templ2;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x18: /*SBB l,#32*/
                                seteal(templ-(templ2+tempc));   if (abrt) break;
                                setsbc32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x28: /*SUB l,#32*/
                                seteal(templ-templ2);           if (abrt) break;
                                setsub32(templ,templ2);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x30: /*XOR l,#32*/
                                templ^=templ2;
                                seteal(templ);                  if (abrt) break;
                                setznp32(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mr;
                                break;
                                case 0x38: /*CMP l,#32*/
                                setsub32(templ,templ2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:7);
                                break;
                        }
                        break;

                        case 0x84: case 0x184: case 0x284: case 0x384: /*TEST b,reg*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp2=getr8(reg);
                        setznp8(temp&temp2);
                        if (is486) cycles-=((mod==3)?1:2);
                        else       cycles-=((mod==3)?2:5);
                        break;
                        case 0x85: case 0x285: /*TEST w,reg*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw2=regs[reg].w;
                        setznp16(tempw&tempw2);
                        if (is486) cycles-=((mod==3)?1:2);
                        else       cycles-=((mod==3)?2:5);
                        break;
                        case 0x185: case 0x385: /*TEST l,reg*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        templ2=regs[reg].l;
                        setznp32(templ&templ2);
                        if (is486) cycles-=((mod==3)?1:2);
                        else       cycles-=((mod==3)?2:5);
                        break;
                        case 0x86: case 0x186: case 0x286: case 0x386: /*XCHG b,reg*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        seteab(getr8(reg));     if (abrt) break;
                        setr8(reg,temp);
                        cycles-=((mod==3)?3:5);
                        break;
                        case 0x87: case 0x287: /*XCHG w,reg*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        seteaw(regs[reg].w);    if (abrt) break;
                        regs[reg].w=tempw;
                        cycles-=((mod==3)?3:5);
                        break;
                        case 0x187: case 0x387: /*XCHG l,reg*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        seteal(regs[reg].l);    if (abrt) break;
                        regs[reg].l=templ;
                        cycles-=((mod==3)?3:5);
                        break;

                        case 0x88: case 0x188: case 0x288: case 0x388: /*MOV b,reg*/
                        fetchea();
                        seteab(getr8(reg));
                        cycles-=(is486)?1:2;
                        break;
                        case 0x89: case 0x289: /*MOV w,reg*/
                        fetchea();
                        seteaw(regs[reg].w);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x189: case 0x389: /*MOV l,reg*/
                        fetchea();
                        //if (output==3) pclog("Write %08X to %08X:%08X %08X %08X %08X\n",regs[reg].l,easeg,eaaddr,);
                        seteal(regs[reg].l);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x8A: case 0x18A: case 0x28A: case 0x38A: /*MOV reg,b*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        setr8(reg,temp);
                        if (is486) cycles--;
                        else       cycles-=((mod==3)?2:4);
                        break;
                        case 0x8B: case 0x28B: /*MOV reg,w*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        regs[reg].w=tempw;
                        if (is486) cycles--;
                        else       cycles-=((mod==3)?2:4);
                        break;
                        case 0x18B: case 0x38B: /*MOV reg,l*/
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        regs[reg].l=templ;
                        if (is486) cycles--;
                        else       cycles-=((mod==3)?2:4);
                        break;

                        case 0x8C: case 0x28C: /*MOV w,sreg*/
                        fetchea();
//                        if (output==3) pclog("MOV sreg %02X %08X\n",rmdat,fetchdat);
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ES*/
                                seteaw(ES);
                                break;
                                case 0x08: /*CS*/
                                seteaw(CS);
                                break;
                                case 0x18: /*DS*/
                                if (ssegs) ds=oldds;
                                seteaw(DS);
                                break;
                                case 0x10: /*SS*/
                                if (ssegs) ss=oldss;
                                seteaw(SS);
                                break;
                                case 0x20: /*FS*/
                                seteaw(FS);
                                break;
                                case 0x28: /*GS*/
                                seteaw(GS);
                                break;
                        }
                        cycles-=((mod==3)?2:3);
                        break;
                        case 0x18C: case 0x38C: /*MOV l,sreg*/
                        fetchea();
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ES*/
                                if (mod==3) regs[rm].l=ES;
                                else        seteaw(ES);
                                break;
                                case 0x08: /*CS*/
                                if (mod==3) regs[rm].l=CS;
                                else        seteaw(CS);
                                break;
                                case 0x18: /*DS*/
                                if (ssegs) ds=oldds;
                                if (mod==3) regs[rm].l=DS;
                                else        seteaw(DS);
                                break;
                                case 0x10: /*SS*/
                                if (ssegs) ss=oldss;
                                if (mod==3) regs[rm].l=SS;
                                else        seteaw(SS);
                                break;
                                case 0x20: /*FS*/
                                if (mod==3) regs[rm].l=FS;
                                else        seteaw(FS);
                                break;
                                case 0x28: /*GS*/
                                if (mod==3) regs[rm].l=GS;
                                else        seteaw(GS);
                                break;
                        }
                        cycles-=((mod==3)?2:3);
                        break;

                        case 0x8D: case 0x28D: /*LEA*/
                        fetchea();
                        regs[reg].w=eaaddr;
                        cycles -= timing_rr;
                        break;
                        case 0x18D: /*LEA*/
                        fetchea();
                        regs[reg].l=eaaddr&0xFFFF;
                        cycles -= timing_rr;
                        break;
                        case 0x38D: /*LEA*/
                        fetchea();
                        regs[reg].l=eaaddr;
                        cycles -= timing_rr;
                        break;

                        case 0x8E: case 0x18E: case 0x28E: case 0x38E: /*MOV sreg,w*/
                        fetchea();
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ES*/
                                tempw=geteaw();         if (abrt) break;
                                loadseg(tempw,&_es);
                                break;
                                case 0x18: /*DS*/
                                tempw=geteaw();         if (abrt) break;
                                loadseg(tempw,&_ds);
                                if (ssegs) oldds=ds;
                                break;
                                case 0x10: /*SS*/
//                                if (output==3) pclog("geteaw\n");
                                tempw=geteaw();         if (abrt) break;
//                                if (output==3) pclog("loadseg\n");
                                loadseg(tempw,&_ss);
//                                if (output==3) pclog("done\n");
                                if (ssegs) oldss=ss;
                                skipnextprint=1;
				noint=1;
                                break;
                                case 0x20: /*FS*/
                                tempw=geteaw();         if (abrt) break;
                                loadseg(tempw,&_fs);
                                break;
                                case 0x28: /*GS*/
                                tempw=geteaw();         if (abrt) break;
                                loadseg(tempw,&_gs);
                                break;
                        }
                        cycles-=((mod==3)?2:5);
                        break;

                        case 0x8F: case 0x28F: /*POPW*/
                        if (ssegs) templ2=oldss;
                        else       templ2=ss;
                        if (stack32)
                        {
                                tempw=readmemw(templ2,ESP);     if (abrt) break;
                                ESP+=2;
                        }
                        else
                        {
                                tempw=readmemw(templ2,SP);      if (abrt) break;
                                SP+=2;
                        }
                        fetchea();
                        if (ssegs) ss=oldss;
                        seteaw(tempw);
                        if (abrt)
                        {
                                if (stack32) ESP-=2;
                                else         SP-=2;
                        }
                        if (is486) cycles-=((mod==3)?1:6);
                        else       cycles-=((mod==3)?4:5);
                        break;
                        case 0x18F: case 0x38F: /*POPL*/
                        if (ssegs) templ2=oldss;
                        else       templ2=ss;
                        if (stack32)
                        {
                                templ=readmeml(templ2,ESP);     if (abrt) break;
                                ESP+=4;
                        }
                        else
                        {
                                templ=readmeml(templ2,SP);      if (abrt) break;
                                SP+=4;
                        }
                        fetchea();
                        if (ssegs) ss=oldss;
                        seteal(templ);
                        if (abrt)
                        {
                                if (stack32) ESP-=4;
                                else         SP-=4;
                        }
                        if (is486) cycles-=((mod==3)?1:6);
                        else       cycles-=((mod==3)?4:5);
                        break;

                        case 0x90: case 0x190: case 0x290: case 0x390: /*NOP*/
                        cycles-=(is486)?1:3;
                        break;

                        case 0x91: case 0x92: case 0x93: /*XCHG AX*/
                        case 0x94: case 0x95: case 0x96: case 0x97:
                        case 0x291: case 0x292: case 0x293:
                        case 0x294: case 0x295: case 0x296: case 0x297:
                        tempw=AX;
                        AX=regs[opcode&7].w;
                        regs[opcode&7].w=tempw;
                        cycles-=3;
                        break;
                        case 0x191: case 0x192: case 0x193: /*XCHG EAX*/
                        case 0x194: case 0x195: case 0x196: case 0x197:
                        case 0x391: case 0x392: case 0x393: /*XCHG EAX*/
                        case 0x394: case 0x395: case 0x396: case 0x397:
                        templ=EAX;
                        EAX=regs[opcode&7].l;
                        regs[opcode&7].l=templ;
                        cycles-=3;
                        break;

                        case 0x98: case 0x298: /*CBW*/
                        AH=(AL&0x80)?0xFF:0;
                        cycles-=3;
                        break;
                        case 0x198: case 0x398: /*CWDE*/
                        EAX=(AX&0x8000)?(0xFFFF0000|AX):AX;
                        cycles-=3;
                        break;
                        case 0x99: case 0x299: /*CWD*/
                        DX=(AX&0x8000)?0xFFFF:0;
                        cycles-=2;
                        break;
                        case 0x199: case 0x399: /*CDQ*/
                        EDX=(EAX&0x80000000)?0xFFFFFFFF:0;
                        cycles-=2;
                        break;
                        case 0x9A: case 0x29A: /*CALL FAR*/
                        tempw=getword();
                        tempw2=getword();       if (abrt) break;
                        if (output == 3) pclog("Call far %04X:%04X\n",tempw2,tempw);
                        tempw3=CS;
                        templ2 = pc;
                        if (output) pclog("Call far %08X\n",templ2);
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        pc=tempw;
                        optype=CALL;
                        cgate32=0;
                        if (output == 3) pclog("Load CS\n");
                        if (msw&1) loadcscall(tempw2);
                        else       loadcs(tempw2);
                        if (output == 3) pclog("%i %i\n", abrt, cgate32);
                        optype=0;
//                        if (output==3) pclog("CALL FAR 16 complete\n");
                        if (abrt) break;
                        oldss=ss;
                        if (cgate32) goto writecall32;
                writecall16:
                        cgate16=0;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,tempw3);
                                writememw(ss,ESP-4,templ2);     if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememw(ss,(SP-2)&0xFFFF,tempw3);
                                if (output) pclog("Write CS to %04X:%04X\n",SS,SP-2);
                                writememw(ss,(SP-4)&0xFFFF,templ2);     if (abrt) break;
                                if (output) pclog("Write PC %08X to %04X:%04X\n", templ2, SS,SP-4);
                                SP-=4;
                        }
                        cycles-=(is486)?18:17;
                        break;
                        case 0x19A: case 0x39A: /*CALL FAR*/
//                        if (output==3) pclog("CF 1 %08X\n",pc);
                        templ=getword(); templ|=(getword()<<16);
//                        if (output==3) pclog("CF 2\n");
                        tempw2=getword();       if (abrt) break;
//                        if (output==3) pclog("CF 3 %04X:%08X\n",tempw2,templ);
                        tempw3=CS;
                        templ2 = pc;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        pc=templ;
                        optype=CALL;
                        cgate16=0;
//                        if (output==3) pclog("Load CS\n");
                        if (msw&1) loadcscall(tempw2);
                        else       loadcs(tempw2);
//                        if (output==3) pclog("%i %i\n",notpresent,abrt);
                        optype=0;
//                        if (output==3) pclog("CALL FAR 32 complete\n");
                        if (abrt) break;
                        oldss=ss;
                        if (cgate16) goto writecall16;
                writecall32:
                        cgate32=0;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,tempw3);
                                writememl(ss,ESP-8,templ2);             if (abrt) break;
                                if (output) pclog("Write PC %08X to %04X:%04X\n", templ2, SS,ESP-8);
                                ESP-=8;
                        }
                        else
                        {
                                writememl(ss,(SP-4)&0xFFFF,tempw3);
                                writememl(ss,(SP-8)&0xFFFF,templ2);     if (abrt) break;
                                if (output) pclog("Write PC %08X to %04X:%04X\n", templ2, SS,SP-8);
                                SP-=8;
                        }
                        cycles-=(is486)?18:17;
                        break;
                        case 0x9B: case 0x19B: case 0x29B: case 0x39B: /*WAIT*/
                        cycles-=4;
                        break;
                        case 0x9C: case 0x29C: /*PUSHF*/
                        if (ssegs) ss=oldss;
                        if ((eflags&VM_FLAG) && (IOPL<3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
                        if (stack32)
                        {
                                writememw(ss,ESP-2,flags);              if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),flags);    if (abrt) break;
                                SP-=2;
                        }
                        cycles-=4;
                        break;
                        case 0x19C: case 0x39C: /*PUSHFD*/
//                        pclog("PUSHFD %04X(%08X):%08X\n",CS,cs,pc);
                        if (ssegs) ss=oldss;
                        if ((eflags&VM_FLAG) && (IOPL<3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
                        if (CPUID) tempw=eflags&0x24;
                        else       tempw=eflags&4;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,tempw);
                                writememw(ss,ESP-4,flags);              if (abrt) break;
                                ESP-=4;
//                                if (output==3) pclog("Pushing %04X %04X\n",eflags,flags);
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),tempw);
                                writememw(ss,((SP-4)&0xFFFF),flags);    if (abrt) break;
                                SP-=4;
                        }
                        cycles-=4;
                        break;
                        case 0x9D: case 0x29D: /*POPF*/
//                        if (CS!=0x21 && CS!=0xF000) pclog("POPF %04X:%04X\n",CS,pc);
                        if (ssegs) ss=oldss;
                        if ((eflags&VM_FLAG) && (IOPL<3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP);                 if (abrt) break;
                                ESP+=2;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);                  if (abrt) break;
                                SP+=2;
                        }
//                        pclog("POPF! %i %i %i\n",CPL,msw&1,IOPLp);
                        if (!(CPL) || !(msw&1)) flags=(tempw&0xFFD5)|2;
                        else if (IOPLp) flags=(flags&0x3000)|(tempw&0xCFD5)|2;
                        else            flags=(flags&0xF200)|(tempw&0x0DD5)|2;
//                        if (flags==0xF000) pclog("POPF - flags now F000 %04X(%06X):%04X %08X %08X %08X\n",CS,cs,pc,old8,old82,old83);
                        cycles-=5;
                        break;
                        case 0x19D: case 0x39D: /*POPFD*/
                        if (ssegs) ss=oldss;
                        if ((eflags&VM_FLAG) && (IOPL<3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP);
                                tempw2=readmemw(ss,ESP+2);              if (abrt) break;
                                ESP+=4;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);
                                tempw2=readmemw(ss,SP+2);               if (abrt) break;
                                SP+=4;
                        }
//                        eflags|=0x200000;
                        if (!(CPL) || !(msw&1)) flags=(tempw&0xFFD5)|2;
                        else if (IOPLp) flags=(flags&0x3000)|(tempw&0xCFD5)|2;
                        else            flags=(flags&0xF200)|(tempw&0x0DD5)|2;
                        tempw2&=(is486)?0x24:0;
                        tempw2|=(eflags&3);
                        if (CPUID) eflags=tempw2&0x27;
                        else if (is486) eflags=tempw2&7;
                        else       eflags=tempw2&3;
//                        if (flags==0xF000) pclog("POPF - flags now F000 %04X(%06X):%04X %08X %08X %08X\n",CS,cs,pc,old8,old82,old83);
                        cycles-=5;
                        break;
                        case 0x9E: case 0x19E: case 0x29E: case 0x39E: /*SAHF*/
                        flags=(flags&0xFF00)|(AH&0xD5)|2;
                        cycles-=3;
                        break;
                        case 0x9F: case 0x19F: case 0x29F: case 0x39F: /*LAHF*/
                        AH=flags&0xFF;
                        cycles-=3;
                        break;

                        case 0xA0: case 0x1A0: /*MOV AL,(w)*/
                        addr=getword(); if (abrt) break;
                        temp=readmemb(ds,addr);         if (abrt) break;
                        AL=temp;
                        cycles-=(is486)?1:4;
                        break;
                        case 0x2A0: case 0x3A0: /*MOV AL,(l)*/
                        addr=getlong(); if (abrt) break;
                        temp=readmemb(ds,addr);         if (abrt) break;
                        AL=temp;
                        cycles-=(is486)?1:4;
                        break;
                        case 0xA1: /*MOV AX,(w)*/
                        addr=getword(); if (abrt) break;
                        tempw=readmemw(ds,addr);        if (abrt) break;
                        AX=tempw;
                        cycles-=(is486)?1:4;
                        break;
                        case 0x1A1: /*MOV EAX,(w)*/
                        addr=getword(); if (abrt) break;
                        templ=readmeml(ds,addr);        if (abrt) break;
                        EAX=templ;
                        cycles-=(is486)?1:4;
                        break;
                        case 0x2A1: /*MOV AX,(l)*/
                        addr=getlong(); if (abrt) break;
                        tempw=readmemw(ds,addr);        if (abrt) break;
                        AX=tempw;
                        cycles-=(is486)?1:4;
                        break;
                        case 0x3A1: /*MOV EAX,(l)*/
                        addr=getlong(); if (abrt) break;
                        templ=readmeml(ds,addr);        if (abrt) break;
                        EAX=templ;
                        cycles-=(is486)?1:4;
                        break;
                        case 0xA2: case 0x1A2: /*MOV (w),AL*/
                        addr=getword(); if (abrt) break;
                        writememb(ds,addr,AL);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x2A2: case 0x3A2: /*MOV (l),AL*/
                        addr=getlong(); if (abrt) break;
                        writememb(ds,addr,AL);
                        cycles-=(is486)?1:2;
                        break;
                        case 0xA3: /*MOV (w),AX*/
                        addr=getword(); if (abrt) break;
                        writememw(ds,addr,AX);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x1A3: /*MOV (w),EAX*/
                        addr=getword(); if (abrt) break;
                        writememl(ds,addr,EAX);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x2A3: /*MOV (l),AX*/
                        addr=getlong(); if (abrt) break;
                        writememw(ds,addr,AX);
                        cycles-=(is486)?1:2;
                        break;
                        case 0x3A3: /*MOV (l),EAX*/
                        addr=getlong(); if (abrt) break;
                        writememl(ds,addr,EAX);
                        cycles-=(is486)?1:2;
                        break;

                        case 0xA4: case 0x1A4: /*MOVSB*/
                        temp=readmemb(ds,SI);  if (abrt) break;
                        writememb(es,DI,temp); if (abrt) break;
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        cycles-=7;
                        break;
                        case 0x2A4: case 0x3A4: /*MOVSB*/
                        temp=readmemb(ds,ESI);  if (abrt) break;
                        writememb(es,EDI,temp); if (abrt) break;
                        if (flags&D_FLAG) { EDI--; ESI--; }
                        else              { EDI++; ESI++; }
                        cycles-=7;
                        break;
                        case 0xA5: /*MOVSW*/
                        tempw=readmemw(ds,SI);  if (abrt) break;
                        writememw(es,DI,tempw); if (abrt) break;
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        cycles-=7;
                        break;
                        case 0x2A5: /*MOVSW*/
                        tempw=readmemw(ds,ESI);  if (abrt) break;
                        writememw(es,EDI,tempw); if (abrt) break;
                        if (flags&D_FLAG) { EDI-=2; ESI-=2; }
                        else              { EDI+=2; ESI+=2; }
                        cycles-=7;
                        break;
                        case 0x1A5: /*MOVSL*/
                        templ=readmeml(ds,SI);  if (abrt) break;
                        writememl(es,DI,templ); if (abrt) break;
                        if (flags&D_FLAG) { DI-=4; SI-=4; }
                        else              { DI+=4; SI+=4; }
                        cycles-=7;
                        break;
                        case 0x3A5: /*MOVSL*/
                        templ=readmeml(ds,ESI);  if (abrt) break;
                        writememl(es,EDI,templ); if (abrt) break;
                        if (flags&D_FLAG) { EDI-=4; ESI-=4; }
                        else              { EDI+=4; ESI+=4; }
                        cycles-=7;
                        break;
                        case 0xA6: case 0x1A6: /*CMPSB*/
                        temp =readmemb(ds,SI);
                        temp2=readmemb(es,DI);
                        if (abrt) break;
                        setsub8(temp,temp2);
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        cycles-=(is486)?8:10;
                        break;
                        case 0x2A6: case 0x3A6: /*CMPSB*/
                        temp =readmemb(ds,ESI);
                        temp2=readmemb(es,EDI);
                        if (abrt) break;
                        setsub8(temp,temp2);
                        if (flags&D_FLAG) { EDI--; ESI--; }
                        else              { EDI++; ESI++; }
                        cycles-=(is486)?8:10;
                        break;
                        case 0xA7: /*CMPSW*/
                        tempw =readmemw(ds,SI);
                        tempw2=readmemw(es,DI);
                        if (abrt) break;
                        setsub16(tempw,tempw2);
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        cycles-=(is486)?8:10;
                        break;
                        case 0x1A7: /*CMPSL*/
                        templ =readmeml(ds,SI);
                        templ2=readmeml(es,DI);
                        if (abrt) break;
                        setsub32(templ,templ2);
                        if (flags&D_FLAG) { DI-=4; SI-=4; }
                        else              { DI+=4; SI+=4; }
                        cycles-=(is486)?8:10;
                        break;
                        case 0x2A7: /*CMPSW*/
                        tempw =readmemw(ds,ESI);
                        tempw2=readmemw(es,EDI);
                        if (abrt) break;
                        setsub16(tempw,tempw2);
                        if (flags&D_FLAG) { EDI-=2; ESI-=2; }
                        else              { EDI+=2; ESI+=2; }
                        cycles-=(is486)?8:10;
                        break;
                        case 0x3A7: /*CMPSL*/
                        templ =readmeml(ds,ESI);
                        templ2=readmeml(es,EDI);
                        if (abrt) break;
                        setsub32(templ,templ2);
                        if (flags&D_FLAG) { EDI-=4; ESI-=4; }
                        else              { EDI+=4; ESI+=4; }
                        cycles-=(is486)?8:10;
                        break;
                        case 0xA8: case 0x1A8: case 0x2A8: case 0x3A8: /*TEST AL,#8*/
                        temp=getbytef();
                        setznp8(AL&temp);
                        cycles -= timing_rr;
                        break;
                        case 0xA9: case 0x2A9: /*TEST AX,#16*/
                        tempw=getwordf();
                        setznp16(AX&tempw);
                        cycles -= timing_rr;
                        break;
                        case 0x1A9: case 0x3A9: /*TEST EAX,#32*/
                        templ=getlong();        if (abrt) break;
                        setznp32(EAX&templ);
                        cycles -= timing_rr;
                        break;
                        case 0xAA: case 0x1AA: /*STOSB*/
                        writememb(es,DI,AL);    if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        cycles-=4;
                        break;
                        case 0x2AA: case 0x3AA: /*STOSB*/
                        writememb(es,EDI,AL);   if (abrt) break;
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        cycles-=4;
                        break;
                        case 0xAB: /*STOSW*/
                        writememw(es,DI,AX);    if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        cycles-=4;
                        break;
                        case 0x1AB: /*STOSL*/
                        writememl(es,DI,EAX);   if (abrt) break;
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        cycles-=4;
                        break;
                        case 0x2AB: /*STOSW*/
                        writememw(es,EDI,AX);   if (abrt) break;
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        cycles-=4;
                        break;
                        case 0x3AB: /*STOSL*/
                        writememl(es,EDI,EAX);  if (abrt) break;
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        cycles-=4;
                        break;
                        case 0xAC: case 0x1AC: /*LODSB*/
                        temp=readmemb(ds,SI);
                        if (abrt) break;
                        AL=temp;
//                        if (output==3) pclog("LODSB %02X from %05X:%04X\n",AL,ds,SI);
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        cycles-=5;
                        break;
                        case 0x2AC: case 0x3AC: /*LODSB*/
                        temp=readmemb(ds,ESI);
                        if (abrt) break;
                        AL=temp;
                        if (flags&D_FLAG) ESI--;
                        else              ESI++;
                        cycles-=5;
                        break;
                        case 0xAD: /*LODSW*/
                        tempw=readmemw(ds,SI);
                        if (abrt) break;
                        AX=tempw;
//                        if (output) pclog("Load from %05X:%04X\n",ds,SI);
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        cycles-=5;
                        break;
                        case 0x1AD: /*LODSL*/
                        templ=readmeml(ds,SI);
                        if (abrt) break;
                        EAX=templ;
                        if (flags&D_FLAG) SI-=4;
                        else              SI+=4;
                        cycles-=5;
                        break;
                        case 0x2AD: /*LODSW*/
                        tempw=readmemw(ds,ESI);
                        if (abrt) break;
                        AX=tempw;
                        if (flags&D_FLAG) ESI-=2;
                        else              ESI+=2;
                        cycles-=5;
                        break;
                        case 0x3AD: /*LODSL*/
                        templ=readmeml(ds,ESI);
                        if (abrt) break;
                        EAX=templ;
                        if (flags&D_FLAG) ESI-=4;
                        else              ESI+=4;
                        cycles-=5;
                        break;
                        case 0xAE: case 0x1AE: /*SCASB*/
                        temp=readmemb(es,DI);
                        if (abrt) break;
                        setsub8(AL,temp);
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        cycles-=7;
                        break;
                        case 0x2AE: case 0x3AE: /*SCASB*/
                        temp=readmemb(es,EDI);
                        if (abrt) break;
                        setsub8(AL,temp);
                        if (flags&D_FLAG) EDI--;
                        else              EDI++;
                        cycles-=7;
                        break;
                        case 0xAF: /*SCASW*/
                        tempw=readmemw(es,DI);
                        if (abrt) break;
                        setsub16(AX,tempw);
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        cycles-=7;
                        break;
                        case 0x1AF: /*SCASL*/
                        templ=readmeml(es,DI);
                        if (abrt) break;
                        setsub32(EAX,templ);
                        if (flags&D_FLAG) DI-=4;
                        else              DI+=4;
                        cycles-=7;
                        break;
                        case 0x2AF: /*SCASW*/
                        tempw=readmemw(es,EDI);
                        if (abrt) break;
                        setsub16(AX,tempw);
                        if (flags&D_FLAG) EDI-=2;
                        else              EDI+=2;
                        cycles-=7;
                        break;
                        case 0x3AF: /*SCASL*/
                        templ=readmeml(es,EDI);
                        if (abrt) break;
                        setsub32(EAX,templ);
                        if (flags&D_FLAG) EDI-=4;
                        else              EDI+=4;
                        cycles-=7;
                        break;

                        case 0xB0: case 0x1B0: case 0x2B0: case 0x3B0: /*MOV AL,#8*/
                        AL=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB1: case 0x1B1: case 0x2B1: case 0x3B1: /*MOV CL,#8*/
                        CL=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB2: case 0x1B2: case 0x2B2: case 0x3B2: /*MOV DL,#8*/
                        DL=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB3: case 0x1B3: case 0x2B3: case 0x3B3: /*MOV BL,#8*/
                        BL=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB4: case 0x1B4: case 0x2B4: case 0x3B4: /*MOV AH,#8*/
                        AH=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB5: case 0x1B5: case 0x2B5: case 0x3B5: /*MOV CH,#8*/
                        CH=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB6: case 0x1B6: case 0x2B6: case 0x3B6: /*MOV DH,#8*/
                        DH=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB7: case 0x1B7: case 0x2B7: case 0x3B7: /*MOV BH,#8*/
                        BH=getbytef();
                        cycles -= timing_rr;
                        break;
                        case 0xB8: case 0xB9: case 0xBA: case 0xBB: /*MOV reg,#16*/
                        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                        case 0x2B8: case 0x2B9: case 0x2BA: case 0x2BB:
                        case 0x2BC: case 0x2BD: case 0x2BE: case 0x2BF:
                        regs[opcode&7].w=getwordf();
                        cycles -= timing_rr;
                        break;
                        case 0x1B8: case 0x1B9: case 0x1BA: case 0x1BB: /*MOV reg,#32*/
                        case 0x1BC: case 0x1BD: case 0x1BE: case 0x1BF:
                        case 0x3B8: case 0x3B9: case 0x3BA: case 0x3BB:
                        case 0x3BC: case 0x3BD: case 0x3BE: case 0x3BF:
                        templ=getlong();     if (abrt) break;
                        regs[opcode&7].l=templ;
                        cycles -= timing_rr;
                        break;

                        case 0xC0: case 0x1C0: case 0x2C0: case 0x3C0:
                        fetchea();
                        c=readmemb(cs,pc); pc++;
                        temp=geteab();          if (abrt) break;
                        c&=31;
                        if (!c) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL b,CL*/
                                while (c>0)
                                {
                                        temp2=(temp&0x80)?1:0;
                                        temp=(temp<<1)|temp2;
                                        c--;
                                }
                                seteab(temp);   if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(temp>>7)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR b,CL*/
                                while (c>0)
                                {
                                        temp2=temp&1;
                                        temp>>=1;
                                        if (temp2) temp|=0x80;
                                        c--;
                                }
                                seteab(temp);   if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL b,CL*/
                                temp2=flags&C_FLAG;
                                while (c>0)
                                {
                                        tempc=(temp2)?1:0;
                                        temp2=temp&0x80;
                                        temp=(temp<<1)|tempc;
                                        c--;
                                        if (is486) cycles--;
                                }
                                seteab(temp);   if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(temp>>7)) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;
                                case 0x18: /*RCR b,CL*/
                                temp2=flags&C_FLAG;
                                while (c>0)
                                {
                                        tempc=(temp2)?0x80:0;
                                        temp2=temp&1;
                                        temp=(temp>>1)|tempc;
                                        c--;
                                        if (is486) cycles--;
                                }
                                seteab(temp);   if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;
                                case 0x20: case 0x30: /*SHL b,CL*/
                                seteab(temp<<c);        if (abrt) break;
                                setznp8(temp<<c);
                                if ((temp<<(c-1))&0x80) flags|=C_FLAG;
                                if (((temp<<c)^(temp<<(c-1)))&0x80) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x28: /*SHR b,CL*/
                                seteab(temp>>c);        if (abrt) break;
                                setznp8(temp>>c);
                                if ((temp>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && temp&0x80) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x38: /*SAR b,CL*/
                                tempc=((temp>>(c-1))&1);
                                while (c>0)
                                {
                                        temp>>=1;
                                        if (temp&0x40) temp|=0x80;
                                        c--;
                                }
                                seteab(temp);   if (abrt) break;
                                setznp8(temp);
                                if (tempc) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;

                        case 0xC1: case 0x2C1:
                        fetchea();
                        c=readmemb(cs,pc)&31; pc++;
                        tempw=geteaw();         if (abrt) break;
                        if (!c) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL w,CL*/
                                while (c>0)
                                {
                                        temp=(tempw&0x8000)?1:0;
                                        tempw=(tempw<<1)|temp;
                                        c--;
                                }
                                seteaw(tempw);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(tempw>>15)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR w,CL*/
                                while (c>0)
                                {
                                        tempw2=(tempw&1)?0x8000:0;
                                        tempw=(tempw>>1)|tempw2;
                                        c--;
                                }
                                seteaw(tempw);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (tempw2) flags|=C_FLAG;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL w,CL*/
                                temp2=flags&C_FLAG;
                                while (c>0)
                                {
                                        tempc=(temp2)?1:0;
                                        temp2=(tempw>>15);
                                        tempw=(tempw<<1)|tempc;
                                        c--;
                                        if (is486) cycles--;
                                }
                                seteaw(tempw);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(tempw>>15)) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;
                                case 0x18: /*RCR w,CL*/
                                temp2=flags&C_FLAG;
                                while (c>0)
                                {
                                        tempc=(temp2)?0x8000:0;
                                        temp2=tempw&1;
                                        tempw=(tempw>>1)|tempc;
                                        c--;
                                        if (is486) cycles--;
                                }
                                seteaw(tempw);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;

                                case 0x20: case 0x30: /*SHL w,CL*/
                                seteaw(tempw<<c); if (abrt) break;
                                setznp16(tempw<<c);
                                if ((tempw<<(c-1))&0x8000) flags|=C_FLAG;
                                if (((tempw<<c)^(tempw<<(c-1)))&0x8000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x28:            /*SHR w,CL*/
                                seteaw(tempw>>c); if (abrt) break;
                                setznp16(tempw>>c);
                                if ((tempw>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && tempw&0x8000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x38:            /*SAR w,CL*/
                                tempw2=tempw&0x8000;
                                tempc=(tempw>>(c-1))&1;
                                while (c>0)
                                {
                                        tempw=(tempw>>1)|tempw2;
                                        c--;
                                }
                                seteaw(tempw);  if (abrt) break;
                                setznp16(tempw);
                                if (tempc) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;
                        case 0x1C1: case 0x3C1:
                        fetchea();
                        c=readmemb(cs,pc); pc++;
                        c&=31;
                        templ=geteal();         if (abrt) break;
                        if (!c) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL l,CL*/
                                while (c>0)
                                {
                                        temp=(templ&0x80000000)?1:0;
                                        templ=(templ<<1)|temp;
                                        c--;
                                }
                                seteal(templ);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(templ>>31)) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;
                                case 0x08: /*ROR l,CL*/
                                while (c>0)
                                {
                                        templ2=(templ&1)?0x80000000:0;
                                        templ=(templ>>1)|templ2;
                                        c--;
                                }
                                seteal(templ);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (templ2) flags|=C_FLAG;
                                if ((templ^(templ>>1))&0x40000000) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;
                                case 0x10: /*RCL l,CL*/
                                temp2=flags&C_FLAG;
                                while (c>0)
                                {
                                        tempc=(flags&C_FLAG)?1:0;
                                        temp2=templ>>31;
                                        templ=(templ<<1)|tempc;
                                        c--;
                                        if (is486) cycles--;
                                }
                                seteal(templ);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(templ>>31)) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;
                                case 0x18: /*RCR l,CL*/
                                temp2=flags&C_FLAG;
                                while (c>0)
                                {
                                        tempc=(temp2)?0x80000000:0;
                                        temp2=templ&1;
                                        templ=(templ>>1)|tempc;
                                        c--;
                                        if (is486) cycles--;
                                }
                                seteal(templ);  if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((templ^(templ>>1))&0x40000000) flags|=V_FLAG;
                                cycles-=((mod==3)?9:10);
                                break;

                                case 0x20: case 0x30: /*SHL l,CL*/
                                seteal(templ<<c); if (abrt) break;
                                setznp32(templ<<c);
                                if ((templ<<(c-1))&0x80000000) flags|=C_FLAG;
                                if (((templ<<c)^(templ<<(c-1)))&0x80000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x28:            /*SHR l,CL*/
                                seteal(templ>>c);       if (abrt) break;
                                setznp32(templ>>c);
                                if ((templ>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && templ&0x80000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x38:            /*SAR l,CL*/
                                templ2=templ&0x80000000;
                                tempc=(templ>>(c-1))&1;
                                while (c>0)
                                {
                                        templ=(templ>>1)|templ2;
                                        c--;
                                }
                                seteal(templ);  if (abrt) break;
                                setznp32(templ);
                                if (tempc) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;


                        case 0xC2: case 0x2C2: /*RET*/
                        tempw=getword();
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw2=readmemw(ss,ESP); if (abrt) break;
                                ESP+=2+tempw;
                        }
                        else
                        {
                                tempw2=readmemw(ss,SP); if (abrt) break;
                                SP+=2+tempw;
                        }
                        pc=tempw2;
                        cycles-=(is486)?5:10;
                        break;
                        case 0x1C2: case 0x3C2: /*RET*/
                        tempw=getword();
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                templ=readmeml(ss,ESP); if (abrt) break;
                                ESP+=4+tempw;
                        }
                        else
                        {
                                templ=readmeml(ss,SP);  if (abrt) break;
                                SP+=4+tempw;
                        }
                        pc=templ;
                        cycles-=(is486)?5:10;
                        break;
                        case 0xC3: case 0x2C3: /*RET*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                tempw=readmemw(ss,ESP); if (abrt) break;
                                ESP+=2;
                        }
                        else
                        {
                                tempw=readmemw(ss,SP);  if (abrt) break;
                                SP+=2;
                        }
                        pc=tempw;
                        cycles-=(is486)?5:10;
                        break;
                        case 0x1C3: case 0x3C3: /*RET*/
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                templ=readmeml(ss,ESP); if (abrt) break;
                                ESP+=4;
                        }
                        else
                        {
                                templ=readmeml(ss,SP);  if (abrt) break;
                                SP+=4;
                        }
                        pc=templ;
                        cycles-=(is486)?5:10;
                        break;
                        case 0xC4: case 0x2C4: /*LES*/
                        fetchea();
                        tempw2=readmemw(easeg,eaaddr);
                        tempw=readmemw(easeg,eaaddr+2);
//                        if (output==3) pclog("LES %04X:%08X %04X:%08X\n",easeg,eaaddr,tempw,tempw2);
                        if (abrt) break;
                        loadseg(tempw,&_es);
                        if (abrt) break;
                        regs[reg].w=tempw2;
//                        if (output==3) pclog("LES complete\n");
//                        if (!ES) pclog("LES=0 %04X(%06X):%04X\n",CS,cs,pc);
//                        if (cr0&1) pclog("ES loaded with %04X  %04X(%08X):%08X\n",tempw,CS,cs,pc);
                        cycles-=7;
                        break;
                        case 0x1C4: case 0x3C4: /*LES*/
                        fetchea();
                        templ=readmeml(easeg,eaaddr);
                        tempw=readmemw(easeg,eaaddr+4);
                        if (abrt) break;
                        loadseg(tempw,&_es);
                        if (abrt) break;
                        regs[reg].l=templ;
                        cycles-=7;
                        break;
                        case 0xC5: case 0x2C5: /*LDS*/
                        fetchea();
                        tempw2=readmemw(easeg,eaaddr);
                        tempw=readmemw(easeg,eaaddr+2);
                        if (abrt) break;
                        loadseg(tempw,&_ds);
                        if (abrt) break;
                        if (ssegs) oldds=ds;
                        regs[reg].w=tempw2;
                        cycles-=7;
                        break;
                        case 0x1C5: case 0x3C5: /*LDS*/
                        fetchea();
                        templ=readmeml(easeg,eaaddr);
                        tempw=readmemw(easeg,eaaddr+4);
                        if (abrt) break;
                        loadseg(tempw,&_ds);
                        if (abrt) break;
                        if (ssegs) oldds=ds;
                        regs[reg].l=templ;
                        cycles-=7;
                        break;
                        case 0xC6: case 0x1C6: case 0x2C6: case 0x3C6: /*MOV b,#8*/
                        fetchea();
                        temp=readmemb(cs,pc); pc++;     if (abrt) break;
                        seteab(temp);
                        cycles -= timing_rr;
                        break;
                        case 0xC7: case 0x2C7: /*MOV w,#16*/
                        fetchea();
                        tempw=getword();                if (abrt) break;
                        seteaw(tempw);
                        cycles -= timing_rr;
                        break;
                        case 0x1C7: case 0x3C7: /*MOV l,#32*/
                        fetchea();
                        templ=getlong();                if (abrt) break;
                        seteal(templ);
                        cycles -= timing_rr;
                        break;
                        case 0xC8: case 0x2C8: /*ENTER*/
                        tempw2=getword();
                        tempi=readmemb(cs,pc); pc++;
                        templ=EBP;
                        if (stack32) { writememw(ss,(ESP-2),BP);         if (abrt) break; ESP-=2; }
                        else         { writememw(ss,((SP-2)&0xFFFF),BP); if (abrt) break; SP-=2; }
                        templ2=ESP;
                        if (tempi>0)
                        {
                                while (--tempi)
                                {
                                        EBP-=2;
                                        if (stack32) tempw=readmemw(ss,EBP);
                                        else         tempw=readmemw(ss,BP);
                                        if (abrt) { ESP=templ2; EBP=templ; break; }
                                        if (stack32) { writememw(ss,(ESP-2),tempw);        ESP-=2; }
                                        else         { writememw(ss,((SP-2)&0xFFFF),tempw); SP-=2; }
                                        if (abrt) { ESP=templ2; EBP=templ; break; }
                                        cycles-=(is486)?3:4;
                                }
                                if (stack32) { writememw(ss,(ESP-2),templ2);        ESP-=2; }
                                else         { writememw(ss,((SP-2)&0xFFFF),templ2); SP-=2; }
                                if (abrt) { ESP=templ2; EBP=templ; break; }
                                cycles-=(is486)?3:5;
                        }
                        BP = templ2;
                        if (stack32) ESP-=tempw2;
                        else         SP-=tempw2;
                        cycles-=(is486)?14:10;
//                        if (cr0&1) pclog("BP %04X\n",BP);
//                        if (output==3) pclog("\n");
                        break;
                        case 0x1C8: case 0x3C8: /*ENTER*/
//                        if (output==1 && SS==0xA0)
//                        {
//                                enters++;
//                                for (ec=0;ec<enters;ec++) pclog(" ");
//                                pclog("ENTER %02X %04X %04X %i\n",SS,BP,SP,eflags&VM_FLAG);
//                        }
                        tempw=getword();
                        tempi=readmemb(cs,pc); pc++;
                        if (stack32) { writememl(ss,(ESP-4),EBP);         if (abrt) break; ESP-=4; }
                        else         { writememl(ss,((SP-4)&0xFFFF),EBP); if (abrt) break; SP-=4; }
                        templ2=ESP; templ3=EBP;
                        if (tempi>0)
                        {
                                while (--tempi)
                                {
                                        EBP-=4;
                                        if (stack32) templ=readmeml(ss,EBP);
                                        else         templ=readmeml(ss,BP);
                                        if (abrt) { ESP=templ2; EBP=templ3; break; }
                                        if (stack32) { writememl(ss,(ESP-4),templ);        ESP-=4; }
                                        else         { writememl(ss,((SP-4)&0xFFFF),templ); SP-=4; }
                                        if (abrt) { ESP=templ2; EBP=templ3; break; }
                                        cycles-=(is486)?3:4;
                                }
                                if (stack32) { writememl(ss,(ESP-4),templ2);        ESP-=4; }
                                else         { writememl(ss,((SP-4)&0xFFFF),templ2); SP-=4; }
                                if (abrt) { ESP=templ2; EBP=templ3; break; }
                                cycles-=(is486)?3:5;
                        }
                        EBP=templ2;
                        if (stack32) ESP-=tempw;
                        else         SP-=tempw;
                        cycles-=(is486)?14:10;
//                        if (output==3) pclog("\n");
                        break;
                        case 0xC9: case 0x2C9: /*LEAVE*/
                        templ=ESP;
                        SP=BP;
                        if (stack32) { tempw=readmemw(ss,ESP); ESP+=2; }
                        else         { tempw=readmemw(ss,SP);   SP+=2; }
                        if (abrt) { ESP=templ; break; }
                        BP=tempw;
                        cycles-=4;
//                        if (output==1 && SS==0xA0)
//                        {
//                                for (ec=0;ec<enters;ec++) pclog(" ");
//                                pclog("LEAVE %02X %04X %04X %i\n",SS,BP,SP,eflags&VM_FLAG);
//                                enters--;
//                        }
//                        if (output==3) pclog("\n");
                        break;
                        case 0x3C9: case 0x1C9: /*LEAVE*/
                        templ=ESP;
                        ESP=EBP;
                        if (stack32) { templ2=readmeml(ss,ESP); ESP+=4; }
                        else         { templ2=readmeml(ss,SP);  SP+=4;  }
                        if (abrt) { ESP=templ; break; }
                        EBP=templ2;
                        cycles-=4;
//                        if (output==1 && SS==0xA0)
//                        {
//                                for (ec=0;ec<enters;ec++) pclog(" ");
//                                pclog("LEAVE %02X %04X %04X %i\n",SS,BP,SP,eflags&VM_FLAG);
//                                enters--;
//                        }
//                        if (output==3) pclog("\n");
                        break;
                        case 0xCA: case 0x2CA: /*RETF*/
                        tempw=getword();
                        if ((msw&1) && !(eflags&VM_FLAG))
                        {
                                pmoderetf(0,tempw);
                                break;
                        }
                        tempw2=CPL;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        if (stack32)
                        {
                                pc=readmemw(ss,ESP);
                                loadcs(readmemw(ss,ESP+2));
                        }
                        else
                        {
                                pc=readmemw(ss,SP);
                                loadcs(readmemw(ss,SP+2));
                        }
                        if (abrt) break;
                        if (stack32) ESP+=4+tempw;
                        else         SP+=4+tempw;
                        cycles-=(is486)?13:18;
                        break;
                        case 0x1CA: case 0x3CA: /*RETF*/
                        tempw=getword();
                        if ((msw&1) && !(eflags&VM_FLAG))
                        {
                                pmoderetf(1,tempw);
                                break;
                        }
                        tempw2=CPL;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        if (stack32)
                        {
                                pc=readmeml(ss,ESP);
                                loadcs(readmeml(ss,ESP+4)&0xFFFF);
                        }
                        else
                        {
                                pc=readmeml(ss,SP);
                                loadcs(readmeml(ss,SP+4)&0xFFFF);
                        }
                        if (abrt) break;
                        if (stack32) ESP+=8+tempw;
                        else         SP+=8+tempw;
                        cycles-=(is486)?13:18;
                        break;
                        case 0xCB: case 0x2CB: /*RETF*/
                        if ((msw&1) && !(eflags&VM_FLAG))
                        {
                                pmoderetf(0,0);
                                break;
                        }
                        tempw2=CPL;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        if (stack32)
                        {
                                pc=readmemw(ss,ESP);
                                loadcs(readmemw(ss,ESP+2));
                        }
                        else
                        {
                                pc=readmemw(ss,SP);
                                loadcs(readmemw(ss,SP+2));
                        }
                        if (abrt) break;
                        if (stack32) ESP+=4;
                        else         SP+=4;
                        cycles-=(is486)?13:18;
                        break;
                        case 0x1CB: case 0x3CB: /*RETF*/
                        if ((msw&1) && !(eflags&VM_FLAG))
                        {
                                pmoderetf(1,0);
                                break;
                        }
                        tempw2=CPL;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        if (stack32)
                        {
                                pc=readmeml(ss,ESP);
                                loadcs(readmemw(ss,ESP+4));
                        }
                        else
                        {
                                pc=readmeml(ss,SP);
                                loadcs(readmemw(ss,SP+4));
                        }
                        if (abrt) break;
                        if (stack32) ESP+=8;
                        else         SP+=8;
                        if ((msw&1) && CPL>tempw2)
                        {
                                if (stack32)
                                {
                                        templ=readmeml(ss,ESP);
                                        loadseg(readmeml(ss,ESP+4),&_ss);
                                        ESP=templ;
                                }
                                else
                                {
                                        templ=readmeml(ss,SP);
                                        loadseg(readmeml(ss,SP+4),&_ss);
                                        ESP=templ;
                                }
                        }
                        cycles-=(is486)?13:18;
                        break;
                        case 0xCC: case 0x1CC: case 0x2CC: case 0x3CC: /*INT 3*/
                        if (msw&1)
                        {
                                pmodeint(3,1);
                                cycles-=(is486)?44:59;
                        }
                        else
                        {
                                if (ssegs) ss=oldss;
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
                                addr=3<<2;
//                                flags&=~I_FLAG;
                                flags&=~T_FLAG;
                                oxpc=pc;
                                pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                                cycles-=(is486)?26:33;
                        }
                        cycles-=23;
                        break;
                        case 0xCD: case 0x1CD: case 0x2CD: case 0x3CD: /*INT*/
                        /*if (msw&1) pclog("INT %i %i %i\n",cr0&1,eflags&VM_FLAG,IOPL);*/
                        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
                        lastpc=pc;
                        lastcs=CS;
                        temp=readmemb(cs,pc); pc++;
                        intrt:
                                
//                        if (temp == 0x15 && AX == 0xc203) output = 3;
//                        /*if (temp == 0x13) */pclog("INT %02X %04X %04X %04X %04X:%04X %04X:%04X %c %i %i\n",temp,AX,BX,CX,DS,DX,CS,pc,(AL>31)?AL:' ', ins, ins2);
                        if (1)
                        {
                                if (msw&1)
                                {
//                                        pclog("PMODE int %02X %04X at %04X:%04X  ",temp,AX,CS,pc);
                                        pmodeint(temp,1);
                                        cycles-=(is486)?44:59;
//                                        pclog("to %04X:%04X\n",CS,pc);
                                }
                                else
                                {
                                        if (ssegs) ss=oldss;
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
                                        addr=temp<<2;
//                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
//                                        pclog("%04X:%04X : ",CS,pc);
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                        cycles-=(is486)?30:37;
//                                        pclog("INT %02X - %04X %04X:%04X\n",temp,addr,CS,pc);
                                }
                        }
                        break;
                        case 0xCE: /*INTO*/
                        if (flags&V_FLAG)
                        {
                                temp=4;
                                goto intrt;
                        }
                        cycles-=3;
                        break;
                        case 0xCF: case 0x2CF: /*IRET*/
//                        if (CS == 0xc000) output = 0;
//                        pclog("IRET\n");
                        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
//                        output=0;
                        if (ssegs) ss=oldss;
                        if (msw&1)
                        {
                                optype=IRET;
                                pmodeiret(0);
                                optype=0;
                        }
                        else
                        {
                                tempw=CS;
                                tempw2=pc;
                                inint=0;
                                oxpc=pc;
                                if (stack32)
                                {
                                        pc=readmemw(ss,ESP);
                                        loadcs(readmemw(ss,ESP+2));
                                }
                                else
                                {
                                        pc=readmemw(ss,SP);
                                        loadcs(readmemw(ss,((SP+2)&0xFFFF)));
                                }
                                if (stack32)
                                {
                                        flags=(readmemw(ss,ESP+4)&0xFFD5)|2;
                                        ESP+=6;
                                }
                                else
                                {
                                        flags=(readmemw(ss,((SP+4)&0xFFFF))&0xFFD5)|2;
                                        SP+=6;
                                }
                        }
                        cycles-=(is486)?15:22;
                        break;
                        case 0x1CF: case 0x3CF: /*IRETD*/
//                        if (output==3) output=1;
//                        pclog("IRET\n");
                        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
                        {
                                x86gpf(NULL,0);
                                break;
                        }
//                        output=0;
                        if (ssegs) ss=oldss;
                        if (msw&1)
                        {
                                optype=IRET;
                                pmodeiret(1);
                                optype=0;
                        }
                        else
                        {
                                tempw=CS;
                                tempw2=pc;
                                inint=0;
                                oxpc=pc;
                                if (stack32)
                                {
                                        pc=readmeml(ss,ESP);
                                        templ=readmeml(ss,ESP+4);
                                }
                                else
                                {
                                        pc=readmeml(ss,SP);
                                        templ=readmeml(ss,((SP+4)&0xFFFF));
                                }
                                if (stack32)
                                {
                                        flags=(readmemw(ss,ESP+8)&0xFFD5)|2;
                                        eflags=readmemw(ss,ESP+10);
                                        ESP+=12;
                                }
                                else
                                {
                                        flags=(readmemw(ss,(SP+8)&0xFFFF)&0xFFD5)|2;
                                        eflags=readmemw(ss,(SP+10)&0xFFFF);
                                        SP+=12;
                                }
                                loadcs(templ);
                        }
                        cycles-=(is486)?15:22;
                        break;
                        
                        case 0xD0: case 0x1D0: case 0x2D0: case 0x3D0:
                        fetchea();
                        temp=geteab(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL b,1*/
                                seteab((temp<<1)|((temp&0x80)?1:0)); if (abrt) break;
                                if (temp&0x80) flags|=C_FLAG;
                                else           flags&=~C_FLAG;
                                temp<<=1;
                                if (flags&C_FLAG) temp|=1;
                                if ((flags&C_FLAG)^(temp>>7)) flags|=V_FLAG;
                                else                          flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR b,1*/
                                seteab((temp>>1)|((temp&1)?0x80:0)); if (abrt) break;
                                if (temp&1) flags|=C_FLAG;
                                else        flags&=~C_FLAG;
                                temp>>=1;
                                if (flags&C_FLAG) temp|=0x80;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                else                       flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL b,1*/
                                temp2=flags&C_FLAG;
                                seteab((temp<<1)|temp2); if (abrt) break;
                                if (temp&0x80) flags|=C_FLAG;
                                else           flags&=~C_FLAG;
                                temp<<=1;
                                if (temp2) temp|=1;
                                if ((flags&C_FLAG)^(temp>>7)) flags|=V_FLAG;
                                else                          flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x18: /*RCR b,1*/
                                temp2=flags&C_FLAG;
                                seteab((temp>>1)|(temp2?0x80:0)); if (abrt) break;
                                if (temp&1) flags|=C_FLAG;
                                else        flags&=~C_FLAG;
                                temp>>=1;
                                if (temp2) temp|=0x80;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                else                       flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x20: case 0x30: /*SHL b,1*/
                                seteab(temp<<1); if (abrt) break;
                                setznp8(temp<<1);
                                if (temp&0x80) flags|=C_FLAG;
                                if ((temp^(temp<<1))&0x80) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x28: /*SHR b,1*/
                                seteab(temp>>1); if (abrt) break;
                                setznp8(temp>>1);
                                if (temp&1) flags|=C_FLAG;
                                if (temp&0x80) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x38: /*SAR b,1*/
                                seteab((temp>>1)|(temp&0x80)); if (abrt) break;
                                setznp8((temp>>1)|(temp&0x80));
                                if (temp&1) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;
                        case 0xD1: case 0x2D1:
                        fetchea();
                        tempw=geteaw(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL w,1*/
                                seteaw((tempw<<1)|(tempw>>15)); if (abrt) break;
                                if (tempw&0x8000) flags|=C_FLAG;
                                else              flags&=~C_FLAG;
                                tempw<<=1;
                                if (flags&C_FLAG) tempw|=1;
                                if ((flags&C_FLAG)^(tempw>>15)) flags|=V_FLAG;
                                else                            flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR w,1*/
                                seteaw((tempw>>1)|(tempw<<15)); if (abrt) break;
                                if (tempw&1) flags|=C_FLAG;
                                else         flags&=~C_FLAG;
                                tempw>>=1;
                                if (flags&C_FLAG) tempw|=0x8000;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                else                           flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL w,1*/
                                temp2=flags&C_FLAG;
                                seteaw((tempw<<1)|temp2);       if (abrt) break;
                                if (tempw&0x8000) flags|=C_FLAG;
                                else              flags&=~C_FLAG;
                                tempw<<=1;
                                if (temp2) tempw|=1;
                                if ((flags&C_FLAG)^(tempw>>15)) flags|=V_FLAG;
                                else                            flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x18: /*RCR w,1*/
                                temp2=flags&C_FLAG;
                                seteaw((tempw>>1)|(temp2?0x8000:0));      if (abrt) break;
                                if (tempw&1) flags|=C_FLAG;
                                else         flags&=~C_FLAG;
                                tempw>>=1;
                                if (temp2) tempw|=0x8000;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                else                           flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x20: case 0x30: /*SHL w,1*/
                                seteaw(tempw<<1);       if (abrt) break;
                                setznp16(tempw<<1);
                                if (tempw&0x8000) flags|=C_FLAG;
                                if ((tempw^(tempw<<1))&0x8000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x28: /*SHR w,1*/
                                seteaw(tempw>>1);       if (abrt) break;
                                setznp16(tempw>>1);
                                if (tempw&1) flags|=C_FLAG;
                                if (tempw&0x8000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x38: /*SAR w,1*/
                                seteaw((tempw>>1)|(tempw&0x8000)); if (abrt) break;
                                setznp16((tempw>>1)|(tempw&0x8000));
                                if (tempw&1) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                default:
                                pclog("Bad D1 opcode %02X\n",rmdat&0x38);
                        }
                        break;
                        case 0x1D1: case 0x3D1:
                        fetchea();
                        templ=geteal();         if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL l,1*/
                                seteal((templ<<1)|(templ>>31)); if (abrt) break;
                                if (templ&0x80000000) flags|=C_FLAG;
                                else                  flags&=~C_FLAG;
                                templ<<=1;
                                if (flags&C_FLAG) templ|=1;
                                if ((flags&C_FLAG)^(templ>>31)) flags|=V_FLAG;
                                else                            flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR l,1*/
                                seteal((templ>>1)|(templ<<31)); if (abrt) break;
                                if (templ&1) flags|=C_FLAG;
                                else         flags&=~C_FLAG;
                                templ>>=1;
                                if (flags&C_FLAG) templ|=0x80000000;
                                if ((templ^(templ>>1))&0x40000000) flags|=V_FLAG;
                                else                               flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL l,1*/
                                temp2=flags&C_FLAG;
                                seteal((templ<<1)|temp2);       if (abrt) break;
                                if (templ&0x80000000) flags|=C_FLAG;
                                else                  flags&=~C_FLAG;
                                templ<<=1;
                                if (temp2) templ|=1;
                                if ((flags&C_FLAG)^(templ>>31)) flags|=V_FLAG;
                                else                            flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x18: /*RCR l,1*/
                                temp2=flags&C_FLAG;
                                seteal((templ>>1)|(temp2?0x80000000:0)); if (abrt) break;
                                temp2=flags&C_FLAG;
                                if (templ&1) flags|=C_FLAG;
                                else         flags&=~C_FLAG;
                                templ>>=1;
                                if (temp2) templ|=0x80000000;
                                if ((templ^(templ>>1))&0x40000000) flags|=V_FLAG;
                                else                               flags&=~V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x20: case 0x30: /*SHL l,1*/
                                seteal(templ<<1); if (abrt) break;
                                setznp32(templ<<1);
                                if (templ&0x80000000) flags|=C_FLAG;
                                if ((templ^(templ<<1))&0x80000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x28: /*SHR l,1*/
                                seteal(templ>>1); if (abrt) break;
                                setznp32(templ>>1);
                                if (templ&1) flags|=C_FLAG;
                                if (templ&0x80000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x38: /*SAR l,1*/
                                seteal((templ>>1)|(templ&0x80000000)); if (abrt) break;
                                setznp32((templ>>1)|(templ&0x80000000));
                                if (templ&1) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                default:
                                pclog("Bad D1 opcode %02X\n",rmdat&0x38);
                        }
                        break;

                        case 0xD2: case 0x1D2: case 0x2D2: case 0x3D2:
                        fetchea();
                        temp=geteab();  if (abrt) break;
                        c=CL&31;
//                        cycles-=c;
                        if (!c) break;
//                        if (c>7) pclog("Shiftb %i %02X\n",rmdat&0x38,c);
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL b,CL*/
                                while (c>0)
                                {
                                        temp2=(temp&0x80)?1:0;
                                        temp=(temp<<1)|temp2;
                                        c--;
                                }
                                seteab(temp); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(temp>>7)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR b,CL*/
                                while (c>0)
                                {
                                        temp2=temp&1;
                                        temp>>=1;
                                        if (temp2) temp|=0x80;
                                        c--;
                                }
                                seteab(temp); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp2) flags|=C_FLAG;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL b,CL*/
                                tempc=flags&C_FLAG;
                                while (c>0)
                                {
                                        templ=tempc;
                                        tempc=temp&0x80;
                                        temp<<=1;
                                        if (templ) temp|=1;
                                        c--;
                                }
                                seteab(temp); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (tempc) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(temp>>7)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x18: /*RCR b,CL*/
                                tempc=flags&C_FLAG;
                                while (c>0)
                                {
                                        templ=tempc;
                                        tempc=temp&1;
                                        temp>>=1;
                                        if (templ) temp|=0x80;
                                        c--;
                                }
                                seteab(temp); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (tempc) flags|=C_FLAG;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x20: case 0x30: /*SHL b,CL*/
                                seteab(temp<<c); if (abrt) break;
                                setznp8(temp<<c);
                                if ((temp<<(c-1))&0x80) flags|=C_FLAG;
                                if (((temp<<c)^(temp<<(c-1)))&0x80) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x28: /*SHR b,CL*/
                                seteab(temp>>c); if (abrt) break;
                                setznp8(temp>>c);
                                if ((temp>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && temp&0x80) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x38: /*SAR b,CL*/
                                tempc=(temp>>(c-1))&1;
                                while (c>0)
                                {
                                        temp>>=1;
                                        if (temp&0x40) temp|=0x80;
                                        c--;
                                }
                                seteab(temp); if (abrt) break;
                                setznp8(temp);
                                if (tempc) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;

                        case 0xD3: case 0x2D3:
                        fetchea();
                        tempw=geteaw(); if (abrt) break;
                        c=CL&31;
                        if (!c) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL w,CL*/
                                while (c>0)
                                {
                                        temp=(tempw&0x8000)?1:0;
                                        tempw=(tempw<<1)|temp;
                                        c--;
                                }
                                seteaw(tempw); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(tempw>>15)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR w,CL*/
                                while (c>0)
                                {
                                        tempw2=(tempw&1)?0x8000:0;
                                        tempw=(tempw>>1)|tempw2;
                                        c--;
                                }
                                seteaw(tempw); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (tempw2) flags|=C_FLAG;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL w,CL*/
                                tempc=flags&C_FLAG;
                                while (c>0)
                                {
                                        templ=tempc;
                                        tempc=tempw&0x8000;
                                        tempw=(tempw<<1)|templ;
                                        c--;
                                }
                                seteaw(tempw); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (tempc) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(tempw>>15)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x18: /*RCR w,CL*/
                                tempc=flags&C_FLAG;
                                while (c>0)
                                {
                                        templ=tempc;
                                        tempw2=(templ&1)?0x8000:0;
                                        tempc=tempw&1;
                                        tempw=(tempw>>1)|tempw2;
                                        c--;
                                }
                                seteaw(tempw); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (tempc) flags|=C_FLAG;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x20: case 0x30: /*SHL w,CL*/
                                seteaw(tempw<<c); if (abrt) break;
                                setznp16(tempw<<c);
                                if ((tempw<<(c-1))&0x8000) flags|=C_FLAG;
                                if (((tempw<<c)^(tempw<<(c-1)))&0x8000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x28:            /*SHR w,CL*/
                                seteaw(tempw>>c); if (abrt) break;
                                setznp16(tempw>>c);
                                if ((tempw>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && tempw&0x8000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x38:            /*SAR w,CL*/
                                tempw2=tempw&0x8000;
                                tempc=((int16_t)tempw>>(c-1))&1;
                                while (c>0)
                                {
                                        tempw=(tempw>>1)|tempw2;
                                        c--;
                                }
                                seteaw(tempw); if (abrt) break;
                                setznp16(tempw);
                                if (tempc) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;
                        case 0x1D3: case 0x3D3:
                        fetchea();
                        templ=geteal(); if (abrt) break;
                        c=CL&31;
                        if (!c) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ROL l,CL*/
                                while (c>0)
                                {
                                        temp=(templ&0x80000000)?1:0;
                                        templ=(templ<<1)|temp;
                                        c--;
                                }
                                seteal(templ); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (temp) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(templ>>31)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x08: /*ROR l,CL*/
                                while (c>0)
                                {
                                        templ2=(templ&1)?0x80000000:0;
                                        templ=(templ>>1)|templ2;
                                        c--;
                                }
                                seteal(templ); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (templ2) flags|=C_FLAG;
                                if ((templ^(templ>>1))&0x40000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x10: /*RCL l,CL*/
                                tempc=flags&C_FLAG;
                                while (c>0)
                                {
                                        templ2=tempc;
                                        tempc=(templ>>31);
                                        templ=(templ<<1)|templ2;
                                        c--;
                                }
                                seteal(templ); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (templ2) flags|=C_FLAG;
                                if ((flags&C_FLAG)^(templ>>31)) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                                case 0x18: /*RCR l,CL*/
                                tempc=flags&C_FLAG;
                                while (c>0)
                                {
                                        templ2=(tempc)?0x80000000:0;
                                        tempc=templ&1;
                                        templ=(templ>>1)|templ2;
                                        c--;
                                }
                                seteal(templ); if (abrt) break;
                                flags&=~(C_FLAG|V_FLAG);
                                if (templ2) flags|=C_FLAG;
                                if ((templ^(templ>>1))&0x40000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x20: case 0x30: /*SHL l,CL*/
                                seteal(templ<<c); if (abrt) break;
                                setznp32(templ<<c);
                                if ((templ<<(c-1))&0x80000000) flags|=C_FLAG;
                                if (((templ<<c)^(templ<<(c-1)))&0x80000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x28:            /*SHR l,CL*/
                                seteal(templ>>c); if (abrt) break;
                                setznp32(templ>>c);
                                if ((templ>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && templ&0x80000000) flags|=V_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;

                                case 0x38:            /*SAR w,CL*/
                                templ2=templ&0x80000000;
                                tempc=(templ>>(c-1))&1;
                                while (c>0)
                                {
                                        templ=(templ>>1)|templ2;
                                        c--;
                                }
                                seteal(templ); if (abrt) break;
                                setznp32(templ);
                                if (tempc) flags|=C_FLAG;
                                cycles-=((mod==3)?3:7);
                                break;
                        }
                        break;


                        case 0xD4: case 0x1D4: case 0x2D4: case 0x3D4: /*AAM*/
                        tempws=readmemb(cs,pc); pc++;
                        if (!tempws || cpu_manufacturer != MANU_INTEL) tempws = 10;
                        AH=AL/tempws;
                        AL%=tempws;
                        setznp16(AX);
                        cycles-=(is486)?15:17;
                        break;
                        case 0xD5: case 0x1D5: case 0x2D5: case 0x3D5: /*AAD*/
                        tempws=readmemb(cs,pc); pc++;
                        if (cpu_manufacturer != MANU_INTEL) tempws = 10;
                        AL=(AH*tempws)+AL;
                        AH=0;
                        setznp16(AX);
                        cycles-=(is486)?14:19;
                        break;
                        case 0xD6: case 0x1D6: case 0x2D6: case 0x3D6: /*SETALC*/
                        AL=(flags&C_FLAG)?0xFF:0;
                        cycles -= timing_rr;
                        break;
                        case 0xD7: case 0x1D7: /*XLAT*/
                        addr=(BX+AL)&0xFFFF;
                        temp=readmemb(ds,addr); if (abrt) break;
                        AL=temp;
                        cycles-=5;
                        break;
                        case 0x2D7: case 0x3D7: /*XLAT*/
                        addr=EBX+AL;
                        temp=readmemb(ds,addr); if (abrt) break;
                        AL=temp;
                        cycles-=5;
                        break;
                        case 0xD9: case 0xDA: case 0xDB: case 0xDD:     /*ESCAPE*/
                        case 0x1D9: case 0x1DA: case 0x1DB: case 0x1DD: /*ESCAPE*/
                        case 0x2D9: case 0x2DA: case 0x2DB: case 0x2DD: /*ESCAPE*/
                        case 0x3D9: case 0x3DA: case 0x3DB: case 0x3DD: /*ESCAPE*/
                        case 0xD8: case 0x1D8: case 0x2D8: case 0x3D8:
                        case 0xDC: case 0x1DC: case 0x2DC: case 0x3DC:
                        case 0xDE: case 0x1DE: case 0x2DE: case 0x3DE:
                        case 0xDF: case 0x1DF: case 0x2DF: case 0x3DF:
                        if ((cr0&6)==4)
                        {
                                pc=oldpc;
                                pmodeint(7,0);
                                cycles-=59;
                        }
                        else
                        {
                                fpucount++;
                                fetchea();
                                if (hasfpu)
                                {
                                        x87_pc_off=oldpc;
                                        x87_pc_seg=CS;
                                        x87_op_off=eaaddr;
                                        x87_op_seg=ea_rseg;
                                        switch (opcode)
                                        {
                                                case 0xD8: x87_d8(); break;
                                                case 0xD9: x87_d9(); break;
                                                case 0xDA: x87_da(); break;
                                                case 0xDB: x87_db(); break;
                                                case 0xDC: x87_dc(); break;
                                                case 0xDD: x87_dd(); break;
                                                case 0xDE: x87_de(); break;
                                                case 0xDF: x87_df(); break;
                                        }
                                }
                                //cycles-=8;
                        }
                        break;

                        case 0xE0: case 0x1E0: /*LOOPNE*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        CX--;
                        if (CX && !(flags&Z_FLAG)) { pc+=offset; }
                        cycles-=(is486)?7:11;
                        break;
                        case 0x2E0: case 0x3E0: /*LOOPNE*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        ECX--;
                        if (ECX && !(flags&Z_FLAG)) { pc+=offset; }
                        cycles-=(is486)?7:11;
                        break;
                        case 0xE1: case 0x1E1: /*LOOPE*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        CX--;
                        if (CX && (flags&Z_FLAG)) { pc+=offset; }
                        cycles-=(is486)?7:11;
                        break;
                        case 0x2E1: case 0x3E1: /*LOOPE*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        ECX--;
                        if (ECX && (flags&Z_FLAG)) { pc+=offset; }
                        cycles-=(is486)?7:11;
                        break;
                        case 0xE2: case 0x1E2: /*LOOP*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        CX--;
                        if (CX) { pc+=offset; }
                        cycles-=(is486)?7:11;
                        break;
                        case 0x2E2: case 0x3E2: /*LOOP*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        ECX--;
                        if (ECX) { pc+=offset; }
                        cycles-=(is486)?7:11;
                        break;
                        case 0xE3: case 0x1E3: /*JCXZ*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        if (!CX) { pc+=offset; cycles-=4; }
                        cycles-=5;
                        break;
                        case 0x2E3: case 0x3E3: /*JECXZ*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        if (!ECX) { pc+=offset; cycles-=4; }
                        cycles-=5;
                        break;

                        case 0xE4: case 0x1E4: case 0x2E4: case 0x3E4: /*IN AL*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        pc++;
                        AL=inb(temp);
                        cycles-=12;
                        break;
                        case 0xE5: case 0x2E5: /*IN AX*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        checkio_perm(temp+1);
                        pc++;
                        AX=inw(temp);
                        cycles-=12;
                        break;
                        case 0x1E5: case 0x3E5: /*IN EAX*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        checkio_perm(temp+1);
                        checkio_perm(temp+2);
                        checkio_perm(temp+3);
                        pc++;
                        EAX=inl(temp);
                        cycles-=12;
                        break;
                        case 0xE6: case 0x1E6: case 0x2E6: case 0x3E6: /*OUT AL*/
                        temp=readmemb(cs,pc);
//                        if (output) pclog("OUT %02X,%02X - %08X\n",temp,AL,writelookup2);
                        checkio_perm(temp);
  //                      if (output) pclog("              - %08X\n",writelookup2);
                        pc++;
                        outb(temp,AL);
//                        if (output) pclog("              - %08X\n",writelookup2);
                        cycles-=10;
                        break;
                        case 0xE7: case 0x2E7: /*OUT AX*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        checkio_perm(temp+1);
                        pc++;
                        outw(temp,AX);
                        cycles-=10;
                        break;
                        case 0x1E7: case 0x3E7: /*OUT EAX*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        checkio_perm(temp+1);
                        checkio_perm(temp+2);
                        checkio_perm(temp+3);
                        pc++;
                        outl(temp,EAX);
                        cycles-=10;
                        break;

                        case 0xE8: /*CALL rel 16*/
                        tempw=getword(); if (abrt) break;
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememw(ss,ESP-2,pc); if (abrt) break;
                                ESP-=2;
                        }
                        else
                        {
                                writememw(ss,((SP-2)&0xFFFF),pc); if (abrt) break;
                                SP-=2;
                        }
                        pc+=(int16_t)tempw;
                        cycles-=(is486)?3:7;
                        break;
                        case 0x3E8: /*CALL rel 16*/
                        templ=getlong(); if (abrt) break;
                        if (ssegs) ss=oldss;
                        if (stack32)
                        {
                                writememl(ss,ESP-4,pc); if (abrt) break;
                                ESP-=4;
                        }
                        else
                        {
                                writememl(ss,((SP-4)&0xFFFF),pc); if (abrt) break;
                                SP-=4;
                        }
                        pc+=templ;
                        cycles-=(is486)?3:7;
                        break;
                        case 0xE9: case 0x2E9: /*JMP rel 16*/
                        tempw=getword(); if (abrt) break;
                        pc+=(int16_t)tempw;
                        cycles-=(is486)?3:7;
                        break;
                        case 0x1E9: case 0x3E9: /*JMP rel 32*/
                        templ=getlong(); if (abrt) break;
                        pc+=templ;
                        cycles-=(is486)?3:7;
                        break;
                        case 0xEA: case 0x2EA: /*JMP far*/
                        addr=getword();
                        tempw=getword(); if (abrt) { if (output==3) pclog("JMP ABRT\n"); break; }
                        oxpc=pc;
                        pc=addr;
                        loadcsjmp(tempw,oxpc);
                        cycles-=(is486)?17:12;
                        break;
                        case 0x1EA: case 0x3EA: /*JMP far*/
                        templ=getlong();
                        tempw=getword(); if (abrt) break;
                        oxpc=pc;
                        pc=templ;
                        loadcsjmp(tempw,oxpc);
                        cycles-=(is486)?17:12;
                        break;
                        case 0xEB: case 0x1EB: case 0x2EB: case 0x3EB: /*JMP rel*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        pc+=offset;
                        cycles-=(is486)?3:7;
                        break;
                        case 0xEC: case 0x1EC: case 0x2EC: case 0x3EC: /*IN AL,DX*/
                        checkio_perm(DX);
                        AL=inb(DX);
                        cycles-=13;
                        break;
                        case 0xED: case 0x2ED: /*IN AX,DX*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        AX=inw(DX);
                        cycles-=13;
                        break;
                        case 0x1ED: case 0x3ED: /*IN EAX,DX*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        checkio_perm(DX+2);
                        checkio_perm(DX+3);
                        EAX=inl(DX);
                        cycles-=13;
                        break;
                        case 0xEE: case 0x1EE: case 0x2EE: case 0x3EE: /*OUT DX,AL*/
                        checkio_perm(DX);
                        outb(DX,AL);
                        cycles-=11;
                        break;
                        case 0xEF: case 0x2EF: /*OUT DX,AX*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        outw(DX,AX);
                        cycles-=11;
                        break;
                        case 0x1EF: case 0x3EF: /*OUT DX,EAX*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        checkio_perm(DX+2);
                        checkio_perm(DX+3);
                        outl(DX,EAX);
                        cycles-=11;
                        break;

                        case 0xF0: case 0x1F0: case 0x2F0: case 0x3F0: /*LOCK*/
                        cycles-=4;
                        break;

                        case 0xF2: case 0x1F2: case 0x2F2: case 0x3F2: /*REPNE*/
                        rep386(0);
                        break;
                        case 0xF3: case 0x1F3: case 0x2F3: case 0x3F3: /*REPE*/
                        rep386(1);
                        break;

                        case 0xF4: case 0x1F4: case 0x2F4: case 0x3F4: /*HLT*/
                                if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
                                {
//                                        pclog("Can't HLT\n");
                                        x86gpf(NULL,0);
                                        //output=3;
                                        break;
                                }
                        inhlt=1;
                        pc--;
                        if (!((flags&I_FLAG) && (((pic.pend&~pic.mask)&~pic.mask2) || ((pic2.pend&~pic2.mask)&~pic2.mask2)) && !ssegs && !noint)) cycles = oldcyc - 100;
                        else cycles-=5;
                        break;
                        case 0xF5: case 0x1F5: case 0x2F5: case 0x3F5: /*CMC*/
                        flags^=C_FLAG;
                        cycles-=2;
                        break;

                        case 0xF6: case 0x1F6: case 0x2F6: case 0x3F6:
                        fetchea();
                        temp=geteab(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*TEST b,#8*/
                                temp2=readmemb(cs,pc); pc++; if (abrt) break;
//                                pclog("TEST %02X,%02X\n",temp,temp2);
                                temp&=temp2;
                                setznp8(temp);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:5);
                                break;
                                case 0x10: /*NOT b*/
                                temp=~temp;
                                seteab(temp);
                                cycles -= (mod == 3) ? timing_rr : timing_mm;
                                break;
                                case 0x18: /*NEG b*/
                                setsub8(0,temp);
                                temp=0-temp;
                                seteab(temp);
                                cycles -= (mod == 3) ? timing_rr : timing_mm;
                                break;
                                case 0x20: /*MUL AL,b*/
//                                setznp8(AL);
                                AX=AL*temp;
//                                if (AX) flags&=~Z_FLAG;
//                                else    flags|=Z_FLAG;
                                if (AH) flags|=(C_FLAG|V_FLAG);
                                else    flags&=~(C_FLAG|V_FLAG);
                                cycles-=13;
                                break;
                                case 0x28: /*IMUL AL,b*/
//                                setznp8(AL);
                                tempws=(int)((int8_t)AL)*(int)((int8_t)temp);
                                AX=tempws&0xFFFF;
//                                if (AX) flags&=~Z_FLAG;
//                                else    flags|=Z_FLAG;
                                if (AH && AH!=0xFF) flags|=(C_FLAG|V_FLAG);
                                else                flags&=~(C_FLAG|V_FLAG);
                                cycles-=14;
                                break;
                                case 0x30: /*DIV AL,b*/
                                tempw=AX;
                                if (temp) tempw2=tempw/temp;
//                                pclog("DIV %04X/%02X %04X:%04X\n",tempw,temp,CS,pc);
                                if (temp && !(tempw2&0xFF00))
                                {
                                        tempw2=tempw%temp;
                                        AH=tempw2;
                                        tempw/=temp;
                                        AL=tempw&0xFF;
                                        if (!cpu_iscyrix) flags|=0x8D5; /*Not a Cyrix*/
                                }
                                else
                                {
//                                        fatal("DIVb BY 0\n");
                                        pc=oldpc;
                                        if (msw&1) pmodeint(0,0);
                                        else
                                        {
//                                        pclog("%04X:%04X\n",cs>>4,pc);
                                                writememw(ss,(SP-2)&0xFFFF,flags);
                                                writememw(ss,(SP-4)&0xFFFF,CS);
                                                writememw(ss,(SP-6)&0xFFFF,pc);
                                                SP-=6;
                                                flags&=~I_FLAG;
                                                oxpc=pc;
                                                pc=readmemw(0,0);
                                                loadcs(readmemw(0,2));
                                        }
//                                                cs=loadcs(CS);
//                                                cs=CS<<4;
                                }
                                cycles-=(is486)?16:14;
                                break;
                                case 0x38: /*IDIV AL,b*/
//                                pclog("IDIV %04X/%02X\n",tempw,temp);
                                tempws=(int)(int16_t)AX;
                                if (temp!=0) tempws2=tempws/(int)((int8_t)temp);
                                temps=tempws2&0xFF;
                                if ((temp!=0) && ((int)temps==tempws2))
                                {
                                        tempw2=tempws%(int)((int8_t)temp);
                                        AH=tempw2&0xFF;
                                        AL=tempws2&0xFF;
                                        if (!cpu_iscyrix) flags|=0x8D5; /*Not a Cyrix*/
                                }
                                else
                                {
                                        pclog("IDIVb exception - %X / %08X = %X\n",tempws,temp,tempws2);
//                                        pclog("IDIVb BY 0 %04X:%04X\n",cs>>4,pc);
                                        pc=oldpc;
                                        if (msw&1) pmodeint(0,0);
                                        else
                                        {
                                                writememw(ss,(SP-2)&0xFFFF,flags);
                                                writememw(ss,(SP-4)&0xFFFF,CS);
                                                writememw(ss,(SP-6)&0xFFFF,pc);
                                                SP-=6;
                                                flags&=~I_FLAG;
                                                oxpc=pc;
                                                pc=readmemw(0,0);
                                                loadcs(readmemw(0,2));
                                        }
//                                                cs=loadcs(CS);
//                                                cs=CS<<4;
//                                        pclog("Div by zero %04X:%04X %02X %02X\n",cs>>4,pc,0xf6,0x38);
                                }
                                cycles-=19;
                                break;

                                default:
                                pclog("Bad F6 opcode %02X\n",rmdat&0x38);
                                x86illegal();
                        }
                        break;

                        case 0xF7: case 0x2F7:
                        fetchea();
                        tempw=geteaw(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*TEST w*/
                                tempw2=getword(); if (abrt) break;
//                                if (output==3) pclog("TEST %04X %04X\n",tempw,tempw2);
                                setznp16(tempw&tempw2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:5);
                                break;
                                case 0x10: /*NOT w*/
                                seteaw(~tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mm;
                                break;
                                case 0x18: /*NEG w*/
                                setsub16(0,tempw);
                                tempw=0-tempw;
                                seteaw(tempw);
                                cycles -= (mod == 3) ? timing_rr : timing_mm;
                                break;
                                case 0x20: /*MUL AX,w*/
//                                setznp16(AX);
                                templ=AX*tempw;
                                AX=templ&0xFFFF;
                                DX=templ>>16;
//                                if (AX|DX) flags&=~Z_FLAG;
//                                else       flags|=Z_FLAG;
                                if (DX)    flags|=(C_FLAG|V_FLAG);
                                else       flags&=~(C_FLAG|V_FLAG);
                                cycles-=21;
                                break;
                                case 0x28: /*IMUL AX,w*/
//                                setznp16(AX);
//                                pclog("IMUL %i %i ",(int)((int16_t)AX),(int)((int16_t)tempw));
                                templ=(int)((int16_t)AX)*(int)((int16_t)tempw);
//                                pclog("%i ",tempws);
                                AX=templ&0xFFFF;
                                DX=templ>>16;
                                if (DX && DX!=0xFFFF) flags|=(C_FLAG|V_FLAG);
                                else                  flags&=~(C_FLAG|V_FLAG);
                                cycles-=22;
                                break;
                                case 0x30: /*DIV AX,w*/
                                templ=(DX<<16)|AX;
                                if (tempw) templ2=templ/tempw;
                                if (tempw && !(templ2&0xFFFF0000))
                                {
                                        tempw2=templ%tempw;
                                        DX=tempw2;
                                        templ/=tempw;
                                        AX=templ&0xFFFF;
                                        if (!cpu_iscyrix) setznp16(AX); /*Not a Cyrix*/
                                }
                                else
                                {
//                                        fatal("DIVw BY 0 %04X:%04X %i\n",cs>>4,pc,ins);
                                        pc=oldpc;
                                        if (msw&1) pmodeint(0,0);
                                        else
                                        {
//                                        pclog("%04X:%04X\n",cs>>4,pc);
                                                writememw(ss,(SP-2)&0xFFFF,flags);
                                                writememw(ss,(SP-4)&0xFFFF,CS);
                                                writememw(ss,(SP-6)&0xFFFF,pc);
                                                SP-=6;
                                                flags&=~I_FLAG;
                                                oxpc=pc;
                                                pc=readmemw(0,0);
                                                loadcs(readmemw(0,2));
                                        }
//                                                cs=loadcs(CS);
//                                                cs=CS<<4;
//                                        pclog("Div by zero %04X:%04X %02X %02X 1\n",cs>>4,pc,0xf7,0x30);
                                }
                                cycles-=(is486)?24:22;
                                break;
                                case 0x38: /*IDIV AX,w*/
                                tempws=(int)((DX<<16)|AX);
                                if (tempw!=0) tempws2=tempws/(int)((int16_t)tempw);
                                temps16=tempws2&0xFFFF;
//                                pclog("IDIV %i %i ",tempws,tempw);
                                if ((tempw!=0) && ((int)temps16==tempws2))
                                {
                                        DX=tempws%(int)((int16_t)tempw);
                                        AX=tempws2&0xFFFF;
                                        if (!cpu_iscyrix) setznp16(AX); /*Not a Cyrix*/
                                }
                                else
                                {
                                        pclog("IDIVw exception - %X / %08X = %X\n",tempws,tempw,tempws2);
//                                        pclog("IDIVw BY 0 %04X:%04X\n",cs>>4,pc);
//                                        DX=0;
//                                        AX=0xFFFF;
                                        pc=oldpc;
                                        if (msw&1) pmodeint(0,0);
                                        else
                                        {
//                                        pclog("%04X:%04X\n",cs>>4,pc);
                                                writememw(ss,(SP-2)&0xFFFF,flags);
                                                writememw(ss,(SP-4)&0xFFFF,CS);
                                                writememw(ss,(SP-6)&0xFFFF,pc);
                                                SP-=6;
                                                flags&=~I_FLAG;
                                                oxpc=pc;
                                                pc=readmemw(0,0);
                                                loadcs(readmemw(0,2));
                                        }
//                                                cs=loadcs(CS);
//                                                cs=CS<<4;
//                                        pclog("Div by zero %04X:%04X %02X %02X 1\n",cs>>4,pc,0xf7,0x38);
                                }
                                cycles-=27;
                                break;

                                default:
                                pclog("Bad F7 opcode %02X\n",rmdat&0x38);
                                x86illegal();
                        }
                        break;
                        case 0x1F7: case 0x3F7:
                        fetchea();
                        templ=geteal(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*TEST l*/
                                templ2=getlong(); if (abrt) break;
                                setznp32(templ&templ2);
                                if (is486) cycles-=((mod==3)?1:2);
                                else       cycles-=((mod==3)?2:5);
                                break;
                                case 0x10: /*NOT l*/
                                seteal(~templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mml;
                                break;
                                case 0x18: /*NEG l*/
                                setsub32(0,templ);
                                templ=0-templ;
                                seteal(templ);
                                cycles -= (mod == 3) ? timing_rr : timing_mml;
                                break;
                                case 0x20: /*MUL EAX,l*/
                                temp64=(uint64_t)EAX*(uint64_t)templ;
                                EAX=temp64&0xFFFFFFFF;
                                EDX=temp64>>32;
                                if (EDX) flags|= (C_FLAG|V_FLAG);
                                else     flags&=~(C_FLAG|V_FLAG);
                                cycles-=21;
                                break;
                                case 0x28: /*IMUL EAX,l*/
                                temp64=(int64_t)(int32_t)EAX*(int64_t)(int32_t)templ;
                                EAX=temp64&0xFFFFFFFF;
                                EDX=temp64>>32;
                                if (EDX && EDX!=0xFFFFFFFF) flags|=(C_FLAG|V_FLAG);
                                else                        flags&=~(C_FLAG|V_FLAG);
                                cycles-=38;
                                break;
                                case 0x30: /*DIV EAX,l*/
                                divl(templ);
                                if (!cpu_iscyrix) setznp32(EAX); /*Not a Cyrix*/
                                cycles-=(is486)?40:38;
                                break;
                                case 0x38: /*IDIV EAX,l*/
                                idivl((int32_t)templ);
                                if (!cpu_iscyrix) setznp32(EAX); /*Not a Cyrix*/
                                cycles-=43;
                                break;

                                default:
                                pclog("Bad F7 opcode %02X\n",rmdat&0x38);
                                x86illegal();
                        }
                        break;

                        case 0xF8: case 0x1F8: case 0x2F8: case 0x3F8: /*CLC*/
                        flags&=~C_FLAG;
                        cycles-=2;
                        break;
                        case 0xF9: case 0x1F9: case 0x2F9: case 0x3F9: /*STC*/
//                        pclog("STC %04X\n",pc);
                        flags|=C_FLAG;
                        cycles-=2;
                        break;
                        case 0xFA: case 0x1FA: case 0x2FA: case 0x3FA: /*CLI*/
                        if (!IOPLp)
                        {
//                                output=3;
//                                pclog("Can't CLI %i %i %04X:%08X\n",IOPL,CPL,CS,pc);
//                                if (CS==0x1C7 && pc==0x5BE) output=3;
                                x86gpf(NULL,0);
                        }
                        else
                           flags&=~I_FLAG;
//                        pclog("CLI at %04X:%04X\n",cs>>4,pc);
                        cycles-=3;
                        break;
                        case 0xFB: case 0x1FB: case 0x2FB: case 0x3FB: /*STI*/
                        if (!IOPLp)
                        {
//                                pclog("Can't STI %04X:%08X\n",CS,pc);
//                                output=3;
                                x86gpf(NULL,0);
                        }
                        else
                           flags|=I_FLAG;
//                        pclog("STI at %04X:%04X\n",cs>>4,pc);
                        cycles-=2;
                        break;
                        case 0xFC: case 0x1FC: case 0x2FC: case 0x3FC: /*CLD*/
                        flags&=~D_FLAG;
                        cycles-=2;
                        break;
                        case 0xFD: case 0x1FD: case 0x2FD: case 0x3FD: /*STD*/
                        flags|=D_FLAG;
                        cycles-=2;
                        break;

                        case 0xFE: case 0x1FE: case 0x2FE: case 0x3FE: /*INC/DEC b*/
                        fetchea();
                        temp=geteab(); if (abrt) break;
                        if (rmdat&0x38)
                        {
                                seteab(temp-1); if (abrt) break;
                                flags&=~V_FLAG;
                                setsub8nc(temp,1);
                                temp2=temp-1;
                                if ((temp&0x80) && !(temp2&0x80)) flags|=V_FLAG;
                        }
                        else
                        {
                                seteab(temp+1); if (abrt) break;
                                flags&=~V_FLAG;
                                setadd8nc(temp,1);
                                temp2=temp+1;
                                if ((temp2&0x80) && !(temp&0x80)) flags|=V_FLAG;
                        }
                        cycles -= (mod == 3) ? timing_rr : timing_mm;
                        break;

                        case 0xFF: case 0x2FF:
                        fetchea();
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*INC w*/
                                tempw=geteaw();  if (abrt) break;
                                seteaw(tempw+1); if (abrt) break;
                                setadd16nc(tempw,1);
                                cycles -= (mod == 3) ? timing_rr : timing_mm;
                                break;
                                case 0x08: /*DEC w*/
                                tempw=geteaw();  if (abrt) break;
                                seteaw(tempw-1); if (abrt) break;
                                setsub16nc(tempw,1);
                                cycles -= (mod == 3) ? timing_rr : timing_mm;
                                break;
                                case 0x10: /*CALL*/
                                tempw=geteaw();
                                if (output) pclog("CALL %04X  %08X:%08X\n", tempw, easeg, eaaddr);
                                if (abrt) break;
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememw(ss,ESP-2,pc); if (abrt) break;
                                        ESP-=2;
                                }
                                else
                                {
                                        writememw(ss,(SP-2)&0xFFFF,pc); if (abrt) break;
                                        SP-=2;
                                }
                                pc=tempw;
                                if (is486) cycles-=5;
                                else       cycles-=((mod==3)?7:10);
                                break;
                                case 0x18: /*CALL far*/
                                tempw=readmemw(easeg,eaaddr);
                                tempw2=readmemw(easeg,(eaaddr+2)); if (output==3) pclog("CALL FAR %04X:%04X\n",tempw,tempw2); if (abrt) break;
                                tempw3=CS;
                                templ2=pc;
                                if (ssegs) ss=oldss;
                                oxpc=pc;
                                pc=tempw;
                                optype=CALL;
                                cgate32=0;
                                if (msw&1) loadcscall(tempw2);
                                else       loadcs(tempw2);
                                optype=0;
                                if (abrt) break;
                                oldss=ss;
                                if (cgate32) goto writecall32_2;
                                writecall16_2:
                                if (stack32)
                                {
                                        writememw(ss,ESP-2,tempw3);
                                        writememw(ss,ESP-4,templ2);
                                        ESP-=4;
                                }
                                else
                                {
                                        writememw(ss,(SP-2)&0xFFFF,tempw3);
                                        writememw(ss,((SP-4)&0xFFFF),templ2);
                                        SP-=4;
                                }
                                cycles-=(is486)?17:22;
                                break;
                                case 0x20: /*JMP*/
                                tempw=geteaw(); if (abrt) break;
                                pc=tempw;
                                if (is486) cycles-=5;
                                else       cycles-=((mod==3)?7:10);
                                break;
                                case 0x28: /*JMP far*/
                                oxpc=pc;
                                tempw=readmemw(easeg,eaaddr);
                                tempw2=readmemw(easeg,eaaddr+2); if (abrt) break;
                                pc=tempw;                                
                                loadcsjmp(tempw2,oxpc); if (abrt) break;
                                cycles-=(is486)?13:12;
                                break;
                                case 0x30: /*PUSH w*/
                                tempw=geteaw(); if (abrt) break;
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememw(ss,ESP-2,tempw); if (abrt) break;
                                        ESP-=2;
                                }
                                else
                                {
//                                        if (output) pclog("PUSH %04X to %04X\n",tempw,SP-2);
                                        writememw(ss,((SP-2)&0xFFFF),tempw); if (abrt) break;
                                        SP-=2;
                                }
                                cycles-=((mod==3)?2:5);
                                break;

                                default:
                                pclog("Bad FF opcode %02X\n",rmdat&0x38);
                                x86illegal();
                        }
                        break;
                        case 0x1FF: case 0x3FF:
                        fetchea();
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*INC l*/
                                templ=geteal();  if (abrt) break;
                                seteal(templ+1); if (abrt) break;
                                setadd32nc(templ,1);
                                cycles -= (mod == 3) ? timing_rr : timing_mml;
                                break;
                                case 0x08: /*DEC l*/
                                templ=geteal();  if (abrt) break;
                                seteal(templ-1); if (abrt) break;
                                setsub32nc(templ,1);
                                cycles -= (mod == 3) ? timing_rr : timing_mml;
                                break;
                                case 0x10: /*CALL*/
                                templ=geteal(); if (abrt) break;
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememl(ss,ESP-4,pc); if (abrt) break;
                                        ESP-=4;
                                }
                                else
                                {
                                        writememl(ss,(SP-4)&0xFFFF,pc); if (abrt) break;
                                        SP-=4;
                                }
                                pc=templ;
                                if (pc==0xFFFFFFFF) pclog("Failed CALL!\n");
                                if (is486) cycles-=5;
                                else       cycles-=((mod==3)?7:10);
                                break;
                                case 0x18: /*CALL far*/
                                templ=readmeml(easeg,eaaddr);
                                tempw2=readmemw(easeg,(eaaddr+4)); if (abrt) break;
                                tempw3=CS;
                                templ2=pc;
                                if (ssegs) ss=oldss;
                                oxpc=pc;
                                pc=templ;
                                optype=CALL;
                                cgate16=0;
                                if (msw&1) loadcscall(tempw2);
                                else       loadcs(tempw2);
                                optype=0;
                                if (abrt) break;
                                oldss=ss;
                                if (cgate16) goto writecall16_2;
                                writecall32_2:
                                if (stack32)
                                {
                                        writememl(ss,ESP-4,tempw3);
                                        writememl(ss,ESP-8,templ2);
                                        ESP-=8;
                                }
                                else
                                {
                                        writememl(ss,(SP-4)&0xFFFF,tempw3);
                                        writememl(ss,(SP-8)&0xFFFF,templ2);
                                        SP-=8;
                                }
                                if (pc==0xFFFFFFFF) pclog("Failed CALL far!\n");
                                cycles-=(is486)?17:22;
                                break;
                                case 0x20: /*JMP*/
                                templ=geteal(); if (abrt) break;
                                pc=templ;
                                if (is486) cycles-=5;
                                else       cycles-=((mod==3)?7:12);
                                if (pc==0xFFFFFFFF) pclog("Failed JMP!\n");
                                break;
                                case 0x28: /*JMP far*/
                                oxpc=pc;
                                templ=readmeml(easeg,eaaddr);
                                templ2=readmeml(easeg,eaaddr+4); if (abrt) break;
                                pc=templ;                                
                                loadcsjmp(templ2,oxpc);
                                if (pc==0xFFFFFFFF) pclog("Failed JMP far!\n");
                                cycles-=(is486)?13:12;
                                break;
                                case 0x30: /*PUSH l*/
                                templ=geteal(); if (abrt) break;
                                if (ssegs) ss=oldss;
                                if (stack32)
                                {
                                        writememl(ss,ESP-4,templ); if (abrt) break;
                                        ESP-=4;
                                }
                                else
                                {
                                        writememl(ss,((SP-4)&0xFFFF),templ); if (abrt) break;
                                        SP-=4;
                                }
                                cycles-=((mod==3)?2:5);
                                break;

                                default:
                                pclog("Bad 32-bit FF opcode %02X\n",rmdat&0x38);
                                x86illegal();
                        }
                        break;

                        default:
//                        pc--;
//                        cycles-=8;
//                        break;
                        pclog("Bad opcode %02X %i %03X at %04X:%04X from %04X:%04X %08X\n",opcode,op32>>8,opcode|op32,cs>>4,pc,old8>>16,old8&0xFFFF,old82);
                        x86illegal();
                }
                opcodeend:
                if (!use32) pc&=0xFFFF;

                if (ssegs)
                {
                        ds=oldds;
                        ss=oldss;
                        rds=DS;
                        ssegs=0;
                }
                if (abrt)
                {

//                        if (CS == 0x228) pclog("Abort at %04X:%04X - %i %i %i\n",CS,pc,notpresent,nullseg,abrt);
/*                        if (testr[0]!=EAX) pclog("EAX corrupted %08X\n",pc);
                        if (testr[1]!=EBX) pclog("EBX corrupted %08X\n",pc);
                        if (testr[2]!=ECX) pclog("ECX corrupted %08X\n",pc);
                        if (testr[3]!=EDX) pclog("EDX corrupted %08X\n",pc);
                        if (testr[4]!=ESI) pclog("ESI corrupted %08X\n",pc);
                        if (testr[5]!=EDI) pclog("EDI corrupted %08X\n",pc);
                        if (testr[6]!=EBP) pclog("EBP corrupted %08X\n",pc);
                        if (testr[7]!=ESP) pclog("ESP corrupted %08X\n",pc);
                        if (testr[8]!=flags) pclog("FLAGS corrupted %08X\n",pc);*/
                        tempi = abrt;
                        abrt = 0;
                        x86_doabrt(tempi);
                        if (abrt)
                        {
                                abrt = 0;
                                CS = oldcs;
                                pc = oldpc;
                                pclog("Double fault %i %i\n", ins, resets);
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

                if (trap && (flags&T_FLAG) && !noint)
                {
//                        oldpc=pc;
//                        oldcs=CS;
//                        printf("TRAP!!! %04X:%04X\n",CS,pc);
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
                                addr=1<<2;
                                flags&=~I_FLAG;
                                flags&=~T_FLAG;
                                pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                        }
                }
                else if ((flags&I_FLAG) && (((pic.pend&~pic.mask)&~pic.mask2) || ((pic2.pend&~pic2.mask)&~pic2.mask2)) && !noint)
                {
                        temp=picinterrupt();
                        if (temp!=0xFF)
                        {
//                                if (temp == 0x54) pclog("Take int 54\n");
//                                if (output) output=3;
//                                pclog("Hardware int %02X %i %i %04X(%08X):%08X\n",temp,ins, ins2, CS,cs,pc);
//                                if (temp==0x54) output=3;
                                if (inhlt) pc++;
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
                                        addr=temp<<2;
                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
//                                        if (temp==0x76) pclog("INT to %04X:%04X\n",CS,pc);
                                }
                                inint=1;
                        }
                }
                if (noint) noint=0;
                ins++;
                insc++;
                
/*                if (times && ins == 9000000) output = 3;
                if (CS == 0x1000 && pc == 0x1200)
                   fatal("Hit it\n");*/
//                if (!ins) ins2++;

                }
                
                keybsenddelay -= cycdiff;
                if (keybsenddelay<1)
                {
                        keybsenddelay = 1000;
                        keyboard_at_poll();
                }
                
                pit.c[0]-=cycdiff;
                pit.c[1]-=cycdiff;
                if (ppi.pb&1) pit.c[2]-=cycdiff;
                if ((pit.c[0]<1)||(pit.c[1]<1)||(pit.c[2]<1)) pit_poll();
                spktime-=cycdiff;
                if (spktime<=0.0)
                {
                        spktime+=SPKCONST;
//                        pclog("1Poll spk\n");
                        pollspk();
                        pollgussamp();
                        getsbsamp();
                        polladlib();
                        getdacsamp();
//                        pclog("2Poll spk\n");
                }
                soundtime-=cycdiff;
                if (soundtime<=0.0)
                {
                        soundtime+=SOUNDCONST;
//                        pclog("1Poll sound60hz\n");
                        pollsound60hz();
//                        pclog("2Poll sound60hz\n");
                }
                gustime-=cycdiff;
                while (gustime<=0.0)
                {
                        gustime+=GUSCONST;
                        pollgus();
                }
                gustime2-=cycdiff;
                while (gustime2<=0.0)
                {
                        gustime2+=GUSCONST2;
                        pollgus2();
                }
                vidtime-=cycdiff;
                if (vidtime<=0.0)
                {
//                        pclog("1Poll video\n");
                        pollvideo();
//                        pclog("2Poll video\n");
                }
                if (disctime)
                {
                        disctime-=(cycdiff);
                        if (disctime<=0)
                        {
//                                pclog("1Poll disc\n");
                                disctime=0;
                                fdc_poll();
//                                pclog("2Poll disc\n");
                        }
                }
                if (mousedelay)
                {
                        mousedelay-=20;
                        if (!mousedelay)
                        {
//                                pclog("1Poll mouse\n");
                                mousecallback();
//                                pclog("2Poll disc\n");
                        }
                }
                if (sbenable)
                {
                        sbcount-=cycdiff;
                        if (sbcount<0)
                        {
                                sbcount+=sblatcho;
                                pollsb();
                        }
                }
                if (sb_enable_i)
                {
                        sb_count_i-=cycdiff;
                        if (sb_count_i<0)
                        {
                                sb_count_i+=sblatchi;
                                sb_poll_i();
                        }
                }
                if (idecallback[0])
                {
                        idecallback[0]--;
                        if (idecallback[0]<=0)
                        {
//                                pclog("IDE time over\n");
                                idecallback[0]=0;
                                callbackide(0);
                        }
                }
                if (idecallback[1])
                {
                        idecallback[1]--;
                        if (idecallback[1]<=0)
                        {
//                                pclog("IDE time over\n");
                                idecallback[1]=0;
                                callbackide(1);
                        }
                }
                rtctime-=cycdiff;
                if (rtctime<0)
                {
                        nvr_rtc();
                }
        }
}
