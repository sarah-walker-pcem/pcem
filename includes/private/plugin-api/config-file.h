#ifndef _CONFIG_FILE_H_
#define _CONFIG_FILE_H_

#include <pcem/api.h>

#ifdef __cplusplus
extern "C" {
#endif

PCEM_API void *config_file_new(void);
PCEM_API void *config_file_load(const char *path);
PCEM_API int config_file_save(void *config, const char *path);
PCEM_API void config_file_free(void *config);

PCEM_API int config_file_has_entry(void *config, const char *name);
PCEM_API int config_file_get_string(void *config, const char *name, char *dst, int size, const char *def);
PCEM_API int config_file_get_int(void *config, const char *name, int *dst, int def);
PCEM_API int config_file_get_float(void *config, const char *name, float *dst, float def);
PCEM_API int config_file_get_bool(void *config, const char *name, int *dst, int def);

PCEM_API void config_file_set_string(void *config, const char *name, const char *value);
PCEM_API void config_file_set_int(void *config, const char *name, int value);
PCEM_API void config_file_set_float(void *config, const char *name, float value);
PCEM_API void config_file_set_bool(void *config, const char *name, int value);

#ifdef __cplusplus
}
#endif

#endif /* _CONFIG_FILE_H_ */
