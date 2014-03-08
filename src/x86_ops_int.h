static int opINT3(uint32_t fetchdat)
{
        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 0;
        }
        x86_int_sw(3);
        cycles -= (is486) ? 44 : 59;
        return 0;
}

static int opINT(uint32_t fetchdat)
{
        uint8_t temp;
        
        /*if (msw&1) pclog("INT %i %i %i\n",cr0&1,eflags&VM_FLAG,IOPL);*/
        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 0;
        }
        temp = getbytef();
        if (temp == 0x10 && AH == 0xe) pclog("INT %02X : %04X %04X %04X %04X   %c   %04X:%04X\n", temp, AX, BX, CX, DX, (AL < 32) ? ' ' : AL, CS, pc);
        x86_int_sw(temp);
        return 0;
}

static int opINTO(uint32_t fetchdat)
{
        if ((cr0 & 1) && (eflags & VM_FLAG) && (IOPL != 3))
        {
                x86gpf(NULL,0);
                return 0;
        }
        if (VF_SET())
        {
                oldpc = pc;
                x86_int_sw(4);
        }
        cycles -= 3;
        return 0;
}

