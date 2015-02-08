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
        int host_reg = LOAD_REG_B((fetchdat >> 3) & 7);
        
        if ((fetchdat & 0xc0) == 0xc0)
        {
                STORE_REG_TARGET_B_RELEASE(host_reg, fetchdat & 7);
        }
        else
        {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                        
                CHECK_SEG_WRITE(target_seg);
                CHECK_SEG_LIMITS(target_seg, 0);

                MEM_STORE_ADDR_EA_B(target_seg, host_reg);
                RELEASE_REG(host_reg);
        }
        
        return op_pc + 1;
}
static uint32_t ropMOV_w_r(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_W((fetchdat >> 3) & 7);
        
        if ((fetchdat & 0xc0) == 0xc0)
        {
                STORE_REG_TARGET_W_RELEASE(host_reg, fetchdat & 7);
        }
        else
        {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                        
                CHECK_SEG_WRITE(target_seg);
                CHECK_SEG_LIMITS(target_seg, 1);

                MEM_STORE_ADDR_EA_W(target_seg, host_reg);
                RELEASE_REG(host_reg);
        }
        
        return op_pc + 1;
}

static uint32_t ropMOV_l_r(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        
        host_reg = LOAD_REG_L((fetchdat >> 3) & 7);
        
        if ((fetchdat & 0xc0) == 0xc0)
        {
                STORE_REG_TARGET_L_RELEASE(host_reg, fetchdat & 7);
        }
        else
        {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                        
                CHECK_SEG_WRITE(target_seg);
                CHECK_SEG_LIMITS(target_seg, 3);
                
                MEM_STORE_ADDR_EA_L(target_seg, host_reg);
                RELEASE_REG(host_reg);
                
        }

        return op_pc + 1;
}

static uint32_t ropMOV_r_b(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        if ((fetchdat & 0xc0) == 0xc0)
        {
                int host_reg = LOAD_REG_B(fetchdat & 7);
                STORE_REG_TARGET_B_RELEASE(host_reg, (fetchdat >> 3) & 7);
        }
        else
        {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                        
                CHECK_SEG_READ(target_seg);

                MEM_LOAD_ADDR_EA_B(target_seg);
                STORE_REG_TARGET_B_RELEASE(0, (fetchdat >> 3) & 7);
        }
        
        return op_pc + 1;
}
static uint32_t ropMOV_r_w(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        if ((fetchdat & 0xc0) == 0xc0)
        {
                int host_reg = LOAD_REG_W(fetchdat & 7);
                STORE_REG_TARGET_W_RELEASE(host_reg, (fetchdat >> 3) & 7);
        }
        else
        {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);

                CHECK_SEG_READ(target_seg);

                MEM_LOAD_ADDR_EA_W(target_seg);
                STORE_REG_TARGET_W_RELEASE(0, (fetchdat >> 3) & 7);
        }
        
        return op_pc + 1;
}
static uint32_t ropMOV_r_l(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        if ((fetchdat & 0xc0) == 0xc0)
        {
                int host_reg = LOAD_REG_L(fetchdat & 7);
                STORE_REG_TARGET_L_RELEASE(host_reg, (fetchdat >> 3) & 7);
        }
        else
        {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);

                CHECK_SEG_READ(target_seg);

                MEM_LOAD_ADDR_EA_L(target_seg);
                STORE_REG_TARGET_L_RELEASE(0, (fetchdat >> 3) & 7);
        }
        
        return op_pc + 1;
}

static uint32_t ropMOV_b_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        if ((fetchdat & 0xc0) == 0xc0)
        {
                STORE_IMM_REG_B(fetchdat & 7, (fetchdat >> 8) & 0xff);
        }
        else
        {
//                return 0;
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);
                uint32_t imm = fastreadb(cs + op_pc + 1);
                int host_reg = LOAD_REG_IMM(imm);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                CHECK_SEG_WRITE(target_seg);

                MEM_STORE_ADDR_EA_B(target_seg, host_reg);
                RELEASE_REG(host_reg);
        }

        return op_pc + 2;
}
static uint32_t ropMOV_w_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        if ((fetchdat & 0xc0) == 0xc0)
        {
                STORE_IMM_REG_W(fetchdat & 7, (fetchdat >> 8) & 0xffff);
        }
        else
        {
//                if ((cs+pc) <= 0xbff70000 || (cs+pc) > 0xbff80000)
//                        return 0;
//                {
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);
                uint32_t imm = fastreadw(cs + op_pc + 1);
                int host_reg = LOAD_REG_IMM(imm);
//pclog("MOV imm %04x(%08x):%08x %08x %04x\n", CS, cs, pc, op_pc, imm);
                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                CHECK_SEG_WRITE(target_seg);

                MEM_STORE_ADDR_EA_W(target_seg, host_reg);
                RELEASE_REG(host_reg);
//                }
        }

        return op_pc + 3;
}
static uint32_t ropMOV_l_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        if ((fetchdat & 0xc0) == 0xc0)
        {
                uint32_t imm = fastreadl(cs + op_pc + 1);

                STORE_IMM_REG_L(fetchdat & 7, imm);
        }
        else
        {
//                return 0;
                x86seg *target_seg = FETCH_EA(op_ea_seg, fetchdat, op_ssegs, &op_pc, op_32);
                uint32_t imm = fastreadl(cs + op_pc + 1);
                int host_reg = LOAD_REG_IMM(imm);

                STORE_IMM_ADDR_L((uintptr_t)&oldpc, op_old_pc);
                CHECK_SEG_WRITE(target_seg);

                MEM_STORE_ADDR_EA_L(target_seg, host_reg);
                RELEASE_REG(host_reg);
        }
        
        return op_pc + 5;
}


static uint32_t ropMOV_AL_a(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        CHECK_SEG_READ(op_ea_seg);

        MEM_LOAD_ADDR_IMM_B(op_ea_seg, addr);
        STORE_REG_TARGET_B_RELEASE(0, REG_AL);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
static uint32_t ropMOV_AX_a(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        CHECK_SEG_READ(op_ea_seg);

        MEM_LOAD_ADDR_IMM_W(op_ea_seg, addr);
        STORE_REG_TARGET_W_RELEASE(0, REG_AX);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
static uint32_t ropMOV_EAX_a(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t addr;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        CHECK_SEG_READ(op_ea_seg);

        MEM_LOAD_ADDR_IMM_L(op_ea_seg, addr);
        STORE_REG_TARGET_L_RELEASE(0, REG_EAX);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}

static uint32_t ropMOV_a_AL(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t addr;
        int host_reg;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        CHECK_SEG_WRITE(op_ea_seg);
        
        host_reg = LOAD_REG_B(REG_AL);

        MEM_STORE_ADDR_IMM_B(op_ea_seg, addr, host_reg);
        RELEASE_REG(host_reg);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
static uint32_t ropMOV_a_AX(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t addr;
        int host_reg;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        CHECK_SEG_WRITE(op_ea_seg);
        
        host_reg = LOAD_REG_W(REG_AX);

        MEM_STORE_ADDR_IMM_W(op_ea_seg, addr, host_reg);
        RELEASE_REG(host_reg);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
static uint32_t ropMOV_a_EAX(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t addr;
        int host_reg;

        if (op_32 & 0x200)
                addr = fastreadl(cs + op_pc);
        else
                addr = fastreadw(cs + op_pc);

        CHECK_SEG_WRITE(op_ea_seg);
        
        host_reg = LOAD_REG_L(REG_EAX);

        MEM_STORE_ADDR_IMM_L(op_ea_seg, addr, host_reg);
        RELEASE_REG(host_reg);
        
        return op_pc + ((op_32 & 0x200) ? 4 : 2);
}
