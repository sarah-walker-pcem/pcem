/*Matrox Mystique emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "pci.h"
#include "rom.h"
#include "thread.h"
#include "video.h"
#include "vid_mga.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_tvp3026_ramdac.h"

#define FIFO_SIZE 65536
#define FIFO_MASK (FIFO_SIZE - 1)
#define FIFO_ENTRY_SIZE (1 << 31)
#define FIFO_THRESHOLD 0xe000

#define WAKE_DELAY (100 * TIMER_USEC) /*100us*/

#define FIFO_ENTRIES (mystique->fifo_write_idx - mystique->fifo_read_idx)
#define FIFO_FULL    ((mystique->fifo_write_idx - mystique->fifo_read_idx) >= (FIFO_SIZE-1))
#define FIFO_EMPTY   (mystique->fifo_read_idx == mystique->fifo_write_idx)

#define FIFO_TYPE 0xff000000
#define FIFO_ADDR 0x00ffffff

enum
{
        FIFO_INVALID = (0x00 << 24),
        FIFO_WRITE_CTRL_BYTE = (0x01 << 24),
        FIFO_WRITE_CTRL_LONG = (0x02 << 24),
        FIFO_WRITE_ILOAD_LONG = (0x03 << 24)
};

typedef struct
{
        uint32_t addr_type;
        uint32_t val;
} fifo_entry_t;

enum
{
        MGA_2064W,  /*Millennium*/
        MGA_1064SG  /*Mystique*/
};

typedef struct mystique_t
{
        svga_t svga;

        rom_t bios_rom;

        int type;

        tvp3026_ramdac_t tvp3026; /*Millennium*/

        mem_mapping_t lfb_mapping;
        mem_mapping_t ctrl_mapping;
        mem_mapping_t iload_mapping;

        uint8_t pci_regs[256];
        uint8_t int_line;
        int card;

        int vram_size;
        uint32_t vram_mask, vram_mask_w, vram_mask_l;

        uint32_t lfb_base, ctrl_base, iload_base;

        uint8_t crtcext_regs[6];
        int crtcext_idx;

        uint8_t xreg_regs[256];
        int xreg_idx;

        uint32_t ma_latch_old;

        uint32_t maccess, mctlwtst;
        uint32_t maccess_running;

        uint32_t status;

        uint8_t xcurctrl;

        uint8_t xsyspllm, xsysplln, xsyspllp;
        struct
        {
                int m, n, p, s;
        } xpixpll[3];

        uint8_t xgenioctrl;
        uint8_t xgeniodata;
        uint8_t xmulctrl;
        uint8_t xgenctrl;
        uint8_t xmiscctrl;
        uint8_t xpixclkctrl;
        uint8_t xvrefctrl;
        uint8_t xcolkeymskl, xcolkeymskh;
        uint8_t xcolkeyl, xcolkeyh;
        int xzoomctrl;
        uint8_t xcrcbitsel;

        uint8_t ien;

        struct
        {
                uint8_t funcnt, stylelen;
                int xoff, yoff;

                uint16_t cxleft, cxright;
                int16_t fxleft, fxright;
                uint16_t length;

                uint32_t dwgctrl;
                uint32_t dwgctrl_running;
                uint32_t bcol, fcol;
                uint32_t pitch;
                uint32_t plnwt;
                uint32_t ybot;
                uint32_t ydstorg;
                uint32_t ytop;
                int selline;

                uint32_t src[4];
                uint32_t ar[7];
                uint32_t dr[16];

                int pattern[8][8];

                struct
                {
                        int sdydxl;
                        int scanleft;
                        int sdxl;
                        int sdy;
                        int sdxr;
                } sgn;

                uint32_t tmr[9];
                uint32_t texorg, texwidth, texheight;
                uint32_t texctl, textrans;
                uint32_t zorg;

                int ydst;
                uint32_t ydst_lin;

                int length_cur;
                int16_t xdst;
                uint32_t src_addr;
                uint32_t z_base;

                int iload_rem_count;
                uint32_t iload_rem_data;
                int idump_end_of_line;

                int words;

                int ta_key, ta_mask;

                int lastpix_r, lastpix_g, lastpix_b;

                int highv_line;
                uint32_t highv_data;

                int beta;

                int dither;

                uint8_t dmamod;
        } dwgreg;

        struct
        {
                uint8_t r, g, b;
        } lut[256];

        struct
        {
                uint16_t pos_x, pos_y;
                uint16_t addr;
                uint32_t col[3];
        } cursor;

        uint8_t dmamod, dmadatasiz, dirdatasiz;
        struct
        {
                uint32_t primaddress, primend;
                uint32_t secaddress, secend;

                int pri_pos, sec_pos, iload_pos;
                uint32_t pri_header, sec_header, iload_header;

                int pri_state, sec_state, iload_state;

                int state;

                mutex_t* lock;
//                pc_timer_t timer;
        } dma;

        uint8_t dmamap[16];

        volatile int busy;
        volatile int blitter_submit_refcount;
        volatile int blitter_submit_dma_refcount;
        volatile int blitter_complete_refcount;

        volatile int endprdmasts_pending;
        volatile int softrap_pending;
        uint32_t softrap_pending_val;

        pc_timer_t softrap_pending_timer;

        uint64_t blitter_time;
        uint64_t status_time;

        int pixel_count, trap_count;

        fifo_entry_t fifo[FIFO_SIZE];
        volatile int fifo_read_idx, fifo_write_idx;

        thread_t* fifo_thread;
        event_t* wake_fifo_thread;
        event_t* fifo_not_full_event;

        pc_timer_t wake_timer;
} mystique_t;

static void mystique_start_blit(mystique_t* mystique);
static void mystique_update_irqs(mystique_t* mystique);

static void wake_fifo_thread(mystique_t* mystique);
static void wait_fifo_idle(mystique_t* mystique);
static void mystique_queue(mystique_t* mystique, uint32_t addr, uint32_t val, uint32_t type);

static uint8_t mystique_readb_linear(uint32_t addr, void* p);
static uint16_t mystique_readw_linear(uint32_t addr, void* p);
static uint32_t mystique_readl_linear(uint32_t addr, void* p);
static void mystique_writeb_linear(uint32_t addr, uint8_t val, void* p);
static void mystique_writew_linear(uint32_t addr, uint16_t val, void* p);
static void mystique_writel_linear(uint32_t addr, uint32_t val, void* p);

static const uint8_t trans_masks[16][16] =
        {
                {
                        1, 1, 1, 1,
                        1, 1, 1, 1,
                        1, 1, 1, 1,
                        1, 1, 1, 1
                },
                {
                        1, 0, 1, 0,
                        0, 1, 0, 1,
                        1, 0, 1, 0,
                        0, 1, 0, 1
                },
                {
                        0, 1, 0, 1,
                        1, 0, 1, 0,
                        0, 1, 0, 1,
                        1, 0, 1, 0
                },
                {
                        1, 0, 1, 0,
                        0, 0, 0, 0,
                        1, 0, 1, 0,
                        0, 0, 0, 0
                },
                {
                        0, 1, 0, 1,
                        0, 0, 0, 0,
                        0, 1, 0, 1,
                        0, 0, 0, 0
                },
                {
                        0, 0, 0, 0,
                        1, 0, 1, 0,
                        0, 0, 0, 0,
                        1, 0, 1, 0
                },
                {
                        0, 0, 0, 0,
                        0, 1, 0, 1,
                        0, 0, 0, 0,
                        0, 1, 0, 1
                },
                {
                        1, 0, 0, 0,
                        0, 0, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 0
                },
                {
                        0, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 0, 0,
                        0, 0, 0, 1
                },
                {
                        0, 0, 0, 1,
                        0, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 0, 0
                },
                {
                        0, 0, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 0,
                        1, 0, 0, 0
                },
                {
                        0, 0, 0, 0,
                        1, 0, 0, 0,
                        0, 0, 0, 0,
                        0, 0, 1, 0
                },
                {
                        0, 1, 0, 0,
                        0, 0, 0, 0,
                        0, 0, 0, 1,
                        0, 0, 0, 0
                },
                {
                        0, 0, 0, 0,
                        0, 0, 0, 1,
                        0, 0, 0, 0,
                        0, 1, 0, 0
                },
                {
                        0, 0, 1, 0,
                        0, 0, 0, 0,
                        1, 0, 0, 0,
                        0, 0, 0, 0
                },
                {
                        0, 0, 0, 0,
                        0, 0, 0, 0,
                        0, 0, 0, 0,
                        0, 0, 0, 0
                }
        };

static int8_t dither5[256][2][2];
static int8_t dither6[256][2][2];

enum
{
        DMA_STATE_IDLE = 0,
        DMA_STATE_PRI,
        DMA_STATE_SEC
};

#define DMA_POLL_TIME_US 100 /*100us*/
#define DMA_MAX_WORDS 256 /*256 quad words per 100us poll*/

/*These registers are also mirrored into 0x1dxx, with the mirrored versions starting
  the blitter*/
#define REG_DWGCTL       0x1c00
#define REG_MACCESS      0x1c04
#define REG_MCTLWTST     0x1c08
#define REG_ZORG         0x1c0c
#define REG_PAT0         0x1c10
#define REG_PAT1         0x1c14
#define REG_PLNWT        0x1c1c
#define REG_BCOL         0x1c20
#define REG_FCOL         0x1c24
#define REG_SRC0         0x1c30
#define REG_SRC1         0x1c34
#define REG_SRC2         0x1c38
#define REG_SRC3         0x1c3c
#define REG_XYSTRT       0x1c40
#define REG_XYEND        0x1c44
#define REG_SHIFT        0x1c50
#define REG_DMAPAD       0x1c54
#define REG_SGN          0x1c58
#define REG_LEN          0x1c5c
#define REG_AR0          0x1c60
#define REG_AR1          0x1c64
#define REG_AR2          0x1c68
#define REG_AR3          0x1c6c
#define REG_AR4          0x1c70
#define REG_AR5          0x1c74
#define REG_AR6          0x1c78
#define REG_CXBNDRY      0x1c80
#define REG_FXBNDRY      0x1c84
#define REG_YDSTLEN      0x1c88
#define REG_PITCH        0x1c8c
#define REG_YDST         0x1c90
#define REG_YDSTORG      0x1c94
#define REG_YTOP         0x1c98
#define REG_YBOT         0x1c9c
#define REG_CXLEFT       0x1ca0
#define REG_CXRIGHT      0x1ca4
#define REG_FXLEFT       0x1ca8
#define REG_FXRIGHT      0x1cac
#define REG_XDST         0x1cb0
#define REG_DR0          0x1cc0
#define REG_DR2          0x1cc8
#define REG_DR3          0x1ccc
#define REG_DR4          0x1cd0
#define REG_DR6          0x1cd8
#define REG_DR7          0x1cdc
#define REG_DR8          0x1ce0
#define REG_DR10         0x1ce8
#define REG_DR11         0x1cec
#define REG_DR12         0x1cf0
#define REG_DR14         0x1cf8
#define REG_DR15         0x1cfc

#define REG_FIFOSTATUS   0x1e10
#define REG_STATUS       0x1e14
#define REG_ICLEAR       0x1e18
#define REG_IEN          0x1e1c
#define REG_VCOUNT       0x1e20
#define REG_DMAMAP       0x1e30
#define REG_RST          0x1e40
#define REG_OPMODE       0x1e54
#define REG_PRIMADDRESS  0x1e58
#define REG_PRIMEND      0x1e5c
#define REG_DWG_INDIR_WT 0x1e80

#define REG_ATTR_IDX     0x1fc0
#define REG_ATTR_DATA    0x1fc1
#define REG_INSTS0       0x1fc2
#define REG_MISC         0x1fc2
#define REG_SEQ_IDX      0x1fc4
#define REG_SEQ_DATA     0x1fc5
#define REG_MISCREAD     0x1fcc
#define REG_GCTL_IDX     0x1fce
#define REG_GCTL_DATA    0x1fcf
#define REG_CRTC_IDX     0x1fd4
#define REG_CRTC_DATA    0x1fd5
#define REG_INSTS1       0x1fda
#define REG_CRTCEXT_IDX  0x1fde
#define REG_CRTCEXT_DATA 0x1fdf
#define REG_CACHEFLUSH   0x1fff

/*Mystique only*/
#define REG_TMR0         0x2c00
#define REG_TMR1         0x2c04
#define REG_TMR2         0x2c08
#define REG_TMR3         0x2c0c
#define REG_TMR4         0x2c10
#define REG_TMR5         0x2c14
#define REG_TMR6         0x2c18
#define REG_TMR7         0x2c1c
#define REG_TMR8         0x2c20
#define REG_TEXORG       0x2c24
#define REG_TEXWIDTH     0x2c28
#define REG_TEXHEIGHT    0x2c2c
#define REG_TEXCTL       0x2c30
#define REG_TEXTRANS     0x2c34
#define REG_SECADDRESS   0x2c40
#define REG_SECEND       0x2c44
#define REG_SOFTRAP      0x2c48

/*Mystique only*/
#define REG_PALWTADD     0x3c00
#define REG_PALDATA      0x3c01
#define REG_PIXRDMSK     0x3c02
#define REG_PALRDADD     0x3c03
#define REG_X_DATAREG    0x3c0a
#define REG_CURPOSX      0x3c0c
#define REG_CURPOSY      0x3c0e

#define REG_STATUS_VSYNCSTS (1 << 3)

#define CRTCX_R0_STARTADD_MASK (0xf << 0)
#define CRTCX_R0_OFFSET_MASK   (3 << 4)

#define CRTCX_R1_HTOTAL8     (1 << 0)

#define CRTCX_R2_VTOTAL10    (1 << 0)
#define CRTCX_R2_VTOTAL11    (1 << 1)
#define CRTCX_R2_VDISPEND10  (1 << 2)
#define CRTCX_R2_VBLKSTR10   (1 << 3)
#define CRTCX_R2_VBLKSTR11   (1 << 4)
#define CRTCX_R2_VSYNCSTR10  (1 << 5)
#define CRTCX_R2_VSYNCSTR11  (1 << 6)
#define CRTCX_R2_LINECOMP10  (1 << 7)

#define CRTCX_R3_MGAMODE (1 << 7)

#define XREG_XCURADDL    0x04
#define XREG_XCURADDH    0x05
#define XREG_XCURCTRL    0x06

#define XREG_XCURCOL0R   0x08
#define XREG_XCURCOL0G   0x09
#define XREG_XCURCOL0B   0x0a

#define XREG_XCURCOL1R   0x0c
#define XREG_XCURCOL1G   0x0d
#define XREG_XCURCOL1B   0x0e

#define XREG_XCURCOL2R   0x10
#define XREG_XCURCOL2G   0x11
#define XREG_XCURCOL2B   0x12

#define XREG_XVREFCTRL   0x18
#define XREG_XMULCTRL    0x19
#define XREG_XPIXCLKCTRL 0x1a
#define XREG_XGENCTRL    0x1d
#define XREG_XMISCCTRL   0x1e

#define XREG_XGENIOCTRL  0x2a
#define XREG_XGENIODATA  0x2b

#define XREG_XSYSPLLM    0x2c
#define XREG_XSYSPLLN    0x2d
#define XREG_XSYSPLLP    0x2e
#define XREG_XSYSPLLSTAT 0x2f

#define XREG_XZOOMCTRL   0x38

#define XREG_XSENSETEST  0x3a

#define XREG_XCRCREML    0x3c
#define XREG_XCRCREMH    0x3d
#define XREG_XCRCBITSEL  0x3e

#define XREG_XCOLKEYMSKL 0x40
#define XREG_XCOLKEYMSKH 0x41
#define XREG_XCOLKEYL    0x42
#define XREG_XCOLKEYH    0x43

#define XREG_XPIXPLLCM   0x4c
#define XREG_XPIXPLLCN   0x4d
#define XREG_XPIXPLLCP   0x4e
#define XREG_XPIXPLLSTAT 0x4f

#define XMISCCTRL_VGA8DAC (1 << 3)

#define XMULCTRL_DEPTH_MASK         (7 << 0)
#define XMULCTRL_DEPTH_8            (0 << 0)
#define XMULCTRL_DEPTH_15           (1 << 0)
#define XMULCTRL_DEPTH_16           (2 << 0)
#define XMULCTRL_DEPTH_24           (3 << 0)
#define XMULCTRL_DEPTH_32_OVERLAYED (4 << 0)
#define XMULCTRL_DEPTH_2G8V16       (5 << 0)
#define XMULCTRL_DEPTH_G16V16       (6 << 0)
#define XMULCTRL_DEPTH_32           (7 << 0)

#define XSYSPLLSTAT_SYSLOCK (1 << 6)

#define XPIXPLLSTAT_SYSLOCK (1 << 6)

#define XCURCTRL_CURMODE_MASK (3 << 0)
#define XCURCTRL_CURMODE_3COL (1 << 0)
#define XCURCTRL_CURMODE_XGA  (2 << 0)
#define XCURCTRL_CURMODE_XWIN (3 << 0)

#define DWGCTRL_OPCODE_MASK           (0xf << 0)
#define DWGCTRL_OPCODE_LINE_OPEN      (0x0 << 0)
#define DWGCTRL_OPCODE_AUTOLINE_OPEN  (0x1 << 0)
#define DWGCTRL_OPCODE_LINE_CLOSE     (0x2 << 0)
#define DWGCTRL_OPCODE_AUTOLINE_CLOSE (0x3 << 0)
#define DWGCTRL_OPCODE_TRAP           (0x4 << 0)
#define DWGCTRL_OPCODE_TEXTURE_TRAP   (0x6 << 0)
#define DWGCTRL_OPCODE_ILOAD_HIGH     (0x7 << 0)
#define DWGCTRL_OPCODE_BITBLT         (0x8 << 0)
#define DWGCTRL_OPCODE_ILOAD          (0x9 << 0)
#define DWGCTRL_OPCODE_IDUMP          (0xa << 0)
#define DWGCTRL_OPCODE_FBITBLT        (0xc << 0)
#define DWGCTRL_OPCODE_ILOAD_SCALE    (0xd << 0)
#define DWGCTRL_OPCODE_ILOAD_HIGHV    (0xe << 0)
#define DWGCTRL_ATYPE_MASK      (7 << 4)
#define DWGCTRL_ATYPE_RPL       (0 << 4)
#define DWGCTRL_ATYPE_RSTR      (1 << 4)
#define DWGCTRL_ATYPE_ZI        (3 << 4)
#define DWGCTRL_ATYPE_BLK       (4 << 4)
#define DWGCTRL_ATYPE_I         (7 << 4)
#define DWGCTRL_LINEAR          (1 << 7)
#define DWGCTRL_ZMODE_MASK      (7 << 8)
#define DWGCTRL_ZMODE_NOZCMP    (0 << 8)
#define DWGCTRL_ZMODE_ZE        (2 << 8)
#define DWGCTRL_ZMODE_ZNE       (3 << 8)
#define DWGCTRL_ZMODE_ZLT       (4 << 8)
#define DWGCTRL_ZMODE_ZLTE      (5 << 8)
#define DWGCTRL_ZMODE_ZGT       (6 << 8)
#define DWGCTRL_ZMODE_ZGTE      (7 << 8)
#define DWGCTRL_SOLID           (1 << 11)
#define DWGCTRL_ARZERO          (1 << 12)
#define DWGCTRL_SGNZERO         (1 << 13)
#define DWGCTRL_SHTZERO         (1 << 14)
#define DWGCTRL_BOP_MASK        (0xf << 16)
#define DWGCTRL_TRANS_SHIFT     (20)
#define DWGCTRL_TRANS_MASK      (0xf << DWGCTRL_TRANS_SHIFT)
#define DWGCTRL_BLTMOD_MASK     (0xf << 25)
#define DWGCTRL_BLTMOD_BMONOLEF (0x0 << 25)
#define DWGCTRL_BLTMOD_BFCOL    (0x2 << 25)
#define DWGCTRL_BLTMOD_BMONOWF  (0x4 << 25)
#define DWGCTRL_BLTMOD_BU32RGB  (0x7 << 25)
#define DWGCTRL_BLTMOD_BUYUV    (0xe << 25)
#define DWGCTRL_BLTMOD_BU24RGB  (0xf << 25)
#define DWGCTRL_PATTERN         (1 << 29)
#define DWGCTRL_TRANSC          (1 << 30)
#define BOP(x) ((x) << 16)

#define MACCESS_PWIDTH_MASK  (3 << 0)
#define MACCESS_PWIDTH_8     (0 << 0)
#define MACCESS_PWIDTH_16    (1 << 0)
#define MACCESS_PWIDTH_32    (2 << 0)
#define MACCESS_PWIDTH_24    (3 << 0)
#define MACCESS_TLUTLOAD     (1 << 29)
#define MACCESS_NODITHER     (1 << 30)
#define MACCESS_DIT555       (1 << 31)

#define PITCH_MASK  0x7e0
#define PITCH_YLIN  (1 << 15)

#define SGN_SDYDXL   (1 << 0)
#define SGN_SCANLEFT (1 << 0)
#define SGN_SDXL     (1 << 1)
#define SGN_SDY      (1 << 2)
#define SGN_SDXR     (1 << 5)

#define DMA_ADDR_MASK   0xfffffffc
#define DMA_MODE_MASK   3

#define DMA_MODE_REG    0
#define DMA_MODE_BLIT   1
#define DMA_MODE_VECTOR 2

#define STATUS_SOFTRAPEN   (1 << 0)
#define STATUS_VSYNCPEN    (1 << 4)
#define STATUS_VLINEPEN    (1 << 5)
#define STATUS_DWGENGSTS   (1 << 16)
#define STATUS_ENDPRDMASTS (1 << 17)

#define ICLEAR_SOFTRAPICLR (1 << 0)
#define ICLEAR_VLINEICLR   (1 << 5)

#define IEN_SOFTRAPEN      (1 << 0)

#define TEXCTL_TEXFORMAT_MASK (7 << 0)
#define TEXCTL_TEXFORMAT_TW4  (0 << 0)
#define TEXCTL_TEXFORMAT_TW8  (1 << 0)
#define TEXCTL_TEXFORMAT_TW15 (2 << 0)
#define TEXCTL_TEXFORMAT_TW16 (3 << 0)
#define TEXCTL_TEXFORMAT_TW12 (4 << 0)
#define TEXCTL_PALSEL_MASK    (0xf << 4)
#define TEXCTL_TPITCH_SHIFT   (16)
#define TEXCTL_TPITCH_MASK    (7 << TEXCTL_TPITCH_SHIFT)
#define TEXCTL_NPCEN          (1 << 21)
#define TEXCTL_DECALCKEY      (1 << 24)
#define TEXCTL_TAKEY          (1 << 25)
#define TEXCTL_TAMASK         (1 << 26)
#define TEXCTL_CLAMPV         (1 << 27)
#define TEXCTL_CLAMPU         (1 << 28)
#define TEXCTL_TMODULATE      (1 << 29)
#define TEXCTL_STRANS         (1 << 30)
#define TEXCTL_ITRANS         (1 << 31)

#define TEXHEIGHT_TH_MASK      (0x3f << 0)
#define TEXHEIGHT_THMASK_SHIFT (18)
#define TEXHEIGHT_THMASK_MASK  (0x7ff << TEXHEIGHT_THMASK_SHIFT)

#define TEXWIDTH_TW_MASK       (0x3f << 0)
#define TEXWIDTH_TWMASK_SHIFT  (18)
#define TEXWIDTH_TWMASK_MASK   (0x7ff << TEXWIDTH_TWMASK_SHIFT)

#define TEXTRANS_TCKEY_MASK (0xffff)
#define TEXTRANS_TKMASK_SHIFT (16)
#define TEXTRANS_TKMASK_MASK (0xffff << TEXTRANS_TKMASK_SHIFT)

#define DITHER_565      0
#define DITHER_NONE_565 1
#define DITHER_555      2
#define DITHER_NONE_555 3

/*PCI configuration registers*/
#define OPTION_INTERLEAVE (1 << 12)

static void mystique_recalc_mapping(mystique_t* mystique);
static int mystique_line_compare(svga_t* svga);

static uint8_t mystique_iload_read_b(uint32_t addr, void* p);
static uint32_t mystique_iload_read_l(uint32_t addr, void* p);
static void mystique_iload_write_b(uint32_t addr, uint8_t val, void* p);
static void mystique_iload_write_l(uint32_t addr, uint32_t val, void* p);

static uint32_t blit_idump_read(mystique_t* mystique);
static void blit_iload_write(mystique_t* mystique, uint32_t data, int size);

void mystique_out(uint16_t addr, uint8_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        svga_t* svga = &mystique->svga;
        uint8_t old;

        if ((((addr & 0xFFF0) == 0x3D0 || (addr & 0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga->miscout & 1)) addr ^= 0x60; // mono / color addr selection

//       pclog("mystique_out : %04X %02X\n", addr, val);
//       pclog("  %04X:%04X\n", CS,cpu_state.pc);
        switch (addr)
        {
        case 0x3c8:
                mystique->xreg_idx = val;
        case 0x3c6:
        case 0x3c7:
        case 0x3c9:
                if (mystique->type == MGA_2064W)
                {
                        tvp3026_ramdac_out(addr & 3, val, &mystique->tvp3026, svga);
                        return;
                }
                break;

        case 0x3cf:
                if ((svga->gdcaddr & 15) == 6 && svga->gdcreg[6] != val)
                {
                        svga->gdcreg[svga->gdcaddr & 15] = val;
                        mystique_recalc_mapping(mystique);
                        return;
                }
                break;

        case 0x3D4:
                svga->crtcreg = val & 0x3f;
                return;
        case 0x3D5:
//                if (svga->crtcreg == 0xc)
//                        pclog("Buffer swap\n");
                if (((svga->crtcreg & 0x3f) < 7) && (svga->crtc[0x11] & 0x80))
                        return;
                if (((svga->crtcreg & 0x3f) == 7) && (svga->crtc[0x11] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                old = svga->crtc[svga->crtcreg & 0x3f];
                svga->crtc[svga->crtcreg & 0x3f] = val;
                if (old != val)
                {
                        if ((svga->crtcreg & 0x3f) < 0xE || (svga->crtcreg & 0x3f) > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                        if (svga->crtcreg == 0x11)
                        {
                                if (!(val & 0x10))
                                        mystique->status &= ~STATUS_VSYNCPEN;
                                mystique_update_irqs(mystique);
                        }
                }
                break;

        case 0x3de:
                mystique->crtcext_idx = val;
                break;
        case 0x3df:
                if (mystique->crtcext_idx < 6)
                        mystique->crtcext_regs[mystique->crtcext_idx] = val;
                if (mystique->crtcext_idx < 4)
                {
                        svga->fullchange = changeframecount;
                        svga_recalctimings(svga);
                }
                if (mystique->crtcext_idx == 3)
                {
                        if (val & CRTCX_R3_MGAMODE)
                                svga->fb_only = 1;
                        else
                                svga->fb_only = 0;
                        svga_recalctimings(svga);

//                        pclog("Write CRTCX[3]=%02x\n", val);
                }
                if (mystique->crtcext_idx == 4)
                {
                        if (svga->gdcreg[6] & 0xc)
                        {
                                /*64k banks*/
                                svga->read_bank = (val & 0x7f) << 16;
                                svga->write_bank = (val & 0x7f) << 16;
                        }
                        else
                        {
                                /*128k banks*/
                                svga->read_bank = (val & 0x7e) << 16;
                                svga->write_bank = (val & 0x7e) << 16;
                        }
//                        pclog("Write CRTCX[4]=%02x %08x\n", val, svga->read_bank);
                }
                break;

        }
        svga_out(addr, val, svga);
}

uint8_t mystique_in(uint16_t addr, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        svga_t* svga = &mystique->svga;
        uint8_t temp = 0xff;

        if ((((addr & 0xFFF0) == 0x3D0 || (addr & 0xFFF0) == 0x3B0) && addr < 0x3de) && !(svga->miscout & 1)) addr ^= 0x60;  // mono / color addr selection

//        if (addr != 0x3da && addr != 0x3ba) pclog("mystique_in : %04X ", addr);

        switch (addr)
        {
        case 0x3c1:
                if (svga->attraddr >= 0x15)
                        temp = 0;
                else
                        temp = svga->attrregs[svga->attraddr];
                break;

        case 0x3c6:
        case 0x3c7:
        case 0x3c8:
        case 0x3c9:
                if (mystique->type == MGA_2064W)
                        temp = tvp3026_ramdac_in(addr & 3, &mystique->tvp3026, svga);
                else
                        temp = svga_in(addr, svga);
                break;

        case 0x3D4:
                temp = svga->crtcreg;
                break;
        case 0x3D5:
                if ((svga->crtcreg >= 0x19 && svga->crtcreg <= 0x21) ||
                    svga->crtcreg == 0x23 || svga->crtcreg == 0x25 || svga->crtcreg >= 0x27)
                        temp = 0;
                else
                        temp = svga->crtc[svga->crtcreg & 0x3f];
                break;

        case 0x3de:
                temp = mystique->crtcext_idx;
                break;
        case 0x3df:
                if (mystique->crtcext_idx < 6)
                        temp = mystique->crtcext_regs[mystique->crtcext_idx];
                break;

        default:
                temp = svga_in(addr, svga);
                break;
        }

//        if (addr != 0x3da && addr != 0x3ba) pclog("%02X\n", temp);
        return temp;
}

static int mystique_line_compare(svga_t* svga)
{
        mystique_t* mystique = (mystique_t*)svga->p;

        mystique->status |= STATUS_VLINEPEN;
        mystique_update_irqs(mystique);

        return 0;
}

static void mystique_vsync_callback(svga_t* svga)
{
        mystique_t* mystique = (mystique_t*)svga->p;

        if (svga->crtc[0x11] & 0x10)
        {
                mystique->status |= STATUS_VSYNCPEN;
                mystique_update_irqs(mystique);
        }

}

void mystique_recalctimings(svga_t* svga)
{
        mystique_t* mystique = (mystique_t*)svga->p;
        int clk_sel = (svga->miscout >> 2) & 3;

        if (clk_sel & 2)
        {
                if (mystique->type == MGA_1064SG)
                {
                        int m = mystique->xpixpll[2].m;
                        int n = mystique->xpixpll[2].n;
                        int p = mystique->xpixpll[2].p;

                        double fvco = 14318181.0 * (n + 1) / (m + 1);
                        double fo = fvco / (p + 1);

                        svga->clock = (cpuclock * (float)(1ull << 32)) / fo;
                }
                else
                {
                        svga->clock = (cpuclock * (float)(1ull << 32)) / tvp3026_getclock(&mystique->tvp3026);
                }
        }

        if (mystique->crtcext_regs[1] & CRTCX_R1_HTOTAL8)
                svga->htotal += 0x100;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VTOTAL10)
                svga->vtotal += 0x400;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VTOTAL11)
                svga->vtotal += 0x800;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VDISPEND10)
                svga->dispend += 0x400;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VBLKSTR10)
                svga->vblankstart += 0x400;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VBLKSTR11)
                svga->vblankstart += 0x800;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VSYNCSTR10)
                svga->vsyncstart += 0x400;
        if (mystique->crtcext_regs[2] & CRTCX_R2_VSYNCSTR11)
                svga->vsyncstart += 0x800;
        if (mystique->crtcext_regs[2] & CRTCX_R2_LINECOMP10)
                svga->split += 0x400;
//        pclog("svga->split=%i\n", svga->split);

        if (mystique->crtcext_regs[3] & CRTCX_R3_MGAMODE)
        {
                svga->packed_chain4 = 1;

//                svga->split = 99999;
                svga->lowres = 0;
                svga->char_width = 8;
                svga->hdisp = (svga->crtc[1] + 1) * 8;
                svga->hdisp_time = svga->hdisp;
                svga->rowoffset = svga->crtc[0x13] | ((mystique->crtcext_regs[0] & CRTCX_R0_OFFSET_MASK) << 4);
                svga->ma_latch = ((mystique->crtcext_regs[0] & CRTCX_R0_STARTADD_MASK) << 16) |
                                 (svga->crtc[0xc] << 8) | svga->crtc[0xd];
                if (mystique->pci_regs[0x41] & (OPTION_INTERLEAVE >> 8))
                {
                        svga->rowoffset *= 2;
                        svga->ma_latch *= 2;
                }

                if (mystique->type == MGA_1064SG)
                {
                        /*Mystique, unlike most SVGA cards, allows display start to take
                          effect mid-screen*/
                        if (svga->ma_latch != mystique->ma_latch_old)
                        {
//                        pclog("ma_latch change: old=%08x new=%08x ma=%08x ", mystique->ma_latch_old, svga->ma_latch, svga->maback);
                                svga->ma = svga->maback = (svga->maback - (mystique->ma_latch_old << 2)) + (svga->ma_latch << 2);
//                        pclog("ma_new=%08x\n", svga->maback);
                                mystique->ma_latch_old = svga->ma_latch;
                        }
                        switch (mystique->xmulctrl & XMULCTRL_DEPTH_MASK)
                        {
                        case XMULCTRL_DEPTH_8:
                        case XMULCTRL_DEPTH_2G8V16:
                                svga->render = svga_render_8bpp_highres;
                                svga->bpp = 8;
                                break;
                        case XMULCTRL_DEPTH_15:
                        case XMULCTRL_DEPTH_G16V16:
                                svga->render = svga_render_15bpp_highres;
                                svga->bpp = 15;
                                break;
                        case XMULCTRL_DEPTH_16:
                                svga->render = svga_render_16bpp_highres;
                                svga->bpp = 15;
                                break;
                        case XMULCTRL_DEPTH_24:
                                svga->render = svga_render_24bpp_highres;
                                svga->bpp = 24;
                                break;
                        case XMULCTRL_DEPTH_32:
                        case XMULCTRL_DEPTH_32_OVERLAYED:
                                svga->render = svga_render_32bpp_highres;
                                svga->bpp = 32;
                                break;
                        }
                }
                else
                {
                        switch (svga->bpp)
                        {
                        case 8:
                                svga->render = svga_render_8bpp_highres;
                                break;
                        case 15:
                                svga->render = svga_render_15bpp_highres;
                                break;
                        case 16:
                                svga->render = svga_render_16bpp_highres;
                                break;
                        case 24:
                                svga->render = svga_render_24bpp_highres;
                                break;
                        case 32:
                                svga->render = svga_render_32bpp_highres;
                                break;
                        }
                }
                svga->video_bpp = svga->bpp;

                svga->line_compare = mystique_line_compare;
//                pclog("recalc: htotal=%i vtotal=%i dispend=%i\n", svga->htotal, svga->vtotal, svga->dispend);
        }
        else
        {
                svga->packed_chain4 = 0;

                svga->line_compare = NULL;
                if (mystique->type == MGA_1064SG)
                        svga->bpp = 8;
        }
}

static void mystique_recalc_mapping(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;

        io_removehandler(0x03c0, 0x0020, mystique_in, NULL, NULL, mystique_out, NULL, NULL, mystique);
        if ((mystique->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO) && (mystique->pci_regs[0x41] & 1))
                io_sethandler(0x03c0, 0x0020, mystique_in, NULL, NULL, mystique_out, NULL, NULL, mystique);

        if (!(mystique->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM))
        {
                mem_mapping_disable(&svga->mapping);
                mem_mapping_disable(&mystique->ctrl_mapping);
                mem_mapping_disable(&mystique->lfb_mapping);
                mem_mapping_disable(&mystique->iload_mapping);
                return;
        }

        if (mystique->ctrl_base)
                mem_mapping_set_addr(&mystique->ctrl_mapping, mystique->ctrl_base, 0x4000);
        else
                mem_mapping_disable(&mystique->ctrl_mapping);
        if (mystique->lfb_base)
                mem_mapping_set_addr(&mystique->lfb_mapping, mystique->lfb_base, 0x800000);
        else
                mem_mapping_disable(&mystique->lfb_mapping);
        if (mystique->iload_base)
                mem_mapping_set_addr(&mystique->iload_mapping, mystique->iload_base, 0x800000);
        else
                mem_mapping_disable(&mystique->iload_mapping);

        if (mystique->pci_regs[0x41] & 1)
        {
                switch (svga->gdcreg[6] & 0x0C)
                {
                case 0x0: /*128k at A0000*/
                        mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
                        svga->banked_mask = 0x1ffff;
                        break;
                case 0x4: /*64k at A0000*/
                        mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
                        svga->banked_mask = 0xffff;
                        break;
                case 0x8: /*32k at B0000*/
                        mem_mapping_set_addr(&svga->mapping, 0xb0000, 0x08000);
                        svga->banked_mask = 0x7fff;
                        break;
                case 0xC: /*32k at B8000*/
                        mem_mapping_set_addr(&svga->mapping, 0xb8000, 0x08000);
                        svga->banked_mask = 0x7fff;
                        break;
                }
                if (svga->gdcreg[6] & 0xc)
                {
                        /*64k banks*/
                        svga->read_bank = (mystique->crtcext_regs[4] & 0x7f) << 16;
                        svga->write_bank = (mystique->crtcext_regs[4] & 0x7f) << 16;
                }
                else
                {
                        /*128k banks*/
                        svga->read_bank = (mystique->crtcext_regs[4] & 0x7e) << 16;
                        svga->write_bank = (mystique->crtcext_regs[4] & 0x7e) << 16;
                }
        }
        else
                mem_mapping_disable(&svga->mapping);
}

static void mystique_update_irqs(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;
        int irq = 0;

        if ((mystique->status & mystique->ien) & STATUS_SOFTRAPEN)
                irq = 1;
        if ((mystique->status & STATUS_VSYNCPEN) && (svga->crtc[0x11] & 0x30) == 0x10)
                irq = 1;

        if (irq)
                pci_set_irq(mystique->card, PCI_INTA);
        else
                pci_clear_irq(mystique->card, PCI_INTA);
}

#define READ8(addr, var)        switch ((addr) & 3)                                     \
                                {                                                       \
                                        case 0: ret = (var) & 0xff;         break;      \
                                        case 1: ret = ((var) >> 8) & 0xff;  break;      \
                                        case 2: ret = ((var) >> 16) & 0xff; break;      \
                                        case 3: ret = ((var) >> 24) & 0xff; break;      \
                                }

#define WRITE8(addr, var, val)  switch ((addr) & 3)                                             \
                                {                                                               \
                                        case 0: var = (var & 0xffffff00) | (val);         break;  \
                                        case 1: var = (var & 0xffff00ff) | ((val) << 8);  break;  \
                                        case 2: var = (var & 0xff00ffff) | ((val) << 16); break;  \
                                        case 3: var = (var & 0x00ffffff) | ((val) << 24); break;  \
                                }

static uint8_t mystique_read_xreg(mystique_t* mystique, int reg)
{
        uint8_t ret = 0xff;

        switch (reg)
        {
        case XREG_XCURADDL:
                ret = mystique->cursor.addr & 0xff;
                break;
        case XREG_XCURADDH:
                ret = mystique->cursor.addr >> 8;
                break;
        case XREG_XCURCTRL:
                ret = mystique->xcurctrl;
                break;

        case XREG_XCURCOL0R:
        case XREG_XCURCOL0G:
        case XREG_XCURCOL0B:
                READ8(reg, mystique->cursor.col[0]);
                break;
        case XREG_XCURCOL1R:
        case XREG_XCURCOL1G:
        case XREG_XCURCOL1B:
                READ8(reg, mystique->cursor.col[1]);
                break;
        case XREG_XCURCOL2R:
        case XREG_XCURCOL2G:
        case XREG_XCURCOL2B:
                READ8(reg, mystique->cursor.col[2]);
                break;

        case XREG_XMULCTRL:
                ret = mystique->xmulctrl;
                break;

        case XREG_XMISCCTRL:
                ret = mystique->xmiscctrl;
                break;

        case XREG_XGENCTRL:
                ret = mystique->xgenctrl;
                break;

        case XREG_XVREFCTRL:
                ret = mystique->xvrefctrl;
                break;

        case XREG_XGENIOCTRL:
                ret = mystique->xgenioctrl;
                break;
        case XREG_XGENIODATA:
                ret = mystique->xgeniodata;
                break;

        case XREG_XSYSPLLM:
                ret = mystique->xsyspllm;
                break;
        case XREG_XSYSPLLN:
                ret = mystique->xsysplln;
                break;
        case XREG_XSYSPLLP:
                ret = mystique->xsyspllp;
                break;

        case XREG_XZOOMCTRL:
                ret = mystique->xzoomctrl;
                break;

        case XREG_XSENSETEST:
                ret = 0;
                if (mystique->svga.vgapal[0].b < 0x80)
                        ret |= 1;
                if (mystique->svga.vgapal[0].g < 0x80)
                        ret |= 2;
                if (mystique->svga.vgapal[0].r < 0x80)
                        ret |= 4;
                break;

        case XREG_XCRCREML: /*CRC not implemented*/
                ret = 0;
                break;
        case XREG_XCRCREMH:
                ret = 0;
                break;
        case XREG_XCRCBITSEL:
                ret = mystique->xcrcbitsel;
                break;

        case XREG_XCOLKEYMSKL:
                ret = mystique->xcolkeymskl;
                break;
        case XREG_XCOLKEYMSKH:
                ret = mystique->xcolkeymskh;
                break;
        case XREG_XCOLKEYL:
                ret = mystique->xcolkeyl;
                break;
        case XREG_XCOLKEYH:
                ret = mystique->xcolkeyh;
                break;

        case XREG_XPIXCLKCTRL:
                ret = mystique->xpixclkctrl;
                break;

        case XREG_XSYSPLLSTAT:
                ret = XSYSPLLSTAT_SYSLOCK;
                break;

        case XREG_XPIXPLLSTAT:
                ret = XPIXPLLSTAT_SYSLOCK;
                break;

        case XREG_XPIXPLLCM:
                ret = mystique->xpixpll[2].m;
                break;
        case XREG_XPIXPLLCN:
                ret = mystique->xpixpll[2].n;
                break;
        case XREG_XPIXPLLCP:
                ret = mystique->xpixpll[2].p | (mystique->xpixpll[2].s << 3);
                break;

        case 0x00:
        case 0x20:
        case 0x3f:
                ret = 0xff;
                break;

        default:
                if (reg >= 0x50)
                        ret = 0xff;
#ifndef RELEASE_BUILD
                else
                        fatal("Read XREG %02x\n", reg);
#endif
                break;
        }
        return ret;
}

static void mystique_write_xreg(mystique_t* mystique, int reg, uint8_t val)
{
        svga_t* svga = &mystique->svga;

        switch (reg)
        {
        case XREG_XCURADDL:
                mystique->cursor.addr = (mystique->cursor.addr & 0x1f00) | val;
                svga->hwcursor.addr = mystique->cursor.addr << 10;
                break;
        case XREG_XCURADDH:
                mystique->cursor.addr = (mystique->cursor.addr & 0x00ff) | ((val & 0x1f) << 8);
                svga->hwcursor.addr = mystique->cursor.addr << 10;
                break;

        case XREG_XCURCTRL:
                mystique->xcurctrl = val;
                svga->hwcursor.ena = (val & 3) ? 1 : 0;
                break;

        case XREG_XCURCOL0R:
        case XREG_XCURCOL0G:
        case XREG_XCURCOL0B:
                WRITE8(reg, mystique->cursor.col[0], val);
                break;
        case XREG_XCURCOL1R:
        case XREG_XCURCOL1G:
        case XREG_XCURCOL1B:
                WRITE8(reg, mystique->cursor.col[1], val);
                break;
        case XREG_XCURCOL2R:
        case XREG_XCURCOL2G:
        case XREG_XCURCOL2B:
                WRITE8(reg, mystique->cursor.col[2], val);
                break;

        case XREG_XMULCTRL:
                mystique->xmulctrl = val;
                break;

        case XREG_XMISCCTRL:
                mystique->xmiscctrl = val;
                svga_set_ramdac_type(svga, (val & XMISCCTRL_VGA8DAC) ? RAMDAC_8BIT : RAMDAC_6BIT);
                break;

        case XREG_XGENCTRL:
                mystique->xgenctrl = val;
                break;

        case XREG_XVREFCTRL:
                mystique->xvrefctrl = val;
                break;

        case XREG_XGENIOCTRL:
                mystique->xgenioctrl = val;
                break;
        case XREG_XGENIODATA:
                mystique->xgeniodata = val;
                break;

        case XREG_XSYSPLLM:
                mystique->xsyspllm = val;
                break;
        case XREG_XSYSPLLN:
                mystique->xsysplln = val;
                break;
        case XREG_XSYSPLLP:
                mystique->xsyspllp = val;
                break;

        case XREG_XZOOMCTRL:
                mystique->xzoomctrl = val & 3;
                break;

        case XREG_XSENSETEST:
                break;

        case XREG_XCRCREML: /*CRC not implemented*/
                break;
        case XREG_XCRCREMH:
                break;
        case XREG_XCRCBITSEL:
                mystique->xcrcbitsel = val & 0x1f;
                break;

        case XREG_XCOLKEYMSKL:
                mystique->xcolkeymskl = val;
                break;
        case XREG_XCOLKEYMSKH:
                mystique->xcolkeymskh = val;
                break;
        case XREG_XCOLKEYL:
                mystique->xcolkeyl = val;
                break;
        case XREG_XCOLKEYH:
                mystique->xcolkeyh = val;
                break;

        case XREG_XSYSPLLSTAT:
                break;

        case XREG_XPIXPLLSTAT:
                break;

        case XREG_XPIXCLKCTRL:
                mystique->xpixclkctrl = val;
                break;

        case XREG_XPIXPLLCM:
                mystique->xpixpll[2].m = val;
                break;
        case XREG_XPIXPLLCN:
                mystique->xpixpll[2].n = val;
                break;
        case XREG_XPIXPLLCP:
                mystique->xpixpll[2].p = val & 7;
                mystique->xpixpll[2].s = (val >> 3) & 3;
                break;

        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x07:
        case 0x0b:
        case 0x0f:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x1b:
        case 0x1c:
        case 0x20:
        case 0x39:
        case 0x3b:
        case 0x3f:
        case 0x47:
        case 0x4b:
                break;

#ifndef RELEASE_BUILD
        default:
                if (reg < 0x50)
                        fatal("Write XREG %02x %02x\n", reg, val);
                break;
#endif
        }
}

static uint8_t mystique_ctrl_read_b(uint32_t addr, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        svga_t* svga = &mystique->svga;
        uint8_t ret = 0xff;
        int fifocount;

//        pclog("mystique read %04x  %08x\n", addr & 0x3fff, cpu_state.pc);

        if (mystique->type == MGA_2064W && (addr & 0x3e00) == 0x3c00)
        {
                /*RAMDAC*/
                ret = tvp3026_ramdac_in(addr & 0xf, &mystique->tvp3026, svga);
        }
        else
                switch (addr & 0x3fff)
                {
                case REG_FIFOSTATUS:
                        fifocount = FIFO_SIZE - FIFO_ENTRIES;
                        if (fifocount > 64)
                                fifocount = 64;
                        ret = fifocount;
                        break;
                case REG_FIFOSTATUS + 1:
                        if (FIFO_EMPTY)
                                ret |= 2;
                        else if (FIFO_ENTRIES >= 64)
                                ret |= 1;
//                ret = 2; /*Empty*/
                        break;
                case REG_FIFOSTATUS + 2:
                case REG_FIFOSTATUS + 3:
                        ret = 0;
                        break;

                case REG_STATUS:
                        ret = mystique->status & 0xff;
                        if (svga->cgastat & 8)
                                ret |= REG_STATUS_VSYNCSTS;
//                pclog("REG_STATUS %02x\n", ret);
                        break;
                case REG_STATUS + 1:
                        ret = (mystique->status >> 8) & 0xff;
                        break;
                case REG_STATUS + 2:
                        ret = (mystique->status >> 16) & 0xff;
                        if (mystique->busy ||
                            ((mystique->blitter_submit_refcount + mystique->blitter_submit_dma_refcount) != mystique->blitter_complete_refcount) ||
                            !FIFO_EMPTY)
                                ret |= (STATUS_DWGENGSTS >> 16);
/*                thread_lock_mutex(mystique->dma.lock);
                if (mystique->dma.state == DMA_STATE_IDLE)
                        ret |= (STATUS_ENDPRDMASTS >> 16);
                thread_unlock_mutex(mystique->dma.lock);*/
/*                pclog("Read status %02x %i %i %i  fifo=%i\n", ret, mystique->busy,
                                ((mystique->blitter_submit_refcount + mystique->blitter_submit_dma_refcount) != mystique->blitter_complete_refcount),
                                !FIFO_EMPTY, FIFO_ENTRIES);*/
//                ret |= 2;
                        break;
                case REG_STATUS + 3:
                        ret = (mystique->status >> 24) & 0xff;
                        break;

                case REG_IEN:
                        ret = mystique->ien & 0x64;
                        break;
                case REG_IEN + 1:
                case REG_IEN + 2:
                case REG_IEN + 3:
                        ret = 0;
                        break;

                case REG_OPMODE:
                        ret = mystique->dmamod << 2;
                        break;
                case REG_OPMODE + 1:
                        ret = mystique->dmadatasiz;
                        break;
                case REG_OPMODE + 2:
                        ret = mystique->dirdatasiz;
                        break;
                case REG_OPMODE + 3:
                        ret = 0;
                        break;

                case REG_PRIMADDRESS:
                case REG_PRIMADDRESS + 1:
                case REG_PRIMADDRESS + 2:
                case REG_PRIMADDRESS + 3:
                        READ8(addr, mystique->dma.primaddress);
                        break;
                case REG_PRIMEND:
                case REG_PRIMEND + 1:
                case REG_PRIMEND + 2:
                case REG_PRIMEND + 3:
                        READ8(addr, mystique->dma.primend);
                        break;

                case REG_SECADDRESS:
                case REG_SECADDRESS + 1:
                case REG_SECADDRESS + 2:
                case REG_SECADDRESS + 3:
                        READ8(addr, mystique->dma.secaddress);
                        break;

                case REG_VCOUNT:
                case REG_VCOUNT + 1:
                case REG_VCOUNT + 2:
                case REG_VCOUNT + 3:
                        READ8(addr, svga->vc);
                        break;

                case REG_ATTR_IDX:
                        ret = svga_in(0x3c0, svga);
                        break;
                case REG_ATTR_DATA:
                        ret = svga_in(0x3c1, svga);
                        break;

                case REG_INSTS0:
                        ret = svga_in(0x3c2, svga);
                        break;

                case REG_SEQ_IDX:
                        ret = svga_in(0x3c4, svga);
                        break;
                case REG_SEQ_DATA:
                        ret = svga_in(0x3c5, svga);
                        break;

                case REG_MISCREAD:
                        ret = svga_in(0x3cc, svga);
                        break;

                case REG_GCTL_IDX:
                        ret = mystique_in(0x3ce, mystique);
                        break;
                case REG_GCTL_DATA:
                        ret = mystique_in(0x3cf, mystique);
                        break;

                case REG_CRTC_IDX:
                        ret = mystique_in(0x3d4, mystique);
                        break;
                case REG_CRTC_DATA:
                        ret = mystique_in(0x3d5, mystique);
                        break;

                case REG_INSTS1:
                        ret = mystique_in(0x3da, mystique);
                        break;

                case REG_CRTCEXT_IDX:
                        ret = mystique_in(0x3de, mystique);
                        break;
                case REG_CRTCEXT_DATA:
                        ret = mystique_in(0x3df, mystique);
                        break;

                case REG_PALWTADD:
                        ret = svga_in(0x3c8, svga);
                        break;
                case REG_PALDATA:
                        ret = svga_in(0x3c9, svga);
                        break;
                case REG_PIXRDMSK:
                        ret = svga_in(0x3c6, svga);
                        break;
                case REG_PALRDADD:
                        ret = svga_in(0x3c7, svga);
                        break;

                case REG_X_DATAREG:
                        ret = mystique_read_xreg(mystique, mystique->xreg_idx);
                        break;

                case 0x1c40:
                case 0x1c41:
                case 0x1c42:
                case 0x1c43:
                case 0x1d44:
                case 0x1d45:
                case 0x1d46:
                case 0x1d47:
                case 0x1e50:
                case 0x1e51:
                case 0x1e52:
                case 0x1e53:
                case REG_ICLEAR:
                case REG_ICLEAR + 1:
                case REG_ICLEAR + 2:
                case REG_ICLEAR + 3:
                case 0x2c30:
                case 0x2c31:
                case 0x2c32:
                case 0x2c33:
                case 0x3e08:
                        break;

                case 0x3c08:
                case 0x3c09:
                case 0x3c0b:
                        break;

                default:
                        if ((addr & 0x3fff) >= 0x2c00 && (addr & 0x3fff) < 0x2c40)
                                break;
                        if ((addr & 0x3fff) >= 0x3e00)
                                break;
#ifndef RELEASE_BUILD
                        fatal("Read MGA control %08x\n", addr & 0x3fff);
#endif
                }

//        pclog("mcrb: addr=%08x ret=%02x\n", addr, ret);
        return ret;
}

static void mystique_accel_ctrl_write_b(uint32_t addr, uint8_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        int start_blit = 0;
        int x;

//        pclog("mystique accel addr=%04x val=%02x\n", addr & 0x3fff, val);

        if ((addr & 0x300) == 0x100)
        {
                addr &= ~0x100;
                start_blit = 1;
        }

        switch (addr & 0x3fff)
        {
        case REG_MACCESS:
        case REG_MACCESS + 1:
        case REG_MACCESS + 2:
        case REG_MACCESS + 3:
                WRITE8(addr, mystique->maccess, val);
                mystique->dwgreg.dither = mystique->maccess >> 30;
                break;

        case REG_MCTLWTST:
        case REG_MCTLWTST + 1:
        case REG_MCTLWTST + 2:
        case REG_MCTLWTST + 3:
                WRITE8(addr, mystique->mctlwtst, val);
                break;

        case REG_PAT0:
        case REG_PAT0 + 1:
        case REG_PAT0 + 2:
        case REG_PAT0 + 3:
        case REG_PAT1:
        case REG_PAT1 + 1:
        case REG_PAT1 + 2:
        case REG_PAT1 + 3:
                for (x = 0; x < 8; x++)
                        mystique->dwgreg.pattern[addr & 7][x] = val & (1 << (7 - x));
                break;

        case REG_XYSTRT:
        case REG_XYSTRT + 1:
                WRITE8(addr & 1, mystique->dwgreg.ar[5], val);
                if (mystique->dwgreg.ar[5] & 0x8000)
                        mystique->dwgreg.ar[5] |= 0xffff8000;
                else
                        mystique->dwgreg.ar[5] &= ~0xffff8000;
                WRITE8(addr & 1, mystique->dwgreg.xdst, val);
                break;
        case REG_XYSTRT + 2:
        case REG_XYSTRT + 3:
                WRITE8(addr & 1, mystique->dwgreg.ar[6], val);
                if (mystique->dwgreg.ar[6] & 0x8000)
                        mystique->dwgreg.ar[6] |= 0xffff8000;
                else
                        mystique->dwgreg.ar[6] &= ~0xffff8000;
                WRITE8(addr & 1, mystique->dwgreg.ydst, val);
                mystique->dwgreg.ydst_lin = ((int32_t)(int16_t)mystique->dwgreg.ydst * (mystique->dwgreg.pitch & PITCH_MASK)) + mystique->dwgreg.ydstorg;
                break;

        case REG_XYEND:
        case REG_XYEND + 1:
                WRITE8(addr & 1, mystique->dwgreg.ar[0], val);
                if (mystique->dwgreg.ar[0] & 0x8000)
                        mystique->dwgreg.ar[0] |= 0xffff8000;
                else
                        mystique->dwgreg.ar[0] &= ~0xffff8000;
//                pclog("XYEND AR[0]=%08x\n", mystique->dwgreg.ar[0]);
                break;
        case REG_XYEND + 2:
        case REG_XYEND + 3:
                WRITE8(addr & 1, mystique->dwgreg.ar[2], val);
                if (mystique->dwgreg.ar[2] & 0x8000)
                        mystique->dwgreg.ar[2] |= 0xffff8000;
                else
                        mystique->dwgreg.ar[2] &= ~0xffff8000;
                break;

        case REG_SGN:
                mystique->dwgreg.sgn.sdydxl = val & SGN_SDYDXL;
                mystique->dwgreg.sgn.scanleft = val & SGN_SCANLEFT;
                mystique->dwgreg.sgn.sdxl = val & SGN_SDXL;
                mystique->dwgreg.sgn.sdy = val & SGN_SDY;
                mystique->dwgreg.sgn.sdxr = val & SGN_SDXR;
                break;
        case REG_SGN + 1:
        case REG_SGN + 2:
        case REG_SGN + 3:
                break;

        case REG_LEN:
        case REG_LEN + 1:
                WRITE8(addr, mystique->dwgreg.length, val);
                break;
        case REG_LEN + 2:
                break;
        case REG_LEN + 3:
                mystique->dwgreg.beta = val >> 4;
                if (!mystique->dwgreg.beta)
                        mystique->dwgreg.beta = 16;
                break;

        case REG_CXBNDRY:
        case REG_CXBNDRY + 1:
                WRITE8(addr, mystique->dwgreg.cxleft, val);
                break;
        case REG_CXBNDRY + 2:
        case REG_CXBNDRY + 3:
                WRITE8(addr & 1, mystique->dwgreg.cxright, val);
                break;
        case REG_FXBNDRY:
        case REG_FXBNDRY + 1:
                WRITE8(addr, mystique->dwgreg.fxleft, val);
                break;
        case REG_FXBNDRY + 2:
        case REG_FXBNDRY + 3:
                WRITE8(addr & 1, mystique->dwgreg.fxright, val);
                break;

        case REG_YDSTLEN:
        case REG_YDSTLEN + 1:
                WRITE8(addr, mystique->dwgreg.length, val);
//                pclog("Write YDSTLEN+%i %i\n", addr&1, mystique->dwgreg.length);
                break;
        case REG_YDSTLEN + 2:
                mystique->dwgreg.ydst = (mystique->dwgreg.ydst & ~0xff) | val;
                if (mystique->dwgreg.pitch & PITCH_YLIN)
                        mystique->dwgreg.ydst_lin = (mystique->dwgreg.ydst << 5) + mystique->dwgreg.ydstorg;
                else
                {
                        mystique->dwgreg.ydst_lin = ((int32_t)(int16_t)mystique->dwgreg.ydst * (mystique->dwgreg.pitch & PITCH_MASK)) + mystique->dwgreg.ydstorg;
                        mystique->dwgreg.selline = val & 7;
                }
//                pclog("Write YDSTLEN+2 %i %08x\n", mystique->dwgreg.ydst, mystique->dwgreg.ydst_lin);
                break;
        case REG_YDSTLEN + 3:
                mystique->dwgreg.ydst = (mystique->dwgreg.ydst & 0xff) | (((int32_t)(int8_t)val) << 8);
                if (mystique->dwgreg.pitch & PITCH_YLIN)
                        mystique->dwgreg.ydst_lin = (mystique->dwgreg.ydst << 5) + mystique->dwgreg.ydstorg;
                else
                        mystique->dwgreg.ydst_lin = ((int32_t)(int16_t)mystique->dwgreg.ydst * (mystique->dwgreg.pitch & PITCH_MASK)) + mystique->dwgreg.ydstorg;
//                pclog("Write YDSTLEN+3 %i %08x\n", mystique->dwgreg.ydst, mystique->dwgreg.ydst_lin);
                break;

        case REG_XDST:
        case REG_XDST + 1:
                WRITE8(addr & 1, mystique->dwgreg.xdst, val);
                break;
        case REG_XDST + 2:
        case REG_XDST + 3:
                break;

        case REG_YDSTORG:
        case REG_YDSTORG + 1:
        case REG_YDSTORG + 2:
        case REG_YDSTORG + 3:
                WRITE8(addr, mystique->dwgreg.ydstorg, val);
                mystique->dwgreg.z_base = mystique->dwgreg.ydstorg * 2 + mystique->dwgreg.zorg;
//                pclog("ydstorg=%06x z_base=%06x\n", mystique->dwgreg.ydstorg, mystique->dwgreg.z_base);
                break;
        case REG_YTOP:
        case REG_YTOP + 1:
        case REG_YTOP + 2:
        case REG_YTOP + 3:
                WRITE8(addr, mystique->dwgreg.ytop, val);
                break;
        case REG_YBOT:
        case REG_YBOT + 1:
        case REG_YBOT + 2:
        case REG_YBOT + 3:
                WRITE8(addr, mystique->dwgreg.ybot, val);
                break;

        case REG_CXLEFT:
        case REG_CXLEFT + 1:
                WRITE8(addr, mystique->dwgreg.cxleft, val);
                break;
        case REG_CXLEFT + 2:
        case REG_CXLEFT + 3:
                break;
        case REG_CXRIGHT:
        case REG_CXRIGHT + 1:
                WRITE8(addr, mystique->dwgreg.cxright, val);
                break;
        case REG_CXRIGHT + 2:
        case REG_CXRIGHT + 3:
                break;

        case REG_FXLEFT:
        case REG_FXLEFT + 1:
                WRITE8(addr, mystique->dwgreg.fxleft, val);
                break;
        case REG_FXLEFT + 2:
        case REG_FXLEFT + 3:
                break;
        case REG_FXRIGHT:
        case REG_FXRIGHT + 1:
                WRITE8(addr, mystique->dwgreg.fxright, val);
                break;
        case REG_FXRIGHT + 2:
        case REG_FXRIGHT + 3:
                break;

        case REG_SECADDRESS:
        case REG_SECADDRESS + 1:
        case REG_SECADDRESS + 2:
        case REG_SECADDRESS + 3:
                WRITE8(addr, mystique->dma.secaddress, val);
                mystique->dma.sec_state = 0;
                break;

        case REG_TMR0:
        case REG_TMR0 + 1:
        case REG_TMR0 + 2:
        case REG_TMR0 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[0], val);
                break;
        case REG_TMR1:
        case REG_TMR1 + 1:
        case REG_TMR1 + 2:
        case REG_TMR1 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[1], val);
                break;
        case REG_TMR2:
        case REG_TMR2 + 1:
        case REG_TMR2 + 2:
        case REG_TMR2 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[2], val);
                break;
        case REG_TMR3:
        case REG_TMR3 + 1:
        case REG_TMR3 + 2:
        case REG_TMR3 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[3], val);
                break;
        case REG_TMR4:
        case REG_TMR4 + 1:
        case REG_TMR4 + 2:
        case REG_TMR4 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[4], val);
                break;
        case REG_TMR5:
        case REG_TMR5 + 1:
        case REG_TMR5 + 2:
        case REG_TMR5 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[5], val);
                break;
        case REG_TMR6:
        case REG_TMR6 + 1:
        case REG_TMR6 + 2:
        case REG_TMR6 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[6], val);
                break;
        case REG_TMR7:
        case REG_TMR7 + 1:
        case REG_TMR7 + 2:
        case REG_TMR7 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[7], val);
                break;
        case REG_TMR8:
        case REG_TMR8 + 1:
        case REG_TMR8 + 2:
        case REG_TMR8 + 3:
                WRITE8(addr, mystique->dwgreg.tmr[8], val);
                break;

        case REG_TEXORG:
        case REG_TEXORG + 1:
        case REG_TEXORG + 2:
        case REG_TEXORG + 3:
                WRITE8(addr, mystique->dwgreg.texorg, val);
//                if ((addr & 3) == 3) pclog("texorg=%08x\n", mystique->dwgreg.texorg);
                break;
        case REG_TEXWIDTH:
        case REG_TEXWIDTH + 1:
        case REG_TEXWIDTH + 2:
        case REG_TEXWIDTH + 3:
                WRITE8(addr, mystique->dwgreg.texwidth, val);
//                if ((addr & 3) == 3) pclog("texwith=%08x\n", mystique->dwgreg.texwidth);
                break;
        case REG_TEXHEIGHT:
        case REG_TEXHEIGHT + 1:
        case REG_TEXHEIGHT + 2:
        case REG_TEXHEIGHT + 3:
                WRITE8(addr, mystique->dwgreg.texheight, val);
//                if ((addr & 3) == 3) pclog("texheight=%08x %i %i\n", mystique->dwgreg.texheight, mystique->dwgreg.texheight&0x3f, (int)(((mystique->dwgreg.texheight >> 9) & 0x3f) | ((mystique->dwgreg.texheight & (1 << 14)) ? 0xffffffc0 : 0)));
                break;
        case REG_TEXCTL:
        case REG_TEXCTL + 1:
        case REG_TEXCTL + 2:
        case REG_TEXCTL + 3:
                WRITE8(addr, mystique->dwgreg.texctl, val);
//                if ((addr & 3) == 3) pclog("texctl=%08x\n", mystique->dwgreg.texctl);
                mystique->dwgreg.ta_key = (mystique->dwgreg.texctl & TEXCTL_TAKEY) ? 1 : 0;
                mystique->dwgreg.ta_mask = (mystique->dwgreg.texctl & TEXCTL_TAMASK) ? 1 : 0;
                break;
        case REG_TEXTRANS:
        case REG_TEXTRANS + 1:
        case REG_TEXTRANS + 2:
        case REG_TEXTRANS + 3:
                WRITE8(addr, mystique->dwgreg.textrans, val);
//                /*if ((addr & 3) == 3) */pclog("textrans=%08x\n", mystique->dwgreg.textrans);
                break;

        case 0x1c18:
        case 0x1c19:
        case 0x1c1a:
        case 0x1c1b:
        case 0x1c28:
        case 0x1c29:
        case 0x1c2a:
        case 0x1c2b:
        case 0x1c2c:
        case 0x1c2d:
        case 0x1c2e:
        case 0x1c2f:
        case 0x1cc4:
        case 0x1cc5:
        case 0x1cc6:
        case 0x1cc7:
        case 0x1cd4:
        case 0x1cd5:
        case 0x1cd6:
        case 0x1cd7:
        case 0x1ce4:
        case 0x1ce5:
        case 0x1ce6:
        case 0x1ce7:
        case 0x1cf4:
        case 0x1cf5:
        case 0x1cf6:
        case 0x1cf7:
                break;

        case REG_OPMODE:
                mystique->dwgreg.dmamod = (val >> 2) & 3;
                mystique->dma.iload_state = 0;
                break;

        default:
                if ((addr & 0x3fff) >= 0x2c4c && (addr & 0x3fff) <= 0x2cff)
                        break;
#ifndef RELEASE_BUILD
                fatal("Write MGA accel control %08x %02x\n", addr & 0x3fff, val);
#endif
                break;
        }

        if (start_blit)
        {
                mystique_start_blit(mystique);
//                fatal("Start blit\n");
        }
}
static void mystique_ctrl_write_b(uint32_t addr, uint8_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        svga_t* svga = &mystique->svga;

//        pclog("mystique addr=%04x val=%02x\n", addr & 0x3fff, val);

        if (mystique->type == MGA_2064W && (addr & 0x3e00) == 0x3c00)
        {
                /*RAMDAC*/
                tvp3026_ramdac_out(addr & 0xf, val, &mystique->tvp3026, svga);
                return;
        }
        if ((addr & 0x3fff) < 0x1c00)
        {
                mystique_iload_write_b(addr, val, p);
                return;
        }
        if ((addr & 0x3e00) == 0x1c00 || (addr & 0x3e00) == 0x2c00)
        {
                if ((addr & 0x300) == 0x100)
                        mystique->blitter_submit_refcount++;
//                pclog("mystique queue addr=%04x val=%02x\n", addr & 0x3fff, val);
                mystique_queue(mystique, addr & 0x3fff, val, FIFO_WRITE_CTRL_BYTE);
                return;
        }

        switch (addr & 0x3fff)
        {
        case REG_ICLEAR:
//                pclog("ICLEAR %02x\n", val);
                if (val & ICLEAR_SOFTRAPICLR)
                {
                        mystique->status &= ~STATUS_SOFTRAPEN;
                        mystique_update_irqs(mystique);
                }
                if (val & ICLEAR_VLINEICLR)
                {
                        mystique->status &= ~STATUS_VLINEPEN;
                        mystique_update_irqs(mystique);
                }
                break;
        case REG_ICLEAR + 1:
        case REG_ICLEAR + 2:
        case REG_ICLEAR + 3:
                break;

        case REG_IEN:
                mystique->ien = val & 0x65;
//                pclog("Write IEN %02x\n", val);
                break;
        case REG_IEN + 1:
        case REG_IEN + 2:
        case REG_IEN + 3:
                break;

        case REG_OPMODE:
                mystique->dmamod = (val >> 2) & 3;
//                pclog("OPMODE\n");
                mystique_queue(mystique, addr & 0x3fff, val, FIFO_WRITE_CTRL_BYTE);
                break;
        case REG_OPMODE + 1:
                mystique->dmadatasiz = val & 3;
                break;
        case REG_OPMODE + 2:
                mystique->dirdatasiz = val & 3;
                break;
        case REG_OPMODE + 3:
                break;

        case REG_PRIMADDRESS:
        case REG_PRIMADDRESS + 1:
        case REG_PRIMADDRESS + 2:
        case REG_PRIMADDRESS + 3:
                thread_lock_mutex(mystique->dma.lock);
                WRITE8(addr, mystique->dma.primaddress, val);
                mystique->dma.pri_state = 0;
//                pclog("PRIMADDRESS write %04x %08x\n", addr, mystique->dma.primaddress);
                thread_unlock_mutex(mystique->dma.lock);
                break;

        case REG_DMAMAP:
        case REG_DMAMAP + 0x1:
        case REG_DMAMAP + 0x2:
        case REG_DMAMAP + 0x3:
        case REG_DMAMAP + 0x4:
        case REG_DMAMAP + 0x5:
        case REG_DMAMAP + 0x6:
        case REG_DMAMAP + 0x7:
        case REG_DMAMAP + 0x8:
        case REG_DMAMAP + 0x9:
        case REG_DMAMAP + 0xa:
        case REG_DMAMAP + 0xb:
        case REG_DMAMAP + 0xc:
        case REG_DMAMAP + 0xd:
        case REG_DMAMAP + 0xe:
        case REG_DMAMAP + 0xf:
                mystique->dmamap[addr & 0xf] = val;
                break;

        case REG_RST:
        case REG_RST + 1:
        case REG_RST + 2:
        case REG_RST + 3:
//                pclog("Write REG_RST\n");
                wait_fifo_idle(mystique);
                mystique->busy = 0;
                mystique->blitter_submit_refcount = 0;
                mystique->blitter_submit_dma_refcount = 0;
                mystique->blitter_complete_refcount = 0;
                mystique->dwgreg.iload_rem_count = 0;
                mystique->status = STATUS_ENDPRDMASTS;
                break;

        case REG_ATTR_IDX:
                svga_out(0x3c0, val, svga);
                break;
        case REG_ATTR_DATA:
                svga_out(0x3c1, val, svga);
                break;

        case REG_MISC:
                svga_out(0x3c2, val, svga);
                break;

        case REG_SEQ_IDX:
                svga_out(0x3c4, val, svga);
                break;
        case REG_SEQ_DATA:
                svga_out(0x3c5, val, svga);
                break;

        case REG_GCTL_IDX:
                mystique_out(0x3ce, val, mystique);
                break;
        case REG_GCTL_DATA:
                mystique_out(0x3cf, val, mystique);
                break;

        case REG_CRTC_IDX:
                mystique_out(0x3d4, val, mystique);
                break;
        case REG_CRTC_DATA:
                mystique_out(0x3d5, val, mystique);
                break;

        case REG_CRTCEXT_IDX:
                mystique_out(0x3de, val, mystique);
                break;
        case REG_CRTCEXT_DATA:
                mystique_out(0x3df, val, mystique);
                break;

        case REG_CACHEFLUSH:
                break;

        case REG_PALWTADD:
                svga_out(0x3c8, val, svga);
                mystique->xreg_idx = val;
                break;
        case REG_PALDATA:
                svga_out(0x3c9, val, svga);
                break;
        case REG_PIXRDMSK:
                svga_out(0x3c6, val, svga);
                break;
        case REG_PALRDADD:
                svga_out(0x3c7, val, svga);
                break;

        case REG_X_DATAREG:
                mystique_write_xreg(mystique, mystique->xreg_idx, val);
                break;

        case REG_CURPOSX:
        case REG_CURPOSX + 1:
                WRITE8(addr, mystique->cursor.pos_x, val);
                svga->hwcursor.x = mystique->cursor.pos_x - 64;
                break;
        case REG_CURPOSY:
        case REG_CURPOSY + 1:
                WRITE8(addr & 1, mystique->cursor.pos_y, val);
                svga->hwcursor.y = mystique->cursor.pos_y - 64;
                break;

        case 0x1e50:
        case 0x1e51:
        case 0x1e52:
        case 0x1e53:
        case 0x3c0b:
        case 0x3e02:
        case 0x3e08:
                break;

        default:
                if ((addr & 0x3fff) >= 0x2c4c && (addr & 0x3fff) <= 0x2cff)
                        break;
                if ((addr & 0x3fff) >= 0x3e00)
                        break;
#ifndef RELEASE_BUILD
                fatal("Write MGA control %08x %02x\n", addr & 0x3fff, val);
#endif
                break;
        }
}

static uint32_t mystique_ctrl_read_l(uint32_t addr, void* p)
{
        uint32_t ret;

//        pclog("mystique_ctrl_read_l: addr=%08x %08x\n", addr, cpu_state.pc);
        if ((addr & 0x3fff) < 0x1c00)
                return mystique_iload_read_l(addr, p);

        ret = mystique_ctrl_read_b(addr, p);
        ret |= mystique_ctrl_read_b(addr + 1, p) << 8;
        ret |= mystique_ctrl_read_b(addr + 2, p) << 16;
        ret |= mystique_ctrl_read_b(addr + 3, p) << 24;
//        pclog("mcrl: addr=%08x ret=%08x  %08x\n", addr, ret, mystique->status);

        return ret;
}

static void mystique_accel_ctrl_write_l(uint32_t addr, uint32_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        int start_blit = 0;

//        pclog("mystique accel addr=%04x val=%08x\n", addr & 0x3fff, val);
        if ((addr & 0x300) == 0x100)
        {
                addr &= ~0x100;
                start_blit = 1;
        }

        switch (addr & 0x3ffc)
        {
        case REG_DWGCTL:
//                pclog("  dwgctrl write %08x %08x\n", addr2, val);
                mystique->dwgreg.dwgctrl = val;
                break;

        case REG_ZORG:
                mystique->dwgreg.zorg = val;
                mystique->dwgreg.z_base = mystique->dwgreg.ydstorg * 2 + mystique->dwgreg.zorg;
//                pclog("zorg=%08x z_base=%08x\n", mystique->dwgreg.zorg, mystique->dwgreg.z_base);
                break;

        case REG_PLNWT:
                mystique->dwgreg.plnwt = val;
                break;

        case REG_SHIFT:
                mystique->dwgreg.funcnt = val & 0xff;
                mystique->dwgreg.xoff = val & 7;
                mystique->dwgreg.yoff = (val >> 4) & 7;
                mystique->dwgreg.stylelen = (val >> 16) & 0xff;
                break;

        case REG_PITCH:
                mystique->dwgreg.pitch = val & 0xffff;
                if (mystique->dwgreg.pitch & PITCH_YLIN)
                        mystique->dwgreg.ydst_lin = (mystique->dwgreg.ydst << 5) + mystique->dwgreg.ydstorg;
                else
                        mystique->dwgreg.ydst_lin = ((int32_t)(int16_t)mystique->dwgreg.ydst * (mystique->dwgreg.pitch & PITCH_MASK)) + mystique->dwgreg.ydstorg;
//                pclog("pitch=%04x %i\n", mystique->dwgreg.pitch, addr&1);
                break;

        case REG_YDST:
                mystique->dwgreg.ydst = val & 0x3fffff;
                if (mystique->dwgreg.pitch & PITCH_YLIN)
                {
                        mystique->dwgreg.ydst_lin = (mystique->dwgreg.ydst << 5) + mystique->dwgreg.ydstorg;
                        mystique->dwgreg.selline = val >> 29;
                }
                else
                {
                        mystique->dwgreg.ydst_lin = ((int32_t)(int16_t)mystique->dwgreg.ydst * (mystique->dwgreg.pitch & PITCH_MASK)) + mystique->dwgreg.ydstorg;
                        mystique->dwgreg.selline = val & 7;
                }
//                pclog("ydst=%i ydsth=%08x %08x  pitch=%08x\n", mystique->dwgreg.ydst, mystique->dwgreg.ydst, mystique->dwgreg.ydst_lin, mystique->dwgreg.pitch);
                break;

        case REG_BCOL:
                mystique->dwgreg.bcol = val;
//                pclog("BCOL %i %02x\n", addr & 3, val);
                break;
        case REG_FCOL:
                mystique->dwgreg.fcol = val;
//                pclog("FCOL %i %02x\n", addr & 3, val);
                break;

        case REG_SRC0:
                mystique->dwgreg.src[0] = val;
                if (mystique->busy && (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK) == DWGCTRL_OPCODE_ILOAD)
                        blit_iload_write(mystique, mystique->dwgreg.src[0], 32);
                break;
        case REG_SRC1:
                mystique->dwgreg.src[1] = val;
                if (mystique->busy && (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK) == DWGCTRL_OPCODE_ILOAD)
                        blit_iload_write(mystique, mystique->dwgreg.src[1], 32);
                break;
        case REG_SRC2:
                mystique->dwgreg.src[2] = val;
                if (mystique->busy && (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK) == DWGCTRL_OPCODE_ILOAD)
                        blit_iload_write(mystique, mystique->dwgreg.src[2], 32);
                break;
        case REG_SRC3:
                mystique->dwgreg.src[3] = val;
                if (mystique->busy && (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK) == DWGCTRL_OPCODE_ILOAD)
                        blit_iload_write(mystique, mystique->dwgreg.src[3], 32);
                break;

        case REG_DMAPAD:
                if (mystique->busy && (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK) == DWGCTRL_OPCODE_ILOAD)
                        blit_iload_write(mystique, val, 32);
                break;

        case REG_AR0:
                mystique->dwgreg.ar[0] = val;
//                pclog("AR[0]=%08x\n", mystique->dwgreg.ar[0]);
                break;
        case REG_AR1:
                mystique->dwgreg.ar[1] = val;
                break;
        case REG_AR2:
                mystique->dwgreg.ar[2] = val;
                break;
        case REG_AR3:
                mystique->dwgreg.ar[3] = val;
//                pclog("AR[3]=%08x\n", mystique->dwgreg.ar[3]);
                break;
        case REG_AR4:
                mystique->dwgreg.ar[4] = val;
                break;
        case REG_AR5:
                mystique->dwgreg.ar[5] = val;
                break;
        case REG_AR6:
                mystique->dwgreg.ar[6] = val;
                break;

        case REG_DR0:
                mystique->dwgreg.dr[0] = val;
                break;
        case REG_DR2:
                mystique->dwgreg.dr[2] = val;
                break;
        case REG_DR3:
                mystique->dwgreg.dr[3] = val;
                break;
        case REG_DR4:
                mystique->dwgreg.dr[4] = val;
//                if ((addr & 3) == 3) pclog("DR[4]=%08x\n", mystique->dwgreg.dr[4]);
                break;
        case REG_DR6:
                mystique->dwgreg.dr[6] = val;
//                if ((addr & 3) == 3) pclog("DR[6]=%08x\n", mystique->dwgreg.dr[6]);
                break;
        case REG_DR7:
                mystique->dwgreg.dr[7] = val;
//                if ((addr & 3) == 3) pclog("DR[7]=%08x\n", mystique->dwgreg.dr[7]);
                break;
        case REG_DR8:
                mystique->dwgreg.dr[8] = val;
                break;
        case REG_DR10:
                mystique->dwgreg.dr[10] = val;
                break;
        case REG_DR11:
                mystique->dwgreg.dr[11] = val;
                break;
        case REG_DR12:
                mystique->dwgreg.dr[12] = val;
                break;
        case REG_DR14:
                mystique->dwgreg.dr[14] = val;
                break;
        case REG_DR15:
                mystique->dwgreg.dr[15] = val;
                break;

        case REG_SECEND:
//                pclog("SECEND write %08x\n", val);
                mystique->dma.secend = val;
                if (mystique->dma.state != DMA_STATE_SEC && (mystique->dma.secaddress & DMA_ADDR_MASK) != (mystique->dma.secend & DMA_ADDR_MASK))
                {
//                        pclog(" DMA state STATE_SEC\n");
                        mystique->dma.state = DMA_STATE_SEC;
                }
                break;

        case REG_SOFTRAP:
//                pclog("SOFTRAP\n");
/*                mystique->dma.secaddress = val;*/
                mystique->dma.state = DMA_STATE_IDLE;
                mystique->endprdmasts_pending = 1;
                mystique->softrap_pending_val = val;
                mystique->softrap_pending = 1;
//                pclog("SOFTRAP DMA state STATE_IDLE\n");
/*                mystique->status |= STATUS_SOFTRAPEN;
                mystique_update_irqs(mystique);*/
                break;

        default:
                mystique_accel_ctrl_write_b(addr, val & 0xff, p);
                mystique_accel_ctrl_write_b(addr + 1, (val >> 8) & 0xff, p);
                mystique_accel_ctrl_write_b(addr + 2, (val >> 16) & 0xff, p);
                mystique_accel_ctrl_write_b(addr + 3, (val >> 24) & 0xff, p);
                break;
        }

        if (start_blit)
        {
                mystique_start_blit(mystique);
//                fatal("Start blit\n");
        }
}

static void mystique_ctrl_write_l(uint32_t addr, uint32_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        uint32_t reg_addr;

//        pclog("mystique addr=%04x val=%08x\n", addr & 0x3fff, val);
        if ((addr & 0x3fff) < 0x1c00)
        {
                mystique_iload_write_l(addr, val, p);
                return;
        }

        if ((addr & 0x3e00) == 0x1c00 || (addr & 0x3e00) == 0x2c00)
        {
                if ((addr & 0x300) == 0x100)
                        mystique->blitter_submit_refcount++;
//                pclog("mystique queue addr=%04x val=%08x\n", addr & 0x3fff, val);
                mystique_queue(mystique, addr & 0x3fff, val, FIFO_WRITE_CTRL_LONG);
                return;
        }

        switch (addr & 0x3ffc)
        {
        case REG_PRIMEND:
                thread_lock_mutex(mystique->dma.lock);
//                pclog("PRIMEND write %08x\n", val);
                mystique->dma.primend = val;
                if (mystique->dma.state == DMA_STATE_IDLE && (mystique->dma.primaddress & DMA_ADDR_MASK) != (mystique->dma.primend & DMA_ADDR_MASK))
                {
//                        pclog("Clear DMA end flag\n");
                        mystique->endprdmasts_pending = 0;
                        mystique->status &= ~STATUS_ENDPRDMASTS;

                        mystique->dma.state = DMA_STATE_PRI;
//                        pclog(" DMA state STATE_PRI\n");
                        mystique->dma.pri_state = 0;
                        wake_fifo_thread(mystique);
                }
                thread_unlock_mutex(mystique->dma.lock);
                break;

        case REG_DWG_INDIR_WT:
        case REG_DWG_INDIR_WT + 0x04:
        case REG_DWG_INDIR_WT + 0x08:
        case REG_DWG_INDIR_WT + 0x0c:
        case REG_DWG_INDIR_WT + 0x10:
        case REG_DWG_INDIR_WT + 0x14:
        case REG_DWG_INDIR_WT + 0x18:
        case REG_DWG_INDIR_WT + 0x1c:
        case REG_DWG_INDIR_WT + 0x20:
        case REG_DWG_INDIR_WT + 0x24:
        case REG_DWG_INDIR_WT + 0x28:
        case REG_DWG_INDIR_WT + 0x2c:
        case REG_DWG_INDIR_WT + 0x30:
        case REG_DWG_INDIR_WT + 0x34:
        case REG_DWG_INDIR_WT + 0x38:
        case REG_DWG_INDIR_WT + 0x3c:
                reg_addr = (mystique->dmamap[(addr >> 2) & 0xf] & 0x7f) << 2;
                if (mystique->dmamap[(addr >> 2) & 0xf] & 0x80)
                        reg_addr += 0x2c00;
                else
                        reg_addr += 0x1c00;

                if ((reg_addr & 0x300) == 0x100)
                        mystique->blitter_submit_refcount++;

//                pclog("mystique queue dwg addr=%04x val=%08x\n", reg_addr, val);
                mystique_queue(mystique, reg_addr, val, FIFO_WRITE_CTRL_LONG);
                break;

        default:
                mystique_ctrl_write_b(addr, val & 0xff, p);
                mystique_ctrl_write_b(addr + 1, (val >> 8) & 0xff, p);
                mystique_ctrl_write_b(addr + 2, (val >> 16) & 0xff, p);
                mystique_ctrl_write_b(addr + 3, (val >> 24) & 0xff, p);
                break;
        }
}

static uint8_t mystique_iload_read_b(uint32_t addr, void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        wait_fifo_idle(mystique);
        if (!mystique->busy)
        {
//                fatal("mystique_iload_read_b: %08x !busy\n", addr);
                return 0xff;
        }

//        pclog("Readb MGA ILOAD %08x\n", addr);
        return blit_idump_read(mystique);
}
static uint32_t mystique_iload_read_l(uint32_t addr, void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        wait_fifo_idle(mystique);
        if (!mystique->busy)
        {
//                fatal("mystique_iload_read_l: !busy\n");
                return 0xffffffff;
        }

//        pclog("Readl MGA ILOAD %08x  %i\n", addr, mystique->dwgreg.words);
        mystique->dwgreg.words++;
        return blit_idump_read(mystique);
}
static void mystique_iload_write_b(uint32_t addr, uint8_t val, void* p)
{
#ifndef RELEASE_BUILD
        fatal("Write MGA ILOAD %08x %02x\n", addr, val);
#endif
}
static void mystique_iload_write_l(uint32_t addr, uint32_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;

//        pclog("mystique queue iload addr=%04x val=%08x\n", addr, val);
        mystique_queue(mystique, 0, val, FIFO_WRITE_ILOAD_LONG);
}

static void mystique_accel_iload_write_l(uint32_t addr, uint32_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;

//        pclog("  Write ILOAD %08x %i %i  %08x\n", val, mystique->dwgreg.dmamod, mystique->dma.iload_state, mystique->dwgreg.dwgctrl);
        switch (mystique->dwgreg.dmamod)
        {
        case DMA_MODE_REG:
                if (mystique->dma.iload_state == 0)
                {
//                        pclog("   ILOAD header\n");
                        mystique->dma.iload_header = val;
                        mystique->dma.iload_state = 1;
                }
                else
                {
                        uint32_t reg_addr = (mystique->dma.iload_header & 0x7f) << 2;
                        if (mystique->dma.iload_header & 0x80)
                                reg_addr += 0x2c00;
                        else
                                reg_addr += 0x1c00;

//                        pclog("   ILOAD write %08x to %02x %04x\n", val, mystique->dma.iload_header & 0xff, reg_addr);
                        if ((reg_addr & 0x300) == 0x100)
                                mystique->blitter_submit_dma_refcount++;
                        mystique_accel_ctrl_write_l(reg_addr, val, mystique);

                        mystique->dma.iload_header >>= 8;
                        mystique->dma.iload_state = (mystique->dma.iload_state == 4) ? 0 : (mystique->dma.iload_state + 1);
                }
                break;

        case DMA_MODE_BLIT:
                if (mystique->busy)
                        blit_iload_write(mystique, val, 32);
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("ILOAD write DMAMOD %i\n", mystique->dwgreg.dmamod);
#endif
        }
}

static uint8_t mystique_readb_linear(uint32_t addr, void* p)
{
        svga_t* svga = (svga_t*)p;

        egareads++;

        cycles -= video_timing_read_b;
        cycles_lost += video_timing_read_b;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return 0xff;

        return svga->vram[addr & svga->vram_mask];
}
static uint16_t mystique_readw_linear(uint32_t addr, void* p)
{
        svga_t* svga = (svga_t*)p;

        egareads += 2;

        cycles -= video_timing_read_w;
        cycles_lost += video_timing_read_w;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return 0xffff;

        return *(uint16_t*)&svga->vram[addr & svga->vram_mask];
}
static uint32_t mystique_readl_linear(uint32_t addr, void* p)
{
        svga_t* svga = (svga_t*)p;

        egareads += 4;

        cycles -= video_timing_read_l;
        cycles_lost += video_timing_read_l;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return 0xffffffff;

        return *(uint32_t*)&svga->vram[addr & svga->vram_mask];
}

static void mystique_writeb_linear(uint32_t addr, uint8_t val, void* p)
{
        svga_t* svga = (svga_t*)p;

        egawrites++;

        cycles -= video_timing_write_b;
        cycles_lost += video_timing_write_b;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        svga->changedvram[addr >> 12] = changeframecount;
        svga->vram[addr] = val;
}
static void mystique_writew_linear(uint32_t addr, uint16_t val, void* p)
{
        svga_t* svga = (svga_t*)p;

        egawrites += 2;

        cycles -= video_timing_write_w;
        cycles_lost += video_timing_write_w;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        svga->changedvram[addr >> 12] = changeframecount;
        *(uint16_t*)&svga->vram[addr] = val;
}
static void mystique_writel_linear(uint32_t addr, uint32_t val, void* p)
{
        svga_t* svga = (svga_t*)p;

        egawrites += 4;

        cycles -= video_timing_write_l;
        cycles_lost += video_timing_write_l;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        svga->changedvram[addr >> 12] = changeframecount;
        *(uint32_t*)&svga->vram[addr] = val;
}

static uint32_t mystique_dma_read(uint32_t addr)
{
        if ((addr & DMA_ADDR_MASK) > (mem_size * 1024))
        {
//                pclog("PCI DMA out of range\n");
                return -1;
        }
        return *(uint32_t*)&ram[addr & DMA_ADDR_MASK];
}

static void run_dma(mystique_t* mystique)
{
        int words_transferred = 0;

        thread_lock_mutex(mystique->dma.lock);

        if (mystique->dma.state == DMA_STATE_IDLE)
        {
                thread_unlock_mutex(mystique->dma.lock);
                return;
        }

        while (words_transferred < DMA_MAX_WORDS && mystique->dma.state != DMA_STATE_IDLE)
        {
                switch (mystique->dma.state)// != DMA_STATE_IDLE)
                {
                case DMA_STATE_PRI:
                        switch (mystique->dma.primaddress & DMA_MODE_MASK)
                        {
                        case DMA_MODE_REG:
                                if (mystique->dma.pri_state == 0)
                                {
                                        mystique->dma.pri_header = mystique_dma_read(mystique->dma.primaddress);
//                                        pclog("DMA pri read header %08x from %08x  %08x\n", mystique->dma.pri_header, mystique->dma.primaddress, mystique->dma.primend);
                                        mystique->dma.primaddress += 4;
                                }

//                                if ((mystique->dma.pri_header & 0xff) != 0x15)
                                {
                                        uint32_t val = mystique_dma_read(mystique->dma.primaddress);
                                        uint32_t reg_addr;

                                        mystique->dma.primaddress += 4;

                                        reg_addr = (mystique->dma.pri_header & 0x7f) << 2;
                                        if (mystique->dma.pri_header & 0x80)
                                                reg_addr += 0x2c00;
                                        else
                                                reg_addr += 0x1c00;

//                                        pclog(" DMA pri write %08x to %02x %04x  %08x %08x\n", val, mystique->dma.pri_header & 0xff, reg_addr,  (mystique->dma.primaddress & DMA_ADDR_MASK), (mystique->dma.primend & DMA_ADDR_MASK));
                                        if ((reg_addr & 0x300) == 0x100)
                                                mystique->blitter_submit_dma_refcount++;

                                        mystique_accel_ctrl_write_l(reg_addr, val, mystique);
                                }

                                mystique->dma.pri_header >>= 8;
                                mystique->dma.pri_state = (mystique->dma.pri_state + 1) & 3;

                                words_transferred++;
                                if (mystique->dma.state == DMA_STATE_SEC)
                                        mystique->dma.pri_state = 0;
                                else if ((mystique->dma.primaddress & DMA_ADDR_MASK) == (mystique->dma.primend & DMA_ADDR_MASK))
                                {
//                                        pclog("DMA pri end\n");
                                        mystique->endprdmasts_pending = 1;
                                        mystique->dma.state = DMA_STATE_IDLE;
                                }
                                break;

#ifndef RELEASE_BUILD
                        default:
                                fatal("DMA_STATE_PRI: mode %i\n", mystique->dma.primaddress & DMA_MODE_MASK);
#endif
                        }
                        break;

                case DMA_STATE_SEC:
                        switch (mystique->dma.secaddress & DMA_MODE_MASK)
                        {
                        case DMA_MODE_REG:
                                if (mystique->dma.sec_state == 0)
                                {
                                        mystique->dma.sec_header = mystique_dma_read(mystique->dma.secaddress);
//                                        pclog("DMA sec read header %08x from %08x  %08x\n", mystique->dma.sec_header, mystique->dma.secaddress, mystique->dma.secend);
                                        mystique->dma.secaddress += 4;
                                }

//                                if ((mystique->dma.sec_header & 0xff) != 0x15)
                                {
                                        uint32_t val = mystique_dma_read(mystique->dma.secaddress);
                                        uint32_t reg_addr;

                                        mystique->dma.secaddress += 4;

                                        reg_addr = (mystique->dma.sec_header & 0x7f) << 2;
                                        if (mystique->dma.sec_header & 0x80)
                                                reg_addr += 0x2c00;
                                        else
                                                reg_addr += 0x1c00;

//                                        pclog(" DMA sec write %08x to %02x %04x  %08x %08x\n", val, mystique->dma.sec_header & 0xff, reg_addr,  (mystique->dma.secaddress & DMA_ADDR_MASK), (mystique->dma.secend & DMA_ADDR_MASK));
                                        if ((reg_addr & 0x300) == 0x100)
                                                mystique->blitter_submit_dma_refcount++;

                                        mystique_accel_ctrl_write_l(reg_addr, val, mystique);
                                }

                                mystique->dma.sec_header >>= 8;
                                mystique->dma.sec_state = (mystique->dma.sec_state + 1) & 3;

                                words_transferred++;
                                if ((mystique->dma.secaddress & DMA_ADDR_MASK) == (mystique->dma.secend & DMA_ADDR_MASK))
                                {
//                                        pclog("DMA sec end\n");
                                        if ((mystique->dma.primaddress & DMA_ADDR_MASK) == (mystique->dma.primend & DMA_ADDR_MASK))
                                        {
//                                                pclog("DMA pri end\n");
                                                mystique->endprdmasts_pending = 1;
                                                mystique->dma.state = DMA_STATE_IDLE;
                                        }
                                        else
                                        {
//                                                pclog("DMA pri resume\n");
                                                mystique->dma.state = DMA_STATE_PRI;
                                        }
                                }
                                break;

                        case DMA_MODE_BLIT:
                        {
                                uint32_t val = mystique_dma_read(mystique->dma.secaddress);
//                                        pclog(" DMA_MODE_BLIT %08x %08x %08x %08x\n", mystique->dma.secaddress & DMA_ADDR_MASK, mystique->dma.secend & DMA_ADDR_MASK, mystique->dwgreg.dwgctrl_running, mystique->maccess_running);
                                mystique->dma.secaddress += 4;

                                if (mystique->busy)
                                        blit_iload_write(mystique, val, 32);

                                words_transferred++;
                                if ((mystique->dma.secaddress & DMA_ADDR_MASK) == (mystique->dma.secend & DMA_ADDR_MASK))
                                {
//                                                pclog("DMA sec end\n");
                                        if ((mystique->dma.primaddress & DMA_ADDR_MASK) == (mystique->dma.primend & DMA_ADDR_MASK))
                                        {
//                                                        pclog("DMA pri end\n");
                                                mystique->endprdmasts_pending = 1;
                                                mystique->dma.state = DMA_STATE_IDLE;
                                        }
                                        else
                                        {
//                                                        pclog("DMA pri resume\n");
                                                mystique->dma.state = DMA_STATE_PRI;
                                        }
                                }
                        }
                                break;

#ifndef RELEASE_BUILD
                        default:
                                fatal("DMA_STATE_SEC: mode %i\n", mystique->dma.secaddress & DMA_MODE_MASK);
#endif
                        }
                        break;
                }
        }
        thread_unlock_mutex(mystique->dma.lock);
}

static void fifo_thread(void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        while (1)
        {
                thread_set_event(mystique->fifo_not_full_event);
                thread_wait_event(mystique->wake_fifo_thread, -1);
                thread_reset_event(mystique->wake_fifo_thread);

                while (!FIFO_EMPTY || mystique->dma.state != DMA_STATE_IDLE)
                {
                        int words_transferred = 0;

                        while (!FIFO_EMPTY && words_transferred < 100)
                        {
                                fifo_entry_t* fifo = &mystique->fifo[mystique->fifo_read_idx & FIFO_MASK];

                                switch (fifo->addr_type & FIFO_TYPE)
                                {
                                case FIFO_WRITE_CTRL_BYTE:
                                        mystique_accel_ctrl_write_b(fifo->addr_type & FIFO_ADDR, fifo->val, mystique);
                                        break;
                                case FIFO_WRITE_CTRL_LONG:
                                        mystique_accel_ctrl_write_l(fifo->addr_type & FIFO_ADDR, fifo->val, mystique);
                                        break;
                                case FIFO_WRITE_ILOAD_LONG:
                                        mystique_accel_iload_write_l(fifo->addr_type & FIFO_ADDR, fifo->val, mystique);
                                        break;
                                }

                                fifo->addr_type = FIFO_INVALID;
                                mystique->fifo_read_idx++;

                                if (FIFO_ENTRIES > FIFO_THRESHOLD)
                                        thread_set_event(mystique->fifo_not_full_event);

                                words_transferred++;
                        }
                        /*Only run DMA once the FIFO is empty. Required by
                          Screamer 2 / Rally which will incorrectly clip an ILOAD
                          if DMA runs ahead*/
                        if (!words_transferred)
                                run_dma(mystique);
                }
        }
}

static void wake_fifo_thread(mystique_t* mystique)
{
        if (!timer_is_enabled(&mystique->wake_timer))
        {
                /*Don't wake FIFO thread immediately - if we do that it will probably
                  process one word and go back to sleep, requiring it to be woken on
                  almost every write. Instead, wait a short while so that the CPU
                  emulation writes more data so we have more batched-up work.*/
                timer_set_delay_u64(&mystique->wake_timer, WAKE_DELAY);
        }
}

static void wake_fifo_thread_now(mystique_t* mystique)
{
        thread_set_event(mystique->wake_fifo_thread);
}

static void mystique_wake_timer(void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        thread_set_event(mystique->wake_fifo_thread); /*Wake up FIFO thread if moving from idle*/
}

static void wait_fifo_idle(mystique_t* mystique)
{
        while (!FIFO_EMPTY)
        {
                wake_fifo_thread_now(mystique);
                thread_wait_event(mystique->fifo_not_full_event, 1);
        }
}

/*IRQ code (PCI & PIC) is not currently thread safe. SOFTRAP IRQ requests must
  therefore be submitted from the main emulation thread, in this case via a timer
  callback. End-of-DMA status is also deferred here to prevent races between
  SOFTRAP IRQs and code reading the status register. Croc will get into an IRQ
  loop and triple fault if the ENDPRDMASTS flag is seen before the IRQ is taken*/
static void mystique_softrap_pending_timer(void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        timer_advance_u64(&mystique->softrap_pending_timer, TIMER_USEC * 100);

        if (mystique->endprdmasts_pending)
        {
                mystique->endprdmasts_pending = 0;
                mystique->status |= STATUS_ENDPRDMASTS;
        }
        if (mystique->softrap_pending)
        {
                mystique->softrap_pending = 0;

//                pclog("Take SOFTRAP %08x\n", cpu_state.pc);
                mystique->dma.secaddress = mystique->softrap_pending_val;
                mystique->status |= STATUS_SOFTRAPEN;
                mystique_update_irqs(mystique);
        }
}

static void mystique_queue(mystique_t* mystique, uint32_t addr, uint32_t val, uint32_t type)
{
        fifo_entry_t* fifo = &mystique->fifo[mystique->fifo_write_idx & FIFO_MASK];

        if (FIFO_FULL)
        {
                thread_reset_event(mystique->fifo_not_full_event);
                if (FIFO_FULL)
                {
                        thread_wait_event(mystique->fifo_not_full_event, -1); /*Wait for room in ringbuffer*/
                }
        }

        fifo->val = val;
        fifo->addr_type = (addr & FIFO_ADDR) | type;

        mystique->fifo_write_idx++;

        if (FIFO_ENTRIES > FIFO_THRESHOLD || FIFO_ENTRIES < 8)
                wake_fifo_thread(mystique);

//        wait_fifo_idle(mystique);
}

static uint32_t bitop(uint32_t src, uint32_t dst, uint32_t dwgctrl)
{
        switch (dwgctrl & DWGCTRL_BOP_MASK)
        {
        case BOP(0x0):
                return 0;
        case BOP(0x1):
                return ~(dst | src);
        case BOP(0x2):
                return dst & ~src;
        case BOP(0x3):
                return ~src;
        case BOP(0x4):
                return ~dst & src;
        case BOP(0x5):
                return ~dst;
        case BOP(0x6):
                return dst ^ src;
        case BOP(0x7):
                return ~(dst & src);
        case BOP(0x8):
                return dst & src;
        case BOP(0x9):
                return ~(dst ^ src);
        case BOP(0xa):
                return dst;
        case BOP(0xb):
                return dst | ~src;
        case BOP(0xc):
                return src;
        case BOP(0xd):
                return ~dst | src;
        case BOP(0xe):
                return dst | src;
        case BOP(0xf):
                return ~0;
        }
        return 0;
}

static uint16_t dither(mystique_t* mystique, int r, int g, int b, int x, int y)
{
        switch (mystique->dwgreg.dither)
        {
        case DITHER_NONE_555:
                return (b >> 3) | ((g >> 3) << 5) | ((r >> 3) << 10);

        case DITHER_NONE_565:
                return (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);

        case DITHER_555:
                return dither5[b][y][x] | (dither5[g][y][x] << 5) | (dither5[r][y][x] << 10);

        case DITHER_565:
        default:
                return dither5[b][y][x] | (dither6[g][y][x] << 5) | (dither5[r][y][x] << 11);
        }
}

static uint32_t blit_idump_idump(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;
        uint64_t val64 = 0;
        uint32_t val = 0;
        int count = 0;

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BU32RGB:
                case DWGCTRL_BLTMOD_BFCOL:
                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                        {
                        case MACCESS_PWIDTH_8:
//                                pclog(" IDUMP %08x\n", mystique->dwgreg.src_addr);
                                while (count < 32)
                                {
                                        val |= (svga->vram[mystique->dwgreg.src_addr & mystique->vram_mask] << count);

                                        if (mystique->dwgreg.src_addr == mystique->dwgreg.ar[0])
                                        {
                                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.src_addr = mystique->dwgreg.ar[3];
                                        }
                                        else
                                                mystique->dwgreg.src_addr++;

                                        if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                        {
                                                mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                                mystique->dwgreg.length_cur--;
                                                if (!mystique->dwgreg.length_cur)
                                                {
                                                        mystique->busy = 0;
                                                        mystique->blitter_complete_refcount++;
                                                        break;
                                                }
                                                break;
                                        }
                                        else
                                                mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;

                                        count += 8;
                                }
                                break;

                        case MACCESS_PWIDTH_16:
                                //                              pclog(" IDUMP %08x\n", mystique->dwgreg.src_addr);
                                while (count < 32)
                                {
                                        val |= (((uint16_t*)svga->vram)[mystique->dwgreg.src_addr & mystique->vram_mask_w] << count);

                                        if (mystique->dwgreg.src_addr == mystique->dwgreg.ar[0])
                                        {
                                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.src_addr = mystique->dwgreg.ar[3];
                                        }
                                        else
                                                mystique->dwgreg.src_addr++;

                                        if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                        {
                                                mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                                mystique->dwgreg.length_cur--;
                                                if (!mystique->dwgreg.length_cur)
                                                {
                                                        mystique->busy = 0;
                                                        mystique->blitter_complete_refcount++;
                                                        break;
                                                }
                                                break;
                                        }
                                        else
                                                mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;

                                        count += 16;
                                }
                                break;

                        case MACCESS_PWIDTH_24:
                                if (mystique->dwgreg.idump_end_of_line)
                                {
                                        mystique->dwgreg.idump_end_of_line = 0;
                                        val = mystique->dwgreg.iload_rem_data;
                                        mystique->dwgreg.iload_rem_count = 0;
                                        mystique->dwgreg.iload_rem_data = 0;
                                        if (!mystique->dwgreg.length_cur)
                                        {
                                                mystique->busy = 0;
                                                mystique->blitter_complete_refcount++;
                                        }
                                        break;
                                }

                                count += mystique->dwgreg.iload_rem_count;
                                val64 = mystique->dwgreg.iload_rem_data;

//                                pclog(" IDUMP %08x\n", mystique->dwgreg.src_addr);
                                while ((count < 32) && !mystique->dwgreg.idump_end_of_line)
                                {
                                        val64 |= (uint64_t)((*(uint32_t*)&svga->vram[(mystique->dwgreg.src_addr * 3) & mystique->vram_mask]) & 0xffffff) << count;
/*                                        pclog("   %i %i %i %016llx\n", count, mystique->dwgreg.xdst,
                                                        mystique->dwgreg.length_cur, val64);*/

                                        if (mystique->dwgreg.src_addr == mystique->dwgreg.ar[0])
                                        {
                                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.src_addr = mystique->dwgreg.ar[3];
                                        }
                                        else
                                                mystique->dwgreg.src_addr++;

                                        if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                        {
                                                mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                                mystique->dwgreg.length_cur--;
                                                if (!mystique->dwgreg.length_cur)
                                                {
                                                        if (count > 8)
                                                                mystique->dwgreg.idump_end_of_line = 1;
                                                        else
                                                        {
                                                                count = 32;
                                                                mystique->busy = 0;
                                                                mystique->blitter_complete_refcount++;
                                                        }
                                                        break;
                                                }
                                                if (!(mystique->dwgreg.dwgctrl_running & DWGCTRL_LINEAR))
                                                {
                                                        if (count > 8)
                                                                mystique->dwgreg.idump_end_of_line = 1;
                                                        else
                                                        {
                                                                count = 32;
                                                                break;
                                                        }
                                                }
                                        }
                                        else
                                                mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;

                                        count += 24;
                                }
                                if (count > 32)
                                        mystique->dwgreg.iload_rem_count = count - 32;
                                else
                                        mystique->dwgreg.iload_rem_count = 0;
                                mystique->dwgreg.iload_rem_data = (uint32_t)(val64 >> 32);
                                val = val64 & 0xffffffff;
                                break;

                        case MACCESS_PWIDTH_32:
//                                pclog(" IDUMP %08x\n", mystique->dwgreg.src_addr);

                                val = (((uint32_t*)svga->vram)[mystique->dwgreg.src_addr & mystique->vram_mask_l] << count);

                                if (mystique->dwgreg.src_addr == mystique->dwgreg.ar[0])
                                {
                                        mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                        mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                        mystique->dwgreg.src_addr = mystique->dwgreg.ar[3];
                                }
                                else
                                        mystique->dwgreg.src_addr++;

                                if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                {
                                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                        mystique->dwgreg.length_cur--;
                                        if (!mystique->dwgreg.length_cur)
                                        {
                                                mystique->busy = 0;
                                                mystique->blitter_complete_refcount++;
                                                break;
                                        }
                                        break;
                                }
                                else
                                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                                break;

#ifndef RELEASE_BUILD
                        default:
                                fatal("IDUMP DWGCTRL_BLTMOD_BU32RGB %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->maccess_running);
#endif
                        }
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("IDUMP DWGCTRL_ATYPE_RPL %08x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK, mystique->dwgreg.dwgctrl_running);
                        break;
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown IDUMP atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }

        return val;
}

static uint32_t blit_idump_read(mystique_t* mystique)
{
        uint32_t ret = 0xffffffff;

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK)
        {
        case DWGCTRL_OPCODE_IDUMP:
                ret = blit_idump_idump(mystique);
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("blit_idump_read: bad opcode %08x\n", mystique->dwgreg.dwgctrl_running);
#endif
        }

//        pclog("  idump returned %08x\n", ret);
        return ret;
}

static void blit_iload_iload(mystique_t* mystique, uint32_t data, int size)
{
        svga_t* svga = &mystique->svga;
        uint32_t src, dst;
        uint64_t data64;
        int min_size;
        uint32_t bltckey = mystique->dwgreg.fcol, bltcmsk = mystique->dwgreg.bcol;
        const int transc = mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC;
        const int trans_sel = (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANS_MASK) >> DWGCTRL_TRANS_SHIFT;
        uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
        uint32_t data_mask = 1;

        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
        {
        case MACCESS_PWIDTH_8:
                bltckey &= 0xff;
                bltcmsk &= 0xff;
                break;
        case MACCESS_PWIDTH_16:
                bltckey &= 0xffff;
                bltcmsk &= 0xffff;
                break;
        }

        mystique->dwgreg.words++;
//        pclog("blit_iload_iload: %08x %06x %02x %i  %08x  words=%i %08x  %08x\n", data, mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst, data, (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK) == DWGCTRL_BLTMOD_BMONOWF, mystique->dwgreg.dwgctrl_running, mystique->dwgreg.words,mystique->dwgreg.words, mystique->maccess_running);
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
                if (mystique->maccess_running & MACCESS_TLUTLOAD)
                {
                        while ((mystique->dwgreg.length_cur > 0) && (size >= 16))
                        {
                                uint16_t src = data & 0xffff;
//                                pclog("  LUT[%02x]=%04x\n", mystique->dwgreg.ydst & 0xff, src);

                                mystique->lut[mystique->dwgreg.ydst & 0xff].r = (src >> 11) << 3;
                                mystique->lut[mystique->dwgreg.ydst & 0xff].g = ((src >> 5) & 0x3f) << 2;
                                mystique->lut[mystique->dwgreg.ydst & 0xff].b = (src & 0x1f) << 3;
                                mystique->dwgreg.ydst++;
                                mystique->dwgreg.length_cur--;
                                data >>= 16;
                                size -= 16;
                        }

                        if (!mystique->dwgreg.length_cur)
                        {
                                mystique->busy = 0;
                                mystique->blitter_complete_refcount++;
                        }
                        break;
                }
        case DWGCTRL_ATYPE_RSTR:
        case DWGCTRL_ATYPE_BLK:
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BFCOL:
                        size += mystique->dwgreg.iload_rem_count;
//                        pclog("   size=%i xdst=%i cur=%i data= %08x %08x  ", size, mystique->dwgreg.xdst, mystique->dwgreg.length_cur, mystique->dwgreg.iload_rem_data, data);
                        data64 = mystique->dwgreg.iload_rem_data | ((uint64_t)data << mystique->dwgreg.iload_rem_count);
//                        pclog("%016llx  words=%i\n", data64, mystique->dwgreg.words);

                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                        {
                        case MACCESS_PWIDTH_8:
                                min_size = 8;
                                break;
                        case MACCESS_PWIDTH_16:
                                min_size = 16;
                                break;
                        case MACCESS_PWIDTH_24:
                                min_size = 24;
                                break;
                        case MACCESS_PWIDTH_32:
                                min_size = 32;
                                break;
                        }

                        while (size >= min_size)
                        {
                                int draw = (!transc || (data & bltcmsk) != bltckey) && trans[mystique->dwgreg.xdst & 3];

                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                {
                                case MACCESS_PWIDTH_8:
                                        if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot && draw)
                                        {
                                                dst = svga->vram[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask];

                                                dst = bitop(data & 0xff, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     8 %06x=%04x\n", mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst, dst);
                                                svga->vram[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask) >> 12] = changeframecount;
                                        }
/*                                        else
                                                pclog("  Clipped  %i,%i %i  %08x,%08x %08x\n",
                                                        mystique->dwgreg.cxleft, mystique->dwgreg.cxright, mystique->dwgreg.xdst,
                                                        mystique->dwgreg.ytop, mystique->dwgreg.ybot, mystique->dwgreg.ydst_lin);*/

                                        data >>= 8;
                                        size -= 8;
                                        break;

                                case MACCESS_PWIDTH_16:
                                        if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot && draw)
                                        {
                                                dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w];

                                                dst = bitop(data & 0xffff, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     16 %06x=%04x\n", mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst, dst);
                                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w) >> 11] = changeframecount;
                                        }

                                        data >>= 16;
                                        size -= 16;
                                        break;

                                case MACCESS_PWIDTH_24:
                                        if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                                        {
                                                uint32_t old_dst = *((uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) * 3) & mystique->vram_mask]);

                                                dst = bitop(data64, old_dst, mystique->dwgreg.dwgctrl_running);
                                                *((uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) * 3) & mystique->vram_mask]) = (dst & 0xffffff) | (old_dst & 0xff000000);
                                                svga->changedvram[(((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                        }

                                        data64 >>= 24;
                                        size -= 24;
                                        break;

                                case MACCESS_PWIDTH_32:
                                        if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot && draw)
                                        {
                                                dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l];

                                                dst = bitop(data, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     32 %06x=%04x\n", mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst, dst);
                                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l) >> 10] = changeframecount;
                                        }

                                        size = 0;
                                        break;

#ifndef RELEASE_BUILD
                                default:
                                        fatal("ILOAD RSTR/RPL BFCOL pwidth %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
                                }

                                if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                {
                                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                                        mystique->dwgreg.selline = (mystique->dwgreg.selline + 1) & 7;
                                        mystique->dwgreg.length_cur--;
                                        if (!mystique->dwgreg.length_cur)
                                        {
                                                mystique->busy = 0;
                                                mystique->blitter_complete_refcount++;
                                                break;
                                        }
                                        data64 = 0;
                                        size = 0;
                                        break;
                                }
                                else
                                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                        }
                        mystique->dwgreg.iload_rem_count = size;
                        mystique->dwgreg.iload_rem_data = data64;
                        break;

                case DWGCTRL_BLTMOD_BMONOWF:
                        data = (data >> 24) | ((data & 0x00ff0000) >> 8) | ((data & 0x0000ff00) << 8) | (data << 24);
                        data_mask = (1 << 31);
                case DWGCTRL_BLTMOD_BMONOLEF:
                        while (size)
                        {
                                if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                    ((data & data_mask) || !(mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC)) &&
                                    trans[mystique->dwgreg.xdst & 3])
                                {
                                        uint32_t old_dst;

                                        src = (data & data_mask) ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                        {
                                        case MACCESS_PWIDTH_8:
                                                dst = svga->vram[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask];

//                                                pclog("   %02x %02x %06x ", src, dst, mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst);
                                                dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("%02x\n", dst);

                                                svga->vram[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask) >> 12] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_16:
                                                dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w];

//                                                pclog("   %02x %02x %06x ", src, dst, mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst);
                                                dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("%02x\n", dst);

                                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_24:
                                                old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) * 3) & mystique->vram_mask];

//                                                pclog("   %02x %02x %06x ", src, old_dst, mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst);
                                                dst = bitop(src, old_dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("%02x   %08x %08x %08x  %i\n", dst, data, mystique->dwgreg.fcol, mystique->dwgreg.bcol, mystique->dwgreg.xdst);

                                                *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) * 3) & mystique->vram_mask] = (dst & 0xffffff) | (old_dst & 0xff000000);
                                                svga->changedvram[(((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_32:
                                                dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l];

//                                                pclog("   %02x %02x %06x ", src, dst, mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst);
                                                dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("%02x   %08x %08x %08x  %i\n", dst, data, mystique->dwgreg.fcol, mystique->dwgreg.bcol, mystique->dwgreg.xdst);

                                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                break;

#ifndef RELEASE_BUILD
                                        default:
                                                fatal("ILOAD RSTR/RPL BMONOWF pwidth %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
                                        }
                                }

                                if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                {
                                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                                        mystique->dwgreg.length_cur--;
                                        if (!mystique->dwgreg.length_cur)
                                        {
                                                mystique->busy = 0;
                                                mystique->blitter_complete_refcount++;
                                                break;
                                        }
                                        if (!(mystique->dwgreg.dwgctrl_running & DWGCTRL_LINEAR))
                                                break;
                                }
                                else
                                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                                if (data_mask == 1)
                                        data >>= 1;
                                else
                                        data <<= 1;
                                size--;
                        }
                        break;

                case DWGCTRL_BLTMOD_BU24RGB:
                        size += mystique->dwgreg.iload_rem_count;
//                        pclog("   size=%i data= %08x %08x  ", size, mystique->dwgreg.iload_rem_data, data);
                        data64 = mystique->dwgreg.iload_rem_data | ((uint64_t)data << mystique->dwgreg.iload_rem_count);
//                        pclog("%016llx\n", data64);

                        while (size >= 24)
                        {
                                if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                                {
                                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                        {
                                        case MACCESS_PWIDTH_32:
                                                dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l];

//                                                pclog("   %06llx %02x %06x   ", data64 & 0xffffff, dst, mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst);
//                                                pclog("x=%i l=%i  ", mystique->dwgreg.xdst, mystique->dwgreg.length_cur);
                                                dst = bitop(data64 & 0xffffff, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("%02x\n", dst);

                                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                break;

#ifndef RELEASE_BUILD
                                        default:
                                                fatal("ILOAD RSTR/RPL BU24RGB pwidth %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
                                        }
                                }

                                data64 >>= 24;
                                size -= 24;
                                if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                {
                                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                                        mystique->dwgreg.length_cur--;
                                        if (!mystique->dwgreg.length_cur)
                                        {
                                                mystique->busy = 0;
                                                mystique->blitter_complete_refcount++;
                                                break;
                                        }
                                        data64 = 0;
                                        size = 0;
                                        break;
                                }
                                else
                                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                        }

                        mystique->dwgreg.iload_rem_count = size;
                        mystique->dwgreg.iload_rem_data = data64;
                        break;

                case DWGCTRL_BLTMOD_BU32RGB:
                        size += mystique->dwgreg.iload_rem_count;
//                        pclog("   size=%i xdst=%i cur=%i data= %08x %08x  ", size, mystique->dwgreg.xdst, mystique->dwgreg.length_cur, mystique->dwgreg.iload_rem_data, data);
                        data64 = mystique->dwgreg.iload_rem_data | ((uint64_t)data << mystique->dwgreg.iload_rem_count);
//                        pclog("%016llx  words=%i\n", data64, mystique->dwgreg.words);

                        while (size >= 32)
                        {
                                int draw = (!transc || (data & bltcmsk) != bltckey) && trans[mystique->dwgreg.xdst & 3];

                                if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot && draw)
                                {
                                        dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l];

                                        dst = bitop(data, dst, mystique->dwgreg.dwgctrl_running);
                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l] = dst;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l) >> 10] = changeframecount;
                                }

                                size = 0;

                                if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                                {
                                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                                        mystique->dwgreg.selline = (mystique->dwgreg.selline + 1) & 7;
                                        mystique->dwgreg.length_cur--;
                                        if (!mystique->dwgreg.length_cur)
                                        {
                                                mystique->busy = 0;
                                                mystique->blitter_complete_refcount++;
                                                break;
                                        }
                                        data64 = 0;
                                        size = 0;
                                        break;
                                }
                                else
                                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                        }
                        mystique->dwgreg.iload_rem_count = size;
                        mystique->dwgreg.iload_rem_data = data64;
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("ILOAD DWGCTRL_ATYPE_RPL\n");
                        break;
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown ILOAD iload atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
}

#define CLAMP(x) do                                     \
        {                                               \
                if ((x) & ~0xff)                        \
                        x = ((x) < 0) ? 0 : 0xff;       \
        }                               \
        while (0)

static void blit_iload_iload_scale(mystique_t* mystique, uint32_t data, int size)
{
        svga_t* svga = &mystique->svga;
        uint64_t data64 = 0;
        int y0, y1;
        int u, v;
        int dR, dG, dB;
        int r0, g0, b0;
        int r1, g1, b1;

//        pclog(" iload_scale: data=%08x\n", data);

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
        {
        case DWGCTRL_BLTMOD_BUYUV:
                y0 = (298 * ((int)(data & 0xff) - 16)) >> 8;
                u = ((data >> 8) & 0xff) - 0x80;
                y1 = (298 * ((int)((data >> 16) & 0xff) - 16)) >> 8;
                v = ((data >> 24) & 0xff) - 0x80;

                dR = (309 * v) >> 8;
                dG = (100 * u + 208 * v) >> 8;
                dB = (516 * u) >> 8;

                r0 = y0 + dR;
                CLAMP(r0);
                g0 = y0 - dG;
                CLAMP(g0);
                b0 = y0 + dB;
                CLAMP(b0);
                r1 = y1 + dR;
                CLAMP(r1);
                g1 = y1 - dG;
                CLAMP(g1);
                b1 = y1 + dB;
                CLAMP(b1);

                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                {
                case MACCESS_PWIDTH_16:
                        data = (b0 >> 3) | ((g0 >> 2) << 5) | ((r0 >> 3) << 11);
                        data |= (((b1 >> 3) | ((g1 >> 2) << 5) | ((r1 >> 3) << 11)) << 16);
                        size = 32;
                        break;
                case MACCESS_PWIDTH_32:
                        data64 = b0 | (g0 << 8) | (r0 << 16);
                        data64 |= ((uint64_t)b0 << 32) | ((uint64_t)g0 << 40) | ((uint64_t)r0 << 48);
                        size = 64;
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("blit_iload_iload_scale BUYUV pwidth %i\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("blit_iload_iload_scale bltmod %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK);
                break;
#endif
        }

        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
        {
        case MACCESS_PWIDTH_16:
                while (size >= 16)
                {
//                pclog("  Write %i %i %08x\n", mystique->dwgreg.xdst, mystique->dwgreg.fxright, mystique->dwgreg.ar[6]);
                        if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                        {
                                uint16_t dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w];
                                dst = bitop(data & 0xffff, dst, mystique->dwgreg.dwgctrl_running);
                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w] = dst;
                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w) >> 11] = changeframecount;
                        }

                        mystique->dwgreg.ar[6] += mystique->dwgreg.ar[2];
                        if ((int32_t)mystique->dwgreg.ar[6] >= 0)
                        {
                                mystique->dwgreg.ar[6] -= (mystique->dwgreg.fxright - mystique->dwgreg.fxleft);
                                data >>= 16;
                                size -= 16;
                        }

                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                        if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                        {
                                mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                mystique->dwgreg.ar[6] = mystique->dwgreg.ar[2] - (mystique->dwgreg.fxright - mystique->dwgreg.fxleft);
                                mystique->dwgreg.length_cur--;
                                if (!mystique->dwgreg.length_cur)
                                {
                                        mystique->busy = 0;
                                        mystique->blitter_complete_refcount++;
                                        break;
                                }
                                break;
                        }
                }
                break;

        case MACCESS_PWIDTH_32:
                while (size >= 32)
                {
//                pclog("  Write %i %i %08x\n", mystique->dwgreg.xdst, mystique->dwgreg.fxright, mystique->dwgreg.ar[6]);
                        if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                        {
                                uint32_t dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l];
                                dst = bitop(data64, dst, mystique->dwgreg.dwgctrl_running);
                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l] = dst;
                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l) >> 10] = changeframecount;
                        }

                        mystique->dwgreg.ar[6] += mystique->dwgreg.ar[2];
                        if ((int32_t)mystique->dwgreg.ar[6] >= 0)
                        {
                                mystique->dwgreg.ar[6] -= (mystique->dwgreg.fxright - mystique->dwgreg.fxleft);
                                data64 >>= 32;
                                size -= 32;
                        }

                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
                        if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                        {
                                mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                                mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                mystique->dwgreg.ar[6] = mystique->dwgreg.ar[2] - (mystique->dwgreg.fxright - mystique->dwgreg.fxleft);
                                mystique->dwgreg.length_cur--;
                                if (!mystique->dwgreg.length_cur)
                                {
                                        mystique->busy = 0;
                                        mystique->blitter_complete_refcount++;
                                        break;
                                }
                                break;
                        }
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("ILOAD_SCALE pwidth %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
        }
}

static void blit_iload_iload_high(mystique_t* mystique, uint32_t data, int size)
{
        svga_t* svga = &mystique->svga;
        uint32_t out_data;
        int y0, y1, u, v;
        int dR, dG, dB;
        int r = 0, g = 0, b = 0;
        int next_r = 0, next_g = 0, next_b = 0;

//        pclog(" iload_high: data=%08x\n", data);

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
        {
        case DWGCTRL_BLTMOD_BUYUV:
                y0 = (298 * ((int)(data & 0xff) - 16)) >> 8;
                u = ((data >> 8) & 0xff) - 0x80;
                y1 = (298 * ((int)((data >> 16) & 0xff) - 16)) >> 8;
                v = ((data >> 24) & 0xff) - 0x80;

                dR = (309 * v) >> 8;
                dG = (100 * u + 208 * v) >> 8;
                dB = (516 * u) >> 8;

                r = y0 + dR;
                CLAMP(r);
                g = y0 - dG;
                CLAMP(g);
                b = y0 + dB;
                CLAMP(b);

                next_r = y1 + dR;
                CLAMP(next_r);
                next_g = y1 - dG;
                CLAMP(next_g);
                next_b = y1 + dB;
                CLAMP(next_b);

                size = 32;
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("blit_iload_iload_high bltmod %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK);
                break;
#endif
        }

        while (size >= 16)
        {
                if (mystique->dwgreg.xdst >= mystique->dwgreg.cxleft && mystique->dwgreg.xdst <= mystique->dwgreg.cxright &&
                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                {
                        uint32_t dst;
                        int f1 = (mystique->dwgreg.ar[6] >> 12) & 0xf;
                        int f0 = 0x10 - f1;
                        int out_r = ((mystique->dwgreg.lastpix_r * f0) + (r * f1)) >> 4;
                        int out_g = ((mystique->dwgreg.lastpix_g * f0) + (g * f1)) >> 4;
                        int out_b = ((mystique->dwgreg.lastpix_b * f0) + (b * f1)) >> 4;

//                        pclog("  Write %i %i %08x  %i %i  %i %i  %i\n", mystique->dwgreg.xdst, mystique->dwgreg.fxright, mystique->dwgreg.ar[6], mystique->dwgreg.lastpix_r, r, f0, f1, out_r);

                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                        {
                        case MACCESS_PWIDTH_16:
                                out_data = (out_b >> 3) | ((out_g >> 2) << 5) | ((out_r >> 3) << 11);
                                dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w];
                                dst = bitop(out_data, dst, mystique->dwgreg.dwgctrl_running);
                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w] = dst;
                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_w) >> 11] = changeframecount;
                                break;
                        case MACCESS_PWIDTH_32:
                                out_data = out_b | (out_g << 8) | (out_r << 16);
                                dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l];
                                dst = bitop(out_data, dst, mystique->dwgreg.dwgctrl_running);
                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l] = dst;
                                svga->changedvram[((mystique->dwgreg.ydst_lin + mystique->dwgreg.xdst) & mystique->vram_mask_l) >> 10] = changeframecount;
                                break;

#ifndef RELEASE_BUILD
                        default:
                                fatal("ILOAD_SCALE_HIGH RSTR/RPL BUYUV pwidth %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
                        }
                }

                mystique->dwgreg.ar[6] += mystique->dwgreg.ar[2];
                if ((int32_t)mystique->dwgreg.ar[6] >= 0)
                {
                        mystique->dwgreg.ar[6] -= 65536;
                        size -= 16;

                        mystique->dwgreg.lastpix_r = r;
                        mystique->dwgreg.lastpix_g = g;
                        mystique->dwgreg.lastpix_b = b;
                        r = next_r;
                        g = next_g;
                        b = next_b;
                }

                if (mystique->dwgreg.xdst == mystique->dwgreg.fxright)
                {
                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                        mystique->dwgreg.ar[6] = mystique->dwgreg.ar[2] - (mystique->dwgreg.fxright - mystique->dwgreg.fxleft);
                        mystique->dwgreg.lastpix_r = 0;
                        mystique->dwgreg.lastpix_g = 0;
                        mystique->dwgreg.lastpix_b = 0;

                        mystique->dwgreg.length_cur--;
                        if (!mystique->dwgreg.length_cur)
                        {
                                mystique->busy = 0;
                                mystique->blitter_complete_refcount++;
                                break;
                        }
                        break;
                }
                else
                        mystique->dwgreg.xdst = (mystique->dwgreg.xdst + 1) & 0xffff;
        }
}

static void blit_iload_iload_highv(mystique_t* mystique, uint32_t data, int size)
{
        uint8_t* src0, * src1;

//        pclog(" iload_highv: data=%08x\n", data);

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
        {
        case DWGCTRL_BLTMOD_BUYUV:
                if (!mystique->dwgreg.highv_line)
                {
                        mystique->dwgreg.highv_data = data;
                        mystique->dwgreg.highv_line = 1;
                        return;
                }
                mystique->dwgreg.highv_line = 0;

                src0 = (uint8_t*)&mystique->dwgreg.highv_data;
                src1 = (uint8_t*)&data;

                src1[0] = ((src0[0] * mystique->dwgreg.beta) + (src1[0] * (16 - mystique->dwgreg.beta))) >> 4;
                src1[1] = ((src0[1] * mystique->dwgreg.beta) + (src1[1] * (16 - mystique->dwgreg.beta))) >> 4;
                src1[2] = ((src0[2] * mystique->dwgreg.beta) + (src1[2] * (16 - mystique->dwgreg.beta))) >> 4;
                src1[3] = ((src0[3] * mystique->dwgreg.beta) + (src1[3] * (16 - mystique->dwgreg.beta))) >> 4;
                blit_iload_iload_high(mystique, data, 32);
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("blit_iload_iload_highv bltmod %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK);
                break;
#endif
        }
}

static void blit_iload_write(mystique_t* mystique, uint32_t data, int size)
{
//        pclog("  ILOAD %08x %08x\n", data, mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK);
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK)
        {
        case DWGCTRL_OPCODE_ILOAD:
                blit_iload_iload(mystique, data, size);
                break;

        case DWGCTRL_OPCODE_ILOAD_SCALE:
                blit_iload_iload_scale(mystique, data, size);
                break;

        case DWGCTRL_OPCODE_ILOAD_HIGH:
                blit_iload_iload_high(mystique, data, size);
                break;

        case DWGCTRL_OPCODE_ILOAD_HIGHV:
                blit_iload_iload_highv(mystique, data, size);
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("blit_iload_write: bad opcode %08x\n", mystique->dwgreg.dwgctrl_running);
#endif
        }
}

static int z_check(uint16_t z, uint16_t old_z, uint32_t z_mode)//mystique->dwgreg.dwgctrl & DWGCTRL_ZMODE_MASK)
{
        switch (z_mode)
        {
        case DWGCTRL_ZMODE_ZE:
                return (z == old_z);
        case DWGCTRL_ZMODE_ZNE:
                return (z != old_z);
        case DWGCTRL_ZMODE_ZLT:
                return (z < old_z);
        case DWGCTRL_ZMODE_ZLTE:
                return (z <= old_z);
        case DWGCTRL_ZMODE_ZGT:
                return (z > old_z);
        case DWGCTRL_ZMODE_ZGTE:
                return (z >= old_z);

        case DWGCTRL_ZMODE_NOZCMP:
        default:
                return 1;
        }
}

static void blit_line(mystique_t* mystique, int closed)
{
        svga_t* svga = &mystique->svga;
        uint32_t src, dst, old_dst;
        int x;
        int z_write;

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RSTR:
        case DWGCTRL_ATYPE_RPL:
                x = mystique->dwgreg.xdst;
                while (mystique->dwgreg.length > 0)
                {
                        if (x >= mystique->dwgreg.cxleft && x <= mystique->dwgreg.cxright &&
                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                        {
                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                {
                                case MACCESS_PWIDTH_8:
                                        src = mystique->dwgreg.fcol;
                                        dst = svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask];

//                                        pclog("    LINE %i %08x  %08x\n", x, mystique->dwgreg.ydst_lin, mystique->dwgreg.ar[1]);
                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
                                        svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask] = dst;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask) >> 12] = changeframecount;
                                        break;

                                case MACCESS_PWIDTH_16:
                                        src = mystique->dwgreg.fcol;
                                        dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w];

//                                        pclog("    LINE %i %08x  %08x\n", x, mystique->dwgreg.ydst_lin, mystique->dwgreg.ar[1]);
                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] = dst;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 11] = changeframecount;
                                        break;

                                case MACCESS_PWIDTH_24:
                                        src = mystique->dwgreg.fcol;
                                        old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask];

//                                        pclog("    LINE %i %08x  %08x\n", x, mystique->dwgreg.ydst_lin, mystique->dwgreg.ar[1]);
                                        dst = bitop(src, old_dst, mystique->dwgreg.dwgctrl_running);
                                        *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask] = (dst & 0xffffff) | (old_dst & 0xff000000);
                                        svga->changedvram[(((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                        break;

                                case MACCESS_PWIDTH_32:
                                        src = mystique->dwgreg.fcol;
                                        dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l];

//                                        pclog("    LINE %i %08x  %08x\n", x, mystique->dwgreg.ydst_lin, mystique->dwgreg.ar[1]);
                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l] = dst;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l) >> 10] = changeframecount;
                                        break;

#ifndef RELEASE_BUILD
                                default:
                                        fatal("LINE RSTR/RPL PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                }
                        }

                        if (mystique->dwgreg.sgn.sdydxl)
                                x += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                        else
                                mystique->dwgreg.ydst_lin += (mystique->dwgreg.sgn.sdy ? -(mystique->dwgreg.pitch & PITCH_MASK) : (mystique->dwgreg.pitch & PITCH_MASK));

                        if ((int32_t)mystique->dwgreg.ar[1] >= 0)
                        {
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[2];

                                if (mystique->dwgreg.sgn.sdydxl)
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.sgn.sdy ? -(mystique->dwgreg.pitch & PITCH_MASK) : (mystique->dwgreg.pitch & PITCH_MASK));
                                else
                                        x += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                        }
                        else
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[0];

                        mystique->dwgreg.length--;
                }
                break;

        case DWGCTRL_ATYPE_I:
        case DWGCTRL_ATYPE_ZI:
                z_write = ((mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK) == DWGCTRL_ATYPE_ZI);
                x = mystique->dwgreg.xdst;
                while (mystique->dwgreg.length > 0)
                {
                        if (x >= mystique->dwgreg.cxleft && x <= mystique->dwgreg.cxright &&
                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                        {
                                uint16_t z = ((int32_t)mystique->dwgreg.dr[0] < 0) ? 0 : (mystique->dwgreg.dr[0] >> 15);
                                uint16_t* z_p = (uint16_t*)&svga->vram[(mystique->dwgreg.ydst_lin * 2 + mystique->dwgreg.zorg) & mystique->vram_mask];
                                uint16_t old_z = z_p[x];

                                if (z_check(z, old_z, mystique->dwgreg.dwgctrl_running & DWGCTRL_ZMODE_MASK))
                                {
                                        int r = 0, g = 0, b = 0;

                                        if (z_write)
                                                z_p[x] = z;

                                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                        {
                                        case MACCESS_PWIDTH_16:
                                                if (!(mystique->dwgreg.dr[4] & (1 << 23)))
                                                        r = (mystique->dwgreg.dr[4] >> 18) & 0x1f;
                                                if (!(mystique->dwgreg.dr[8] & (1 << 23)))
                                                        g = (mystique->dwgreg.dr[8] >> 17) & 0x3f;
                                                if (!(mystique->dwgreg.dr[12] & (1 << 23)))
                                                        b = (mystique->dwgreg.dr[12] >> 18) & 0x1f;
                                                dst = (r << 11) | (g << 5) | b;

                                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                break;

#ifndef RELEASE_BUILD
                                        default:
                                                fatal("LINE I/ZI PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                        }
                                }
                        }

                        if (mystique->dwgreg.sgn.sdydxl)
                                x += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                        else
                                mystique->dwgreg.ydst_lin += (mystique->dwgreg.sgn.sdy ? -(mystique->dwgreg.pitch & PITCH_MASK) : (mystique->dwgreg.pitch & PITCH_MASK));

                        mystique->dwgreg.dr[0] += mystique->dwgreg.dr[2];
                        mystique->dwgreg.dr[4] += mystique->dwgreg.dr[6];
                        mystique->dwgreg.dr[8] += mystique->dwgreg.dr[10];
                        mystique->dwgreg.dr[12] += mystique->dwgreg.dr[14];

                        if ((int32_t)mystique->dwgreg.ar[1] >= 0)
                        {
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[2];

                                if (mystique->dwgreg.sgn.sdydxl)
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.sgn.sdy ? -(mystique->dwgreg.pitch & PITCH_MASK) : (mystique->dwgreg.pitch & PITCH_MASK));
                                else
                                        x += (mystique->dwgreg.sgn.sdxl ? -1 : 1);

                                mystique->dwgreg.dr[0] += mystique->dwgreg.dr[3];
                                mystique->dwgreg.dr[4] += mystique->dwgreg.dr[7];
                                mystique->dwgreg.dr[8] += mystique->dwgreg.dr[11];
                                mystique->dwgreg.dr[12] += mystique->dwgreg.dr[15];
                        }
                        else
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[0];

                        mystique->dwgreg.length--;
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown atype %03x %08x LINE\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
        mystique->blitter_complete_refcount++;
}

static void blit_autoline(mystique_t* mystique, int closed)
{
        int start_x = (int32_t)mystique->dwgreg.ar[5];
        int start_y = (int32_t)mystique->dwgreg.ar[6];
        int end_x = (int32_t)mystique->dwgreg.ar[0];
        int end_y = (int32_t)mystique->dwgreg.ar[2];
        int dx = end_x - start_x;
        int dy = end_y - start_y;

        if (ABS(dx) > ABS(dy))
        {
                mystique->dwgreg.sgn.sdydxl = 1;
                mystique->dwgreg.ar[0] = 2 * ABS(dy);
                mystique->dwgreg.ar[1] = 2 * ABS(dy) - ABS(dx) - ((start_y > end_y) ? 1 : 0);
                mystique->dwgreg.ar[2] = 2 * ABS(dy) - 2 * ABS(dx);
                mystique->dwgreg.length = ABS(end_x - start_x);
        }
        else
        {
                mystique->dwgreg.sgn.sdydxl = 0;
                mystique->dwgreg.ar[0] = 2 * ABS(dx);
                mystique->dwgreg.ar[1] = 2 * ABS(dx) - ABS(dy) - ((start_y > end_y) ? 1 : 0);
                mystique->dwgreg.ar[2] = 2 * ABS(dx) - 2 * ABS(dy);
                mystique->dwgreg.length = ABS(end_y - start_y);
        }
        mystique->dwgreg.sgn.sdxl = (start_x > end_x) ? 1 : 0;
        mystique->dwgreg.sgn.sdy = (start_y > end_y) ? 1 : 0;


/*        pclog("Autoline %s\n", closed ? "closed" : "open");
        pclog("Start %i,%i\n", start_x, start_y);
        pclog("End %i,%i\n", end_x, end_y);
        pclog("SGN=%x\n", mystique->dwgreg.sgn);
        pclog("AR0=%08x AR1=%08x AR2=%08x\n", mystique->dwgreg.ar[0],
                                              mystique->dwgreg.ar[1],
                                              mystique->dwgreg.ar[2]);
        pclog("Length %i\n", mystique->dwgreg.length);*/
        blit_line(mystique, closed);

        mystique->dwgreg.ar[5] = end_x;
        mystique->dwgreg.xdst = end_x;
        mystique->dwgreg.ar[6] = end_y;
        mystique->dwgreg.ydst = end_y;
        mystique->dwgreg.ydst_lin = ((int32_t)(int16_t)mystique->dwgreg.ydst * (mystique->dwgreg.pitch & PITCH_MASK)) + mystique->dwgreg.ydstorg;
}

static void blit_trap(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;
        uint32_t z_back, r_back, g_back, b_back;
        int z_write;
        int y;
        const int trans_sel = (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANS_MASK) >> DWGCTRL_TRANS_SHIFT;

        mystique->trap_count++;

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_BLK:
        case DWGCTRL_ATYPE_RPL:
                //fxleft, fxright
                //fcol
/*                pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
                pclog("fcol=%08x\n", mystique->dwgreg.fcol);
                pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                pclog("pitch=%08x\n", mystique->dwgreg.pitch);
                pclog("ydst_lin=%08x\n", mystique->dwgreg.ydst_lin);*/
//                if (!(mystique->dwgreg.dwgctrl_running & DWGCTRL_SGNZERO))
//                        fatal("Not rectangle\n");

                for (y = 0; y < mystique->dwgreg.length; y++)
                {
                        uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
                        int16_t x_l = mystique->dwgreg.fxleft & 0xffff;
                        int16_t x_r = mystique->dwgreg.fxright & 0xffff;
                        int yoff = (mystique->dwgreg.yoff + mystique->dwgreg.ydst) & 7;

//                        pclog("  line %03i: %08x\n", y, mystique->dwgreg.ydst_lin);
                        while (x_l != x_r)
                        {
                                if (x_l >= mystique->dwgreg.cxleft && x_l <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                    trans[x_l & 3])
                                {
                                        int xoff = (mystique->dwgreg.xoff + x_l) & 7;
                                        int pattern = mystique->dwgreg.pattern[yoff][xoff];
                                        uint32_t dst;

//                                        pclog("     %06x %02x\n", mystique->dwgreg.ydst_lin + x, mystique->dwgreg.fcol & 0xff);
                                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                        {
                                        case MACCESS_PWIDTH_8:
                                                svga->vram[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask] =
                                                        (pattern ? mystique->dwgreg.fcol : mystique->dwgreg.bcol) & 0xff;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask) >> 12] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_16:
                                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w] =
                                                        (pattern ? mystique->dwgreg.fcol : mystique->dwgreg.bcol) & 0xffff;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_24:
                                                dst = *(uint32_t*)(&svga->vram[((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask]) & 0xff000000;
                                                *(uint32_t*)(&svga->vram[((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask]) =
                                                        ((pattern ? mystique->dwgreg.fcol : mystique->dwgreg.bcol) & 0xffffff) | dst;
                                                svga->changedvram[(((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_32:
                                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l] =
                                                        pattern ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                break;

#ifndef RELEASE_BUILD
                                        default:
                                                fatal("TRAP BLK/RPL PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                        }
                                }
                                x_l++;
                                mystique->pixel_count++;
                        }

                        if ((int32_t)mystique->dwgreg.ar[1] < 0)
                        {
                                while ((int32_t)mystique->dwgreg.ar[1] < 0 && mystique->dwgreg.ar[0])
                                {
                                        mystique->dwgreg.ar[1] += mystique->dwgreg.ar[0];
                                        mystique->dwgreg.fxleft += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                                }
                        }
                        else
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[2];

                        if ((int32_t)mystique->dwgreg.ar[4] < 0)
                        {
                                while ((int32_t)mystique->dwgreg.ar[4] < 0 && mystique->dwgreg.ar[6])
                                {
                                        mystique->dwgreg.ar[4] += mystique->dwgreg.ar[6];
                                        mystique->dwgreg.fxright += (mystique->dwgreg.sgn.sdxr ? -1 : 1);
                                }
                        }
                        else
                                mystique->dwgreg.ar[4] += mystique->dwgreg.ar[5];

                        mystique->dwgreg.ydst++;
                        mystique->dwgreg.ydst &= 0x7fffff;
                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);

                        mystique->dwgreg.selline = (mystique->dwgreg.selline + 1) & 7;
                }
                break;

        case DWGCTRL_ATYPE_RSTR:
                for (y = 0; y < mystique->dwgreg.length; y++)
                {
                        uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
                        int16_t x_l = mystique->dwgreg.fxleft & 0xffff;
                        int16_t x_r = mystique->dwgreg.fxright & 0xffff;
                        int yoff = (mystique->dwgreg.yoff + mystique->dwgreg.ydst) & 7;

//                        pclog("  line %03i: %08x\n", y, mystique->dwgreg.ydst_lin);
                        while (x_l != x_r)
                        {
                                if (x_l >= mystique->dwgreg.cxleft && x_l <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                    trans[x_l & 3])
                                {
                                        int xoff = (mystique->dwgreg.xoff + x_l) & 7;
                                        int pattern = mystique->dwgreg.pattern[yoff][xoff];
                                        uint32_t src = pattern ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                        uint32_t dst, old_dst;

                                        switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                        {
                                        case MACCESS_PWIDTH_8:
                                                dst = svga->vram[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask];

                                                dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     %06x %02x\n", mystique->dwgreg.ydst_lin + x, mystique->dwgreg.fcol & 0xff);
                                                svga->vram[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask) >> 12] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_16:
                                                dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w];

                                                dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     %06x %02x\n", mystique->dwgreg.ydst_lin + x, mystique->dwgreg.fcol & 0xff);
                                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_24:
                                                old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask];

                                                dst = bitop(src, old_dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     %06x %02x\n", mystique->dwgreg.ydst_lin + x, mystique->dwgreg.fcol & 0xff);
                                                *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask] = (dst & 0xffffff) | (old_dst & 0xff000000);
                                                svga->changedvram[(((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                break;

                                        case MACCESS_PWIDTH_32:
                                                dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l];

                                                dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);
//                                                pclog("     %06x %02x\n", mystique->dwgreg.ydst_lin + x, mystique->dwgreg.fcol & 0xff);
                                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l] = dst;
                                                svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                break;

#ifndef RELEASE_BUILD
                                        default:
                                                fatal("TRAP RSTR PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                        }
                                }
                                x_l++;
                                mystique->pixel_count++;
                        }

                        if ((int32_t)mystique->dwgreg.ar[1] < 0)
                        {
                                while ((int32_t)mystique->dwgreg.ar[1] < 0 && mystique->dwgreg.ar[0])
                                {
                                        mystique->dwgreg.ar[1] += mystique->dwgreg.ar[0];
                                        mystique->dwgreg.fxleft += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                                }
                        }
                        else
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[2];

                        if ((int32_t)mystique->dwgreg.ar[4] < 0)
                        {
                                while ((int32_t)mystique->dwgreg.ar[4] < 0 && mystique->dwgreg.ar[6])
                                {
                                        mystique->dwgreg.ar[4] += mystique->dwgreg.ar[6];
                                        mystique->dwgreg.fxright += (mystique->dwgreg.sgn.sdxr ? -1 : 1);
                                }
                        }
                        else
                                mystique->dwgreg.ar[4] += mystique->dwgreg.ar[5];

                        mystique->dwgreg.ydst++;
                        mystique->dwgreg.ydst &= 0x7fffff;
                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);

                        mystique->dwgreg.selline = (mystique->dwgreg.selline + 1) & 7;
                }
                break;

        case DWGCTRL_ATYPE_I:
        case DWGCTRL_ATYPE_ZI:
                z_write = ((mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK) == DWGCTRL_ATYPE_ZI);
/*                pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
                pclog("fcol=%08x\n", mystique->dwgreg.fcol);
                pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                pclog("pitch=%08x\n", mystique->dwgreg.pitch);
                pclog("ydst_lin=%08x\n", mystique->dwgreg.ydst_lin);
                pclog("trap! maccess=%08x  %08x %08x %08x\n", mystique->maccess_running, mystique->dwgreg.dr[4], mystique->dwgreg.dr[8], mystique->dwgreg.dr[12]);*/

                for (y = 0; y < mystique->dwgreg.length; y++)
                {
                        uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
                        uint16_t* z_p = (uint16_t*)&svga->vram[(mystique->dwgreg.ydst_lin * 2 + mystique->dwgreg.zorg) & mystique->vram_mask];
                        int16_t x_l = mystique->dwgreg.fxleft & 0xffff;
                        int16_t x_r = mystique->dwgreg.fxright & 0xffff;
                        int16_t old_x_l = x_l;
                        int dx;

                        z_back = mystique->dwgreg.dr[0];
                        r_back = mystique->dwgreg.dr[4];
                        g_back = mystique->dwgreg.dr[8];
                        b_back = mystique->dwgreg.dr[12];

//                        pclog("  line %03i: %08x\n", y, mystique->dwgreg.ydst_lin);
                        while (x_l != x_r)
                        {
                                if (x_l >= mystique->dwgreg.cxleft && x_l <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                    trans[x_l & 3])
                                {
                                        uint16_t z = ((int32_t)mystique->dwgreg.dr[0] < 0) ? 0 : (mystique->dwgreg.dr[0] >> 15);
                                        uint16_t old_z = z_p[x_l];

                                        if (z_check(z, old_z, mystique->dwgreg.dwgctrl_running & DWGCTRL_ZMODE_MASK))
                                        {
                                                uint32_t dst = 0, old_dst;
                                                int r = 0, g = 0, b = 0;

                                                if (!(mystique->dwgreg.dr[4] & (1 << 23)))
                                                        r = (mystique->dwgreg.dr[4] >> 15) & 0xff;
                                                if (!(mystique->dwgreg.dr[8] & (1 << 23)))
                                                        g = (mystique->dwgreg.dr[8] >> 15) & 0xff;
                                                if (!(mystique->dwgreg.dr[12] & (1 << 23)))
                                                        b = (mystique->dwgreg.dr[12] >> 15) & 0xff;

                                                if (z_write)
                                                        z_p[x_l] = z;

//                                                pclog("     %06x %02x\n", mystique->dwgreg.ydst_lin + x, mystique->dwgreg.fcol & 0xff);
                                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                                {
                                                case MACCESS_PWIDTH_8:
                                                        svga->vram[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_16:
                                                        dst = dither(mystique, r, g, b, x_l & 1, mystique->dwgreg.selline & 1);
//                                                        pclog("  %08x %08x %08x %04x\n", mystique->dwgreg.dr[4], mystique->dwgreg.dr[8], mystique->dwgreg.dr[12], dst);
                                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_24:
                                                        old_dst = *(uint32_t*)(&svga->vram[((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask]) & 0xff000000;
                                                        *(uint32_t*)(&svga->vram[((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask]) = old_dst | dst;
                                                        svga->changedvram[(((mystique->dwgreg.ydst_lin + x_l) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_32:
                                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l] = b | (g << 8) | (r << 16);
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                        break;

#ifndef RELEASE_BUILD
                                                default:
                                                        fatal("TRAP BLK/RPL PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                                }
                                        }
                                }

                                mystique->dwgreg.dr[0] += mystique->dwgreg.dr[2];
                                mystique->dwgreg.dr[4] += mystique->dwgreg.dr[6];
                                mystique->dwgreg.dr[8] += mystique->dwgreg.dr[10];
                                mystique->dwgreg.dr[12] += mystique->dwgreg.dr[14];

                                x_l++;
                                mystique->pixel_count++;
                        }

                        mystique->dwgreg.dr[0] = z_back + mystique->dwgreg.dr[3];
                        mystique->dwgreg.dr[4] = r_back + mystique->dwgreg.dr[7];
                        mystique->dwgreg.dr[8] = g_back + mystique->dwgreg.dr[11];
                        mystique->dwgreg.dr[12] = b_back + mystique->dwgreg.dr[15];

                        while ((int32_t)mystique->dwgreg.ar[1] < 0 && mystique->dwgreg.ar[0])
                        {
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[0];
                                mystique->dwgreg.fxleft += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                        }
                        mystique->dwgreg.ar[1] += mystique->dwgreg.ar[2];

                        while ((int32_t)mystique->dwgreg.ar[4] < 0 && mystique->dwgreg.ar[6])
                        {
                                mystique->dwgreg.ar[4] += mystique->dwgreg.ar[6];
                                mystique->dwgreg.fxright += (mystique->dwgreg.sgn.sdxr ? -1 : 1);
                        }
                        mystique->dwgreg.ar[4] += mystique->dwgreg.ar[5];

                        dx = (int16_t)((mystique->dwgreg.fxleft - old_x_l) & 0xffff);
                        mystique->dwgreg.dr[0] += dx * mystique->dwgreg.dr[2];
                        mystique->dwgreg.dr[4] += dx * mystique->dwgreg.dr[6];
                        mystique->dwgreg.dr[8] += dx * mystique->dwgreg.dr[10];
                        mystique->dwgreg.dr[12] += dx * mystique->dwgreg.dr[14];

                        mystique->dwgreg.ydst++;
                        mystique->dwgreg.ydst &= 0x7fffff;
                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);

                        mystique->dwgreg.selline = (mystique->dwgreg.selline + 1) & 7;
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown atype %03x %08x TRAP\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }

        mystique->blitter_complete_refcount++;
}

static int texture_read(mystique_t* mystique, int* tex_r, int* tex_g, int* tex_b, int* atransp)
{
        svga_t* svga = &mystique->svga;
        const int tex_shift = 3 + ((mystique->dwgreg.texctl & TEXCTL_TPITCH_MASK) >> TEXCTL_TPITCH_SHIFT);
        const unsigned int palsel = mystique->dwgreg.texctl & TEXCTL_PALSEL_MASK;
        const uint16_t tckey = mystique->dwgreg.textrans & TEXTRANS_TCKEY_MASK;
        const uint16_t tkmask = (mystique->dwgreg.textrans & TEXTRANS_TKMASK_MASK) >> TEXTRANS_TKMASK_SHIFT;
        const unsigned int w_mask = (mystique->dwgreg.texwidth & TEXWIDTH_TWMASK_MASK) >> TEXWIDTH_TWMASK_SHIFT;
        const unsigned int h_mask = (mystique->dwgreg.texheight & TEXHEIGHT_THMASK_MASK) >> TEXHEIGHT_THMASK_SHIFT;
        uint16_t src = 0;
        int s, t;

        if (mystique->dwgreg.texctl & TEXCTL_NPCEN)
        {
                const int s_shift = 20 - (mystique->dwgreg.texwidth & TEXWIDTH_TW_MASK);
                const int t_shift = 20 - (mystique->dwgreg.texheight & TEXHEIGHT_TH_MASK);

                s = (int32_t)mystique->dwgreg.tmr[6] >> s_shift;
                t = (int32_t)mystique->dwgreg.tmr[7] >> t_shift;
        }
        else
        {
                const int s_shift = (20 + 16) - (mystique->dwgreg.texwidth & TEXWIDTH_TW_MASK);
                const int t_shift = (20 + 16) - (mystique->dwgreg.texheight & TEXHEIGHT_TH_MASK);
                int64_t q = mystique->dwgreg.tmr[8] ? ((0x100000000ll / (int64_t)(int32_t)mystique->dwgreg.tmr[8]) /*>> 16*/) : 0;

                s = (((int64_t)(int32_t)mystique->dwgreg.tmr[6] * q) /*<< 8*/) >> s_shift;/*((16+20)-12);*/
                t = (((int64_t)(int32_t)mystique->dwgreg.tmr[7] * q) /*<< 8*/) >> t_shift;/*((16+20)-9);*/
        }

        if (mystique->dwgreg.texctl & TEXCTL_CLAMPU)
        {
                if (s < 0)
                        s = 0;
                else if (s > w_mask)
                        s = w_mask;
        }
        else
                s &= w_mask;

        if (mystique->dwgreg.texctl & TEXCTL_CLAMPV)
        {
                if (t < 0)
                        t = 0;
                else if (t > h_mask)
                        t = h_mask;
        }
        else
                t &= h_mask;

        switch (mystique->dwgreg.texctl & TEXCTL_TEXFORMAT_MASK)
        {
        case TEXCTL_TEXFORMAT_TW4:
                src = svga->vram[(mystique->dwgreg.texorg + (((t << tex_shift) + s) >> 1)) & mystique->vram_mask];
                if (s & 1)
                        src >>= 4;
                else
                        src &= 0xf;
                *tex_r = mystique->lut[src | palsel].r;
                *tex_g = mystique->lut[src | palsel].g;
                *tex_b = mystique->lut[src | palsel].b;
                *atransp = 0;
                break;
        case TEXCTL_TEXFORMAT_TW8:
                src = svga->vram[(mystique->dwgreg.texorg + (t << tex_shift) + s) & mystique->vram_mask];
                *tex_r = mystique->lut[src].r;
                *tex_g = mystique->lut[src].g;
                *tex_b = mystique->lut[src].b;
                *atransp = 0;
                break;
        case TEXCTL_TEXFORMAT_TW15:
                src = ((uint16_t*)svga->vram)[((mystique->dwgreg.texorg >> 1) + (t << tex_shift) + s) & mystique->vram_mask_w];
                *tex_r = ((src >> 10) & 0x1f) << 3;
                *tex_g = ((src >> 5) & 0x1f) << 3;
                *tex_b = (src & 0x1f) << 3;
                if (((src >> 15) & mystique->dwgreg.ta_mask) == mystique->dwgreg.ta_key)
                        *atransp = 1;
                else
                        *atransp = 0;
                break;
        case TEXCTL_TEXFORMAT_TW16:
                src = ((uint16_t*)svga->vram)[((mystique->dwgreg.texorg >> 1) + (t << tex_shift) + s) & mystique->vram_mask_w];
                *tex_r = (src >> 11) << 3;
                *tex_g = ((src >> 5) & 0x3f) << 2;
                *tex_b = (src & 0x1f) << 3;
                *atransp = 0;
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown texture format %i\n", mystique->dwgreg.texctl & TEXCTL_TEXFORMAT_MASK);
                break;
#endif
        }

        return ((src & tkmask) == tckey);
}

static void blit_texture_trap(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;
        int y;
        int z_write;
        const int trans_sel = (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANS_MASK) >> DWGCTRL_TRANS_SHIFT;
        const int dest32 = ((mystique->maccess_running & MACCESS_PWIDTH_MASK) == MACCESS_PWIDTH_32);

        mystique->trap_count++;

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_I:
        case DWGCTRL_ATYPE_ZI:
                z_write = ((mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK) == DWGCTRL_ATYPE_ZI);
//                pclog("fxleft=%i fxright=%i texctl=%08x texorg=%08x ydst=%i len=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright, mystique->dwgreg.texctl, mystique->dwgreg.texorg, mystique->dwgreg.ydst, mystique->dwgreg.length);
/*                pclog("fcol=%08x\n", mystique->dwgreg.fcol);
                pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                pclog("pitch=%08x\n", mystique->dwgreg.pitch);
                pclog("ydst=%i ydst_lin=%08x\n", mystique->dwgreg.ydst, mystique->dwgreg.ydst_lin);
                pclog("dYl =%08x errl  =%08x -|dXl|=%08x\n", mystique->dwgreg.ar[0], mystique->dwgreg.ar[1], mystique->dwgreg.ar[2]);
                pclog("errr=%08x -|dXr|=%08x dYr   =%08x\n", mystique->dwgreg.ar[4], mystique->dwgreg.ar[5], mystique->dwgreg.ar[6]);
                pclog(" q/wc=%08x q/wc/dX=%08x q/wc/dY=%08x\n",
                        mystique->dwgreg.tmr[8], mystique->dwgreg.tmr[4], mystique->dwgreg.tmr[5]);
                pclog(" s/wc=%08x s/wc/dX=%08x s/wc/dY=%08x\n",
                        mystique->dwgreg.tmr[6], mystique->dwgreg.tmr[0], mystique->dwgreg.tmr[1]);
                pclog(" t/wc=%08x t/wc/dX=%08x t/wc/dY=%08x\n",
                        mystique->dwgreg.tmr[7], mystique->dwgreg.tmr[2], mystique->dwgreg.tmr[3]);*/

                for (y = 0; y < mystique->dwgreg.length; y++)
                {
                        uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
                        uint16_t* z_p = (uint16_t*)&svga->vram[(mystique->dwgreg.ydst_lin * 2 + mystique->dwgreg.zorg) & mystique->vram_mask];
                        int16_t x_l = mystique->dwgreg.fxleft & 0xffff;
                        int16_t x_r = mystique->dwgreg.fxright & 0xffff;
                        int16_t old_x_l = x_l;
                        int dx;

                        uint32_t z_back = mystique->dwgreg.dr[0];
                        uint32_t r_back = mystique->dwgreg.dr[4];
                        uint32_t g_back = mystique->dwgreg.dr[8];
                        uint32_t b_back = mystique->dwgreg.dr[12];
                        uint32_t s_back = mystique->dwgreg.tmr[6];
                        uint32_t t_back = mystique->dwgreg.tmr[7];
                        uint32_t q_back = mystique->dwgreg.tmr[8];

//                        pclog("  line %i: %i %i  %08x %08x %08x  %08x %i\n", __y, x_l, x_r, mystique->dwgreg.dr[0],mystique->dwgreg.dr[2],mystique->dwgreg.dr[3], mystique->dwgreg.ydst_lin*2 + mystique->dwgreg.zorg, z_write);
//                        pclog("    %08x %08x %08x\n", mystique->dwgreg.tmr[4],mystique->dwgreg.tmr[5], mystique->dwgreg.tmr[8]);
//                        pclog("    %08x %08x %08x\n", mystique->dwgreg.ar[4], mystique->dwgreg.ar[5], mystique->dwgreg.ar[6]);

                        while (x_l != x_r)
                        {
                                if (x_l >= mystique->dwgreg.cxleft && x_l <= mystique->dwgreg.cxright &&
                                    mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                    trans[x_l & 3])
                                {
                                        uint16_t z = ((int32_t)mystique->dwgreg.dr[0] < 0) ? 0 : (mystique->dwgreg.dr[0] >> 15);
                                        uint16_t old_z = z_p[x_l];

                                        if (z_check(z, old_z, mystique->dwgreg.dwgctrl_running & DWGCTRL_ZMODE_MASK))
                                        {
                                                int tex_r = 0, tex_g = 0, tex_b = 0;
                                                int ctransp, atransp = 0;
                                                int i_r = 0, i_g = 0, i_b = 0;

                                                if (!(mystique->dwgreg.dr[4] & (1 << 23)))
                                                        i_r = (mystique->dwgreg.dr[4] >> 15) & 0xff;
                                                if (!(mystique->dwgreg.dr[8] & (1 << 23)))
                                                        i_g = (mystique->dwgreg.dr[8] >> 15) & 0xff;
                                                if (!(mystique->dwgreg.dr[12] & (1 << 23)))
                                                        i_b = (mystique->dwgreg.dr[12] >> 15) & 0xff;

                                                ctransp = texture_read(mystique, &tex_r, &tex_g, &tex_b, &atransp);

                                                switch (mystique->dwgreg.texctl & (TEXCTL_TMODULATE | TEXCTL_STRANS | TEXCTL_ITRANS | TEXCTL_DECALCKEY))
                                                {
                                                case 0:
                                                        if (ctransp)
                                                                goto skip_pixel;
                                                        if (atransp)
                                                        {
                                                                tex_r = i_r;
                                                                tex_g = i_g;
                                                                tex_b = i_b;
                                                        }
                                                        break;

                                                case TEXCTL_DECALCKEY:
                                                        if (ctransp)
                                                        {
                                                                tex_r = i_r;
                                                                tex_g = i_g;
                                                                tex_b = i_b;
                                                        }
                                                        break;

                                                case (TEXCTL_STRANS | TEXCTL_DECALCKEY):
                                                        if (ctransp)
                                                                goto skip_pixel;
                                                        break;

                                                case TEXCTL_TMODULATE:
                                                        if (ctransp)
                                                                goto skip_pixel;
                                                        if (mystique->dwgreg.texctl & TEXCTL_TMODULATE)
                                                        {
                                                                tex_r = (tex_r * i_r) >> 8;
                                                                tex_g = (tex_g * i_g) >> 8;
                                                                tex_b = (tex_b * i_b) >> 8;
                                                        }
                                                        break;

                                                case (TEXCTL_TMODULATE | TEXCTL_STRANS):
                                                        if (ctransp || atransp)
                                                                goto skip_pixel;
                                                        if (mystique->dwgreg.texctl & TEXCTL_TMODULATE)
                                                        {
                                                                tex_r = (tex_r * i_r) >> 8;
                                                                tex_g = (tex_g * i_g) >> 8;
                                                                tex_b = (tex_b * i_b) >> 8;
                                                        }
                                                        break;

#ifndef RELEASE_BUILD
                                                default:
                                                        fatal("Bad TEXCTL %08x %08x\n", mystique->dwgreg.texctl,
                                                                mystique->dwgreg.texctl & (TEXCTL_TMODULATE | TEXCTL_STRANS | TEXCTL_ITRANS | TEXCTL_DECALCKEY));
#endif
                                                }

                                                if (dest32)
                                                {
                                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l] = tex_b | (tex_g << 8) | (tex_r << 16);
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                }
                                                else
                                                {
                                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w] = dither(mystique, tex_r, tex_g, tex_b,
                                                                x_l & 1, mystique->dwgreg.selline & 1);
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x_l) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                }
                                                if (z_write)
                                                        z_p[x_l] = z;
                                        }
                                }
                            skip_pixel:
                                x_l++;
                                mystique->pixel_count++;

                                mystique->dwgreg.dr[0] += mystique->dwgreg.dr[2];
                                mystique->dwgreg.dr[4] += mystique->dwgreg.dr[6];
                                mystique->dwgreg.dr[8] += mystique->dwgreg.dr[10];
                                mystique->dwgreg.dr[12] += mystique->dwgreg.dr[14];
                                mystique->dwgreg.tmr[6] += mystique->dwgreg.tmr[0];
                                mystique->dwgreg.tmr[7] += mystique->dwgreg.tmr[2];
                                mystique->dwgreg.tmr[8] += mystique->dwgreg.tmr[4];
                        }

                        mystique->dwgreg.dr[0] = z_back + mystique->dwgreg.dr[3];
                        mystique->dwgreg.dr[4] = r_back + mystique->dwgreg.dr[7];
                        mystique->dwgreg.dr[8] = g_back + mystique->dwgreg.dr[11];
                        mystique->dwgreg.dr[12] = b_back + mystique->dwgreg.dr[15];
                        mystique->dwgreg.tmr[6] = s_back + mystique->dwgreg.tmr[1];
                        mystique->dwgreg.tmr[7] = t_back + mystique->dwgreg.tmr[3];
                        mystique->dwgreg.tmr[8] = q_back + mystique->dwgreg.tmr[5];

                        while ((int32_t)mystique->dwgreg.ar[1] < 0 && mystique->dwgreg.ar[0])
                        {
                                mystique->dwgreg.ar[1] += mystique->dwgreg.ar[0];
                                mystique->dwgreg.fxleft += (mystique->dwgreg.sgn.sdxl ? -1 : 1);
                        }
                        mystique->dwgreg.ar[1] += mystique->dwgreg.ar[2];

                        while ((int32_t)mystique->dwgreg.ar[4] < 0 && mystique->dwgreg.ar[6])
                        {
                                mystique->dwgreg.ar[4] += mystique->dwgreg.ar[6];
                                mystique->dwgreg.fxright += (mystique->dwgreg.sgn.sdxr ? -1 : 1);
                        }
                        mystique->dwgreg.ar[4] += mystique->dwgreg.ar[5];

                        dx = (int16_t)((mystique->dwgreg.fxleft - old_x_l) & 0xffff);
                        mystique->dwgreg.dr[0] += dx * mystique->dwgreg.dr[2];
                        mystique->dwgreg.dr[4] += dx * mystique->dwgreg.dr[6];
                        mystique->dwgreg.dr[8] += dx * mystique->dwgreg.dr[10];
                        mystique->dwgreg.dr[12] += dx * mystique->dwgreg.dr[14];
                        mystique->dwgreg.tmr[6] += dx * mystique->dwgreg.tmr[0];
                        mystique->dwgreg.tmr[7] += dx * mystique->dwgreg.tmr[2];
                        mystique->dwgreg.tmr[8] += dx * mystique->dwgreg.tmr[4];

                        mystique->dwgreg.ydst++;
                        mystique->dwgreg.ydst &= 0x7fffff;
                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);

                        mystique->dwgreg.selline = (mystique->dwgreg.selline + 1) & 7;
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown atype %03x %08x TEXTURE_TRAP\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }

        mystique->blitter_complete_refcount++;
}

static void blit_bitblt(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;
        uint32_t src_addr;
        int y;
        int x_dir = mystique->dwgreg.sgn.scanleft ? -1 : 1;
        int16_t x_start = mystique->dwgreg.sgn.scanleft ? mystique->dwgreg.fxright : mystique->dwgreg.fxleft;
        int16_t x_end = mystique->dwgreg.sgn.scanleft ? mystique->dwgreg.fxleft : mystique->dwgreg.fxright;
        const int trans_sel = (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANS_MASK) >> DWGCTRL_TRANS_SHIFT;

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_BLK:
//                if (mystique->maccess_running & MACCESS_TLUTLOAD)
//                        fatal("Bitblt TLUTLOAD BLK\n");
//                if (mystique->dwgreg.dwgctrl_running & DWGCTRL_PATTERN)
//                        fatal("BITBLT BLK with pattern\n");
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BMONOLEF:
//                        pclog("ar0=%08x ar3=%08x ar5=%08x dwgctrl=%08x\n", mystique->dwgreg.ar[0], mystique->dwgreg.ar[3], mystique->dwgreg.ar[5], mystique->dwgreg.dwgctrl_running);
//                        pclog("fxleft=%i fxright=%i ydst=%i fcol=%08x bcol=%08x\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright, mystique->dwgreg.ydst, mystique->dwgreg.fcol, mystique->dwgreg.bcol);

                        src_addr = mystique->dwgreg.ar[3];
//                        mystique->dwgreg.bit_offset = 0;

                        for (y = 0; y < mystique->dwgreg.length; y++)
                        {
                                int16_t x = x_start;

//                                pclog("  line %03i: %08x %08x %02x\n", y, mystique->dwgreg.ydst_lin, src_addr, svga->vram[0x2eba3]);
                                while (1)
                                {
                                        if (x >= mystique->dwgreg.cxleft && x <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                                        {
                                                uint32_t byte_addr = (src_addr >> 3) & mystique->vram_mask;
                                                int bit_offset = src_addr & 7;
                                                uint32_t old_dst;

                                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                                {
                                                case MACCESS_PWIDTH_8:
//                                                        pclog("   pix %i %02x %i %i  %06x\n", x, svga->vram[byte_addr], svga->vram[byte_addr] & (1 << bit_offset), mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC, byte_addr);
                                                        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC)
                                                        {
                                                                if (svga->vram[byte_addr] & (1 << bit_offset))
                                                                        svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask] = mystique->dwgreg.fcol;
                                                        }
                                                        else
                                                                svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask] =
                                                                        (svga->vram[byte_addr] & (1 << bit_offset)) ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_16:
//                                                        pclog("   pix %i %02x %i %i  %06x\n", x, svga->vram[byte_addr], svga->vram[byte_addr] & (1 << bit_offset), mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC, byte_addr);
                                                        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC)
                                                        {
                                                                if (svga->vram[byte_addr] & (1 << bit_offset))
                                                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] = mystique->dwgreg.fcol;
                                                        }
                                                        else
                                                                ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] =
                                                                        (svga->vram[byte_addr] & (1 << bit_offset)) ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_24:
                                                        old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask];
//                                                        pclog("   pix %i %02x %i %i  %06x\n", x, svga->vram[byte_addr], svga->vram[byte_addr] & (1 << bit_offset), mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC, byte_addr);
                                                        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC)
                                                        {
                                                                if (svga->vram[byte_addr] & (1 << bit_offset))
                                                                        *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask] =
                                                                                (old_dst & 0xff000000) | (mystique->dwgreg.fcol & 0xffffff);
                                                        }
                                                        else
                                                                *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask] =
                                                                        (old_dst & 0xff000000)
                                                                        | (((svga->vram[byte_addr] & (1 << bit_offset)) ? mystique->dwgreg.fcol : mystique->dwgreg.bcol) & 0xffffff);
                                                        svga->changedvram[(((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_32:
//                                                        pclog("   pix %i %02x %i %i  %06x\n", x, svga->vram[byte_addr], svga->vram[byte_addr] & (1 << bit_offset), mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC, byte_addr);
                                                        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC)
                                                        {
                                                                if (svga->vram[byte_addr] & (1 << bit_offset))
                                                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & 0xfffff] = mystique->dwgreg.fcol;
                                                        }
                                                        else
                                                                ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & 0xfffff] =
                                                                        (svga->vram[byte_addr] & (1 << bit_offset)) ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                        break;

#ifndef RELEASE_BUILD
                                                default:
                                                        fatal("BITBLT DWGCTRL_ATYPE_BLK unknown MACCESS %i\n", mystique->maccess_running & MACCESS_PWIDTH_MASK);
#endif
                                                }
                                        }

                                        if (src_addr == mystique->dwgreg.ar[0])
                                        {
                                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                                src_addr = mystique->dwgreg.ar[3];
                                        }
                                        else
                                                src_addr += x_dir;

                                        if (x != x_end)
                                                x += x_dir;
                                        else
                                                break;
                                }

                                if (mystique->dwgreg.sgn.sdy)
                                        mystique->dwgreg.ydst_lin -= (mystique->dwgreg.pitch & PITCH_MASK);
                                else
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                        }
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("BITBLT BLK %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK);
                        break;
#endif
                }
                break;

        case DWGCTRL_ATYPE_RPL:
//                pclog("  bitblt RPL\n");
                if (mystique->maccess_running & MACCESS_TLUTLOAD)
                {
//                        pclog("TLUTLOAD!\n");
                        src_addr = mystique->dwgreg.ar[3];

                        y = mystique->dwgreg.ydst;

                        while (mystique->dwgreg.length)
                        {
                                uint16_t src = ((uint16_t*)svga->vram)[src_addr & mystique->vram_mask_w];
//                                pclog("  LUT[%02x]=VRAM[%05X]=%04x\n", y & 0xff, src_addr, src);

                                mystique->lut[y & 0xff].r = (src >> 11) << 3;
                                mystique->lut[y & 0xff].g = ((src >> 5) & 0x3f) << 2;
                                mystique->lut[y & 0xff].b = (src & 0x1f) << 3;
                                src_addr++;
                                y++;
                                mystique->dwgreg.length--;
                        }
                        break;
                }
        case DWGCTRL_ATYPE_RSTR:
//                pclog("  bitblt RPL or RSTR\n");
//                if (mystique->maccess_running & MACCESS_TLUTLOAD)
//                        fatal("Bitblt TLUTLOAD RSTR\n");
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BMONOLEF:
//                        pclog("ar0=%08x ar3=%08x ar5=%08x\n", mystique->dwgreg.ar[0], mystique->dwgreg.ar[3], mystique->dwgreg.ar[5]);
//                        pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);

#ifndef RELEASE_BUILD
                        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_PATTERN)
                                fatal("BITBLT RPL/RSTR BMONOLEF with pattern\n");
#endif

                        src_addr = mystique->dwgreg.ar[3];

                        for (y = 0; y < mystique->dwgreg.length; y++)
                        {
                                uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
                                int16_t x = x_start;

//                                pclog("  line %03i: %08x %08x\n", y, mystique->dwgreg.ydst_lin, src_addr);
                                while (1)
                                {
                                        uint32_t byte_addr = (src_addr >> 3) & mystique->vram_mask;
                                        int bit_offset = src_addr & 7;

                                        if (x >= mystique->dwgreg.cxleft && x <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                            ((svga->vram[byte_addr] & (1 << bit_offset)) || !(mystique->dwgreg.dwgctrl_running & DWGCTRL_TRANSC)) &&
                                            trans[x & 3])
                                        {
                                                uint32_t src = (svga->vram[byte_addr] & (1 << bit_offset)) ? mystique->dwgreg.fcol : mystique->dwgreg.bcol;
                                                uint32_t dst, old_dst;

                                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                                {
                                                case MACCESS_PWIDTH_8:
                                                        dst = svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask];

//                                                        pclog("    %02x %02x  ", src, dst);
                                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_16:
                                                        dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w];

//                                                        pclog("    %02x %02x  ", src, dst);
                                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_24:
                                                        old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask];

//                                                        pclog("    %02x %02x  ", src, old_dst);
                                                        dst = bitop(src, old_dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask] = (dst & 0xffffff) | (old_dst & 0xff000000);
                                                        svga->changedvram[(((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_32:
                                                        dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l];

//                                                        pclog("    %02x %02x  ", src, dst);
                                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 10] = changeframecount;
                                                        break;

#ifndef RELEASE_BUILD
                                                default:
                                                        fatal("BITBLT RPL BMONOLEF PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                                }
                                        }

                                        if (src_addr == mystique->dwgreg.ar[0])
                                        {
                                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                                src_addr = mystique->dwgreg.ar[3];
                                        }
                                        else
                                                src_addr += x_dir;

                                        if (x != x_end)
                                                x += x_dir;
                                        else
                                                break;
                                }

                                if (mystique->dwgreg.sgn.sdy)
                                        mystique->dwgreg.ydst_lin -= (mystique->dwgreg.pitch & PITCH_MASK);
                                else
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                        }
                        break;

                case DWGCTRL_BLTMOD_BFCOL:
                case DWGCTRL_BLTMOD_BU32RGB:
//                        pclog("ar0=%08x ar3=%08x ar5=%08x\n", mystique->dwgreg.ar[0], mystique->dwgreg.ar[3], mystique->dwgreg.ar[5]);
//                        pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);

                        src_addr = mystique->dwgreg.ar[3];

                        for (y = 0; y < mystique->dwgreg.length; y++)
                        {
                                uint8_t const* const trans = &trans_masks[trans_sel][(mystique->dwgreg.selline & 3) * 4];
                                uint32_t old_src_addr = src_addr;
                                int16_t x = x_start;

//                                pclog("  line %03i: %08x %08x\n", y, mystique->dwgreg.ydst_lin, src_addr);
                                while (1)
                                {
                                        if (x >= mystique->dwgreg.cxleft && x <= mystique->dwgreg.cxright &&
                                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot &&
                                            trans[x & 3])
                                        {
                                                uint32_t src, dst, old_dst;

                                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                                {
                                                case MACCESS_PWIDTH_8:
                                                        src = svga->vram[src_addr & mystique->vram_mask];
                                                        dst = svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask];

//                                                        pclog("    %02x %02x  ", src, dst);
                                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_16:
                                                        src = ((uint16_t*)svga->vram)[src_addr & mystique->vram_mask_w];
                                                        dst = ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w];

//                                                        pclog("    %02x %02x  ", src, dst);
                                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 11] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_24:
                                                        src = *(uint32_t*)&svga->vram[(src_addr * 3) & mystique->vram_mask];
                                                        old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask];

//                                                        pclog("    %02x %02x  ", src, old_dst);
                                                        dst = bitop(src, old_dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask] = (dst & 0xffffff) | (old_dst & 0xff000000);
                                                        svga->changedvram[(((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                                        break;

                                                case MACCESS_PWIDTH_32:
                                                        src = ((uint32_t*)svga->vram)[src_addr & mystique->vram_mask_l];
                                                        dst = ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l];

//                                                        pclog("    %02x %02x  ", src, dst);
                                                        dst = bitop(src, dst, mystique->dwgreg.dwgctrl_running);// & DWGCTRL_BOP_MASK
//                                                        pclog("%08x %08x %i dst=%02x\n", src_addr, mystique->dwgreg.ydst_lin + x, mystique->dwgreg.sgn.scanleft, dst);

                                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l] = dst;
                                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l) >> 10] = changeframecount;
                                                        break;

#ifndef RELEASE_BUILD
                                                default:
                                                        fatal("BITBLT RPL BFCOL PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                                }
                                        }

                                        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_PATTERN)
                                                src_addr = ((src_addr + x_dir) & 7) | (src_addr & ~7);
                                        else if (src_addr == mystique->dwgreg.ar[0])
                                        {
                                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                                src_addr = mystique->dwgreg.ar[3];
                                        }
                                        else
                                                src_addr += x_dir;

                                        if (x != x_end)
                                                x += x_dir;
                                        else
                                                break;
                                }

                                if (mystique->dwgreg.dwgctrl_running & DWGCTRL_PATTERN)
                                {
                                        src_addr = old_src_addr;
                                        if (mystique->dwgreg.sgn.sdy)
                                                src_addr = ((src_addr - 32) & 0xe0) | (src_addr & ~0xe0);
                                        else
                                                src_addr = ((src_addr + 32) & 0xe0) | (src_addr & ~0xe0);
                                }

                                if (mystique->dwgreg.sgn.sdy)
                                        mystique->dwgreg.ydst_lin -= (mystique->dwgreg.pitch & PITCH_MASK);
                                else
                                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
                        }
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("BITBLT DWGCTRL_ATYPE_RPL unknown BLTMOD %08x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown BITBLT atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
        mystique->blitter_complete_refcount++;
}

static void blit_fbitblt(mystique_t* mystique)
{
        svga_t* svga = &mystique->svga;
        uint32_t src_addr;
        int y;
        int x_dir = mystique->dwgreg.sgn.scanleft ? -1 : 1;
        int16_t x_start = mystique->dwgreg.sgn.scanleft ? mystique->dwgreg.fxright : mystique->dwgreg.fxleft;
        int16_t x_end = mystique->dwgreg.sgn.scanleft ? mystique->dwgreg.fxleft : mystique->dwgreg.fxright;

/*        pclog("FBITBLT  %08x:\n", mystique->dwgreg.dwgctrl_running);
        pclog("ar0=%08x ar3=%08x ar5=%08x  %i %i\n", mystique->dwgreg.ar[0], mystique->dwgreg.ar[3], mystique->dwgreg.ar[5],
                        mystique->dwgreg.ar[0]-mystique->dwgreg.ar[3],
                        mystique->dwgreg.fxright-mystique->dwgreg.fxleft);
        pclog("fxleft=%i(%x) fxright=%i(%x) pitch=%08x\n",
                mystique->dwgreg.fxleft, mystique->dwgreg.fxleft,
                mystique->dwgreg.fxright, mystique->dwgreg.fxright,  mystique->dwgreg.pitch);*/

        src_addr = mystique->dwgreg.ar[3];

        for (y = 0; y < mystique->dwgreg.length; y++)
        {
                int16_t x = x_start;

//                pclog("  line %03i: %08x %08x %08x  %x\n", y, mystique->dwgreg.ydst_lin, src_addr, src_addr-x, x);
                while (1)
                {
                        if (x >= mystique->dwgreg.cxleft && x <= mystique->dwgreg.cxright &&
                            mystique->dwgreg.ydst_lin >= mystique->dwgreg.ytop && mystique->dwgreg.ydst_lin <= mystique->dwgreg.ybot)
                        {
                                uint32_t src, old_dst;

                                switch (mystique->maccess_running & MACCESS_PWIDTH_MASK)
                                {
                                case MACCESS_PWIDTH_8:
                                        src = svga->vram[src_addr & mystique->vram_mask];

                                        svga->vram[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask] = src;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask) >> 12] = changeframecount;
                                        break;

                                case MACCESS_PWIDTH_16:
                                        src = ((uint16_t*)svga->vram)[src_addr & mystique->vram_mask_w];

                                        ((uint16_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w] = src;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_w) >> 11] = changeframecount;
                                        break;

                                case MACCESS_PWIDTH_24:
                                        src = *(uint32_t*)&svga->vram[(src_addr * 3) & mystique->vram_mask];
                                        old_dst = *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask];

                                        *(uint32_t*)&svga->vram[((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask] = (src & 0xffffff) | (old_dst & 0xff000000);
                                        svga->changedvram[(((mystique->dwgreg.ydst_lin + x) * 3) & mystique->vram_mask) >> 12] = changeframecount;
                                        break;

                                case MACCESS_PWIDTH_32:
                                        src = ((uint32_t*)svga->vram)[src_addr & mystique->vram_mask_l];

                                        ((uint32_t*)svga->vram)[(mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l] = src;
                                        svga->changedvram[((mystique->dwgreg.ydst_lin + x) & mystique->vram_mask_l) >> 10] = changeframecount;
                                        break;

#ifndef RELEASE_BUILD
                                default:
                                        fatal("BITBLT RPL BFCOL PWIDTH %x %08x\n", mystique->maccess_running & MACCESS_PWIDTH_MASK, mystique->dwgreg.dwgctrl_running);
#endif
                                }
                        }

                        if (src_addr == mystique->dwgreg.ar[0])
                        {
                                mystique->dwgreg.ar[0] += mystique->dwgreg.ar[5];
                                mystique->dwgreg.ar[3] += mystique->dwgreg.ar[5];
                                src_addr = mystique->dwgreg.ar[3];
                                break;
                        }
                        else
                                src_addr += x_dir;

                        if (x != x_end)
                                x += x_dir;
                        else
                                break;
                }

                if (mystique->dwgreg.sgn.sdy)
                        mystique->dwgreg.ydst_lin -= (mystique->dwgreg.pitch & PITCH_MASK);
                else
                        mystique->dwgreg.ydst_lin += (mystique->dwgreg.pitch & PITCH_MASK);
        }

        mystique->blitter_complete_refcount++;
}

static void blit_iload(mystique_t* mystique)
{
//        if (mystique->maccess_running & MACCESS_TLUTLOAD)
//                fatal("ILOAD TLUTLOAD\n");
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
        case DWGCTRL_ATYPE_RSTR:
        case DWGCTRL_ATYPE_BLK:
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BFCOL:
                case DWGCTRL_BLTMOD_BMONOLEF:
                case DWGCTRL_BLTMOD_BMONOWF:
                case DWGCTRL_BLTMOD_BU24RGB:
                case DWGCTRL_BLTMOD_BU32RGB:
//                        pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
//                        pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                        mystique->dwgreg.length_cur = mystique->dwgreg.length;
                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                        mystique->dwgreg.iload_rem_data = 0;
                        mystique->dwgreg.iload_rem_count = 0;
                        mystique->busy = 1;
                        mystique->dwgreg.words = 0;
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("ILOAD DWGCTRL_ATYPE_RPL %08x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK, mystique->dwgreg.dwgctrl_running);
                        break;
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown ILOAD atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
}

static void blit_idump(mystique_t* mystique)
{
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
//                pclog("ar0=%08x ar3=%08x ar5=%08x\n", mystique->dwgreg.ar[0], mystique->dwgreg.ar[3], mystique->dwgreg.ar[5]);
//                pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
                mystique->dwgreg.length_cur = mystique->dwgreg.length;
                mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                mystique->dwgreg.src_addr = mystique->dwgreg.ar[3];
                mystique->dwgreg.words = 0;
                mystique->dwgreg.iload_rem_count = 0;
                mystique->dwgreg.iload_rem_data = 0;
                mystique->dwgreg.idump_end_of_line = 0;
                mystique->busy = 1;
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown IDUMP atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
}

static void blit_iload_scale(mystique_t* mystique)
{
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BUYUV:
/*                        pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
                        pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                        pclog("ar0=%08x\n", mystique->dwgreg.ar[0]);
                        pclog("ar2=%08x\n", mystique->dwgreg.ar[2]);
                        pclog("ar3=%08x\n", mystique->dwgreg.ar[3]);
                        pclog("ar5=%08x\n", mystique->dwgreg.ar[5]);
                        pclog("ar6=%08x\n", mystique->dwgreg.ar[6]);*/
                        mystique->dwgreg.length_cur = mystique->dwgreg.length;
                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                        mystique->dwgreg.iload_rem_data = 0;
                        mystique->dwgreg.iload_rem_count = 0;
                        mystique->busy = 1;
                        mystique->dwgreg.words = 0;
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("ILOAD_SCALE DWGCTRL_ATYPE_RPL %08x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK, mystique->dwgreg.dwgctrl_running);
                        break;
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown ILOAD_SCALE atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
}
static void blit_iload_high(mystique_t* mystique)
{
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BUYUV:
/*                        pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
                        pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                        pclog("ar0=%08x\n", mystique->dwgreg.ar[0]);
                        pclog("ar2=%08x\n", mystique->dwgreg.ar[2]);
                        pclog("ar3=%08x\n", mystique->dwgreg.ar[3]);
                        pclog("ar5=%08x\n", mystique->dwgreg.ar[5]);
                        pclog("ar6=%08x\n", mystique->dwgreg.ar[6]);*/
                        mystique->dwgreg.length_cur = mystique->dwgreg.length;
                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                        mystique->dwgreg.iload_rem_data = 0;
                        mystique->dwgreg.iload_rem_count = 0;
                        mystique->busy = 1;
                        mystique->dwgreg.words = 0;
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("ILOAD_HIGH DWGCTRL_ATYPE_RPL %08x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK, mystique->dwgreg.dwgctrl_running);
                        break;
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown ILOAD_HIGH atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
}
static void blit_iload_highv(mystique_t* mystique)
{
        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK)
        {
        case DWGCTRL_ATYPE_RPL:
                switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK)
                {
                case DWGCTRL_BLTMOD_BUYUV:
/*                        pclog("fxleft=%i fxright=%i\n", mystique->dwgreg.fxleft, mystique->dwgreg.fxright);
                        pclog("ytop=%i length=%i\n", mystique->dwgreg.ytop, mystique->dwgreg.length);
                        pclog("ar0=%08x\n", mystique->dwgreg.ar[0]);
                        pclog("ar2=%08x\n", mystique->dwgreg.ar[2]);
                        pclog("ar3=%08x\n", mystique->dwgreg.ar[3]);
                        pclog("ar5=%08x\n", mystique->dwgreg.ar[5]);
                        pclog("ar6=%08x\n", mystique->dwgreg.ar[6]);*/
                        mystique->dwgreg.length_cur = mystique->dwgreg.length;
                        mystique->dwgreg.xdst = mystique->dwgreg.fxleft;
                        mystique->dwgreg.iload_rem_data = 0;
                        mystique->dwgreg.iload_rem_count = 0;
                        mystique->busy = 1;
                        mystique->dwgreg.words = 0;
                        mystique->dwgreg.highv_line = 0;
                        mystique->dwgreg.lastpix_r = 0;
                        mystique->dwgreg.lastpix_g = 0;
                        mystique->dwgreg.lastpix_b = 0;
                        break;

#ifndef RELEASE_BUILD
                default:
                        fatal("ILOAD_HIGHV DWGCTRL_ATYPE_RPL %08x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_BLTMOD_MASK, mystique->dwgreg.dwgctrl_running);
                        break;
#endif
                }
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("Unknown ILOAD_HIGHV atype %03x %08x\n", mystique->dwgreg.dwgctrl_running & DWGCTRL_ATYPE_MASK, mystique->dwgreg.dwgctrl_running);
#endif
        }
}

static void mystique_start_blit(mystique_t* mystique)
{
        uint64_t start_time = timer_read();
        uint64_t end_time;

        mystique->dwgreg.dwgctrl_running = mystique->dwgreg.dwgctrl;
        mystique->maccess_running = mystique->maccess;
//        pclog("start_blit %08x  length=%i fxleft=%i fxright=%i %02x %i %i %i ylin=%08x ytop=%08x ybot=%08x\n", mystique->dwgreg.dwgctrl_running, mystique->dwgreg.length, mystique->dwgreg.fxleft, mystique->dwgreg.fxright, mystique->svga.vram[0x5c056], mystique->blitter_submit_refcount, mystique->blitter_submit_dma_refcount, mystique->blitter_complete_refcount, mystique->dwgreg.ydst_lin, mystique->dwgreg.ytop, mystique->dwgreg.ybot);
#ifndef RELEASE_BUILD
        if (mystique->busy)
                fatal("mystique_start_blit: mystique->busy!\n");
#endif
        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_SOLID)
        {
                int x, y;

                for (y = 0; y < 8; y++)
                {
                        for (x = 0; x < 8; x++)
                                mystique->dwgreg.pattern[y][x] = 1;
                }
                mystique->dwgreg.src[0] = 0xffffffff;
                mystique->dwgreg.src[1] = 0xffffffff;
                mystique->dwgreg.src[2] = 0xffffffff;
                mystique->dwgreg.src[3] = 0xffffffff;
        }
        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_ARZERO)
        {
                mystique->dwgreg.ar[0] = 0;
                mystique->dwgreg.ar[1] = 0;
                mystique->dwgreg.ar[2] = 0;
                mystique->dwgreg.ar[4] = 0;
                mystique->dwgreg.ar[5] = 0;
                mystique->dwgreg.ar[6] = 0;
        }
        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_SGNZERO)
        {
                mystique->dwgreg.sgn.sdydxl = 0;
                mystique->dwgreg.sgn.scanleft = 0;
                mystique->dwgreg.sgn.sdxl = 0;
                mystique->dwgreg.sgn.sdy = 0;
                mystique->dwgreg.sgn.sdxr = 0;
        }
        if (mystique->dwgreg.dwgctrl_running & DWGCTRL_SHTZERO)
        {
                mystique->dwgreg.funcnt = 0;
                mystique->dwgreg.stylelen = 0;
                mystique->dwgreg.xoff = 0;
                mystique->dwgreg.yoff = 0;
        }

        switch (mystique->dwgreg.dwgctrl_running & DWGCTRL_OPCODE_MASK)
        {
        case DWGCTRL_OPCODE_LINE_OPEN:
                blit_line(mystique, 0);
                break;

        case DWGCTRL_OPCODE_LINE_CLOSE:
                blit_line(mystique, 0);
                break;

        case DWGCTRL_OPCODE_AUTOLINE_OPEN:
                blit_autoline(mystique, 0);
                break;

        case DWGCTRL_OPCODE_AUTOLINE_CLOSE:
                blit_autoline(mystique, 1);
                break;

        case DWGCTRL_OPCODE_TRAP:
                blit_trap(mystique);
                break;

        case DWGCTRL_OPCODE_TEXTURE_TRAP:
                blit_texture_trap(mystique);
                break;

        case DWGCTRL_OPCODE_ILOAD_HIGH:
                blit_iload_high(mystique);
                break;

        case DWGCTRL_OPCODE_BITBLT:
                blit_bitblt(mystique);
                break;

        case DWGCTRL_OPCODE_FBITBLT:
                blit_fbitblt(mystique);
                break;

        case DWGCTRL_OPCODE_ILOAD:
                blit_iload(mystique);
                break;

        case DWGCTRL_OPCODE_IDUMP:
                blit_idump(mystique);
                break;

        case DWGCTRL_OPCODE_ILOAD_SCALE:
                blit_iload_scale(mystique);
                break;

        case DWGCTRL_OPCODE_ILOAD_HIGHV:
                blit_iload_highv(mystique);
                break;

#ifndef RELEASE_BUILD
        default:
                fatal("mystique_start_blit: unknown blit %08x\n", mystique->dwgreg.dwgctrl_running);
#endif
        }
//        pclog("blit done\n");

        end_time = timer_read();
        mystique->blitter_time += end_time - start_time;
}

static void mystique_hwcursor_draw(svga_t* svga, int displine)
{
        mystique_t* mystique = (mystique_t*)svga->p;

        if (mystique->type == MGA_2064W)
        {
                tvp3026_hwcursor_draw(&mystique->tvp3026, svga, displine);
        }
        else
        {
                int x;
                uint64_t dat[2];
                int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;

                dat[0] = *(uint64_t*)(&svga->vram[svga->hwcursor_latch.addr]);
                dat[1] = *(uint64_t*)(&svga->vram[svga->hwcursor_latch.addr + 8]);
                svga->hwcursor_latch.addr += 16;
                switch (mystique->xcurctrl & XCURCTRL_CURMODE_MASK)
                {
                case XCURCTRL_CURMODE_XGA:
                        for (x = 0; x < 64; x++)
                        {
                                if (!(dat[1] & (1ull << 63)))
                                        ((uint32_t*)buffer32->line[displine])[offset + 32] = (dat[0] & (1ull << 63)) ? mystique->cursor.col[1] : mystique->cursor.col[0];
                                else if (dat[0] & (1ull << 63))
                                        ((uint32_t*)buffer32->line[displine])[offset + 32] ^= 0xffffff;

                                offset++;
                                dat[0] <<= 1;
                                dat[1] <<= 1;
                        }
                        break;
                }
        }
}

static uint8_t mystique_pci_read(int func, int addr, void* p)
{
        mystique_t* mystique = (mystique_t*)p;

//        pclog("mystique PCI read %08X\n", addr);
        switch (addr)
        {
        case 0x00:
                return 0x2b; /*Matrox*/
        case 0x01:
                return 0x10;

        case 0x02:
                return (mystique->type == MGA_2064W) ? 0x19 : 0x1a; /*MGA-1064SG*/
        case 0x03:
                return 0x05;

        case PCI_REG_COMMAND:
                return mystique->pci_regs[PCI_REG_COMMAND]; /*Respond to IO and memory accesses*/

        case 0x07:
                return 0 << 1; /*Fast DEVSEL timing*/

        case 0x08:
                return 0; /*Revision ID*/
        case 0x09:
                return 0; /*Programming interface*/

        case 0x0a:
                return 0x00; /*Supports VGA interface*/
        case 0x0b:
                return 0x03;

        case 0x10:
                return 0x00; /*Control aperture*/
        case 0x11:
                return (mystique->ctrl_base >> 8) & 0xc0;
        case 0x12:
                return mystique->ctrl_base >> 16;
        case 0x13:
                return mystique->ctrl_base >> 24;

        case 0x14:
                return 0x00; /*Linear frame buffer*/
        case 0x16:
                return (mystique->lfb_base >> 16) & 0x80;
        case 0x17:
                return mystique->lfb_base >> 24;

        case 0x18:
                return 0x00; /*Pseudo-DMA (ILOAD)*/
        case 0x1a:
                return (mystique->iload_base >> 16) & 0x80;
        case 0x1b:
                return mystique->iload_base >> 24;

        case 0x30:
                return mystique->pci_regs[0x30] & 0x01; /*BIOS ROM address*/
        case 0x31:
                return 0x00;
        case 0x32:
                return mystique->pci_regs[0x32];
        case 0x33:
                return mystique->pci_regs[0x33];

        case 0x3c:
                return mystique->int_line;
        case 0x3d:
                return PCI_INTA;

        case 0x40:
                return mystique->pci_regs[0x40];
        case 0x41:
                return mystique->pci_regs[0x41];
        case 0x42:
                return mystique->pci_regs[0x42];
        case 0x43:
                return mystique->pci_regs[0x43];

        case 0x44:
                return mystique->pci_regs[0x44];
        case 0x45:
                return mystique->pci_regs[0x45];

        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
                addr = (mystique->pci_regs[0x44] & 0xfc) |
                       ((mystique->pci_regs[0x45] & 0x3f) << 8) |
                       (addr & 3);
                return mystique_ctrl_read_b(addr, mystique);
        }
        return 0;
}

static void mystique_pci_write(int func, int addr, uint8_t val, void* p)
{
        mystique_t* mystique = (mystique_t*)p;

//        pclog("mystique_pci_write: func=%i addr=%02x val=%02x\n", func, addr, val);
        switch (addr)
        {
        case PCI_REG_COMMAND:
                mystique->pci_regs[PCI_REG_COMMAND] = val & 0x23;
                mystique_recalc_mapping(mystique);
                break;

        case 0x11:
                mystique->ctrl_base = (mystique->ctrl_base & 0xffff0000) | ((val & 0xc0) << 8);
                mystique_recalc_mapping(mystique);
                break;
        case 0x12:
                mystique->ctrl_base = (mystique->ctrl_base & 0xff00c000) | (val << 16);
                mystique_recalc_mapping(mystique);
                break;
        case 0x13:
                mystique->ctrl_base = (mystique->ctrl_base & 0x00ffc000) | (val << 24);
                mystique_recalc_mapping(mystique);
                break;

        case 0x16:
                mystique->lfb_base = (mystique->lfb_base & 0xff000000) | ((val & 0x80) << 16);
                mystique_recalc_mapping(mystique);
                break;
        case 0x17:
                mystique->lfb_base = (mystique->lfb_base & 0x00800000) | (val << 24);
                mystique_recalc_mapping(mystique);
                break;

        case 0x1a:
                mystique->iload_base = (mystique->iload_base & 0xff000000) | ((val & 0x80) << 16);
                mystique_recalc_mapping(mystique);
                break;
        case 0x1b:
                mystique->iload_base = (mystique->iload_base & 0x00800000) | (val << 24);
                mystique_recalc_mapping(mystique);
                break;

        case 0x30:
        case 0x32:
        case 0x33:
                mystique->pci_regs[addr] = val;
                if (mystique->pci_regs[0x30] & 0x01)
                {
                        uint32_t addr = (mystique->pci_regs[0x32] << 16) | (mystique->pci_regs[0x33] << 24);
                        mem_mapping_set_addr(&mystique->bios_rom.mapping, addr, 0x8000);
                }
                else
                        mem_mapping_disable(&mystique->bios_rom.mapping);
                return;

        case 0x3c:
                mystique->int_line = val;
                return;

        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
                mystique->pci_regs[addr] = val;
                break;

        case 0x4c:
        case 0x4d:
        case 0x4e:
        case 0x4f:
                mystique->pci_regs[addr - 0x20] = val;
                break;

        case 0x44:
        case 0x45:
                mystique->pci_regs[addr] = val;
                break;

        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
                addr = (mystique->pci_regs[0x44] & 0xfc) |
                       ((mystique->pci_regs[0x45] & 0x3f) << 8) |
                       (addr & 3);
                mystique_ctrl_write_b(addr, val, mystique);
                break;
        }
}

void* mga_init_common(char* bios_fn, int type)
{
        int c;
        mystique_t* mystique = malloc(sizeof(mystique_t));
        memset(mystique, 0, sizeof(mystique_t));

        rom_init(&mystique->bios_rom, bios_fn, 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

        mystique->type = type;
        mystique->vram_size = device_get_config_int("memory");
        mystique->vram_mask = (mystique->vram_size << 20) - 1;
        mystique->vram_mask_w = mystique->vram_mask >> 1;
        mystique->vram_mask_l = mystique->vram_mask >> 2;

        svga_init(&mystique->svga, mystique, mystique->vram_size << 20,
                mystique_recalctimings,
                mystique_in, mystique_out,
                mystique_hwcursor_draw,
                NULL);

        io_sethandler(0x03c0, 0x0020, mystique_in, NULL, NULL, mystique_out, NULL, NULL, mystique);
        mem_mapping_add(&mystique->ctrl_mapping, 0, 0,
                mystique_ctrl_read_b, NULL, mystique_ctrl_read_l,
                mystique_ctrl_write_b, NULL, mystique_ctrl_write_l,
                NULL, 0, mystique);
        mem_mapping_add(&mystique->lfb_mapping, 0, 0,
                mystique_readb_linear, mystique_readw_linear, mystique_readl_linear,
                mystique_writeb_linear, mystique_writew_linear, mystique_writel_linear,
                NULL, 0, mystique);
        mem_mapping_add(&mystique->iload_mapping, 0, 0,
                mystique_iload_read_b, NULL, mystique_iload_read_l,
                mystique_iload_write_b, NULL, mystique_iload_write_l,
                NULL, 0, mystique);

        mystique->card = pci_add(mystique_pci_read, mystique_pci_write, mystique);
        mystique->pci_regs[0x2c] = mystique->bios_rom.rom[0x7ff8];
        mystique->pci_regs[0x2d] = mystique->bios_rom.rom[0x7ff8];
        mystique->pci_regs[0x2e] = mystique->bios_rom.rom[0x7ff8];
        mystique->pci_regs[0x2f] = mystique->bios_rom.rom[0x7ff8];

        mystique->svga.miscout = 1;
        mystique->pci_regs[0x41] = 0x01; /*vgaboot=1*/

//        timer_add(&mystique->dma.timer, dma_callback, mystique, 1);

        for (c = 0; c < 256; c++)
        {
                dither5[c][0][0] = c >> 3;
                dither5[c][1][1] = (c + 2) >> 3;
                dither5[c][1][0] = (c + 4) >> 3;
                dither5[c][0][1] = (c + 6) >> 3;

                if (dither5[c][1][1] > 31)
                        dither5[c][1][1] = 31;
                if (dither5[c][1][0] > 31)
                        dither5[c][1][0] = 31;
                if (dither5[c][0][1] > 31)
                        dither5[c][0][1] = 31;

                dither6[c][0][0] = c >> 2;
                dither6[c][1][1] = (c + 1) >> 2;
                dither6[c][1][0] = (c + 2) >> 2;
                dither6[c][0][1] = (c + 3) >> 2;

                if (dither6[c][1][1] > 63)
                        dither6[c][1][1] = 63;
                if (dither6[c][1][0] > 63)
                        dither6[c][1][0] = 63;
                if (dither6[c][0][1] > 63)
                        dither6[c][0][1] = 63;
        }

        mystique->wake_fifo_thread = thread_create_event();
        mystique->fifo_not_full_event = thread_create_event();
        mystique->fifo_thread = thread_create(fifo_thread, mystique);
        mystique->dma.lock = thread_create_mutex();

        timer_add(&mystique->wake_timer, mystique_wake_timer, (void*)mystique, 0);
        timer_add(&mystique->softrap_pending_timer, mystique_softrap_pending_timer, (void*)mystique, 1);

        mystique->status = STATUS_ENDPRDMASTS;

        mystique->svga.vsync_callback = mystique_vsync_callback;

        return mystique;
}

void* millennium_init()
{
        mystique_t* mystique = mga_init_common("matroxisstormr2.BIN", MGA_2064W);

        tvp3026_init(&mystique->tvp3026);
        tvp3026_set_cursor_enable(&mystique->tvp3026, &mystique->svga, 1);

        return mystique;
}

void* mystique_init()
{
        return mga_init_common("MYSTIQUE.VBI", MGA_1064SG);
}

static int millennium_available()
{
        return rom_present("matroxisstormr2.BIN");
}

static int mystique_available()
{
        return rom_present("MYSTIQUE.VBI");
}

static void mystique_close(void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        thread_kill(mystique->fifo_thread);
        thread_destroy_event(mystique->wake_fifo_thread);
        thread_destroy_event(mystique->fifo_not_full_event);
        thread_destroy_mutex(mystique->dma.lock);

        svga_close(&mystique->svga);

        free(mystique);
}

static void mystique_speed_changed(void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        svga_recalctimings(&mystique->svga);
}

static void mystique_force_redraw(void* p)
{
        mystique_t* mystique = (mystique_t*)p;

        mystique->svga.fullchange = changeframecount;
}

static void mystique_add_status_info(char* s, int max_len, void* p)
{
        mystique_t* mystique = (mystique_t*)p;
        char temps[256];
        uint64_t new_time = timer_read();
        uint64_t status_diff = new_time - mystique->status_time;
        mystique->status_time = new_time;

        if (!status_diff)
                status_diff = 1;

        svga_add_status_info(s, max_len, &mystique->svga);
        sprintf(temps, "In %s mode\n%f Mpixels/sec\n%f ktraps/sec\n%f%% CPU\n%f%% CPU (real)\n\n",
                (mystique->crtcext_regs[3] & CRTCX_R3_MGAMODE) ? "MGA" : "SVGA",
                (double)mystique->pixel_count / 1000000.0,
                (double)mystique->trap_count / 1000.0,
                ((double)mystique->blitter_time * 100.0) / timer_freq,
                ((double)mystique->blitter_time * 100.0) / status_diff);
        strncat(s, temps, max_len);

        mystique->pixel_count = mystique->trap_count = 0;
        mystique->blitter_time = 0;
}

static device_config_t millennium_config[] =
        {
                {
                        .name = "memory",
                        .description = "Memory size",
                        .type = CONFIG_SELECTION,
                        .selection =
                                {
                                        {
                                                .description = "2 MB",
                                                .value = 2
                                        },
                                        {
                                                .description = "4 MB",
                                                .value = 4
                                        },
                                        {
                                                .description = "8 MB",
                                                .value = 8
                                        },
                                        {
                                                .description = ""
                                        }
                                },
                        .default_int = 4
                },
                {
                        .type = -1
                }
        };

static device_config_t mystique_config[] =
        {
                {
                        .name = "memory",
                        .description = "Memory size",
                        .type = CONFIG_SELECTION,
                        .selection =
                                {
                                        {
                                                .description = "2 MB",
                                                .value = 2
                                        },
                                        {
                                                .description = "4 MB",
                                                .value = 4
                                        },
                                        {
                                                .description = ""
                                        }
                                },
                        .default_int = 4
                },
                {
                        .type = -1
                }
        };

device_t millennium_device =
        {
                "Matrox Millennium",
                0,
                millennium_init,
                mystique_close,
                millennium_available,
                mystique_speed_changed,
                mystique_force_redraw,
                mystique_add_status_info,
                millennium_config
        };

device_t mystique_device =
        {
                "Matrox Mystique",
                DEVICE_NOT_WORKING,
                mystique_init,
                mystique_close,
                mystique_available,
                mystique_speed_changed,
                mystique_force_redraw,
                mystique_add_status_info,
                mystique_config
        };
