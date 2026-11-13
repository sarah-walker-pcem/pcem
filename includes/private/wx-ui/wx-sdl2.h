#ifndef _WX_SDL2_H_
#define _WX_SDL2_H_
#include "sdl2-emu.h"

#ifdef __cplusplus
extern "C" {
#endif

int getfile(void *hwnd, char *f, char *fn);
int getsfile(void *hwnd, char *f, char *fn, char *dir, char *ext);
int getfilewithcaption(void *hwnd, char *f, char *fn, char *caption);

#ifdef __cplusplus
}
#endif

extern char openfilestring[260];

#endif /* _WX_SDL2_H_ */
