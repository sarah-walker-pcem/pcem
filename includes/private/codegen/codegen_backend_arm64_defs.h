#ifndef _CODEGEN_BACKEND_ARM64_DEFS_H_
#define _CODEGEN_BACKEND_ARM64_DEFS_H_

#define REG_W0 0
#define REG_W1 1
#define REG_W2 2
#define REG_W3 3
#define REG_W4 4
#define REG_W5 5
#define REG_W6 6
#define REG_W7 7
#define REG_W8 8
#define REG_W9 9
#define REG_W10 10
#define REG_W11 11
#define REG_W12 12
#define REG_W13 13
#define REG_W14 14
#define REG_W15 15
#define REG_W16 16
#define REG_W17 17
#define REG_W18 18
#define REG_W19 19
#define REG_W20 20
#define REG_W21 21
#define REG_W22 22
#define REG_W23 23
#define REG_W24 24
#define REG_W25 25
#define REG_W26 26
#define REG_W27 27
#define REG_W28 28
#define REG_W29 29
#define REG_W30 30
#define REG_WZR 31

#define REG_X0 0
#define REG_X1 1
#define REG_X2 2
#define REG_X3 3
#define REG_X4 4
#define REG_X5 5
#define REG_X6 6
#define REG_X7 7
#define REG_X8 8
#define REG_X9 9
#define REG_X10 10
#define REG_X11 11
#define REG_X12 12
#define REG_X13 13
#define REG_X14 14
#define REG_X15 15
#define REG_X16 16
#define REG_X17 17
#define REG_X18 18
#define REG_X19 19
#define REG_X20 20
#define REG_X21 21
#define REG_X22 22
#define REG_X23 23
#define REG_X24 24
#define REG_X25 25
#define REG_X26 26
#define REG_X27 27
#define REG_X28 28
#define REG_X29 29
#define REG_X30 30
#define REG_XZR 31

#define REG_V0 0
#define REG_V1 1
#define REG_V2 2
#define REG_V3 3
#define REG_V4 4
#define REG_V5 5
#define REG_V6 6
#define REG_V7 7
#define REG_V8 8
#define REG_V9 9
#define REG_V10 10
#define REG_V11 11
#define REG_V12 12
#define REG_V13 13
#define REG_V14 14
#define REG_V15 15
#define REG_V16 16
#define REG_V17 17
#define REG_V18 18
#define REG_V19 19
#define REG_V20 20
#define REG_V21 21
#define REG_V22 22
#define REG_V23 23
#define REG_V24 24
#define REG_V25 25
#define REG_V26 26
#define REG_V27 27
#define REG_V28 28
#define REG_V29 29
#define REG_V30 30
#define REG_V31 31

#define REG_XSP 31

#define REG_ARG0 REG_X0
#define REG_ARG1 REG_X1
#define REG_ARG2 REG_X2
#define REG_ARG3 REG_X3

#define REG_CPUSTATE REG_X29

#define REG_TEMP REG_X7
#define REG_TEMP2 REG_X6

#define REG_V_TEMP REG_V0

#define CODEGEN_HOST_REGS 10
#define CODEGEN_HOST_FP_REGS 8

extern void *codegen_mem_load_byte;
extern void *codegen_mem_load_word;
extern void *codegen_mem_load_long;
extern void *codegen_mem_load_quad;
extern void *codegen_mem_load_single;
extern void *codegen_mem_load_double;

extern void *codegen_mem_store_byte;
extern void *codegen_mem_store_word;
extern void *codegen_mem_store_long;
extern void *codegen_mem_store_quad;
extern void *codegen_mem_store_single;
extern void *codegen_mem_store_double;

extern void *codegen_fp_round;
extern void *codegen_fp_round_quad;

extern void *codegen_gpf_rout;
extern void *codegen_exit_rout;

#endif /* _CODEGEN_BACKEND_ARM64_DEFS_H_ */
