#include "mouse.h"
#include "device.h"

void amstrad_init();

extern mouse_t mouse_amstrad;
extern int amstrad_latch;

extern device_t ams1512_device;
extern device_t ams2086_device;
extern device_t ams3086_device;

enum
{
        AMSTRAD_NOLATCH,
        AMSTRAD_SW9,
        AMSTRAD_SW10
};
