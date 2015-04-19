static uint32_t ropFXCH(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();

        FP_FXCH(opcode & 7);
        
        return op_pc;
}

static uint32_t ropFLD(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();

        FP_FLD(opcode & 7);
        
        return op_pc;
}

static uint32_t ropFST(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();

        FP_FST(opcode & 7);
        
        return op_pc;
}
static uint32_t ropFSTP(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();

        FP_FST(opcode & 7);
        FP_POP();
        
        return op_pc;
}


static uint32_t ropFLDs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
        
        CHECK_SEG_READ(target_seg);
        MEM_LOAD_ADDR_EA_L(target_seg);

        FP_LOAD_S();

        return op_pc + 1;
}

static uint32_t ropFILDl(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
        
        CHECK_SEG_READ(target_seg);
        MEM_LOAD_ADDR_EA_L(target_seg);

        FP_LOAD_IL();

        return op_pc + 1;
}

static uint32_t ropFSTs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        int host_reg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        host_reg = FP_LOAD_REG(0);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);

        CHECK_SEG_WRITE(target_seg);
                
        MEM_STORE_ADDR_EA_L(target_seg, host_reg);

        return op_pc + 1;
}

static uint32_t ropFSTPs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t new_pc = ropFSTs(opcode, fetchdat, op_32, op_pc, block);
        
        FP_POP();
        
        return new_pc;
}

static uint32_t ropFADDs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
        
        CHECK_SEG_READ(target_seg);
        MEM_LOAD_ADDR_EA_L(target_seg);

        FP_OP_S(FPU_ADD);

        return op_pc + 1;
}
static uint32_t ropFDIVs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
        
        CHECK_SEG_READ(target_seg);
        MEM_LOAD_ADDR_EA_L(target_seg);

        FP_OP_S(FPU_DIV);

        return op_pc + 1;
}
static uint32_t ropFMULs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
        
        CHECK_SEG_READ(target_seg);
        MEM_LOAD_ADDR_EA_L(target_seg);

        FP_OP_S(FPU_MUL);

        return op_pc + 1;
}
static uint32_t ropFSUBs(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        
        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
        
        CHECK_SEG_READ(target_seg);
        MEM_LOAD_ADDR_EA_L(target_seg);

        FP_OP_S(FPU_SUB);

        return op_pc + 1;
}


static uint32_t ropFADD(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_ADD, 0, opcode & 7);
       
        return op_pc;
}
static uint32_t ropFDIV(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_DIV, 0, opcode & 7);
       
        return op_pc;
}
static uint32_t ropFMUL(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_MUL, 0, opcode & 7);
       
        return op_pc;
}
static uint32_t ropFSUB(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_SUB, 0, opcode & 7);
       
        return op_pc;
}

static uint32_t ropFADDr(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_ADD, opcode & 7, 0);
       
        return op_pc;
}
static uint32_t ropFMULr(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_MUL, opcode & 7, 0);
       
        return op_pc;
}

static uint32_t ropFADDP(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_ADD, opcode & 7, 0);
        FP_POP();
       
        return op_pc;
}
static uint32_t ropFDIVP(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_DIV, opcode & 7, 0);
        FP_POP();
       
        return op_pc;
}
static uint32_t ropFMULP(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_MUL, opcode & 7, 0);
        FP_POP();
       
        return op_pc;
}
static uint32_t ropFSUBP(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        FP_ENTER();
        FP_OP_REG(FPU_SUB, opcode & 7, 0);
        FP_POP();
       
        return op_pc;
}


static uint32_t ropFSTSW_AX(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        
        FP_ENTER();
        host_reg = LOAD_VAR_W(&npxs);
        STORE_REG_TARGET_W_RELEASE(host_reg, REG_AX);
        
        return op_pc;
}


static uint32_t ropFISTl(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        x86seg *target_seg;
        int host_reg;

        FP_ENTER();
        op_pc--;
        target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

        host_reg = FP_LOAD_REG_INT(0);

        STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);

        CHECK_SEG_WRITE(target_seg);
                
        MEM_STORE_ADDR_EA_L(target_seg, host_reg);

        return op_pc + 1;
}

static uint32_t ropFISTPl(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t new_pc = ropFISTl(opcode, fetchdat, op_32, op_pc, block);
        
        FP_POP();
        
        return new_pc;
}
