static uint32_t ropMOV_rb_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        STORE_IMM_REG_B(opcode & 7, fetchdat & 0xff);
        
        return op_pc + 1;
}
static uint32_t ropMOV_rw_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        STORE_IMM_REG_W(opcode & 7, fetchdat & 0xffff);

        return op_pc + 2;
}
static uint32_t ropMOV_rl_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        fetchdat = fastreadl(cs + op_pc);

        STORE_IMM_REG_L(opcode & 7, fetchdat);
        
        return op_pc + 4;
}


static uint32_t ropMOV_b_r(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        
        if ((fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_B((fetchdat >> 3) & 7);
        STORE_REG_TARGET_B_RELEASE(host_reg, fetchdat & 7);
        
        return op_pc + 1;
}
static uint32_t ropMOV_w_r(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_W((fetchdat >> 3) & 7);
        STORE_REG_TARGET_W_RELEASE(host_reg, fetchdat & 7);
        
        return op_pc + 1;
}
static uint32_t ropMOV_l_r(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_L((fetchdat >> 3) & 7);
        STORE_REG_TARGET_L_RELEASE(host_reg, fetchdat & 7);
        
        return op_pc + 1;
}

static uint32_t ropMOV_r_b(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_B(fetchdat & 7);
        STORE_REG_TARGET_B_RELEASE(host_reg, (fetchdat >> 3) & 7);
        
        return op_pc + 1;
}
static uint32_t ropMOV_r_w(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_W(fetchdat & 7);
        STORE_REG_TARGET_W_RELEASE(host_reg, (fetchdat >> 3) & 7);
        
        return op_pc + 1;
}
static uint32_t ropMOV_r_l(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        
        if ((fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_L(fetchdat & 7);
        STORE_REG_TARGET_L_RELEASE(host_reg, (fetchdat >> 3) & 7);
        
        return op_pc + 1;
}

static uint32_t ropMOV_b_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        STORE_IMM_REG_B(fetchdat & 7, (fetchdat >> 8) & 0xff);
        
        return op_pc + 2;
}
static uint32_t ropMOV_w_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        STORE_IMM_REG_W(fetchdat & 7, (fetchdat >> 8) & 0xffff);
        
        return op_pc + 3;
}
static uint32_t ropMOV_l_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t imm = fastreadl(cs + op_pc + 1);
        int host_reg;

        if ((fetchdat & 0xc0) != 0xc0)
                return 0;

        STORE_IMM_REG_L(fetchdat & 7, imm);
        
        return op_pc + 5;
}
