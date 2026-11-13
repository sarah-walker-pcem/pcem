#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(linux) || defined(__linux__) || defined(__APPLE__)
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
#include "fs-utils.h"

#include <pcem/logging.h>

extern MODEL *models[ROM_MAX];
extern VIDEO_CARD *video_cards[GFX_MAX];
extern SOUND_CARD *sound_cards[SOUND_MAX];
extern HDD_CONTROLLER *hdd_controllers[HDDCONTROLLERS_MAX];
extern NETWORK_CARD *network_cards[NETWORK_CARD_MAX];
extern LPT_DEVICE *lpt_devices[LPT_MAX];
extern mouse_t *mouse_list[MOUSE_MAX];
extern joystick_if_t *joystick_list[JOYSTICK_MAX];

pcem_loaded_plugin_t loaded_plugins[PLUGIN_MAX];
int loaded_plugin_count = 0;

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

        if (!pcem_dir_exists(plugin_path))
                pcem_create_directory(plugin_path);
}

static int has_extension(const char *name, const char *ext) {
        const char *dot = strrchr(name, '.');
        if (!dot)
                return 0;
        return strcmp(dot + 1, ext) == 0;
}

#if defined(_WIN32)
static void *plugin_open(const char *path) { return (void *)LoadLibraryA(path); }
static void *plugin_symbol(void *handle, const char *name) { return (void *)GetProcAddress((HMODULE)handle, name); }
static void plugin_close(void *handle) { FreeLibrary((HMODULE)handle); }
static const char *plugin_last_error() { return "LoadLibrary/GetProcAddress failed"; }
#else
static void *plugin_open(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
static void *plugin_symbol(void *handle, const char *name) { return dlsym(handle, name); }
static void plugin_close(void *handle) { dlclose(handle); }
static const char *plugin_last_error() {
        const char *err = dlerror();
        return err ? err : "unknown error";
}
#endif

static void load_plugin_file(const char *dir_path, const char *filename) {
        char full_path[1024];
        void (*initialize_loaded_plugin)();
        const int *api_version;
        pcem_loaded_plugin_t *record;
        void *handle;

        size_t dir_len = strlen(dir_path);
        int needs_sep = dir_len > 0 && dir_path[dir_len - 1] != '/' && dir_path[dir_len - 1] != '\\';

        snprintf(full_path, sizeof(full_path), "%s%s%s", dir_path, needs_sep ? "/" : "", filename);
        pclog("plugin loading: %s\n", filename);

        if (loaded_plugin_count >= PLUGIN_MAX) {
                error("Cannot load plugin %s: too many plugins loaded (max %d)\n", filename, PLUGIN_MAX);
                return;
        }

        handle = plugin_open(full_path);
        if (!handle) {
                error("Cannot load plugin %s: %s\n", full_path, plugin_last_error());
                return;
        }

        api_version = (const int *)plugin_symbol(handle, "pcem_plugin_api_version");
        if (!api_version) {
                error("Cannot load plugin %s: no pcem_plugin_api_version, rebuild it against the current PCem headers\n",
                      full_path);
                plugin_close(handle);
                return;
        }
        if (*api_version != PCEM_PLUGIN_API_VERSION) {
                error("Cannot load plugin %s: built for plugin API version %d, PCem provides version %d\n", full_path,
                      *api_version, PCEM_PLUGIN_API_VERSION);
                plugin_close(handle);
                return;
        }

        *(void **)(&initialize_loaded_plugin) = plugin_symbol(handle, "init_plugin");
        if (!initialize_loaded_plugin) {
                error("Cannot load plugin %s: no init_plugin function\n", full_path);
                plugin_close(handle);
                return;
        }

        record = &loaded_plugins[loaded_plugin_count++];
        memset(record, 0, sizeof(*record));
        safe_strncpy(record->filename, filename, sizeof(record->filename));
        record->handle = handle;
        record->api_version = *api_version;
        record->info = (const pcem_plugin_info_t *)plugin_symbol(handle, "pcem_plugin_info");

        initialize_loaded_plugin();
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
        memset(mouse_list, 0, sizeof(mouse_list));
        memset(joystick_list, 0, sizeof(joystick_list));
        memset(loaded_plugins, 0, sizeof(loaded_plugins));
        loaded_plugin_count = 0;

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

        size_t dir_len = strlen(plugin_path);
        int needs_sep = dir_len > 0 && plugin_path[dir_len - 1] != '/' && plugin_path[dir_len - 1] != '\\';

        snprintf(search_path, sizeof(search_path), "%s%s*.pplg", plugin_path, needs_sep ? "\\" : "");
        hFind = FindFirstFileA(search_path, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
                do {
                        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                                load_plugin_file(plugin_path, fd.cFileName);
                        }
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
        }
#elif defined(linux) || defined(__linux__) || defined(__APPLE__)
        struct dirent **entries;
        int n = scandir(plugin_path, &entries, NULL, alphasort);
        if (n >= 0) {
                int i;
                for (i = 0; i < n; i++) {
                        if (has_extension(entries[i]->d_name, "pplg"))
                                load_plugin_file(plugin_path, entries[i]->d_name);
                        free(entries[i]);
                }
                free(entries);
        }
#endif
#endif
}
