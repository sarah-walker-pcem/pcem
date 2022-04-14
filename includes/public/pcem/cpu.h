#ifndef _PCEM_CPU_H_
#define _PCEM_CPU_H_

#include <stdint.h>

typedef struct FPU
{
        const char *name;
        const char *internal_name;
        const int type;
} FPU;

typedef struct CPU
{
        char name[32];
        int cpu_type;
        const FPU *fpus;
        int speed;
        int rspeed;
        int multi;
        int pci_speed;
        uint32_t edx_reset;
        uint32_t cpuid_model;
        uint16_t cyrix_id;
        int cpu_flags;
        int mem_read_cycles, mem_write_cycles;
        int cache_read_cycles, cache_write_cycles;
        int atclk_div;
} CPU;

extern FPU fpus_none[];
extern FPU fpus_8088[];
extern FPU fpus_80286[];
extern FPU fpus_80386[];
extern FPU fpus_builtin[];
extern CPU cpus_8088[];
extern CPU cpus_pcjr[];
extern CPU cpus_europc[];
extern CPU cpus_8086[];
extern CPU cpus_pc1512[];
extern CPU cpus_286[];
extern CPU cpus_super286tr[];
extern CPU cpus_ibmat[];
extern CPU cpus_ibmxt286[];
extern CPU cpus_ps1_m2011[];
extern CPU cpus_ps2_m30_286[];
extern CPU cpus_i386SX[];
extern CPU cpus_i386DX[];
extern CPU cpus_acer[];
extern CPU cpus_Am386SX[];
extern CPU cpus_Am386DX[];
extern CPU cpus_486SLC[];
extern CPU cpus_486DLC[];
extern CPU cpus_i486[];
extern CPU cpus_Am486[];
extern CPU cpus_Cx486[];
extern CPU cpus_6x86[];
extern CPU cpus_6x86_SS7[];
extern CPU cpus_WinChip[];
extern CPU cpus_WinChip_SS7[];
extern CPU cpus_Pentium5V[];
extern CPU cpus_PentiumS5[];
extern CPU cpus_Pentium[];
extern CPU cpus_K6_S7[];
extern CPU cpus_K6_SS7[];
extern CPU cpus_PentiumPro[];
extern CPU cpus_Slot1_100MHz[];
extern CPU cpus_VIA_100MHz[];

#endif /* _PCEM_CPU_H_ */
