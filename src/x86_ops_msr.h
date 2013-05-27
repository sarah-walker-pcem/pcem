static int opRDTSC(uint32_t fetchdat)
{
        if (!cpu_hasrdtsc)
        {
                pc = oldpc;
                x86illegal();
                return 0;
        }
        EAX = tsc & 0xffffffff;
        EDX = tsc >> 32;
        cycles--;
        return 0;
}
