#ifndef _CODEGEN_ACCUMULATE_H_
#define _CODEGEN_ACCUMULATE_H_

enum
{
        ACCREG_ins    = 0,
        ACCREG_cycles = 1,
        
        ACCREG_COUNT
};

struct ir_data_t;

void codegen_accumulate(int acc_reg, int delta);
void codegen_accumulate_flush(struct ir_data_t *ir);
void codegen_accumulate_reset();


#endif /* _CODEGEN_ACCUMULATE_H_ */
