static int opCBW(uint32_t fetchdat)
{
        AH = (AL & 0x80) ? 0xff : 0;
        cycles -= 3;
        return 0;
}
static int opCWDE(uint32_t fetchdat)
{
        EAX = (AX & 0x8000) ? (0xffff0000 | AX) : AX;
        cycles -= 3;
        return 0;
}
static int opCWD(uint32_t fetchdat)
{
        DX = (AX & 0x8000) ? 0xFFFF : 0;
        cycles -= 2;
        return 0;
}
static int opCDQ(uint32_t fetchdat)
{
        EDX = (EAX & 0x80000000) ? 0xffffffff : 0;
        cycles -= 2;
        return 0;
}

static int opNOP(uint32_t fetchdat)
{
        cycles -= (is486) ? 1 : 3;
        return 0;
}

static int opSETALC(uint32_t fetchdat)
{
        AL = (CF_SET()) ? 0xff : 0;
        cycles -= timing_rr;
        return 0;
}



static int opF6_a16(uint32_t fetchdat)
{
        int tempws, tempws2;
        uint16_t tempw, src16;
        uint8_t src, dst;
        int8_t temps;
        
        fetch_ea_16(fetchdat);
        dst = geteab();                 if (abrt) return 0;
        switch (rmdat & 0x38)
        {
                case 0x00: /*TEST b,#8*/
                src = readmemb(cs, pc); pc++;           if (abrt) return 0;
                setznp8(src & dst);
                if (is486) cycles -= ((mod == 3) ? 1 : 2);
                else       cycles -= ((mod == 3) ? 2 : 5);
                break;
                case 0x10: /*NOT b*/
                seteab(~dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x18: /*NEG b*/
                setsub8(0, dst);
                seteab(0 - dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x20: /*MUL AL,b*/
                AX = AL * dst;
                flags_rebuild();
                if (AH) flags |=  (C_FLAG | V_FLAG);
                else    flags &= ~(C_FLAG | V_FLAG);
                cycles -= 13;
                break;
                case 0x28: /*IMUL AL,b*/
                tempws = (int)((int8_t)AL) * (int)((int8_t)dst);
                AX = tempws & 0xffff;
                flags_rebuild();
                if (AH && AH != 0xff) flags |=  (C_FLAG | V_FLAG);
                else                  flags &= ~(C_FLAG | V_FLAG);
                cycles -= 14;
                break;
                case 0x30: /*DIV AL,b*/
                src16 = AX;
                if (dst) tempw = src16 / dst;
                if (dst && !(tempw & 0xff00))
                {
                        AH = src16 % dst;
                        AL = (src16 / dst) &0xff;
                        if (!cpu_iscyrix) 
                        {
                                flags_rebuild();
                                flags |= 0x8D5; /*Not a Cyrix*/
                        }
                }
                else
                {
                        x86_int(0);
                }
                cycles -= is486 ? 16 : 14;
                break;
                case 0x38: /*IDIV AL,b*/
                tempws = (int)(int16_t)AX;
                if (dst != 0) tempws2 = tempws / (int)((int8_t)dst);
                temps = tempws2 & 0xff;
                if (dst && ((int)temps == tempws2))
                {
                        AH = (tempws % (int)((int8_t)dst)) & 0xff;
                        AL = tempws2 & 0xff;
                        if (!cpu_iscyrix) 
                        {
                                flags_rebuild();
                                flags|=0x8D5; /*Not a Cyrix*/
                        }
                }
                else
                {
//                        pclog("IDIVb exception - %X / %08X = %X\n", tempws, dst, tempws2);
                        x86_int(0);
                }
                cycles -= 19;
                break;

                default:
                pclog("Bad F6 opcode %02X\n", rmdat & 0x38);
                x86illegal();
        }
        return 0;
}
static int opF6_a32(uint32_t fetchdat)
{
        int tempws, tempws2;
        uint16_t tempw, src16;
        uint8_t src, dst;
        int8_t temps;
        
        fetch_ea_32(fetchdat);
        dst = geteab();                 if (abrt) return 0;
        switch (rmdat & 0x38)
        {
                case 0x00: /*TEST b,#8*/
                src = readmemb(cs, pc); pc++;           if (abrt) return 0;
                setznp8(src & dst);
                if (is486) cycles -= ((mod == 3) ? 1 : 2);
                else       cycles -= ((mod == 3) ? 2 : 5);
                break;
                case 0x10: /*NOT b*/
                seteab(~dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x18: /*NEG b*/
                setsub8(0, dst);
                seteab(0 - dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x20: /*MUL AL,b*/
                AX = AL * dst;
                flags_rebuild();
                if (AH) flags |=  (C_FLAG | V_FLAG);
                else    flags &= ~(C_FLAG | V_FLAG);
                cycles -= 13;
                break;
                case 0x28: /*IMUL AL,b*/
                tempws = (int)((int8_t)AL) * (int)((int8_t)dst);
                AX = tempws & 0xffff;
                flags_rebuild();
                if (AH && AH != 0xff) flags |=  (C_FLAG | V_FLAG);
                else                  flags &= ~(C_FLAG | V_FLAG);
                cycles -= 14;
                break;
                case 0x30: /*DIV AL,b*/
                src16 = AX;
                if (dst) tempw = src16 / dst;
                if (dst && !(tempw & 0xff00))
                {
                        AH = src16 % dst;
                        AL = (src16 / dst) &0xff;
                        if (!cpu_iscyrix) 
                        {
                                flags_rebuild();
                                flags |= 0x8D5; /*Not a Cyrix*/
                        }
                }
                else
                {
                        x86_int(0);
                }
                cycles -= is486 ? 16 : 14;
                break;
                case 0x38: /*IDIV AL,b*/
                tempws = (int)(int16_t)AX;
                if (dst != 0) tempws2 = tempws / (int)((int8_t)dst);
                temps = tempws2 & 0xff;
                if (dst && ((int)temps == tempws2))
                {
                        AH = (tempws % (int)((int8_t)dst)) & 0xff;
                        AL = tempws2 & 0xff;
                        if (!cpu_iscyrix) 
                        {
                                flags_rebuild();
                                flags|=0x8D5; /*Not a Cyrix*/
                        }
                }
                else
                {
//                        pclog("IDIVb exception - %X / %08X = %X\n", tempws, dst, tempws2);
                        x86_int(0);
                }
                cycles -= 19;
                break;

                default:
                pclog("Bad F6 opcode %02X\n", rmdat & 0x38);
                x86illegal();
        }
        return 0;
}



static int opF7_w_a16(uint32_t fetchdat)
{
        uint32_t templ, templ2;
        int tempws, tempws2;
        int16_t temps16;
        uint16_t src, dst;
        
        fetch_ea_16(fetchdat);
        dst = geteaw();        if (abrt) return 0;
        switch (rmdat & 0x38)
        {
                case 0x00: /*TEST w*/
                src = getword();        if (abrt) return 0;
                setznp16(src & dst);
                if (is486) cycles -= ((mod == 3) ? 1 : 2);
                else       cycles -= ((mod == 3) ? 2 : 5);
                break;
                case 0x10: /*NOT w*/
                seteaw(~dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x18: /*NEG w*/
                setsub16(0, dst);
                seteaw(0 - dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x20: /*MUL AX,w*/
                templ = AX * dst;
                AX = templ & 0xFFFF;
                DX = templ >> 16;
                flags_rebuild();
                if (DX)    flags |=  (C_FLAG | V_FLAG);
                else       flags &= ~(C_FLAG | V_FLAG);
                cycles -= 21;
                break;
                case 0x28: /*IMUL AX,w*/
                templ = (int)((int16_t)AX) * (int)((int16_t)dst);
                AX = templ & 0xFFFF;
                DX = templ >> 16;
                flags_rebuild();
                if (DX && DX != 0xFFFF) flags |=  (C_FLAG | V_FLAG);
                else                    flags &= ~(C_FLAG | V_FLAG);
                cycles -= 22;
                break;
                case 0x30: /*DIV AX,w*/
                templ = (DX << 16) | AX;
                if (dst) templ2 = templ / dst;
                if (dst && !(templ2 & 0xffff0000))
                {
                        DX = templ % dst;
                        AX = (templ / dst) & 0xffff;
                        if (!cpu_iscyrix) setznp16(AX); /*Not a Cyrix*/                                                
                }
                else
                {
//                        fatal("DIVw BY 0 %04X:%04X %i\n",cs>>4,pc,ins);
                        x86_int(0);
                }
                cycles -=  is486 ? 24 : 22;
                break;
                case 0x38: /*IDIV AX,w*/
                tempws = (int)((DX << 16)|AX);
                if (dst) tempws2 = tempws / (int)((int16_t)dst);
                temps16 = tempws2 & 0xffff;
                if ((dst != 0) && ((int)temps16 == tempws2))
                {
                        DX = tempws % (int)((int16_t)dst);
                        AX = tempws2 & 0xffff;
                        if (!cpu_iscyrix) setznp16(AX); /*Not a Cyrix*/
                }
                else
                {
//                        pclog("IDIVw exception - %X / %08X = %X\n",tempws, dst, tempws2);
                        x86_int(0);
                }
                cycles -= 27;
                break;

                default:
                pclog("Bad F7 opcode %02X\n", rmdat & 0x38);
                x86illegal();
        }
        return 0;
}
static int opF7_w_a32(uint32_t fetchdat)
{
        uint32_t templ, templ2;
        int tempws, tempws2;
        int16_t temps16;
        uint16_t src, dst;

        fetch_ea_32(fetchdat);
        dst = geteaw();        if (abrt) return 0;
        switch (rmdat & 0x38)
        {
                case 0x00: /*TEST w*/
                src = getword();        if (abrt) return 0;
                setznp16(src & dst);
                if (is486) cycles -= ((mod == 3) ? 1 : 2);
                else       cycles -= ((mod == 3) ? 2 : 5);
                break;
                case 0x10: /*NOT w*/
                seteaw(~dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x18: /*NEG w*/
                setsub16(0, dst);
                seteaw(0 - dst);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x20: /*MUL AX,w*/
                templ = AX * dst;
                AX = templ & 0xFFFF;
                DX = templ >> 16;
                flags_rebuild();
                if (DX)    flags |=  (C_FLAG | V_FLAG);
                else       flags &= ~(C_FLAG | V_FLAG);
                cycles -= 21;
                break;
                case 0x28: /*IMUL AX,w*/
                templ = (int)((int16_t)AX) * (int)((int16_t)dst);
                AX = templ & 0xFFFF;
                DX = templ >> 16;
                flags_rebuild();
                if (DX && DX != 0xFFFF) flags |=  (C_FLAG | V_FLAG);
                else                    flags &= ~(C_FLAG | V_FLAG);
                cycles -= 22;
                break;
                case 0x30: /*DIV AX,w*/
                templ = (DX << 16) | AX;
                if (dst) templ2 = templ / dst;
                if (dst && !(templ2 & 0xffff0000))
                {
                        DX = templ % dst;
                        AX = (templ / dst) & 0xffff;
                        if (!cpu_iscyrix) setznp16(AX); /*Not a Cyrix*/                                                
                }
                else
                {
//                        fatal("DIVw BY 0 %04X:%04X %i\n",cs>>4,pc,ins);
                        x86_int(0);
                }
                cycles -=  is486 ? 24 : 22;
                break;
                case 0x38: /*IDIV AX,w*/
                tempws = (int)((DX << 16)|AX);
                if (dst) tempws2 = tempws / (int)((int16_t)dst);
                temps16 = tempws2 & 0xffff;
                if ((dst != 0) && ((int)temps16 == tempws2))
                {
                        DX = tempws % (int)((int16_t)dst);
                        AX = tempws2 & 0xffff;
                        if (!cpu_iscyrix) setznp16(AX); /*Not a Cyrix*/
                }
                else
                {
//                        pclog("IDIVw exception - %X / %08X = %X\n", tempws, dst, tempws2);
                        x86_int(0);
                }
                cycles -= 27;
                break;

                default:
                pclog("Bad F7 opcode %02X\n", rmdat & 0x38);
                x86illegal();
        }
        return 0;
}

static int opF7_l_a16(uint32_t fetchdat)
{
        uint64_t temp64;
        uint32_t src, dst;

        fetch_ea_16(fetchdat);
        dst = geteal();                 if (abrt) return 0;

        switch (rmdat & 0x38)
        {
                case 0x00: /*TEST l*/
                src = getlong();        if (abrt) return 0;
                setznp32(src & dst);
                if (is486) cycles -= ((mod == 3) ? 1 : 2);
                else       cycles -= ((mod == 3) ? 2 : 5);
                break;
                case 0x10: /*NOT l*/
                seteal(~dst);
                cycles -= (mod == 3) ? timing_rr : timing_mml;
                break;
                case 0x18: /*NEG l*/
                setsub32(0, dst);
                seteal(0 - dst);
                cycles -= (mod == 3) ? timing_rr : timing_mml;
                break;
                case 0x20: /*MUL EAX,l*/
                temp64 = (uint64_t)EAX * (uint64_t)dst;
                EAX = temp64 & 0xffffffff;
                EDX = temp64 >> 32;
                flags_rebuild();
                if (EDX) flags |=  (C_FLAG|V_FLAG);
                else     flags &= ~(C_FLAG|V_FLAG);
                cycles -= 21;
                break;
                case 0x28: /*IMUL EAX,l*/
                temp64 = (int64_t)(int32_t)EAX * (int64_t)(int32_t)dst;
                EAX = temp64 & 0xffffffff;
                EDX = temp64 >> 32;
                flags_rebuild();
                if (EDX && EDX != 0xffffffff) flags |=  (C_FLAG|V_FLAG);
                else                          flags &= ~(C_FLAG|V_FLAG);
                cycles -= 38;
                break;
                case 0x30: /*DIV EAX,l*/
                divl(dst);
                if (!cpu_iscyrix) setznp32(EAX); /*Not a Cyrix*/
                cycles -= (is486) ? 40 : 38;
                break;
                case 0x38: /*IDIV EAX,l*/
                idivl((int32_t)dst);
                if (!cpu_iscyrix) setznp32(EAX); /*Not a Cyrix*/
                cycles -= 43;
                break;

                default:
                pclog("Bad F7 opcode %02X\n", rmdat & 0x38);
                x86illegal();
        }
        return 0;
}
static int opF7_l_a32(uint32_t fetchdat)
{
        uint64_t temp64;
        uint32_t src, dst;

        fetch_ea_32(fetchdat);
        dst = geteal();                 if (abrt) return 0;

        switch (rmdat & 0x38)
        {
                case 0x00: /*TEST l*/
                src = getlong();        if (abrt) return 0;
                setznp32(src & dst);
                if (is486) cycles -= ((mod == 3) ? 1 : 2);
                else       cycles -= ((mod == 3) ? 2 : 5);
                break;
                case 0x10: /*NOT l*/
                seteal(~dst);
                cycles -= (mod == 3) ? timing_rr : timing_mml;
                break;
                case 0x18: /*NEG l*/
                setsub32(0, dst);
                seteal(0 - dst);
                cycles -= (mod == 3) ? timing_rr : timing_mml;
                break;
                case 0x20: /*MUL EAX,l*/
                temp64 = (uint64_t)EAX * (uint64_t)dst;
                EAX = temp64 & 0xffffffff;
                EDX = temp64 >> 32;
                flags_rebuild();
                if (EDX) flags |=  (C_FLAG|V_FLAG);
                else     flags &= ~(C_FLAG|V_FLAG);
                cycles -= 21;
                break;
                case 0x28: /*IMUL EAX,l*/
                temp64 = (int64_t)(int32_t)EAX * (int64_t)(int32_t)dst;
                EAX = temp64 & 0xffffffff;
                EDX = temp64 >> 32;
                flags_rebuild();
                if (EDX && EDX != 0xffffffff) flags |=  (C_FLAG|V_FLAG);
                else                          flags &= ~(C_FLAG|V_FLAG);
                cycles -= 38;
                break;
                case 0x30: /*DIV EAX,l*/
                divl(dst);
                if (!cpu_iscyrix) setznp32(EAX); /*Not a Cyrix*/
                cycles -= (is486) ? 40 : 38;
                break;
                case 0x38: /*IDIV EAX,l*/
                idivl((int32_t)dst);
                if (!cpu_iscyrix) setznp32(EAX); /*Not a Cyrix*/
                cycles -= 43;
                break;

                default:
                pclog("Bad F7 opcode %02X\n", rmdat & 0x38);
                x86illegal();
        }
        return 0;
}


static int opHLT(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                x86gpf(NULL,0);
                return 0;
        }
        if (!((flags&I_FLAG) && pic_intpending && !ssegs)) 
        {
                cycles -= 100;
                pc--;
        }
        else
                cycles -= 5;
        return 0;
}


static int opLOCK(uint32_t fetchdat)
{
        cycles -= 4;
        return 0;
}



static int opBOUND_w_a16(uint32_t fetchdat)
{       
        int16_t low, high;
        
        fetch_ea_16(fetchdat);
        low = geteaw();
        high = readmemw(easeg, eaaddr + 2);     if (abrt) return 0;
        
        if (((int16_t)regs[reg].w < low) || ((int16_t)regs[reg].w > high))
        {
                x86_int(5);
        }
        
        cycles -= is486 ? 7 : 10;
        return 0;
}
static int opBOUND_w_a32(uint32_t fetchdat)
{       
        int16_t low, high;
        
        fetch_ea_32(fetchdat);
        low = geteaw();
        high = readmemw(easeg, eaaddr + 2);     if (abrt) return 0;
        
        if (((int16_t)regs[reg].w < low) || ((int16_t)regs[reg].w > high))
        {
                x86_int(5);
        }
        
        cycles -= is486 ? 7 : 10;
        return 0;
}

static int opBOUND_l_a16(uint32_t fetchdat)
{       
        int32_t low, high;
        
        fetch_ea_16(fetchdat);
        low = geteal();
        high = readmeml(easeg, eaaddr + 4);     if (abrt) return 0;
        
        if (((int32_t)regs[reg].l < low) || ((int32_t)regs[reg].l > high))
        {
                x86_int(5);
        }
        
        cycles -= is486 ? 7 : 10;
        return 0;
}
static int opBOUND_l_a32(uint32_t fetchdat)
{       
        int32_t low, high;
        
        fetch_ea_32(fetchdat);
        low = geteal();
        high = readmeml(easeg, eaaddr + 4);     if (abrt) return 0;
        
        if (((int32_t)regs[reg].l < low) || ((int32_t)regs[reg].l > high))
        {
                x86_int(5);
        }
        
        cycles -= is486 ? 7 : 10;
        return 0;
}


static int opCLTS(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't CLTS\n");
                x86gpf(NULL,0);
                return 0;
        }
        cr0 &= ~8;
        cycles -= 5;
        return 0;
}

static int opINVD(uint32_t fetchdat)
{
        if (!is486) 
        {
                x86illegal();
                return 0;
        }
        cycles -= 1000;
        return 0;
}
static int opWBINVD(uint32_t fetchdat)
{
        if (!is486) 
        {
                x86illegal();
                return 0;
        }
        cycles -= 10000;
        return 0;
}



static int opLOADALL(uint32_t fetchdat)
{
        flags = (readmemw(0, 0x818) & 0xffd5) | 2;
        flags_extract();
        pc = readmemw(0, 0x81A);
        DS = readmemw(0, 0x81E);
        SS = readmemw(0, 0x820);
        CS = readmemw(0, 0x822);
        ES = readmemw(0, 0x824);
        DI = readmemw(0, 0x826);
        SI = readmemw(0, 0x828);
        BP = readmemw(0, 0x82A);
        SP = readmemw(0, 0x82C);
        BX = readmemw(0, 0x82E);
        DX = readmemw(0, 0x830);
        CX = readmemw(0, 0x832);
        AX = readmemw(0, 0x834);
        es = readmemw(0, 0x836) | (readmemb(0, 0x838) << 16);
        cs = readmemw(0, 0x83C) | (readmemb(0, 0x83E) << 16);
        ss = readmemw(0, 0x842) | (readmemb(0, 0x844) << 16);
        ds = readmemw(0, 0x848) | (readmemb(0, 0x84A) << 16);
        cycles-=195;
        return 0;
}                                

static int opCPUID(uint32_t fetchdat)
{
        if (CPUID)
        {
                cpu_CPUID();
                cycles -= 9;
                return 0;
        }
        pc = oldpc;
        x86illegal();
        return 0;
}

static int opRDMSR(uint32_t fetchdat)
{
        if (cpu_hasMSR)
        {
                cpu_RDMSR();
                cycles -= 9;
                return 0;
        }
        pc = oldpc;
        x86illegal();
        return 0;
}

static int opWRMSR(uint32_t fetchdat)
{
        if (cpu_hasMSR)
        {
                cpu_WRMSR();
                cycles -= 9;
                return 0;
        }
        pc = oldpc;
        x86illegal();
        return 0;
}

