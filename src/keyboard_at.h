void keyboard_at_init();
void keyboard_at_init_ps2();
void keyboard_at_reset();
void keyboard_at_poll();
void keyboard_at_set_mouse(void (*mouse_write)(uint8_t val, void *p), void *p);

extern int mouse_queue_start, mouse_queue_end;
extern int mouse_scan;
