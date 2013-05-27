void sound_add_handler(void (*poll)(void *p), void (*get_buffer)(int16_t *buffer, int len, void *p), void *p);

extern int wasgated;
extern int sbtype;

extern int sound_card_current;

char *sound_card_getname(int card);
void sound_card_init();
