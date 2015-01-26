static uint32_t ropINC_rw(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_W(opcode & 7);
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        ADD_HOST_REG_IMM(host_reg, 1);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, 1);
        AND_HOST_REG_IMM(host_reg, 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD16);
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);
        STORE_REG_W_RELEASE(host_reg);

        return op_pc;
}
static uint32_t ropINC_rl(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_L(opcode & 7);
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        ADD_HOST_REG_IMM(host_reg, 1);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, 1);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD32);
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);
        STORE_REG_L_RELEASE(host_reg);

        return op_pc;
}
static uint32_t ropDEC_rw(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_W(opcode & 7);
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        SUB_HOST_REG_IMM(host_reg, 1);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, 1);
        AND_HOST_REG_IMM(host_reg, 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);
        STORE_REG_W_RELEASE(host_reg);

        return op_pc;
}
static uint32_t ropDEC_rl(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_L(opcode & 7);
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        SUB_HOST_REG_IMM(host_reg, 1);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, 1);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);
        STORE_REG_L_RELEASE(host_reg);

        return op_pc;
}

#define ROP_ARITH(name, op, writeback) \
        static uint32_t rop ## name ## _b_rmw(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)    \
        {                                                                                                                               \
                int src_reg, dst_reg;                                                                                                   \
                                                                                                                                        \
                if ((fetchdat & 0xc0) != 0xc0)                                                                                          \
                        return 0;                                                                                                       \
                                                                                                                                        \
                dst_reg = LOAD_REG_B(fetchdat & 7);                                                                                     \
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ ## op ## 8);                                                               \
                src_reg = LOAD_REG_B((fetchdat >> 3) & 7);                                                                              \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, dst_reg);                                                                     \
                op ## _HOST_REG_B(dst_reg, src_reg);                                                                                    \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op2, src_reg);                                                                     \
                FLAG_C_COPY();                                                                                                          \
                STORE_HOST_REG_ADDR((uint32_t)&flags_res, dst_reg);                                                                     \
                if (writeback) STORE_REG_B_RELEASE(dst_reg);                                                                            \
                else           RELEASE_REG(dst_reg);                                                                                    \
                RELEASE_REG(src_reg);                                                                                                   \
                                                                                                                                        \
                return op_pc + 1;                                                                                                       \
        }                                                                                                                               \
        static uint32_t rop ## name ## _w_rmw(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)    \
        {                                                                                                                               \
                int src_reg, dst_reg;                                                                                                   \
                                                                                                                                        \
                if ((fetchdat & 0xc0) != 0xc0)                                                                                          \
                        return 0;                                                                                                       \
                                                                                                                                        \
                dst_reg = LOAD_REG_W(fetchdat & 7);                                                                                     \
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ ## op ## 16);                                                              \
                src_reg = LOAD_REG_W((fetchdat >> 3) & 7);                                                                              \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, dst_reg);                                                                     \
                op ## _HOST_REG_W(dst_reg, src_reg);                                                                                    \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op2, src_reg);                                                                     \
                FLAG_C_COPY();                                                                                                          \
                STORE_HOST_REG_ADDR((uint32_t)&flags_res, dst_reg);                                                                     \
                if (writeback) STORE_REG_W_RELEASE(dst_reg);                                                                            \
                else           RELEASE_REG(dst_reg);                                                                                    \
                RELEASE_REG(src_reg);                                                                                                   \
                                                                                                                                        \
                return op_pc + 1;                                                                                                       \
        }                                                                                                                               \
        static uint32_t rop ## name ## _l_rmw(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)    \
        {                                                                                                                               \
                int src_reg, dst_reg;                                                                                                   \
                                                                                                                                        \
                if ((fetchdat & 0xc0) != 0xc0)                                                                                          \
                        return 0;                                                                                                       \
                                                                                                                                        \
                dst_reg = LOAD_REG_L(fetchdat & 7);                                                                                     \
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ ## op ## 32);                                                              \
                src_reg = LOAD_REG_L((fetchdat >> 3) & 7);                                                                              \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, dst_reg);                                                                     \
                op ## _HOST_REG_L(dst_reg, src_reg);                                                                                    \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op2, src_reg);                                                                     \
                FLAG_C_COPY();                                                                                                          \
                STORE_HOST_REG_ADDR((uint32_t)&flags_res, dst_reg);                                                                     \
                if (writeback) STORE_REG_L_RELEASE(dst_reg);                                                                            \
                else           RELEASE_REG(dst_reg);                                                                                    \
                RELEASE_REG(src_reg);                                                                                                   \
                                                                                                                                        \
                return op_pc + 1;                                                                                                       \
        }                                                                                                                               \
        static uint32_t rop ## name ## _b_rm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)     \
        {                                                                                                                               \
                int src_reg, dst_reg;                                                                                                   \
                                                                                                                                        \
                if ((fetchdat & 0xc0) != 0xc0)                                                                                          \
                        return 0;                                                                                                       \
                                                                                                                                        \
                dst_reg = LOAD_REG_B((fetchdat >> 3) & 7);                                                                              \
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ ## op ## 8);                                                               \
                src_reg = LOAD_REG_B(fetchdat & 7);                                                                                     \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, dst_reg);                                                                     \
                op ## _HOST_REG_B(dst_reg, src_reg);                                                                                    \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op2, src_reg);                                                                     \
                FLAG_C_COPY();                                                                                                          \
                STORE_HOST_REG_ADDR((uint32_t)&flags_res, dst_reg);                                                                     \
                if (writeback) STORE_REG_B_RELEASE(dst_reg);                                                                            \
                else           RELEASE_REG(dst_reg);                                                                                    \
                RELEASE_REG(src_reg);                                                                                                   \
                                                                                                                                        \
                return op_pc + 1;                                                                                                       \
        }                                                                                                                               \
        static uint32_t rop ## name ## _w_rm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)     \
        {                                                                                                                               \
                int src_reg, dst_reg;                                                                                                   \
                                                                                                                                        \
                if ((fetchdat & 0xc0) != 0xc0)                                                                                          \
                        return 0;                                                                                                       \
                                                                                                                                        \
                dst_reg = LOAD_REG_W((fetchdat >> 3) & 7);                                                                              \
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ ## op ## 16);                                                              \
                src_reg = LOAD_REG_W(fetchdat & 7);                                                                                     \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, dst_reg);                                                                     \
                op ## _HOST_REG_W(dst_reg, src_reg);                                                                                    \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op2, src_reg);                                                                     \
                FLAG_C_COPY();                                                                                                          \
                STORE_HOST_REG_ADDR((uint32_t)&flags_res, dst_reg);                                                                     \
                if (writeback) STORE_REG_W_RELEASE(dst_reg);                                                                            \
                else           RELEASE_REG(dst_reg);                                                                                    \
                RELEASE_REG(src_reg);                                                                                                   \
                                                                                                                                        \
                return op_pc + 1;                                                                                                       \
        }                                                                                                                               \
        static uint32_t rop ## name ## _l_rm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)     \
        {                                                                                                                               \
                int src_reg, dst_reg;                                                                                                   \
                                                                                                                                        \
                if ((fetchdat & 0xc0) != 0xc0)                                                                                          \
                        return 0;                                                                                                       \
                                                                                                                                        \
                dst_reg = LOAD_REG_L((fetchdat >> 3) & 7);                                                                              \
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ ## op ## 32);                                                              \
                src_reg = LOAD_REG_L(fetchdat & 7);                                                                                     \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, dst_reg);                                                                     \
                op ## _HOST_REG_L(dst_reg, src_reg);                                                                                    \
                STORE_HOST_REG_ADDR((uint32_t)&flags_op2, src_reg);                                                                     \
                FLAG_C_COPY();                                                                                                          \
                STORE_HOST_REG_ADDR((uint32_t)&flags_res, dst_reg);                                                                     \
                if (writeback) STORE_REG_L_RELEASE(dst_reg);                                                                            \
                else           RELEASE_REG(dst_reg);                                                                                    \
                RELEASE_REG(src_reg);                                                                                                   \
                                                                                                                                        \
                return op_pc + 1;                                                                                                       \
        }

ROP_ARITH(ADD, ADD, 1)
ROP_ARITH(SUB, SUB, 1)
ROP_ARITH(CMP, SUB, 0)

static uint32_t ropADD_AL_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_B(REG_AL);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        ADD_HOST_REG_IMM_B(host_reg, fetchdat & 0xff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat & 0xff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD8);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        STORE_REG_B_RELEASE(host_reg);
        
        return op_pc + 1;
}
static uint32_t ropADD_AX_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_W(REG_AX);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        ADD_HOST_REG_IMM_W(host_reg, fetchdat & 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat & 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD16);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        STORE_REG_W_RELEASE(host_reg);
        
        return op_pc + 2;
}
static uint32_t ropADD_EAX_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_L(REG_EAX);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        fetchdat = fastreadl(cs + op_pc);
        ADD_HOST_REG_IMM(host_reg, fetchdat);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD32);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        STORE_REG_L_RELEASE(host_reg);
        
        return op_pc + 4;
}

static uint32_t ropCMP_AL_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_B(REG_AL);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        SUB_HOST_REG_IMM_B(host_reg, fetchdat & 0xff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat & 0xff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB8);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        RELEASE_REG(host_reg);
        
        return op_pc + 1;
}
static uint32_t ropCMP_AX_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_W(REG_AX);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        SUB_HOST_REG_IMM_W(host_reg, fetchdat & 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat & 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        RELEASE_REG(host_reg);
        
        return op_pc + 2;
}
static uint32_t ropCMP_EAX_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_L(REG_EAX);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        fetchdat = fastreadl(cs + op_pc);
        SUB_HOST_REG_IMM(host_reg, fetchdat);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        RELEASE_REG(host_reg);
        
        return op_pc + 4;
}

static uint32_t ropSUB_AL_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_B(REG_AL);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        SUB_HOST_REG_IMM_B(host_reg, fetchdat & 0xff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat & 0xff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB8);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        STORE_REG_B_RELEASE(host_reg);
        
        return op_pc + 1;
}
static uint32_t ropSUB_AX_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_W(REG_AX);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        SUB_HOST_REG_IMM_W(host_reg, fetchdat & 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat & 0xffff);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        STORE_REG_W_RELEASE(host_reg);
        
        return op_pc + 2;
}
static uint32_t ropSUB_EAX_imm(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg = LOAD_REG_L(REG_EAX);

        STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
        fetchdat = fastreadl(cs + op_pc);
        SUB_HOST_REG_IMM(host_reg, fetchdat);
        STORE_IMM_ADDR_L((uint32_t)&flags_op2, fetchdat);
        STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
        FLAG_C_COPY();
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        STORE_REG_L_RELEASE(host_reg);
        
        return op_pc + 4;
}

static uint32_t rop80(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        uint32_t imm;
        
        if ((fetchdat & 0x30) == 0x10 || (fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_B(fetchdat & 7);
        imm = (fetchdat >> 8) & 0xff;
        
        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                ADD_HOST_REG_IMM_B(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD8);
                FLAG_C_COPY();
                break;
                case 0x08: /*OR*/
                OR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN8);
                FLAG_C_CLEAR();
                break;
                case 0x20: /*AND*/
                AND_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN8);
                FLAG_C_CLEAR();
                break;
                case 0x28: /*SUB*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM_B(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB8);
                FLAG_C_COPY();
                break;
                case 0x30: /*XOR*/
                XOR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN8);
                FLAG_C_CLEAR();
                break;
                case 0x38: /*CMP*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM_B(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB8);
                FLAG_C_COPY();
                break;
        }
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        if ((fetchdat & 0x38) != 0x38)
                STORE_REG_B_RELEASE(host_reg);
        else
                RELEASE_REG(host_reg);
        
        return op_pc + 2;
}

static uint32_t rop81_w(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        uint32_t imm;
        
        if ((fetchdat & 0x30) == 0x10 || (fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_W(fetchdat & 7);
        imm = (fetchdat >> 8) & 0xffff;
        
        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                ADD_HOST_REG_IMM_W(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD16);
                FLAG_C_COPY();
                break;
                case 0x08: /*OR*/
                OR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN16);
                FLAG_C_CLEAR();
                break;
                case 0x20: /*AND*/
                AND_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN16);
                FLAG_C_CLEAR();
                break;
                case 0x28: /*SUB*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM_W(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
                FLAG_C_COPY();
                break;
                case 0x30: /*XOR*/
                XOR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN16);
                FLAG_C_CLEAR();
                break;
                case 0x38: /*CMP*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM_W(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
                FLAG_C_COPY();
                break;
        }
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        if ((fetchdat & 0x38) != 0x38)
                STORE_REG_W_RELEASE(host_reg);
        else
                RELEASE_REG(host_reg);
        
        return op_pc + 3;
}
static uint32_t rop81_l(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        uint32_t imm;
        
        if ((fetchdat & 0x30) == 0x10 || (fetchdat & 0xc0) != 0xc0)
                return 0;
        
        imm = fastreadl(cs + op_pc + 1);
                        
        host_reg = LOAD_REG_L(fetchdat & 7);
        
        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                ADD_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD32);
                FLAG_C_COPY();
                break;
                case 0x08: /*OR*/
                OR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN32);
                FLAG_C_CLEAR();
                break;
                case 0x20: /*AND*/
                AND_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN32);
                FLAG_C_CLEAR();
                break;
                case 0x28: /*SUB*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
                FLAG_C_COPY();
                break;
                case 0x30: /*XOR*/
                XOR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN32);
                FLAG_C_CLEAR();
                break;
                case 0x38: /*CMP*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
                FLAG_C_COPY();
                break;
        }
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        if ((fetchdat & 0x38) != 0x38)
                STORE_REG_L_RELEASE(host_reg);
        else
                RELEASE_REG(host_reg);
        
        return op_pc + 5;
}

static uint32_t rop83_w(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        uint32_t imm;
        
        if ((fetchdat & 0x30) == 0x10 || (fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_W(fetchdat & 7);
        imm = (fetchdat >> 8) & 0xff;
        if (imm & 0x80)
                imm |= 0xff80;
        
        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                ADD_HOST_REG_IMM_W(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD16);
                FLAG_C_COPY();
                break;
                case 0x08: /*OR*/
                OR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN16);
                FLAG_C_CLEAR();
                break;
                case 0x20: /*AND*/
                AND_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN16);
                FLAG_C_CLEAR();
                break;
                case 0x28: /*SUB*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM_W(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
                FLAG_C_COPY();
                break;
                case 0x30: /*XOR*/
                XOR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN16);
                FLAG_C_CLEAR();
                break;
                case 0x38: /*CMP*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM_W(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB16);
                FLAG_C_COPY();
                break;
        }
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        if ((fetchdat & 0x38) != 0x38)
                STORE_REG_W_RELEASE(host_reg);
        else
                RELEASE_REG(host_reg);
        
        return op_pc + 2;
}
static uint32_t rop83_l(uint8_t opcode, uint32_t fetchdat, uint32_t op_32, uint32_t op_pc, codeblock_t *block)
{
        int host_reg;
        uint32_t imm;

        if ((fetchdat & 0x30) == 0x10 || (fetchdat & 0xc0) != 0xc0)
                return 0;
        
        host_reg = LOAD_REG_L(fetchdat & 7);
        imm = (fetchdat >> 8) & 0xff;
        if (imm & 0x80)
                imm |= 0xffffff80;
        
        switch (fetchdat & 0x38)
        {
                case 0x00: /*ADD*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                ADD_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ADD32);
                FLAG_C_COPY();
                break;
                case 0x08: /*OR*/
                OR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN32);
                FLAG_C_CLEAR();
                break;
                case 0x20: /*AND*/
                AND_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN32);
                FLAG_C_CLEAR();
                break;
                case 0x28: /*SUB*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
                FLAG_C_COPY();
                break;
                case 0x30: /*XOR*/
                XOR_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_ZN32);
                FLAG_C_CLEAR();
                break;
                case 0x38: /*CMP*/
                STORE_HOST_REG_ADDR((uint32_t)&flags_op1, host_reg);
                SUB_HOST_REG_IMM(host_reg, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op2, imm);
                STORE_IMM_ADDR_L((uint32_t)&flags_op, FLAGS_SUB32);
                FLAG_C_COPY();
                break;
        }
        
        STORE_HOST_REG_ADDR((uint32_t)&flags_res, host_reg);        
        if ((fetchdat & 0x38) != 0x38)
                STORE_REG_L_RELEASE(host_reg);
        else
                RELEASE_REG(host_reg);
        
        return op_pc + 2;
}
