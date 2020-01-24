#include "ibm.h"
#include "cpu.h"
#include "model.h"
#include "io.h"
#include "x86_ops.h"
#include "mem.h"
#include "pci.h"
#include "codegen.h"
#include "x87_timings.h"

int fpu_type;
uint32_t cpu_features;

static int cpu_turbo_speed, cpu_nonturbo_speed;
static int cpu_turbo = 1;

int isa_cycles;
int has_vlb;
static uint8_t ccr0, ccr1, ccr2, ccr3, ccr4, ccr5, ccr6;

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
OpFn *x86_dynarec_opcodes_REPE;
OpFn *x86_dynarec_opcodes_REPNE;
OpFn *x86_dynarec_opcodes_3DNOW;

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
OpFn *x86_opcodes_REPE;
OpFn *x86_opcodes_REPNE;
OpFn *x86_opcodes_3DNOW;

enum
{
        CPUID_FPU = (1 << 0),
        CPUID_VME = (1 << 1),
        CPUID_PSE = (1 << 3),
        CPUID_TSC = (1 << 4),
        CPUID_MSR = (1 << 5),
        CPUID_CMPXCHG8B = (1 << 8),
        CPUID_CMOV = (1 << 15),
        CPUID_MMX = (1 << 23)
};

/*Addition flags returned by CPUID function 0x80000001*/
enum
{
        CPUID_3DNOW = (1 << 31)
};

int cpu = 3, cpu_manufacturer = 0;
CPU *cpu_s;
int cpu_multi;
int cpu_iscyrix;
int cpu_16bitbus;
int cpu_busspeed;
int cpu_use_dynarec;
int cpu_cyrix_alignment;

uint64_t cpu_CR4_mask;

int cpu_cycles_read, cpu_cycles_read_l, cpu_cycles_write, cpu_cycles_write_l;
int cpu_prefetch_cycles, cpu_prefetch_width, cpu_mem_prefetch_cycles, cpu_rom_prefetch_cycles;
int cpu_waitstates;
int cpu_cache_int_enabled, cpu_cache_ext_enabled;

int is386;
int is486;
int CPUID;

uint64_t tsc = 0;

int timing_rr;
int timing_mr, timing_mrl;
int timing_rm, timing_rml;
int timing_mm, timing_mml;
int timing_bt, timing_bnt;
int timing_int, timing_int_rm, timing_int_v86, timing_int_pm, timing_int_pm_outer;
int timing_iret_rm, timing_iret_v86, timing_iret_pm, timing_iret_pm_outer;
int timing_call_rm, timing_call_pm, timing_call_pm_gate, timing_call_pm_gate_inner;
int timing_retf_rm, timing_retf_pm, timing_retf_pm_outer;
int timing_jmp_rm, timing_jmp_pm, timing_jmp_pm_gate;
int timing_misaligned;

static struct
{
        uint32_t tr1, tr12;
        uint32_t cesr;
        uint32_t fcr;
        uint64_t fcr2, fcr3;
} msr;

void cpu_set_edx()
{
        EDX = models[model].cpu[cpu_manufacturer].cpus[cpu].edx_reset;
}

int fpu_get_type(int model, int manu, int cpu, const char *internal_name)
{
        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
        const FPU *fpus = cpu_s->fpus;
        int fpu_type = fpus[0].type;
        int c = 0;
        
        while (fpus[c].internal_name)
        {
                if (!strcmp(internal_name, fpus[c].internal_name))
                        fpu_type = fpus[c].type;
                c++;
        }
        
        return fpu_type;
}

const char *fpu_get_internal_name(int model, int manu, int cpu, int type)
{
        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
        const FPU *fpus = cpu_s->fpus;
        int c = 0;

        while (fpus[c].internal_name)
        {
                if (fpus[c].type == type)
                        return fpus[c].internal_name;
                c++;
        }

        return fpus[0].internal_name;
}

const char *fpu_get_name_from_index(int model, int manu, int cpu, int c)
{
        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
        const FPU *fpus = cpu_s->fpus;
        
        return fpus[c].name;
}

int fpu_get_type_from_index(int model, int manu, int cpu, int c)
{
        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
        const FPU *fpus = cpu_s->fpus;

        return fpus[c].type;
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
        is8086   = (cpu_s->cpu_type > CPU_8088);
        is386    = (cpu_s->cpu_type >= CPU_386SX);
        is486    = (cpu_s->cpu_type >= CPU_i486SX) || (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC);
        hasfpu   = (fpu_type != FPU_NONE);

         cpu_iscyrix = (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC || cpu_s->cpu_type == CPU_Cx486S || cpu_s->cpu_type == CPU_Cx486DX || cpu_s->cpu_type == CPU_Cx5x86 || cpu_s->cpu_type == CPU_Cx6x86 || cpu_s->cpu_type == CPU_Cx6x86MX || cpu_s->cpu_type == CPU_Cx6x86L || cpu_s->cpu_type == CPU_CxGX1);
        cpu_16bitbus = (cpu_s->cpu_type == CPU_286 || cpu_s->cpu_type == CPU_386SX || cpu_s->cpu_type == CPU_486SLC);
        if (cpu_s->multi) 
           cpu_busspeed = cpu_s->rspeed / cpu_s->multi;
        cpu_multi = cpu_s->multi;
        ccr0 = ccr1 = ccr2 = ccr3 = ccr4 = ccr5 = ccr6 = 0;
        has_vlb = (cpu_s->cpu_type >= CPU_i486SX) && (cpu_s->cpu_type <= CPU_Cx5x86);

        cpu_turbo_speed = cpu_s->rspeed;
        if (cpu_s->cpu_type < CPU_286)
                cpu_nonturbo_speed = 4772728;
        else if (cpu_s->rspeed < 8000000)
                cpu_nonturbo_speed = cpu_s->rspeed;
        else
                cpu_nonturbo_speed = 8000000;

        cpu_update_waitstates();
  
        isa_cycles = cpu_s->atclk_div;      
        
        if (cpu_s->rspeed <= 8000000)
                cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
        else
                cpu_rom_prefetch_cycles = cpu_s->rspeed / 1000000;

        if (cpu_s->pci_speed)
        {
                pci_nonburst_time = 4*cpu_s->rspeed / cpu_s->pci_speed;
                pci_burst_time = cpu_s->rspeed / cpu_s->pci_speed;
        }
        else
        {
                pci_nonburst_time = 4;
                pci_burst_time = 1;
        }
        pclog("PCI burst=%i nonburst=%i\n", pci_burst_time, pci_nonburst_time);

        if (cpu_iscyrix)
           io_sethandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
        else
           io_removehandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
        
        pclog("hasfpu - %i\n",hasfpu);
        pclog("is486 - %i  %i\n",is486,cpu_s->cpu_type);

        x86_setopcodes(ops_386, ops_386_0f, dynarec_ops_386, dynarec_ops_386_0f);
        x86_opcodes_REPE = ops_REPE;
        x86_opcodes_REPNE = ops_REPNE;
        x86_opcodes_3DNOW = ops_3DNOW;
        x86_dynarec_opcodes_REPE = dynarec_ops_REPE;
        x86_dynarec_opcodes_REPNE = dynarec_ops_REPNE;
        x86_dynarec_opcodes_3DNOW = dynarec_ops_3DNOW;

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
                x86_dynarec_opcodes_d8_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_d9_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_d9_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_da_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_da_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_db_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_dc_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_dc_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_dd_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_dd_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_de_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_de_a32 = dynarec_ops_nofpu_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_nofpu_a16;
                x86_dynarec_opcodes_df_a32 = dynarec_ops_nofpu_a32;
        }
        codegen_timing_set(&codegen_timing_486);

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

        timing_misaligned = 0;
        cpu_cyrix_alignment = 0;
        
        switch (cpu_s->cpu_type)
        {
                case CPU_8088:
                case CPU_8086:
                break;
                
                case CPU_286:
                x86_setopcodes(ops_286, ops_286_0f, dynarec_ops_286, dynarec_ops_286_0f);
                timing_rr  = 2;   /*register dest - register src*/
                timing_rm  = 7;   /*register dest - memory src*/
                timing_mr  = 7;   /*memory dest   - register src*/
                timing_mm  = 7;   /*memory dest   - memory src*/
                timing_rml = 9;   /*register dest - memory src long*/
                timing_mrl = 11;  /*memory dest   - register src long*/
                timing_mml = 11;  /*memory dest   - memory src*/
                timing_bt  = 7-3; /*branch taken*/
                timing_bnt = 3;   /*branch not taken*/
                timing_int = 0;
                timing_int_rm       = 23;
                timing_int_v86      = 0;
                timing_int_pm       = 40;
                timing_int_pm_outer = 78;
                timing_iret_rm       = 17;
                timing_iret_v86      = 0;
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 55;
                timing_call_rm            = 13;
                timing_call_pm            = 26;
                timing_call_pm_gate       = 52;
                timing_call_pm_gate_inner = 82;
                timing_retf_rm       = 15;
                timing_retf_pm       = 25;
                timing_retf_pm_outer = 55;
                timing_jmp_rm      = 11;
                timing_jmp_pm      = 23;
                timing_jmp_pm_gate = 38;
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
                timing_int = 0;
                timing_int_rm       = 37;
                timing_int_v86      = 59;
                timing_int_pm       = 99;
                timing_int_pm_outer = 119;
                timing_iret_rm       = 22;
                timing_iret_v86      = 60;
                timing_iret_pm       = 38;
                timing_iret_pm_outer = 82;
                timing_call_rm            = 17;
                timing_call_pm            = 34;
                timing_call_pm_gate       = 52;
                timing_call_pm_gate_inner = 86;
                timing_retf_rm       = 18;
                timing_retf_pm       = 32;
                timing_retf_pm_outer = 68;
                timing_jmp_rm      = 12;
                timing_jmp_pm      = 27;
                timing_jmp_pm_gate = 45;
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
                timing_int = 0;
                timing_int_rm       = 37;
                timing_int_v86      = 59;
                timing_int_pm       = 99;
                timing_int_pm_outer = 119;
                timing_iret_rm       = 22;
                timing_iret_v86      = 60;
                timing_iret_pm       = 38;
                timing_iret_pm_outer = 82;
                timing_call_rm            = 17;
                timing_call_pm            = 34;
                timing_call_pm_gate       = 52;
                timing_call_pm_gate_inner = 86;
                timing_retf_rm       = 18;
                timing_retf_pm       = 32;
                timing_retf_pm_outer = 68;
                timing_jmp_rm      = 12;
                timing_jmp_pm      = 27;
                timing_jmp_pm_gate = 45;
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
                /*unknown*/
                timing_int = 4;
                timing_int_rm       = 14;
                timing_int_v86      = 82;
                timing_int_pm       = 49;
                timing_int_pm_outer = 77;
                timing_iret_rm       = 14;
                timing_iret_v86      = 66;
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 66;
                timing_call_rm = 12;
                timing_call_pm = 30;
                timing_call_pm_gate = 41;
                timing_call_pm_gate_inner = 83;
                timing_retf_rm       = 13;
                timing_retf_pm       = 26;
                timing_retf_pm_outer = 61;
                timing_jmp_rm      = 9;
                timing_jmp_pm      = 26;
                timing_jmp_pm_gate = 37;
                timing_misaligned = 3;
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
                /*unknown*/
                timing_int = 4;
                timing_int_rm       = 14;
                timing_int_v86      = 82;
                timing_int_pm       = 49;
                timing_int_pm_outer = 77;
                timing_iret_rm       = 14;
                timing_iret_v86      = 66;
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 66;
                timing_call_rm = 12;
                timing_call_pm = 30;
                timing_call_pm_gate = 41;
                timing_call_pm_gate_inner = 83;
                timing_retf_rm       = 13;
                timing_retf_pm       = 26;
                timing_retf_pm_outer = 61;
                timing_jmp_rm      = 9;
                timing_jmp_pm      = 26;
                timing_jmp_pm_gate = 37;
                timing_misaligned = 3;
                break;

                case CPU_iDX4:
                cpu_features = CPU_FEATURE_CR4 | CPU_FEATURE_VME;
                cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_VME;
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
                timing_int = 4;
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 15;
                timing_iret_v86      = 36; /*unknown*/
                timing_iret_pm       = 20;
                timing_iret_pm_outer = 36;
                timing_call_rm = 18;
                timing_call_pm = 20;
                timing_call_pm_gate = 35;
                timing_call_pm_gate_inner = 69;
                timing_retf_rm       = 13;
                timing_retf_pm       = 17;
                timing_retf_pm_outer = 35;
                timing_jmp_rm      = 17;
                timing_jmp_pm      = 19;
                timing_jmp_pm_gate = 32;
                timing_misaligned = 3;
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
                timing_int = 4;
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 15;
                timing_iret_v86      = 36; /*unknown*/
                timing_iret_pm       = 20;
                timing_iret_pm_outer = 36;
                timing_call_rm = 18;
                timing_call_pm = 20;
                timing_call_pm_gate = 35;
                timing_call_pm_gate_inner = 69;
                timing_retf_rm       = 13;
                timing_retf_pm       = 17;
                timing_retf_pm_outer = 35;
                timing_jmp_rm      = 17;
                timing_jmp_pm      = 19;
                timing_jmp_pm_gate = 32;
                timing_misaligned = 3;
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
                timing_int = 4;
                timing_int_rm       = 14;
                timing_int_v86      = 82;
                timing_int_pm       = 49;
                timing_int_pm_outer = 77;
                timing_iret_rm       = 14;
                timing_iret_v86      = 66; /*unknown*/
                timing_iret_pm       = 31;
                timing_iret_pm_outer = 66;
                timing_call_rm = 12;
                timing_call_pm = 30;
                timing_call_pm_gate = 41;
                timing_call_pm_gate_inner = 83;
                timing_retf_rm       = 13;
                timing_retf_pm       = 26;
                timing_retf_pm_outer = 61;
                timing_jmp_rm      = 9;
                timing_jmp_pm      = 26;
                timing_jmp_pm_gate = 37;
                timing_misaligned = 3;
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
                timing_int = 0;
                timing_int_rm       = 9;
                timing_int_v86      = 82; /*unknown*/
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 4;
                timing_call_pm = 15;
                timing_call_pm_gate = 26;
                timing_call_pm_gate_inner = 35;
                timing_retf_rm       = 4;
                timing_retf_pm       = 7;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 5;
                timing_jmp_pm      = 7;
                timing_jmp_pm_gate = 17;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                break;

                case CPU_WINCHIP:
                x86_setopcodes(ops_386, ops_winchip_0f, dynarec_ops_386, dynarec_ops_winchip_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MMX | CPU_FEATURE_MSR | CPU_FEATURE_CR4;
                msr.fcr = (1 << 8) | (1 << 16);
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                /*unknown*/
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 4;
                timing_call_pm = 15;
                timing_call_pm_gate = 26;
                timing_call_pm_gate_inner = 35;
                timing_retf_rm       = 4;
                timing_retf_pm       = 7;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 5;
                timing_jmp_pm      = 7;
                timing_jmp_pm_gate = 17;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                codegen_timing_set(&codegen_timing_winchip);
                break;

                case CPU_WINCHIP2:
                x86_setopcodes(ops_386, ops_winchip2_0f, dynarec_ops_386, dynarec_ops_winchip2_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 3-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MMX | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_3DNOW;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 18) | (1 << 19) | (1 << 20) | (1 << 21);
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
                /*unknown*/
                timing_int_rm       = 26;
                timing_int_v86      = 82;
                timing_int_pm       = 44;
                timing_int_pm_outer = 71;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 4;
                timing_call_pm = 15;
                timing_call_pm_gate = 26;
                timing_call_pm_gate_inner = 35;
                timing_retf_rm       = 4;
                timing_retf_pm       = 7;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 5;
                timing_jmp_pm      = 7;
                timing_jmp_pm_gate = 17;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                codegen_timing_set(&codegen_timing_winchip2);
                break;

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
                timing_int = 6;
                timing_int_rm       = 11;
                timing_int_v86      = 54;
                timing_int_pm       = 25;
                timing_int_pm_outer = 42;
                timing_iret_rm       = 7;
                timing_iret_v86      = 27; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 27;
                timing_call_rm = 4;
                timing_call_pm = 4;
                timing_call_pm_gate = 22;
                timing_call_pm_gate_inner = 44;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 3;
                timing_jmp_pm      = 3;
                timing_jmp_pm_gate = 18;
                timing_misaligned = 3;
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
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
                timing_int = 6;
                timing_int_rm       = 11;
                timing_int_v86      = 54;
                timing_int_pm       = 25;
                timing_int_pm_outer = 42;
                timing_iret_rm       = 7;
                timing_iret_v86      = 27; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 27;
                timing_call_rm = 4;
                timing_call_pm = 4;
                timing_call_pm_gate = 22;
                timing_call_pm_gate_inner = 44;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 3;
                timing_jmp_pm      = 3;
                timing_jmp_pm_gate = 18;
                timing_misaligned = 3;
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
                codegen_timing_set(&codegen_timing_pentium);
                break;

  		case CPU_Cx6x86:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int_rm       = 9;
                timing_int_v86      = 46;
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 3;
                timing_call_pm = 4;
                timing_call_pm_gate = 15;
                timing_call_pm_gate_inner = 26;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 1;
                timing_jmp_pm      = 4;
                timing_jmp_pm_gate = 14;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                cpu_features = CPU_FEATURE_RDTSC;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
  		codegen_timing_set(&codegen_timing_686);
  		CPUID = 0; /*Disabled on powerup by default*/
                break;

                case CPU_Cx6x86L:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int_rm       = 9;
                timing_int_v86      = 46;
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 3;
                timing_call_pm = 4;
                timing_call_pm_gate = 15;
                timing_call_pm_gate_inner = 26;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 1;
                timing_jmp_pm      = 4;
                timing_jmp_pm_gate = 14;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                cpu_features = CPU_FEATURE_RDTSC;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
         	codegen_timing_set(&codegen_timing_686);
         	ccr4 = 0x80;
                break;


                case CPU_CxGX1:
                x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 5-1; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_PCE;
         	codegen_timing_set(&codegen_timing_686);
                break;

  
                case CPU_Cx6x86MX:
                x86_setopcodes(ops_386, ops_c6x86mx_0f, dynarec_ops_386, dynarec_ops_c6x86mx_0f);
                x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
                x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
                x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
                x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
                x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
                x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
                x86_opcodes_da_a16 = ops_fpu_686_da_a16;
                x86_opcodes_da_a32 = ops_fpu_686_da_a32;
                x86_opcodes_db_a16 = ops_fpu_686_db_a16;
                x86_opcodes_db_a32 = ops_fpu_686_db_a32;
                x86_opcodes_df_a16 = ops_fpu_686_df_a16;
                x86_opcodes_df_a32 = ops_fpu_686_df_a32;
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 1; /*register dest - memory src*/
                timing_mr  = 2; /*memory dest   - register src*/
                timing_mm  = 2;
                timing_rml = 1; /*register dest - memory src long*/
                timing_mrl = 2; /*memory dest   - register src long*/
                timing_mml = 2;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 2; /*branch not taken*/
                timing_int_rm       = 9;
                timing_int_v86      = 46;
                timing_int_pm       = 21;
                timing_int_pm_outer = 32;
                timing_iret_rm       = 7;
                timing_iret_v86      = 26;
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 26;
                timing_call_rm = 3;
                timing_call_pm = 4;
                timing_call_pm_gate = 15;
                timing_call_pm_gate_inner = 26;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 1;
                timing_jmp_pm      = 4;
                timing_jmp_pm_gate = 14;
                timing_misaligned = 2;
                cpu_cyrix_alignment = 1;
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_MMX;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_PCE;
         	codegen_timing_set(&codegen_timing_686);
         	ccr4 = 0x80;
                break;

                case CPU_K6:
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
                timing_int = 6;
                timing_int_rm       = 11;
                timing_int_v86      = 54;
                timing_int_pm       = 25;
                timing_int_pm_outer = 42;
                timing_iret_rm       = 7;
                timing_iret_v86      = 27; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 27;
                timing_call_rm = 4;
                timing_call_pm = 4;
                timing_call_pm_gate = 22;
                timing_call_pm_gate_inner = 44;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 3;
                timing_jmp_pm      = 3;
                timing_jmp_pm_gate = 18;
                timing_misaligned = 3;
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE;
                codegen_timing_set(&codegen_timing_k6);
                break;

                case CPU_K6_2:
                case CPU_K6_3:
                case CPU_K6_2P:
                case CPU_K6_3P:
                x86_setopcodes(ops_386, ops_winchip2_0f, dynarec_ops_386, dynarec_ops_winchip2_0f);
                timing_rr  = 1; /*register dest - register src*/
                timing_rm  = 2; /*register dest - memory src*/
                timing_mr  = 3; /*memory dest   - register src*/
                timing_mm  = 3;
                timing_rml = 2; /*register dest - memory src long*/
                timing_mrl = 3; /*memory dest   - register src long*/
                timing_mml = 3;
                timing_bt  = 0; /*branch taken*/
                timing_bnt = 1; /*branch not taken*/
                timing_int = 6;
                timing_int_rm       = 11;
                timing_int_v86      = 54;
                timing_int_pm       = 25;
                timing_int_pm_outer = 42;
                timing_iret_rm       = 7;
                timing_iret_v86      = 27; /*unknown*/
                timing_iret_pm       = 10;
                timing_iret_pm_outer = 27;
                timing_call_rm = 4;
                timing_call_pm = 4;
                timing_call_pm_gate = 22;
                timing_call_pm_gate_inner = 44;
                timing_retf_rm       = 4;
                timing_retf_pm       = 4;
                timing_retf_pm_outer = 23;
                timing_jmp_rm      = 3;
                timing_jmp_pm      = 3;
                timing_jmp_pm_gate = 18;
                timing_misaligned = 3;
                cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX | CPU_FEATURE_3DNOW;
                msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
                cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE;
                codegen_timing_set(&codegen_timing_k6);
                break;

                default:
                fatal("cpu_set : unknown CPU type %i\n", cpu_s->cpu_type);
        }
        
        switch (fpu_type)
        {
                case FPU_NONE:
                break;

                case FPU_8087:
                x87_timings = x87_timings_8087;
                break;

                case FPU_287:
                x87_timings = x87_timings_287;
                break;

                case FPU_287XL:
                case FPU_387:
                x87_timings = x87_timings_387;
                break;
                
                default:
                x87_timings = x87_timings_486;
                break;
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
                        EAX = EBX = ECX = EDX = 0;
                break;

                case CPU_iDX4:
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
                        EDX = CPUID_FPU | CPUID_VME;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
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
                        EAX = EBX = ECX = EDX = 0;
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
                        EAX = EBX = ECX = EDX = 0;
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
			if (cpu_has_feature(CPU_FEATURE_CX8))
				EDX |= CPUID_CMPXCHG8B;
                        if (msr.fcr & (1 << 9))
                                EDX |= CPUID_MMX;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
                break;
                
                case CPU_WINCHIP2:
                switch (EAX)
                {
                        case 0:
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
                        break;
                        case 1:
                        EAX = 0x580;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
			if (cpu_has_feature(CPU_FEATURE_CX8))
				EDX |= CPUID_CMPXCHG8B;							
                        if (msr.fcr & (1 << 9))
                                EDX |= CPUID_MMX;
                        break;
                        case 0x80000000:
                        EAX = 0x80000005;
                        break;
                        case 0x80000001:
                        EAX = 0x580;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
			if (cpu_has_feature(CPU_FEATURE_CX8))
				EDX |= CPUID_CMPXCHG8B;
                        if (msr.fcr & (1 << 9))
                                EDX |= CPUID_MMX;
			if (cpu_has_feature(CPU_FEATURE_3DNOW))
                                EDX |= CPUID_3DNOW;
                        break;
                                
                        case 0x80000002: /*Processor name string*/
                        EAX = 0x20544449; /*IDT WinChip 2-3D*/
                        EBX = 0x436e6957;
                        ECX = 0x20706968;
                        EDX = 0x44332d32;
                        break;
                        
                        case 0x80000005: /*Cache information*/
                        EBX = 0x08800880; /*TLBs*/
                        ECX = 0x20040120; /*L1 data cache*/
                        EDX = 0x20020120; /*L1 instruction cache*/
                        break;
                        
                        default:
                        EAX = EBX = ECX = EDX = 0;
                        break;
                }
                break;

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
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
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
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
                break;


                case CPU_Cx6x86:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
                break;


                case CPU_Cx6x86L:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_CMPXCHG8B;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
                break;


                case CPU_CxGX1:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
                break;



                case CPU_Cx6x86MX:
                if (!EAX)
                {
                        EAX = 0x00000001;
                        EBX = 0x69727943;
                        EDX = 0x736e4978;
                        ECX = 0x64616574;
                }
                else if (EAX == 1)
                {
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_CMOV | CPUID_MMX;
                }
                else
                        EAX = EBX = ECX = EDX = 0;
                break;

                case CPU_K6:
                switch (EAX)
                {
                        case 0:
                        EAX = 1;
                        EBX = 0x68747541; /*AuthenticAMD*/
                        ECX = 0x444d4163;
                        EDX = 0x69746e65;
                        break;
                        case 1:
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                        break;
                        case 0x80000000:
                        EAX = 0x80000005;
                        break;
                        case 0x80000001:
                        EAX = CPUID+0x100;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                        break;

                        case 0x80000002: /*Processor name string*/
                        EAX = 0x2d444d41; /*AMD-K6tm w/ mult*/
                        EBX = 0x6d74364b;
                        ECX = 0x202f7720;
                        EDX = 0x746c756d;
                        break;

                        case 0x80000003: /*Processor name string*/
                        EAX = 0x64656d69; /*imedia extension*/
                        EBX = 0x65206169;
                        ECX = 0x6e657478;
                        EDX = 0x6e6f6973;
                        break;

                        case 0x80000004: /*Processor name string*/
                        EAX = 0x00000073; /*s*/
                        EBX = 0x00000000;
                        ECX = 0x00000000;
                        EDX = 0x00000000;
                        break;

                        case 0x80000005: /*Cache information*/
                        EBX = 0x02800140; /*TLBs*/
                        ECX = 0x20020220; /*L1 data cache*/
                        EDX = 0x20020220; /*L1 instruction cache*/
                        break;

                        default:
                        EAX = EBX = ECX = EDX = 0;
                        break;
                }
                break;

                case CPU_K6_2:
                switch (EAX)
                {
                        case 0:
                        EAX = 1;
                        EBX = 0x68747541; /*AuthenticAMD*/
                        ECX = 0x444d4163;
                        EDX = 0x69746e65;
                        break;
                        case 1:
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                        break;
                        case 0x80000000:
                        EAX = 0x80000005;
                        break;
                        case 0x80000001:
                        EAX = CPUID+0x100;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX | CPUID_3DNOW;
                        break;

                        case 0x80000002: /*Processor name string*/
                        EAX = 0x2d444d41; /*AMD-K6(tm) 3D pr*/
                        EBX = 0x7428364b;
                        ECX = 0x3320296d;
                        EDX = 0x72702044;
                        break;

                        case 0x80000003: /*Processor name string*/
                        EAX = 0x7365636f; /*ocessor*/
                        EBX = 0x00726f73;
                        ECX = 0x00000000;
                        EDX = 0x00000000;
                        break;

                        case 0x80000005: /*Cache information*/
                        EBX = 0x02800140; /*TLBs*/
                        ECX = 0x20020220; /*L1 data cache*/
                        EDX = 0x20020220; /*L1 instruction cache*/
                        break;

                        default:
                        EAX = EBX = ECX = EDX = 0;
                        break;
                }
                break;

                case CPU_K6_3:
                switch (EAX)
                {
                        case 0:
                        EAX = 1;
                        EBX = 0x68747541; /*AuthenticAMD*/
                        ECX = 0x444d4163;
                        EDX = 0x69746e65;
                        break;
                        case 1:
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                        break;
                        case 0x80000000:
                        EAX = 0x80000006;
                        break;
                        case 0x80000001:
                        EAX = CPUID+0x100;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX | CPUID_3DNOW;
                        break;

                        case 0x80000002: /*Processor name string*/
                        EAX = 0x2d444d41; /*AMD-K6(tm) 3D+ P*/
                        EBX = 0x7428364b;
                        ECX = 0x3320296d;
                        EDX = 0x50202b44;
                        break;

                        case 0x80000003: /*Processor name string*/
                        EAX = 0x65636f72; /*rocessor*/
                        EBX = 0x726f7373;
                        ECX = 0x00000000;
                        EDX = 0x00000000;
                        break;

                        case 0x80000005: /*Cache information*/
                        EBX = 0x02800140; /*TLBs*/
                        ECX = 0x20020220; /*L1 data cache*/
                        EDX = 0x20020220; /*L1 instruction cache*/
                        break;

                        case 0x80000006: /*L2 Cache information*/
                        ECX = 0x01004220;
                        break;
                        
                        default:
                        EAX = EBX = ECX = EDX = 0;
                        break;
                }
                break;

                case CPU_K6_2P:
                case CPU_K6_3P:
                switch (EAX)
                {
                        case 0:
                        EAX = 1;
                        EBX = 0x68747541; /*AuthenticAMD*/
                        ECX = 0x444d4163;
                        EDX = 0x69746e65;
                        break;
                        case 1:
                        EAX = CPUID;
                        EBX = ECX = 0;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
                        break;
                        case 0x80000000:
                        EAX = 0x80000007;
                        break;
                        case 0x80000001:
                        EAX = CPUID+0x100;
                        EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX | CPUID_3DNOW;
                        break;

                        case 0x80000002: /*Processor name string*/
                        EAX = 0x2d444d41; /*AMD-K6(tm)-III P*/
                        EBX = 0x7428364b;
                        ECX = 0x492d296d;
                        EDX = 0x50204949;
                        break;

                        case 0x80000003: /*Processor name string*/
                        EAX = 0x65636f72; /*rocessor*/
                        EBX = 0x726f7373;
                        ECX = 0x00000000;
                        EDX = 0x00000000;
                        break;

                        case 0x80000005: /*Cache information*/
                        EBX = 0x02800140; /*TLBs*/
                        ECX = 0x20020220; /*L1 data cache*/
                        EDX = 0x20020220; /*L1 instruction cache*/
                        break;

                        case 0x80000006: /*L2 Cache information*/
                        if (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type == CPU_K6_3P)
                                ECX = 0x01004220;
                        else
                                ECX = 0x00804220;
                        break;

                        case 0x80000007: /*PowerNow information*/
                        EDX = 7;
                        break;

                        default:
                        EAX = EBX = ECX = EDX = 0;
                        break;
                }
                break;
        }
}

void cpu_RDMSR()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_WINCHIP:
                case CPU_WINCHIP2:
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
                case CPU_Cx6x86:
                case CPU_Cx6x86L:
                case CPU_CxGX1:
                case CPU_Cx6x86MX:
                switch (ECX)
                {
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                }
 		break;
                case CPU_K6:
                case CPU_K6_2:
                case CPU_K6_3:
                case CPU_K6_2P:
                case CPU_K6_3P:
                EAX = EDX = 0;
                switch (ECX)
                {
                        case 0x10:
                        EAX = tsc & 0xffffffff;
                        EDX = tsc >> 32;
                        break;
                }
                break;
        }
}

void cpu_WRMSR()
{
        switch (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type)
        {
                case CPU_WINCHIP:
                case CPU_WINCHIP2:
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
                        if (EAX & (1 << 9))
                                cpu_features |= CPU_FEATURE_MMX;
                        else
                                cpu_features &= ~CPU_FEATURE_MMX;
			if (EAX & (1 << 1))
                                cpu_features |= CPU_FEATURE_CX8;
			else
                                cpu_features &= ~CPU_FEATURE_CX8;
			if ((EAX & (1 << 20)) && models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type >= CPU_WINCHIP2)
                                cpu_features |= CPU_FEATURE_3DNOW;
			else
                                cpu_features &= ~CPU_FEATURE_3DNOW;
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

                case CPU_PENTIUM:
                case CPU_PENTIUMMMX:
                switch (ECX)
                {
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
                case CPU_Cx6x86:
                case CPU_Cx6x86L:
                case CPU_CxGX1:
                case CPU_Cx6x86MX:
                switch (ECX)
                {
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
                case CPU_K6:
                case CPU_K6_2:
                case CPU_K6_3:
                case CPU_K6_2P:
                case CPU_K6_3P:
                switch (ECX)
                {
                        case 0x10:
                        tsc = EAX | ((uint64_t)EDX << 32);
                        break;
                }
                break;
        }
}

static int cyrix_addr;

void cyrix_write(uint16_t addr, uint8_t val, void *priv)
{
        if (!(addr & 1))
                cyrix_addr = val;
        else switch (cyrix_addr)
        {
                case 0xc0: /*CCR0*/
                ccr0 = val;
                break;
                case 0xc1: /*CCR1*/
                ccr1 = val;
                break;
                case 0xc2: /*CCR2*/
                ccr2 = val;
                break;
                case 0xc3: /*CCR3*/
                ccr3 = val;
                break;
                case 0xe8: /*CCR4*/
                if ((ccr3 & 0xf0) == 0x10)
                {
                        ccr4 = val;
                        if (models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type >= CPU_Cx6x86)
                        {
                                if (val & 0x80)
                                        CPUID = models[model].cpu[cpu_manufacturer].cpus[cpu].cpuid_model;
                                else
                                        CPUID = 0;
                        }
                }
                break;
                case 0xe9: /*CCR5*/
                if ((ccr3 & 0xf0) == 0x10)
                        ccr5 = val;
                break;
                case 0xea: /*CCR6*/
                if ((ccr3 & 0xf0) == 0x10)
                        ccr6 = val;
                break;
        }
}

uint8_t cyrix_read(uint16_t addr, void *priv)
{
        if (addr & 1)
        {
                switch (cyrix_addr)
                {
                        case 0xc0: return ccr0;
                        case 0xc1: return ccr1;
                        case 0xc2: return ccr2;
                        case 0xc3: return ccr3;
                        case 0xe8: return ((ccr3 & 0xf0) == 0x10) ? ccr4 : 0xff;
                        case 0xe9: return ((ccr3 & 0xf0) == 0x10) ? ccr5 : 0xff;
                        case 0xea: return ((ccr3 & 0xf0) == 0x10) ? ccr6 : 0xff;
                        case 0xfe: return models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id & 0xff;
                        case 0xff: return models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id >> 8;
                }
                if ((cyrix_addr & ~0xf0) == 0xc0) return 0xff;
                if (cyrix_addr == 0x20 && models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type == CPU_Cx5x86) return 0xff;
        }
        return 0xff;
}

void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f, OpFn *dynarec_opcodes, OpFn *dynarec_opcodes_0f)
{
        x86_opcodes = opcodes;
        x86_opcodes_0f = opcodes_0f;
        x86_dynarec_opcodes = dynarec_opcodes;
        x86_dynarec_opcodes_0f = dynarec_opcodes_0f;
}

void cpu_update_waitstates()
{
        cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];
        
        if (is486)
                cpu_prefetch_width = 16;
        else
                cpu_prefetch_width = cpu_16bitbus ? 2 : 4;
        
        if (cpu_cache_int_enabled)
        {
                /* Disable prefetch emulation */
                cpu_prefetch_cycles = 0;
        }
        else if (cpu_waitstates && (cpu_s->cpu_type >= CPU_286 && cpu_s->cpu_type <= CPU_386DX))
        {
                /* Waitstates override */
                cpu_prefetch_cycles = cpu_waitstates+1;
                cpu_cycles_read = cpu_waitstates+1;
                cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
                cpu_cycles_write = cpu_waitstates+1;
                cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
        }
        else if (cpu_cache_ext_enabled)
        {
                /* Use cache timings */
                cpu_prefetch_cycles = cpu_s->cache_read_cycles;
                cpu_cycles_read = cpu_s->cache_read_cycles;
                cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu_s->cache_read_cycles;
                cpu_cycles_write = cpu_s->cache_write_cycles;
                cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu_s->cache_write_cycles;
        }
        else
        {
                /* Use memory timings */
                cpu_prefetch_cycles = cpu_s->mem_read_cycles;
                cpu_cycles_read = cpu_s->mem_read_cycles;
                cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu_s->mem_read_cycles;
                cpu_cycles_write = cpu_s->mem_write_cycles;
                cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu_s->mem_write_cycles;
        }
        if (is486)
                cpu_prefetch_cycles = (cpu_prefetch_cycles * 11) / 16;
        cpu_mem_prefetch_cycles = cpu_prefetch_cycles;
        if (cpu_s->rspeed <= 8000000)
                cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
}

void cpu_set_turbo(int turbo)
{
        if (cpu_turbo != turbo)
        {
                cpu_turbo = turbo;

                cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];
                if (cpu_s->cpu_type >= CPU_286)
                {
                        if (cpu_turbo)
                                setpitclock(cpu_turbo_speed);
                        else
                                setpitclock(cpu_nonturbo_speed);
                }
                else
                        setpitclock(14318184.0);
        }
}

int cpu_get_speed()
{
        if (cpu_turbo)
                return cpu_turbo_speed;
        return cpu_nonturbo_speed;
}
