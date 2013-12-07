#ifdef __cplusplus
extern "C" {
#endif
        void mouse_init();
        void mouse_close();
        extern int mouse_b;
        void poll_mouse();
        void position_mouse(int x, int y);
        void get_mouse_mickeys(int *x, int *y);
#ifdef __cplusplus
}
#endif

