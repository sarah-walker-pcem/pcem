#ifndef _X86_H_
#define _X86_H_

#define EAX cpu_state.regs[0].l
#define ECX cpu_state.regs[1].l
#define EDX cpu_state.regs[2].l
#define EBX cpu_state.regs[3].l
#define ESP cpu_state.regs[4].l
#define EBP cpu_state.regs[5].l
#define ESI cpu_state.regs[6].l
#define EDI cpu_state.regs[7].l
#define AX cpu_state.regs[0].w
#define CX cpu_state.regs[1].w
#define DX cpu_state.regs[2].w
#define BX cpu_state.regs[3].w
#define SP cpu_state.regs[4].w
#define BP cpu_state.regs[5].w
#define SI cpu_state.regs[6].w
#define DI cpu_state.regs[7].w
#define AL cpu_state.regs[0].b.l
#define AH cpu_state.regs[0].b.h
#define CL cpu_state.regs[1].b.l
#define CH cpu_state.regs[1].b.h
#define DL cpu_state.regs[2].b.l
#define DH cpu_state.regs[2].b.h
#define BL cpu_state.regs[3].b.l
#define BH cpu_state.regs[3].b.h

typedef union
{
        uint32_t l;
        uint16_t w;
        struct
        {
                uint8_t l,h;
        } b;
} x86reg;

typedef struct
{
        uint32_t base;
        uint32_t limit;
        uint8_t access;
        uint16_t seg;
        uint32_t limit_low, limit_high;
        int checked; /*Non-zero if selector is known to be valid*/
} x86seg;

typedef union MMX_REG
{
        uint64_t q;
        int64_t  sq;
        uint32_t l[2];
        int32_t  sl[2];
        uint16_t w[4];
        int16_t  sw[4];
        uint8_t  b[8];
        int8_t   sb[8];
        float    f[2];
} MMX_REG;

struct
{
        x86reg regs[8];

        uint8_t tag[8];

        x86seg *ea_seg;
        uint32_t eaaddr;

        int flags_op;
        uint32_t flags_res;
        uint32_t flags_op1, flags_op2;

        uint32_t pc;
        uint32_t oldpc;
        uint32_t op32;

        int TOP;

        union
        {
                struct
                {
                        int8_t rm, mod, reg;
                } rm_mod_reg;
                uint32_t rm_mod_reg_data;
        } rm_data;

        int8_t ssegs;
        int8_t ismmx;
        int8_t abrt;

        int _cycles;
        int cpu_recomp_ins;

        uint16_t npxs, npxc;

        double ST[8];

        uint16_t MM_w4[8];

        MMX_REG MM[8];

        uint32_t old_fp_control, new_fp_control;
#if defined i386 || defined __i386 || defined __i386__ || defined _X86_
        uint16_t old_fp_control2, new_fp_control2;
#endif
#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined __amd64__
        uint32_t trunc_fp_control;
#endif
        x86seg seg_cs,seg_ds,seg_es,seg_ss,seg_fs,seg_gs;

        union
        {
                uint32_t l;
                uint16_t w;
        } CR0;

        uint16_t flags, eflags;
} cpu_state;

#define cpu_state_offset(MEMBER) ((uintptr_t)&cpu_state.MEMBER - (uintptr_t)&cpu_state - 128)

#define cycles cpu_state._cycles

#define cr0 cpu_state.CR0.l
#define msw cpu_state.CR0.w

/*Segments -
  _cs,_ds,_es,_ss are the segment structures
  CS,DS,ES,SS is the 16-bit data
  cs,ds,es,ss are defines to the bases*/
#define CS cpu_state.seg_cs.seg
#define DS cpu_state.seg_ds.seg
#define ES cpu_state.seg_es.seg
#define SS cpu_state.seg_ss.seg
#define FS cpu_state.seg_fs.seg
#define GS cpu_state.seg_gs.seg
#define cs cpu_state.seg_cs.base
#define ds cpu_state.seg_ds.base
#define es cpu_state.seg_es.base
#define ss cpu_state.seg_ss.base
//#define seg_fs _fs.base
#define gs cpu_state.seg_gs.base

#define CPL ((cpu_state.seg_cs.access>>5)&3)

#define C_FLAG  0x0001
#define P_FLAG  0x0004
#define A_FLAG  0x0010
#define Z_FLAG  0x0040
#define N_FLAG  0x0080
#define T_FLAG  0x0100
#define I_FLAG  0x0200
#define D_FLAG  0x0400
#define V_FLAG  0x0800
#define NT_FLAG 0x4000
#define VM_FLAG 0x0002 /*In EFLAGS*/
#define VIF_FLAG 0x0008 /*In EFLAGS*/
#define VIP_FLAG 0x0010 /*In EFLAGS*/

#define WP_FLAG 0x10000 /*In CR0*/

#define CR4_VME (1 << 0)
#define CR4_PVI (1 << 1)
#define CR4_PSE (1 << 4)

#define IOPL ((cpu_state.flags >> 12) & 3)

#define IOPLp ((!(msw&1)) || (CPL<=IOPL))

extern x86seg gdt, ldt, idt, tr;

extern uint32_t cr2, cr3, cr4;
extern uint32_t dr[8];


extern uint16_t cpu_cur_status;

/*The flags below must match in both cpu_cur_status and block->status for a block
  to be valid*/
#define CPU_STATUS_USE32   (1 << 0)
#define CPU_STATUS_STACK32 (1 << 1)
#define CPU_STATUS_PMODE   (1 << 2)
#define CPU_STATUS_V86     (1 << 3)
#define CPU_STATUS_FLAGS 0xff

/*If the flags below are set in cpu_cur_status, they must be set in block->status.
  Otherwise they are ignored*/
#define CPU_STATUS_NOTFLATDS  (1 << 8)
#define CPU_STATUS_NOTFLATSS  (1 << 9)
#define CPU_STATUS_MASK 0xff00


extern uint32_t rmdat;
#define fetchdat rmdat

extern uint32_t easeg;
extern uint32_t *eal_r, *eal_w;

extern int oldcpl;
extern uint32_t oldss;

extern int nmi_enable;

extern int trap;

extern uint32_t use32;
extern int stack32;

extern uint8_t znptable8[256];
extern uint16_t *mod1add[2][8];
extern uint32_t *mod1seg[8];

extern int cgate16, cgate32;
extern int cpl_override;

extern int x86_was_reset;

extern int insc;
extern int fpucount;
extern float mips,flops;

#define setznp168 setznp16

#define getr8(r)   ((r&4)?cpu_state.regs[r&3].b.h:cpu_state.regs[r&3].b.l)
#define getr16(r)  cpu_state.regs[r].w
#define getr32(r)  cpu_state.regs[r].l

#define setr8(r,v) if (r&4) cpu_state.regs[r&3].b.h=v; \
                   else     cpu_state.regs[r&3].b.l=v;
#define setr16(r,v) cpu_state.regs[r].w=v
#define setr32(r,v) cpu_state.regs[r].l=v

#define fetchea()   { rmdat=readmemb(cs+pc); pc++;  \
                    reg=(rmdat>>3)&7;               \
                    mod=(rmdat>>6)&3;               \
                    rm=rmdat&7;                   \
                    if (mod!=3) fetcheal(); }


extern int optype;
#define JMP 1
#define CALL 2
#define IRET 3
#define OPTYPE_INT 4


enum
{
        ABRT_NONE = 0,
        ABRT_GEN,
        ABRT_TS  = 0xA,
        ABRT_NP  = 0xB,
        ABRT_SS  = 0xC,
        ABRT_GPF = 0xD,
        ABRT_PF  = 0xE
};

extern uint32_t abrt_error;

void x86_doabrt(int x86_abrt);

extern int codegen_flat_ds;
extern int codegen_flat_ss;

extern uint32_t pccache;
extern uint8_t *pccache2;

void x86illegal();

void x86seg_reset();
void x86gpf(char *s, uint16_t error);

void resetx86();
void softresetx86();
void refreshread();
void dumpregs();

void execx86(int cycs);
void exec386(int cycs);
void exec386_dynarec(int cycs);

void pmodeint(int num, int soft);
int loadseg(uint16_t seg, x86seg *s);
void loadcs(uint16_t seg);
void loadcscall(uint16_t seg, uint32_t old_pc);
void loadcsjmp(uint16_t seg, uint32_t old_pc);
void pmoderetf(int is32, uint16_t off);
void pmodeiret(int is32);

void x86_int(int num);
void x86_int_sw(int num);
int x86_int_sw_rm(int num);

int divl(uint32_t val);
int idivl(int32_t val);

#endif
