void keyboard_at_init();
void keyboard_at_reset();
void keyboard_at_poll();

void (*mouse_write)(uint8_t val);
extern int mouse_queue_start, mouse_queue_end;
