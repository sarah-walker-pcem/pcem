#ifndef SRC_WX_DISPLAY_H_
#define SRC_WX_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

int display_init();
void display_close();
void display_start(void* hwnd);
void display_stop();
void display_resize(int width, int height);

#ifdef __cplusplus
}
#endif

extern int mousecapture;

extern int video_scale_mode;
extern int video_vsync;
extern int video_focus_dim;
extern int video_fullscreen_mode;
extern int video_alternative_update_lock;

extern int win_doresize;

extern int window_doreset;
extern int window_dosetresize;
extern int renderer_doreset;
extern int window_dofullscreen;
extern int window_dowindowed;
extern int window_doremember;
extern int window_doinputgrab;
extern int window_doinputrelease;
extern int window_dotogglefullscreen;

#endif /* SRC_WX_DISPLAY_H_ */
