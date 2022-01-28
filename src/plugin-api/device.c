#include <pcem/defines.h>
#include <pcem/devices.h>
#include <pcem/logging.h>
#include <string.h>
#include <pcem/unsafe/config.h>

extern void *device_priv[256];
extern device_t *devices[DEV_MAX];
extern device_t *current_device;
extern char *current_device_name;

extern struct device_t *model_getdevice(int model);

int model;
device_t *devices[DEV_MAX];
MODEL *models[ROM_MAX];
VIDEO_CARD* video_cards[GFX_MAX];
SOUND_CARD* sound_cards[SOUND_MAX];
HDD_CONTROLLER* hdd_controllers[HDDCONTROLLERS_MAX];
NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
LPT_DEVICE* lpt_devices[LPT_MAX];
device_t *current_device;
char *current_device_name = NULL;
void *device_priv[DEV_MAX];

void (*_sound_speed_changed)();

device_t *model_getdevice(int model)
{
        return models[model]->device;
}

void device_init()
{
        memset(devices, 0, sizeof(devices));
}

void device_add(device_t *d)
{
        pcem_add_device(d);
}

void device_close_all()
{
        int c;
        
        for (c = 0; c < 256; c++)
        {
                if (devices[c] != NULL)
                {
                        if (devices[c]->close != NULL)
                                devices[c]->close(device_priv[c]);
                        devices[c] = device_priv[c] = NULL;
                }
        }
}

int device_available(device_t *d)
{
#ifdef RELEASE_BUILD
        if (d->flags & DEVICE_NOT_WORKING)
                return 0;
#endif
        if (d->available)
                return d->available();
                
        return 1;        
}

void device_speed_changed()
{
        int c;
        
        for (c = 0; c < 256; c++)
        {
                if (devices[c] != NULL)
                {
                        if (devices[c]->speed_changed != NULL)
                        {
                                devices[c]->speed_changed(device_priv[c]);
                        }
                }
        }
        
        _sound_speed_changed();
}

void device_force_redraw()
{
        int c;
        
        for (c = 0; c < 256; c++)
        {
                if (devices[c] != NULL)
                {
                        if (devices[c]->force_redraw != NULL)
                        {
                                devices[c]->force_redraw(device_priv[c]);
                        }
                }
        }
}

void device_add_status_info(char *s, int max_len)
{
        int c;
        
        for (c = 0; c < 256; c++)
        {
                if (devices[c] != NULL)
                {
                        if (devices[c]->add_status_info != NULL)
                                devices[c]->add_status_info(s, max_len, device_priv[c]);
                }
        }
}

int device_get_config_int(char *s)
{
        device_config_t *config = current_device->config;
        
        while (config->type != -1)
        {
                if (!strcmp(s, config->name))
                        return config_get_int(CFG_MACHINE, current_device->name, s, config->default_int);

                config++;
        }
        return 0;
}

char *device_get_config_string(char *s)
{
        device_config_t *config = current_device->config;
        
        while (config->type != -1)
        {
                if (!strcmp(s, config->name))
                        return config_get_string(CFG_MACHINE, current_device->name, s, config->default_string);

                config++;
        }
        return NULL;
}

int model_get_config_int(char *s)
{
        device_t *device = model_getdevice(model);
        device_config_t *config;

        if (!device)
                return 0;                

        config = device->config;
        
        while (config->type != -1)
        {
                if (!strcmp(s, config->name))
                        return config_get_int(CFG_MACHINE, device->name, s, config->default_int);

                config++;
        }
        return 0;
}

char *model_get_config_string(char *s)
{
        device_t *device = model_getdevice(model);
        device_config_t *config;
        
        if (!device)
                return 0;                

        config = device->config;
        
        while (config->type != -1)
        {
                if (!strcmp(s, config->name))
                        return config_get_string(CFG_MACHINE, device->name, s, config->default_string);

                config++;
        }
        return NULL;
}

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
