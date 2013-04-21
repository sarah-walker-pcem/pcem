/*Jazz sample rates :
  386-33 - 12kHz
  486-33 - 20kHz
  486-50 - 32kHz
  Pentium - 45kHz*/

#include <stdint.h>
#include <stdio.h>
#include "ibm.h"
#include "filters.h"
#include "io.h"

static int sb_8_length,  sb_8_format,  sb_8_autoinit,  sb_8_pause,  sb_8_enable,  sb_8_autolen,  sb_8_output;
static int sb_16_length, sb_16_format, sb_16_autoinit, sb_16_pause, sb_16_enable, sb_16_autolen, sb_16_output;
static int sb_pausetime = -1;

static uint8_t sb_read_data[256];
static int sb_read_wp, sb_read_rp;
static int sb_speaker;

static int sb_data_stat=-1;

static int sb_irqnum = 7;

static uint8_t sbe2;
static int sbe2count;

static int sbe2dat[4][9] = {
  {  0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 },
  { -0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165 },
  { -0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151 },
  {  0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90 }
};

static uint8_t sb_data[8];
static int sb_commands[256]=
{
        -1,-1,-1,-1, 1, 2,-1, 0, 1,-1,-1,-1,-1,-1, 2, 1,
         1,-1,-1,-1, 2,-1, 2, 2,-1,-1,-1,-1, 0,-1,-1, 0,
         0,-1,-1,-1, 2,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         1, 2, 2,-1,-1,-1,-1,-1, 2,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1, 2, 2, 2, 2,-1,-1,-1,-1,-1, 0,-1, 0,
         2, 2,-1,-1,-1,-1,-1,-1, 2, 2,-1,-1,-1,-1,-1,-1,
         0,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
         3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
         0, 0,-1, 0, 0, 0, 0,-1, 0, 0, 0,-1,-1,-1,-1,-1,
         1, 0, 1, 0, 1,-1,-1, 0, 0,-1,-1,-1,-1,-1,-1,-1,
        -1,-1, 0,-1,-1,-1,-1,-1,-1, 1, 2,-1,-1,-1,-1, 0
};

char sb16_copyright[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";
uint16_t sb_dsp_versions[] = {0, 0, 0x105, 0x200, 0x201, 0x300, 0x302, 0x405};

/*These tables were 'borrowed' from DOSBox*/
	int8_t scaleMap4[64] = {
		0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
		1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
		2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
		4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
	};
	uint8_t adjustMap4[64] = {
		  0, 0, 0, 0, 0, 16, 16, 16,
		  0, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0,  0,  0,  0,
		240, 0, 0, 0, 0,  0,  0,  0
	};

	int8_t scaleMap26[40] = {
		0,  1,  2,  3,  0,  -1,  -2,  -3,
		1,  3,  5,  7, -1,  -3,  -5,  -7,
		2,  6, 10, 14, -2,  -6, -10, -14,
		4, 12, 20, 28, -4, -12, -20, -28,
		5, 15, 25, 35, -5, -15, -25, -35
	};
	uint8_t adjustMap26[40] = {
		  0, 0, 0, 8,   0, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 0, 248, 0, 0, 0
	};

	int8_t scaleMap2[24] = {
		0,  1,  0,  -1, 1,  3,  -1,  -3,
		2,  6, -2,  -6, 4, 12,  -4, -12,
		8, 24, -8, -24, 6, 48, -16, -48
	};
	uint8_t adjustMap2[24] = {
		  0, 4,   0, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 0, 252, 0
	};


int sb_freq;

int sbtype=0;

int writebusy=0; /*Needed for Amnesia*/

int16_t sbdatl=0,sbdatr=0;
int16_t sb16datl=0,sb16datr=0;

int16_t sbdat;
uint8_t sbref;
int sbdacmode,sbdacpos,sbdat2;
int8_t sbstep;
int sbleftright=0;
int sbinput=0;
int sbreadstat,sbwritestat;
int sbreset;
uint8_t sbreaddat;
uint8_t sb_command,sbdata;
int sbcommandnext=1;
uint8_t sb_test;
int sb_timei,sb_timeo;
int sbcommandstat;
int sbhalted;
int adpcmstat=0;
int sbautolen;
int sbautoinit=0;
uint8_t sbcurcommand,sbdmacommand;
float SBCONST;
int sbstereo=0;

int sbsamprate;
uint8_t sb_transfertype;
int sb_newtransfer;
int sb_dma16;
int sb_uart;
int sb_irq8,sb_irq16;
int sb_16_exit;

uint8_t mixerindex;
uint8_t sb_mixer[256];
uint8_t sb_asp_regs[256];


int sb_att[]=
{
        310,368,437,520,618,735,873,1038,1234,1467,1743,2072,2463,2927,3479,
        4134,4914,5840,6941,8250,9805,11653,13850,16461,19564,23252,27635,32845,
        39036,46395,55140,65535
};

void sb_irq(int irq8)
{
//        pclog("IRQ %i %02X\n",irq8,pic.mask);
        if (irq8) sb_irq8=1;
        else      sb_irq16=1;
        picint(1 << sb_irqnum);
}
void sb_irqc(int irq8)
{
        if (irq8) sb_irq8=0;
        else      sb_irq16=0;
        picintc(1 << sb_irqnum);
}

void sb_mixer_set()
{
        if (sbtype==SB16)
        {
                mixer.master_l=sb_att[sb_mixer[0x30]>>3];
                mixer.master_r=sb_att[sb_mixer[0x31]>>3];
                mixer.voice_l =sb_att[sb_mixer[0x32]>>3];
                mixer.voice_r =sb_att[sb_mixer[0x33]>>3];
                mixer.fm_l    =sb_att[sb_mixer[0x34]>>3];
                mixer.fm_r    =sb_att[sb_mixer[0x35]>>3];
                mixer.bass_l  =sb_mixer[0x46]>>4;
                mixer.bass_r  =sb_mixer[0x47]>>4;
                mixer.treble_l=sb_mixer[0x44]>>4;
                mixer.treble_r=sb_mixer[0x45]>>4;
                mixer.filter=0;
                pclog("%02X %02X %02X %02X %02X %02X\n",sb_mixer[0x30],sb_mixer[0x31],sb_mixer[0x32],sb_mixer[0x33],sb_mixer[0x34],sb_mixer[0x35]);
                pclog("Mixer - %04X %04X %04X %04X %04X %04X\n",mixer.master_l,mixer.master_r,mixer.voice_l,mixer.voice_r,mixer.fm_l,mixer.fm_r);
        }
        else if (sbtype==SBPRO || sbtype==SBPRO2)
        {
                mixer.master_l=sb_att[(sb_mixer[0x22]>>4)|0x11];
                mixer.master_r=sb_att[(sb_mixer[0x22]&0xF)|0x11];
                mixer.voice_l =sb_att[(sb_mixer[0x04]>>4)|0x11];
                mixer.voice_r =sb_att[(sb_mixer[0x04]&0xF)|0x11];
                mixer.fm_l    =sb_att[(sb_mixer[0x26]>>4)|0x11];
                mixer.fm_r    =sb_att[(sb_mixer[0x26]&0xF)|0x11];
                mixer.filter  =!(sb_mixer[0xE]&0x20);
                mixer.bass_l  =mixer.bass_r  =8;
                mixer.treble_l=mixer.treble_r=8;
                pclog("%02X %02X %02X\n",sb_mixer[0x04],sb_mixer[0x22],sb_mixer[0x26]);
                pclog("Mixer - %04X %04X %04X %04X %04X %04X\n",mixer.master_l,mixer.master_r,mixer.voice_l,mixer.voice_r,mixer.fm_l,mixer.fm_r);
        }
        else
        {
                mixer.master_l=mixer.master_r=65535;
                mixer.voice_l =mixer.voice_r =65535;
                mixer.fm_l    =mixer.fm_r    =65535;
                mixer.bass_l  =mixer.bass_r  =8;
                mixer.treble_l=mixer.treble_r=8;
                mixer.filter=1;
        }
}

void sb_mixer_reset()
{
        pclog("Mixer reset\n");
        if (sbtype==SB16)
        {
                sb_mixer[0x30]=31<<3;
                sb_mixer[0x31]=31<<3;
                sb_mixer[0x32]=31<<3;
                sb_mixer[0x33]=31<<3;
                sb_mixer[0x34]=31<<3;
                sb_mixer[0x35]=31<<3;
                sb_mixer[0x44]=8<<4;
                sb_mixer[0x45]=8<<4;
                sb_mixer[0x46]=8<<4;
                sb_mixer[0x47]=8<<4;
                sb_mixer[0x22]=(sb_mixer[0x30]&0xF0)|(sb_mixer[0x31]>>4);
                sb_mixer[0x04]=(sb_mixer[0x32]&0xF0)|(sb_mixer[0x33]>>4);
                sb_mixer[0x26]=(sb_mixer[0x34]&0xF0)|(sb_mixer[0x35]>>4);
        }
        if (sbtype==SBPRO || sbtype==SBPRO2)
        {
                sb_mixer[0x22]=0xFF;
                sb_mixer[0x04]=0xFF;
                sb_mixer[0x26]=0xFF;
                sb_mixer[0xE]=0;
        }
}

void sb_dsp_reset()
{
        sbenable = sb_enable_i = 0;
        sb_command = 0;
        
        sb_uart = 0;
        sb_8_length = sbautolen = 0xFFFF;

        sb_irqc(0);
        sb_irqc(1);
        sb_16_pause = 0;
        sb_read_wp = sb_read_rp = 0;
        sb_data_stat = -1;
        sb_speaker = 0;
        sb_pausetime = -1;
        sbe2 = 0xAA;
        sbe2count = 0;

        sbreset = 0;
        sbenable = sb_enable_i = sb_count_i = 0;
        sbhalted = 0;
        sbdmacommand = 0;
        sbstereo = 0;

        picintc(1 << sb_irqnum);
}

void sb_reset()
{
        int c;
        
        sb_dsp_reset();
        sb_mixer_reset();
        sb_mixer_set();
        
        if (sbtype==SB16) sb_commands[8]=1;
        else              sb_commands[8]=-1;
        
        for (c = 0; c < 256; c++)
            sb_asp_regs[c] = 0;
        sb_asp_regs[5] = 0x01;
        sb_asp_regs[9] = 0xf8;
}

void setsbclock(float clock)
{
        SBCONST=clock/1000000.0f;
        printf("SBCONST %f %f\n",SBCONST,clock);
        if (sb_timeo>255) sblatcho=(int)(SBCONST*(1000000.0f/(float)(sb_timeo-256)));
        else              sblatcho=SBCONST*(256-sb_timeo);
        if (sb_timei>255) sblatchi=(int)(SBCONST*(1000000.0f/(float)(sb_timei-256)));
        else              sblatchi=SBCONST*(256-sb_timei);
}

void outmidi(uint8_t v)
{
        printf("Write MIDI %02X\n",v);
}

void setsbtype(int type)
{
        sbtype=type;
        sbstereo=0;
        sb_mixer_set();
        if (sbtype==SB16) sb_commands[8]=1;
        else              sb_commands[8]=-1;
}







void sb_add_data(uint8_t v)
{
        sb_read_data[sb_read_wp++]=v;
        sb_read_wp&=0xFF;
}

#define ADPCM_4  1
#define ADPCM_26 2
#define ADPCM_2  3

void sb_start_dma(int dma8, int autoinit, uint8_t format, int len)
{
        if (dma8)
        {
                sb_8_length=len;
                sb_8_format=format;
                sb_8_autoinit=autoinit;
                sb_8_pause=0;
                sb_8_enable=1;
                if (sb_16_enable && sb_16_output) sb_16_enable = 0;
                sb_8_output=1;
                sbenable=sb_8_enable;
                sbleftright=0;
                sbdacpos=0;
//                pclog("Start 8-bit DMA addr %06X len %04X\n",dma.ac[1]+(dma.page[1]<<16),len);
        }
        else
        {
                sb_16_length=len;
                sb_16_format=format;
                sb_16_autoinit=autoinit;
                sb_16_pause=0;
                sb_16_enable=1;
                if (sb_8_enable && sb_8_output) sb_8_enable = 0;
                sb_16_output=1;
                sbenable=sb_16_enable;
//                pclog("Start 16-bit DMA addr %06X len %04X\n",dma16.ac[1]+(dma16.page[1]<<16),len);
        }
}

void sb_start_dma_i(int dma8, int autoinit, uint8_t format, int len)
{
        if (dma8)
        {
                sb_8_length=len;
                sb_8_format=format;
                sb_8_autoinit=autoinit;
                sb_8_pause=0;
                sb_8_enable=1;
                if (sb_16_enable && !sb_16_output) sb_16_enable = 0;                
                sb_8_output=0;
                sb_enable_i=sb_8_enable;
//                pclog("Start 8-bit input DMA addr %06X len %04X\n",dma.ac[1]+(dma.page[1]<<16),len);
        }
        else
        {
                sb_16_length=len;
                sb_16_format=format;
                sb_16_autoinit=autoinit;
                sb_16_pause=0;
                sb_16_enable=1;
                if (sb_8_enable && !sb_8_output) sb_8_enable = 0;                
                sb_16_output=0;
                sb_enable_i=sb_16_enable;
//                pclog("Start 16-bit input DMA addr %06X len %04X\n",dma.ac[1]+(dma.page[1]<<16),len);
        }
}

uint8_t sb_8_read_dma()
{
        return readdma1();
}
void sb_8_write_dma(uint8_t val)
{
        writedma1(val);
}
uint16_t sb_16_read_dma()
{
        return readdma5();
}
void sb_16_write_dma(uint16_t val)
{
        writedma5(val);
}

void sb_exec_command()
{
        int temp,c;
//        pclog("SB command %02X\n",sb_command);
        switch (sb_command)
        {
                case 0x10: /*8-bit direct mode*/
                sbdat=sbdatl=sbdatr=(sb_data[0]^0x80)<<8;
                break;
                case 0x14: /*8-bit single cycle DMA output*/
                sb_start_dma(1,0,0,sb_data[0]+(sb_data[1]<<8));
                break;
                case 0x17: /*2-bit ADPCM output with reference*/
                sbref=sb_8_read_dma();
                sbstep=0;
//                pclog("Ref byte 2 %02X\n",sbref);
                case 0x16: /*2-bit ADPCM output*/
                sb_start_dma(1,0,ADPCM_2,sb_data[0]+(sb_data[1]<<8));
                sbdat2=sb_8_read_dma();
                sb_8_length--;
                break;
                case 0x1C: /*8-bit autoinit DMA output*/
                if (sbtype<SB15) break;
                sb_start_dma(1,1,0,sb_8_autolen);
                break;
                case 0x1F: /*2-bit ADPCM autoinit output*/
                if (sbtype<SB15) break;
                sb_start_dma(1,1,ADPCM_2,sb_data[0]+(sb_data[1]<<8));
                sbdat2=sb_8_read_dma();
                sb_8_length--;
                break;
                case 0x20: /*8-bit direct input*/
                sb_add_data(0);
                break;
                case 0x24: /*8-bit single cycle DMA input*/
                sb_start_dma_i(1,0,0,sb_data[0]+(sb_data[1]<<8));
                break;
                case 0x2C: /*8-bit autoinit DMA input*/
                if (sbtype<SB15) break;
                sb_start_dma_i(1,1,0,sb_data[0]+(sb_data[1]<<8));
                break;
                case 0x40: /*Set time constant*/
                sb_timei=sb_timeo=sb_data[0];
                sblatcho=sblatchi=SBCONST*(256-sb_data[0]);
                temp=256-sb_data[0];
                temp=1000000/temp;
//                printf("Sample rate - %ihz (%i)\n",temp, sblatcho);
                sb_freq=temp;
                break;
                case 0x41: /*Set output sampling rate*/
                if (sbtype<SB16) break;
                sblatcho=(int)(SBCONST*(1000000.0f/(float)(sb_data[1]+(sb_data[0]<<8))));
//                printf("Sample rate - %ihz (%i)\n",sb_data[1]+(sb_data[0]<<8), sblatcho);
                sb_freq=sb_data[1]+(sb_data[0]<<8);
                sb_timeo=256+sb_freq;
                break;
                case 0x42: /*Set input sampling rate*/
                if (sbtype<SB16) break;
                sblatchi=(int)(SBCONST*(1000000.0f/(float)(sb_data[1]+(sb_data[0]<<8))));
//                printf("iample rate - %ihz\n",sb_data[1]+(sb_data[0]<<8));
                sb_timei=256+sb_data[1]+(sb_data[0]<<8);
                break;
                case 0x48: /*Set DSP block transfer size*/
                sb_8_autolen=sb_data[0]+(sb_data[1]<<8);
                break;
                case 0x75: /*4-bit ADPCM output with reference*/
                sbref=sb_8_read_dma();
                sbstep=0;
//                pclog("Ref byte 4 %02X\n",sbref);
                case 0x74: /*4-bit ADPCM output*/
                sb_start_dma(1,0,ADPCM_4,sb_data[0]+(sb_data[1]<<8));
                sbdat2=sb_8_read_dma();
                sb_8_length--;
                break;
                case 0x77: /*2.6-bit ADPCM output with reference*/
                sbref=sb_8_read_dma();
                sbstep=0;
//                pclog("Ref byte 26 %02X\n",sbref);
                case 0x76: /*2.6-bit ADPCM output*/
                sb_start_dma(1,0,ADPCM_26,sb_data[0]+(sb_data[1]<<8));
                sbdat2=sb_8_read_dma();
                sb_8_length--;
                break;
                case 0x7D: /*4-bit ADPCM autoinit output*/
                if (sbtype<SB15) break;
                sb_start_dma(1,1,ADPCM_4,sb_data[0]+(sb_data[1]<<8));
                sbdat2=sb_8_read_dma();
                sb_8_length--;
                break;
                case 0x7F: /*2.6-bit ADPCM autoinit output*/
                if (sbtype<SB15) break;
                sb_start_dma(1,1,ADPCM_26,sb_data[0]+(sb_data[1]<<8));
                sbdat2=sb_8_read_dma();
                sb_8_length--;
                break;
                case 0x80: /*Pause DAC*/
                sb_pausetime=sb_data[0]+(sb_data[1]<<8);
//                pclog("SB pause %04X\n",sb_pausetime);
                sbenable=1;
                break;
                case 0x90: /*High speed 8-bit autoinit DMA output*/
                if (sbtype<SB2) break;
                sb_start_dma(1,1,0,sb_8_autolen);
                break;
                case 0x91: /*High speed 8-bit single cycle DMA output*/
                if (sbtype<SB2) break;
                sb_start_dma(1,0,0,sb_8_autolen);
                break;
                case 0x98: /*High speed 8-bit autoinit DMA input*/
                if (sbtype<SB2) break;
                sb_start_dma_i(1,1,0,sb_8_autolen);
                break;
                case 0x99: /*High speed 8-bit single cycle DMA input*/
                if (sbtype<SB2) break;
                sb_start_dma_i(1,0,0,sb_8_autolen);
                break;
                case 0xA0: /*Set input mode to mono*/
                case 0xA8: /*Set input mode to stereo*/
                break;
                case 0xB0: case 0xB1: case 0xB2: case 0xB3:
                case 0xB4: case 0xB5: case 0xB6: case 0xB7: /*16-bit DMA output*/
                if (sbtype<SB16) break;
                sb_start_dma(0,sb_command&4,sb_data[0],sb_data[1]+(sb_data[2]<<8));
                sb_16_autolen=sb_data[1]+(sb_data[2]<<8);
                break;
                case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                case 0xBC: case 0xBD: case 0xBE: case 0xBF: /*16-bit DMA input*/
                if (sbtype<SB16) break;
                sb_start_dma_i(0,sb_command&4,sb_data[0],sb_data[1]+(sb_data[2]<<8));
                sb_16_autolen=sb_data[1]+(sb_data[2]<<8);
                break;
                case 0xC0: case 0xC1: case 0xC2: case 0xC3:
                case 0xC4: case 0xC5: case 0xC6: case 0xC7: /*8-bit DMA output*/
                if (sbtype<SB16) break;
                sb_start_dma(1,sb_command&4,sb_data[0],sb_data[1]+(sb_data[2]<<8));
                sb_8_autolen=sb_data[1]+(sb_data[2]<<8);
                break;
                case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                case 0xCC: case 0xCD: case 0xCE: case 0xCF: /*8-bit DMA input*/
                if (sbtype<SB16) break;
                sb_start_dma_i(1,sb_command&4,sb_data[0],sb_data[1]+(sb_data[2]<<8));
                sb_8_autolen=sb_data[1]+(sb_data[2]<<8);
                break;
                case 0xD0: /*Pause 8-bit DMA*/
                sb_8_pause=1;
                break;
                case 0xD1: /*Speaker on*/
                sb_speaker=1;
                break;
                case 0xD3: /*Speaker off*/
                sb_speaker=0;
                break;
                case 0xD4: /*Continue 8-bit DMA*/
                sb_8_pause=0;
                break;
                case 0xD5: /*Pause 16-bit DMA*/
                if (sbtype<SB16) break;
                sb_16_pause=1;
                break;
                case 0xD6: /*Continue 16-bit DMA*/
                if (sbtype<SB16) break;
                sb_16_pause=0;
                break;
                case 0xD8: /*Get speaker status*/
                sb_add_data(sb_speaker?0xFF:0);
                break;
                case 0xD9: /*Exit 16-bit auto-init mode*/
                if (sbtype<SB16) break;
                sb_16_autoinit=0;
                break;
                case 0xDA: /*Exit 8-bit auto-init mode*/
                sb_8_autoinit=0;
                break;
                case 0xE0: /*DSP identification*/
                sb_add_data(~sb_data[0]);
                break;
                case 0xE1: /*Get DSP version*/
                sb_add_data(sb_dsp_versions[sbtype]>>8);
                sb_add_data(sb_dsp_versions[sbtype]&0xFF);
                break;
                case 0xE2: /*Stupid ID/protection*/
                for (c=0;c<8;c++)
                    if (sb_data[0]&(1<<c)) sbe2+=sbe2dat[sbe2count&3][c];
                sbe2+=sbe2dat[sbe2count&3][8];
                sbe2count++;
                sb_8_write_dma(sbe2);
                break;
                case 0xE3: /*DSP copyright*/
                if (sbtype<SB16) break;
                c=0;
                while (sb16_copyright[c])
                      sb_add_data(sb16_copyright[c++]);
                sb_add_data(0);
                break;
                case 0xE4: /*Write test register*/
                sb_test=sb_data[0];
                break;
                case 0xE8: /*Read test register*/
                sb_add_data(sb_test);
                break;
                case 0xF2: /*Trigger 8-bit IRQ*/
//                pclog("Trigger IRQ\n");
                sb_irq(1);
                break;
                case 0xE7: /*???*/
                case 0xFA: /*???*/
                break;
                case 0x07: /*No, that's not how you program auto-init DMA*/
                case 0xFF:
                break;
                case 0x08: /*ASP get version*/
                if (sbtype<SB16) break;
                sb_add_data(0x18);
                break;
                case 0x0E: /*ASP set register*/
                if (sbtype<SB16) break;
                sb_asp_regs[sb_data[0]] = sb_data[1];
//                pclog("ASP write reg %02X %02X\n", sb_data[0], sb_data[1]);
                break;
                case 0x0F: /*ASP get register*/
                if (sbtype<SB16) break;
//                sb_add_data(0);
                sb_add_data(sb_asp_regs[sb_data[0]]);
//                pclog("ASP read reg %02X %02X\n", sb_data[0], sb_asp_regs[sb_data[0]]);
                break;
                case 0xF9:
                if (sbtype<SB16) break;
                if (sb_data[0] == 0x0e)      sb_add_data(0xff);
                else if (sb_data[0] == 0x0f) sb_add_data(0x07);
                else if (sb_data[0] == 0x37) sb_add_data(0x38);
                else                         sb_add_data(0x00);
                case 0x04:
                case 0x05:
                break;
//                default:
//                fatal("Exec bad SB command %02X\n",sb_command);
        }
}
        
void sb_write(uint16_t a, uint8_t v)
{
        int temp;
        int c;
//        printf("Write soundblaster %04X %02X %04X:%04X %02X\n",a,v,cs>>4,pc,sb_command);
        switch (a&0xF)
        {
                case 0: case 1: case 2: case 3: /*OPL or Gameblaster*/
                if (GAMEBLASTER && sbtype < SBPRO)
                   writecms(a, v);
                else if (sbtype >= SBPRO)
                   adlib_write(a,v); 
                return;
                case 4: mixerindex=v; return;
                case 5:
                sb_mixer[mixerindex]=v;
//                pclog("Write mixer data %02X %02X\n",mixerindex,sb_mixer[mixerindex]);
                if (sbtype==SB16)
                {
                        switch (mixerindex)
                        {
                                case 0x22:
                                sb_mixer[0x30]=((sb_mixer[0x22]>>4)|0x11)<<3;
                                sb_mixer[0x31]=((sb_mixer[0x22]&0xF)|0x11)<<3;
                                break;
                                case 0x04:
                                sb_mixer[0x32]=((sb_mixer[0x04]>>4)|0x11)<<3;
                                sb_mixer[0x33]=((sb_mixer[0x04]&0xF)|0x11)<<3;
                                break;
                                case 0x26:
                                sb_mixer[0x34]=((sb_mixer[0x26]>>4)|0x11)<<3;
                                sb_mixer[0x35]=((sb_mixer[0x26]&0xF)|0x11)<<3;
                                break;
                                case 0x80:
                                if (v & 1) sb_irqnum = 2;
                                if (v & 2) sb_irqnum = 5;
                                if (v & 4) sb_irqnum = 7;
                                if (v & 8) sb_irqnum = 10;
                                break;
                        }
                }
//                if (!mixerindex) sb_mixer_reset();
                sb_mixer_set();
                return;
                case 6: /*Reset*/
                if (!(v&1) && (sbreset&1))
                {
                        sb_dsp_reset();
                        sb_add_data(0xAA);
                }
                sbreset=v;
                return;
                case 8: case 9: adlib_write(a,v); return; /*OPL*/
                case 0xC: /*Command/data write*/
                if (sb_uart) return;
                if (sb_data_stat==-1)
                {
                        sb_command=v;
//                        if (sb_commands[v]==-1)
//                           fatal("Bad SB command %02X\n",v);
                        sb_data_stat++;
                }
                else
                   sb_data[sb_data_stat++]=v;
                if (sb_data_stat==sb_commands[sb_command] || sb_commands[sb_command]==-1)
                {
                        sb_exec_command();
                        sb_data_stat=-1;
                }
                break;
        }
}

uint8_t sb_read(uint16_t a)
{
        uint8_t temp;
//        if (a==0x224) output=1;
//        printf("Read soundblaster %04X %04X:%04X\n",a,cs,pc);
        switch (a&0xF)
        {
                case 0: case 1: case 2: case 3: /*OPL or GameBlaster*/
                if (GAMEBLASTER && sbtype < SBPRO)
                   return readcms(a);
                return adlib_read(a); 
                break;
                case 5:
                if (sbtype<SBPRO) return 0;
//                pclog("Read mixer data %02X %02X\n",mixerindex,sb_mixer[mixerindex]);
                if (mixerindex==0x80 && sbtype>=SB16)
                {
                        switch (sb_irqnum)
                        {
                                case 2: return 1; /*IRQ 7*/
                                case 5: return 2; /*IRQ 7*/
                                case 7: return 4; /*IRQ 7*/
                                case 10: return 8; /*IRQ 7*/
                        }
                }
                if (mixerindex==0x81 && sbtype>=SB16) return 0x22; /*DMA 1 and 5*/
                if (mixerindex==0x82 && sbtype>=SB16) return ((sb_irq8)?1:0)|((sb_irq16)?2:0);
                return sb_mixer[mixerindex];
                case 8: case 9: return adlib_read(a); /*OPL*/
                case 0xA: /*Read data*/
                if (sb_uart) return;
                sbreaddat=sb_read_data[sb_read_rp];
                if (sb_read_rp!=sb_read_wp)
                {
                        sb_read_rp++;
                        sb_read_rp&=0xFF;
                }
//                pclog("SB read %02X\n",sbreaddat);
                return sbreaddat;
                case 0xC: /*Write data ready*/
                cycles-=100;
                writebusy++;
                if (writebusy&8) return 0xFF;
                return 0x7F;
                case 0xE: /*Read data ready*/
                picintc(1 << sb_irqnum);
                sb_irq8=sb_irq16=0;
                return (sb_read_rp==sb_read_wp)?0x7f:0xff;
                case 0xF: /*16-bit ack*/
                sb_irq16=0;
                if (!sb_irq8) picintc(1 << sb_irqnum);
                return 0xff;
        }
        return 0;
}

void sb_init()
{
        int c;
        
        sb_reset();
        
        io_sethandler(0x0220, 0x0010, sb_read, NULL, NULL, sb_write, NULL, NULL);
}


int sbshift4[8]={-2,-1,0,0,1,1,1,1};
void pollsb()
{
        int tempi,ref;
//        pclog("PollSB %i %i %i %i\n",sb_8_enable,sb_8_pause,sb_pausetime,sb_8_output);
        if (sb_8_enable && !sb_8_pause && sb_pausetime<0 && sb_8_output)
        {
//                pclog("Dopoll %i %02X %i\n", sb_8_length, sb_8_format, sblatcho);
                switch (sb_8_format)
                {
                        case 0x00: /*Mono unsigned*/
                        sbdat=(sb_8_read_dma()^0x80)<<8;
                        if (sbtype>=SBPRO && sbtype<SB16 && sb_mixer[0xE]&2)
                        {
                                if (sbleftright) sbdatl=sbdat;
                                else             sbdatr=sbdat;
                                sbleftright=!sbleftright;
                        }
                        else
                           sbdatl=sbdatr=sbdat;
                        sb_8_length--;
                        break;
                        case 0x10: /*Mono signed*/
                        sbdat=sb_8_read_dma()<<8;
                        if (sbtype>=SBPRO && sbtype<SB16 && sb_mixer[0xE]&2)
                        {
                                if (sbleftright) sbdatl=sbdat;
                                else             sbdatr=sbdat;
                                sbleftright=!sbleftright;
                        }
                        else
                           sbdatl=sbdatr=sbdat;
                        sb_8_length--;
                        break;
                        case 0x20: /*Stereo unsigned*/
                        sbdatl=(sb_8_read_dma()^0x80)<<8;
                        sbdatr=(sb_8_read_dma()^0x80)<<8;
                        sb_8_length-=2;
                        break;
                        case 0x30: /*Stereo signed*/
                        sbdatl=sb_8_read_dma()<<8;
                        sbdatr=sb_8_read_dma()<<8;
                        sb_8_length-=2;
                        break;

                        case ADPCM_4:
                        if (sbdacpos) tempi=(sbdat2&0xF)+sbstep;
                        else          tempi=(sbdat2>>4)+sbstep;
                        if (tempi<0) tempi=0;
                        if (tempi>63) tempi=63;

                        ref = sbref + scaleMap4[tempi];
                        if (ref > 0xff) sbref = 0xff;
                        else if (ref < 0x00) sbref = 0x00;
                        else sbref = ref;

                        sbstep = (sbstep + adjustMap4[tempi]) & 0xff;

                        sbdat=(sbref^0x80)<<8;

                        sbdacpos++;
                        if (sbdacpos>=2)
                        {
                                sbdacpos=0;
                                sbdat2=sb_8_read_dma();
                                sb_8_length--;
                        }

                        if (sbtype>=SBPRO && sbtype<SB16 && sb_mixer[0xE]&2)
                        {
                                if (sbleftright) sbdatl=sbdat;
                                else             sbdatr=sbdat;
                                sbleftright=!sbleftright;
                        }
                        else
                           sbdatl=sbdatr=sbdat;
                        break;
                        
                        case ADPCM_26:
                        if (!sbdacpos)        tempi=(sbdat2>>5)+sbstep;
                        else if (sbdacpos==1) tempi=((sbdat2>>2)&7)+sbstep;
                        else                  tempi=((sbdat2<<1)&7)+sbstep;

                        if (tempi<0) tempi=0;
                        if (tempi>39) tempi=39;

                        ref = sbref + scaleMap26[tempi];
                        if (ref > 0xff) sbref = 0xff;
                        else if (ref < 0x00) sbref = 0x00;
                        else sbref = ref;
                        sbstep = (sbstep + adjustMap26[tempi]) & 0xff;

                        sbdat=(sbref^0x80)<<8;

                        sbdacpos++;
                        if (sbdacpos>=3)
                        {
                                sbdacpos=0;
                                sbdat2=readdma1();
                                sb_8_length--;
                        }

                        if (sbtype>=SBPRO && sbtype<SB16 && sb_mixer[0xE]&2)
                        {
                                if (sbleftright) sbdatl=sbdat;
                                else             sbdatr=sbdat;
                                sbleftright=!sbleftright;
                        }
                        else
                           sbdatl=sbdatr=sbdat;
                        break;

                        case ADPCM_2:
                        tempi=((sbdat2>>((3-sbdacpos)*2))&3)+sbstep;
                        if (tempi<0) tempi=0;
                        if (tempi>23) tempi=23;

                        ref = sbref + scaleMap2[tempi];
                        if (ref > 0xff) sbref = 0xff;
                        else if (ref < 0x00) sbref = 0x00;
                        else sbref = ref;
                        sbstep = (sbstep + adjustMap2[tempi]) & 0xff;
                        
                        sbdat=(sbref^0x80)<<8;

                        sbdacpos++;
                        if (sbdacpos>=4)
                        {
                                sbdacpos=0;
                                sbdat2=readdma1();
                        }

                        if (sbtype>=SBPRO && sbtype<SB16 && sb_mixer[0xE]&2)
                        {
                                if (sbleftright) sbdatl=sbdat;
                                else             sbdatr=sbdat;
                                sbleftright=!sbleftright;
                        }
                        else
                           sbdatl=sbdatr=sbdat;
                        break;

//                        default:
                                //fatal("Unrecognised SB 8-bit format %02X\n",sb_8_format);
                }
                
                if (sb_8_length<0)
                {
//                        pclog("DMA over %i %i\n", sb_8_autoinit, sb_irqnum);
                        if (sb_8_autoinit) sb_8_length=sb_8_autolen;
                        else               sb_8_enable=sbenable=0;
                        sb_irq(1);
                }
        }
        if (sb_16_enable && !sb_16_pause && sb_pausetime<0 && sb_16_output)
        {
                switch (sb_16_format)
                {
                        case 0x00: /*Mono unsigned*/
                        sbdatl=sbdatr=sb_16_read_dma()^0x8000;
                        sb_16_length--;
                        break;
                        case 0x10: /*Mono signed*/
                        sbdatl=sbdatr=sb_16_read_dma();
                        sb_16_length--;
                        break;
                        case 0x20: /*Stereo unsigned*/
                        sbdatl=sb_16_read_dma()^0x8000;
                        sbdatr=sb_16_read_dma()^0x8000;
                        sb_16_length-=2;
                        break;
                        case 0x30: /*Stereo signed*/
                        sbdatl=sb_16_read_dma();
                        sbdatr=sb_16_read_dma();
                        sb_16_length-=2;
                        break;

//                        default:
//                                fatal("Unrecognised SB 16-bit format %02X\n",sb_16_format);
                }

                if (sb_16_length<0)
                {
//                        pclog("16DMA over %i\n",sb_16_autoinit);
                        if (sb_16_autoinit) sb_16_length=sb_16_autolen;
                        else                sb_16_enable=sbenable=0;
                        sb_irq(0);
                }
        }
        if (sb_pausetime>-1)
        {
                sb_pausetime--;
                if (sb_pausetime<0)
                {
                        sb_irq(1);
                        sbenable=sb_8_enable;
                        pclog("SB pause over\n");
                }
        }
}

void sb_poll_i()
{
//        pclog("PollSBi %i %i %i %i\n",sb_8_enable,sb_8_pause,sb_pausetime,sb_8_output);        
        if (sb_8_enable && !sb_8_pause && sb_pausetime<0 && !sb_8_output)
        {
                switch (sb_8_format)
                {
                        case 0x00: /*Mono unsigned*/
                        sb_8_write_dma(0x80);
                        sb_8_length--;
                        break;
                        case 0x10: /*Mono signed*/
                        sb_8_write_dma(0x00);
                        sb_8_length--;
                        break;
                        case 0x20: /*Stereo unsigned*/
                        sb_8_write_dma(0x80);
                        sb_8_write_dma(0x80);
                        sb_8_length-=2;
                        break;
                        case 0x30: /*Stereo signed*/
                        sb_8_write_dma(0x00);
                        sb_8_write_dma(0x00);
                        sb_8_length-=2;
                        break;

//                        default:
//                                fatal("Unrecognised SB 8-bit input format %02X\n",sb_8_format);
                }
                
                if (sb_8_length<0)
                {
//                        pclog("Input DMA over %i\n",sb_8_autoinit);
                        if (sb_8_autoinit) sb_8_length=sb_8_autolen;
                        else               sb_8_enable=sbenable=0;
                        sb_irq(1);
                }
        }
        if (sb_16_enable && !sb_16_pause && sb_pausetime<0 && !sb_16_output)
        {
                switch (sb_16_format)
                {
                        case 0x00: /*Unsigned mono*/
                        sb_16_write_dma(0x8000);
                        sb_16_length--;
                        break;
                        case 0x10: /*Signed mono*/
                        sb_16_write_dma(0);
                        sb_16_length--;
                        break;
                        case 0x20: /*Unsigned stereo*/
                        sb_16_write_dma(0x8000);
                        sb_16_write_dma(0x8000);
                        sb_16_length-=2;
                        break;
                        case 0x30: /*Signed stereo*/
                        sb_16_write_dma(0);
                        sb_16_write_dma(0);
                        sb_16_length-=2;
                        break;
                        
//                        default:
//                                fatal("Unrecognised SB 16-bit input format %02X\n",sb_16_format);
                }
                
                if (sb_16_length<0)
                {
//                        pclog("16iDMA over %i\n",sb_16_autoinit);
                        if (sb_16_autoinit) sb_16_length=sb_16_autolen;
                        else                sb_16_enable=sbenable=0;
                        sb_irq(0);
                }
        }
}

int16_t sbbuffer[2][SOUNDBUFLEN+20];
int sbbufferpos=0;
int _sampcnt=0;
int16_t sb_filtbuf[4]={0,0,0,0};
void getsbsamp()
{
        int vocl=sbpmixer.vocl*sbpmixer.masl;
        int vocr=sbpmixer.vocr*sbpmixer.masr;
        int16_t sbdat[2];
        double t;
        
        vocl=256-vocl;
        vocr=256-vocr;
        if (!vocl) vocl=1;
        if (!vocr) vocr=1;
        if (sbbufferpos>=(SOUNDBUFLEN+20)) return;
        
        sbdat[0]=sbdatl;
        sbdat[1]=sbdatr;

        if (mixer.filter)
        {
                /*3.2kHz output filter*/
                sbdat[0]=(int16_t)(sb_iir(0,(float)sbdat[0])/1.3);
                sbdat[1]=(int16_t)(sb_iir(1,(float)sbdat[1])/1.3);
        }

        sbbuffer[0][sbbufferpos]=sbdat[0];
        sbbuffer[1][sbbufferpos]=sbdat[1];
        sbbufferpos++;
}

void addsb(int16_t *p)
{
        int c;
        if (sbbufferpos>SOUNDBUFLEN) sbbufferpos=SOUNDBUFLEN;
//        printf("Addsb - %i %04X\n",sbbufferpos,p[0]);
        for (c=0;c<sbbufferpos;c++)
        {
                p[c<<1]+=(((sbbuffer[0][c]/3)*mixer.voice_l)>>16);
                p[(c<<1)+1]+=(((sbbuffer[1][c]/3)*mixer.voice_r)>>16);
//                if (!c) pclog("V %04X %04X %04X\n",sbbuffer[0][c],p[c<<1],mixer.voice_l);
        }
        for (;c<SOUNDBUFLEN;c++)
        {
                p[c<<1]+=(((sbbuffer[0][sbbufferpos-1]/3)*mixer.voice_l)>>16);
                p[(c<<1)+1]+=(((sbbuffer[0][sbbufferpos-1]/3)*mixer.voice_l)>>16);
        }
        sbbufferpos=0;
}

