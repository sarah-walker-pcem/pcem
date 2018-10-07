//#ifdef __amd64__
//#include "codegen_x86-64.h"
#if defined __amd64__
#include "codegen_backend_x86-64.h"
#elif defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32
#include "codegen_backend_x86.h"
#elif defined __ARM_EABI__
#include "codegen_backend_arm.h"
#elif defined __aarch64__
#include "codegen_backend_arm64.h"
#else
#error Dynamic recompiler not implemented on your platform
#endif

void codegen_backend_init();
void codegen_backend_prologue(codeblock_t *block);
void codegen_backend_epilogue(codeblock_t *block);

static inline void addbyte(uint8_t val)
{
        codeblock[block_current].data[block_pos++] = val;
        if (block_pos >= BLOCK_MAX)
        {
                CPU_BLOCK_END();
        }
}

static inline void addword(uint16_t val)
{
        *(uint16_t *)(void *)&codeblock[block_current].data[block_pos] = val;
        block_pos += 2;
        if (block_pos >= BLOCK_MAX)
        {
                CPU_BLOCK_END();
        }
}

static inline void addlong(uint32_t val)
{
        *(uint32_t *)&codeblock[block_current].data[block_pos] = val;
        block_pos += 4;
        if (block_pos >= BLOCK_MAX)
        {
                CPU_BLOCK_END();
        }
}

static inline void addquad(uint64_t val)
{
        *(uint64_t *)&codeblock[block_current].data[block_pos] = val;
        block_pos += 8;
        if (block_pos >= BLOCK_MAX)
        {
                CPU_BLOCK_END();
        }
}

struct ir_data_t;
struct uop_t;

struct ir_data_t *codegen_get_ir_data();

typedef int (*uOpFn)(codeblock_t *codeblock, struct uop_t *uop);

extern const uOpFn uop_handlers[];

extern int codegen_host_reg_list[CODEGEN_HOST_REGS];
extern int codegen_host_fp_reg_list[CODEGEN_HOST_FP_REGS];
