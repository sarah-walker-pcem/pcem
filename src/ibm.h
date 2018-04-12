#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef ABS
#undef ABS
#endif

#define ABS(x) ((x) > 0 ? (x) : -(x))

#define printf pclog

#define READFLASH_FDC 0
#define READFLASH_HDC 4
#define readflash_set(offset, drive) readflash |= 1<<((offset)+(drive))
#define readflash_clear(offset, drive) readflash &= ~(1<<((offset)+(drive)))
#define readflash_get(offset, drive) ((readflash&(1<<((offset)+(drive)))) != 0)

/*Memory*/
uint8_t *ram;

uint32_t rammask;

int readlookup[256],readlookupp[256];
uintptr_t *readlookup2;
int readlnext;
int writelookup[256],writelookupp[256];
uintptr_t *writelookup2;
int writelnext;

extern int mmu_perm;

#define readmemb(a) ((readlookup2[(a)>>12]==-1)?readmembl(a):*(uint8_t *)(readlookup2[(a) >> 12] + (a)))
#define readmemw(s,a) ((readlookup2[(uint32_t)((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF || (((s)+(a)) & 1))?readmemwl(s,a):*(uint16_t *)(readlookup2[(uint32_t)((s)+(a))>>12]+(uint32_t)((s)+(a))))
#define readmeml(s,a) ((readlookup2[(uint32_t)((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF || (((s)+(a)) & 3))?readmemll(s,a):*(uint32_t *)(readlookup2[(uint32_t)((s)+(a))>>12]+(uint32_t)((s)+(a))))

//#define writememb(a,v) if (writelookup2[(a)>>12]==0xFFFFFFFF) writemembl(a,v); else ram[writelookup2[(a)>>12]+((a)&0xFFF)]=v
//#define writememw(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF) writememwl(s,a,v); else *((uint16_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v
//#define writememl(s,a,v) if (writelookup2[((s)+(a))>>12]==0xFFFFFFFF || (s)==0xFFFFFFFF) writememll(s,a,v); else *((uint32_t *)(&ram[writelookup2[((s)+(a))>>12]+(((s)+(a))&0xFFF)]))=v
//#define readmemb(a) ((isram[((a)>>16)&255] && !(cr0>>31))?ram[a&0xFFFFFF]:readmembl(a))
//#define writememb(a,v) if (isram[((a)>>16)&255] && !(cr0>>31)) ram[a&0xFFFFFF]=v; else writemembl(a,v)

//void writememb(uint32_t addr, uint8_t val);
uint8_t readmembl(uint32_t addr);
void writemembl(uint32_t addr, uint8_t val);
uint8_t readmemb386l(uint32_t seg, uint32_t addr);
void writememb386l(uint32_t seg, uint32_t addr, uint8_t val);
uint16_t readmemwl(uint32_t seg, uint32_t addr);
void writememwl(uint32_t seg, uint32_t addr, uint16_t val);
uint32_t readmemll(uint32_t seg, uint32_t addr);
void writememll(uint32_t seg, uint32_t addr, uint32_t val);
uint64_t readmemql(uint32_t seg, uint32_t addr);
void writememql(uint32_t seg, uint32_t addr, uint64_t val);

uint8_t *getpccache(uint32_t a);

uint32_t mmutranslatereal(uint32_t addr, int rw);

void addreadlookup(uint32_t virt, uint32_t phys);
void addwritelookup(uint32_t virt, uint32_t phys);


/*IO*/
uint8_t  inb(uint16_t port);
void outb(uint16_t port, uint8_t  val);
uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t val);
uint32_t inl(uint16_t port);
void outl(uint16_t port, uint32_t val);

FILE *romfopen(char *fn, char *mode);
extern int shadowbios,shadowbios_write;
extern int mem_size;
extern int readlnum,writelnum;


/*Processor*/
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
        
        uint16_t old_npxc, new_npxc;
} cpu_state;

#define cycles cpu_state._cycles

extern uint32_t cpu_cur_status;

/*The flags below must match in both cpu_cur_status and block->status for a block
  to be valid*/
#define CPU_STATUS_USE32   (1 << 0)
#define CPU_STATUS_STACK32 (1 << 1)
#define CPU_STATUS_PMODE   (1 << 2)
#define CPU_STATUS_V86     (1 << 3)
#define CPU_STATUS_FLAGS 0xffff

/*If the flags below are set in cpu_cur_status, they must be set in block->status.
  Otherwise they are ignored*/
#define CPU_STATUS_NOTFLATDS  (1 << 16)
#define CPU_STATUS_NOTFLATSS  (1 << 17)
#define CPU_STATUS_MASK 0xffff0000

#define COMPILE_TIME_ASSERT(expr) typedef char COMP_TIME_ASSERT[(expr) ? 1 : 0];

COMPILE_TIME_ASSERT(sizeof(cpu_state) <= 128);

#define cpu_state_offset(MEMBER) ((uintptr_t)&cpu_state.MEMBER - (uintptr_t)&cpu_state - 128)

uint16_t flags,eflags;
uint32_t oldds,oldss,olddslimit,oldsslimit,olddslimitw,oldsslimitw;

extern int ins,output;
extern int cycdiff;

x86seg gdt,ldt,idt,tr;
x86seg _cs,_ds,_es,_ss,_fs,_gs;
x86seg _oldds;

uint32_t pccache;
uint8_t *pccache2;
/*Segments -
  _cs,_ds,_es,_ss are the segment structures
  CS,DS,ES,SS is the 16-bit data
  cs,ds,es,ss are defines to the bases*/
#define CS _cs.seg
#define DS _ds.seg
#define ES _es.seg
#define SS _ss.seg
#define FS _fs.seg
#define GS _gs.seg
#define cs _cs.base
#define ds _ds.base
#define es _es.base
#define ss _ss.base
#define seg_fs _fs.base
#define gs _gs.base

#define CPL ((_cs.access>>5)&3)

void loadseg(uint16_t seg, x86seg *s);
void loadcs(uint16_t seg);

union
{
        uint32_t l;
        uint16_t w;
} CR0;

#define cr0 CR0.l
#define msw CR0.w

uint32_t cr2, cr3, cr4;
uint32_t dr[8];

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

#define IOPL ((flags>>12)&3)

#define IOPLp ((!(msw&1)) || (CPL<=IOPL))
//#define IOPLp 1

//#define IOPLV86 ((!(msw&1)) || (CPL<=IOPL))
extern int cycles_lost;
extern int is486;
extern uint8_t opcode;
extern int insc;
extern int fpucount;
extern float mips,flops;
extern int cgate16;
extern int CPUID;

extern int cpl_override;

/*Timer*/
typedef struct PIT_nr
{
        int nr;
        struct PIT *pit;
} PIT_nr;

typedef struct PIT
{
        uint32_t l[3];
        int c[3];
        uint8_t m[3];
        uint8_t ctrl,ctrls[3];
        int wp,rm[3],wm[3];
        uint16_t rl[3];
        int thit[3];
        int delay[3];
        int rereadlatch[3];
        int gate[3];
        int out[3];
        int running[3];
        int enabled[3];
        int newcount[3];
        int count[3];
        int using_timer[3];
        int initial[3];
        int latched[3];
        int disabled[3];
        
        uint8_t read_status[3];
        int do_read_status[3];
        
        PIT_nr pit_nr[3];
        
        void (*set_out_funcs[3])(int new_out, int old_out);
} PIT;

PIT pit, pit2;
void setpitclock(float clock);

float pit_timer0_freq();

#define cpu_rm  cpu_state.rm_data.rm_mod_reg.rm
#define cpu_mod cpu_state.rm_data.rm_mod_reg.mod
#define cpu_reg cpu_state.rm_data.rm_mod_reg.reg



/*DMA*/
typedef struct dma_t
{
        uint32_t ab, ac;
        uint16_t cb;
        int cc;
        int wp;
        uint8_t m, mode;
        uint8_t page;
        uint8_t stat, stat_rq;
        uint8_t command;
        int size;
        
        uint8_t ps2_mode;
        uint8_t arb_level;
        uint16_t io_addr;
} dma_t;

dma_t dma[8];

/*PPI*/
typedef struct PPI
{
        int s2;
        uint8_t pa,pb;
} PPI;

PPI ppi;


/*PIC*/
typedef struct PIC
{
        uint8_t icw1,icw4,mask,ins,pend,mask2;
        int icw;
        uint8_t vector;
        int read;
        uint8_t level_sensitive;
} PIC;

PIC pic,pic2;
extern int pic_intpending;


int disctime;
char discfns[2][256];
int driveempty[2];

#define PCJR (romset == ROM_IBMPCJR)

int GAMEBLASTER, GUS, SSI2001, voodoo_enabled;
extern int AMSTRAD, AT, is386, PCI, TANDY;

enum
{
        ROM_IBMPC = 0,  /*301 keyboard error, 131 cassette (!!!) error*/
        ROM_IBMXT,      /*301 keyboard error*/
        ROM_IBMPCJR,
        ROM_GENXT,      /*'Generic XT BIOS'*/
        ROM_DTKXT,
        ROM_EUROPC,
        ROM_OLIM24,
        ROM_TANDY,
        ROM_PC1512,
        ROM_PC200,
        ROM_PC1640,
        ROM_PC2086,
        ROM_PC3086,        
        ROM_AMIXT,      /*XT Clone with AMI BIOS*/
	ROM_LTXT,
	ROM_LXT3,
	ROM_PX386,
        ROM_DTK386,
        ROM_PXXT,
        ROM_JUKOPC,
        ROM_TANDY1000HX,
        ROM_TANDY1000SL2,
        ROM_IBMAT,
        ROM_CMDPC30,
        ROM_AMI286,
        ROM_AWARD286,
        ROM_GW286CT,
        ROM_SPC4200P,
        ROM_SPC4216P,
        ROM_DELL200,
        ROM_MISC286,
        ROM_IBMAT386,
        ROM_ACER386,
        ROM_KMXC02,
        ROM_MEGAPC,
        ROM_AMI386SX,
        ROM_AMI486,
        ROM_WIN486,
        ROM_PCI486,
        ROM_SIS496,
        ROM_430VX,
        ROM_ENDEAVOR,
        ROM_REVENGE,
        ROM_IBMPS1_2011,
        ROM_DESKPRO_386,
        ROM_IBMPS1_2121,
        ROM_AMI386DX_OPTI495,
        ROM_MR386DX_OPTI495,
	ROM_IBMPS2_M30_286,
	ROM_IBMPS2_M50,
	ROM_IBMPS2_M55SX,
	ROM_IBMPS2_M80,
        ROM_ATARIPC3,
        ROM_IBMXT286,
        ROM_EPSON_PCAX,
        ROM_EPSON_PCAX2E,
        ROM_EPSON_PCAX3,
        ROM_T3100E,
	ROM_T1000,
	ROM_T1200,
	ROM_PB_L300SX,
	ROM_NCR_PC4I,
	ROM_TO16_PC,
	ROM_COMPAQ_PII,
	ROM_ELX_PC425X,
	ROM_PB570,
	ROM_ZAPPA,
	ROM_PB520R,
	ROM_COMPAQ_PIP,
	ROM_XI8088,
	ROM_IBMPS2_M70_TYPE3,
	ROM_IBMPS2_M70_TYPE4,
                	
        ROM_MAX
};

extern int romspresent[ROM_MAX];

int hasfpu;
int romset;

enum
{
        GFX_BUILTIN = -1,
        GFX_CGA = 0,
        GFX_MDA,
        GFX_HERCULES,
        GFX_EGA,        /*Using IBM EGA BIOS*/
        GFX_TVGA,       /*Using Trident TVGA8900D BIOS*/
        GFX_ET4000,     /*Tseng ET4000*/
        GFX_ET4000W32,  /*Tseng ET4000/W32p (Diamond Stealth 32)*/
        GFX_BAHAMAS64,  /*S3 Vision864 (Paradise Bahamas 64)*/
        GFX_N9_9FX,     /*S3 764/Trio64 (Number Nine 9FX)*/
        GFX_VIRGE,      /*S3 Virge*/
        GFX_TGUI9440,   /*Trident TGUI9440*/
        GFX_VGA,        /*IBM VGA*/        
        GFX_VGAEDGE16,  /*ATI VGA Edge-16 (18800-1)*/
        GFX_ATIKOREANVGA, /*ATI Korean VGA (28800-5)*/
        GFX_VGACHARGER, /*ATI VGA Charger (28800-5)*/
        GFX_OTI067,     /*Oak OTI-067*/
        GFX_MACH64GX,   /*ATI Graphics Pro Turbo (Mach64)*/
        GFX_CL_GD5429,  /*Cirrus Logic CL-GD5429*/
        GFX_VIRGEDX,    /*S3 Virge/DX*/
        GFX_PHOENIX_TRIO32, /*S3 732/Trio32 (Phoenix)*/
        GFX_PHOENIX_TRIO64, /*S3 764/Trio64 (Phoenix)*/
       	GFX_INCOLOR,	/* Hercules InColor */ 
	GFX_COLORPLUS,	/* Plantronics ColorPlus */
	GFX_WY700,	/* Wyse 700 */
	GFX_GENIUS,	/* MDSI Genius */
        GFX_MACH64VT2,  /*ATI Mach64 VT2*/
        GFX_OLIVETTI_GO481, /*Olivetti GO481 PVGA1A*/
        GFX_TGUI9400CXI, /*Trident TGUI9440CXi*/
        GFX_CL_GD5430,  /*Cirrus Logic CL-GD5430*/
        GFX_CL_GD5434,  /*Cirrus Logic CL-GD5434*/
        GFX_OTI037,     /*Oak OTI-037*/
        GFX_COMPAQ_CGA,	/*Compaq CGA*/
        GFX_MAX
};

extern int gfx_present[GFX_MAX];

int gfxcard;

int cpuspeed;


/*Video*/
int readflash;
extern int egareads,egawrites;
extern int vid_resize;
extern int vid_api;
extern int winsizex,winsizey;

extern int changeframecount;


/*Sound*/
int ppispeakon;
float CGACONST;
float MDACONST;
float VGACONST1,VGACONST2;
float RTCCONST;
int gated,speakval,speakon;


/*Sound Blaster*/
#define SADLIB    1     /*No DSP*/
#define SB1       2     /*DSP v1.05*/
#define SB15      3     /*DSP v2.00*/
#define SB2       4     /*DSP v2.01 - needed for high-speed DMA*/
#define SBPRO     5     /*DSP v3.00*/
#define SBPRO2    6     /*DSP v3.02 + OPL3*/
#define SB16      7     /*DSP v4.05 + OPL3*/
#define SADGOLD   8     /*AdLib Gold*/
#define SND_WSS   9     /*Windows Sound System*/
#define SND_PAS16 10    /*Pro Audio Spectrum 16*/


/*Hard disc*/

typedef struct
{
        FILE *f;
        int spt,hpc; /*Sectors per track, heads per cylinder*/
        int tracks;
} PcemHDC;

PcemHDC hdc[7];

/*Keyboard*/
int keybsenddelay;


/*CD-ROM*/
extern int cdrom_drive;
extern int old_cdrom_drive;
extern int idecallback[2];

#define CD_STATUS_EMPTY		0
#define CD_STATUS_DATA_ONLY	1
#define CD_STATUS_PLAYING	2
#define CD_STATUS_PAUSED	3
#define CD_STATUS_STOPPED	4

extern uint32_t atapi_get_cd_volume(int channel);

void pclog(const char *format, ...);
void fatal(const char *format, ...);
void warning(const char *format, ...);
extern int nmi;
extern int nmi_auto_clear;


extern float isa_timing, bus_timing;


uint64_t timer_read();
extern uint64_t timer_freq;


void loadconfig(char *fn);
extern int config_override;

extern int infocus;

void onesec();

void resetpc_cad();

extern int start_in_fullscreen;
extern int window_w, window_h, window_x, window_y, window_remember;

void startblit();
void endblit();

void set_window_title(const char *s);

void updatewindowsize(int x, int y);

void initpc(int argc, char *argv[]);
void runpc();
void closepc();
void resetpc();
void resetpchard();
void speedchanged();

void saveconfig(char *fn);
void saveconfig_global_only();

#define UNUSED(x) (void)x

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void ide_padstr(char *str, const char *src, int len);
