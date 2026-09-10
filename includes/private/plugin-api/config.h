#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <pcem/api.h>

#include <pcem/config.h>

PCEM_API extern char *get_filename(char *s);
PCEM_API extern void append_filename(char *dest, char *s1, char *s2, int size);
PCEM_API extern void append_slash(char *s, int size);
PCEM_API extern void put_backslash(char *s);
PCEM_API extern char *get_extension(char *s);

PCEM_API extern void config_load(int is_global, char *fn);
PCEM_API extern void config_save(int is_global, char *fn);
PCEM_API extern void config_dump(int is_global);
PCEM_API extern void config_free(int is_global);

PCEM_API extern char config_file_default[256];
PCEM_API extern char config_name[256];

typedef struct config_callback_t {
        void (*loadconfig)();
        void (*saveconfig)();
        void (*onloaded)();
} config_callback_t;
PCEM_API extern config_callback_t config_callbacks[CALLBACK_MAX];
PCEM_API extern int num_config_callbacks;

#endif /* _CONFIG_H_ */
