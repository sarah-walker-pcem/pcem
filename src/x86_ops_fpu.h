#define opESCAPE(num)                                                           \
        static int opESCAPE_ ## num ##_a16(uint32_t fetchdat)                   \
        {                                                                       \
                flags_rebuild();                                                \
                if ((cr0 & 6) == 4)                                             \
                {                                                               \
                        x86_int(7);                                             \
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
                                x87_ ## num();                                  \
                        }                                                       \
                }                                                               \
                return 0;                                                       \
        }                                                                       \
        static int opESCAPE_ ## num ## _a32(uint32_t fetchdat)                  \
        {                                                                       \
                flags_rebuild();                                                \
                if ((cr0 & 6) == 4)                                             \
                {                                                               \
                        x86_int(7);                                             \
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
                                x87_ ## num();                                  \
                        }                                                       \
                }                                                               \
                return 0;                                                       \
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
        cycles -= 4;
        return 0;
}
