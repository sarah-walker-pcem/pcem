/*Due to the lack of a real hard disc BIOS (other than the WD ST-506 one in MESS),
  I wrote this. It's better as it can handle bigger discs, but is (potentially)
  less compatible.*/
#include <stdio.h>
#include <string.h>
#include "ibm.h"

extern int output;
void inithdc()
{
        return;
        hdc[0].f=romfopen("hdc.img","rb+");
        if (!hdc[0].f)
        {
                hdc[0].f=romfopen("hdc.img","wb");
                putc(0,hdc[0].f);
                fclose(hdc[0].f);
                hdc[0].f=romfopen("hdc.img","rb+");
        }
//        hdc[0].spt=16;
//        hdc[0].hpc=5;
//        hdc[0].tracks=977;

        hdc[1].f=romfopen("hdd.img","rb+");
        if (!hdc[1].f)
        {
                hdc[1].f=romfopen("hdd.img","wb");
                putc(0,hdc[1].f);
                fclose(hdc[1].f);
                hdc[1].f=romfopen("hdd.img","rb+");
        }
//        hdc[1].spt=32;
//        hdc[1].hpc=16;
//        hdc[1].tracks=447;
}

void int13hdc()
{
        int drv=DL&1;
        int track,head,sector;
        uint32_t addr;
        int c,d;
        uint8_t buf[512];
        fullspeed();
//        ram[0x475]=2;
//        printf("INT 13 HDC %04X %04X %04X %04X  %04X:%04X\n",AX,BX,CX,DX,CS,pc);
        switch (AH)
        {
                case 0: /*Reset disk system*/
                AH=0;
                flags&=~C_FLAG;
                break;
                case 2: /*Read sectors into memory*/
//                printf("Read %i sectors to %04X:%04X\n",AL,es>>4,BX);
//                if (es==0xf940) output=3;
                track=CH|((CL&0xC0)<<2);
                sector=(CL&63)-1;
                head=DH;
                addr=((((track*hdc[drv].hpc)+head)*hdc[drv].spt)+sector)*512;
//                printf("Read track %i head %i sector %i addr %08X  HPC %i SPT %i %08X\n",track,head,sector,addr,hdc[drv].hpc,hdc[drv].spt,old8);
                fseek(hdc[drv].f,addr,SEEK_SET);
                for (c=0;c<AL;c++)
                {
                        fread(buf,512,1,hdc[drv].f);
                        for (d=0;d<512;d++)
                        {
                                writemembl(es+BX+(c*512)+d,buf[d]/*getc(hdc[drv].f)*/);
                        }
                }
                AH=0;
                flags&=~C_FLAG;
                readflash=1;
                break;
                case 3: /*Write sectors*/
                track=CH|((CL&0xC0)<<2);
                sector=(CL&63)-1;
                head=DH;
                addr=((((track*hdc[drv].hpc)+head)*hdc[drv].spt)+sector)*512;
                fseek(hdc[drv].f,addr,SEEK_SET);
                for (c=0;c<AL;c++)
                {
                        for (d=0;d<512;d++)
                        {
                                putc(readmembl(es+BX+(c*512)+d),hdc[drv].f);
                        }
                }
                AH=0;
                flags&=~C_FLAG;
                readflash=1;
                break;
                case 4: /*Verify sectors*/
                AH=0; /*We don't actually do anything here*/
                flags&=~C_FLAG;
                break;
                case 8: /*Get drive parameters*/
                AH=0;
                CH=hdc[drv].tracks&255;
                CL=hdc[drv].spt;
                CL|=((hdc[drv].tracks>>2)&0xC0);
//                printf("Drive params - %02X %02X %i %i\n",CL,CH,hdc[drv].tracks,hdc[drv].spt);
                DH=hdc[drv].hpc-1;
                DL=2;
                flags&=~C_FLAG;
                break;
                case 0x10: /*Check drive ready*/
                AH=0;
                flags&=~C_FLAG;
                break;
                case 0x18: /*Set media type*/
                AH=1;
                flags|=C_FLAG;
                break;
                
                default:
                        AH=1;
                        flags|=C_FLAG;
//                printf("Bad HDC int 13 %04X\n",AX);
//                dumpregs();
//                exit(-1);
        }
}

char tempbuf[512*63];
void resizedrive(int drv)
{
        FILE *f;
        int c,d,e;
//        char temp[512*63];
        if (!drv)
        {
                fflush(hdc[0].f);
                if (hdc[0].f) fclose(hdc[0].f);
                f=romfopen("hdc.img","wb");
        }
        else
        {
                fflush(hdc[1].f);
                if (hdc[1].f) fclose(hdc[1].f);
                f=romfopen("hdd.img","wb");
        }
        memset(tempbuf,0,512*63);
        for (c=0;c<hdc[drv].tracks;c++)
        {
                for (d=0;d<hdc[drv].hpc;d++)
                {
//                        for (e=0;e<hdc[drv].spt;e++)
//                        {
                                fwrite(tempbuf,512*hdc[drv].spt,1,f);
//                        }
                }
        }
        fclose(f);
        if (!drv) hdc[0].f=romfopen("hdc.img","rb+");
        else      hdc[1].f=romfopen("hdd.img","rb+");
}
