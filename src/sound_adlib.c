#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "sound.h"

#include "sound_adlib.h"
#include "sound_opl.h"

typedef struct adlib_t
{
        opl_t   opl;
        int16_t buffer[SOUNDBUFLEN * 2];
        int     pos;
} adlib_t;

static void adlib_poll(void *p)
{
        adlib_t *adlib = (adlib_t *)p;
        
        if (adlib->pos >= SOUNDBUFLEN) return;
        
        opl2_poll(&adlib->opl, &adlib->buffer[adlib->pos * 2], &adlib->buffer[(adlib->pos * 2) + 1]);
        adlib->pos++;
}

static void adlib_get_buffer(int16_t *buffer, int len, void *p)
{
        adlib_t *adlib = (adlib_t *)p;
        int c;

        for (c = 0; c < len * 2; c++)
                buffer[c] += adlib->buffer[c];

        adlib->pos = 0;
}

void *adlib_init()
{
        adlib_t *adlib = malloc(sizeof(adlib_t));
        memset(adlib, 0, sizeof(adlib_t));
        
        pclog("adlib_init\n");
        opl2_init(&adlib->opl);
        io_sethandler(0x0388, 0x0002, opl2_read, NULL, NULL, opl2_write, NULL, NULL, &adlib->opl);
        sound_add_handler(adlib_poll, adlib_get_buffer, adlib);
        return adlib;
}

void adlib_close(void *p)
{
        adlib_t *adlib = (adlib_t *)p;

        free(adlib);
}

device_t adlib_device =
{
        "AdLib",
        0,
        adlib_init,
        adlib_close,
        NULL,
        NULL,
        NULL,
        NULL
};
