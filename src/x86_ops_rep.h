static int opREPNE(uint32_t fetchdat)
{
        rep386(0);
        return 0;
}
static int opREPE(uint32_t fetchdat)
{       
        rep386(1);
        return 0;
}

