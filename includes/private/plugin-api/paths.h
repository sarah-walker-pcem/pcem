#ifndef _PATHS_H_
#define _PATHS_H_

#include <pcem/api.h>

PCEM_API extern int num_roms_paths;
PCEM_API extern char pcem_path[512];
PCEM_API extern char configs_path[512];
PCEM_API extern char nvr_path[512];
PCEM_API extern char logs_path[512];
PCEM_API extern char screenshots_path[512];
PCEM_API extern char nvr_default_path[512];

PCEM_API void get_pcem_path(char *s, int size);
PCEM_API char get_path_separator();
PCEM_API void paths_init();
PCEM_API int dir_exists(char *path);

PCEM_API int get_roms_path(int p, char *s, int size);

/* set the default paths to make them permanent */
PCEM_API void set_default_roms_paths(char *s);
PCEM_API void set_default_nvr_path(char *s);
PCEM_API void set_default_logs_path(char *s);
PCEM_API void set_default_configs_path(char *s);
PCEM_API void set_default_screenshots_path(char *s);
PCEM_API void set_default_nvr_default_path(char *s);

/* set the paths temporarily for this session */
PCEM_API void set_roms_paths(char *path);
PCEM_API void set_nvr_path(char *s);
PCEM_API void set_logs_path(char *s);
PCEM_API void set_configs_path(char *s);
PCEM_API void set_screenshots_path(char *s);

#define safe_strncpy(a, b, n)                                                                                                    \
        do {                                                                                                                     \
                strncpy((a), (b), (n)-1);                                                                                        \
                (a)[(n)-1] = 0;                                                                                                  \
        } while (0)

#endif /* _PATHS_H_ */
