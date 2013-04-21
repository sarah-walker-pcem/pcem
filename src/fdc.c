#include <stdio.h>
#include <string.h>
#include "ibm.h"

void fdc_poll();
int timetolive;
int TRACKS[2] = {80, 80};
int SECTORS[2]={9,9};
int SIDES[2]={2,2};
//#define SECTORS 9
int output;
int lastbyte=0;
uint8_t disc[2][2][80][36][512];
uint8_t disc_3f7;

int discchanged[2];
int discmodified[2];
int discrate[2];

void loaddisc(int d, char *fn)
{
        FILE *f=fopen(fn,"rb");
        int h,t,s,b;
        if (!f)
        {
                SECTORS[d]=9; SIDES[d]=1;
                driveempty[d]=1; discrate[d]=4;
                return;
        }
        driveempty[d]=0;
        SIDES[d]=2;
        fseek(f,-1,SEEK_END);
        if (ftell(f)<=(160*1024))       { SECTORS[d]=8;  TRACKS[d] = 40; SIDES[d]=1; discrate[d]=2; }
        else if (ftell(f)<=(180*1024))  { SECTORS[d]=9;  TRACKS[d] = 40; SIDES[d]=1; discrate[d]=2; }
        else if (ftell(f)<=(320*1024))  { SECTORS[d]=8;  TRACKS[d] = 40; discrate[d]=2; }
        else if (ftell(f)<(1024*1024))  { SECTORS[d]=9;  TRACKS[d] = 80; discrate[d]=2; } /*Double density*/
        else if (ftell(f)<(0x1A4000-1)) { SECTORS[d]=18; TRACKS[d] = 80; discrate[d]=0; } /*High density (not supported by Tandy 1000)*/
        else if (ftell(f) == 1884159)   { SECTORS[d]=23; TRACKS[d] = 80; discrate[d]=0; } /*XDF format - used by OS/2 Warp*/
        else if (ftell(f) == 1763327)   { SECTORS[d]=21; TRACKS[d] = 82; discrate[d]=0; } /*XDF format - used by OS/2 Warp*/
        else if (ftell(f)<(2048*1024))  { SECTORS[d]=21; TRACKS[d] = 80; discrate[d]=0; } /*DMF format - used by Windows 95*/
        else                            { SECTORS[d]=36; TRACKS[d] = 80; discrate[d]=3; } /*E density (not supported by anything)*/
        printf("Drive %c: has %i sectors and %i sides and is %i bytes long\n",'A'+d,SECTORS[0],SIDES[0],ftell(f));
        fseek(f,0,SEEK_SET);
        for (t=0;t<80;t++)
        {
                for (h=0;h<SIDES[d];h++)
                {
                        for (s=0;s<SECTORS[d];s++)
                        {
                                for (b=0;b<512;b++)
                                {
                                        disc[d][h][t][s][b]=getc(f);
                                }
                        }
                }
        }
        fclose(f);
        discmodified[d]=0;
        strcpy(discfns[d],fn);
        discchanged[d]=1;
}

void ejectdisc(int d)
{
        discfns[d][0]=0;
        SECTORS[d]=9; SIDES[d]=1;
        driveempty[d]=1; discrate[d]=4;
}

void savedisc(int d)
{
        FILE *f;
        int h,t,s,b;
        if (!discmodified[d]) return;
        f=fopen(discfns[d],"wb");
        if (!f) return;
        printf("Save disc %c: %s %i %i\n",'A'+d,discfns[0],SIDES[0],SECTORS[0]);
        for (t=0;t<80;t++)
        {
                for (h=0;h<SIDES[d];h++)
                {
                        for (s=0;s<SECTORS[d];s++)
                        {
                                for (b=0;b<512;b++)
                                {
                                        putc(disc[d][h][t][s][b],f);
                                }
                        }
                }
        }
        fclose(f);
}

int discint;
void fdc_reset()
{
        fdc.stat=0x80;
        fdc.pnum=fdc.ptot=0;
        fdc.st0=0xC0;
        fdc.lock = 0;
        fdc.head = 0;
        if (!AT)
           fdc.rate=2;
//        pclog("Reset FDC\n");
}
int ins;

void fdc_write(uint16_t addr, uint8_t val)
{
        int c;
//        printf("Write FDC %04X %02X %04X:%04X %i %02X %i rate=%i\n",addr,val,cs>>4,pc,ins,fdc.st0,ins,fdc.rate);
        switch (addr&7)
        {
                case 1: return;
                case 2: /*DOR*/
//                if (val == 0xD && (cs >> 4) == 0xFC81600 && ins > 769619936) output = 3;
//                printf("DOR was %02X\n",fdc.dor);
                if (val&4)
                {
                        fdc.stat=0x80;
                        fdc.pnum=fdc.ptot=0;
                }
                if ((val&4) && !(fdc.dor&4))
                {
                        disctime=128;
                        discint=-1;
                        fdc_reset();
                }
                fdc.dor=val;
//                printf("DOR now %02X\n",val);
                return;
                case 4:
                if (val & 0x80)
                {
                        disctime=128;
                        discint=-1;
                        fdc_reset();
                }
                return;
                case 5: /*Command register*/
//                pclog("Write command reg %i %i\n",fdc.pnum, fdc.ptot);
                if (fdc.pnum==fdc.ptot)
                {
                        fdc.command=val;
//                        printf("Starting FDC command %02X\n",fdc.command);
                        switch (fdc.command&0x1F)
                        {
                                case 2: /*Read track*/
//                                printf("Read track!\n");
                                fdc.pnum=0;
                                fdc.ptot=8;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
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
                                readflash=1;
                                break;
                                case 6: /*Read data*/
                                fullspeed();
                                fdc.pnum=0;
                                fdc.ptot=8;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                readflash=1;
                                break;
                                case 7: /*Recalibrate*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                break;
                                case 8: /*Sense interrupt status*/
//                                printf("Sense interrupt status %i\n",fdc.drive);
                                fdc.lastdrive = fdc.drive;
//                                fdc.stat = 0x10 | (fdc.stat & 0xf);
//                                disctime=1024;
                                discint = 8;
                                fdc.pos = 0;
                                fdc_poll();
                                break;
                                case 10: /*Read sector ID*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                fdc.pos=0;
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
                                fdc_poll();
                                break;
                                case 0x10: /*Get version*/
                                fdc.lastdrive = fdc.drive;
                                discint = 0x10;
                                fdc.pos = 0;
                                fdc_poll();
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
                                fdc_poll();
                                break;

                                case 0x18:
                                fdc.stat = 0x10;
                                discint  = 0xfc;
                                fdc_poll();
                                break;

                                default:
                                pclog("Bad FDC command %02X\n",val);
//                                dumpregs();
//                                exit(-1);
                                fdc.stat=0x10;
                                discint=0xfc;
                                disctime=200;
                                break;
                        }
                }
                else
                {
                        fdc.params[fdc.pnum++]=val;
                        if (fdc.pnum==fdc.ptot)
                        {
//                                pclog("Got all params\n");
                                fdc.stat=0x30;
                                discint=fdc.command&0x1F;
                                disctime=1024;
                                fdc.drive=fdc.params[0]&1;
                                if (discint==2 || discint==5 || discint==6)
                                {
                                        fdc.track[fdc.drive]=fdc.params[1];
                                        fdc.head=fdc.params[2];
                                        fdc.sector=fdc.params[3];
                                        fdc.eot[fdc.drive] = fdc.params[4];
                                        if (!fdc.params[5])
                                        {
                                                fdc.params[5]=fdc.sector;
                                        }
                                        if (fdc.params[5]>SECTORS[fdc.drive]) fdc.params[5]=SECTORS[fdc.drive];
                                        if (driveempty[fdc.drive])
                                        {
//                                                pclog("Drive empty\n");
                                                discint=0xFE;
                                        }
                                }
                                if (discint==2 || discint==5 || discint==6 || discint==10)
                                {
//                                        pclog("Rate %i %i %i\n",fdc.rate,discrate[fdc.drive],driveempty[fdc.drive]);
                                        if (fdc.rate!=discrate[fdc.drive])
                                        {
//                                                pclog("Wrong rate %i %i\n",fdc.rate,discrate[fdc.drive]);
                                                discint=0xFF;
                                                disctime=1024;
                                        }
                                        if (driveempty[fdc.drive])
                                        {
//                                                pclog("Drive empty\n");
                                                discint=0xFE;
                                                disctime=1024;
                                        }
                                }
                                if (discint == 7 || discint == 0xf)
                                {
                                        fdc.stat =  1 << fdc.drive;
//                                        disctime = 8000000;
                                }
                                if (discint == 0xf)
                                {
                                        fdc.head = (fdc.params[0] & 4) ? 1 : 0;
                                }
//                                if (discint==5) fdc.pos=512;
                        }
                }
                return;
                case 7:
                        if (!AT) return;
                fdc.rate=val&3;

                disc_3f7=val;
                return;
        }
//        printf("Write FDC %04X %02X\n",addr,val);
//        dumpregs();
//        exit(-1);
}

int paramstogo=0;
uint8_t fdc_read(uint16_t addr)
{
        uint8_t temp;
//        /*if (addr!=0x3f4) */printf("Read FDC %04X %04X:%04X %04X %i %02X %i ",addr,cs>>4,pc,BX,fdc.pos,fdc.st0,ins);
        switch (addr&7)
        {
                case 1: /*???*/
                temp=0x50;
                break;
                case 3:
                temp = 0x20;
                break;
                case 4: /*Status*/
                temp=fdc.stat;
                break;
                case 5: /*Data*/
                fdc.stat&=~0x80;
                if (paramstogo)
                {
                        paramstogo--;
                        temp=fdc.res[10 - paramstogo];
//                        printf("Read param %i %02X\n",6-paramstogo,temp);
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
                           fdc.stat=0x80;
                        lastbyte=0;
                        temp=fdc.dat;
                }
                if (discint==0xA) disctime=1024;
                fdc.stat &= 0xf0;
                break;
                case 7: /*Disk change*/
                if (fdc.dor & (0x10 << (fdc.dor & 1)))
                   temp = (discchanged[fdc.dor & 1] || driveempty[fdc.dor & 1])?0x80:0;
                else
                   temp = 0;
                if (AMSTRADIO)  /*PC2086/3086 seem to reverse this bit*/
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
//        /*if (addr!=0x3f4) */printf("%02X rate=%i\n",temp,fdc.rate);
        return temp;
}

int fdc_abort_f = 0;

void fdc_abort()
{
        fdc_abort_f = 1;
//        pclog("FDC ABORT\n");
}

static int fdc_reset_stat = 0;
void fdc_poll()
{
        int temp;
//        pclog("fdc_poll %08X %i %02X\n", discint, fdc.drive, fdc.st0);
        switch (discint)
        {
                case -3: /*End of command with interrupt*/
//                if (output) printf("EOC - interrupt!\n");
//pclog("EOC\n");
                picint(0x40);
                case -2: /*End of command*/
                fdc.stat = (fdc.stat & 0xf) | 0x80;
                return;
                case -1: /*Reset*/
//pclog("Reset\n");
                picint(0x40);
                fdc_reset_stat = 4;
                return;
                case 2: /*Read track*/
                if (!fdc.pos)
                {
//                        printf("Read Track Side %i Track %i Sector %02X sector size %i end sector %02X %05X\n",fdc.head,fdc.track,fdc.sector,fdc.params[4],fdc.params[5],(dma.page[2]<<16)+dma.ac[2]);
                }
                if (fdc.pos<512)
                {
                        fdc.dat=disc[fdc.drive][fdc.head][fdc.track[fdc.drive]][fdc.sector-1][fdc.pos];
//                        pclog("Read %i %i %i %i %02X\n",fdc.head,fdc.track,fdc.sector,fdc.pos,fdc.dat);
                        writedma2(fdc.dat);
                        disctime=60;
                }
                else
                {
                        disctime=0;
                        discint=-2;
//                        pclog("RT\n");
                        picint(0x40);
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                        fdc.res[5]=fdc.res[2]=0;
                        fdc.res[6]=fdc.track[fdc.drive];
                        fdc.res[7]=fdc.head;
                        fdc.res[8]=fdc.sector;
                        fdc.res[9]=fdc.params[4];
                        paramstogo=7;
                        return;
                        disctime=1024;
                        picint(0x40);
                        fdc.stat=0xD0;
                        switch (fdc.pos-512)
                        {
                                case 0: case 1: case 2: fdc.dat=0; break;
                                case 3: fdc.dat=fdc.track[fdc.drive]; break;
                                case 4: fdc.dat=fdc.head; break;
                                case 5: fdc.dat=fdc.sector; break;
                                case 6: fdc.dat=fdc.params[4]; discint=-2; lastbyte=1; break;
                        }
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
                }
                return;
                case 3: /*Specify*/
                fdc.stat=0x80;
                fdc.specify[0] = fdc.params[0];
                fdc.specify[1] = fdc.params[1];
                return;
                case 4: /*Sense drive status*/
                fdc.res[10] = (fdc.params[0] & 7) | 0x28;
                if (!fdc.track[fdc.drive]) fdc.res[10] |= 0x10;

                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                paramstogo = 1;
                discint = 0;
                disctime = 0;
                return;
                case 5: /*Write data*/
                discmodified[fdc.drive]=1;
//                printf("Write data %i\n",fdc.pos);
                if (!fdc.pos)
                {
//                        printf("Write data Side %i Track %i Sector %02X sector size %i end sector %02X\n",fdc.params[2],fdc.params[1],fdc.params[3],fdc.params[4],fdc.params[5]);
//                        dumpregs();
//                        exit(-1);
                }
                if (fdc.pos<512)
                {
                        temp=readdma2();
                        if (fdc_abort_f)
                        {
                                fdc_abort_f=0;
                                discint=0xFD;
                                disctime=50;
                                return;
                        }
                        else
                        {
                                fdc.dat=disc[fdc.drive][fdc.head][fdc.track[fdc.drive]][fdc.sector-1][fdc.pos]=temp;
//                                printf("Write data %i %i %02X   %i:%i:%i:%i\n",fdc.sector-1,fdc.pos,fdc.dat,fdc.head,fdc.track[fdc.drive],fdc.sector-1,fdc.pos);
                                disctime=60;
                        }
                }
                else
                {
                        disctime=0;
                        discint=-2;
//                        pclog("WD\n");
                        picint(0x40);
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                        fdc.res[5]=0;
                        fdc.res[6]=0;
                        fdc.res[7]=fdc.track[fdc.drive];
                        fdc.res[8]=fdc.head;
                        fdc.res[9]=fdc.sector;
                        fdc.res[10]=fdc.params[4];
                        paramstogo=7;
                        return;
                        disctime=1024;
                        picint(0x40);
                        fdc.stat=0xD0;
                        switch (fdc.pos-512)
                        {
                                case 0: fdc.dat=0x40; break;
                                case 1: fdc.dat=2; break;
                                case 2: fdc.dat=0; break;
                                case 3: fdc.dat=fdc.track[fdc.drive]; break;
                                case 4: fdc.dat=fdc.head; break;
                                case 5: fdc.dat=fdc.sector; break;
                                case 6: fdc.dat=fdc.params[4]; discint=-2; break;
                        }
                }
                fdc.pos++;
                if (fdc.pos==512 && fdc.sector!=fdc.params[5])
                {
                        fdc.pos=0;
                        fdc.sector++;
                }
                return;
                case 6: /*Read data*/
                if (!fdc.pos)
                {
//                        printf("Reading sector %i track %i side %i drive %i %02X to %05X\n",fdc.sector,fdc.track[fdc.drive],fdc.head,fdc.drive,fdc.params[5],(dma.ac[2]+(dma.page[2]<<16))&rammask);
                }
                if (fdc.pos<512)
                {
                        fdc.dat=disc[fdc.drive][fdc.head][fdc.track[fdc.drive]][fdc.sector-1][fdc.pos];
//                        printf("Read disc %i %i %i %i %02X\n",fdc.head,fdc.track,fdc.sector,fdc.pos,fdc.dat);
                        writedma2(fdc.dat);
                        disctime=60;
                }
                else
                {
//                        printf("End of command - params to go!\n");
                        fdc_abort_f = 0;
                        disctime=0;
                        discint=-2;
                        picint(0x40);
//                        pclog("RD\n");
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                        fdc.res[5]=fdc.res[6]=0;
                        fdc.res[7]=fdc.track[fdc.drive];
                        fdc.res[8]=fdc.head;
                        fdc.res[9]=fdc.sector;
                        fdc.res[10]=fdc.params[4];
                        paramstogo=7;
                        return;
                        switch (fdc.pos-512)
                        {
                                case 0: case 1: case 2: fdc.dat=0; break;
                                case 3: fdc.dat=fdc.track[fdc.drive]; break;
                                case 4: fdc.dat=fdc.head; break;
                                case 5: fdc.dat=fdc.sector; break;
                                case 6: fdc.dat=fdc.params[4]; discint=-2; lastbyte=1; break;
                        }
                }
                fdc.pos++;
                if (fdc.pos==512)// && fdc.sector!=fdc.params[5])
                {
//                        printf("Sector complete! %02X\n",fdc.params[5]);
                        fdc.pos=0;
                        fdc.sector++;
                        if (fdc.sector > fdc.params[5])
                        {
//                                printf("Overrunnit!\n");
//                                dumpregs();
//                                exit(-1);
                                fdc.sector=1;
                                if (fdc.command & 0x80)
                                {
                                        fdc.head ^= 1;
                                        if (!fdc.head)
                                        {
                                                fdc.track[fdc.drive]++;
                                                if (fdc.track[fdc.drive] >= TRACKS[fdc.drive])
                                                {
                                                        fdc.track[fdc.drive] = TRACKS[fdc.drive];
                                                        fdc.pos = 512;
                                                }
                                        }
                                }
                                else
                                   fdc.pos = 512;
                        }
                        if (fdc_abort_f)
                           fdc.pos = 512;
                }
                return;
/*                printf("Read data\n");
                printf("Side %i Track %i Sector %i sector size %i\n",fdc.params[1],fdc.params[2],fdc.params[3],fdc.params[4]);
                dumpregs();
                exit(-1);*/
                case 7: /*Recalibrate*/
                fdc.track[fdc.drive]=0;
                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
                fdc.st0=0x20|fdc.drive|(fdc.head?4:0);
                discint=-3;
                disctime=2048;
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
                case 10: /*Read sector ID*/
                disctime=0;
                discint=-2;
                picint(0x40);
                fdc.stat=0xD0;
                fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                fdc.res[5]=0;
                fdc.res[6]=0;
                fdc.res[7]=fdc.track[fdc.drive];
                fdc.res[8]=fdc.head;
                fdc.res[9]=fdc.sector;
                fdc.res[10]=fdc.params[4];
                paramstogo=7;
                fdc.sector++;
                if (fdc.sector==SECTORS[fdc.drive]+1)
                   fdc.sector=1;
                return;
                case 15: /*Seek*/
                fdc.track[fdc.drive]=fdc.params[1];
                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
//                printf("Seeked to track %i %i\n",fdc.track[fdc.drive], fdc.drive);
                fdc.st0=0x20|fdc.drive|(fdc.head?4:0);
                discint=-3;
                disctime=2048;
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
                disctime=0;
                return;

                case 0x10: /*Version*/
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0x90;
                paramstogo=1;
                discint=0;
                disctime=0;
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
                disctime=0;
                return;
                case 0x94: /*Lock*/
                fdc.lock = 1;
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0x10;
                paramstogo=1;
                discint=0;
                disctime=0;
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
                disctime=0;
                return;

                case 0xFD: /*DMA aborted (PC1512)*/
                /*In the absence of other information, lie and claim the command completed successfully.
                  The PC1512 BIOS likes to program the FDC to write to all sectors on the track, but
                  program the DMA length to the number of sectors actually transferred. Not aborting
                  correctly causes disc corruption.
                  This only matters on writes, on reads the DMA controller will ignore excess data.
                  */
                pclog("DMA Aborted\n");
                disctime=0;
                discint=-2;
                picint(0x40);
                fdc.stat=0xD0;
                fdc.res[4]=(fdc.head?4:0)|fdc.drive;
                fdc.res[5]=0;
                fdc.res[6]=0;
                fdc.res[7]=fdc.track[fdc.drive];
                fdc.res[8]=fdc.head;
                fdc.res[9]=fdc.sector;
                fdc.res[10]=fdc.params[4];
                paramstogo=7;
                return;
                case 0xFE: /*Drive empty*/
                pclog("Drive empty\n");
                fdc.stat = 0x10;
                disctime = 0;
/*                disctime=0;
                discint=-2;
                picint(0x40);
                fdc.stat=0xD0;
                fdc.res[4]=0xC8|(fdc.head?4:0)|fdc.drive;
                fdc.res[5]=0;
                fdc.res[6]=0;
                fdc.res[7]=0;
                fdc.res[8]=0;
                fdc.res[9]=0;
                fdc.res[10]=0;
                paramstogo=7;*/
                return;
                case 0xFF: /*Wrong rate*/
                pclog("Wrong rate\n");
                disctime=0;
                discint=-2;
                picint(0x40);
                fdc.stat=0xD0;
                fdc.res[4]=0x40|(fdc.head?4:0)|fdc.drive;
                fdc.res[5]=5;
                fdc.res[6]=0;
                fdc.res[7]=0;
                fdc.res[8]=0;
                fdc.res[9]=0;
                fdc.res[10]=0;
                paramstogo=7;
                return;
        }
//        printf("Bad FDC disc int %i\n",discint);
//        dumpregs();
//        exit(-1);
}


void fdc_init()
{
        io_sethandler(0x03f0, 0x0006, fdc_read, NULL, NULL, fdc_write, NULL, NULL);
        io_sethandler(0x03f7, 0x0001, fdc_read, NULL, NULL, fdc_write, NULL, NULL);        
}

void fdc_remove()
{
        io_removehandler(0x03f0, 0x0006, fdc_read, NULL, NULL, fdc_write, NULL, NULL);
        io_removehandler(0x03f7, 0x0001, fdc_read, NULL, NULL, fdc_write, NULL, NULL);        
}
