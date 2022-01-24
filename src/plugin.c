#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "plugin.h"

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

void init_plugin_engine()
{
        memset(models, 0, sizeof(models));
        memset(video_cards, 0, sizeof(video_cards));
        memset(lpt_devices, 0, sizeof(lpt_devices));
        memset(sound_cards, 0, sizeof(sound_cards));
        memset(hdd_controllers, 0, sizeof(hdd_controllers));
        memset(network_cards, 0, sizeof(network_cards));
}

void load_plugins()
{
#ifdef PLUGIN_ENGINE

#endif
}