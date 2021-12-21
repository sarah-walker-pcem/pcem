//Quake timedemo demo1 - 8.1FPS

//11A00 - D_SCAlloc
//11C1C - D_CacheSurface

//36174 - SCR_CalcRefdef

//SCR_CalcRefdef
//Calls R_SetVrect and R_ViewChanged

#define fplog 0

#include <math.h>
#include "ibm.h"
#include "pic.h"
#include "x86.h"
#include "x86_flags.h"
#include "x86_ops.h"
#include "x87.h"
#include "386_common.h"

#define X87_TAG_VALID   0
#define X87_TAG_ZERO    1
#define X87_TAG_INVALID 2
#define X87_TAG_EMPTY   3

uint16_t x87_gettag()
{
        uint16_t ret = 0;
        int c;
        
        for (c = 0; c < 8; c++)
        {
                if (cpu_state.tag[c] == TAG_EMPTY)
                        ret |= X87_TAG_EMPTY << (c * 2);
                else if (cpu_state.tag[c] & TAG_UINT64)
                        ret |= 2 << (c*2);
                else if (cpu_state.ST[c] == 0.0 && !cpu_state.ismmx)
                        ret |= X87_TAG_ZERO << (c * 2);
                else
                        ret |= X87_TAG_VALID << (c * 2);
        }

        return ret;
}

void x87_settag(uint16_t new_tag)
{
        int c;
        
        for (c = 0; c < 8; c++)
        {
                int tag = (new_tag >> (c * 2)) & 3;
                
                if (tag == X87_TAG_EMPTY)
                        cpu_state.tag[c] = TAG_EMPTY;
                else if (tag == 2)
                        cpu_state.tag[c] = TAG_VALID | TAG_UINT64;
                else
                        cpu_state.tag[c] = TAG_VALID;
        }
}

void x87_dumpregs()
{
        if (cpu_state.ismmx)
        {
                pclog("MM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\n", cpu_state.MM[0].q, cpu_state.MM[1].q, cpu_state.MM[2].q, cpu_state.MM[3].q);
                pclog("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", cpu_state.MM[4].q, cpu_state.MM[5].q, cpu_state.MM[6].q, cpu_state.MM[7].q);
        }
        else
        {
                pclog("ST(0)=%f\tST(1)=%f\tST(2)=%f\tST(3)=%f\t\n",cpu_state.ST[cpu_state.TOP&7],cpu_state.ST[(cpu_state.TOP+1)&7],cpu_state.ST[(cpu_state.TOP+2)&7],cpu_state.ST[(cpu_state.TOP+3)&7]);
                pclog("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\t\n",cpu_state.ST[(cpu_state.TOP+4)&7],cpu_state.ST[(cpu_state.TOP+5)&7],cpu_state.ST[(cpu_state.TOP+6)&7],cpu_state.ST[(cpu_state.TOP+7)&7]);
        }
        pclog("Status = %04X  Control = %04X  Tag = %04X\n", cpu_state.npxs, cpu_state.npxc, x87_gettag());
}

void x87_print()
{
        if (cpu_state.ismmx)
        {
                pclog("\tMM0=%016llX\tMM1=%016llX\tMM2=%016llX\tMM3=%016llX\t", cpu_state.MM[0].q, cpu_state.MM[1].q, cpu_state.MM[2].q, cpu_state.MM[3].q);
                pclog("MM4=%016llX\tMM5=%016llX\tMM6=%016llX\tMM7=%016llX\n", cpu_state.MM[4].q, cpu_state.MM[5].q, cpu_state.MM[6].q, cpu_state.MM[7].q);
        }
        else
        {
                pclog("\tST(0)=%.20f\tST(1)=%.20f\tST(2)=%f\tST(3)=%f\t",cpu_state.ST[cpu_state.TOP&7],cpu_state.ST[(cpu_state.TOP+1)&7],cpu_state.ST[(cpu_state.TOP+2)&7],cpu_state.ST[(cpu_state.TOP+3)&7]);
                pclog("ST(4)=%f\tST(5)=%f\tST(6)=%f\tST(7)=%f\tTOP=%i CR=%04X SR=%04X TAG=%04X\n",cpu_state.ST[(cpu_state.TOP+4)&7],cpu_state.ST[(cpu_state.TOP+5)&7],cpu_state.ST[(cpu_state.TOP+6)&7],cpu_state.ST[(cpu_state.TOP+7)&7], cpu_state.TOP, cpu_state.npxc, cpu_state.npxs, x87_gettag());
        }
}

void x87_reset()
{
}
