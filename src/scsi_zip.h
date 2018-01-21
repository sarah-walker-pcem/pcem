#include "scsi.h"

extern scsi_device_t scsi_zip;

void zip_load(char *fn);
void zip_eject();
int zip_loaded();
