#ifndef __PRIV_PLUGIN_H__
#define __PRIV_PLUGIN_H__

#include <pcem/devices.h>
#include <pcem/defines.h>

void init_plugin_engine();
void load_plugins();

extern void (*_savenvr)();
extern void (*_dumppic)();
extern void (*_dumpregs)();
extern void (*_sound_speed_changed)();
#endif /* __PRIV_PLUGIN_H__ */