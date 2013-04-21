#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "mame/fmopl.h"
#include "mame/ymf262.h"

/*Interfaces between PCem and the actual Adlib emulator*/

static int adlib_inited = 0;
int adlibpos=0;
void *YM3812[2];
void *YMF262;
int fm_timers[2][2],fm_timers_enable[2][2];

void adlib_write(uint16_t a, uint8_t v)
{
//        printf("Adlib write %04X %02X %i\n",a,v,sbtype);
        if (!sbtype) return;
        if (sbtype<SBPRO && a<0x224) return;
        switch (a)
        {
                case 0x220: case 0x221:
                if (sbtype<SBPRO2) ym3812_write(YM3812[0],a,v);
                else               ymf262_write(YMF262,a,v);
                break;
                case 0x222: case 0x223:
                if (sbtype<SBPRO2) ym3812_write(YM3812[1],a,v);
                else               ymf262_write(YMF262,a,v);
                break;
                case 0x228: case 0x229: case 0x388: case 0x389:
                if (sbtype<SBPRO2)
                {
                        ym3812_write(YM3812[0],a,v);
                        ym3812_write(YM3812[1],a,v);
                }
                else
                        ymf262_write(YMF262,a,v);
                break;
        }
}

uint8_t adlib_read(uint16_t a)
{
        uint8_t temp;
//        printf("Adlib read %04X\n",a);
        if (sbtype>=SBPRO2)
        {
                switch (a)
                {
                        case 0x220: case 0x221:
                        case 0x222: case 0x223:
                        case 0x228: case 0x229:
                        case 0x388: case 0x389:
                        temp=ymf262_read(YMF262,a);
//                        pclog("YMF262 read %03X %02X\n",a,temp);
                        cycles-=(int)(isa_timing * 8);
                        return temp;
                }
        }
        if (!sbtype) return 0xFF;
        switch (a)
        {
                case 0x220: case 0x221:
                if (sbtype<SBPRO) return 0xFF;
                case 0x228: case 0x229:
                case 0x388: case 0x389:
                cycles-=(int)(isa_timing * 8);
                return ym3812_read(YM3812[0],a);
                case 0x222: case 0x223:
                if (sbtype<SBPRO) return 0xFF;
                return ym3812_read(YM3812[1],a);
        }
/*        if (sbtype<SBPRO && a<0x224) return 0xFF;
        if (a==0x222) return adlibstat2;
        if (!(a&1)) return adlibstat;
        return 0;*/
}

signed short *ad_bufs[4];
int16_t ad_filtbuf[2]={0,0};
void getadlib(signed short *bufl, signed short *bufr, int size)
{
        int c;
        if (sbtype>=SBPRO2)
        {
                ymf262_update_one(YMF262,ad_bufs,size);
                for (c=0;c<size;c++)
                {
                        ad_filtbuf[0]=bufl[c]=(ad_bufs[0][c]/4)+((ad_filtbuf[0]*11)/16);
                        ad_filtbuf[1]=bufr[c]=(ad_bufs[1][c]/4)+((ad_filtbuf[1]*11)/16);
                }
                if (fm_timers_enable[0][0])
                {
                        fm_timers[0][0]--;
                        if (fm_timers[0][0]<0) ymf262_timer_over(YMF262,0);
                }
                if (fm_timers_enable[0][1])
                {
                        fm_timers[0][1]--;
                        if (fm_timers[0][1]<0) ymf262_timer_over(YMF262,1);
                }
        }
        else
        {
                ym3812_update_one(YM3812[0],bufl,size);
                ym3812_update_one(YM3812[1],bufr,size);
                for (c=0;c<size;c++)
                {
                        ad_filtbuf[0]=bufl[c]=(bufl[c]/4)+((ad_filtbuf[0]*11)/16);
                        ad_filtbuf[1]=bufr[c]=(bufr[c]/4)+((ad_filtbuf[1]*11)/16);
                }
                if (fm_timers_enable[0][0])
                {
                        fm_timers[0][0]--;
                        if (fm_timers[0][0]<0) ym3812_timer_over(YM3812[0],0);
                }
                if (fm_timers_enable[0][1])
                {
                        fm_timers[0][1]--;
                        if (fm_timers[0][1]<0) ym3812_timer_over(YM3812[0],1);
                }
                if (fm_timers_enable[1][0])
                {
                        fm_timers[1][0]--;
                        if (fm_timers[1][0]<0) ym3812_timer_over(YM3812[1],0);
                }
                if (fm_timers_enable[1][1])
                {
                        fm_timers[1][1]--;
                        if (fm_timers[1][1]<0) ym3812_timer_over(YM3812[1],1);
                }
        }
//        for (c=0;c<size;c++) buf[c]/=2;
}

void ym3812_timer_set_0(void *param,int timer,attotime period)
{
        fm_timers[0][timer]=period/20833;
        if (!fm_timers[0][timer]) fm_timers[0][timer]=1;
        fm_timers_enable[0][timer]=(period)?1:0;
}
void ym3812_timer_set_1(void *param,int timer,attotime period)
{
        fm_timers[1][timer]=period/20833;
        if (!fm_timers[1][timer]) fm_timers[1][timer]=1;
        fm_timers_enable[1][timer]=(period)?1:0;
}

void ymf262_timer_set(void *param,int timer,attotime period)
{
        fm_timers[0][timer]=period/20833;
        if (!fm_timers[0][timer]) fm_timers[0][timer]=1;
        fm_timers_enable[0][timer]=(period)?1:0;
}

void adlib_init()
{
        if (!adlib_inited)
        {
                ad_bufs[0]=(signed short *)malloc(48000);
                ad_bufs[1]=(signed short *)malloc(48000);
                ad_bufs[2]=(signed short *)malloc(48000);
                ad_bufs[3]=(signed short *)malloc(48000);
                
                YM3812[0]=ym3812_init((void *)NULL,3579545,48000);
                ym3812_reset_chip(YM3812[0]);
                ym3812_set_timer_handler(YM3812[0],ym3812_timer_set_0,NULL);
                YM3812[1]=ym3812_init((void *)NULL,3579545,48000);
                ym3812_reset_chip(YM3812[1]);
                ym3812_set_timer_handler(YM3812[1],ym3812_timer_set_1,NULL);
                
                YMF262=ymf262_init((void *)NULL,3579545*4,48000);
                ymf262_reset_chip(YMF262);
                ymf262_set_timer_handler(YMF262,ymf262_timer_set,NULL);
        }        
        
        adlib_inited = 1;
        
        io_sethandler(0x0388, 0x0002, adlib_read, NULL, NULL, adlib_write, NULL, NULL);
}

void adlib_reset()
{
        ym3812_reset_chip(YM3812[0]);
        ym3812_reset_chip(YM3812[1]);
        ymf262_reset_chip(YMF262);
}
