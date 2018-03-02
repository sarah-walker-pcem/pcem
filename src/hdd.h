#include "device.h"

char *hdd_controller_get_name(int hdd);
char *hdd_controller_get_internal_name(int hdd);
int hdd_controller_get_flags(int hdd);
int hdd_controller_available(int hdd);
int hdd_controller_is_mfm(char* internal_name);
int hdd_controller_is_ide(char *internal_name);
int hdd_controller_is_scsi(char *internal_name);
int hdd_controller_has_config(char *internal_name);
device_t *hdd_controller_get_device(char *internal_name);
int hdd_controller_current_is_mfm();
int hdd_controller_current_is_ide();
int hdd_controller_current_is_scsi();
void hdd_controller_init(char *internal_name);

extern char hdd_controller_name[16];
