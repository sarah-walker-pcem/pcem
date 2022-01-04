#include "ibm.h"
#include "device.h"
#include "hdd.h"

#include "esdi_at.h"
#include "hdd_esdi.h"
#include "ide.h"
#include "mfm_at.h"
#include "mfm_xebec.h"
#include "scsi_53c400.h"
#include "scsi_aha1540.h"
#include "scsi_ibm.h"
#include "xtide.h"

#include <pcem/devices.h>
#include <pcem/defines.h>

char hdd_controller_name[16];

static device_t null_hdd_device;

static int hdd_controller_current;

HDD_CONTROLLER *hdd_controllers[HDDCONTROLLERS_MAX];

char *hdd_controller_get_name(int hdd)
{
        if(hdd_controllers[hdd] != NULL)
                return hdd_controllers[hdd]->name;
        return "";
}

char *hdd_controller_get_internal_name(int hdd)
{
        if(hdd_controllers[hdd] != NULL)
                return hdd_controllers[hdd]->internal_name;
        return "";
}

int hdd_controller_get_flags(int hdd)
{
        if(hdd_controllers[hdd] != NULL)
                return hdd_controllers[hdd]->device->flags;
        return 0;
}

int hdd_controller_available(int hdd)
{
        if(hdd_controllers[hdd] != NULL)
                return device_available(hdd_controllers[hdd]->device);
        return 0;
}

int hdd_controller_is_mfm(char *internal_name)
{
        int c = 0;

        while (hdd_controllers[c] != NULL)
        {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name))
                {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->is_mfm;
                }
                c++;
        }

        return 0;
}
int hdd_controller_is_ide(char *internal_name)
{
        int c = 0;

        while (hdd_controllers[c] != NULL)
        {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name))
                {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->is_ide;
                }
                c++;
        }

        return 0;
}
int hdd_controller_is_scsi(char *internal_name)
{
        int c = 0;

        while (hdd_controllers[c] != NULL)
        {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name))
                {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->is_scsi;
                }
                c++;
        }

        return 0;
}
int hdd_controller_has_config(char *internal_name)
{
        int c = 0;

        while (hdd_controllers[c] != NULL)
        {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name))
                {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->device->config ? 1 : 0;
                }
                c++;
        }

        return 0;
}

device_t *hdd_controller_get_device(char *internal_name)
{
        int c = 0;

        while (hdd_controllers[c] != NULL)
        {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name))
                {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->device;
                }
                c++;
        }

        return NULL;
}

int hdd_controller_current_is_mfm()
{
        return hdd_controller_is_mfm(hdd_controller_name);
}
int hdd_controller_current_is_ide()
{
        return hdd_controller_is_ide(hdd_controller_name);
}
int hdd_controller_current_is_scsi()
{
        return hdd_controller_is_scsi(hdd_controller_name);
}

void hdd_controller_init(char *internal_name)
{
        int c = 0;

        while (hdd_controllers[c] != NULL)
        {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name))
                {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                device_add(hdd_controllers[c]->device);
                        return;
                }
                c++;
        }
        /*        fatal("Could not find hdd_controller %s\n", internal_name);*/
}

static void *null_hdd_init()
{
        return NULL;
}

static void null_hdd_close(void *p)
{
}

static device_t null_hdd_device =
    {
        "Null HDD controller",
        0,
        null_hdd_init,
        null_hdd_close,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL};

int hdd_controller_count()
{
        int ret = 0;

        while (hdd_controllers[ret] != NULL && ret < HDDCONTROLLERS_MAX)
                ret++;

        return ret;
}

HDD_CONTROLLER h1 = {"None", "none", &null_hdd_device, 0, 0, 0};
HDD_CONTROLLER h2 = {"[MFM] AT Fixed Disk Adapter", "mfm_at", &mfm_at_device, 1, 0, 0};
HDD_CONTROLLER h3 = {"[MFM] DTC 5150X", "dtc5150x", &dtc_5150x_device, 1, 0, 0};
HDD_CONTROLLER h4 = {"[MFM] Fixed Disk Adapter (Xebec)", "mfm_xebec", &mfm_xebec_device, 1, 0, 0};
HDD_CONTROLLER h5 = {"[ESDI] IBM ESDI Fixed Disk Controller", "esdi_mca", &hdd_esdi_device, 1, 0, 0};
HDD_CONTROLLER h6 = {"[ESDI] Western Digital WD1007V-SE1", "wd1007vse1", &wd1007vse1_device, 0, 0, 0};
HDD_CONTROLLER h7 = {"[IDE] Standard IDE", "ide", &ide_device, 0, 1, 0};
HDD_CONTROLLER h8 = {"[IDE] XTIDE", "xtide", &xtide_device, 0, 1, 0};
HDD_CONTROLLER h9 = {"[IDE] XTIDE (AT)", "xtide_at", &xtide_at_device, 0, 1, 0};
HDD_CONTROLLER h10 = {"[IDE] XTIDE (PS/1)", "xtide_ps1", &xtide_ps1_device, 0, 1, 0};
HDD_CONTROLLER h11 = {"[SCSI] Adaptec AHA-1542C", "aha1542c", &scsi_aha1542c_device, 0, 0, 1};
HDD_CONTROLLER h12 = {"[SCSI] BusLogic BT-545S", "bt545s", &scsi_bt545s_device, 0, 0, 1};
HDD_CONTROLLER h13 = {"[SCSI] IBM SCSI Adapter with Cache", "ibmscsi_mca", &scsi_ibm_device, 0, 0, 1};
HDD_CONTROLLER h14 = {"[SCSI] Longshine LCS-6821N", "lcs6821n", &scsi_lcs6821n_device, 0, 0, 1};
HDD_CONTROLLER h15 = {"[SCSI] Rancho RT1000B", "rt1000b", &scsi_rt1000b_device, 0, 0, 1};
HDD_CONTROLLER h16 = {"[SCSI] Trantor T130B", "t130b", &scsi_t130b_device, 0, 0, 1};

void pcem_add_hddcontroller(HDD_CONTROLLER *hddcontroller)
{
        hdd_controllers[hdd_controller_count()] = hddcontroller;
}

void hdd_controller_init_builtin()
{
        memset(hdd_controllers, 0, sizeof(hdd_controllers));

        pcem_add_hddcontroller(&h1);
        pcem_add_hddcontroller(&h2);
        pcem_add_hddcontroller(&h3);
        pcem_add_hddcontroller(&h4);
        pcem_add_hddcontroller(&h5);
        pcem_add_hddcontroller(&h6);
        pcem_add_hddcontroller(&h7);
        pcem_add_hddcontroller(&h8);
        pcem_add_hddcontroller(&h9);
        pcem_add_hddcontroller(&h10);
        pcem_add_hddcontroller(&h11);
        pcem_add_hddcontroller(&h12);
        pcem_add_hddcontroller(&h13);
        pcem_add_hddcontroller(&h14);
        pcem_add_hddcontroller(&h15);
        pcem_add_hddcontroller(&h16);
}