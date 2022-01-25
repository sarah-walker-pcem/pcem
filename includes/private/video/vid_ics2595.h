#ifndef _VID_ICS2595_H_
#define _VID_ICS2595_H_
typedef struct ics2595_t
{
        int oldfs3, oldfs2;
        int dat;
        int pos;
        int state;

        double clocks[16];
        double output_clock;
} ics2595_t;

void ics2595_write(ics2595_t *ics2595, int strobe, int dat);


#endif /* _VID_ICS2595_H_ */
