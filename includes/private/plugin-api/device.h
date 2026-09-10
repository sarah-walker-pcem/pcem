#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <pcem/api.h>

#include <pcem/devices.h>

PCEM_API extern device_t *current_device;
PCEM_API extern char *current_device_name;
PCEM_API extern int model;

PCEM_API int device_get_config_int(char *name);
PCEM_API char *device_get_config_string(char *s);
PCEM_API int model_get_config_int(char *s);
PCEM_API char *model_get_config_string(char *s);
PCEM_API device_t *model_getdevice(int model);

PCEM_API void device_init();
PCEM_API void device_add(device_t *d);
PCEM_API void device_close_all();
PCEM_API int device_available(device_t *d);
PCEM_API void device_speed_changed();
PCEM_API void device_force_redraw();
PCEM_API void device_add_status_info(char *s, int max_len);

#endif /* _DEVICE_H_ */
