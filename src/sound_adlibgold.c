#include <stdlib.h>
#include <string.h>
#include "ibm.h"
#include "device.h"

#include "sound_opl.h"
#include "dma.h"
#include "io.h"
#include "pic.h"
#include "pit.h"
#include "timer.h"

void adgold_timer_poll();

typedef struct adgold_t
{
        int adgold_irq_status;

        uint8_t adgold_eeprom[0x19];

        uint8_t adgold_status;
        int adgold_38x_state, adgold_38x_addr;
        uint8_t adgold_38x_regs[0x19];

        int adgold_mma_addr;
        uint8_t adgold_mma_regs[2][0xe];

        int adgold_mma_enable[2];
        uint8_t adgold_mma_fifo[2][256];
        int adgold_mma_fifo_start[2], adgold_mma_fifo_end[2];
        uint8_t adgold_mma_status;

        int16_t adgold_mma_out[2];
        int adgold_mma_intpos[2];

        int adgold_mma_timer_count;

        struct
        {
                int timer0_latch, timer0_count;
                int timerbase_latch, timerbase_count;
                int timer1_latch, timer1_count;
                int timer2_latch, timer2_count, timer2_read;
        
                int voice_count[2], voice_latch[2];
        } adgold_mma;

        opl_t    opl;

        int16_t opl_buffer[SOUNDBUFLEN * 2];
        int16_t mma_buffer[2][SOUNDBUFLEN];

        int pos;
} adgold_t;

void adgold_update_irq_status(adgold_t *adgold)
{
        uint8_t temp = 0xf;

        if (!(adgold->adgold_mma_regs[0][8] & 0x10) && (adgold->adgold_mma_status & 0x10))      /*Timer 0*/
                temp &= ~2;
        if (!(adgold->adgold_mma_regs[0][8] & 0x20) && (adgold->adgold_mma_status & 0x20))      /*Timer 1*/
                temp &= ~2;
        if (!(adgold->adgold_mma_regs[0][8] & 0x40) && (adgold->adgold_mma_status & 0x40))      /*Timer 2*/
                temp &= ~2;

        if ((adgold->adgold_mma_status & 0x01) &&  !(adgold->adgold_mma_regs[0][0xc] & 2))
                temp &= ~2;
        if ((adgold->adgold_mma_status & 0x02) &&  !(adgold->adgold_mma_regs[1][0xc] & 2))
                temp &= ~2;
        adgold->adgold_status = temp;
        
        if ((adgold->adgold_status ^ 0xf) && !adgold->adgold_irq_status)
        {
                pclog("adgold irq %02X\n", adgold->adgold_status);
                picint(0x80);
        }
                
        adgold->adgold_irq_status = adgold->adgold_status ^ 0xf;
}

void adgold_getsamp_dma(adgold_t *adgold, int channel)
{
        int temp;
        
        if ((adgold->adgold_mma_regs[channel][0xc] & 0x60) && (((adgold->adgold_mma_fifo_end[channel] - adgold->adgold_mma_fifo_start[channel]) & 255) >= 127))
                return;
                
        temp = dma_channel_read(1);
//        pclog("adgold DMA1 return %02X %i L\n", temp, channel);
        if (temp == DMA_NODATA) return;
        adgold->adgold_mma_fifo[channel][adgold->adgold_mma_fifo_end[channel]] = temp;
        adgold->adgold_mma_fifo_end[channel] = (adgold->adgold_mma_fifo_end[channel] + 1) & 255;
        if (adgold->adgold_mma_regs[channel][0xc] & 0x60)
        {
                temp = dma_channel_read(1);
//                pclog("adgold DMA1 return %02X %i H\n", temp, channel);
                adgold->adgold_mma_fifo[channel][adgold->adgold_mma_fifo_end[channel]] = temp;
                adgold->adgold_mma_fifo_end[channel] = (adgold->adgold_mma_fifo_end[channel] + 1) & 255;
        }      
        if (((adgold->adgold_mma_fifo_end[channel] - adgold->adgold_mma_fifo_start[channel]) & 255) >= adgold->adgold_mma_intpos[channel])
        {
                adgold->adgold_mma_status &= ~(0x01 << channel);
                adgold_update_irq_status(adgold);
        }
}

void adgold_write(uint16_t addr, uint8_t val, void *p)
{
        adgold_t *adgold = (adgold_t *)p;
        if (addr > 0x389) pclog("adgold_write : addr %04X val %02X %04X:%04X\n", addr, val, CS, pc);
        switch (addr & 7)
        {
                case 0: case 1:
                opl3_write(addr, val, &adgold->opl);
                break;

                case 2:
                if (val == 0xff)
                {
                        adgold->adgold_38x_state = 1;
                        return;
                }
                if (val == 0xfe)
                {                
                        adgold->adgold_38x_state = 0;
                        return;
                }
                if (adgold->adgold_38x_state)   /*Write to control chip*/
                        adgold->adgold_38x_addr = val;
                else
                        opl3_write(addr, val, &adgold->opl);
                break;
                case 3:
                if (adgold->adgold_38x_state)
                {
                        if (adgold->adgold_38x_addr >= 0x19) break;
                        switch (adgold->adgold_38x_addr)
                        {
                                case 0x00: /*Control/ID*/
                                if (val & 1)
                                        memcpy(adgold->adgold_38x_regs, adgold->adgold_eeprom, 0x19);
                                if (val & 2)
                                        memcpy(adgold->adgold_eeprom, adgold->adgold_38x_regs, 0x19);
                                break;
                                
                                default:
                                adgold->adgold_38x_regs[adgold->adgold_38x_addr] = val;
                                break;
                        }
                }
                else
                        opl3_write(addr, val, &adgold->opl);
                break;
                case 4: case 6:
                adgold->adgold_mma_addr = val;
                break;
                case 5:
                if (adgold->adgold_mma_addr >= 0xf) break;
                switch (adgold->adgold_mma_addr)
                {
                        case 0x2:
                        adgold->adgold_mma.timer0_latch = (adgold->adgold_mma.timer0_latch & 0xff00) | val;
                        break;
                        case 0x3:
                        adgold->adgold_mma.timer0_latch = (adgold->adgold_mma.timer0_latch & 0xff) | (val << 8);
                        break;
                        case 0x4:
                        adgold->adgold_mma.timerbase_latch = (adgold->adgold_mma.timerbase_latch & 0xf00) | val;
                        break;
                        case 0x5:
                        adgold->adgold_mma.timerbase_latch = (adgold->adgold_mma.timerbase_latch & 0xff) | ((val & 0xf) << 8);
                        adgold->adgold_mma.timer1_latch = val >> 4;
                        break;
                        case 0x6:
                        adgold->adgold_mma.timer2_latch = (adgold->adgold_mma.timer2_latch & 0xff00) | val;
                        break;
                        case 0x7:
                        adgold->adgold_mma.timer2_latch = (adgold->adgold_mma.timer2_latch & 0xff) | (val << 8);
                        break;
                        
                        case 0x8: 
                        if ((val & 1) && !(adgold->adgold_mma_regs[0][8] & 1)) /*Reload timer 0*/
                                adgold->adgold_mma.timer0_count = adgold->adgold_mma.timer0_latch;
                                
                        if ((val & 2) && !(adgold->adgold_mma_regs[0][8] & 2)) /*Reload timer 1*/
                                adgold->adgold_mma.timer1_count = adgold->adgold_mma.timer1_latch;

                        if ((val & 4) && !(adgold->adgold_mma_regs[0][8] & 4)) /*Reload timer 2*/
                                adgold->adgold_mma.timer2_count = adgold->adgold_mma.timer2_latch;

                        if ((val & 8) && !(adgold->adgold_mma_regs[0][8] & 8)) /*Reload base timer*/
                                adgold->adgold_mma.timerbase_count = adgold->adgold_mma.timerbase_latch;
                        break;
                        
                        case 0x9:
                        switch (val & 0x18)
                        {
                                case 0x00: adgold->adgold_mma.voice_latch[0] = 12; break; /*44100 Hz*/
                                case 0x08: adgold->adgold_mma.voice_latch[0] = 24; break; /*22050 Hz*/
                                case 0x10: adgold->adgold_mma.voice_latch[0] = 48; break; /*11025 Hz*/
                                case 0x18: adgold->adgold_mma.voice_latch[0] = 72; break; /* 7350 Hz*/
                        }
                        if (val & 0x80)
                        {
                                adgold->adgold_mma_enable[0] = 0;
                                adgold->adgold_mma_fifo_end[0] = adgold->adgold_mma_fifo_start[0] = 0;
                                adgold->adgold_mma_status &= ~0x01;
                                adgold_update_irq_status(adgold);
                        }
                        if ((val & 0x01))     /*Start playback*/
                        {
                                if (!(adgold->adgold_mma_regs[0][0x9] & 1))
                                        adgold->adgold_mma.voice_count[0] = adgold->adgold_mma.voice_latch[0];
                                        
                                pclog("adgold start! FIFO fill %i  %i %i %02X\n", (adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255, adgold->adgold_mma_fifo_end[0], adgold->adgold_mma_fifo_start[0], adgold->adgold_mma_regs[0][0xc]);
                                if (adgold->adgold_mma_regs[0][0xc] & 1)
                                {
                                        if (adgold->adgold_mma_regs[0][0xc] & 0x80)
                                        {
//                                                pclog("adgold start interleaved %i %i
                                                adgold->adgold_mma_enable[1] = 1;
                                                adgold->adgold_mma.voice_count[1] = adgold->adgold_mma.voice_latch[1];

                                                while (((adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255) < 128)
                                                {
                                                        adgold_getsamp_dma(adgold, 0);
                                                        adgold_getsamp_dma(adgold, 1);
                                                }
                                                if (((adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255) >= adgold->adgold_mma_intpos[0])
                                                {
                                                        adgold->adgold_mma_status &= ~0x01;
                                                        adgold_update_irq_status(adgold);
                                                }
                                                if (((adgold->adgold_mma_fifo_end[1] - adgold->adgold_mma_fifo_start[1]) & 255) >= adgold->adgold_mma_intpos[1])
                                                {
                                                        adgold->adgold_mma_status &= ~0x02;
                                                        adgold_update_irq_status(adgold);
                                                }
                                        }
                                        else
                                        {
                                                while (((adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255) < 128)
                                                {
                                                        adgold_getsamp_dma(adgold, 0);
                                                }
                                                if (((adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255) >= adgold->adgold_mma_intpos[0])
                                                {
                                                        adgold->adgold_mma_status &= ~0x01;
                                                        adgold_update_irq_status(adgold);
                                                }
                                        }
                                }
                                pclog("adgold end\n");
                        }
                        adgold->adgold_mma_enable[0] = val & 0x01;
                        break;
                        
                        case 0xb:
                        if (((adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255) < 128)
                        {
                                adgold->adgold_mma_fifo[0][adgold->adgold_mma_fifo_end[0]] = val;
                                adgold->adgold_mma_fifo_end[0] = (adgold->adgold_mma_fifo_end[0] + 1) & 255;
                                if (((adgold->adgold_mma_fifo_end[0] - adgold->adgold_mma_fifo_start[0]) & 255) >= adgold->adgold_mma_intpos[0])
                                {
                                        adgold->adgold_mma_status &= ~0x01;
                                        adgold_update_irq_status(adgold);
                                }
                        }
                        break;
                        
                        case 0xc:
                        adgold->adgold_mma_intpos[0] = (7 - ((val >> 2) & 7)) * 8;
                        break;
                }
                adgold->adgold_mma_regs[0][adgold->adgold_mma_addr] = val;
                break;
                case 7:
                if (adgold->adgold_mma_addr >= 0xf) break;
                switch (adgold->adgold_mma_addr)
                {
                        case 0x9:
                        switch (val & 0x18)
                        {
                                case 0x00: adgold->adgold_mma.voice_latch[1] = 12; break; /*44100 Hz*/
                                case 0x08: adgold->adgold_mma.voice_latch[1] = 24; break; /*22050 Hz*/
                                case 0x10: adgold->adgold_mma.voice_latch[1] = 48; break; /*11025 Hz*/
                                case 0x18: adgold->adgold_mma.voice_latch[1] = 72; break; /* 7350 Hz*/
                        }
                        if (val & 0x80)
                        {
                                adgold->adgold_mma_enable[1] = 0;
                                adgold->adgold_mma_fifo_end[1] = adgold->adgold_mma_fifo_start[1] = 0;
                                adgold->adgold_mma_status &= ~0x02;
                                adgold_update_irq_status(adgold);
                        }
                        if ((val & 0x01))     /*Start playback*/
                        {
                                if (!(adgold->adgold_mma_regs[1][0x9] & 1)) 
                                        adgold->adgold_mma.voice_count[1] = adgold->adgold_mma.voice_latch[1];
                                        
                                pclog("adgold start! FIFO fill %i  %i %i %02X\n", (adgold->adgold_mma_fifo_end[1] - adgold->adgold_mma_fifo_start[1]) & 255, adgold->adgold_mma_fifo_end[1], adgold->adgold_mma_fifo_start[1], adgold->adgold_mma_regs[1][0xc]);
                                if (adgold->adgold_mma_regs[1][0xc] & 1)
                                {
                                        while (((adgold->adgold_mma_fifo_end[1] - adgold->adgold_mma_fifo_start[1]) & 255) < 128)
                                        {
                                                adgold_getsamp_dma(adgold, 1);
                                        }
                                }
                                pclog("adgold end\n");
                        }
                        adgold->adgold_mma_enable[1] = val & 0x01;
                        break;
                        
                        case 0xb:
                        if (((adgold->adgold_mma_fifo_end[1] - adgold->adgold_mma_fifo_start[1]) & 255) < 128)
                        {
                                adgold->adgold_mma_fifo[1][adgold->adgold_mma_fifo_end[1]] = val;
                                adgold->adgold_mma_fifo_end[1] = (adgold->adgold_mma_fifo_end[1] + 1) & 255;
                                if (((adgold->adgold_mma_fifo_end[1] - adgold->adgold_mma_fifo_start[1]) & 255) >= adgold->adgold_mma_intpos[1])
                                {
                                        adgold->adgold_mma_status &= ~0x02;
                                        adgold_update_irq_status(adgold);
                                }
                        }
                        break;

                        case 0xc:
                        adgold->adgold_mma_intpos[1] = (7 - ((val >> 2) & 7)) * 8;
                        break;
                }
                adgold->adgold_mma_regs[1][adgold->adgold_mma_addr] = val;
                break;
        }
}

uint8_t adgold_read(uint16_t addr, void *p)
{
        adgold_t *adgold = (adgold_t *)p;
        uint8_t temp;
        
        switch (addr & 7)
        {
                case 0: case 1:
                temp = opl3_read(addr, &adgold->opl);
                break;
                
                case 2:
                if (adgold->adgold_38x_state)   /*Read from control chip*/
                        temp = adgold->adgold_status;
                else
                        temp = opl3_read(addr, &adgold->opl);
                break;
                
                case 3:
                if (adgold->adgold_38x_state)
                {
                        if (adgold->adgold_38x_addr >= 0x19) temp = 0xff;
                        switch (adgold->adgold_38x_addr)
                        {
                                case 0x00: /*Control/ID*/
                                temp = 0x70; /*16-bit ISA, no telephone/surround/CD-ROM*/
                                break;
                                
                                default:
                                temp = adgold->adgold_38x_regs[adgold->adgold_38x_addr];
                        }                
                }
                else
                        temp = opl3_read(addr, &adgold->opl);
                break;

                case 4: case 6:
                temp = adgold->adgold_mma_status;
                adgold->adgold_mma_status = 0; /*JUKEGOLD expects timer status flags to auto-clear*/
                adgold_update_irq_status(adgold);
                break;
                case 5:
                if (adgold->adgold_mma_addr >= 0xf) temp = 0xff;
                switch (adgold->adgold_mma_addr)
                {
                        case 6: /*Timer 2 low*/
                        adgold->adgold_mma.timer2_read = adgold->adgold_mma.timer2_count;
                        temp = adgold->adgold_mma.timer2_read & 0xff;
                        break;
                        case 7: /*Timer 2 high*/
                        temp = adgold->adgold_mma.timer2_read >> 8;
                        break;
                        
                        default:
                        temp = adgold->adgold_mma_regs[0][adgold->adgold_mma_addr];
                        break;
                }
                break;
                case 7:
                if (adgold->adgold_mma_addr >= 0xf) temp = 0xff;
                temp = adgold->adgold_mma_regs[1][adgold->adgold_mma_addr];
                break;
        }
        if (addr > 0x389) pclog("adgold_read : addr %04X %02X\n", addr, temp);
        return temp;
}

void adgold_mma_poll(adgold_t *adgold, int channel)
{
        int16_t dat;

        if (adgold->adgold_mma_fifo_start[channel] != adgold->adgold_mma_fifo_end[channel])
        {
                switch (adgold->adgold_mma_regs[channel][0xc] & 0x60)
                {
                        case 0x00: /*8-bit*/
                        dat = adgold->adgold_mma_fifo[channel][adgold->adgold_mma_fifo_start[channel]] * 256;
                        adgold->adgold_mma_out[channel] = dat;
                        adgold->adgold_mma_fifo_start[channel] = (adgold->adgold_mma_fifo_start[channel] + 1) & 255;
                        break;
                        
                        case 0x40: /*12-bit sensible format*/
                        if (((adgold->adgold_mma_fifo_end[channel] - adgold->adgold_mma_fifo_start[channel]) & 255) < 2)
                                return;
                                
                        dat = adgold->adgold_mma_fifo[channel][adgold->adgold_mma_fifo_start[channel]] & 0xf0;
                        adgold->adgold_mma_fifo_start[channel] = (adgold->adgold_mma_fifo_start[channel] + 1) & 255;
                        dat |= (adgold->adgold_mma_fifo[channel][adgold->adgold_mma_fifo_start[channel]] << 8);
                        adgold->adgold_mma_fifo_start[channel] = (adgold->adgold_mma_fifo_start[channel] + 1) & 255;
                        adgold->adgold_mma_out[channel] = dat;
                        break;
                }
                
                if (adgold->adgold_mma_regs[channel][0xc] & 1)
                {
                        adgold_getsamp_dma(adgold, channel);
                }
                if (((adgold->adgold_mma_fifo_end[channel] - adgold->adgold_mma_fifo_start[channel]) & 255) < adgold->adgold_mma_intpos[channel] && !(adgold->adgold_mma_status & 0x01))
                {
//                        pclog("adgold_mma_poll - IRQ! %i\n", channel);
                        adgold->adgold_mma_status |= 1 << channel;
                        adgold_update_irq_status(adgold);
                }
        }
        if (adgold->adgold_mma_fifo_start[channel] == adgold->adgold_mma_fifo_end[channel])
        {
                adgold->adgold_mma_enable[channel] = 0;
        }
}

void adgold_timer_poll(void *p)
{
        adgold_t *adgold = (adgold_t *)p;
        
        while (adgold->adgold_mma_timer_count <= 0)
        {
                adgold->adgold_mma_timer_count += (int)((double)TIMER_USEC * 1.88964);
                if (adgold->adgold_mma_regs[0][8] & 0x01) /*Timer 0*/
                {
                        adgold->adgold_mma.timer0_count--;
                        if (!adgold->adgold_mma.timer0_count)
                        {
                                adgold->adgold_mma.timer0_count = adgold->adgold_mma.timer0_latch;
                                pclog("Timer 0 interrupt\n");
                                adgold->adgold_mma_status |= 0x10;
                                adgold_update_irq_status(adgold);
                        }
                }
                if (adgold->adgold_mma_regs[0][8] & 0x08) /*Base timer*/
                {
                        adgold->adgold_mma.timerbase_count--;
                        if (!adgold->adgold_mma.timerbase_count)
                        {
                                adgold->adgold_mma.timerbase_count = adgold->adgold_mma.timerbase_latch;
                                if (adgold->adgold_mma_regs[0][8] & 0x02) /*Timer 1*/
                                {
                                        adgold->adgold_mma.timer1_count--;
                                        if (!adgold->adgold_mma.timer1_count)
                                        {
                                                adgold->adgold_mma.timer1_count = adgold->adgold_mma.timer1_latch;
                                                pclog("Timer 1 interrupt\n");
                                                adgold->adgold_mma_status |= 0x20;
                                                adgold_update_irq_status(adgold);
                                        }
                                }
                                if (adgold->adgold_mma_regs[0][8] & 0x04) /*Timer 2*/
                                {
                                        adgold->adgold_mma.timer2_count--;
                                        if (!adgold->adgold_mma.timer2_count)
                                        {
                                                adgold->adgold_mma.timer2_count = adgold->adgold_mma.timer2_latch;
                                                pclog("Timer 2 interrupt\n");
                                                adgold->adgold_mma_status |= 0x40;
                                                adgold_update_irq_status(adgold);
                                        }
                                }
                        }
                }

                if (adgold->adgold_mma_enable[0])
                {
                        adgold->adgold_mma.voice_count[0]--;
                        if (!adgold->adgold_mma.voice_count[0])
                        {
                                adgold->adgold_mma.voice_count[0] = adgold->adgold_mma.voice_latch[0];
                                adgold_mma_poll(adgold, 0);
                        }
                }
                if (adgold->adgold_mma_enable[1])
                {
                        adgold->adgold_mma.voice_count[1]--;
                        if (!adgold->adgold_mma.voice_count[1])
                        {
                                adgold->adgold_mma.voice_count[1] = adgold->adgold_mma.voice_latch[1];
                                adgold_mma_poll(adgold, 1);
                        }
                }
        }
}

void adgold_poll(void *p)
{
        adgold_t *adgold = (adgold_t *)p;
        
        if (adgold->pos >= SOUNDBUFLEN)
                return;

        opl3_poll(&adgold->opl, &adgold->opl_buffer[adgold->pos * 2], &adgold->opl_buffer[(adgold->pos * 2) + 1]);
        adgold->mma_buffer[0][adgold->pos] = adgold->mma_buffer[1][adgold->pos] = 0;
        
        if (adgold->adgold_mma_regs[0][9] & 0x20)
                adgold->mma_buffer[0][adgold->pos] += adgold->adgold_mma_out[0] / 2;
        if (adgold->adgold_mma_regs[0][9] & 0x40)
                adgold->mma_buffer[1][adgold->pos] += adgold->adgold_mma_out[0] / 2;

        if (adgold->adgold_mma_regs[1][9] & 0x20)
                adgold->mma_buffer[0][adgold->pos] += adgold->adgold_mma_out[1] / 2;
        if (adgold->adgold_mma_regs[1][9] & 0x40)
                adgold->mma_buffer[1][adgold->pos] += adgold->adgold_mma_out[1] / 2;

        adgold->pos++;
}

static void adgold_get_buffer(int16_t *buffer, int len, void *p)
{
        adgold_t *adgold = (adgold_t *)p;
        
        int c;

        for (c = 0; c < len * 2; c++)
        {
                buffer[c] += adgold->opl_buffer[c];
                buffer[c] += adgold->mma_buffer[c & 1][c >> 1] / 2;
        }

        adgold->pos = 0;
}


void *adgold_init()
{
        adgold_t *adgold = malloc(sizeof(adgold_t));
        memset(adgold, 0, sizeof(adgold_t));

        opl3_init(&adgold->opl);

        adgold->adgold_status = 0xf;
        adgold->adgold_38x_addr = 0;
        adgold->adgold_eeprom[0x09] = adgold->adgold_eeprom[0x0a] = 255;
        adgold->adgold_eeprom[0x13] = 3 | (1 << 4);     /*IRQ 7, DMA 1*/
        adgold->adgold_eeprom[0x14] = 3 << 4;           /*DMA 3*/
        adgold->adgold_eeprom[0x15] = 0x388 / 8;        /*Present at 388-38f*/
        memcpy(adgold->adgold_38x_regs, adgold->adgold_eeprom, 0x19);

        adgold->adgold_mma_enable[0] = 0;
        adgold->adgold_mma_fifo_start[0] = adgold->adgold_mma_fifo_end[0] = 0;
        
        /*388/389 are handled by adlib_init*/
        io_sethandler(0x0388, 0x0008, adgold_read, NULL, NULL, adgold_write, NULL, NULL, adgold);
        
        timer_add(adgold_timer_poll, &adgold->adgold_mma_timer_count, TIMER_ALWAYS_ENABLED, adgold);

        sound_add_handler(adgold_poll, adgold_get_buffer, adgold);
        
        return adgold;
}

void adgold_close(void *p)
{
        adgold_t *adgold = (adgold_t *)p;
        
        free(adgold);
}

device_t adgold_device =
{
        "AdLib Gold",
        0,
        adgold_init,
        adgold_close,
        NULL,
        NULL,
        NULL,
        NULL
};
