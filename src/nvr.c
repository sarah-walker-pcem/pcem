#include <stdio.h>
#include "ibm.h"
#include "io.h"
#include "nvr.h"
#include "pic.h"
#include "timer.h"

int oldromset;
int nvrmask=63;
uint8_t nvrram[128];
int nvraddr;

int nvr_dosave = 0;

static int nvr_onesec_time = 0, nvr_onesec_cnt = 0;

void getnvrtime()
{
	time_get(nvrram);
}

void nvr_recalc()
{
        int c;
        int newrtctime;
        c=1<<((nvrram[0xA]&0xF)-1);
        newrtctime=(int)(RTCCONST * c * (1 << TIMER_SHIFT));
        if (rtctime>newrtctime) rtctime=newrtctime;
}

void nvr_rtc(void *p)
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
        nvrram[0xC] |= 0x40;
        if (nvrram[0xB]&0x40)
        {
                nvrram[0xC]|=0x80;
                if (AMSTRAD) picint(2);
                else         picint(0x100);
//                pclog("RTC int\n");
        }
}

void nvr_onesec(void *p)
{
        nvr_onesec_cnt++;
        if (nvr_onesec_cnt >= 100)
        {
                nvr_onesec_cnt = 0;
                nvrram[0xC] |= 0x10;
                if (nvrram[0xB] & 0x10)
                {
                        nvrram[0xC] |= 0x80;
                        if (AMSTRAD) picint(2);
                        else         picint(0x100);
                }
//                pclog("RTC onesec\n");
        }
        nvr_onesec_time += (int)(10000 * TIMER_USEC);
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
                case ROM_PC1512:      f = romfopen("nvr/pc1512.nvr",      "rb"); break;
                case ROM_PC1640:      f = romfopen("nvr/pc1640.nvr",      "rb"); break;
                case ROM_PC200:       f = romfopen("nvr/pc200.nvr",       "rb"); break;
                case ROM_PC2086:      f = romfopen("nvr/pc2086.nvr",      "rb"); break;
                case ROM_PC3086:      f = romfopen("nvr/pc3086.nvr",      "rb"); break;                
                case ROM_IBMAT:       f = romfopen("nvr/at.nvr",          "rb"); break;
                case ROM_IBMPS1_2011: f = romfopen("nvr/ibmps1_2011.nvr", "rb"); /*nvrmask = 127; */break;
                case ROM_CMDPC30:     f = romfopen("nvr/cmdpc30.nvr",     "rb"); nvrmask = 127; break;
                case ROM_AMI286:      f = romfopen("nvr/ami286.nvr",      "rb"); nvrmask = 127; break;
                case ROM_DELL200:     f = romfopen("nvr/dell200.nvr",     "rb"); nvrmask = 127; break;
                case ROM_IBMAT386:    f = romfopen("nvr/at386.nvr",       "rb"); nvrmask = 127; break;
                case ROM_DESKPRO_386: f = romfopen("nvr/deskpro386.nvr",  "rb"); break;
                case ROM_ACER386:     f = romfopen("nvr/acer386.nvr",     "rb"); nvrmask = 127; break;
                case ROM_MEGAPC:      f = romfopen("nvr/megapc.nvr",      "rb"); nvrmask = 127; break;
                case ROM_AMI386:      f = romfopen("nvr/ami386.nvr",      "rb"); nvrmask = 127; break;
                case ROM_AMI486:      f = romfopen("nvr/ami486.nvr",      "rb"); nvrmask = 127; break;
                case ROM_WIN486:      f = romfopen("nvr/win486.nvr",      "rb"); nvrmask = 127; break;
                case ROM_PCI486:      f = romfopen("nvr/hot-433.nvr",     "rb"); nvrmask = 127; break;
                case ROM_SIS496:      f = romfopen("nvr/sis496.nvr",      "rb"); nvrmask = 127; break;
                case ROM_430VX:       f = romfopen("nvr/430vx.nvr",       "rb"); nvrmask = 127; break;
                case ROM_REVENGE:     f = romfopen("nvr/revenge.nvr",     "rb"); nvrmask = 127; break;
                case ROM_ENDEAVOR:    f = romfopen("nvr/endeavor.nvr",    "rb"); nvrmask = 127; break;
                case ROM_PX386:       f = romfopen("nvr/px386.nvr",       "rb"); nvrmask = 127; break;
                case ROM_DTK386:      f = romfopen("nvr/dtk386.nvr",      "rb"); nvrmask = 127; break;
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
                case ROM_PC1512:      f = romfopen("nvr/pc1512.nvr",      "wb"); break;
                case ROM_PC1640:      f = romfopen("nvr/pc1640.nvr",      "wb"); break;
                case ROM_PC200:       f = romfopen("nvr/pc200.nvr",       "wb"); break;
                case ROM_PC2086:      f = romfopen("nvr/pc2086.nvr",      "wb"); break;
                case ROM_PC3086:      f = romfopen("nvr/pc3086.nvr",      "wb"); break;
                case ROM_IBMAT:       f = romfopen("nvr/at.nvr",          "wb"); break;
                case ROM_IBMPS1_2011: f = romfopen("nvr/ibmps1_2011.nvr", "wb"); break;
                case ROM_CMDPC30:     f = romfopen("nvr/cmdpc30.nvr",     "wb"); break;                
                case ROM_AMI286:      f = romfopen("nvr/ami286.nvr",      "wb"); break;
                case ROM_DELL200:     f = romfopen("nvr/dell200.nvr",     "wb"); break;
                case ROM_IBMAT386:    f = romfopen("nvr/at386.nvr",       "wb"); break;
                case ROM_DESKPRO_386: f = romfopen("nvr/deskpro386.nvr",  "wb"); break;
                case ROM_ACER386:     f = romfopen("nvr/acer386.nvr",     "wb"); break;
                case ROM_MEGAPC:      f = romfopen("nvr/megapc.nvr",      "wb"); break;
                case ROM_AMI386:      f = romfopen("nvr/ami386.nvr",      "wb"); break;
                case ROM_AMI486:      f = romfopen("nvr/ami486.nvr",      "wb"); break;
                case ROM_WIN486:      f = romfopen("nvr/win486.nvr",      "wb"); break;
                case ROM_PCI486:      f = romfopen("nvr/hot-433.nvr",     "wb"); break;
                case ROM_SIS496:      f = romfopen("nvr/sis496.nvr",      "wb"); break;
                case ROM_430VX:       f = romfopen("nvr/430vx.nvr",       "wb"); break;
                case ROM_REVENGE:     f = romfopen("nvr/revenge.nvr",     "wb"); break;
                case ROM_ENDEAVOR:    f = romfopen("nvr/endeavor.nvr",    "wb"); break;
                case ROM_PX386:       f = romfopen("nvr/px386.nvr",       "wb"); break;
                case ROM_DTK386:      f = romfopen("nvr/dtk386.nvr",      "wb"); break;
                default: return;
        }
        fwrite(nvrram,128,1,f);
        fclose(f);
}

void nvr_init()
{
        io_sethandler(0x0070, 0x0002, readnvr, NULL, NULL, writenvr, NULL, NULL,  NULL);
        timer_add(nvr_rtc, &rtctime, TIMER_ALWAYS_ENABLED, NULL);
        timer_add(nvr_onesec, &nvr_onesec_time, TIMER_ALWAYS_ENABLED, NULL);
}
