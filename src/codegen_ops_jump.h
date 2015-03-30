static uint32_t ropJMP_r8(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t offset = fetchdat & 0xff;

        if (offset & 0x80)
                offset |= 0xffffff00;

        STORE_IMM_ADDR_L((uintptr_t)&pc, op_pc+1+offset);
        
        return -1;
}

static uint32_t ropJMP_r16(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint16_t offset = fetchdat & 0xffff;

        STORE_IMM_ADDR_L((uintptr_t)&pc, (op_pc+2+offset) & 0xffff);
        
        return -1;
}

static uint32_t ropJMP_r32(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        uint32_t offset = fastreadl(cs + op_pc);

        STORE_IMM_ADDR_L((uintptr_t)&pc, op_pc+4+offset);
        
        return -1;
}
