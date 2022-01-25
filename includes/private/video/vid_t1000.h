#ifndef _VID_T1000_H_
#define _VID_T1000_H_
extern device_t t1000_device;

void t1000_video_options_set(uint8_t options);
void t1000_display_set(uint8_t internal);
void t1000_video_enable(uint8_t enabled);


#endif /* _VID_T1000_H_ */
