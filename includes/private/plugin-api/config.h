#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <pcem/config.h>

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

typedef struct config_callback_t {
	void (*loadconfig)();
	void (*saveconfig)();
	void (*onloaded)();
} config_callback_t;
extern config_callback_t config_callbacks[CALLBACK_MAX];
extern int num_config_callbacks;

#endif /* _CONFIG_H_ */
