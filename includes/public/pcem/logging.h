#ifndef __PCEM_LOGGING_H__
#define __PCEM_LOGGING_H__

#include <pcem/api.h>

#define printf pclog
PCEM_API extern void pclog(const char *format, ...);
PCEM_API extern void error(const char *format, ...);
PCEM_API extern void fatal(const char *format, ...);
PCEM_API extern void warning(const char *format, ...);

#endif /* __PCEM_LOGGING_H__ */