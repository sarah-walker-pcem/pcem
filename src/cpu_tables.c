#include "ibm.h"
#include "cpu.h"

/*Available cpuspeeds :
        0 = 16 MHz
        1 = 20 MHz
        2 = 25 MHz
        3 = 33 MHz
        4 = 40 MHz
        5 = 50 MHz
        6 = 66 MHz
        7 = 75 MHz
        8 = 80 MHz
        9 = 90 MHz
        10 = 100 MHz
        11 = 120 MHz
        12 = 133 MHz
        13 = 150 MHz
        14 = 160 MHz
        15 = 166 MHz
        16 = 180 MHz
        17 = 200 MHz
*/

FPU fpus_none[] =
{
        {"None", "none", FPU_NONE},
        {NULL, NULL, 0}
};
FPU fpus_8088[] =
{
        {"None", "none", FPU_NONE},
        {"8087", "8087", FPU_8087},
        {NULL, NULL, 0}
};
FPU fpus_80286[] =
{
        {"None",  "none", FPU_NONE},
        {"287",   "287",  FPU_287},
        {"287XL", "287xl",  FPU_287XL},
        {NULL, NULL, 0}
};
FPU fpus_80386[] =
{
        {"None", "none", FPU_NONE},
        {"387",  "387",  FPU_387},
        {NULL, NULL, 0}
};
FPU fpus_builtin[] =
{
        {"Built-in", "builtin", FPU_BUILTIN},
        {NULL, NULL, 0}
};

CPU cpus_8088[] =
{
        /*8088 standard*/
        {"8088/4.77",    CPU_8088,  fpus_8088, 0,  4772728,   1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8088/7.16",    CPU_8088,  fpus_8088, 1, 14318184/2, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8088/8",       CPU_8088,  fpus_8088, 1,  8000000,   1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8088/10",      CPU_8088,  fpus_8088, 2, 10000000,   1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8088/12",      CPU_8088,  fpus_8088, 3, 12000000,   1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8088/16",      CPU_8088,  fpus_8088, 4, 16000000,   1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_pcjr[] =
{
        /*8088 PCjr*/
        {"8088/4.77",    CPU_8088,  fpus_none, 0,  4772728, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_europc[] =
{
         /*8088 EuroPC*/
         {"8088/4.77",    CPU_8088,  fpus_8088, 0,  4772728,   1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
         {"8088/7.16",    CPU_8088,  fpus_8088, 1, 14318184/2, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
         {"8088/9.54",    CPU_8088,  fpus_8088, 1,  4772728*2, 1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
         {"",             -1,        0, 0, 0, 0}
};

CPU cpus_8086[] =
{
        /*8086 standard*/
        {"8086/7.16",    CPU_8086,  fpus_8088, 1, 14318184/2,     1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8086/8",       CPU_8086,  fpus_8088, 1,  8000000,       1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8086/9.54",    CPU_8086,  fpus_8088, 1,  4772728*2,     1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8086/10",      CPU_8086,  fpus_8088, 2, 10000000,       1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8086/12",      CPU_8086,  fpus_8088, 3, 12000000,       1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"8086/16",      CPU_8086,  fpus_8088, 4, 16000000,       1, 0, 0, 0, 0, 0, 0,0,0,0, 2},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_pc1512[] =
{
        /*8086 Amstrad*/
        {"8086/8",       CPU_8086,  fpus_8088, 1,  8000000,       1, 0, 0, 0, 0, 0, 0,0,0,0, 1},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_286[] =
{
        /*286*/
        {"286/6",        CPU_286,   fpus_80286, 0,  6000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1},
        {"286/8",        CPU_286,   fpus_80286, 1,  8000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1},
        {"286/10",       CPU_286,   fpus_80286, 2, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1},
        {"286/12",       CPU_286,   fpus_80286, 3, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2},
        {"286/16",       CPU_286,   fpus_80286, 4, 16000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2},
        {"286/20",       CPU_286,   fpus_80286, 5, 20000000, 1, 0, 0, 0, 0, 0, 4,4,4,4, 3},
        {"286/25",       CPU_286,   fpus_80286, 6, 25000000, 1, 0, 0, 0, 0, 0, 4,4,4,4, 3},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_super286tr[] =
{
        /*286*/
        {"286/12",       CPU_286,   fpus_80286, 3, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ibmat[] =
{
        /*286*/
        {"286/6",        CPU_286,   fpus_80286, 0,  6000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 1},
        {"286/8",        CPU_286,   fpus_80286, 0,  8000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 1},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ibmxt286[] =
{
        /*286*/
        {"286/6",        CPU_286,   fpus_80286, 0,  6000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ps1_m2011[] =
{
        /*286*/
        {"286/10",       CPU_286,   fpus_80286, 2, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ps2_m30_286[] =
{
        /*286*/
        {"286/10",       CPU_286,   fpus_80286, 2, 10000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1},
        {"286/12",       CPU_286,   fpus_80286, 3, 12000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2},
        {"286/16",       CPU_286,   fpus_80286, 4, 16000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 2},
        {"286/20",       CPU_286,   fpus_80286, 5, 20000000, 1, 0, 0, 0, 0, 0, 4,4,4,4, 3},
        {"286/25",       CPU_286,   fpus_80286, 6, 25000000, 1, 0, 0, 0, 0, 0, 4,4,4,4, 3},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_i386SX[] =
{
        /*i386SX*/
        {"i386SX/16",    CPU_386SX, fpus_80386, 0, 16000000, 1, 0, 0x2308, 0, 0, 0, 3,3,3,3, 2},
        {"i386SX/20",    CPU_386SX, fpus_80386, 1, 20000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3, 3},
        {"i386SX/25",    CPU_386SX, fpus_80386, 2, 25000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3, 3},
        {"i386SX/33",    CPU_386SX, fpus_80386, 3, 33333333, 1, 0, 0x2308, 0, 0, 0, 6,6,3,3, 4},
        {"",             -1,        0, 0, 0}
};

CPU cpus_i386DX[] =
{
        /*i386DX*/
        {"i386DX/16",    CPU_386DX, fpus_80386, 0, 16000000, 1, 0, 0x0308, 0, 0, 0, 3,3,3,3, 2},
        {"i386DX/20",    CPU_386DX, fpus_80386, 1, 20000000, 1, 0, 0x0308, 0, 0, 0, 4,4,3,3, 3},
        {"i386DX/25",    CPU_386DX, fpus_80386, 2, 25000000, 1, 0, 0x0308, 0, 0, 0, 4,4,3,3, 3},
        {"i386DX/33",    CPU_386DX, fpus_80386, 3, 33333333, 1, 0, 0x0308, 0, 0, 0, 6,6,3,3, 4},
        {"",             -1,        0, 0, 0}
};

CPU cpus_acer[] =
{
        /*i386SX*/
        {"i386SX/25",    CPU_386SX, fpus_80386, 2, 25000000, 1, 0, 0x2308, 0, 0, 0, 4,4,4,4, 3},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am386SX[] =
{
        /*Am386*/
        {"Am386SX/16",   CPU_386SX, fpus_80386, 0, 16000000, 1, 0, 0x2308, 0, 0, 0, 3,3,3,3, 2},
        {"Am386SX/20",   CPU_386SX, fpus_80386, 1, 20000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3, 3},
        {"Am386SX/25",   CPU_386SX, fpus_80386, 2, 25000000, 1, 0, 0x2308, 0, 0, 0, 4,4,3,3, 3},
        {"Am386SX/33",   CPU_386SX, fpus_80386, 3, 33333333, 1, 0, 0x2308, 0, 0, 0, 6,6,3,3, 4},
        {"Am386SX/40",   CPU_386SX, fpus_80386, 4, 40000000, 1, 0, 0x2308, 0, 0, 0, 7,7,3,3, 5},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am386DX[] =
{
        /*Am386*/
        {"Am386DX/25",   CPU_386DX, fpus_80386, 2, 25000000, 1, 0, 0x0308, 0, 0, 0, 4,4,3,3, 3},
        {"Am386DX/33",   CPU_386DX, fpus_80386, 3, 33333333, 1, 0, 0x0308, 0, 0, 0, 6,6,3,3, 4},
        {"Am386DX/40",   CPU_386DX, fpus_80386, 4, 40000000, 1, 0, 0x0308, 0, 0, 0, 7,7,3,3, 5},
        {"",             -1,        0, 0, 0}
};

CPU cpus_486SLC[] =
{
        /*Cx486SLC*/
        {"Cx486SLC/20",  CPU_486SLC, fpus_80386, 1, 20000000, 1, 0, 0x400, 0, 0x0000, 0, 4,4,3,3, 3},
        {"Cx486SLC/25",  CPU_486SLC, fpus_80386, 2, 25000000, 1, 0, 0x400, 0, 0x0000, 0, 4,4,3,3, 3},
        {"Cx486SLC/33",  CPU_486SLC, fpus_80386, 3, 33333333, 1, 0, 0x400, 0, 0x0000, 0, 6,6,3,3, 4},
        {"Cx486SRx2/32", CPU_486SLC, fpus_80386, 3, 32000000, 2, 0, 0x406, 0, 0x0006, 0, 6,6,6,6, 2*2},
        {"Cx486SRx2/40", CPU_486SLC, fpus_80386, 4, 40000000, 2, 0, 0x406, 0, 0x0006, 0, 8,8,6,6, 2*3},
        {"Cx486SRx2/50", CPU_486SLC, fpus_80386, 5, 50000000, 2, 0, 0x406, 0, 0x0006, 0, 8,8,6,6, 2*3},
        {"",             -1,        0, 0, 0}
};

CPU cpus_486DLC[] =
{
        /*Cx486DLC*/
        {"Cx486DLC/25",  CPU_486DLC, fpus_80386, 2, 25000000, 1, 0, 0x401, 0, 0x0001, 0, 4,4,3,3, 3},
        {"Cx486DLC/33",  CPU_486DLC, fpus_80386, 3, 33333333, 1, 0, 0x401, 0, 0x0001, 0, 6,6,3,3, 4},
        {"Cx486DLC/40",  CPU_486DLC, fpus_80386, 4, 40000000, 1, 0, 0x401, 0, 0x0001, 0, 7,7,3,3, 5},
        {"Cx486DRx2/32", CPU_486DLC, fpus_80386, 3, 32000000, 2, 0, 0x407, 0, 0x0007, 0, 6,6,6,6, 2*2},
        {"Cx486DRx2/40", CPU_486DLC, fpus_80386, 4, 40000000, 2, 0, 0x407, 0, 0x0007, 0, 8,8,6,6, 2*3},
        {"Cx486DRx2/50", CPU_486DLC, fpus_80386, 5, 50000000, 2, 0, 0x407, 0, 0x0007, 0, 8,8,6,6, 2*3},
        {"Cx486DRx2/66", CPU_486DLC, fpus_80386, 6, 66666666, 2, 0, 0x407, 0, 0x0007, 0, 12,12,6,6, 2*4},
        {"",             -1,        0, 0, 0}
};

CPU cpus_i486[] =
{
        /*i486*/
        {"i486SX/16",    CPU_i486SX, fpus_none,    0,  16000000, 1, 16000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 3,3,3,3, 2},
        {"i486SX/20",    CPU_i486SX, fpus_none,    1,  20000000, 1, 20000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3},
        {"i486SX/25",    CPU_i486SX, fpus_none,    2,  25000000, 1, 25000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3},
        {"i486SX/33",    CPU_i486SX, fpus_none,    3,  33333333, 1, 33333333, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4},
        {"i486SX2/50",   CPU_i486SX, fpus_none,    5,  50000000, 2, 25000000, 0x45b, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 2*3},
        {"i486DX/25",    CPU_i486DX, fpus_builtin, 2,  25000000, 1, 25000000, 0x404, 0, 0, CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3},
        {"i486DX/33",    CPU_i486DX, fpus_builtin, 3,  33333333, 1, 33333333, 0x404, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4},
        {"i486DX/50",    CPU_i486DX, fpus_builtin, 5,  50000000, 1, 25000000, 0x404, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,4,4, 6},
        {"i486DX2/40",   CPU_i486DX, fpus_builtin, 4,  40000000, 2, 20000000, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 2*3},
        {"i486DX2/50",   CPU_i486DX, fpus_builtin, 5,  50000000, 2, 25000000, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 2*3},
        {"i486DX2/66",   CPU_i486DX, fpus_builtin, 6,  66666666, 2, 33333333, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 2*4},
        {"iDX4/75",      CPU_iDX4,   fpus_builtin, 7,  75000000, 3, 25000000, 0x481, 0x481, 0, CPU_SUPPORTS_DYNAREC, 12,12,9,9, 3*3}, /*CPUID available on DX4, >= 75 MHz*/
        {"iDX4/100",     CPU_iDX4,   fpus_builtin, 10, 100000000, 3, 33333333, 0x481, 0x481, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*4}, /*Is on some real Intel DX2s, limit here is pretty arbitary*/
        {"Pentium OverDrive/63",       CPU_PENTIUM,     fpus_builtin, 6,  62500000, 3, 25000000, 0x1531, 0x1531, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,7,7, (5*3)/2},
        {"Pentium OverDrive/83",       CPU_PENTIUM,     fpus_builtin, 8,  83333333, 3, 33333333, 0x1532, 0x1532, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,8,8, (5*4)/2},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am486[] =
{
        /*Am486/5x86*/
        {"Am486SX/33",   CPU_Am486SX, fpus_none,    3,  33333333, 1, 33333333, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4},
        {"Am486SX/40",   CPU_Am486SX, fpus_none,    4,  40000000, 1, 20000000, 0x42a, 0, 0, CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5},
        {"Am486SX2/50",  CPU_Am486SX, fpus_none,    5,  50000000, 2, 25000000, 0x45b, 0x45b, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 2*3}, /*CPUID available on SX2, DX2, DX4, 5x86, >= 50 MHz*/
        {"Am486SX2/66",  CPU_Am486SX, fpus_none,    6,  66666666, 2, 33333333, 0x45b, 0x45b, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 2*4}, /*Isn't on all real AMD SX2s and DX2s, availability here is pretty arbitary (and distinguishes them from the Intel chips)*/
        {"Am486DX/33",   CPU_Am486DX, fpus_builtin, 3,  33333333, 1, 33333333, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4},
        {"Am486DX/40",   CPU_Am486DX, fpus_builtin, 4,  40000000, 1, 20000000, 0x430, 0, 0, CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5},
        {"Am486DX2/50",  CPU_Am486DX, fpus_builtin, 5,  50000000, 2, 25000000, 0x470, 0x470, 0, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 2*3},
        {"Am486DX2/66",  CPU_Am486DX, fpus_builtin, 6,  66666666, 2, 33333333, 0x470, 0x470, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 2*4},
        {"Am486DX2/80",  CPU_Am486DX, fpus_builtin, 8,  80000000, 2, 20000000, 0x470, 0x470, 0, CPU_SUPPORTS_DYNAREC, 14,14,6,6, 2*5},
        {"Am486DX4/75",  CPU_Am486DX, fpus_builtin, 7,  75000000, 3, 25000000, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 12,12,9,9, 3*3},
        {"Am486DX4/90",  CPU_Am486DX, fpus_builtin, 9,  90000000, 3, 30000000, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 15,15,9,9, 3*4},
        {"Am486DX4/100", CPU_Am486DX, fpus_builtin, 10, 100000000, 3, 33333333, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 15,15,9,9, 3*4},
        {"Am486DX4/120", CPU_Am486DX, fpus_builtin, 11, 120000000, 3, 20000000, 0x482, 0x482, 0, CPU_SUPPORTS_DYNAREC, 21,21,9,9, 3*5},
        {"Am5x86/P75",   CPU_Am486DX, fpus_builtin, 12, 133333333, 4, 33333333, 0x4e0, 0x4e0, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 4*4},
        {"Am5x86/P75+",  CPU_Am486DX, fpus_builtin, 13, 160000000, 4, 20000000, 0x4e0, 0x4e0, 0, CPU_SUPPORTS_DYNAREC, 28,28,12,12, 4*5},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Cx486[] =
{
        /*Cx486/5x86*/
        {"Cx486S/25",    CPU_Cx486S,  fpus_none,    2,  25000000, 1, 25000000, 0x420, 0, 0x0010, CPU_SUPPORTS_DYNAREC, 4,4,3,3, 3},
        {"Cx486S/33",    CPU_Cx486S,  fpus_none,    3,  33333333, 1, 33333333, 0x420, 0, 0x0010, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4},
        {"Cx486S/40",    CPU_Cx486S,  fpus_none,    4,  40000000, 1, 20000000, 0x420, 0, 0x0010, CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5},
        {"Cx486DX/33",   CPU_Cx486DX, fpus_builtin, 3,  33333333, 1, 33333333, 0x430, 0, 0x051a, CPU_SUPPORTS_DYNAREC, 6,6,3,3, 4},
        {"Cx486DX/40",   CPU_Cx486DX, fpus_builtin, 4,  40000000, 1, 20000000, 0x430, 0, 0x051a, CPU_SUPPORTS_DYNAREC, 7,7,3,3, 5},
        {"Cx486DX2/50",  CPU_Cx486DX, fpus_builtin, 5,  50000000, 2, 25000000, 0x430, 0, 0x081b, CPU_SUPPORTS_DYNAREC, 8,8,6,6, 2*3},
        {"Cx486DX2/66",  CPU_Cx486DX, fpus_builtin, 6,  66666666, 2, 33333333, 0x430, 0, 0x0b1b, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 2*4},
        {"Cx486DX2/80",  CPU_Cx486DX, fpus_builtin, 8,  80000000, 2, 20000000, 0x430, 0, 0x311b, CPU_SUPPORTS_DYNAREC, 14,14,16,16, 2*5},
        {"Cx486DX4/75",  CPU_Cx486DX, fpus_builtin, 7,  75000000, 3, 25000000, 0x480, 0, 0x361f, CPU_SUPPORTS_DYNAREC, 12,12,9,9, 3*3},
        {"Cx486DX4/100", CPU_Cx486DX, fpus_builtin, 10, 100000000, 3, 33333333, 0x480, 0, 0x361f, CPU_SUPPORTS_DYNAREC, 15,15,9,9, 3*4},
        {"Cx5x86/100",   CPU_Cx5x86,  fpus_builtin, 10, 100000000, 3, 33333333, 0x480, 0, 0x002f, CPU_SUPPORTS_DYNAREC, 15,15,9,9, 3*4},
        {"Cx5x86/120",   CPU_Cx5x86,  fpus_builtin, 11, 120000000, 3, 20000000, 0x480, 0, 0x002f, CPU_SUPPORTS_DYNAREC, 21,21,9,9, 3*5},
        {"Cx5x86/133",   CPU_Cx5x86,  fpus_builtin, 12, 133333333, 4, 33333333, 0x480, 0, 0x002f, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 4*4},
        {"",             -1,        0, 0, 0}
};

CPU cpus_6x86[] =
{
        /*Cyrix 6x86*/
        {"6x86-P90",     CPU_Cx6x86, fpus_builtin, 17,     80000000, 3, 40000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 8,8,6,6, 2*5},
        {"6x86-PR120+",  CPU_Cx6x86, fpus_builtin, 17,   100000000, 3, 25000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*6},
        {"6x86-PR133+",  CPU_Cx6x86, fpus_builtin, 17,   110000000, 3, 27500000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*7},
        {"6x86-PR150+",  CPU_Cx6x86, fpus_builtin, 17,   120000000, 3, 30000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"6x86-PR166+",  CPU_Cx6x86, fpus_builtin, 17,   133333333, 3, 33333333, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"6x86-PR200+",  CPU_Cx6x86, fpus_builtin, 17,   150000000, 3, 37500000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*9},

        /*Cyrix 6x86L*/
        {"6x86L-PR133+",  CPU_Cx6x86L, fpus_builtin, 19,   110000000, 3, 27500000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*7},
        {"6x86L-PR150+",  CPU_Cx6x86L, fpus_builtin, 19,   120000000, 3, 30000000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"6x86L-PR166+",  CPU_Cx6x86L, fpus_builtin, 19,   133333333, 3, 33333333, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"6x86L-PR200+",  CPU_Cx6x86L, fpus_builtin, 19,   150000000, 3, 37500000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*9},

        /*Cyrix 6x86MX*/
        {"6x86MX-PR166",  CPU_Cx6x86MX, fpus_builtin, 18, 133333333, 3, 33333333, 0x600, 0x600, 0x0451, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"6x86MX-PR200",  CPU_Cx6x86MX, fpus_builtin, 18, 166666666, 3, 33333333, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"6x86MX-PR233",  CPU_Cx6x86MX, fpus_builtin, 18, 188888888, 3, 37500000, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*9)/2},
        {"6x86MX-PR266",  CPU_Cx6x86MX, fpus_builtin, 18, 207500000, 3, 41666667, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7, (5*10)/2},

        /*Cyrix M II*/
        {"M II-200",  CPU_Cx6x86MX, fpus_builtin, 18, 166666666, 3, 33333333, 0x600, 0x600, 0x0854, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7,   (5*8)/2},
        {"M II-233",  CPU_Cx6x86MX, fpus_builtin, 18, 188888888, 3, 37500000, 0x601, 0x601, 0x0854, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7,   (5*9)/2},
        {"M II-266",  CPU_Cx6x86MX, fpus_builtin, 18, 207500000, 3, 41666667, 0x601, 0x601, 0x0853, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7,   (5*10)/2},
        {"M II-300",  CPU_Cx6x86MX, fpus_builtin, 18, 233333333, 3, 33333333, 0x601, 0x601, 0x0852, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,7,7,   (7*8)/2},
        {"M II-333",  CPU_Cx6x86MX, fpus_builtin, 18, 250000000, 3, 41666667, 0x601, 0x601, 0x0853, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 20,20,9,9,   3*10},

        {"",             -1,        0, 0, 0}
};

CPU cpus_6x86_SS7[] =
{
        /*Cyrix 6x86*/
        {"6x86-P90",  CPU_Cx6x86, fpus_builtin, 17,     80000000, 3, 40000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 8,8,6,6, 2*5},
        {"6x86-PR120+",  CPU_Cx6x86, fpus_builtin, 17,   100000000, 3, 25000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*6},
        {"6x86-PR133+",  CPU_Cx6x86, fpus_builtin, 17,   110000000, 3, 27500000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*7},
        {"6x86-PR150+",  CPU_Cx6x86, fpus_builtin, 17,   120000000, 3, 30000000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"6x86-PR166+",  CPU_Cx6x86, fpus_builtin, 17,   133333333, 3, 33333333, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"6x86-PR200+",  CPU_Cx6x86, fpus_builtin, 17,   150000000, 3, 37500000, 0x520, 0x520, 0x1731, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*9},

        /*Cyrix 6x86L*/
        {"6x86L-PR133+",  CPU_Cx6x86L, fpus_builtin, 19,   110000000, 3, 27500000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*7},
        {"6x86L-PR150+",  CPU_Cx6x86L, fpus_builtin, 19,   120000000, 3, 30000000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"6x86L-PR166+",  CPU_Cx6x86L, fpus_builtin, 19,   133333333, 3, 33333333, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"6x86L-PR200+",  CPU_Cx6x86L, fpus_builtin, 19,   150000000, 3, 37500000, 0x540, 0x540, 0x2231, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*9},

        /*Cyrix 6x86MX*/
        {"6x86MX-PR166",  CPU_Cx6x86MX, fpus_builtin, 18, 133333333, 3, 33333333, 0x600, 0x600, 0x0451, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"6x86MX-PR200",  CPU_Cx6x86MX, fpus_builtin, 18, 166666666, 3, 33333333, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"6x86MX-PR233",  CPU_Cx6x86MX, fpus_builtin, 18, 188888888, 3, 37500000, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*9)/2},
        {"6x86MX-PR266",  CPU_Cx6x86MX, fpus_builtin, 18, 207500000, 3, 41666667, 0x600, 0x600, 0x0452, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7, (5*10)/2},

        /*Cyrix M II*/
        {"M II-200",  CPU_Cx6x86MX, fpus_builtin, 18, 166666666, 3, 33333333, 0x600, 0x600, 0x0854, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7,   (5*8)/2},
        {"M II-233",  CPU_Cx6x86MX, fpus_builtin, 18, 188888888, 3, 37500000, 0x601, 0x601, 0x0854, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7,   (5*9)/2},
        {"M II-266",  CPU_Cx6x86MX, fpus_builtin, 18, 207500000, 3, 41666667, 0x601, 0x601, 0x0853, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 17,17,7,7,   (5*10)/2},
        {"M II-300",  CPU_Cx6x86MX, fpus_builtin, 18, 233333333, 3, 33333333, 0x601, 0x601, 0x0852, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,7,7,   (7*8)/2},
        {"M II-333",  CPU_Cx6x86MX, fpus_builtin, 18, 250000000, 3, 41666667, 0x601, 0x601, 0x0853, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 20,20,9,9,   3*10},
        {"M II-366",  CPU_Cx6x86MX, fpus_builtin, 18, 250000000, 3, 33333333, 0x601, 0x601, 0x0852, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 3*10},
        {"M II-400",  CPU_Cx6x86MX, fpus_builtin, 18, 285000000, 3, 31666667, 0x601, 0x601, 0x0853, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9,   3*11},
        {"M II-433",  CPU_Cx6x86MX, fpus_builtin, 18, 300000000, 3, 33333333, 0x601, 0x601, 0x0853, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9,   3*12},
        {"",             -1,        0, 0, 0}
};

CPU cpus_WinChip[] =
{
        /*IDT WinChip*/
        {"WinChip 75",   CPU_WINCHIP, fpus_builtin,  7,  75000000, 2, 25000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 8,8,4,4, (3*6)/2},
        {"WinChip 90",   CPU_WINCHIP, fpus_builtin,  9,  90000000, 2, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 9,9,4,4, (3*7)/2},
        {"WinChip 100",  CPU_WINCHIP, fpus_builtin, 10, 100000000, 2, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 9,9,4,4, (3*8)/2},
        {"WinChip 120",  CPU_WINCHIP, fpus_builtin, 11, 120000000, 2, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 2*7},
        {"WinChip 133",  CPU_WINCHIP, fpus_builtin, 12, 133333333, 2, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 12,12,6,6, 2*8},
        {"WinChip 150",  CPU_WINCHIP, fpus_builtin, 13, 150000000, 3, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 15,15,7,7, (5*7)/2},
        {"WinChip 166",  CPU_WINCHIP, fpus_builtin, 15, 166666666, 3, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 15,15,7,7, (5*8)/2},
        {"WinChip 180",  CPU_WINCHIP, fpus_builtin, 16, 180000000, 3, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*7},
        {"WinChip 200",  CPU_WINCHIP, fpus_builtin, 17, 200000000, 3, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*8},
        {"WinChip 240",  CPU_WINCHIP, fpus_builtin, 17, 240000000, 6, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 4*7},
        {"WinChip 2/200",  CPU_WINCHIP2, fpus_builtin, 17, 200000000, 3, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*8},
        {"WinChip 2/240",  CPU_WINCHIP2, fpus_builtin, 17, 240000000, 6, 30000000, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30},
        {"WinChip 2A/200",  CPU_WINCHIP2, fpus_builtin, 17, 200000000, 3, 33333333, 0x587, 0x587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*8},
        {"WinChip 2A/233",  CPU_WINCHIP2, fpus_builtin, 17, 233333333, 3, 33333333, 0x587, 0x587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, (7*8)/2},
        {"",             -1,        0, 0, 0}
};

CPU cpus_WinChip_SS7[] =
{
        /*IDT WinChip (Super Socket 7)*/
        {"WinChip 200",     CPU_WINCHIP,  fpus_builtin, 17, 200000000, 3, 33333333, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*8},
        {"WinChip 225",     CPU_WINCHIP,  fpus_builtin, 17, 225000000, 3, 37500000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*9},
        {"WinChip 240",     CPU_WINCHIP,  fpus_builtin, 17, 240000000, 6, 30000000, 0x540, 0x540, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 4*7},
        {"WinChip 2/200",   CPU_WINCHIP2, fpus_builtin, 17, 200000000, 3, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*8},
        {"WinChip 2/225",   CPU_WINCHIP2, fpus_builtin, 17, 225000000, 3, 37500000, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 3*9},
        {"WinChip 2/240",   CPU_WINCHIP2, fpus_builtin, 17, 240000000, 6, 30000000, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30},
        {"WinChip 2/250",   CPU_WINCHIP2, fpus_builtin, 17, 250000000, 6, 41666667, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30},
        {"WinChip 2A/200",  CPU_WINCHIP2, fpus_builtin, 17, 200000000, 3, 33333333, 0x587, 0x587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 24},
        {"WinChip 2A/233",  CPU_WINCHIP2, fpus_builtin, 17, 233333333, 3, 33333333, 0x587, 0x587, 0, CPU_SUPPORTS_DYNAREC, 18,18,9,9, 28},
        {"WinChip 2A/266",  CPU_WINCHIP2, fpus_builtin, 17, 233333333, 6, 33333333, 0x587, 0x587, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 28},
        {"WinChip 2A/300",  CPU_WINCHIP2, fpus_builtin, 17, 250000000, 6, 33333333, 0x587, 0x587, 0, CPU_SUPPORTS_DYNAREC, 24,24,12,12, 30},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Pentium5V[] =
{
        /*Intel Pentium (5V, socket 4)*/
        {"Pentium 60",            CPU_PENTIUM, fpus_builtin, 6,  60000000, 1, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 7},
        {"Pentium 66",            CPU_PENTIUM, fpus_builtin, 6,  66666666, 1, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 6,6,3,3, 8},
        {"Pentium OverDrive 120", CPU_PENTIUM, fpus_builtin, 14, 120000000, 2, 30000000, 0x51A, 0x51A, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"Pentium OverDrive 133", CPU_PENTIUM, fpus_builtin, 16, 133333333, 2, 33333333, 0x51A, 0x51A, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"",             -1,        0, 0, 0}
};

CPU cpus_PentiumS5[] =
{
        /*Intel Pentium (Socket 5)*/
        {"Pentium 75",       CPU_PENTIUM,    fpus_builtin,  9,  75000000, 2, 25000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, (3*6)/2},
        {"Pentium 90",       CPU_PENTIUM,    fpus_builtin, 12,  90000000, 2, 30000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, (3*7)/2},
        {"Pentium 100/50",   CPU_PENTIUM,    fpus_builtin, 13, 100000000, 2, 25000000, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*6},
        {"Pentium 100/66",   CPU_PENTIUM,    fpus_builtin, 13, 100000000, 2, 33333333, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, (3*8)/2},
        {"Pentium 120",      CPU_PENTIUM,    fpus_builtin, 14, 120000000, 2, 30000000, 0x526, 0x526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"Pentium 133",      CPU_PENTIUM,    fpus_builtin, 16, 133333333, 2, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"Pentium OverDrive 125",CPU_PENTIUM,fpus_builtin, 15, 125000000, 3, 25000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, (5*6)/2},
        {"Pentium OverDrive 150",CPU_PENTIUM,fpus_builtin, 17, 150000000, 3, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*7)/2},
        {"Pentium OverDrive 166",CPU_PENTIUM,fpus_builtin, 17, 166666666, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Pentium OverDrive MMX 125",       CPU_PENTIUMMMX, fpus_builtin, 15,125000000, 3, 25000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, (5*6)/2},
        {"Pentium OverDrive MMX 150/60",    CPU_PENTIUMMMX, fpus_builtin, 17,150000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*7)/2},
        {"Pentium OverDrive MMX 166",       CPU_PENTIUMMMX, fpus_builtin, 19,166000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Pentium OverDrive MMX 180",       CPU_PENTIUMMMX, fpus_builtin, 20,180000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*7},
        {"Pentium OverDrive MMX 200",       CPU_PENTIUMMMX, fpus_builtin, 21,200000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*8},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Pentium[] =
{
        /*Intel Pentium*/
        {"Pentium 75",       CPU_PENTIUM,    fpus_builtin,  9,  75000000, 2, 25000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 7,7,4,4, (3*6)/2},
        {"Pentium 90",       CPU_PENTIUM,    fpus_builtin, 12,  90000000, 2, 30000000, 0x524, 0x524, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, (3*7)/2},
        {"Pentium 100/50",   CPU_PENTIUM,    fpus_builtin, 13, 100000000, 2, 25000000, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 10,10,6,6, 2*6},
        {"Pentium 100/66",   CPU_PENTIUM,    fpus_builtin, 13, 100000000, 2, 33333333, 0x525, 0x525, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 9,9,4,4, (3*8)/2},
        {"Pentium 120",      CPU_PENTIUM,    fpus_builtin, 14, 120000000, 2, 30000000, 0x526, 0x526, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"Pentium 133",      CPU_PENTIUM,    fpus_builtin, 16, 133333333, 2, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"Pentium 150",      CPU_PENTIUM,    fpus_builtin, 17, 150000000, 3, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*7)/2},
        {"Pentium 166",      CPU_PENTIUM,    fpus_builtin, 19, 166666666, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Pentium 200",      CPU_PENTIUM,    fpus_builtin, 21, 200000000, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*8},
        {"Pentium MMX 166",  CPU_PENTIUMMMX, fpus_builtin, 19, 166666666, 3, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Pentium MMX 200",  CPU_PENTIUMMMX, fpus_builtin, 21, 200000000, 3, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*8},
        {"Pentium MMX 233",  CPU_PENTIUMMMX, fpus_builtin, 24, 233333333, 4, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, (7*8)/2},
        {"Mobile Pentium MMX 120",       CPU_PENTIUMMMX, fpus_builtin, 14, 120000000, 2, 30000000, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*7},
        {"Mobile Pentium MMX 133",       CPU_PENTIUMMMX, fpus_builtin, 16, 133333333, 2, 33333333, 0x543, 0x543, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,6,6, 2*8},
        {"Mobile Pentium MMX 150",       CPU_PENTIUMMMX, fpus_builtin, 17, 150000000, 3, 30000000, 0x544, 0x544, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*7)/2},
        {"Mobile Pentium MMX 166",       CPU_PENTIUMMMX, fpus_builtin, 19, 166666666, 3, 33333333, 0x544, 0x544, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Mobile Pentium MMX 200",       CPU_PENTIUMMMX, fpus_builtin, 21, 200000000, 3, 33333333, 0x581, 0x581, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*8},
        {"Mobile Pentium MMX 233",       CPU_PENTIUMMMX, fpus_builtin, 24, 233333333, 4, 33333333, 0x581, 0x581, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, (7*8)/2},
        {"Mobile Pentium MMX 266",       CPU_PENTIUMMMX, fpus_builtin, 26, 266666666, 4, 33333333, 0x582, 0x582, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 4*8},
        {"Mobile Pentium MMX 300",       CPU_PENTIUMMMX, fpus_builtin, 28, 300000000, 5, 33333333, 0x582, 0x582, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, (9*8)/2},
        {"Pentium OverDrive 125",        CPU_PENTIUM,    fpus_builtin, 15, 125000000, 3, 25000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, (5*6)/2},
        {"Pentium OverDrive 150",        CPU_PENTIUM,    fpus_builtin, 17, 150000000, 3, 30000000, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*7)/2},
        {"Pentium OverDrive 166",        CPU_PENTIUM,    fpus_builtin, 17, 166666666, 3, 33333333, 0x52c, 0x52c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Pentium OverDrive MMX 125",    CPU_PENTIUMMMX, fpus_builtin, 15,125000000, 3, 25000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 12,12,7,7, (5*6)/2},
        {"Pentium OverDrive MMX 150/60", CPU_PENTIUMMMX, fpus_builtin, 17,150000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*7)/2},
        {"Pentium OverDrive MMX 166",    CPU_PENTIUMMMX, fpus_builtin, 19,166000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, (5*8)/2},
        {"Pentium OverDrive MMX 180",    CPU_PENTIUMMMX, fpus_builtin, 20,180000000, 3, 30000000, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*7},
        {"Pentium OverDrive MMX 200",    CPU_PENTIUMMMX, fpus_builtin, 21,200000000, 3, 33333333, 0x1542, 0x1542, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 3*8},
        {"",             -1,        0, 0, 0}
};

CPU cpus_K6_S7[] =
{
        /*AMD K6*/
        {"K6/166",          CPU_K6,    fpus_builtin, 19, 166666666, 3, 33333333, 0x561, 0x561, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20},
        {"K6/200",          CPU_K6,    fpus_builtin, 21, 200000000, 3, 33333333, 0x561, 0x561, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24},
        {"K6/233",          CPU_K6,    fpus_builtin, 24, 233333333, 4, 33333333, 0x561, 0x561, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28},
        {"K6/266",          CPU_K6,    fpus_builtin, 26, 266666666, 4, 33333333, 0x570, 0x570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32},
        {"K6/300",          CPU_K6,    fpus_builtin, 28, 300000000, 5, 33333333, 0x570, 0x570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36},
        {"K6-2/233",        CPU_K6_2,  fpus_builtin, 24, 233333333, 4, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28},
        {"K6-2/266",        CPU_K6_2,  fpus_builtin, 26, 266666666, 4, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32},
        {"K6-2/300 AFR-66", CPU_K6_2,  fpus_builtin, 28, 300000000, 5, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36},
        {"",                -1,        0, 0, 0}
};

CPU cpus_K6_SS7[] =
{
        /*AMD K6*/
        {"K6/166",      CPU_K6,    fpus_builtin, 19, 166666666, 3, 33333333, 0x561, 0x561, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 15,15,7,7, 20},
        {"K6/200",      CPU_K6,    fpus_builtin, 21, 200000000, 3, 33333333, 0x561, 0x561, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 18,18,9,9, 24},
        {"K6/233",      CPU_K6,    fpus_builtin, 24, 233333333, 4, 33333333, 0x561, 0x561, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28},
        {"K6/266",      CPU_K6,    fpus_builtin, 26, 266666666, 4, 33333333, 0x570, 0x570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32},
        {"K6/300",      CPU_K6,    fpus_builtin, 28, 300000000, 5, 33333333, 0x570, 0x570, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36},
        {"K6-2/233",    CPU_K6_2,  fpus_builtin, 24, 233333333, 4, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 21,21,10,10, 28},
        {"K6-2/266",    CPU_K6_2,  fpus_builtin, 26, 266666666, 4, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 24,24,12,12, 32},
        {"K6-2/300",    CPU_K6_2,  fpus_builtin, 28, 300000000, 5, 33333333, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 36},
        {"K6-2/333",    CPU_K6_2,  fpus_builtin, 28, 333333333, 5, 31666667, 0x580, 0x580, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 40},
        {"K6-2/350",    CPU_K6_2,  fpus_builtin, 28, 350000000, 5, 33333333, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 42},
        {"K6-2/366",    CPU_K6_2,  fpus_builtin, 28, 366666666, 5, 33333333, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 44},
        {"K6-2/380",    CPU_K6_2,  fpus_builtin, 28, 380000000, 5, 31666667, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 46},
        {"K6-2/400",    CPU_K6_2,  fpus_builtin, 28, 400000000, 5, 33333333, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 48},
        {"K6-2/450",    CPU_K6_2,  fpus_builtin, 28, 450000000, 5, 33333333, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 54},
        {"K6-2/475",    CPU_K6_2,  fpus_builtin, 28, 475000000, 5, 31666667, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 57},
        {"K6-2/500",    CPU_K6_2,  fpus_builtin, 28, 500000000, 5, 33333333, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 60},
        {"K6-2/533",    CPU_K6_2,  fpus_builtin, 28, 533333333, 5, 31666667, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 64},
        {"K6-2/550",    CPU_K6_2,  fpus_builtin, 28, 550000000, 5, 33333333, 0x58c, 0x58c, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 66},
        {"K6-2+/450",   CPU_K6_2P, fpus_builtin, 28, 450000000, 5, 33333333, 0x5d4, 0x5d4, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 54},
        {"K6-2+/475",   CPU_K6_2P, fpus_builtin, 28, 475000000, 5, 31666667, 0x5d4, 0x5d4, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 57},
        {"K6-2+/500",   CPU_K6_2P, fpus_builtin, 28, 500000000, 5, 33333333, 0x5d4, 0x5d4, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 60},
        {"K6-2+/533",   CPU_K6_2P, fpus_builtin, 28, 533333333, 5, 31666667, 0x5d4, 0x5d4, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 64},
        {"K6-2+/550",   CPU_K6_2P, fpus_builtin, 28, 550000000, 5, 33333333, 0x5d4, 0x5d4, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 66},
        {"K6-III/400",  CPU_K6_3,  fpus_builtin, 28, 400000000, 5, 33333333, 0x591, 0x591, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 48},
        {"K6-III/450",  CPU_K6_3,  fpus_builtin, 28, 450000000, 5, 33333333, 0x591, 0x591, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 54},
        {"K6-III+/400", CPU_K6_3P, fpus_builtin, 28, 400000000, 5, 33333333, 0x5d0, 0x5d0, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 48},
        {"K6-III+/450", CPU_K6_3P, fpus_builtin, 28, 450000000, 5, 33333333, 0x5d0, 0x5d0, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 54},
        {"K6-III+/475", CPU_K6_3P, fpus_builtin, 28, 475000000, 5, 31666667, 0x5d0, 0x5d0, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 57},
        {"K6-III+/500", CPU_K6_3P, fpus_builtin, 28, 500000000, 5, 33333333, 0x5d0, 0x5d0, 0, CPU_SUPPORTS_DYNAREC | CPU_REQUIRES_DYNAREC, 27,27,13,13, 60},
        {"",             -1,        0, 0, 0}
};

