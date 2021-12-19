#define opFPU(name, optype, a_size, load_var, get, use_var, cycle_postfix)     \
static int opFADD ## name ## _a ## a_size(uint32_t fetchdat)    \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        if ((cpu_state.npxc >> 10) & 3)                                   \
                fesetround(rounding_modes[(cpu_state.npxc >> 10) & 3]);   \
        ST(0) += use_var;                                       \
        if ((cpu_state.npxc >> 10) & 3)                                   \
                fesetround(FE_TONEAREST);                       \
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;             \
        CLOCK_CYCLES(x87_timings.fadd ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFCOM ## name ## _a ## a_size(uint32_t fetchdat)    \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        cpu_state.npxs &= ~(C0|C2|C3);                                    \
        cpu_state.npxs |= x87_compare(ST(0), (double)use_var);            \
        CLOCK_CYCLES(x87_timings.fcom ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFCOMP ## name ## _a ## a_size(uint32_t fetchdat)   \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        cpu_state.npxs &= ~(C0|C2|C3);                                    \
        cpu_state.npxs |= x87_compare(ST(0), (double)use_var);            \
        x87_pop();                                              \
        CLOCK_CYCLES(x87_timings.fcom ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFDIV ## name ## _a ## a_size(uint32_t fetchdat)    \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        x87_div(ST(0), ST(0), use_var);                         \
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;             \
        CLOCK_CYCLES(x87_timings.fdiv ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFDIVR ## name ## _a ## a_size(uint32_t fetchdat)   \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        x87_div(ST(0), use_var, ST(0));                         \
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;             \
        CLOCK_CYCLES(x87_timings.fdiv ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFMUL ## name ## _a ## a_size(uint32_t fetchdat)    \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        ST(0) *= use_var;                                       \
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;             \
        CLOCK_CYCLES(x87_timings.fmul ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFSUB ## name ## _a ## a_size(uint32_t fetchdat)    \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        ST(0) -= use_var;                                       \
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;             \
        CLOCK_CYCLES(x87_timings.fadd ## cycle_postfix);        \
        return 0;                                               \
}                                                               \
static int opFSUBR ## name ## _a ## a_size(uint32_t fetchdat)   \
{                                                               \
        optype t;                                               \
        FP_ENTER();                                             \
        fetch_ea_ ## a_size(fetchdat);                          \
        SEG_CHECK_READ(cpu_state.ea_seg);                       \
        load_var = get(); if (cpu_state.abrt) return 1;                   \
        ST(0) = use_var - ST(0);                                \
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;             \
        CLOCK_CYCLES(x87_timings.fadd ## cycle_postfix);        \
        return 0;                                               \
}


opFPU(s, x87_ts, 16, t.i, geteal, t.s, _32)
opFPU(s, x87_ts, 32, t.i, geteal, t.s, _32)
opFPU(d, x87_td, 16, t.i, geteaq, t.d, _64)
opFPU(d, x87_td, 32, t.i, geteaq, t.d, _64)

opFPU(iw, uint16_t, 16, t, geteaw, (double)(int16_t)t, _i16)
opFPU(iw, uint16_t, 32, t, geteaw, (double)(int16_t)t, _i16)
opFPU(il, uint32_t, 16, t, geteal, (double)(int32_t)t, _i32)
opFPU(il, uint32_t, 32, t, geteal, (double)(int32_t)t, _i32)




static int opFADD(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FADD\n");
        ST(0) = ST(0) + ST(fetchdat & 7);
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFADDr(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FADD\n");
        ST(fetchdat & 7) = ST(fetchdat & 7) + ST(0);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFADDP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FADDP\n");
        ST(fetchdat & 7) = ST(fetchdat & 7) + ST(0);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}

static int opFCOM(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FCOM\n");
        cpu_state.npxs &= ~(C0|C2|C3);
        if (ST(0) == ST(fetchdat & 7))     cpu_state.npxs |= C3;
        else if (ST(0) < ST(fetchdat & 7)) cpu_state.npxs |= C0;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}

static int opFCOMP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FCOMP\n");
        cpu_state.npxs &= ~(C0|C2|C3);
        cpu_state.npxs |= x87_compare(ST(0), ST(fetchdat & 7));
        x87_pop();
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}

static int opFCOMPP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FCOMPP\n");
        cpu_state.npxs &= ~(C0|C2|C3);
        if (*(uint64_t *)&ST(0) == ((uint64_t)1 << 63) && *(uint64_t *)&ST(1) == 0)
                cpu_state.npxs |= C0; /*Nasty hack to fix 80387 detection*/
        else
                cpu_state.npxs |= x87_compare(ST(0), ST(1));

        x87_pop();
        x87_pop();
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFUCOMPP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FUCOMPP\n", easeg, cpu_state.eaaddr);
        cpu_state.npxs &= ~(C0|C2|C3);
        cpu_state.npxs |= x87_ucompare(ST(0), ST(1));
        x87_pop();
        x87_pop();
        CLOCK_CYCLES(x87_timings.fucom);
        return 0;
}

static int opFCOMI(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FICOM\n");
        flags_rebuild();
        cpu_state.flags &= ~(Z_FLAG | P_FLAG | C_FLAG);
        if (ST(0) == ST(fetchdat & 7))     cpu_state.flags |= Z_FLAG;
        else if (ST(0) < ST(fetchdat & 7)) cpu_state.flags |= C_FLAG;
        CLOCK_CYCLES(x87_timings.fcom);
        return 0;
}
static int opFCOMIP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FICOMP\n");
        flags_rebuild();
        cpu_state.flags &= ~(Z_FLAG | P_FLAG | C_FLAG);
        if (ST(0) == ST(fetchdat & 7))     cpu_state.flags |= Z_FLAG;
        else if (ST(0) < ST(fetchdat & 7)) cpu_state.flags |= C_FLAG;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fcom);
        return 0;
}

static int opFDIV(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FDIV\n");
        x87_div(ST(0), ST(0), ST(fetchdat & 7));
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fdiv);
        return 0;
}
static int opFDIVr(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FDIV\n");
        x87_div(ST(fetchdat & 7), ST(fetchdat & 7), ST(0));
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fdiv);
        return 0;
}
static int opFDIVP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FDIVP\n");
        x87_div(ST(fetchdat & 7), ST(fetchdat & 7), ST(0));
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fdiv);
        return 0;
}

static int opFDIVR(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FDIVR\n");
        x87_div(ST(0), ST(fetchdat&7), ST(0));
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fdiv);
        return 0;
}
static int opFDIVRr(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FDIVR\n");
        x87_div(ST(fetchdat & 7), ST(0), ST(fetchdat & 7));
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fdiv);
        return 0;
}
static int opFDIVRP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FDIVR\n");
        x87_div(ST(fetchdat & 7), ST(0), ST(fetchdat & 7));
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fdiv);
        return 0;
}

static int opFMUL(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FMUL\n");
        ST(0) = ST(0) * ST(fetchdat & 7);
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fmul);
        return 0;
}
static int opFMULr(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FMUL\n");
        ST(fetchdat & 7) = ST(0) * ST(fetchdat & 7);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fmul);
        return 0;
}
static int opFMULP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FMULP\n");
        ST(fetchdat & 7) = ST(0) * ST(fetchdat & 7);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fmul);
        return 0;
}

static int opFSUB(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FSUB\n");
        ST(0) = ST(0) - ST(fetchdat & 7);
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFSUBr(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FSUB\n");
        ST(fetchdat & 7) = ST(fetchdat & 7) - ST(0);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFSUBP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FSUBP\n");
        ST(fetchdat & 7) = ST(fetchdat & 7) - ST(0);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}

static int opFSUBR(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FSUBR\n");
        ST(0) = ST(fetchdat & 7) - ST(0);
        cpu_state.tag[cpu_state.TOP&7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFSUBRr(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FSUBR\n");
        ST(fetchdat & 7) = ST(0) - ST(fetchdat & 7);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}
static int opFSUBRP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FSUBRP\n");
        ST(fetchdat & 7) = ST(0) - ST(fetchdat & 7);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = TAG_VALID;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fadd);
        return 0;
}

static int opFUCOM(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FUCOM\n");
        cpu_state.npxs &= ~(C0|C2|C3);
        cpu_state.npxs |= x87_ucompare(ST(0), ST(fetchdat & 7));
        CLOCK_CYCLES(x87_timings.fucom);
        return 0;
}

static int opFUCOMP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FUCOMP\n");
        cpu_state.npxs &= ~(C0|C2|C3);
        cpu_state.npxs |= x87_ucompare(ST(0), ST(fetchdat & 7));
        x87_pop();
        CLOCK_CYCLES(x87_timings.fucom);
        return 0;
}

static int opFUCOMI(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FUCOMI\n");
        flags_rebuild();
        cpu_state.flags &= ~(Z_FLAG | P_FLAG | C_FLAG);
        if (ST(0) == ST(fetchdat & 7))     cpu_state.flags |= Z_FLAG;
        else if (ST(0) < ST(fetchdat & 7)) cpu_state.flags |= C_FLAG;
        CLOCK_CYCLES(x87_timings.fucom);
        return 0;
}
static int opFUCOMIP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) pclog("FUCOMIP\n");
        flags_rebuild();
        cpu_state.flags &= ~(Z_FLAG | P_FLAG | C_FLAG);
        if (ST(0) == ST(fetchdat & 7))     cpu_state.flags |= Z_FLAG;
        else if (ST(0) < ST(fetchdat & 7)) cpu_state.flags |= C_FLAG;
        x87_pop();
        CLOCK_CYCLES(x87_timings.fucom);
        return 0;
}
