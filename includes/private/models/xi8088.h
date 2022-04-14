#ifndef _XI8088_H_
#define _XI8088_H_
#include "device.h"

extern device_t xi8088_device;

uint8_t xi8088_turbo_get();
void xi8088_turbo_set(uint8_t value);
int xi8088_bios_128kb();


#endif /* _XI8088_H_ */
