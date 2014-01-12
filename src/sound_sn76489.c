#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "sound.h"
#include "sound_sn76489.h"

int sn76489_mute;

static float volslog[16]=
{
	0.00000f,0.59715f,0.75180f,0.94650f,
        1.19145f,1.50000f,1.88835f,2.37735f,
        2.99295f,3.76785f,4.74345f,5.97165f,
        7.51785f,9.46440f,11.9194f,15.0000f
};

typedef struct sn76489_t
{
        int stat[4];
        int latch[4], count[4];
        int freqlo[4], freqhi[4];
        int vol[4];
        uint32_t shift;
        uint8_t noise;
        int lasttone;
        uint8_t firstdat;
        
        int16_t buffer[SOUNDBUFLEN];
        int pos;
} sn76489_t;

#define PSGCONST ((3579545.0 / 64.0) / 48000.0)

void sn76489_poll(void *p)
{
        sn76489_t *sn76489 = (sn76489_t *)p;
        int c;
        int16_t result = 0;
        
        if (sn76489->pos >= SOUNDBUFLEN)
                return;
                
        for (c = 1; c < 4; c++)
        {
                if (sn76489->latch[c] > 256) result += (int16_t) (volslog[sn76489->vol[c]] * sn76489->stat[c]);
                else                         result += (int16_t) (volslog[sn76489->vol[c]] * 127);

                sn76489->count[c] -= (256 * PSGCONST);
                while ((int)sn76489->count[c] < 0 && sn76489->latch[c])
                {
                        sn76489->count[c] += sn76489->latch[c];
                        sn76489->stat[c] = -sn76489->stat[c];
                }
        }
        result += (((sn76489->shift & 1) ^ 1) * 127 * volslog[sn76489->vol[0]] * 2);

        sn76489->count[0] -= (512 * PSGCONST);
        while ((int)sn76489->count[0] < 0 && sn76489->latch[0])
        {
                sn76489->count[0] += (sn76489->latch[0] * 2);
                if (!(sn76489->noise & 4))
                {
                        if (sn76489->shift & 1) 
                                sn76489->shift |= 0x8000;
                        sn76489->shift >>= 1;
                }
                else
                {
                        if ((sn76489->shift & 1) ^ ((sn76489->shift >> 1) & 1)) 
                                sn76489->shift |= 0x8000;
                        sn76489->shift >>= 1;
                }
        }
        
        sn76489->buffer[sn76489->pos++] = result;
}

void sn76489_get_buffer(int16_t *buffer, int len, void *p)
{
        sn76489_t *sn76489 = (sn76489_t *)p;
        
        int c;
        
        if (!sn76489_mute)
        {
                for (c = 0; c < len * 2; c++)
                        buffer[c] += sn76489->buffer[c >> 1];
        }

        sn76489->pos = 0;
}

void sn76489_write(uint16_t addr, uint8_t data, void *p)
{
        sn76489_t *sn76489 = (sn76489_t *)p;
        int freq;

        if (data & 0x80)
        {
                sn76489->firstdat = data;
                switch (data & 0x70)
                {
                        case 0:
                        sn76489->freqlo[3] = data & 0xf;
                        sn76489->latch[3] = (sn76489->freqlo[3] | (sn76489->freqhi[3] << 4)) << 6;
                        sn76489->lasttone = 3;
                        break;
                        case 0x10:
                        data &= 0xf;
                        sn76489->vol[3] = 0xf - data;
                        break;
                        case 0x20:
                        sn76489->freqlo[2] = data & 0xf;
                        sn76489->latch[2] = (sn76489->freqlo[2] | (sn76489->freqhi[2] << 4)) << 6;
                        sn76489->lasttone = 2;
                        break;
                        case 0x30:
                        data &= 0xf;
                        sn76489->vol[2] = 0xf - data;
                        break;
                        case 0x40:
                        sn76489->freqlo[1] = data & 0xf;
                        sn76489->latch[1] = (sn76489->freqlo[1] | (sn76489->freqhi[1] << 4)) << 6;
                        sn76489->lasttone = 1;
                        break;
                        case 0x50:
                        data &= 0xf;
                        sn76489->vol[1] = 0xf - data;
                        break;
                        case 0x60:
                        sn76489->shift = 0x4000;
                        if ((data & 3) != (sn76489->noise & 3)) sn76489->count[0] = 0;
                        sn76489->noise = data & 0xf;
                        if ((data & 3) == 3) sn76489->latch[0] = sn76489->latch[1];
                        else                 sn76489->latch[0] = 0x400 << (data & 3);
                        break;
                        case 0x70:
                        data &= 0xf;
                        sn76489->vol[0] = 0xf - data;
                        break;
                }
        }
        else
        {
                if ((sn76489->firstdat & 0x70) == 0x60)
                {
                        sn76489->shift = 0x4000;
                        if ((data & 3) != (sn76489->noise & 3)) 
                                sn76489->count[0] = 0;
                        sn76489->noise = data & 0xf;
                        if ((data & 3) == 3) sn76489->latch[0] = sn76489->latch[1];
                        else                 sn76489->latch[0] = 0x400 << (data & 3);
                }
                else
                {
                        sn76489->freqhi[sn76489->lasttone] = data & 0x3F;
                        freq = sn76489->freqlo[sn76489->lasttone] | (sn76489->freqhi[sn76489->lasttone] << 4);
                        if ((sn76489->noise & 3) == 3 && sn76489->lasttone == 1)
                           sn76489->latch[0] = freq << 6;
                        sn76489->latch[sn76489->lasttone] = freq << 6;
                }
        }
}

void *sn76489_init()
{
        sn76489_t *sn76489 = malloc(sizeof(sn76489_t));
        memset(sn76489, 0, sizeof(sn76489_t));

        io_sethandler(0x00C0, 0x0001, NULL, NULL, NULL, sn76489_write, NULL, NULL, sn76489);

        sound_add_handler(sn76489_poll, sn76489_get_buffer, sn76489);

        sn76489->latch[0] = sn76489->latch[1] = sn76489->latch[2] = sn76489->latch[3] = 0x3FF << 6;
        sn76489->vol[0] = 0;
        sn76489->vol[1] = sn76489->vol[2] = sn76489->vol[3] = 8;
        sn76489->stat[0] = sn76489->stat[1] = sn76489->stat[2] = sn76489->stat[3] = 127;
        srand(time(NULL));
        sn76489->count[0] = 0;
        sn76489->count[1] = (rand()&0x3FF)<<6;
        sn76489->count[2] = (rand()&0x3FF)<<6;
        sn76489->count[3] = (rand()&0x3FF)<<6;
        sn76489->noise = 3;
        sn76489->shift = 0x4000;

        sn76489_mute = 0;

        return sn76489;
}

void sn76489_close(void *p)
{
        sn76489_t *sn76489 = (sn76489_t *)p;

        free(sn76489);        
}

device_t sn76489_device =
{
        "TI SN74689 PSG",
        0,
        sn76489_init,
        sn76489_close,
        NULL,
        NULL,
        NULL,
        NULL
};
