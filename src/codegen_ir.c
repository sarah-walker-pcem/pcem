#include "ibm.h"
#include "codegen.h"
#include "codegen_allocator.h"
#include "codegen_backend.h"
#include "codegen_ir.h"
#include "codegen_reg.h"

extern int has_ea;
static ir_data_t ir_block;

ir_data_t *codegen_ir_init()
{
        ir_block.wr_pos = 0;
//        pclog("codegen_ir_init %04x:%04x\n", CS,cpu_state.pc);

        return &ir_block;
}

void codegen_ir_compile(ir_data_t *ir, codeblock_t *block)
{
        int jump_target_at_end = -1;
        int c;

        block_write_data = codeblock_allocator_get_ptr(block->head_mem_block);
        block_pos = 0;
        codegen_backend_prologue(block);

        for (c = 0; c < ir->wr_pos; c++)
        {
                uop_t *uop = &ir->uops[c];
                
//                pclog("uOP %i : %08x\n", c, uop->type);

                if (uop->type & UOP_TYPE_BARRIER)
                        codegen_reg_flush_invalidate(ir, block);
                else if (uop->type & UOP_TYPE_ORDER_BARRIER)
                        codegen_reg_flush(ir, block);

                if (uop->type & UOP_TYPE_JUMP_DEST)
                {
                        uop_t *uop_dest = uop;

                        while (uop_dest->jump_list_next != -1)
                        {
                                uop_dest = &ir->uops[uop_dest->jump_list_next];
                                codegen_set_jump_dest(block, uop_dest->p);
                        }
                }

                if (uop->type & UOP_TYPE_PARAMS_REGS)
                {
                        codegen_reg_alloc_register(uop->dest_reg_a, uop->src_reg_a, uop->src_reg_b, uop->src_reg_c);
                        if (uop->src_reg_a.reg != IREG_INVALID)
                        {
                                uop->src_reg_a_real = codegen_reg_alloc_read_reg(block, uop->src_reg_a, NULL);
                        }
                        if (uop->src_reg_b.reg != IREG_INVALID)
                        {
                                uop->src_reg_b_real = codegen_reg_alloc_read_reg(block, uop->src_reg_b, NULL);
                        }
                        if (uop->src_reg_c.reg != IREG_INVALID)
                        {
                                uop->src_reg_c_real = codegen_reg_alloc_read_reg(block, uop->src_reg_c, NULL);
                        }
                        if (uop->dest_reg_a.reg != IREG_INVALID)
                        {
                                uop->dest_reg_a_real = codegen_reg_alloc_write_reg(block, uop->dest_reg_a);
                        }
                }
                
                if (!uop_handlers[uop->type & UOP_MASK])
                        fatal("!uop_handlers[uop->type & UOP_MASK] %08x\n", uop->type);
                uop_handlers[uop->type & UOP_MASK](block, uop);

                if (uop->type & UOP_TYPE_JUMP)
                {
                        if (uop->jump_dest_uop == ir->wr_pos)
                        {
                                if (jump_target_at_end == -1)
                                        jump_target_at_end = c;
                                else
                                {
                                        uop_t *uop_dest = &ir->uops[jump_target_at_end];

                                        while (uop_dest->jump_list_next != -1)
                                                uop_dest = &ir->uops[uop_dest->jump_list_next];

                                        uop_dest->jump_list_next = c;
                                }
                        }
                        else
                        {
                                uop_t *uop_dest = &ir->uops[uop->jump_dest_uop];
                                
                                while (uop_dest->jump_list_next != -1)
                                        uop_dest = &ir->uops[uop_dest->jump_list_next];

                                uop_dest->jump_list_next = c;
                                ir->uops[uop->jump_dest_uop].type |= UOP_TYPE_JUMP_DEST;
                        }
                }
        }

        codegen_reg_flush_invalidate(ir, block);
        
        if (jump_target_at_end != -1)
        {
                uop_t *uop_dest = &ir->uops[jump_target_at_end];

                while (1)
                {
                        codegen_set_jump_dest(block, uop_dest->p);
                        if (uop_dest->jump_list_next == -1)
                                break;
                        uop_dest = &ir->uops[uop_dest->jump_list_next];
                }
        }

        codegen_backend_epilogue(block);
        block_write_data = NULL;
//        if (has_ea)
//                fatal("IR compilation complete\n");
}
