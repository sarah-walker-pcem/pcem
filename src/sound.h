void sound_add_handler(void (*poll)(void *p), void (*get_buffer)(int16_t *buffer, int len, void *p), void *p);

extern int sbtype;

extern int sound_card_current;

int sound_card_available(int card);
char *sound_card_getname(int card);
struct device_t *sound_card_getdevice(int card);
int sound_card_has_config(int card);
void sound_card_init();
