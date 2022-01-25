#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "plugin.h"


extern MODEL *models[ROM_MAX];
extern VIDEO_CARD* video_cards[GFX_MAX];
extern SOUND_CARD* sound_cards[SOUND_MAX];
extern HDD_CONTROLLER* hdd_controllers[HDDCONTROLLERS_MAX];
extern NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
extern LPT_DEVICE* lpt_devices[LPT_MAX];

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