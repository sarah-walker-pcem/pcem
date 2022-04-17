#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <pcem/devices.h>

extern device_t *current_device;
extern char *current_device_name;
extern int model;

int device_get_config_int(char *name);
char *device_get_config_string(char *s);
int model_get_config_int(char *s);
char *model_get_config_string(char *s);
device_t *model_getdevice(int model);

void device_init();
void device_add(device_t *d);
void device_close_all();
int device_available(device_t *d);
void device_speed_changed();
void device_force_redraw();
void device_add_status_info(char *s, int max_len);

#endif /* _DEVICE_H_ */
