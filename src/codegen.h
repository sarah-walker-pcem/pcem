#ifdef DYNAREC

#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32
#include "codegen_x86.h"
#else
#error Dynamic recompiler not implemented on your platform
#endif

typedef struct codeblock_t
{
        struct codeblock_t *prev, *next;
        
        uint32_t pc;
        uint32_t _cs;
        uint32_t endpc;
        uint32_t checksum;
        uint32_t phys;
        uint32_t use32;
        int pnt;
        int ins;
        
        uint8_t data[8192];
} codeblock_t;

extern codeblock_t *codeblock;

extern codeblock_t **codeblock_hash;
extern uint8_t *codeblock_page_dirty;

void codegen_init();
void codegen_block_init(uint32_t phys_addr);
void codegen_block_remove();
void codegen_generate_call(uint8_t opcode, OpFn op, uint32_t fetchdat, uint32_t new_pc, uint32_t old_pc);
void codegen_generate_seg_restore();
void codegen_check_abrt();
void codegen_set_op32();
void codegen_flush();
void codegen_dirty(uint32_t addr);
int codegen_check_dirty(uint32_t phys_addr);

extern int cpu_block_end;

extern int cpu_recomp_blocks, cpu_recomp_ins, cpu_new_blocks;
extern int cpu_recomp_blocks_latched, cpu_recomp_ins_latched, cpu_new_blocks_latched;
extern int cpu_recomp_flushes, cpu_recomp_flushes_latched;
extern int cpu_recomp_evicted, cpu_recomp_evicted_latched;
extern int cpu_recomp_reuse, cpu_recomp_reuse_latched;
extern int cpu_recomp_removed, cpu_recomp_removed_latched;

extern int cpu_reps, cpu_reps_latched;
extern int cpu_notreps, cpu_notreps_latched;

extern int codegen_block_cycles;

extern void (*codegen_timing_start)();
extern void (*codegen_timing_prefix)(uint8_t prefix, uint32_t fetchdat);
extern void (*codegen_timing_opcode)(uint8_t opcode, uint32_t fetchdat, int op_32);
extern void (*codegen_timing_block_start)();
extern void (*codegen_timing_block_end)();

typedef struct codegen_timing_t
{
        void (*start)();
        void (*prefix)(uint8_t prefix, uint32_t fetchdat);
        void (*opcode)(uint8_t opcode, uint32_t fetchdat, int op_32);
        void (*block_start)();
        void (*block_end)();
} codegen_timing_t;

extern codegen_timing_t codegen_timing_pentium;
extern codegen_timing_t codegen_timing_486;

void codegen_timing_set(codegen_timing_t *timing);

#endif
