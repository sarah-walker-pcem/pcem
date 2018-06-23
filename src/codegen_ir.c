#include "ibm.h"
#include "codegen.h"
#include "codegen_backend.h"
#include "codegen_ir.h"
#include "codegen_reg.h"

static ir_data_t ir_block;

ir_data_t *codegen_ir_init()
{
        ir_block.wr_pos = 0;
        //pclog("codegen_ir_init\n");

        return &ir_block;
}

void codegen_ir_compile(ir_data_t *ir, codeblock_t *block)
{
        int c;

        codegen_backend_prologue(block);

        for (c = 0; c < ir->wr_pos; c++)
        {
                uop_t *uop = &ir->uops[c];
                
                //pclog("uOP %i : %08x\n", c, uop->type);
                
                if (uop->type & UOP_TYPE_PARAMS_REGS)
                {
                        if (uop->dest_reg_a.reg != IREG_INVALID)
                        {
                                uop->dest_reg_a_real = codegen_reg_alloc_write_reg(block, uop->dest_reg_a);
                        }
                }
                
                if (uop->type & UOP_TYPE_BARRIER)
                        codegen_reg_flush(ir, block);
                
                uop_handlers[uop->type & UOP_MASK](block, uop);
        }

        codegen_backend_epilogue(block);

        //fatal("IR compilation complete\n");
}
