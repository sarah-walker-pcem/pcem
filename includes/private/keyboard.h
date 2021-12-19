extern void (*keyboard_send)(uint8_t val);
extern void (*keyboard_poll)();
void keyboard_process();
extern int keyboard_scan;

extern uint8_t pcem_key[272];

enum
{
        SCANCODE_SET_1,
        SCANCODE_SET_2,
        SCANCODE_SET_3
};

void keyboard_set_scancode_set(int set);
void keyboard_send_scancode(int code, int is_break);
