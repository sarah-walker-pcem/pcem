#ifndef _F82C710_UPC_H_
#define _F82C710_UPC_H_
#include "device.h"

void upc_set_mouse(void (*mouse_write)(uint8_t val, void *p), void *p);

extern device_t f82c710_upc_device;



#endif /* _F82C710_UPC_H_ */
