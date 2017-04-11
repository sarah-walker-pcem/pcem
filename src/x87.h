uint32_t x87_pc_off,x87_op_off;
uint16_t x87_pc_seg,x87_op_seg;

static inline void x87_set_mmx()
{
        cpu_state.TOP = 0;
        *(uint64_t *)cpu_state.tag = 0;
        cpu_state.ismmx = 1;
}

static inline void x87_emms()
{
        *(uint64_t *)cpu_state.tag = 0x0303030303030303ll;
        cpu_state.ismmx = 0;
}

uint16_t x87_gettag();
void x87_settag(uint16_t new_tag);
void x87_dumpregs();
void x87_reset();

/*Hack for FPU copy. If set then MM[].q contains the 64-bit integer loaded by FILD*/
#define TAG_UINT64 (1 << 2)
