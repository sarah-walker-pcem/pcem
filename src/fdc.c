#include <stdio.h>
#include <string.h>
#include "ibm.h"

#include "disc.h"
#include "dma.h"
#include "io.h"
#include "pic.h"
#include "timer.h"

extern int motoron;

/*FDC*/
typedef struct FDC
{
        uint8_t dor,stat,command,dat,st0;
        int head,track[256],sector,drive,lastdrive;
        int pos;
        uint8_t params[256];
        uint8_t res[256];
        int pnum,ptot;
        int rate;
        uint8_t specify[256];
        int eot[256];
        int lock;
        int perp;
        uint8_t config, pretrk;
        int abort;
        uint8_t format_dat[256];
        int format_state;
        int tc;
        int written;
        
        int pcjr;
        
        int watchdog_timer;
        int watchdog_count;

        int data_ready;
        int inread;
        
        int dskchg_activelow;
        int enable_3f1;
} FDC;

static FDC fdc;

void fdc_callback();
int timetolive;
//#define SECTORS 9
int lastbyte=0;
uint8_t disc_3f7;

int discmodified[2];
int discrate[2];

int discint;
void fdc_reset()
{
        fdc.stat=0x80;
        fdc.pnum=fdc.ptot=0;
        fdc.st0=0xC0;
        fdc.lock = 0;
        fdc.head = 0;
        fdc.abort = 0;
        if (!AT)
           fdc.rate=2;
//        pclog("Reset FDC\n");
}
int ins;

static void fdc_int()
{
        if (!fdc.pcjr)
                picint(1 << 6);
}

static void fdc_watchdog_poll(void *p)
{
        FDC *fdc = (FDC *)p;

        fdc->watchdog_count--;
        if (fdc->watchdog_count)
                fdc->watchdog_timer += 1000 * TIMER_USEC;
        else
        {
//                pclog("Watchdog timed out\n");
        
                fdc->watchdog_timer = 0;
                if (fdc->dor & 0x20)
                        picint(1 << 6);
        }
}

void fdc_write(uint16_t addr, uint8_t val, void *priv)
{
//        pclog("Write FDC %04X %02X %04X:%04X %i %02X %i rate=%i  %i\n",addr,val,cs>>4,pc,ins,fdc.st0,ins,fdc.rate, fdc.data_ready);
        switch (addr&7)
        {
                case 1: return;
                case 2: /*DOR*/
//                if (val == 0xD && (cs >> 4) == 0xFC81600 && ins > 769619936) output = 3;
//                printf("DOR was %02X\n",fdc.dor);
                if (fdc.pcjr)
                {
                        if ((fdc.dor & 0x40) && !(val & 0x40))
                        {
                                fdc.watchdog_timer = 1000 * TIMER_USEC;
                                fdc.watchdog_count = 1000;
                                picintc(1 << 6);
//                                pclog("watchdog set %i %i\n", fdc.watchdog_timer, TIMER_USEC);
                        }
                        if ((val & 0x80) && !(fdc.dor & 0x80))
                        {
        			timer_process();
                                disctime = 128 * (1 << TIMER_SHIFT);
                                timer_update_outstanding();
                                discint=-1;
                                fdc_reset();
                        }
                        motoron = val & 0x01;
/*                        if (motoron)
                                output = 3;
                        else
                                output = 0;*/
                }
                else
                {
                        if (val&4)
                        {
                                fdc.stat=0x80;
                                fdc.pnum=fdc.ptot=0;
                        }
                        if ((val&4) && !(fdc.dor&4))
                        {
        			timer_process();
                                disctime = 128 * (1 << TIMER_SHIFT);
                                timer_update_outstanding();
                                discint=-1;
                                fdc_reset();
                        }
			timer_process();
                        motoron = (val & 0xf0) ? 1 : 0;
			timer_update_outstanding();
                }
                fdc.dor=val;
//                printf("DOR now %02X\n",val);
                return;
                case 4:
                if (val & 0x80)
                {
			timer_process();
                        disctime = 128 * (1 << TIMER_SHIFT);
                        timer_update_outstanding();
                        discint=-1;
                        fdc_reset();
                }
                return;
                case 5: /*Command register*/
                if ((fdc.stat & 0xf0) == 0xb0)
                {
                        fdc.dat = val;
                        fdc.stat &= ~0x80;
                        break;
                }
//                if (fdc.inread)
//                        rpclog("c82c711_fdc_write : writing while inread! %02X\n", val);
//                rpclog("Write command reg %i %i\n",fdc.pnum, fdc.ptot);
                if (fdc.pnum==fdc.ptot)
                {
                        fdc.tc = 0;
                        fdc.data_ready = 0;
                        
                        fdc.command=val;
//                        pclog("Starting FDC command %02X\n",fdc.command);
                        switch (fdc.command&0x1F)
                        {
                                case 3: /*Specify*/
                                fdc.pnum=0;
                                fdc.ptot=2;
                                fdc.stat=0x90;
                                break;
                                case 4: /*Sense drive status*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                break;
                                case 5: /*Write data*/
//                                printf("Write data!\n");
                                fdc.pnum=0;
                                fdc.ptot=8;
                                fdc.stat=0x90;
                                fdc.pos=0;
//                                readflash=1;
                                break;
                                case 6: /*Read data*/
                                fdc.pnum=0;
                                fdc.ptot=8;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 7: /*Recalibrate*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                break;
                                case 8: /*Sense interrupt status*/
//                                printf("Sense interrupt status %i\n",curdrive);
                                fdc.lastdrive = fdc.drive;
//                                fdc.stat = 0x10 | (fdc.stat & 0xf);
//                                fdc_time=1024;
                                discint = 8;
                                fdc.pos = 0;
                                fdc_callback();
                                break;
                                case 10: /*Read sector ID*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 0x0d: /*Format track*/
                                fdc.pnum=0;
                                fdc.ptot=5;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                fdc.format_state = 0;
                                break;
                                case 15: /*Seek*/
                                fdc.pnum=0;
                                fdc.ptot=2;
                                fdc.stat=0x90;
                                break;
                                case 0x0e: /*Dump registers*/
                                fdc.lastdrive = fdc.drive;
                                discint = 0x0e;
                                fdc.pos = 0;
                                fdc_callback();
                                break;
                                case 0x10: /*Get version*/
                                fdc.lastdrive = fdc.drive;
                                discint = 0x10;
                                fdc.pos = 0;
                                fdc_callback();
                                break;
                                case 0x12: /*Set perpendicular mode*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 0x13: /*Configure*/
                                fdc.pnum=0;
                                fdc.ptot=3;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 0x14: /*Unlock*/
                                case 0x94: /*Lock*/
                                fdc.lastdrive = fdc.drive;
                                discint = fdc.command;
                                fdc.pos = 0;
                                fdc_callback();
                                break;

                                case 0x18:
                                fdc.stat = 0x10;
                                discint  = 0xfc;
                                fdc_callback();
                                break;

                                default:
                                fatal("Bad FDC command %02X\n",val);
//                                dumpregs();
//                                exit(-1);
                                fdc.stat=0x10;
                                discint=0xfc;
        			timer_process();
        			disctime = 200 * (1 << TIMER_SHIFT);
        			timer_update_outstanding();
                                break;
                        }
                }
                else
                {
                        fdc.params[fdc.pnum++]=val;
                        if (fdc.pnum==fdc.ptot)
                        {
//                                pclog("Got all params %02X\n", fdc.command);
                                fdc.stat=0x30;
                                discint=fdc.command&0x1F;
        			timer_process();
        			disctime = 1024 * (1 << TIMER_SHIFT);
        			timer_update_outstanding();
                                fdc.drive = fdc.params[0] & 3;
                                disc_drivesel = fdc.drive & 1;
                                switch (discint)
                                {
                                        case 5: /*Write data*/
                                        fdc.track[fdc.drive]=fdc.params[1];
                                        fdc.head=fdc.params[2];
                                        fdc.sector=fdc.params[3];
                                        fdc.eot[fdc.drive] = fdc.params[4];
                                        if (fdc.config & 0x40)
                                                disc_seek(fdc.drive, fdc.track[fdc.drive]);

                                        disc_writesector(fdc.drive, fdc.sector, fdc.track[fdc.drive], fdc.head, fdc.rate);
                                        disctime = 0;
                                        fdc.written = 0;
                                        readflash = 1;
                                        fdc.pos = 0;
                                        if (fdc.pcjr)
                                                fdc.stat = 0xb0;
//                                        ioc_fiq(IOC_FIQ_DISC_DATA);
                                        break;
                                        
                                        case 6: /*Read data*/
                                        fdc.track[fdc.drive]=fdc.params[1];
                                        fdc.head=fdc.params[2];
                                        fdc.sector=fdc.params[3];
                                        fdc.eot[fdc.drive] = fdc.params[4];
                                        if (fdc.config & 0x40)
                                                disc_seek(fdc.drive, fdc.track[fdc.drive]);

                                        disc_readsector(fdc.drive, fdc.sector, fdc.track[fdc.drive], fdc.head, fdc.rate);
                                        disctime = 0;
                                        readflash = 1;
                                        fdc.inread = 1;
                                        break;
                                        
                                        case 7: /*Recalibrate*/
                                        fdc.stat =  1 << fdc.drive;
                                        disctime = 0;
                                        disc_seek(fdc.drive, 0);
                                        break;

                                        case 0x0d: /*Format*/
                                        fdc.format_state = 1;
                                        fdc.pos = 0;
                                        fdc.stat = 0x30;
                                        break;
                                        
                                        case 0xf: /*Seek*/
                                        fdc.stat =  1 << fdc.drive;
                                        fdc.head = (fdc.params[0] & 4) ? 1 : 0;
                                        disctime = 0;
                                        disc_seek(fdc.drive, fdc.params[1]);
                                        break;
                                        
                                        case 10: /*Read sector ID*/
                                        disctime = 0;
                                        fdc.head = (fdc.params[0] & 4) ? 1 : 0;                                        
//                                        rpclog("Read sector ID %i %i\n", fdc.rate, fdc.drive);
                                        disc_readaddress(fdc.drive, fdc.track[fdc.drive], fdc.head, fdc.rate);
                                        break;
                                }
                        }
                }
                return;
                case 7:
                        if (!AT) return;
                fdc.rate=val&3;
                disc_set_rate(fdc.rate);

                disc_3f7=val;
                return;
        }
//        printf("Write FDC %04X %02X\n",addr,val);
//        dumpregs();
//        exit(-1);
}

int paramstogo=0;
uint8_t fdc_read(uint16_t addr, void *priv)
{
        uint8_t temp;
//        /*if (addr!=0x3f4) */printf("Read FDC %04X %04X:%04X %04X %i %02X %02x %i ",addr,cs>>4,pc,BX,fdc.pos,fdc.st0,fdc.stat,ins);
        switch (addr&7)
        {
                case 1: /*???*/
                if (!fdc.enable_3f1)
                        return 0xff;
//                temp=0x50;
                temp = 0x70;
                if (fdc.dor & 1)
                        temp &= ~0x40;
                else
                        temp &= ~0x20;
                break;
                case 3:
                temp = 0x20;
                break;
                case 4: /*Status*/
                temp=fdc.stat;
                break;
                case 5: /*Data*/
                fdc.stat&=~0x80;
                if ((fdc.stat & 0xf0) == 0xf0)
                {
                        temp = fdc.dat;
                        break;
                }
                if (paramstogo)
                {
                        paramstogo--;
                        temp=fdc.res[10 - paramstogo];
//                        pclog("Read param %i %02X\n",10-paramstogo,temp);
                        if (!paramstogo)
                        {
                                fdc.stat=0x80;
//                                fdc.st0=0;
                        }
                        else
                        {
                                fdc.stat|=0xC0;
//                                fdc_poll();
                        }
                }
                else
                {
                        if (lastbyte)
                                fdc.stat = 0x80;
                        lastbyte=0;
                        temp=fdc.dat;
                        fdc.data_ready = 0;
                }
                if (discint==0xA) 
		{
			timer_process();
			disctime = 1024 * (1 << TIMER_SHIFT);
			timer_update_outstanding();
		}
                fdc.stat &= 0xf0;
                break;
                case 7: /*Disk change*/
                if (fdc.dor & (0x10 << (fdc.dor & 1)))
                   temp = (disc_changed[fdc.dor & 1] || drive_empty[fdc.dor & 1])?0x80:0;
                else
                   temp = 0;
                if (fdc.dskchg_activelow)  /*PC2086/3086 seem to reverse this bit*/
                   temp ^= 0x80;
//                printf("- DC %i %02X %02X %i %i - ",fdc.dor & 1, fdc.dor, 0x10 << (fdc.dor & 1), discchanged[fdc.dor & 1], driveempty[fdc.dor & 1]);
//                discchanged[fdc.dor&1]=0;
                break;
                default:
                        temp=0xFF;
//                printf("Bad read FDC %04X\n",addr);
//                dumpregs();
//                exit(-1);
        }
//        /*if (addr!=0x3f4) */printf("%02X rate=%i  %i\n",temp,fdc.rate, fdc.data_ready);
        return temp;
}

static int fdc_reset_stat = 0;
void fdc_callback()
{
        int temp;
        disctime = 0;
//        pclog("fdc_callback %i %i\n", discint, disctime);
//        if (fdc.inread)
//                rpclog("c82c711_fdc_callback : while inread! %08X %i %02X  %i\n", discint, fdc.drive, fdc.st0, ins);
        switch (discint)
        {
                case -3: /*End of command with interrupt*/
//                if (output) printf("EOC - interrupt!\n");
//rpclog("EOC\n");
                fdc_int();
                case -2: /*End of command*/
                fdc.stat = (fdc.stat & 0xf) | 0x80;
                return;
                case -1: /*Reset*/
//rpclog("Reset\n");
                fdc_int();
                fdc_reset_stat = 4;
                return;
                case 2: /*Read track*/
/*                if (!fdc.pos)
                {
//                        printf("Read Track Side %i Track %i Sector %02X sector size %i end sector %02X %05X\n",fdc.head,fdc.track,fdc.sector,fdc.params[4],fdc.params[5],(dma.page[2]<<16)+dma.ac[2]);
                }
                if (fdc.pos<512)
                {
                        fdc.dat=disc[fdc.drive][fdc.head][fdc.track[fdc.drive]][fdc.sector-1][fdc.pos];
//                        pclog("Read %i %i %i %i %02X\n",fdc.head,fdc.track,fdc.sector,fdc.pos,fdc.dat);
                        if (fdc.pcjr)
                                fdc.stat = 0xf0;
                        else
                                dma_channel_write(2, fdc.dat);
                        timer_process();
                        disctime = 60 * (1 << TIMER_SHIFT);
                        timer_update_outstanding();
                }
                else
                {
                        disctime=0;
                        discint=-2;
//                        pclog("RT\n");
                        fdc_int();
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                        fdc.res[5]=fdc.res[2]=0;
                        fdc.res[6]=fdc.track[fdc.drive];
                        fdc.res[7]=fdc.head;
                        fdc.res[8]=fdc.sector;
                        fdc.res[9]=fdc.params[4];
                        paramstogo=7;
                        return;
                }
                fdc.pos++;
                if (fdc.pos==512 && fdc.params[5]!=1)
                {
                        fdc.pos=0;
                        fdc.sector++;
                        if (fdc.sector==SECTORS[fdc.drive]+1)
                        {
                                fdc.sector=1;
                        }
                        fdc.params[5]--;
                }*/
                return;
                case 3: /*Specify*/
                fdc.stat=0x80;
                fdc.specify[0] = fdc.params[0];
                fdc.specify[1] = fdc.params[1];
                return;
                case 4: /*Sense drive status*/
                fdc.res[10] = (fdc.params[0] & 7) | 0x28;
                if (!fdc.track[fdc.drive])
                        fdc.res[10] |= 0x10;
                if (writeprot[fdc.drive])
                        fdc.res[10] |= 0x40;

                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                paramstogo = 1;
                discint = 0;
                disctime = 0;
                return;
                case 5: /*Write data*/
                readflash = 1;
                fdc.sector++;
                if (fdc.sector > fdc.params[5])
                {
                        fdc.sector = 1;
                        if (fdc.command & 0x80)
                        {
                                fdc.head ^= 1;
                                if (!fdc.head)
                                {
                                        fdc.track[fdc.drive]++;
/*                                        if (fdc.track[fdc.drive] >= 79)
                                        {
                                                fdc.track[fdc.drive] = 79;
                                                fdc.tc = 1;
                                        }*/
                                }
                        }
                        else
                                fdc.tc = 1;
                }
                if (fdc.tc)
                {
                        discint=-2;
                        fdc_int();
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                        fdc.res[5]=fdc.res[6]=0;
                        fdc.res[7]=fdc.track[fdc.drive];
                        fdc.res[8]=fdc.head;
                        fdc.res[9]=fdc.sector;
                        fdc.res[10]=fdc.params[4];
                        paramstogo=7;
                        return;
                }                        
                disc_writesector(fdc.drive, fdc.sector, fdc.track[fdc.drive], fdc.head, fdc.rate);
//                ioc_fiq(IOC_FIQ_DISC_DATA);
                return;
                case 6: /*Read data*/
//                rpclog("Read data %i\n", fdc.tc);
                readflash = 1;
                fdc.sector++;
                if (fdc.sector > fdc.params[5])
                {
                        fdc.sector = 1;
                        if (fdc.command & 0x80)
                        {
                                fdc.head ^= 1;
                                if (!fdc.head)
                                {
                                        fdc.track[fdc.drive]++;
/*                                        if (fdc.track[fdc.drive] >= 79)
                                        {
                                                fdc.track[fdc.drive] = 79;
                                                fdc.tc = 1;
                                        }*/
                                }
                        }
                        else
                                fdc.tc = 1;
                }
                if (fdc.tc)
                {
                        fdc.inread = 0;
                        discint=-2;
                        fdc_int();
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                        fdc.res[5]=fdc.res[6]=0;
                        fdc.res[7]=fdc.track[fdc.drive];
                        fdc.res[8]=fdc.head;
                        fdc.res[9]=fdc.sector;
                        fdc.res[10]=fdc.params[4];
                        paramstogo=7;
                        return;
                }                        
                disc_readsector(fdc.drive, fdc.sector, fdc.track[fdc.drive], fdc.head, fdc.rate);
                fdc.inread = 1;
                return;

                case 7: /*Recalibrate*/
                fdc.track[fdc.drive]=0;
//                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
                fdc.st0=0x20|fdc.drive|(fdc.head?4:0);
                discint=-3;
		timer_process();
		disctime = 2048 * (1 << TIMER_SHIFT);
		timer_update_outstanding();
//                printf("Recalibrate complete!\n");
                fdc.stat = 0x80 | (1 << fdc.drive);
                return;

                case 8: /*Sense interrupt status*/
//                pclog("Sense interrupt status %i\n", fdc_reset_stat);
                
                fdc.dat = fdc.st0;

                if (fdc_reset_stat)
                {
                        fdc.st0 = (fdc.st0 & 0xf8) | (4 - fdc_reset_stat) | (fdc.head ? 4 : 0);
                        fdc_reset_stat--;
                }
                fdc.stat    = (fdc.stat & 0xf) | 0xd0;
                fdc.res[9]  = fdc.st0;
                fdc.res[10] = fdc.track[fdc.drive];
                if (!fdc_reset_stat) fdc.st0 = 0x80;

                paramstogo = 2;
                discint = 0;
		disctime = 0;
                return;
                
                case 0x0d: /*Format track*/
//                rpclog("Format\n");
                if (fdc.format_state == 1)
                {
//                        ioc_fiq(IOC_FIQ_DISC_DATA);
                        fdc.format_state = 2;
			timer_process();
			disctime = 128 * (1 << TIMER_SHIFT);
			timer_update_outstanding();
                }                
                else if (fdc.format_state == 2)
                {
                        temp = fdc_getdata(fdc.pos == ((fdc.params[2] * 4) - 1));
                        if (temp == -1)
                        {
        			timer_process();
        			disctime = 128 * (1 << TIMER_SHIFT);
        			timer_update_outstanding();
                                return;
                        }
                        fdc.format_dat[fdc.pos++] = temp;
                        if (fdc.pos == (fdc.params[2] * 4))
                                fdc.format_state = 3;
			timer_process();
			disctime = 128 * (1 << TIMER_SHIFT);
			timer_update_outstanding();
                }
                else if (fdc.format_state == 4)
                {
//                        rpclog("Format next stage\n");
                        disc_format(fdc.drive, fdc.track[fdc.drive], fdc.head, fdc.rate);
                        fdc.format_state = 4;
                }
                else
                {
                        discint=-2;
                        fdc_int();
                        fdc.stat=0xD0;
                        fdc.res[4] = (fdc.head?4:0)|fdc.drive;
                        fdc.res[5] = fdc.res[6] = 0;
                        fdc.res[7] = fdc.track[fdc.drive];
                        fdc.res[8] = fdc.head;
                        fdc.res[9] = fdc.format_dat[fdc.pos - 2] + 1;
                        fdc.res[10] = fdc.params[4];
                        paramstogo=7;
                        fdc.format_state = 0;
                        return;
                }
                return;
                
                case 15: /*Seek*/
                fdc.track[fdc.drive]=fdc.params[1];
//                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
//                printf("Seeked to track %i %i\n",fdc.track[fdc.drive], fdc.drive);
                fdc.st0=0x20|fdc.drive|(fdc.head?4:0);
                discint=-3;
		timer_process();
		disctime = 2048 * (1 << TIMER_SHIFT);
		timer_update_outstanding();
                fdc.stat = 0x80 | (1 << fdc.drive);
//                pclog("Stat %02X ST0 %02X\n", fdc.stat, fdc.st0);
                return;
                case 0x0e: /*Dump registers*/
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[3] = fdc.track[0];
                fdc.res[4] = fdc.track[1];
                fdc.res[5] = 0;
                fdc.res[6] = 0;
                fdc.res[7] = fdc.specify[0];
                fdc.res[8] = fdc.specify[1];
                fdc.res[9] = fdc.eot[fdc.drive];
                fdc.res[10] = (fdc.perp & 0x7f) | ((fdc.lock) ? 0x80 : 0);
                paramstogo=10;
                discint=0;
		disctime = 0;
                return;

                case 0x10: /*Version*/
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0x90;
                paramstogo=1;
                discint=0;
                disctime = 0;
                return;
                
                case 0x12:
                fdc.perp = fdc.params[0];
                fdc.stat = 0x80;
                disctime = 0;
//                picint(0x40);
                return;
                case 0x13: /*Configure*/
                fdc.config = fdc.params[1];
                fdc.pretrk = fdc.params[2];
                fdc.stat = 0x80;
                disctime = 0;
//                picint(0x40);
                return;
                case 0x14: /*Unlock*/
                fdc.lock = 0;
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0;
                paramstogo=1;
                discint=0;
                disctime = 0;
                return;
                case 0x94: /*Lock*/
                fdc.lock = 1;
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0x10;
                paramstogo=1;
                discint=0;
                disctime = 0;
                return;


                case 0xfc: /*Invalid*/
                fdc.dat = fdc.st0 = 0x80;
//                pclog("Inv!\n");
                //picint(0x40);
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
//                fdc.stat|=0xC0;
                fdc.res[10] = fdc.st0;
                paramstogo=1;
                discint=0;
                disctime = 0;
                return;
        }
//        printf("Bad FDC disc int %i\n",discint);
//        dumpregs();
//        exit(-1);
}

void fdc_overrun()
{
        img_stop();
        disctime = 0;

        fdc_int();
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|fdc.drive;
        fdc.res[5]=0x10; /*Overrun*/
        fdc.res[6]=0;
        fdc.res[7]=0;
        fdc.res[8]=0;
        fdc.res[9]=0;
        fdc.res[10]=0;
        paramstogo=7;
}

int fdc_data(uint8_t data)
{
        if (fdc.tc)
                return 0;

        if (fdc.pcjr)
        {
                if (fdc.data_ready)
                {
                        fdc_overrun();
//                        pclog("Overrun\n");
                        return -1;
                }

                fdc.dat = data;
                fdc.data_ready = 1;
                fdc.stat = 0xf0;
//                pclog("fdc_data\n");
        }
        else
        {
                if (dma_channel_write(2, data) & DMA_OVER)
                        fdc.tc = 1;
        }
        
        return 0;
}

void fdc_finishread()
{
        fdc.inread = 0;
        disctime = 200 * TIMER_USEC;
//        rpclog("fdc_finishread\n");
}

void fdc_notfound()
{
        disctime = 0;

        fdc_int();
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|fdc.drive;
        fdc.res[5]=5;
        fdc.res[6]=0;
        fdc.res[7]=0;
        fdc.res[8]=0;
        fdc.res[9]=0;
        fdc.res[10]=0;
        paramstogo=7;
//        rpclog("c82c711_fdc_notfound\n");
}

void fdc_datacrcerror()
{
        disctime = 0;

        fdc_int();
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|fdc.drive;
        fdc.res[5]=0x20; /*Data error*/
        fdc.res[6]=0x20; /*Data error in data field*/
        fdc.res[7]=0;
        fdc.res[8]=0;
        fdc.res[9]=0;
        fdc.res[10]=0;
        paramstogo=7;
//        rpclog("c82c711_fdc_datacrcerror\n");
}

void fdc_headercrcerror()
{
        disctime = 0;

        fdc_int();
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|fdc.drive;
        fdc.res[5]=0x20; /*Data error*/
        fdc.res[6]=0;
        fdc.res[7]=0;
        fdc.res[8]=0;
        fdc.res[9]=0;
        fdc.res[10]=0;
        paramstogo=7;
//        rpclog("c82c711_fdc_headercrcerror\n");
}

void fdc_writeprotect()
{
        disctime = 0;

        fdc_int();
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|fdc.drive;
        fdc.res[5]=0x02; /*Not writeable*/
        fdc.res[6]=0;
        fdc.res[7]=0;
        fdc.res[8]=0;
        fdc.res[9]=0;
        fdc.res[10]=0;
        paramstogo=7;
}

int fdc_getdata(int last)
{
        int data;
        
        if (fdc.pcjr)
        {
                if (fdc.written)
                {
                        fdc_overrun();
//                        pclog("Overrun\n");
                        return -1;
                }
                data = fdc.dat;
                if (!last)
                        fdc.stat = 0xb0;
        }
        else
        {
                data = dma_channel_read(2);
        
                if (data & DMA_OVER)
                        fdc.tc = 1;
        }
        
        fdc.written = 0;
        return data & 0xff;
}

void fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2)
{
//        pclog("SectorID %i %i %i %i\n", track, side, sector, size);
        fdc_int();
        fdc.stat=0xD0;
        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
        fdc.res[5]=0;
        fdc.res[6]=0;
        fdc.res[7]=track;
        fdc.res[8]=side;
        fdc.res[9]=sector;
        fdc.res[10]=size;
        paramstogo=7;
}

void fdc_indexpulse()
{
//        ioc_irqa(IOC_IRQA_DISC_INDEX);
//        rpclog("c82c711_fdc_indexpulse\n");
}

void fdc_init()
{
	timer_add(fdc_callback, &disctime, &disctime, NULL);
	fdc.dskchg_activelow = 0;
	fdc.enable_3f1 = 1;
}

void fdc_add()
{
        io_sethandler(0x03f0, 0x0006, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);
        io_sethandler(0x03f7, 0x0001, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);
        fdc.pcjr = 0;
}

void fdc_add_pcjr()
{
        io_sethandler(0x00f0, 0x0006, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);
	timer_add(fdc_watchdog_poll, &fdc.watchdog_timer, &fdc.watchdog_timer, &fdc);
        fdc.pcjr = 1;
}

void fdc_remove()
{
        io_removehandler(0x03f0, 0x0006, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);
        io_removehandler(0x03f7, 0x0001, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);        
}

void fdc_discchange_clear(int drive)
{
        if (drive < 2)
                disc_changed[drive] = 0;
}

void fdc_set_dskchg_activelow()
{
        fdc.dskchg_activelow = 1;
}

void fdc_3f1_enable(int enable)
{
        fdc.enable_3f1 = enable;
}
