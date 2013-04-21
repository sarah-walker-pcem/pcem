/*PCem v0.7 by Tom Walker

  ICD2061 clock generator emulation
  Used by ET4000w32/p (Diamond Stealth 32)*/
#include "ibm.h"
#include "vid_icd2061.h"

static int icd2061_state;
static int icd2061_status = 0;
static int icd2061_pos = 0;
static int icd2061_unlock = 0;
static uint32_t icd2061_data;

static double icd2061_freq[4];
static uint32_t icd2061_ctrl;

void icd2061_write(int val)
{
        int q, p, m, i, a;
        if ((val & 1) && !(icd2061_state & 1))
        {
                pclog("ICD2061 write %02X %i %08X %i\n", val, icd2061_unlock, icd2061_data, icd2061_pos);
                if (!icd2061_status)
                {
                        if (val & 2) icd2061_unlock++;
                        else         
                        {
                                if (icd2061_unlock >= 5)
                                {
                                        icd2061_status = 1;
                                        icd2061_pos = 0;
                                }
                                else
                                   icd2061_unlock = 0;
                        }
                }
                else if (val & 1)
                {
                        icd2061_data = (icd2061_data >> 1) | (((val & 2) ? 1 : 0) << 24);
                        icd2061_pos++;
                        if (icd2061_pos == 26)
                        {
                                pclog("ICD2061 data - %08X\n", icd2061_data);
                                a = (icd2061_data >> 21) & 0x7;                                
                                if (!(a & 4))
                                {
                                        q = (icd2061_data & 0x7f) - 2;
                                        m = 1 << ((icd2061_data >> 7) & 0x7);
                                        p = ((icd2061_data >> 10) & 0x7f) - 3;
                                        i = (icd2061_data >> 17) & 0xf;
                                        pclog("p %i q %i m %i\n", p, q, m);
                                        if (icd2061_ctrl & (1 << a))
                                           p <<= 1;
                                        icd2061_freq[a] = ((double)p / (double)q) * 2.0 * 14318184.0 / (double)m;
                                        pclog("ICD2061 freq %i = %f\n", a, icd2061_freq[a]);
                                }
                                else if (a == 6)
                                {
                                        icd2061_ctrl = val;
                                        pclog("ICD2061 ctrl = %08X\n", val);
                                }
                                icd2061_unlock = icd2061_data = 0;
                                icd2061_status = 0;
                        }
                }
        }
        icd2061_state = val;
}

double icd2061_getfreq(int i)
{
        pclog("Return freq %f\n", icd2061_freq[i]);
        return icd2061_freq[i];
}
