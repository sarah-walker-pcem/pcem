#ifndef _GAMEPORT_H_
#define _GAMEPORT_H_

#include <pcem/devices.h>

extern device_t gameport_device;
extern device_t gameport_201_device;

extern int joystick_type;
char *joystick_get_name(int joystick);
int joystick_get_from_name(char *s);
int joystick_get_max_joysticks(int joystick);
int joystick_get_axis_count(int joystick);
int joystick_get_button_count(int joystick);
int joystick_get_pov_count(int joystick);
char *joystick_get_axis_name(int joystick, int id);
char *joystick_get_button_name(int joystick, int id);
char *joystick_get_pov_name(int joystick, int id);

void gameport_update_joystick_type();
void joystick_init_builtin();

#endif /* _GAMEPORT_H_ */
