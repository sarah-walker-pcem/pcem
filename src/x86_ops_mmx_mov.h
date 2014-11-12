static int opMOVD_l_mm_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (mod == 3)
        {
                MM[reg].l[0] = regs[rm].l;
                MM[reg].l[1] = 0;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t dst;
        
                dst = readmeml(easeg, eaaddr); if (abrt) return 1;
                MM[reg].l[0] = dst;
                MM[reg].l[1] = 0;

                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVD_l_mm_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (mod == 3)
        {
                MM[reg].l[0] = regs[rm].l;
                MM[reg].l[1] = 0;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t dst;
        
                dst = readmeml(easeg, eaaddr); if (abrt) return 1;
                MM[reg].l[0] = dst;
                MM[reg].l[1] = 0;

                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opMOVD_mm_l_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (mod == 3)
        {
                regs[rm].l = MM[reg].l[0];
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(ea_seg, eaaddr, eaaddr + 3);
                writememl(easeg, eaaddr, MM[reg].l[0]); if (abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVD_mm_l_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (mod == 3)
        {
                regs[rm].l = MM[reg].l[0];
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(ea_seg, eaaddr, eaaddr + 3);
                writememl(easeg, eaaddr, MM[reg].l[0]); if (abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opMOVQ_q_mm_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (mod == 3)
        {
                MM[reg].q = MM[rm].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t dst[2];
        
                dst[0] = readmeml(easeg, eaaddr);
                dst[1] = readmeml(easeg, eaaddr + 4); if (abrt) return 1;
                MM[reg].l[0] = dst[0];
                MM[reg].l[1] = dst[1];
                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVQ_q_mm_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (mod == 3)
        {
                MM[reg].q = MM[rm].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                uint32_t dst[2];
        
                dst[0] = readmeml(easeg, eaaddr);
                dst[1] = readmeml(easeg, eaaddr + 4); if (abrt) return 1;
                MM[reg].l[0] = dst[0];
                MM[reg].l[1] = dst[1];
                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opMOVQ_mm_q_a16(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_16(fetchdat);
        if (mod == 3)
        {
                MM[rm].q = MM[reg].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(ea_seg, eaaddr, eaaddr + 7);
                writememl(easeg, eaaddr,     MM[reg].l[0]);
                writememl(easeg, eaaddr + 4, MM[reg].l[1]); if (abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}
static int opMOVQ_mm_q_a32(uint32_t fetchdat)
{
        MMX_ENTER();
        
        fetch_ea_32(fetchdat);
        if (mod == 3)
        {
                MM[rm].q = MM[reg].q;
                CLOCK_CYCLES(1);
        }
        else
        {
                CHECK_WRITE(ea_seg, eaaddr, eaaddr + 7);
                writememl(easeg, eaaddr,     MM[reg].l[0]);
                writememl(easeg, eaaddr + 4, MM[reg].l[1]); if (abrt) return 1;
                CLOCK_CYCLES(2);
        }
        return 0;
}
