#ifndef _PCEM_CONFIG_H_
#define _PCEM_CONFIG_H_

#include <pcem/api.h>

#include <pcem/defines.h>

#define CFG_MACHINE 0
#define CFG_GLOBAL 1

PCEM_API extern float config_get_float(int is_global, char *head, char *name, float def);
PCEM_API extern int config_get_int(int is_global, char *head, char *name, int def);
PCEM_API extern char *config_get_string(int is_global, char *head, char *name, char *def);
PCEM_API extern void config_set_float(int is_global, char *head, char *name, float val);
PCEM_API extern void config_set_int(int is_global, char *head, char *name, int val);
PCEM_API extern void config_set_string(int is_global, char *head, char *name, char *val);
PCEM_API extern int config_free_section(int is_global, char *head);
PCEM_API extern void add_config_callback(void (*loadconfig)(), void (*saveconfig)(), void (*onloaded)());

#endif /* _PCEM_CONFIG_H_ */
