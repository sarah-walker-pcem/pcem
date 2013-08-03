#define MMX_GETSHIFT()                                                  \
        if (mod == 3)                                                   \
        {                                                               \
                shift = MM[rm].b[0];                                    \
                cycles--;                                               \
        }                                                               \
        else                                                            \
        {                                                               \
                shift = readmemb(easeg, eaaddr); if (abrt) return 0;    \
                cycles -= 2;                                            \
        }

int opPSxxW_imm(uint32_t fetchdat)
{
        int reg = fetchdat & 7;
        int op = fetchdat & 0x38;
        int shift = (fetchdat >> 8) & 0xff;
        
        pc += 2;
        MMX_ENTER();

        switch (op)
        {
                case 0x10: /*PSRLW*/
                if (shift > 15)
                        MM[reg].q = 0;
                else
                {
                        MM[reg].w[0] >>= shift;
                        MM[reg].w[1] >>= shift;
                        MM[reg].w[2] >>= shift;
                        MM[reg].w[3] >>= shift;
                }
                break;
                case 0x20: /*PSRAW*/
                if (shift > 15)
                        shift = 15;
                MM[reg].sw[0] >>= shift;
                MM[reg].sw[1] >>= shift;
                MM[reg].sw[2] >>= shift;
                MM[reg].sw[3] >>= shift;
                break;
                case 0x30: /*PSLLW*/
                if (shift > 15)
                        MM[reg].q = 0;
                else
                {
                        MM[reg].w[0] <<= shift;
                        MM[reg].w[1] <<= shift;
                        MM[reg].w[2] <<= shift;
                        MM[reg].w[3] <<= shift;
                }
                break;
                default:
                pclog("Bad PSxxW (0F 71) instruction %02X\n", op);
                pc = oldpc;
                x86illegal();
                return 0;
        }

        cycles--;
        return 0;
}

int opPSLLW_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 15)
                MM[reg].q = 0;
        else
        {
                MM[reg].w[0] <<= shift;
                MM[reg].w[1] <<= shift;
                MM[reg].w[2] <<= shift;
                MM[reg].w[3] <<= shift;
        }

        return 0;
}
int opPSLLW_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 15)
                MM[reg].q = 0;
        else
        {
                MM[reg].w[0] <<= shift;
                MM[reg].w[1] <<= shift;
                MM[reg].w[2] <<= shift;
                MM[reg].w[3] <<= shift;
        }

        return 0;
}

int opPSRLW_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 15)
                MM[reg].q = 0;
        else
        {
                MM[reg].w[0] >>= shift;
                MM[reg].w[1] >>= shift;
                MM[reg].w[2] >>= shift;
                MM[reg].w[3] >>= shift;
        }

        return 0;
}
int opPSRLW_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 15)
                MM[reg].q = 0;
        else
        {
                MM[reg].w[0] >>= shift;
                MM[reg].w[1] >>= shift;
                MM[reg].w[2] >>= shift;
                MM[reg].w[3] >>= shift;
        }

        return 0;
}

int opPSRAW_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 15)
                shift = 15;

        MM[reg].sw[0] >>= shift;
        MM[reg].sw[1] >>= shift;
        MM[reg].sw[2] >>= shift;
        MM[reg].sw[3] >>= shift;
        
        return 0;
}
int opPSRAW_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 15)
                shift = 15;

        MM[reg].sw[0] >>= shift;
        MM[reg].sw[1] >>= shift;
        MM[reg].sw[2] >>= shift;
        MM[reg].sw[3] >>= shift;
        
        return 0;
}

int opPSxxD_imm(uint32_t fetchdat)
{
        int reg = fetchdat & 7;
        int op = fetchdat & 0x38;
        int shift = (fetchdat >> 8) & 0xff;
        
        pc += 2;
        MMX_ENTER();

        switch (op)
        {
                case 0x10: /*PSRLD*/
                if (shift > 31)
                        MM[reg].q = 0;
                else
                {
                        MM[reg].l[0] >>= shift;
                        MM[reg].l[1] >>= shift;
                }
                break;
                case 0x20: /*PSRAD*/
                if (shift > 31)
                        shift = 31;
                MM[reg].sl[0] >>= shift;
                MM[reg].sl[1] >>= shift;
                break;
                case 0x30: /*PSLLD*/
                if (shift > 31)
                        MM[reg].q = 0;
                else
                {
                        MM[reg].l[0] <<= shift;
                        MM[reg].l[1] <<= shift;
                }
                break;
                default:
                pclog("Bad PSxxD (0F 72) instruction %02X\n", op);
                pc = oldpc;
                x86illegal();
                return 0;
        }

        cycles--;
        return 0;
}

int opPSLLD_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 31)
                MM[reg].q = 0;
        else
        {
                MM[reg].l[0] <<= shift;
                MM[reg].l[1] <<= shift;
        }

        return 0;
}
int opPSLLD_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 31)
                MM[reg].q = 0;
        else
        {
                MM[reg].l[0] <<= shift;
                MM[reg].l[1] <<= shift;
        }

        return 0;
}

int opPSRLD_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 31)
                MM[reg].q = 0;
        else
        {
                MM[reg].l[0] >>= shift;
                MM[reg].l[1] >>= shift;
        }

        return 0;
}
int opPSRLD_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 31)
                MM[reg].q = 0;
        else
        {
                MM[reg].l[0] >>= shift;
                MM[reg].l[1] >>= shift;
        }

        return 0;
}

int opPSRAD_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 31)
                shift = 31;

        MM[reg].sl[0] >>= shift;
        MM[reg].sl[1] >>= shift;
        
        return 0;
}
int opPSRAD_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 31)
                shift = 31;

        MM[reg].sl[0] >>= shift;
        MM[reg].sl[1] >>= shift;

        return 0;
}

int opPSxxQ_imm(uint32_t fetchdat)
{
        int reg = fetchdat & 7;
        int op = fetchdat & 0x38;
        int shift = (fetchdat >> 8) & 0xff;
        
        pc += 2;
        MMX_ENTER();

        switch (op)
        {
                case 0x10: /*PSRLW*/
                if (shift > 63)
                        MM[reg].q = 0;
                else
                        MM[reg].q >>= shift;
                break;
                case 0x20: /*PSRAW*/
                if (shift > 63)
                        shift = 63;
                MM[reg].sq >>= shift;
                break;
                case 0x30: /*PSLLW*/
                if (shift > 63)
                        MM[reg].q = 0;
                else
                        MM[reg].q <<= shift;
                break;
                default:
                pclog("Bad PSxxQ (0F 73) instruction %02X\n", op);
                pc = oldpc;
                x86illegal();
                return 0;
        }

        cycles--;
        return 0;
}

int opPSLLQ_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 63)
                MM[reg].q = 0;
        else
                MM[reg].q <<= shift;

        return 0;
}
int opPSLLQ_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 63)
                MM[reg].q = 0;
        else
                MM[reg].q <<= shift;

        return 0;
}

int opPSRLQ_a16(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        MMX_GETSHIFT();

        if (shift > 63)
                MM[reg].q = 0;
        else
                MM[reg].q >>= shift;

        return 0;
}
int opPSRLQ_a32(uint32_t fetchdat)
{
        int shift;
        
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        MMX_GETSHIFT();

        if (shift > 63)
                MM[reg].q = 0;
        else
                MM[reg].q >>= shift;

        return 0;
}
