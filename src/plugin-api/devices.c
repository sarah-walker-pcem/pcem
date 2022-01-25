#include <stdint.h>
#include <stddef.h>

#include <pcem/logging.h>
#include "plugin.h"

device_t *current_device;
char *current_device_name = NULL;
void *device_priv[DEV_MAX];

device_t *devices[DEV_MAX];
MODEL *models[ROM_MAX];
VIDEO_CARD* video_cards[GFX_MAX];
SOUND_CARD* sound_cards[SOUND_MAX];
HDD_CONTROLLER* hdd_controllers[HDDCONTROLLERS_MAX];
NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
LPT_DEVICE* lpt_devices[LPT_MAX];

int model_count()
{
        int ret = 0;

        while (models[ret] != NULL && ret < ROM_MAX)
                ret++;

        return ret;
}

void pcem_add_model(MODEL *model)
{
        //TODO: Add sanity check to not go past MAX amount
        models[model_count()] = model;
}

int video_count()
{
        int ret = 0;

        while (video_cards[ret] != NULL && ret < GFX_MAX)
                ret++;

        return ret;
}

void pcem_add_video(VIDEO_CARD* video)
{
        //TODO: Add sanity check to not go past MAX amount
        video_cards[video_count()] = video;
}

int lpt_count()
{
        int ret = 0;

        while (lpt_devices[ret] != NULL && ret < LPT_MAX)
                ret++;

        return ret;
}

void pcem_add_lpt(LPT_DEVICE* lpt)
{
        //TODO: Add sanity check to not go past MAX amount
        lpt_devices[lpt_count()] = lpt;
}

int sound_count()
{
        int ret = 0;

        while (sound_cards[ret] != NULL && ret < SOUND_MAX)
                ret++;

        return ret;
}

void pcem_add_sound(SOUND_CARD* sound)
{
        //TODO: Add sanity check to not go past MAX amount
        sound_cards[sound_count()] = sound;
}

int hdd_controller_count()
{
        int ret = 0;

        while (hdd_controllers[ret] != NULL && ret < HDDCONTROLLERS_MAX)
                ret++;

        return ret;
}

void pcem_add_hddcontroller(HDD_CONTROLLER* hddcontroller)
{
        //TODO: Add sanity check to not go past MAX amount
        hdd_controllers[hdd_controller_count()] = hddcontroller;
}

int network_card_count()
{
        int ret = 0;

        while (network_cards[ret] != NULL && ret < NETWORK_CARD_MAX)
                ret++;

        return ret;
}

void pcem_add_networkcard(NETWORK_CARD *netcard)
{
        //TODO: Add sanity check to not go past MAX amount
        network_cards[network_card_count()] = netcard;
}

void pcem_add_device(device_t *d)
{
        int c = 0;
        void *priv = NULL;

        while (devices[c] != NULL && c < 256)
                c++;

        if (c >= 256)
                fatal("device_add : too many devices\n");

        current_device = d;
        current_device_name = d->name;

        if (d->init != NULL)
        {
                priv = d->init();
                if (priv == NULL)
                        fatal("device_add : device init failed\n");
        }

        devices[c] = d;
        device_priv[c] = priv;
        current_device_name = NULL;
}
