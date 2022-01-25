#ifndef _PIC_H_
#define _PIC_H_
void pic_init();
void pic2_init();
void pic_init_elcrx();
void pic_reset();

void picint(uint16_t num);
void picintlevel(uint16_t num);
void picintc(uint16_t num);
uint8_t picinterrupt();
void picclear(int num);
void dumppic();


#endif /* _PIC_H_ */
