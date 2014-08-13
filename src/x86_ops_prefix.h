static int op_CS(uint32_t fetchdat)
{
        ea_seg = &_cs;
        ssegs = 1;
        cycles -= 4;
        return 1;
}
static int op_DS(uint32_t fetchdat)
{
        ea_seg = &_ds;
        ssegs = 1;
        cycles -= 4;
        return 1;
}
static int op_ES(uint32_t fetchdat)
{
        ea_seg = &_es;
        ssegs = 1;
        cycles -= 4;
        return 1;
}      
static int op_FS(uint32_t fetchdat)
{
        ea_seg = &_fs;
        ssegs = 1;
        cycles -= 4;
        return 1;
}
static int op_GS(uint32_t fetchdat)
{
        ea_seg = &_gs;
        ssegs = 1;
        cycles -= 4;
        return 1;
}
static int op_SS(uint32_t fetchdat)
{
        ea_seg = &_ss;
        ssegs = 1;
        cycles -= 4;
        return 1;
}


static int op_66(uint32_t fetchdat) /*Data size select*/
{
        op32 = ((use32 & 0x100) ^ 0x100) | (op32 & 0x200);
        cycles -= 2;
        return 1;
}
static int op_67(uint32_t fetchdat) /*Address size select*/
{
        op32 = ((use32 & 0x200) ^ 0x200) | (op32 & 0x100);
        cycles -= 2;
        return 1;
}
