#define opESCAPE(num)                                                           \
        static int opESCAPE_ ## num ##_a16(uint32_t fetchdat)                   \
        {                                                                       \
                flags_rebuild();                                                \
                if (cr0 & 0xc)                                                  \
                {                                                               \
                        x86_int(7);                                             \
                        return 1;                                               \
                }                                                               \
                else                                                            \
                {                                                               \
                        fpucount++;                                             \
                        fetch_ea_16(fetchdat);                                  \
                        if (hasfpu)                                             \
                        {                                                       \
                                x87_pc_off = oldpc;                             \
                                x87_pc_seg = CS;                                \
                                x87_op_off = eaaddr;                            \
                                x87_op_seg = ea_rseg;                           \
                                x87_ ## num(fetchdat);                          \
                        }                                                       \
                }                                                               \
                return abrt;                                                       \
        }                                                                       \
        static int opESCAPE_ ## num ## _a32(uint32_t fetchdat)                  \
        {                                                                       \
                flags_rebuild();                                                \
                if (cr0 & 0xc)                                                  \
                {                                                               \
                        x86_int(7);                                             \
                        return 1;                                               \
                }                                                               \
                else                                                            \
                {                                                               \
                        fpucount++;                                             \
                        fetch_ea_32(fetchdat);                                  \
                        if (hasfpu)                                             \
                        {                                                       \
                                x87_pc_off = oldpc;                             \
                                x87_pc_seg = CS;                                \
                                x87_op_off = eaaddr;                            \
                                x87_op_seg = ea_rseg;                           \
                                x87_ ## num(fetchdat);                          \
                        }                                                       \
                }                                                               \
                return abrt;                                                       \
        }

opESCAPE(d8);
opESCAPE(d9);
opESCAPE(da);
opESCAPE(db);
opESCAPE(dc);
opESCAPE(dd);
opESCAPE(de);
opESCAPE(df);

static int opWAIT(uint32_t fetchdat)
{
        if ((cr0 & 0xa) == 0xa)
        {
                x86_int(7);
                return 1;
        }
        cycles -= 4;
        return 0;
}
