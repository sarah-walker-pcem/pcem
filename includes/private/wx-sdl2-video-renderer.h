#ifndef _WX_SDL2_VIDEO_RENDERER_H_
#define _WX_SDL2_VIDEO_RENDERER_H_
sdl_renderer_t* sdl2_renderer_create();
void sdl2_renderer_close(sdl_renderer_t* renderer);
int sdl2_renderer_available(struct sdl_render_driver* driver);


#endif /* _WX_SDL2_VIDEO_RENDERER_H_ */
