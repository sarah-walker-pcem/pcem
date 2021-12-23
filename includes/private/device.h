#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <pcem/devices.h>

#define CONFIG_STRING 0
#define CONFIG_INT 1
#define CONFIG_BINARY 2
#define CONFIG_SELECTION 3
#define CONFIG_MIDI 4

void device_init();
void device_add(device_t *d);
void device_close_all();
int device_available(device_t *d);
void device_speed_changed();
void device_force_redraw();
void device_add_status_info(char *s, int max_len);

extern char *current_device_name;

int device_get_config_int(char *name);
char *device_get_config_string(char *name);

enum
{
        DEVICE_NOT_WORKING = 1, /*Device does not currently work correctly and will be disabled in a release build*/
        DEVICE_AT = 2,          /*Device requires an AT-compatible system*/
        DEVICE_MCA = 0x20,      /*Device requires an MCA system*/
        DEVICE_PCI = 0x40,      /*Device requires a PCI system*/
        DEVICE_PS1 = 0x80       /*Device is only for IBM PS/1 Model 2011*/
};

int model_get_config_int(char *s);
char *model_get_config_string(char *s);

#endif /* _DEVICE_H_ */
