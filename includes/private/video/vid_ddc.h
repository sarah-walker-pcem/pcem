#ifndef _VID_DDC_H_
#define _VID_DDC_H_
void ddc_init(void);
void ddc_i2c_change(int new_clock, int new_data);
int ddc_read_clock(void);
int ddc_read_data(void);


#endif /* _VID_DDC_H_ */
