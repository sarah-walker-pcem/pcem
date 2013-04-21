#ifdef __cplusplus
extern "C" {
#endif
        void keyboard_init();
        void keyboard_close();
        void keyboard_poll_host();
        int key[256];
        
        #define KEY_LCONTROL 0x1d
        #define KEY_RCONTROL (0x1d | 0x80)
        #define KEY_END      (0x4f | 0x80)
#ifdef __cplusplus
}
#endif

