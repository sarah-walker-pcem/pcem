#ifndef _VID_ICD2061_H_
#define _VID_ICD2061_H_
typedef struct icd2061_t
{
        int state;
        int status;
        int pos;
        int unlock;
        uint32_t data;

        double freq[4];
        uint32_t ctrl;
} icd2061_t;

void icd2061_write(icd2061_t *icd2061, int val);
double icd2061_getfreq(icd2061_t *icd2061, int i);


#endif /* _VID_ICD2061_H_ */
