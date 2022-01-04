#ifndef _PCEM_PLUGIN_H_
#define _PCEM_PLUGIN_H_

#define PLUGIN_INIT(name) \
    const char *plugin_name = #name; \
    void init_plugin()

extern void init_plugin_engine();

#endif /* _PCEM_PLUGIN_H_ */