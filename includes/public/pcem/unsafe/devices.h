#ifndef _UNSAFE_DEVICES_H_
#define _UNSAFE_DEVICES_H_

#include <pcem/devices.h>

extern device_t *current_device;
extern char *current_device_name;
extern int model;

extern int device_get_config_int(char *name);
extern char *device_get_config_string(char *s);
extern int model_get_config_int(char *s);
extern char *model_get_config_string(char *s);
extern device_t *model_getdevice(int model);

#endif /* _UNSAFE_DEVICES_H_ */
