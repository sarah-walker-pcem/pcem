#ifndef _SOUND_MMB_H_
#define _SOUND_MMB_H_
#include "sound.h"
#include "ayumi/ayumi.h"

extern device_t mmb_device;

typedef struct ay_3_891x_t {
        uint8_t index;
        uint8_t regs[16];
        struct ayumi chip;
} ay_3_891x_t;

typedef struct mmb_t {
        ay_3_891x_t first;
        ay_3_891x_t second;

        int16_t buffer[MAXSOUNDBUFLEN * 2];
        int pos;
} mmb_t;

void mmb_init(mmb_t *mmb, uint16_t base, uint16_t size, int freq);

#endif /* _SOUND_MMB_H_ */
