#include <stdint.h>
#include <stdlib.h>
#include "ibm.h"
#include "io.h"
#include "sound_opl.h"
#include "sound_dbopl.h"

/*Interfaces between PCem and the actual OPL emulator*/


uint8_t opl2_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return opl_read(0, a);
}
void opl2_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        opl_write(0, a, v);
        opl_write(1, a, v);
}

uint8_t opl2_l_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return opl_read(0, a);
}
void opl2_l_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        opl_write(0, a, v);
}

uint8_t opl2_r_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return opl_read(1, a);
}
void opl2_r_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        opl_write(1, a, v);
}

uint8_t opl3_read(uint16_t a, void *priv)
{
        opl_t *opl = (opl_t *)priv;

        cycles -= (int)(isa_timing * 8);
        return opl_read(0, a);
}
void opl3_write(uint16_t a, uint8_t v, void *priv)
{
        opl_t *opl = (opl_t *)priv;
        
        opl_write(0, a, v);
}


void opl2_poll(opl_t *opl, int16_t *bufl, int16_t *bufr)
{
        opl2_update(0, bufl, 1);
        opl2_update(1, bufr, 1);

        opl->filtbuf[0] = *bufl = ((*bufl) / 4) + ((opl->filtbuf[0] * 11) / 16);
        opl->filtbuf[1] = *bufr = ((*bufr) / 4) + ((opl->filtbuf[1] * 11) / 16);

        if (opl->timers_enable[0][0])
        {
                opl->timers[0][0]--;
                if (opl->timers[0][0] < 0) opl_timer_over(0, 0);
        }
        if (opl->timers_enable[0][1])
        {
                opl->timers[0][1]--;
                if (opl->timers[0][1] < 0) opl_timer_over(0, 1);
        }
        if (opl->timers_enable[1][0])
        {
                opl->timers[1][0]--;
                if (opl->timers[1][0] < 0) opl_timer_over(1, 0);
        }
        if (opl->timers_enable[1][1])
        {
                opl->timers[1][1]--;
                if (opl->timers[1][1] < 0) opl_timer_over(1, 1);
        }
}

void opl3_poll(opl_t *opl, int16_t *bufl, int16_t *bufr)
{
        opl3_update(0, bufl, bufr, 1);

        opl->filtbuf[0] = *bufl = ((*bufl) / 4) + ((opl->filtbuf[0] * 11) / 16);
        opl->filtbuf[1] = *bufr = ((*bufr) / 4) + ((opl->filtbuf[1] * 11) / 16);

        if (opl->timers_enable[0][0])
        {
                opl->timers[0][0]--;
                if (opl->timers[0][0] < 0) 
                {
                        opl->timers_enable[0][0] = 0;
                        opl_timer_over(0, 0);
                }
        }
        if (opl->timers_enable[0][1])
        {
                opl->timers[0][1]--;
                if (opl->timers[0][1] < 0) 
                {
                        opl->timers_enable[0][1] = 0;
                        opl_timer_over(0, 1);
                }
        }
}

void ym3812_timer_set_0(void *param, int timer, int64_t period)
{
        opl_t *opl = (opl_t *)param;
        
        opl->timers[0][timer] = period / 20833;
        if (!opl->timers[0][timer]) opl->timers[0][timer] = 1;
        opl->timers_enable[0][timer] = period ? 1 : 0;
}
void ym3812_timer_set_1(void *param, int timer, int64_t period)
{
        opl_t *opl = (opl_t *)param;

        opl->timers[1][timer] = period / 20833;
        if (!opl->timers[1][timer]) opl->timers[1][timer] = 1;
        opl->timers_enable[1][timer] = period ? 1 : 0;
}

void ymf262_timer_set(void *param, int timer, int64_t period)
{
        opl_t *opl = (opl_t *)param;

        opl->timers[0][timer] = period / 20833;
        if (!opl->timers[0][timer]) opl->timers[0][timer] = 1;
        opl->timers_enable[0][timer] = period ? 1 : 0;
}

void opl2_init(opl_t *opl)
{
        opl_init(ym3812_timer_set_0, opl, 0, 0);
        opl_init(ym3812_timer_set_1, opl, 1, 0);
}

void opl3_init(opl_t *opl)
{
        opl_init(ymf262_timer_set, opl, 0, 1);
}

