#include "ibm.h"
#include "device.h"

static void *device_priv[256];
static device_t *devices[256];

void device_init()
{
        memset(devices, 0, sizeof(devices));
}

void device_add(device_t *d)
{
        int c = 0;
        void *priv;
        
        while (devices[c] != NULL && c < 256)
                c++;
        
        if (c >= 256)
                fatal("device_add : too many devices\n");
        
        priv = d->init();
        if (priv == NULL)
                fatal("device_add : device init failed\n");
        
        devices[c] = d;
        device_priv[c] = priv;        
}

void device_close_all()
{
        int c;
        
        for (c = 0; c < 256; c++)
        {
                if (devices[c] != NULL)
                {
                        devices[c]->close(device_priv[c]);
                        devices[c] = device_priv[c] = NULL;
                }
        }
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
}
