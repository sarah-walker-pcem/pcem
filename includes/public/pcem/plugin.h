#ifndef _PCEM_PLUGIN_H_
#define _PCEM_PLUGIN_H_

#if defined(linux)
#define PLUGIN_INIT(name) \
    const char *plugin_name = #name; \
    void init_plugin()
#elif defined(WIN32)
#define PLUGIN_INIT(name) \
    const char *plugin_name = #name; \
    void __declspec(dllexport) __stdcall init_plugin()
#endif

#endif /* _PCEM_PLUGIN_H_ */