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

#endif /* _DEVICE_H_ */
