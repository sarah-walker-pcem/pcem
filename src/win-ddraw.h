#ifdef __cplusplus
extern "C" {
#endif
        void ddraw_init(HWND h);
        void ddraw_close();
        
        void ddraw_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h);
        void ddraw_blit_memtoscreen_8(int x, int y, int w, int h);
#ifdef __cplusplus
}
#endif

