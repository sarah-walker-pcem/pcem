static inline void setznp8(uint8_t val)
{
        flags&=~0x8d5;
        flags|=znptable8[val];
}

static inline void setznp16(uint16_t val)
{
        flags&=~0x8d5;
        flags|=znptable16[val];
}
static inline void setznp32(uint32_t val)
{
        flags&=~0x8d5;
        flags|=((val&0x80000000)?N_FLAG:((!val)?Z_FLAG:0));
        flags|=znptable8[val&0xFF]&P_FLAG;
}
