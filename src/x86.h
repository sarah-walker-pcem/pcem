extern uint32_t rmdat;
#define fetchdat rmdat

extern uint32_t easeg;

extern int oldcpl;

extern int nmi_enable;

extern int trap;

extern int output;
extern int timetolive;

extern uint8_t znptable8[256];

extern uint32_t use32;
extern int stack32;

extern uint16_t *mod1add[2][8];
extern uint32_t *mod1seg[8];

extern int cgate32;

extern uint32_t *eal_r, *eal_w;

extern int x86_was_reset;



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
void loadcscall(uint16_t seg, uint32_t old_pc);
void loadcsjmp(uint16_t seg, uint32_t old_pc);
void pmoderetf(int is32, uint16_t off);
void pmodeiret(int is32);

void x86_int(int num);
void x86_int_sw(int num);
int x86_int_sw_rm(int num);

int divl(uint32_t val);
int idivl(int32_t val);
