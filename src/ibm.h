#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "timer.h"

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

uint8_t readmembl(uint32_t addr);
void writemembl(uint32_t addr, uint8_t val);
uint16_t readmemwl(uint32_t addr);
void writememwl(uint32_t addr, uint16_t val);
uint32_t readmemll(uint32_t addr);
void writememll(uint32_t addr, uint32_t val);
uint64_t readmemql(uint32_t addr);
void writememql(uint32_t addr, uint64_t val);

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
extern int mem_size;
extern int readlnum,writelnum;


/*Processor*/
extern int ins, output, timetolive;

extern int cycles_lost;

/*Timer*/
typedef struct PIT_nr
{
        int nr;
        struct PIT *pit;
} PIT_nr;

typedef struct PIT
{
        uint32_t l[3];
	pc_timer_t timer[3];
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


char discfns[2][256];
int driveempty[2];

#define PCJR (romset == ROM_IBMPCJR)

int GAMEBLASTER, GUS, SSI2001, voodoo_enabled;
extern int AMSTRAD, AT, is386, PCI, TANDY, MCA;

enum
{
        ROM_IBMPC = 0,
        ROM_IBMXT,
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
        ROM_TG286M,
        ROM_AWARD286,
        ROM_GDC212M,
        ROM_GW286CT,
        ROM_SPC4200P,
        ROM_SPC4216P,
        ROM_SPC4620P,
        ROM_DELL200,
        ROM_MISC286,
        ROM_IBMAT386,
        ROM_ACER386,
        ROM_KMXC02,
        ROM_MEGAPC,
        ROM_AMA932J,
        ROM_AMI386SX,
        ROM_AMI486,
        ROM_WIN486,
        ROM_PCI486,
        ROM_SIS496,
        ROM_P55VA,   /* Epox P55-VA/430VX/Award/SMC FDC37C932FR*/
        ROM_P55TVP4, /* ASUS P/I-P55TVP4/430VX/Award/Winbond W8387F*/
        ROM_430VX,
        ROM_ENDEAVOR,
        ROM_REVENGE,
        ROM_IBMPS1_2011,
        ROM_DESKPRO_386,
        ROM_IBMPS1_2121,
        ROM_AMI386DX_OPTI495,
        ROM_MR386DX_OPTI495,
        ROM_P55T2P4, /* ASUS P/I-P55T2P4/430HX/Award/Winbond W8387F*/
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
	ROM_TULIP_TC7,
	ROM_ZD_SUPERS,   /* [8088] Zenith Data Systems SupersPort */
	ROM_PB410A,
        ROM_PPC512,
	ROM_BULL_MICRAL_45,
	ROM_FIC_VA503P,
        ROM_CBM_SL386SX25,
        ROM_IBMPS1_2133_451,
        ROM_ECS_386_32,
        ROM_LEDGE_MODELM,
        ROM_HYUNDAI_SUPER286TR,
        ROM_ITAUTEC_INFOWAYM,

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
        GFX_TGKOREANVGA, /*Trigem Korean VGA(Tseng ET4000AX)*/
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
	GFX_SIGMA400,	/*Sigma Designs Color 400 */
	GFX_PGC,	/*Professional Graphics Controller */
	GFX_IM1024,	/*Vermont Microsystems IM1024 */
	GFX_EGAWONDER800, /*ATI EGA Wonder 800+*/
	GFX_MYSTIQUE,   /*Matrox Mystique*/
	GFX_AVGA2,      /*Acumos AVGA2 / Cirrus Logic CL-GD5402*/
        GFX_CL_GD5428,  /*Cirrus Logic CL-GD5428*/
        GFX_IBM_GD5428, /*IBM 1MB SVGA Adapter/A*/

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
extern uint64_t CGACONST;
extern uint64_t MDACONST;
extern uint64_t VGACONST1,VGACONST2;
extern uint64_t RTCCONST;
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
