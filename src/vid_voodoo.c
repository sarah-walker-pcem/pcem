#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "mem.h"
#include "pci.h"
#include "thread.h"
#include "timer.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_voodoo.h"

#define RB_SIZE 16384
#define RB_MASK (RB_SIZE - 1)

#define RB_ENTRIES (voodoo->params_write_idx - voodoo->params_read_idx)
#define RB_FULL (voodoo->params_buffer[voodoo->params_write_idx & RB_MASK].command != CMD_INVALID)
#define RB_EMPTY (voodoo->params_buffer[voodoo->params_read_idx & RB_MASK].command == CMD_INVALID)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define CLAMP(x) (((x) < 0) ? 0 : (((x) > 0xff) ? 0xff : (x)))
#define CLAMP16(x) (((x) < 0) ? 0 : (((x) > 0xffff) ? 0xffff : (x)))

#define LOD_MAX 8

static int tris = 0;

static uint64_t status_time = 0;
static uint64_t voodoo_time = 0;

typedef union int_float
{
        uint32_t i;
        float f;
} int_float;

typedef struct rgb_t
{
        uint8_t r, g, b;
} rgb_t;

typedef union rgba_u
{
        struct
        {
                uint8_t b, g, r, a;
        } rgba;
        uint32_t u;
} rgba_u;

static rgba_u rgb332[0x100], rgb565[0x10000], argb1555[0x10000], argb4444[0x10000];

typedef struct voodoo_params_t
{
        int command;

        int32_t vertexAx, vertexAy, vertexBx, vertexBy, vertexCx, vertexCy;

        uint32_t startR, startG, startB, startZ, startA;
        
         int32_t dRdX, dGdX, dBdX, dZdX, dAdX;
        
         int32_t dRdY, dGdY, dBdY, dZdY, dAdY;

        int64_t startW, dWdX, dWdY;

        struct
        {
                int64_t startS, dSdX, dSdY;
                int64_t startT, dTdX, dTdY;
                int64_t startW, dWdX, dWdY;
        } tmu[1];

        uint32_t color0, color1;

        uint32_t fbzMode;
        uint32_t fbzColorPath;
        
        uint32_t fogMode;
        rgb_t fogColor;
        struct
        {
                uint8_t fog, dfog;
        } fogTable[64];

        uint32_t alphaMode;
        
        uint32_t zaColor;
        
        int chromaKey_r, chromaKey_g, chromaKey_b;

        uint32_t textureMode;
        uint32_t tLOD;

        uint32_t texBaseAddr, texBaseAddr1, texBaseAddr2, texBaseAddr38;
        
        uint32_t tex_base[LOD_MAX+1];
        int tex_width;
        int tex_w_mask[LOD_MAX+1];
        int tex_h_mask[LOD_MAX+1];
        int tex_shift[LOD_MAX+1];
        
        uint32_t draw_offset, aux_offset;

        int tformat;
        
        int clipLeft, clipRight, clipLowY, clipHighY;
        
        int sign;
        
        uint32_t front_offset;
        
        uint32_t swapbufferCMD;

        rgba_u palette[256];
} voodoo_params_t;

typedef struct voodoo_t
{
        mem_mapping_t mmio_mapping;
        mem_mapping_t   fb_mapping;
        mem_mapping_t  tex_mapping;
        
        int pci_enable;

        uint8_t dac_data[8];
        int dac_reg;
        uint8_t dac_readdata;
        
        voodoo_params_t params;
        
        uint32_t fbiInit0, fbiInit1, fbiInit2, fbiInit3, fbiInit4;
        
        uint8_t initEnable;
        
        uint32_t lfbMode;
        
        uint32_t memBaseAddr;

        int_float fvertexAx, fvertexAy, fvertexBx, fvertexBy, fvertexCx, fvertexCy;

        int_float fstartR, fstartG, fstartB, fstartZ, fstartA, fstartS, fstartT, fstartW;

        int_float fdRdX, fdGdX, fdBdX, fdZdX, fdAdX;
        
        int_float fdRdY, fdGdY, fdBdY, fdZdY, fdAdY;

        uint32_t front_offset, back_offset;
        
        uint32_t fb_read_offset, fb_write_offset;
        
        int row_width;
        
        uint8_t *fb_mem, *tex_mem;
        uint16_t *tex_mem_w;
               
        int rgb_sel;
        
        uint32_t trexInit1;
        
        int swap_count;
        
        int disp_buffer;
        int timer_count;
        
        int line;
        svga_t *svga;
        
        uint32_t backPorch;
        uint32_t videoDimensions;
        uint32_t hSync, vSync;
        
        int v_total, v_disp;
        int h_disp;

        struct
        {
                uint32_t y[4], i[4], q[4];
        } nccTable[2];

        rgba_u palette[256];
        
        rgba_u ncc_lookup[2][256];

        voodoo_params_t params_buffer[RB_SIZE];
        int params_read_idx, params_write_idx;
        int params_busy;

        thread_t *render_thread;
        event_t *wake_render_thread;
        event_t *wake_main_thread;
        event_t *not_full_event;
        
        int voodoo_busy;
        
        int pixel_count, tri_count;
        
        int retrace_count;
        int swap_interval;
        uint32_t swap_offset;
        int swap_pending;
        
        int bilinear_enabled;
} voodoo_t;

enum
{
        SST_status = 0x000,
        
        SST_vertexAx = 0x008,
        SST_vertexAy = 0x00c,
        SST_vertexBx = 0x010,
        SST_vertexBy = 0x014,
        SST_vertexCx = 0x018,
        SST_vertexCy = 0x01c,
        
        SST_startR   = 0x0020,
        SST_startG   = 0x0024,
        SST_startB   = 0x0028,
        SST_startZ   = 0x002c,
        SST_startA   = 0x0030,
        SST_startS   = 0x0034,
        SST_startT   = 0x0038,
        SST_startW   = 0x003c,

        SST_dRdX     = 0x0040,
        SST_dGdX     = 0x0044,
        SST_dBdX     = 0x0048,
        SST_dZdX     = 0x004c,
        SST_dAdX     = 0x0050,
        SST_dSdX     = 0x0054,
        SST_dTdX     = 0x0058,
        SST_dWdX     = 0x005c,
        
        SST_dRdY     = 0x0060,
        SST_dGdY     = 0x0064,
        SST_dBdY     = 0x0068,
        SST_dZdY     = 0x006c,
        SST_dAdY     = 0x0070,
        SST_dSdY     = 0x0074,
        SST_dTdY     = 0x0078,
        SST_dWdY     = 0x007c,
        
        SST_triangleCMD = 0x0080,
        
        SST_fvertexAx = 0x088,
        SST_fvertexAy = 0x08c,
        SST_fvertexBx = 0x090,
        SST_fvertexBy = 0x094,
        SST_fvertexCx = 0x098,
        SST_fvertexCy = 0x09c,
        
        SST_fstartR   = 0x00a0,
        SST_fstartG   = 0x00a4,
        SST_fstartB   = 0x00a8,
        SST_fstartZ   = 0x00ac,
        SST_fstartA   = 0x00b0,
        SST_fstartS   = 0x00b4,
        SST_fstartT   = 0x00b8,
        SST_fstartW   = 0x00bc,

        SST_fdRdX     = 0x00c0,
        SST_fdGdX     = 0x00c4,
        SST_fdBdX     = 0x00c8,
        SST_fdZdX     = 0x00cc,
        SST_fdAdX     = 0x00d0,
        SST_fdSdX     = 0x00d4,
        SST_fdTdX     = 0x00d8,
        SST_fdWdX     = 0x00dc,
        
        SST_fdRdY     = 0x00e0,
        SST_fdGdY     = 0x00e4,
        SST_fdBdY     = 0x00e8,
        SST_fdZdY     = 0x00ec,
        SST_fdAdY     = 0x00f0,
        SST_fdSdY     = 0x00f4,
        SST_fdTdY     = 0x00f8,
        SST_fdWdY     = 0x00fc,
        
        SST_ftriangleCMD = 0x0100,

        SST_fbzColorPath = 0x104,
        SST_fogMode = 0x108,

        SST_alphaMode = 0x10c,        
        SST_fbzMode = 0x110,
        SST_lfbMode = 0x114,
        
        SST_clipLeftRight = 0x118,
        SST_clipLowYHighY = 0x11c,
        
        SST_fastfillCMD = 0x124,
        SST_swapbufferCMD = 0x128,

        SST_fogColor = 0x12c,        
        SST_zaColor = 0x130,
        SST_chromaKey = 0x134,
                
        SST_color0 = 0x144,
        SST_color1 = 0x148,
        
        SST_fogTable00 = 0x160,
        SST_fogTable01 = 0x164,
        SST_fogTable02 = 0x168,
        SST_fogTable03 = 0x16c,
        SST_fogTable04 = 0x170,
        SST_fogTable05 = 0x174,
        SST_fogTable06 = 0x178,
        SST_fogTable07 = 0x17c,
        SST_fogTable08 = 0x180,
        SST_fogTable09 = 0x184,
        SST_fogTable0a = 0x188,
        SST_fogTable0b = 0x18c,
        SST_fogTable0c = 0x190,
        SST_fogTable0d = 0x194,
        SST_fogTable0e = 0x198,
        SST_fogTable0f = 0x19c,
        SST_fogTable10 = 0x1a0,
        SST_fogTable11 = 0x1a4,
        SST_fogTable12 = 0x1a8,
        SST_fogTable13 = 0x1ac,
        SST_fogTable14 = 0x1b0,
        SST_fogTable15 = 0x1b4,
        SST_fogTable16 = 0x1b8,
        SST_fogTable17 = 0x1bc,
        SST_fogTable18 = 0x1c0,
        SST_fogTable19 = 0x1c4,
        SST_fogTable1a = 0x1c8,
        SST_fogTable1b = 0x1cc,
        SST_fogTable1c = 0x1d0,
        SST_fogTable1d = 0x1d4,
        SST_fogTable1e = 0x1d8,
        SST_fogTable1f = 0x1dc,
        
        SST_fbiInit4 = 0x200,

        SST_backPorch = 0x208,
        SST_videoDimensions = 0x20c,
        SST_fbiInit0 = 0x210,
        SST_fbiInit1 = 0x214,
        SST_fbiInit2 = 0x218,
        SST_fbiInit3 = 0x21c,
        SST_hSync = 0x220,
        SST_vSync = 0x224,
        SST_dacData = 0x22c,

        SST_textureMode = 0x300,
        SST_tLOD = 0x304,
        
        SST_texBaseAddr = 0x30c,
        SST_texBaseAddr1 = 0x310,
        SST_texBaseAddr2 = 0x314,
        SST_texBaseAddr38 = 0x318,
        
        SST_trexInit1 = 0x320,
        
        SST_nccTable0_Y0 = 0x324,
        SST_nccTable0_Y1 = 0x328,
        SST_nccTable0_Y2 = 0x32c,
        SST_nccTable0_Y3 = 0x330,
        SST_nccTable0_I0 = 0x334,
        SST_nccTable0_I1 = 0x338,
        SST_nccTable0_I2 = 0x33c,
        SST_nccTable0_I3 = 0x340,
        SST_nccTable0_Q0 = 0x344,
        SST_nccTable0_Q1 = 0x348,
        SST_nccTable0_Q2 = 0x34c,
        SST_nccTable0_Q3 = 0x350,
        
        SST_nccTable1_Y0 = 0x354,
        SST_nccTable1_Y1 = 0x358,
        SST_nccTable1_Y2 = 0x35c,
        SST_nccTable1_Y3 = 0x360,
        SST_nccTable1_I0 = 0x364,
        SST_nccTable1_I1 = 0x368,
        SST_nccTable1_I2 = 0x36c,
        SST_nccTable1_I3 = 0x370,
        SST_nccTable1_Q0 = 0x374,
        SST_nccTable1_Q1 = 0x378,
        SST_nccTable1_Q2 = 0x37c,
        SST_nccTable1_Q3 = 0x380,

        SST_remap_status = 0x000 | 0x400,
        
        SST_remap_vertexAx = 0x008 | 0x400,
        SST_remap_vertexAy = 0x00c | 0x400,
        SST_remap_vertexBx = 0x010 | 0x400,
        SST_remap_vertexBy = 0x014 | 0x400,
        SST_remap_vertexCx = 0x018 | 0x400,
        SST_remap_vertexCy = 0x01c | 0x400,
        
        SST_remap_startR   = 0x0020 | 0x400,
        SST_remap_startG   = 0x002c | 0x400,
        SST_remap_startB   = 0x0038 | 0x400,
        SST_remap_startZ   = 0x0044 | 0x400,
        SST_remap_startA   = 0x0050 | 0x400,
        SST_remap_startS   = 0x005c | 0x400,
        SST_remap_startT   = 0x0068 | 0x400,
        SST_remap_startW   = 0x0074 | 0x400,

        SST_remap_dRdX     = 0x0024 | 0x400,
        SST_remap_dGdX     = 0x0030 | 0x400,
        SST_remap_dBdX     = 0x003c | 0x400,
        SST_remap_dZdX     = 0x0048 | 0x400,
        SST_remap_dAdX     = 0x0054 | 0x400,
        SST_remap_dSdX     = 0x0060 | 0x400,
        SST_remap_dTdX     = 0x006c | 0x400,
        SST_remap_dWdX     = 0x0078 | 0x400,
        
        SST_remap_dRdY     = 0x0028 | 0x400,
        SST_remap_dGdY     = 0x0034 | 0x400,
        SST_remap_dBdY     = 0x0040 | 0x400,
        SST_remap_dZdY     = 0x004c | 0x400,
        SST_remap_dAdY     = 0x0058 | 0x400,
        SST_remap_dSdY     = 0x0064 | 0x400,
        SST_remap_dTdY     = 0x0070 | 0x400,
        SST_remap_dWdY     = 0x007c | 0x400,
        
        SST_remap_triangleCMD = 0x0080 | 0x400,
        
        SST_remap_fvertexAx = 0x088 | 0x400,
        SST_remap_fvertexAy = 0x08c | 0x400,
        SST_remap_fvertexBx = 0x090 | 0x400,
        SST_remap_fvertexBy = 0x094 | 0x400,
        SST_remap_fvertexCx = 0x098 | 0x400,
        SST_remap_fvertexCy = 0x09c | 0x400,
        
        SST_remap_fstartR   = 0x00a0 | 0x400,
        SST_remap_fstartG   = 0x00ac | 0x400,
        SST_remap_fstartB   = 0x00b8 | 0x400,
        SST_remap_fstartZ   = 0x00c4 | 0x400,
        SST_remap_fstartA   = 0x00d0 | 0x400,
        SST_remap_fstartS   = 0x00dc | 0x400,
        SST_remap_fstartT   = 0x00e8 | 0x400,
        SST_remap_fstartW   = 0x00f4 | 0x400,

        SST_remap_fdRdX     = 0x00a4 | 0x400,
        SST_remap_fdGdX     = 0x00b0 | 0x400,
        SST_remap_fdBdX     = 0x00bc | 0x400,
        SST_remap_fdZdX     = 0x00c8 | 0x400,
        SST_remap_fdAdX     = 0x00d4 | 0x400,
        SST_remap_fdSdX     = 0x00e0 | 0x400,
        SST_remap_fdTdX     = 0x00ec | 0x400,
        SST_remap_fdWdX     = 0x00f8 | 0x400,
        
        SST_remap_fdRdY     = 0x00a8 | 0x400,
        SST_remap_fdGdY     = 0x00b4 | 0x400,
        SST_remap_fdBdY     = 0x00c0 | 0x400,
        SST_remap_fdZdY     = 0x00cc | 0x400,
        SST_remap_fdAdY     = 0x00d8 | 0x400,
        SST_remap_fdSdY     = 0x00e4 | 0x400,
        SST_remap_fdTdY     = 0x00f0 | 0x400,
        SST_remap_fdWdY     = 0x00fc | 0x400,
};

enum
{
        LFB_WRITE_FRONT = 0x0000,
        LFB_WRITE_BACK  = 0x0010,
        LFB_WRITE_MASK  = 0x0030
};

enum
{
        LFB_READ_FRONT = 0x0000,
        LFB_READ_BACK  = 0x0040,
        LFB_READ_AUX   = 0x0080,
        LFB_READ_MASK  = 0x00c0
};

enum
{
        LFB_FORMAT_RGB565 = 0,
        LFB_FORMAT_ARGB8888 = 5,
        LFB_FORMAT_DEPTH = 15,
        LFB_FORMAT_MASK = 15
};

enum
{
        LFB_WRITE_COLOUR = 1,
        LFB_WRITE_DEPTH = 2
};

enum
{
        FBZ_CHROMAKEY = (1 << 1),
        FBZ_W_BUFFER = (1 << 3),
        FBZ_DEPTH_ENABLE = (1 << 4),
        
        FBZ_DITHER  = 0x100,
        FBZ_RGB_WMASK = (1 << 9),
        FBZ_DEPTH_WMASK = (1 << 10),
        
        FBZ_DRAW_FRONT = 0x0000,
        FBZ_DRAW_BACK  = 0x4000,
        FBZ_DRAW_MASK  = 0xc000,
        
        FBZ_PARAM_ADJUST = (1 << 26)
};

enum
{
        TEX_RGB332 = 0x0,
        TEX_Y4I2Q2 = 0x1,
        TEX_A8 = 0x2,
        TEX_I8 = 0x3,
        TEX_AI8 = 0x4,
        TEX_PAL8 = 0x5,
        TEX_R5G6B5 = 0xa,
        TEX_ARGB1555 = 0xb,
        TEX_ARGB4444 = 0xc,
        TEX_A8I8 = 0xd,
        TEX_APAL88 = 0xe
};

enum
{
        TEXTUREMODE_NCC_SEL = (1 << 5),
        TEXTUREMODE_TCLAMPS = (1 << 6),
        TEXTUREMODE_TCLAMPT = (1 << 7)
};

enum
{
        FBIINIT0_VGA_PASS = 1
};

enum
{
        FBIINIT3_REMAP = 1
};

enum
{
        CC_LOCALSELECT_ITER_RGB = 0,
        CC_LOCALSELECT_TEX = 1,
        CC_LOCALSELECT_COLOR1 = 2,
        CC_LOCALSELECT_LFB = 3
};

enum
{
        CCA_LOCALSELECT_ITER_A = 0,
        CCA_LOCALSELECT_COLOR0 = 1,
        CCA_LOCALSELECT_ITER_Z = 2
};

enum
{
        C_SEL_ITER_RGB = 0,
        C_SEL_TEX      = 1,
        C_SEL_COLOR1   = 2,
        C_SEL_LFB      = 3
};

enum
{
        A_SEL_ITER_A = 0,
        A_SEL_TEX    = 1,
        A_SEL_COLOR1 = 2,
        A_SEL_LFB    = 3
};

enum
{
        CC_MSELECT_ZERO   = 0,
        CC_MSELECT_CLOCAL = 1,
        CC_MSELECT_AOTHER = 2,
        CC_MSELECT_ALOCAL = 3,
        CC_MSELECT_TEX    = 4
};

enum
{
        CCA_MSELECT_ZERO    = 0,
        CCA_MSELECT_ALOCAL  = 1,
        CCA_MSELECT_AOTHER  = 2,
        CCA_MSELECT_ALOCAL2 = 3,
        CCA_MSELECT_TEX     = 4
};

enum
{
        CC_ADD_CLOCAL = 1,
        CC_ADD_ALOCAL = 2
};

enum
{
        CCA_ADD_CLOCAL = 1,
        CCA_ADD_ALOCAL = 2
};

enum
{
        AFUNC_AZERO = 0x0,
        AFUNC_ASRC_ALPHA = 0x1,
        AFUNC_A_COLOR = 0x2,
        AFUNC_ADST_ALPHA = 0x3,
        AFUNC_AONE = 0x4,
        AFUNC_AOMSRC_ALPHA = 0x5,
        AFUNC_AOM_COLOR = 0x6,
        AFUNC_AOMDST_ALPHA = 0x7,
        AFUNC_ASATURATE = 0xf
};

enum
{
        AFUNC_ACOLORBEFOREFOG = 0xf
};

enum
{
        AFUNC_NEVER    = 0,
        AFUNC_LESSTHAN = 1,
        AFUNC_EQUAL = 2,
        AFUNC_LESSTHANEQUAL = 3,
        AFUNC_GREATERTHAN = 4,
        AFUNC_NOTEQUAL = 5,
        AFUNC_GREATERTHANEQUAL = 6,
        AFUNC_ALWAYS = 7
};

enum
{
        DEPTHOP_NEVER    = 0,
        DEPTHOP_LESSTHAN = 1,
        DEPTHOP_EQUAL = 2,
        DEPTHOP_LESSTHANEQUAL = 3,
        DEPTHOP_GREATERTHAN = 4,
        DEPTHOP_NOTEQUAL = 5,
        DEPTHOP_GREATERTHANEQUAL = 6,
        DEPTHOP_ALWAYS = 7
};

enum
{
        FOG_ENABLE   = 0x01,
        FOG_ADD      = 0x02,
        FOG_MULT     = 0x04,
        FOG_ALPHA    = 0x08,
        FOG_Z        = 0x10,
        FOG_CONSTANT = 0x20
};

enum
{
        LOD_S_IS_WIDER = (1 << 20)
};
enum
{
        CMD_INVALID = 0,
        CMD_DRAWTRIANGLE,
        CMD_FASTFILL,
        CMD_SWAPBUF
};

static void voodoo_update_ncc(voodoo_t *voodoo)
{
        int tbl;
        
        for (tbl = 0; tbl < 2; tbl++)
        {
                int col;
                
                for (col = 0; col < 256; col++)
                {
                        int y = (col >> 4), i = (col >> 2) & 3, q = col & 3;
                        int _y = (col >> 4), _i = (col >> 2) & 3, _q = col & 3;
                        int i_r, i_g, i_b;
                        int q_r, q_g, q_b;
                        int r, g, b;
                        
                        y = (voodoo->nccTable[tbl].y[y >> 2] >> ((y & 3) * 8)) & 0xff;
                        
                        i_r = (voodoo->nccTable[tbl].i[i] >> 18) & 0x1ff;
                        if (i_r & 0x100)
                                i_r |= 0xfffffe00;
                        i_g = (voodoo->nccTable[tbl].i[i] >> 9) & 0x1ff;
                        if (i_g & 0x100)
                                i_g |= 0xfffffe00;
                        i_b = voodoo->nccTable[tbl].i[i] & 0x1ff;
                        if (i_b & 0x100)
                                i_b |= 0xfffffe00;

                        q_r = (voodoo->nccTable[tbl].q[q] >> 18) & 0x1ff;
                        if (q_r & 0x100)
                                q_r |= 0xfffffe00;
                        q_g = (voodoo->nccTable[tbl].q[q] >> 9) & 0x1ff;
                        if (q_g & 0x100)
                                q_g |= 0xfffffe00;
                        q_b = voodoo->nccTable[tbl].q[q] & 0x1ff;
                        if (q_b & 0x100)
                                q_b |= 0xfffffe00;
                        
                        voodoo->ncc_lookup[tbl][col].rgba.r = CLAMP(y + i_r + q_r);
                        voodoo->ncc_lookup[tbl][col].rgba.g = CLAMP(y + i_g + q_g);
                        voodoo->ncc_lookup[tbl][col].rgba.b = CLAMP(y + i_b + q_b);
                        voodoo->ncc_lookup[tbl][col].rgba.a = 0xff;
                }
        }
}


static void voodoo_recalc(voodoo_t *voodoo)
{
        uint32_t buffer_offset = ((voodoo->fbiInit2 >> 11) & 511) * 4096;
        
        if (voodoo->disp_buffer)
        {
                voodoo->back_offset = 0;
                voodoo->params.front_offset = buffer_offset;
        }
        else
        {
                voodoo->params.front_offset = 0;
                voodoo->back_offset = buffer_offset;
        }
        voodoo->params.aux_offset = buffer_offset * 2;
        
        switch (voodoo->lfbMode & LFB_WRITE_MASK)
        {
                case LFB_WRITE_FRONT:
                voodoo->fb_write_offset = voodoo->params.front_offset;
                break;
                case LFB_WRITE_BACK:
                voodoo->fb_write_offset = voodoo->back_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown lfb destination\n");
        }

        switch (voodoo->lfbMode & LFB_READ_MASK)
        {
                case LFB_READ_FRONT:
                voodoo->fb_read_offset = voodoo->params.front_offset;
                break;
                case LFB_READ_BACK:
                voodoo->fb_read_offset = voodoo->back_offset;
                break;
                case LFB_READ_AUX:
                voodoo->fb_read_offset = voodoo->params.aux_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown lfb source\n");
        }
        
        switch (voodoo->params.fbzMode & FBZ_DRAW_MASK)
        {
                case FBZ_DRAW_FRONT:
                voodoo->params.draw_offset = voodoo->params.front_offset;
                break;
                case FBZ_DRAW_BACK:
                voodoo->params.draw_offset = voodoo->back_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown draw buffer\n");
        }
                
        voodoo->row_width = ((voodoo->fbiInit1 >> 4) & 15) * 64 * 2;
//        pclog("voodoo_recalc : front_offset %08X  back_offset %08X  aux_offset %08X draw_offset %08x\n", voodoo->params.front_offset, voodoo->back_offset, voodoo->params.aux_offset, voodoo->params.draw_offset);
//        pclog("                fb_read_offset %08X  fb_write_offset %08X  row_width %i  %08x %08x\n", voodoo->fb_read_offset, voodoo->fb_write_offset, voodoo->row_width, voodoo->lfbMode, voodoo->params.fbzMode);
}

static void voodoo_recalc_tex(voodoo_t *voodoo)
{
        int aspect = (voodoo->params.tLOD >> 21) & 3;
        int width = 256, height = 256;
        int shift = 8;
        int lod;
        int lod_min = (voodoo->params.tLOD >> 2) & 15;
        int lod_max = (voodoo->params.tLOD >> 8) & 15;
        uint32_t base = voodoo->params.texBaseAddr;
        
        if (voodoo->params.tLOD & LOD_S_IS_WIDER)
                height >>= aspect;
        else
        {
                width >>= aspect;
                shift -= aspect;
        }
        
        for (lod = 0; lod <= LOD_MAX; lod++)
        {
                if (!width)
                        width = 1;
                if (!height)
                        height = 1;
                if (shift < 0)
                        shift = 0;
                voodoo->params.tex_base[lod] = base;
                voodoo->params.tex_w_mask[lod] = width - 1;
                voodoo->params.tex_h_mask[lod] = height - 1;
                voodoo->params.tex_shift[lod] = shift;
//                pclog("LOD%i base=%08x %i-%i %i,%i wm=%02x hm=%02x sh=%i\n", lod, base, lod_min, lod_max, width, height, voodoo->tex_w_mask[lod], voodoo->tex_h_mask[lod], voodoo->tex_shift[lod]);
                
                if (voodoo->params.tformat & 8)
                        base += width * height * 2;
                else
                        base += width * height;

                width >>= 1;
                height >>= 1;
                shift--;
        }
        
        voodoo->params.tex_width = width;
}

static int dither_matrix_4x4[16] =
{
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
};

static int dither_rb[256][4][4];
static int dither_g[256][4][4];

static void voodoo_make_dither()
{
        int c, x, y;
        int seen_rb[1024];
        int seen_g[1024];
        
        for (c = 0; c < 256; c++)
        {
                int rb = (int)(((double)c *  496.0) / 256.0);
                int  g = (int)(((double)c * 1008.0) / 256.0);
//                pclog("rb %i %i\n", rb, c);
                for (y = 0; y < 4; y++)
                {
                        for (x = 0; x < 4; x++)
                        {
                                int val;
                                
                                val = rb + dither_matrix_4x4[x + (y << 2)];
                                dither_rb[c][y][x] = val >> 4;
//                                pclog("RB %i %i, %i  %i %i %i\n", c, x, y, val, val >> 4, dither_rb[c][y][x]);
                                
                                if (dither_rb[c][y][x] > 31)
                                        fatal("RB overflow %i %i %i  %i\n", c, x, y, dither_rb[c][y][x]);

                                val = g + dither_matrix_4x4[x + (y << 2)];
                                dither_g[c][y][x] = val >> 4;                                

                                if (dither_g[c][y][x] > 63)
                                        fatal("G overflow %i %i %i  %i\n", c, x, y, dither_g[c][y][x]);
                        }
                }
        }
        
        memset(seen_rb, 0, sizeof(seen_rb));
        memset(seen_g,  0, sizeof(seen_g));
        
        for (c = 0; c < 256; c++)
        {
                int total_rb = 0;
                int total_g = 0;
                for (y = 0; y < 4; y++)
                {
                        for (x = 0; x < 4; x++)
                        {
                                total_rb += dither_rb[c][y][x];
//                                pclog("total_rb %i %i, %i  %i %i\n", c, x, y, total_rb, dither_rb[c][y][x]);
                                total_g += dither_g[c][y][x];
                        }
                }

                if (seen_rb[total_rb])
                        fatal("Duplicate rb %i %i\n", c, total_rb);
                seen_rb[total_rb] = 1;

                if (seen_g[total_g])
                        fatal("Duplicate g %i %i\n", c, total_g);
                seen_g[total_g] = 1;
        }
}

typedef struct voodoo_state_t
{
        int xstart, xend, xdir;
        uint32_t base_r, base_g, base_b, base_a, base_z;
        struct
        {
                int64_t base_s, base_t, base_w;
        } tmu[1];
        int64_t base_w;
        int lod;
        int dx1, dx2;
        int y, yend, ydir;
        int32_t dxAB, dxAC, dxBC;
        int tex_r, tex_g, tex_b, tex_a;
        int64_t tex_s, tex_t;
        int clamp_s, clamp_t;
        
        uint8_t *tex;
        uint16_t *tex_w;
        int tformat;
        
        rgba_u *palette;


        int *tex_w_mask;
        int *tex_h_mask;
        int *tex_shift;
} voodoo_state_t;

static int voodoo_output = 0;

static inline int fls(uint16_t val)
{
        int num = 0;
        
//pclog("fls(%04x) = ", val);
        if (!(val & 0xff00))
        {
                num += 8;
                val <<= 8;
        }
        if (!(val & 0xf000))
        {
                num += 4;
                val <<= 4;
        }
        if (!(val & 0xc000))
        {
                num += 2;
                val <<= 2;
        }
        if (!(val & 0x8000))
        {
                num += 1;
                val <<= 1;
        }
//pclog("%i %04x\n", num, val);
        return num;
}

typedef struct rgba_t
{
        int r, g, b, a;
} rgba_t;

typedef struct voodoo_texture_state_t
{
        int s, t;
        int w_mask, h_mask;
        int tex_shift;
} voodoo_texture_state_t;

static inline void tex_read(voodoo_state_t *state, voodoo_texture_state_t *texture_state, rgba_u *out)
{
        uint16_t dat;
        
        if (texture_state->s & ~texture_state->w_mask)
        {
                if (state->clamp_s)
                {
                        if (texture_state->s < 0)
                                texture_state->s = 0;
                        if (texture_state->s > texture_state->w_mask)
                                texture_state->s = texture_state->w_mask;
                }
                else
                        texture_state->s &= texture_state->w_mask;
        }
        if (texture_state->t & ~texture_state->h_mask)
        {
                if (state->clamp_t)
                {
                        if (texture_state->t < 0)
                                texture_state->t = 0;
                        if (texture_state->t > texture_state->h_mask)
                                texture_state->t = texture_state->h_mask;
                }
                else
                        texture_state->t &= texture_state->h_mask;
        }

        if (state->tformat & 8)
                dat = state->tex_w[texture_state->s + (texture_state->t << texture_state->tex_shift)];
        else
                dat = state->tex[texture_state->s + (texture_state->t << texture_state->tex_shift)];

        switch (state->tformat)
        {
                case TEX_RGB332:
                out->u = rgb332[dat].u;
                break;
                                                
                case TEX_Y4I2Q2:
                out->u = state->palette[dat].u;
                break;
                                                        
                case TEX_A8:
                out->rgba.r = out->rgba.g = out->rgba.b = out->rgba.a = dat & 0xff;
                break;

                case TEX_I8:
                out->rgba.r = out->rgba.g = out->rgba.b = dat & 0xff;
                out->rgba.a = 0xff;
                break;

                case TEX_AI8:
                out->rgba.r = out->rgba.g = out->rgba.b = (dat & 0x0f) | ((dat << 4) & 0xf0);
                out->rgba.a = (dat & 0xf0) | ((dat >> 4) & 0x0f);
                break;
                                                
                case TEX_PAL8:
                out->u = state->palette[dat].u;
                break;
                                                        
                case TEX_R5G6B5:
                out->u = rgb565[dat].u;
                break;

                case TEX_ARGB1555:
                out->u = argb1555[dat].u;
                break;

                case TEX_ARGB4444:
                out->u = argb4444[dat].u;
                break;
                                                        
                case TEX_A8I8:
                out->rgba.r = out->rgba.g = out->rgba.b = dat & 0xff;
                out->rgba.a = dat >> 8;
                break;

                case TEX_APAL88:
                out->u = state->palette[dat & 0xff].u;
                out->rgba.a = dat >> 8;
                break;
                                                        
                default:
                fatal("Unknown texture format %i\n", state->tformat);
        }
}

static inline void voodoo_get_texture(voodoo_t *voodoo, voodoo_params_t *params, voodoo_state_t *state)
{
        rgba_u tex_samples[4];
        voodoo_texture_state_t texture_state;
        int d[4];
        int _ds, dt;
        int s, t;

        texture_state.w_mask = state->tex_w_mask[state->lod];
        texture_state.h_mask = state->tex_h_mask[state->lod];
        texture_state.tex_shift = state->tex_shift[state->lod];
        
        if (voodoo->bilinear_enabled && params->textureMode & 6)
        {
                
                state->tex_s -= 1 << (17+state->lod);
                state->tex_t -= 1 << (17+state->lod);
        
                s = state->tex_s >> (18+state->lod);
                t = state->tex_t >> (18+state->lod);

                _ds = (state->tex_s >> (14+state->lod)) & 0xf;
                dt = (state->tex_t >> (14+state->lod)) & 0xf;

//if (x == 80)
//if (voodoo_output)
//        pclog("s=%08x t=%08x _ds=%02x _dt=%02x\n", s, t, _ds, dt);

                texture_state.s = s;
                texture_state.t = t;
                tex_read(state, &texture_state, &tex_samples[0]);

                texture_state.s = s+1;
                texture_state.t = t;
                tex_read(state, &texture_state, &tex_samples[1]);

                texture_state.s = s;
                texture_state.t = t+1;
                tex_read(state, &texture_state, &tex_samples[2]);

                texture_state.s = s+1;
                texture_state.t = t+1;
                tex_read(state, &texture_state, &tex_samples[3]);

                d[0] = (16 - _ds) * (16 - dt);
                d[1] =  _ds * (16 - dt);
                d[2] = (16 - _ds) * dt;
                d[3] = _ds * dt;
        
                state->tex_r = (tex_samples[0].rgba.r * d[0] + tex_samples[1].rgba.r * d[1] + tex_samples[2].rgba.r * d[2] + tex_samples[3].rgba.r * d[3]) >> 8;
                state->tex_g = (tex_samples[0].rgba.g * d[0] + tex_samples[1].rgba.g * d[1] + tex_samples[2].rgba.g * d[2] + tex_samples[3].rgba.g * d[3]) >> 8;
                state->tex_b = (tex_samples[0].rgba.b * d[0] + tex_samples[1].rgba.b * d[1] + tex_samples[2].rgba.b * d[2] + tex_samples[3].rgba.b * d[3]) >> 8;
                state->tex_a = (tex_samples[0].rgba.a * d[0] + tex_samples[1].rgba.a * d[1] + tex_samples[2].rgba.a * d[2] + tex_samples[3].rgba.a * d[3]) >> 8;
/*                state->tex_r = tex_samples[0].r;
                state->tex_g = tex_samples[0].g;
                state->tex_b = tex_samples[0].b;
                state->tex_a = tex_samples[0].a;*/
        }
        else
        {
        //        rgba_t tex_samples;
        //        voodoo_texture_state_t texture_state;
//                int s = state->tex_s >> (18+state->lod);
//                int t = state->tex_t >> (18+state->lod);
        //        int s, t;

//                state->tex_s -= 1 << (17+state->lod);
//                state->tex_t -= 1 << (17+state->lod);
        
                s = state->tex_s >> (18+state->lod);
                t = state->tex_t >> (18+state->lod);
//pclog("texture %08x %08x %i %i\n", state->tex_s, state->tex_t, s, t);
                texture_state.s = s;
                texture_state.t = t;
                tex_read(state, &texture_state, &tex_samples[0]);

                state->tex_r = tex_samples[0].rgba.r;
                state->tex_g = tex_samples[0].rgba.g;
                state->tex_b = tex_samples[0].rgba.b;
                state->tex_a = tex_samples[0].rgba.a;
        }
}


static void voodoo_half_triangle(voodoo_t *voodoo, voodoo_params_t *params, voodoo_state_t *state, int ystart, int yend)
{
        int rgb_sel                 = params->fbzColorPath & 3;
        int a_sel                   = (params->fbzColorPath >> 2) & 3;
        int cc_localselect          = params->fbzColorPath & (1 << 4);
        int cca_localselect         = (params->fbzColorPath >> 5) & 3;
        int cc_localselect_override = params->fbzColorPath & (1 << 7);
        int cc_zero_other           = params->fbzColorPath & (1 << 8);
        int cc_sub_clocal           = params->fbzColorPath & (1 << 9);
        int cc_mselect              = (params->fbzColorPath >> 10) & 7;
        int cc_reverse_blend        = params->fbzColorPath & (1 << 13);
        int cc_add                  = (params->fbzColorPath >> 14) & 3;
        int cc_add_alocal           = params->fbzColorPath & (1 << 15);
        int cc_invert_output        = params->fbzColorPath & (1 << 16);
        int cca_zero_other          = params->fbzColorPath & (1 << 17);
        int cca_sub_clocal          = params->fbzColorPath & (1 << 18);
        int cca_mselect             = (params->fbzColorPath >> 19) & 7;
        int cca_reverse_blend       = params->fbzColorPath & (1 << 22);
        int cca_add                 = (params->fbzColorPath >> 23) & 3;
        int cca_invert_output       = params->fbzColorPath & (1 << 25);
        int src_afunc = (params->alphaMode >> 8) & 0xf;
        int dest_afunc = (params->alphaMode >> 12) & 0xf;
        int alpha_func = (params->alphaMode >> 1) & 7;
        int a_ref = params->alphaMode >> 24;
        int depth_op = (params->fbzMode >> 5) & 7;
        int dither = (params->fbzMode & FBZ_DITHER) && rgb_sel == 2;
        
        if (voodoo->trexInit1 & (1 << 18))
                dither = 1;

        state->clamp_s = params->textureMode & TEXTUREMODE_TCLAMPS;
        state->clamp_t = params->textureMode & TEXTUREMODE_TCLAMPT;
//        int last_x;
//        pclog("voodoo_triangle : bottom-half %X %X %X %X %X %i  %i %i %i\n", xstart, xend, dx1, dx2, dx2 * 36, xdir,  y, yend, ydir);

        state->tex_w = &voodoo->tex_mem_w[(params->tex_base[state->lod] >> 1) & 0xfffff];
        state->tex = &voodoo->tex_mem[params->tex_base[state->lod] & 0x1fffff];
        
        state->palette = params->palette;
        state->tformat = params->tformat;

        state->tex_w_mask = params->tex_w_mask;
        state->tex_h_mask = params->tex_h_mask;
        state->tex_shift = params->tex_shift;

        if ((params->fbzMode & 1) && (ystart < params->clipLowY))
        {
                int dy = params->clipLowY - ystart;

                state->base_r += params->dRdY*dy;
                state->base_g += params->dGdY*dy;
                state->base_b += params->dBdY*dy;
                state->base_a += params->dAdY*dy;
                state->base_z += params->dZdY*dy;
                state->tmu[0].base_s += params->tmu[0].dSdY*dy;
                state->tmu[0].base_t += params->tmu[0].dTdY*dy;
                state->tmu[0].base_w += params->tmu[0].dWdY*dy;
                state->base_w += params->dWdY*dy;
                state->xstart += state->dx1*dy;
                state->xend   += state->dx2*dy;

                ystart = params->clipLowY;
        }

        if ((params->fbzMode & 1) && (yend > params->clipHighY))
                yend = params->clipHighY;

        state->y = ystart;
//        yend--;
        if (voodoo_output)
                pclog("dxAB=%08x dxBC=%08x dxAC=%08x\n", state->dxAB, state->dxBC, state->dxAC);
//        pclog("Start %i %i\n", ystart, voodoo->fbzMode & (1 << 17));
        for (; state->y < yend; state->y++)
        {
                int x, x2;// = state->xstart, x2 = state->xend;
                int real_y = (state->y << 4) + 8;
                int start_x, start_x2;
                int dx;
                int32_t ir = state->base_r;
                int32_t ig = state->base_g;
                int32_t ib = state->base_b;
                int32_t ia = state->base_a;
                int32_t z = state->base_z;
                int64_t tmu0_s = state->tmu[0].base_s;
                int64_t tmu0_t = state->tmu[0].base_t;
                int64_t tmu0_w = state->tmu[0].base_w;
                int64_t w = state->base_w;
                uint16_t *fb_mem, *aux_mem;

                x = (params->vertexAx << 12) + ((state->dxAC * (real_y - params->vertexAy)) >> 4);

                if (real_y < params->vertexBy)
                        x2 = (params->vertexAx << 12) + ((state->dxAB * (real_y - params->vertexAy)) >> 4);
                else
                        x2 = (params->vertexBx << 12) + ((state->dxBC * (real_y - params->vertexBy)) >> 4);

                start_x = x;

                if (state->xdir > 0)
                        x2 -= (1 << 16);
                else
                        x -= (1 << 16);
                dx = (x + 0x7f) - (params->vertexAx << 12);
                dx = ((x + 0x7f) >> 16) - (((params->vertexAx << 12) + 0x7f) >> 16);
                start_x2 = x + 0x7f;
                x = (x + 0x7f) >> 16;
                x2 = (x2 + 0x7f) >> 16;
                
                if (voodoo_output)
                        pclog("%03i:%03i : Ax=%08x start_x=%08x  dSdX=%016llx dx=%08x  s=%08x -> ", x, state->y, params->vertexAx << 8, start_x, params->tmu[0].dTdX, dx, tmu0_t);
                        
                ir += (params->dRdX * dx);
                ig += (params->dGdX * dx);
                ib += (params->dBdX * dx);
                ia += (params->dAdX * dx);
                z += (params->dZdX * dx);
                tmu0_s += (params->tmu[0].dSdX * dx);
                tmu0_t += (params->tmu[0].dTdX * dx);
                tmu0_w += (params->tmu[0].dWdX * dx);
                w += (params->dWdX * dx);

                if (voodoo_output)
                        pclog("%08llx %lli %lli\n", tmu0_t, tmu0_t >> (18+state->lod), (tmu0_t + (1 << 17+state->lod)) >> (18+state->lod));

                if (params->fbzMode & (1 << 17))
                        state->y = voodoo->v_disp - state->y;

                if (params->fbzMode & 1)
                {
                        if (state->xdir > 0)
                        {
                                if (x < params->clipLeft)
                                {
                                        int dx = params->clipLeft - x;

                                        ir += params->dRdX*dx;
                                        ig += params->dGdX*dx;
                                        ib += params->dBdX*dx;
                                        ia += params->dAdX*dx;
                                        z += params->dZdX*dx;
                                        tmu0_s += params->tmu[0].dSdX*dx;
                                        tmu0_t += params->tmu[0].dTdX*dx;
                                        tmu0_w += params->tmu[0].dWdX*dx;
                                        w += params->dWdX*dx;
                                        
                                        x = params->clipLeft;
                                }
                                if (x2 > params->clipRight)
                                        x2 = params->clipRight;
                        }
                        else
                        {
                                if (x > params->clipRight)
                                {
                                        int dx = params->clipRight - x;

                                        ir += params->dRdX*dx;
                                        ig += params->dGdX*dx;
                                        ib += params->dBdX*dx;
                                        ia += params->dAdX*dx;
                                        z += params->dZdX*dx;
                                        tmu0_s += params->tmu[0].dSdX*dx;
                                        tmu0_t += params->tmu[0].dTdX*dx;
                                        tmu0_w += params->tmu[0].dWdX*dx;
                                        w += params->dWdX*dx;
                                        
                                        x = params->clipRight;
                                }
                                if (x2 < params->clipLeft)
                                        x2 = params->clipLeft;
                        }
                }
                
                if (x2 < x && state->xdir > 0)
                        goto next_line;
                if (x2 > x && state->xdir < 0)
                        goto next_line;

                fb_mem = &voodoo->fb_mem[params->draw_offset + (state->y * voodoo->row_width)];
                aux_mem = &voodoo->fb_mem[params->aux_offset + (state->y * voodoo->row_width)];
                
                if (voodoo_output)
                        pclog("%03i: x=%08x x2=%08x xstart=%08x xend=%08x dx=%08x start_x2=%08x\n", state->y, x, x2, state->xstart, state->xend, dx, start_x2);

                do
                {
                        start_x = x;
                        voodoo->pixel_count++;
                        if (voodoo_output)
                                pclog("  X=%03i T=%08x\n", x, tmu0_t);
//                        if (voodoo->fbzMode & FBZ_RGB_WMASK)
                        {
                                int update = 1;
                                int cother_r, cother_g, cother_b, aother;
                                int clocal_r, clocal_g, clocal_b, alocal;
                                int src_r, src_g, src_b, src_a;
                                int msel_r, msel_g, msel_b, msel_a;
                                int dest_r, dest_g, dest_b, dest_a;
                                uint16_t dat;
                                uint16_t aux_dat;
                                int sel;
                                int32_t new_depth, w_depth;
                                int exp, mant;

                                if (w & 0xffff00000000)
                                        w_depth = 0xfffff000;
                                else if (!(w & 0xffff0000))
                                        w_depth = 0xf001;
                                else
                                {
                                        exp = fls((uint16_t)(w >> 16));
                                        mant = ((~w >> (19 - exp))) & 0xfff;
                                        w_depth = (exp << 12) + mant + 1;
                                }

                                if (params->fbzMode & FBZ_W_BUFFER)
                                        new_depth = w_depth;
                                else
                                        new_depth = z >> 12;
                                
                                new_depth = CLAMP16(new_depth);
                                w_depth = CLAMP16(w_depth);

                                if (params->fbzMode & FBZ_DEPTH_ENABLE)
                                {
                                        uint16_t old_depth = aux_mem[x];

                                        switch (depth_op)
                                        {
                                                case DEPTHOP_NEVER:
                                                goto skip_pixel;

                                                case DEPTHOP_LESSTHAN:
                                                if (!(new_depth < old_depth))
                                                        goto skip_pixel;
                                                break;
                                                case DEPTHOP_EQUAL:
                                                if (!(new_depth == old_depth))
                                                        goto skip_pixel;
                                                break;
                                                case DEPTHOP_LESSTHANEQUAL:
                                                if (!(new_depth <= old_depth))
                                                        goto skip_pixel;
                                                break;
                                                case DEPTHOP_GREATERTHAN:
                                                if (!(new_depth > old_depth))
                                                        goto skip_pixel;
                                                break;
                                                case DEPTHOP_NOTEQUAL:
                                                if (!(new_depth != old_depth))
                                                        goto skip_pixel;
                                                break;
                                                case DEPTHOP_GREATERTHANEQUAL:
                                                if (!(new_depth >= old_depth))
                                                        goto skip_pixel;
                                                break;
                                                case DEPTHOP_ALWAYS:
                                                break;
                                        }
                                }
                                
                                dat = fb_mem[x];
                                dest_r = (dat >> 8) & 0xf8;
                                dest_g = (dat >> 3) & 0xf8;
                                dest_b = (dat << 3) & 0xf8;
                                dest_r |= (dest_r >> 5);
                                dest_g |= (dest_g >> 5);
                                dest_b |= (dest_b >> 5);
                                dest_a = 0xff;

                                if (params->textureMode & 1)
                                {
                                        int64_t _w = 0;
                                        if (w)
                                                _w = (int64_t)((1ULL << 48) / tmu0_w);

                                        state->tex_s = (int32_t)((tmu0_s * _w) >> 16);
                                        state->tex_t = (int32_t)((tmu0_t * _w) >> 16);
                                }
                                else
                                {
                                        state->tex_s = tmu0_s;
                                        state->tex_t = tmu0_t;
                                }

                                voodoo_get_texture(voodoo, params, state);
                                
                                if ((params->fbzMode & FBZ_CHROMAKEY) &&
                                        state->tex_r == params->chromaKey_r &&
                                        state->tex_g == params->chromaKey_g &&
                                        state->tex_b == params->chromaKey_b)
                                        goto skip_pixel;

                                if (voodoo->trexInit1 & (1 << 18))
                                {
                                        state->tex_r = state->tex_g = 0;
                                        state->tex_b = 1;
                                }

                                if (cc_localselect_override)
                                        sel = (state->tex_a & 0x80) ? 1 : 0;
                                else
                                        sel = cc_localselect;
                                
                                if (sel)
                                {
                                        clocal_r = (params->color0 >> 16) & 0xff;
                                        clocal_g = (params->color0 >> 8)  & 0xff;
                                        clocal_b =  params->color0        & 0xff;
                                }
                                else
                                {
                                        clocal_r = CLAMP(ir >> 12);
                                        clocal_g = CLAMP(ig >> 12);
                                        clocal_b = CLAMP(ib >> 12);
                                }

                                switch (rgb_sel)
                                {
                                        case CC_LOCALSELECT_ITER_RGB: /*Iterated RGB*/
                                        cother_r = CLAMP(ir >> 12);
                                        cother_g = CLAMP(ig >> 12);
                                        cother_b = CLAMP(ib >> 12);
                                        break;                                        

                                        case CC_LOCALSELECT_TEX: /*TREX Color Output*/
                                        cother_r = state->tex_r;
                                        cother_g = state->tex_g;
                                        cother_b = state->tex_b;
                                        break;
                                                
                                        case CC_LOCALSELECT_COLOR1: /*Color1 RGB*/
                                        cother_r = (params->color1 >> 16) & 0xff;
                                        cother_g = (params->color1 >> 8)  & 0xff;
                                        cother_b =  params->color1        & 0xff;
                                        break;
                                        
                                        case CC_LOCALSELECT_LFB: /*Linear Frame Buffer*/
                                        cother_r = src_r;
                                        cother_g = src_g;
                                        cother_b = src_b;
                                        break;
                                }

                                switch (cca_localselect)
                                {
                                        case CCA_LOCALSELECT_ITER_A:
                                        alocal = CLAMP(ia >> 12);
                                        break;

                                        case CCA_LOCALSELECT_COLOR0:
                                        alocal = (params->color0 >> 24) & 0xff;
                                        break;
                                        
                                        case CCA_LOCALSELECT_ITER_Z:
                                        alocal = CLAMP(z >> 20);
                                        break;
                                        
                                        default:
                                        alocal = 0xff;
                                        break;
                                }
                                
                                switch (a_sel)
                                {
                                        case A_SEL_ITER_A:
                                        aother = CLAMP(ia >> 12);
                                        break;
                                        case A_SEL_TEX:
                                        aother = state->tex_a;
                                        break;
                                        case A_SEL_COLOR1:
                                        aother = (params->color1 >> 24) & 0xff;
                                        break;
                                        default:
                                        aother = 0;
                                        break;
                                }
                                
                                if (cc_zero_other)
                                {
                                        src_r = 0;
                                        src_g = 0;                                
                                        src_b = 0;
                                }
                                else
                                {
                                        src_r = cother_r;
                                        src_g = cother_g;
                                        src_b = cother_b;
                                }

                                if (cca_zero_other)
                                        src_a = 0;
                                else
                                        src_a = aother;

                                if (cc_sub_clocal)
                                {
                                        src_r -= clocal_r;
                                        src_g -= clocal_g;
                                        src_b -= clocal_b;
                                }

                                if (cc_sub_clocal)
                                        src_a -= alocal;

                                switch (cc_mselect)
                                {
                                        case CC_MSELECT_ZERO:
                                        msel_r = 0;
                                        msel_g = 0;
                                        msel_b = 0;
                                        break;
                                        case CC_MSELECT_CLOCAL:
                                        msel_r = clocal_r;
                                        msel_g = clocal_g;
                                        msel_b = clocal_b;
                                        break;
                                        case CC_MSELECT_AOTHER:
                                        msel_r = aother;
                                        msel_g = aother;
                                        msel_b = aother;
                                        break;
                                        case CC_MSELECT_ALOCAL:
                                        msel_r = alocal;
                                        msel_g = alocal;
                                        msel_b = alocal;
                                        break;
                                        case CC_MSELECT_TEX:
                                        msel_r = state->tex_a;
                                        msel_g = state->tex_a;
                                        msel_b = state->tex_a;
                                        break;

                                        default:
                                        msel_r = 0;
                                        msel_g = 0;
                                        msel_b = 0;
                                        break;
                                }                                        

                                switch (cca_mselect)
                                {
                                        case CCA_MSELECT_ZERO:
                                        msel_a = 0;
                                        break;
                                        case CCA_MSELECT_ALOCAL:
                                        msel_a = alocal;
                                        break;
                                        case CCA_MSELECT_AOTHER:
                                        msel_a = aother;
                                        break;
                                        case CCA_MSELECT_ALOCAL2:
                                        msel_a = alocal;
                                        break;
                                        case CCA_MSELECT_TEX:
                                        msel_a = state->tex_a;
                                        break;

                                        default:
                                        msel_a = 0;
                                        break;
                                }                                        

                                if (!cc_reverse_blend)
                                {
                                        msel_r ^= 0xff;
                                        msel_g ^= 0xff;
                                        msel_b ^= 0xff;
                                }
                                msel_r++;
                                msel_g++;
                                msel_b++;

                                if (!cca_reverse_blend)
                                        msel_a ^= 0xff;
                                msel_a++;
                                
                                src_r = (src_r * msel_r) >> 8;
                                src_g = (src_g * msel_g) >> 8;
                                src_b = (src_b * msel_b) >> 8;
                                src_a = (src_a * msel_a) >> 8;

                                switch (cc_add)
                                {
                                        case CC_ADD_CLOCAL:
                                        src_r += clocal_r;
                                        src_g += clocal_g;
                                        src_b += clocal_b;
                                        break;
                                        case CC_ADD_ALOCAL:
                                        src_r += alocal;
                                        src_g += alocal;
                                        src_b += alocal;
                                        break;
                                }

                                if (cca_add)
                                        src_a += alocal;
                                
                                src_r = CLAMP(src_r);
                                src_g = CLAMP(src_g);
                                src_b = CLAMP(src_b);
                                src_a = CLAMP(src_a);

                                if (cc_invert_output)
                                {
                                        src_r ^= 0xff;
                                        src_g ^= 0xff;
                                        src_b ^= 0xff;
                                }
                                if (cca_invert_output)
                                        src_a ^= 0xff;

                                if (params->fogMode & FOG_ENABLE)
                                {
                                        if (params->fogMode & FOG_CONSTANT)
                                        {
                                                src_r += params->fogColor.r;
                                                src_g += params->fogColor.g;
                                                src_b += params->fogColor.b;
                                        }
                                        else
                                        {
                                                int fog_r, fog_g, fog_b, fog_a;
                                                
                                                if (!(params->fogMode & FOG_ADD))
                                                {
                                                        fog_r = params->fogColor.r;
                                                        fog_g = params->fogColor.g;
                                                        fog_b = params->fogColor.b;
                                                }
                                                else
                                                        fog_r = fog_g = fog_b = 0;

                                                if (!(params->fogMode & FOG_MULT))
                                                {
                                                        fog_r -= src_r;
                                                        fog_g -= src_g;
                                                        fog_b -= src_b;
                                                }

                                                if (params->fogMode & FOG_Z)
                                                        fog_a = (z >> 20) & 0xff;
                                                else if (params->fogMode & FOG_ALPHA)
                                                        fog_a = CLAMP(ia >> 12);
                                                else
                                                {
                                                        int fog_idx = (w_depth >> 10) & 0x3f;
                                                        
                                                        fog_a = params->fogTable[fog_idx].fog;
                                                        fog_a += (params->fogTable[fog_idx].dfog * ((w_depth >> 2) & 0xff)) >> 10;
                                                        if ((x == 32 && state->y == 461) || x == 320) pclog("x=%03i y=%03i: w=%04x idx=%02x a=%02x w2=%02x a2=%02x  %02x %02x\n", x, state->y, w_depth, fog_idx, params->fogTable[fog_idx].fog, (w_depth >> 2) & 0xff, fog_a, params->fogTable[0x20].fog, params->fogTable[0x21].fog);
                                                }
                                                fog_a++;
//pclog("src %02x %02x %02x fog_a %02x\n", src_r, src_g, src_b, fog_a);  
                                                if ((x == 32 && state->y == 461) || x == 320) pclog("  fog_r=%02x fog_g=%02x fog_b=%02x\n", fog_r, fog_g, fog_b);
                                                fog_r = (fog_r * fog_a) >> 8;
                                                fog_g = (fog_g * fog_a) >> 8;
                                                fog_b = (fog_b * fog_a) >> 8;
                                                if ((x == 32 && state->y == 461) || x == 320) pclog("  fog_r=%02x fog_g=%02x fog_b=%02x\n", fog_r, fog_g, fog_b);
                                                if ((x == 32 && state->y == 461) || x == 320) pclog("  src_r=%02x src_g=%02x src_b=%02x\n", src_r, src_g, src_b);
                                                if (params->fogMode & FOG_MULT)
                                                {
                                                        src_r = fog_r;
                                                        src_g = fog_g;
                                                        src_b = fog_b;
                                                }
                                                else
                                                {
                                                        src_r += fog_r;
                                                        src_g += fog_g;
                                                        src_b += fog_b;
                                                }
                                                if ((x == 32 && state->y == 461) || x == 320) pclog("  src_r=%02x src_g=%02x src_b=%02x %016llx\n", src_r, src_g, src_b, w);
                                        }

                                        src_r = CLAMP(src_r);
                                        src_g = CLAMP(src_g);
                                        src_b = CLAMP(src_b);
                                }
                                
                                if (params->alphaMode & 1)
                                {
                                        switch (alpha_func)
                                        {
                                                case AFUNC_NEVER:
                                                goto skip_pixel;
                                                break;
                                                case AFUNC_LESSTHAN:
                                                if (!(src_a < a_ref))
                                                        goto skip_pixel;
                                                break;
                                                case AFUNC_EQUAL:
                                                if (!(src_a == a_ref))
                                                        goto skip_pixel;
                                                break;
                                                case AFUNC_LESSTHANEQUAL:
                                                if (!(src_a <= a_ref))
                                                        goto skip_pixel;
                                                break;
                                                case AFUNC_GREATERTHAN:
                                                if (!(src_a > a_ref))
                                                        goto skip_pixel;
                                                break;
                                                case AFUNC_NOTEQUAL:
                                                if (!(src_a != a_ref))
                                                        goto skip_pixel;
                                                break;
                                                case AFUNC_GREATERTHANEQUAL:
                                                if (!(src_a >= a_ref))
                                                        goto skip_pixel;
                                                break;
                                                case AFUNC_ALWAYS:
                                                break;
                                        }
                                }

                                if (params->alphaMode & (1 << 4))
                                {
                                        int _a;
                                        int newdest_r, newdest_g, newdest_b;
                                        
                                        switch (dest_afunc)
                                        {
                                                case AFUNC_AZERO:
                                                newdest_r = newdest_g = newdest_b = 0;
                                                break;
                                                case AFUNC_ASRC_ALPHA:
                                                newdest_r = (dest_r * src_a) / 255;
                                                newdest_g = (dest_g * src_a) / 255;
                                                newdest_b = (dest_b * src_a) / 255;
                                                break;
                                                case AFUNC_A_COLOR:
                                                newdest_r = (dest_r * src_r) / 255;
                                                newdest_g = (dest_g * src_g) / 255;
                                                newdest_b = (dest_b * src_b) / 255;
                                                break;
                                                case AFUNC_ADST_ALPHA:
                                                newdest_r = (dest_r * dest_a) / 255;
                                                newdest_g = (dest_g * dest_a) / 255;
                                                newdest_b = (dest_b * dest_a) / 255;
                                                break;
                                                case AFUNC_AONE:
                                                newdest_r = dest_r;
                                                newdest_g = dest_g;
                                                newdest_b = dest_b;
                                                break;
                                                case AFUNC_AOMSRC_ALPHA:
                                                newdest_r = (dest_r * (255-src_a)) / 255;
                                                newdest_g = (dest_g * (255-src_a)) / 255;
                                                newdest_b = (dest_b * (255-src_a)) / 255;
                                                break;
                                                case AFUNC_AOM_COLOR:
                                                newdest_r = (dest_r * (255-src_r)) / 255;
                                                newdest_g = (dest_g * (255-src_g)) / 255;
                                                newdest_b = (dest_b * (255-src_b)) / 255;
                                                break;
                                                case AFUNC_AOMDST_ALPHA:
                                                newdest_r = (dest_r * (255-dest_a)) / 255;
                                                newdest_g = (dest_g * (255-dest_a)) / 255;
                                                newdest_b = (dest_b * (255-dest_a)) / 255;
                                                break;
                                                case AFUNC_ASATURATE:
                                                _a = MIN(src_a, 1-dest_a);
                                                newdest_r = (dest_r * _a) / 255;
                                                newdest_g = (dest_g * _a) / 255;
                                                newdest_b = (dest_b * _a) / 255;
                                                break;
                                        }

                                        switch (src_afunc)
                                        {
                                                case AFUNC_AZERO:
                                                src_r = src_g = src_b = 0;
                                                break;
                                                case AFUNC_ASRC_ALPHA:
                                                src_r = (src_r * src_a) / 255;
                                                src_g = (src_g * src_a) / 255;
                                                src_b = (src_b * src_a) / 255;
                                                break;
                                                case AFUNC_A_COLOR:
                                                src_r = (src_r * dest_r) / 255;
                                                src_g = (src_g * dest_g) / 255;
                                                src_b = (src_b * dest_b) / 255;
                                                break;
                                                case AFUNC_ADST_ALPHA:
                                                src_r = (src_r * dest_a) / 255;
                                                src_g = (src_g * dest_a) / 255;
                                                src_b = (src_b * dest_a) / 255;
                                                break;
                                                case AFUNC_AONE:
                                                break;
                                                case AFUNC_AOMSRC_ALPHA:
                                                src_r = (src_r * (255-src_a)) / 255;
                                                src_g = (src_g * (255-src_a)) / 255;
                                                src_b = (src_b * (255-src_a)) / 255;
                                                break;
                                                case AFUNC_AOM_COLOR:
                                                src_r = (src_r * (255-dest_r)) / 255;
                                                src_g = (src_g * (255-dest_g)) / 255;
                                                src_b = (src_b * (255-dest_b)) / 255;
                                                break;
                                                case AFUNC_AOMDST_ALPHA:
                                                src_r = (src_r * (255-dest_a)) / 255;
                                                src_g = (src_g * (255-dest_a)) / 255;
                                                src_b = (src_b * (255-dest_a)) / 255;
                                                break;
                                                case AFUNC_ACOLORBEFOREFOG:
                                                break;
                                        }

                                        src_r += newdest_r;
                                        src_g += newdest_g;
                                        src_b += newdest_b;

                                        src_r = CLAMP(src_r);
                                        src_g = CLAMP(src_g);
                                        src_b = CLAMP(src_b);
                                }

                                if (update)
                                {
                                        if (dither)
                                        {
                                                src_r = dither_rb[src_r][state->y & 3][x & 3];
                                                src_g =  dither_g[src_g][state->y & 3][x & 3];
                                                src_b = dither_rb[src_b][state->y & 3][x & 3];
                                        }
                                        else
                                        {
                                                src_r >>= 3;
                                                src_g >>= 2;
                                                src_b >>= 3;
                                        }

                                        if (params->fbzMode & FBZ_RGB_WMASK)
                                                fb_mem[x] = src_b | (src_g << 5) | (src_r << 11);
                                        if (params->fbzMode & FBZ_DEPTH_WMASK)
                                                aux_mem[x] = new_depth;
                                }
                        }
                        voodoo_output &= ~2;
skip_pixel:
                        if (state->xdir > 0)
                        {                                
                                ir += params->dRdX;
                                ig += params->dGdX;
                                ib += params->dBdX;
                                ia += params->dAdX;
                                z += params->dZdX;
                                tmu0_s += params->tmu[0].dSdX;
                                tmu0_t += params->tmu[0].dTdX;
                                tmu0_w += params->tmu[0].dWdX;
                                w += params->dWdX;
                        }
                        else
                        {                                
                                ir -= params->dRdX;
                                ig -= params->dGdX;
                                ib -= params->dBdX;
                                ia -= params->dAdX;
                                z -= params->dZdX;
                                tmu0_s -= params->tmu[0].dSdX;
                                tmu0_t -= params->tmu[0].dTdX;
                                tmu0_w -= params->tmu[0].dWdX;
                                w -= params->dWdX;
                        }

                        x += state->xdir;
                } while (start_x != x2);

next_line:
                if (params->fbzMode & (1 << 17))
                        state->y = voodoo->v_disp - state->y;

                state->base_r += params->dRdY;
                state->base_g += params->dGdY;
                state->base_b += params->dBdY;
                state->base_a += params->dAdY;
                state->base_z += params->dZdY;
                state->tmu[0].base_s += params->tmu[0].dSdY;
                state->tmu[0].base_t += params->tmu[0].dTdY;
                state->tmu[0].base_w += params->tmu[0].dWdY;
                state->base_w += params->dWdY;
                state->xstart += state->dx1;
                state->xend += state->dx2;
        }
}
        
static void voodoo_triangle(voodoo_t *voodoo, voodoo_params_t *params)
{
        voodoo_state_t state;
        int vertexAy_adjusted;
        int vertexBy_adjusted;
        int vertexCy_adjusted;
        int dx, dy;
        
        voodoo->tri_count++;
        
        dx = 8 - ((params->vertexAx + 7) & 0xf);
        dy = 8 - ((params->vertexAy + 7) & 0xf);
        
/*        pclog("voodoo_triangle %i : vA %f, %f  vB %f, %f  vC %f, %f f %i\n", tris, (float)params->vertexAx / 16.0, (float)params->vertexAy / 16.0, 
                                                                     (float)params->vertexBx / 16.0, (float)params->vertexBy / 16.0, 
                                                                     (float)params->vertexCx / 16.0, (float)params->vertexCy / 16.0, params->tformat);*/

        params->startR += (dx*params->dRdX + dy*params->dRdY) >> 4;
        params->startG += (dx*params->dGdX + dy*params->dGdY) >> 4;
        params->startB += (dx*params->dBdX + dy*params->dBdY) >> 4;
        params->startA += (dx*params->dAdX + dy*params->dAdY) >> 4;
        params->startZ += (dx*params->dZdX + dy*params->dZdY) >> 4;
        params->tmu[0].startS += (dx*params->tmu[0].dSdX + dy*params->tmu[0].dSdY) >> 4;
        params->tmu[0].startT += (dx*params->tmu[0].dTdX + dy*params->tmu[0].dTdY) >> 4;
        params->tmu[0].startW += (dx*params->tmu[0].dWdX + dy*params->tmu[0].dWdY) >> 4;
        params->startW += (dx*params->dWdX + dy*params->dWdY) >> 4;

        tris++;

        if (params->vertexAy & 0x8000)
                params->vertexAy |= 0xffff0000;
        else
                params->vertexAy &= ~0xffff0000;
        if (params->vertexBy & 0x8000)
                params->vertexBy |= 0xffff0000;
        else
                params->vertexBy &= ~0xffff0000;
        if (params->vertexCy & 0x8000)
                params->vertexCy |= 0xffff0000;
        else
                params->vertexCy &= ~0xffff0000;

        params->vertexAx = params->vertexAx & ~0xffff0000;
        if (params->vertexAx & 0x8000)
                params->vertexAx |= 0xffff0000;
        params->vertexBx = params->vertexBx & ~0xffff0000;
        if (params->vertexBx & 0x8000)
                params->vertexBx |= 0xffff0000;
        params->vertexCx = params->vertexCx & ~0xffff0000;
        if (params->vertexCx & 0x8000)
                params->vertexCx |= 0xffff0000;

        vertexAy_adjusted = (params->vertexAy+7) >> 4;
        vertexBy_adjusted = (params->vertexBy+7) >> 4;
        vertexCy_adjusted = (params->vertexCy+7) >> 4;

        if (params->vertexBy - params->vertexAy)
                state.dxAB = (int)((((int64_t)params->vertexBx << 12) - ((int64_t)params->vertexAx << 12)) << 4) / (int)(params->vertexBy - params->vertexAy);
        else
                state.dxAB = 0;
        if (params->vertexCy - params->vertexAy)
                state.dxAC = (int)((((int64_t)params->vertexCx << 12) - ((int64_t)params->vertexAx << 12)) << 4) / (int)(params->vertexCy - params->vertexAy);
        else
                state.dxAC = 0;
        if (params->vertexCy - params->vertexBy)
                state.dxBC = (int)((((int64_t)params->vertexCx << 12) - ((int64_t)params->vertexBx << 12)) << 4) / (int)(params->vertexCy - params->vertexBy);
        else
                state.dxBC = 0;

        state.lod = (params->tLOD >> 2) & 15;
        state.xstart = state.xend = params->vertexAx << 8;
        state.xdir = params->sign ? -1 : 1;

        state.y = (params->vertexAy + 8) >> 4;
        state.ydir = 1;

        state.base_r = params->startR;
        state.base_g = params->startG;
        state.base_b = params->startB;
        state.base_a = params->startA;
        state.base_z = params->startZ;
        state.tmu[0].base_s = params->tmu[0].startS;
        state.tmu[0].base_t = params->tmu[0].startT;
        state.tmu[0].base_w = params->tmu[0].startW;
        state.base_w = params->startW;

        voodoo_half_triangle(voodoo, params, &state, vertexAy_adjusted, vertexCy_adjusted);
}

static void voodoo_fastfill(voodoo_t *voodoo, voodoo_params_t *params)
{
        int y;

        if (params->fbzMode & FBZ_RGB_WMASK)
        {
                int r, g, b;
                uint16_t col;

                r = ((params->color1 >> 16) >> 3) & 0x1f;
                g = ((params->color1 >> 8) >> 2) & 0x3f;
                b = (params->color1 >> 3) & 0x1f;
                col = b | (g << 5) | (r << 11);

                for (y = params->clipLowY; y < params->clipHighY; y++)
                {
                        uint16_t *cbuf = (uint16_t *)&voodoo->fb_mem[(params->draw_offset + y*voodoo->row_width) & 0x1fffff];
                        int x;
                
                        for (x = params->clipLeft; x < params->clipRight; x++)
                                cbuf[x] = col;
                }
        }
        if (params->fbzMode & FBZ_DEPTH_WMASK)
        {        
                for (y = params->clipLowY; y < params->clipHighY; y++)
                {
                        uint16_t *abuf = (uint16_t *)&voodoo->fb_mem[(params->aux_offset + y*voodoo->row_width) & 0x1fffff];
                        int x;
                
                        for (x = params->clipLeft; x < params->clipRight; x++)
                                abuf[x] = params->zaColor & 0xffff;
                }
        }
}

static void voodoo_exec_command(voodoo_t *voodoo, voodoo_params_t *params)
{
//        pclog("voodoo_exec_command %08x\n", params->draw_offset);
        switch (params->command)
        {
                case CMD_DRAWTRIANGLE:
                voodoo_triangle(voodoo, params);
                break;
                case CMD_FASTFILL:
                voodoo_fastfill(voodoo, params);
                break;
                case CMD_SWAPBUF:
//                pclog("Swap buffer %08x\n", params->swapbufferCMD);
//                voodoo->front_offset = params->front_offset;
                if (!(params->swapbufferCMD & 1))
                        voodoo->front_offset = params->front_offset;
                else
                {
                        voodoo->swap_interval = (params->swapbufferCMD >> 1) & 0xff;
                        voodoo->swap_offset = params->front_offset;
                        voodoo->swap_pending = 1;

                        while (voodoo->swap_pending)
                        {
                                thread_wait_event(voodoo->wake_render_thread, -1);
                                thread_reset_event(voodoo->wake_render_thread);
                        }
                }
                break;
        }
}

static void render_thread(void *param)
{
        voodoo_t *voodoo = (voodoo_t *)param;
        
        while (1)
        {
                thread_wait_event(voodoo->wake_render_thread, -1);
                thread_reset_event(voodoo->wake_render_thread);

                voodoo->voodoo_busy = 1;
                while (!RB_EMPTY)
                {
                        uint64_t start_time = timer_read();
                        uint64_t end_time;

                        voodoo_exec_command(voodoo, &voodoo->params_buffer[voodoo->params_read_idx & RB_MASK]);
                        voodoo->params_buffer[voodoo->params_read_idx & RB_MASK].command = CMD_INVALID;
                        end_time = timer_read();
        
                        voodoo_time += end_time - start_time;

                        voodoo->params_read_idx++;
                        thread_set_event(voodoo->not_full_event);
                }
                voodoo->voodoo_busy = 0;
        }
}

static void queue_command(voodoo_t *voodoo, voodoo_params_t *params, int command)
{
        voodoo_params_t *params_new = &voodoo->params_buffer[voodoo->params_write_idx & RB_MASK];
        int c;

        if (RB_FULL)
        {
                thread_reset_event(voodoo->not_full_event);
                if (RB_FULL)
                {
                        thread_wait_event(voodoo->not_full_event, -1); /*Wait for room in ringbuffer*/
                }
        }

        memcpy(params_new, params, sizeof(voodoo_params_t) - sizeof(voodoo->palette));

        if (command == CMD_DRAWTRIANGLE)
        {
                /*Copy palette data if required*/
                switch (params->tformat)
                {
                        case TEX_PAL8: case TEX_APAL88:
                        memcpy(params_new->palette, voodoo->palette, sizeof(voodoo->palette));
                        break;
                        case TEX_Y4I2Q2:
                        memcpy(params_new->palette, voodoo->ncc_lookup[(params->textureMode & TEXTUREMODE_NCC_SEL) ? 1 : 0], sizeof(voodoo->palette));
                        break;
                }
        }

        /*Set command last, so that the render thread doesn't start executing
          before all parameters are copied.*/
        params_new->command = command;

        voodoo->params_write_idx++;
        thread_set_event(voodoo->wake_render_thread); /*Wake up render thread if moving from idle*/
}

static uint32_t voodoo_readl(uint32_t addr, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        uint32_t temp;
        
        switch (addr & 0x3fc)
        {
                case SST_status:
                if (RB_FULL)
                        temp = 0; /*FIFOs full*/
                else if (RB_EMPTY)
                        temp = 0x0ffff03f; /*FIFOs empty*/
                else
                        temp = 0x000ff000; /*FIFOs middle*/
                temp |= (voodoo->swap_count << 28);
                if (voodoo->swap_count)
                        temp &= ~0x3f;
                if (!RB_EMPTY)
                        temp |= 0x380; /*Busy*/
                break;
                
                case SST_lfbMode:
                temp = voodoo->lfbMode;
                break;
                
                case SST_fbiInit4:
                temp = voodoo->fbiInit4;
                break;
                case SST_fbiInit0:
                temp = voodoo->fbiInit0;
                break;
                case SST_fbiInit1:
                temp = voodoo->fbiInit1 & ~5; /*Pass-thru board with one SST-1*/
                break;              
                case SST_fbiInit2:
                if (voodoo->initEnable & 0x04)
                        temp = voodoo->dac_readdata;
                else
                        temp = voodoo->fbiInit2;
                break;
                case SST_fbiInit3:
                temp = voodoo->fbiInit3;
                break;
                
                default:
                fatal("voodoo_readl  : bad addr %08X\n", addr);
                temp = 0xffffffff;
        }

        return temp;
}

enum
{
        CHIP_FBI = 0x1,
        CHIP_TREX0 = 0x2,
        CHIP_TREX1 = 0x2,
        CHIP_TREX2 = 0x2
};

static void voodoo_writel(uint32_t addr, uint32_t val, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        union
        {
                uint32_t i;
                float f;
        } tempif;
        int ad21 = addr & (1 << 21);
        int chip = (addr >> 10) & 0xf;
        if (!chip)
                chip = 0xf;
        
        tempif.i = val;

        addr &= 0x3fc;

        if ((voodoo->fbiInit3 & FBIINIT3_REMAP) && addr < 0x100 && ad21)
                addr |= 0x400;
        switch (addr)
        {
                case SST_swapbufferCMD:
//                pclog("  start swap buffer command\n");

                voodoo->disp_buffer = !voodoo->disp_buffer;
                voodoo_recalc(voodoo);

                voodoo->swap_count++;
                voodoo->params.swapbufferCMD = val;
                queue_command(voodoo, &voodoo->params, CMD_SWAPBUF);
                break;
                        
                case SST_vertexAx: case SST_remap_vertexAx:
                voodoo->params.vertexAx = val & 0xffff;
                break;
                case SST_vertexAy: case SST_remap_vertexAy:
                voodoo->params.vertexAy = val & 0xffff;
                break;
                case SST_vertexBx: case SST_remap_vertexBx:
                voodoo->params.vertexBx = val & 0xffff;
                break;
                case SST_vertexBy: case SST_remap_vertexBy:
                voodoo->params.vertexBy = val & 0xffff;
                break;
                case SST_vertexCx: case SST_remap_vertexCx:
                voodoo->params.vertexCx = val & 0xffff;
                break;
                case SST_vertexCy: case SST_remap_vertexCy:
                voodoo->params.vertexCy = val & 0xffff;
                break;
                
                case SST_startR: case SST_remap_startR:
                voodoo->params.startR = val & 0xffffff;
                break;
                case SST_startG: case SST_remap_startG:
                voodoo->params.startG = val & 0xffffff;
                break;
                case SST_startB: case SST_remap_startB:
                voodoo->params.startB = val & 0xffffff;
                break;
                case SST_startZ: case SST_remap_startZ:
                voodoo->params.startZ = val;
                break;
                case SST_startA: case SST_remap_startA:
                voodoo->params.startA = val & 0xffffff;
                break;
                case SST_startS: case SST_remap_startS:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].startS = (int64_t)(int32_t)val;
                break;
                case SST_startT: case SST_remap_startT:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].startT = (int64_t)(int32_t)val;
                break;
                case SST_startW: case SST_remap_startW:
                if (chip & CHIP_FBI)
                        voodoo->params.startW = (int64_t)(int32_t)val << 2;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].startW = (int64_t)(int32_t)val << 2;
                break;

                case SST_dRdX: case SST_remap_dRdX:
                voodoo->params.dRdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dGdX: case SST_remap_dGdX:
                voodoo->params.dGdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dBdX: case SST_remap_dBdX:
                voodoo->params.dBdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dZdX: case SST_remap_dZdX:
                voodoo->params.dZdX = val;
                break;
                case SST_dAdX: case SST_remap_dAdX:
                voodoo->params.dAdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dSdX: case SST_remap_dSdX:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dSdX = (int64_t)(int32_t)val;
                break;
                case SST_dTdX: case SST_remap_dTdX:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dTdX = (int64_t)(int32_t)val;
                break;
                case SST_dWdX: case SST_remap_dWdX:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dWdX = (int64_t)(int32_t)val << 2;                
                if (chip & CHIP_FBI)
                        voodoo->params.dWdX = (int64_t)(int32_t)val << 2;
                break;

                case SST_dRdY: case SST_remap_dRdY:
                voodoo->params.dRdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dGdY: case SST_remap_dGdY:
                voodoo->params.dGdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dBdY: case SST_remap_dBdY:
                voodoo->params.dBdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dZdY: case SST_remap_dZdY:
                voodoo->params.dZdY = val;
                break;
                case SST_dAdY: case SST_remap_dAdY:
                voodoo->params.dAdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dSdY: case SST_remap_dSdY:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dSdY = (int64_t)(int32_t)val;
                break;
                case SST_dTdY: case SST_remap_dTdY:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dTdY = (int64_t)(int32_t)val;
                break;
                case SST_dWdY: case SST_remap_dWdY:
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dWdY = (int64_t)(int32_t)val << 2;
                if (chip & CHIP_FBI)
                        voodoo->params.dWdY = (int64_t)(int32_t)val << 2;
                break;

                case SST_triangleCMD: case SST_remap_triangleCMD:
                voodoo->params.sign = val & (1 << 31);

                queue_command(voodoo, &voodoo->params, CMD_DRAWTRIANGLE);
                break;

                case SST_fvertexAx: case SST_remap_fvertexAx:
                voodoo->fvertexAx.i = val;
                break;
                case SST_fvertexAy: case SST_remap_fvertexAy:
                voodoo->fvertexAy.i = val;
                break;
                case SST_fvertexBx: case SST_remap_fvertexBx:
                voodoo->fvertexBx.i = val;
                break;
                case SST_fvertexBy: case SST_remap_fvertexBy:
                voodoo->fvertexBy.i = val;
                break;
                case SST_fvertexCx: case SST_remap_fvertexCx:
                voodoo->fvertexCx.i = val;
                break;
                case SST_fvertexCy: case SST_remap_fvertexCy:
                voodoo->fvertexCy.i = val;
                break;

                case SST_fstartR: case SST_remap_fstartR:
                voodoo->fstartR.i = val;
                break;
                case SST_fstartG: case SST_remap_fstartG:
                voodoo->fstartG.i = val;
                break;
                case SST_fstartB: case SST_remap_fstartB:
                voodoo->fstartB.i = val;
                break;
                case SST_fstartZ: case SST_remap_fstartZ:
                voodoo->fstartZ.i = val;
                break;
                case SST_fstartA: case SST_remap_fstartA:
                voodoo->fstartA.i = val;
                break;
                case SST_fstartS: case SST_remap_fstartS:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].startS = (int64_t)(tempif.f * 262144.0f);
                break;
                case SST_fstartT: case SST_remap_fstartT:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].startT = (int64_t)(tempif.f * 262144.0f);
                break;
                case SST_fstartW: case SST_remap_fstartW:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].startW = (int64_t)(tempif.f * 4294967296.0f);
                if (chip & CHIP_FBI)
                        voodoo->params.startW = (int64_t)(tempif.f * 4294967296.0f);
                break;

                case SST_fdRdX: case SST_remap_fdRdX:
                voodoo->fdRdX.i = val;
                break;
                case SST_fdGdX: case SST_remap_fdGdX:
                voodoo->fdGdX.i = val;
                break;
                case SST_fdBdX: case SST_remap_fdBdX:
                voodoo->fdBdX.i = val;
                break;
                case SST_fdZdX: case SST_remap_fdZdX:
                voodoo->fdZdX.i = val;
                break;
                case SST_fdAdX: case SST_remap_fdAdX:
                voodoo->fdAdX.i = val;
                break;
                case SST_fdSdX: case SST_remap_fdSdX:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dSdX = (int64_t)(tempif.f * 262144.0f);
                break;
                case SST_fdTdX: case SST_remap_fdTdX:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dTdX = (int64_t)(tempif.f * 262144.0f);
                break;
                case SST_fdWdX: case SST_remap_fdWdX:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dWdX = (int64_t)(tempif.f * 4294967296.0f);
                if (chip & CHIP_FBI)
                        voodoo->params.dWdX = (int64_t)(tempif.f * 4294967296.0f);
                break;

                case SST_fdRdY: case SST_remap_fdRdY:
                voodoo->fdRdY.i = val;
                break;
                case SST_fdGdY: case SST_remap_fdGdY:
                voodoo->fdGdY.i = val;
                break;
                case SST_fdBdY: case SST_remap_fdBdY:
                voodoo->fdBdY.i = val;
                break;
                case SST_fdZdY: case SST_remap_fdZdY:
                voodoo->fdZdY.i = val;
                break;
                case SST_fdAdY: case SST_remap_fdAdY:
                voodoo->fdAdY.i = val;
                break;
                case SST_fdSdY: case SST_remap_fdSdY:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dSdY = (int64_t)(tempif.f * 262144.0f);
                break;
                case SST_fdTdY: case SST_remap_fdTdY:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dTdY = (int64_t)(tempif.f * 262144.0f);
                break;
                case SST_fdWdY: case SST_remap_fdWdY:
                tempif.i = val;
                if (chip & CHIP_TREX0)
                        voodoo->params.tmu[0].dWdY = (int64_t)(tempif.f * 4294967296.0f);
                if (chip & CHIP_FBI)
                        voodoo->params.dWdY = (int64_t)(tempif.f * 4294967296.0f);
                break;

                case SST_ftriangleCMD:
                voodoo->params.vertexAx = (int32_t)(int16_t)(int32_t)(voodoo->fvertexAx.f * 16.0f) & 0xffff;
                voodoo->params.vertexAy = (int32_t)(int16_t)(int32_t)(voodoo->fvertexAy.f * 16.0f) & 0xffff;
                voodoo->params.vertexBx = (int32_t)(int16_t)(int32_t)(voodoo->fvertexBx.f * 16.0f) & 0xffff;
                voodoo->params.vertexBy = (int32_t)(int16_t)(int32_t)(voodoo->fvertexBy.f * 16.0f) & 0xffff;
                voodoo->params.vertexCx = (int32_t)(int16_t)(int32_t)(voodoo->fvertexCx.f * 16.0f) & 0xffff;
                voodoo->params.vertexCy = (int32_t)(int16_t)(int32_t)(voodoo->fvertexCy.f * 16.0f) & 0xffff;
/*                pclog("ftriangle %f(%08x),%f(%08x) %f,%f %f,%f  %08x,%08x %08x,%08x %08x,%08x\n",
                        voodoo->fvertexAx.f, voodoo->fvertexAx.i, voodoo->fvertexAy.f, voodoo->fvertexAy.i,
                        voodoo->fvertexBx.f, voodoo->fvertexBy.f,
                        voodoo->fvertexCx.f, voodoo->fvertexCy.f,
                        voodoo->vertexAx, voodoo->vertexAy,
                        voodoo->vertexBx, voodoo->vertexBy,
                        voodoo->vertexCx, voodoo->vertexCy);*/
                voodoo->params.startR = (int32_t)(voodoo->fstartR.f * 4096.0f);
                voodoo->params.startG = (int32_t)(voodoo->fstartG.f * 4096.0f);
                voodoo->params.startB = (int32_t)(voodoo->fstartB.f * 4096.0f);
                voodoo->params.startZ = (int32_t)(voodoo->fstartZ.f * 4096.0f);
                voodoo->params.startA = (int32_t)(voodoo->fstartA.f * 4096.0f);

                voodoo->params.dRdX = (int32_t)(voodoo->fdRdX.f * 4096.0f);
                voodoo->params.dGdX = (int32_t)(voodoo->fdGdX.f * 4096.0f);
                voodoo->params.dBdX = (int32_t)(voodoo->fdBdX.f * 4096.0f);
                voodoo->params.dZdX = (int32_t)(voodoo->fdZdX.f * 4096.0f);
                voodoo->params.dAdX = (int32_t)(voodoo->fdAdX.f * 4096.0f);

                voodoo->params.dRdY = (int32_t)(voodoo->fdRdY.f * 4096.0f);
                voodoo->params.dGdY = (int32_t)(voodoo->fdGdY.f * 4096.0f);
                voodoo->params.dBdY = (int32_t)(voodoo->fdBdY.f * 4096.0f);
                voodoo->params.dZdY = (int32_t)(voodoo->fdZdY.f * 4096.0f);
                voodoo->params.dAdY = (int32_t)(voodoo->fdAdY.f * 4096.0f);

                voodoo->params.sign = val & (1 << 31);

                queue_command(voodoo, &voodoo->params, CMD_DRAWTRIANGLE);
                break;

                
                case SST_fbzColorPath:
                voodoo->params.fbzColorPath = val;
                voodoo->rgb_sel = val & 3;
                break;

                case SST_fogMode:
                voodoo->params.fogMode = val;
                break;
                case SST_alphaMode:
                voodoo->params.alphaMode = val;
                break;
                case SST_fbzMode:
                voodoo->params.fbzMode = val;
                voodoo_recalc(voodoo);
                break;
                case SST_lfbMode:
                voodoo->lfbMode = val;
                voodoo_recalc(voodoo);
                break;

                case SST_clipLeftRight:
                voodoo->params.clipRight = val & 0x3ff;
                voodoo->params.clipLeft = (val >> 16) & 0x3ff;
                break;
                case SST_clipLowYHighY:
                voodoo->params.clipHighY = val & 0x3ff;
                voodoo->params.clipLowY = (val >> 16) & 0x3ff;
                break;

                case SST_fastfillCMD:
                queue_command(voodoo, &voodoo->params, CMD_FASTFILL);
                break;

                case SST_fogColor:
                voodoo->params.fogColor.r = (val >> 16) & 0xff;
                voodoo->params.fogColor.g = (val >> 8) & 0xff;
                voodoo->params.fogColor.b = val & 0xff;
                break;
                
                case SST_zaColor:
                voodoo->params.zaColor = val;
                break;
                case SST_chromaKey:
                voodoo->params.chromaKey_r = (val >> 16) & 0xff;
                voodoo->params.chromaKey_g = (val >> 8) & 0xff;
                voodoo->params.chromaKey_b = val & 0xff;
                break;
                
                case SST_color0:
                voodoo->params.color0 = val;
                break;
                case SST_color1:
                voodoo->params.color1 = val;
                break;

                case SST_fogTable00: case SST_fogTable01: case SST_fogTable02: case SST_fogTable03:
                case SST_fogTable04: case SST_fogTable05: case SST_fogTable06: case SST_fogTable07:
                case SST_fogTable08: case SST_fogTable09: case SST_fogTable0a: case SST_fogTable0b:
                case SST_fogTable0c: case SST_fogTable0d: case SST_fogTable0e: case SST_fogTable0f:
                case SST_fogTable10: case SST_fogTable11: case SST_fogTable12: case SST_fogTable13:
                case SST_fogTable14: case SST_fogTable15: case SST_fogTable16: case SST_fogTable17:
                case SST_fogTable18: case SST_fogTable19: case SST_fogTable1a: case SST_fogTable1b:
                case SST_fogTable1c: case SST_fogTable1d: case SST_fogTable1e: case SST_fogTable1f:
                addr = (addr - SST_fogTable00) >> 1;
                voodoo->params.fogTable[addr].dfog   = val & 0xff;
                voodoo->params.fogTable[addr].fog    = (val >> 8) & 0xff;
                voodoo->params.fogTable[addr+1].dfog = (val >> 16) & 0xff;
                voodoo->params.fogTable[addr+1].fog  = (val >> 24) & 0xff;
                break;

                case SST_fbiInit4:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit4 = val;
                break;
                case SST_backPorch:
                voodoo->backPorch = val;
                break;
                case SST_videoDimensions:
                voodoo->videoDimensions = val;
                voodoo->h_disp = (val & 0xfff) + 1;
                voodoo->v_disp = (val >> 16) & 0xfff;
                break;
                case SST_fbiInit0:
                if (voodoo->initEnable & 0x01)
                {
                        voodoo->fbiInit0 = val;
                        svga_set_override(voodoo->svga, val & 1);
                }
                break;
                case SST_fbiInit1:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit1 = val;
                break;
                case SST_fbiInit2:
                if (voodoo->initEnable & 0x01)
                {
                        voodoo->fbiInit2 = val;
                        voodoo_recalc(voodoo);
                }
                break;
                case SST_fbiInit3:
                if (voodoo->initEnable & 0x01)
                        voodoo->fbiInit3 = val;
                break;

                case SST_hSync:
                voodoo->hSync = val;
                break;
                case SST_vSync:
                voodoo->vSync = val;
                voodoo->v_total = (val & 0xffff) + (val >> 16);
                break;
                
                case SST_dacData:
                voodoo->dac_reg = (val >> 8) & 7;
                voodoo->dac_readdata = 0xff;
                if (val & 0x800)
                {
//                        pclog("  dacData read %i %02X\n", voodoo->dac_reg, voodoo->dac_data[7]);
                        if (voodoo->dac_reg == 5)
                        {
                                switch (voodoo->dac_data[7])
                                {
        				case 0x01: voodoo->dac_readdata = 0x55; break;
        				case 0x07: voodoo->dac_readdata = 0x71; break;
        				case 0x0b: voodoo->dac_readdata = 0x79; break;
                                }
                        }
                        else
                                voodoo->dac_readdata = voodoo->dac_data[voodoo->dac_readdata];
                }
                else
                        voodoo->dac_data[voodoo->dac_reg] = val & 0xff;
                break;

                case SST_textureMode:
                voodoo->params.textureMode = val;
                voodoo->params.tformat = (val >> 8) & 0xf;
                break;
                case SST_tLOD:
                voodoo->params.tLOD = val;
                voodoo_recalc_tex(voodoo);
                break;

                case SST_texBaseAddr:
//                pclog("Write texBaseAddr %08x\n", val);
                voodoo->params.texBaseAddr = (val & 0x7ffff) << 3;
                voodoo_recalc_tex(voodoo);
                break;
                case SST_texBaseAddr1:
                voodoo->params.texBaseAddr1 = (val & 0x7ffff) << 3;
                voodoo_recalc_tex(voodoo);
                break;
                case SST_texBaseAddr2:
                voodoo->params.texBaseAddr2 = (val & 0x7ffff) << 3;
                voodoo_recalc_tex(voodoo);
                break;
                case SST_texBaseAddr38:
                voodoo->params.texBaseAddr38 = (val & 0x7ffff) << 3;
                voodoo_recalc_tex(voodoo);
                break;
                
                case SST_trexInit1:
                voodoo->trexInit1 = val;
                break;

                case SST_nccTable0_Y0:
                voodoo->nccTable[0].y[0] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable0_Y1:
                voodoo->nccTable[0].y[1] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable0_Y2:
                voodoo->nccTable[0].y[2] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable0_Y3:
                voodoo->nccTable[0].y[3] = val;
                voodoo_update_ncc(voodoo);
                break;
                
                case SST_nccTable0_I0:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].i[0] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                case SST_nccTable0_I2:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].i[2] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                case SST_nccTable0_Q0:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].q[0] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                case SST_nccTable0_Q2:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].q[2] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                if (val & (1 << 31))
                {
                        int p = (val >> 23) & 0xfe;
                        voodoo->palette[p].u = val | 0xff000000;
                }
                break;
                        
                case SST_nccTable0_I1:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].i[1] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                case SST_nccTable0_I3:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].i[3] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                case SST_nccTable0_Q1:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].q[1] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                case SST_nccTable0_Q3:
                if (!(val & (1 << 31)))
                {
                        voodoo->nccTable[0].q[3] = val;
                        voodoo_update_ncc(voodoo);
                        break;
                }
                if (val & (1 << 31))
                {
                        int p = ((val >> 23) & 0xfe) | 0x01;
                        voodoo->palette[p].u = val | 0xff000000;
                }
                break;

                case SST_nccTable1_Y0:
                voodoo->nccTable[1].y[0] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Y1:
                voodoo->nccTable[1].y[1] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Y2:
                voodoo->nccTable[1].y[2] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Y3:
                voodoo->nccTable[1].y[3] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_I0:
                voodoo->nccTable[1].i[0] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_I1:
                voodoo->nccTable[1].i[1] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_I2:
                voodoo->nccTable[1].i[2] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_I3:
                voodoo->nccTable[1].i[3] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Q0:
                voodoo->nccTable[1].q[0] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Q1:
                voodoo->nccTable[1].q[1] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Q2:
                voodoo->nccTable[1].q[2] = val;
                voodoo_update_ncc(voodoo);
                break;
                case SST_nccTable1_Q3:
                voodoo->nccTable[1].q[3] = val;
                voodoo_update_ncc(voodoo);
                break;
        }
}

extern int timetolive;
static int fb_reads = 0;
static uint16_t voodoo_fb_readw(uint32_t addr, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        int x, y;
        uint32_t read_addr;
        uint16_t temp;
        
        x = (addr >> 1) & 0x3ff;
        y = (addr >> 11) & 0x3ff;
        read_addr = voodoo->fb_read_offset + (x << 1) + (y * voodoo->row_width);

        if (read_addr >= 0x200000)
                return 0xffff;

        temp = *(uint16_t *)(&voodoo->fb_mem[read_addr & 0x1fffff]);

//        pclog("voodoo_fb_readw : %08X %08X  %i %i  %08X %08X  %08x:%08x %i\n", addr, temp, x, y, read_addr, *(uint32_t *)(&voodoo->fb_mem[4]), cs, pc, fb_reads++);
        return temp;
}
static uint32_t voodoo_fb_readl(uint32_t addr, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        int x, y;
        uint32_t read_addr;
        uint32_t temp;
        
        x = addr & 0x7fe;
        y = (addr >> 11) & 0x3ff;
        read_addr = voodoo->fb_read_offset + x + (y * voodoo->row_width);

        if (read_addr >= 0x200000)
                return 0xffffffff;

        temp = *(uint32_t *)(&voodoo->fb_mem[read_addr & 0x1fffff]);

//        pclog("voodoo_fb_readl : %08X %08x %08X  x=%i y=%i  %08X %08X  %08x:%08x %i ro=%08x rw=%i\n", addr, read_addr, temp, x, y, read_addr, *(uint32_t *)(&voodoo->fb_mem[4]), cs, pc, fb_reads++, voodoo->fb_read_offset, voodoo->row_width);
        return temp;
}
static void voodoo_fb_writew(uint32_t addr, uint16_t val, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        int x, y;
        uint32_t write_addr, write_addr_aux;
        uint32_t colour_data, depth_data;
        int write_mask, count = 1;
        
//        pclog("voodoo_fb_writew : %08X %04X\n", addr, val);
        
        if (voodoo->lfbMode & 0x100)
                fatal("voodoo_fb_writew : using pixel processing\n");
                
        switch (voodoo->lfbMode & LFB_FORMAT_MASK)
        {
                case LFB_FORMAT_RGB565:
                colour_data = val;
                write_mask = LFB_WRITE_COLOUR;
                break;
                
                default:
                fatal("voodoo_fb_writew : bad LFB format %08X\n", voodoo->lfbMode);
        }

        x = addr & 0x7fe;
        y = (addr >> 11) & 0x3ff;

        write_addr = voodoo->fb_write_offset + x + (y * voodoo->row_width);      
        write_addr_aux = voodoo->params.aux_offset + x + (y * voodoo->row_width);
        
//        pclog("fb_writew %08x %i %i %i %08x\n", addr, x, y, voodoo->row_width, write_addr);
        
        while (count--)
        {               
                if (write_mask & LFB_WRITE_COLOUR)
                        *(uint16_t *)(&voodoo->fb_mem[write_addr & 0x1ffffe]) = colour_data;
                if (write_mask & LFB_WRITE_DEPTH)
                        *(uint16_t *)(&voodoo->fb_mem[write_addr_aux & 0x1ffffe]) = depth_data;
                        
                colour_data >>= 16;
                depth_data >>= 16;
                
                write_addr += 2;
                write_addr_aux += 2;
        }
}
static void voodoo_fb_writel(uint32_t addr, uint32_t val, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        int x, y;
        uint32_t write_addr, write_addr_aux;
        uint32_t colour_data, depth_data;
        int write_mask, count = 1;
        
//        pclog("voodoo_fb_writel : %08X %08X\n", addr, val);
        
        if (voodoo->lfbMode & 0x100)
                fatal("voodoo_fb_writel : using pixel processing\n");
                
        switch (voodoo->lfbMode & LFB_FORMAT_MASK)
        {
                case LFB_FORMAT_RGB565:
                colour_data = val;
                write_mask = LFB_WRITE_COLOUR;
                count = 2;
                break;
                
                case LFB_FORMAT_ARGB8888:
                colour_data = ((val >> 3) & 0x1f) | ((val >> 5) & 0x7e0) | ((val >> 8) & 0xf800);
                write_mask = LFB_WRITE_COLOUR;
                addr >>= 1;
                break;
                
                case LFB_FORMAT_DEPTH:
                depth_data = val;
                write_mask = LFB_WRITE_DEPTH;
                count = 2;
                break;
                
                default:
                fatal("voodoo_fb_writel : bad LFB format %08X\n", voodoo->lfbMode);
        }

        x = addr & 0x7fe;
        y = (addr >> 11) & 0x3ff;

        write_addr = voodoo->fb_write_offset + x + (y * voodoo->row_width);
        write_addr_aux = voodoo->params.aux_offset + x + (y * voodoo->row_width);
        
//        pclog("fb_writel %08x x=%i y=%i rw=%i %08x wo=%08x\n", addr, x, y, voodoo->row_width, write_addr, voodoo->fb_write_offset);
        
        while (count--)
        {               
                if (write_mask & LFB_WRITE_COLOUR)
                        *(uint16_t *)(&voodoo->fb_mem[write_addr & 0x1ffffe]) = colour_data;
                if (write_mask & LFB_WRITE_DEPTH)
                        *(uint16_t *)(&voodoo->fb_mem[write_addr_aux & 0x1ffffe]) = depth_data;
                        
                colour_data >>= 16;
                depth_data >>= 16;
                
                write_addr += 2;
                write_addr_aux += 2;
        }
}

static void voodoo_tex_writel(uint32_t addr, uint32_t val, void *p)
{
        int lod, s, t;
        voodoo_t *voodoo = (voodoo_t *)p;

        if (addr & 0x600000)
                return; /*TREX != 0*/

//        pclog("voodoo_tex_writel : %08X %08X %i\n", addr, val, voodoo->params.tformat);
        
        lod = (addr >> 17) & 0xf;
        t = (addr >> 9) & 0xff;
        if (voodoo->params.tformat & 8)
                s = (addr >> 1) & 0xfe;
        else
        {
                if (voodoo->params.textureMode & (1 << 31))
                        s = addr & 0xfc;
                else
                        s = (addr >> 1) & 0xfc;
        }

        if (lod > LOD_MAX)
                return;
        
//        if (addr >= 0x200000)
//                return;
        
        if (voodoo->params.tformat & 8)
                addr = voodoo->params.tex_base[lod] + s*2 + (t << voodoo->params.tex_shift[lod])*2;
        else
                addr = voodoo->params.tex_base[lod] + s + (t << voodoo->params.tex_shift[lod]);
//        pclog("  write to taddr=%08x lod=%i s=%02x t=%02x\n", addr, lod, s, t);
        *(uint32_t *)(&voodoo->tex_mem[addr & 0x1fffff]) = val;
}

static void voodoo_recalcmapping(voodoo_t *voodoo)
{
        if (voodoo->pci_enable)
        {
                pclog("voodoo_recalcmapping : memBaseAddr %08X\n", voodoo->memBaseAddr);
                mem_mapping_set_addr(&voodoo->mmio_mapping, voodoo->memBaseAddr,              0x00400000);
                mem_mapping_set_addr(&voodoo->fb_mapping,   voodoo->memBaseAddr + 0x00400000, 0x00400000);
                mem_mapping_set_addr(&voodoo->tex_mapping,  voodoo->memBaseAddr + 0x00800000, 0x00800000);
        }
        else
        {
                pclog("voodoo_recalcmapping : disabled\n");
                mem_mapping_disable(&voodoo->mmio_mapping);
                mem_mapping_disable(&voodoo->fb_mapping);
                mem_mapping_disable(&voodoo->tex_mapping);
        }
}

uint8_t voodoo_pci_read(int func, int addr, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;

        pclog("Voodoo PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x1a; /*3dfx*/
                case 0x01: return 0x12;
                
                case 0x02: return 0x01; /*SST-1 (Voodoo Graphics)*/
                case 0x03: return 0x00;
                
                case 0x04: return voodoo->pci_enable ? 0x02 : 0x00; /*Respond to memory accesses*/

                case 0x08: return 2; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x10: return 0x00; /*memBaseAddr*/
                case 0x11: return 0x00;
                case 0x12: return 0x00;
                case 0x13: return voodoo->memBaseAddr >> 24;

                case 0x40: return voodoo->initEnable;
        }
        return 0;
}

void voodoo_pci_write(int func, int addr, uint8_t val, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        
        pclog("Voodoo PCI write %04X %02X\n", addr, val);
        switch (addr)
        {
                case 0x04:
                voodoo->pci_enable = val & 2;
                voodoo_recalcmapping(voodoo);
                break;
                
                case 0x13:
                voodoo->memBaseAddr = val << 24;
                voodoo_recalcmapping(voodoo);
                break;
                
                case 0x40:
                voodoo->initEnable = val; 
                break;
        }
}

void voodoo_callback(void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;

        if (voodoo->fbiInit0 & FBIINIT0_VGA_PASS)
        {
                if (voodoo->line < voodoo->v_disp)
                {
                        uint32_t *p = &((uint32_t *)buffer32->line[voodoo->line])[32];
                        uint16_t *src = (uint16_t *)&voodoo->fb_mem[voodoo->front_offset + voodoo->line*voodoo->row_width];
                        int x;

                        for (x = 0; x < voodoo->h_disp; x++)
                        {
                                p[x] = video_16to32[src[x]];
                        }
                }
                if (voodoo->line == voodoo->v_disp)
                {
//                        pclog("retrace %i %i %08x %i\n", voodoo->retrace_count, voodoo->swap_interval, voodoo->swap_offset, voodoo->swap_pending);
                        voodoo->retrace_count++;
                        if (voodoo->swap_pending && (voodoo->retrace_count > voodoo->swap_interval))
                        {
                                voodoo->retrace_count = 0;
                                voodoo->front_offset = voodoo->swap_offset;
                                voodoo->swap_count--;
                                voodoo->swap_pending = 0;
                                thread_set_event(voodoo->wake_render_thread);
                        }
                }
        }
        voodoo->line++;
        
        if (voodoo->fbiInit0 & FBIINIT0_VGA_PASS)
        {
                if (voodoo->line == voodoo->v_disp)
                {
                        svga_doblit(0, voodoo->v_disp, voodoo->h_disp, voodoo->v_disp, voodoo->svga);
                }
        }
        
        if (voodoo->line >= voodoo->v_total)
                voodoo->line = 0;
                
        voodoo->timer_count += TIMER_USEC * 32;
}

static void voodoo_add_status_info(char *s, int max_len, void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;
        char temps[256];
        uint64_t new_time = timer_read();
        uint64_t status_diff = new_time - status_time;
        status_time = new_time;

        if (!status_diff)
                status_diff = 1;

        svga_add_status_info(s, max_len, &voodoo->svga);
        sprintf(temps, "%f Mpixels/sec\n%f ktris/sec\n%f%% CPU\n%f%% CPU (real)\n\n", (double)voodoo->pixel_count/1000000.0, (double)voodoo->tri_count/1000.0, ((double)voodoo_time * 100.0) / timer_freq, ((double)voodoo_time * 100.0) / status_diff);
        strncat(s, temps, max_len);

        voodoo->pixel_count = voodoo->tri_count = 0;
        voodoo_time = 0;
}

void *voodoo_init()
{
        int c;
        voodoo_t *voodoo = malloc(sizeof(voodoo_t));
        memset(voodoo, 0, sizeof(voodoo_t));

        voodoo->bilinear_enabled = device_get_config_int("bilinear");
                
        voodoo_make_dither();
        
        pci_add(voodoo_pci_read, voodoo_pci_write, voodoo);

        mem_mapping_add(&voodoo->mmio_mapping, 0, 0, NULL, NULL,            voodoo_readl,    NULL,       NULL,              voodoo_writel,     NULL, 0, voodoo);
        mem_mapping_add(&voodoo->fb_mapping,   0, 0, NULL, voodoo_fb_readw, voodoo_fb_readl, NULL,       voodoo_fb_writew,  voodoo_fb_writel,  NULL, 0, voodoo);
        mem_mapping_add(&voodoo->tex_mapping,  0, 0, NULL, NULL,            NULL,            NULL,       NULL,              voodoo_tex_writel, NULL, 0, voodoo);

        voodoo->fb_mem = malloc(2 * 1024 * 1024);
        voodoo->tex_mem = malloc(2 * 1024 * 1024);
        voodoo->tex_mem_w = (uint16_t *)voodoo->tex_mem;

        timer_add(voodoo_callback, &voodoo->timer_count, TIMER_ALWAYS_ENABLED, voodoo);
        
        voodoo->svga = svga_get_pri();
        voodoo->fbiInit0 = 0;

        voodoo->wake_render_thread = thread_create_event();
        voodoo->wake_main_thread = thread_create_event();
        voodoo->not_full_event = thread_create_event();
        voodoo->render_thread = thread_create(render_thread, voodoo);

        for (c = 0; c < 0x100; c++)
        {
                rgb332[c].rgba.r = c & 0xe0;
                rgb332[c].rgba.g = (c << 3) & 0xe0;
                rgb332[c].rgba.b = (c << 6) & 0xc0;
                rgb332[c].rgba.r = rgb332[c].rgba.r | (rgb332[c].rgba.r >> 3) | (rgb332[c].rgba.r >> 6);
                rgb332[c].rgba.g = rgb332[c].rgba.g | (rgb332[c].rgba.g >> 3) | (rgb332[c].rgba.g >> 6);
                rgb332[c].rgba.b = rgb332[c].rgba.b | (rgb332[c].rgba.b >> 2);
                rgb332[c].rgba.b = rgb332[c].rgba.b | (rgb332[c].rgba.b >> 4);
                rgb332[c].rgba.a = 0xff;
        }
                
        for (c = 0; c < 0x10000; c++)
        {
                rgb565[c].rgba.r = (c >> 8) & 0xf8;
                rgb565[c].rgba.g = (c >> 3) & 0xfc;
                rgb565[c].rgba.b = (c << 3) & 0xf8;
                rgb565[c].rgba.r |= (rgb565[c].rgba.r >> 5);
                rgb565[c].rgba.g |= (rgb565[c].rgba.g >> 6);
                rgb565[c].rgba.b |= (rgb565[c].rgba.b >> 5);
                rgb565[c].rgba.a = 0xff;

                argb1555[c].rgba.r = (c >> 7) & 0xf8;
                argb1555[c].rgba.g = (c >> 2) & 0xf8;
                argb1555[c].rgba.b = (c << 3) & 0xf8;
                argb1555[c].rgba.r |= (argb1555[c].rgba.r >> 5);
                argb1555[c].rgba.g |= (argb1555[c].rgba.g >> 5);
                argb1555[c].rgba.b |= (argb1555[c].rgba.b >> 5);
                argb1555[c].rgba.a = (c & 0x8000) ? 0xff : 0;

                argb4444[c].rgba.a = (c >> 8) & 0xf0;
                argb4444[c].rgba.r = (c >> 4) & 0xf0;
                argb4444[c].rgba.g = c & 0xf0;
                argb4444[c].rgba.b = (c << 4) & 0xf0;
                argb4444[c].rgba.a |= (argb4444[c].rgba.a >> 4);
                argb4444[c].rgba.r |= (argb4444[c].rgba.r >> 4);
                argb4444[c].rgba.g |= (argb4444[c].rgba.g >> 4);
                argb4444[c].rgba.b |= (argb4444[c].rgba.b >> 4);
        }

        return voodoo;
}

void voodoo_close(void *p)
{
        FILE *f;
        voodoo_t *voodoo = (voodoo_t *)p;
        
        f = romfopen("texram.dmp", "wb");
        fwrite(voodoo->tex_mem, 2048*1024, 1, f);
        fclose(f);
        
        thread_kill(voodoo->render_thread);
        thread_destroy_event(voodoo->not_full_event);
        thread_destroy_event(voodoo->wake_main_thread);
        thread_destroy_event(voodoo->wake_render_thread);

        free(voodoo->fb_mem);
        free(voodoo->tex_mem);
        free(voodoo);
}

static device_config_t voodoo_config[] =
{
        {
                .name = "bilinear",
                .description = "Bilinear filtering",
                .type = CONFIG_BINARY,
                .default_int = 1
        },
        {
                .type = -1
        }
};

device_t voodoo_device =
{
        "3DFX Voodoo Graphics",
        0,
        voodoo_init,
        voodoo_close,
        NULL,
        NULL,
        NULL,
        voodoo_add_status_info,
        voodoo_config
};
