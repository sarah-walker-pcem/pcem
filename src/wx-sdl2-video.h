#include <SDL2/SDL.h>

#define RENDERER_AUTO 0
#define RENDERER_DIRECT3D 1
#define RENDERER_OPENGL 2
#define RENDERER_OPENGLES2 3
#define RENDERER_OPENGLES 4
#define RENDERER_SOFTWARE 5

typedef struct sdl_render_driver {
        int id;
        const char* sdl_id;
        const char* name;
} sdl_render_driver;

#ifdef __cplusplus
extern "C" {
#endif
int sdl_video_init();
void sdl_video_close();
int sdl_renderer_init(SDL_Window* window);
void sdl_renderer_close();
int sdl_renderer_update(SDL_Window* window);
void sdl_renderer_present(SDL_Window* window);
void sdl_scale(int scale, SDL_Rect src, SDL_Rect* dst, int w, int h);
int sdl_is_fullscreen(SDL_Window* window);

sdl_render_driver* sdl_get_render_drivers(int* num_renderers);
sdl_render_driver sdl_get_render_driver_by_id(int id, int def);
sdl_render_driver sdl_get_render_driver_by_name(const char* name, int def);
sdl_render_driver* sdl_get_render_driver_by_name_ptr(const char* name);

#ifdef __cplusplus
}
#endif

extern sdl_render_driver requested_render_driver;
extern char current_render_driver_name[50];
