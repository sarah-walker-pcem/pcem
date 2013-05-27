/*ICS2595 clock chip emulation
  Used by ATI Mach64*/

#include "ibm.h"
#include "vid_ics2595.h"

enum
{
        ICS2595_IDLE,
        ICS2595_WRITE,
        ICS2595_READ
};

static int ics2595_oldfs3, ics2595_oldfs2;
static int ics2595_dat;
static int ics2595_pos;
static int ics2595_state = ICS2595_IDLE;

static int ics2595_div[4] = {8, 4, 2, 1};

static double ics2595_clocks[16];
double ics2595_output_clock;

void ics2595_write(int strobe, int dat)
{
        pclog("ics2595_write : %i %i\n", strobe, dat);
        if (strobe)
        {
                if ((dat & 8) && !ics2595_oldfs3) /*Data clock*/
                {
                        pclog(" - new dat %i\n", dat & 4);
                        switch (ics2595_state)
                        {
                                case ICS2595_IDLE:
                                ics2595_state = (dat & 4) ? ICS2595_WRITE : ICS2595_IDLE;
                                ics2595_pos = 0;
                                break;
                                case ICS2595_WRITE:
                                ics2595_dat = (ics2595_dat >> 1);
                                if (dat & 4)
                                        ics2595_dat |= (1 << 19);
                                ics2595_pos++;
                                if (ics2595_pos == 20)
                                {
                                        int d, n, l;
                                        pclog("ICS2595_WRITE : dat %08X\n", ics2595_dat);
                                        l = (ics2595_dat >> 2) & 31;
                                        n = ((ics2595_dat >> 7) & 255) + 257;
                                        d = ics2595_div[(ics2595_dat >> 16) & 3];

                                        ics2595_clocks[l] = (14318181.8 * ((double)n / 46.0)) / (double)d;
                                        pclog("ICS2595 clock set - L %i N %i D %i freq = %f\n", l, n, d, (14318181.8 * ((double)n / 46.0)) / (double)d);
                                        ics2595_state = ICS2595_IDLE;
                                }
                                break;                                                
                        }
                }
                        
                ics2595_oldfs2 = dat & 4;
                ics2595_oldfs3 = dat & 8;
        }
        ics2595_output_clock = ics2595_clocks[dat];
}
