/*Trident TVGA (8900D) emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_tkd8001_ramdac.h"
#include "vid_tvga.h"

typedef struct tvga_t
{
        svga_t svga;
        tkd8001_ramdac_t ramdac;

        struct
        {
        	uint16_t src_x, src_y;
        	uint16_t dst_x, dst_y;
        	uint16_t size_x, size_y;
        	uint16_t fg_col, bg_col;
        	uint8_t rop;
        	uint16_t flags;
        	uint8_t pattern[0x80];
        	int command;
        	int offset;
        	uint8_t ger22;
	
        	int x, y;
        	uint32_t src, dst, src_old, dst_old;
        	int pat_x, pat_y;
        	int use_src;
	
        	int pitch, bpp;

                uint16_t tvga_pattern[8][8];
        } accel;

        uint8_t tvga_3d8, tvga_3d9;
        int oldmode;
        uint8_t oldctrl2,newctrl2;

        uint32_t linear_base, linear_size;
} tvga_t;

void tvga_recalcmapping(tvga_t *tvga);


uint8_t tvga_accel_read(uint32_t addr, void *priv);
uint16_t tvga_accel_read_w(uint32_t addr, void *priv);
uint32_t tvga_accel_read_l(uint32_t addr, void *priv);

void tvga_accel_write(uint32_t addr, uint8_t val, void *priv);
void tvga_accel_write_w(uint32_t addr, uint16_t val, void *priv);
void tvga_accel_write_l(uint32_t addr, uint32_t val, void *priv);


void tvga_accel_write_fb_l(uint32_t addr, uint32_t val, void *priv);

void tvga_out(uint16_t addr, uint8_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
        svga_t *svga = &tvga->svga;

        uint8_t old;

//	pclog("tvga_out : %04X %02X  %04X:%04X  %i\n", addr, val, CS,pc, bpp);
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x3C5:
                switch (svga->seqaddr & 0xf)
                {
                        case 0xB: 
                        tvga->oldmode=1; 
                        break;
                        case 0xC: 
                        if (svga->seqregs[0xe] & 0x80) 
                        svga->seqregs[0xc] = val; 
                        break;
                        case 0xd: 
                        if (tvga->oldmode) 
                                tvga->oldctrl2 = val; 
                        else 
                                tvga->newctrl2=val; 
                        break;
                        case 0xE:
                        svga->seqregs[0xe] = val ^ 2;
                        svga->write_bank = (svga->seqregs[0xe] & 0xf) * 65536;
                        if (!(svga->gdcreg[0xf] & 1)) 
                                svga->read_bank = svga->write_bank;
                        return;
                }
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
		if (gfxcard != GFX_TGUI9440)
		{
                	tkd8001_ramdac_out(addr, val, &tvga->ramdac, svga);
                	return;
		}
		break;

                case 0x3CF:
                switch (svga->gdcaddr & 15)
                {
			case 0x6:
			if (svga->gdcreg[6] != val)
			{
				svga->gdcreg[6] = val;
				tvga_recalcmapping(tvga);
			}
			return;
			
                        case 0xE:
                        svga->gdcreg[0xe] = val ^ 2;
                        if ((svga->gdcreg[0xf] & 1) == 1)
                           svga->read_bank = (svga->gdcreg[0xe] & 0xf) * 65536;
                        break;
                        case 0xF:
                        if (val & 1) svga->read_bank = (svga->gdcreg[0xe]  & 0xf)  *65536;
                        else         svga->read_bank = (svga->seqregs[0xe] & 0xf)  *65536;
                        svga->write_bank = (svga->seqregs[0xe] & 0xf) * 65536;
                        break;
                }
                break;
                case 0x3D4:
		if (gfxcard == GFX_TGUI9440)	svga->crtcreg = val & 0x7f;
		else				svga->crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if (svga->crtcreg <= 7 && svga->crtc[0x11] & 0x80) return;
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                //if (crtcreg!=0xE && crtcreg!=0xF) pclog("CRTC R%02X = %02X\n",crtcreg,val);
                if (old != val)
                {
                        if (svga->crtcreg < 0xE || svga->crtcreg > 0x10)
                        {
                                fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                switch (svga->crtcreg)
                {
			case 0x21:
			if (old != val)
				tvga_recalcmapping(tvga);
			break;


			case 0x38: /*Pixel bus register*/
//			pclog("Pixel bus register %02X\n", val);
			if (gfxcard != GFX_TGUI9440)
				break;
			
			if ((val & 0xc) == 4)	svga->bpp = 16;
			else if (!(val & 0xc))	svga->bpp = 8;
			else			svga->bpp = 24;
			break;

			case 0x40: case 0x41: case 0x42: case 0x43:
			case 0x44: case 0x45: case 0x46: case 0x47:
			svga->hwcursor.x = (svga->crtc[0x40] | (svga->crtc[0x41] << 8)) & 0x7ff;
			svga->hwcursor.y = (svga->crtc[0x42] | (svga->crtc[0x43] << 8)) & 0x7ff;
			svga->hwcursor.xoff = svga->crtc[0x46] & 0x3f;
			svga->hwcursor.yoff = svga->crtc[0x47] & 0x3f;
			svga->hwcursor.addr = (svga->crtc[0x44] << 10) | ((svga->crtc[0x45] & 0x7) << 18) | (svga->hwcursor.yoff * 8);
			break;
			
			case 0x50:
			svga->hwcursor.ena = val & 0x80;
			break;
		}
                return;
                case 0x3D8:
                tvga->tvga_3d8 = val;
                if (svga->gdcreg[0xf] & 4)
                {
                        svga->write_bank = (val & 0x1f) * 65536;
//                                pclog("SVGAWBANK 3D8 %08X %04X:%04X\n",svgawbank,CS,pc);
                        if (!(svga->gdcreg[0xf] & 1))
                        {
                                svga->read_bank = (val & 0x1f) * 65536;
//                                        pclog("SVGARBANK 3D8 %08X %04X:%04X\n",svgarbank,CS,pc);
                        }
                }
                return;
                case 0x3D9:
                tvga->tvga_3d9=val;
                if ((svga->gdcreg[0xf] & 5) == 5)
                {
                        svga->read_bank = (val & 0x1F) * 65536;
//                                pclog("SVGARBANK 3D9 %08X %04X:%04X\n",svgarbank,CS,pc);
                }
                return;
        }
        svga_out(addr, val, svga);
}

uint8_t tvga_in(uint16_t addr, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
        svga_t *svga = &tvga->svga;

//        if (addr != 0x3da) pclog("tvga_in : %04X  %04X:%04X\n", addr, CS,pc);
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3C5:
                if ((svga->seqaddr & 0xf) == 0xb)
                {
//                        printf("Read Trident ID %04X:%04X %04X\n",CS,pc,readmemw(ss,SP));
                        tvga->oldmode = 0;
                        if (gfxcard == GFX_TVGA) return 0x33; /*TVGA8900D*/
                        else                     return 0xe3; /*TGUI9440AGi*/
                }
                if ((svga->seqaddr & 0xf) == 0xc)
                {
//                        printf("Read Trident Power Up 1 %04X:%04X %04X\n",CS,pc,readmemw(ss,SP));
//                        return 0x20; /*2 DRAM banks*/
                }
                if ((svga->seqaddr & 0xf) == 0xd)
                {
                        if (tvga->oldmode) return tvga->oldctrl2;
                        return tvga->newctrl2;
                }
                break;
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
		if (gfxcard != GFX_TGUI9440)
                	return tkd8001_ramdac_in(addr, &tvga->ramdac, svga);
                break;
                case 0x3D4:
                return svga->crtcreg;
                case 0x3D5:
                return svga->crtc[svga->crtcreg];
                case 0x3d8:
		return tvga->tvga_3d8;
		case 0x3d9:
		return tvga->tvga_3d9;
        }
        return svga_in(addr, svga);
}

void tvga_recalctimings(svga_t *svga)
{
        tvga_t *tvga = (tvga_t *)svga->p;
        if (!svga->rowoffset) svga->rowoffset = 0x100; /*This is the only sensible way I can see this being handled,
                                                         given that TVGA8900D has no overflow bits.
                                                         Some sort of overflow is required for 320x200x24 and 1024x768x16*/

        if ((svga->crtc[0x1e] & 0xA0) == 0xA0) svga->ma_latch |= 0x10000;
        if ((svga->crtc[0x27] & 0x01) == 0x01) svga->ma_latch |= 0x20000;
        if ((svga->crtc[0x27] & 0x02) == 0x02) svga->ma_latch |= 0x40000;
        
        if (tvga->oldctrl2 & 0x10)
        {
                svga->rowoffset <<= 1;
                svga->ma_latch <<= 1;
        }
        if (tvga->oldctrl2 & 0x10) /*I'm not convinced this is the right register for this function*/
           svga->lowres=0;
        if (gfxcard == GFX_TGUI9440)
	   svga->lowres = !(svga->crtc[0x2a] & 0x40); 
	   
        if (svga->gdcreg[0xf] & 8)
        {
                svga->htotal <<= 1;
                svga->hdisp <<= 1;
        }
        svga->interlace = svga->crtc[0x1e] & 4;
        if (svga->interlace)
           svga->rowoffset >>= 1;
        
        switch (((svga->miscout >> 2) & 3) | ((tvga->newctrl2 << 2) & 4))
        {
                case 2: svga->clock = cpuclock/44900000.0; break;
                case 3: svga->clock = cpuclock/36000000.0; break;
                case 4: svga->clock = cpuclock/57272000.0; break;
                case 5: svga->clock = cpuclock/65000000.0; break;
                case 6: svga->clock = cpuclock/50350000.0; break;
                case 7: svga->clock = cpuclock/40000000.0; break;
        }

        if ((tvga->oldctrl2 & 0x10) || (gfxcard == GFX_TGUI9440 && (svga->crtc[0x2a] & 0x40)))
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
}

void tvga_recalcmapping(tvga_t *tvga)
{
        svga_t *svga = &tvga->svga;
        
	pclog("tvga_recalcmapping : %02X %02X\n", svga->crtc[0x21], svga->gdcreg[6]);
        mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    svga);
	mem_removehandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    svga);
	mem_removehandler(0xbc000, 0x4000, tvga_accel_read, NULL, NULL, tvga_accel_write, NULL, NULL,    tvga);

	if (svga->crtc[0x21] & 0x20)
	{
		tvga->linear_base = ((svga->crtc[0x21] & 0xf) | ((svga->crtc[0x21] >> 2) & 0x30)) << 20;
		tvga->linear_size = (svga->crtc[0x21] & 0x10) ? 0x200000 : 0x100000;
		mem_sethandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    svga);
		pclog("Trident linear framebuffer at %08X - size %06X\n", tvga->linear_base, tvga->linear_size);
		mem_sethandler(0xbc000, 0x4000, tvga_accel_read, tvga_accel_read_w, tvga_accel_read_l, tvga_accel_write, tvga_accel_write_w, tvga_accel_write_l,    tvga);
	}
	else
	{
//                                pclog("Write mapping %02X\n", val);
                switch (svga->gdcreg[6] & 0xC)
                {
                        case 0x0: /*128k at A0000*/
                        mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    svga);
                        break;
                        case 0x4: /*64k at A0000*/
                        mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    svga);
                        mem_sethandler(0xbc000, 0x4000, tvga_accel_read, NULL, NULL, tvga_accel_write, NULL, NULL,    tvga);
                        break;
                        case 0x8: /*32k at B0000*/
                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    svga);
                        break;
                        case 0xC: /*32k at B8000*/
                	mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    svga);
                	break;
		}
        }
}

void tvga_hwcursor_draw(svga_t *svga, int displine)
{
        uint32_t dat[2];
        int xx;
        int offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
        
        dat[0] = (svga->vram[svga->hwcursor_latch.addr]     << 24) | (svga->vram[svga->hwcursor_latch.addr + 1] << 16) | (svga->vram[svga->hwcursor_latch.addr + 2] << 8) | svga->vram[svga->hwcursor_latch.addr + 3];
        dat[1] = (svga->vram[svga->hwcursor_latch.addr + 4] << 24) | (svga->vram[svga->hwcursor_latch.addr + 5] << 16) | (svga->vram[svga->hwcursor_latch.addr + 6] << 8) | svga->vram[svga->hwcursor_latch.addr + 7];
        for (xx = 0; xx < 32; xx++)
        {
                if (offset >= svga->hwcursor_latch.x)
                {
                        if (!(dat[0] & 0x80000000))
                                ((uint32_t *)buffer32->line[displine])[offset + 32]  = (dat[1] & 0x80000000) ? 0xffffff : 0;
                        else if (dat[1] & 0x80000000)
                                ((uint32_t *)buffer32->line[displine])[offset + 32] ^= 0xffffff;
//                        pclog("Plot %i, %i (%i %i) %04X %04X\n", offset, displine, x+xx, svga_hwcursor_on, dat[0], dat[1]);
                }
                           
                offset++;
                dat[0] <<= 1;
                dat[1] <<= 1;
        }
        svga->hwcursor_latch.addr += 8;
}

void *tvga8900d_init()
{
        tvga_t *tvga = malloc(sizeof(tvga_t));
        memset(tvga, 0, sizeof(tvga_t));
        
        svga_init(&tvga->svga, tvga, 1 << 20, /*1mb - chip supports 2mb, but drivers are buggy*/
                   tvga_recalctimings,
                   tvga_in, tvga_out,
                   NULL);

        io_sethandler(0x03c0, 0x0020, tvga_in, NULL, NULL, tvga_out, NULL, NULL, tvga);

        return tvga;
}
void *tgui9440_init()
{
        tvga_t *tvga = malloc(sizeof(tvga_t));
        memset(tvga, 0, sizeof(tvga_t));
        
        svga_init(&tvga->svga, tvga, 1 << 21, /*2mb*/
                   tvga_recalctimings,
                   tvga_in, tvga_out,
                   tvga_hwcursor_draw);

        io_sethandler(0x03c0, 0x0020, tvga_in, NULL, NULL, tvga_out, NULL, NULL, tvga);

        return tvga;
}

void tvga_close(void *p)
{
        tvga_t *tvga = (tvga_t *)p;

        svga_close(&tvga->svga);
        
        free(tvga);
}

void tvga_speed_changed(void *p)
{
        tvga_t *tvga = (tvga_t *)p;
        
        svga_recalctimings(&tvga->svga);
}

device_t tvga8900d_device =
{
        "Trident TVGA 8900D",
        tvga8900d_init,
        tvga_close,
        tvga_speed_changed,
        svga_add_status_info
};
device_t tgui9440_device =
{
        "Trident TGUI 9440",
        tgui9440_init,
        tvga_close,
        tvga_speed_changed,
        svga_add_status_info
};

enum
{
	TGUI_BITBLT = 1
};

enum
{
        TGUI_SRCCPU = 0,
        
	TGUI_SRCDISP = 0x04,	/*Source is from display*/
	TGUI_PATMONO = 0x20,	/*Pattern is monochrome and needs expansion*/
	TGUI_SRCMONO = 0x40,	/*Source is monochrome from CPU and needs expansion*/
	TGUI_TRANSENA  = 0x1000, /*Transparent (no draw when source == bg col)*/
	TGUI_TRANSREV  = 0x2000, /*Reverse fg/bg for transparent*/
	TGUI_SOLIDFILL = 0x4000	/*Pattern all zero?*/
};

#define READ(addr, dat) if (tvga->accel.bpp == 0) dat = svga->vram[addr & 0x1fffff]; \
                        else                     dat = vram_w[addr & 0xfffff];
                        
#define MIX() do \
	{								\
		out = 0;						\
        	for (c=0;c<16;c++)					\
	        {							\
			d=(dst_dat & (1<<c)) ? 1:0;			\
                       	if (src_dat & (1<<c)) d|=2;			\
               	        if (pat_dat & (1<<c)) d|=4;			\
                        if (tvga->accel.rop & (1<<d)) out|=(1<<c);	\
	        }							\
	} while (0)

#define WRITE(addr, dat)        if (tvga->accel.bpp == 0)                                                \
                                {                                                                       \
                                        svga->vram[addr & 0x1fffff] = dat;                                    \
                                        svga->changedvram[((addr) & 0x1fffff) >> 10] = changeframecount;      \
                                }                                                                       \
                                else                                                                    \
                                {                                                                       \
                                        vram_w[addr & 0xfffff] = dat;                                   \
                                        svga->changedvram[((addr) & 0xfffff) >> 9] = changeframecount;        \
                                }
                                
void tvga_accel_write_fb_b(uint32_t addr, uint8_t val, void *priv);
void tvga_accel_write_fb_w(uint32_t addr, uint16_t val, void *priv);

void tvga_accel_command(int count, uint32_t cpu_dat, tvga_t *tvga)
{
        svga_t *svga = &tvga->svga;
	int x, y;
	int c, d;
	uint16_t src_dat, dst_dat, pat_dat;
	uint16_t out;
	int xdir = (tvga->accel.flags & 0x200) ? -1 : 1;
	int ydir = (tvga->accel.flags & 0x100) ? -1 : 1;
	uint16_t trans_col = (tvga->accel.flags & TGUI_TRANSREV) ? tvga->accel.fg_col : tvga->accel.bg_col;
        uint16_t *vram_w = (uint16_t *)svga->vram;
        
	if (tvga->accel.bpp == 0)
                trans_col &= 0xff;
	
	if (count != -1 && !tvga->accel.x)
	{
		count -= tvga->accel.offset;
		cpu_dat <<= tvga->accel.offset;
	}
	if (count == -1)
	{
		tvga->accel.x = tvga->accel.y = 0;
	}
	if (tvga->accel.flags & TGUI_SOLIDFILL)
	{
//		pclog("SOLIDFILL\n");
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				tvga->accel.tvga_pattern[y][x] = tvga->accel.fg_col;
			}
		}
	}
	else if (tvga->accel.flags & TGUI_PATMONO)
	{
//		pclog("PATMONO\n");
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				tvga->accel.tvga_pattern[y][x] = (tvga->accel.pattern[y] & (1 << x)) ? tvga->accel.fg_col : tvga->accel.bg_col;
			}
		}
	}
	else
	{
//		pclog("OTHER\n");
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				tvga->accel.tvga_pattern[y][x] = tvga->accel.pattern[x + y*8];
			}
		}
	}
	for (y = 0; y < 8; y++)
	{
		if (count == -1) pclog("Pattern %i : %02X %02X %02X %02X %02X %02X %02X %02X\n", y, tvga->accel.tvga_pattern[y][0], tvga->accel.tvga_pattern[y][1], tvga->accel.tvga_pattern[y][2], tvga->accel.tvga_pattern[y][3], tvga->accel.tvga_pattern[y][4], tvga->accel.tvga_pattern[y][5], tvga->accel.tvga_pattern[y][6], tvga->accel.tvga_pattern[y][7]);
	}
	if (count == -1) pclog("Command %i %i\n", tvga->accel.command, TGUI_BITBLT);
	switch (tvga->accel.command)
	{
		case TGUI_BITBLT:
		if (count == -1) pclog("BITBLT src %i,%i dst %i,%i size %i,%i flags %04X\n", tvga->accel.src_x, tvga->accel.src_y, tvga->accel.dst_x, tvga->accel.dst_y, tvga->accel.size_x, tvga->accel.size_y, tvga->accel.flags);
		if (count == -1)
		{
			tvga->accel.src = tvga->accel.src_old = tvga->accel.src_x + (tvga->accel.src_y * tvga->accel.pitch);
			tvga->accel.dst = tvga->accel.dst_old = tvga->accel.dst_x + (tvga->accel.dst_y * tvga->accel.pitch);
			tvga->accel.pat_x = tvga->accel.dst_x;
			tvga->accel.pat_y = tvga->accel.dst_y;
		}

		switch (tvga->accel.flags & (TGUI_SRCMONO|TGUI_SRCDISP))
		{
			case TGUI_SRCCPU:
			if (count == -1)
			{
//				pclog("Blit start\n");
				if (svga->crtc[0x21] & 0x20)
				{
					mem_removehandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
					mem_sethandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
				}
				if (tvga->accel.use_src)
                                        return;
			}
			else
			     count >>= 3;
			while (count)
			{
				if (tvga->accel.bpp == 0)
				{
                                        src_dat = cpu_dat >> 24;
                                        cpu_dat <<= 8;
                                }
                                else
                                {
                                        src_dat = (cpu_dat >> 24) | ((cpu_dat >> 8) & 0xff00);
                                        cpu_dat <<= 16;
                                        count--;
                                }
				READ(tvga->accel.dst, dst_dat);
				pat_dat = tvga->accel.tvga_pattern[tvga->accel.pat_y & 7][tvga->accel.pat_x & 7];
	
                                if (!(tvga->accel.flags & TGUI_TRANSENA) || src_dat != trans_col)
                                {
        				MIX();
	
                                        WRITE(tvga->accel.dst, out);
                                }

//				pclog("  %i,%i  %02X %02X %02X  %02X\n", tvga->accel.x, tvga->accel.y, src_dat,dst_dat,pat_dat, out);
                                	
				tvga->accel.src += xdir;
				tvga->accel.dst += xdir;
				tvga->accel.pat_x += xdir;
	
				tvga->accel.x++;
				if (tvga->accel.x > tvga->accel.size_x)
				{
					tvga->accel.x = 0;
					tvga->accel.y++;
					
					tvga->accel.pat_x = tvga->accel.dst_x;
	
					tvga->accel.src = tvga->accel.src_old = tvga->accel.src_old + (ydir * tvga->accel.pitch);
					tvga->accel.dst = tvga->accel.dst_old = tvga->accel.dst_old + (ydir * tvga->accel.pitch);
					tvga->accel.pat_y += ydir;
					
					if (tvga->accel.y > tvga->accel.size_y)
					{
						if (svga->crtc[0x21] & 0x20)
						{
//							pclog("Blit end\n");
							mem_removehandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
							mem_sethandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
						}
						return;
					}
					if (tvga->accel.use_src)
                                                return;
				}
				count--;
			}
			break;
						
			case TGUI_SRCMONO | TGUI_SRCCPU:
			if (count == -1)
			{
//				pclog("Blit start\n");
				if (svga->crtc[0x21] & 0x20)
				{
					mem_removehandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
					mem_sethandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
				}
				if (tvga->accel.use_src)
                                        return;
			}
//			pclog("TGUI_SRCMONO | TGUI_SRCCPU\n");
			while (count)
			{
				src_dat = ((cpu_dat >> 31) ? tvga->accel.fg_col : tvga->accel.bg_col);
				if (tvga->accel.bpp == 0)
				    src_dat &= 0xff;
				    
				READ(tvga->accel.dst, dst_dat);
				pat_dat = tvga->accel.tvga_pattern[tvga->accel.pat_y & 7][tvga->accel.pat_x & 7];

                                if (!(tvga->accel.flags & TGUI_TRANSENA) || src_dat != trans_col)
                                {
        				MIX();

				        WRITE(tvga->accel.dst, out);
                                }
//				pclog("  %i,%i  %02X %02X %02X  %02X %i\n", tvga->accel.x, tvga->accel.y, src_dat,dst_dat,pat_dat, out, (!(tvga->accel.flags & TGUI_TRANSENA) || src_dat != trans_col));
				cpu_dat <<= 1;
				tvga->accel.src += xdir;
				tvga->accel.dst += xdir;
				tvga->accel.pat_x += xdir;
	
				tvga->accel.x++;
				if (tvga->accel.x > tvga->accel.size_x)
				{
					tvga->accel.x = 0;
					tvga->accel.y++;
					
					tvga->accel.pat_x = tvga->accel.dst_x;
	
					tvga->accel.src = tvga->accel.src_old = tvga->accel.src_old + (ydir * tvga->accel.pitch);
					tvga->accel.dst = tvga->accel.dst_old = tvga->accel.dst_old + (ydir * tvga->accel.pitch);
					tvga->accel.pat_y += ydir;
					
					if (tvga->accel.y > tvga->accel.size_y)
					{
						if (svga->crtc[0x21] & 0x20)
						{
//							pclog("Blit end\n");
							mem_removehandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
							mem_sethandler(tvga->linear_base, tvga->linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
						}
						return;
					}
					if (tvga->accel.use_src)
                                                return;
				}
				count--;
			}
			break;

			default:
			while (count)
			{
				READ(tvga->accel.src, src_dat);
				READ(tvga->accel.dst, dst_dat);                                                                
				pat_dat = tvga->accel.tvga_pattern[tvga->accel.pat_y & 7][tvga->accel.pat_x & 7];
	
                                if (!(tvga->accel.flags & TGUI_TRANSENA) || src_dat != trans_col)
                                {
        				MIX();
	
                                        WRITE(tvga->accel.dst, out);
                                }
//                                pclog("  %i,%i  %02X %02X %02X  %02X\n", tvga->accel.x, tvga->accel.y, src_dat,dst_dat,pat_dat, out);
	
				tvga->accel.src += xdir;
				tvga->accel.dst += xdir;
				tvga->accel.pat_x += xdir;
	
				tvga->accel.x++;
				if (tvga->accel.x > tvga->accel.size_x)
				{
					tvga->accel.x = 0;
					tvga->accel.y++;
					
					tvga->accel.pat_x = tvga->accel.dst_x;
	
					tvga->accel.src = tvga->accel.src_old = tvga->accel.src_old + (ydir * tvga->accel.pitch);
					tvga->accel.dst = tvga->accel.dst_old = tvga->accel.dst_old + (ydir * tvga->accel.pitch);
					tvga->accel.pat_y += ydir;
					
					if (tvga->accel.y > tvga->accel.size_y)
						return;
				}
				count--;
			}
			break;
		}
		break;
	}
}

void tvga_accel_write(uint32_t addr, uint8_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
	pclog("tvga_accel_write : %08X %02X  %04X(%08X):%08X %02X\n", addr, val, CS,cs,pc, opcode);
	if ((addr & ~0xff) != 0xbff00)
		return;
	switch (addr & 0xff)
	{
                case 0x22:
                tvga->accel.ger22 = val;
                tvga->accel.pitch = 512 << ((val >> 2) & 3);
                tvga->accel.bpp = (val & 3) ? 1 : 0;
                tvga->accel.pitch >>= tvga->accel.bpp;
                break;
                
		case 0x24: /*Command*/
		tvga->accel.command = val;
		tvga_accel_command(-1, 0, tvga);
		break;
		
		case 0x27: /*ROP*/
		tvga->accel.rop = val;
		tvga->accel.use_src = (val & 0x33) ^ ((val >> 2) & 0x33);
		pclog("Write ROP %02X %i\n", val, tvga->accel.use_src);
		break;
		
		case 0x28: /*Flags*/
		tvga->accel.flags = (tvga->accel.flags & 0xff00) | val;
		break;
		case 0x29: /*Flags*/
		tvga->accel.flags = (tvga->accel.flags & 0xff) | (val << 8);
		break;

		case 0x2b:
		tvga->accel.offset = val & 7;
		break;
		
		case 0x2c: /*Foreground colour*/
		tvga->accel.fg_col = (tvga->accel.fg_col & 0xff00) | val;
		break;
		case 0x2d: /*Foreground colour*/
		tvga->accel.fg_col = (tvga->accel.fg_col & 0xff) | (val << 8);
		break;

		case 0x30: /*Background colour*/
		tvga->accel.bg_col = (tvga->accel.bg_col & 0xff00) | val;
		break;
		case 0x31: /*Background colour*/
		tvga->accel.bg_col = (tvga->accel.bg_col & 0xff) | (val << 8);
		break;

		case 0x38: /*Dest X*/
		tvga->accel.dst_x = (tvga->accel.dst_x & 0xff00) | val;
		break;
		case 0x39: /*Dest X*/
		tvga->accel.dst_x = (tvga->accel.dst_x & 0xff) | (val << 8);
		break;
		case 0x3a: /*Dest Y*/
		tvga->accel.dst_y = (tvga->accel.dst_y & 0xff00) | val;
		break;
		case 0x3b: /*Dest Y*/
		tvga->accel.dst_y = (tvga->accel.dst_y & 0xff) | (val << 8);
		break;

		case 0x3c: /*Src X*/
		tvga->accel.src_x = (tvga->accel.src_x & 0xff00) | val;
		break;
		case 0x3d: /*Src X*/
		tvga->accel.src_x = (tvga->accel.src_x & 0xff) | (val << 8);
		break;
		case 0x3e: /*Src Y*/
		tvga->accel.src_y = (tvga->accel.src_y & 0xff00) | val;
		break;
		case 0x3f: /*Src Y*/
		tvga->accel.src_y = (tvga->accel.src_y & 0xff) | (val << 8);
		break;

		case 0x40: /*Size X*/
		tvga->accel.size_x = (tvga->accel.size_x & 0xff00) | val;
		break;
		case 0x41: /*Size X*/
		tvga->accel.size_x = (tvga->accel.size_x & 0xff) | (val << 8);
		break;
		case 0x42: /*Size Y*/
		tvga->accel.size_y = (tvga->accel.size_y & 0xff00) | val;
		break;
		case 0x43: /*Size Y*/
		tvga->accel.size_y = (tvga->accel.size_y & 0xff) | (val << 8);
		break;
		
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xac: case 0xad: case 0xae: case 0xaf:
		case 0xb0: case 0xb1: case 0xb2: case 0xb3:
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb:
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb:
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
		tvga->accel.pattern[addr & 0x7f] = val;
		break;
	}
}

void tvga_accel_write_w(uint32_t addr, uint16_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
	pclog("tvga_accel_write_w %08X %04X\n", addr, val);
	tvga_accel_write(addr, val, tvga);
	tvga_accel_write(addr + 1, val >> 8, tvga);
}

void tvga_accel_write_l(uint32_t addr, uint32_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
	pclog("tvga_accel_write_l %08X %08X\n", addr, val);
	tvga_accel_write(addr, val, tvga);
	tvga_accel_write(addr + 1, val >> 8, tvga);
	tvga_accel_write(addr + 2, val >> 16, tvga);
	tvga_accel_write(addr + 3, val >> 24, tvga);
}

uint8_t tvga_accel_read(uint32_t addr, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
//	pclog("tvga_accel_read : %08X\n", addr);
	if ((addr & ~0xff) != 0xbff00)
		return 0xff;
	switch (addr & 0xff)
	{
		case 0x20: /*Status*/
		return 0;
		
		case 0x27: /*ROP*/
		return tvga->accel.rop;
		
		case 0x28: /*Flags*/
		return tvga->accel.flags & 0xff;
		case 0x29: /*Flags*/
		return tvga->accel.flags >> 8;

		case 0x2b:
		return tvga->accel.offset;
		
		case 0x2c: /*Background colour*/
		return tvga->accel.bg_col & 0xff;
		case 0x2d: /*Background colour*/
		return tvga->accel.bg_col >> 8;

		case 0x30: /*Foreground colour*/
		return tvga->accel.fg_col & 0xff;
		case 0x31: /*Foreground colour*/
		return tvga->accel.fg_col >> 8;

		case 0x38: /*Dest X*/
		return tvga->accel.dst_x & 0xff;
		case 0x39: /*Dest X*/
		return tvga->accel.dst_x >> 8;
		case 0x3a: /*Dest Y*/
		return tvga->accel.dst_y & 0xff;
		case 0x3b: /*Dest Y*/
		return tvga->accel.dst_y >> 8;

		case 0x3c: /*Src X*/
		return tvga->accel.src_x & 0xff;
		case 0x3d: /*Src X*/
		return tvga->accel.src_x >> 8;
		case 0x3e: /*Src Y*/
		return tvga->accel.src_y & 0xff;
		case 0x3f: /*Src Y*/
		return tvga->accel.src_y >> 8;

		case 0x40: /*Size X*/
		return tvga->accel.size_x & 0xff;
		case 0x41: /*Size X*/
		return tvga->accel.size_x >> 8;
		case 0x42: /*Size Y*/
		return tvga->accel.size_y & 0xff;
		case 0x43: /*Size Y*/
		return tvga->accel.size_y >> 8;
		
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xac: case 0xad: case 0xae: case 0xaf:
		case 0xb0: case 0xb1: case 0xb2: case 0xb3:
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb:
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb:
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
		return tvga->accel.pattern[addr & 0x7f];
	}
	return 0xff;
}

uint16_t tvga_accel_read_w(uint32_t addr, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
	pclog("tvga_accel_read_w %08X\n", addr);
	return tvga_accel_read(addr, tvga) | (tvga_accel_read(addr + 1, tvga) << 8);
}

uint32_t tvga_accel_read_l(uint32_t addr, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
	pclog("tvga_accel_read_l %08X\n", addr);
	return tvga_accel_read_w(addr, tvga) | (tvga_accel_read_w(addr + 2, tvga) << 16);
}

void tvga_accel_write_fb_b(uint32_t addr, uint8_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
//	pclog("tvga_accel_write_fb_b %08X %02X\n", addr, val);
	tvga_accel_command(8, val << 24, tvga);
}

void tvga_accel_write_fb_w(uint32_t addr, uint16_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
//	pclog("tvga_accel_write_fb_w %08X %04X\n", addr, val);
	tvga_accel_command(16, (((val & 0xff00) >> 8) | ((val & 0x00ff) << 8)) << 16, tvga);
}

void tvga_accel_write_fb_l(uint32_t addr, uint32_t val, void *p)
{
        tvga_t *tvga = (tvga_t *)p;
//	pclog("tvga_accel_write_fb_l %08X %08X\n", addr, val);
	tvga_accel_command(32, ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24), tvga);
}
