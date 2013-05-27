static int opMOVSB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(ds,SI);         if (abrt) return 0;
        writememb(es, DI, temp);                if (abrt) return 0;
        if (flags & D_FLAG) { DI--; SI--; }
        else                { DI++; SI++; }
        cycles -= 7;
        return 0;
}
static int opMOVSB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(ds,ESI);        if (abrt) return 0;
        writememb(es, EDI, temp);               if (abrt) return 0;
        if (flags & D_FLAG) { EDI--; ESI--; }
        else                { EDI++; ESI++; }
        cycles -= 7;
        return 0;
}

static int opMOVSW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(ds,SI);        if (abrt) return 0;
        writememw(es, DI, temp);                if (abrt) return 0;
        if (flags & D_FLAG) { DI -= 2; SI -= 2; }
        else                { DI += 2; SI += 2; }
        cycles -= 7;
        return 0;
}
static int opMOVSW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(ds,ESI);       if (abrt) return 0;
        writememw(es, EDI, temp);               if (abrt) return 0;
        if (flags & D_FLAG) { EDI -= 2; ESI -= 2; }
        else                { EDI += 2; ESI += 2; }
        cycles -= 7;
        return 0;
}

static int opMOVSL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(ds,SI);        if (abrt) return 0;
        writememl(es, DI, temp);                if (abrt) return 0;
        if (flags & D_FLAG) { DI -= 4; SI -= 4; }
        else                { DI += 4; SI += 4; }
        cycles -= 7;
        return 0;
}
static int opMOVSL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(ds,ESI);       if (abrt) return 0;
        writememl(es, EDI, temp);               if (abrt) return 0;
        if (flags & D_FLAG) { EDI -= 4; ESI -= 4; }
        else                { EDI += 4; ESI += 4; }
        cycles -= 7;
        return 0;
}


static int opCMPSB_a16(uint32_t fetchdat)
{
        uint8_t src = readmemb(ds, SI);
        uint8_t dst = readmemb(es, DI);         if (abrt) return 0;
        setsub8(src, dst);
        if (flags & D_FLAG) { DI--; SI--; }
        else                { DI++; SI++; }
        cycles -= (is486) ? 8 : 10;
        return 0;
}
static int opCMPSB_a32(uint32_t fetchdat)
{
        uint8_t src = readmemb(ds, ESI);
        uint8_t dst = readmemb(es, EDI);        if (abrt) return 0;
        setsub8(src, dst);
        if (flags & D_FLAG) { EDI--; ESI--; }
        else                { EDI++; ESI++; }
        cycles -= (is486) ? 8 : 10;
        return 0;
}

static int opCMPSW_a16(uint32_t fetchdat)
{
        uint16_t src = readmemw(ds, SI);
        uint16_t dst = readmemw(es, DI);        if (abrt) return 0;
        setsub16(src, dst);
        if (flags & D_FLAG) { DI -= 2; SI -= 2; }
        else                { DI += 2; SI += 2; }
        cycles -= (is486) ? 8 : 10;
        return 0;
}
static int opCMPSW_a32(uint32_t fetchdat)
{
        uint16_t src = readmemw(ds, ESI);
        uint16_t dst = readmemw(es, EDI);        if (abrt) return 0;
        setsub16(src, dst);
        if (flags & D_FLAG) { EDI -= 2; ESI -= 2; }
        else                { EDI += 2; ESI += 2; }
        cycles -= (is486) ? 8 : 10;
        return 0;
}

static int opCMPSL_a16(uint32_t fetchdat)
{
        uint32_t src = readmeml(ds, SI);
        uint32_t dst = readmeml(es, DI);        if (abrt) return 0;
        setsub32(src, dst);
        if (flags & D_FLAG) { DI -= 4; SI -= 4; }
        else                { DI += 4; SI += 4; }
        cycles -= (is486) ? 8 : 10;
        return 0;
}
static int opCMPSL_a32(uint32_t fetchdat)
{
        uint32_t src = readmeml(ds, ESI);
        uint32_t dst = readmeml(es, EDI);        if (abrt) return 0;
        setsub32(src, dst);
        if (flags & D_FLAG) { EDI -= 4; ESI -= 4; }
        else                { EDI += 4; ESI += 4; }
        cycles -= (is486) ? 8 : 10;
        return 0;
}

static int opSTOSB_a16(uint32_t fetchdat)
{
        writememb(es, DI, AL);                  if (abrt) return 0;
        if (flags & D_FLAG) DI--;
        else                DI++;
        cycles -= 4;
        return 0;
}
static int opSTOSB_a32(uint32_t fetchdat)
{
        writememb(es, EDI, AL);                 if (abrt) return 0;
        if (flags & D_FLAG) EDI--;
        else                EDI++;
        cycles -= 4;
        return 0;
}

static int opSTOSW_a16(uint32_t fetchdat)
{
        writememw(es, DI, AX);                  if (abrt) return 0;
        if (flags & D_FLAG) DI -= 2;
        else                DI += 2;
        cycles -= 4;
        return 0;
}
static int opSTOSW_a32(uint32_t fetchdat)
{
        writememw(es, EDI, AX);                 if (abrt) return 0;
        if (flags & D_FLAG) EDI -= 2;
        else                EDI += 2;
        cycles -= 4;
        return 0;
}

static int opSTOSL_a16(uint32_t fetchdat)
{
        writememl(es, DI, EAX);                 if (abrt) return 0;
        if (flags & D_FLAG) DI -= 4;
        else                DI += 4;
        cycles -= 4;
        return 0;
}
static int opSTOSL_a32(uint32_t fetchdat)
{
        writememl(es, EDI, EAX);                if (abrt) return 0;
        if (flags & D_FLAG) EDI -= 4;
        else                EDI += 4;
        cycles -= 4;
        return 0;
}


static int opLODSB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(ds, SI);        if (abrt) return 0;
        AL = temp;
        if (flags & D_FLAG) SI--;
        else                SI++;
        cycles -= 5;
        return 0;
}
static int opLODSB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(ds, ESI);       if (abrt) return 0;
        AL = temp;
        if (flags & D_FLAG) ESI--;
        else                ESI++;
        cycles -= 5;
        return 0;
}

static int opLODSW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(ds, SI);       if (abrt) return 0;
        AX = temp;
        if (flags & D_FLAG) SI -= 2;
        else                SI += 2;
        cycles -= 5;
        return 0;
}
static int opLODSW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(ds, ESI);      if (abrt) return 0;
        AX = temp;
        if (flags & D_FLAG) ESI -= 2;
        else                ESI += 2;
        cycles -= 5;
        return 0;
}

static int opLODSL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(ds, SI);       if (abrt) return 0;
        EAX = temp;
        if (flags & D_FLAG) SI -= 4;
        else                SI += 4;
        cycles -= 5;
        return 0;
}
static int opLODSL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(ds, ESI);      if (abrt) return 0;
        EAX = temp;
        if (flags & D_FLAG) ESI -= 4;
        else                ESI += 4;
        cycles -= 5;
        return 0;
}


static int opSCASB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(es, DI);        if (abrt) return 0;
        setsub8(AL, temp);
        if (flags & D_FLAG) DI--;
        else                DI++;
        cycles -= 7;
        return 0;
}
static int opSCASB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(es, EDI);       if (abrt) return 0;
        setsub8(AL, temp);
        if (flags & D_FLAG) EDI--;
        else                EDI++;
        cycles -= 7;
        return 0;
}

static int opSCASW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(es, DI);       if (abrt) return 0;
        setsub16(AX, temp);
        if (flags & D_FLAG) DI -= 2;
        else                DI += 2;
        cycles -= 7;
        return 0;
}
static int opSCASW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(es, EDI);      if (abrt) return 0;
        setsub16(AX, temp);
        if (flags & D_FLAG) EDI -= 2;
        else                EDI += 2;
        cycles -= 7;
        return 0;
}

static int opSCASL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(es, DI);       if (abrt) return 0;
        setsub32(EAX, temp);
        if (flags & D_FLAG) DI -= 4;
        else                DI += 4;
        cycles -= 7;
        return 0;
}
static int opSCASL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(es, EDI);      if (abrt) return 0;
        setsub32(EAX, temp);
        if (flags & D_FLAG) EDI -= 4;
        else                EDI += 4;
        cycles -= 7;
        return 0;
}

static int opINSB_a16(uint32_t fetchdat)
{
        uint8_t temp;
        check_io_perm(DX);
        temp = inb(DX);
        writememb(es, DI, temp);                if (abrt) return 0;
        if (flags & D_FLAG) DI--;
        else                DI++;
        cycles -= 15;
        return 0;
}
static int opINSB_a32(uint32_t fetchdat)
{
        uint8_t temp;
        check_io_perm(DX);
        temp = inb(DX);
        writememb(es, EDI, temp);               if (abrt) return 0;
        if (flags & D_FLAG) EDI--;
        else                EDI++;
        cycles -= 15;
        return 0;
}

static int opINSW_a16(uint32_t fetchdat)
{
        uint16_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        temp = inw(DX);
        writememw(es, DI, temp);                if (abrt) return 0;
        if (flags & D_FLAG) DI -= 2;
        else                DI += 2;
        cycles -= 15;
        return 0;
}
static int opINSW_a32(uint32_t fetchdat)
{
        uint16_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        temp = inw(DX);
        writememw(es, EDI, temp);               if (abrt) return 0;
        if (flags & D_FLAG) EDI -= 2;
        else                EDI += 2;
        cycles -= 15;
        return 0;
}

static int opINSL_a16(uint32_t fetchdat)
{
        uint32_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        temp = inl(DX);
        writememl(es, DI, temp);                if (abrt) return 0;
        if (flags & D_FLAG) DI -= 4;
        else                DI += 4;
        cycles -= 15;
        return 0;
}
static int opINSL_a32(uint32_t fetchdat)
{
        uint32_t temp;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        temp = inl(DX);
        writememl(es, EDI, temp);               if (abrt) return 0;
        if (flags & D_FLAG) EDI -= 4;
        else                EDI += 4;
        cycles -= 15;
        return 0;
}

static int opOUTSB_a16(uint32_t fetchdat)
{
        uint8_t temp = readmemb(ds, SI);        if (abrt) return 0;
        check_io_perm(DX);
        if (flags & D_FLAG) SI--;
        else                SI++;
        outb(DX, temp);
        cycles -= 14;
        return 0;
}
static int opOUTSB_a32(uint32_t fetchdat)
{
        uint8_t temp = readmemb(ds, ESI);        if (abrt) return 0;
        check_io_perm(DX);
        if (flags & D_FLAG) ESI--;
        else                ESI++;
        outb(DX, temp);
        cycles -= 14;
        return 0;
}

static int opOUTSW_a16(uint32_t fetchdat)
{
        uint16_t temp = readmemw(ds, SI);       if (abrt) return 0;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        if (flags & D_FLAG) SI -= 2;
        else                SI += 2;
        outw(DX, temp);
        cycles -= 14;
        return 0;
}
static int opOUTSW_a32(uint32_t fetchdat)
{
        uint16_t temp = readmemw(ds, ESI);      if (abrt) return 0;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        if (flags & D_FLAG) ESI -= 2;
        else                ESI += 2;
        outw(DX, temp);
        cycles -= 14;
        return 0;
}

static int opOUTSL_a16(uint32_t fetchdat)
{
        uint32_t temp = readmeml(ds, SI);       if (abrt) return 0;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        if (flags & D_FLAG) SI -= 4;
        else                SI += 4;
        outl(EDX, temp);
        cycles -= 14;
        return 0;
}
static int opOUTSL_a32(uint32_t fetchdat)
{
        uint32_t temp = readmeml(ds, ESI);      if (abrt) return 0;
        check_io_perm(DX);
        check_io_perm(DX + 1);
        check_io_perm(DX + 2);
        check_io_perm(DX + 3);
        if (flags & D_FLAG) ESI -= 4;
        else                ESI += 4;
        outl(EDX, temp);
        cycles -= 14;
        return 0;
}
