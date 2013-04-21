#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ibm.h"

extern int ins;
extern int timetolive;
int output;
//#define GUSRATECONST (44100.0/48000.0)
uint8_t *gusram;

struct
{
        int global;
        uint32_t addr,dmaaddr;
        int voice;
        uint32_t start[32],end[32],cur[32];
        uint32_t startx[32],endx[32],curx[32];
        uint32_t rstart[32],rend[32],rcur[32];
        uint16_t freq[32];
        uint16_t rfreq[32];
        uint8_t ctrl[32];
        uint8_t rctrl[32];
        int curvol[32];
        int t1on,t2on;
        uint8_t tctrl;
        uint16_t t1,t2,t1l,t2l;
        uint8_t irqstatus,irqstatus2;
        uint8_t adcommand;
        int waveirqs[32],rampirqs[32];
        int voices;
        uint8_t dmactrl;
} gus;

double vol16bit[4096];

void initgus()
{
        int c;
        for (c=0;c<32;c++)
        {
                gus.ctrl[c]=1;
                gus.rctrl[c]=1;
                gus.rfreq[c]=63*512;
        }
        gusram=malloc(1024*1024);

	double out = 1.0;
	for (c=4095;c>=0;c--) {
		vol16bit[c]=out;//(float)c/4095.0;//out;
		out/=1.002709201;		/* 0.0235 dB Steps */
	}

	printf("Top volume %f %f %f %f\n",vol16bit[4095],vol16bit[3800],vol16bit[3000],vol16bit[2048]);
	gus.voices=14;

}

void dumpgus()
{
/*        FILE *f=fopen("gusram.dmp","wb");
        fwrite(gusram,1024*1024,1,f);
        fclose(f);*/
}

void pollgusirqs()
{
        int c;
        gus.irqstatus&=~0x60;
        for (c=0;c<32;c++)
        {
                if (gus.waveirqs[c])
                {
//                        gus.waveirqs[c]=0;
                        gus.irqstatus2=0x60|c;
                        gus.irqstatus|=0x20;
//                        printf("Voice IRQ %i %02X %i\n",c,gus.irqstatus2,ins);
                        picintlevel(0x20);
//                        output=3;
//                        timetolive=5000;
                        return;
                }
                if (gus.rampirqs[c])
                {
//                        gus.rampirqs[c]=0;
                        gus.irqstatus2=0xA0|c;
                        gus.irqstatus|=0x40;
//                        printf("Ramp IRQ %i %02X %i\n",c,gus.irqstatus2,ins);
                        picintlevel(0x20);
                        return;
                }
        }
        gus.irqstatus2=0xE0;
//        gus.irqstatus&=~0x20;
        if (!gus.irqstatus) picintc(0x20);
}

int gusirqnext=0;

void writegus(uint16_t addr, uint8_t val)
{
        int c,d;
//        printf("Write GUS %04X %02X %04X:%04X\n",addr,val,CS,pc);
        switch (addr)
        {
                case 0x342: /*Voice select*/
                gus.voice=val&31;
                break;
                case 0x343: /*Global select*/
                gus.global=val;
                break;
                case 0x344: /*Global low*/
//                if (gus.global!=0x43 && gus.global!=0x44) printf("Writing register %02X %02X %02X\n",gus.global,gus.voice,val);
                switch (gus.global)
                {
                        case 0: /*Voice control*/
//                        if (val&1 && !(gus.ctrl[gus.voice]&1)) printf("Voice on %i\n",gus.voice);
                        gus.ctrl[gus.voice]=val;
                        break;
                        case 1: /*Frequency control*/
                        gus.freq[gus.voice]=(gus.freq[gus.voice]&0xFF00)|val;
                        break;
                        case 2: /*Start addr high*/
                        gus.startx[gus.voice]=(gus.startx[gus.voice]&0xF807F)|(val<<7);
                        gus.start[gus.voice]=(gus.start[gus.voice]&0x1F00FFFF)|(val<<16);
//                        printf("Write %i start %08X %08X\n",gus.voice,gus.start[gus.voice],gus.startx[gus.voice]);
                        break;
                        case 3: /*Start addr low*/
                        gus.start[gus.voice]=(gus.start[gus.voice]&0x1FFFFF00)|val;
//                        printf("Write %i start %08X %08X\n",gus.voice,gus.start[gus.voice],gus.startx[gus.voice]);
                        break;
                        case 4: /*End addr high*/
                        gus.endx[gus.voice]=(gus.endx[gus.voice]&0xF807F)|(val<<7);
                        gus.end[gus.voice]=(gus.end[gus.voice]&0x1F00FFFF)|(val<<16);
//                        printf("Write %i end %08X %08X\n",gus.voice,gus.end[gus.voice],gus.endx[gus.voice]);
                        break;
                        case 5: /*End addr low*/
                        gus.end[gus.voice]=(gus.end[gus.voice]&0x1FFFFF00)|val;
//                        printf("Write %i end %08X %08X\n",gus.voice,gus.end[gus.voice],gus.endx[gus.voice]);
                        break;

                        case 0x6: /*Ramp frequency*/
                        gus.rfreq[gus.voice] = (int)( (double)((val & 63)*512)/(double)(1 << (3*(val >> 6))));
//                        printf("RFREQ %02X %i %i %f\n",val,gus.voice,gus.rfreq[gus.voice],(double)(val & 63)/(double)(1 << (3*(val >> 6))));
                        break;

                        case 0x9: /*Current volume*/
                        gus.curvol[gus.voice]=gus.rcur[gus.voice]=(gus.rcur[gus.voice]&0x1FE0000)|(val<<9);
//                        printf("Vol %i is %04X\n",gus.voice,gus.curvol[gus.voice]);
                        break;

                        case 0xA: /*Current addr high*/
                        gus.cur[gus.voice]=(gus.cur[gus.voice]&0x1F00FFFF)|(val<<16);
gus.curx[gus.voice]=(gus.curx[gus.voice]&0xF807F00)|((val<<7)<<8);
//                      gus.cur[gus.voice]=(gus.cur[gus.voice]&0x0F807F00)|((val<<7)<<8);
//                        printf("Write %i cur %08X\n",gus.voice,gus.cur[gus.voice],gus.curx[gus.voice]);
                        break;
                        case 0xB: /*Current addr low*/
                        gus.cur[gus.voice]=(gus.cur[gus.voice]&0x1FFFFF00)|val;
//                        printf("Write %i cur %08X\n",gus.voice,gus.cur[gus.voice],gus.curx[gus.voice]);
                        break;

                        case 0x42: /*DMA address low*/
                        gus.dmaaddr=(gus.dmaaddr&0xFF000)|(val<<4);
                        break;

                        case 0x43: /*Address low*/
                        gus.addr=(gus.addr&0xFFF00)|val;
                        break;
                        case 0x45: /*Timer control*/
//                        printf("Timer control %02X\n",val);
                        gus.tctrl=val;
                        break;
                }
                break;
                case 0x345: /*Global high*/
//                if (gus.global!=0x43 && gus.global!=0x44) printf("HWriting register %02X %02X %02X %04X:%04X\n",gus.global,gus.voice,val,CS,pc);
                switch (gus.global)
                {
                        case 0: /*Voice control*/
                        if (!(val&1) && gus.ctrl[gus.voice]&1)
                        {
//                                printf("Voice on %i - start %05X end %05X freq %04X\n",gus.voice,gus.start[gus.voice],gus.end[gus.voice],gus.freq[gus.voice]);
//                                if (val&0x40) gus.cur[gus.voice]=gus.end[gus.voice]<<8;
//                                else          gus.cur[gus.voice]=gus.start[gus.voice]<<8;
                        }
                        if (val&2) val|=1;
                        gus.waveirqs[gus.voice]=val&0x80;
                        gus.ctrl[gus.voice]=val&0x7F;
                        pollgusirqs();
                        break;
                        case 1: /*Frequency control*/
                        gus.freq[gus.voice]=(gus.freq[gus.voice]&0xFF)|(val<<8);
                        break;
                        case 2: /*Start addr high*/
                        gus.startx[gus.voice]=(gus.startx[gus.voice]&0x07FFF)|(val<<15);
                        gus.start[gus.voice]=(gus.start[gus.voice]&0x00FFFFFF)|((val&0x1F)<<24);
//                        printf("Write %i start %08X %08X %02X\n",gus.voice,gus.start[gus.voice],gus.startx[gus.voice],val);
                        break;
                        case 3: /*Start addr low*/
                        gus.startx[gus.voice]=(gus.startx[gus.voice]&0xFFF80)|(val&0x7F);
                        gus.start[gus.voice]=(gus.start[gus.voice]&0x1FFF00FF)|(val<<8);
//                        printf("Write %i start %08X %08X\n",gus.voice,gus.start[gus.voice],gus.startx[gus.voice]);
                        break;
                        case 4: /*End addr high*/
                        gus.endx[gus.voice]=(gus.endx[gus.voice]&0x07FFF)|(val<<15);
                        gus.end[gus.voice]=(gus.end[gus.voice]&0x00FFFFFF)|((val&0x1F)<<24);
//                        printf("Write %i end %08X %08X %02X\n",gus.voice,gus.end[gus.voice],gus.endx[gus.voice],val);
                        break;
                        case 5: /*End addr low*/
                        gus.endx[gus.voice]=(gus.endx[gus.voice]&0xFFF80)|(val&0x7F);
                        gus.end[gus.voice]=(gus.end[gus.voice]&0x1FFF00FF)|(val<<8);
//                        printf("Write %i end %08X %08X\n",gus.voice,gus.end[gus.voice],gus.endx[gus.voice]);
                        break;

                        case 0x6: /*Ramp frequency*/
                        gus.rfreq[gus.voice] = (int)( (double)((val & 63)*512)/(double)(1 << (3*(val >> 6))));
//                        printf("RFREQ %02X %i %i %f %i\n",val,gus.voice,gus.rfreq[gus.voice],(double)(val & 63)/(double)(1 << (3*(val >> 6))),ins);
                        break;
                        case 0x7: /*Ramp start*/
                        gus.rstart[gus.voice]=val<<17;
                        break;
                        case 0x8: /*Ramp end*/
                        gus.rend[gus.voice]=val<<17;
                        break;
                        case 0x9: /*Current volume*/
                        gus.curvol[gus.voice]=gus.rcur[gus.voice]=(gus.rcur[gus.voice]&0x1FE00)|(val<<17);
//                        printf("Vol %i is %04X\n",gus.voice,gus.curvol[gus.voice]);
                        break;

                        case 0xA: /*Current addr high*/
                        gus.cur[gus.voice]=(gus.cur[gus.voice]&0x00FFFFFF)|((val&0x1F)<<24);
gus.curx[gus.voice]=(gus.curx[gus.voice]&0x07FFF00)|((val<<15)<<8);
//                        printf("Write %i cur %08X %08X %02X\n",gus.voice,gus.cur[gus.voice],gus.curx[gus.voice],val);
//                      gus.cur[gus.voice]=(gus.cur[gus.voice]&0x007FFF00)|((val<<15)<<8);
                        break;
                        case 0xB: /*Current addr low*/
                        gus.cur[gus.voice]=(gus.cur[gus.voice]&0x1FFF00FF)|(val<<8);
gus.curx[gus.voice]=(gus.curx[gus.voice]&0xFFF8000)|((val&0x7F)<<8);
//                      gus.cur[gus.voice]=(gus.cur[gus.voice]&0x0FFF8000)|((val&0x7F)<<8);
//                        printf("Write %i cur %08X %08X\n",gus.voice,gus.cur[gus.voice],gus.curx[gus.voice]);
                        break;
                        case 0xD: /*Ramp control*/
                        if (val&2) val|=1;
                        gus.rampirqs[gus.voice]=val&0x80;
                        gus.rctrl[gus.voice]=val&0x7F;
                        pollgusirqs();
//                        printf("Ramp control %02i %02X %02X %i\n",gus.voice,val,gus.rampirqs[gus.voice],ins);
                        break;

                        case 0xE:
                        gus.voices=(val&63)+1;
                        if (gus.voices>32) gus.voices=32;
                        if (gus.voices<14) gus.voices=14;
                        gus.global=val;
//                        printf("GUS voices %i\n",val&31);
                        break;

                        case 0x41: /*DMA*/
                        if (val&1)
                        {
//                                printf("DMA start! %05X %02X\n",gus.dmaaddr,val);
                                c=0;
                                while (c<65536)
                                {
                                        d=readdma3();
                                        if (d==-1) break;
                                        if (val&0x80) d^=0x80;
                                        gusram[gus.dmaaddr]=d;
                                        gus.dmaaddr++;
                                        gus.dmaaddr&=0xFFFFF;
                                        c++;
                                }
//                                printf("Transferred %i bytes\n",c);
                                gus.dmactrl=val&~0x40;
                                if (val&0x20) gusirqnext=1;
//                                exit(-1);
                        }
                        break;

                        case 0x42: /*DMA address low*/
                        gus.dmaaddr=(gus.dmaaddr&0xFF0)|(val<<12);
                        break;

                        case 0x43: /*Address low*/
                        gus.addr=(gus.addr&0xF00FF)|(val<<8);
                        break;
                        case 0x44: /*Address high*/
                        gus.addr=(gus.addr&0xFFFF)|((val<<16)&0xF0000);
                        break;
                        case 0x45: /*Timer control*/
                        if (!(val&4)) gus.irqstatus&=~4;
                        if (!(val&8)) gus.irqstatus&=~8;
//                        printf("Timer control %02X\n",val);
/*                        if ((val&4) && !(gus.tctrl&4))
                        {
                                gus.t1=gus.t1l;
                                gus.t1on=1;
                        }*/
                        gus.tctrl=val;
                        break;
                        case 0x46: /*Timer 1*/
                        gus.t1=gus.t1l=val;
                        gus.t1on=1;
//                        printf("GUS timer 1 %i\n",val);
                        break;
                        case 0x47: /*Timer 2*/
                        gus.t2=gus.t2l=val<<2;
                        gus.t2on=1;
//                        printf("GUS timer 2 %i\n",val);
                        break;
                }
                break;
                case 0x347: /*DRAM access*/
                gusram[gus.addr]=val;
//                pclog("GUS RAM write %05X %02X\n",gus.addr,val);
                gus.addr&=0xFFFFF;
                break;
                case 0x248: case 0x388: gus.adcommand=val; break;
        }
}

uint8_t readgus(uint16_t addr)
{
        uint8_t val;
//        /*if (addr!=0x246) */printf("Read GUS %04X %04X(%06X):%04X %02X\n",addr,CS,cs,pc,gus.global);
//        output=3;
        switch (addr)
        {
                case 0x240: return 0;
                case 0x246: /*IRQ status*/
                val=gus.irqstatus;
//                printf("246 status %02X\n",val);
//                gus.irqstatus=0;
//                if (gus.irqstatus2==0xE0) picintc(0x20);
                return val;
                case 0x24A:
                return gus.adcommand;
                case 0x24B: case 0x24F: return 0;
                case 0x340: /*MIDI status*/
                case 0x341: /*MIDI data*/
                return 0;
                case 0x342: return gus.voice;
                case 0x343: return gus.global;
                case 0x344: /*Global low*/
//                /*if (gus.global!=0x43 && gus.global!=0x44) */printf("Reading register %02X %02X\n",gus.global,gus.voice);
                switch (gus.global)
                {
                        case 0x82: /*Start addr high*/
                        return gus.start[gus.voice]>>16;
                        case 0x83: /*Start addr low*/
                        return gus.start[gus.voice]&0xFF;

                        case 0x89: /*Current volume*/
                        return gus.rcur[gus.voice]>>9;
                        case 0x8A: /*Current addr high*/
                        return gus.cur[gus.voice]>>16;
                        case 0x8B: /*Current addr low*/
                        return gus.cur[gus.voice]&0xFF;

                        case 0x8F: /*IRQ status*/
                        val=gus.irqstatus2;
//                        pclog("Read IRQ status - %02X\n",val);
                        gus.rampirqs[gus.irqstatus2&0x1F]=0;
                        gus.waveirqs[gus.irqstatus2&0x1F]=0;
                        pollgusirqs();
                        return val;
                }
                //fatal("Bad GUS global low read %02X\n",gus.global);
                break;
                case 0x345: /*Global high*/
//                /*if (gus.global!=0x43 && gus.global!=0x44) */printf("HReading register %02X %02X\n",gus.global,gus.voice);
                switch (gus.global)
                {
                        case 0x80: /*Voice control*/
                        return gus.ctrl[gus.voice]|(gus.waveirqs[gus.voice]?0x80:0);

                        case 0x82: /*Start addr high*/
                        return gus.start[gus.voice]>>24;
                        case 0x83: /*Start addr low*/
                        return gus.start[gus.voice]>>8;

                        case 0x89: /*Current volume*/
                        return gus.rcur[gus.voice]>>17;

                        case 0x8A: /*Current addr high*/
                        return gus.cur[gus.voice]>>24;
                        case 0x8B: /*Current addr low*/
                        return gus.cur[gus.voice]>>8;

                        case 0x8D:
//                                pclog("Read ramp control %02X %08X %08X  %08X %08X\n",gus.rctrl[gus.voice]|(gus.rampirqs[gus.voice]?0x80:0),gus.rcur[gus.voice],gus.rfreq[gus.voice],gus.rstart[gus.voice],gus.rend[gus.voice]);
                        return gus.rctrl[gus.voice]|(gus.rampirqs[gus.voice]?0x80:0);

                        case 0x8F: /*IRQ status*/
                        val=gus.irqstatus2;
//                        pclog("Read IRQ status - %02X\n",val);
                        gus.rampirqs[gus.irqstatus2&0x1F]=0;
                        gus.waveirqs[gus.irqstatus2&0x1F]=0;
                        pollgusirqs();
                        return val;

                        case 0x41: /*DMA control*/
                        val=gus.dmactrl|((gus.irqstatus&0x80)?0x40:0);
                        gus.irqstatus&=~0x80;
                        return val;
                        case 0x45: /*Timer control*/
                        return gus.tctrl;
                        case 0x49: /*Sampling control*/
                        return 0;
                }
                //fatal("Bad GUS global high read %02X\n",gus.global);
                break;
                case 0x346: return 0;
                case 0x347: /*DRAM access*/
                val=gusram[gus.addr];
//                pclog("GUS RAM read %05X %02X\n",gus.addr,val);
//                output=3;
                gus.addr&=0xFFFFF;
                return val;
                case 0x349: return 0;
        }
//        printf("Bad GUS read %04X! %02X\n",addr,gus.global);
//        exit(-1);
        return 0;
}

void pollgus()
{
        if (gus.t1on)
        {
                gus.t1++;
                if (gus.t1>=0xFF)
                {
//                        gus.t1on=0;
                        gus.t1=gus.t1l;
                        if (gus.tctrl&4)
                        {
                                picintlevel(0x20);
                                gus.irqstatus|=4;
//                                printf("GUS T1 IRQ!\n");
                        }
                }
        }
        if (gusirqnext)
        {
                gusirqnext=0;
                gus.irqstatus|=0x80;
                picintlevel(0x20);
        }
}

void pollgus2()
{
        if (gus.t2on)
        {
                gus.t2++;
                if (gus.t2>=(0xFF<<2))
                {
//                        gus.t2on=0;
                        gus.t2=gus.t2l;
                        if (gus.tctrl&8)
                        {
                                picintlevel(0x20);
                                gus.irqstatus|=8;
//                                printf("GUS T2 IRQ!\n");
                        }
                }
        }
        if (gusirqnext)
        {
                gusirqnext=0;
                gus.irqstatus|=0x80;
                picintlevel(0x20);
        }
}

float gusfreqs[]=
{
        44100,41160,38587,36317,34300,32494,30870,29400,28063,26843,25725,24696,
        23746,22866,22050,21289,20580,19916,19293
};

int16_t gusbufferx[65536];
int guspos=0;
void getgus(int16_t *p, int count)
{
        memcpy(p,gusbufferx,count*4);
//        printf("Get %i samples %i\n",guspos,count);
        guspos=0;
        pollgusirqs();
}

void pollgussamp()
{
        uint32_t addr;
        int c,d;
        int16_t v;
        int32_t vl;
        int16_t p[2];
        float GUSRATECONST;
        if (guspos>65500) return;
//        return;
        if (gus.voices<14) GUSRATECONST=44100.0/48000.0;
        else               GUSRATECONST=gusfreqs[gus.voices-14]/48000.0;
//        printf("Voices %i freq %f\n",gus.voices,GUSRATECONST*48000.0);
//        for (c=0;c<count;c++)
//        {
                p[0]=p[1]=0;
                for (d=0;d<32;d++)
                {
                        if (!(gus.ctrl[d]&1))
                        {
                                if (gus.ctrl[d]&4)
                                {
                                        addr=gus.cur[d]>>9;
                                        addr=(addr&0xC0000)|((addr<<1)&0x3FFFE);
                                        if (!(gus.freq[d]>>10)) /*Interpolate*/
                                        {
                                                vl=(int16_t)(int8_t)((gusram[(addr+1)&0xFFFFF]^0x80)-0x80)*(511-(gus.cur[d]&511));
                                                vl+=(int16_t)(int8_t)((gusram[(addr+3)&0xFFFFF]^0x80)-0x80)*(gus.cur[d]&511);
                                                v=vl>>9;
                                        }
                                        else
                                           v=(int16_t)(int8_t)((gusram[(addr+1)&0xFFFFF]^0x80)-0x80);
                                }
                                else
                                {
                                        if (!(gus.freq[d]>>10)) /*Interpolate*/
                                        {
                                                vl=((int8_t)((gusram[(gus.cur[d]>>9)&0xFFFFF]^0x80)-0x80))*(511-(gus.cur[d]&511));
                                                vl+=((int8_t)((gusram[((gus.cur[d]>>9)+1)&0xFFFFF]^0x80)-0x80))*(gus.cur[d]&511);
                                                v=vl>>9;
                                        }
                                        else
                                           v=(int16_t)(int8_t)((gusram[(gus.cur[d]>>9)&0xFFFFF]^0x80)-0x80);
                                }
//                                   v=(int16_t)((float)((gusram[(gus.cur[d]>>9)&0xFFFFF]^0x80)-0x80)*32.0*vol16bit[(gus.rcur[d]>>13)&4095]);
                                if ((gus.rcur[d]>>13)>4095) v=(int16_t)(float)(v)*24.0*vol16bit[4095];
                                else                        v=(int16_t)(float)(v)*24.0*vol16bit[(gus.rcur[d]>>13)&4095];
//                                if (!d) printf("%08X  %08X %08X  %05X  %08X %08X  %04X %f %04X %08X %i\n",gus.cur[d],gus.start[d],gus.end[d],gus.cur[d]>>9,gus.startx[d],gus.endx[d],gus.rcur[d],vol16bit[0],v,gus.freq[d],ins);
//                                if (!d)
//                                {
                                        p[0]+=v;
                                        p[1]+=v;
//                                }
//                                printf("Data from %08X\n",gus.cur[d]>>8);
                                if (gus.ctrl[d]&0x40)
                                {
                                        gus.cur[d]-=(gus.freq[d]>>1)*GUSRATECONST;
                                        if (gus.cur[d]<=gus.start[d])
                                        {
                                                if (!(gus.rctrl[d]&4))
                                                {
                                                        if (!(gus.ctrl[d]&8)) gus.ctrl[d]|=1;
                                                        else if (gus.ctrl[d]&0x10) gus.ctrl[d]^=0x40;
                                                        gus.cur[d]=(gus.ctrl[d]&0x40)?gus.end[d]:gus.start[d];
                                                }
                                                if (gus.ctrl[d]&0x20) gus.waveirqs[d]=1;
                                        }
                                }
                                else
                                {
                                        gus.cur[d]+=(gus.freq[d]>>1)*GUSRATECONST;
//                                        pclog("GUS add %08X %f\n",gus.freq[d],GUSRATECONST);
                                        if (gus.cur[d]>=gus.end[d])
                                        {
                                                if (!(gus.rctrl[d]&4))
                                                {
//                                                        gus.ctrl[d]|=1;
                                                        if (!(gus.ctrl[d]&8)) gus.ctrl[d]|=1;
                                                        else if (gus.ctrl[d]&0x10) gus.ctrl[d]^=0x40;
                                                        gus.cur[d]=(gus.ctrl[d]&0x40)?gus.end[d]:gus.start[d];
                                                }
                                                if (gus.ctrl[d]&0x20) gus.waveirqs[d]=1;
                                        }
                                }
                        }
                        if (!(gus.rctrl[d]&1))
                        {
                                if (gus.rctrl[d]&0x40)
                                {
                                        gus.rcur[d]-=gus.rfreq[d]*GUSRATECONST*16;
//                                        printf("RCUR- %i %i %i %i %i\n",d,gus.rfreq[d],gus.rcur[d],gus.rstart[d],gus.rend[d]);
                                        if (gus.rcur[d]<=gus.rstart[d])
                                        {
                                                if (gus.rctrl[d]&8)    gus.rcur[d]=gus.rend[d]<<8;
                                                else                   gus.rctrl[d]|=1;
                                                if (gus.rctrl[d]&0x20)
                                                {
                                                        gus.rampirqs[d]=1;
//                                                        pclog("Causing ramp IRQ %02X\n",gus.rctrl[d]);
                                                }
                                        }
                                }
                                else
                                {
                                        gus.rcur[d]+=gus.rfreq[d]*GUSRATECONST*16;
//                                        printf("RCUR+ %i %08X %08X %08X %08X\n",d,gus.rfreq[d],gus.rcur[d],gus.rstart[d],gus.rend[d]);
                                        if (gus.rcur[d]>=gus.rend[d])
                                        {
                                                if (gus.rctrl[d]&8) gus.rcur[d]=gus.rstart[d]<<8;
                                                else                gus.rctrl[d]|=1;
                                                if (gus.rctrl[d]&0x20)
                                                {
                                                        gus.rampirqs[d]=1;
//                                                        pclog("Causing ramp IRQ %02X\n",gus.rctrl[d]);
                                                }
                                        }
                                }
                        }
                }
                gusbufferx[guspos++]=p[0];
                gusbufferx[guspos++]=p[1];
                if (guspos==65536) guspos=0;
//        }
        pollgusirqs();
}

void gus_init()
{
        io_sethandler(0x0240, 0x0010, readgus, NULL, NULL, writegus, NULL, NULL);
        io_sethandler(0x0340, 0x0010, readgus, NULL, NULL, writegus, NULL, NULL);
        io_sethandler(0x0388, 0x0001, NULL,    NULL, NULL, writegus, NULL, NULL);
}
