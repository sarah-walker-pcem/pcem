#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <pcem/devices.h>
#include <pcem/unsafe/devices.h>

void device_init();
void device_add(device_t *d);
void device_close_all();
int device_available(device_t *d);
void device_speed_changed();
void device_force_redraw();
void device_add_status_info(char *s, int max_len);

enum
{
        DEVICE_NOT_WORKING = 1, /*Device does not currently work correctly and will be disabled in a release build*/
        DEVICE_AT = 2,          /*Device requires an AT-compatible system*/
        DEVICE_MCA = 0x20,      /*Device requires an MCA system*/
        DEVICE_PCI = 0x40,      /*Device requires a PCI system*/
        DEVICE_PS1 = 0x80       /*Device is only for IBM PS/1 Model 2011*/
};

#endif /* _DEVICE_H_ */
