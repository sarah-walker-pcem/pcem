#ifndef _CODEGEN_BACKEND_X86_OPS_FPU_H_
#define _CODEGEN_BACKEND_X86_OPS_FPU_H_

void host_x87_FILDq_BASE(codeblock_t *block, int base_reg);
void host_x87_FISTPq_BASE(codeblock_t *block, int base_reg);
void host_x87_FLDCW(codeblock_t *block, void *p);
void host_x87_FLDd_BASE(codeblock_t *block, int base_reg);
void host_x87_FSTPd_BASE(codeblock_t *block, int base_reg);


#endif /* _CODEGEN_BACKEND_X86_OPS_FPU_H_ */
