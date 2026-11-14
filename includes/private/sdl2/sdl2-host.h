#ifndef _SDL2_HOST_H_
#define _SDL2_HOST_H_

#include "sdl2-common.h"

#define SDL_DISPLAY_CLOSE 1
#define SDL_DISPLAY_ACTIVATE 2
#define SDL_DISPLAY_DEACTIVATE 3
#define SDL_DISPLAY_SIZE 4
#define SDL_DISPLAY_MOUSE_UP 5
#define SDL_DISPLAY_DESTROY 6
#define SDL_DISPLAY_KEY 7

typedef int (*SDL_HOST_CALLBACK)(void *data);
typedef void (*SDL_DISPLAY_CALLBACK)(int event, int param1, int param2);

typedef struct sdl_host_t {
        int defer_display_init;

        void (*message)(const char *title, const char *message);
        void (*ui_call)(SDL_HOST_CALLBACK callback, void *data);
        void (*start_render_loop)(void (*loop)(void));

        void (*request_stop)(void);
        void (*request_stop_now)(void);

        void (*config_loaded)(void);
        void (*emulation_started)(void);
        void (*emulation_paused)(void);
        void (*emulation_resumed)(void);
        void (*emulation_stopped)(void);

        int (*image_save)(const char *path, const char *name, const char *format, unsigned char *rgb, int width, int height);
        unsigned char *(*image_load)(const char *path, int *width, int *height);

        void *(*panel_create)(void *parent, const char *title, SDL_DISPLAY_CALLBACK callback);
        void *(*panel_native)(void *panel);
        void (*panel_canvas_size)(void *panel, int *width, int *height);
        void (*panel_set_size)(void *panel, int width, int height, int resizable);
        void (*panel_get_rect)(void *panel, int *x, int *y, int *width, int *height);
        void (*panel_set_position)(void *panel, int x, int y);
        void (*panel_center)(void *panel);
        void (*panel_show)(void *panel, int show);
        void (*panel_set_fullscreen)(void *panel, int fullscreen);
        void (*panel_set_title)(void *panel, const char *title);
        void (*panel_set_status)(void *panel, const char *message, int speed, int fps);
} sdl_host_t;

#ifdef __cplusplus
extern "C" {
#endif

void sdl_host_set(const sdl_host_t *host);
const sdl_host_t *sdl_host_get(void);

void sdl_host_message(const char *title, const char *format, ...);
void sdl_host_ui_call(SDL_HOST_CALLBACK callback, void *data);
int sdl_host_image_save(const char *path, const char *name, const char *format, unsigned char *rgb, int width, int height);
unsigned char *sdl_host_image_load(const char *path, int *width, int *height);
void sdl_host_date_format(char *s, int size, const char *format);

void sdl_display_event(int event, int param1, int param2);
void display_run(void);
void display_end(void);

#ifdef __cplusplus
}
#endif

#endif /* _SDL2_HOST_H_ */
