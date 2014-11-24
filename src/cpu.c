#include "ibm.h"
#include "cpu.h"
#include "model.h"
#include "io.h"
#include "x86_ops.h"
#ifdef DYNAREC
#include "codegen.h"
#endif

#ifdef DYNAREC
OpFn *x86_dynarec_opcodes;
OpFn *x86_dynarec_opcodes_0f;
OpFn *x86_dynarec_opcodes_d8_a16;
OpFn *x86_dynarec_opcodes_d8_a32;
OpFn *x86_dynarec_opcodes_d9_a16;
OpFn *x86_dynarec_opcodes_d9_a32;
OpFn *x86_dynarec_opcodes_da_a16;
OpFn *x86_dynarec_opcodes_da_a32;
OpFn *x86_dynarec_opcodes_db_a16;
OpFn *x86_dynarec_opcodes_db_a32;
OpFn *x86_dynarec_opcodes_dc_a16;
OpFn *x86_dynarec_opcodes_dc_a32;
OpFn *x86_dynarec_opcodes_dd_a16;
OpFn *x86_dynarec_opcodes_dd_a32;
OpFn *x86_dynarec_opcodes_de_a16;
OpFn *x86_dynarec_opcodes_de_a32;
OpFn *x86_dynarec_opcodes_df_a16;
OpFn *x86_dynarec_opcodes_df_a32;
#endif

OpFn *x86_opcodes;
OpFn *x86_opcodes_0f;
OpFn *x86_opcodes_d8_a16;
OpFn *x86_opcodes_d8_a32;
OpFn *x86_opcodes_d9_a16;
OpFn *x86_opcodes_d9_a32;
OpFn *x86_opcodes_da_a16;
OpFn *x86_opcodes_da_a32;
OpFn *x86_opcodes_db_a16;
OpFn *x86_opcodes_db_a32;
OpFn *x86_opcodes_dc_a16;
OpFn *x86_opcodes_dc_a32;
OpFn *x86_opcodes_dd_a16;
OpFn *x86_opcodes_dd_a32;
OpFn *x86_opcodes_de_a16;
OpFn *x86_opcodes_de_a32;
OpFn *x86_opcodes_df_a16;
OpFn *x86_opcodes_df_a32;

enum
{
        CPUID_FPU = (1 << 0),
        CPUID_TSC = (1 << 4),
        CPUID_MSR = (1 << 5),
        CPUID_CMPXCHG8B = (1 << 8),
        CPUID_MMX = (1 << 23)
};

int cpu = 3, cpu_manufacturer = 0;
CPU *cpu_s;
int cpu_multi;
int cpu_iscyrix;
int cpu_16bitbus;
int cpu_busspeed;
int cpu_hasrdtsc;
int cpu_hasMMX, cpu_hasMSR;
int cpu_hasCR4;

uint64_t cpu_CR4_mask;

uint64_t tsc = 0;

int timing_rr;
int timing_mr, timing_mrl;
int timing_rm, timing_rml;
int timing_mm, timing_mml;
int timing_bt, timing_bnt;

static struct
{
        uint32_t tr1, tr12;
        uint32_t cesr;
        uint32_t fcr;
        uint64_t fcr2, fcr3;
} msr;

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

CPU cpus_8088[] =
{
        /*8088 standard*/
        {"8088/4.77",    CPU_8088,  0,  4772727, 1, 0, 0, 0},
        {"8088/8",       CPU_8088,  1,  8000000, 1, 0, 0, 0},
        {"8088/10",      CPU_8088,  2, 10000000, 1, 0, 0, 0},
        {"8088/12",      CPU_8088,  3, 12000000, 1, 0, 0, 0},
        {"8088/16",      CPU_8088,  4, 16000000, 1, 0, 0, 0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_pcjr[] =
{
        /*8088 PCjr*/
        {"8088/4.77",    CPU_8088,  0,  4772727, 1, 0, 0, 0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_8086[] =
{
        /*8086 standard*/
        {"8086/7.16",    CPU_8086,  1,  3579545*2,     1, 0, 0, 0},
        {"8086/8",       CPU_8086,  1,  8000000,       1, 0, 0, 0},
        {"8086/9.54",    CPU_8086,  1, (3579545*8)/3,  1, 0, 0, 0},
        {"8086/10",      CPU_8086,  2, 10000000,       1, 0, 0, 0},
        {"8086/12",      CPU_8086,  3, 12000000,       1, 0, 0, 0},
        {"8086/16",      CPU_8086,  4, 16000000,       1, 0, 0, 0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_pc1512[] =
{
        /*8086 Amstrad*/
        {"8086/8",       CPU_8086,  1,  8000000,       1, 0, 0, 0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_286[] =
{
        /*286*/
        {"286/6",        CPU_286,   0,  6000000, 1, 0, 0, 0},
        {"286/8",        CPU_286,   1,  8000000, 1, 0, 0, 0},
        {"286/10",       CPU_286,   2, 10000000, 1, 0, 0, 0},
        {"286/12",       CPU_286,   3, 12000000, 1, 0, 0, 0},
        {"286/16",       CPU_286,   4, 16000000, 1, 0, 0, 0},
        {"286/20",       CPU_286,   5, 20000000, 1, 0, 0, 0},
        {"286/25",       CPU_286,   6, 25000000, 1, 0, 0, 0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_ibmat[] =
{
        /*286*/
        {"286/6",        CPU_286,   0,  6000000, 1, 0, 0, 0},
        {"286/8",        CPU_286,   0,  8000000, 1, 0, 0, 0},
        {"",             -1,        0, 0, 0, 0}
};

CPU cpus_i386[] =
{
        /*i386*/
        {"i386SX/16",    CPU_386SX, 0, 16000000, 1, 0x2308, 0, 0},
        {"i386SX/20",    CPU_386SX, 1, 20000000, 1, 0x2308, 0, 0},
        {"i386SX/25",    CPU_386SX, 2, 25000000, 1, 0x2308, 0, 0},
        {"i386SX/33",    CPU_386SX, 3, 33333333, 1, 0x2308, 0, 0},
        {"i386DX/16",    CPU_386DX, 0, 16000000, 1, 0x0308, 0, 0},
        {"i386DX/20",    CPU_386DX, 1, 20000000, 1, 0x0308, 0, 0},
        {"i386DX/25",    CPU_386DX, 2, 25000000, 1, 0x0308, 0, 0},
        {"i386DX/33",    CPU_386DX, 3, 33333333, 1, 0x0308, 0, 0},
        {"",             -1,        0, 0, 0}
};

CPU cpus_acer[] =
{
        /*i386*/
        {"i386SX/25",    CPU_386SX, 2, 25000000, 1, 0x2308, 0, 0},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am386[] =
{
        /*Am386*/
        {"Am386SX/16",   CPU_386SX, 0, 16000000, 1, 0x2308, 0, 0},
        {"Am386SX/20",   CPU_386SX, 1, 20000000, 1, 0x2308, 0, 0},
        {"Am386SX/25",   CPU_386SX, 2, 25000000, 1, 0x2308, 0, 0},
        {"Am386SX/33",   CPU_386SX, 3, 33333333, 1, 0x2308, 0, 0},
        {"Am386SX/40",   CPU_386SX, 4, 40000000, 1, 0x2308, 0, 0},
        {"Am386DX/25",   CPU_386DX, 2, 25000000, 1, 0x0308, 0, 0},
        {"Am386DX/33",   CPU_386DX, 3, 33333333, 1, 0x0308, 0, 0},
        {"Am386DX/40",   CPU_386DX, 4, 40000000, 1, 0x0308, 0, 0},
        {"",             -1,        0, 0, 0}
};

CPU cpus_486SDLC[] =
{
        /*Cx486SLC/DLC*/
        {"Cx486SLC/20",  CPU_486SLC, 1, 20000000, 1, 0, 0, 0x0000},
        {"Cx486SLC/25",  CPU_486SLC, 2, 25000000, 1, 0, 0, 0x0000},
        {"Cx486SLC/33",  CPU_486SLC, 3, 33333333, 1, 0, 0, 0x0000},
        {"Cx486SRx2/32", CPU_486SLC, 3, 32000000, 2, 0, 0, 0x0006},
        {"Cx486SRx2/40", CPU_486SLC, 4, 40000000, 2, 0, 0, 0x0006},
        {"Cx486SRx2/50", CPU_486SLC, 5, 50000000, 2, 0, 0, 0x0006},
        {"Cx486DLC/25",  CPU_486DLC, 2, 25000000, 1, 0, 0, 0x0001},
        {"Cx486DLC/33",  CPU_486DLC, 3, 33333333, 1, 0, 0, 0x0001},
        {"Cx486DLC/40",  CPU_486DLC, 4, 40000000, 1, 0, 0, 0x0001},
        {"Cx486DRx2/32", CPU_486DLC, 3, 32000000, 2, 0, 0, 0x0007},
        {"Cx486DRx2/40", CPU_486DLC, 4, 40000000, 2, 0, 0, 0x0007},
        {"Cx486DRx2/50", CPU_486DLC, 5, 50000000, 2, 0, 0, 0x0007},
        {"Cx486DRx2/66", CPU_486DLC, 6, 66666666, 2, 0, 0, 0x0007},
        {"",             -1,        0, 0, 0}
};

CPU cpus_i486[] =
{
        /*i486*/
        {"i486SX/16",    CPU_i486SX, 0,  16000000, 1, 0x42a, 0, 0},
        {"i486SX/20",    CPU_i486SX, 1,  20000000, 1, 0x42a, 0, 0},
        {"i486SX/25",    CPU_i486SX, 2,  25000000, 1, 0x42a, 0, 0},
        {"i486SX/33",    CPU_i486SX, 3,  33333333, 1, 0x42a, 0, 0},
        {"i486SX2/50",   CPU_i486SX, 5,  50000000, 2, 0x45b, 0, 0},
        {"i486DX/25",    CPU_i486DX, 2,  25000000, 1, 0x404, 0, 0},
        {"i486DX/33",    CPU_i486DX, 3,  33333333, 1, 0x404, 0, 0},
        {"i486DX/50",    CPU_i486DX, 5,  50000000, 1, 0x404, 0, 0},
        {"i486DX2/40",   CPU_i486DX, 4,  40000000, 2, 0x430, 0, 0},
        {"i486DX2/50",   CPU_i486DX, 5,  50000000, 2, 0x430, 0, 0},
        {"i486DX2/66",   CPU_i486DX, 6,  66666666, 2, 0x430, 0, 0},
        {"iDX4/75",      CPU_i486DX, 7,  75000000, 3, 0x481, 0x481, 0}, /*CPUID available on DX4, >= 75 MHz*/
        {"iDX4/100",     CPU_i486DX,10, 100000000, 3, 0x481, 0x481, 0}, /*Is on some real Intel DX2s, limit here is pretty arbitary*/
#ifdef DYNAREC
        {"Pentium OverDrive/63",       CPU_PENTIUM,     6,  62500000, 3, 0x1531, 0x1531, 0},
        {"Pentium OverDrive/83",       CPU_PENTIUM,     8,  83333333, 3, 0x1532, 0x1532, 0},
#endif
        {"",             -1,        0, 0, 0}
};

CPU cpus_Am486[] =
{
        /*Am486/5x86*/
        {"Am486SX/33",   CPU_Am486SX,  3,  33333333, 1, 0x42a, 0, 0},
        {"Am486SX/40",   CPU_Am486SX,  4,  40000000, 1, 0x42a, 0, 0},
        {"Am486SX2/50",  CPU_Am486SX,  5,  50000000, 2, 0x45b, 0x45b, 0}, /*CPUID available on SX2, DX2, DX4, 5x86, >= 50 MHz*/
        {"Am486SX2/66",  CPU_Am486SX,  6,  66666666, 2, 0x45b, 0x45b, 0}, /*Isn't on all real AMD SX2s and DX2s, availability here is pretty arbitary (and distinguishes them from the Intel chips)*/
        {"Am486DX/33",   CPU_Am486DX,  3,  33333333, 1, 0x430, 0, 0},
        {"Am486DX/40",   CPU_Am486DX,  4,  40000000, 1, 0x430, 0, 0},
        {"Am486DX2/50",  CPU_Am486DX,  5,  50000000, 2, 0x470, 0x470, 0},
        {"Am486DX2/66",  CPU_Am486DX,  6,  66666666, 2, 0x470, 0x470, 0},
        {"Am486DX2/80",  CPU_Am486DX,  8,  80000000, 2, 0x470, 0x470, 0},
        {"Am486DX4/75",  CPU_Am486DX,  7,  75000000, 3, 0x482, 0x482, 0},
        {"Am486DX4/90",  CPU_Am486DX,  9,  90000000, 3, 0x482, 0x482, 0},
        {"Am486DX4/100", CPU_Am486DX, 10, 100000000, 3, 0x482, 0x482, 0},
        {"Am486DX4/120", CPU_Am486DX, 11, 120000000, 3, 0x482, 0x482, 0},
        {"Am5x86/P75",   CPU_Am486DX, 12, 133333333, 4, 0x4e0, 0x4e0, 0},
        {"Am5x86/P75+",  CPU_Am486DX, 13, 150000000, 4, 0x4e0, 0x4e0, 0},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Cx486[] =
{
        /*Cx486/5x86*/
        {"Cx486S/25",    CPU_Cx486S,   2,  25000000, 1, 0x420, 0, 0x0010},
        {"Cx486S/33",    CPU_Cx486S,   3,  33333333, 1, 0x420, 0, 0x0010},
        {"Cx486S/40",    CPU_Cx486S,   4,  40000000, 1, 0x420, 0, 0x0010},
        {"Cx486DX/33",   CPU_Cx486DX,  3,  33333333, 1, 0x430, 0, 0x051a},
        {"Cx486DX/40",   CPU_Cx486DX,  4,  40000000, 1, 0x430, 0, 0x051a},
        {"Cx486DX2/50",  CPU_Cx486DX,  5,  50000000, 2, 0x430, 0, 0x081b},
        {"Cx486DX2/66",  CPU_Cx486DX,  6,  66666666, 2, 0x430, 0, 0x0b1b},
        {"Cx486DX2/80",  CPU_Cx486DX,  8,  80000000, 2, 0x430, 0, 0x311b},
        {"Cx486DX4/75",  CPU_Cx486DX,  7,  75000000, 3, 0x480, 0, 0x361f},
        {"Cx486DX4/100", CPU_Cx486DX, 10, 100000000, 3, 0x480, 0, 0x361f},
        {"Cx5x86/100",   CPU_Cx5x86,  10, 100000000, 3, 0x480, 0, 0x002f},
        {"Cx5x86/120",   CPU_Cx5x86,  11, 120000000, 3, 0x480, 0, 0x002f},
        {"Cx5x86/133",   CPU_Cx5x86,  12, 133333333, 4, 0x480, 0, 0x002f},
        {"",             -1,        0, 0, 0}
};

CPU cpus_WinChip[] =
{
        /*IDT WinChip*/
        {"WinChip 75",   CPU_WINCHIP,  7,  75000000, 2, 0x540, 0x540, 0},
        {"WinChip 90",   CPU_WINCHIP,  9,  90000000, 2, 0x540, 0x540, 0},
        {"WinChip 100",  CPU_WINCHIP, 10, 100000000, 2, 0x540, 0x540, 0},
        {"WinChip 120",  CPU_WINCHIP, 11, 120000000, 2, 0x540, 0x540, 0},
        {"WinChip 133",  CPU_WINCHIP, 12, 133333333, 2, 0x540, 0x540, 0},
        {"WinChip 150",  CPU_WINCHIP, 13, 150000000, 3, 0x540, 0x540, 0},        
        {"WinChip 166",  CPU_WINCHIP, 15, 166666666, 3, 0x540, 0x540, 0},
        {"WinChip 180",  CPU_WINCHIP, 16, 180000000, 3, 0x540, 0x540, 0},
        {"WinChip 200",  CPU_WINCHIP, 17, 200000000, 3, 0x540, 0x540, 0},
        {"WinChip 225",  CPU_WINCHIP, 17, 225000000, 3, 0x540, 0x540, 0},
        {"WinChip 240",  CPU_WINCHIP, 17, 240000000, 6, 0x540, 0x540, 0},
        {"",             -1,        0, 0, 0}
};
#ifdef DYNAREC
CPU cpus_Pentium5V[] =
{
        /*Intel Pentium (5V, socket 4)*/
        {"Pentium 60",       CPU_PENTIUM,     6,  60000000, 1, 0x52c, 0x52c, 0},
        {"Pentium 66",       CPU_PENTIUM,     6,  66666666, 1, 0x52c, 0x52c, 0},
        {"",             -1,        0, 0, 0}
};

CPU cpus_Pentium[] =
{
        /*Intel Pentium*/
        {"Pentium 75",       CPU_PENTIUM,     7,  75000000, 2, 0x52c, 0x52c, 0},
        {"Pentium 90",       CPU_PENTIUM,     9,  90000000, 2, 0x52c, 0x52c, 0},
        {"Pentium 100",      CPU_PENTIUM,    10, 100000000, 2, 0x52c, 0x52c, 0},
        {"Pentium 120",      CPU_PENTIUM,    11, 120000000, 2, 0x52c, 0x52c, 0},
        {"Pentium 133",      CPU_PENTIUM,    12, 133333333, 2, 0x52c, 0x52c, 0},
        {"Pentium 150",      CPU_PENTIUM,    12, 150000000, 3, 0x52c, 0x52c, 0},
        {"Pentium 166",      CPU_PENTIUM,    12, 166666666, 3, 0x52c, 0x52c, 0},
        {"Pentium 200",      CPU_PENTIUM,    12, 200000000, 3, 0x52c, 0x52c, 0},
        {"Pentium MMX 166",  CPU_PENTIUMMMX, 12, 166666666, 3, 0x543, 0x543, 0},
        {"Pentium MMX 200",  CPU_PENTIUMMMX, 12, 200000000, 3, 0x543, 0x543, 0},
        {"Pentium MMX 233",  CPU_PENTIUMMMX, 12, 233333333, 4, 0x543, 0x543, 0},
        {"",             -1,        0, 0, 0}
};
#endif
void cpu_set_edx()
{
        EDX = models[model].cpu[cpu_manufacturer].cpus[cpu].edx_reset;
}

void cpu_set()
{
        CPU *cpu_s;
        
        if (!models[model].cpu[cpu_manufacturer].cpus)
        {
                /*CPU is invalid, set to default*/
                cpu_manufacturer = 0;
                cpu = 0;
        }
        
        cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];

        CPUID    = cpu_s->cpuid_model;
        cpuspeed = cpu_s->speed;
        is8086   = (cpu_s->cpu_type >= CPU_8088);
        is486    = (cpu_s->cpu_type >= CPU_i486SX) || (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC);
        hasfpu   = (cpu_s->cpu_type >= CPU_i486DX);
        cpu_iscyrix = (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC || cpu_s->cpu_type == CPU_Cx486S || cpu_s->cpu_type == CPU_Cx486DX || cpu_s->cpu_type == CPU_Cx5x86);
        cpu_16bitbus = (cpu_s->cpu_type == CPU_386SX || cpu_s->cpu_type == CPU_486SLC);
        if (cpu_s->multi) 
           cpu_busspeed = cpu_s->rspeed / cpu_s->multi;
        cpu_multi = cpu_s->multi;
        cpu_hasrdtsc = 0;
        cpu_hasMMX = 0;
        cpu_hasMSR = 0;
        cpu_hasCR4 = 0;

        if (cpu_iscyrix)
           io_sethandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
        else
           io_removehandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
        
        pclog("hasfpu - %i\n",hasfpu);
        pclog("is486 - %i  %i\n",is486,cpu_s->cpu_type);

#ifdef DYNAREC
        x86_setopcodes(ops_386, ops_386_0f, dynarec_ops_386, dynarec_ops_386_0f);
#else
        x86_setopcodes(ops_386, ops_386_0f);
#endif                        
#ifdef DYNAREC
        if (hasfpu)
        {
                x86_dynarec_opcodes_d8_a16 = dynarec_ops_fpu_d8_a16;
                x86_dynarec_opcodes_d8_a32 = dynarec_ops_fpu_d8_a32;
                x86_dynarec_opcodes_d9_a16 = dynarec_ops_fpu_d9_a16;
                x86_dynarec_opcodes_d9_a32 = dynarec_ops_fpu_d9_a32;
                x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_da_a16;
                x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_da_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_db_a16;
                x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_db_a32;
                x86_dynarec_opcodes_dc_a16 = dynarec_ops_fpu_dc_a16;
                x86_dynarec_opcodes_dc_a32 = dynarec_ops_fpu_dc_a32;
                x86_dynarec_opcodes_dd_a16 = dynarec_ops_fpu_dd_a16;
                x86_dynarec_opcodes_dd_a32 = dynarec_ops_fpu_dd_a32;
                x86_dynarec_opcodes_de_a16 = dynarec_ops_fpu_de_a16;
                x86_dynarec_opcodes_de_a32 = dynarec_ops_fpu_de_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_df_a16;
                x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_df_a32;
        }
        else
        {
                x86_dynarec_opcodes_d8_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_d8_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_d9_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_d9_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_da_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_da_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_dc_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_dc_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_dd_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_dd_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_de_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_de_a16 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_nofpu_a32;
        }
        codegen_timing_set(&codegen_timing_486);
#endif
        if (hasfpu)
        {
                x86_opcodes_d8_a16 = ops_fpu_d8_a16;
                x86_opcodes_d8_a32 = ops_fpu_d8_a32;
                x86_opcodes_d9_a16 = ops_fpu_d9_a16;
                x86_opcodes_d9_a32 = ops_fpu_d9_a32;
                x86_opcodes_da_a16 = ops_fpu_da_a16;
                x86_opcodes_da_a32 = ops_fpu_da_a32;
                x86_opcodes_db_a16 = ops_fpu_db_a16;
                x86_opcodes_db_a32 = ops_fpu_db_a32;
                x86_opcodes_dc_a16 = ops_fpu_dc_a16;
                x86_opcodes_dc_a32 = ops_fpu_dc_a32;
                x86_opcodes_dd_a16 = ops_fpu_dd_a16;
                x86_opcodes_dd_a32 = ops_fpu_dd_a32;
                x86_opcodes_de_a16 = ops_fpu_de_a16;
                x86_opcodes_de_a32 = ops_fpu_de_a32;
                x86_opcodes_df_a16 = ops_fpu_df_a16;
                x86_opcodes_df_a32 = ops_fpu_df_a32;
        }
        else
        {
                x86_opcodes_d8_a16 = ops_nofpu_a16;
                x86_opcodes_d8_a32 = ops_nofpu_a32;
                x86_opcodes_d9_a16 = ops_nofpu_a16;
                x86_opcodes_d9_a32 = ops_nofpu_a32;
                x86_opcodes_da_a16 = ops_nofpu_a16;
                x86_opcodes_da_a32 = ops_nofpu_a32;
                x86_opcodes_db_a16 = ops_nofpu_a16;
                x86_opcodes_db_a32 = ops_nofpu_a32;
                x86_opcodes_dc_a16 = ops_nofpu_a16;
                x86_opcodes_dc_a32 = ops_nofpu_a32;
                x86_opcodes_dd_a16 = ops_nofpu_a16;
                x86_opcodes_dd_a32 = ops_nofpu_a32;
                x86_opcodes_de_a16 = ops_nofpu_a16;
                x86_opcodes_de_a32 = ops_nofpu_a32;
                x86_opcodes_df_a16 = ops_nofpu_a16;
                x86_opcodes_df_a32 = ops_nofpu_a32;
        }

        memset(&msr, 0, sizeof(msr));
        
        switch (cpu_s->cpu_type)
        {
                case CPU_8088:
                case CPU_8086:
                break;
                
                case CPU_286:
#ifdef DYNAREC
                x86_setopcodes(ops_286, ops_286_0f, dynarec_ops_286, dynarec_ops_286_0f);
#else
                x86_setopcodes(ops_286, ops_286_0f);
#endif                        
                timing_rr  = 2;   /*register dest - register src*/
                timing_rm  = 7;   /*register dest - memory src*/
                timing_mr  = 7;   /*memory dest   - register src*/
                timing_mm  = 7;   /*memory dest   - memory src*/
                timing_rml = 9;   /*register dest - memory src long*/
                timing_mrl = 11;  /*memory dest   - register src long*/
                timing_mml = 11;  /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3;   /*branch not taken*/
                break;

                case CPU_386SX:
                timing_rr  = 2;   /*register dest - register src*/
                timing_rm  = 6;   /*register dest - memory src*/
                timing_mr  = 7;   /*memory dest   - register src*/
                timing_mm  = 6;   /*memory dest   - memory src*/
                timing_rml = 8;   /*register dest - memory src long*/
                timing_mrl = 11;  /*memory dest   - register src long*/
                timing_mml = 10;  /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3;   /*branch not taken*/
                break;

                case CPU_386DX:
                timing_rr  = 2; /*register dest - register src*/
                timing_rm  = 6; /*register dest - memory src*/
                timing_mr  = 7; /*memory dest   - register src*/
                timing_mm  = 6; /*memory dest   - memory src*/
                timing_rml = 6; /*register dest - memory src long*/
                timing_mrl = 7; /*memory dest   - register src long*/
                timing_mml = 6; /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3; /*branch not taken*/
                break;
                
                case CPU_486SLC:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 3; /*register dest - memory src*/
                timing_mr  = 5; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 5; /*register dest - memory src long*/
                timing_mrl = 7; /*memory dest   - register src long*/
                timing_mml = 7;
                timing_bt  = 6-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                break;
                
                case CPU_486DLC:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 3; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 3; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 6-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                break;
                
                case CPU_i486SX:
                case CPU_i486DX:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                break;

                case CPU_Am486SX:
                case CPU_Am486DX:
                /*AMD timing identical to Intel*/
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                break;
                
                case CPU_Cx486S:
                case CPU_Cx486DX:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 3; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 3; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 4-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                break;
                
                case CPU_Cx5x86:
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 5-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                break;

                case CPU_WINCHIP:
#ifdef DYNAREC
                x86_setopcodes(ops_386, ops_winchip_0f, dynarec_ops_386, dynarec_ops_winchip_0f);
#else
                x86_setopcodes(ops_386, ops_winchip_0f);
#endif                        
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                break;

#ifdef DYNAREC                
                case CPU_PENTIUM:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 0;
                cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                codegen_timing_set(&codegen_timing_pentium);
                break;

                case CPU_PENTIUMMMX:
                x86_setopcodes(ops_386, ops_pentiummmx_0f, dynarec_ops_386, dynarec_ops_pentiummmx_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_hasrdtsc = 1;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_hasMMX = 1;
                cpu_hasMSR = 1;
                cpu_hasCR4 = 1;
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                codegen_timing_set(&codegen_timing_pentium);
                break;
#endif

                default:
                fatal("cpu_set : unknown CPU type %i\n", cpu_s->cpu_type);
        }
}

void cpu_CPUID()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_i486DX:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x756e6547;
                        EDX = 0x49656e69;
                        ECX = 0x6c65746e;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU; /*FPU*/
                }
                else
                   EAX = 0;
                break;

                case CPU_Am486SX:
                if (!EAX)
                {
                        EAX = 1;
                        EBX = 0x68747541;
                        ECX = 0x444D4163;
                        EDX = 0x69746E65;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = EDX = 0; /*No FPU*/
                }
                else
                   EAX = 0;
                break;

                case CPU_Am486DX:
                if (!EAX)
                {
                        EAX = 1;
                        EBX = 0x68747541;
                        ECX = 0x444D4163;
                        EDX = 0x69746E65;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU; /*FPU*/
                }
                else
                   EAX = 0;
                break;
                
                case CPU_WINCHIP:
                if (!EAX)
                {
                        EAX = 1;
                        if (msr.fcr2 & (1 << 14))
                        {
                                EBX = msr.fcr3 >> 32;
                                ECX = msr.fcr3 & 0xffffffff;
                                EDX = msr.fcr2 >> 32;
                        }
                        else
                        {
                                EBX = 0x746e6543; /*CentaurHauls*/
                                ECX = 0x736c7561;                        
                                EDX = 0x48727561;
                        }
                }
                else if (EAX == 1)
                {
                        EAX = 0x540;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
                        if (msr.fcr & (1 << 9))
                                EDX |= CPUID_MMX;
                }
                else
                   EAX = 0;
                break;
#ifdef DYNAREC
                case CPU_PENTIUM:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x756e6547;
                        EDX = 0x49656e69;
                        ECX = 0x6c65746e;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
                }
                else
                        EAX = 0;
                break;

                case CPU_PENTIUMMMX:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x756e6547;
                        EDX = 0x49656e69;
                        ECX = 0x6c65746e;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                }
                else
                        EAX = 0;
                break;
#endif
        }
}

void cpu_RDMSR()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_WINCHIP:
                EAX = EDX = 0;
                switch (ECX)
                {
                        case 0x02:
                        EAX = msr.tr1;
                        break;
                        case 0x0e:
                        EAX = msr.tr12;
                        break;
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                        case 0x11:
                        EAX = msr.cesr;
                        break;
                        case 0x107:
                        EAX = msr.fcr;
                        break;
                        case 0x108:
                        EAX = msr.fcr2 & 0xffffffff;
                        EDX = msr.fcr2 >> 32;
                        break;
                        case 0x10a:
                        EAX = cpu_multi & 3;
                        break;
                }
                break;
#ifdef DYNAREC
                case CPU_PENTIUM:
                case CPU_PENTIUMMMX:
                EAX = EDX = 0;
                switch (ECX)
                {
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                }
                break;
#endif
        }
}

void cpu_WRMSR()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_WINCHIP:
                switch (ECX)
                {
                        case 0x02:
                        msr.tr1 = EAX & 2;
                        break;
                        case 0x0e:
                        msr.tr12 = EAX & 0x228;
                        break;
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                        case 0x11:
                        msr.cesr = EAX & 0xff00ff;
                        break;
                        case 0x107:
                        msr.fcr = EAX;
                        cpu_hasMMX = EAX & (1 << 9);
                        if (EAX & (1 << 29))
                                CPUID = 0;
                        else
                                CPUID = models[model].cpu[cpu_manufacturer].cpus[cpu].cpuid_model;
                        break;
                        case 0x108:
                        msr.fcr2 = EAX | ((uint64_t)EDX << 32);
                        break;
                        case 0x109:
                        msr.fcr3 = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
#ifdef DYNAREC
                case CPU_PENTIUM:
                case CPU_PENTIUMMMX:
                switch (ECX)
                {
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
#endif
        }
}

static int cyrix_addr;

void cyrix_write(uint16_t addr, uint8_t val, void *priv)
{
        if (!(addr & 1)) cyrix_addr = val;
//        else pclog("Write Cyrix %02X %02X\n",cyrix_addr,val);
}

uint8_t cyrix_read(uint16_t addr, void *priv)
{
        if (addr & 1)
        {
                switch (cyrix_addr)
                {
                        case 0xfe: return models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id & 0xff;
                        case 0xff: return models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id >> 8;
                }
                if ((cyrix_addr & ~0xf0) == 0xc0) return 0xff;
                if (cyrix_addr == 0x20 && models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type == CPU_Cx5x86) return 0xff;
        }
        return 0xff;
}

#ifdef DYNAREC
void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f, OpFn *dynarec_opcodes, OpFn *dynarec_opcodes_0f)
{
        x86_opcodes = opcodes;
        x86_opcodes_0f = opcodes_0f;
        x86_dynarec_opcodes = dynarec_opcodes;
        x86_dynarec_opcodes_0f = dynarec_opcodes_0f;
}
#else
void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f)
{
        x86_opcodes = opcodes;
        x86_opcodes_0f = opcodes_0f;
}
#endif
