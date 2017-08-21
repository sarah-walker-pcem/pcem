#define RENDERER_AUTO 0
#define RENDERER_DIRECT3D 1
#define RENDERER_OPENGL 2
#define RENDERER_OPENGLES2 3
#define RENDERER_OPENGLES 4
#define RENDERER_SOFTWARE 5
#define RENDERER_GL3 6

struct sdl_render_driver;

typedef struct sdl_renderer_t {
        int (*init)(SDL_Window* window, struct sdl_render_driver driver, SDL_Rect screen);
        void (*close)();
        void (*update)(SDL_Window* window, SDL_Rect updated_rect, BITMAP* screen);
        void (*present)(SDL_Window* window, SDL_Rect texture_rect, SDL_Rect window_rect, SDL_Rect screen);
        int always_update;
} sdl_renderer_t;

typedef struct sdl_render_driver {
        int id;
        const char* sdl_id;
        const char* name;
        int sdl_window_params;
        sdl_renderer_t* (*renderer_create)();
        void (*renderer_close)(sdl_renderer_t* renderer);
        int (*renderer_available)(struct sdl_render_driver* driver);
} sdl_render_driver;

typedef float (*FLASH_FUNC)(float v);

typedef struct flash_t {
        int enabled;
        int time;
        char color[4];
        int start;
        char alpha;
        FLASH_FUNC func;
} flash_t;

flash_t flash;

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

void color_flash(FLASH_FUNC func, int time_ms, char r, char g, char b, char a);

#ifdef __cplusplus
}
#endif

extern sdl_render_driver requested_render_driver;
extern char current_render_driver_name[50];

extern int custom_resolution_width;
extern int custom_resolution_height;
