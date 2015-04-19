/*Register allocation :
        EBX, ECX, EDX - emulated registers
        EAX - work register, EA storage
        ESI, EDI - work registers
        EBP - points at emulated register array
*/
#define HOST_REG_START 1
#define HOST_REG_END 4
static inline int find_host_reg()
{
        int c;
        for (c = HOST_REG_START; c < HOST_REG_END; c++)
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
        addbyte(0x45);
        if (reg & 4)
                addbyte((uint32_t)&regs[reg & 3].b.h - (uint32_t)&EAX);
        else
                addbyte((uint32_t)&regs[reg & 3].b.l - (uint32_t)&EAX);
        addbyte(val);
}
static void STORE_IMM_REG_W(int reg, uint16_t val)
{
        addbyte(0x66); /*MOVW [addr],val*/
        addbyte(0xC7);
        addbyte(0x45);
        addbyte((uint32_t)&regs[reg & 7].w - (uint32_t)&EAX);
        addword(val);
}
static void STORE_IMM_REG_L(int reg, uint32_t val)
{
        addbyte(0xC7); /*MOVL [addr],val*/
        addbyte(0x45);
        addbyte((uint32_t)&regs[reg & 7].l - (uint32_t)&EAX);
        addlong(val);
}

static int LOAD_REG_B(int reg)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x0f); /*MOVZX B[reg],host_reg*/
        addbyte(0xb6);
        addbyte(0x45 | (host_reg << 3));
        if (reg & 4)
                addbyte((uint32_t)&regs[reg & 3].b.h - (uint32_t)&EAX);
        else
                addbyte((uint32_t)&regs[reg & 3].b.l - (uint32_t)&EAX);

        return host_reg;
}
static int LOAD_REG_W(int reg)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x0f); /*MOVZX W[reg],host_reg*/
        addbyte(0xb7);
        addbyte(0x45 | (host_reg << 3));
        addbyte((uint32_t)&regs[reg & 7].w - (uint32_t)&EAX);

        return host_reg;
}
static int LOAD_REG_L(int reg)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x8b); /*MOVL host_reg,[reg]*/
        addbyte(0x45 | (host_reg << 3));
        addbyte((uint32_t)&regs[reg & 7].l - (uint32_t)&EAX);

        return host_reg;
}

static int LOAD_VAR_W(uintptr_t addr)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x66); /*MOVL host_reg,[reg]*/
        addbyte(0x8b);
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)addr);

        return host_reg;
}
static int LOAD_VAR_L(uintptr_t addr)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;

        addbyte(0x8b); /*MOVL host_reg,[reg]*/
        addbyte(0x05 | (host_reg << 3));
        addlong((uint32_t)addr);

        return host_reg;
}

static int LOAD_REG_IMM(uint32_t imm)
{
        int host_reg = find_host_reg();
        host_reg_mapping[host_reg] = reg;
        
        addbyte(0xc7); /*MOVL host_reg, imm*/
        addbyte(0xc0 | host_reg);
        addlong(imm);
        
        return host_reg;
}

static void STORE_REG_B_RELEASE(int host_reg)
{
        addbyte(0x88); /*MOVB [reg],host_reg*/
        addbyte(0x45 | (host_reg << 3));
        if (host_reg_mapping[host_reg] & 4)
                addbyte((uint32_t)&regs[host_reg_mapping[host_reg] & 3].b.h - (uint32_t)&EAX);
        else
                addbyte((uint32_t)&regs[host_reg_mapping[host_reg] & 3].b.l - (uint32_t)&EAX);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_W_RELEASE(int host_reg)
{
        addbyte(0x66); /*MOVW [reg],host_reg*/
        addbyte(0x89);
        addbyte(0x45 | (host_reg << 3));
        addbyte((uint32_t)&regs[host_reg_mapping[host_reg]].w - (uint32_t)&EAX);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_L_RELEASE(int host_reg)
{
        addbyte(0x89); /*MOVL [reg],host_reg*/
        addbyte(0x45 | (host_reg << 3));
        addbyte((uint32_t)&regs[host_reg_mapping[host_reg]].l - (uint32_t)&EAX);
        host_reg_mapping[host_reg] = -1;
}

static void STORE_REG_TARGET_B_RELEASE(int host_reg, int guest_reg)
{
        addbyte(0x88); /*MOVB [guest_reg],host_reg*/
        addbyte(0x45 | (host_reg << 3));
        if (guest_reg & 4)
                addbyte((uint32_t)&regs[guest_reg & 3].b.h - (uint32_t)&EAX);
        else
                addbyte((uint32_t)&regs[guest_reg & 3].b.l - (uint32_t)&EAX);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_TARGET_W_RELEASE(int host_reg, int guest_reg)
{
        addbyte(0x66); /*MOVW [guest_reg],host_reg*/
        addbyte(0x89);
        addbyte(0x45 | (host_reg << 3));
        addbyte((uint32_t)&regs[guest_reg & 7].w - (uint32_t)&EAX);
        host_reg_mapping[host_reg] = -1;
}
static void STORE_REG_TARGET_L_RELEASE(int host_reg, int guest_reg)
{
        addbyte(0x89); /*MOVL [guest_reg],host_reg*/
        addbyte(0x45 | (host_reg << 3));
        addbyte((uint32_t)&regs[guest_reg & 7].l - (uint32_t)&EAX);
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
        if (imm < 0x80 || imm >= 0xff80)
        {
                addbyte(0x66); /*ADDW host_reg, imm*/
                addbyte(0x83);
                addbyte(0xC0 | host_reg);
                addbyte(imm & 0xff);
        }
        else
        {
                addbyte(0x66); /*ADDW host_reg, imm*/
                addbyte(0x81);
                addbyte(0xC0 | host_reg);
                addword(imm);
        }
}
static void ADD_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        if (imm < 0x80 || imm >= 0xffffff80)
        {
                addbyte(0x83); /*ADDL host_reg, imm*/
                addbyte(0xC0 | host_reg);
                addbyte(imm & 0xff);
        }
        else
        {
                addbyte(0x81); /*ADDL host_reg, imm*/
                addbyte(0xC0 | host_reg);
                addlong(imm);
        }
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
        if (imm < 0x80 || imm >= 0xffffff80)
        {
                addbyte(0x83); /*ANDL host_reg, imm*/
                addbyte(0xE0 | host_reg);
                addbyte(imm & 0xff);
        }
        else
        {
                addbyte(0x81); /*ANDL host_reg, imm*/
                addbyte(0xE0 | host_reg);
                addlong(imm);
        }
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
        if (imm < 0x80 || imm >= 0xffffff80)
        {
                addbyte(0x83); /*ORL host_reg, imm*/
                addbyte(0xC8 | host_reg);
                addbyte(imm & 0xff);
        }
        else
        {
                addbyte(0x81); /*ORL host_reg, imm*/
                addbyte(0xC8 | host_reg);
                addlong(imm);
        }
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
        if (imm < 0x80 || imm >= 0xff80)
        {
                addbyte(0x66); /*SUBW host_reg, imm*/
                addbyte(0x83);
                addbyte(0xE8 | host_reg);
                addbyte(imm & 0xff);
        }
        else
        {
                addbyte(0x66); /*SUBW host_reg, imm*/
                addbyte(0x81);
                addbyte(0xE8 | host_reg);
                addword(imm);
        }
}
static void SUB_HOST_REG_IMM(int host_reg, uint32_t imm)
{
        if (imm < 0x80 || imm >= 0xffffff80)
        {
                addbyte(0x83); /*SUBL host_reg, imm*/
                addbyte(0xE8 | host_reg);
                addbyte(imm);
        }
        else
        {
                addbyte(0x81); /*SUBL host_reg, imm*/
                addbyte(0xE8 | host_reg);
                addlong(imm);
        }
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
        if (imm < 0x80 || imm >= 0xffffff80)
        {
                addbyte(0x83); /*XORL host_reg, imm*/
                addbyte(0xF0 | host_reg);
                addbyte(imm & 0xff);
        }
        else
        {
                addbyte(0x81); /*XORL host_reg, imm*/
                addbyte(0xF0 | host_reg);
                addlong(imm);
        }
}

static void CALL_FUNC(void *dest)
{
        addbyte(0xE8); /*CALL*/
        addlong(((uint8_t *)dest - (uint8_t *)(&codeblock[block_current].data[block_pos + 4])));
}




static void CHECK_SEG_READ(x86seg *seg)
{
        addbyte(0x83); /*CMP seg->base, -1*/
        addbyte(0x05|0x38);
        addlong((uint32_t)&seg->base);
        addbyte(-1);
        addbyte(0x0f);
        addbyte(0x84); /*JE end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
}
static void CHECK_SEG_WRITE(x86seg *seg)
{
        addbyte(0x83); /*CMP seg->base, -1*/
        addbyte(0x05|0x38);
        addlong((uint32_t)&seg->base);
        addbyte(-1);
        addbyte(0x0f);
        addbyte(0x84); /*JE end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
}
static void CHECK_SEG_LIMITS(x86seg *seg, int end_offset)
{
        addbyte(0x3b); /*CMP EAX, seg->limit_low*/
        addbyte(0x05);
        addlong((uint32_t)&seg->limit_low);
        addbyte(0x0f); /*JB BLOCK_GPF_OFFSET*/
        addbyte(0x82);
        addlong(BLOCK_GPF_OFFSET - (block_pos + 4));
        if (end_offset)
        {
                addbyte(0x83); /*ADD EAX, end_offset*/
                addbyte(0xc0);
                addbyte(end_offset);
                addbyte(0x3b); /*CMP EAX, seg->limit_high*/
                addbyte(0x05);
                addlong((uint32_t)&seg->limit_high);
                addbyte(0x0f); /*JNBE BLOCK_GPF_OFFSET*/
                addbyte(0x87);
                addlong(BLOCK_GPF_OFFSET - (block_pos + 4));
                addbyte(0x83); /*SUB EAX, end_offset*/
                addbyte(0xe8);
                addbyte(end_offset);
        }
}

static void MEM_LOAD_ADDR_EA_B(x86seg *seg)
{
        addbyte(0x8b); /*MOVL EDX, seg->base*/
        addbyte(0x05 | (REG_EDX << 3));
        addlong((uint32_t)&seg->base);
        addbyte(0x89); /*MOV ESI, EDX*/
        addbyte(0xd6);
        addbyte(0x01); /*ADDL EDX, EAX*/
        addbyte(0xc2);
        addbyte(0x89); /*MOV EDI, EDX*/
        addbyte(0xd7);
        addbyte(0xc1); /*SHR EDX, 12*/
        addbyte(0xea);
        addbyte(12);
        addbyte(0x8b); /*MOV EDX, readlookup2[EDX*4]*/
        addbyte(0x14);
        addbyte(0x95);
        addlong((uint32_t)readlookup2);
        addbyte(0x83); /*CMP EDX, -1*/
        addbyte(0xfa);
        addbyte(-1);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+2);
        addbyte(0x8a); /*MOV AL, [EDX+EDI]*/
        addbyte(0x04);
        addbyte(0x3a);
        addbyte(0xeb); /*JMP done*/
        addbyte(4+3+5+7+6);
        addbyte(0x89); /*slowpath: MOV [ESP+4], EAX*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addbyte(0x89); /*MOV [ESP], ESI*/
        addbyte(0x34);
        addbyte(0x24);
        addbyte(0xe8); /*CALL readmemb386l*/
        addlong((uint32_t)readmemb386l - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0x83); /*CMP abrt, 0*/
        addbyte(0x3d);
        addlong((uint32_t)&abrt);
        addbyte(0);
        addbyte(0x0f); /*JNE end*/
        addbyte(0x85);
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        /*done:*/
        addbyte(0x0f); /*MOVZX EAX, AL*/
        addbyte(0xb6);
        addbyte(0xc0);
        host_reg_mapping[0] = 8;
}
static void MEM_LOAD_ADDR_EA_W(x86seg *seg)
{
        addbyte(0x8b); /*MOVL EDX, seg->base*/
        addbyte(0x05 | (REG_EDX << 3));
        addlong((uint32_t)&seg->base);
        addbyte(0x89); /*MOV ESI, EDX*/
        addbyte(0xd6);
        addbyte(0x01); /*ADDL EDX, EAX*/
        addbyte(0xc2);
        addbyte(0x89); /*MOV EDI, EDX*/
        addbyte(0xd7);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0xc1); /*SHR EDX, 12*/
        addbyte(0xea);
        addbyte(12);
        addbyte(0xf7); /*TEST EDI, 0xfff*/
        addbyte(0xc7);
        addlong(0xfff);
        addbyte(0x8b); /*MOV EDX, readlookup2[EDX*4]*/
        addbyte(0x14);
        addbyte(0x95);
        addlong((uint32_t)readlookup2);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+3+2+4+2);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0x83); /*CMP EDX, -1*/
        addbyte(0xfa);
        addbyte(-1);
        addbyte(0x74); /*JE slowpath*/
        addbyte(4+2);
        addbyte(0x66); /*MOV AX, [EDX+EDI]*/
        addbyte(0x8b);
        addbyte(0x04);
        addbyte(0x3a);
        addbyte(0xeb); /*JMP done*/
        addbyte(4+3+5+7+6);
        addbyte(0x89); /*slowpath: MOV [ESP+4], EAX*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addbyte(0x89); /*MOV [ESP], ESI*/
        addbyte(0x34);
        addbyte(0x24);
        addbyte(0xe8); /*CALL readmemwl*/
        addlong((uint32_t)readmemwl - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0x83); /*CMP abrt, 0*/
        addbyte(0x3d);
        addlong((uint32_t)&abrt);
        addbyte(0);
        addbyte(0x0f);
        addbyte(0x85); /*JNE end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        /*done:*/
        addbyte(0x0f); /*MOVZX EAX, AX*/
        addbyte(0xb7);
        addbyte(0xc0);
        host_reg_mapping[0] = 8;
}
static void MEM_LOAD_ADDR_EA_L(x86seg *seg)
{
        addbyte(0x8b); /*MOVL EDX, seg->base*/
        addbyte(0x05 | (REG_EDX << 3));
        addlong((uint32_t)&seg->base);
        addbyte(0x89); /*MOV ESI, EDX*/
        addbyte(0xd6);
        addbyte(0x01); /*ADDL EDX, EAX*/
        addbyte(0xc2);
        addbyte(0x89); /*MOV EDI, EDX*/
        addbyte(0xd7);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0xc1); /*SHR EDX, 12*/
        addbyte(0xea);
        addbyte(12);
        addbyte(0xf7); /*TEST EDI, 0xffc*/
        addbyte(0xc7);
        addlong(0xffc);
        addbyte(0x8b); /*MOV EDX, readlookup2[EDX*4]*/
        addbyte(0x14);
        addbyte(0x95);
        addlong((uint32_t)readlookup2);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+3+2+3+2);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0x83); /*CMP EDX, -1*/
        addbyte(0xfa);
        addbyte(-1);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+2);
        addbyte(0x8b); /*MOV EAX, [EDX+EDI]*/
        addbyte(0x04);
        addbyte(0x3a);
        addbyte(0xeb); /*JMP done*/
        addbyte(4+3+5+7+6);
        addbyte(0x89); /*slowpath: MOV [ESP+4], EAX*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addbyte(0x89); /*MOV [ESP], ESI*/
        addbyte(0x34);
        addbyte(0x24);
        addbyte(0xe8); /*CALL readmemll*/
        addlong((uint32_t)readmemll - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0x83); /*CMP abrt, 0*/
        addbyte(0x3d);
        addlong((uint32_t)&abrt);
        addbyte(0);
        addbyte(0x0f);
        addbyte(0x85); /*JNE end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        /*done:*/
        host_reg_mapping[0] = 8;
}

static void MEM_LOAD_ADDR_IMM_B(x86seg *seg, uint32_t addr)
{
        addbyte(0xb8); /*MOV EAX, addr*/
        addlong(addr);
        MEM_LOAD_ADDR_EA_B(seg);
}
static void MEM_LOAD_ADDR_IMM_W(x86seg *seg, uint32_t addr)
{
        addbyte(0xb8); /*MOV EAX, addr*/
        addlong(addr);
        MEM_LOAD_ADDR_EA_W(seg);
}
static void MEM_LOAD_ADDR_IMM_L(x86seg *seg, uint32_t addr)
{
        addbyte(0xb8); /*MOV EAX, addr*/
        addlong(addr);
        MEM_LOAD_ADDR_EA_L(seg);
}

static void MEM_STORE_ADDR_EA_B(x86seg *seg, int host_reg)
{
        addbyte(0x8b); /*MOVL ESI, seg->base*/
        addbyte(0x05 | (REG_ESI << 3));
        addlong((uint32_t)&seg->base);
        addbyte(0x01); /*ADDL ESI, EAX*/
        addbyte(0xc0 | (REG_EAX << 3) | REG_ESI);
        addbyte(0x89); /*MOV EDI, ESI*/
        addbyte(0xc0 | (REG_ESI << 3) | REG_EDI);
        addbyte(0xc1); /*SHR ESI, 12*/
        addbyte(0xe8 | REG_ESI);
        addbyte(12);
        addbyte(0xf7); /*TEST EDI, 0xfff*/
        addbyte(0xc7);
        addlong(0xfff);
        addbyte(0x8b); /*MOV ESI, readlookup2[ESI*4]*/
        addbyte(0x04 | (REG_ESI << 3));
        addbyte(0x85 | (REG_ESI << 3));
        addlong((uint32_t)writelookup2);
        addbyte(0x83); /*CMP ESI, -1*/
        addbyte(0xf8 | REG_ESI);
        addbyte(-1);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+2);
        addbyte(0x88); /*MOV [EDI+ESI],host_reg*/
        addbyte(0x04 | (host_reg << 3));
        addbyte(REG_EDI | (REG_ESI << 3));
        addbyte(0xeb); /*JMP done*/
        addbyte(4+5+4+3+5+7+6);
        addbyte(0x89); /*slowpath: MOV [ESP+4], EAX*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addbyte(0xa1); /*MOV EAX, seg->base*/
        addlong((uint32_t)&seg->base);
        addbyte(0x89); /*MOV [ESP+8], host_reg*/
        addbyte(0x44 | (host_reg << 3));
        addbyte(0x24);
        addbyte(0x08);
        addbyte(0x89); /*MOV [ESP], EAX*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0xe8); /*CALL writememb386l*/
        addlong((uint32_t)writememb386l - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0x83); /*CMP abrt, 0*/
        addbyte(0x3d);
        addlong((uint32_t)&abrt);
        addbyte(0);
        addbyte(0x0f); /*JNE end*/
        addbyte(0x85);
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        /*done:*/
}
static void MEM_STORE_ADDR_EA_W(x86seg *seg, int host_reg)
{
        addbyte(0x8b); /*MOVL ESI, seg->base*/
        addbyte(0x05 | (REG_ESI << 3));
        addlong((uint32_t)&seg->base);
        addbyte(0x01); /*ADDL ESI, EAX*/
        addbyte(0xc0 | (REG_EAX << 3) | REG_ESI);
        addbyte(0x89); /*MOV EDI, ESI*/
        addbyte(0xc0 | (REG_ESI << 3) | REG_EDI);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0xc1); /*SHR ESI, 12*/
        addbyte(0xe8 | REG_ESI);
        addbyte(12);
        addbyte(0xf7); /*TEST EDI, 0xfff*/
        addbyte(0xc7);
        addlong(0xfff);
        addbyte(0x8b); /*MOV ESI, readlookup2[ESI*4]*/
        addbyte(0x04 | (REG_ESI << 3));
        addbyte(0x85 | (REG_ESI << 3));
        addlong((uint32_t)writelookup2);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+3+2+4+2);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0x83); /*CMP ESI, -1*/
        addbyte(0xf8 | REG_ESI);
        addbyte(-1);
        addbyte(0x74); /*JE slowpath*/
        addbyte(4+2);
        addbyte(0x66); /*MOV [EDI+ESI],host_reg*/
        addbyte(0x89);
        addbyte(0x04 | (host_reg << 3));
        addbyte(REG_EDI | (REG_ESI << 3));
        addbyte(0xeb); /*JMP done*/
        addbyte(4+5+4+3+5+7+6);
        addbyte(0x89); /*slowpath: MOV [ESP+4], EAX*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addbyte(0xa1); /*MOV EAX, seg->base*/
        addlong((uint32_t)&seg->base);
        addbyte(0x89); /*MOV [ESP+8], host_reg*/
        addbyte(0x44 | (host_reg << 3));
        addbyte(0x24);
        addbyte(0x08);
        addbyte(0x89); /*MOV [ESP], EAX*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0xe8); /*CALL writememwl*/
        addlong((uint32_t)writememwl - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0x83); /*CMP abrt, 0*/
        addbyte(0x3d);
        addlong((uint32_t)&abrt);
        addbyte(0);
        addbyte(0x0f); /*JNE end*/
        addbyte(0x85);
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        /*done:*/
}
static void MEM_STORE_ADDR_EA_L(x86seg *seg, int host_reg)
{
        addbyte(0x8b); /*MOVL ESI, seg->base*/
        addbyte(0x05 | (REG_ESI << 3));
        addlong((uint32_t)&seg->base);
        addbyte(0x01); /*ADDL ESI, EAX*/
        addbyte(0xc0 | (REG_EAX << 3) | REG_ESI);
        addbyte(0x89); /*MOV EDI, ESI*/
        addbyte(0xc0 | (REG_ESI << 3) | REG_EDI);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0xc1); /*SHR ESI, 12*/
        addbyte(0xe8 | REG_ESI);
        addbyte(12);
        addbyte(0xf7); /*TEST EDI, 0xffc*/
        addbyte(0xc7);
        addlong(0xffc);
        addbyte(0x8b); /*MOV ESI, readlookup2[ESI*4]*/
        addbyte(0x04 | (REG_ESI << 3));
        addbyte(0x85 | (REG_ESI << 3));
        addlong((uint32_t)writelookup2);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+3+2+3+2);
        addbyte(0x83); /*XOR EDI, -1*/
        addbyte(0xf7);
        addbyte(-1);
        addbyte(0x83); /*CMP ESI, -1*/
        addbyte(0xf8 | REG_ESI);
        addbyte(-1);
        addbyte(0x74); /*JE slowpath*/
        addbyte(3+2);
        addbyte(0x89); /*MOV [EDI+ESI],host_reg*/
        addbyte(0x04 | (host_reg << 3));
        addbyte(REG_EDI | (REG_ESI << 3));
        addbyte(0xeb); /*JMP done*/
        addbyte(4+5+4+3+5+7+6);
        addbyte(0x89); /*slowpath: MOV [ESP+4], EAX*/
        addbyte(0x44);
        addbyte(0x24);
        addbyte(0x04);
        addbyte(0xa1); /*MOV EAX, seg->base*/
        addlong((uint32_t)&seg->base);
        addbyte(0x89); /*MOV [ESP+8], host_reg*/
        addbyte(0x44 | (host_reg << 3));
        addbyte(0x24);
        addbyte(0x08);
        addbyte(0x89); /*MOV [ESP], EAX*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0xe8); /*CALL writememll*/
        addlong((uint32_t)writememll - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0x83); /*CMP abrt, 0*/
        addbyte(0x3d);
        addlong((uint32_t)&abrt);
        addbyte(0);
        addbyte(0x0f); /*JNE end*/
        addbyte(0x85);
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        /*done:*/
}

static void MEM_STORE_ADDR_IMM_B(x86seg *seg, uint32_t addr, int host_reg)
{
        addbyte(0xb8); /*MOV EAX, addr*/
        addlong(addr);
        MEM_STORE_ADDR_EA_B(seg, host_reg);
}
static void MEM_STORE_ADDR_IMM_L(x86seg *seg, uint32_t addr, int host_reg)
{
        addbyte(0xb8); /*MOV EAX, addr*/
        addlong(addr);
        MEM_STORE_ADDR_EA_L(seg, host_reg);
}
static void MEM_STORE_ADDR_IMM_W(x86seg *seg, uint32_t addr, int host_reg)
{
        addbyte(0xb8); /*MOV EAX, addr*/
        addlong(addr);
        MEM_STORE_ADDR_EA_W(seg, host_reg);
}


static x86seg *FETCH_EA_16(x86seg *op_ea_seg, uint32_t fetchdat, int op_ssegs, uint32_t *op_pc)
{
        int mod = (fetchdat >> 6) & 3;
        int reg = (fetchdat >> 3) & 7;
        int rm = fetchdat & 7;
        if (!mod && rm == 6) 
        { 
                addbyte(0xb8); /*MOVL EAX, imm16*/
                addlong((fetchdat >> 8) & 0xffff);
                (*op_pc) += 2;
        }
        else
        {
                switch (mod)
                {
                        case 0:
                        addbyte(0xa1); /*MOVL EAX, *mod1add[0][rm]*/
                        addlong((uint32_t)mod1add[0][rm]);
                        if (mod1add[1][rm] != &zero)
                        {
                                addbyte(0x03); /*ADDL EAX, *mod1add[1][rm]*/
                                addbyte(0x05);
                                addlong((uint32_t)mod1add[1][rm]);
                        }
                        break;
                        case 1:
                        addbyte(0xa1); /*MOVL EAX, *mod1add[0][rm]*/
                        addlong((uint32_t)mod1add[0][rm]);
                        addbyte(0x83); /*ADDL EAX, imm8*/
                        addbyte(0xc0 | REG_EAX);
                        addbyte((int8_t)(rmdat >> 8));
                        if (mod1add[1][rm] != &zero)
                        {
                                addbyte(0x03); /*ADDL EAX, *mod1add[1][rm]*/
                                addbyte(0x05);
                                addlong((uint32_t)mod1add[1][rm]);
                        }
                        (*op_pc)++;
                        break;
                        case 2:
                        addbyte(0xb8); /*MOVL EAX, imm16*/
                        addlong((fetchdat >> 8) & 0xffff);// pc++;
                        addbyte(0x03); /*ADDL EAX, *mod1add[0][rm]*/
                        addbyte(0x05);
                        addlong((uint32_t)mod1add[0][rm]);
                        if (mod1add[1][rm] != &zero)
                        {
                                addbyte(0x03); /*ADDL EAX, *mod1add[1][rm]*/
                                addbyte(0x05);
                                addlong((uint32_t)mod1add[1][rm]);
                        }
                        (*op_pc) += 2;
                        break;
                }
                addbyte(0x25); /*ANDL EAX, 0xffff*/
                addlong(0xffff);

                if (mod1seg[rm] == &ss && !op_ssegs)
                        op_ea_seg = &_ss;
        }
        return op_ea_seg;
}

static x86seg *FETCH_EA_32(x86seg *op_ea_seg, uint32_t fetchdat, int op_ssegs, uint32_t *op_pc, int stack_offset)
{
        uint32_t new_eaaddr;
        int mod = (fetchdat >> 6) & 3;
        int reg = (fetchdat >> 3) & 7;
        int rm = fetchdat & 7;

        if (rm == 4)
        {
                uint8_t sib = fetchdat >> 8;
                (*op_pc)++;
                
                switch (mod)
                {
                        case 0:
                        if ((sib & 7) == 5)
                        {
                                new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                                addbyte(0xb8); /*MOVL EAX, imm32*/
                                addlong(new_eaaddr);// pc++;
                                (*op_pc) += 4;
                        }
                        else
                        {
                                addbyte(0x8b); /*MOVL EAX, regs[sib&7].l*/
                                addbyte(0x45);
                                addbyte((uint32_t)&regs[sib & 7].l - (uint32_t)&EAX);
                        }
                        break;
                        case 1: 
                        addbyte(0x8b); /*MOVL EAX, regs[sib&7].l*/
                        addbyte(0x45);
                        addbyte((uint32_t)&regs[sib & 7].l - (uint32_t)&EAX);
                        addbyte(0x83); /*ADDL EAX, imm8*/
                        addbyte(0xc0 | REG_EAX);
                        addbyte((int8_t)(rmdat >> 16));
                        (*op_pc)++;
                        break;
                        case 2:
                        new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                        addbyte(0xb8); /*MOVL EAX, new_eaaddr*/
                        addlong(new_eaaddr);
                        addbyte(0x03); /*ADDL EAX, regs[sib&7].l*/
                        addbyte(0x45);
                        addbyte((uint32_t)&regs[sib & 7].l - (uint32_t)&EAX);
                        (*op_pc) += 4;
                        break;
                }
                if (stack_offset && (sib & 7) == 4 && (mod || (sib & 7) != 5)) /*ESP*/
                {
                        if (stack_offset < 0x80 || stack_offset >= 0xffffff80)
                        {
                                addbyte(0x83);
                                addbyte(0xc0 | REG_EAX);
                                addbyte(stack_offset);
                        }
                        else
                        {
                                addbyte(0x05); /*ADDL EAX, stack_offset*/
                                addlong(stack_offset);
                        }
                }
                if (((sib & 7) == 4 || (mod && (sib & 7) == 5)) && !op_ssegs)
                        op_ea_seg = &_ss;
                if (((sib >> 3) & 7) != 4)
                {
                        switch (sib >> 6)
                        {
                                case 0:
                                addbyte(0x03); /*ADDL EAX, regs[sib&7].l*/
                                addbyte(0x45);
                                addbyte((uint32_t)&regs[(sib >> 3) & 7].l - (uint32_t)&EAX);
                                break;
                                case 1:
                                addbyte(0x8B); addbyte(0x45 | (REG_EDI << 3)); addbyte((uint32_t)&regs[(sib >> 3) & 7].l - (uint32_t)&EAX); /*MOVL EDI, reg*/
                                addbyte(0x01); addbyte(0xc0 | REG_EAX | (REG_EDI << 3)); /*ADDL EAX, EDI*/
                                addbyte(0x01); addbyte(0xc0 | REG_EAX | (REG_EDI << 3)); /*ADDL EAX, EDI*/
                                break;
                                case 2:
                                addbyte(0x8B); addbyte(0x45 | (REG_EDI << 3)); addbyte((uint32_t)&regs[(sib >> 3) & 7].l - (uint32_t)&EAX); /*MOVL EDI, reg*/
                                addbyte(0xC1); addbyte(0xE0 | REG_EDI); addbyte(2); /*SHL EDI, 2*/
                                addbyte(0x01); addbyte(0xc0 | REG_EAX | (REG_EDI << 3)); /*ADDL EAX, EDI*/
                                break;
                                case 3:
                                addbyte(0x8B); addbyte(0x45 | (REG_EDI << 3)); addbyte((uint32_t)&regs[(sib >> 3) & 7].l - (uint32_t)&EAX); /*MOVL EDI reg*/
                                addbyte(0xC1); addbyte(0xE0 | REG_EDI); addbyte(3); /*SHL EDI, 3*/
                                addbyte(0x01); addbyte(0xc0 | REG_EAX | (REG_EDI << 3)); /*ADDL EAX, EDI*/
                                break;
                        }
                }
        }
        else
        {
                if (!mod && rm == 5)
                {                
                        new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                        addbyte(0xb8); /*MOVL EAX, imm32*/
                        addlong(new_eaaddr);
                        (*op_pc) += 4;
                        return op_ea_seg;
                }
                addbyte(0x8b); /*MOVL EAX, regs[rm].l*/
                addbyte(0x45);
                addbyte((uint32_t)&regs[rm].l - (uint32_t)&EAX);
                eaaddr = regs[rm].l;
                if (mod) 
                {
                        if (rm == 5 && !op_ssegs)
                                op_ea_seg = &_ss;
                        if (mod == 1) 
                        {
                                addbyte(0x83); /*ADD EAX, imm8*/
                                addbyte(0xc0 | REG_EAX);
                                addbyte((int8_t)(fetchdat >> 8)); 
                                (*op_pc)++; 
                        }
                        else          
                        {
                                new_eaaddr = fastreadl(cs + (*op_pc) + 1);
                                addbyte(0x05); /*ADD EAX, imm32*/
                                addlong(new_eaaddr); 
                                (*op_pc) += 4;
                        }
                }
        }
        return op_ea_seg;
}

static x86seg *FETCH_EA(x86seg *op_ea_seg, uint32_t fetchdat, int op_ssegs, uint32_t *op_pc, uint32_t op_32)
{
        if (op_32 & 0x200)
                return FETCH_EA_32(op_ea_seg, fetchdat, op_ssegs, op_pc, 0);
        return FETCH_EA_16(op_ea_seg, fetchdat, op_ssegs, op_pc);
}


static void LOAD_STACK_TO_EA(int off)
{
        if (stack32)
        {
                addbyte(0x8b); /*MOVL EAX,[ESP]*/
                addbyte(0x45 | (REG_EAX << 3));
                addbyte((uint32_t)&ESP - (uint32_t)&EAX);
                if (off)
                {
                        addbyte(0x83); /*ADD EAX, off*/
                        addbyte(0xc0 | (0 << 3) | REG_EAX);
                        addbyte(off);
                }
        }
        else
        {
                addbyte(0x0f); /*MOVZX EAX,W[ESP]*/
                addbyte(0xb7);
                addbyte(0x45 | (REG_EAX << 3));
                addbyte((uint32_t)&ESP - (uint32_t)&EAX);
                if (off)
                {
                        addbyte(0x66); /*ADD AX, off*/
                        addbyte(0x05);
                        addword(off);
                }
        }
}

static void LOAD_EBP_TO_EA(int off)
{
        if (stack32)
        {
                addbyte(0x8b); /*MOVL EAX,[EBP]*/
                addbyte(0x45 | (REG_EAX << 3));
                addbyte((uint32_t)&EBP - (uint32_t)&EAX);
                if (off)
                {
                        addbyte(0x83); /*ADD EAX, off*/
                        addbyte(0xc0 | (0 << 3) | REG_EAX);
                        addbyte(off);
                }
        }
        else
        {
                addbyte(0x0f); /*MOVZX EAX,W[EBP]*/
                addbyte(0xb7);
                addbyte(0x45 | (REG_EAX << 3));
                addbyte((uint32_t)&EBP - (uint32_t)&EAX);
                if (off)
                {
                        addbyte(0x66); /*ADD AX, off*/
                        addbyte(0x05);
                        addword(off);
                }
        }
}

static void SP_MODIFY(int off)
{
        if (stack32)
        {
                if (off < 0x80)
                {
                        addbyte(0x83); /*ADD [ESP], off*/
                        addbyte(0x45);
                        addbyte((uint32_t)&ESP - (uint32_t)&EAX);
                        addbyte(off);
                }
                else
                {
                        addbyte(0x81); /*ADD [ESP], off*/
                        addbyte(0x45);
                        addbyte((uint32_t)&ESP - (uint32_t)&EAX);
                        addlong(off);
                }
        }
        else
        {
                if (off < 0x80)
                {
                        addbyte(0x66); /*ADD [SP], off*/
                        addbyte(0x83);
                        addbyte(0x45);
                        addbyte((uint32_t)&SP - (uint32_t)&EAX);
                        addbyte(off);
                }
                else
                {
                        addbyte(0x66); /*ADD [SP], off*/
                        addbyte(0x81);
                        addbyte(0x45);
                        addbyte((uint32_t)&SP - (uint32_t)&EAX);
                        addword(off);
                }
        }
}


static void TEST_ZERO_JUMP_W(int host_reg, uint32_t new_pc, int taken_cycles)
{
        addbyte(0x66); /*CMPW host_reg, 0*/
        addbyte(0x83);
        addbyte(0xc0 | 0x38 | host_reg);
        addbyte(0);
        addbyte(0x75); /*JNZ +*/
        addbyte(10+5+(taken_cycles ? 7 : 0));
        addbyte(0xC7); /*MOVL [pc], new_pc*/
        addbyte(0x05);
        addlong((uintptr_t)&pc);
        addlong(new_pc);
        if (taken_cycles)
        {
                addbyte(0x83); /*SUB $codegen_block_cycles, cyclcs*/
                addbyte(0x2d);
                addlong((uintptr_t)&cycles);
                addbyte(taken_cycles);
        }
        addbyte(0xe9); /*JMP end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
}
static void TEST_ZERO_JUMP_L(int host_reg, uint32_t new_pc, int taken_cycles)
{
        addbyte(0x83); /*CMPW host_reg, 0*/
        addbyte(0xc0 | 0x38 | host_reg);
        addbyte(0);
        addbyte(0x75); /*JNZ +*/
        addbyte(10+5+(taken_cycles ? 7 : 0));
        addbyte(0xC7); /*MOVL [pc], new_pc*/
        addbyte(0x05);
        addlong((uintptr_t)&pc);
        addlong(new_pc);
        if (taken_cycles)
        {
                addbyte(0x83); /*SUB $codegen_block_cycles, cyclcs*/
                addbyte(0x2d);
                addlong((uintptr_t)&cycles);
                addbyte(taken_cycles);
        }
        addbyte(0xe9); /*JMP end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
}

static void TEST_NONZERO_JUMP_W(int host_reg, uint32_t new_pc, int taken_cycles)
{
        addbyte(0x66); /*CMPW host_reg, 0*/
        addbyte(0x83);
        addbyte(0xc0 | 0x38 | host_reg);
        addbyte(0);
        addbyte(0x74); /*JZ +*/
        addbyte(10+5+(taken_cycles ? 7 : 0));
        addbyte(0xC7); /*MOVL [pc], new_pc*/
        addbyte(0x05);
        addlong((uintptr_t)&pc);
        addlong(new_pc);
        if (taken_cycles)
        {
                addbyte(0x83); /*SUB $codegen_block_cycles, cyclcs*/
                addbyte(0x2d);
                addlong((uintptr_t)&cycles);
                addbyte(taken_cycles);
        }
        addbyte(0xe9); /*JMP end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
}
static void TEST_NONZERO_JUMP_L(int host_reg, uint32_t new_pc, int taken_cycles)
{
        addbyte(0x83); /*CMPW host_reg, 0*/
        addbyte(0xc0 | 0x38 | host_reg);
        addbyte(0);
        addbyte(0x74); /*JZ +*/
        addbyte(10+5+(taken_cycles ? 7 : 0));
        addbyte(0xC7); /*MOVL [pc], new_pc*/
        addbyte(0x05);
        addlong((uintptr_t)&pc);
        addlong(new_pc);
        if (taken_cycles)
        {
                addbyte(0x83); /*SUB $codegen_block_cycles, cyclcs*/
                addbyte(0x2d);
                addlong((uintptr_t)&cycles);
                addbyte(taken_cycles);
        }
        addbyte(0xe9); /*JMP end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
}


static void FP_ENTER()
{
        if (codegen_fpu_entered)
                return;
                
        addbyte(0xf6); /*TEST cr0, 0xc*/
        addbyte(0x05);
        addlong((uintptr_t)&cr0);
        addbyte(0xc);
        addbyte(0x74); /*JZ +*/
        addbyte(7+5+5);
        addbyte(0xc7); /*MOV [ESP], 7*/
        addbyte(0x04);
        addbyte(0x24);
        addlong(7);
        addbyte(0xe8); /*CALL x86_int*/
        addlong((uint32_t)x86_int - (uint32_t)(&codeblock[block_current].data[block_pos + 4]));
        addbyte(0xe9); /*JMP end*/
        addlong(BLOCK_EXIT_OFFSET - (block_pos + 4));
        
        codegen_fpu_entered = 1;
}

static void FP_FLD(int reg)
{
        addbyte(0xa1); /*MOV EAX, [TOP]*/
        addlong((uintptr_t)&TOP);
        addbyte(0x89); /*MOV EBX, EAX*/
        addbyte(0xc3);
        if (reg)
        {
                addbyte(0x83); /*ADD EAX, reg*/
                addbyte(0xc0);
                addbyte(reg);
                addbyte(0x83); /*SUB EBX, 1*/
                addbyte(0xeb);
                addbyte(0x01);
                addbyte(0x83); /*AND EAX, 7*/
                addbyte(0xe0);
                addbyte(0x07);
        }
        else
        {
                addbyte(0x83); /*SUB EBX, 1*/
                addbyte(0xeb);
                addbyte(0x01);
        }       

        addbyte(0xdd); /*FLD [ST+EAX*8]*/
        addbyte(0x04);
        addbyte(0xc5);
        addlong((uintptr_t)ST);
        addbyte(0x83); /*AND EBX, 7*/
        addbyte(0xe3);
        addbyte(0x07);
        addbyte(0x8a); /*MOV AL, [tag+EAX]*/
        addbyte(0x80);
        addlong((uintptr_t)tag);
        addbyte(0xdd); /*FSTP [ST+EBX*8]*/
        addbyte(0x1c);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0x88); /*MOV [tag+EBX], AL*/
        addbyte(0x83);
        addlong((uintptr_t)tag);

        addbyte(0x89); /*MOV [TOP], EBX*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
}

static void FP_FST(int reg)
{
        addbyte(0xa1); /*MOV EAX, [TOP]*/
        addlong((uintptr_t)&TOP);
        addbyte(0xdd); /*FLD [ST+EAX*8]*/
        addbyte(0x04);
        addbyte(0xc5);
        addlong((uintptr_t)ST);
        addbyte(0x88); /*MOV BL, [tag+EAX]*/
        addbyte(0x98);
        addlong((uintptr_t)tag);

        if (reg)
        {
                addbyte(0x83); /*ADD EAX, reg*/
                addbyte(0xc0);
                addbyte(reg);
                addbyte(0x83); /*AND EAX, 7*/
                addbyte(0xe0);
                addbyte(0x07);
        }

        addbyte(0xdd); /*FSTP [ST+EAX*8]*/
        addbyte(0x1c);
        addbyte(0xc5);
        addlong((uintptr_t)ST);
        addbyte(0x8a); /*MOV [tag+EAX], BL*/
        addbyte(0x98);
        addlong((uintptr_t)tag);
}

static void FP_FXCH(int reg)
{
        addbyte(0xa1); /*MOV EAX, [TOP]*/
        addlong((uintptr_t)&TOP);
        addbyte(0x89); /*MOV EBX, EAX*/
        addbyte(0xc3);
        addbyte(0x83); /*ADD EAX, reg*/
        addbyte(0xc0);
        addbyte(reg);
//#if 0
        addbyte(0xdd); /*FLD [ST+EBX*8]*/
        addbyte(0x04);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0x83); /*AND EAX, 7*/
        addbyte(0xe0);
        addbyte(0x07);
        addbyte(0xdd); /*FLD [ST+EAX*8]*/
        addbyte(0x04);
        addbyte(0xc5);
        addlong((uintptr_t)ST);
        addbyte(0xdd); /*FSTP [ST+EBX*8]*/
        addbyte(0x1c);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0xdd); /*FSTP [ST+EAX*8]*/
        addbyte(0x1c);
        addbyte(0xc5);
        addlong((uintptr_t)ST);
//#endif
#if 0
        addbyte(0xbe); /*MOVL ESI, ST*/
        addlong((uintptr_t)ST);
        
        addbyte(0x8b); /*MOVL EDX, [ESI+EBX*8]*/
        addbyte(0x14);
        addbyte(0xde);
        addbyte(0x83); /*AND EAX, 7*/
        addbyte(0xe0);
        addbyte(0x07);
        addbyte(0x8b); /*MOVL ECX, [ESI+EAX*8]*/
        addbyte(0x0c);
        addbyte(0xc6);
        addbyte(0x89); /*MOVL [ESI+EBX*8], ECX*/
        addbyte(0x0c);
        addbyte(0xde);
        addbyte(0x89); /*MOVL [ESI+EAX*8], EDX*/
        addbyte(0x14);
        addbyte(0xc6);

        addbyte(0x8b); /*MOVL ECX, [4+ESI+EAX*8]*/
        addbyte(0x4c);
        addbyte(0xc6);
        addbyte(0x04);
        addbyte(0x8b); /*MOVL EDX, [4+ESI+EBX*8]*/
        addbyte(0x54);
        addbyte(0xde);
        addbyte(0x04);
        addbyte(0x89); /*MOVL [4+ESI+EBX*8], ECX*/
        addbyte(0x4c);
        addbyte(0xde);
        addbyte(0x04);
        addbyte(0x89); /*MOVL [4+ESI+EAX*8], EDX*/
        addbyte(0x54);
        addbyte(0xc6);
        addbyte(0x04);
#endif
}


static void FP_LOAD_S()
{
        addbyte(0x8b); /*MOV EBX, TOP*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        addbyte(0x89); /*MOV [ESP], EAX*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0x83); /*SUB EBX, 1*/
        addbyte(0xeb);
        addbyte(1);
        addbyte(0xd9); /*FLD [ESP]*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0x83); /*AND EBX, 7*/
        addbyte(0xe3);
        addbyte(7);
        addbyte(0x83); /*CMP EAX, 0*/
        addbyte(0xf8);
        addbyte(0);
        addbyte(0x89); /*MOV TOP, EBX*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        addbyte(0xdd); /*FSTP [ST+EBX*8]*/
        addbyte(0x1c);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0x0f); /*SETE [tag+EBX]*/
        addbyte(0x94);
        addbyte(0x83);
        addlong((uintptr_t)tag);
}
static void FP_LOAD_IL()
{
        addbyte(0x8b); /*MOV EBX, TOP*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        addbyte(0x89); /*MOV [ESP], EAX*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0x83); /*SUB EBX, 1*/
        addbyte(0xeb);
        addbyte(1);
        addbyte(0xdb); /*FILDl [ESP]*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0x83); /*AND EBX, 7*/
        addbyte(0xe3);
        addbyte(7);
        addbyte(0x83); /*CMP EAX, 0*/
        addbyte(0xf8);
        addbyte(0);
        addbyte(0x89); /*MOV TOP, EBX*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        addbyte(0xdd); /*FSTP [ST+EBX*8]*/
        addbyte(0x1c);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0x0f); /*SETE [tag+EBX]*/
        addbyte(0x94);
        addbyte(0x83);
        addlong((uintptr_t)tag);
}

static int FP_LOAD_REG(int reg)
{
        addbyte(0x8b); /*MOV EBX, TOP*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        if (reg)
        {
                addbyte(0x83); /*ADD EBX, reg*/
                addbyte(0xc3);
                addbyte(reg);
                addbyte(0x83); /*AND EBX, 7*/
                addbyte(0xe3);
                addbyte(7);
        }
        addbyte(0xdd); /*FLD ST[EBX*8]*/
        addbyte(0x04);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0xd9); /*FSTP [ESP]*/
        addbyte(0x1c);
        addbyte(0x24);
        addbyte(0x8b); /*MOV EAX, [ESP]*/
        addbyte(0x04 | (REG_EBX << 3));
        addbyte(0x24);
        
        return REG_EBX;
}

static double _fp_half = 0.5;

static int FP_LOAD_REG_INT(int reg)
{
        addbyte(0x8b); /*MOV EBX, TOP*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        if (reg)
        {
                addbyte(0x83); /*ADD EBX, reg*/
                addbyte(0xc3);
                addbyte(reg);
                addbyte(0x83); /*AND EBX, 7*/
                addbyte(0xe3);
                addbyte(7);
        }
        addbyte(0xdd); /*FLD ST[EBX*8]*/
        addbyte(0x04);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        
        /*Slightly dodgy assumption here, that the rounding mode will be the same
          each time the block is called. It probably will be though. */
        switch ((npxc>>10)&3)
        {
                case 0: /*Nearest*/
                break;
                case 1: /*Down*/
                addbyte(0x9b); /*FSTCW [ESP+4]*/
                addbyte(0xd9);
                addbyte(0x7c);
                addbyte(0x24);
                addbyte(0x04);
                addbyte(0xbe); /*MOVL ESI, 0x400*/
                addlong(0x400);
                addbyte(0x0b); /*ORL ESI, [ESP+4]*/
                addbyte(0x74);
                addbyte(0x24);
                addbyte(0x04);
                addbyte(0x81); /*ANDL ESI, ~0x800*/
                addbyte(0xe6);
                addlong(~0x800);
                addbyte(0x89); /*MOVL [ESP], ESI*/
                addbyte(0x34);
                addbyte(0x24);
                addbyte(0xd9); /*FLDCW [ESP]*/
                addbyte(0x2c);
                addbyte(0x24);
                break;
                case 2: /*Up*/
                addbyte(0x9b); /*FSTCW [ESP+4]*/
                addbyte(0xd9);
                addbyte(0x7c);
                addbyte(0x24);
                addbyte(0x04);
                addbyte(0xbe); /*MOVL ESI, 0x800*/
                addlong(0x800);
                addbyte(0x0b); /*ORL ESI, [ESP+4]*/
                addbyte(0x74);
                addbyte(0x24);
                addbyte(0x04);
                addbyte(0x81); /*ANDL ESI, ~0x400*/
                addbyte(0xe6);
                addlong(~0x400);
                addbyte(0x89); /*MOVL [ESP], ESI*/
                addbyte(0x34);
                addbyte(0x24);
                addbyte(0xd9); /*FLDCW [ESP]*/
                addbyte(0x2c);
                addbyte(0x24);
                break;
                case 3: /*Chop*/
                addbyte(0x9b); /*FSTCW [ESP+4]*/
                addbyte(0xd9);
                addbyte(0x7c);
                addbyte(0x24);
                addbyte(0x04);
                addbyte(0xbe); /*MOVL ESI, 0x300*/
                addlong(0x300);
                addbyte(0x0b); /*ORL ESI, [ESP+4]*/
                addbyte(0x74);
                addbyte(0x24);
                addbyte(0x04);
                addbyte(0x89); /*MOVL [ESP], ESI*/
                addbyte(0x34);
                addbyte(0x24);
                addbyte(0xd9); /*FLDCW [ESP]*/
                addbyte(0x2c);
                addbyte(0x24);
                break;
        }
        
        addbyte(0xdb); /*FISTPl [ESP]*/
        addbyte(0x1c);
        addbyte(0x24);

        switch ((npxc>>10)&3)
        {
                case 1: /*Down*/
                case 2: /*Up*/
                case 3: /*Chop*/
                addbyte(0xd9); /*FLDCW [ESP+4]*/
                addbyte(0x6c);
                addbyte(0x24);
                addbyte(0x04);
                break;
        }

        addbyte(0x8b); /*MOV EAX, [ESP]*/
        addbyte(0x04 | (REG_EBX << 3));
        addbyte(0x24);
        
        return REG_EBX;
}

static void FP_POP()
{
        addbyte(0xa1); /*MOV EAX, TOP*/
        addlong((uintptr_t)&TOP);
        addbyte(0xc6); /*MOVB tag[EAX], 3*/
        addbyte(0x80);
        addlong((uintptr_t)tag);
        addbyte(3);
        addbyte(0x04); /*ADD AL, 1*/
        addbyte(1);
        addbyte(0x24); /*AND AL, 7*/
        addbyte(7);
        addbyte(0xa2); /*MOV TOP, AL*/
        addlong((uintptr_t)&TOP);
}

#define FPU_ADD 0x00
#define FPU_DIV 0x30
#define FPU_MUL 0x08
#define FPU_SUB 0x20

static void FP_OP_S(int op)
{
        addbyte(0x8b); /*MOV EBX, TOP*/
        addbyte(0x1d);
        addlong((uintptr_t)&TOP);
        addbyte(0x89); /*MOV [ESP], EAX*/
        addbyte(0x04);
        addbyte(0x24);
        addbyte(0xdd); /*FLD ST[EBX*8]*/
        addbyte(0x04);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
        addbyte(0xd8); /*FADD [ESP]*/
        addbyte(0x04 | op);
        addbyte(0x24);
        addbyte(0xdd); /*FSTP [ST+EBX*8]*/
        addbyte(0x1c);
        addbyte(0xdd);
        addlong((uintptr_t)ST);
}

static void FP_OP_REG(int op, int dst, int src)
{
        addbyte(0xa1); /*MOV EAX, TOP*/
        addlong((uintptr_t)&TOP);
        addbyte(0xbe); /*MOVL ESI, ST*/
        addlong((uintptr_t)ST);
        addbyte(0x89); /*MOV EBX, EAX*/
        addbyte(0xc3);
        if (src || dst)
        {
                addbyte(0x83); /*ADD EAX, 1*/
                addbyte(0xc0);
                addbyte(src ? src : dst);
                addbyte(0x83); /*AND EAX, 7*/
                addbyte(0xe0);
                addbyte(7);
        }

        if (src)
        {
                addbyte(0xdd); /*FLD ST[EBX*8]*/
                addbyte(0x04);
                addbyte(0xde);
                addbyte(0xdc); /*FADD ST[EAX*8]*/
                addbyte(0x04 | op);
                addbyte(0xc6);
                addbyte(0xdd); /*FSTP ST[EBX*8]*/
                addbyte(0x1c);
                addbyte(0xde);
        }
        else
        {
                addbyte(0xdd); /*FLD [ESI+EAX*8]*/
                addbyte(0x04);
                addbyte(0xc6);
                addbyte(0xdc); /*FADD ST[EBX*8]*/
                addbyte(0x04 | op);
                addbyte(0xde);
                addbyte(0xdd); /*FSTP ST[EAX*8]*/
                addbyte(0x1c);
                addbyte(0xc6);
        }
}
