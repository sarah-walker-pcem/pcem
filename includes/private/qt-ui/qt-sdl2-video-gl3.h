#ifndef _QT_SDL2_VIDEO_GL3_H_
#define _QT_SDL2_VIDEO_GL3_H_
sdl_renderer_t *gl3_renderer_create();
void gl3_renderer_close(sdl_renderer_t *renderer);
int gl3_renderer_available(struct sdl_render_driver *driver);

#endif /* _QT_SDL2_VIDEO_GL3_H_ */
