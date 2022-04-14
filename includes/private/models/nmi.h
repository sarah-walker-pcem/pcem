#ifndef _NMI_H_
#define _NMI_H_
void nmi_init();
void nmi_write(uint16_t port, uint8_t val, void *p);
extern int nmi_mask;




#endif /* _NMI_H_ */
