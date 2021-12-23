#include <stdio.h>
#include <stdlib.h>
#include "ibm.h"
#include "device.h"

#include "cdrom-ioctl.h"
#include "cdrom-image.h"
#include "ide.h"

#include "filters.h"

#include "sound_opl.h"

#include "sound.h"
#include "sound_adlib.h"
#include "sound_adlibgold.h"
#include "sound_audiopci.h"
#include "sound_azt2316a.h"
#include "sound_pas16.h"
#include "sound_sb.h"
#include "sound_sb_dsp.h"
#include "sound_wss.h"

#include "timer.h"
#include "thread.h"

#include <pcem/devices.h>

int sound_card_current = 0;
static int sound_card_last = 0;

SOUND_CARD *sound_cards[256];

SOUND_CARD sc_1 = {"None", "none", NULL};
SOUND_CARD sc_2 = {"Adlib", "adlib", &adlib_device};
SOUND_CARD sc_3 = {"Adlib", "adlib_mca", &adlib_mca_device};
SOUND_CARD sc_4 = {"Sound Blaster 1.0", "sb", &sb_1_device};
SOUND_CARD sc_5 = {"Sound Blaster 1.5", "sb1.5", &sb_15_device};
SOUND_CARD sc_6 = {"Sound Blaster MCV", "sbmcv", &sb_mcv_device};
SOUND_CARD sc_7 = {"Sound Blaster 2.0", "sb2.0", &sb_2_device};
SOUND_CARD sc_8 = {"Sound Blaster Pro v1", "sbprov1", &sb_pro_v1_device};
SOUND_CARD sc_9 = {"Sound Blaster Pro v2", "sbprov2", &sb_pro_v2_device};
SOUND_CARD sc_10 = {"Sound Blaster Pro MCV", "sbpromcv", &sb_pro_mcv_device};
SOUND_CARD sc_11 = {"Sound Blaster 16", "sb16", &sb_16_device};
SOUND_CARD sc_12 = {"Sound Blaster AWE32", "sbawe32", &sb_awe32_device};
SOUND_CARD sc_13 = {"Adlib Gold", "adlibgold", &adgold_device};
SOUND_CARD sc_14 = {"Windows Sound System", "wss", &wss_device};
SOUND_CARD sc_15 = {"Aztech Sound Galaxy Pro 16 AB (Washington)", "azt2316a", &azt2316a_device};
SOUND_CARD sc_16 = {"Aztech Sound Galaxy Nova 16 Extra (Clinton)", "azt1605", &azt1605_device};
SOUND_CARD sc_17 = {"Pro Audio Spectrum 16", "pas16", &pas16_device};
SOUND_CARD sc_18 = {"Ensoniq AudioPCI (ES1371)", "es1371", &es1371_device};
SOUND_CARD sc_19 = {"Sound Blaster PCI 128", "sbpci128", &es1371_device};

int sound_card_available(int card)
{
        if (sound_cards[card] != NULL && sound_cards[card]->device != NULL)
                return device_available(sound_cards[card]->device);

        return 1;
}

char *sound_card_getname(int card)
{
        if (sound_cards[card] != NULL)
                return sound_cards[card]->name;

        return "";
}

device_t *sound_card_getdevice(int card)
{
        if (sound_cards[card] != NULL)
                return sound_cards[card]->device;

        return NULL;
}

int sound_card_has_config(int card)
{
        if (sound_cards[card] != NULL || !sound_cards[card]->device)
                return 0;
        return sound_cards[card]->device->config ? 1 : 0;
}

char *sound_card_get_internal_name(int card)
{
        if (sound_cards[card] != NULL)
                return sound_cards[card]->internal_name;
}

int sound_card_get_from_internal_name(char *s)
{
	int c = 0;
	
	while (sound_cards[c] != NULL)
	{
		if (!strcmp(sound_cards[c]->internal_name, s))
			return c;
		c++;
	}
	
	return 0;
}

void sound_card_init()
{
        if (sound_cards[sound_card_current]->device)
                device_add(sound_cards[sound_card_current]->device);
        sound_card_last = sound_card_current;
}

static struct
{
        void (*get_buffer)(int32_t *buffer, int len, void *p);
        void *priv;
} sound_handlers[8];

static int sound_handlers_num;

static pc_timer_t sound_poll_timer;
static uint64_t sound_poll_latch;
int sound_pos_global = 0;

int soundon = 1;

static int16_t cd_buffer[CD_BUFLEN * 2];
static thread_t *sound_cd_thread_h;
static event_t *sound_cd_event;
static unsigned int cd_vol_l, cd_vol_r;

int sound_buf_len = 48000 / 10;
int sound_gain = 0;

void sound_update_buf_length()
{
        int new_buf_len = (48000 / (1000 / sound_buf_len)) / 4;
        
        if (new_buf_len > MAXSOUNDBUFLEN)
                new_buf_len = MAXSOUNDBUFLEN;
                
        SOUNDBUFLEN = new_buf_len;
}

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
                thread_reset_event(sound_cd_event);
                memset(cd_buffer, 0, CD_BUFLEN*2 * 2);
                ioctl_audio_callback(cd_buffer, CD_BUFLEN*2);
                image_audio_callback(cd_buffer, CD_BUFLEN*2);
                if (soundon)
                {
                        int32_t atapi_vol_l = atapi_get_cd_volume(0);
                        int32_t atapi_vol_r = atapi_get_cd_volume(1);
                        int channel_select[2];
                        
                        channel_select[0] = atapi_get_cd_channel(0);
                        channel_select[1] = atapi_get_cd_channel(1);
                        
                        for (c = 0; c < CD_BUFLEN*2; c += 2)
                        {
                                int32_t cd_buffer_temp[2] = {0, 0};
                                
        			/*First, adjust input from drive according to ATAPI volume.*/
        			cd_buffer[c]   = ((int32_t)cd_buffer[c]   * atapi_vol_l) / 255;
                                cd_buffer[c+1] = ((int32_t)cd_buffer[c+1] * atapi_vol_r) / 255;

                                /*Apply ATAPI channel select*/
                                if (channel_select[0] & 1)
                                        cd_buffer_temp[0] += cd_buffer[c];
                                if (channel_select[0] & 2)
                                        cd_buffer_temp[1] += cd_buffer[c];
                                if (channel_select[1] & 1)
                                        cd_buffer_temp[0] += cd_buffer[c+1];
                                if (channel_select[1] & 2)
                                        cd_buffer_temp[1] += cd_buffer[c+1];
                                
                                /*Apply sound card CD volume*/
                                cd_buffer_temp[0] = (cd_buffer_temp[0] * (int)cd_vol_l) / 65535;
                                cd_buffer_temp[1] = (cd_buffer_temp[1] * (int)cd_vol_r) / 65535;

                                if (cd_buffer_temp[0] > 32767)
                                        cd_buffer_temp[0] = 32767;
                                if (cd_buffer_temp[0] < -32768)
                                        cd_buffer_temp[0] = -32768;
                                if (cd_buffer_temp[1] > 32767)
                                        cd_buffer_temp[1] = 32767;
                                if (cd_buffer_temp[1] < -32768)
                                        cd_buffer_temp[1] = -32768;

                                cd_buffer[c]   = cd_buffer_temp[0];
                                cd_buffer[c+1] = cd_buffer_temp[1];
                        }

                        givealbuffer_cd(cd_buffer);
                }
        }
}

static int32_t *outbuffer;

void sound_init()
{
        initalmain(0,NULL);
        inital();

        outbuffer = malloc(MAXSOUNDBUFLEN * 2 * sizeof(int32_t));
        
        sound_cd_event = thread_create_event();
        sound_cd_thread_h = thread_create(sound_cd_thread, NULL);
}

void sound_add_handler(void (*get_buffer)(int32_t *buffer, int len, void *p), void *p)
{
        sound_handlers[sound_handlers_num].get_buffer = get_buffer;
        sound_handlers[sound_handlers_num].priv = p;
        sound_handlers_num++;
}

static int cd_pos = 0;
void sound_poll(void *priv)
{
	timer_advance_u64(&sound_poll_timer, sound_poll_latch);

        cd_pos++;
        if (cd_pos == (CD_BUFLEN * 48000) / CD_FREQ)
        {
                cd_pos = 0;
                thread_set_event(sound_cd_event);
        }
        
        sound_pos_global++;
        if (sound_pos_global == SOUNDBUFLEN)
        {
                int c;
/*                int16_t buf16[SOUNDBUFLEN * 2 ];*/

                memset(outbuffer, 0, SOUNDBUFLEN * 2 * sizeof(int32_t));

                for (c = 0; c < sound_handlers_num; c++)
                        sound_handlers[c].get_buffer(outbuffer, SOUNDBUFLEN, sound_handlers[c].priv);


/*                for (c=0;c<SOUNDBUFLEN*2;c++)
                {
                        if (outbuffer[c] < -32768)
                                buf16[c] = -32768;
                        else if (outbuffer[c] > 32767)
                                buf16[c] = 32767;
                        else
                                buf16[c] = outbuffer[c];
                }

        if (!soundf) soundf=fopen("sound.pcm","wb");
        fwrite(buf16,(SOUNDBUFLEN)*2*2,1,soundf);*/
        
                if (soundon) givealbuffer(outbuffer);

                sound_pos_global = 0;
                sound_update_buf_length();
        }
}

void sound_speed_changed()
{
        sound_poll_latch = (uint64_t)((double)TIMER_USEC * (1000000.0 / 48000.0));
}

void sound_reset()
{
        timer_add(&sound_poll_timer, sound_poll, NULL, 1);

        sound_handlers_num = 0;
        
        sound_set_cd_volume(65535, 65535);
        ioctl_audio_stop();
        image_audio_stop();
}

void sound_init_builtin()
{
        memset(sound_cards, 0, sizeof(sound_cards));

        sound_cards[0] = &sc_1;
        sound_cards[1] = &sc_2;
        sound_cards[2] = &sc_3;
        sound_cards[3] = &sc_4;
        sound_cards[4] = &sc_5;
        sound_cards[5] = &sc_6;
        sound_cards[6] = &sc_7;
        sound_cards[7] = &sc_8;
        sound_cards[8] = &sc_9;
        sound_cards[9] = &sc_10;
        sound_cards[10] = &sc_11;
        sound_cards[11] = &sc_12;
        sound_cards[12] = &sc_13;
        sound_cards[13] = &sc_14;
        sound_cards[14] = &sc_15;
        sound_cards[15] = &sc_16;
        sound_cards[16] = &sc_17;
        sound_cards[17] = &sc_18;
        sound_cards[18] = &sc_19;
}