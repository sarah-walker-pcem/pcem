#ifndef _KEYBOARD_AT_H_
#define _KEYBOARD_AT_H_
void keyboard_at_init();
void keyboard_at_init_ps2();
void keyboard_at_reset();
void keyboard_at_poll();
void keyboard_at_set_mouse(void (*mouse_write)(uint8_t val, void *p), void *p);
void keyboard_at_adddata_mouse(uint8_t val);

extern uint8_t mouse_queue[16];
extern int mouse_queue_start, mouse_queue_end;
extern int mouse_scan;


#endif /* _KEYBOARD_AT_H_ */
