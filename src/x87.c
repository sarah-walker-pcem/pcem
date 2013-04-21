//Quake timedemo demo1 - 8.1FPS

//11A00 - D_SCAlloc
//11C1C - D_CacheSurface

//36174 - SCR_CalcRefdef

//SCR_CalcRefdef
//Calls R_SetVrect and R_ViewChanged

#include <math.h>
#include "ibm.h"
#include "x86.h"
#include "x87.h"

#undef readmemb
#undef writememb

#define readmemb(s,a) ((readlookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF)?readmemb386l(s,a):ram[readlookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)])
#define writememb(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF) writememb386l(s,a,v); else ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]=v

#define writememw(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFE) writememwl(s,a,v); else *((uint16_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v
#define writememl(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFC) writememll(s,a,v); else *((uint32_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v

static inline uint8_t geteab()
{
        if (mod==3)
           return (rm&4)?regs[rm&3].b.h:regs[rm&3].b.l;
//        cycles-=3;
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



double ST[8];
uint16_t npxs,npxc,tag;

int TOP;
#define ST(x) ST[((TOP+(x))&7)]

#define C0 (1<<8)
#define C1 (1<<9)
#define C2 (1<<10)
#define C3 (1<<14)

void x87_reset()
{
}

void x87_dumpregs()
{
        pclog("ST(0)=%f\tST(1)=%f\tST(2)=%f\tST(3)=%f\t\n",ST[TOP],ST[(TOP+1)&7],ST[(TOP+2)&7],ST[(TOP+3)&7]);
        pclog("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t\n",ST[(TOP+4)&7],ST[(TOP+5)&7],ST[(TOP+6)&7],ST[(TOP+7)&7]);
        pclog("Status = %04X  Control = %04X  Tag = %04X\n",npxs,npxc,tag);
}

void x87_print()
{
        pclog("\tST(0)=%.20f\tST(1)=%.20f\tST(2)=%f\tST(3)=%f\t",ST[TOP],ST[(TOP+1)&7],ST[(TOP+2)&7],ST[(TOP+3)&7]);
        pclog("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t\n\n",ST[(TOP+4)&7],ST[(TOP+5)&7],ST[(TOP+6)&7],ST[(TOP+7)&7]);
}

void x87_push(double i)
{
        TOP=(TOP-1)&7;
        ST[TOP]=i;
        tag&=~(3<<((TOP&7)<<1));
        if (i==0.0) tag|=(1<<((TOP&7)<<1));
}

double x87_pop()
{
        double t=ST[TOP];
        tag|=(3<<((TOP&7)<<1));
        TOP=(TOP+1)&7;
        return t;
}

int64_t x87_fround(double b)
{
        switch ((npxc>>10)&3)
        {
                case 0: /*Nearest*/
                return (int64_t)(b+0.5);
                case 1: /*Down*/
                return (int64_t)floor(b);
                case 2: /*Up*/
                return (int64_t)ceil(b);
                case 3: /*Chop*/
                return (int64_t)b;
        }
}
#define BIAS80 16383
#define BIAS64 1023

double x87_ld80()
{
	struct {
		int16_t begin;
		union
		{
                        double d;
                        uint64_t ll;
                } eind;
	} test;
	test.eind.ll = readmeml(easeg,eaaddr);
	test.eind.ll |= (uint64_t)readmeml(easeg,eaaddr+4)<<32;
	test.begin = readmemw(easeg,eaaddr+8);

	int64_t exp64 = (((test.begin&0x7fff) - BIAS80));
	int64_t blah = ((exp64 >0)?exp64:-exp64)&0x3ff;
	int64_t exp64final = ((exp64 >0)?blah:-blah) +BIAS64;

	int64_t mant64 = (test.eind.ll >> 11) & (0xfffffffffffff);
	int64_t sign = (test.begin&0x8000)?1:0;

        if (test.eind.ll&0x400) mant64++;
//        pclog("LD80 %08X %08X\n",test.eind.ll,mant64);
	test.eind.ll = (sign <<63)|(exp64final << 52)| mant64;
	return test.eind.d;
}

void x87_st80(double d)
{
	struct {
		int16_t begin;
		union
		{
                        double d;
                        uint64_t ll;
                } eind;
	} test;
	test.eind.d=d;
	int64_t sign80 = (test.eind.ll&(0x8000000000000000))?1:0;
	int64_t exp80 =  test.eind.ll&(0x7ff0000000000000);
	int64_t exp80final = (exp80>>52);
	int64_t mant80 = test.eind.ll&(0x000fffffffffffff);
	int64_t mant80final = (mant80 << 11);
	if(d != 0){ //Zero is a special case
		// Elvira wants the 8 and tcalc doesn't
		mant80final |= (0x8000000000000000);
		//Ca-cyber doesn't like this when result is zero.
		exp80final += (BIAS80 - BIAS64);
	}
	test.begin = (((int16_t)sign80)<<15)| (int16_t)exp80final;
	test.eind.ll = mant80final;
	writememl(easeg,eaaddr,test.eind.ll);
	writememl(easeg,eaaddr+4,test.eind.ll>>32);
	writememw(easeg,eaaddr+8,test.begin);
}

void x87_d8()
{
        union
        {
                float s;
                uint32_t i;
        } ts;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*FADD*/
                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                        ST(0)=ST(0)+ST(rmdat32&7);
                        cycles-=8;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FMUL*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        ST(0)=ST(0)*ST(rmdat32&7);
                        cycles-=16;
                        return;
                        case 0xD0: case 0xD1: case 0xD2: case 0xD3: /*FCOM*/
                        case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        cycles-=4;
                        return;
                        case 0xD8: case 0xD9: case 0xDA: case 0xDB: /*FCOMP*/
                        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        x87_pop();
                        cycles-=4;
                        return;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: /*FSUB*/
                        case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                        ST(0)=ST(0)-ST(rmdat32&7);
                        cycles-=8;
                        return;
                        case 0xE8: case 0xE9: case 0xEA: case 0xEB: /*FSUBR*/
                        case 0xEC: case 0xED: case 0xEE: case 0xEF:
                        ST(0)=ST(rmdat32&7)-ST(0);
                        cycles-=8;
                        return;
                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*FDIV*/
                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                        ST(0)=ST(0)/ST(rmdat32&7);
                        cycles-=73;
                        return;
                        case 0xF8: case 0xF9: case 0xFA: case 0xFB: /*FDIVR*/
                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                        ST(0)=ST(rmdat32&7)/ST(0);
                        cycles-=73;
                        return;
                }
        }
        else
        {
                ts.i=geteal(); if (abrt) return;
                switch (reg)
                {
                        case 0: /*FADD short*/
                        ST(0)+=ts.s;
                        cycles-=8;
                        return;
                        case 1: /*FMUL short*/
                        ST(0)*=ts.s;
                        cycles-=11;
                        return;
                        case 2: /*FCOM short*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ts.s)     npxs|=C3;
                        else if (ST(0)<ts.s) npxs|=C0;
                        cycles-=4;
                        return;
                        case 3: /*FCOMP short*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ts.s)     npxs|=C3;
                        else if (ST(0)<ts.s) npxs|=C0;
                        cycles-=4;
                        x87_pop();
                        return;
                        case 4: /*FSUB short*/
                        ST(0)-=ts.s;
                        cycles-=8;
                        return;
                        case 5: /*FSUBR short*/
                        ST(0)=ts.s-ST(0);
                        cycles-=8;
                        return;
                        case 6: /*FDIV short*/
                        ST(0)=ST(0)/ts.s;
                        cycles-=73;
                        return;
                        case 7: /*FDIVR short*/
                        ST(0)=ts.s/ST(0);
                        cycles-=73;
                        return;
                }
        }
        x86illegal();
//        fatal("Bad x87 D8 opcode %i %02X\n",reg,rmdat32&0xFF);
}
void x87_d9()
{
        union
        {
                float s;
                uint32_t i;
        } ts;
        float t;
        double td;
        int64_t temp64;
        uint16_t tempw;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*FLD*/
                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                        x87_push(ST(rmdat32&7));
                        cycles-=4;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FXCH*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        td=ST(0);
                        ST(0)=ST(rmdat32&7);
                        ST(rmdat32&7)=td;
                        cycles-=4;
                        return;
                        case 0xD0: /*FNOP*/
                        cycles-=3;
                        return;
                        case 0xE0: /*FCHS*/
                        ST(0)=-ST(0);
                        cycles-=6;
                        return;
                        case 0xE1: /*FABS*/
                        ST(0)=fabs(ST(0));
                        cycles-=3;
                        return;
                        case 0xE4: /*FTST*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==0.0)     npxs|=C3;
                        else if (ST(0)<0.0) npxs|=C0;
                        cycles-=4;
                        return;
                        case 0xE5: /*FXAM*/
//                        pclog("FXAM %f %i\n",ST(0),(tag>>((TOP&1)<<1))&3);
                        npxs&=~(C0|C2|C3);
                        if (((tag>>((TOP&1)<<1))&3)==3) npxs|=(C0|C3);
                        else if (ST(0)==0.0)            npxs|=C3;
                        else                            npxs|=C2;
                        if (ST(0)<0.0)                  npxs|=C1;
                        cycles-=8;
                        return;
                        case 0xE8: /*FLD1*/
                        x87_push(1.0);
                        cycles-=4;
                        return;
                        case 0xE9: /*FLDL2T*/
                        x87_push(3.3219280948873623);
                        cycles-=8;
                        return;
                        case 0xEA: /*FLDL2E*/
                        x87_push(1.4426950408889634);
                        cycles-=8;
                        return;
                        case 0xEB: /*FLDPI*/
                        x87_push(3.141592653589793);
                        cycles-=8;
                        return;
                        case 0xEC: /*FLDEG2*/
                        x87_push(0.3010299956639812);
                        cycles-=8;
                        return;
                        case 0xED: /*FLDLN2*/
                        x87_push(0.693147180559945);
                        cycles-=8;
                        return;
                        case 0xEE: /*FLDZ*/
//                        pclog("FLDZ %04X:%08X\n",CS,pc);
                        x87_push(0.0);
                        tag|=(1<<((TOP&7)<<1));
                        cycles-=4;
                        return;
                        case 0xF0: /*F2XM1*/
                        ST(0)=pow(2.0,ST(0))-1.0;
                        cycles-=200;
                        return;
                        case 0xF1: /*FYL2X*/
                        ST(1)=ST(1)*(log(ST(0))/log(2.0));
                        x87_pop();
                        cycles-=250;
                        return;
                        case 0xF2: /*FPTAN*/
//                        pclog("Tan of %f = ",ST(0));
                        ST(0)=tan(ST(0));
//                        pclog("%f\n",ST(0));
                        x87_push(1.0);
                        npxs&=~C2;
                        cycles-=235;
                        return;
                        case 0xF3: /*FPATAN*/
//                        pclog("FPATAN %08X\n",pc);
                        ST(1)=atan2(ST(1),ST(0));
                        x87_pop();
                        cycles-=250;
                        return;
                        case 0xF6: /*FDECSTP*/
                        TOP=(TOP-1)&7;
                        return;
                        case 0xF7: /*FINCSTP*/
                        TOP=(TOP+1)&7;
                        return;
                        case 0xF8: /*FPREM*/
                        temp64=(int64_t)(ST(0)/ST(1));
                        ST(0)=ST(0)-(ST(1)*(double)temp64);
                        npxs&=~(C0|C1|C2|C3);
                        if (temp64&4) npxs|=C0;
                        if (temp64&2) npxs|=C3;
                        if (temp64&1) npxs|=C1;
                        cycles-=100;
                        return;
                        case 0xFA: /*FSQRT*/
                        ST(0)=sqrt(ST(0));
                        cycles-=83;
                        return;
                        case 0xFB: /*FSINCOS*/
                        td=ST(0);
                        ST(0)=sin(td);
                        x87_push(cos(td));
                        npxs&=~C2;
                        cycles-=330;
                        return;
                        case 0xFC: /*FRNDINT*/
//                        pclog("FRNDINT %f %i ",ST(0),(npxc>>10)&3);
                        ST(0)=(double)x87_fround(ST(0));
//                        pclog("%f\n",ST(0));
                        cycles-=21;
                        return;
                        case 0xFD: /*FSCALE*/
                        temp64=(int64_t)ST(1);
                        ST(0)=ST(0)*pow(2.0,(double)temp64);
                        cycles-=30;
                        return;
                        case 0xFE: /*FSIN*/
                        ST(0)=sin(ST(0));
                        npxs&=~C2;
                        cycles-=300;
                        return;
                        case 0xFF: /*FCOS*/
                        ST(0)=cos(ST(0));
                        npxs&=~C2;
                        cycles-=300;
                        return;
                }
        }
        else
        {
                switch (reg)
                {
                        case 0: /*FLD single-precision*/
//                        if ((rmdat32&0xFFFF)==0xD445) { pclog("FLDS\n"); output=3; dumpregs(); exit(-1); }
                        ts.i=(float)geteal(); if (abrt) return;
                        x87_push((double)ts.s);
                        cycles-=3;
                        return;
                        case 2: /*FST single-precision*/
                        ts.s=(float)ST(0);
                        seteal(ts.i);
                        cycles-=7;
                        return;
                        case 3: /*FSTP single-precision*/
                        ts.s=(float)ST(0);
                        seteal(ts.i); if (abrt) return;
  //                      if (pc==0x3f50c) pclog("FSTP %f %f %08X\n",ST(0),ts.s,ts.i);
                        x87_pop();
                        cycles-=7;
                        return;
                        case 4: /*FLDENV*/
                        switch ((cr0&1)|(op32&0x100))
                        {
                                case 0x000: /*16-bit real mode*/
                                case 0x001: /*16-bit protected mode*/
                                npxc=readmemw(easeg,eaaddr);
                                npxs=readmemw(easeg,eaaddr+2);
                                tag=readmemw(easeg,eaaddr+4);
                                TOP=(npxs>>11)&7;
                                break;
                                case 0x100: /*32-bit real mode*/
                                case 0x101: /*32-bit protected mode*/
                                npxc=readmemw(easeg,eaaddr);
                                npxs=readmemw(easeg,eaaddr+4);
                                tag=readmemw(easeg,eaaddr+8);
                                TOP=(npxs>>11)&7;
                                break;
                        }
                        cycles-=(cr0&1)?34:44;
                        return;
                        case 5: /*FLDCW*/
                        tempw=geteaw();
                        if (abrt) return;
                        npxc=tempw;
                        cycles-=4;
                        return;
                        case 6: /*FSTENV*/
                        switch ((cr0&1)|(op32&0x100))
                        {
                                case 0x000: /*16-bit real mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+2,npxs);
                                writememw(easeg,eaaddr+4,tag);
                                writememw(easeg,eaaddr+6,x87_pc_off);
                                writememw(easeg,eaaddr+10,x87_op_off);
                                break;
                                case 0x001: /*16-bit protected mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+2,npxs);
                                writememw(easeg,eaaddr+4,tag);
                                writememw(easeg,eaaddr+6,x87_pc_off);
                                writememw(easeg,eaaddr+8,x87_pc_seg);
                                writememw(easeg,eaaddr+10,x87_op_off);
                                writememw(easeg,eaaddr+12,x87_op_seg);
                                break;
                                case 0x100: /*32-bit real mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+4,npxs);
                                writememw(easeg,eaaddr+8,tag);
                                writememw(easeg,eaaddr+12,x87_pc_off);
                                writememw(easeg,eaaddr+20,x87_op_off);
                                writememl(easeg,eaaddr+24,(x87_op_off>>16)<<12);
                                break;
                                case 0x101: /*32-bit protected mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+4,npxs);
                                writememw(easeg,eaaddr+8,tag);
                                writememl(easeg,eaaddr+12,x87_pc_off);
                                writememl(easeg,eaaddr+16,x87_pc_seg);
                                writememl(easeg,eaaddr+20,x87_op_off);
                                writememl(easeg,eaaddr+24,x87_op_seg);
                                break;
                        }
                        cycles-=(cr0&1)?56:67;
                        return;
                        case 7: /*FSTCW*/
                        seteaw(npxc);
                        cycles-=3;
                        return;
                }
        }
//        fatal("Bad x87 D9 opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
void x87_da()
{
        int32_t templ;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xE9: /*FUCOMPP*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(1))     npxs|=C3;
                        else if (ST(0)<ST(1)) npxs|=C0;
                        x87_pop();
                        x87_pop();
                        cycles-=5;
                        return;
                }
        }
        else
        {
                templ=geteal(); if (abrt) return;
                switch (reg)
                {
                        case 0: /*FIADD*/
                        ST(0)=ST(0)+(double)templ;
                        cycles-=20;
                        return;
                        case 1: /*FIMUL*/
                        ST(0)=ST(0)*(double)templ;
                        cycles-=22;
                        return;
                        case 2: /*FICOM*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        cycles-=15;
                        return;
                        case 3: /*FICOMP*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        x87_pop();
                        cycles-=15;
                        return;
                        case 4: /*FISUB*/
                        ST(0)=ST(0)-(double)templ;
                        cycles-=20;
                        return;
                        case 5: /*FISUBR*/
                        ST(0)=(double)templ-ST(0);
                        cycles-=19;
                        return;
                        case 6: /*FIDIV*/
                        ST(0)=ST(0)/(double)templ;
                        cycles-=84;
                        return;
                        case 7: /*FIDIVR*/
                        ST(0)=(double)templ/ST(0);
                        cycles-=84;
                        return;
                }
        }
//        fatal("Bad x87 DA opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
void x87_db()
{
        double t;
        int32_t templ;
        if (mod==3)
        {
                switch (reg)
                {
                        case 4:
                        switch (rmdat32&0xFF)
                        {
                                case 0xE1:
                                return;
                                case 0xE2: /*FCLEX*/
                                npxs&=0xFF00;
                                return;
                                case 0xE3: /*FINIT*/
                                npxc=0x37F;
                                npxs=0;
                                tag=0xFFFF;
                                TOP=0;
                                cycles-=17;
                                return;
                                case 0xE4:
                                return;
                        }
                        break;
                }
        }
        else
        {
                switch (reg)
                {
                        case 0: /*FILD short*/
                        templ=geteal(); if (abrt) return;
                        x87_push((double)templ);
                        cycles-=9;
                        return;
                        case 2: /*FIST short*/
                        seteal((int32_t)x87_fround(ST(0)));
                        cycles-=28;
                        return;
                        case 3: /*FISTP short*/
                        seteal((int32_t)x87_fround(ST(0))); if (abrt) return;
                        x87_pop();
                        cycles-=28;
                        return;
                        case 5: /*FLD extended*/
                        t=x87_ld80(); if (abrt) return;
                        x87_push(t);
                        cycles-=6;
                        return;
                        case 7: /*FSTP extended*/
                        x87_st80(ST(0)); if (abrt) return;
                        x87_pop();
                        cycles-=6;
                        return;
                }
        }
//        fatal("Bad x87 DB opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
void x87_dc()
{
        union
        {
                double d;
                uint64_t i;
        } t;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*FADD*/
                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                        ST(rmdat32&7)=ST(rmdat32&7)+ST(0);
                        cycles-=8;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FMUL*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        ST(rmdat32&7)=ST(rmdat32&7)*ST(0);
                        cycles-=16;
                        return;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: /*FSUBR*/
                        case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                        ST(rmdat32&7)=ST(0)-ST(rmdat32&7);
                        cycles-=8;
                        return;
                        case 0xE8: case 0xE9: case 0xEA: case 0xEB: /*FSUB*/
                        case 0xEC: case 0xED: case 0xEE: case 0xEF:
                        ST(rmdat32&7)=ST(rmdat32&7)-ST(0);
                        cycles-=8;
                        return;
                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*FDIVR*/
                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                        ST(rmdat32&7)=ST(0)/ST(rmdat32&7);
                        cycles-=73;
                        return;
                        case 0xF8: case 0xF9: case 0xFA: case 0xFB: /*FDIV*/
                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                        ST(rmdat32&7)=ST(rmdat32&7)/ST(0);
                        cycles-=73;
                        return;
                }
        }
        else
        {
                t.i=readmeml(easeg,eaaddr);
                t.i|=(uint64_t)readmeml(easeg,eaaddr+4)<<32;
                if (abrt) return;
                switch (reg)
                {
                        case 0: /*FADD double*/
                        ST(0)+=t.d;
                        cycles-=8;
                        return;
                        case 1: /*FMUL double*/
                        ST(0)*=t.d;
                        cycles-=14;
                        return;
                        case 2: /*FCOM double*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==t.d)     npxs|=C3;
                        else if (ST(0)<t.d) npxs|=C0;
                        cycles-=4;
                        return;
                        case 3: /*FCOMP double*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==t.d)     npxs|=C3;
                        else if (ST(0)<t.d) npxs|=C0;
                        cycles-=4;
                        x87_pop();
                        return;
                        case 4: /*FSUB double*/
                        ST(0)-=t.d;
                        cycles-=8;
                        return;
                        case 5: /*FSUBR double*/
                        ST(0)=t.d-ST(0);
                        cycles-=8;
                        return;
                        case 6: /*FDIV double*/
                        ST(0)=ST(0)/t.d;
                        cycles-=73;
                        return;
                        case 7: /*FDIVR double*/
                        ST(0)=t.d/ST(0);
                        cycles-=73;
                        return;
                }
        }
//        fatal("Bad x87 DC opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
void x87_dd()
{
        union
        {
                double d;
                uint64_t i;
        } t;
        int temp;
        if (mod==3)
        {
                switch (reg)
                {
                        case 0: /*FFREE*/
                        tag|=(3<<((rmdat32&7)<<1));
                        cycles-=3;
                        return;
                        case 2: /*FST*/
                        ST(rmdat32&7)=ST(0);
                        temp=(tag>>((TOP&7)<<1))&3;
                        tag&=~(3<<((rmdat32&7)<<1));
                        tag|=(temp<<((rmdat32&7)<<1));
                        cycles-=3;
                        return;
                        case 3: /*FSTP*/
                        ST(rmdat32&7)=ST(0);
                        temp=(tag>>((TOP&7)<<1))&3;
                        tag&=~(3<<((rmdat32&7)<<1));
                        tag|=(temp<<((rmdat32&7)<<1));
                        x87_pop();
                        cycles-=3;
                        return;
                        case 4: /*FUCOM*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        cycles-=4;
                        return;
                        case 5: /*FUCOMP*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        x87_pop();
                        cycles-=4;
                        return;
                }
        }
        else
        {
                switch (reg)
                {
                        case 0: /*FLD double-precision*/
                        t.i=readmeml(easeg,eaaddr);
                        t.i|=(uint64_t)readmeml(easeg,eaaddr+4)<<32; if (abrt) return;
                        x87_push(t.d);
                        cycles-=3;
                        return;
                        case 2: /*FST double-precision*/
                        t.d=ST(0);
                        writememl(easeg,eaaddr,t.i);
                        writememl(easeg,eaaddr+4,(t.i>>32));
                        cycles-=8;
                        return;
                        case 3: /*FSTP double-precision*/
                        t.d=ST(0);
                        writememl(easeg,eaaddr,t.i);
                        writememl(easeg,eaaddr+4,(t.i>>32)); if (abrt) return;
                        x87_pop();
                        cycles-=8;
                        return;
                        case 4: /*FRSTOR*/
                        switch ((cr0&1)|(op32&0x100))
                        {
                                case 0x000: /*16-bit real mode*/
                                case 0x001: /*16-bit protected mode*/
                                npxc=readmemw(easeg,eaaddr);
                                npxs=readmemw(easeg,eaaddr+2);
                                tag=readmemw(easeg,eaaddr+4);
                                TOP=(npxs>>11)&7;
                                eaaddr+=14;
                                break;
                                case 0x100: /*32-bit real mode*/
                                case 0x101: /*32-bit protected mode*/
                                npxc=readmemw(easeg,eaaddr);
                                npxs=readmemw(easeg,eaaddr+4);
                                tag=readmemw(easeg,eaaddr+8);
                                TOP=(npxs>>11)&7;
                                eaaddr+=28;
                                break;
                        }
                        ST(0)=x87_ld80(); eaaddr+=10;
                        ST(1)=x87_ld80(); eaaddr+=10;
                        ST(2)=x87_ld80(); eaaddr+=10;
                        ST(3)=x87_ld80(); eaaddr+=10;
                        ST(4)=x87_ld80(); eaaddr+=10;
                        ST(5)=x87_ld80(); eaaddr+=10;
                        ST(6)=x87_ld80(); eaaddr+=10;
                        ST(7)=x87_ld80();
                        cycles-=(cr0&1)?34:44;
                        return;
                        case 6: /*FSAVE*/
                        switch ((cr0&1)|(op32&0x100))
                        {
                                case 0x000: /*16-bit real mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+2,npxs);
                                writememw(easeg,eaaddr+4,tag);
                                writememw(easeg,eaaddr+6,x87_pc_off);
                                writememw(easeg,eaaddr+10,x87_op_off);
                                eaaddr+=14;
                                x87_st80(ST(0)); eaaddr+=10;
                                x87_st80(ST(1)); eaaddr+=10;
                                x87_st80(ST(2)); eaaddr+=10;
                                x87_st80(ST(3)); eaaddr+=10;
                                x87_st80(ST(4)); eaaddr+=10;
                                x87_st80(ST(5)); eaaddr+=10;
                                x87_st80(ST(6)); eaaddr+=10;
                                x87_st80(ST(7));
                                break;
                                case 0x001: /*16-bit protected mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+2,npxs);
                                writememw(easeg,eaaddr+4,tag);
                                writememw(easeg,eaaddr+6,x87_pc_off);
                                writememw(easeg,eaaddr+8,x87_pc_seg);
                                writememw(easeg,eaaddr+10,x87_op_off);
                                writememw(easeg,eaaddr+12,x87_op_seg);
                                eaaddr+=14;
                                x87_st80(ST(0)); eaaddr+=10;
                                x87_st80(ST(1)); eaaddr+=10;
                                x87_st80(ST(2)); eaaddr+=10;
                                x87_st80(ST(3)); eaaddr+=10;
                                x87_st80(ST(4)); eaaddr+=10;
                                x87_st80(ST(5)); eaaddr+=10;
                                x87_st80(ST(6)); eaaddr+=10;
                                x87_st80(ST(7));
                                break;
                                case 0x100: /*32-bit real mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+4,npxs);
                                writememw(easeg,eaaddr+8,tag);
                                writememw(easeg,eaaddr+12,x87_pc_off);
                                writememw(easeg,eaaddr+20,x87_op_off);
                                writememl(easeg,eaaddr+24,(x87_op_off>>16)<<12);
                                eaaddr+=28;
                                x87_st80(ST(0)); eaaddr+=10;
                                x87_st80(ST(1)); eaaddr+=10;
                                x87_st80(ST(2)); eaaddr+=10;
                                x87_st80(ST(3)); eaaddr+=10;
                                x87_st80(ST(4)); eaaddr+=10;
                                x87_st80(ST(5)); eaaddr+=10;
                                x87_st80(ST(6)); eaaddr+=10;
                                x87_st80(ST(7));
                                break;
                                case 0x101: /*32-bit protected mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+4,npxs);
                                writememw(easeg,eaaddr+8,tag);
                                writememl(easeg,eaaddr+12,x87_pc_off);
                                writememl(easeg,eaaddr+16,x87_pc_seg);
                                writememl(easeg,eaaddr+20,x87_op_off);
                                writememl(easeg,eaaddr+24,x87_op_seg);
                                eaaddr+=28;
                                x87_st80(ST(0)); eaaddr+=10;
                                x87_st80(ST(1)); eaaddr+=10;
                                x87_st80(ST(2)); eaaddr+=10;
                                x87_st80(ST(3)); eaaddr+=10;
                                x87_st80(ST(4)); eaaddr+=10;
                                x87_st80(ST(5)); eaaddr+=10;
                                x87_st80(ST(6)); eaaddr+=10;
                                x87_st80(ST(7));
                                break;
                        }
                        cycles-=(cr0&1)?56:67;
                        return;
                        case 7: /*FSTSW*/
                        seteaw((npxs&0xC7FF)|(TOP<<11));
                        cycles-=3;
                        return;
                }
        }
//        fatal("Bad x87 DD opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
void x87_de()
{
        double t;
        int32_t templ;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*FADDP*/
                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                        ST(rmdat32&7)=ST(rmdat32&7)+ST[TOP];
                        x87_pop();
                        cycles-=8;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FMULP*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        ST(rmdat32&7)=ST(rmdat32&7)*ST[TOP];
                        x87_pop();
                        cycles-=16;
                        return;
                        case 0xD9: /*FCOMPP*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(1))     npxs|=C3;
                        else if (ST(0)<ST(1)) npxs|=C0;
                        x87_pop();
                        x87_pop();
                        cycles-=5;
                        return;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: /*FSUBRP*/
                        case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                        ST(rmdat32&7)=ST(0)-ST(rmdat32&7);
                        x87_pop();
                        cycles-=8;
                        return;
                        case 0xE8: case 0xE9: case 0xEA: case 0xEB: /*FSUBP*/
                        case 0xEC: case 0xED: case 0xEE: case 0xEF:
                        ST(rmdat32&7)=ST(rmdat32&7)-ST[TOP];
                        x87_pop();
                        cycles-=8;
                        return;
                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*FDIVRP*/
                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                        ST(rmdat32&7)=ST(0)/ST(rmdat32&7);
                        x87_pop();
                        cycles-=73;
                        return;
                        case 0xF8: case 0xF9: case 0xFA: case 0xFB: /*FDIVP*/
                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                        ST(rmdat32&7)=ST(rmdat32&7)/ST[TOP];
                        x87_pop();
                        cycles-=73;
                        return;
                }
        }
        else
        {
                templ=(int32_t)geteaw(); if (abrt) return;
                switch (reg)
                {
                        case 0: /*FIADD*/
                        ST(0)=ST(0)+(double)templ;
                        cycles-=20;
                        return;
                        case 1: /*FIMUL*/
                        ST(0)=ST(0)*(double)templ;
                        cycles-=22;
                        return;
                        case 2: /*FICOM*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        cycles-=15;
                        return;
                        case 3: /*FICOMP*/
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        x87_pop();
                        cycles-=15;
                        return;
                        case 4: /*FISUB*/
                        ST(0)=ST(0)-(double)templ;
                        cycles-=20;
                        return;
                        case 5: /*FISUBR*/
                        ST(0)=(double)templ-ST(0);
                        cycles-=19;
                        return;
                        case 6: /*FIDIV*/
                        ST(0)=ST(0)/(double)templ;
                        cycles-=84;
                        return;
                        case 7: /*FIDIVR*/
                        ST(0)=(double)templ/ST(0);
                        cycles-=84;
                        return;
                }
        }
//        fatal("Bad x87 DE opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
void x87_df()
{
        int c;
        int64_t temp64;
        int16_t temp16;
        double tempd;
        uint8_t tempc;
        if (mod==3)
        {
                switch (reg)
                {
                        case 4:
                        switch (rmdat32&0xFF)
                        {
                                case 0xE0: /*FSTSW AX*/
                                AX=npxs;
                                cycles-=3;
                                return;
                        }
                        break;
                }
        }
        else
        {
                switch (reg)
                {
                        case 0: /*FILD short*/
                        temp16=geteaw(); if (abrt) return;
                        x87_push((double)temp16);
                        cycles-=13;
                        return;
                        case 2: /*FIST word*/
                        temp16=x87_fround(ST(0));
                        seteaw(temp16);
                        cycles-=29;
                        return;
                        case 3: /*FISTP word*/
                        temp16=x87_fround(ST(0));
                        seteaw(temp16); if (abrt) return;
                        x87_pop();
                        cycles-=29;
                        return;
                        case 5: /*FILD long*/
                        temp64=geteal(); if (abrt) return;
                        temp64|=(uint64_t)readmeml(easeg,eaaddr+4)<<32;
                        x87_push((double)temp64);
                        cycles-=10;
                        return;
                        case 6: /*FBSTP*/
                        tempd=ST(0);
                        for (c=0;c<9;c++)
                        {
                                tempc=(uint8_t)floor(fmod(tempd,10.0));       tempd-=floor(fmod(tempd,10.0)); tempd/=10.0;
                                tempc|=((uint8_t)floor(fmod(tempd,10.0)))<<4; tempd-=floor(fmod(tempd,10.0)); tempd/=10.0;
                                writememb(easeg,eaaddr+c,tempc);
                        }
                        tempc=(uint8_t)floor(fmod(tempd,10.0));
                        if (tempd<0.0) tempc|=0x80;
                        writememb(easeg,eaaddr+9,tempc); if (abrt) return;
                        x87_pop();
                        return;
                        case 7: /*FISTP long*/
                        temp64=x87_fround(ST(0));
                        seteal(temp64);
                        writememl(easeg,eaaddr+4,temp64>>32); if (abrt) return;
                        x87_pop();
                        cycles-=29;
                        return;
                }
        }
//        fatal("Bad x87 DF opcode %i %02X\n",reg,rmdat32&0xFF);
        x86illegal();
}
