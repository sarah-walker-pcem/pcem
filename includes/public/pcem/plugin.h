#ifndef _PCEM_PLUGIN_H_
#define _PCEM_PLUGIN_H_

#define PCEM_PLUGIN_API_VERSION 1

typedef struct pcem_plugin_info_t {
        char name[64];
        char version[32];
        char author[64];
        char description[256];
} pcem_plugin_info_t;

#if defined(WIN32) || defined(_WIN32)
#define PCEM_PLUGIN_VISIBLE __declspec(dllexport)
#define PCEM_PLUGIN_CALL __stdcall
#elif defined(linux) || defined(__linux__) || defined(__APPLE__)
#define PCEM_PLUGIN_VISIBLE __attribute__((visibility("default")))
#define PCEM_PLUGIN_CALL
#endif

#ifdef __cplusplus
#define PCEM_PLUGIN_EXPORT extern "C" PCEM_PLUGIN_VISIBLE
#else
#define PCEM_PLUGIN_EXPORT PCEM_PLUGIN_VISIBLE
#endif

#define PLUGIN_INIT(name)                                                                                                        \
        const char *plugin_name = #name;                                                                                         \
        PCEM_PLUGIN_EXPORT const int pcem_plugin_api_version = PCEM_PLUGIN_API_VERSION;                                          \
        PCEM_PLUGIN_EXPORT void PCEM_PLUGIN_CALL init_plugin()

#define PLUGIN_INFO(name, version, author, description)                                                                          \
        PCEM_PLUGIN_EXPORT const pcem_plugin_info_t pcem_plugin_info = {name, version, author, description}

#endif /* _PCEM_PLUGIN_H_ */
