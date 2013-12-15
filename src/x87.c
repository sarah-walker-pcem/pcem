//Quake timedemo demo1 - 8.1FPS

//11A00 - D_SCAlloc
//11C1C - D_CacheSurface

//36174 - SCR_CalcRefdef

//SCR_CalcRefdef
//Calls R_SetVrect and R_ViewChanged

#define fplog 0

#include <math.h>
#include "ibm.h"
#include "pic.h"
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
MMX_REG MM[8];
int ismmx = 0;
uint16_t npxs,npxc,tag;

int TOP;
#define ST(x) ST[((TOP+(x))&7)]

#define C0 (1<<8)
#define C1 (1<<9)
#define C2 (1<<10)
#define C3 (1<<14)

#define STATUS_ZERODIVIDE 4

#define x87_div(dst, src1, src2) do                             \
        {                                                       \
                if (((double)src2) == 0.0)                      \
                {                                               \
                        npxs |= STATUS_ZERODIVIDE;              \
                        if (npxc & STATUS_ZERODIVIDE)           \
                                dst = src1 / (double)src2;      \
                        else                                    \
                        {                                       \
                                pclog("FPU : divide by zero\n"); \
                                picint(1 << 13);                \
                        }                                       \
                        return;                                 \
                }                                               \
                else                                            \
                        dst = src1 / (double)src2;              \
        } while (0)
        
void x87_set_mmx()
{
        TOP = 0;
        tag = 0;
        ismmx = 1;
}

void x87_emms()
{
        tag = 0xffff;
        ismmx = 0;
}

void x87_checkexceptions()
{
}

void x87_reset()
{
}

void x87_dumpregs()
{
        if (ismmx)
        {
                pclog("MM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\n", MM[0].q, MM[1].q, MM[2].q, MM[3].q);
                pclog("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", MM[4].q, MM[5].q, MM[6].q, MM[7].q);
        }
        else
        {
                pclog("ST(0)=%f\tST(1)=%f\tST(2)=%f\tST(3)=%f\t\n",ST[TOP],ST[(TOP+1)&7],ST[(TOP+2)&7],ST[(TOP+3)&7]);
                pclog("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t\n",ST[(TOP+4)&7],ST[(TOP+5)&7],ST[(TOP+6)&7],ST[(TOP+7)&7]);
        }
        pclog("Status = %04X  Control = %04X  Tag = %04X\n",npxs,npxc,tag);
}

void x87_print()
{
        if (ismmx)
        {
                pclog("\tMM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\t", MM[0].q, MM[1].q, MM[2].q, MM[3].q);
                pclog("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", MM[4].q, MM[5].q, MM[6].q, MM[7].q);
        }
        else
        {
                pclog("\tST(0)=%.20f\tST(1)=%.20f\tST(2)=%f\tST(3)=%f\t",ST[TOP&7],ST[(TOP+1)&7],ST[(TOP+2)&7],ST[(TOP+3)&7]);
                pclog("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t TOP=%i CR=%04X SR=%04X TAG=%04X\n",ST[(TOP+4)&7],ST[(TOP+5)&7],ST[(TOP+6)&7],ST[(TOP+7)&7], TOP, npxc, npxs, tag);
        }
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

        if ((test.begin & 0x7fff) == 0x7fff)
                exp64final = 0x7ff;
        if ((test.begin & 0x7fff) == 0)
                exp64final = 0;
        if (test.eind.ll & 0x400) 
                mant64++;

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

       	if (exp80final == 0x7ff) /*Infinity / Nan*/
       	{
                exp80final = 0x7fff;
                mant80final |= (0x8000000000000000);
        }
       	else if (d != 0){ //Zero is a special case
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

void x87_ldmmx(MMX_REG *r)
{
        r->l[0] = readmeml(easeg, eaaddr);
        r->l[1] = readmeml(easeg, eaaddr + 4);
        r->w[4] = readmemw(easeg, eaaddr + 8);
}

void x87_stmmx(MMX_REG r)
{
        writememl(easeg, eaaddr,     r.l[0]);
        writememl(easeg, eaaddr + 4, r.l[1]);
        writememw(easeg, eaaddr + 8, 0xffff);
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
                        if (fplog) pclog("FADD\n");
                        ST(0)=ST(0)+ST(rmdat32&7);
                        cycles-=8;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FMUL*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        if (fplog) pclog("FMUL\n");
                        ST(0)=ST(0)*ST(rmdat32&7);
                        cycles-=16;
                        return;
                        case 0xD0: case 0xD1: case 0xD2: case 0xD3: /*FCOM*/
                        case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                        if (fplog) pclog("FCOM\n");
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        cycles-=4;
                        return;
                        case 0xD8: case 0xD9: case 0xDA: case 0xDB: /*FCOMP*/
                        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                        if (fplog) pclog("FCOMP\n");
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        x87_pop();
                        cycles-=4;
                        return;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: /*FSUB*/
                        case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                        if (fplog) pclog("FSUB\n");
                        ST(0)=ST(0)-ST(rmdat32&7);
                        cycles-=8;
                        return;
                        case 0xE8: case 0xE9: case 0xEA: case 0xEB: /*FSUBR*/
                        case 0xEC: case 0xED: case 0xEE: case 0xEF:
                        if (fplog) pclog("FSUBR\n");
                        ST(0)=ST(rmdat32&7)-ST(0);
                        cycles-=8;
                        return;
                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*FDIV*/
                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                        if (fplog) pclog("FDIV\n");
                        x87_div(ST(0), ST(0), ST(rmdat32&7));
                        cycles-=73;
                        return;
                        case 0xF8: case 0xF9: case 0xFA: case 0xFB: /*FDIVR*/
                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                        if (fplog) pclog("FDIVR\n");
                        x87_div(ST(0), ST(rmdat32&7), ST(0));
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
                        if (fplog) pclog("FADDs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        ST(0)+=ts.s;
                        cycles-=8;
                        return;
                        case 1: /*FMUL short*/
                        if (fplog) pclog("FMULs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        ST(0)*=ts.s;
                        cycles-=11;
                        if (abrt) pclog("ABRT FMUL\n");
                        return;
                        case 2: /*FCOM short*/
                        if (fplog) pclog("FCOMs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ts.s)     npxs|=C3;
                        else if (ST(0)<ts.s) npxs|=C0;
                        cycles-=4;
                        return;
                        case 3: /*FCOMP short*/
                        if (fplog) pclog("FCOMPs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ts.s)     npxs|=C3;
                        else if (ST(0)<ts.s) npxs|=C0;
                        cycles-=4;
                        x87_pop();
                        return;
                        case 4: /*FSUB short*/
                        if (fplog) pclog("FSUBs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        ST(0)-=ts.s;
                        cycles-=8;
                        return;
                        case 5: /*FSUBR short*/
                        if (fplog) pclog("FSUBRs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        ST(0)=ts.s-ST(0);
                        cycles-=8;
                        return;
                        case 6: /*FDIV short*/
                        if (fplog) pclog("FDIVs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        x87_div(ST(0), ST(0), ts.s);
                        cycles-=73;
                        return;
                        case 7: /*FDIVR short*/
                        if (fplog) pclog("FDIVRs %08X:%08X %f %f\n", easeg, eaaddr, ST(0), ts.s);
                        x87_div(ST(0), ts.s, ST(0));
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
        double td;
        int64_t temp64;
        uint16_t tempw;
        int temp;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*FLD*/
                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                        if (fplog) pclog("FLD %f\n", ST(rmdat32 & 7));
                        x87_push(ST(rmdat32&7));
                        cycles-=4;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FXCH*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        if (fplog) pclog("FXCH\n");
                        td=ST(0);
                        ST(0)=ST(rmdat32&7);
                        ST(rmdat32&7)=td;
                        cycles-=4;
                        return;
                        case 0xD0: /*FNOP*/
                        if (fplog) pclog("FNOP\n");
                        cycles-=3;
                        return;
                        case 0xD8: case 0xD9: case 0xDA: case 0xDB: /*Invalid, but apparently not illegal*/
                        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                        if (fplog) pclog("FSTP\n");
                        ST(rmdat32 & 7) = ST(0);
                        temp = (tag >> ((TOP & 7) << 1)) & 3;
                        tag &= ~(3 << (((TOP + rmdat32) & 7) << 1));
                        tag |= (temp << (((TOP + rmdat32) & 7) << 1));
                        x87_pop();
                        cycles-=3;
                        return;
                        case 0xE0: /*FCHS*/
                        if (fplog) pclog("FCHS\n");
                        ST(0)=-ST(0);
                        cycles-=6;
                        return;
                        case 0xE1: /*FABS*/
                        if (fplog) pclog("FABS %f\n", ST(0));
                        ST(0)=fabs(ST(0));
                        cycles-=3;
                        return;
                        case 0xE4: /*FTST*/
                        if (fplog) pclog("FTST\n");
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==0.0)     npxs|=C3;
                        else if (ST(0)<0.0) npxs|=C0;
                        cycles-=4;
                        return;
                        case 0xE5: /*FXAM*/
                        if (fplog) pclog("FXAM %i %f\n", ((tag>>((TOP&7)<<1))&3), ST(0));
//                        pclog("FXAM %f %i\n",ST(0),(tag>>((TOP&7)<<1))&3);
                        npxs&=~(C0|C2|C3);
                        if (((tag>>((TOP&7)<<1))&3)==3) npxs|=(C0|C3);
                        else if (ST(0)==0.0)            npxs|=C3;
                        else                            npxs|=C2;
                        if (ST(0)<0.0)                  npxs|=C1;
                        cycles-=8;
                        return;
                        case 0xE8: /*FLD1*/
                        if (fplog) pclog("FLD1\n");
                        x87_push(1.0);
                        cycles-=4;
                        return;
                        case 0xE9: /*FLDL2T*/
                        if (fplog) pclog("FLDL2T\n");
                        x87_push(3.3219280948873623);
                        cycles-=8;
                        return;
                        case 0xEA: /*FLDL2E*/
                        if (fplog) pclog("FLDL2E\n");
                        x87_push(1.4426950408889634);
                        cycles-=8;
                        return;
                        case 0xEB: /*FLDPI*/
                        if (fplog) pclog("FLDPI\n");
                        x87_push(3.141592653589793);
                        cycles-=8;
                        return;
                        case 0xEC: /*FLDEG2*/
                        if (fplog) pclog("FLDEG2\n");
                        x87_push(0.3010299956639812);
                        cycles-=8;
                        return;
                        case 0xED: /*FLDLN2*/
                        if (fplog) pclog("FLDLN2\n");
                        x87_push(0.693147180559945);
                        cycles-=8;
                        return;
                        case 0xEE: /*FLDZ*/
                        if (fplog) pclog("FLDZ\n");
//                        pclog("FLDZ %04X:%08X\n",CS,pc);
                        x87_push(0.0);
                        tag|=(1<<((TOP&7)<<1));
                        cycles-=4;
                        return;
                        case 0xF0: /*F2XM1*/
                        if (fplog) pclog("F2XM1\n");
                        ST(0)=pow(2.0,ST(0))-1.0;
                        cycles-=200;
                        return;
                        case 0xF1: /*FYL2X*/
                        if (fplog) pclog("FYL2X\n");
                        ST(1)=ST(1)*(log(ST(0))/log(2.0));
                        x87_pop();
                        cycles-=250;
                        return;
                        case 0xF2: /*FPTAN*/
                        if (fplog) pclog("FPTAN\n");
//                        pclog("Tan of %f = ",ST(0));
                        ST(0)=tan(ST(0));
//                        pclog("%f\n",ST(0));
                        x87_push(1.0);
                        npxs&=~C2;
                        cycles-=235;
                        return;
                        case 0xF3: /*FPATAN*/
                        if (fplog) pclog("FPATAN\n");
/*                        times++;
                        if (times==50)
                        {
                                dumpregs();
                                exit(-1);
                        }*/
//                        pclog("FPATAN %08X\n",pc);
                        ST(1)=atan2(ST(1),ST(0));
                        x87_pop();
                        cycles-=250;
                        return;
                        case 0xF6: /*FDECSTP*/
                        if (fplog) pclog("FDECSTP\n");
                        TOP=(TOP-1)&7;
                        return;
                        case 0xF7: /*FINCSTP*/
                        if (fplog) pclog("FINCSTP\n");
                        TOP=(TOP+1)&7;
                        return;
                        case 0xF8: /*FPREM*/
                        if (fplog) pclog("FPREM %f %f  ", ST(0), ST(1));
                        temp64=(int64_t)(ST(0)/ST(1));
                        ST(0)=ST(0)-(ST(1)*(double)temp64);
                        if (fplog) pclog("%f\n", ST(0));
                        npxs&=~(C0|C1|C2|C3);
                        if (temp64&4) npxs|=C0;
                        if (temp64&2) npxs|=C3;
                        if (temp64&1) npxs|=C1;
                        cycles-=100;
                        return;
                        case 0xFA: /*FSQRT*/
                        if (fplog) pclog("FSQRT\n");
                        ST(0)=sqrt(ST(0));
                        cycles-=83;
                        return;
                        case 0xFB: /*FSINCOS*/
                        if (fplog) pclog("FSINCOS\n");
                        td=ST(0);
                        ST(0)=sin(td);
                        x87_push(cos(td));
                        npxs&=~C2;
                        cycles-=330;
                        return;
                        case 0xFC: /*FRNDINT*/
                        if (fplog) pclog("FRNDINT %g ", ST(0));
//                        pclog("FRNDINT %f %i ",ST(0),(npxc>>10)&3);
                        ST(0)=(double)x87_fround(ST(0));
//                        pclog("%f\n",ST(0));
                        if (fplog) pclog("%g\n", ST(0));
                        cycles-=21;
                        return;
                        case 0xFD: /*FSCALE*/
                        if (fplog) pclog("FSCALE\n");
                        temp64=(int64_t)ST(1);
                        ST(0)=ST(0)*pow(2.0,(double)temp64);
                        cycles-=30;
                        return;
                        case 0xFE: /*FSIN*/
                        if (fplog) pclog("FSIN\n");
                        ST(0)=sin(ST(0));
                        npxs&=~C2;
                        cycles-=300;
                        return;
                        case 0xFF: /*FCOS*/
                        if (fplog) pclog("FCOS\n");
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
                        if (fplog) pclog("FLDs %08X:%08X\n", easeg, eaaddr);                        
//                        if ((rmdat32&0xFFFF)==0xD445) { pclog("FLDS\n"); output=3; dumpregs(); exit(-1); }
                        ts.i=geteal(); if (abrt) { pclog("FLD sp abort\n"); return; }
                        if (fplog) pclog("  %f\n", ts.s);
                        x87_push((double)ts.s);
                        cycles-=3;
                        return;
                        case 2: /*FST single-precision*/
                        if (fplog) pclog("FSTs %08X:%08X\n", easeg, eaaddr);
                        ts.s=(float)ST(0);
                        seteal(ts.i);
                        if (abrt) { pclog("FST sp abort\n"); return; }
                        cycles-=7;
                        return;
                        case 3: /*FSTP single-precision*/
                        if (fplog) pclog("FSTPs %08X:%08X %f\n", easeg, eaaddr, ST(0));
                        ts.s=(float)ST(0);
                        seteal(ts.i); if (abrt) { pclog("FSTP sp abort\n"); return; }
  //                      if (pc==0x3f50c) pclog("FSTP %f %f %08X\n",ST(0),ts.s,ts.i);
                        x87_pop();
                        cycles-=7;
                        return;
                        case 4: /*FLDENV*/
                        if (fplog) pclog("FLDENV %08X:%08X\n", easeg, eaaddr);
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
                        if (abrt) { pclog("FLDENV\n"); return; }
                        return;
                        case 5: /*FLDCW*/
                        if (fplog) pclog("FLDCW %08X:%08X\n", easeg, eaaddr);                        
                        tempw=geteaw();
                        if (abrt) if (abrt) { pclog("FLDCW abort\n"); return; }
                        npxc=tempw;
                        cycles-=4;
                        return;
                        case 6: /*FSTENV*/
                        if (fplog) pclog("FSTENV %08X:%08X\n", easeg, eaaddr);
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
                        if (abrt) { pclog("FSTENV\n"); return; }
                        cycles-=(cr0&1)?56:67;
                        return;
                        case 7: /*FSTCW*/
                        if (fplog) pclog("FSTCW %08X:%08X\n", easeg, eaaddr);
                        seteaw(npxc);
                        if (abrt) { pclog("FSTCW\n"); return; }
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
                        if (fplog) pclog("FUCOMPP\n", easeg, eaaddr);
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
                templ=(int32_t)geteal(); if (abrt) return;
                switch (reg)
                {
                        case 0: /*FIADD*/
                        if (fplog) pclog("FIADDl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=ST(0)+(double)templ;
                        cycles-=20;
                        return;
                        case 1: /*FIMUL*/
                        if (fplog) pclog("FIMULl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=ST(0)*(double)templ;
                        cycles-=22;
                        return;
                        case 2: /*FICOM*/
                        if (fplog) pclog("FICOMl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        cycles-=15;
                        return;
                        case 3: /*FICOMP*/
                        if (fplog) pclog("FICOMPl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        x87_pop();
                        cycles-=15;
                        return;
                        case 4: /*FISUB*/
                        if (fplog) pclog("FISUBl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=ST(0)-(double)templ;
                        cycles-=20;
                        return;
                        case 5: /*FISUBR*/
                        if (fplog) pclog("FISUBRl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=(double)templ-ST(0);
                        cycles-=19;
                        return;
                        case 6: /*FIDIV*/
                        if (fplog) pclog("FIDIVl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        x87_div(ST(0), ST(0), (double)templ);
                        cycles-=84;
                        return;
                        case 7: /*FIDIVR*/
                        if (fplog) pclog("FIDIVRl %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        x87_div(ST(0), (double)templ, ST(0));
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
        int64_t temp64;
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
                                if (fplog) pclog("FCLEX\n");
                                npxs&=0xFF00;
                                return;
                                case 0xE3: /*FINIT*/
                                if (fplog) pclog("FINIT\n");
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
                        if (fplog) pclog("FILDs %08X:%08X\n", easeg, eaaddr);
                        templ=geteal(); if (abrt) return;
                        if (fplog) pclog("  %f %08X %i\n", (double)templ, templ, templ);
                        x87_push((double)templ);
                        cycles-=9;
                        return;
                        case 2: /*FIST short*/
                        if (fplog) pclog("FISTs %08X:%08X\n", easeg, eaaddr);
                        temp64 = x87_fround(ST(0));
/*                        if (temp64 > 2147483647 || temp64 < -2147483647)
                           fatal("FISTl out of range! %i\n", temp64);*/
                        seteal((int32_t)temp64);
                        cycles-=28;
                        return;
                        case 3: /*FISTP short*/
                        if (fplog) pclog("FISTPs %08X:%08X  %g ", easeg, eaaddr, ST(0));
                        temp64 = x87_fround(ST(0));
/*                        if (temp64 > 2147483647 || temp64 < -2147483647)
                           fatal("FISTPl out of range! %i\n", temp64);*/
                        if (fplog) pclog("%lli %llX\n", temp64, temp64);
                        seteal((int32_t)temp64); if (abrt) return;
                        x87_pop();
                        cycles-=28;
                        return;
                        case 5: /*FLD extended*/
                        if (fplog) pclog("FLDe %08X:%08X\n", easeg, eaaddr);                        
                        t=x87_ld80(); if (abrt) return;
                        if (fplog) pclog("  %f\n", t);
                        x87_push(t);
                        cycles-=6;
                        return;
                        case 7: /*FSTP extended*/
                        if (fplog) pclog("FSTPe %08X:%08X\n", easeg, eaaddr);
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
                        if (fplog) pclog("FADD %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(rmdat32&7)+ST(0);
                        cycles-=8;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FMUL*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        if (fplog) pclog("FMUL %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(rmdat32&7)*ST(0);
                        cycles-=16;
                        return;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: /*FSUBR*/
                        case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                        if (fplog) pclog("FSUBR %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(0)-ST(rmdat32&7);
                        cycles-=8;
                        return;
                        case 0xE8: case 0xE9: case 0xEA: case 0xEB: /*FSUB*/
                        case 0xEC: case 0xED: case 0xEE: case 0xEF:
                        if (fplog) pclog("FSUB %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(rmdat32&7)-ST(0);
                        cycles-=8;
                        return;
                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*FDIVR*/
                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                        if (fplog) pclog("FDIVR %f %f\n", ST(rmdat32 & 7), ST(0));
                        x87_div(ST(rmdat32&7), ST(0), ST(rmdat32&7));
                        cycles-=73;
                        return;
                        case 0xF8: case 0xF9: case 0xFA: case 0xFB: /*FDIV*/
                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                        if (fplog) pclog("FDIV %f %f\n", ST(rmdat32 & 7), ST(0));
                        x87_div(ST(rmdat32&7), ST(rmdat32&7), ST(0));
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
                        if (fplog) pclog("FADDd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        ST(0)+=t.d;
                        cycles-=8;
                        return;
                        case 1: /*FMUL double*/
                        if (fplog) pclog("FMULd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        ST(0)*=t.d;
                        cycles-=14;
                        return;
                        case 2: /*FCOM double*/
                        if (fplog) pclog("FCOMd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==t.d)     npxs|=C3;
                        else if (ST(0)<t.d) npxs|=C0;
                        cycles-=4;
                        return;
                        case 3: /*FCOMP double*/
                        if (fplog) pclog("FCOMPd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==t.d)     npxs|=C3;
                        else if (ST(0)<t.d) npxs|=C0;
                        cycles-=4;
                        x87_pop();
                        return;
                        case 4: /*FSUB double*/
                        if (fplog) pclog("FSUBd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        ST(0)-=t.d;
                        cycles-=8;
                        return;
                        case 5: /*FSUBR double*/
                        if (fplog) pclog("FSUBRd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        ST(0)=t.d-ST(0);
                        cycles-=8;
                        return;
                        case 6: /*FDIV double*/
                        if (fplog) pclog("FDIVd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        x87_div(ST(0), ST(0), t.d);
                        cycles-=73;
                        return;
                        case 7: /*FDIVR double*/
                        if (fplog) pclog("FDIVRd %08X:%08X %f %f\n", easeg, eaaddr, ST(0), t.d);
                        x87_div(ST(0), t.d, ST(0));
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
                        if (fplog) pclog("FFREE\n");
                        tag |= (3 << (((TOP + rmdat32) & 7) << 1));
                        cycles-=3;
                        return;
                        case 2: /*FST*/
                        if (fplog) pclog("FST\n");
                        ST(rmdat32 & 7) = ST(0);
                        temp = (tag >> ((TOP & 7) << 1)) & 3;
                        tag &= ~(3 << (((TOP + rmdat32) & 7) << 1));
                        tag |= (temp << (((TOP + rmdat32) & 7) << 1));
                        cycles-=3;
                        return;
                        case 3: /*FSTP*/
                        if (fplog) pclog("FSTP\n");
                        ST(rmdat32 & 7) = ST(0);
                        temp = (tag >> ((TOP & 7) << 1)) & 3;
                        tag &= ~(3 << (((TOP + rmdat32) & 7) << 1));
                        tag |= (temp << (((TOP + rmdat32) & 7) << 1));
                        x87_pop();
                        cycles-=3;
                        return;
                        case 4: /*FUCOM*/
                        if (fplog) pclog("FUCOM\n");
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==ST(rmdat32&7))     npxs|=C3;
                        else if (ST(0)<ST(rmdat32&7)) npxs|=C0;
                        cycles-=4;
                        return;
                        case 5: /*FUCOMP*/
                        if (fplog) pclog("FUCOMP\n");
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
                        if (fplog) pclog("FLDd %08X:%08X\n", easeg, eaaddr);
                        t.i=readmeml(easeg,eaaddr);
                        t.i|=(uint64_t)readmeml(easeg,eaaddr+4)<<32; if (abrt) return;
                        if (fplog) pclog("  %f\n", t.d);                        
                        x87_push(t.d);
                        cycles-=3;
                        return;
                        case 2: /*FST double-precision*/
                        if (fplog) pclog("FSTd %08X:%08X\n", easeg, eaaddr);
                        t.d=ST(0);
                        writememl(easeg,eaaddr,t.i);
                        writememl(easeg,eaaddr+4,(t.i>>32));
                        cycles-=8;
                        return;
                        case 3: /*FSTP double-precision*/
                        if (fplog) pclog("FSTPd %08X:%08X\n", easeg, eaaddr);
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
                        x87_ldmmx(&MM[0]); ST(0)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[1]); ST(1)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[2]); ST(2)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[3]); ST(3)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[4]); ST(4)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[5]); ST(5)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[6]); ST(6)=x87_ld80(); eaaddr += 10;
                        x87_ldmmx(&MM[7]); ST(7)=x87_ld80();
                        ismmx = 0;
                        /*Horrible hack, but as PCem doesn't keep the FPU stack in 80-bit precision at all times
                          something like this is needed*/
                        if (MM[0].w == 0xffff && MM[1].w == 0xffff && MM[2].w == 0xffff && MM[3].w == 0xffff &&
                            MM[4].w == 0xffff && MM[5].w == 0xffff && MM[6].w == 0xffff && MM[7].w == 0xffff &&
                            !TOP && !tag)
                                ismmx = 1;
                                
                        cycles-=(cr0&1)?34:44;
                        if (fplog) pclog("FRSTOR %08X:%08X %i %i %04X\n", easeg, eaaddr, ismmx, TOP, tag);
/*                        pclog("new TOP %i\n", TOP);
                        pclog("%04X %04X %04X  %f %f %f %f  %f %f %f %f\n", npxc, npxs, tag, ST(0), ST(1), ST(2), ST(3), ST(4), ST(5), ST(6), ST(7));*/
                        return;
                        case 6: /*FSAVE*/
                        if (fplog) pclog("FSAVE %08X:%08X %i\n", easeg, eaaddr, ismmx);
                        npxs = (npxs & ~(7 << 11)) | (TOP << 11);
/*                        pclog("old TOP %i %04X\n", TOP, npxs);
                        pclog("%04X %04X %04X  %f %f %f %f  %f %f %f %f\n", npxc, npxs, tag, ST(0), ST(1), ST(2), ST(3), ST(4), ST(5), ST(6), ST(7));*/

                        switch ((cr0&1)|(op32&0x100))
                        {
                                case 0x000: /*16-bit real mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+2,npxs);
                                writememw(easeg,eaaddr+4,tag);
                                writememw(easeg,eaaddr+6,x87_pc_off);
                                writememw(easeg,eaaddr+10,x87_op_off);
                                eaaddr+=14;
                                if (ismmx)
                                {
                                        x87_stmmx(MM[0]); eaaddr+=10;
                                        x87_stmmx(MM[1]); eaaddr+=10;
                                        x87_stmmx(MM[2]); eaaddr+=10;
                                        x87_stmmx(MM[3]); eaaddr+=10;
                                        x87_stmmx(MM[4]); eaaddr+=10;
                                        x87_stmmx(MM[5]); eaaddr+=10;
                                        x87_stmmx(MM[6]); eaaddr+=10;
                                        x87_stmmx(MM[7]);
                                }
                                else
                                {
                                        x87_st80(ST(0)); eaaddr+=10;
                                        x87_st80(ST(1)); eaaddr+=10;
                                        x87_st80(ST(2)); eaaddr+=10;
                                        x87_st80(ST(3)); eaaddr+=10;
                                        x87_st80(ST(4)); eaaddr+=10;
                                        x87_st80(ST(5)); eaaddr+=10;
                                        x87_st80(ST(6)); eaaddr+=10;
                                        x87_st80(ST(7));
                                }
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
                                if (ismmx)
                                {
                                        x87_stmmx(MM[0]); eaaddr+=10;
                                        x87_stmmx(MM[1]); eaaddr+=10;
                                        x87_stmmx(MM[2]); eaaddr+=10;
                                        x87_stmmx(MM[3]); eaaddr+=10;
                                        x87_stmmx(MM[4]); eaaddr+=10;
                                        x87_stmmx(MM[5]); eaaddr+=10;
                                        x87_stmmx(MM[6]); eaaddr+=10;
                                        x87_stmmx(MM[7]);
                                }
                                else
                                {
                                        x87_st80(ST(0)); eaaddr+=10;
                                        x87_st80(ST(1)); eaaddr+=10;
                                        x87_st80(ST(2)); eaaddr+=10;
                                        x87_st80(ST(3)); eaaddr+=10;
                                        x87_st80(ST(4)); eaaddr+=10;
                                        x87_st80(ST(5)); eaaddr+=10;
                                        x87_st80(ST(6)); eaaddr+=10;
                                        x87_st80(ST(7));
                                }
                                break;
                                case 0x100: /*32-bit real mode*/
                                writememw(easeg,eaaddr,npxc);
                                writememw(easeg,eaaddr+4,npxs);
                                writememw(easeg,eaaddr+8,tag);
                                writememw(easeg,eaaddr+12,x87_pc_off);
                                writememw(easeg,eaaddr+20,x87_op_off);
                                writememl(easeg,eaaddr+24,(x87_op_off>>16)<<12);
                                eaaddr+=28;
                                if (ismmx)
                                {
                                        x87_stmmx(MM[0]); eaaddr+=10;
                                        x87_stmmx(MM[1]); eaaddr+=10;
                                        x87_stmmx(MM[2]); eaaddr+=10;
                                        x87_stmmx(MM[3]); eaaddr+=10;
                                        x87_stmmx(MM[4]); eaaddr+=10;
                                        x87_stmmx(MM[5]); eaaddr+=10;
                                        x87_stmmx(MM[6]); eaaddr+=10;
                                        x87_stmmx(MM[7]);
                                }
                                else
                                {
                                        x87_st80(ST(0)); eaaddr+=10;
                                        x87_st80(ST(1)); eaaddr+=10;
                                        x87_st80(ST(2)); eaaddr+=10;
                                        x87_st80(ST(3)); eaaddr+=10;
                                        x87_st80(ST(4)); eaaddr+=10;
                                        x87_st80(ST(5)); eaaddr+=10;
                                        x87_st80(ST(6)); eaaddr+=10;
                                        x87_st80(ST(7));
                                }
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
                                if (ismmx)
                                {
                                        x87_stmmx(MM[0]); eaaddr+=10;
                                        x87_stmmx(MM[1]); eaaddr+=10;
                                        x87_stmmx(MM[2]); eaaddr+=10;
                                        x87_stmmx(MM[3]); eaaddr+=10;
                                        x87_stmmx(MM[4]); eaaddr+=10;
                                        x87_stmmx(MM[5]); eaaddr+=10;
                                        x87_stmmx(MM[6]); eaaddr+=10;
                                        x87_stmmx(MM[7]);
                                }
                                else
                                {
                                        x87_st80(ST(0)); eaaddr+=10;
                                        x87_st80(ST(1)); eaaddr+=10;
                                        x87_st80(ST(2)); eaaddr+=10;
                                        x87_st80(ST(3)); eaaddr+=10;
                                        x87_st80(ST(4)); eaaddr+=10;
                                        x87_st80(ST(5)); eaaddr+=10;
                                        x87_st80(ST(6)); eaaddr+=10;
                                        x87_st80(ST(7));
                                }
                                break;
                        }
                        cycles-=(cr0&1)?56:67;
                        return;
                        case 7: /*FSTSW*/
                        if (fplog) pclog("FSTSW %08X:%08X\n", easeg, eaaddr);
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
        int32_t templ;
        if (mod==3)
        {
                switch (rmdat32&0xFF)
                {
                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*FADDP*/
                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                        if (fplog) pclog("FADDP %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(rmdat32&7)+ST(0);
                        x87_pop();
                        cycles-=8;
                        return;
                        case 0xC8: case 0xC9: case 0xCA: case 0xCB: /*FMULP*/
                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                        if (fplog) pclog("FMULP %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(rmdat32&7)*ST(0);
                        x87_pop();
                        cycles-=16;
                        return;
                        case 0xD9: /*FCOMPP*/
                        if (fplog) pclog("FCOMPP %f %f\n", ST(0), ST(1));
                        npxs&=~(C0|C2|C3);
                        if (*(uint64_t *)&ST(0) == ((uint64_t)1 << 63) && *(uint64_t *)&ST(1) == 0)
                                npxs |= C0; /*Nasty hack to fix 80387 detection*/
                        else if (ST(0) == ST(1))
                                npxs |= C3;
                        else if (ST(0) < ST(1))
                                npxs |= C0;
                        x87_pop();
                        x87_pop();
                        cycles-=5;
                        return;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: /*FSUBRP*/
                        case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                        if (fplog) pclog("FSUBRP %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(0)-ST(rmdat32&7);
                        x87_pop();
                        cycles-=8;
                        return;
                        case 0xE8: case 0xE9: case 0xEA: case 0xEB: /*FSUBP*/
                        case 0xEC: case 0xED: case 0xEE: case 0xEF:
                        if (fplog) pclog("FSUBP %f %f\n", ST(rmdat32 & 7), ST(0));
                        ST(rmdat32&7)=ST(rmdat32&7)-ST(0);
                        x87_pop();
                        cycles-=8;
                        return;
                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*FDIVRP*/
                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                        if (fplog) pclog("FDIVRP %f %f\n", ST(rmdat32 & 7), ST(0));
                        x87_div(ST(rmdat32&7), ST(0), ST(rmdat32&7));
                        x87_pop();
                        cycles-=73;
                        return;
                        case 0xF8: case 0xF9: case 0xFA: case 0xFB: /*FDIVP*/
                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                        if (fplog) pclog("FDIVP %f %f %i\n", ST(rmdat32 & 7), ST(0), rmdat32 & 7);
                        x87_div(ST(rmdat32&7), ST(rmdat32&7), ST(0));
                        x87_pop();
                        cycles-=73;
                        return;
                }
        }
        else
        {
                templ=(int32_t)(int16_t)geteaw(); if (abrt) return;
                switch (reg)
                {
                        case 0: /*FIADD*/
                        if (fplog) pclog("FIADDw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=ST(0)+(double)templ;
                        cycles-=20;
                        return;
                        case 1: /*FIMUL*/
                        if (fplog) pclog("FIMULw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=ST(0)*(double)templ;
                        cycles-=22;
                        return;
                        case 2: /*FICOM*/
                        if (fplog) pclog("FICOMw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        cycles-=15;
                        return;
                        case 3: /*FICOMP*/
                        if (fplog) pclog("FICOMPw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        npxs&=~(C0|C2|C3);
                        if (ST(0)==(double)templ)     npxs|=C3;
                        else if (ST(0)<(double)templ) npxs|=C0;
                        x87_pop();
                        cycles-=15;
                        return;
                        case 4: /*FISUB*/
                        if (fplog) pclog("FISUBw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=ST(0)-(double)templ;
                        cycles-=20;
                        return;
                        case 5: /*FISUBR*/
                        if (fplog) pclog("FISUBRw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        ST(0)=(double)templ-ST(0);
                        cycles-=19;
                        return;
                        case 6: /*FIDIV*/
                        if (fplog) pclog("FIDIVw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        x87_div(ST(0), ST(0), (double)templ);
                        cycles-=84;
                        return;
                        case 7: /*FIDIVR*/
                        if (fplog) pclog("FIDIVRw %08X:%08X %f %f\n", easeg, eaaddr, ST(0), (double)templ);
                        x87_div(ST(0), (double)templ, ST(0));
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
                                if (fplog) pclog("FSTSW\n");
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
                        if (fplog) pclog("FILDw %08X:%08X\n", easeg, eaaddr);
                        temp16=geteaw(); if (abrt) return;
                        if (fplog) pclog("  %f\n", (double)temp16);
                        x87_push((double)temp16);
                        cycles-=13;
                        return;
                        case 2: /*FIST word*/
                        if (fplog) pclog("FISTw %08X:%08X\n", easeg, eaaddr);
                        temp64 = x87_fround(ST(0));
/*                        if (temp64 > 32767 || temp64 < -32768)
                           fatal("FISTw overflow %i\n", temp64);*/
                        seteaw((int16_t)temp64);
                        cycles-=29;
                        return;
                        case 3: /*FISTP word*/
                        if (fplog) pclog("FISTPw %08X:%08X\n", easeg, eaaddr);
                        temp64 = x87_fround(ST(0));
/*                        if (temp64 > 32767 || temp64 < -32768)
                           fatal("FISTw overflow %i\n", temp64);*/
                        seteaw((int16_t)temp64); if (abrt) return;
                        x87_pop();
                        cycles-=29;
                        return;
                        case 5: /*FILD long*/
                        if (fplog) pclog("FILDl %08X:%08X\n", easeg, eaaddr);
                        temp64=geteal(); if (abrt) return;
                        temp64|=(uint64_t)readmeml(easeg,eaaddr+4)<<32;
                        if (fplog) pclog("  %f  %08X %08X\n", (double)temp64, readmeml(easeg,eaaddr), readmeml(easeg,eaaddr+4));
                        x87_push((double)temp64);
                        cycles-=10;
                        return;
                        case 6: /*FBSTP*/
                        if (fplog) pclog("FBSTP %08X:%08X\n", easeg, eaaddr);
                        tempd=ST(0);
                        if (tempd < 0.0) 
                           tempd = -tempd;
                        for (c=0;c<9;c++)
                        {
                                tempc=(uint8_t)floor(fmod(tempd,10.0));       tempd-=floor(fmod(tempd,10.0)); tempd/=10.0;
                                tempc|=((uint8_t)floor(fmod(tempd,10.0)))<<4; tempd-=floor(fmod(tempd,10.0)); tempd/=10.0;
                                writememb(easeg,eaaddr+c,tempc);
                        }
                        tempc=(uint8_t)floor(fmod(tempd,10.0));
                        if (ST(0)<0.0) tempc|=0x80;
                        writememb(easeg,eaaddr+9,tempc); if (abrt) return;
                        x87_pop();
                        return;
                        case 7: /*FISTP long*/
                        if (fplog) pclog("FISTPl %08X:%08X\n", easeg, eaaddr);
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

