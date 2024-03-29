#ifndef _CODEGEN_OPS_FPU_CONSTANT_H_
#define _CODEGEN_OPS_FPU_CONSTANT_H_

uint32_t ropFLD1(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc);
uint32_t ropFLDZ(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc);

#endif /* _CODEGEN_OPS_FPU_CONSTANT_H_ */
