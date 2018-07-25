#define REG_W0   0
#define REG_W1   1
#define REG_W2   2
#define REG_W3   3
#define REG_W4   4
#define REG_W5   5
#define REG_W6   6
#define REG_W7   7
#define REG_W8   8
#define REG_W9   9
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

#define REG_X0   0
#define REG_X1   1
#define REG_X2   2
#define REG_X3   3
#define REG_X4   4
#define REG_X5   5
#define REG_X6   6
#define REG_X7   7
#define REG_X8   8
#define REG_X9   9
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

#define REG_SP  31

#define REG_ARG0 REG_X0
#define REG_ARG1 REG_X1
#define REG_ARG2 REG_X2
#define REG_ARG3 REG_X3

#define REG_CPUSTATE REG_X29

#define REG_TEMP REG_X7

#define CODEGEN_HOST_REGS 5

extern void *codegen_mem_load_byte;
extern void *codegen_mem_load_word;
extern void *codegen_mem_load_long;

extern void *codegen_mem_store_byte;
extern void *codegen_mem_store_word;
extern void *codegen_mem_store_long;