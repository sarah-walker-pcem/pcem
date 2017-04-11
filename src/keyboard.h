extern void (*keyboard_send)(uint8_t val);
extern void (*keyboard_poll)();
void keyboard_process();
extern int keyboard_scan;

extern int pcem_key[272];
