#include <stdint.h>
#include <string.h>
#include <stdio.h>

#if linux
#include <dlfcn.h>
#endif

#include "plugin.h"
#include "tinydir.h"
#include "paths.h"
#include "config.h"

#include <pcem/logging.h>

extern MODEL *models[ROM_MAX];
extern VIDEO_CARD *video_cards[GFX_MAX];
extern SOUND_CARD *sound_cards[SOUND_MAX];
extern HDD_CONTROLLER *hdd_controllers[HDDCONTROLLERS_MAX];
extern NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
extern LPT_DEVICE *lpt_devices[LPT_MAX];

char plugin_path[512];

#ifdef PLUGIN_ENGINE
void set_plugin_path(char *s) {
	safe_strncpy(plugin_path, s, 512);
	set_screenshots_path(s);
}

void pluginengine_load_config()
{
	char* cfg_plugin_path = config_get_string(CFG_GLOBAL, "Paths", "plugin_path", 0);

	if (cfg_plugin_path)
		set_plugin_path(cfg_plugin_path);
}

void pluginengine_save_config()
{
	config_set_string(CFG_GLOBAL, "Paths", "plugin_path", plugin_path);
}

void pluginengine_init_config()
{
	char s[512];
	append_filename(s, pcem_path, "plugins/", 512);
	set_plugin_path(s);
}
#endif

void init_plugin_engine() {
	memset(models, 0, sizeof(models));
	memset(video_cards, 0, sizeof(video_cards));
	memset(lpt_devices, 0, sizeof(lpt_devices));
	memset(sound_cards, 0, sizeof(sound_cards));
	memset(hdd_controllers, 0, sizeof(hdd_controllers));
	memset(network_cards, 0, sizeof(network_cards));

	#ifdef PLUGIN_ENGINE
	add_config_callback(pluginengine_load_config, pluginengine_save_config, pluginengine_init_config);
	#endif
}

void load_plugins() {
#ifdef PLUGIN_ENGINE
	tinydir_dir dir;
	tinydir_open(&dir, plugin_path);

	while (dir.has_next) {
		tinydir_file file;
		tinydir_readfile(&dir, &file);

		if (!strcmp(file.extension, "pplg")) {
			pclog("plugin loading: %s\n", file.name);
			void (* initialize_loaded_plugin)();
#if defined(linux)
			void* handle;
			char *plugin_name;

			handle = dlopen(file.path, RTLD_NOW);
			if (!handle) {
				error("Error: %s\n", dlerror());
			} else {
				*(void**)(&initialize_loaded_plugin) = dlsym(handle, "init_plugin");

				if (!initialize_loaded_plugin) {
					error("Error: %s\n", dlerror());
					dlclose(handle);
				} else {
					initialize_loaded_plugin();
				}
			}
#elif defined(WIN32)
			HMODULE handle = LoadLibrary(file.path);

			if(!handle) {
				error("Cannot load DLL: %s", file.path);
			} else {
				*(void**)(&initialize_loaded_plugin) = GetProcAddress(handle, "init_plugin");
				if(!initialize_loaded_plugin) {
					error("Cannot load init_plugin function from: %s", file.path);
				} else {
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
