#ifndef _SDL2_EMU_H_
#define _SDL2_EMU_H_

#include "sdl2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

int pc_main(int argc, char **argv);
int emulator_check_roms(void);
int emulator_shutdown(void);

int start_emulation(void *params);
int resume_emulation(void);
int pause_emulation(void);
int stop_emulation(void);
void reset_emulation(void);

void leave_fullscreen();
void toggle_fullscreen();
void screenshot_taken(unsigned char *rgb, int width, int height);
void atapi_close(void);

#ifdef __cplusplus
}
#endif

extern int pauseEmulation;
extern int take_screenshot;
extern int screenshot_flash;
extern char screenshot_format[10];
extern int video_scale;
extern int video_width;
extern int video_height;
extern int gfx_present[];
extern int romspresent[];

extern float gl3_shader_refresh_rate;
extern float gl3_input_scale;
extern int gl3_input_stretch;
extern char gl3_shader_file[20][512];

#endif /* _SDL2_EMU_H_ */
