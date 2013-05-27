/*ATI Mach64 emulation*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_ati68860_ramdac.h"
#include "vid_ati_eeprom.h"
#include "vid_ics2595.h"
#include "vid_svga_render.h"

//#define MACH64_DEBUG
static int mach64_bank_r[2] = {0, 0};
static int mach64_bank_w[2] = {0, 0};
void mach64_write(uint32_t addr, uint8_t val, void *priv);
uint8_t mach64_read(uint32_t addr, void *priv);

static uint8_t ati_regs[256];
static int ati_index;

struct
{
        uint32_t config_cntl;

        uint32_t context_load_cntl;
        uint32_t context_mask;
                
        uint32_t crtc_gen_cntl;
        uint8_t  crtc_int_cntl;
        uint32_t crtc_h_total_disp;
        uint32_t crtc_v_sync_strt_wid;
        uint32_t crtc_v_total_disp;
        uint32_t crtc_off_pitch;

        uint32_t clock_cntl;

        uint32_t cur_horz_vert_off;        
        uint32_t cur_horz_vert_posn;
        uint32_t cur_offset;
        
        uint32_t dac_cntl;
        
        uint32_t dp_bkgd_clr;
        uint32_t dp_frgd_clr;
        uint32_t dp_mix;
        uint32_t dp_pix_width;
        uint32_t dp_src;
        
        uint32_t dst_bres_lnth;
        uint32_t dst_bres_dec;
        uint32_t dst_bres_err;
        uint32_t dst_bres_inc;
        
        uint32_t dst_cntl;
        uint32_t dst_height_width;
        uint32_t dst_off_pitch;
        uint32_t dst_y_x;

        uint32_t gen_test_cntl;
        
        uint32_t gui_traj_cntl;
                
        uint32_t mem_cntl;

        uint32_t pat_cntl;        
        uint32_t pat_reg0, pat_reg1;
        
        uint32_t sc_left_right, sc_top_bottom;
        
        uint32_t scratch_reg0, scratch_reg1;

        uint32_t src_cntl;        
        uint32_t src_off_pitch;
        uint32_t src_y_x;

        struct
        {
                int op;
                
                int dst_x, dst_y;
                int dst_x_start;
                int src_x, src_y;
                int src_x_start;
                int xinc, yinc;
                int x_count, y_count;
                uint32_t src_offset, src_pitch;
                uint32_t dst_offset, dst_pitch;                
                int mix_bg, mix_fg;
                int source_bg, source_fg, source_mix;
                int source_host;                
                int dst_width, dst_height;
                int busy;
                int pattern[8][8];
                int sc_left, sc_right, sc_top, sc_bottom;
                int dst_pix_width, src_pix_width, host_pix_width;
                int dst_size, src_size;

                uint32_t dp_bkgd_clr;
                uint32_t dp_frgd_clr;
                
                int err;
        } accel;
} mach64;

enum
{
        SRC_BG      = 0,
        SRC_FG      = 1,
        SRC_HOST    = 2,
        SRC_BLITSRC = 3,
        SRC_PAT     = 4
};

enum
{
        MONO_SRC_1       = 0,
        MONO_SRC_PAT     = 1,
        MONO_SRC_HOST    = 2,
        MONO_SRC_BLITSRC = 3
};

enum
{
        BPP_1  = 0,
        BPP_4  = 1,
        BPP_8  = 2,
        BPP_15 = 3,
        BPP_16 = 4,
        BPP_32 = 5
};

enum
{
        OP_RECT,
        OP_LINE
};

static int mach64_width[6] = {0, 0, 0, 1, 1, 2};

enum
{
        DST_24_ROT_EN = 0x80
};

uint8_t  mach64_ext_readb(uint32_t addr, void *priv);
uint16_t mach64_ext_readw(uint32_t addr, void *priv);
uint32_t mach64_ext_readl(uint32_t addr, void *priv);
void     mach64_ext_writeb(uint32_t addr, uint8_t val, void *priv);
void     mach64_ext_writew(uint32_t addr, uint16_t val, void *priv);
void     mach64_ext_writel(uint32_t addr, uint32_t val, void *priv);

void mach64_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

//        pclog("mach64 out %04X %02X\n", addr, val);
                
        switch (addr)
        {
                case 0x1ce:
                ati_index = val;
                break;
                case 0x1cf:
                ati_regs[ati_index] = val;
                break;
                
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                ati68860_ramdac_out((addr & 3) | ((mach64.dac_cntl & 3) << 2), val, NULL);
                return;
                
                case 0x3cf:
                if (gdcaddr == 6)
                {
                        if ((gdcreg[6] & 0xc) != (val & 0xc))
                        {
                                mem_removehandler(0xa0000, 0x20000, mach64_read, NULL, NULL, mach64_write, NULL, NULL,  NULL);
                                mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,  NULL);
                                mem_removehandler(0xbc000, 0x04000, mach64_ext_readb, mach64_ext_readw, mach64_ext_readl, mach64_ext_writeb, mach64_ext_writew, mach64_ext_writel,  NULL);
                                pclog("Write mapping %02X\n", val);
                                switch (val&0xC)
                                {
                                        case 0x0: /*128k at A0000*/
                                        mem_sethandler(0xa0000, 0x10000, mach64_read, NULL, NULL, mach64_write, NULL, NULL,  NULL);
                                        mem_sethandler(0xbc000, 0x04000, mach64_ext_readb, mach64_ext_readw, mach64_ext_readl, mach64_ext_writeb, mach64_ext_writew, mach64_ext_writel,  NULL);
                                        break;
                                        case 0x4: /*64k at A0000*/
                                        mem_sethandler(0xa0000, 0x10000, mach64_read, NULL, NULL, mach64_write, NULL, NULL,  NULL);
                                        break;
                                        case 0x8: /*32k at B0000*/
                                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,  NULL);
                                        break;
                                        case 0xC: /*32k at B8000*/
                                        mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,  NULL);
                                        break;
                                }
                        }
                        gdcreg[6] = val;
                        return;
                }
                break;
                
                case 0x3D4:
                crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;

                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                svga_recalctimings();
                        }
                }
                break;
        }
        svga_out(addr, val, NULL);
}

uint8_t mach64_in(uint16_t addr, void *priv)
{
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
//        pclog("IN mach64 %04X\n", addr);
        
        switch (addr)
        {
                case 0x1ce:
                return ati_index;
                case 0x1cf:
                return ati_regs[ati_index];

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                return ati68860_ramdac_in((addr & 3) | ((mach64.dac_cntl & 3) << 2), NULL);
                
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
        }
        return svga_in(addr, NULL);
}

void mach64_recalctimings()
{
        if (((mach64.crtc_gen_cntl >> 24) & 3) == 3)
        {
                svga_vtotal = (mach64.crtc_v_total_disp & 2047) + 1;
                svga_dispend = ((mach64.crtc_v_total_disp >> 16) & 2047) + 1;
                svga_htotal = (mach64.crtc_h_total_disp & 255) + 1;
                svga_hdisp_time = svga_hdisp = ((mach64.crtc_h_total_disp >> 16) & 255) + 1;
                svga_vsyncstart = (mach64.crtc_v_sync_strt_wid & 2047) + 1;
                svga_rowoffset = (mach64.crtc_off_pitch >> 22);
                svga_clock = cpuclock / ics2595_output_clock;
                svga_ma = (mach64.crtc_off_pitch & 0x1fffff) * 2;
                svga_linedbl = svga_rowcount = 0;
//                svga_htotal <<= 1;
//                svga_hdisp <<= 1;
                svga_rowoffset <<= 1;
                switch ((mach64.crtc_gen_cntl >> 8) & 7)
                {
                        case 1: 
                        svga_render = svga_render_4bpp_highres; 
                        svga_hdisp *= 8;
                        break;
                        case 2: 
                        svga_render = svga_render_8bpp_highres; 
                        svga_hdisp *= 8;
                        svga_rowoffset /= 2;
                        break;
                        case 3: 
                        svga_render = svga_render_15bpp_highres; 
                        svga_hdisp *= 8;
                        //svga_rowoffset *= 2;
                        break;
                        case 4: 
                        svga_render = svga_render_16bpp_highres; 
                        svga_hdisp *= 8;
                        //svga_rowoffset *= 2;
                        break;
                        case 5: 
                        svga_render = svga_render_24bpp_highres; 
                        svga_hdisp *= 8;
                        svga_rowoffset = (svga_rowoffset * 3) / 2;
                        break;
                        case 6: 
                        svga_render = svga_render_32bpp_highres; 
                        svga_hdisp *= 8;
                        break;
                }
                
                pclog("mach64_recalctimings : frame %i,%i disp %i,%i vsync at %i rowoffset %i pixel clock %f MA %08X\n", svga_htotal, svga_vtotal, svga_hdisp, svga_dispend, svga_vsyncstart, svga_rowoffset, svga_clock, svga_ma);
        }
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

void mach64_cursor_dump()
{
        pclog("Mach64 cursor :\n");
        pclog("Ena = %i X = %i Y = %i Addr = %05X Xoff = %i Yoff = %i\n", svga_hwcursor.ena, svga_hwcursor.x, svga_hwcursor.y, svga_hwcursor.addr, svga_hwcursor.xoff, svga_hwcursor.yoff);
}

void mach64_start_fill()
{
        int x, y;
        
        mach64.accel.dst_x = (mach64.dst_y_x >> 16) & 0xfff;
        mach64.accel.dst_y =  mach64.dst_y_x        & 0xfff;

        mach64.accel.dst_width  = (mach64.dst_height_width >> 16) & 0x1fff;
        mach64.accel.dst_height =  mach64.dst_height_width        & 0x1fff;        
        mach64.accel.x_count = mach64.accel.dst_width;
        
        mach64.accel.src_x = (mach64.src_y_x >> 16) & 0xfff;
        mach64.accel.src_y =  mach64.src_y_x        & 0xfff;

        mach64.accel.src_pitch  = (mach64.src_off_pitch >> 22) * 8;
        mach64.accel.src_offset = (mach64.src_off_pitch & 0xfffff) * 8;

        mach64.accel.dst_pitch  = (mach64.dst_off_pitch >> 22) * 8;
        mach64.accel.dst_offset = (mach64.dst_off_pitch & 0xfffff) * 8;
        
        mach64.accel.mix_fg = (mach64.dp_mix >> 16) & 0x1f;
        mach64.accel.mix_bg = mach64.dp_mix & 0x1f;
        
        mach64.accel.source_bg  =  mach64.dp_src & 7;
        mach64.accel.source_fg  = (mach64.dp_src >> 8) & 7;
        mach64.accel.source_mix = (mach64.dp_src >> 16) & 7;
        
        mach64.accel.dst_pix_width  =  mach64.dp_pix_width        & 7;
        mach64.accel.src_pix_width  = (mach64.dp_pix_width >>  8) & 7;
        mach64.accel.host_pix_width = (mach64.dp_pix_width >> 16) & 7;
        
        mach64.accel.dst_size = mach64_width[mach64.accel.dst_pix_width];
        mach64.accel.src_size = mach64_width[mach64.accel.src_pix_width];

/*        mach64.accel.src_x     *= mach64_inc[mach64.accel.src_pix_width];
        mach64.accel.src_pitch *= mach64_inc[mach64.accel.src_pix_width];
        mach64.accel.dst_x     *= mach64_inc[mach64.accel.dst_pix_width];
        mach64.accel.dst_pitch *= mach64_inc[mach64.accel.dst_pix_width];*/
        
        mach64.accel.src_offset >>= mach64.accel.src_size;
        mach64.accel.dst_offset >>= mach64.accel.dst_size;

        mach64.accel.src_x_start = mach64.accel.src_x;
        mach64.accel.dst_x_start = mach64.accel.dst_x;
                
/*        if (mach64.accel.source_fg == SRC_BLITSRC || mach64.accel.source_bg == SRC_BLITSRC)
        {*/
                mach64.accel.xinc = (mach64.dst_cntl & 1) ? 1 : -1;
                mach64.accel.yinc = (mach64.dst_cntl & 2) ? 1 : -1;        
/*        }
        else
        {
                mach64.accel.xinc = mach64_inc[mach64.accel.src_pix_width];
                mach64.accel.yinc = 1;
        }*/
        
        mach64.accel.source_host = ((mach64.dp_src & 7) == SRC_HOST) || (((mach64.dp_src >> 8) & 7) == SRC_HOST);
        
        
//        pclog("mach64_start_fill : pattern %08X %08X\n", mach64.pat_reg0, mach64.pat_reg1);
        for (y = 0; y < 8; y++)
        {
                for (x = 0; x < 8; x++)
                {
                        uint32_t temp = (y & 4) ? mach64.pat_reg1 : mach64.pat_reg0;
                        mach64.accel.pattern[y][x] = (temp >> (x + ((y & 3) * 8))) & 1;
//                        pclog("%i ", mach64.accel.pattern[y][x]);                        
                }
//                pclog("\n");
        }
        
        mach64.accel.sc_left   =  mach64.sc_left_right & 0x1fff;
        mach64.accel.sc_right  = (mach64.sc_left_right >> 16) & 0x1fff;
        mach64.accel.sc_top    =  mach64.sc_top_bottom & 0x7fff;
        mach64.accel.sc_bottom = (mach64.sc_top_bottom >> 16) & 0x7fff;

/*        mach64.accel.sc_left   *= mach64_inc[mach64.accel.dst_pix_width];
        mach64.accel.sc_right  *= mach64_inc[mach64.accel.dst_pix_width];*/
        
        mach64.accel.dp_frgd_clr = mach64.dp_frgd_clr;
        mach64.accel.dp_bkgd_clr = mach64.dp_bkgd_clr;        
        
        mach64.accel.busy = 1;
#ifdef MACH64_DEBUG
        pclog("mach64_start_fill : dst %i, %i  src %i, %i  size %i, %i  src pitch %i offset %X  dst pitch %i offset %X  scissor %i %i %i %i  src_fg %i\n", mach64.accel.dst_x, mach64.accel.dst_y, mach64.accel.src_x, mach64.accel.src_y, mach64.accel.dst_width, mach64.accel.dst_height, mach64.accel.src_pitch, mach64.accel.src_offset, mach64.accel.dst_pitch, mach64.accel.dst_offset, mach64.accel.sc_left, mach64.accel.sc_right, mach64.accel.sc_top, mach64.accel.sc_bottom, mach64.accel.source_fg);
#endif
        mach64.accel.op = OP_RECT;
}

void mach64_start_line()
{
        int x, y;
        
        mach64.accel.dst_x = (mach64.dst_y_x >> 16) & 0xfff;
        mach64.accel.dst_y =  mach64.dst_y_x        & 0xfff;

        mach64.accel.src_x = (mach64.src_y_x >> 16) & 0xfff;
        mach64.accel.src_y =  mach64.src_y_x        & 0xfff;

        mach64.accel.src_pitch  = (mach64.src_off_pitch >> 22) * 8;
        mach64.accel.src_offset = (mach64.src_off_pitch & 0xfffff) * 8;

        mach64.accel.dst_pitch  = (mach64.dst_off_pitch >> 22) * 8;
        mach64.accel.dst_offset = (mach64.dst_off_pitch & 0xfffff) * 8;
        
        mach64.accel.mix_fg = (mach64.dp_mix >> 16) & 0x1f;
        mach64.accel.mix_bg = mach64.dp_mix & 0x1f;
        
        mach64.accel.source_bg  =  mach64.dp_src & 7;
        mach64.accel.source_fg  = (mach64.dp_src >> 8) & 7;
        mach64.accel.source_mix = (mach64.dp_src >> 16) & 7;
        
        mach64.accel.dst_pix_width  =  mach64.dp_pix_width        & 7;
        mach64.accel.src_pix_width  = (mach64.dp_pix_width >>  8) & 7;
        mach64.accel.host_pix_width = (mach64.dp_pix_width >> 16) & 7;
        
        mach64.accel.dst_size = mach64_width[mach64.accel.dst_pix_width];
        mach64.accel.src_size = mach64_width[mach64.accel.src_pix_width];

        mach64.accel.src_offset >>= mach64.accel.src_size;
        mach64.accel.dst_offset >>= mach64.accel.dst_size;

/*        mach64.accel.src_pitch *= mach64_inc[mach64.accel.src_pix_width];
        mach64.accel.dst_pitch *= mach64_inc[mach64.accel.dst_pix_width];*/
      
        mach64.accel.source_host = ((mach64.dp_src & 7) == SRC_HOST) || (((mach64.dp_src >> 8) & 7) == SRC_HOST);
        
        for (y = 0; y < 8; y++)
        {
                for (x = 0; x < 8; x++)
                {
                        uint32_t temp = (y & 4) ? mach64.pat_reg1 : mach64.pat_reg0;
                        mach64.accel.pattern[y][x] = (temp >> (x + ((y & 3) * 8))) & 1;
                }
        }
        
        mach64.accel.sc_left   =  mach64.sc_left_right & 0x1fff;
        mach64.accel.sc_right  = (mach64.sc_left_right >> 16) & 0x1fff;
        mach64.accel.sc_top    =  mach64.sc_top_bottom & 0x7fff;
        mach64.accel.sc_bottom = (mach64.sc_top_bottom >> 16) & 0x7fff;
        
        mach64.accel.dp_frgd_clr = mach64.dp_frgd_clr;
        mach64.accel.dp_bkgd_clr = mach64.dp_bkgd_clr;        
        
        mach64.accel.x_count = mach64.dst_bres_lnth & 0x7fff;
        mach64.accel.err = (mach64.dst_bres_err & 0x3ffff) | ((mach64.dst_bres_err & 0x40000) ? 0xfffc0000 : 0);
        
        mach64.accel.busy = 1;
#ifdef MACH64_DEBUG
        pclog("mach64_start_line\n");
#endif
        mach64.accel.op = OP_LINE;
}

#define READ(addr, dat, width) if (width == 0)      dat =               vram[((addr))      & 0x7fffff]; \
                               else if (width == 1) dat = *(uint16_t *)&vram[((addr) << 1) & 0x7fffff]; \
                               else                 dat = *(uint32_t *)&vram[((addr) << 2) & 0x7fffff];

#define MIX     switch (mix ? mach64.accel.mix_fg : mach64.accel.mix_bg)                                \
                {                                                                                       \
                        case 0x0: dest_dat =             ~dest_dat;  break;                             \
                        case 0x1: dest_dat =  0;                     break;                             \
                        case 0x2: dest_dat =  0xffffffff;            break;                             \
                        case 0x3: dest_dat =              dest_dat;  break;                             \
                        case 0x4: dest_dat =  ~src_dat;              break;                             \
                        case 0x5: dest_dat =   src_dat ^  dest_dat;  break;                             \
                        case 0x6: dest_dat = ~(src_dat ^  dest_dat); break;                             \
                        case 0x7: dest_dat =   src_dat;              break;                             \
                        case 0x8: dest_dat = ~(src_dat &  dest_dat); break;                             \
                        case 0x9: dest_dat =  ~src_dat |  dest_dat;  break;                             \
                        case 0xa: dest_dat =   src_dat | ~dest_dat;  break;                             \
                        case 0xb: dest_dat =   src_dat |  dest_dat;  break;                             \
                        case 0xc: dest_dat =   src_dat &  dest_dat;  break;                             \
                        case 0xd: dest_dat =   src_dat & ~dest_dat;  break;                             \
                        case 0xe: dest_dat =  ~src_dat &  dest_dat;  break;                             \
                        case 0xf: dest_dat = ~(src_dat |  dest_dat); break;                             \
                }

#define WRITE(addr, width)      if (width == 0)                                                         \
                                {                                                                       \
                                        vram[(addr) & 0x7fffff] = dest_dat;                             \
                                        changedvram[((addr) & 0x7fffff) >> 10] = changeframecount;      \
                                }                                                                       \
                                else if (width == 1)                                                    \
                                {                                                                       \
                                        *(uint16_t *)&vram[((addr) << 1) & 0x7fffff] = dest_dat;          \
                                        changedvram[(((addr) << 1) & 0x7fffff) >> 10] = changeframecount; \
                                }                                                                       \
                                else                                                                    \
                                {                                                                       \
                                        *(uint32_t *)&vram[((addr) << 2) & 0x7fffff] = dest_dat;          \
                                        changedvram[(((addr) << 2) & 0x7fffff) >> 10] = changeframecount; \
                                }

void mach64_blit(uint32_t cpu_dat, int count)
{
        if (!mach64.accel.busy)
        {
#ifdef MACH64_DEBUG
                pclog("mach64_blit : return as not busy\n");
#endif
                return;
        }
        switch (mach64.accel.op)
        {
                case OP_RECT:
                while (count)
                {
                        uint32_t src_dat, dest_dat;
                        uint32_t host_dat;
                        int mix;
                
                        if (mach64.accel.source_host)
                        {
                                host_dat = cpu_dat;
                                switch (mach64.accel.src_size)
                                {
                                        case 0:
                                        cpu_dat >>= 8;
                                        count -= 8;
                                        break;
                                        case 1:
                                        cpu_dat >>= 16;
                                        count -= 16;
                                        break;
                                        case 2:
                                        count -= 32;
                                        break;
                                }
                        }
                        else
                           count--;

                        switch (mach64.accel.source_mix)
                        {
                                case MONO_SRC_HOST:
                                mix = cpu_dat >> 31;
                                cpu_dat <<= 1;
                                break;
                                case MONO_SRC_PAT:
                                mix = mach64.accel.pattern[mach64.accel.dst_y & 7][mach64.accel.dst_x & 7];
                                break;
                                case MONO_SRC_1:
                                default:
                                mix = 1;
                                break;
                        }
                
                        if (mach64.accel.dst_x >= mach64.accel.sc_left && mach64.accel.dst_x <= mach64.accel.sc_right &&
                            mach64.accel.dst_y >= mach64.accel.sc_top  && mach64.accel.dst_y <= mach64.accel.sc_bottom)
                        {
                                switch (mix ? mach64.accel.source_fg : mach64.accel.source_bg)
                                {
                                        case SRC_HOST:
                                        src_dat = host_dat;
                                        break;
                                        case SRC_BLITSRC:
                                        READ(mach64.accel.src_offset + (mach64.accel.src_y * mach64.accel.src_pitch) + mach64.accel.src_x, src_dat, mach64.accel.src_size);
                                        break;
                                        case SRC_FG:
                                        src_dat = mach64.accel.dp_frgd_clr;
                                        break;
                                        case SRC_BG:
                                        src_dat = mach64.accel.dp_bkgd_clr;
                                        break;
                                        default:
                                        src_dat = 0;
                                        break;
                                }
                
                                READ(mach64.accel.dst_offset + (mach64.accel.dst_y * mach64.accel.dst_pitch) + mach64.accel.dst_x, dest_dat, mach64.accel.dst_size);

//                                pclog("Blit %i,%i %i,%i  %X %X  %i  %02X %02X %i ", mach64.accel.src_x, mach64.accel.src_y, mach64.accel.dst_x, mach64.accel.dst_y,  (mach64.accel.src_offset + (mach64.accel.src_y * mach64.accel.src_pitch) + mach64.accel.src_x) & 0x7fffff, (mach64.accel.dst_offset + (mach64.accel.dst_y * mach64.accel.dst_pitch) + mach64.accel.dst_x) & 0x7fffff, count, src_dat, dest_dat, mix);
                                
                                MIX

//                                pclog("%02X  %i\n", dest_dat, mach64.accel.dst_height);

                                WRITE(mach64.accel.dst_offset + (mach64.accel.dst_y * mach64.accel.dst_pitch) + mach64.accel.dst_x, mach64.accel.dst_size);
                        }
                
                        if (mach64.dst_cntl & DST_24_ROT_EN)
                        {
                                mach64.accel.dp_frgd_clr = ((mach64.accel.dp_frgd_clr >> 8) & 0xffff) | (mach64.accel.dp_frgd_clr << 16);
                                mach64.accel.dp_bkgd_clr = ((mach64.accel.dp_bkgd_clr >> 8) & 0xffff) | (mach64.accel.dp_bkgd_clr << 16);
                        }
                
                        mach64.accel.src_x += mach64.accel.xinc;
                        mach64.accel.dst_x += mach64.accel.xinc;
                        mach64.accel.x_count--;
                        
                        if (mach64.accel.x_count <= 0)
                        {
                                mach64.accel.x_count = mach64.accel.dst_width;
                                mach64.accel.src_x = mach64.accel.src_x_start;
                                mach64.accel.dst_x = mach64.accel.dst_x_start;

                                mach64.accel.src_y += mach64.accel.yinc;                        
                                mach64.accel.dst_y += mach64.accel.yinc;
                                mach64.accel.dst_height--;
                
                                if (mach64.accel.dst_height <= 0)
                                {
                                        /*Blit finished*/
#ifdef MACH64_DEBUG
                                        pclog("mach64 blit finished\n");
#endif
                                        mach64.accel.busy = 0;
                                        return;
                                }
                                if (mach64.accel.source_host)
                                        return;
                        }
                }        
                break;
                
                case OP_LINE:
                while (count)
                {
                        uint32_t src_dat, dest_dat;
                        uint32_t host_dat;
                        int mix;
                
                        if (mach64.accel.source_host)
                        {
                                host_dat = cpu_dat;
                                switch (mach64.accel.src_size)
                                {
                                        case 0:
                                        cpu_dat >>= 8;
                                        count -= 8;
                                        break;
                                        case 1:
                                        cpu_dat >>= 16;
                                        count -= 16;
                                        break;
                                        case 2:
                                        count -= 32;
                                        break;
                                }
                        }
                        else
                           count--;

                        switch (mach64.accel.source_mix)
                        {
                                case MONO_SRC_HOST:
                                mix = cpu_dat >> 31;
                                cpu_dat <<= 1;
                                break;
                                case MONO_SRC_PAT:
                                mix = mach64.accel.pattern[mach64.accel.dst_y & 7][mach64.accel.dst_x & 7];
                                break;
                                case MONO_SRC_1:
                                default:
                                mix = 1;
                                break;
                        }
                
                        if (mach64.accel.dst_x >= mach64.accel.sc_left && mach64.accel.dst_x <= mach64.accel.sc_right &&
                            mach64.accel.dst_y >= mach64.accel.sc_top  && mach64.accel.dst_y <= mach64.accel.sc_bottom)
                        {
                                switch (mix ? mach64.accel.source_fg : mach64.accel.source_bg)
                                {
                                        case SRC_HOST:
                                        src_dat = host_dat;
                                        break;
                                        case SRC_BLITSRC:
                                        READ(mach64.accel.src_offset + (mach64.accel.src_y * mach64.accel.src_pitch) + mach64.accel.src_x, src_dat, mach64.accel.src_size);
                                        break;
                                        case SRC_FG:
                                        src_dat = mach64.accel.dp_frgd_clr;
                                        break;
                                        case SRC_BG:
                                        src_dat = mach64.accel.dp_bkgd_clr;
                                        break;
                                        default:
                                        src_dat = 0;
                                        break;
                                }
                
                                READ(mach64.accel.dst_offset + (mach64.accel.dst_y * mach64.accel.dst_pitch) + mach64.accel.dst_x, dest_dat, mach64.accel.dst_size);

//                                pclog("Blit %i,%i %i,%i  %X %X  %i  %02X %02X %i ", mach64.accel.src_x, mach64.accel.src_y, mach64.accel.dst_x, mach64.accel.dst_y,  (mach64.accel.src_offset + (mach64.accel.src_y * mach64.accel.src_pitch) + mach64.accel.src_x) & 0x7fffff, (mach64.accel.dst_offset + (mach64.accel.dst_y * mach64.accel.dst_pitch) + mach64.accel.dst_x) & 0x7fffff, count, src_dat, dest_dat, mix);
                                
                                MIX

//                                pclog("%02X  %i\n", dest_dat, mach64.accel.dst_height);

                                WRITE(mach64.accel.dst_offset + (mach64.accel.dst_y * mach64.accel.dst_pitch) + mach64.accel.dst_x, mach64.accel.dst_size);
                        }
                
                        mach64.accel.x_count--;                
                        if (mach64.accel.x_count <= 0)
                        {
                                /*Blit finished*/
#ifdef MACH64_DEBUG
                                pclog("mach64 blit finished\n");
#endif
                                mach64.accel.busy = 0;
                                return;
                        }
                        
                        switch (mach64.dst_cntl & 7)
                        {
                                case 0: case 2: 
                                mach64.accel.src_x--;
                                mach64.accel.dst_x--;
                                break;
                                case 1: case 3:
                                mach64.accel.src_x++;
                                mach64.accel.dst_x++;
                                break;
                                case 4: case 5: 
                                mach64.accel.src_y--;
                                mach64.accel.dst_y--;
                                break;
                                case 6: case 7:
                                mach64.accel.src_y++;
                                mach64.accel.dst_y++;
                                break;
                        }
#ifdef MACH64_DEBUG
                        pclog("x %i y %i err %i inc %i dec %i\n", mach64.accel.dst_x, mach64.accel.dst_y, mach64.accel.err, mach64.dst_bres_inc, mach64.dst_bres_dec);
#endif
                        if (mach64.accel.err >= 0)
                        {
                                mach64.accel.err += mach64.dst_bres_dec;
                                
                                switch (mach64.dst_cntl & 7)
                                {
                                        case 0: case 1: 
                                        mach64.accel.src_y--;
                                        mach64.accel.dst_y--;
                                        break;
                                        case 2: case 3:
                                        mach64.accel.src_y++;
                                        mach64.accel.dst_y++;
                                        break;
                                        case 4: case 6: 
                                        mach64.accel.src_x--;
                                        mach64.accel.dst_x--;
                                        break;
                                        case 5: case 7:
                                        mach64.accel.src_x++;
                                        mach64.accel.dst_x++;
                                        break;
                                }
                        }
                        else
                                mach64.accel.err += mach64.dst_bres_inc;
                }
                break;
        }
}

void mach64_load_context()
{
        uint32_t addr;
        
        while (mach64.context_load_cntl & 0x30000)
        {
                addr = (0x3fff - (mach64.context_load_cntl & 0x3fff)) * 256;
                mach64.context_mask = *(uint32_t *)&vram[addr];
#ifdef MACH64_DEBUG
                pclog("mach64_load_context %08X from %08X : mask %08X\n", mach64.context_load_cntl, addr, mach64.context_mask);
#endif
 
                if (mach64.context_mask & (1 << 2))
                        mach64_ext_writel(0x100, *(uint32_t *)&vram[addr + 0x08], NULL);
                if (mach64.context_mask & (1 << 3))
                        mach64_ext_writel(0x10c, *(uint32_t *)&vram[addr + 0x0c], NULL);
                if (mach64.context_mask & (1 << 4))
                        mach64_ext_writel(0x118, *(uint32_t *)&vram[addr + 0x10], NULL);
                if (mach64.context_mask & (1 << 5))
                        mach64_ext_writel(0x124, *(uint32_t *)&vram[addr + 0x14], NULL);
                if (mach64.context_mask & (1 << 6))
                        mach64_ext_writel(0x128, *(uint32_t *)&vram[addr + 0x18], NULL);
                if (mach64.context_mask & (1 << 7))
                        mach64_ext_writel(0x12c, *(uint32_t *)&vram[addr + 0x1c], NULL);
                if (mach64.context_mask & (1 << 8))
                        mach64_ext_writel(0x180, *(uint32_t *)&vram[addr + 0x20], NULL);
                if (mach64.context_mask & (1 << 9))
                        mach64_ext_writel(0x18c, *(uint32_t *)&vram[addr + 0x24], NULL);
                if (mach64.context_mask & (1 << 10))
                        mach64_ext_writel(0x198, *(uint32_t *)&vram[addr + 0x28], NULL);
                if (mach64.context_mask & (1 << 11))
                        mach64_ext_writel(0x1a4, *(uint32_t *)&vram[addr + 0x2c], NULL);
                if (mach64.context_mask & (1 << 12))
                        mach64_ext_writel(0x1b0, *(uint32_t *)&vram[addr + 0x30], NULL);
                if (mach64.context_mask & (1 << 13))
                        mach64_ext_writel(0x280, *(uint32_t *)&vram[addr + 0x34], NULL);
                if (mach64.context_mask & (1 << 14))
                        mach64_ext_writel(0x284, *(uint32_t *)&vram[addr + 0x38], NULL);
                if (mach64.context_mask & (1 << 15))
                        mach64_ext_writel(0x2a8, *(uint32_t *)&vram[addr + 0x3c], NULL);
                if (mach64.context_mask & (1 << 16))
                        mach64_ext_writel(0x2b4, *(uint32_t *)&vram[addr + 0x40], NULL);
                if (mach64.context_mask & (1 << 17))
                        mach64_ext_writel(0x2c0, *(uint32_t *)&vram[addr + 0x44], NULL);
                if (mach64.context_mask & (1 << 18))
                        mach64_ext_writel(0x2c4, *(uint32_t *)&vram[addr + 0x48], NULL);
                if (mach64.context_mask & (1 << 19))
                        mach64_ext_writel(0x2c8, *(uint32_t *)&vram[addr + 0x4c], NULL);
                if (mach64.context_mask & (1 << 20))
                        mach64_ext_writel(0x2cc, *(uint32_t *)&vram[addr + 0x50], NULL);
                if (mach64.context_mask & (1 << 21))
                        mach64_ext_writel(0x2d0, *(uint32_t *)&vram[addr + 0x54], NULL);
                if (mach64.context_mask & (1 << 22))
                        mach64_ext_writel(0x2d4, *(uint32_t *)&vram[addr + 0x58], NULL);
                if (mach64.context_mask & (1 << 23))
                        mach64_ext_writel(0x2d8, *(uint32_t *)&vram[addr + 0x5c], NULL);
                if (mach64.context_mask & (1 << 24))
                        mach64_ext_writel(0x300, *(uint32_t *)&vram[addr + 0x60], NULL);
                if (mach64.context_mask & (1 << 25))
                        mach64_ext_writel(0x304, *(uint32_t *)&vram[addr + 0x64], NULL);
                if (mach64.context_mask & (1 << 26))
                        mach64_ext_writel(0x308, *(uint32_t *)&vram[addr + 0x68], NULL);
                if (mach64.context_mask & (1 << 27))
                        mach64_ext_writel(0x330, *(uint32_t *)&vram[addr + 0x6c], NULL);
                
                mach64.context_load_cntl = *(uint32_t *)&vram[addr + 0x70];
        }
}

uint8_t mach64_ext_readb(uint32_t addr, void *priv)
{
        uint8_t ret;
        switch (addr & 0x3ff)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                READ8(addr, mach64.crtc_h_total_disp);
                break;
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                READ8(addr, mach64.crtc_v_total_disp);
                break;
                case 0x0c: case 0x0d: case 0x0e: case 0x0f:
                READ8(addr, mach64.crtc_v_sync_strt_wid);
                break;

                case 0x14: case 0x15: case 0x16: case 0x17:
                READ8(addr, mach64.crtc_off_pitch);
                break;
                
                case 0x18:
                ret = mach64.crtc_int_cntl & ~1;
                if (cgastat & 8)
                        ret |= 1;
                break;

                case 0x1c: case 0x1d: case 0x1e: case 0x1f:
                READ8(addr, mach64.crtc_gen_cntl);
                break;

                case 0x68: case 0x69: case 0x6a: case 0x6b:
                READ8(addr, mach64.cur_offset);
                break;
                case 0x6c: case 0x6d: case 0x6e: case 0x6f:
                READ8(addr, mach64.cur_horz_vert_posn);
                break;
                case 0x70: case 0x71: case 0x72: case 0x73:
                READ8(addr, mach64.cur_horz_vert_off);
                break;

                case 0x80: case 0x81: case 0x82: case 0x83:
                READ8(addr, mach64.scratch_reg0);
                break;
                case 0x84: case 0x85: case 0x86: case 0x87:
                READ8(addr, mach64.scratch_reg1);
                break;

                case 0x90: case 0x91: case 0x92: case 0x93:
                READ8(addr, mach64.clock_cntl);
                break;

                case 0xb0: case 0xb1: case 0xb2: case 0xb3:
                READ8(addr, mach64.mem_cntl);
                break;

                case 0xc4: case 0xc5: case 0xc6: case 0xc7:
                READ8(addr, mach64.dac_cntl);
                break;

                case 0xd0: case 0xd1: case 0xd2: case 0xd3:
                READ8(addr, mach64.gen_test_cntl);
                break;
                
                case 0xe0: case 0xe1: case 0xe2: case 0xe3:
                READ8(addr, 0xd7); /*88800GX*/
                break;
                
                case 0x100: case 0x101: case 0x102: case 0x103:
                READ8(addr, mach64.dst_off_pitch);
                break;
                case 0x104: case 0x105:
                READ8(addr, mach64.dst_y_x);
                break;
                case 0x108: case 0x109: case 0x11c: case 0x11d:
                READ8(addr + 2, mach64.dst_y_x);
                break;
                case 0x10c: case 0x10d: case 0x10e: case 0x10f:
                READ8(addr, mach64.dst_y_x);
                break;
                case 0x110: case 0x111:
                addr += 2;
                case 0x114: case 0x115:
                case 0x118: case 0x119: case 0x11a: case 0x11b:
                case 0x11e: case 0x11f:
                READ8(addr, mach64.dst_height_width);
                break;

                case 0x120: case 0x121: case 0x122: case 0x123:
                READ8(addr, mach64.dst_bres_lnth);
                break;
                case 0x124: case 0x125: case 0x126: case 0x127:
                READ8(addr, mach64.dst_bres_err);
                break;
                case 0x128: case 0x129: case 0x12a: case 0x12b:
                READ8(addr, mach64.dst_bres_inc);
                break;
                case 0x12c: case 0x12d: case 0x12e: case 0x12f:
                READ8(addr, mach64.dst_bres_dec);
                break;

                case 0x130: case 0x131: case 0x132: case 0x133:
                READ8(addr, mach64.dst_cntl);
                break;

                case 0x180: case 0x181: case 0x182: case 0x183:
                READ8(addr, mach64.src_off_pitch);
                break;
                case 0x184: case 0x185:
                READ8(addr, mach64.src_y_x);
                break;
                case 0x188: case 0x189:
                READ8(addr + 2, mach64.src_y_x);
                break;
                case 0x18c: case 0x18d: case 0x18e: case 0x18f:
                READ8(addr, mach64.src_y_x);
                break;

                case 0x1b4: case 0x1b5: case 0x1b6: case 0x1b7:
                READ8(addr, mach64.src_cntl);
                break;

                case 0x280: case 0x281: case 0x282: case 0x283:
                READ8(addr, mach64.pat_reg0);
                break;
                case 0x284: case 0x285: case 0x286: case 0x287:
                READ8(addr, mach64.pat_reg1);
                break;

                case 0x2a0: case 0x2a1: case 0x2a8: case 0x2a9:
                READ8(addr, mach64.sc_left_right);
                break;
                case 0x2a4: case 0x2a5:
                addr += 2;
                case 0x2aa: case 0x2ab:                       
                READ8(addr, mach64.sc_left_right);
                break;

                case 0x2ac: case 0x2ad: case 0x2b4: case 0x2b5:
                READ8(addr, mach64.sc_top_bottom);
                break;
                case 0x2b0: case 0x2b1:
                addr += 2;
                case 0x2b6: case 0x2b7:
                READ8(addr, mach64.sc_top_bottom);
                break;

                case 0x2c0: case 0x2c1: case 0x2c2: case 0x2c3:
                READ8(addr, mach64.dp_bkgd_clr);
                break;
                case 0x2c4: case 0x2c5: case 0x2c6: case 0x2c7:
                READ8(addr, mach64.dp_frgd_clr);
                break;
                        
                case 0x2d0: case 0x2d1: case 0x2d2: case 0x2d3:
                READ8(addr, mach64.dp_pix_width);
                break;
                case 0x2d4: case 0x2d5: case 0x2d6: case 0x2d7:
                READ8(addr, mach64.dp_mix);
                break;
                case 0x2d8: case 0x2d9: case 0x2da: case 0x2db:
                READ8(addr, mach64.dp_src);
                break;

                case 0x320: case 0x321: case 0x322: case 0x323:
                READ8(addr, mach64.context_mask);
                break;

                case 0x330: case 0x331:
                READ8(addr, mach64.dst_cntl);
                break;
                case 0x332:
                READ8(addr - 2, mach64.src_cntl);
                break;
                case 0x333:
                READ8(addr - 3, mach64.pat_cntl);
                break;

                default:
                ret = 0;
                break;
        }
#ifdef MACH64_DEBUG
        if ((addr & 0x3fc) != 0x018) pclog("mach64_ext_readb : addr %08X ret %02X\n", addr, ret);
#endif
        return ret;
}
uint16_t mach64_ext_readw(uint32_t addr, void *priv)
{
        uint16_t ret;
        switch (addr & 0x3ff)
        {
                default:
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                ret = mach64_ext_readb(addr, priv);
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                ret |= mach64_ext_readb(addr + 1, priv) << 8;
                break;
        }
#ifdef MACH64_DEBUG
        if ((addr & 0x3fc) != 0x018) pclog("mach64_ext_readw : addr %08X ret %04X\n", addr, ret);
#endif
        return ret;        
}
uint32_t mach64_ext_readl(uint32_t addr, void *priv)
{
        uint32_t ret;
        switch (addr & 0x3ff)
        {
                case 0x18:
                ret = mach64.crtc_int_cntl & ~1;
                if (cgastat & 8)
                        ret |= 1;
                break;

                case 0xb4:
                ret = (mach64_bank_w[0] >> 15) | ((mach64_bank_w[1] >> 15) << 16);
                break;
                case 0xb8:
                ret = (mach64_bank_r[0] >> 15) | ((mach64_bank_r[1] >> 15) << 16);
                break;
                
                default:
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                ret = mach64_ext_readw(addr, priv);
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                ret |= mach64_ext_readw(addr + 2, priv) << 16;
                break;
        }
#ifdef MACH64_DEBUG
        if ((addr & 0x3fc) != 0x018) pclog("mach64_ext_readl : addr %08X ret %08X\n", addr, ret);
#endif
        return ret;
}

void mach64_ext_writeb(uint32_t addr, uint8_t val, void *priv)
{
#ifdef MACH64_DEBUG
        pclog("mach64_ext_writeb : addr %08X val %02X\n", addr, val);
#endif
        switch (addr & 0x3ff)
        {
                case 0x00: case 0x01: case 0x02: case 0x03:
                WRITE8(addr, mach64.crtc_h_total_disp, val);
                svga_recalctimings();
                break;
                case 0x08: case 0x09: case 0x0a: case 0x0b:
                WRITE8(addr, mach64.crtc_v_total_disp, val);
                svga_recalctimings();
                break;
                case 0x0c: case 0x0d: case 0x0e: case 0x0f:
                WRITE8(addr, mach64.crtc_v_sync_strt_wid, val);
                svga_recalctimings();
                break;

                case 0x14: case 0x15: case 0x16: case 0x17:
                WRITE8(addr, mach64.crtc_off_pitch, val);
                svga_recalctimings();
                fullchange = changeframecount;
                break;
                
                case 0x18:
                mach64.crtc_int_cntl = val;
                break;

                case 0x1c: case 0x1d: case 0x1e: case 0x1f:
                WRITE8(addr, mach64.crtc_gen_cntl, val);
                svga_recalctimings();
                break;

                case 0x68: case 0x69: case 0x6a: case 0x6b:
                WRITE8(addr, mach64.cur_offset, val);
                svga_hwcursor.addr = (mach64.cur_offset & 0xfffff) * 8;
                mach64_cursor_dump();
                break;
                case 0x6c: case 0x6d: case 0x6e: case 0x6f:
                WRITE8(addr, mach64.cur_horz_vert_posn, val);
                svga_hwcursor.x = mach64.cur_horz_vert_posn & 0x7ff;
                svga_hwcursor.y = (mach64.cur_horz_vert_posn >> 16) & 0x7ff;
                mach64_cursor_dump();
                break;
                case 0x70: case 0x71: case 0x72: case 0x73:
                WRITE8(addr, mach64.cur_horz_vert_off, val);
                svga_hwcursor.xoff = mach64.cur_horz_vert_off & 0x3f;
                svga_hwcursor.yoff = (mach64.cur_horz_vert_off >> 16) & 0x3f;
                mach64_cursor_dump();
                break;

                case 0x80: case 0x81: case 0x82: case 0x83:
                WRITE8(addr, mach64.scratch_reg0, val);
                break;
                case 0x84: case 0x85: case 0x86: case 0x87:
                WRITE8(addr, mach64.scratch_reg1, val);
                break;

                case 0x90: case 0x91: case 0x92: case 0x93:
                WRITE8(addr, mach64.clock_cntl, val);
                ics2595_write(val & 0x40, val & 0xf);
                break;

                case 0xb0: case 0xb1: case 0xb2: case 0xb3:
                WRITE8(addr, mach64.mem_cntl, val);
                break;

                case 0xb4:
                mach64_bank_w[0] = val * 32768;
#ifdef MACH64_DEBUG
                pclog("mach64 : write bank A0000-A7FFF set to %08X\n", mach64_bank_w[0]);
#endif
                break;
                case 0xb5: case 0xb6:
                mach64_bank_w[1] = val * 32768;
#ifdef MACH64_DEBUG
                pclog("mach64 : write bank A8000-AFFFF set to %08X\n", mach64_bank_w[1]);
#endif
                break;
                case 0xb8:
                mach64_bank_r[0] = val * 32768;
#ifdef MACH64_DEBUG
                pclog("mach64 :  read bank A0000-A7FFF set to %08X\n", mach64_bank_r[0]);
#endif
                break;
                case 0xb9: case 0xba:
                mach64_bank_r[1] = val * 32768;
#ifdef MACH64_DEBUG
                pclog("mach64 :  read bank A8000-AFFFF set to %08X\n", mach64_bank_r[1]);
#endif
                break;

                case 0xc4: case 0xc5: case 0xc6: case 0xc7:
                WRITE8(addr, mach64.dac_cntl, val);
                break;

                case 0xd0: case 0xd1: case 0xd2: case 0xd3:
                WRITE8(addr, mach64.gen_test_cntl, val);
//                if (val == 2) output = 3;
                ati_eeprom_write(mach64.gen_test_cntl & 0x10, mach64.gen_test_cntl & 2, mach64.gen_test_cntl & 1);
                mach64.gen_test_cntl = (mach64.gen_test_cntl & ~8) | (ati_eeprom_read() ? 8 : 0);
                svga_hwcursor.ena = mach64.gen_test_cntl & 0x80;
                mach64_cursor_dump();
                break;

                case 0x100: case 0x101: case 0x102: case 0x103:
                WRITE8(addr, mach64.dst_off_pitch, val);
                break;
                case 0x104: case 0x105: case 0x11c: case 0x11d:
                WRITE8(addr + 2, mach64.dst_y_x, val);
                break;
                case 0x108: case 0x109:
                WRITE8(addr, mach64.dst_y_x, val);
                break;
                case 0x10c: case 0x10d: case 0x10e: case 0x10f:
                WRITE8(addr, mach64.dst_y_x, val);
                break;
                case 0x110: case 0x111:
                addr += 2;
                case 0x114: case 0x115:
                case 0x118: case 0x119: case 0x11a: case 0x11b:
                case 0x11e: case 0x11f:
                WRITE8(addr, mach64.dst_height_width, val);
                if ((addr & 0x3ff) == 0x11b || (addr & 0x3ff) == 0x11f)
                {
                        mach64_start_fill();
#ifdef MACH64_DEBUG
                        pclog("%i %i %i %i %i\n", (mach64.dst_height_width & 0x7ff), (mach64.dst_height_width & 0x7ff0000),
                            ((mach64.dp_src & 7) != SRC_HOST), (((mach64.dp_src >> 8) & 7) != SRC_HOST),
                            (((mach64.dp_src >> 16) & 3) != MONO_SRC_HOST));
#endif                            
                        if ((mach64.dst_height_width & 0x7ff) && (mach64.dst_height_width & 0x7ff0000) && 
                            ((mach64.dp_src & 7) != SRC_HOST) && (((mach64.dp_src >> 8) & 7) != SRC_HOST) && 
                            (((mach64.dp_src >> 16) & 3) != MONO_SRC_HOST))
                                mach64_blit(0, -1);
                }
                break;

                case 0x120: case 0x121: case 0x122: case 0x123:
                WRITE8(addr, mach64.dst_bres_lnth, val);
                if ((addr & 0x3ff) == 0x123 && !(val & 0x80))
                {
                        mach64_start_line();

                        if ((mach64.dst_bres_lnth & 0x7fff) &&
                            ((mach64.dp_src & 7) != SRC_HOST) && (((mach64.dp_src >> 8) & 7) != SRC_HOST) && 
                            (((mach64.dp_src >> 16) & 3) != MONO_SRC_HOST))
                                mach64_blit(0, -1);
                }
                break;
                case 0x124: case 0x125: case 0x126: case 0x127:
                WRITE8(addr, mach64.dst_bres_err, val);
                break;
                case 0x128: case 0x129: case 0x12a: case 0x12b:
                WRITE8(addr, mach64.dst_bres_inc, val);
                break;
                case 0x12c: case 0x12d: case 0x12e: case 0x12f:
                WRITE8(addr, mach64.dst_bres_dec, val);
                break;

                case 0x130: case 0x131: case 0x132: case 0x133:
                WRITE8(addr, mach64.dst_cntl, val);
                break;

                case 0x180: case 0x181: case 0x182: case 0x183:
                WRITE8(addr, mach64.src_off_pitch, val);
                break;
                case 0x184: case 0x185:
                WRITE8(addr, mach64.src_y_x, val);
                break;
                case 0x188: case 0x189:
                WRITE8(addr + 2, mach64.src_y_x, val);
                break;
                case 0x18c: case 0x18d: case 0x18e: case 0x18f:
                WRITE8(addr, mach64.src_y_x, val);
                break;

                case 0x1b4: case 0x1b5: case 0x1b6: case 0x1b7:
                WRITE8(addr, mach64.src_cntl, val);
                break;

                case 0x200: case 0x201: case 0x202: case 0x203:
                case 0x204: case 0x205: case 0x206: case 0x207:
                case 0x208: case 0x209: case 0x20a: case 0x20b:
                case 0x20c: case 0x20d: case 0x20e: case 0x20f:
                case 0x210: case 0x211: case 0x212: case 0x213:
                case 0x214: case 0x215: case 0x216: case 0x217:
                case 0x218: case 0x219: case 0x21a: case 0x21b:
                case 0x21c: case 0x21d: case 0x21e: case 0x21f:
                case 0x220: case 0x221: case 0x222: case 0x223:
                case 0x224: case 0x225: case 0x226: case 0x227:
                case 0x228: case 0x229: case 0x22a: case 0x22b:
                case 0x22c: case 0x22d: case 0x22e: case 0x22f:
                case 0x230: case 0x231: case 0x232: case 0x233:
                case 0x234: case 0x235: case 0x236: case 0x237:
                case 0x238: case 0x239: case 0x23a: case 0x23b:
                case 0x23c: case 0x23d: case 0x23e: case 0x23f:
                mach64_blit(val, 8);
                break;

                case 0x280: case 0x281: case 0x282: case 0x283:
                WRITE8(addr, mach64.pat_reg0, val);
                break;
                case 0x284: case 0x285: case 0x286: case 0x287:
                WRITE8(addr, mach64.pat_reg1, val);
                break;

                case 0x2a0: case 0x2a1: case 0x2a8: case 0x2a9:
                WRITE8(addr, mach64.sc_left_right, val);
                break;
                case 0x2a4: case 0x2a5:
                addr += 2;
                case 0x2aa: case 0x2ab:                       
                WRITE8(addr, mach64.sc_left_right, val);
                break;

                case 0x2ac: case 0x2ad: case 0x2b4: case 0x2b5:
                WRITE8(addr, mach64.sc_top_bottom, val);
                break;
                case 0x2b0: case 0x2b1:
                addr += 2;
                case 0x2b6: case 0x2b7:
                WRITE8(addr, mach64.sc_top_bottom, val);
                break;

                case 0x2c0: case 0x2c1: case 0x2c2: case 0x2c3:
                WRITE8(addr, mach64.dp_bkgd_clr, val);
                break;
                case 0x2c4: case 0x2c5: case 0x2c6: case 0x2c7:
                WRITE8(addr, mach64.dp_frgd_clr, val);
                break;

                case 0x2d0: case 0x2d1: case 0x2d2: case 0x2d3:
                WRITE8(addr, mach64.dp_pix_width, val);
                break;
                case 0x2d4: case 0x2d5: case 0x2d6: case 0x2d7:
                WRITE8(addr, mach64.dp_mix, val);
                break;
                case 0x2d8: case 0x2d9: case 0x2da: case 0x2db:
                WRITE8(addr, mach64.dp_src, val);
                break;

                case 0x320: case 0x321: case 0x322: case 0x323:
                WRITE8(addr, mach64.context_mask, val);
                break;

                case 0x330: case 0x331:
                WRITE8(addr, mach64.dst_cntl, val);
                break;
                case 0x332:
                WRITE8(addr - 2, mach64.src_cntl, val);
                break;
                case 0x333:
                WRITE8(addr - 3, mach64.pat_cntl, val & 7);
                break;
        }
}
void mach64_ext_writew(uint32_t addr, uint16_t val, void *priv)
{
#ifdef MACH64_DEBUG
        pclog("mach64_ext_writew : addr %08X val %04X\n", addr, val);
#endif
        switch (addr & 0x3fe)
        {
                case 0x200: case 0x202: case 0x204: case 0x206:
                case 0x208: case 0x20a: case 0x20c: case 0x20e:
                case 0x210: case 0x212: case 0x214: case 0x216:
                case 0x218: case 0x21a: case 0x21c: case 0x21e:
                case 0x220: case 0x222: case 0x224: case 0x226:
                case 0x228: case 0x22a: case 0x22c: case 0x22e:
                case 0x230: case 0x232: case 0x234: case 0x236:
                case 0x238: case 0x23a: case 0x23c: case 0x23e:
                mach64_blit(val, 16);
                break;

                default:
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_writeb(addr, val, priv);
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_writeb(addr + 1, val >> 8, priv);
                break;
        }
}
void mach64_ext_writel(uint32_t addr, uint32_t val, void *priv)
{
#ifdef MACH64_DEBUG
        if ((addr & 0x3c0) != 0x200)
                pclog("mach64_ext_writel : addr %08X val %08X\n", addr, val);
#endif
        switch (addr & 0x3fc)
        {
                case 0x32c:
                mach64.context_load_cntl = val;
                if (val & 0x30000)
                        mach64_load_context();
                break;
                
                case 0x200: case 0x204: case 0x208: case 0x20c:
                case 0x210: case 0x214: case 0x218: case 0x21c:
                case 0x220: case 0x224: case 0x228: case 0x22c:
                case 0x230: case 0x234: case 0x238: case 0x23c:
                if (mach64.accel.source_host)
                        mach64_blit(val, 32);
                else
                        mach64_blit(((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24), 32);                
                break;
                
                default:
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_writew(addr, val, priv);
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_writew(addr + 2, val >> 16, priv);
                break;
        }
}

uint8_t mach64_ext_inb(uint16_t port, void *priv)
{
        uint8_t ret;
//        if (CS == 0x2be7) output = 3;
        switch (port)
        {
                case 0x02ec: case 0x02ed: case 0x02ee: case 0x02ef:
                ret = mach64_ext_readb(0x00 | (port & 3), NULL);
                break;
                case 0x0aec: case 0x0aed: case 0x0aee: case 0x0aef:
                ret = mach64_ext_readb(0x08 | (port & 3), NULL);
                break;
                case 0x0eec: case 0x0eed: case 0x0eee: case 0x0eef:
                ret = mach64_ext_readb(0x0c | (port & 3), NULL);
                break;

                case 0x16ec: case 0x16ed: case 0x16ee: case 0x16ef:
                ret = mach64_ext_readb(0x14 | (port & 3), NULL);
                break;

                case 0x1aec:
                ret = mach64_ext_readb(0x18, NULL);
                break;

                case 0x1eec: case 0x1eed: case 0x1eee: case 0x1eef:
                ret = mach64_ext_readb(0x1c | (port & 3), NULL);
                break;

                case 0x36ec: case 0x36ed: case 0x36ee: case 0x36ef:
                ret = mach64_ext_readb(0x68 | (port & 3), NULL);
                break;
                case 0x3aec: case 0x3aed: case 0x3aee: case 0x3aef:
                ret = mach64_ext_readb(0x6c | (port & 3), NULL);
                break;
                case 0x3eec: case 0x3eed: case 0x3eee: case 0x3eef:
                ret = mach64_ext_readb(0x70 | (port & 3), NULL);
                break;

                case 0x42ec: case 0x42ed: case 0x42ee: case 0x42ef:
                ret = mach64_ext_readb(0x80 | (port & 3), NULL);
                break;
                case 0x46ec: case 0x46ed: case 0x46ee: case 0x46ef:
                ret = mach64_ext_readb(0x84 | (port & 3), NULL);
                break;
                case 0x4aec: case 0x4aed: case 0x4aee: case 0x4aef:
                ret = mach64_ext_readb(0x90 | (port & 3), NULL);
                break;

                case 0x52ec: case 0x52ed: case 0x52ee: case 0x52ef:
                ret = mach64_ext_readb(0xb0 | (port & 3), NULL);
                break;
                        
                case 0x56ec:
                ret = mach64_ext_readb(0xb4, NULL);
                break;
                case 0x56ed: case 0x56ee:
                ret = mach64_ext_readb(0xb5, NULL);
                break;
                case 0x5aec:
                ret = mach64_ext_readb(0xb8, NULL);
                break;
                case 0x5aed: case 0x5aee:
                ret = mach64_ext_readb(0xb9, NULL);
                break;

                case 0x5eec: case 0x5eed: case 0x5eee: case 0x5eef:
                ret = ati68860_ramdac_in((port & 3) | ((mach64.dac_cntl & 3) << 2), NULL);
                break;
                
                case 0x62ec: case 0x62ed: case 0x62ee: case 0x62ef:
                ret = mach64_ext_readb(0xc4 | (port & 3), NULL);
                break;
                        
                case 0x66ec: case 0x66ed: case 0x66ee: case 0x66ef:
                ret = mach64_ext_readb(0xd0 | (port & 3), NULL);
                break;

                case 0x6aec: case 0x6aed: case 0x6aee: case 0x6aef:
                READ8(port, mach64.config_cntl);
                break;
                
                case 0x6eec: case 0x6eed: case 0x6eee: case 0x6eef:
                ret = mach64_ext_readb(0xe0 | (port & 3), NULL);
                break;

                case 0x72ec:
                ret = 6 | (3 << 3); /*VLB, 256Kx16 DRAM*/
                break;
                
                default:
                ret = 0;
                break;
        }
#ifdef MACH64_DEBUG
        pclog("mach64_ext_inb : port %04X ret %02X  %04X:%04X\n", port, ret, CS,pc);
#endif
        return ret;
}
uint16_t mach64_ext_inw(uint16_t port, void *priv)
{
        uint16_t ret;
        switch (port)
        {
                default:
                pclog("  ");
                ret = mach64_ext_inb(port, priv);
                pclog("  ");
                ret |= (mach64_ext_inb(port + 1, priv) << 8);
                break;
        }
#ifdef MACH64_DEBUG
        pclog("mach64_ext_inw : port %04X ret %04X\n", port, ret);
#endif
        return ret;        
}
uint32_t mach64_ext_inl(uint16_t port, void *priv)
{
        uint32_t ret;
        switch (port)
        {
                case 0x56ec:
                ret = mach64_ext_readl(0xb4, NULL);
                break;
                case 0x5aec:
                ret = mach64_ext_readl(0xb8, NULL);
                break;

                default:
                pclog("  ");
                ret = mach64_ext_inw(port, priv);
                pclog("  ");
                ret |= (mach64_ext_inw(port + 2, priv) << 16);
                break;
        }
#ifdef MACH64_DEBUG
        pclog("mach64_ext_inl : port %04X ret %08X\n", port, ret);
#endif
        return ret;
}

void mach64_ext_outb(uint16_t port, uint8_t val, void *priv)
{
#ifdef MACH64_DEBUG
        pclog("mach64_ext_outb : port %04X val %02X  %04X:%04X\n", port, val, CS,pc);
#endif
        switch (port)
        {
                case 0x02ec: case 0x02ed: case 0x02ee: case 0x02ef:
                case 0x7eec: case 0x7eed: case 0x7eee: case 0x7eef:
                mach64_ext_writeb(0x00 | (port & 3), val, NULL);
                break;
                case 0x0aec: case 0x0aed: case 0x0aee: case 0x0aef:
                mach64_ext_writeb(0x08 | (port & 3), val, NULL);
                break;
                case 0x0eec: case 0x0eed: case 0x0eee: case 0x0eef:
                mach64_ext_writeb(0x0c | (port & 3), val, NULL);
                break;

                case 0x16ec: case 0x16ed: case 0x16ee: case 0x16ef:
                mach64_ext_writeb(0x14 | (port & 3), val, NULL);
                break;
                
                case 0x1aec:
                mach64_ext_writeb(0x18, val, NULL);
                break;

                case 0x1eec: case 0x1eed: case 0x1eee: case 0x1eef:
                mach64_ext_writeb(0x1c | (port & 3), val, NULL);
                break;

                case 0x36ec: case 0x36ed: case 0x36ee: case 0x36ef:
                mach64_ext_writeb(0x68 | (port & 3), val, NULL);
                break;
                case 0x3aec: case 0x3aed: case 0x3aee: case 0x3aef:
                mach64_ext_writeb(0x6c | (port & 3), val, NULL);
                break;
                case 0x3eec: case 0x3eed: case 0x3eee: case 0x3eef:
                mach64_ext_writeb(0x70 | (port & 3), val, NULL);
                break;

                case 0x42ec: case 0x42ed: case 0x42ee: case 0x42ef:
                mach64_ext_writeb(0x80 | (port & 3), val, NULL);
                break;
                case 0x46ec: case 0x46ed: case 0x46ee: case 0x46ef:
                mach64_ext_writeb(0x84 | (port & 3), val, NULL);
                break;
                case 0x4aec: case 0x4aed: case 0x4aee: case 0x4aef:
                mach64_ext_writeb(0x90 | (port & 3), val, NULL);
                break;

                case 0x52ec: case 0x52ed: case 0x52ee: case 0x52ef:
                mach64_ext_writeb(0xb0 | (port & 3), val, NULL);
                break;

                case 0x56ec:
                mach64_ext_writeb(0xb4, val, NULL);
                break;
                case 0x56ed: case 0x56ee:
                mach64_ext_writeb(0xb5, val, NULL);
                break;
                case 0x5aec:
                mach64_ext_writeb(0xb8, val, NULL);
                break;
                case 0x5aed: case 0x5aee:
                mach64_ext_writeb(0xb9, val, NULL);
                break;

                case 0x5eec: case 0x5eed: case 0x5eee: case 0x5eef:
                ati68860_ramdac_out((port & 3) | ((mach64.dac_cntl & 3) << 2), val, NULL);
                break;

                case 0x62ec: case 0x62ed: case 0x62ee: case 0x62ef:
                mach64_ext_writeb(0xc4 | (port & 3), val, NULL);
                break;

                case 0x66ec: case 0x66ed: case 0x66ee: case 0x66ef:
                mach64_ext_writeb(0xd0 | (port & 3), val, NULL);
                break;

                case 0x6aec: case 0x6aed: case 0x6aee: case 0x6aef:
                WRITE8(port, mach64.config_cntl, val);
                break;
        }
}
void mach64_ext_outw(uint16_t port, uint16_t val, void *priv)
{
#ifdef MACH64_DEBUG
        pclog("mach64_ext_outw : port %04X val %04X\n", port, val);
#endif
        switch (port)
        {
                default:
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_outb(port, val, priv);
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_outb(port + 1, val >> 8, priv);
                break;
        }
}
void mach64_ext_outl(uint16_t port, uint32_t val, void *priv)
{
        pclog("mach64_ext_outl : port %04X val %08X\n", port, val);
        switch (port)
        {
                default:
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_outw(port, val, priv);
#ifdef MACH64_DEBUG
                pclog("  ");
#endif
                mach64_ext_outw(port + 2, val >> 16, priv);
                break;
        }
}

void mach64_write(uint32_t addr, uint8_t val, void *priv)
{
//        pclog("mach64_write : %05X %02X  ", addr, val);
        addr = (addr & 0x7fff) + mach64_bank_w[(addr >> 15) & 1];
//        pclog("%08X\n", addr);
        svga_write_linear(addr, val, priv);
}

uint8_t mach64_read(uint32_t addr, void *priv)
{
        uint8_t ret;
//        pclog("mach64_read : %05X ", addr);
        addr = (addr & 0x7fff) + mach64_bank_r[(addr >> 15) & 1];
        ret = svga_read_linear(addr, priv);
//        pclog("%08X %02X\n", addr, ret);  
        return ret;      
}

void mach64_hwcursor_draw(int displine)
{
        int x, offset;
        uint8_t dat;
        offset = svga_hwcursor_latch.xoff;
        for (x = 0; x < 64 - svga_hwcursor_latch.xoff; x += 4)
        {
                dat = vram[svga_hwcursor_latch.addr + (offset >> 2)];
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 32]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 32] ^= 0xFFFFFF;
                dat >>= 2;
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 33]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 33] ^= 0xFFFFFF;
                dat >>= 2;
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 34]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 34] ^= 0xFFFFFF;
                dat >>= 2;
                if (!(dat & 2))          ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 35]  = (dat & 1) ? 0xFFFFFF : 0;
                else if ((dat & 3) == 3) ((uint32_t *)buffer32->line[displine])[svga_hwcursor_latch.x + x + 35] ^= 0xFFFFFF;
                dat >>= 2;
                offset += 4;
        }
        svga_hwcursor_latch.addr += 16;
}

int mach64_init()
{
        int c;
        
        svga_recalctimings_ex = mach64_recalctimings;
        svga_hwcursor_draw    = mach64_hwcursor_draw;
                
        svga_vram_limit       = 1 << 22; /*4mb*/
        vrammask = 0x3fffff;
        
        for (c = 0; c < 8; c++)
        {
                io_sethandler((c * 0x1000) + 0x2ec, 0x0004, mach64_ext_inb, mach64_ext_inw, mach64_ext_inl, mach64_ext_outb, mach64_ext_outw, mach64_ext_outl, NULL);
                io_sethandler((c * 0x1000) + 0x6ec, 0x0004, mach64_ext_inb, mach64_ext_inw, mach64_ext_inl, mach64_ext_outb, mach64_ext_outw, mach64_ext_outl, NULL);
                io_sethandler((c * 0x1000) + 0xaec, 0x0004, mach64_ext_inb, mach64_ext_inw, mach64_ext_inl, mach64_ext_outb, mach64_ext_outw, mach64_ext_outl, NULL);
                io_sethandler((c * 0x1000) + 0xeec, 0x0004, mach64_ext_inb, mach64_ext_inw, mach64_ext_inl, mach64_ext_outb, mach64_ext_outw, mach64_ext_outl, NULL);                                                
        }

        io_sethandler(0x01ce, 0x0002, mach64_in, NULL, NULL, mach64_out, NULL, NULL, NULL);
        
        ati_eeprom_load("mach64.nvr", 1);

        mach64.dac_cntl = 5 << 16; /*ATI 68860 RAMDAC*/        
        
        mach64.dst_cntl = 3;
        return svga_init();
}

GFXCARD vid_mach64 =
{
        mach64_init,
        /*IO at 3Cx/3Dx*/
        mach64_out,
        mach64_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        svga_poll,
        svga_recalctimings,

        svga_write,
        video_write_null,
        video_write_null,

        svga_read,
        video_read_null,
        video_read_null
};
