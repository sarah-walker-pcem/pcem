#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "io.h"
#include "sound_opl.h"

/*Interfaces between PCem and the actual OPL emulator*/


uint8_t opl2_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return ym3812_read(opl->YM3812[0], a);
}
void opl2_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        ym3812_write(opl->YM3812[0],a,v);
        ym3812_write(opl->YM3812[1],a,v);
}

uint8_t opl2_l_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return ym3812_read(opl->YM3812[0], a);
}
void opl2_l_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        ym3812_write(opl->YM3812[0],a,v);
}

uint8_t opl2_r_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return ym3812_read(opl->YM3812[1], a);
}
void opl2_r_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        ym3812_write(opl->YM3812[1],a,v);
}

uint8_t opl3_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return ymf262_read(opl->YMF262, a);
}
void opl3_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;
        
        ymf262_write(opl->YMF262, a, v);
}


void opl2_poll(opl_t *opl, int16_t *bufl, int16_t *bufr)
{
        ym3812_update_one(opl->YM3812[0], bufl, 1);
        ym3812_update_one(opl->YM3812[1], bufr, 1);

        opl->filtbuf[0] = *bufl = ((*bufl) / 4) + ((opl->filtbuf[0] * 11) / 16);
        opl->filtbuf[1] = *bufr = ((*bufr) / 4) + ((opl->filtbuf[1] * 11) / 16);

        if (opl->timers_enable[0][0])
        {
                opl->timers[0][0]--;
                if (opl->timers[0][0] < 0) ym3812_timer_over(opl->YM3812[0], 0);
        }
        if (opl->timers_enable[0][1])
        {
                opl->timers[0][1]--;
                if (opl->timers[0][1] < 0) ym3812_timer_over(opl->YM3812[0], 1);
        }
        if (opl->timers_enable[1][0])
        {
                opl->timers[1][0]--;
                if (opl->timers[1][0] < 0) ym3812_timer_over(opl->YM3812[1], 0);
        }
        if (opl->timers_enable[1][1])
        {
                opl->timers[1][1]--;
                if (opl->timers[1][1] < 0) ym3812_timer_over(opl->YM3812[1], 1);
        }
}

void opl3_poll(opl_t *opl, int16_t *bufl, int16_t *bufr)
{
        ymf262_update_one(opl->YMF262, opl->bufs, 1);

        opl->filtbuf[0] = *bufl = ((opl->bufs[0][0]) / 4) + ((opl->filtbuf[0] * 11) / 16);
        opl->filtbuf[1] = *bufr = ((opl->bufs[1][0]) / 4) + ((opl->filtbuf[1] * 11) / 16);

        if (opl->timers_enable[0][0])
        {
                opl->timers[0][0]--;
                if (opl->timers[0][0] < 0) 
                {
                        opl->timers_enable[0][0] = 0;
                        ymf262_timer_over(opl->YMF262, 0);
                }
        }
        if (opl->timers_enable[0][1])
        {
                opl->timers[0][1]--;
                if (opl->timers[0][1] < 0) 
                {
                        opl->timers_enable[0][1] = 0;
                        ymf262_timer_over(opl->YMF262, 1);
                }
        }
}

void ym3812_timer_set_0(void *param, int timer, attotime period)
{
        opl_t *opl = (opl_t *)param;
        
        opl->timers[0][timer] = period / 20833;
        if (!opl->timers[0][timer]) opl->timers[0][timer] = 1;
        opl->timers_enable[0][timer] = period ? 1 : 0;
}
void ym3812_timer_set_1(void *param, int timer, attotime period)
{
        opl_t *opl = (opl_t *)param;

        opl->timers[1][timer] = period / 20833;
        if (!opl->timers[1][timer]) opl->timers[1][timer] = 1;
        opl->timers_enable[1][timer] = period ? 1 : 0;
}

void ymf262_timer_set(void *param, int timer, attotime period)
{
        opl_t *opl = (opl_t *)param;

        opl->timers[0][timer] = period / 20833;
        if (!opl->timers[0][timer]) opl->timers[0][timer] = 1;
        opl->timers_enable[0][timer] = period ? 1 : 0;
}

void opl2_init(opl_t *opl)
{
        opl->bufs[0] = (int16_t *)malloc(4);
        opl->bufs[1] = (int16_t *)malloc(4);
        opl->bufs[2] = (int16_t *)malloc(4);
        opl->bufs[3] = (int16_t *)malloc(4);
        
        opl->YM3812[0] = ym3812_init(NULL, 3579545, 48000);
        ym3812_reset_chip(opl->YM3812[0]);
        ym3812_set_timer_handler(opl->YM3812[0], ym3812_timer_set_0, opl);

        opl->YM3812[1] = ym3812_init(NULL, 3579545, 48000);
        ym3812_reset_chip(opl->YM3812[1]);
        ym3812_set_timer_handler(opl->YM3812[1], ym3812_timer_set_1, opl);
}

void opl3_init(opl_t *opl)
{
        opl->bufs[0] = (int16_t *)malloc(4);
        opl->bufs[1] = (int16_t *)malloc(4);
        opl->bufs[2] = (int16_t *)malloc(4);
        opl->bufs[3] = (int16_t *)malloc(4);

        opl->YMF262 = ymf262_init(NULL, 3579545 * 4, 48000);
        ymf262_reset_chip(opl->YMF262);
        ymf262_set_timer_handler(opl->YMF262, ymf262_timer_set, opl);
}

void opl2_close(opl_t *opl)
{
        free(opl->bufs[0]);
        free(opl->bufs[1]);
        
        ym3812_shutdown(opl->YM3812[0]);
        ym3812_shutdown(opl->YM3812[1]);
}

void opl3_close(opl_t *opl)
{
        free(opl->bufs[0]);
        free(opl->bufs[1]);
        
        ym3812_shutdown(opl->YM3812[0]);
        ymf262_shutdown(opl->YMF262);
}
