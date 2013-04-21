#include "ibm.h"

float volslog[16]=
{
	0.00000f,0.59715f,0.75180f,0.94650f,
        1.19145f,1.50000f,1.88835f,2.37735f,
        2.99295f,3.76785f,4.74345f,5.97165f,
        7.51785f,9.46440f,11.9194f,15.0000f
};

float psgcount[4],psglatch[4];
int psgstat[4];
int snlatch[4],sncount[4];
int snfreqlo[4],snfreqhi[4];
int snvol[4];
int curfreq[4];
uint32_t snshift[2]={0x8000,0x8000};

#define SNCLOCK (2386360>>5)
uint8_t snnoise;

void initpsg()
{
        int c;
        for (c=0;c<4;c++)
        {
                psgcount[c]=psglatch[c]=100000;
                psgstat[c]=0;
        }
}

#define PSGCONST (32000.0/48000.0)

void getpsg(signed short *p, int size)
{
//        printf("Getpsg %08X %i\n",p,size);
//        return;
        int c,d;
        for (c=0;c<size;c++)
        {
                p[c]=0;
                for (d=1;d<4;d++)
                {
                        if (psgstat[d]) p[c]+=volslog[snvol[d]]*170;
                        else            p[c]-=volslog[snvol[d]]*170;
                        psgcount[d]-=(512*PSGCONST);
                        while (psgcount[d]<=0.0 && psglatch[d]>1.0)
                        {
                                psgcount[d]+=psglatch[d];
                                psgstat[d]^=1;
                        }
                        if (psgcount[d]<=0.0) psgcount[d]=1.0;
                }
                d=(snnoise&4)?0:1;
                if (snshift[d]&1) p[c]+=volslog[snvol[0]]*170;
                psgcount[0]-=(512*PSGCONST);
                while (psgcount[0]<=0.0 && psglatch[0]>1.0)
                {
                        psgcount[0]+=psglatch[0];
                        snshift[1]>>=1;
                        if (!snshift[1]) snshift[1]=0x4000;
                        if ((snshift[0]&1)^((snshift[0]&4)?1:0)^((snshift[0]&0x8000)?1:0))
                           snshift[0]|=0x10000;
                        snshift[0]>>=1;
                }
                if (psgcount[0]<=0.0) psgcount[0]=1.0;
        }
}

int lasttone;
uint8_t firstdat;
void writepsg(uint16_t addr, uint8_t data)
{
        int c;
        int freq;
        pclog("Write PSG %02X\n", data);
        if (data&0x80)
        {
                firstdat=data;
                switch (data&0x70)
                {
                        case 0:
                        snfreqlo[3]=data&0xF;
                        lasttone=3;
                        break;
                        case 0x10:
                        data&=0xF;
                        snvol[3]=0xF-data;
                        break;
                        case 0x20:
                        snfreqlo[2]=data&0xF;
                        lasttone=2;
                        break;
                        case 0x30:
                        data&=0xF;
                        snvol[2]=0xF-data;
                        break;
                        case 0x40:
                        snfreqlo[1]=data&0xF;
                        lasttone=1;
                        break;
                        case 0x50:
                        data&=0xF;
                        snvol[1]=0xF-data;
                        break;
                        case 0x60:
                        if ((data&3)!=(snnoise&3)) sncount[0]=0;
                        snnoise=data&0xF;
                        if ((data&3)==3)
                        {
                                curfreq[0]=curfreq[1]>>4;
                                snlatch[0]=snlatch[1];
                        }
                        else
                        {
                                switch (data&3)
                                {
                                        case 0:
                                        snlatch[0]=128<<7;
                                        curfreq[0]=SNCLOCK/256;
                                        snlatch[0]=0x400;
                                        sncount[0]=0;
                                        break;
                                        case 1:
                                        snlatch[0]=256<<7;
                                        curfreq[0]=SNCLOCK/512;
                                        snlatch[0]=0x800;
                                        sncount[0]=0;
                                        break;
                                        case 2:
                                        snlatch[0]=512<<7;
                                        curfreq[0]=SNCLOCK/1024;
                                        snlatch[0]=0x1000;
                                        sncount[0]=0;
                                        break;
                                        case 3:
                                        snlatch[0]=snlatch[1];
                                        sncount[0]=0;
                                }
                                if (snnoise&4) snlatch[0]<<=1;
                        }
                        break;
                        case 0x70:
                        data&=0xF;
                        snvol[0]=0xF-data;
                        break;
                }
        }
        else
        {
                if ((firstdat&0x70)==0x60)
                {
                        if ((data&3)!=(snnoise&3)) sncount[0]=0;
                        snnoise=data&0xF;
                        if ((data&3)==3)
                        {
                                curfreq[0]=curfreq[1]>>4;
                                snlatch[0]=snlatch[1];
//                                printf("SN 0 latch %04X\n",snlatch[0]);
                        }
                        else
                        {
                                switch (data&3)
                                {
                                        case 0:
                                        snlatch[0]=128<<7;
                                        curfreq[0]=SNCLOCK/256;
                                        snlatch[0]=0x400;
                                        sncount[0]=0;
                                        break;
                                        case 1:
                                        snlatch[0]=256<<7;
                                        curfreq[0]=SNCLOCK/512;
                                        snlatch[0]=0x800;
                                        sncount[0]=0;
                                        break;
                                        case 2:
                                        snlatch[0]=512<<7;
                                        curfreq[0]=SNCLOCK/1024;
                                        snlatch[0]=0x1000;
                                        sncount[0]=0;
                                        break;
                                        case 3:
                                        snlatch[0]=snlatch[1];
//                                        printf("SN 0 latch %04X\n",snlatch[0]);
                                        sncount[0]=0;
                                }
                                if (snnoise&4) snlatch[0]<<=1;
                        }
                        return;
                }
                else
                {
                        snfreqhi[lasttone]=data&0x3F;
                        freq=snfreqlo[lasttone]|(snfreqhi[lasttone]<<4);
                        snlatch[lasttone]=freq<<6;
                }
        }
        for (c=0;c<4;c++) psglatch[c]=(float)snlatch[c];
}

void psg_init()
{
        pclog("psg_init\n");
        io_sethandler(0x00C0, 0x0001, NULL, NULL, NULL, writepsg, NULL, NULL);
}
