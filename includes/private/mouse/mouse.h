#ifndef _MOUSE_H_
#define _MOUSE_H_

#include <pcem/devices.h>

void mouse_emu_init();
void mouse_emu_close();
void mouse_poll(int x, int y, int z, int b);

char *mouse_get_name(int mouse);
int mouse_get_type(int mouse);
int mouse_get_from_name(char *s);
void mouse_init_builtin();

extern int mouse_type;

#endif /* _MOUSE_H_ */
