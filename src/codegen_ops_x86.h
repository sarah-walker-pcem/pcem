static inline int find_host_reg()
{
        int c;
        for (c = 0; c < NR_HOST_REGS; c++)
        {
                if (host_reg_mapping[c] == -1)
                        break;
        }
        
        if (c == NR_HOST_REGS)
                fatal("Out of host regs!\n");
        return c;
}

static void STORE_IMM_ADDR_B(uintptr_t addr, uint8_t val)
{
        addbyte(0xC6); /*MOVB [addr],val*/
        addbyte(0x05);
        addlong(addr);
        addbyte(val);
}
static void STORE_IMM_ADDR_W(uintptr_t addr, uint16_t val)
{
        addbyte(0x66); /*MOVW [addr],val*/
        addbyte(0xC7);
        addbyte(0x05);
        addlong(addr);
        addword(val);
}
static void STORE_IMM_ADDR_L(uintptr_t addr, uint32_t val)
{
        addbyte(0xC7); /*MOVL [addr],val*/
        addbyte(0x05);
        addlong(addr);
        addlong(val);
}

static void STORE_IMM_REG_B(int reg, uint8_t val)
{
        addbyte(0xC6); /*MOVB [addr],val*/
        addbyte(0x05);
        if (reg & 4)
                addlong((uint32_t)&regs[reg & 3].b.h);
        else
                addlong((uint32_t)&regs[reg & 3].b.l);
        addbyte(val);
}
static void STORE_IMM_REG_W(int reg, uint16_t val)
{
        addbyte(0x66); /*MOVW [addr],val*/
        addbyte(0xC7);
        addbyte(0x05);
        addlong((uint32_t)&regs[reg & 7].w);
        addword(val);
}
static void STORE_IMM_REG_L(int reg, uint32_t val)
{
        addbyte(0xC7); /*MOVL [addr],val*/
        addbyte(0x05);
        addlong((uint32_t)&regs[reg & 7].l);
        addlong(val);
}

static int LOAD_REG_B(int reg)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x0f); /*MOVZX B[reg],host_reg*/
        addbyte(0xb6);
        addbyte(0x05 | (host_reg << 3));
        if (reg & 4)
                addlong((uint32_t)&regs[reg & 3].b.h);
        else
                addlong((uint32_t)&regs[reg & 3].b.l);
        
        return host_reg;
}
static int LOAD_REG_W(int reg)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x0f); /*MOVZX W[reg],host_reg*/
        addbyte(0xb7);
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)&regs[host_reg_mapping[host_reg]].w);
        
        return host_reg;
}
static int LOAD_REG_L(int reg)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x8b); /*MOVL host_reg,[reg]*/
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)&regs[host_reg_mapping[host_reg]].l);
        
        return host_reg;
}

static void STORE_REG_B_RELEASE(int host_reg)
{
        addbyte(0x88); /*MOVB [reg],host_reg*/
        addbyte(0x05 | (host_reg << 3));
        if (host_reg_mapping[host_reg] & 4)
                addlong((uint32_t)&regs[host_reg_mapping[host_reg] & 3].b.h);
        else
                addlong((uint32_t)&regs[host_reg_mapping[host_reg] & 3].b.l);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_W_RELEASE(int host_reg)
{
        addbyte(0x66); /*MOVW [reg],host_reg*/
        addbyte(0x89);
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)&regs[host_reg_mapping[host_reg]].w);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_L_RELEASE(int host_reg)
{
        addbyte(0x89); /*MOVL [reg],host_reg*/
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)&regs[host_reg_mapping[host_reg]].l);
        host_reg_mapping[host_reg] = -1;
}

static void STORE_REG_TARGET_B_RELEASE(int host_reg, int guest_reg)
{
        addbyte(0x88); /*MOVB [guest_reg],host_reg*/
        addbyte(0x05 | (host_reg << 3));
        if (guest_reg & 4)
                addlong((uint32_t)&regs[guest_reg & 3].b.h);
        else
                addlong((uint32_t)&regs[guest_reg & 3].b.l);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_TARGET_W_RELEASE(int host_reg, int guest_reg)
{
        addbyte(0x66); /*MOVB [guest_reg],host_reg*/
        addbyte(0x89);
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)&regs[guest_reg & 7].w);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_TARGET_L_RELEASE(int host_reg, int guest_reg)
{
        addbyte(0x89); /*MOVB [guest_reg],host_reg*/
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)&regs[guest_reg & 7].l);
        host_reg_mapping[host_reg] = -1;
}

static void RELEASE_REG(int host_reg)
{
        host_reg_mapping[host_reg] = -1;
}

static void STORE_HOST_REG_ADDR(uintptr_t addr, int host_reg)
{
        addbyte(0x89); /*MOVL [reg],host_reg*/
        addbyte(0x05 | (host_reg << 3));
        addlong(addr);
}

static void ADD_HOST_REG_B(int dst_reg, int src_reg)
{
        addbyte(0x00); /*ADDB dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void ADD_HOST_REG_W(int dst_reg, int src_reg)
{
        addbyte(0x66); /*ADDW dst_reg, src_reg*/
        addbyte(0x01);
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void ADD_HOST_REG_L(int dst_reg, int src_reg)
{
        addbyte(0x01); /*ADDL dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void ADD_HOST_REG_IMM_B(int host_reg, uint8_t imm)
{
        addbyte(0x80); /*ADDB host_reg, imm*/
        addbyte(0xC0 | host_reg);
        addbyte(imm);
}
static void ADD_HOST_REG_IMM_W(int host_reg, uint16_t imm)
{
        addbyte(0x66); /*ADDW host_reg, imm*/
        addbyte(0x81);
        addbyte(0xC0 | host_reg);
        addword(imm);
}
static void ADD_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        addbyte(0x81); /*ADDL host_reg, imm*/
        addbyte(0xC0 | host_reg);
        addlong(imm);
}

#define AND_HOST_REG_B AND_HOST_REG_L
#define AND_HOST_REG_W AND_HOST_REG_L
static void AND_HOST_REG_L(int dst_reg, int src_reg)
{
        addbyte(0x21); /*ANDL dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void AND_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        addbyte(0x81); /*ANDL host_reg, imm*/
        addbyte(0xE0 | host_reg);
        addlong(imm);
}

#define OR_HOST_REG_B OR_HOST_REG_L
#define OR_HOST_REG_W OR_HOST_REG_L
static void OR_HOST_REG_L(int dst_reg, int src_reg)
{
        addbyte(0x09); /*ORL dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void OR_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        addbyte(0x81); /*ORL host_reg, imm*/
        addbyte(0xC8 | host_reg);
        addlong(imm);
}


static void SUB_HOST_REG_B(int dst_reg, int src_reg)
{
        addbyte(0x28); /*SUBB dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void SUB_HOST_REG_W(int dst_reg, int src_reg)
{
        addbyte(0x66); /*SUBW dst_reg, src_reg*/
        addbyte(0x29);
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void SUB_HOST_REG_L(int dst_reg, int src_reg)
{
        addbyte(0x29); /*SUBL dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void SUB_HOST_REG_IMM_B(int host_reg, uint8_t imm)
{
        addbyte(0x80); /*SUBB host_reg, imm*/
        addbyte(0xE8 | host_reg);
        addbyte(imm);
}
static void SUB_HOST_REG_IMM_W(int host_reg, uint16_t imm)
{
        addbyte(0x66); /*SUBW host_reg, imm*/
        addbyte(0x81);
        addbyte(0xE8 | host_reg);
        addword(imm);
}
static void SUB_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        addbyte(0x81); /*SUBL host_reg, imm*/
        addbyte(0xE8 | host_reg);
        addlong(imm);
}

#define XOR_HOST_REG_B XOR_HOST_REG_L
#define XOR_HOST_REG_W XOR_HOST_REG_L
static void XOR_HOST_REG_L(int dst_reg, int src_reg)
{
        addbyte(0x31); /*XORL dst_reg, src_reg*/
        addbyte(0xc0 | dst_reg | (src_reg << 3));
}
static void XOR_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        addbyte(0x81); /*XORL host_reg, imm*/
        addbyte(0xF0 | host_reg);
        addlong(imm);
}

static void CALL_FUNC(void *dest)
{
        addbyte(0xE8); /*CALL*/
        addlong(((uint8_t *)dest - (uint8_t *)(&codeblock[block_current].data[block_pos + 4])));
}
