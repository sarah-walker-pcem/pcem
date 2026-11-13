#ifndef _FS_UTILS_H_
#define _FS_UTILS_H_

#include <pcem/api.h>

#ifdef __cplusplus
extern "C" {
#endif

PCEM_API void pcem_get_home_directory(char *path);
PCEM_API int pcem_create_directory(char *path);
PCEM_API int pcem_dir_exists(char *path);

#ifdef __cplusplus
}
#endif

#endif /* _FS_UTILS_H_ */
