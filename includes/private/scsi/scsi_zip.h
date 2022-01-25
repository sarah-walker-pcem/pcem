#ifndef _SCSI_ZIP_H_
#define _SCSI_ZIP_H_
#include "scsi.h"

extern scsi_device_t scsi_zip;

void zip_load(char *fn);
void zip_eject();
int zip_loaded();


#endif /* _SCSI_ZIP_H_ */
