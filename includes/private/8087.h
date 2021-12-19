/*This file is a series of macros to get the 386+ based x87 emulation working with
  the 808x emulation*/
#define X8087
#define OP_TABLE(name) ops_808x_ ## name

#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#define fetch_ea_16(fetchdat)
#define fetch_ea_32(fetchdat)
#define SEG_CHECK_READ(seg)
#define SEG_CHECK_WRITE(seg)
#define CHECK_WRITE(seg, addr_lo, addr_hi)
#define PREFETCH_RUN(timing, bytes, rmdat, a, b, c, d, e)

static uint32_t readmeml(uint32_t seg, uint32_t addr)
{
        return readmemw(seg, addr) | (readmemw(seg, addr+2) << 16);
}
static uint64_t readmemq(uint32_t seg, uint32_t addr)
{
        return (uint64_t)readmemw(seg, addr) | ((uint64_t)readmemw(seg, addr+2) << 16) |
                        ((uint64_t)readmemw(seg, addr+4) << 32) |
                        ((uint64_t)readmemw(seg, addr+6) << 48);
}

static void writememb_8087(uint32_t seg, uint32_t addr, uint8_t val)
{
        writememb(seg+addr, val);
}
static void writememl(uint32_t seg, uint32_t addr, uint32_t val)
{
        writememw(seg, addr, val & 0xffff);
        writememw(seg, addr+2, (val >> 16) & 0xffff);
}
static void writememq(uint32_t seg, uint32_t addr, uint64_t val)
{
        writememw(seg, addr, val & 0xffff);
        writememw(seg, addr+2, (val >> 16) & 0xffff);
        writememw(seg, addr+4, (val >> 32) & 0xffff);
        writememw(seg, addr+6, (val >> 48) & 0xffff);
}

static inline uint32_t geteal()
{
        if (cpu_mod == 3)
                fatal("geteal cpu_mod==3\n");
        return readmeml(easeg, cpu_state.eaaddr);
}
static inline uint64_t geteaq()
{
        if (cpu_mod == 3)
                fatal("geteaq cpu_mod==3\n");
        return readmemq(easeg, cpu_state.eaaddr);
}
static inline void seteal(uint32_t val)
{
        if (cpu_mod == 3)
                fatal("seteal cpu_mod==3\n");
        else
                writememl(easeg, cpu_state.eaaddr, val);
}
static inline void seteaq(uint64_t val)
{
        if (cpu_mod == 3)
                fatal("seteaq cpu_mod==3\n");
        else
                writememq(easeg, cpu_state.eaaddr, val);
}

#define flags_rebuild()

#define CF_SET() (cpu_state.flags & C_FLAG)
#define NF_SET() (cpu_state.flags & N_FLAG)
#define PF_SET() (cpu_state.flags & P_FLAG)
#define VF_SET() (cpu_state.flags & V_FLAG)
#define ZF_SET() (cpu_state.flags & Z_FLAG)

#define cond_O   ( VF_SET())
#define cond_NO  (!VF_SET())
#define cond_B   ( CF_SET())
#define cond_NB  (!CF_SET())
#define cond_E   ( ZF_SET())
#define cond_NE  (!ZF_SET())
#define cond_BE  ( CF_SET() ||  ZF_SET())
#define cond_NBE (!CF_SET() && !ZF_SET())
#define cond_S   ( NF_SET())
#define cond_NS  (!NF_SET())
#define cond_P   ( PF_SET())
#define cond_NP  (!PF_SET())
#define cond_L   (((NF_SET()) ? 1 : 0) != ((VF_SET()) ? 1 : 0))
#define cond_NL  (((NF_SET()) ? 1 : 0) == ((VF_SET()) ? 1 : 0))
#define cond_LE  (((NF_SET()) ? 1 : 0) != ((VF_SET()) ? 1 : 0) ||  (ZF_SET()))
#define cond_NLE (((NF_SET()) ? 1 : 0) == ((VF_SET()) ? 1 : 0) && (!ZF_SET()))

#define writememb writememb_8087
#include "x87_ops.h"
#undef writememb
