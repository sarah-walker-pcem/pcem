static int opMOV_AL_imm(uint32_t fetchdat)
{
        AL = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_AH_imm(uint32_t fetchdat)
{
        AH = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_BL_imm(uint32_t fetchdat)
{
        BL = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_BH_imm(uint32_t fetchdat)
{
        BH = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_CL_imm(uint32_t fetchdat)
{
        CL = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_CH_imm(uint32_t fetchdat)
{
        CH = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_DL_imm(uint32_t fetchdat)
{
        DL = getbytef();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_DH_imm(uint32_t fetchdat)
{
        DH = getbytef();
        cycles -= timing_rr;
        return 0;
}

static int opMOV_AX_imm(uint32_t fetchdat)
{
        AX = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_BX_imm(uint32_t fetchdat)
{
        BX = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_CX_imm(uint32_t fetchdat)
{
        CX = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_DX_imm(uint32_t fetchdat)
{
        DX = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_SI_imm(uint32_t fetchdat)
{
        SI = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_DI_imm(uint32_t fetchdat)
{
        DI = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_BP_imm(uint32_t fetchdat)
{
        BP = getwordf();
        cycles -= timing_rr;
        return 0;
}
static int opMOV_SP_imm(uint32_t fetchdat)
{
        SP = getwordf();
        cycles -= timing_rr;
        return 0;
}

static int opMOV_EAX_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        EAX = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_EBX_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        EBX = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_ECX_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        ECX = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_EDX_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        EDX = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_ESI_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        ESI = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_EDI_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        EDI = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_EBP_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        EBP = templ;
        cycles -= timing_rr;
        return 0;
}
static int opMOV_ESP_imm(uint32_t fetchdat)
{
        uint32_t templ = getlong();     if (abrt) return 1;
        ESP = templ;
        cycles -= timing_rr;
        return 0;
}

static int opMOV_b_imm_a16(uint32_t fetchdat)
{
        uint8_t temp;
        fetch_ea_16(fetchdat);
        temp = readmemb(cs,pc); pc++;               if (abrt) return 1;
        seteab(temp);
        cycles -= timing_rr;
        return abrt;
}
static int opMOV_b_imm_a32(uint32_t fetchdat)
{
        uint8_t temp;
        fetch_ea_32(fetchdat);
        temp = getbyte();               if (abrt) return 1;
        seteab(temp);
        cycles -= timing_rr;
        return abrt;
}

static int opMOV_w_imm_a16(uint32_t fetchdat)
{
        uint16_t temp;
        fetch_ea_16(fetchdat);
        temp = getword();               if (abrt) return 1;
        seteaw(temp);
        cycles -= timing_rr;
        return abrt;
}
static int opMOV_w_imm_a32(uint32_t fetchdat)
{
        uint16_t temp;
        fetch_ea_32(fetchdat);
        temp = getword();               if (abrt) return 1;
        seteaw(temp);
        cycles -= timing_rr;
        return abrt;
}
static int opMOV_l_imm_a16(uint32_t fetchdat)
{
        uint32_t temp;
        fetch_ea_16(fetchdat);
        temp = getlong();               if (abrt) return 1;
        seteal(temp);
        cycles -= timing_rr;
        return abrt;
}
static int opMOV_l_imm_a32(uint32_t fetchdat)
{
        uint32_t temp;
        fetch_ea_32(fetchdat);
        temp = getlong();               if (abrt) return 1;
        seteal(temp);
        cycles -= timing_rr;
        return abrt;
}


static int opMOV_AL_a16(uint32_t fetchdat)
{
        uint16_t addr = getwordf();
        uint8_t temp = readmemb(ea_seg->base, addr);      if (abrt) return 1;
        AL = temp;
        cycles -= (is486) ? 1 : 4;
        return 0;        
}
static int opMOV_AL_a32(uint32_t fetchdat)
{
        uint32_t addr = getlong();
        uint8_t temp = readmemb(ea_seg->base, addr);      if (abrt) return 1;
        AL = temp;
        cycles -= (is486) ? 1 : 4;
        return 0;        
}
static int opMOV_AX_a16(uint32_t fetchdat)
{
        uint16_t addr = getwordf();
        uint16_t temp = readmemw(ea_seg->base, addr);     if (abrt) return 1;
        AX = temp;
        cycles -= (is486) ? 1 : 4;
        return 0;        
}
static int opMOV_AX_a32(uint32_t fetchdat)
{
        uint32_t addr = getlong();
        uint16_t temp = readmemw(ea_seg->base, addr);     if (abrt) return 1;
        AX = temp;
        cycles -= (is486) ? 1 : 4;
        return 0;        
}
static int opMOV_EAX_a16(uint32_t fetchdat)
{
        uint16_t addr = getwordf();
        uint32_t temp = readmeml(ea_seg->base, addr);     if (abrt) return 1;
        EAX = temp;
        cycles -= (is486) ? 1 : 4;
        return 0;        
}
static int opMOV_EAX_a32(uint32_t fetchdat)
{
        uint32_t addr = getlong();
        uint32_t temp = readmeml(ea_seg->base, addr);     if (abrt) return 1;
        EAX = temp;
        cycles -= (is486) ? 1 : 4;
        return 0;        
}

static int opMOV_a16_AL(uint32_t fetchdat)
{
        uint16_t addr = getwordf();
        writememb(ea_seg->base, addr, AL);
        cycles -= (is486) ? 1 : 2;
        return abrt;
}
static int opMOV_a32_AL(uint32_t fetchdat)
{
        uint32_t addr = getlong();
        writememb(ea_seg->base, addr, AL);
        cycles -= (is486) ? 1 : 2;
        return abrt;
}
static int opMOV_a16_AX(uint32_t fetchdat)
{
        uint16_t addr = getwordf();
        writememw(ea_seg->base, addr, AX);
        cycles -= (is486) ? 1 : 2;
        return abrt;
}
static int opMOV_a32_AX(uint32_t fetchdat)
{
        uint32_t addr = getlong();             if (abrt) return 1;
        writememw(ea_seg->base, addr, AX);
        cycles -= (is486) ? 1 : 2;
        return abrt;
}
static int opMOV_a16_EAX(uint32_t fetchdat)
{
        uint16_t addr = getwordf();
        writememl(ea_seg->base, addr, EAX);
        cycles -= (is486) ? 1 : 2;
        return abrt;
}
static int opMOV_a32_EAX(uint32_t fetchdat)
{
        uint32_t addr = getlong();             if (abrt) return 1;
        writememl(ea_seg->base, addr, EAX);
        cycles -= (is486) ? 1 : 2;
        return abrt;
}


static int opLEA_w_a16(uint32_t fetchdat)
{
        fetch_ea_16(fetchdat);
        ILLEGAL_ON(mod == 3);
        regs[reg].w = eaaddr;
        cycles -= timing_rr;
        return 0;
}
static int opLEA_w_a32(uint32_t fetchdat)
{
        fetch_ea_32(fetchdat);
        ILLEGAL_ON(mod == 3);
        regs[reg].w = eaaddr;
        cycles -= timing_rr;
        return 0;
}

static int opLEA_l_a16(uint32_t fetchdat)
{
        fetch_ea_16(fetchdat);
        ILLEGAL_ON(mod == 3);
        regs[reg].l = eaaddr & 0xffff;
        cycles -= timing_rr;
        return 0;
}
static int opLEA_l_a32(uint32_t fetchdat)
{
        fetch_ea_32(fetchdat);
        ILLEGAL_ON(mod == 3);
        regs[reg].l = eaaddr;
        cycles -= timing_rr;
        return 0;
}



static int opXLAT_a16(uint32_t fetchdat)
{
        uint32_t addr = (BX + AL)&0xFFFF;
        uint8_t temp = readmemb(ea_seg->base, addr); if (abrt) return 1;
        AL = temp;
        cycles -= 5;
        return 0;
}
static int opXLAT_a32(uint32_t fetchdat)
{
        uint32_t addr = EBX + AL;
        uint8_t temp = readmemb(ea_seg->base, addr); if (abrt) return 1;
        AL = temp;
        cycles -= 5;
        return 0;
}

static int opMOV_b_r_a16(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                setr8(fetchdat & 7, getr8((fetchdat >> 3) & 7));
                cycles -= timing_rr;
        }
        else
        {
                fetch_ea_16(fetchdat);
                CHECK_WRITE(ea_seg, eaaddr, eaaddr);
                seteab(getr8(reg));
                cycles -= is486 ? 1 : 2;
        }
        return abrt;
}
static int opMOV_b_r_a32(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                setr8(fetchdat & 7, getr8((fetchdat >> 3) & 7));
                cycles -= timing_rr;
        }
        else
        {
                fetch_ea_32(fetchdat);
                CHECK_WRITE(ea_seg, eaaddr, eaaddr);
                seteab(getr8(reg));
                cycles -= is486 ? 1 : 2;
        }
        return abrt;
}
static int opMOV_w_r_a16(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[fetchdat & 7].w = regs[(fetchdat >> 3) & 7].w;
                cycles -= timing_rr;
        }
        else
        { 
                fetch_ea_16(fetchdat);
                CHECK_WRITE(ea_seg, eaaddr, eaaddr+1);
                seteaw(regs[reg].w);
                cycles -= is486 ? 1 : 2;
        }
        return abrt;
}
static int opMOV_w_r_a32(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[fetchdat & 7].w = regs[(fetchdat >> 3) & 7].w;
                cycles -= timing_rr;
        }
        else
        { 
                fetch_ea_32(fetchdat);
                CHECK_WRITE(ea_seg, eaaddr, eaaddr+1);
                seteaw(regs[reg].w);
                cycles -= is486 ? 1 : 2;
        }
        return abrt;
}
static int opMOV_l_r_a16(uint32_t fetchdat)
{                       
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[fetchdat & 7].l = regs[(fetchdat >> 3) & 7].l;
                cycles -= timing_rr;
        }
        else
        {
                fetch_ea_16(fetchdat);
                CHECK_WRITE(ea_seg, eaaddr, eaaddr+3);
                seteal(regs[reg].l);
                cycles -= is486 ? 1 : 2;
        }
        return abrt;
}
static int opMOV_l_r_a32(uint32_t fetchdat)
{                       
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[fetchdat & 7].l = regs[(fetchdat >> 3) & 7].l;
                cycles -= timing_rr;
        }
        else
        {
                fetch_ea_32(fetchdat);
                CHECK_WRITE(ea_seg, eaaddr, eaaddr+3);
                seteal(regs[reg].l);
                cycles -= is486 ? 1 : 2;
        }
        return abrt;
}

static int opMOV_r_b_a16(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                setr8((fetchdat >> 3) & 7, getr8(fetchdat & 7));
                cycles -= timing_rr;
        }
        else
        {
                uint8_t temp;
                fetch_ea_16(fetchdat);
                CHECK_READ(ea_seg, eaaddr, eaaddr);
                temp = geteab();                if (abrt) return 1;
                setr8(reg, temp);
                cycles -= is486 ? 1 : 4;
        }
        return 0;
}
static int opMOV_r_b_a32(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                setr8((fetchdat >> 3) & 7, getr8(fetchdat & 7));
                cycles -= timing_rr;
        }
        else
        {
                uint8_t temp;
                fetch_ea_32(fetchdat);
                CHECK_READ(ea_seg, eaaddr, eaaddr);
                temp = geteab();                if (abrt) return 1;
                setr8(reg, temp);
                cycles -= is486 ? 1 : 4;
        }
        return 0;
}
static int opMOV_r_w_a16(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[(fetchdat >> 3) & 7].w = regs[fetchdat & 7].w;
                cycles -= timing_rr;
        }
        else
        {
                uint16_t temp;
                fetch_ea_16(fetchdat);
                CHECK_READ(ea_seg, eaaddr, eaaddr+1);
                temp = geteaw();                if (abrt) return 1;
                regs[reg].w = temp;
                cycles -= (is486) ? 1 : 4;
        }
        return 0;
}
static int opMOV_r_w_a32(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[(fetchdat >> 3) & 7].w = regs[fetchdat & 7].w;
                cycles -= timing_rr;
        }
        else
        {
                uint16_t temp;
                fetch_ea_32(fetchdat);
                CHECK_READ(ea_seg, eaaddr, eaaddr+1);
                temp = geteaw();                if (abrt) return 1;
                regs[reg].w = temp;
                cycles -= (is486) ? 1 : 4;
        }
        return 0;
}
static int opMOV_r_l_a16(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[(fetchdat >> 3) & 7].l = regs[fetchdat & 7].l;
                cycles -= timing_rr;
        }
        else
        {
                uint32_t temp;
                fetch_ea_16(fetchdat);
                CHECK_READ(ea_seg, eaaddr, eaaddr+3);
                temp = geteal();                if (abrt) return 1;
                regs[reg].l = temp;
                cycles -= is486 ? 1 : 4;
        }
        return 0;
}
static int opMOV_r_l_a32(uint32_t fetchdat)
{
        if ((uint8_t)fetchdat >= 0xc0)
        {
                pc++;
                regs[(fetchdat >> 3) & 7].l = regs[fetchdat & 7].l;
                cycles -= timing_rr;
        }
        else
        {
                uint32_t temp;
                fetch_ea_32(fetchdat);
                CHECK_READ(ea_seg, eaaddr, eaaddr+3);
                temp = geteal();                if (abrt) return 1;
                regs[reg].l = temp;
                cycles -= is486 ? 1 : 4;
        }
        return 0;
}
