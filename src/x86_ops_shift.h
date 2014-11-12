#define OP_SHIFT_b(c)                                                                   \
        {                                                                               \
                uint8_t temp_orig = temp;                                               \
                if (!c) return 0;                                                       \
                flags_rebuild();                                                        \
                switch (rmdat & 0x38)                                                   \
                {                                                                       \
                        case 0x00: /*ROL b, c*/                                         \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp2 = (temp & 0x80) ? 1 : 0;                          \
                                temp = (temp << 1) | temp2;                             \
                                c--;                                                    \
                        }                                                               \
                        seteab(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((flags & C_FLAG) ^ (temp >> 7)) flags |= V_FLAG;            \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x08: /*ROR b,CL*/                                         \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp2 = temp & 1;                                       \
                                temp >>= 1;                                             \
                                if (temp2) temp |= 0x80;                                \
                                c--;                                                    \
                        }                                                               \
                        seteab(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((temp ^ (temp >> 1)) & 0x40) flags |= V_FLAG;               \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x10: /*RCL b,CL*/                                         \
                        temp2 = flags & C_FLAG;                                         \
                        if (is486) CLOCK_CYCLES_ALWAYS(c);                              \
                        while (c > 0)                                                   \
                        {                                                               \
                                tempc = temp2 ? 1 : 0;                                  \
                                temp2 = temp & 0x80;                                    \
                                temp = (temp << 1) | tempc;                             \
                                c--;                                                    \
                        }                                                               \
                        seteab(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((flags & C_FLAG) ^ (temp >> 7)) flags |= V_FLAG;            \
                        CLOCK_CYCLES((mod == 3) ? 9 : 10);                                \
                        break;                                                          \
                        case 0x18: /*RCR b,CL*/                                         \
                        temp2 = flags & C_FLAG;                                         \
                        if (is486) CLOCK_CYCLES_ALWAYS(c);                              \
                        while (c > 0)                                                   \
                        {                                                               \
                                tempc = temp2 ? 0x80 : 0;                               \
                                temp2 = temp & 1;                                       \
                                temp = (temp >> 1) | tempc;                             \
                                c--;                                                    \
                        }                                                               \
                        seteab(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((temp ^ (temp >> 1)) & 0x40) flags |= V_FLAG;               \
                        CLOCK_CYCLES((mod == 3) ? 9 : 10);                                \
                        break;                                                          \
                        case 0x20: case 0x30: /*SHL b,CL*/                              \
                        seteab(temp << c);      if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SHL8, temp_orig, c, (temp << c) & 0xff);  \
                        if ((temp << (c - 1)) & 0x80) flags |= C_FLAG;                  \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x28: /*SHR b,CL*/                                         \
                        seteab(temp >> c);      if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SHR8, temp_orig, c, temp >> c);           \
                        if ((temp >> (c - 1)) & 1) flags |= C_FLAG;                     \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x38: /*SAR b,CL*/                                         \
                        tempc = ((temp >> (c - 1)) & 1);                                \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp >>= 1;                                             \
                                if (temp & 0x40) temp |= 0x80;                          \
                                c--;                                                    \
                        }                                                               \
                        seteab(temp);           if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SAR8, temp_orig, c, temp);                \
                        if (tempc) flags |= C_FLAG;                                     \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                }                                                                       \
        }

#define OP_SHIFT_w(c)                                                                   \
        {                                                                               \
                uint16_t temp_orig = temp;                                              \
                if (!c) return 0;                                                       \
                flags_rebuild();                                                        \
                switch (rmdat & 0x38)                                                   \
                {                                                                       \
                        case 0x00: /*ROL w, c*/                                         \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp2 = (temp & 0x8000) ? 1 : 0;                        \
                                temp = (temp << 1) | temp2;                             \
                                c--;                                                    \
                        }                                                               \
                        seteaw(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((flags & C_FLAG) ^ (temp >> 15)) flags |= V_FLAG;           \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x08: /*ROR w, c*/                                         \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp2 = temp & 1;                                       \
                                temp >>= 1;                                             \
                                if (temp2) temp |= 0x8000;                              \
                                c--;                                                    \
                        }                                                               \
                        seteaw(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((temp ^ (temp >> 1)) & 0x4000) flags |= V_FLAG;             \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x10: /*RCL w, c*/                                         \
                        temp2 = flags & C_FLAG;                                         \
                        if (is486) CLOCK_CYCLES_ALWAYS(c);                              \
                        while (c > 0)                                                   \
                        {                                                               \
                                tempc = temp2 ? 1 : 0;                                  \
                                temp2 = temp & 0x8000;                                  \
                                temp = (temp << 1) | tempc;                             \
                                c--;                                                    \
                        }                                                               \
                        seteaw(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((flags & C_FLAG) ^ (temp >> 15)) flags |= V_FLAG;           \
                        CLOCK_CYCLES((mod == 3) ? 9 : 10);                                \
                        break;                                                          \
                        case 0x18: /*RCR w, c*/                                         \
                        temp2 = flags & C_FLAG;                                         \
                        if (is486) CLOCK_CYCLES_ALWAYS(c);                              \
                        while (c > 0)                                                   \
                        {                                                               \
                                tempc = temp2 ? 0x8000 : 0;                             \
                                temp2 = temp & 1;                                       \
                                temp = (temp >> 1) | tempc;                             \
                                c--;                                                    \
                        }                                                               \
                        seteaw(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((temp ^ (temp >> 1)) & 0x4000) flags |= V_FLAG;             \
                        CLOCK_CYCLES((mod == 3) ? 9 : 10);                                \
                        break;                                                          \
                        case 0x20: case 0x30: /*SHL w, c*/                              \
                        seteaw(temp << c);      if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SHL16, temp_orig, c, (temp << c) & 0xffff); \
                        if ((temp << (c - 1)) & 0x8000) flags |= C_FLAG;                \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x28: /*SHR w, c*/                                         \
                        seteaw(temp >> c);      if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SHR16, temp_orig, c, temp >> c);          \
                        if ((temp >> (c - 1)) & 1) flags |= C_FLAG;                     \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x38: /*SAR w, c*/                                         \
                        tempc = ((temp >> (c - 1)) & 1);                                \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp >>= 1;                                             \
                                if (temp & 0x4000) temp |= 0x8000;                      \
                                c--;                                                    \
                        }                                                               \
                        seteaw(temp);           if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SAR16, temp_orig, c, temp);               \
                        if (tempc) flags |= C_FLAG;                                     \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                }                                                                       \
        }

#define OP_SHIFT_l(c)                                                                   \
        {                                                                               \
                uint32_t temp_orig = temp;                                              \
                if (!c) return 0;                                                       \
                flags_rebuild();                                                        \
                switch (rmdat & 0x38)                                                   \
                {                                                                       \
                        case 0x00: /*ROL l, c*/                                         \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp2 = (temp & 0x80000000) ? 1 : 0;                    \
                                temp = (temp << 1) | temp2;                             \
                                c--;                                                    \
                        }                                                               \
                        seteal(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((flags & C_FLAG) ^ (temp >> 31)) flags |= V_FLAG;           \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x08: /*ROR l, c*/                                         \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp2 = temp & 1;                                       \
                                temp >>= 1;                                             \
                                if (temp2) temp |= 0x80000000;                          \
                                c--;                                                    \
                        }                                                               \
                        seteal(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((temp ^ (temp >> 1)) & 0x40000000) flags |= V_FLAG;         \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x10: /*RCL l, c*/                                         \
                        temp2 = flags & C_FLAG;                                         \
                        if (is486) CLOCK_CYCLES_ALWAYS(c);                              \
                        while (c > 0)                                                   \
                        {                                                               \
                                tempc = temp2 ? 1 : 0;                                  \
                                temp2 = temp & 0x80000000;                              \
                                temp = (temp << 1) | tempc;                             \
                                c--;                                                    \
                        }                                                               \
                        seteal(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((flags & C_FLAG) ^ (temp >> 31)) flags |= V_FLAG;           \
                        CLOCK_CYCLES((mod == 3) ? 9 : 10);                                \
                        break;                                                          \
                        case 0x18: /*RCR l, c*/                                         \
                        temp2 = flags & C_FLAG;                                         \
                        if (is486) CLOCK_CYCLES_ALWAYS(c);                              \
                        while (c > 0)                                                   \
                        {                                                               \
                                tempc = temp2 ? 0x80000000 : 0;                         \
                                temp2 = temp & 1;                                       \
                                temp = (temp >> 1) | tempc;                             \
                                c--;                                                    \
                        }                                                               \
                        seteal(temp);           if (abrt) return 1;                     \
                        flags &= ~(C_FLAG | V_FLAG);                                    \
                        if (temp2) flags |= C_FLAG;                                     \
                        if ((temp ^ (temp >> 1)) & 0x40000000) flags |= V_FLAG;         \
                        CLOCK_CYCLES((mod == 3) ? 9 : 10);                                \
                        break;                                                          \
                        case 0x20: case 0x30: /*SHL l, c*/                              \
                        seteal(temp << c);      if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SHL32, temp_orig, c, temp << c);          \
                        if ((temp << (c - 1)) & 0x80000000) flags |= C_FLAG;            \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x28: /*SHR l, c*/                                         \
                        seteal(temp >> c);      if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SHR32, temp_orig, c, temp >> c);          \
                        if ((temp >> (c - 1)) & 1) flags |= C_FLAG;                     \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                        case 0x38: /*SAR l, c*/                                         \
                        tempc = ((temp >> (c - 1)) & 1);                                \
                        while (c > 0)                                                   \
                        {                                                               \
                                temp >>= 1;                                             \
                                if (temp & 0x40000000) temp |= 0x80000000;              \
                                c--;                                                    \
                        }                                                               \
                        seteal(temp);           if (abrt) return 1;                     \
                        set_flags_shift(FLAGS_SAR32, temp_orig, c, temp);               \
                        if (tempc) flags |= C_FLAG;                                     \
                        CLOCK_CYCLES((mod == 3) ? 3 : 7);                                 \
                        break;                                                          \
                }                                                                       \
        }

static int opC0_a16(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint8_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        c = readmemb(cs, pc) & 31; pc++;
        temp = geteab();                if (abrt) return 1;
        OP_SHIFT_b(c);
        return 0;
}
static int opC0_a32(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint8_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        c = readmemb(cs, pc) & 31; pc++;
        temp = geteab();                if (abrt) return 1;
        OP_SHIFT_b(c);
        return 0;
}
static int opC1_w_a16(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint16_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        c = readmemb(cs, pc) & 31; pc++;
        temp = geteaw();                if (abrt) return 1;
        OP_SHIFT_w(c);
        return 0;
}
static int opC1_w_a32(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint16_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        c = readmemb(cs, pc) & 31; pc++;
        temp = geteaw();                if (abrt) return 1;
        OP_SHIFT_w(c);
        return 0;
}
static int opC1_l_a16(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint32_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        c = readmemb(cs, pc) & 31; pc++;
        temp = geteal();                if (abrt) return 1;
        OP_SHIFT_l(c);
        return 0;
}
static int opC1_l_a32(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint32_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        c = readmemb(cs, pc) & 31; pc++;
        temp = geteal();                if (abrt) return 1;
        OP_SHIFT_l(c);
        return 0;
}

static int opD0_a16(uint32_t fetchdat)
{
        int c = 1;
        int tempc;
        uint8_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        temp = geteab();                if (abrt) return 1;
        OP_SHIFT_b(c);
        return 0;
}
static int opD0_a32(uint32_t fetchdat)
{
        int c = 1;
        int tempc;
        uint8_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        temp = geteab();                if (abrt) return 1;
        OP_SHIFT_b(c);
        return 0;
}
static int opD1_w_a16(uint32_t fetchdat)
{
        int c = 1;
        int tempc;
        uint16_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        temp = geteaw();                if (abrt) return 1;
        OP_SHIFT_w(c);
        return 0;
}
static int opD1_w_a32(uint32_t fetchdat)
{
        int c = 1;
        int tempc;
        uint16_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        temp = geteaw();                if (abrt) return 1;
        OP_SHIFT_w(c);
        return 0;
}
static int opD1_l_a16(uint32_t fetchdat)
{
        int c = 1;
        int tempc;
        uint32_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        temp = geteal();                if (abrt) return 1;
        OP_SHIFT_l(c);
        return 0;
}
static int opD1_l_a32(uint32_t fetchdat)
{
        int c = 1;
        int tempc;
        uint32_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        temp = geteal();                if (abrt) return 1;
        OP_SHIFT_l(c);
        return 0;
}

static int opD2_a16(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint8_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        c = CL & 31;
        temp = geteab();                if (abrt) return 1;
        OP_SHIFT_b(c);
        return 0;
}
static int opD2_a32(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint8_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        c = CL & 31;
        temp = geteab();                if (abrt) return 1;
        OP_SHIFT_b(c);
        return 0;
}
static int opD3_w_a16(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint16_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        c = CL & 31;
        temp = geteaw();                if (abrt) return 1;
        OP_SHIFT_w(c);
        return 0;
}
static int opD3_w_a32(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint16_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        c = CL & 31;
        temp = geteaw();                if (abrt) return 1;
        OP_SHIFT_w(c);
        return 0;
}
static int opD3_l_a16(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint32_t temp, temp2;
        
        fetch_ea_16(fetchdat);
        c = CL & 31;
        temp = geteal();                if (abrt) return 1;
        OP_SHIFT_l(c);
        return 0;
}
static int opD3_l_a32(uint32_t fetchdat)
{
        int c;
        int tempc;
        uint32_t temp, temp2;
        
        fetch_ea_32(fetchdat);
        c = CL & 31;
        temp = geteal();                if (abrt) return 1;
        OP_SHIFT_l(c);
        return 0;
}


#define SHLD_w()                                                                \
        if (count)                                                              \
        {                                                                       \
                uint16_t tempw = geteaw();      if (abrt) return 1;             \
                int tempc = ((tempw << (count - 1)) & (1 << 15)) ? 1 : 0;       \
                uint32_t templ = (tempw << 16) | regs[reg].w;                   \
                if (count <= 16) tempw =  templ >> (16 - count);                \
                else             tempw = (templ << count) >> 16;                \
                seteaw(tempw);                  if (abrt) return 1;             \
                setznp16(tempw);                                                \
                flags_rebuild();                                                \
                if (tempc) flags |= C_FLAG;                                     \
        }

#define SHLD_l()                                                                \
        if (count)                                                              \
        {                                                                       \
                uint32_t templ = geteal();      if (abrt) return 1;             \
                int tempc = ((templ << (count - 1)) & (1 << 31)) ? 1 : 0;       \
                templ = (templ << count) | (regs[reg].l >> (32 - count));       \
                seteal(templ);                  if (abrt) return 1;             \
                setznp32(templ);                                                \
                flags_rebuild();                                                \
                if (tempc) flags |= C_FLAG;                                     \
        }


#define SHRD_w()                                                                \
        if (count)                                                              \
        {                                                                       \
                uint16_t tempw = geteaw();      if (abrt) return 1;             \
                int tempc = (tempw >> (count - 1)) & 1;                         \
                uint32_t templ = tempw | (regs[reg].w << 16);                   \
                tempw = templ >> count;                                         \
                seteaw(tempw);                  if (abrt) return 1;             \
                setznp16(tempw);                                                \
                flags_rebuild();                                                \
                if (tempc) flags |= C_FLAG;                                     \
        }

#define SHRD_l()                                                                \
        if (count)                                                              \
        {                                                                       \
                uint32_t templ = geteal();      if (abrt) return 1;             \
                int tempc = (templ >> (count - 1)) & 1;                         \
                templ = (templ >> count) | (regs[reg].l << (32 - count));       \
                seteal(templ);                  if (abrt) return 1;             \
                setznp32(templ);                                                \
                flags_rebuild();                                                \
                if (tempc) flags |= C_FLAG;                                     \
        }

#define opSHxD(operation)                                                       \
        static int op ## operation ## _i_a16(uint32_t fetchdat)                 \
        {                                                                       \
                int count;                                                      \
                                                                                \
                fetch_ea_16(fetchdat);                                          \
                count = getbyte() & 31;                                         \
                operation();                                                    \
                                                                                \
                CLOCK_CYCLES(3);                                                \
                return 0;                                                       \
        }                                                                       \
        static int op ## operation ## _CL_a16(uint32_t fetchdat)                \
        {                                                                       \
                int count;                                                      \
                                                                                \
                fetch_ea_16(fetchdat);                                          \
                count = CL & 31;                                                \
                operation();                                                    \
                                                                                \
                CLOCK_CYCLES(3);                                                \
                return 0;                                                       \
        }                                                                       \
        static int op ## operation ## _i_a32(uint32_t fetchdat)                 \
        {                                                                       \
                int count;                                                      \
                                                                                \
                fetch_ea_32(fetchdat);                                          \
                count = getbyte() & 31;                                         \
                operation();                                                    \
                                                                                \
                CLOCK_CYCLES(3);                                                \
                return 0;                                                       \
        }                                                                       \
        static int op ## operation ## _CL_a32(uint32_t fetchdat)                \
        {                                                                       \
                int count;                                                      \
                                                                                \
                fetch_ea_32(fetchdat);                                          \
                count = CL & 31;                                                \
                operation();                                                    \
                                                                                \
                CLOCK_CYCLES(3);                                                \
                return 0;                                                       \
        }
              
opSHxD(SHLD_w)
opSHxD(SHLD_l)
opSHxD(SHRD_w)
opSHxD(SHRD_l)
