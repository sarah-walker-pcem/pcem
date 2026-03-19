#ifndef QT_DEVICECONFIG_H_
#define QT_DEVICECONFIG_H_

extern "C" {
#include <stdint.h>
#include "device.h"
void deviceconfig_open(void *hwnd, device_t *device);
}

#endif /* QT_DEVICECONFIG_H_ */
