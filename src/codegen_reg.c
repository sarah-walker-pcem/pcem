#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_ir_defs.h"
#include "codegen_reg.h"

uint8_t reg_last_version[IREG_COUNT];
uint8_t reg_version_refcount[IREG_COUNT][256];

ir_reg_t invalid_ir_reg = {IREG_INVALID};

ir_reg_t host_regs[CODEGEN_HOST_REGS];

enum
{
        REG_BYTE,
        REG_WORD,
        REG_DWORD,
        REG_QWORD,
        REG_POINTER
};

struct
{
        int native_size;
        void *p;
} ireg_data[IREG_COUNT] =
{
        [IREG_EAX] = {REG_DWORD, &EAX},
	[IREG_ECX] = {REG_DWORD, &ECX},
	[IREG_EDX] = {REG_DWORD, &EDX},
	[IREG_EBX] = {REG_DWORD, &EBX},
	[IREG_ESP] = {REG_DWORD, &ESP},
	[IREG_EBP] = {REG_DWORD, &EBP},
	[IREG_ESI] = {REG_DWORD, &ESI},
	[IREG_EDI] = {REG_DWORD, &EDI},

	[IREG_flags_op]  = {REG_DWORD, &cpu_state.flags_op},
	[IREG_flags_res] = {REG_DWORD, &cpu_state.flags_res},
	[IREG_flags_op1] = {REG_DWORD, &cpu_state.flags_op1},
	[IREG_flags_op2] = {REG_DWORD, &cpu_state.flags_op2},

	[IREG_pc]    = {REG_DWORD, &cpu_state.pc},
	[IREG_oldpc] = {REG_DWORD, &cpu_state.oldpc},

	[IREG_eaaddr] = {REG_DWORD, &cpu_state.eaaddr},
	[IREG_ea_seg] = {REG_POINTER, &cpu_state.ea_seg},

	[IREG_op32] = {REG_DWORD, &cpu_state.op32},
	[IREG_ssegs] = {REG_BYTE, &cpu_state.ssegs},

	/*Temporary registers are stored on the stack, and are not guaranteed to
          be preserved across uOPs. They will not be written back if they will
          not be read again.*/
	[IREG_temp0] = {REG_DWORD, NULL},
	[IREG_temp1] = {REG_DWORD, NULL},
	[IREG_temp2] = {REG_DWORD, NULL},
	[IREG_temp3] = {REG_DWORD, NULL}
};

void codegen_reg_reset()
{
        int c;
        
        for (c = 0; c < IREG_COUNT; c++)
        {
                reg_last_version[c] = 0;
                reg_version_refcount[c][0] = 0;
        }
        for (c = 0; c < CODEGEN_HOST_REGS; c++)
                host_regs[c] = invalid_ir_reg;
}

static inline int ir_reg_is_invalid(ir_reg_t ir_reg)
{
        return (ir_reg.reg == IREG_INVALID);
}

static void codegen_reg_writeback(codeblock_t *block, int c)
{
        int ir_reg = host_regs[c].reg;

        switch (ireg_data[ir_reg].native_size)
        {
                case REG_BYTE:
                codegen_direct_write_8(block, ireg_data[ir_reg].p, codegen_host_reg_list[c]);
                break;

                case REG_DWORD:
                codegen_direct_write_32(block, ireg_data[ir_reg].p, codegen_host_reg_list[c]);
                break;

                case REG_POINTER:
                codegen_direct_write_ptr(block, ireg_data[ir_reg].p, codegen_host_reg_list[c]);
                break;

                default:
                fatal("codegen_reg_flush - native_size=%i\n", ireg_data[ir_reg].native_size);
        }

        host_regs[c] = invalid_ir_reg;
}

ir_host_reg_t codegen_reg_alloc_write_reg(codeblock_t *block, ir_reg_t ir_reg)
{
        int c;
        
        /*Search for unused registers*/
        for (c = 0; c < CODEGEN_HOST_REGS; c++)
        {
                if (ir_reg_is_invalid(host_regs[c]))
                        break;
        }
        
        if (c == CODEGEN_HOST_REGS)
        {
                c = 0;
                codegen_reg_writeback(block, c);
//                fatal("codegen_reg_alloc_write_reg - out of registers\n");
        }
        
        host_regs[c].reg = ir_reg.reg;
        host_regs[c].version = ir_reg.version;
        
        return codegen_host_reg_list[c];
}

void codegen_reg_flush(ir_data_t *ir, codeblock_t *block)
{
        int c;
        
        for (c = 0; c < CODEGEN_HOST_REGS; c++)
        {
                if (!ir_reg_is_invalid(host_regs[c]))
                {
                        codegen_reg_writeback(block, c);
                }
        }
}
