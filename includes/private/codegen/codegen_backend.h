#ifndef _CODEGEN_BACKEND_H_
#define _CODEGEN_BACKEND_H_

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

struct ir_data_t;
struct uop_t;

struct ir_data_t *codegen_get_ir_data();

typedef int (*uOpFn)(codeblock_t *codeblock, struct uop_t *uop);

extern const uOpFn uop_handlers[];

/*Register will not be preserved across function calls*/
#define HOST_REG_FLAG_VOLATILE (1 << 0)

typedef struct host_reg_def_t
{
        int reg;
        int flags;
} host_reg_def_t;

extern host_reg_def_t codegen_host_reg_list[CODEGEN_HOST_REGS];
extern host_reg_def_t codegen_host_fp_reg_list[CODEGEN_HOST_FP_REGS];

#endif /* _CODEGEN_BACKEND_H_ */
