static int op_CS(uint32_t fetchdat)
{
        oldss = ss;
        oldds = ds;
        ds = ss = cs;
        rds = CS;
        ssegs = 2;
        cycles -= 4;
        return 1;
}
static int op_DS(uint32_t fetchdat)
{
        oldss = ss;
        oldds = ds;
        ds = ss = ds;
        ssegs = 2;
        cycles -= 4;
        return 1;
}
static int op_ES(uint32_t fetchdat)
{
        oldss = ss;
        oldds = ds;
        ds = ss = es;
        rds = ES;
        ssegs = 2;
        cycles -= 4;
        return 1;
}      
static int op_FS(uint32_t fetchdat)
{
        oldss = ss;
        oldds = ds;
        rds = FS;
        ds = ss = fs;
        ssegs = 2;
        cycles -= 4;
        return 1;
}
static int op_GS(uint32_t fetchdat)
{
        oldss = ss;
        oldds = ds;
        rds = GS;
        ds = ss = gs;
        ssegs = 2;
        cycles -= 4;
        return 1;
}
static int op_SS(uint32_t fetchdat)
{
        oldss = ss;
        oldds = ds;
        ds = ss = ss;
        rds = SS;
        ssegs = 2;
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
