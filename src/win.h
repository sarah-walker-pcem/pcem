extern HINSTANCE hinstance;
extern HWND ghwnd;
extern int mousecapture;

#ifdef __cplusplus
extern "C" {
#endif

#define szClassName "PCemMainWnd"
#define szSubClassName "PCemSubWnd"

void leave_fullscreen();

#ifdef __cplusplus
}
#endif
