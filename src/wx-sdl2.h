#ifdef __cplusplus
extern "C" {
#endif

void leave_fullscreen();
int getfile(void* hwnd, char *f, char *fn);
int getsfile(void* hwnd, char *f, char *fn, char *dir, char *ext);

#ifdef __cplusplus
}
#endif

extern char openfilestring[260];

extern int pause;

