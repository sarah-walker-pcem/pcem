#ifndef _CODEGEN_OPS_MMX_LOGIC_H_
#define _CODEGEN_OPS_MMX_LOGIC_H_
uint32_t ropPAND(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc);
uint32_t ropPANDN(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc);
uint32_t ropPOR(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc);
uint32_t ropPXOR(codeblock_t *block, ir_data_t *ir, uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc);


#endif /* _CODEGEN_OPS_MMX_LOGIC_H_ */
