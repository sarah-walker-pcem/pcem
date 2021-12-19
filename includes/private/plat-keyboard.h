#ifdef __cplusplus
extern "C" {
#endif
        #include <stdint.h>
        
        void keyboard_init();
        void keyboard_close();
        void keyboard_poll_host();
        extern uint8_t pcem_key[272];
	extern int rawinputkey[272];
	
#ifndef __unix
        #define KEY_LCONTROL 0x1d
        #define KEY_RCONTROL (0x1d | 0x80)
        #define KEY_END      (0x4f | 0x80)
#endif

#ifdef __cplusplus
}
#endif

