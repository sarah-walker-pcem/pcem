#include <stdio.h>
#include <windows.h>
#include "ibm.h"
#include "io.h"
#include "nvr.h"
#include "pic.h"
#include "timer.h"

int oldromset;
int nvrmask=63;
uint8_t nvrram[128];
int nvraddr;

SYSTEMTIME systemtime;

int nvr_dosave = 0;

void getnvrtime()
{
        int c,d;
        uint8_t baknvr[10];
//        memset(nvrram,0,10);
//        return;
        memcpy(baknvr,nvrram,10);
        GetLocalTime(&systemtime);
        d=systemtime.wSecond%10;
        c=systemtime.wSecond/10;
        nvrram[0]=d|(c<<4);
        d=systemtime.wMinute%10;
        c=systemtime.wMinute/10;
        nvrram[2]=d|(c<<4);
        d=systemtime.wHour%10;
        c=systemtime.wHour/10;
        nvrram[4]=d|(c<<4);
        d=systemtime.wDayOfWeek%10;
        c=systemtime.wDayOfWeek/10;
        nvrram[6]=d|(c<<4);
        d=systemtime.wDay%10;
        c=systemtime.wDay/10;
        nvrram[7]=d|(c<<4);
        d=systemtime.wMonth%10;
        c=systemtime.wMonth/10;
        nvrram[8]=d|(c<<4);
        d=systemtime.wYear%10;
        c=(systemtime.wYear/10)%10;
        nvrram[9]=d|(c<<4);
        if (baknvr[0]!=nvrram[0] ||
            baknvr[2]!=nvrram[2] ||
            baknvr[4]!=nvrram[4] ||
            baknvr[6]!=nvrram[6] ||
            baknvr[7]!=nvrram[7] ||
            baknvr[8]!=nvrram[8] ||
            baknvr[9]!=nvrram[9]) nvrram[0xA]|=0x80;
}

void nvr_recalc()
{
        int c;
        int newrtctime;
        c=1<<((nvrram[0xA]&0xF)-1);
        newrtctime=(int)(RTCCONST * c * (1 << TIMER_SHIFT));
        if (rtctime>newrtctime) rtctime=newrtctime;
}

void nvr_rtc()
{
        int c;
        if (!(nvrram[0xA]&0xF))
        {
                rtctime=0x7fffffff;
                return;
        }
        c=1<<((nvrram[0xA]&0xF)-1);
        rtctime += (int)(RTCCONST * c * (1 << TIMER_SHIFT));
//        pclog("RTCtime now %f\n",rtctime);
        nvrram[0xC]=0x40;
        if (nvrram[0xB]&0x40)
        {
                nvrram[0xC]|=0x80;
                if (AMSTRAD) picint(2);
                else         picint(0x100);
//                pclog("RTC int\n");
        }
}

void writenvr(uint16_t addr, uint8_t val, void *priv)
{
        int c;
//        printf("Write NVR %03X %02X %02X %04X:%04X %i\n",addr,nvraddr,val,cs>>4,pc,ins);
        if (addr&1)
        {
//                if (nvraddr == 0x33) pclog("NVRWRITE33 %02X %04X:%04X %i\n",val,CS,pc,ins);
                if (nvraddr >= 0xe && nvrram[nvraddr] != val) 
                   nvr_dosave = 1;
                if (nvraddr!=0xC && nvraddr!=0xD) nvrram[nvraddr]=val;
                
                if (nvraddr==0xA)
                {
//                        pclog("NVR rate %i\n",val&0xF);
                        if (val&0xF)
                        {
                                c=1<<((val&0xF)-1);
                                rtctime += (int)(RTCCONST * c * (1 << TIMER_SHIFT));
                        }
                        else
                           rtctime = 0x7fffffff;
                }
        }
        else        nvraddr=val&nvrmask;
}

uint8_t readnvr(uint16_t addr, void *priv)
{
        uint8_t temp;
//        printf("Read NVR %03X %02X %02X %04X:%04X\n",addr,nvraddr,nvrram[nvraddr],cs>>4,pc);
        if (addr&1)
        {
                if (nvraddr<=0xA) getnvrtime();
                if (nvraddr==0xD) nvrram[0xD]|=0x80;
                if (nvraddr==0xA)
                {
                        temp=nvrram[0xA];
                        nvrram[0xA]&=~0x80;
                        return temp;
                }
                if (nvraddr==0xC)
                {
                        if (AMSTRAD) picintc(2);
                        else         picintc(0x100);
                        temp=nvrram[0xC];
                        nvrram[0xC]=0;
                        return temp;
                }
//                if (AMIBIOS && nvraddr==0x36) return 0;
//                if (nvraddr==0xA) nvrram[0xA]^=0x80;
                return nvrram[nvraddr];
        }
        return nvraddr;
}

void loadnvr()
{
        FILE *f;
        int c;
        nvrmask=63;
        oldromset=romset;
        switch (romset)
        {
                case ROM_PC1512:   f = romfopen("pc1512.nvr",  "rb"); break;
                case ROM_PC1640:   f = romfopen("pc1640.nvr",  "rb"); break;
                case ROM_PC200:    f = romfopen("pc200.nvr",   "rb"); break;
                case ROM_PC2086:   f = romfopen("pc2086.nvr",  "rb"); break;
                case ROM_PC3086:   f = romfopen("pc3086.nvr",  "rb"); break;                
                case ROM_IBMAT:    f = romfopen("at.nvr"    ,  "rb"); break;
                case ROM_CMDPC30:  f = romfopen("cmdpc30.nvr", "rb"); nvrmask = 127; break;                
                case ROM_AMI286:   f = romfopen("ami286.nvr",  "rb"); nvrmask = 127; break;
                case ROM_DELL200:  f = romfopen("dell200.nvr", "rb"); nvrmask = 127; break;
                case ROM_IBMAT386: f = romfopen("at386.nvr"  , "rb"); nvrmask = 127; break;
                case ROM_ACER386:  f = romfopen("acer386.nvr", "rb"); nvrmask = 127; break;
                case ROM_MEGAPC:   f = romfopen("megapc.nvr",  "rb"); nvrmask = 127; break;
                case ROM_AMI386:   f = romfopen("ami386.nvr",  "rb"); nvrmask = 127; break;
                case ROM_AMI486:   f = romfopen("ami486.nvr",  "rb"); nvrmask = 127; break;
                case ROM_WIN486:   f = romfopen("win486.nvr",  "rb"); nvrmask = 127; break;
                case ROM_PCI486:   f = romfopen("hot-433.nvr", "rb"); nvrmask = 127; break;
                case ROM_430VX:    f = romfopen("hot-433.nvr", "rb"); nvrmask = 127; break;
                default: return;
        }
        if (!f)
        {
                memset(nvrram,0xFF,128);
                return;
        }
        fread(nvrram,128,1,f);
        fclose(f);
        nvrram[0xA]=6;
        nvrram[0xB]=0;
        c=1<<((6&0xF)-1);
        rtctime += (int)(RTCCONST * c * (1 << TIMER_SHIFT));
}
void savenvr()
{
        FILE *f;
        switch (oldromset)
        {
                case ROM_PC1512:   f = romfopen("pc1512.nvr" , "wb"); break;
                case ROM_PC1640:   f = romfopen("pc1640.nvr" , "wb"); break;
                case ROM_PC200:    f = romfopen("pc200.nvr"  , "wb"); break;
                case ROM_PC2086:   f = romfopen("pc2086.nvr" , "wb"); break;
                case ROM_PC3086:   f = romfopen("pc3086.nvr" , "wb"); break;
                case ROM_IBMAT:    f = romfopen("at.nvr"     , "wb"); break;
                case ROM_CMDPC30:  f = romfopen("cmdpc30.nvr", "wb"); break;                
                case ROM_AMI286:   f = romfopen("ami286.nvr" , "wb"); break;
                case ROM_DELL200:  f = romfopen("dell200.nvr", "wb"); break;
                case ROM_IBMAT386: f = romfopen("at386.nvr"  , "wb"); break;
                case ROM_ACER386:  f = romfopen("acer386.nvr", "wb"); break;
                case ROM_MEGAPC:   f = romfopen("megapc.nvr" , "wb"); break;
                case ROM_AMI386:   f = romfopen("ami386.nvr" , "wb"); break;
                case ROM_AMI486:   f = romfopen("ami486.nvr" , "wb"); break;
                case ROM_WIN486:   f = romfopen("win486.nvr" , "wb"); break;
                case ROM_PCI486:   f = romfopen("hot-433.nvr", "wb"); break;                
                case ROM_430VX:    f = romfopen("hot-433.nvr", "wb"); break;
                default: return;
        }
        fwrite(nvrram,128,1,f);
        fclose(f);
}

void nvr_init()
{
        io_sethandler(0x0070, 0x0002, readnvr, NULL, NULL, writenvr, NULL, NULL,  NULL);
        timer_add(nvr_rtc, &rtctime, TIMER_ALWAYS_ENABLED, NULL);
}
