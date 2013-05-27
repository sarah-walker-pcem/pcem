#include "mame/fmopl.h"
#include "mame/ymf262.h"

typedef struct opl_t
{
        void *YM3812[2];
        void *YMF262;
        
        int timers[2][2];
        int timers_enable[2][2];

        int16_t *bufs[4];
        int16_t filtbuf[2];
} opl_t;

uint8_t opl_read(uint16_t a, void *priv);
void opl_write(uint16_t a, uint8_t v, void *priv);

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
void opl3_init(opl_t *opl);

void opl2_close(opl_t *opl);
void opl3_close(opl_t *opl);
