#ifndef _PCEM_CONFIG_H_
#define _PCEM_CONFIG_H_

#include <pcem/defines.h>

extern float config_get_float(int is_global, char *head, char *name, float def);
extern int config_get_int(int is_global, char *head, char *name, int def);
extern char *config_get_string(int is_global, char *head, char *name, char *def);
extern void config_set_float(int is_global, char *head, char *name, float val);
extern void config_set_int(int is_global, char *head, char *name, int val);
extern void config_set_string(int is_global, char *head, char *name, char *val);

extern int config_free_section(int is_global, char *head);

extern void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)());

extern char *get_filename(char *s);
extern void append_filename(char *dest, char *s1, char *s2, int size);
extern void append_slash(char *s, int size);
extern void put_backslash(char *s);
extern char *get_extension(char *s);

extern void config_load(int is_global, char *fn);
extern void config_save(int is_global, char *fn);
extern void config_dump(int is_global);
extern void config_free(int is_global);

extern char config_file_default[256];
extern char config_name[256];

typedef struct config_callback_t
{
        void (* loadconfig)();
        void (* saveconfig)();
        void (* onloaded)();
} config_callback_t;
extern config_callback_t config_callbacks[CALLBACK_MAX];
extern int num_config_callbacks;

#define CFG_MACHINE 0
#define CFG_GLOBAL  1

#endif /* _PCEM_CONFIG_H_ */