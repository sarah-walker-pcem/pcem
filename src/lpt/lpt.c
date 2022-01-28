#include "ibm.h"
#include "io.h"

#include "lpt.h"
#include "lpt_dac.h"
#include "lpt_dss.h"

#include <pcem/devices.h>
#include <pcem/defines.h>

#ifdef USE_EXPERIMENTAL_PRINTER
#include "lpt_epsonlx810.h"
#endif

char lpt1_device_name[16];

int lpt1_current = 0;

extern LPT_DEVICE* lpt_devices[LPT_MAX];

LPT_DEVICE l_none = { "None", "none", NULL };
LPT_DEVICE l_dss = { "Disney Sound Source", "dss", &dss_device };
LPT_DEVICE l_lpt_dac = { "LPT DAC / Covox Speech Thing", "lpt_dac", &lpt_dac_device };
LPT_DEVICE l_lpt_dac_stereo = { "Stereo LPT DAC", "lpt_dac_stereo", &lpt_dac_stereo_device };

char* lpt_device_get_name(int id)
{
        if (lpt_devices[id] == NULL || strlen(lpt_devices[id]->name) == 0)
                return NULL;
        return lpt_devices[id]->name;
}
char* lpt_device_get_internal_name(int id)
{
        if (lpt_devices[id] == NULL || strlen(lpt_devices[id]->internal_name) == 0)
                return NULL;
        return lpt_devices[id]->internal_name;
}

static lpt_device_t* lpt1_device;
static void* lpt1_device_p;

void lpt1_device_init()
{
        int c = 0;

        while (lpt_devices[c] != NULL && strcmp(lpt_devices[c]->internal_name, lpt1_device_name) && strlen(lpt_devices[c]->internal_name) != 0)
                c++;

        if (lpt_devices[c] == NULL || strlen(lpt_devices[c]->internal_name) == 0)
                lpt1_device = NULL;
        else if (lpt_devices[c] != NULL)
        {
                lpt1_device = lpt_devices[c]->device;
                if (lpt1_device)
                        lpt1_device_p = lpt1_device->init();
        }
}

void lpt1_device_close()
{
        if (lpt1_device)
                lpt1_device->close(lpt1_device_p);
        lpt1_device = NULL;
}

static uint8_t lpt1_dat, lpt2_dat;
static uint8_t lpt1_ctrl, lpt2_ctrl;

void lpt1_write(uint16_t port, uint8_t val, void* priv)
{
        switch (port & 3)
        {
        case 0:
                if (lpt1_device)
                        lpt1_device->write_data(val, lpt1_device_p);
                lpt1_dat = val;
                break;
        case 2:
                if (lpt1_device)
                        lpt1_device->write_ctrl(val, lpt1_device_p);
                lpt1_ctrl = val;
                break;
        }
}

int lpt_device_has_config(int devId)
{
        if (lpt_devices[devId] == NULL || !lpt_devices[devId]->device)
                return 0;
        return lpt_devices[devId]->device->config ? 1 : 0;
}


lpt_device_t *lpt_get_device(int devId)
{
        if (lpt_devices[devId] == NULL || !lpt_devices[devId]->device)
                return 0;

        return lpt_devices[devId]->device;
}

int lpt_device_get_from_internal_name(char *s)
{
        	int c = 0;

                	while (lpt_devices[c] != NULL && strlen(lpt_devices[c]->internal_name))
                	{
                        		if (!strcmp(lpt_devices[c]->internal_name, s))
                        			return c;
                        		c++;
                        	}

                	return 0;
}

uint8_t lpt1_read(uint16_t port, void* priv)
{
        switch (port & 3)
        {
        case 0:
                return lpt1_dat;
        case 1:
                if (lpt1_device)
                        return lpt1_device->read_status(lpt1_device_p);
                return 0;
        case 2:
                return lpt1_ctrl;
        }
        return 0xff;
}

void lpt2_write(uint16_t port, uint8_t val, void* priv)
{
        switch (port & 3)
        {
        case 0:
                lpt2_dat = val;
                break;
        case 2:
                lpt2_ctrl = val;
                break;
        }
}
uint8_t lpt2_read(uint16_t port, void* priv)
{
        switch (port & 3)
        {
        case 0:
                return lpt2_dat;
        case 1:
                return 0;
        case 2:
                return lpt2_ctrl;
        }
        return 0xff;
}

void lpt_init()
{
        io_sethandler(0x0378, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL, NULL);
        io_sethandler(0x0278, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL, NULL);
}

void lpt1_init(uint16_t port)
{
        if (port)
                io_sethandler(port, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL, NULL);
}
void lpt1_remove()
{
        io_removehandler(0x0278, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL, NULL);
        io_removehandler(0x0378, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL, NULL);
        io_removehandler(0x03bc, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL, NULL);
}
void lpt2_init(uint16_t port)
{
        if (port)
                io_sethandler(port, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL, NULL);
}
void lpt2_remove()
{
        io_removehandler(0x0278, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL, NULL);
        io_removehandler(0x0378, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL, NULL);
        io_removehandler(0x03bc, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL, NULL);
}

void lpt2_remove_ams()
{
        io_removehandler(0x0379, 0x0002, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL, NULL);
}

void lpt_init_builtin()
{
        pcem_add_lpt(&l_none);
        pcem_add_lpt(&l_dss);
        pcem_add_lpt(&l_lpt_dac);
        pcem_add_lpt(&l_lpt_dac_stereo);
}