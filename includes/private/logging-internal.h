#ifndef __PCEM_LOGGING_INTERNAL_H__
#define __PCEM_LOGGING_INTERNAL_H__

#include <pcem/api.h>

#ifdef __cplusplus
extern "C" {
#endif

// Used to flush the log contents to disk.
PCEM_API void pclog_flush();
// Used to close the file handle and flush the last contents to disk. Only use it when closing the application
PCEM_API void pclog_end();

#ifdef __cplusplus
}
#endif

#endif /* __PCEM_LOGGING_INTERNAL_H__ */