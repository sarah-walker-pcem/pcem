static int opCMPXCHG_b_a16(uint32_t fetchdat)
{
        uint8_t temp, temp2 = AL;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_16(fetchdat);
        temp = geteab();                        if (abrt) return 0;
        if (AL == temp) seteab(getr8(reg));
        else            AL = temp;
        if (abrt) return 0;
        setsub8(temp2, temp);
        cycles -= (mod == 3) ? 6 : 10;
        return 0;
}
static int opCMPXCHG_b_a32(uint32_t fetchdat)
{
        uint8_t temp, temp2 = AL;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_32(fetchdat);
        temp = geteab();                        if (abrt) return 0;
        if (AL == temp) seteab(getr8(reg));
        else            AL = temp;
        if (abrt) return 0;
        setsub8(temp2, temp);
        cycles -= (mod == 3) ? 6 : 10;
        return 0;
}

static int opCMPXCHG_w_a16(uint32_t fetchdat)
{
        uint16_t temp, temp2 = AX;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_16(fetchdat);
        temp = geteaw();                        if (abrt) return 0;
        if (AX == temp) seteaw(regs[reg].w);
        else            AX = temp;
        if (abrt) return 0;
        setsub16(temp2, temp);
        cycles -= (mod == 3) ? 6 : 10;
        return 0;
}
static int opCMPXCHG_w_a32(uint32_t fetchdat)
{
        uint16_t temp, temp2 = AX;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_32(fetchdat);
        temp = geteaw();                        if (abrt) return 0;
        if (AX == temp) seteaw(regs[reg].w);
        else            AX = temp;
        if (abrt) return 0;
        setsub16(temp2, temp);
        cycles -= (mod == 3) ? 6 : 10;
        return 0;
}

static int opCMPXCHG_l_a16(uint32_t fetchdat)
{
        uint32_t temp, temp2 = EAX;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_16(fetchdat);
        temp = geteal();                        if (abrt) return 0;
        if (EAX == temp) seteal(regs[reg].l);
        else             EAX = temp;
        if (abrt) return 0;
        setsub32(temp2, temp);
        cycles -= (mod == 3) ? 6 : 10;
        return 0;
}
static int opCMPXCHG_l_a32(uint32_t fetchdat)
{
        uint32_t temp, temp2 = EAX;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_32(fetchdat);
        temp = geteal();                        if (abrt) return 0;
        if (EAX == temp) seteal(regs[reg].l);
        else             EAX = temp;
        if (abrt) return 0;
        setsub32(temp2, temp);
        cycles -= (mod == 3) ? 6 : 10;
        return 0;
}

static int opXADD_b_a16(uint32_t fetchdat)
{
        uint8_t temp;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_16(fetchdat);
        temp = geteab();                        if (abrt) return 0;
        seteab(temp + getr8(reg));              if (abrt) return 0;
        setadd8(temp, getr8(reg));
        setr8(reg, temp);
        return 0;
}
static int opXADD_b_a32(uint32_t fetchdat)
{
        uint8_t temp;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_32(fetchdat);
        temp = geteab();                        if (abrt) return 0;
        seteab(temp + getr8(reg));              if (abrt) return 0;
        setadd8(temp, getr8(reg));
        setr8(reg, temp);
        return 0;
}

static int opXADD_w_a16(uint32_t fetchdat)
{
        uint16_t temp;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_16(fetchdat);
        temp = geteaw();                        if (abrt) return 0;
        seteaw(temp + regs[reg].w);             if (abrt) return 0;
        setadd16(temp, regs[reg].w);
        regs[reg].w = temp;
        return 0;
}
static int opXADD_w_a32(uint32_t fetchdat)
{
        uint16_t temp;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_32(fetchdat);
        temp = geteaw();                        if (abrt) return 0;
        seteaw(temp + regs[reg].w);             if (abrt) return 0;
        setadd16(temp, regs[reg].w);
        regs[reg].w = temp;
        return 0;
}

static int opXADD_l_a16(uint32_t fetchdat)
{
        uint32_t temp;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_16(fetchdat);
        temp = geteal();                        if (abrt) return 0;
        seteal(temp + regs[reg].l);             if (abrt) return 0;
        setadd32(temp, regs[reg].l);
        regs[reg].l = temp;
        return 0;
}
static int opXADD_l_a32(uint32_t fetchdat)
{
        uint32_t temp;
        if (!is486)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        fetch_ea_32(fetchdat);
        temp = geteal();                        if (abrt) return 0;
        seteal(temp + regs[reg].l);             if (abrt) return 0;
        setadd32(temp, regs[reg].l);
        regs[reg].l = temp;
        return 0;
}
