#include "ibm.h"

#include "dma.h"
#include "fdc.h"
#include "io.h"
#include "video.h"

extern int ins;
int output;
uint8_t dmaregs[16];
int dmaon[4];
uint8_t dma16regs[16];
int dma16on[4];

void dma_reset()
{
        int c;
        dma.wp=0;
        for (c=0;c<16;c++) dmaregs[c]=0;
        for (c=0;c<4;c++)
        {
                dma.mode[c]=0;
                dma.ac[c]=0;
                dma.cc[c]=0;
                dma.ab[c]=0;
                dma.cb[c]=0;
        }
        dma.m=0;
        
        dma16.wp=0;
        for (c=0;c<16;c++) dma16regs[c]=0;
        for (c=0;c<4;c++)
        {
                dma16.mode[c]=0;
                dma16.ac[c]=0;
                dma16.cc[c]=0;
                dma16.ab[c]=0;
                dma16.cb[c]=0;
        }
        dma16.m=0;
}

uint8_t dma_read(uint16_t addr, void *priv)
{
        uint8_t temp;
//        printf("Read DMA %04X %04X:%04X %i %02X\n",addr,CS,pc, pic_intpending, pic.pend);
        switch (addr&0xF)
        {
                case 0:
/*                if (((dma.mode[0]>>2)&3)==2)
                {
                        dma.ac[0]++;
                        dma.cc[0]--;
                        if (dma.cc[0]<0)
                        {
                                dma.ac[0]=dma.ab[0];
                                dma.cc[0]=dma.cb[0];
                        }
                }*/
                case 2: case 4: case 6: /*Address registers*/
                dma.wp^=1;
                if (dma.wp) return dma.ac[(addr>>1)&3]&0xFF;
                return dma.ac[(addr>>1)&3]>>8;
                case 1: case 3: case 5: case 7: /*Count registers*/
//                printf("DMA count %i = %04X\n", (addr>>1)&3, dma.cc[(addr>>1)&3]);
                dma.wp^=1;
                if (dma.wp) temp=dma.cc[(addr>>1)&3]&0xFF;
                else        temp=dma.cc[(addr>>1)&3]>>8;
//                printf("%02X\n",temp);
                return temp;
                case 8: /*Status register*/
                temp=dma.stat;
                dma.stat=0;
//                pclog("Read DMA status %02X\n", temp);
                return temp;
                case 0xD:
                return 0;
        }
//        printf("Bad DMA read %04X %04X:%04X\n",addr,CS,pc);
        return dmaregs[addr&0xF];
}

void dma_write(uint16_t addr, uint8_t val, void *priv)
{
        printf("Write DMA %04X %02X %04X:%04X\n",addr,val,CS,pc);
        dmaregs[addr&0xF]=val;
        switch (addr&0xF)
        {
                case 0: case 2: case 4: case 6: /*Address registers*/
                dma.wp^=1;
                if (dma.wp) dma.ab[(addr>>1)&3]=(dma.ab[(addr>>1)&3]&0xFF00)|val;
                else        dma.ab[(addr>>1)&3]=(dma.ab[(addr>>1)&3]&0xFF)|(val<<8);
                dma.ac[(addr>>1)&3]=dma.ab[(addr>>1)&3];
                dmaon[(addr>>1)&3]=1;
//                printf("DMA addr %i now %04X\n",(addr>>1)&3,dma.ac[(addr>>1)&3]);
                return;
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma.wp^=1;
                if (dma.wp) dma.cb[(addr>>1)&3]=(dma.cb[(addr>>1)&3]&0xFF00)|val;
                else        dma.cb[(addr>>1)&3]=(dma.cb[(addr>>1)&3]&0xFF)|(val<<8);
                dma.cc[(addr>>1)&3]=dma.cb[(addr>>1)&3];
                dmaon[(addr>>1)&3]=1;
//                printf("DMA count %i now %04X\n",(addr>>1)&3,dma.cc[(addr>>1)&3]);
                return;
                case 8: /*Control register*/
                dma.command = val;
                return;
                case 0xA: /*Mask*/
                if (val&4) dma.m|=(1<<(val&3));
                else       dma.m&=~(1<<(val&3));
                return;
                case 0xB: /*Mode*/
                dma.mode[val&3]=val;
                return;
                case 0xC: /*Clear FF*/
                dma.wp=0;
                return;
                case 0xD: /*Master clear*/
                dma.wp=0;
                dma.m=0xF;
                return;
                case 0xF: /*Mask write*/
                dma.m=val&0xF;
                return;
        }
}

uint8_t dma16_read(uint16_t addr, void *priv)
{
        uint8_t temp;
//        printf("Read DMA %04X %04X:%04X\n",addr,cs>>4,pc);
        addr>>=1;
        switch (addr&0xF)
        {
                case 0:
                if (((dma16.mode[0]>>2)&3)==2)
                {
                        dma16.ac[0]++;
                        dma16.cc[0]--;
                        if (dma16.cc[0]<0)
                        {
                                dma16.ac[0]=dma16.ab[0];
                                dma16.cc[0]=dma16.cb[0];
                        }
                }
                case 2: case 4: case 6: /*Address registers*/
                dma16.wp^=1;
                if (dma16.wp) return dma16.ac[(addr>>1)&3]&0xFF;
                return dma16.ac[(addr>>1)&3]>>8;
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma16.wp^=1;
//                printf("Read %04X\n",dma16.cc[1]);
                if (dma16.wp) temp=dma16.cc[(addr>>1)&3]&0xFF;
                else        temp=dma16.cc[(addr>>1)&3]>>8;
//                printf("%02X\n",temp);
                return temp;
                case 8: /*Status register*/
                temp=dma16.stat;
                dma16.stat=0;
                return temp;
        }
        return dma16regs[addr&0xF];
}

void dma16_write(uint16_t addr, uint8_t val, void *priv)
{
//        printf("Write dma16 %04X %02X %04X:%04X\n",addr,val,CS,pc);
        addr>>=1;
        dma16regs[addr&0xF]=val;
        switch (addr&0xF)
        {
                case 0: case 2: case 4: case 6: /*Address registers*/
                dma16.wp^=1;
                if (dma16.wp) dma16.ab[(addr>>1)&3]=(dma16.ab[(addr>>1)&3]&0xFF00)|val;
                else        dma16.ab[(addr>>1)&3]=(dma16.ab[(addr>>1)&3]&0xFF)|(val<<8);
                dma16.ac[(addr>>1)&3]=dma16.ab[(addr>>1)&3];
                dma16on[(addr>>1)&3]=1;
//                printf("dma16 addr %i now %04X\n",(addr>>1)&3,dma16.ac[(addr>>1)&3]);
                return;
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma16.wp^=1;
                if (dma16.wp) dma16.cb[(addr>>1)&3]=(dma16.cb[(addr>>1)&3]&0xFF00)|val;
                else        dma16.cb[(addr>>1)&3]=(dma16.cb[(addr>>1)&3]&0xFF)|(val<<8);
                dma16.cc[(addr>>1)&3]=dma16.cb[(addr>>1)&3];
                dma16on[(addr>>1)&3]=1;
//                printf("dma16 count %i now %04X\n",(addr>>1)&3,dma16.cc[(addr>>1)&3]);
                return;
                case 8: /*Control register*/
                return;
                case 0xA: /*Mask*/
                if (val&4) dma16.m|=(1<<(val&3));
                else       dma16.m&=~(1<<(val&3));
                return;
                case 0xB: /*Mode*/
                dma16.mode[val&3]=val;
                return;
                case 0xC: /*Clear FF*/
                dma16.wp=0;
                return;
                case 0xD: /*Master clear*/
                dma16.wp=0;
                dma16.m=0xF;
                return;
                case 0xF: /*Mask write*/
                dma16.m=val&0xF;
                return;
        }
}

uint8_t dmapages[16];

void dma_page_write(uint16_t addr, uint8_t val, void *priv)
{
/*        if (!(addr&0xF))
        {*/
//                pclog("Write page %03X %02X %04X:%04X\n",addr,val,CS,pc);
//                if (val==0x29 && pc==0xD25) output=1;
//        }
        dmapages[addr&0xF]=val;
        switch (addr&0xF)
        {
                case 1:
                dma.page[2]=(AT)?val:val&0xF;
                break;
                case 2:
                dma.page[3]=(AT)?val:val&0xF;
                break;
                case 3:
                dma.page[1]=(AT)?val:val&0xF;
//                pclog("DMA1 page %02X\n",val);
                break;
                case 0xB:
                dma16.page[1]=val;
                break;
        }
//        printf("Page write %04X %02X\n",addr,val);
}

uint8_t dma_page_read(uint16_t addr, void *priv)
{
        return dmapages[addr&0xF];
}

void dma_init()
{
        io_sethandler(0x0000, 0x0010, dma_read,      NULL, NULL, dma_write,      NULL, NULL,  NULL);
        io_sethandler(0x0080, 0x0008, dma_page_read, NULL, NULL, dma_page_write, NULL, NULL,  NULL);
}

void dma16_init()
{
        io_sethandler(0x00C0, 0x0020, dma16_read,    NULL, NULL, dma16_write,    NULL, NULL,  NULL);
        io_sethandler(0x0088, 0x0008, dma_page_read, NULL, NULL, dma_page_write, NULL, NULL,  NULL);
}


uint8_t _dma_read(uint32_t addr)
{
        switch (addr&0xFFFF8000)
        {
/*                case 0xA0000: case 0xA8000:
                return video_read_a000(addr, NULL);
                case 0xB0000:
                return video_read_b000(addr, NULL);
                case 0xB8000:
                return video_read_b800(addr, NULL);*/
        }
        if (isram[addr>>16]) return ram[addr];
        return 0xff;
}

void _dma_write(uint32_t addr, uint8_t val)
{
        pclog("_dma_write %08X %02X\n", addr, val);
        switch (addr&0xFFFF8000)
        {
/*                case 0xA0000: case 0xA8000:
                video_write_a000(addr,val, NULL);
                return;
                case 0xB0000:
                video_write_b000(addr,val, NULL);
                return;
                case 0xB8000:
                video_write_b800(addr,val, NULL);
                return;
                case 0xC0000: case 0xC8000: case 0xD0000: case 0xD8000:
                case 0xE0000: case 0xE8000: case 0xF0000: case 0xF8000:
                return;*/
        }
        if (isram[addr >> 16]) 
                ram[addr] = val;
}
/*void writedma2(uint8_t val)
{
//        printf("Write to %05X %02X %i\n",(dma.page[2]<<16)+dma.ac[2],val,dma.m&4);
        if (!(dma.m&4))
        {
                ram[((dma.page[2]<<16)+dma.ac[2])&rammask]=val;
                dma.ac[2]++;
                dma.cc[2]--;
                if (dma.cc[2]==-1)
                {
                        dma.m|=4;
                        dma.stat|=4;
                }
        }
}*/

int dma_channel_read(int channel)
{
        uint16_t temp;

        if (channel < 4)
        {
//                pclog("Read DMA channel %i\n", channel);
                if (dma.m & (1 << channel))
                        return DMA_NODATA;
                if ((dma.mode[channel] & 0xC) != 8)
                        return DMA_NODATA;
                temp = _dma_read(dma.ac[channel] + (dma.page[channel] << 16)); //ram[(dma.ac[2]+(dma.page[2]<<16))&rammask];
//                pclog("                - %02X %05X\n", temp, dma.ac[channel] + (dma.page[channel] << 16));
                if (dma.mode[channel] & 0x20) dma.ac[channel]--;
                else                          dma.ac[channel]++;
                dma.cc[channel]--;
                if (!dma.cc[channel] && (dma.mode[channel] & 0x10))
                {
                        dma.cc[channel] = dma.cb[channel] + 1;
                        dma.ac[channel] = dma.ab[channel];
                        dma.stat |= (1 << channel);
                }
                else if (dma.cc[channel]<=-1)
                {
                        dma.m |= (1 << channel);
                        dma.stat |= (1 << channel);
                }
                return temp;
        }
        else
        {
                channel &= 3;
                if (dma16.m & (1 << channel))
                        return DMA_NODATA;
                if ((dma16.mode[channel] & 0xC) != 8)
                        return DMA_NODATA;
                temp =  _dma_read((dma16.ac[channel] << 1) + ((dma16.page[channel] & ~1) << 16)) |
                       (_dma_read((dma16.ac[channel] << 1) + ((dma16.page[channel] & ~1) << 16) + 1) << 8);
                if (dma16.mode[channel] & 0x20) dma16.ac[channel]--;
                else                            dma16.ac[channel]++;
                dma16.cc[channel]--;
                if (!dma16.cc[channel] && (dma16.mode[channel] & 0x10))
                {
                        dma16.cc[channel] = dma16.cb[channel] + 1;
                        dma16.ac[channel] = dma16.ab[channel];
                        dma16.stat |= (1 << channel);
                }
                else if (dma16.cc[channel]<=-1)
                {
                        dma16.m |= (1 << channel);
                        dma16.stat |= (1 << channel);
                }
                return temp;
        }
}

int dma_channel_write(int channel, uint16_t val)
{
        if (channel < 4)
        {
                if (dma.m & (1 << channel))
                        return DMA_NODATA;
                if ((dma.mode[channel] & 0xC) != 4)
                        return DMA_NODATA;
                _dma_write(dma.ac[channel] + (dma.page[channel] << 16), val);
                if (dma.mode[channel]&0x20) dma.ac[channel]--;
                else                        dma.ac[channel]++;
                dma.cc[channel]--;
                if (!dma.cc[channel] && (dma.mode[channel] & 0x10))
                {
                        dma.cc[channel] = dma.cb[channel] + 1;
                        dma.ac[channel] = dma.ab[channel];
                        dma.stat |= (1 << channel);
                }
                else if (dma.cc[channel]<=-1)
                {
                        dma.m    |= (1 << channel);
                        dma.stat |= (1 << channel);
                }
        }
        else
        {
                channel &= 3;
                if (dma16.m & (1 << channel))
                        return DMA_NODATA;
                if ((dma16.mode[channel] & 0xC) != 4)
                        return DMA_NODATA;
                _dma_write((dma16.ac[channel] << 1) + ((dma16.page[channel] & ~1) << 16),     val);
                _dma_write((dma16.ac[channel] << 1) + ((dma16.page[channel] & ~1) << 16) + 1, val >> 8);                
                if (dma16.mode[channel]&0x20) dma16.ac[channel]--;
                else                          dma16.ac[channel]++;
                dma16.cc[channel]--;
                if (!dma16.cc[channel] && (dma16.mode[channel] & 0x10))
                {
                        dma16.cc[channel] = dma16.cb[channel] + 1;
                        dma16.ac[channel] = dma16.ab[channel];
                        dma16.stat |= (1 << channel);
                }
                else if (dma16.cc[channel] <= -1)
                {
                        dma16.m    |= (1 << channel);
                        dma16.stat |= (1 << channel);
                }
        }
        return 0;
}

extern int sbbufferpos;
int readdma1()
{
        uint8_t temp;
        pclog("readdma1 : Read DMA1 %02X %02X %i\n",dma.m,dma.mode[1], dma.cc[1]);
        if (dma.m & (1 << 1))
                return DMA_NODATA;
        if ((dma.mode[1]&0xC)!=8)
                return DMA_NODATA;
        temp=_dma_read((dma.ac[1]+(dma.page[1]<<16))&rammask); //ram[(dma.ac[2]+(dma.page[2]<<16))&rammask];
        pclog("readdma1 : DMA1 %02X %05X\n",temp,(dma.ac[1]+(dma.page[1]<<16))&rammask);
        if (dma.mode[1]&0x20) dma.ac[1]--;
        else                  dma.ac[1]++;
        dma.cc[1]--;
        if (!dma.cc[1] && (dma.mode[1]&0x10))
        {
                dma.cc[1]=dma.cb[1]+1;
                dma.ac[1]=dma.ab[1];
                dma.stat |= (1 << 1);
        }
        else if (dma.cc[1]<=-1)
        {
                dma.m |= (1 << 1);
                dma.stat |= (1 << 1);
        }
        return temp;
}
uint8_t readdma2()
{
        uint8_t temp;
//        pclog("Read DMA2 %02X %02X %i\n",dma.m,dma.mode[2], dma.cc[2]);
        if (dma.m&4)
        {
                fdc_abort();
                return 0xFF;
        }
        if ((dma.mode[2]&0xC)!=8)
        {
                fdc_abort();
                return 0xFF;
        }
        temp=_dma_read((dma.ac[2]+(dma.page[2]<<16))&rammask); //ram[(dma.ac[2]+(dma.page[2]<<16))&rammask];
//        pclog("DMA2 %02X %05X\n",temp,(dma.ac[2]+(dma.page[2]<<16))&rammask);
        if (dma.mode[2]&0x20) dma.ac[2]--;
        else                  dma.ac[2]++;
        dma.cc[2]--;
        if (!dma.cc[2] && (dma.mode[2]&0x10))
        {
                dma.cc[2]=dma.cb[2]+1;
                dma.ac[2]=dma.ab[2];
                dma.stat |= (1 << 2);
        }
        else if (dma.cc[2]<=-1)
        {
                dma.m|=4;
                dma.stat |= (1 << 2);
        }
        return temp;
}
int readdma3()
{
        uint8_t temp;
//        pclog("Read DMA1 %02X %02X %i\n",dma.m,dma.mode[1], dma.cc[1]);
        if (dma.m & (1 << 3))
                return DMA_NODATA;
        if ((dma.mode[3]&0xC)!=8)
                return DMA_NODATA;
        temp=_dma_read((dma.ac[3]+(dma.page[3]<<16))&rammask); //ram[(dma.ac[2]+(dma.page[2]<<16))&rammask];
        //pclog("DMA3 %02X %05X\n",temp,(dma.ac[3]+(dma.page[3]<<16))&rammask);
        if (dma.mode[3]&0x20) dma.ac[3]--;
        else                  dma.ac[3]++;
        dma.cc[3]--;
        if (!dma.cc[3] && (dma.mode[3]&0x10))
        {
                dma.cc[3]=dma.cb[3]+1;
                dma.ac[3]=dma.ab[3];
                dma.stat |= (1 << 3);
        }
        else if (dma.cc[3]<=-1)
        {
                dma.m |= (1 << 3);
                dma.stat |= (1 << 3);
        }
        return temp;
}

void writedma1(uint8_t temp)
{
//        pclog("Write DMA1 %02X %02X %04X\n",dma.m,dma.mode[1],dma.cc[1]);
        if (dma.m & (1 << 1))
                return;
        if ((dma.mode[1] & 0xC) != 4)
                return;
//        pclog("Write %05X %05X %02X\n",(dma.ac[2]+(dma.page[2]<<16)),rammask,temp);
//        ram[(dma.ac[2]+(dma.page[2]<<16))&rammask]=temp;
        _dma_write((dma.ac[1]+(dma.page[1]<<16))&rammask,temp);
        if (dma.mode[1]&0x20) dma.ac[1]--;
        else                  dma.ac[1]++;
        dma.cc[1]--;
        if (!dma.cc[1] && (dma.mode[1]&0x10))
        {
                dma.cc[1]=dma.cb[1]+1;
                dma.ac[1]=dma.ab[1];
                dma.stat |= (1 << 1);
        }
        else if (dma.cc[1]<=-1)
        {
//                pclog("Reached TC\n");
                dma.m |= (1 << 1);
                dma.stat |= (1 << 1);
        }
}
void writedma2(uint8_t temp)
{
//        pclog("Write DMA2 %02X %02X %04X\n",dma.m,dma.mode[2],dma.cc[2]);
        if (dma.m&4)
        {
                fdc_abort();
                return;
        }
        if ((dma.mode[2]&0xC)!=4)
        {
                fdc_abort();
                return;
        }
//        pclog("Write %05X %05X %02X\n",(dma.ac[2]+(dma.page[2]<<16)),rammask,temp);
//        ram[(dma.ac[2]+(dma.page[2]<<16))&rammask]=temp;
        _dma_write((dma.ac[2]+(dma.page[2]<<16))&rammask,temp);
        if (dma.mode[2]&0x20) dma.ac[2]--;
        else                  dma.ac[2]++;
        dma.cc[2]--;
        if (!dma.cc[2] && (dma.mode[2]&0x10))
        {
                dma.cc[2]=dma.cb[2]+1;
                dma.ac[2]=dma.ab[2];
                dma.stat |= (1 << 2);
        }
        else if (dma.cc[2]<=-1)
        {
//                pclog("Reached TC\n");
                fdc_abort();
                dma.m|=4;
                dma.stat |= (1 << 2);
        }
}
void writedma3(uint8_t temp)
{
//        pclog("Write DMA3 %02X %02X %04X\n",dma.m,dma.mode[3],dma.cc[3]);
        if (dma.m & (1 << 3))
                return;
        if ((dma.mode[3] & 0xC) != 4)
                return;
//        pclog("Write %05X %05X %02X\n",(dma.ac[2]+(dma.page[2]<<16)),rammask,temp);
//        ram[(dma.ac[2]+(dma.page[2]<<16))&rammask]=temp;
        _dma_write((dma.ac[3]+(dma.page[3]<<16))&rammask,temp);
        if (dma.mode[3]&0x20) dma.ac[3]--;
        else                  dma.ac[3]++;
        dma.cc[3]--;
        if (!dma.cc[3] && (dma.mode[3]&0x10))
        {
                dma.cc[3]=dma.cb[3]+1;
                dma.ac[3]=dma.ab[3];
                dma.stat |= (1 << 3);
        }
        else if (dma.cc[3]<=-1)
        {
//                pclog("Reached TC\n");
                dma.m |= (1 << 3);
                dma.stat |= (1 << 3);
        }
}

uint16_t readdma5()
{
        uint16_t temp=0;
//        pclog("Read DMA5 %i %04X\n",dma16on[1],dma16.cc[1]);
        /*if ((dma16.ac[1]+(dma16.page[1]<<16))<0x800000)  */temp=ram[((dma16.ac[1]<<1)+((dma16.page[1]&~1)<<16))&rammask]|(ram[((dma16.ac[1]<<1)+((dma16.page[1]&~1)<<16)+1)&rammask]<<8);
        //readmemwl(dma16.ac[1]+(dma16.page[1]<<16));
//        printf("Read DMA1 from %05X %05X %02X %04X\n",dma.ac[1]+(dma.page[1]<<16),(dma16.ac[1]<<1)+((dma16.page[1]&~1)<<16),dma16.mode[1],temp);
        if (!dma16on[1])
        {
//                printf("DMA off!\n");
                return temp;
        }
        dma16.ac[1]++;
        dma16.cc[1]--;
        if (dma16.cc[1]<=-1 && (dma16.mode[1]&0x10))
        {
                dma16.cc[1]=dma16.cb[1];
                dma16.ac[1]=dma16.ab[1];
                dma16.stat |= (1 << 1);
        }
        else if (dma16.cc[1]<=-1)
        {
                dma16on[1]=0;
                dma16.stat |= (1 << 1);
        }
        return temp;
}

void writedma5(uint16_t temp)
{
        if (!dma16on[1]) return;
        ram[((dma16.ac[1]<<1)+((dma16.page[1]&~1)<<16))&rammask]=temp;
        ram[((dma16.ac[1]<<1)+((dma16.page[1]&~1)<<16)+1)&rammask]=temp>>8;

        dma16.ac[1]++;
        dma16.cc[1]--;
        if (dma16.cc[1]<=-1 && (dma16.mode[1]&0x10))
        {
                dma16.cc[1]=dma16.cb[1];
                dma16.ac[1]=dma16.ab[1];
                dma16.stat |= (1 << 1);
        }
        else if (dma16.cc[1]<=-1)
        {
                dma16on[1]=0;
                dma16.stat |= (1 << 1);
        }
}

void readdma0()
{
        if (AT) ppi.pb^=0x10;        
        if (dma.command & 4) return;
//        if (AMSTRAD) return;
        refreshread();
//        pclog("Read refresh %02X %02X %04X %04X\n",dma.m,dma.mode[0],dma.ac[0],dma.cc[0]);
        if (dma.m&1) return;
//        readmembl((dma.page[0]<<16)+dma.ac[0]);
//        if (!(dma.m&1))
//        {
                dma.ac[0]+=2;
                dma.cc[0]--;
                if (dma.cc[0]==-1)
                {
                        dma.ac[0]=dma.ab[0];
                        dma.cc[0]=dma.cb[0];
                        dma.stat |= 1;
                }
//        }
//        ppi.pb^=0x10;
}

void dumpdma()
{
        printf("Address : %04X %04X %04X %04X\n",dma.ac[0],dma.ac[1],dma.ac[2],dma.ac[3]);
        printf("Count   : %04X %04X %04X %04X\n",dma.cc[0],dma.cc[1],dma.cc[2],dma.cc[3]);
        printf("Mask %02X Stat %02X\n",dma.m,dma.stat);
}
