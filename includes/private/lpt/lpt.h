#ifndef _LPT_H_
#define _LPT_H_
extern void lpt_init();
extern void lpt1_init(uint16_t port);
extern void lpt1_remove();
extern void lpt2_init(uint16_t port);
extern void lpt2_remove();
extern void lpt2_remove_ams();
extern int lpt1_current;

void lpt1_write(uint16_t port, uint8_t val, void *priv);
uint8_t lpt1_read(uint16_t port, void *priv);

void lpt1_device_init();
void lpt1_device_close();

char *lpt_device_get_name(int id);
char *lpt_device_get_internal_name(int id);
int lpt_device_has_config(int devId);
lpt_device_t *lpt_get_device(int devId);
int lpt_device_get_from_internal_name(char *s);

extern char lpt1_device_name[16];

void lpt_init_builtin();

#endif /* _LPT_H_ */
