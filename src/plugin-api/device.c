#include <pcem/defines.h>
#include <pcem/devices.h>
#include <string.h>
#include <pcem/unsafe/config.h>

extern void *device_priv[256];
extern device_t *devices[DEV_MAX];
extern device_t *current_device;
extern char *current_device_name;

extern struct device_t *model_getdevice(int model);
extern int model;
extern void sound_speed_changed();

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
        
        sound_speed_changed();
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
