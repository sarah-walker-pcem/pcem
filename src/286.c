#include "ibm.h"
#include "x86.h"
#include "x86_flags.h"
#include "cpu.h"
#include "pit.h"
#include "keyboard.h"

#define getword getword286

#define checkio_perm(port) if (!IOPLp) \
                        { \
                                x86gpf(NULL,0); \
                                break; \
                        }

#undef readmemb
#undef writememb

#define readmemb(s,a) ((readlookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF)?readmemb386l(s,a):ram[readlookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)])
#define writememb(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF) writememb386l(s,a,v); else ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]=v
#define writememw(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFE) writememwl(s,a,v); else *((uint16_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v

extern int keywaiting;

#define checklimit(a)

/*#define checklimit(a)   if (msw&1 && (a)) \
                        { \
                                pclog("Seg limit GPF at %04X:%04X\n",CS,pc); \
                                pclog("%04X %04X %04X %i %04X %04X\n", ealimitw, ealimit, eaaddr, mod, _ds.limit, _ss.limit); \
                                x86gpf(NULL,0); \
                                break; \
                        }*/

#define NOTRM   if (!(msw & 1))\
                { \
                        x86_int(6); \
                        break; \
                }


static inline uint8_t geteab()
{
        if (mod==3)
           return (rm&4)?regs[rm&3].b.h:regs[rm&3].b.l;
//        cycles-=3;
        return readmemb(easeg, eaaddr);
}

static inline uint16_t geteaw()
{
        if (mod==3)
           return regs[rm].w;
//        cycles-=3;
//        if (output==3) printf("GETEAW %04X:%08X\n",easeg,eaaddr);
        return readmemw(easeg, eaaddr);
}

static inline uint16_t geteaw2()
{
        if (mod==3)
           return regs[rm].w;
//        cycles-=2;
//        printf("Getting addr from %04X:%04X %05X\n",easeg,eaaddr+2,easeg+eaaddr+2);
        return readmemw(easeg,(eaaddr+2)&0xFFFF);
}

static inline void seteab(uint8_t val)
{
        if (mod==3)
        {
                if (rm&4) regs[rm&3].b.h=val;
                else      regs[rm&3].b.l=val;
        }
        else
        {
//                cycles-=2;
                writememb(easeg, eaaddr, val);
        }
}

static inline void seteaw(uint16_t val)
{
        if (mod==3)
           regs[rm].w=val;
        else
        {
//                cycles-=2;
                writememw(easeg, eaaddr, val);
//                writememb(easeg+eaaddr+1,val>>8);
        }
}

#undef fetchea

#define fetchea()   { rmdat=readmemb(cs, pc); pc++;     \
                    reg=(rmdat>>3)&7;                  \
                    mod=(rmdat>>6)&3;                  \
                    rm=rmdat&7;                        \
                    if (mod!=3) fetcheal286(); }

void fetcheal286()
{
        if (!mod && rm==6) { eaaddr=getword(); easeg=ds; ealimit=_ds.limit; ealimitw=_ds.limitw; }
        else
        {
                switch (mod)
                {
                        case 0:
                        eaaddr=0;
                        break;
                        case 1:
                        eaaddr=(uint16_t)(int8_t)readmemb(cs, pc); pc++;
                        break;
                        case 2:
                        eaaddr=getword();
                        break;
                }
                eaaddr+=(*mod1add[0][rm])+(*mod1add[1][rm]);
                easeg=*mod1seg[rm];
                if (mod1seg[rm] == &ss) { ealimit=_ss.limit; ealimitw=_ss.limitw; }
                else                    { ealimit=_ds.limit; ealimitw=_ds.limitw; }
//                if (output) pclog("Limit %08X %08X %08X %08X\n",ealimit,_ds.limit,mod1seg[rm],&ss);
        }
        eaaddr&=0xFFFF;        
}

void rep286(int fv)
{
        uint8_t temp;
        uint32_t c;//=CX;
        uint8_t temp2;
        uint16_t tempw,tempw2,tempw3,of=flags;
        uint32_t ipc=oldpc;//pc-1;
        int changeds=0;
        uint32_t oldds;
        uint32_t templ,templ2;
        int tempz;
        int tempi;
//        if (output) pclog("REP32 %04X %04X  ",use32,rep32);
        startrep:
        temp=opcode2=readmemb(cs,pc); pc++;
//        if (firstrepcycle && temp==0xA5) pclog("REP MOVSW %06X:%04X %06X:%04X\n",ds,SI,es,DI);
//        if (output) pclog("REP %02X %04X\n",temp,ipc);
        c = CX;
/*        if (rep32 && (msw&1))
        {
                if (temp!=0x67 && temp!=0x66 && (rep32|temp)!=0x1AB && (rep32|temp)!=0x3AB) pclog("32-bit REP %03X %08X %04X:%06X\n",temp|rep32,c,CS,pc);
        }*/
        switch (temp)
        {
                case 0x26: /*ES:*/
                oldds=ds;
                ds=es;
                rds=ES;
                changeds=1;
                goto startrep;
                break;
                case 0x2E: /*CS:*/
                oldds=ds;
                ds=cs;
                rds=CS;
                changeds=1;
                goto startrep;
                break;
                case 0x36: /*SS:*/
                oldds=ds;
                ds=ss;
                rds=SS;
                changeds=1;
                goto startrep;
                break;
                case 0x3E: /*DS:*/
                oldds=ds;
                ds=ds;
                changeds=1;
                goto startrep;
                break;
                case 0x6C: /*REP INSB*/
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
                case 0x6E: /*REP OUTSB*/
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
                case 0xA4: /*REP MOVSB*/
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
                case 0xA6: /*REP CMPSB*/
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
                case 0xA7: /*REP CMPSW*/
                if (fv) flags|=Z_FLAG;
                else    flags&=~Z_FLAG;
                if ((c>0) && (fv==((flags&Z_FLAG)?1:0)))
                {
                        tempw=readmemw(ds,SI);
                        tempw2=readmemw(es,DI);
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

                case 0xAA: /*REP STOSB*/
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
                case 0xAC: /*REP LODSB*/
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
                case 0xAE: /*REP SCASB*/
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

                default:
                        pc=ipc;
                        cycles-=20;
                x86illegal();
                pclog("Bad REP %02X\n", temp);
                //dumpregs();
                //exit(-1);
        }
        CX=c;
        if (changeds) ds=oldds;
//        if (output) pclog("%03X %03X\n",rep32,use32);
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
        if (msw&1)
        {
                pmodeint(6,0);
        }
        else
        {
                if (ssegs) { ss=oldss; _ss.limit=oldsslimit; _ss.limitw=oldsslimitw; }
                writememw(ss,((SP-2)&0xFFFF),flags);
                writememw(ss,((SP-4)&0xFFFF),CS);
                writememw(ss,((SP-6)&0xFFFF),pc);
                SP-=6;
                flags&=~I_FLAG;
                flags&=~T_FLAG;
                addr=6<<2;
                pc=readmemw(0,addr);
                loadcs(readmemw(0,addr+2));
        }
        cycles-=70;
}
//#endif
//#if 0

static inline uint8_t getbytef()
{
        uint8_t temp = readmemb(cs, pc); pc++;
        return temp;
}
static inline uint16_t getwordf()
{
        uint16_t tempw = readmemw(cs, pc); pc+=2;
        return tempw;
}

/*Conditional jump timing is WRONG*/
void exec286(int cycs)
{
        uint8_t temp,temp2;
        uint16_t addr,tempw,tempw2,tempw3,tempw4;
        int8_t offset;
        volatile int tempws, tempws2;
        uint32_t templ, templ2, templ3;
        int8_t temps;
        int16_t temps16;
        int c;//,cycdiff;
        int tempi;
        FILE *f;
        int trap;
        int cyclast;
//        printf("Run 286! %i %i\n",cycles,cycs);
        cycles+=cycs;
//        i86_Execute(cycs);
//        return;
        while (cycles>0)
        {
                cyclast = cycdiff;
                cycdiff=cycles;
                oldcs=CS;
                oldpc=pc;
                oldcpl=CPL;
                
                opcodestart:
                opcode = readmemb(cs, pc);
                if (abrt) goto opcodeend;
                tempc=flags&C_FLAG;
                trap=flags&T_FLAG;
                
                if (output && /*cs<0xF0000 && */!ssegs)//opcode!=0x26 && opcode!=0x36 && opcode!=0x2E && opcode!=0x3E)
                {
                        if ((opcode!=0xF2 && opcode!=0xF3) || firstrepcycle)
                        {
                                if (!skipnextprint) printf("%04X(%06X):%04X : %04X %04X %04X %04X %04X(%06X) %04X(%06X) %04X(%06X) %04X(%06X) %04X %04X %04X %04X %02X %04X  %04X  %04X %04X %04X  %08X %i %i  %i %08X  %i %f %f\n",CS,cs,pc,AX,BX,CX,DX,CS,cs,DS,ds,ES,es,SS,ss,DI,SI,BP,SP,opcode,flags,msw,ram[0x7c3e],ram[(0x7c3e)+3],ram[0x7c40],_ds.limitw,keybsenddelay,keywaiting,ins, _ss.limit, cyclast, pit.c[1], PITCONST);
                                skipnextprint=0;
//                                ins++;
/*                                if (ins==50000)
                                {
                                        dumpregs();
                                        exit(-1);
                                }*/
/*                                if (ins==500000)
                                {
                                        dumpregs();
                                        exit(-1);
                                }*/
                        }
                }
//#endif
                pc++;
                inhlt=0;
//                if (ins==500000) { dumpregs(); exit(0); }*/
                switch (opcode)
                {
                        case 0x00: /*ADD 8,reg*/
                        fetchea();
                        //if (!rmdat && output) pc--;
/*                        if (!rmdat && output)
                        {
                                pclog("Crashed\n");
//                                clear_keybuf();
//                                readkey();
                                dumpregs();
                                exit(-1);
                        }*/
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setadd8(temp, temp2);
                                setr8(rm, temp + temp2);
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();     if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp + temp2); if (abrt) break;
                                setadd8(temp, temp2);
                                cycles -= 7;
                        }
                        break;
                        case 0x01: /*ADD 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setadd16(regs[rm].w, regs[reg].w);
                                regs[rm].w += regs[reg].w;
                                cycles -= 2;
                        }
                        else
                        {
                                tempw = geteaw();             if (abrt) break;
                                seteaw(tempw + regs[reg].w);  if (abrt) break;
                                setadd16(tempw, regs[reg].w);
                                cycles -= 7;
                        }
                        break;
                        case 0x02: /*ADD reg,8*/
                        fetchea();
                        temp=geteab();        if (abrt) break;
                        setadd8(getr8(reg),temp);
                        setr8(reg,getr8(reg)+temp);
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x03: /*ADD reg,16*/
                        fetchea();
                        tempw=geteaw();       if (abrt) break;
                        setadd16(regs[reg].w,tempw);
                        regs[reg].w+=tempw;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x04: /*ADD AL,#8*/
                        temp=getbytef();
                        setadd8(AL,temp);
                        AL+=temp;
                        cycles -= 3;
                        break;
                        case 0x05: /*ADD AX,#16*/
                        tempw=getwordf();
                        setadd16(AX,tempw);
                        AX+=tempw;
                        cycles -= 3;
                        break;

                        case 0x06: /*PUSH ES*/
                        if (ssegs) ss=oldss;
                        writememw(ss,((SP-2)&0xFFFF),ES); if (abrt) break;
                        SP-=2;
                        cycles-=3;
                        break;
                        case 0x07: 
                        if (ssegs) ss=oldss;
                        tempw=readmemw(ss,SP);          if (abrt) break;
                        loadseg(tempw,&_es);            if (abrt) break;
                        SP+=2;
                        cycles -= (msw & 1) ? 20 : 5;
                        break;

                        case 0x08: /*OR 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm) | getr8(reg);
                                setr8(rm, temp);
                                setznp8(temp);
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();          if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp | temp2);     if (abrt) break;
                                setznp8(temp | temp2);
                                cycles -= 7;
                        }
                        break;
                        case 0x09: /*OR 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].w |= regs[reg].w;
                                setznp16(regs[rm].w);
                                cycles -= 2;
                        }
                        else
                        {
                                tempw = geteaw() | regs[reg].w; if (abrt) break;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= 7;
                        }
                        break;
                        case 0x0A: /*OR reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp|=getr8(reg);
                        setznp8(temp);
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x0B: /*OR reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw|=regs[reg].w;
                        setznp16(tempw);
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x0C: /*OR AL,#8*/
                        AL|=getbytef();
                        setznp8(AL);
                        cycles -= 2;
                        break;
                        case 0x0D: /*OR AX,#16*/
                        AX|=getwordf();
                        setznp16(AX);
                        cycles -= 2;
                        break;

                        case 0x0E: /*PUSH CS*/
                        if (ssegs) ss=oldss;
                        writememw(ss,((SP-2)&0xFFFF),CS);       if (abrt) break;
                        SP-=2;
                        cycles-=3;
                        break;

                        case 0x0F:
                        temp = readmemb(cs, pc); pc++;
                        opcode2 = temp;
//                        if (temp>5 && temp!=0x82 && temp!=0x85 && temp!=0x84 && temp!=0x87 && temp!=0x8D && temp!=0x8F && temp!=0x8C && temp!=0x20 && temp!=0x22) pclog("Using magic 386 0F instruction %02X!\n",temp);
                        switch (temp)
                        {
                                case 0:
                                fetchea(); if (abrt) break;
                                switch (rmdat&0x38)
                                {
                                        case 0x00: /*SLDT*/
                                        NOTRM
                                        seteaw(ldt.seg);
                                        cycles -= (mod == 3) ? 3 : 2;
                                        break;
                                        case 0x08: /*STR*/
                                        NOTRM
                                        seteaw(tr.seg);
                                        cycles -= (mod == 3) ? 3 : 2;
                                        break;
                                        case 0x10: /*LLDT*/
                                        if (CPL && (msw & 1))
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

                                        cycles -= (mod == 3) ? 17 : 19;
                                        break;
                                        case 0x18: /*LTR*/
                                        if (CPL && (msw & 1))
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
                                        cycles -= (mod == 3) ? 17 : 19;
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
                                        cycles -= (mod == 3) ? 14 : 16;
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
                                        cycles -= (mod == 3) ? 14 : 16;
                                        break;


                                        default:
                                        pclog("Bad 0F 00 opcode %02X\n",rmdat&0x38);
                                        pc-=3;
                                        x86illegal();
                                        break;
//                                        dumpregs();
//                                        exit(-1);
                                }
                                break;
                                case 1:
                                fetchea(); if (abrt) break;
                                switch (rmdat&0x38)
                                {
                                        case 0x00: /*SGDT*/
                                        seteaw(gdt.limit);
                                        writememw(easeg, eaaddr + 2, gdt.base);
                                        writememw(easeg, eaaddr + 4, (gdt.base >> 16) | 0xFF00);
                                        cycles -= 11;
                                        break;
                                        case 0x08: /*SIDT*/
                                        seteaw(idt.limit);
                                        writememw(easeg, eaaddr + 2, idt.base);
                                        writememw(easeg, eaaddr + 4, (idt.base >> 16) | 0xFF00);
                                        cycles -= 12;
                                        break;
                                        case 0x10: /*LGDT*/
                                        if (CPL && (msw & 1))
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
                                        if (CPL && (msw & 1))
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
                                        cycles-=12;
                                        break;

                                        case 0x20: /*SMSW*/
                                        if (is486) seteaw(msw);
                                        else       seteaw(msw|0xFF00);
//                                        pclog("SMSW %04X:%04X  %04X %i\n",CS,pc,msw,is486);
                                        cycles -= (mod == 3) ? 2 : 3;
                                        break;
                                        case 0x30: /*LMSW*/
                                        if (CPL && (msw&1))
                                        {
                                                pclog("LMSW - ring not zero!\n");
                                                x86gpf(NULL,0);
                                                break;
                                        }
                                        tempw=geteaw(); if (abrt) break;
                                        if (msw&1) tempw|=1;
                                        msw=tempw;
                                        cycles -= (mod == 3) ? 3 : 6;
                                        pclog("LMSW %04X %08X %04X:%04X\n", msw, cs, CS, pc);
                                        break;

                                        default:
                                        pclog("Bad 0F 01 opcode %02X\n",rmdat&0x38);
                                        pc-=3;
                                        x86illegal();
                                        break;
//                                        dumpregs();
//                                        exit(-1);
                                }
                                break;

                                case 2: /*LAR*/
                                NOTRM
                                fetchea();
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
                                cycles -= (mod == 3) ? 14 : 16;
                                break;

                                case 3: /*LSL*/
                                NOTRM
                                fetchea();
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
                                cycles -= (mod == 3) ? 14 : 16;
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
                                /*if (readmemw(0,0x806))
                                {
                                        pclog("Something in LOADALL!\n");
                                        dumpregs();
                                        exit(-1);
                                }*/
                                es=readmemw(0,0x836)|(readmemb(0,0x838)<<16);
                                cs=readmemw(0,0x83C)|(readmemb(0,0x83E)<<16);
                                ss=readmemw(0,0x842)|(readmemb(0,0x844)<<16);
                                ds=readmemw(0,0x848)|(readmemb(0,0x84A)<<16);
                                cycles-=195;
//                                pclog("LOADALL - %06X:%04X %06X:%04X %04X\n",ds,SI,es,DI,DX);
                                break;
                                
                                case 6: /*CLTS*/
                                if (CPL && (msw & 1))
                                {
                                        pclog("Can't CLTS\n");
                                        x86gpf(NULL,0);
                                        break;
                                }
                                msw &= ~8;
                                cycles-=2;
                                break;
                                
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
                                        writememw(ss,((SP-2)&0xFFFF),flags);
                                        writememw(ss,((SP-4)&0xFFFF),CS);
                                        writememw(ss,((SP-6)&0xFFFF),pc);
                                        SP-=6;
                                        addr=6<<2;
                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
/*                                        if (!pc && !cs)
                                        {
                                                pclog("Bad int %02X %04X:%04X\n",temp,oldcs,oldpc);
                                                dumpregs();
                                                exit(-1);
                                        }*/
                                }
                                cycles-=23;
                                break;

                                default:
                                pclog("Bad 16-bit 0F opcode %02X 386 %i\n",temp,ins);
                                pc=oldpc;
//                                output=3;
//                                timetolive=100000;
//                                dumpregs();
//                                exit(-1);
                                x86illegal();
                                break;
                        }
                        break;

                        case 0x10: /*ADC 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setadc8(temp, temp2);
                                setr8(rm, temp + temp2 + tempc);
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();               if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp + temp2 + tempc);   if (abrt) break;
                                setadc8(temp, temp2);
                                cycles -= 7;
                        }
                        break;
                        case 0x11: /*ADC 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setadc16(regs[rm].w, regs[reg].w);
                                regs[rm].w += regs[reg].w + tempc;
                                cycles -= 2;
                        }
                        else
                        {
                                tempw  = geteaw();              if (abrt) break;
                                tempw2 = regs[reg].w;
                                seteaw(tempw + tempw2 + tempc); if (abrt) break;
                                setadc16(tempw, tempw2);
                                cycles -= 7;
                        }
                        break;
                        case 0x12: /*ADC reg,8*/
                        fetchea();
                        temp=geteab();                  if (abrt) break;
                        setadc8(getr8(reg),temp);
                        setr8(reg,getr8(reg)+temp+tempc);
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x13: /*ADC reg,16*/
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        setadc16(regs[reg].w,tempw);
                        regs[reg].w+=tempw+tempc;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x14: /*ADC AL,#8*/
                        tempw=getbytef();
                        setadc8(AL,tempw);
                        AL+=tempw+tempc;
                        cycles -= 3;
                        break;
                        case 0x15: /*ADC AX,#16*/
                        tempw=getwordf();
                        setadc16(AX,tempw);
                        AX+=tempw+tempc;
                        cycles -= 3;
                        break;

                        case 0x16: /*PUSH SS*/
                        if (ssegs) ss=oldss;
                        writememw(ss,((SP-2)&0xFFFF),SS); if (abrt) break;
                        SP-=2;
                        cycles-=3;
                        break;
                        case 0x17: /*POP SS*/
                        if (ssegs) ss=oldss;
                        tempw=readmemw(ss,SP);  if (abrt) break;
                        loadseg(tempw,&_ss); if (abrt) break;
                        SP += 2;
                        cycles -= (msw & 1) ? 20 : 5;
                        break;

                        case 0x18: /*SBB 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setsbc8(temp, temp2);
                                setr8(rm, temp - (temp2 + tempc));
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();                  if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp - (temp2 + tempc));    if (abrt) break;
                                setsbc8(temp, temp2);
                                cycles -= 7;
                        }
                        break;
                        case 0x19: /*SBB 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setsbc16(regs[rm].w, regs[reg].w);
                                regs[rm].w -= (regs[reg].w + tempc);
                                cycles -= 2;
                        }
                        else
                        {
                                tempw  = geteaw();                 if (abrt) break;
                                tempw2 = regs[reg].w;
                                seteaw(tempw - (tempw2 + tempc));  if (abrt) break;
                                setsbc16(tempw, tempw2);
                                cycles -= 7;
                        }
                        break;
                        case 0x1A: /*SBB reg,8*/
                        fetchea();
                        temp=geteab();                  if (abrt) break;
                        setsbc8(getr8(reg),temp);
                        setr8(reg,getr8(reg)-(temp+tempc));
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x1B: /*SBB reg,16*/
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        tempw2=regs[reg].w;
                        setsbc16(tempw2,tempw);
                        tempw2-=(tempw+tempc);
                        regs[reg].w=tempw2;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x1C: /*SBB AL,#8*/
                        temp=getbytef();
                        setsbc8(AL,temp);
                        AL-=(temp+tempc);
                        cycles -= 3;
                        break;
                        case 0x1D: /*SBB AX,#16*/
                        tempw=getwordf();
                        setsbc16(AX,tempw);
                        AX-=(tempw+tempc);
                        cycles -= 3;
                        break;

                        case 0x1E: /*PUSH DS*/
                        if (ssegs) ss=oldss;
                        writememw(ss,((SP-2)&0xFFFF),DS);       if (abrt) break;
                        SP-=2;
                        cycles-=3;
                        break;
                        case 0x1F: case 0x21F: /*POP DS*/
                        if (ssegs) ss=oldss;
                        tempw=readmemw(ss,SP);                  if (abrt) break;
                        loadseg(tempw,&_ds); if (abrt) break;
                        SP+=2;
                        cycles -= (msw & 1) ? 20 : 5;
                        break;

                        case 0x20: /*AND 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm) & getr8(reg);
                                setr8(rm, temp);
                                setznp8(temp);
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();    if (abrt) break;
                                temp &= getr8(reg);
                                seteab(temp);        if (abrt) break;
                                setznp8(temp);
                                cycles -= 7;
                        }
                        break;
                        case 0x21: /*AND 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].w &= regs[reg].w;
                                setznp16(regs[rm].w);
                                cycles -= 2;
                        }
                        else
                        {
                                tempw = geteaw() & regs[reg].w; if (abrt) break;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= 7;
                        }
                        break;
                        case 0x22: /*AND reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp&=getr8(reg);
                        setznp8(temp);
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x23: /*AND reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw&=regs[reg].w;
                        setznp16(tempw);
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x24: /*AND AL,#8*/
                        AL&=getbytef();
                        setznp8(AL);
                        cycles -= 3;
                        break;
                        case 0x25: /*AND AX,#16*/
                        AX&=getwordf();
                        setznp16(AX);
                        cycles -= 3;
                        break;

                        case 0x26: /*ES:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=es;
                        rds=ES;
                        ssegs=2;
                        cycles-=2;
                        goto opcodestart;
//                        break;

                        case 0x27: /*DAA*/
                        if ((flags & A_FLAG) || ((AL & 0xF) > 9))
                        {
                                tempi = ((uint16_t)AL) + 6;
                                AL += 6;
                                flags |= A_FLAG;
                                if (tempi & 0x100) flags |= C_FLAG;
                        }
                        if ((flags&C_FLAG) || (AL>0x9F))
                        {
                                AL+=0x60;
                                flags|=C_FLAG;
                        }
                        tempw = flags & (C_FLAG | A_FLAG);
                        setznp8(AL);
                        flags |= tempw;
                        cycles -= 3;
                        break;

                        case 0x28: /*SUB 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp  = getr8(rm);
                                temp2 = getr8(reg);
                                setsub8(temp, temp2);
                                setr8(rm, temp - temp2);
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();     if (abrt) break;
                                temp2 = getr8(reg);
                                seteab(temp - temp2); if (abrt) break;
                                setsub8(temp, temp2);
                                cycles -= 7;
                        }
                        break;
                        case 0x29: /*SUB 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                setsub16(regs[rm].w, regs[reg].w);
                                regs[rm].w -= regs[reg].w;
                                cycles -= 2;
                        }
                        else
                        {
                                tempw = geteaw();             if (abrt) break;
                                seteaw(tempw - regs[reg].w);  if (abrt) break;
                                setsub16(tempw, regs[reg].w);
                                cycles -= 7;
                        }
                        break;
                        case 0x2A: /*SUB reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        setsub8(getr8(reg),temp);
                        setr8(reg,getr8(reg)-temp);
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x2B: /*SUB reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        setsub16(regs[reg].w,tempw);
                        regs[reg].w-=tempw;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x2C: /*SUB AL,#8*/
                        temp=getbytef();
                        setsub8(AL,temp);
                        AL-=temp;
                        cycles -= 3;
                        break;
                        case 0x2D: /*SUB AX,#16*/
                        tempw=getwordf();
                        setsub16(AX,tempw);
                        AX-=tempw;
                        cycles -= 3;
                        break;

                        case 0x2E: /*CS:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=cs;
                        rds=CS;
                        ssegs=2;
                        cycles-=2;
                        goto opcodestart;
                        
                        case 0x2F: /*DAS*/
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
                        cycles -= 3;
                        break;

                        case 0x30: /*XOR 8,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                temp = getr8(rm) ^ getr8(reg);
                                setr8(rm, temp);
                                setznp8(temp);
                                cycles -= 2;
                        }
                        else
                        {
                                temp  = geteab();    if (abrt) break;
                                temp ^= getr8(reg);
                                seteab(temp);        if (abrt) break;
                                setznp8(temp);
                                cycles -= 7;
                        }
                        break;
                        case 0x31: /*XOR 16,reg*/
                        fetchea();
                        if (mod == 3)
                        {
                                regs[rm].w ^= regs[reg].w;
                                setznp16(regs[rm].w);
                                cycles -= 2;
                        }
                        else
                        {
                                tempw = geteaw() ^ regs[reg].w; if (abrt) break;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= 7;
                        }
                        break;
                        case 0x32: /*XOR reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp^=getr8(reg);
                        setznp8(temp);
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x33: /*XOR reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw^=regs[reg].w;
                        setznp16(tempw);
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x34: /*XOR AL,#8*/
                        AL^=getbytef();
                        setznp8(AL);
                        cycles -= 3;
                        break;
                        case 0x35: /*XOR AX,#16*/
                        AX^=getwordf();
                        setznp16(AX);
                        cycles -= 3;
                        break;

                        case 0x36: /*SS:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=ss;
                        rds=SS;
                        ssegs=2;
                        cycles-=2;
                        goto opcodestart;
//                        break;

                        case 0x37: /*AAA*/
                        if ((flags&A_FLAG)||((AL&0xF)>9))
                        {
                                AL+=6;
                                AH++;
                                flags|=(A_FLAG|C_FLAG);
                        }
                        else
                           flags&=~(A_FLAG|C_FLAG);
                        AL&=0xF;
                        cycles -= 3;
                        break;

                        case 0x38: /*CMP 8,reg*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        setsub8(temp,getr8(reg));
                        cycles -= (mod == 3) ? 2 : 7;
                        break;
                        case 0x39: /*CMP 16,reg*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
//                        if (output) pclog("CMP %04X %04X\n",tempw,regs[reg].w);
                        setsub16(tempw,regs[reg].w);
                        cycles -= (mod == 3) ? 2 : 7;                        
                        break;
                        case 0x3A: /*CMP reg,8*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
//                        if (output) pclog("CMP %02X-%02X\n",getr8(reg),temp);
                        setsub8(getr8(reg),temp);
                        cycles -= (mod == 3) ? 2 : 6;
                        break;
                        case 0x3B: /*CMP reg,16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        setsub16(regs[reg].w,tempw);
                        cycles -= (mod == 3) ? 2 : 6;
                        break;
                        case 0x3C: /*CMP AL,#8*/
                        temp=getbytef();
                        setsub8(AL,temp);
                        cycles -= 3;
                        break;
                        case 0x3D: /*CMP AX,#16*/
                        tempw=getwordf();
                        setsub16(AX,tempw);
                        cycles -= 3;
                        break;

                        case 0x3E: /*DS:*/
                        oldss=ss;
                        oldds=ds;
                        ds=ss=ds;
                        ssegs=2;
                        cycles-=2;
                        goto opcodestart;
//                        break;

                        case 0x3F: /*AAS*/
                        if ((flags&A_FLAG)||((AL&0xF)>9))
                        {
                                AL-=6;
                                AH--;
                                flags|=(A_FLAG|C_FLAG);
                        }
                        else
                           flags&=~(A_FLAG|C_FLAG);
                        AL&=0xF;
                        cycles -= 3;
                        break;

                        case 0x40: case 0x41: case 0x42: case 0x43: /*INC r16*/
                        case 0x44: case 0x45: case 0x46: case 0x47:
                        setadd16nc(regs[opcode&7].w,1);
                        regs[opcode&7].w++;
                        cycles -= 2;
                        break;
                        case 0x48: case 0x49: case 0x4A: case 0x4B: /*DEC r16*/
                        case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                        setsub16nc(regs[opcode&7].w,1);
                        regs[opcode&7].w--;
                        cycles -= 2;
                        break;

                        case 0x50: case 0x51: case 0x52: case 0x53: /*PUSH r16*/
                        case 0x54: case 0x55: case 0x56: case 0x57:
                        if (ssegs) ss=oldss;
                        writememw(ss,(SP-2)&0xFFFF,regs[opcode&7].w);   if (abrt) break;
                        SP-=2;
                        cycles -= 3;
                        break;
                        case 0x58: case 0x59: case 0x5A: case 0x5B: /*POP r16*/
                        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
                        if (ssegs) ss=oldss;
                        SP+=2;
                        tempw=readmemw(ss,(SP-2)&0xFFFF);    if (abrt) { SP-=2; break; }
                        regs[opcode&7].w=tempw;
                        cycles -= 5;
                        break;

                        case 0x60: /*PUSHA*/
                        writememw(ss,((SP-2)&0xFFFF),AX);
                        writememw(ss,((SP-4)&0xFFFF),CX);
                        writememw(ss,((SP-6)&0xFFFF),DX);
                        writememw(ss,((SP-8)&0xFFFF),BX);
                        writememw(ss,((SP-10)&0xFFFF),SP);
                        writememw(ss,((SP-12)&0xFFFF),BP);
                        writememw(ss,((SP-14)&0xFFFF),SI);
                        writememw(ss,((SP-16)&0xFFFF),DI);
                        if (!abrt) SP-=16;
                        cycles -= 17;
                        break;
                        case 0x61: /*POPA*/
                        DI=readmemw(ss,((SP)&0xFFFF));          if (abrt) break;
                        SI=readmemw(ss,((SP+2)&0xFFFF));        if (abrt) break;
                        BP=readmemw(ss,((SP+4)&0xFFFF));        if (abrt) break;
                        BX=readmemw(ss,((SP+8)&0xFFFF));        if (abrt) break;
                        DX=readmemw(ss,((SP+10)&0xFFFF));       if (abrt) break;
                        CX=readmemw(ss,((SP+12)&0xFFFF));       if (abrt) break;
                        AX=readmemw(ss,((SP+14)&0xFFFF));       if (abrt) break;
                        SP+=16;
                        cycles -= 19;
                        break;

                        case 0x62: /*BOUND*/
                        fetchea();
                        tempw=geteaw();
                        tempw2=readmemw(easeg,eaaddr+2); if (abrt) break;
                        if (((int16_t)regs[reg].w<(int16_t)tempw) || ((int16_t)regs[reg].w>(int16_t)tempw2))
                        {
                                x86_int(5);
                        }
                        cycles -= 13;
                        break;
                        
                        case 0x63: /*ARPL*/
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
                        cycles -= (mod == 3) ? 10 : 11;
                        break;

                        case 0x68: /*PUSH #w*/
                        tempw=getword();
                        writememw(ss,((SP-2)&0xFFFF),tempw);    if (abrt) break;
                        SP-=2;
                        cycles -= 3;
                        break;
                        case 0x69: /*IMUL r16*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw2=getword();       if (abrt) break;
                        templ=((int)(int16_t)tempw)*((int)(int16_t)tempw2);
                        if ((templ>>16)!=0 && (templ>>16)!=0xFFFF) flags|=C_FLAG|V_FLAG;
                        else                                       flags&=~(C_FLAG|V_FLAG);
                        regs[reg].w=templ&0xFFFF;
                        cycles -= (mod == 3) ? 21 : 24;
                        break;
                        case 0x6A: /*PUSH #eb*/
                        tempw=readmemb(cs,pc); pc++;
                        if (tempw&0x80) tempw|=0xFF00;
                        writememw(ss,((SP-2)&0xFFFF),tempw);    if (abrt) break;
                        SP-=2;
                        cycles -= 3;
                        break;
                        case 0x6B: /*IMUL r8*/
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        tempw2=readmemb(cs,pc); pc++;   if (abrt) break;
                        if (tempw2&0x80) tempw2|=0xFF00;
                        templ=((int)(int16_t)tempw)*((int)(int16_t)tempw2);
                        if ((templ>>16)!=0 && ((templ>>16)&0xFFFF)!=0xFFFF) flags|=C_FLAG|V_FLAG;
                        else                                                flags&=~(C_FLAG|V_FLAG);
                        regs[reg].w=templ&0xFFFF;
                        cycles -= (mod == 3) ? 21 : 24;
                        break;

                        case 0x6C: /*INSB*/
                        checkio_perm(DX);
                        temp=inb(DX);
                        writememb(es,DI,temp);          if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        cycles -= 5;
                        break;
                        case 0x6D: /*INSW*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        tempw=inw(DX);
                        writememw(es,DI,tempw);         if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        cycles -= 5;
                        break;
                        case 0x6E: /*OUTSB*/
                        temp=readmemb(ds,SI);           if (abrt) break;
                        checkio_perm(DX);
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        outb(DX,temp);
                        cycles -= 5;
                        break;
                        case 0x6F: /*OUTSW*/
                        tempw=readmemw(ds,SI);          if (abrt) break;
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        outw(DX,tempw);
//                        outb(DX+1,tempw>>8);
                        cycles -= 5;
                        break;

                        case 0x70: /*JO*/
                        offset=(int8_t)getbytef();
                        if (flags&V_FLAG) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x71: /*JNO*/
                        offset=(int8_t)getbytef();
                        if (!(flags&V_FLAG)) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x72: /*JB*/
                        offset=(int8_t)getbytef();
                        if (flags&C_FLAG) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x73: /*JNB*/
                        offset=(int8_t)getbytef();
                        if (!(flags&C_FLAG)) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x74: /*JZ*/
                        offset=(int8_t)getbytef();
                        if (flags&Z_FLAG) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x75: /*JNZ*/
                        offset=(int8_t)getbytef();
                        if (!(flags&Z_FLAG)) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x76: /*JBE*/
                        offset=(int8_t)getbytef();
                        if (flags&(C_FLAG|Z_FLAG)) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x77: /*JNBE*/
                        offset=(int8_t)getbytef();
                        if (!(flags&(C_FLAG|Z_FLAG))) { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x78: /*JS*/
                        offset=(int8_t)getbytef();
                        if (flags&N_FLAG)  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x79: /*JNS*/
                        offset=(int8_t)getbytef();
                        if (!(flags&N_FLAG))  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x7A: /*JP*/
                        offset=(int8_t)getbytef();
                        if (flags&P_FLAG)  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x7B: /*JNP*/
                        offset=(int8_t)getbytef();
                        if (!(flags&P_FLAG))  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x7C: /*JL*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if (temp!=temp2)  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x7D: /*JNL*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if (temp==temp2)  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x7E: /*JLE*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if ((flags&Z_FLAG) || (temp!=temp2))  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;
                        case 0x7F: /*JNLE*/
                        offset=(int8_t)getbytef();
                        temp=(flags&N_FLAG)?1:0;
                        temp2=(flags&V_FLAG)?1:0;
                        if (!((flags&Z_FLAG) || (temp!=temp2)))  { pc += offset; cycles -= 4; cycles -= 2; }
                        cycles -= 3;
                        break;


                        case 0x80: 
                        case 0x82:
                        fetchea();
                        temp=geteab();                  if (abrt) break;
                        temp2=readmemb(cs,pc); pc++;    if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD b,#8*/
                                seteab(temp+temp2);             if (abrt) break;
                                setadd8(temp,temp2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x08: /*OR b,#8*/
                                temp|=temp2;
                                seteab(temp);                   if (abrt) break;
                                setznp8(temp);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x10: /*ADC b,#8*/
                                seteab(temp+temp2+tempc);       if (abrt) break;
                                setadc8(temp,temp2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x18: /*SBB b,#8*/
                                seteab(temp-(temp2+tempc));     if (abrt) break;
                                setsbc8(temp,temp2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x20: /*AND b,#8*/
                                temp&=temp2;
                                seteab(temp);                   if (abrt) break;
                                setznp8(temp);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x28: /*SUB b,#8*/
                                seteab(temp-temp2);             if (abrt) break;
                                setsub8(temp,temp2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x30: /*XOR b,#8*/
                                temp^=temp2;
                                seteab(temp);                   if (abrt) break;
                                setznp8(temp);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x38: /*CMP b,#8*/
                                setsub8(temp,temp2);
                                cycles -= (mod == 3) ? 3 : 6;
                                break;

//                                default:
//                                pclog("Bad 80 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0x81: 
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw2=getword();       if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD w,#16*/
                                seteaw(tempw+tempw2);   if (abrt) break;
                                setadd16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x08: /*OR w,#16*/
                                tempw|=tempw2;
                                seteaw(tempw);          if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x10: /*ADC w,#16*/
                                seteaw(tempw+tempw2+tempc); if (abrt) break;
                                setadc16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x20: /*AND w,#16*/
                                tempw&=tempw2;
                                seteaw(tempw);          if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x18: /*SBB w,#16*/
                                seteaw(tempw-(tempw2+tempc));   if (abrt) break;
                                setsbc16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x28: /*SUB w,#16*/
                                seteaw(tempw-tempw2);           if (abrt) break;
                                setsub16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x30: /*XOR w,#16*/
                                tempw^=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x38: /*CMP w,#16*/
//                                pclog("CMP %04X %04X\n", tempw, tempw2);
                                setsub16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 6;
                                break;
                        }
                        break;

                        case 0x83:
                        fetchea();
                        tempw=geteaw();                 if (abrt) break;
                        tempw2=readmemb(cs,pc); pc++;   if (abrt) break;
                        if (tempw2&0x80) tempw2|=0xFF00;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*ADD w,#8*/
                                seteaw(tempw+tempw2);           if (abrt) break;
                                setadd16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x08: /*OR w,#8*/
                                tempw|=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x10: /*ADC w,#8*/
                                seteaw(tempw+tempw2+tempc);     if (abrt) break;
                                setadc16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x18: /*SBB w,#8*/
                                seteaw(tempw-(tempw2+tempc));   if (abrt) break;
                                setsbc16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x20: /*AND w,#8*/
                                tempw&=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x28: /*SUB w,#8*/
                                seteaw(tempw-tempw2);           if (abrt) break;
                                setsub16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x30: /*XOR w,#8*/
                                tempw^=tempw2;
                                seteaw(tempw);                  if (abrt) break;
                                setznp16(tempw);
                                cycles -= (mod == 3) ? 3 : 7;
                                break;
                                case 0x38: /*CMP w,#8*/
                                setsub16(tempw,tempw2);
                                cycles -= (mod == 3) ? 3 : 6;
                                break;

//                                default:
//                                pclog("Bad 83 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0x84: /*TEST b,reg*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        temp2=getr8(reg);
                        setznp8(temp&temp2);
                        cycles -= (mod == 3) ? 2 : 6;
                        break;
                        case 0x85: /*TEST w,reg*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        tempw2=regs[reg].w;
                        setznp16(tempw&tempw2);
                        cycles -= (mod == 3) ? 2 : 6;
                        break;
                        case 0x86: /*XCHG b,reg*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        seteab(getr8(reg));     if (abrt) break;
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? 3 : 5;
                        break;
                        case 0x87: /*XCHG w,reg*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        seteaw(regs[reg].w);    if (abrt) break;
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? 3 : 5;
                        break;

                        case 0x88: /*MOV b,reg*/
                        fetchea();
                        seteab(getr8(reg));
                        cycles -= (mod == 3) ? 2 : 3;
                        break;
                        case 0x89: /*MOV w,reg*/
                        fetchea();
                        seteaw(regs[reg].w);
                        cycles -= (mod == 3) ? 2 : 3;
                        break;
                        case 0x8A: /*MOV reg,b*/
                        fetchea();
                        temp=geteab();          if (abrt) break;
                        setr8(reg,temp);
                        cycles -= (mod == 3) ? 5 : 3;
                        break;
                        case 0x8B: /*MOV reg,w*/
                        fetchea();
                        tempw=geteaw();         if (abrt) break;
                        regs[reg].w=tempw;
                        cycles -= (mod == 3) ? 5 : 3;
                        break;

                        case 0x8C: /*MOV w,sreg*/
                        fetchea();
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
                        }
                        cycles -= (mod == 3) ? 2 : 3;
                        break;

                        case 0x8D: /*LEA*/
                        fetchea();
                        regs[reg].w=eaaddr;
                        cycles -= 3;
                        break;

                        case 0x8E: /*MOV sreg,w*/
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
                        }
                        if (msw & 1) cycles -= (mod == 3) ? 17 : 19;
                        else         cycles -= (mod == 3) ?  2 :  5;
                        break;

                        case 0x8F: /*POPW*/
                        if (ssegs) templ2=oldss;
                        else       templ2=ss;
                        tempw=readmemw(templ2,SP);      if (abrt) break;
                        SP+=2;
                        fetchea();
                        if (ssegs) ss=oldss;
                        seteaw(tempw);
                        if (abrt) SP-=2;
                        cycles -= 5;
                        break;

                        case 0x90: /*NOP*/
                        cycles -= 3;
                        break;

                        case 0x91: case 0x92: case 0x93: /*XCHG AX*/
                        case 0x94: case 0x95: case 0x96: case 0x97:
                        tempw=AX;
                        AX=regs[opcode&7].w;
                        regs[opcode&7].w=tempw;
                        cycles -= 3;
                        break;

                        case 0x98: /*CBW*/
                        AH=(AL&0x80)?0xFF:0;
                        cycles -= 2;
                        break;
                        case 0x99: /*CWD*/
                        DX=(AX&0x8000)?0xFFFF:0;
                        cycles -= 2;
                        break;
                        case 0x9A: /*CALL FAR*/
                        tempw=getword();
                        tempw2=getword();       if (abrt) break;
                        tempw3 = CS;
                        templ2 = pc;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        pc=tempw;
                        optype=CALL;
                        if (msw&1) loadcscall(tempw2);
                        else       loadcs(tempw2);
                        optype=0;
                        if (abrt) break;
                        oldss=ss;
                        writememw(ss,(SP-2)&0xFFFF,tempw3);
                        writememw(ss,(SP-4)&0xFFFF,templ2);     if (abrt) break;
                        SP-=4;
                        cycles -= (msw & 1) ? 26 : 13;
                        break;
                        case 0x9B: /*WAIT*/
                        cycles -= 3;
                        break;
                        case 0x9C: /*PUSHF*/
                        if (ssegs) ss=oldss;
                        if (msw & 1) { writememw(ss, ((SP-2)&0xFFFF), flags & 0x7fff); }
                        else         { writememw(ss, ((SP-2)&0xFFFF), flags & 0x0fff); }   
                        if (abrt) break;
                        SP-=2;
                        cycles -= 3;
                        break;
                        case 0x9D: /*POPF*/
                        if (ssegs) ss=oldss;
                        tempw=readmemw(ss,SP);                  if (abrt) break;
                        SP+=2;
                        if (!(CPL) || !(msw&1)) flags=(tempw&0xFFD5)|2;
                        else if (IOPLp) flags=(flags&0x3000)|(tempw&0x4FD5)|2;
                        else            flags=(flags&0x7200)|(tempw&0x0DD5)|2;
                        pclog("POPF %04X %04X  %04X:%04X\n", tempw, flags, CS, pc);
                        cycles -= 5;
                        break;
                        case 0x9E: /*SAHF*/
                        flags=(flags&0xFF00)|(AH&0xD5)|2;
                        cycles -= 2;
                        break;
                        case 0x9F: /*LAHF*/
                        AH=flags&0xFF;
                        cycles -= 2;
                        break;

                        case 0xA0: /*MOV AL,(w)*/
                        addr=getword(); if (abrt) break;
                        temp=readmemb(ds,addr);         if (abrt) break;
                        AL=temp;
                        cycles -= 5;
                        break;
                        case 0xA1: /*MOV AX,(w)*/
                        addr=getword(); if (abrt) break;
                        tempw=readmemw(ds,addr);        if (abrt) break;
                        AX=tempw;
                        cycles -= 5;
                        break;
                        case 0xA2: /*MOV (w),AL*/
                        addr=getword(); if (abrt) break;
                        writememb(ds,addr,AL);
                        cycles -= 3;
                        break;
                        case 0xA3: /*MOV (w),AX*/
                        addr=getword(); if (abrt) break;
                        writememw(ds,addr,AX);
                        cycles -= 3;
                        break;

                        case 0xA4: /*MOVSB*/
                        temp=readmemb(ds,SI);  if (abrt) break;
                        writememb(es,DI,temp); if (abrt) break;
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        cycles -= 5;
                        break;
                        case 0xA5: /*MOVSW*/
                        tempw=readmemw(ds,SI);  if (abrt) break;
                        writememw(es,DI,tempw); if (abrt) break;
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        cycles -= 5;
                        break;
                        case 0xA6: /*CMPSB*/
                        temp =readmemb(ds,SI);
                        temp2=readmemb(es,DI);
                        if (abrt) break;
                        setsub8(temp,temp2);
                        if (flags&D_FLAG) { DI--; SI--; }
                        else              { DI++; SI++; }
                        cycles -= 8;
                        break;
                        case 0xA7: /*CMPSW*/
                        tempw =readmemw(ds,SI);
                        tempw2=readmemw(es,DI);
                        if (abrt) break;
                        setsub16(tempw,tempw2);
                        if (flags&D_FLAG) { DI-=2; SI-=2; }
                        else              { DI+=2; SI+=2; }
                        cycles -= 8;
                        break;
                        case 0xA8: /*TEST AL,#8*/
                        temp=getbytef();
                        setznp8(AL&temp);
                        cycles -= 3;
                        break;
                        case 0xA9: /*TEST AX,#16*/
                        tempw=getwordf();
                        setznp16(AX&tempw);
                        cycles -= 3;
                        break;
                        case 0xAA: /*STOSB*/
                        writememb(es,DI,AL);    if (abrt) break;
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        cycles -= 3;
                        break;
                        case 0xAB: /*STOSW*/
                        writememw(es,DI,AX);    if (abrt) break;
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        cycles -= 3;
                        break;
                        case 0xAC: /*LODSB*/
                        temp=readmemb(ds,SI);
                        if (abrt) break;
                        AL=temp;
                        if (flags&D_FLAG) SI--;
                        else              SI++;
                        cycles -= 5;
                        break;
                        case 0xAD: /*LODSW*/
                        tempw=readmemw(ds,SI);
                        if (abrt) break;
                        AX=tempw;
//                        if (output) pclog("Load from %05X:%04X\n",ds,SI);
                        if (flags&D_FLAG) SI-=2;
                        else              SI+=2;
                        cycles -= 5;
                        break;
                        case 0xAE: /*SCASB*/
                        temp=readmemb(es,DI);
                        if (abrt) break;
                        setsub8(AL,temp);
                        if (flags&D_FLAG) DI--;
                        else              DI++;
                        cycles -= 7;
                        break;
                        case 0xAF: /*SCASW*/
                        tempw=readmemw(es,DI);
                        if (abrt) break;
                        setsub16(AX,tempw);
                        if (flags&D_FLAG) DI-=2;
                        else              DI+=2;
                        cycles -= 7;
                        break;

                        case 0xB0: /*MOV AL,#8*/
                        AL=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB1: /*MOV CL,#8*/
                        CL=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB2: /*MOV DL,#8*/
                        DL=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB3: /*MOV BL,#8*/
                        BL=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB4: /*MOV AH,#8*/
                        AH=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB5: /*MOV CH,#8*/
                        CH=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB6: /*MOV DH,#8*/
                        DH=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB7: /*MOV BH,#8*/
                        BH=getbytef();
                        cycles -= 2;
                        break;
                        case 0xB8: case 0xB9: case 0xBA: case 0xBB: /*MOV reg,#16*/
                        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                        regs[opcode&7].w=getwordf();
                        cycles -= 2;
                        break;

                        case 0xC0:
                        fetchea();
                        c=readmemb(cs,pc); pc++;
                        temp=geteab();          if (abrt) break;
                        c&=31;
                        if (!c) break;
                        cycles -= c;                        
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;
                                case 0x20: case 0x30: /*SHL b,CL*/
                                seteab(temp<<c);        if (abrt) break;
                                setznp8(temp<<c);
                                if ((temp<<(c-1))&0x80) flags|=C_FLAG;
                                if (((temp<<c)^(temp<<(c-1)))&0x80) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
                                break;
                                case 0x28: /*SHR b,CL*/
                                seteab(temp>>c);        if (abrt) break;
                                setznp8(temp>>c);
                                if ((temp>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && temp&0x80) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

//                                default:
//                                pclog("Bad C0 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0xC1:
                        fetchea();
                        c=readmemb(cs,pc)&31; pc++;
                        tempw=geteaw();         if (abrt) break;
                        if (!c) break;
                        cycles -= c;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

                                case 0x20: case 0x30: /*SHL w,CL*/
                                seteaw(tempw<<c); if (abrt) break;
                                setznp16(tempw<<c);
                                if ((tempw<<(c-1))&0x8000) flags|=C_FLAG;
                                if (((tempw<<c)^(tempw<<(c-1)))&0x8000) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

                                case 0x28:            /*SHR w,CL*/
                                seteaw(tempw>>c); if (abrt) break;
                                setznp16(tempw>>c);
                                if ((tempw>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && tempw&0x8000) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

//                                default:
//                                pclog("Bad C1 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0xC2: /*RET*/
                        tempw=getword();
                        if (ssegs) ss=oldss;
                        tempw2=readmemw(ss,SP); if (abrt) break;
                        SP+=2+tempw;
                        pc=tempw2;
                        cycles -= 11;
                        break;
                        case 0xC3: /*RET*/
                        if (ssegs) ss=oldss;
                        tempw=readmemw(ss,SP);  if (abrt) break;
                        SP+=2;
                        pc=tempw;
                        cycles -= 11;
                        break;
                        case 0xC4: /*LES*/
                        fetchea();
                        tempw2=readmemw(easeg,eaaddr);
                        tempw=readmemw(easeg,eaaddr+2); if (abrt) break;
                        loadseg(tempw,&_es);            if (abrt) break;
                        regs[reg].w=tempw2;
                        cycles -= 7;
                        break;
                        case 0xC5: /*LDS*/
                        fetchea();
                        tempw2=readmemw(easeg,eaaddr);
                        tempw=readmemw(easeg,eaaddr+2); if (abrt) break;
                        loadseg(tempw,&_ds);            if (abrt) break;
                        if (ssegs) oldds=ds;
                        regs[reg].w=tempw2;
                        cycles -= 7;
                        break;
                        case 0xC6: /*MOV b,#8*/
                        fetchea();
                        temp=readmemb(cs,pc); pc++;     if (abrt) break;
                        seteab(temp);
                        cycles -= (mod == 3) ? 2 : 3;
                        break;
                        case 0xC7: /*MOV w,#16*/
                        fetchea();
                        tempw=getword();                if (abrt) break;
                        seteaw(tempw);
                        cycles -= (mod == 3) ? 2 : 3;
                        break;
                        case 0xC8: /*ENTER*/
                        tempw2=getword();
                        tempi=readmemb(cs,pc); pc++;
                        templ=BP;
                        writememw(ss,((SP-2)&0xFFFF),BP); if (abrt) break; 
                        SP-=2;
                        templ2=SP;
                        if (tempi>0)
                        {
                                while (--tempi)
                                {
                                        BP-=2;
                                        tempw=readmemw(ss,BP);
                                        if (abrt) { SP=templ2; BP=templ; break; }
                                        writememw(ss,((SP-2)&0xFFFF),tempw); SP-=2;
                                        if (abrt) { SP=templ2; BP=templ; break; }
                                        cycles-=(is486)?3:4;
                                }
                                writememw(ss,((SP-2)&0xFFFF),templ2); SP-=2;
                                if (abrt) { SP=templ2; BP=templ; break; }
                                cycles -= 4;
                        }
                        BP = templ2;
                        SP-=tempw2;
                        cycles -= 12;
                        break;
                        case 0xC9: /*LEAVE*/
                        templ=SP;
                        SP=BP;
                        tempw=readmemw(ss,SP);   SP+=2;
                        if (abrt) { SP=templ; break; }
                        BP=tempw;
                        cycles -= 5;
                        break;
                        case 0xCA: /*RETF*/
                        tempw=getword();
                        if (msw&1)
                        {
                                pmoderetf(0,tempw);
                                break;
                        }
                        tempw2=CPL;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        pc=readmemw(ss,SP);
                        loadcs(readmemw(ss,SP+2));
                        if (abrt) break;
                        SP+=4+tempw;
                        cycles -= 15;
                        break;
                        case 0xCB: /*RETF*/
                        if (msw&1)
                        {
                                pmoderetf(0,0);
                                break;
                        }
                        tempw2=CPL;
                        if (ssegs) ss=oldss;
                        oxpc=pc;
                        pc=readmemw(ss,SP);
                        loadcs(readmemw(ss,SP+2));
                        if (abrt) break;
                        SP+=4;
                        cycles -= 15;
                        break;
                        case 0xCC: /*INT 3*/
                        if (msw&1)
                        {
                                pmodeint(3,1);
                                cycles -= 40;
                        }
                        else
                        {
                                if (ssegs) ss=oldss;
                                writememw(ss,((SP-2)&0xFFFF),flags);
                                writememw(ss,((SP-4)&0xFFFF),CS);
                                writememw(ss,((SP-6)&0xFFFF),pc);
                                SP-=6;
                                addr=3<<2;
//                                flags&=~I_FLAG;
                                flags&=~T_FLAG;
                                oxpc=pc;
                                pc=readmemw(0,addr);
                                loadcs(readmemw(0,addr+2));
                                cycles -= 23;
                        }
                        break;
                        case 0xCD: /*INT*/
                        lastpc=pc;
                        lastcs=CS;
                        temp=readmemb(cs,pc); pc++;
                        intrt:
                                pclog("INT %02X  %04X %04X %04X %04X  %04X:%04X\n", temp, AX, BX, CX, DX, CS, pc);
                        if (1)
                        {
                                if (msw&1)
                                {
//                                        pclog("PMODE int %02X %04X at %04X:%04X  ",temp,AX,CS,pc);
                                        pmodeint(temp,1);
                                        cycles -= 40;
//                                        pclog("to %04X:%04X\n",CS,pc);
                                }
                                else
                                {
                                        if (ssegs) ss=oldss;
                                        writememw(ss,((SP-2)&0xFFFF),flags);
                                        writememw(ss,((SP-4)&0xFFFF),CS);
                                        writememw(ss,((SP-6)&0xFFFF),pc);
                                        SP-=6;
                                        addr=temp<<2;
//                                        flags&=~I_FLAG;
                                        flags&=~T_FLAG;
                                        oxpc=pc;
//                                        pclog("%04X:%04X : ",CS,pc);
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                        cycles -= 23;
//                                        pclog("INT %02X - %04X %04X:%04X\n",temp,addr,CS,pc);
                                }
                        }
                        break;
                        case 0xCE: /*INTO*/
                        if (flags&V_FLAG)
                        {
                                temp=4;
                                goto intrt;
/*                                pclog("INTO interrupt!\n");
                                dumpregs();
                                exit(-1);*/
                        }
                        cycles-=3;
                        break;
                        case 0xCF: /*IRET*/
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
                                pc=readmemw(ss,SP);
                                loadcs(readmemw(ss,((SP+2)&0xFFFF)));
                                flags=(readmemw(ss,((SP+4)&0xFFFF))&0x0FD5)|2;
                                SP+=6;
                        }
                        cycles -= 17;
                        break;
                        
                        case 0xD0:
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
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x08: /*ROR b,1*/
                                seteab((temp>>1)|((temp&1)?0x80:0)); if (abrt) break;
                                if (temp&1) flags|=C_FLAG;
                                else        flags&=~C_FLAG;
                                temp>>=1;
                                if (flags&C_FLAG) temp|=0x80;
                                if ((temp^(temp>>1))&0x40) flags|=V_FLAG;
                                else                       flags&=~V_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
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
                                cycles -= (mod == 3) ? 2 : 7;
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
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x20: case 0x30: /*SHL b,1*/
                                seteab(temp<<1); if (abrt) break;
                                setznp8(temp<<1);
                                if (temp&0x80) flags|=C_FLAG;
                                if ((temp^(temp<<1))&0x80) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x28: /*SHR b,1*/
                                seteab(temp>>1); if (abrt) break;
                                setznp8(temp>>1);
                                if (temp&1) flags|=C_FLAG;
                                if (temp&0x80) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x38: /*SAR b,1*/
                                seteab((temp>>1)|(temp&0x80)); if (abrt) break;
                                setznp8((temp>>1)|(temp&0x80));
                                if (temp&1) flags|=C_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
                                break;

//                                default:
//                                pclog("Bad D0 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;
                        case 0xD1:
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
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x08: /*ROR w,1*/
                                seteaw((tempw>>1)|(tempw<<15)); if (abrt) break;
                                if (tempw&1) flags|=C_FLAG;
                                else         flags&=~C_FLAG;
                                tempw>>=1;
                                if (flags&C_FLAG) tempw|=0x8000;
                                if ((tempw^(tempw>>1))&0x4000) flags|=V_FLAG;
                                else                           flags&=~V_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
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
                                cycles -= (mod == 3) ? 2 : 7;
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
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x20: case 0x30: /*SHL w,1*/
                                seteaw(tempw<<1);       if (abrt) break;
                                setznp16(tempw<<1);
                                if (tempw&0x8000) flags|=C_FLAG;
                                if ((tempw^(tempw<<1))&0x8000) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x28: /*SHR w,1*/
                                seteaw(tempw>>1);       if (abrt) break;
                                setznp16(tempw>>1);
                                if (tempw&1) flags|=C_FLAG;
                                if (tempw&0x8000) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x38: /*SAR w,1*/
                                seteaw((tempw>>1)|(tempw&0x8000)); if (abrt) break;
                                setznp16((tempw>>1)|(tempw&0x8000));
                                if (tempw&1) flags|=C_FLAG;
                                cycles -= (mod == 3) ? 2 : 7;
                                break;

                                default:
                                pclog("Bad D1 opcode %02X\n",rmdat&0x38);
                        }
                        break;

                        case 0xD2:
                        fetchea();
                        temp=geteab();  if (abrt) break;
                        c=CL&31;
//                        cycles-=c;
                        if (!c) break;
//                        if (c>7) pclog("Shiftb %i %02X\n",rmdat&0x38,c);
                        cycles -= c;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;
                                case 0x20: case 0x30: /*SHL b,CL*/
                                seteab(temp<<c); if (abrt) break;
                                setznp8(temp<<c);
                                if ((temp<<(c-1))&0x80) flags|=C_FLAG;
                                if (((temp<<c)^(temp<<(c-1)))&0x80) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
                                break;
                                case 0x28: /*SHR b,CL*/
                                seteab(temp>>c); if (abrt) break;
                                setznp8(temp>>c);
                                if ((temp>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && temp&0x80) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

//                                default:
//                                pclog("Bad D2 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0xD3:
                        fetchea();
                        tempw=geteaw(); if (abrt) break;
                        c=CL&31;
                        if (!c) break;
                        cycles -= c;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

                                case 0x20: case 0x30: /*SHL w,CL*/
                                seteaw(tempw<<c); if (abrt) break;
                                setznp16(tempw<<c);
                                if ((tempw<<(c-1))&0x8000) flags|=C_FLAG;
                                if (((tempw<<c)^(tempw<<(c-1)))&0x8000) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

                                case 0x28:            /*SHR w,CL*/
                                seteaw(tempw>>c); if (abrt) break;
                                setznp16(tempw>>c);
                                if ((tempw>>(c-1))&1) flags|=C_FLAG;
                                if (c==1 && tempw&0x8000) flags|=V_FLAG;
                                cycles -= (mod == 3) ? 5 : 8;
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
                                cycles -= (mod == 3) ? 5 : 8;
                                break;

//                                default:
//                                pclog("Bad D3 opcode %02X\n",rmdat&0x38);
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0xD4: /*AAM*/
                        tempws=readmemb(cs,pc); pc++;
                        AH=AL/tempws;
                        AL%=tempws;
                        setznp16(AX);
                        cycles -= 16;
                        break;
                        case 0xD5: /*AAD*/
                        tempws=readmemb(cs,pc); pc++;
                        AL=(AH*tempws)+AL;
                        AH=0;
                        setznp16(AX);
                        cycles -= 14;
                        break;
                        case 0xD6: /*SETALC*/
                        AL=(flags&C_FLAG)?0xFF:0;
                        cycles -= 3;
                        break;
                        case 0xD7: /*XLAT*/
                        addr=(BX+AL)&0xFFFF;
                        temp=readmemb(ds,addr); if (abrt) break;
                        AL=temp;
                        cycles -= 5;
                        break;
                        case 0xD9: case 0xDA: case 0xDB: case 0xDD:     /*ESCAPE*/
                        case 0xD8:
                        case 0xDC:
                        case 0xDE:
                        case 0xDF:
                        if ((msw & 6) == 4)
                        {
                                pc=oldpc;
                                pmodeint(7,0);
                                cycles -= 40;
                        }
                        else
                        {
                                fetchea();
                        }
                        break;

                        case 0xE0: /*LOOPNE*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        CX--;
                        if (CX && !(flags&Z_FLAG)) { pc+=offset; cycles -= 4; cycles -= 2; }
                        cycles -= 4;
                        break;
                        case 0xE1: /*LOOPE*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        CX--;
                        if (CX && (flags&Z_FLAG)) { pc+=offset; cycles -= 4; cycles -= 2; }
                        cycles -= 4;
                        break;
                        case 0xE2: /*LOOP*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        CX--;
                        if (CX) { pc+=offset; cycles -= 4; cycles -= 2; }
                        cycles -= 4;
                        break;
                        case 0xE3: /*JCXZ*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        if (!CX) { pc+=offset; cycles -= 4; cycles -= 2; }
                        cycles-=4;
                        break;

                        case 0xE4: /*IN AL*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        pc++;
                        AL=inb(temp);
                        cycles -= 5;
                        break;
                        case 0xE5: /*IN AX*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        checkio_perm(temp+1);
                        pc++;
                        AX=inw(temp);
                        cycles -= 5;
                        break;
                        case 0xE6: /*OUT AL*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        pc++;
                        outb(temp,AL);
                        cycles -= 3;
                        break;
                        case 0xE7: /*OUT AX*/
                        temp=readmemb(cs,pc);
                        checkio_perm(temp);
                        checkio_perm(temp+1);
                        pc++;
                        outw(temp,AX);
                        cycles -= 3;
                        break;

                        case 0xE8: /*CALL rel 16*/
                        tempw=getword(); if (abrt) break;
                        if (ssegs) ss=oldss;
                        writememw(ss,((SP-2)&0xFFFF),pc); if (abrt) break;
                        SP-=2;
                        pc+=(int16_t)tempw;
                        cycles -= 7;
                        break;
                        case 0xE9: /*JMP rel 16*/
                        tempw=getword(); if (abrt) break;
                        pc+=(int16_t)tempw;
                        cycles -= 7;
                        break;
                        case 0xEA: /*JMP far*/
                        addr=getword();
                        tempw=getword(); if (abrt) { if (output==3) pclog("JMP ABRT\n"); break; }
                        oxpc=pc;
                        pc=addr;
                        loadcsjmp(tempw,oxpc);
                        cycles -= 11;
                        break;
                        case 0xEB: /*JMP rel*/
                        offset=(int8_t)readmemb(cs,pc); pc++;
                        pc+=offset;
                        cycles -= 7;
                        break;
                        case 0xEC: /*IN AL,DX*/
                        checkio_perm(DX);
                        AL=inb(DX);
                        cycles -= 5;
                        break;
                        case 0xED: /*IN AX,DX*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        AX=inw(DX);
                        cycles -= 5;
                        break;
                        case 0xEE: /*OUT DX,AL*/
                        checkio_perm(DX);
                        outb(DX,AL);
                        cycles -= 4;
                        break;
                        case 0xEF: /*OUT DX,AX*/
                        checkio_perm(DX);
                        checkio_perm(DX+1);
                        outw(DX,AX);
                        cycles -= 4;
                        break;

                        case 0xF0: /*LOCK*/
                        break;

                        case 0xF2: /*REPNE*/
                        rep386(0);
                        break;
                        case 0xF3: /*REPE*/
                        rep386(1);
                        break;

                        case 0xF4: /*HLT*/
                        inhlt=1;
                        pc--;
                        cycles -= 2;
/*                        if (!(flags & I_FLAG))
                        {
                                pclog("Complete HLT\n");
                                dumpregs();
                                exit(-1);
                        }*/
                        break;
                        case 0xF5: /*CMC*/
                        flags^=C_FLAG;
                        cycles -= 2;
                        break;

                        case 0xF6:
                        fetchea();
                        temp=geteab(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*TEST b,#8*/
                                temp2=readmemb(cs,pc); pc++; if (abrt) break;
//                                pclog("TEST %02X,%02X\n",temp,temp2);
/*                        if (cs==0x700 && !temp && temp2==0x10)
                        {
                                dumpregs();
                                exit(-1);
                        }*/
                                temp&=temp2;
                                setznp8(temp);
                                cycles -= (mod == 3) ? 3 : 6;
                                break;
                                case 0x10: /*NOT b*/
                                temp=~temp;
                                seteab(temp);
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x18: /*NEG b*/
                                setsub8(0,temp);
                                temp=0-temp;
                                seteab(temp);
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x20: /*MUL AL,b*/
//                                setznp8(AL);
                                AX=AL*temp;
//                                if (AX) flags&=~Z_FLAG;
//                                else    flags|=Z_FLAG;
                                if (AH) flags|=(C_FLAG|V_FLAG);
                                else    flags&=~(C_FLAG|V_FLAG);
                                cycles -= (mod == 3) ? 13 : 16;
                                break;
                                case 0x28: /*IMUL AL,b*/
//                                setznp8(AL);
                                tempws=(int)((int8_t)AL)*(int)((int8_t)temp);
                                AX=tempws&0xFFFF;
//                                if (AX) flags&=~Z_FLAG;
//                                else    flags|=Z_FLAG;
                                if (AH && AH!=0xFF) flags|=(C_FLAG|V_FLAG);
                                else                flags&=~(C_FLAG|V_FLAG);
                                cycles -= (mod == 3) ? 13 : 16;
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
                                        flags|=0x8D5; /*Not a Cyrix*/
                                }
                                else
                                {
                                        pclog("DIVb BY 0 %04X:%04X\n",cs>>4,pc);
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
//                                        pclog("Div by zero %04X:%04X %02X %02X\n",cs>>4,pc,0xf6,0x30);
//                                        dumpregs();
//                                        exit(-1);
                                }
                                cycles -= (mod == 3) ? 14 : 17;
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
                                cycles -= (mod == 3) ? 17 : 20;
                                break;

                                default:
                                pclog("Bad F6 opcode %02X\n",rmdat&0x38);
                                x86illegal();
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0xF7:
                        fetchea();
                        tempw=geteaw(); if (abrt) break;
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*TEST w*/
                                tempw2=getword(); if (abrt) break;
//                                if (output==3) pclog("TEST %04X %04X\n",tempw,tempw2);
                                setznp16(tempw&tempw2);
                                cycles -= (mod == 3) ? 3 : 6;
                                break;
                                case 0x10: /*NOT w*/
                                seteaw(~tempw);
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x18: /*NEG w*/
                                setsub16(0,tempw);
                                tempw=0-tempw;
                                seteaw(tempw);
                                cycles -= (mod == 3) ? 2 : 7;
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
                                cycles -= (mod == 3) ? 21 : 24;
                                break;
                                case 0x28: /*IMUL AX,w*/
                                templ=(int)((int16_t)AX)*(int)((int16_t)tempw);
                                AX=templ&0xFFFF;
                                DX=templ>>16;
                                if (DX && DX!=0xFFFF) flags|=(C_FLAG|V_FLAG);
                                else                  flags&=~(C_FLAG|V_FLAG);
                                cycles -= (mod == 3) ? 21 : 24;
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
//                                        AX=DX=0;
//                                        break;
                                        pclog("DIVw BY 0 %04X:%04X %i\n",cs>>4,pc,ins);
//                                        dumpregs();
//                                        exit(-1);
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
                                cycles -= (mod == 3) ? 22 : 25;
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
                                cycles -= (mod == 3) ? 25 : 28;
                                break;

                                default:
                                pclog("Bad F7 opcode %02X\n",rmdat&0x38);
                                x86illegal();
//                                dumpregs();
//                                exit(-1);
                        }
                        break;

                        case 0xF8: /*CLC*/
                        flags&=~C_FLAG;
                        cycles -= 2;
                        break;
                        case 0xF9: /*STC*/
//                        pclog("STC %04X\n",pc);
                        flags|=C_FLAG;
                        cycles -= 2;
                        break;
                        case 0xFA: /*CLI*/
                        if (!IOPLp)
                        {
                                x86gpf(NULL,0);
                        }
                        else
                           flags&=~I_FLAG;
                        cycles -= 3;
                        break;
                        case 0xFB: /*STI*/
                        if (!IOPLp)
                        {
                                x86gpf(NULL,0);
                        }
                        else
                           flags|=I_FLAG;
                        cycles -= 2;
                        break;
                        case 0xFC: /*CLD*/
                        flags&=~D_FLAG;
                        cycles -= 2;
                        break;
                        case 0xFD: /*STD*/
                        flags|=D_FLAG;
                        cycles -= 2;
                        break;

                        case 0xFE: /*INC/DEC b*/
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
                        cycles -= (mod == 3) ? 2 : 7;
                        break;

                        case 0xFF:
                        fetchea();
                        switch (rmdat&0x38)
                        {
                                case 0x00: /*INC w*/
                                tempw=geteaw();  if (abrt) break;
                                seteaw(tempw+1); if (abrt) break;
                                setadd16nc(tempw,1);
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x08: /*DEC w*/
                                tempw=geteaw();  if (abrt) break;
                                seteaw(tempw-1); if (abrt) break;
                                setsub16nc(tempw,1);
                                cycles -= (mod == 3) ? 2 : 7;
                                break;
                                case 0x10: /*CALL*/
                                tempw=geteaw();
                                if (abrt) break;
                                if (ssegs) ss=oldss;
                                writememw(ss,(SP-2)&0xFFFF,pc); if (abrt) break;
                                SP-=2;
                                pc=tempw;
                                cycles -= (mod == 3) ? 7 : 11;
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
                                if (msw&1) loadcscall(tempw2);
                                else       loadcs(tempw2);
                                optype=0;
                                if (abrt) break;
                                oldss=ss;
                                writememw(ss,(SP-2)&0xFFFF,tempw3);
                                writememw(ss,((SP-4)&0xFFFF),templ2);
                                SP-=4;
                                cycles -= 16;
                                break;
                                case 0x20: /*JMP*/
                                tempw=geteaw(); if (abrt) break;
                                pc=tempw;
                                cycles -= (mod == 3) ? 7 : 11;
                                break;
                                case 0x28: /*JMP far*/
                                oxpc=pc;
                                tempw=readmemw(easeg,eaaddr);
                                tempw2=readmemw(easeg,eaaddr+2); if (abrt) break;
                                pc=tempw;                                
                                loadcsjmp(tempw2,oxpc); if (abrt) break;
                                cycles -= 15;
                                break;
                                case 0x30: /*PUSH w*/
                                tempw=geteaw(); if (abrt) break;
                                if (ssegs) ss=oldss;
                                writememw(ss,((SP-2)&0xFFFF),tempw); if (abrt) break;
                                SP-=2;
                                cycles -= (mod==3) ? 3 : 5;
                                break;

                                default:
                                pclog("Bad FF opcode %02X\n",rmdat&0x38);
                                x86illegal();
                                //dumpregs();
                                //exit(-1);
                        }
                        break;

                        default:
//                        pc--;
//                        cycles-=8;
//                        break;
                        pclog("Bad opcode %02X at %04X:%04X from %04X:%04X %08X\n",opcode,cs>>4,pc,old8>>16,old8&0xFFFF,old82);
                        x86illegal();
//                        dumpregs();
//                        exit(-1);
                }
                opcodeend:
                pc&=0xFFFF;

//                output = 3;
/*                output = 3;
                
                if (pc == 0xcdd && CS == 0xf000)
                {
                        dumpregs();
                        exit(-1);
                }*/
                //if (ins == 20768972) output = 3;
                
                if (ssegs)
                {
                        ds=oldds; _ds.limit=olddslimit; _ds.limitw=olddslimitw;
                        ss=oldss; _ss.limit=oldsslimit; _ss.limitw=oldsslimitw;
                        ssegs=0;
                }
                if (abrt)
                {
                        tempi = abrt;
                        abrt = 0;
                        x86_doabrt(tempi);
                        if (abrt)
                        {
                                abrt = 0;
                                CS = oldcs;
                                pc = oldpc;
                                pclog("Double fault\n");
//                                dumpregs();
//                                exit(-1);
                                pmodeint(8, 0);
                                if (abrt)
                                {
                                        abrt = 0;
                                        softresetx86();
                                        pclog("Triple fault - reset\n");
                                }
                        }
                }
                cycdiff-=cycles;

                pit.c[0]-=cycdiff;
                pit.c[1]-=cycdiff;
                if (ppi.pb&1)         pit.c[2]-=cycdiff;

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
                        disctime-=cycdiff/4;
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
                rtctime-=cycdiff;
                if (rtctime<0)
                {
                        nvr_rtc();
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
                if (trap && (flags&T_FLAG) && !noint)
                {
                        if (msw&1)
                        {
                                pmodeint(1,0);
                                cycles -= 40;
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
                                cycles -= 23;
                        }
                }
                else if ((flags&I_FLAG) && (((pic.pend&~pic.mask)&~pic.mask2) || ((pic2.pend&~pic2.mask)&~pic2.mask2)) && !ssegs && !noint)
                {
//                        pclog("Test %02X %02X %02X\n", pic.pend, pic.mask, pic.mask2);
                        temp=picinterrupt();
                        if (temp!=0xFF)
                        {
//                                pclog("286 int %02X %02X %02X %02X\n",temp, pic.pend, pic.mask, pic.mask2);
                                if (inhlt) pc++;
//                                intcount++;
                                if (msw&1)
                                {
                                        pmodeint(temp,0);
                                        cycles -= 40;
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
                                        pc=readmemw(0,addr);
                                        loadcs(readmemw(0,addr+2));
                                        cycles -= 23;
                                }
                                inint=1;
                        }
                }

/*                if (pc==0xCC32 && es>0x180000)
                {
                        pc=0xCBEB;
//                        output=1;
//                        timetolive=500000;
                }*/

                if (noint) noint=0;
                ins++;
                insc++;
                
//                if (ins == 25000000) output = 3;
/*                if (timetolive)
                {
                        timetolive--;
                        if (!timetolive)
                        {
                                dumpregs();
                                exit(-1);
                        } //output=0;
                }*/
                keybsenddelay--;
                if (keybsenddelay<1)
                {
                        keybsenddelay = 2500;
                        keyboard_poll();
                }
                //output = 3;
        }
}
//#endif
