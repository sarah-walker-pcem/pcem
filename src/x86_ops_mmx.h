#define SSATB(val) (((val) < -128) ? -128 : (((val) > 127) ? 127 : (val)))
#define SSATW(val) (((val) < -32768) ? -32768 : (((val) > 32767) ? 32767 : (val)))
#define USATB(val) (((val) < 0) ? 0 : (((val) > 255) ? 255 : (val)))
#define USATW(val) (((val) < 0) ? 0 : (((val) > 65535) ? 65535 : (val)))

#define MMX_GETSRC()                                                            \
        if (mod == 3)                                                           \
        {                                                                       \
                src = MM[rm];                                                   \
                cycles--;                                                       \
        }                                                                       \
        else                                                                    \
        {                                                                       \
                src.l[0] = readmeml(easeg, eaaddr);                             \
                src.l[1] = readmeml(easeg, eaaddr + 4); if (abrt) return 0;     \
                cycles -= 2;                                                    \
        }

#define MMX_ENTER()                                                     \
        if (!cpu_hasMMX)                                                \
        {                                                               \
                pc = oldpc;                                             \
                x86illegal();                                           \
                return 0;                                               \
        }                                                               \
        if (cr0 & 0xc)                                                  \
        {                                                               \
                x86_int(7);                                             \
                return 0;                                               \
        }                                                               \
        x87_set_mmx()

int opEMMS(uint32_t fetchdat)
{
        if (!cpu_hasMMX)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        if (cr0 & 4)
        {
                x86_int(7);
                return 0;
        }
        x87_emms();
        cycles -= 100; /*Guess*/
        return 0;
}
