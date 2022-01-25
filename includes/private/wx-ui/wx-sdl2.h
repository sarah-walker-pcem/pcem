#ifndef _WX_SDL2_H_
#define _WX_SDL2_H_
#ifdef __cplusplus
extern "C" {
#endif

void leave_fullscreen();
int getfile(void* hwnd, char *f, char *fn);
int getsfile(void* hwnd, char *f, char *fn, char *dir, char *ext);
int getfilewithcaption(void* hwnd, char *f, char *fn, char *caption);
void screenshot_taken(unsigned char* rgb, int width, int height);

#ifdef __cplusplus
}
#endif

extern char openfilestring[260];

extern int pause;

extern int take_screenshot;


#endif /* _WX_SDL2_H_ */
