/*IBM AT -
  Write B0
  Write aa55
  Expects aa55 back*/

#include <string.h>
#include "ibm.h"
#include "pit.h"
#include "video.h"
#include "cpu.h"
/*B0 to 40, two writes to 43, then two reads - value does not change!*/
/*B4 to 40, two writes to 43, then two reads - value _does_ change!*/
//Tyrian writes 4300 or 17512
int displine;

int pitsec=0;
double PITCONST;
float cpuclock;
float isa_timing, bus_timing;

int firsttime=1;
void setpitclock(float clock)
{
        float temp;
//        printf("PIT clock %f\n",clock);
        cpuclock=clock;
        PITCONST=clock/1193182.0;
        SPKCONST=clock/48000.0;
        CGACONST=(clock/(19687500.0/11.0));
        MDACONST=(clock/1813000.0);
        VGACONST1=(clock/25175000.0);
        VGACONST2=(clock/28322000.0);
        setsbclock(clock);
        SOUNDCONST=clock/200.0;
        CASCONST=PITCONST*1192;
        isa_timing = clock/8000000.0;
        bus_timing = clock/(double)cpu_busspeed;
        video_updatetiming();
//        pclog("egacycles %i egacycles2 %i temp %f clock %f\n",egacycles,egacycles2,temp,clock);
        GUSCONST=(clock/3125.0)/4.0;
        GUSCONST2=(clock/3125.0)/4.0; //Timer 2 at different rate to 1?
        video_recalctimings();
        RTCCONST=clock/32768.0;
}

//#define PITCONST (8000000.0/1193000.0)
//#define PITCONST (cpuclock/1193000.0)
int pit0;
void pit_reset()
{
        memset(&pit,0,sizeof(PIT));
        pit.l[0]=0xFFFF; pit.c[0]=0xFFFF*PITCONST;
        pit.l[1]=0xFFFF; pit.c[1]=0xFFFF*PITCONST;
        pit.l[2]=0xFFFF; pit.c[2]=0xFFFF*PITCONST;
        pit.m[0]=pit.m[1]=pit.m[2]=0;
        pit.ctrls[0]=pit.ctrls[1]=pit.ctrls[2]=0;
        pit.thit[0]=1;
        spkstat=0;
}

void clearpit()
{
        pit.c[0]=(pit.l[0]<<2);
}

float pit_timer0_freq()
{
//        pclog("PIT timer 0 freq %04X %f %f\n",pit.l[0],(float)pit.l[0],1193182.0f/(float)pit.l[0]);
        return 1193182.0f/(float)pit.l[0];
}
extern int ins;
void pit_write(uint16_t addr, uint8_t val)
{
        int t;
        uint8_t oldctrl=pit.ctrl;
        cycles -= (int)PITCONST;
        pit0=1;
//        printf("Write PIT %04X %02X %04X:%08X %i %i\n",addr,val,CS,pc,ins);
        switch (addr&3)
        {
                case 3: /*CTRL*/
                if ((val&0xC0)==0xC0)
                {
                        if (!(val&0x20))
                        {
                                if (val&2) pit.rl[0]=pit.c[0]/PITCONST;
                                if (val&4) pit.rl[1]=pit.c[1]/PITCONST;
                                if (val&8) pit.rl[2]=pit.c[2]/PITCONST;
                        }
                        return;
                }
                pit.ctrls[val>>6]=pit.ctrl=val;
                if ((val>>7)==3)
                {
                        printf("Bad PIT reg select\n");
                        return;
//                        dumpregs();
//                        exit(-1);
                }
//                printf("CTRL write %02X\n",val);
                if (!(pit.ctrl&0x30))
                {
                        pit.rl[val>>6]=pit.c[val>>6]/PITCONST;
                        if (pit.c[val>>6]<0) pit.rl[val>>6]=0;
//                        pclog("Timer latch %f %04X %04X\n",pit.c[0],pit.rl[0],pit.l[0]);
                        pit.ctrl|=0x30;
                        pit.rereadlatch[val>>6]=0;
                        pit.rm[val>>6]=3;
                }
                else
                {
                                pit.rm[val>>6]=pit.wm[val>>6]=(pit.ctrl>>4)&3;
                                pit.m[val>>6]=(val>>1)&7;
                                if (pit.m[val>>6]>5)
                                   pit.m[val>>6]&=3;
                                if (!(pit.rm[val>>6]))
                                {
                                        pit.rm[val>>6]=3;
                                        pit.rl[val>>6]=pit.c[val>>6]/PITCONST;
                                }
                                pit.rereadlatch[val>>6]=1;
                                if ((val>>6)==2) ppispeakon=speakon=(pit.m[2]==0)?0:1;
//                                pclog("ppispeakon %i\n",ppispeakon);
                }
                pit.wp=0;
                pit.thit[pit.ctrl>>6]=0;
                break;
                case 0: case 1: case 2: /*Timers*/
                t=addr&3;
//                if (t==2) ppispeakon=speakon=0;
//                pclog("Write timer %02X %i\n",pit.ctrls[t],pit.wm[t]);
                switch (pit.wm[t])
                {
                        case 1:
                        pit.l[t]=val;
                        pit.thit[t]=0;
                        pit.c[t]=pit.l[t]*PITCONST;
                        picintc(1);
                        break;
                        case 2:
                        pit.l[t]=(val<<8);
                        pit.thit[t]=0;
                        pit.c[t]=pit.l[t]*PITCONST;
                        picintc(1);
                        break;
                        case 0:
                        pit.l[t]&=0xFF;
                        pit.l[t]|=(val<<8);
                        pit.c[t]=pit.l[t]*PITCONST;
//                        pclog("%04X %f\n",pit.l[t],pit.c[t]);                        
                        pit.thit[t]=0;
                        pit.wm[t]=3;
                        picintc(1);
                        break;
                        case 3:
                        pit.l[t]&=0xFF00;
                        pit.l[t]|=val;
                        pit.wm[t]=0;
                        break;
/*
                        if (pit.wp)
                        {
                                pit.l[t]&=0xFF;
                                pit.l[t]|=(val<<8);
                                pit.c[t]=pit.l[t]*PITCONST;
                                pit.thit[t]=0;
                        }
                        else
                        {
                                pit.l[t]&=0xFF00;
                                pit.l[t]|=val;
                        }
                        pit.rl[t]=pit.l[t];
                        pit.wp^=1;
                        pit.rm[t]=3;
                                pit.rereadlatch[t]=1;
                        break;*/
                }
                speakval=(((float)pit.l[2]/(float)pit.l[0])*0x4000)-0x2000;
//                printf("Speakval now %i\n",speakval);
//                if (speakval>0x2000)
//                   printf("Speaker overflow - %i %i %04X %04X\n",pit.l[0],pit.l[2],pit.l[0],pit.l[2]);
                if (speakval>0x2000) speakval=0x2000;
                if (!pit.l[t])
                {
                        pit.l[t]|=0x10000;
                        pit.c[t]=pit.l[t]*PITCONST;
                }
                break;
        }
}

uint8_t pit_read(uint16_t addr)
{
        uint8_t temp;
        cycles -= (int)PITCONST;        
//        printf("Read PIT %04X ",addr);
        switch (addr&3)
        {
                case 0: case 1: case 2: /*Timers*/
                if (pit.rereadlatch[addr&3])// || !(pit.ctrls[addr&3]&0x30))
                {
                        pit.rereadlatch[addr&3]=0;
                        pit.rl[addr&3]=pit.c[addr&3]/PITCONST;
                        if ((pit.c[addr&3]/PITCONST)>65536) pit.rl[addr&3]=0xFFFF;
                }
                switch (pit.rm[addr&3])
                {
                        case 0:
                        temp=pit.rl[addr&3]>>8;
                        pit.rm[addr&3]=3;
                        pit.rereadlatch[addr&3]=1;
                        break;
                        case 1:
                        temp=(pit.rl[addr&3])&0xFF;
                        pit.rereadlatch[addr&3]=1;
                        break;
                        case 2:
                        temp=(pit.rl[addr&3])>>8;
                        pit.rereadlatch[addr&3]=1;
                        break;
                        case 3:
                        temp=(pit.rl[addr&3])&0xFF;
                        if (pit.m[addr&3]&0x80) pit.m[addr&3]&=7;
                        else pit.rm[addr&3]=0;
                        break;
                }
                break;
                case 3: /*Control*/
                temp=pit.ctrl;
        }
//        printf("%02X %i %i %04X:%04X\n",temp,pit.rm[addr&3],pit.wp,cs>>4,pc);
        return temp;
}

void pit_poll()
{
        pitsec++;
//                printf("Poll pit %f %f %f\n",pit.c[0],pit.c[1],pit.c[2]);
        if (pit.c[0]<1)
        {
                if (pit.m[0]==0 || pit.m[0]==4)
                {
//                        pit.c[0]&=0xFFFF;
                        pit.c[0]+=(0x10000*PITCONST);
                }
                else if (pit.m[0]==3 || pit.m[0]==2)
                {
                        if (pit.l[0]) pit.c[0]+=((float)(pit.l[0]*PITCONST));
                        else          pit.c[0]+=((float)(0x10000*PITCONST));
                }
//                pit.c[0]+=(pit.l[0]*PITCONST);
//                printf("PIT over! %f %i\n",pit.c[0],pit.m[0]);
                if (!pit.thit[0] && (pit.l[0]>0x14))
                {
//                        printf("PIT int!\n");
///                        printf("%05X %05X %02X\n",pit.c[0],pit.l[0],pit.ctrls[0]);
                        picint(1);
                }
                if (!pit.m[0] || pit.m[0]==4) pit.thit[0]=1;
//                if ((pit.ctrls[0]&0xE)==2) pit.thit[0]=1;
                pit0=0;
                pitcount++;
        }
        if (pit.c[1]<1)
        {
 //               if (output) printf("PIT1 over %02X\n",pit.m[1]);
                if (pit.m[1]==0 || pit.m[1]==4)
                {
                        pit.c[1]=0xFFFFFF*PITCONST;
                }
                else
                {
                        pit.c[1]+=(pit.l[1]*PITCONST);
                }
//                if (output) pclog("%f %04X %02X\n",pit.c[1],pit.l[1],pit.ctrls[1]);
//                printf("DMA0!\n");
                readdma0();
        }
        if (pit.c[2]<1)
        {
//                printf("PIT 2 over %i\n",pit.m[2]);
                if (!pit.m[2] || pit.m[2]==4)
                {
                        pit.c[2]+=(0x10000*PITCONST);
                        speakon^=1;
                        ppispeakon^=1;
                }
                else
                {
                        pit.c[2]+=((pit.l[2]*PITCONST)/2);
                        if (pit.l[2]>0x30) /*Some games use very high frequencies as 'speaker off'. This stops them from generating noise*/
                           speakon^=1;
                        ppispeakon^=1;
//                        printf("Speakon %i %04X %i\n",speakon,pit.l[2],pit.c[2]);
                }
//                if (pit.ctrls[2]&0xE) pit.c[2]+=(pit.l[2]*PITCONST);
//                spkstat^=0x20;
        }
}

void pit_init()
{
        io_sethandler(0x0040, 0x0004, pit_read, NULL, NULL, pit_write, NULL, NULL);
}
