enum
{
        FLAGS_UNKNOWN,
        
        FLAGS_ZN8,
        FLAGS_ZN16,
        FLAGS_ZN32,
        
        FLAGS_ADD8,
        FLAGS_ADD16,
        FLAGS_ADD32,
        
        FLAGS_SUB8,
        FLAGS_SUB16,
        FLAGS_SUB32,
        
        FLAGS_SHL8,
        FLAGS_SHL16,
        FLAGS_SHL32,

        FLAGS_SHR8,
        FLAGS_SHR16,
        FLAGS_SHR32,

        FLAGS_SAR8,
        FLAGS_SAR16,
        FLAGS_SAR32,
};

int flags_op;
uint32_t flags_res;
uint32_t flags_op1, flags_op2;

static inline int ZF_SET()
{
        switch (flags_op)
        {
                case FLAGS_ZN8: 
                case FLAGS_ZN16:
                case FLAGS_ZN32:
                case FLAGS_ADD8:
                case FLAGS_ADD16:
                case FLAGS_ADD32:
                case FLAGS_SUB8:
                case FLAGS_SUB16:
                case FLAGS_SUB32:
                case FLAGS_SHL8:
                case FLAGS_SHL16:
                case FLAGS_SHL32:
                case FLAGS_SHR8:
                case FLAGS_SHR16:
                case FLAGS_SHR32:
                case FLAGS_SAR8:
                case FLAGS_SAR16:
                case FLAGS_SAR32:
                return !flags_res;
                
                case FLAGS_UNKNOWN:
                return flags & Z_FLAG;
        }
}

static inline int NF_SET()
{
        switch (flags_op)
        {
                case FLAGS_ZN8: 
                case FLAGS_ADD8:
                case FLAGS_SUB8:
                case FLAGS_SHL8:
                case FLAGS_SHR8:
                case FLAGS_SAR8:
                return flags_res & 0x80;
                
                case FLAGS_ZN16:
                case FLAGS_ADD16:
                case FLAGS_SUB16:
                case FLAGS_SHL16:
                case FLAGS_SHR16:
                case FLAGS_SAR16:
                return flags_res & 0x8000;
                
                case FLAGS_ZN32:
                case FLAGS_ADD32:
                case FLAGS_SUB32:
                case FLAGS_SHL32:
                case FLAGS_SHR32:
                case FLAGS_SAR32:
                return flags_res & 0x80000000;
                
                case FLAGS_UNKNOWN:
                return flags & N_FLAG;
        }
}

static inline int PF_SET()
{
        switch (flags_op)
        {
                case FLAGS_ZN8: 
                case FLAGS_ZN16:
                case FLAGS_ZN32:
                case FLAGS_ADD8:
                case FLAGS_ADD16:
                case FLAGS_ADD32:
                case FLAGS_SUB8:
                case FLAGS_SUB16:
                case FLAGS_SUB32:
                case FLAGS_SHL8:
                case FLAGS_SHL16:
                case FLAGS_SHL32:
                case FLAGS_SHR8:
                case FLAGS_SHR16:
                case FLAGS_SHR32:
                case FLAGS_SAR8:
                case FLAGS_SAR16:
                case FLAGS_SAR32:
                return znptable8[flags_res & 0xff] & P_FLAG;
                
                case FLAGS_UNKNOWN:
                return flags & P_FLAG;
        }
}

static inline int VF_SET()
{
        switch (flags_op)
        {
                case FLAGS_ZN8:
                case FLAGS_ZN16:
                case FLAGS_ZN32:
                case FLAGS_SAR8:
                case FLAGS_SAR16:
                case FLAGS_SAR32:
                return 0;
                
                case FLAGS_ADD8:
                return !((flags_op1 ^ flags_op2) & 0x80) && ((flags_op1 ^ flags_res) & 0x80);
                case FLAGS_ADD16:
                return !((flags_op1 ^ flags_op2) & 0x8000) && ((flags_op1 ^ flags_res) & 0x8000);
                case FLAGS_ADD32:
                return !((flags_op1 ^ flags_op2) & 0x80000000) && ((flags_op1 ^ flags_res) & 0x80000000);
                                
                case FLAGS_SUB8:
                return ((flags_op1 ^ flags_op2) & (flags_op1 ^ flags_res) & 0x80);
                case FLAGS_SUB16:
                return ((flags_op1 ^ flags_op2) & (flags_op1 ^ flags_res) & 0x8000);
                case FLAGS_SUB32:
                return ((flags_op1 ^ flags_op2) & (flags_op1 ^ flags_res) & 0x80000000);

                case FLAGS_SHL8:
                return (((flags_op1 << flags_op2) ^ (flags_op1 << (flags_op2 - 1))) & 0x80);
                case FLAGS_SHL16:
                return (((flags_op1 << flags_op2) ^ (flags_op1 << (flags_op2 - 1))) & 0x8000);
                case FLAGS_SHL32:
                return (((flags_op1 << flags_op2) ^ (flags_op1 << (flags_op2 - 1))) & 0x80000000);
                
                case FLAGS_SHR8:
                return ((flags_op2 == 1) && (flags_op1 & 0x80));
                case FLAGS_SHR16:
                return ((flags_op2 == 1) && (flags_op1 & 0x8000));
                case FLAGS_SHR32:
                return ((flags_op2 == 1) && (flags_op1 & 0x80000000));
                
                case FLAGS_UNKNOWN:
                return flags & V_FLAG;
        }
}

static inline int AF_SET()
{
        switch (flags_op)
        {
                case FLAGS_ZN8: 
                case FLAGS_ZN16:
                case FLAGS_ZN32:
                case FLAGS_SHL8:
                case FLAGS_SHL16:
                case FLAGS_SHL32:
                case FLAGS_SHR8:
                case FLAGS_SHR16:
                case FLAGS_SHR32:
                case FLAGS_SAR8:
                case FLAGS_SAR16:
                case FLAGS_SAR32:
                return 0;
                
                case FLAGS_ADD8:
                case FLAGS_ADD16:
                case FLAGS_ADD32:
                return ((flags_op1 & 0xF) + (flags_op2 & 0xF)) & 0x10;

                case FLAGS_SUB8:
                case FLAGS_SUB16:
                case FLAGS_SUB32:
                return ((flags_op1 & 0xF) - (flags_op2 & 0xF)) & 0x10;
                
                case FLAGS_UNKNOWN:
                return flags & A_FLAG;
        }
}

#if 0
static inline int CF_SET()
{
        switch (flags_op)
        {
                case FLAGS_ZN8: 
                case FLAGS_ZN16:
                case FLAGS_ZN32:
                return flags & C_FLAG; /*Temporary*/
                
                case FLAGS_ADD8:
                return (flags_op1 + flags_op2) & 0x100;
                case FLAGS_ADD16:
//                return (flags_op1 + flags_op2) & 0x10000;
                                
                case FLAGS_UNKNOWN:
                return flags & C_FLAG;
        }
}
#endif

//#define ZF_SET() (flags & Z_FLAG)
//#define NF_SET() (flags & N_FLAG)
//#define PF_SET() (flags & P_FLAG)
//#define VF_SET() (flags & V_FLAG)
#define CF_SET() (flags & C_FLAG)
//#define AF_SET() (flags & A_FLAG)

static inline void flags_rebuild()
{
        if (flags_op != FLAGS_UNKNOWN)
        {
                uint16_t tempf = 0;
                if (CF_SET()) tempf |= C_FLAG;
                if (PF_SET()) tempf |= P_FLAG;
                if (AF_SET()) tempf |= A_FLAG;
                if (ZF_SET()) tempf |= Z_FLAG;                                
                if (NF_SET()) tempf |= N_FLAG;
                if (VF_SET()) tempf |= V_FLAG;
                flags = (flags & ~0x8d5) | tempf;
                flags_op = FLAGS_UNKNOWN;
        }
}

static inline void flags_extract()
{
        flags_op = FLAGS_UNKNOWN;
}

static inline void flags_rebuild_c()
{
        if (flags_op != FLAGS_UNKNOWN)
        {
                if (CF_SET())
                   flags |=  C_FLAG;
                else
                   flags &= ~C_FLAG;
        }                
}

static inline void setznp8(uint8_t val)
{
        flags_op = FLAGS_ZN8;
        flags_res = val;
        flags &= ~C_FLAG;
}
static inline void setznp16(uint16_t val)
{
        flags_op = FLAGS_ZN16;
        flags_res = val;
        flags &= ~C_FLAG;
}
static inline void setznp32(uint32_t val)
{
        flags_op = FLAGS_ZN32;
        flags_res = val;
        flags &= ~C_FLAG;
}

#define set_flags_shift(op, orig, shift, res) \
        flags_op = op;                  \
        flags_res = res;                \
        flags_op1 = orig;               \
        flags_op2 = shift;              \
        flags &= ~C_FLAG;

static inline void setadd8(uint8_t a, uint8_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a + b) & 0xff;
        flags_op = FLAGS_ADD8;

        if ((a + b) & 0x100) flags |=  C_FLAG;
        else                 flags &= ~C_FLAG;
}
static inline void setadd16(uint16_t a, uint16_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a + b) & 0xffff;
        flags_op = FLAGS_ADD16;

        if ((a + b) & 0x10000) flags |= C_FLAG;
        else                   flags &= ~C_FLAG;
}
static inline void setadd32(uint32_t a, uint32_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = a + b;
        flags_op = FLAGS_ADD32;

        if (flags_res < a) flags |= C_FLAG;
        else               flags &= ~C_FLAG;
}
static inline void setadd8nc(uint8_t a, uint8_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a + b) & 0xff;
        flags_op = FLAGS_ADD8;
}
static inline void setadd16nc(uint16_t a, uint16_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a + b) & 0xffff;
        flags_op = FLAGS_ADD16;
}
static inline void setadd32nc(uint32_t a, uint32_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = a + b;
        flags_op = FLAGS_ADD32;
}

static inline void setsub8(uint8_t a, uint8_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a - b) & 0xff;
        flags_op = FLAGS_SUB8;

        if (a < b) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
}
static inline void setsub16(uint16_t a, uint16_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a - b) & 0xffff;
        flags_op = FLAGS_SUB16;

        if (a < b) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
}
static inline void setsub32(uint32_t a, uint32_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = a - b;
        flags_op = FLAGS_SUB32;
        
        if (a < b) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
}

static inline void setsub8nc(uint8_t a, uint8_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a - b) & 0xff;
        flags_op = FLAGS_SUB8;
}
static inline void setsub16nc(uint16_t a, uint16_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = (a - b) & 0xffff;
        flags_op = FLAGS_SUB16;
}
static inline void setsub32nc(uint32_t a, uint32_t b)
{
        flags_op1 = a;
        flags_op2 = b;
        flags_res = a - b;
        flags_op = FLAGS_SUB32;
}

static inline void setadc8(uint8_t a, uint8_t b)
{
        uint16_t c=(uint16_t)a+(uint16_t)b+tempc;
        flags_op = FLAGS_UNKNOWN;
        flags&=~0x8D5;
        flags|=znptable8[c&0xFF];
        if (c&0x100) flags|=C_FLAG;
        if (!((a^b)&0x80)&&((a^c)&0x80)) flags|=V_FLAG;
        if (((a&0xF)+(b&0xF))&0x10)      flags|=A_FLAG;
}
static inline void setadc16(uint16_t a, uint16_t b)
{
        uint32_t c=(uint32_t)a+(uint32_t)b+tempc;
        flags_op = FLAGS_UNKNOWN;
        flags&=~0x8D5;
        flags|=znptable16[c&0xFFFF];
        if (c&0x10000) flags|=C_FLAG;
        if (!((a^b)&0x8000)&&((a^c)&0x8000)) flags|=V_FLAG;
        if (((a&0xF)+(b&0xF))&0x10)      flags|=A_FLAG;
}

static inline void setsbc8(uint8_t a, uint8_t b)
{
        uint16_t c=(uint16_t)a-(((uint16_t)b)+tempc);
        flags_op = FLAGS_UNKNOWN;
        flags&=~0x8D5;
        flags|=znptable8[c&0xFF];
        if (c&0x100) flags|=C_FLAG;
        if ((a^b)&(a^c)&0x80) flags|=V_FLAG;
        if (((a&0xF)-(b&0xF))&0x10)      flags|=A_FLAG;
}
static inline void setsbc16(uint16_t a, uint16_t b)
{
        uint32_t c=(uint32_t)a-(((uint32_t)b)+tempc);
        flags_op = FLAGS_UNKNOWN;
        flags&=~0x8D5;
        flags|=(znptable16[c&0xFFFF]&~4);
        flags|=(znptable8[c&0xFF]&4);
        if (c&0x10000) flags|=C_FLAG;
        if ((a^b)&(a^c)&0x8000) flags|=V_FLAG;
        if (((a&0xF)-(b&0xF))&0x10)      flags|=A_FLAG;
}

static inline void setadc32(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a+(uint32_t)b+tempc;
        flags_op = FLAGS_UNKNOWN;
        flags&=~0x8D5;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if ((c<a) || (c==a && tempc)) flags|=C_FLAG;
        if (!((a^b)&0x80000000)&&((a^c)&0x80000000)) flags|=V_FLAG;
        if (((a&0xF)+(b&0xF)+tempc)&0x10)      flags|=A_FLAG;
}
static inline void setsbc32(uint32_t a, uint32_t b)
{
        uint32_t c=(uint32_t)a-(((uint32_t)b)+tempc);
        flags_op = FLAGS_UNKNOWN;
        flags&=~0x8D5;
        flags|=((c&0x80000000)?N_FLAG:((!c)?Z_FLAG:0));
        flags|=(znptable8[c&0xFF]&P_FLAG);
        if ((c>a) || (c==a && tempc)) flags|=C_FLAG;
        if ((a^b)&(a^c)&0x80000000) flags|=V_FLAG;
        if (((a&0xF)-((b&0xF)+tempc))&0x10)      flags|=A_FLAG;
}

