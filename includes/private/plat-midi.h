#ifndef _PLAT_MIDI_H_
#define _PLAT_MIDI_H_
void midi_init();
void midi_close();
void midi_write(uint8_t val);
int midi_get_num_devs();
void midi_get_dev_name(int num, char *s);

#endif /* _PLAT_MIDI_H_ */
