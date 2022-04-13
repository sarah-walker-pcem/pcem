#include <stdint.h>
#include <string.h>
#include <stdio.h>

#if linux
#include <dlfcn.h>
#endif

#include "plugin.h"
#include "tinydir.h"
#include "paths.h"

#include <pcem/logging.h>

extern MODEL *models[ROM_MAX];
extern VIDEO_CARD* video_cards[GFX_MAX];
extern SOUND_CARD* sound_cards[SOUND_MAX];
extern HDD_CONTROLLER* hdd_controllers[HDDCONTROLLERS_MAX];
extern NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
extern LPT_DEVICE* lpt_devices[LPT_MAX];

void init_plugin_engine()
{
        memset(models, 0, sizeof(models));
        memset(video_cards, 0, sizeof(video_cards));
        memset(lpt_devices, 0, sizeof(lpt_devices));
        memset(sound_cards, 0, sizeof(sound_cards));
        memset(hdd_controllers, 0, sizeof(hdd_controllers));
        memset(network_cards, 0, sizeof(network_cards));
}

void load_plugins()
{
#ifdef PLUGIN_ENGINE
        tinydir_dir dir;
        tinydir_open(&dir, plugins_default_path);

        while (dir.has_next)
        {
                tinydir_file file;
                tinydir_readfile(&dir, &file);

#if defined(linux)
                if (!strcmp(file.extension, "so"))
#elif defined(WIN32)
                if (!strcmp(file.extension, "dll"))
#endif
                {
                        pclog("plugin loading: %s\n", file.name);
                        void (* initialize_loaded_plugin)();
#if defined(linux)
                        void* handle;
                        char *plugin_name;

                        handle = dlopen(file.path, RTLD_NOW);
                        if (!handle)
                        {
                                error("Error: %s\n", dlerror());
                        }
                        else
                        {
                                *(void**)(&initialize_loaded_plugin) = dlsym(handle, "init_plugin");

                                if (!initialize_loaded_plugin)
                                {
                                        error("Error: %s\n", dlerror());
                                        dlclose(handle);
                                }
                                else
                                {
                                        initialize_loaded_plugin();
                                }
                        }
#elif defined(WIN32)
                        HMODULE handle = LoadLibrary(file.path);

                        if(!handle)
                        {
                                error("Cannot load DLL: %s", file.path);
                        }
                        else
                        {
                                *(void**)(&initialize_loaded_plugin) = GetProcAddress(handle, "init_plugin");
                                if(!initialize_loaded_plugin)
                                {
                                        error("Cannot load init_plugin function from: %s", file.path);
                                }
                                else
                                {
                                        initialize_loaded_plugin();
                                }
                        }
#endif
                        pclog("plugin finished loading: %s\n", file.name);
                }

                tinydir_next(&dir);
        }

        tinydir_close(&dir);
#endif
}