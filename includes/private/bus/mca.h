#ifndef _MCA_H_
#define _MCA_H_
void mca_init(int nr_cards);
void mca_add(uint8_t (*read)(int addr, void *priv), void (*write)(int addr, uint8_t val, void *priv), void (*reset)(void *priv), void *priv);
void mca_set_index(int index);
uint8_t mca_read(uint16_t port);
void mca_write(uint16_t port, uint8_t val);
void mca_reset(void);


#endif /* _MCA_H_ */
