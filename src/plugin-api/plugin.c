#include <stdint.h>
#include <string.h>
#include <stdio.h>

#if defined(linux) || defined(__linux__)
#include <dlfcn.h>
#include <dirent.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "plugin.h"
#include "paths.h"
#include "config.h"
#include "ui-utils.h"

#include <pcem/logging.h>

extern MODEL *models[ROM_MAX];
extern VIDEO_CARD *video_cards[GFX_MAX];
extern SOUND_CARD *sound_cards[SOUND_MAX];
extern HDD_CONTROLLER *hdd_controllers[HDDCONTROLLERS_MAX];
extern NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
extern LPT_DEVICE *lpt_devices[LPT_MAX];

char plugin_path[512] = {0};
char default_plugin_path[512] = {0};

#ifdef PLUGIN_ENGINE
void set_plugin_path(char *s) {
        safe_strncpy(plugin_path, s, 512);
}

void pluginengine_load_config() {
        char *cfg_plugin_path = config_get_string(CFG_GLOBAL, "Paths", "plugin_path", 0);

        if (cfg_plugin_path)
                set_plugin_path(cfg_plugin_path);
}

void pluginengine_save_config() { config_set_string(CFG_GLOBAL, "Paths", "plugin_path", plugin_path); }

void pluginengine_init_config() {
        if (strlen(plugin_path) == 0)
                set_plugin_path(default_plugin_path);

        if (!wx_dir_exists(plugin_path))
                wx_create_directory(plugin_path);
}

static int has_extension(const char *name, const char *ext) {
        const char *dot = strrchr(name, '.');
        if (!dot)
                return 0;
        return strcmp(dot + 1, ext) == 0;
}

static void load_plugin_file(const char *dir_path, const char *filename) {
        char full_path[1024];
        void (*initialize_loaded_plugin)();

        snprintf(full_path, sizeof(full_path), "%s%s", dir_path, filename);
        pclog("plugin loading: %s\n", filename);

#if defined(linux) || defined(__linux__)
        void *handle = dlopen(full_path, RTLD_NOW);
        if (!handle) {
                error("Error: %s\n", dlerror());
        } else {
                *(void **)(&initialize_loaded_plugin) = dlsym(handle, "init_plugin");
                if (!initialize_loaded_plugin) {
                        error("Error: %s\n", dlerror());
                        dlclose(handle);
                } else {
                        initialize_loaded_plugin();
                }
        }
#elif defined(_WIN32)
        HMODULE handle = LoadLibraryA(full_path);
        if (!handle) {
                error("Cannot load DLL: %s", full_path);
        } else {
                *(void **)(&initialize_loaded_plugin) = (void *)GetProcAddress(handle, "init_plugin");
                if (!initialize_loaded_plugin) {
                        error("Cannot load init_plugin function from: %s", full_path);
                } else {
                        initialize_loaded_plugin();
                }
        }
#endif
        pclog("plugin finished loading: %s\n", filename);
}
#endif

void init_plugin_engine() {
        memset(models, 0, sizeof(models));
        memset(video_cards, 0, sizeof(video_cards));
        memset(lpt_devices, 0, sizeof(lpt_devices));
        memset(sound_cards, 0, sizeof(sound_cards));
        memset(hdd_controllers, 0, sizeof(hdd_controllers));
        memset(network_cards, 0, sizeof(network_cards));

        append_filename(default_plugin_path, pcem_path, "plugins/", 512);

#ifdef PLUGIN_ENGINE
        add_config_callback(pluginengine_load_config, pluginengine_save_config, pluginengine_init_config);
#endif
}

void load_plugins() {
#ifdef PLUGIN_ENGINE
#if defined(_WIN32)
        char search_path[1024];
        WIN32_FIND_DATAA fd;
        HANDLE hFind;

        snprintf(search_path, sizeof(search_path), "%s*.pplg", plugin_path);
        hFind = FindFirstFileA(search_path, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
                do {
                        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                                load_plugin_file(plugin_path, fd.cFileName);
                        }
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
        }
#elif defined(linux) || defined(__linux__)
        DIR *d = opendir(plugin_path);
        if (d) {
                struct dirent *entry;
                while ((entry = readdir(d)) != NULL) {
                        if (has_extension(entry->d_name, "pplg")) {
                                load_plugin_file(plugin_path, entry->d_name);
                        }
                }
                closedir(d);
        }
#endif
#endif
}
