#include <stdio.h>
#include "ibm.h"

int cmsaddrs[2];
uint8_t cmsregs[2][32];
uint16_t cmslatch[12],cmsnoisefreq[12];
int cmsfreq[12];
float cmscount[12];
int cmsvol[12][2];
int cmsstat[12];
uint16_t cmsnoise[4];
int cmsnoisecount[4];
int cmsnoisetype[4];

#define CMSCONST (62500.0/44100.0)

void getcms(signed short *p, int size)
{
        int c,d;
        int ena[12],noiseena[12];
        for (c=0;c<6;c++)
        {
                ena[c]=(cmsregs[0][0x14]&(1<<c));
                ena[c+6]=(cmsregs[1][0x14]&(1<<c));
        }
        if (!(cmsregs[0][0x1C]&1))
        {
                for (c=0;c<6;c++) ena[c]=0;
        }
        if (!(cmsregs[1][0x1C]&1))
        {
                for (c=0;c<6;c++) ena[c+6]=0;
        }
        for (c=0;c<6;c++)
        {
                noiseena[c]=(cmsregs[0][0x15]&(1<<c));
                noiseena[c+6]=(cmsregs[1][0x15]&(1<<c));
        }
        if (!(cmsregs[0][0x1C]&1))
        {
                for (c=0;c<6;c++) noiseena[c]=0;
        }
        if (!(cmsregs[1][0x1C]&1))
        {
                for (c=0;c<6;c++) noiseena[c+6]=0;
        }
        for (c=0;c<4;c++)
        {
                switch (cmsnoisetype[c])
                {
                        case 0: cmsnoisefreq[c]=31250; break;
                        case 1: cmsnoisefreq[c]=15625; break;
                        case 2: cmsnoisefreq[c]=7812; break;
                        case 3: cmsnoisefreq[c]=cmsfreq[c*3]; break;
                }
        }
        for (c=0;c<(size<<1);c+=2)
        {
                p[c]=0;
                p[c+1]=0;
                for (d=0;d<12;d++)
                {
                        if (ena[d])
                        {
                                if (cmsstat[d]) p[c]  +=(cmsvol[d][0]*90);
                                if (cmsstat[d]) p[c+1]+=(cmsvol[d][1]*90);
                                cmscount[d]+=cmsfreq[d];
                                if (cmscount[d]>=(22050))
                                {
                                        cmscount[d]-=(22050);
                                        cmsstat[d]^=1;
                                }
                        }
                        else if (noiseena[d])
                        {
                                if (cmsnoise[d/3]&1) p[c]  +=(cmsvol[d][0]*90);
                                if (cmsnoise[d/3]&1) p[c+1]+=(cmsvol[d][0]*90);
                        }
                }
                for (d=0;d<4;d++)
                {
                        cmsnoisecount[d]+=cmsnoisefreq[d];
                        while (cmsnoisecount[d]>=22050)
                        {
                                cmsnoisecount[d]-=22050;
                                cmsnoise[d]<<=1;
                                if (!(((cmsnoise[d]&0x4000)>>8)^(cmsnoise[d]&0x40))) cmsnoise[d]|=1;
                        }
                }
        }
}

void writecms(uint16_t addr, uint8_t val)
{
        int voice;
        int chip=(addr&2)>>1;
        if (addr&1)
           cmsaddrs[chip]=val&31;
        else
        {
                cmsregs[chip][cmsaddrs[chip]&31]=val;
                switch (cmsaddrs[chip]&31)
                {
                        case 0x00: case 0x01: case 0x02: /*Volume*/
                        case 0x03: case 0x04: case 0x05:
                        voice=cmsaddrs[chip]&7;
                        if (chip) voice+=6;
                        cmsvol[voice][0]=val&0xF;//((val&0xF)+(val>>4))>>1;
                        cmsvol[voice][1]=val>>4;
                        break;
                        case 0x08: case 0x09: case 0x0A: /*Frequency*/
                        case 0x0B: case 0x0C: case 0x0D:
                        voice=cmsaddrs[chip]&7;
                        if (chip) voice+=6;
                        cmslatch[voice]=(cmslatch[voice]&0x700)|val;
                        cmsfreq[voice]=(15625<<(cmslatch[voice]>>8))/(511-(cmslatch[voice]&255));
                        break;
                        case 0x10: case 0x11: case 0x12: /*Octave*/
                        voice=(cmsaddrs[chip]&3)<<1;
                        if (chip) voice+=6;
                        cmslatch[voice]=(cmslatch[voice]&0xFF)|((val&7)<<8);
                        cmslatch[voice+1]=(cmslatch[voice+1]&0xFF)|((val&0x70)<<4);
                        cmsfreq[voice]=(15625<<(cmslatch[voice]>>8))/(511-(cmslatch[voice]&255));
                        cmsfreq[voice+1]=(15625<<(cmslatch[voice+1]>>8))/(511-(cmslatch[voice+1]&255));
                        break;
                        case 0x16: /*Noise*/
                        voice=chip*2;
                        cmsnoisetype[voice]=val&3;
                        cmsnoisetype[voice+1]=(val>>4)&3;
                        break;
                }
        }
}

uint8_t readcms(uint16_t addr)
{
        int chip=(addr&2)>>1;
        if (addr&1) return cmsaddrs[chip];
        return cmsregs[chip][cmsaddrs[chip]&31];
}

