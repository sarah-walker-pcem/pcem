static int opMOV_r_CRx_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from CRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        switch (reg)
        {
                case 0:
                regs[rm].l = cr0;
                if (is486) regs[rm].l |= 0x10; /*ET hardwired on 486*/
                break;
                case 2:
                regs[rm].l = cr2;
                break;
                case 3:
                regs[rm].l = cr3;
                break;
                default:
                pclog("Bad read of CR%i %i\n",rmdat&7,reg);
                pc = oldpc;
                x86illegal();
                break;
        }
        cycles -= 6;
        return 0;
}
static int opMOV_r_CRx_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from CRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_32(fetchdat);
        switch (reg)
        {
                case 0:
                regs[rm].l = cr0;
                if (is486) regs[rm].l |= 0x10; /*ET hardwired on 486*/
                break;
                case 2:
                regs[rm].l = cr2;
                break;
                case 3:
                regs[rm].l = cr3;
                break;
                default:
                pclog("Bad read of CR%i %i\n",rmdat&7,reg);
                pc = oldpc;
                x86illegal();
                break;
        }
        cycles -= 6;
        return 0;
}

static int opMOV_r_DRx_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from DRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        regs[rm].l = 0;
        cycles -= 6;
        return 0;
}
static int opMOV_r_DRx_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from DRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_32(fetchdat);
        regs[rm].l = 0;
        cycles -= 6;
        return 0;
}

static int opMOV_CRx_r_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load CRx\n");
                x86gpf(NULL,0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        switch (reg)
        {
                case 0:
                if ((regs[rm].l ^ cr0) & 0x80000001) flushmmucache();
                cr0 = regs[rm].l;
                if (cpu_16bitbus) cr0 |= 0x10;
                if (!(cr0 & 0x80000000)) mmu_perm=4;
                break;
                case 2:
                cr2 = regs[rm].l;
                break;
                case 3:
                cr3 = regs[rm].l;
                flushmmucache();
                break;
                default:
                pclog("Bad load CR%i\n", reg);
                pc = oldpc;
                x86illegal();
                break;
        }
        cycles -= 10;
        return 0;
}
static int opMOV_CRx_r_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load CRx\n");
                x86gpf(NULL,0);
                return 0;
        }
        fetch_ea_32(fetchdat);
        switch (reg)
        {
                case 0:
                if ((regs[rm].l ^ cr0) & 0x80000001) flushmmucache();
                cr0 = regs[rm].l;
                if (cpu_16bitbus) cr0 |= 0x10;
                if (!(cr0 & 0x80000000)) mmu_perm=4;
                break;
                case 2:
                cr2 = regs[rm].l;
                break;
                case 3:
                cr3 = regs[rm].l;
                flushmmucache();
                break;
                default:
                pclog("Bad load CR%i\n", reg);
                pc = oldpc;
                x86illegal();
                break;
        }
        cycles -= 10;
        return 0;
}

static int opMOV_DRx_r_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load DRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        cycles -= 6;
        return 0;
}
static int opMOV_DRx_r_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load DRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        cycles -= 6;
        return 0;
}

static int opMOV_r_TRx_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from TRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        regs[rm].l = 0;
        cycles -= 6;
        return 0;
}
static int opMOV_r_TRx_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from TRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_32(fetchdat);
        regs[rm].l = 0;
        cycles -= 6;
        return 0;
}

static int opMOV_TRx_r_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load TRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        cycles -= 6;
        return 0;
}
static int opMOV_TRx_r_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load TRx\n");
                x86gpf(NULL, 0);
                return 0;
        }
        fetch_ea_16(fetchdat);
        cycles -= 6;
        return 0;
}

