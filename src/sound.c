#include <stdio.h>
#include <stdlib.h>
#include "ibm.h"
#include "device.h"

#include "filters.h"

#include "sound_opl.h"

#include "sound.h"
#include "sound_adlib.h"
#include "sound_adlibgold.h"
#include "sound_pas16.h"
#include "sound_sb.h"
#include "sound_sb_dsp.h"
#include "sound_wss.h"

#include "timer.h"
#include "thread.h"

int sound_card_current = 0;
static int sound_card_last = 0;

typedef struct
{
        char name[32];
        device_t *device;
} SOUND_CARD;

static SOUND_CARD sound_cards[] =
{
        {"None",                  NULL},
        {"Adlib",                 &adlib_device},
        {"Sound Blaster 1.0",     &sb_1_device},
        {"Sound Blaster 1.5",     &sb_15_device},
        {"Sound Blaster 2.0",     &sb_2_device},
        {"Sound Blaster Pro v1",  &sb_pro_v1_device},
        {"Sound Blaster Pro v2",  &sb_pro_v2_device},
        {"Sound Blaster 16",      &sb_16_device},
        {"Sound Blaster AWE32",   &sb_awe32_device},
        {"Adlib Gold",            &adgold_device},
        {"Windows Sound System",  &wss_device},        
        {"Pro Audio Spectrum 16", &pas16_device},
        {"", NULL}
};

int sound_card_available(int card)
{
        if (sound_cards[card].device)
                return device_available(sound_cards[card].device);

        return 1;
}

char *sound_card_getname(int card)
{
        return sound_cards[card].name;
}

device_t *sound_card_getdevice(int card)
{
        return sound_cards[card].device;
}

int sound_card_has_config(int card)
{
        if (!sound_cards[card].device)
                return 0;
        return sound_cards[card].device->config ? 1 : 0;
}

void sound_card_init()
{
        if (sound_cards[sound_card_current].device)
                device_add(sound_cards[sound_card_current].device);
        sound_card_last = sound_card_current;
}

static struct
{
        void (*poll)(void *p);
        void (*get_buffer)(int16_t *buffer, int len, void *p);
        void *priv;
} sound_handlers[8];

static int sound_handlers_num;

static int sound_poll_time = 0, sound_get_buffer_time = 0;

int soundon = 1;

static int16_t cd_buffer[CD_BUFLEN * 2];
static thread_t *sound_cd_thread_h;
static event_t *sound_cd_event;
static unsigned int cd_vol_l, cd_vol_r;

void sound_set_cd_volume(unsigned int vol_l, unsigned int vol_r)
{
        cd_vol_l = vol_l;
        cd_vol_r = vol_r;
}

static void sound_cd_thread(void *param)
{
        while (1)
        {
                int c;
                
                thread_wait_event(sound_cd_event, -1);
                ioctl_audio_callback(cd_buffer, CD_BUFLEN*2);
                if (soundon)
                {
                        for (c = 0; c < CD_BUFLEN*2; c += 2)
                        {
                                cd_buffer[c]   = ((int32_t)cd_buffer[c]   * cd_vol_l) / 65535;
                                cd_buffer[c+1] = ((int32_t)cd_buffer[c+1] * cd_vol_r) / 65535;
                        }
                        givealbuffer_cd(cd_buffer);
                }
        }
}

static uint16_t *outbuffer;

void sound_init()
{
        initalmain(0,NULL);
        inital();

        outbuffer = malloc(SOUNDBUFLEN * 2 * sizeof(int16_t));
        
        sound_cd_event = thread_create_event();
        sound_cd_thread_h = thread_create(sound_cd_thread, NULL);
}

void sound_add_handler(void (*poll)(void *p), void (*get_buffer)(int16_t *buffer, int len, void *p), void *p)
{
        sound_handlers[sound_handlers_num].poll = poll;
        sound_handlers[sound_handlers_num].get_buffer = get_buffer;
        sound_handlers[sound_handlers_num].priv = p;
        sound_handlers_num++;
}

void sound_poll(void *priv)
{
        int c;

        sound_poll_time += (int)((double)TIMER_USEC * (1000000.0 / 48000.0));
         
        for (c = 0; c < sound_handlers_num; c++)
                sound_handlers[c].poll(sound_handlers[c].priv);
}

FILE *soundf;

void sound_get_buffer(void *priv)
{
        int c;

        sound_get_buffer_time += (TIMER_USEC * (1000000 / 10));

        memset(outbuffer, 0, SOUNDBUFLEN * 2 * sizeof(int16_t));

        for (c = 0; c < sound_handlers_num; c++)
                sound_handlers[c].get_buffer(outbuffer, SOUNDBUFLEN, sound_handlers[c].priv);

/*        if (!soundf) soundf=fopen("sound.pcm","wb");
        fwrite(outbuffer,(SOUNDBUFLEN)*2*2,1,soundf);*/
        
        if (soundon) givealbuffer(outbuffer);
        
        thread_set_event(sound_cd_event);
}

void sound_reset()
{
        timer_add(sound_poll, &sound_poll_time, TIMER_ALWAYS_ENABLED, NULL);
	timer_add(sound_get_buffer, &sound_get_buffer_time, TIMER_ALWAYS_ENABLED, NULL);

        sound_handlers_num = 0;
        
        sound_set_cd_volume(65535, 65535);
}
