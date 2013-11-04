/*PCem v0.8 by Tom Walker

  Windows Sound System compatible CODEC emulation (AD1848)*/

#include <math.h>  
#include <stdlib.h>
#include "ibm.h"

#include "device.h"
#include "dma.h"
#include "io.h"
#include "pic.h"
#include "sound_opl.h"
#include "sound_wss.h"

/*530, 11, 3 - 530=23*/
/*530, 11, 1 - 530=22*/
/*530, 11, 0 - 530=21*/
/*530, 10, 1 - 530=1a*/
/*530, 9,  1 - 530=12*/
/*530, 7,  1 - 530=0a*/
/*604, 11, 1 - 530=22*/
/*e80, 11, 1 - 530=22*/
/*f40, 11, 1 - 530=22*/


static int wss_dma[4] = {0, 0, 1, 3};
static int wss_irq[8] = {5, 7, 9, 10, 11, 12, 14, 15}; /*W95 only uses 7-9, others may be wrong*/
static uint16_t wss_addr[4] = {0x530, 0x604, 0xe80, 0xf40};

typedef struct wss_t
{
        uint8_t config;
        
        int index;
        uint8_t regs[16];
        uint8_t status;
        
        int trd;
        int mce;
        
        int count;
        
        int16_t out_l, out_r;
        
        int interp_count, inc;

        int enable;

        int irq, dma;
        
        opl_t    opl;

        int16_t opl_buffer[SOUNDBUFLEN * 2];
        int16_t pcm_buffer[2][SOUNDBUFLEN];

        int pos;
} wss_t;

static int wss_vols[64];

uint8_t wss_read(uint16_t addr, void *p)
{
        wss_t *wss = (wss_t *)p;
        uint8_t temp = 0xff;
//        pclog("wss_read - addr %04X %04X(%08X):%08X ", addr, CS, cs, pc);
        switch (addr & 7)
        {
                case 0: case 1: case 2: case 3: /*Version*/
                temp = 4 | (wss->config & 0x40);
                break;
                
                case 4: /*Index*/
                temp = wss->index | wss->trd | wss->mce;
                break;
                case 5:
                temp = wss->regs[wss->index];
                break;
                case 6:
                temp = wss->status;
                break;
        }
//        pclog("return %02X\n", temp);
        return temp;
}

void wss_write(uint16_t addr, uint8_t val, void *p)
{
        wss_t *wss = (wss_t *)p;
        double freq;
        pclog("wss_write - addr %04X val %02X  %04X(%08X):%08X\n", addr, val, CS, cs, pc);
        switch (addr & 7)
        {
                case 0: case 1: case 2: case 3: /*Config*/
                wss->config = val;
                wss->dma = wss_dma[val & 3];
                wss->irq = wss_irq[(val >> 3) & 7];
                break;
                
                case 4: /*Index*/
                wss->index = val & 0xf;
                wss->trd   = val & 0x20;
                wss->mce   = val & 0x40;
                break;
                case 5:
                switch (wss->index)
                {
                        case 8:
                        freq = (val & 1) ? 16934400 : 24576000;
                        switch ((val >> 1) & 7)
                        {
                                case 0: freq /= 3072; break;
                                case 1: freq /= 1536; break;
                                case 2: freq /= 896;  break;
                                case 3: freq /= 768;  break;
                                case 4: freq /= 448;  break;
                                case 5: freq /= 384;  break;
                                case 6: freq /= 512;  break;
                                case 7: freq /= 2560; break;
                        }
                        wss->inc = (int)((freq * 65536) / 48000);
                        break;
                        
                        case 9:
                        if (!wss->enable)
                                wss->interp_count = 0;
                                
                        wss->enable = ((val & 0x41) == 0x01);
                        break;
                                
                        case 12:
                        return;
                        
                        case 14:
                        wss->count = wss->regs[15] | (val << 8);
                        break;
                }
                wss->regs[wss->index] = val;
                break;
                case 6:
                wss->status &= 0xfe;
                break;              
        }
}

static void wss_poll(void *p)
{
        wss_t *wss = (wss_t *)p;
        
        if (wss->pos >= SOUNDBUFLEN)
                return;

        opl3_poll(&wss->opl, &wss->opl_buffer[wss->pos * 2], &wss->opl_buffer[(wss->pos * 2) + 1]);
        
        if (wss->enable)
        {
                int32_t temp;
                
                if (wss->regs[6] & 0x80)
                        wss->pcm_buffer[1][wss->pos] = 0;
                else
                        wss->pcm_buffer[1][wss->pos] = (wss->out_l * wss_vols[wss->regs[6] & 0x3f]) >> 16;

                if (wss->regs[7] & 0x80)
                        wss->pcm_buffer[0][wss->pos] = 0;
                else
                        wss->pcm_buffer[0][wss->pos] = (wss->out_r * wss_vols[wss->regs[7] & 0x3f]) >> 16;
                
                wss->interp_count += wss->inc;
                if (wss->interp_count >= 0x10000)
                {
                        wss->interp_count -= 0x10000;

                        if (wss->count < 0)
                        {               
                                wss->count = wss->regs[15] | (wss->regs[14] << 8);
                                if (!(wss->status & 0x01))
                                {
                                        wss->status |= 0x01;
                                        if (wss->regs[0xa] & 2)
                                                picint(1 << wss->irq);
                                }                                
                        }

                        switch (wss->regs[8] & 0x70)
                        {
                                case 0x00: /*Mono, 8-bit PCM*/
                                wss->out_l = wss->out_r = (dma_channel_read(wss->dma) ^ 0x80) * 256;
                                break;
                                case 0x10: /*Stereo, 8-bit PCM*/
                                wss->out_l = (dma_channel_read(wss->dma) ^ 0x80)  * 256;
                                wss->out_r = (dma_channel_read(wss->dma) ^ 0x80)  * 256;
                                break;
                
                                case 0x40: /*Mono, 16-bit PCM*/
                                temp = dma_channel_read(wss->dma);
                                wss->out_l = wss->out_r = dma_channel_read(wss->dma) | (temp << 8);
                                break;
                                case 0x50: /*Stereo, 16-bit PCM*/
                                temp = dma_channel_read(wss->dma);
                                wss->out_l = dma_channel_read(wss->dma) | (temp << 8);
                                temp = dma_channel_read(wss->dma);
                                wss->out_r = dma_channel_read(wss->dma) | (temp << 8);
                                break;
                        }
        
                        wss->count--;
                }
//                pclog("wss_poll : enable %X %X  %X %X  %X %X\n", wss->pcm_buffer[0][wss->pos], wss->pcm_buffer[1][wss->pos], wss->out_l[0], wss->out_r[0], wss->out_l[1], wss->out_r[1]);
        }
        else
        {
                wss->pcm_buffer[0][wss->pos] = wss->pcm_buffer[1][wss->pos] = 0;
//                pclog("wss_poll : not enable\n");
        }
        wss->pos++;
}

static void wss_get_buffer(int16_t *buffer, int len, void *p)
{
        wss_t *wss = (wss_t *)p;
        
        int c;

        for (c = 0; c < len * 2; c++)
        {
                buffer[c] += wss->opl_buffer[c];
                buffer[c] += (wss->pcm_buffer[c & 1][c >> 1] / 2);
        }

        wss->pos = 0;
}

void *wss_init()
{
        wss_t *wss = malloc(sizeof(wss_t));
        int c;
        double attenuation;

        memset(wss, 0, sizeof(wss_t));

        opl3_init(&wss->opl);
        
        pclog("wss_init - %i %i\n", sbtype, SND_WSS);

        wss->enable = 0;
                        
        wss->status = 0xcc;
        wss->index = wss->trd = 0;
        wss->mce = 0x40;
        
        wss->regs[0] = wss->regs[1] = 0;
        wss->regs[2] = wss->regs[3] = 0x80;
        wss->regs[4] = wss->regs[5] = 0x80;
        wss->regs[6] = wss->regs[7] = 0x80;
        wss->regs[8] = 0;
        wss->regs[9] = 0x08;
        wss->regs[10] = wss->regs[11] = 0;
        wss->regs[12] = 0xa;
        wss->regs[13] = 0;
        wss->regs[14] = wss->regs[15] = 0;
        
        wss->out_l = 0;
        wss->out_r = 0;
        wss->interp_count = 0;
        
        wss->irq = 7;
        wss->dma = 3;

        io_sethandler(0x0388, 0x0004, opl3_read, NULL, NULL, opl3_write, NULL, NULL,  &wss->opl);
        io_sethandler(0x0530, 0x0008, wss_read,  NULL, NULL, wss_write,  NULL, NULL,  wss);
        
        sound_add_handler(wss_poll, wss_get_buffer, wss);
        
        for (c = 0; c < 64; c++)
        {
                attenuation = 0.0;
                if (c & 0x01) attenuation -= 1.5;
                if (c & 0x02) attenuation -= 3.0;
                if (c & 0x04) attenuation -= 6.0;
                if (c & 0x08) attenuation -= 12.0;
                if (c & 0x10) attenuation -= 24.0;
                if (c & 0x20) attenuation -= 48.0;
                
                attenuation = pow(10, attenuation / 10);
                
                wss_vols[c] = (int)(attenuation * 65536);
                pclog("wss_vols %i = %f %i\n", c, attenuation, wss_vols[c]);
        }
        
        return wss;
}

void wss_close(void *p)
{
        wss_t *wss = (wss_t *)p;
        
        free(wss);
}

device_t wss_device =
{
        "Windows Sound System",
        wss_init,
        wss_close,
        NULL,
        NULL,
        NULL,
        NULL
};
