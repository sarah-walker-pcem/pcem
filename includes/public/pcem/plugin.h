#ifndef _PCEM_PLUGIN_H_
#define _PCEM_PLUGIN_H_

#define PLUGIN_INIT(name) \
    const char *plugin_name = #name; \
    void init_plugin()

#endif /* _PCEM_PLUGIN_H_ */