#include "hdd.h"
#include "device.h"
#include "ibm.h"

#include "esdi_at.h"
#include "hdd_esdi.h"
#include "ide.h"
#include "mfm_at.h"
#include "mfm_xebec.h"
#include "scsi_53c400.h"
#include "scsi_aha1540.h"
#include "scsi_ibm.h"
#include "xtide.h"

#include <pcem/defines.h>
#include <pcem/devices.h>

extern HDD_CONTROLLER *hdd_controllers[HDDCONTROLLERS_MAX];
char hdd_controller_name[16];

static device_t null_hdd_device;

static int hdd_controller_current;

char *hdd_controller_get_name(int hdd) {
        if (hdd_controllers[hdd] != NULL)
                return hdd_controllers[hdd]->name;
        return "";
}

char *hdd_controller_get_internal_name(int hdd) {
        if (hdd_controllers[hdd] != NULL)
                return hdd_controllers[hdd]->internal_name;
        return "";
}

int hdd_controller_get_flags(int hdd) {
        if (hdd_controllers[hdd] != NULL)
                return hdd_controllers[hdd]->device->flags;
        return 0;
}

int hdd_controller_available(int hdd) {
        if (hdd_controllers[hdd] != NULL)
                return device_available(hdd_controllers[hdd]->device);
        return 0;
}

int hdd_controller_is_mfm(char *internal_name) {
        int c = 0;

        while (hdd_controllers[c] != NULL) {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name)) {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->is_mfm;
                }
                c++;
        }

        return 0;
}
int hdd_controller_is_ide(char *internal_name) {
        int c = 0;

        while (hdd_controllers[c] != NULL) {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name)) {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->is_ide;
                }
                c++;
        }

        return 0;
}
int hdd_controller_is_scsi(char *internal_name) {
        int c = 0;

        while (hdd_controllers[c] != NULL) {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name)) {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->is_scsi;
                }
                c++;
        }

        return 0;
}
int hdd_controller_has_config(char *internal_name) {
        int c = 0;

        while (hdd_controllers[c] != NULL) {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name)) {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->device->config ? 1 : 0;
                }
                c++;
        }

        return 0;
}

device_t *hdd_controller_get_device(char *internal_name) {
        int c = 0;

        while (hdd_controllers[c] != NULL) {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name)) {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                return hdd_controllers[c]->device;
                }
                c++;
        }

        return NULL;
}

int hdd_controller_current_is_mfm() {
        return hdd_controller_is_mfm(hdd_controller_name);
}
int hdd_controller_current_is_ide() {
        return hdd_controller_is_ide(hdd_controller_name);
}
int hdd_controller_current_is_scsi() {
        return hdd_controller_is_scsi(hdd_controller_name);
}

void hdd_controller_init(char *internal_name) {
        int c = 0;

        while (hdd_controllers[c] != NULL) {
                if (!strcmp(internal_name, hdd_controllers[c]->internal_name)) {
                        hdd_controller_current = c;
                        if (strcmp(internal_name, "none"))
                                device_add(hdd_controllers[c]->device);
                        return;
                }
                c++;
        }
        /*        fatal("Could not find hdd_controller %s\n", internal_name);*/
}

static void *null_hdd_init() {
        return NULL;
}

static void null_hdd_close(void *p) {
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

HDD_CONTROLLER h_none = {"None", "none", &null_hdd_device, 0, 0, 0};
HDD_CONTROLLER h_mfm_at = {"[MFM] AT Fixed Disk Adapter", "mfm_at", &mfm_at_device, 1, 0, 0};
HDD_CONTROLLER h_dtc5150x = {"[MFM] DTC 5150X", "dtc5150x", &dtc_5150x_device, 1, 0, 0};
HDD_CONTROLLER h_mfm_xebec = {"[MFM] Fixed Disk Adapter (Xebec)", "mfm_xebec", &mfm_xebec_device, 1, 0, 0};
HDD_CONTROLLER h_esdi_mca = {"[ESDI] IBM ESDI Fixed Disk Controller", "esdi_mca", &hdd_esdi_device, 1, 0, 0};
HDD_CONTROLLER h_wd1007vse1 = {"[ESDI] Western Digital WD1007V-SE1", "wd1007vse1", &wd1007vse1_device, 0, 0, 0};
HDD_CONTROLLER h_ide = {"[IDE] Standard IDE", "ide", &ide_device, 0, 1, 0};
HDD_CONTROLLER h_xtide = {"[IDE] XTIDE", "xtide", &xtide_device, 0, 1, 0};
HDD_CONTROLLER h_xtide_at = {"[IDE] XTIDE (AT)", "xtide_at", &xtide_at_device, 0, 1, 0};
HDD_CONTROLLER h_xtide_ps1 = {"[IDE] XTIDE (PS/1)", "xtide_ps1", &xtide_ps1_device, 0, 1, 0};
HDD_CONTROLLER h_aha1542c = {"[SCSI] Adaptec AHA-1542C", "aha1542c", &scsi_aha1542c_device, 0, 0, 1};
HDD_CONTROLLER h_bt545s = {"[SCSI] BusLogic BT-545S", "bt545s", &scsi_bt545s_device, 0, 0, 1};
HDD_CONTROLLER h_ibmscsi_mca = {"[SCSI] IBM SCSI Adapter with Cache", "ibmscsi_mca", &scsi_ibm_device, 0, 0, 1};
HDD_CONTROLLER h_lcs6821n = {"[SCSI] Longshine LCS-6821N", "lcs6821n", &scsi_lcs6821n_device, 0, 0, 1};
HDD_CONTROLLER h_rt1000b = {"[SCSI] Rancho RT1000B", "rt1000b", &scsi_rt1000b_device, 0, 0, 1};
HDD_CONTROLLER h_t130b = {"[SCSI] Trantor T130B", "t130b", &scsi_t130b_device, 0, 0, 1};

void hdd_controller_init_builtin() {
        pcem_add_hddcontroller(&h_none);
        pcem_add_hddcontroller(&h_mfm_at);
        pcem_add_hddcontroller(&h_dtc5150x);
        pcem_add_hddcontroller(&h_mfm_xebec);
        pcem_add_hddcontroller(&h_esdi_mca);
        pcem_add_hddcontroller(&h_wd1007vse1);
        pcem_add_hddcontroller(&h_ide);
        pcem_add_hddcontroller(&h_xtide);
        pcem_add_hddcontroller(&h_xtide_at);
        pcem_add_hddcontroller(&h_xtide_ps1);
        pcem_add_hddcontroller(&h_aha1542c);
        pcem_add_hddcontroller(&h_bt545s);
        pcem_add_hddcontroller(&h_ibmscsi_mca);
        pcem_add_hddcontroller(&h_lcs6821n);
        pcem_add_hddcontroller(&h_rt1000b);
        pcem_add_hddcontroller(&h_t130b);
}