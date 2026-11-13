#ifndef __PRIV_PLUGIN_H__
#define __PRIV_PLUGIN_H__

#include <pcem/api.h>

#include <pcem/devices.h>
#include <pcem/defines.h>
#include <pcem/plugin.h>

typedef struct pcem_loaded_plugin_t {
        char filename[256];
        void *handle;
        int api_version;
        const pcem_plugin_info_t *info;
} pcem_loaded_plugin_t;

PCEM_API extern pcem_loaded_plugin_t loaded_plugins[PLUGIN_MAX];
PCEM_API extern int loaded_plugin_count;

PCEM_API void init_plugin_engine();
PCEM_API void load_plugins();

PCEM_API extern void (*_savenvr)();
PCEM_API extern void (*_dumppic)();
PCEM_API extern void (*_dumpregs)();
PCEM_API extern void (*_sound_speed_changed)();
#endif /* __PRIV_PLUGIN_H__ */