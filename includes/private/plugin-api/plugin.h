#ifndef __PRIV_PLUGIN_H__
#define __PRIV_PLUGIN_H__

#include <pcem/api.h>

#include <pcem/devices.h>
#include <pcem/defines.h>

PCEM_API void init_plugin_engine();
PCEM_API void load_plugins();

PCEM_API extern void (*_savenvr)();
PCEM_API extern void (*_dumppic)();
PCEM_API extern void (*_dumpregs)();
PCEM_API extern void (*_sound_speed_changed)();
#endif /* __PRIV_PLUGIN_H__ */