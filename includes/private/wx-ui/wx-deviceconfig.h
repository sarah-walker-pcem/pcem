#ifndef SRC_WX_DEVICECONFIG_H_
#define SRC_WX_DEVICECONFIG_H_

extern "C" {
#include <stdint.h>
#include "device.h"
void deviceconfig_open(void* hwnd, device_t *device);
}

#endif /* SRC_WX_DEVICECONFIG_H_ */
