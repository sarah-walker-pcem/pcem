/*Trident TVGA (8900D) emulation*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_tkd8001_ramdac.h"

void tvga_recalcmapping();

uint8_t trident3d8,trident3d9;
int tridentoldmode;
uint8_t tridentoldctrl2,tridentnewctrl2;
uint8_t tridentdac;

uint32_t tvga_linear_base, tvga_linear_size;

uint8_t tvga_accel_read(uint32_t addr, void *priv);
uint16_t tvga_accel_read_w(uint32_t addr, void *priv);
uint32_t tvga_accel_read_l(uint32_t addr, void *priv);

void tvga_accel_write(uint32_t addr, uint8_t val, void *priv);
void tvga_accel_write_w(uint32_t addr, uint16_t val, void *priv);
void tvga_accel_write_l(uint32_t addr, uint32_t val, void *priv);


void tvga_accel_write_fb_l(uint32_t addr, uint32_t val, void *priv);

void tvga_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;

//	pclog("tvga_out : %04X %02X  %04X:%04X  %i\n", addr, val, CS,pc, bpp);
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x3C5:
                switch (seqaddr&0xF)
                {
                        case 0xB: tridentoldmode=1; break;
                        case 0xC: if (seqregs[0xE]&0x80) seqregs[0xC]=val; break;
                        case 0xD: if (tridentoldmode) { tridentoldctrl2=val; rowdbl=val&0x10; } else tridentnewctrl2=val; break;
                        case 0xE:
                        seqregs[0xE]=val^2;
                        svgawbank=(seqregs[0xE]&0xF)*65536;
                        if (!(gdcreg[0xF]&1)) svgarbank=svgawbank;
                        return;
                }
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
		if (gfxcard != GFX_TGUI9440)
		{
                	tkd8001_ramdac_out(addr, val, NULL);
                	return;
		}
		break;

                case 0x3CF:
                switch (gdcaddr&15)
                {
			case 0x6:
			if (gdcreg[6] != val)
			{
				gdcreg[6] = val;
				tvga_recalcmapping();
			}
			return;
			
                        case 0xE:
                        gdcreg[0xE]=val^2;
                        if ((gdcreg[0xF]&1)==1)
                           svgarbank=(gdcreg[0xE]&0xF)*65536;
                        break;
                        case 0xF:
                        if (val&1) svgarbank=(gdcreg[0xE] &0xF)*65536;
                        else       svgarbank=(seqregs[0xE]&0xF)*65536;
                        svgawbank=(seqregs[0xE]&0xF)*65536;
                        break;
                }
                break;
                case 0x3D4:
		if (gfxcard == GFX_TGUI9440)	crtcreg = val & 0x7f;
		else				crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;
                //if (crtcreg!=0xE && crtcreg!=0xF) pclog("CRTC R%02X = %02X\n",crtcreg,val);
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                svga_recalctimings();
                        }
                }
                switch (crtcreg)
                {
			case 0x21:
			if (old != val)
				tvga_recalcmapping();
			break;


			case 0x38: /*Pixel bus register*/
//			pclog("Pixel bus register %02X\n", val);
			if (gfxcard != GFX_TGUI9440)
				break;
			
			if ((val & 0xc) == 4)	bpp = 16;
			else if (!(val & 0xc))	bpp = 8;
			else			bpp = 24;
			break;

			case 0x40: case 0x41: case 0x42: case 0x43:
			case 0x44: case 0x45: case 0x46: case 0x47:
			svga_hwcursor.x = (crtc[0x40] | (crtc[0x41] << 8)) & 0x7ff;
			svga_hwcursor.y = (crtc[0x42] | (crtc[0x43] << 8)) & 0x7ff;
			svga_hwcursor.xoff = crtc[0x46] & 0x3f;
			svga_hwcursor.yoff = crtc[0x47] & 0x3f;
			svga_hwcursor.addr = (crtc[0x44] << 10) | ((crtc[0x45] & 0x7) << 18) | (svga_hwcursor.yoff * 8);
			break;
			
			case 0x50:
			svga_hwcursor.ena = val & 0x80;
			break;
		}
                return;
                case 0x3D8:
                trident3d8=val;
                if (gdcreg[0xF]&4)
                {
                        svgawbank=(val&0x1F)*65536;
//                                pclog("SVGAWBANK 3D8 %08X %04X:%04X\n",svgawbank,CS,pc);
                        if (!(gdcreg[0xF]&1))
                        {
                                svgarbank=(val&0x1F)*65536;
//                                        pclog("SVGARBANK 3D8 %08X %04X:%04X\n",svgarbank,CS,pc);
                        }
                }
                return;
                case 0x3D9:
                trident3d9=val;
                if ((gdcreg[0xF]&5)==5)
                {
                        svgarbank=(val&0x1F)*65536;
//                                pclog("SVGARBANK 3D9 %08X %04X:%04X\n",svgarbank,CS,pc);
                }
                return;
        }
        svga_out(addr, val, priv);
}

uint8_t tvga_in(uint16_t addr, void *priv)
{
//        if (addr != 0x3da) pclog("tvga_in : %04X  %04X:%04X\n", addr, CS,pc);
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3C5:
                if ((seqaddr&0xF)==0xB)
                {
//                        printf("Read Trident ID %04X:%04X %04X\n",CS,pc,readmemw(ss,SP));
                        tridentoldmode=0;
                        if (gfxcard == GFX_TVGA) return 0x33; /*TVGA8900D*/
                        else                     return 0xe3; /*TGUI9440AGi*/
                }
                if ((seqaddr&0xF)==0xC)
                {
//                        printf("Read Trident Power Up 1 %04X:%04X %04X\n",CS,pc,readmemw(ss,SP));
//                        return 0x20; /*2 DRAM banks*/
                }
                if ((seqaddr&0xF)==0xD)
                {
                        if (tridentoldmode) return tridentoldctrl2;
                        return tridentnewctrl2;
                }
                break;
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
		if (gfxcard != GFX_TGUI9440)
                	return tkd8001_ramdac_in(addr, NULL);
                break;
                case 0x3CD: /*Banking*/
                return svgaseg;
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
                case 0x3d8:
		return trident3d8;
		case 0x3d9:
		return trident3d9;
        }
        return svga_in(addr, priv);
}

void tvga_recalctimings()
{
        if (!svga_rowoffset) svga_rowoffset=0x100; /*This is the only sensible way I can see this being handled,
                                                     given that TVGA8900D has no overflow bits.
                                                     Some sort of overflow is required for 320x200x24 and 1024x768x16*/

        if ((crtc[0x1E]&0xA0)==0xA0) svga_ma|=0x10000;
        if ((crtc[0x27]&0x01)==0x01) svga_ma|=0x20000;
        if ((crtc[0x27]&0x02)==0x02) svga_ma|=0x40000;
        
        if (tridentoldctrl2 & 0x10)
        {
                svga_rowoffset<<=1;
                svga_ma<<=1;
        }
        if (tridentoldctrl2 & 0x10) /*I'm not convinced this is the right register for this function*/
           svga_lowres=0;
        if (gfxcard == GFX_TGUI9440)
	   svga_lowres = !(crtc[0x2a] & 0x40); 
	   
        if (gdcreg[0xF] & 8)
        {
                svga_htotal<<=1;
                svga_hdisp<<=1;
        }
        svga_interlace = crtc[0x1E] & 4;
        if (svga_interlace)
           svga_rowoffset >>= 1;
        
        switch (((svga_miscout>>2)&3) | ((tridentnewctrl2<<2)&4))
        {
                case 2: svga_clock = cpuclock/44900000.0; break;
                case 3: svga_clock = cpuclock/36000000.0; break;
                case 4: svga_clock = cpuclock/57272000.0; break;
                case 5: svga_clock = cpuclock/65000000.0; break;
                case 6: svga_clock = cpuclock/50350000.0; break;
                case 7: svga_clock = cpuclock/40000000.0; break;
        }

        if ((tridentoldctrl2 & 0x10) || (gfxcard == GFX_TGUI9440 && (crtc[0x2a] & 0x40)))
        {
                switch (bpp)
                {
                        case 8: 
                        svga_render = svga_render_8bpp_highres; 
                        break;
                        case 15: 
                        svga_render = svga_render_15bpp_highres; 
                        break;
                        case 16: 
                        svga_render = svga_render_16bpp_highres; 
                        break;
                        case 24: 
                        svga_render = svga_render_24bpp_highres; 
                        break;
                        case 32: 
                        svga_render = svga_render_32bpp_highres; 
                        break;
                }
        }
}

void tvga_recalcmapping()
{
	pclog("tvga_recalcmapping : %02X %02X\n", crtc[0x21], gdcreg[6]);
        mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    NULL);
	mem_removehandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
	mem_removehandler(0xbc000, 0x4000, tvga_accel_read, NULL, NULL, tvga_accel_write, NULL, NULL,    NULL);

	if (crtc[0x21] & 0x20)
	{
		tvga_linear_base = ((crtc[0x21] & 0xf) | ((crtc[0x21] >> 2) & 0x30)) << 20;
		tvga_linear_size = (crtc[0x21] & 0x10) ? 0x200000 : 0x100000;
		mem_sethandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
		pclog("Trident linear framebuffer at %08X - size %06X\n", tvga_linear_base, tvga_linear_size);
		mem_sethandler(0xbc000, 0x4000, tvga_accel_read, tvga_accel_read_w, tvga_accel_read_l, tvga_accel_write, tvga_accel_write_w, tvga_accel_write_l,    NULL);
	}
	else
	{
//                                pclog("Write mapping %02X\n", val);
                switch (gdcreg[6] & 0xC)
                {
                        case 0x0: /*128k at A0000*/
                        mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    NULL);
                        break;
                        case 0x4: /*64k at A0000*/
                        mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    NULL);
                        mem_sethandler(0xbc000, 0x4000, tvga_accel_read, NULL, NULL, tvga_accel_write, NULL, NULL,    NULL);
                        break;
                        case 0x8: /*32k at B0000*/
                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    NULL);
                        break;
                        case 0xC: /*32k at B8000*/
                	mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,    NULL);
                	break;
		}
        }
}

void tvga_hwcursor_draw(int displine)
{
//        int x;
        uint32_t dat[2];
        int xx;
        int offset = svga_hwcursor_latch.x - svga_hwcursor_latch.xoff;
        
//        pclog("HWcursor %i %i\n", svga_hwcursor_latch.x, svga_hwcursor_latch.y);
//        for (x = 0; x < 32; x += 32)
//        {
                dat[0] = (vram[svga_hwcursor_latch.addr]     << 24) | (vram[svga_hwcursor_latch.addr + 1] << 16) | (vram[svga_hwcursor_latch.addr + 2] << 8) | vram[svga_hwcursor_latch.addr + 3];
                dat[1] = (vram[svga_hwcursor_latch.addr + 4] << 24) | (vram[svga_hwcursor_latch.addr + 5] << 16) | (vram[svga_hwcursor_latch.addr + 6] << 8) | vram[svga_hwcursor_latch.addr + 7];
                for (xx = 0; xx < 32; xx++)
                {
                        if (offset >= svga_hwcursor_latch.x)
                        {
                                if (!(dat[0] & 0x80000000))
                                   ((uint32_t *)buffer32->line[displine])[offset + 32]  = (dat[1] & 0x80000000) ? 0xffffff : 0;
                                else if (dat[1] & 0x80000000)
                                   ((uint32_t *)buffer32->line[displine])[offset + 32] ^= 0xffffff;
//                                pclog("Plot %i, %i (%i %i) %04X %04X\n", offset, displine, x+xx, svga_hwcursor_on, dat[0], dat[1]);
                        }
                           
                        offset++;
                        dat[0] <<= 1;
                        dat[1] <<= 1;
                }
                svga_hwcursor_latch.addr += 8;
//        }
}

int tvga_init()
{
        svga_recalctimings_ex = tvga_recalctimings;
        svga_hwcursor_draw    = tvga_hwcursor_draw;
        if (gfxcard == GFX_TVGA) 
	{
		vrammask = 0xfffff;
		svga_vram_limit = 1 << 20; /*1mb - chip supports 2mb, but drivers are buggy*/
	}
        else
	{
		vrammask = 0x1fffff;
		svga_vram_limit = 1 << 21; /*2mb*/
	}
        return svga_init();
}

struct tvga_accel
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
} tvga_accel;

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

#define READ(addr, dat) if (tvga_accel.bpp == 0) dat = vram[addr & 0x1fffff]; \
                        else                     dat = vram_w[addr & 0xfffff];
                        
#define MIX() do \
	{								\
		out = 0;						\
        	for (c=0;c<16;c++)					\
	        {							\
			d=(dst_dat & (1<<c)) ? 1:0;			\
                       	if (src_dat & (1<<c)) d|=2;			\
               	        if (pat_dat & (1<<c)) d|=4;			\
                        if (tvga_accel.rop & (1<<d)) out|=(1<<c);	\
	        }							\
	} while (0)

#define WRITE(addr, dat)        if (tvga_accel.bpp == 0)                                                \
                                {                                                                       \
                                        vram[addr & 0x1fffff] = dat;                                    \
                                        changedvram[((addr) & 0x1fffff) >> 10] = changeframecount;      \
                                }                                                                       \
                                else                                                                    \
                                {                                                                       \
                                        vram_w[addr & 0xfffff] = dat;                                   \
                                        changedvram[((addr) & 0xfffff) >> 9] = changeframecount;        \
                                }
                                
static uint16_t tvga_pattern[8][8];

void tvga_accel_write_fb_b(uint32_t addr, uint8_t val, void *priv);
void tvga_accel_write_fb_w(uint32_t addr, uint16_t val, void *priv);

void tvga_accel_command(int count, uint32_t cpu_dat)
{
	int x, y;
	int c, d;
	uint16_t src_dat, dst_dat, pat_dat;
	uint16_t out;
	int xdir = (tvga_accel.flags & 0x200) ? -1 : 1;
	int ydir = (tvga_accel.flags & 0x100) ? -1 : 1;
	uint16_t trans_col = (tvga_accel.flags & TGUI_TRANSREV) ? tvga_accel.fg_col : tvga_accel.bg_col;
        uint16_t *vram_w = (uint16_t *)vram;
        
	if (tvga_accel.bpp == 0)
                trans_col &= 0xff;
	
	if (count != -1 && !tvga_accel.x)
	{
		count -= tvga_accel.offset;
		cpu_dat <<= tvga_accel.offset;
	}
	if (count == -1)
	{
		tvga_accel.x = tvga_accel.y = 0;
	}
	if (tvga_accel.flags & TGUI_SOLIDFILL)
	{
//		pclog("SOLIDFILL\n");
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				tvga_pattern[y][x] = tvga_accel.fg_col;
			}
		}
	}
	else if (tvga_accel.flags & TGUI_PATMONO)
	{
//		pclog("PATMONO\n");
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				tvga_pattern[y][x] = (tvga_accel.pattern[y] & (1 << x)) ? tvga_accel.fg_col : tvga_accel.bg_col;
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
				tvga_pattern[y][x] = tvga_accel.pattern[x + y*8];
			}
		}
	}
	for (y = 0; y < 8; y++)
	{
		if (count == -1) pclog("Pattern %i : %02X %02X %02X %02X %02X %02X %02X %02X\n", y, tvga_pattern[y][0], tvga_pattern[y][1], tvga_pattern[y][2], tvga_pattern[y][3], tvga_pattern[y][4], tvga_pattern[y][5], tvga_pattern[y][6], tvga_pattern[y][7]);
	}
	if (count == -1) pclog("Command %i %i\n", tvga_accel.command, TGUI_BITBLT);
	switch (tvga_accel.command)
	{
		case TGUI_BITBLT:
		if (count == -1) pclog("BITBLT src %i,%i dst %i,%i size %i,%i flags %04X\n", tvga_accel.src_x, tvga_accel.src_y, tvga_accel.dst_x, tvga_accel.dst_y, tvga_accel.size_x, tvga_accel.size_y, tvga_accel.flags);
		if (count == -1)
		{
			tvga_accel.src = tvga_accel.src_old = tvga_accel.src_x + (tvga_accel.src_y * tvga_accel.pitch);
			tvga_accel.dst = tvga_accel.dst_old = tvga_accel.dst_x + (tvga_accel.dst_y * tvga_accel.pitch);
			tvga_accel.pat_x = tvga_accel.dst_x;
			tvga_accel.pat_y = tvga_accel.dst_y;
		}

		switch (tvga_accel.flags & (TGUI_SRCMONO|TGUI_SRCDISP))
		{
			case TGUI_SRCCPU:
			if (count == -1)
			{
//				pclog("Blit start\n");
				if (crtc[0x21] & 0x20)
				{
					mem_removehandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
					mem_sethandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
				}
				if (tvga_accel.use_src)
                                        return;
			}
			else
			     count >>= 3;
			while (count)
			{
				if (tvga_accel.bpp == 0)
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
				READ(tvga_accel.dst, dst_dat);
				pat_dat = tvga_pattern[tvga_accel.pat_y & 7][tvga_accel.pat_x & 7];
	
                                if (!(tvga_accel.flags & TGUI_TRANSENA) || src_dat != trans_col)
                                {
        				MIX();
	
                                        WRITE(tvga_accel.dst, out);
                                }

//				pclog("  %i,%i  %02X %02X %02X  %02X\n", tvga_accel.x, tvga_accel.y, src_dat,dst_dat,pat_dat, out);
                                	
				tvga_accel.src += xdir;
				tvga_accel.dst += xdir;
				tvga_accel.pat_x += xdir;
	
				tvga_accel.x++;
				if (tvga_accel.x > tvga_accel.size_x)
				{
					tvga_accel.x = 0;
					tvga_accel.y++;
					
					tvga_accel.pat_x = tvga_accel.dst_x;
	
					tvga_accel.src = tvga_accel.src_old = tvga_accel.src_old + (ydir * tvga_accel.pitch);
					tvga_accel.dst = tvga_accel.dst_old = tvga_accel.dst_old + (ydir * tvga_accel.pitch);
					tvga_accel.pat_y += ydir;
					
					if (tvga_accel.y > tvga_accel.size_y)
					{
						if (crtc[0x21] & 0x20)
						{
//							pclog("Blit end\n");
							mem_removehandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
							mem_sethandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
						}
						return;
					}
					if (tvga_accel.use_src)
                                                return;
				}
				count--;
			}
			break;
						
			case TGUI_SRCMONO | TGUI_SRCCPU:
			if (count == -1)
			{
//				pclog("Blit start\n");
				if (crtc[0x21] & 0x20)
				{
					mem_removehandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
					mem_sethandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
				}
				if (tvga_accel.use_src)
                                        return;
			}
//			pclog("TGUI_SRCMONO | TGUI_SRCCPU\n");
			while (count)
			{
				src_dat = ((cpu_dat >> 31) ? tvga_accel.fg_col : tvga_accel.bg_col);
				if (tvga_accel.bpp == 0)
				    src_dat &= 0xff;
				    
				READ(tvga_accel.dst, dst_dat);
				pat_dat = tvga_pattern[tvga_accel.pat_y & 7][tvga_accel.pat_x & 7];

                                if (!(tvga_accel.flags & TGUI_TRANSENA) || src_dat != trans_col)
                                {
        				MIX();

				        WRITE(tvga_accel.dst, out);
                                }
//				pclog("  %i,%i  %02X %02X %02X  %02X %i\n", tvga_accel.x, tvga_accel.y, src_dat,dst_dat,pat_dat, out, (!(tvga_accel.flags & TGUI_TRANSENA) || src_dat != trans_col));
				cpu_dat <<= 1;
				tvga_accel.src += xdir;
				tvga_accel.dst += xdir;
				tvga_accel.pat_x += xdir;
	
				tvga_accel.x++;
				if (tvga_accel.x > tvga_accel.size_x)
				{
					tvga_accel.x = 0;
					tvga_accel.y++;
					
					tvga_accel.pat_x = tvga_accel.dst_x;
	
					tvga_accel.src = tvga_accel.src_old = tvga_accel.src_old + (ydir * tvga_accel.pitch);
					tvga_accel.dst = tvga_accel.dst_old = tvga_accel.dst_old + (ydir * tvga_accel.pitch);
					tvga_accel.pat_y += ydir;
					
					if (tvga_accel.y > tvga_accel.size_y)
					{
						if (crtc[0x21] & 0x20)
						{
//							pclog("Blit end\n");
							mem_removehandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, tvga_accel_write_fb_b, tvga_accel_write_fb_w, tvga_accel_write_fb_l,    NULL);
							mem_sethandler(tvga_linear_base, tvga_linear_size, svga_read_linear, svga_readw_linear, svga_readl_linear, svga_write_linear, svga_writew_linear, svga_writel_linear,    NULL);
						}
						return;
					}
					if (tvga_accel.use_src)
                                                return;
				}
				count--;
			}
			break;

			default:
			while (count)
			{
				READ(tvga_accel.src, src_dat);
				READ(tvga_accel.dst, dst_dat);                                                                
				pat_dat = tvga_pattern[tvga_accel.pat_y & 7][tvga_accel.pat_x & 7];
	
                                if (!(tvga_accel.flags & TGUI_TRANSENA) || src_dat != trans_col)
                                {
        				MIX();
	
                                        WRITE(tvga_accel.dst, out);
                                }
//                                pclog("  %i,%i  %02X %02X %02X  %02X\n", tvga_accel.x, tvga_accel.y, src_dat,dst_dat,pat_dat, out);
	
				tvga_accel.src += xdir;
				tvga_accel.dst += xdir;
				tvga_accel.pat_x += xdir;
	
				tvga_accel.x++;
				if (tvga_accel.x > tvga_accel.size_x)
				{
					tvga_accel.x = 0;
					tvga_accel.y++;
					
					tvga_accel.pat_x = tvga_accel.dst_x;
	
					tvga_accel.src = tvga_accel.src_old = tvga_accel.src_old + (ydir * tvga_accel.pitch);
					tvga_accel.dst = tvga_accel.dst_old = tvga_accel.dst_old + (ydir * tvga_accel.pitch);
					tvga_accel.pat_y += ydir;
					
					if (tvga_accel.y > tvga_accel.size_y)
						return;
				}
				count--;
			}
			break;
		}
		break;
	}
}

void tvga_accel_write(uint32_t addr, uint8_t val, void *priv)
{
	pclog("tvga_accel_write : %08X %02X  %04X(%08X):%08X %02X\n", addr, val, CS,cs,pc, opcode);
	if ((addr & ~0xff) != 0xbff00)
		return;
	switch (addr & 0xff)
	{
                case 0x22:
                tvga_accel.ger22 = val;
                tvga_accel.pitch = 512 << ((val >> 2) & 3);
                tvga_accel.bpp = (val & 3) ? 1 : 0;
                tvga_accel.pitch >>= tvga_accel.bpp;
                break;
                
		case 0x24: /*Command*/
		tvga_accel.command = val;
		tvga_accel_command(-1, 0);
		break;
		
		case 0x27: /*ROP*/
		tvga_accel.rop = val;
		tvga_accel.use_src = (val & 0x33) ^ ((val >> 2) & 0x33);
		pclog("Write ROP %02X %i\n", val, tvga_accel.use_src);
		break;
		
		case 0x28: /*Flags*/
		tvga_accel.flags = (tvga_accel.flags & 0xff00) | val;
		break;
		case 0x29: /*Flags*/
		tvga_accel.flags = (tvga_accel.flags & 0xff) | (val << 8);
		break;

		case 0x2b:
		tvga_accel.offset = val & 7;
		break;
		
		case 0x2c: /*Foreground colour*/
		tvga_accel.fg_col = (tvga_accel.fg_col & 0xff00) | val;
		break;
		case 0x2d: /*Foreground colour*/
		tvga_accel.fg_col = (tvga_accel.fg_col & 0xff) | (val << 8);
		break;

		case 0x30: /*Background colour*/
		tvga_accel.bg_col = (tvga_accel.bg_col & 0xff00) | val;
		break;
		case 0x31: /*Background colour*/
		tvga_accel.bg_col = (tvga_accel.bg_col & 0xff) | (val << 8);
		break;

		case 0x38: /*Dest X*/
		tvga_accel.dst_x = (tvga_accel.dst_x & 0xff00) | val;
		break;
		case 0x39: /*Dest X*/
		tvga_accel.dst_x = (tvga_accel.dst_x & 0xff) | (val << 8);
		break;
		case 0x3a: /*Dest Y*/
		tvga_accel.dst_y = (tvga_accel.dst_y & 0xff00) | val;
		break;
		case 0x3b: /*Dest Y*/
		tvga_accel.dst_y = (tvga_accel.dst_y & 0xff) | (val << 8);
		break;

		case 0x3c: /*Src X*/
		tvga_accel.src_x = (tvga_accel.src_x & 0xff00) | val;
		break;
		case 0x3d: /*Src X*/
		tvga_accel.src_x = (tvga_accel.src_x & 0xff) | (val << 8);
		break;
		case 0x3e: /*Src Y*/
		tvga_accel.src_y = (tvga_accel.src_y & 0xff00) | val;
		break;
		case 0x3f: /*Src Y*/
		tvga_accel.src_y = (tvga_accel.src_y & 0xff) | (val << 8);
		break;

		case 0x40: /*Size X*/
		tvga_accel.size_x = (tvga_accel.size_x & 0xff00) | val;
		break;
		case 0x41: /*Size X*/
		tvga_accel.size_x = (tvga_accel.size_x & 0xff) | (val << 8);
		break;
		case 0x42: /*Size Y*/
		tvga_accel.size_y = (tvga_accel.size_y & 0xff00) | val;
		break;
		case 0x43: /*Size Y*/
		tvga_accel.size_y = (tvga_accel.size_y & 0xff) | (val << 8);
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
		tvga_accel.pattern[addr & 0x7f] = val;
		break;
	}
}

void tvga_accel_write_w(uint32_t addr, uint16_t val, void *priv)
{
	pclog("tvga_accel_write_w %08X %04X\n", addr, val);
	tvga_accel_write(addr, val, NULL);
	tvga_accel_write(addr + 1, val >> 8, NULL);
}

void tvga_accel_write_l(uint32_t addr, uint32_t val, void *priv)
{
	pclog("tvga_accel_write_l %08X %08X\n", addr, val);
	tvga_accel_write(addr, val, NULL);
	tvga_accel_write(addr + 1, val >> 8, NULL);
	tvga_accel_write(addr + 2, val >> 16, NULL);
	tvga_accel_write(addr + 3, val >> 24, NULL);
}

uint8_t tvga_accel_read(uint32_t addr, void *priv)
{
//	pclog("tvga_accel_read : %08X\n", addr);
	if ((addr & ~0xff) != 0xbff00)
		return 0xff;
	switch (addr & 0xff)
	{
		case 0x20: /*Status*/
		return 0;
		
		case 0x27: /*ROP*/
		return tvga_accel.rop;
		
		case 0x28: /*Flags*/
		return tvga_accel.flags & 0xff;
		case 0x29: /*Flags*/
		return tvga_accel.flags >> 8;

		case 0x2b:
		return tvga_accel.offset;
		
		case 0x2c: /*Background colour*/
		return tvga_accel.bg_col & 0xff;
		case 0x2d: /*Background colour*/
		return tvga_accel.bg_col >> 8;

		case 0x30: /*Foreground colour*/
		return tvga_accel.fg_col & 0xff;
		case 0x31: /*Foreground colour*/
		return tvga_accel.fg_col >> 8;

		case 0x38: /*Dest X*/
		return tvga_accel.dst_x & 0xff;
		case 0x39: /*Dest X*/
		return tvga_accel.dst_x >> 8;
		case 0x3a: /*Dest Y*/
		return tvga_accel.dst_y & 0xff;
		case 0x3b: /*Dest Y*/
		return tvga_accel.dst_y >> 8;

		case 0x3c: /*Src X*/
		return tvga_accel.src_x & 0xff;
		case 0x3d: /*Src X*/
		return tvga_accel.src_x >> 8;
		case 0x3e: /*Src Y*/
		return tvga_accel.src_y & 0xff;
		case 0x3f: /*Src Y*/
		return tvga_accel.src_y >> 8;

		case 0x40: /*Size X*/
		return tvga_accel.size_x & 0xff;
		case 0x41: /*Size X*/
		return tvga_accel.size_x >> 8;
		case 0x42: /*Size Y*/
		return tvga_accel.size_y & 0xff;
		case 0x43: /*Size Y*/
		return tvga_accel.size_y >> 8;
		
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
		return tvga_accel.pattern[addr & 0x7f];
	}
	return 0xff;
}

uint16_t tvga_accel_read_w(uint32_t addr, void *priv)
{
	pclog("tvga_accel_read_w %08X\n", addr);
	return tvga_accel_read(addr, NULL) | (tvga_accel_read(addr + 1, NULL) << 8);
}

uint32_t tvga_accel_read_l(uint32_t addr, void *priv)
{
	pclog("tvga_accel_read_l %08X\n", addr);
	return tvga_accel_read_w(addr, NULL) | (tvga_accel_read_w(addr + 2, NULL) << 16);
}

void tvga_accel_write_fb_b(uint32_t addr, uint8_t val, void *priv)
{
//	pclog("tvga_accel_write_fb_b %08X %02X\n", addr, val);
	tvga_accel_command(8, val << 24);
}

void tvga_accel_write_fb_w(uint32_t addr, uint16_t val, void *priv)
{
//	pclog("tvga_accel_write_fb_w %08X %04X\n", addr, val);
	tvga_accel_command(16, (((val & 0xff00) >> 8) | ((val & 0x00ff) << 8)) << 16);
}

void tvga_accel_write_fb_l(uint32_t addr, uint32_t val, void *priv)
{
//	pclog("tvga_accel_write_fb_l %08X %08X\n", addr, val);
	tvga_accel_command(32, ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24));
}

GFXCARD vid_tvga =
{
        tvga_init,
        /*IO at 3Cx/3Dx*/
        tvga_out,
        tvga_in,
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

GFXCARD vid_tgui9440 =
{
        tvga_init,
        /*IO at 3Cx/3Dx*/
        tvga_out,
        tvga_in,
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


