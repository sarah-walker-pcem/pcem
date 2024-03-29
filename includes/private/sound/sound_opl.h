#ifndef _SOUND_OPL_H_
#define _SOUND_OPL_H_
#include "sound.h"

typedef struct opl_t {
        int chip_nr[2];

        pc_timer_t timers[2][2];

        int16_t filtbuf[2];

        int16_t buffer[MAXSOUNDBUFLEN * 2];
        int pos;
} opl_t;

uint8_t opl2_read(uint16_t a, void *priv);
void opl2_write(uint16_t a, uint8_t v, void *priv);
uint8_t opl2_l_read(uint16_t a, void *priv);
void opl2_l_write(uint16_t a, uint8_t v, void *priv);
uint8_t opl2_r_read(uint16_t a, void *priv);
void opl2_r_write(uint16_t a, uint8_t v, void *priv);
uint8_t opl3_read(uint16_t a, void *priv);
void opl3_write(uint16_t a, uint8_t v, void *priv);

void opl2_poll(opl_t *opl, int16_t *bufl, int16_t *bufr);
void opl3_poll(opl_t *opl, int16_t *bufl, int16_t *bufr);

void opl2_init(opl_t *opl);
void opl3_init(opl_t *opl, int opl_emu);

void opl2_update2(opl_t *opl);
void opl3_update2(opl_t *opl);

#define OPL_DBOPL 0
#define OPL_NUKED 1

#endif /* _SOUND_OPL_H_ */
