#ifdef __cplusplus
extern "C" {
#endif
        void joystick_init();
        void joystick_close();
        void joystick_poll();
        
        typedef struct joystick_t
        {
                int x, y;
                int b[4];
        } joystick_t;
        
        extern joystick_t joystick_state[2];
        extern int joysticks_present;
#ifdef __cplusplus
}
#endif