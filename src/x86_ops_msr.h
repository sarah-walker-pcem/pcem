static int opRDTSC(uint32_t fetchdat)
{
        if (!cpu_hasrdtsc)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        if ((cr4 & CR4_TSD) && CPL)
        {
                x86gpf("RDTSC when TSD set and CPL != 0", 0);
                return 0;
        }
        EAX = tsc & 0xffffffff;
        EDX = tsc >> 32;
        cycles--;
        return 0;
}
