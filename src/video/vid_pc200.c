/*PC200/PPC video emulation. */
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "nmi.h"
#include "timer.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_mda.h"
#include "vid_pc200.h"

/* #define PC200LOG(x) pclog x */
#define PC200LOG(x)

/* Display type */
#define PC200_CGA  0	/* CGA monitor */
#define PC200_MDA  1	/* MDA monitor */
#define PC200_TV   2	/* Television */
#define PC200_LCDC 3	/* PPC512 LCD as CGA*/
#define PC200_LCDM 4	/* PPC512 LCD as MDA*/

int pc200_is_mda;

static uint32_t blue, green;

static uint32_t lcdcols[256][2][2];

typedef struct pc200_t
{
        mem_mapping_t cga_mapping;
        mem_mapping_t mda_mapping;
        
        cga_t cga;
	mda_t mda;

        pc_timer_t timer;

	uint8_t emulation;	/* Which display are we emulating? */
	uint8_t dipswitches;	/* DIP switches 1-3 */
        uint8_t crtc_index;	/* CRTC index readback 
				 * Bit 7: CGA control port written
				 * Bit 6: Operation control port written
				 * Bit 5: CRTC register written 
				 * Bits 0-4: Last CRTC register selected */
	uint8_t operation_ctrl;
	uint8_t reg_3df;
} pc200_t;

static uint8_t crtcmask[32] = 
{
        0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x7f, 0x7f, 0xf3, 0x1f, 0x7f, 0x1f, 0x3f, 0xff, 0x3f, 0xff,
        0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* LCD colour mappings
 * 
 * 0 => solid green
 * 1 => blue on green
 * 2 => green on blue
 * 3 => solid blue
 */
static unsigned char mapping1[256] =
{
/*	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/*00*/	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*10*/	2, 0, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1,
/*20*/	2, 2, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1,
/*30*/	2, 2, 2, 0, 2, 2, 1, 1, 2, 2, 2, 1, 2, 2, 1, 1,
/*40*/	2, 2, 1, 1, 0, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1,
/*50*/	2, 2, 1, 1, 2, 0, 1, 1, 2, 2, 1, 1, 2, 1, 1, 1,
/*60*/	2, 2, 2, 2, 2, 2, 0, 1, 2, 2, 2, 2, 2, 2, 1, 1,
/*70*/	2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 1,
/*80*/	2, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
/*90*/	2, 2, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 1, 1, 1, 1,
/*A0*/	2, 2, 2, 1, 2, 2, 1, 1, 2, 2, 0, 1, 2, 2, 1, 1,
/*B0*/	2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 0, 2, 2, 1, 1,
/*C0*/	2, 2, 1, 1, 2, 1, 1, 1, 2, 2, 1, 1, 0, 1, 1, 1,
/*D0*/	2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 0, 1, 1,
/*E0*/	2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 0, 1,
/*F0*/	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0,
};

static unsigned char mapping2[256] =
{
/*	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/*00*/	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*10*/	1, 3, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2,
/*20*/	1, 1, 3, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2,
/*30*/	1, 1, 1, 3, 1, 1, 2, 2, 1, 1, 1, 2, 1, 1, 2, 2,
/*40*/	1, 1, 2, 2, 3, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 2,
/*50*/	1, 1, 2, 2, 1, 3, 2, 2, 1, 1, 2, 2, 1, 2, 2, 2,
/*60*/	1, 1, 1, 1, 1, 1, 3, 2, 1, 1, 1, 1, 1, 1, 2, 2,
/*70*/	1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 2,
/*80*/	2, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
/*90*/	1, 1, 2, 2, 2, 2, 2, 2, 1, 3, 2, 2, 2, 2, 2, 2,
/*A0*/	1, 1, 1, 2, 1, 1, 2, 2, 1, 1, 3, 2, 1, 1, 2, 2,
/*B0*/	1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 3, 1, 1, 2, 2,
/*C0*/	1, 1, 2, 2, 1, 2, 2, 2, 1, 1, 2, 2, 3, 2, 2, 2,
/*D0*/	1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 3, 2, 2,
/*E0*/	1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 2,
/*F0*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3,
};

static void set_lcd_cols(uint8_t mode_reg)
{
	unsigned char *mapping = (mode_reg & 0x80) ? mapping2 : mapping1;
	int c;

	for (c = 0; c < 256; c++)
	{
		switch (mapping[c])
		{
			case 0:
			lcdcols[c][0][0] = lcdcols[c][1][0] = green;
			lcdcols[c][0][1] = lcdcols[c][1][1] = green;
			break;

			case 1:
			lcdcols[c][0][0] = lcdcols[c][1][0] = 
					   lcdcols[c][1][1] = green;
			lcdcols[c][0][1] = blue;
			break;

			case 2:
			lcdcols[c][0][0] = lcdcols[c][1][0] = 
					   lcdcols[c][1][1] = blue;
			lcdcols[c][0][1] = green;
			break;

			case 3:
			lcdcols[c][0][0] = lcdcols[c][1][0] = blue;
			lcdcols[c][0][1] = lcdcols[c][1][1] = blue;
			break;
		}
	}
}


void pc200_out(uint16_t addr, uint8_t val, void *p)
{
        pc200_t *pc200 = (pc200_t *)p;
        cga_t *cga = &pc200->cga;
	mda_t *mda = &pc200->mda;
        uint8_t old;
       
	PC200LOG(("pc200_out(0x%04x), %02x\n", addr, val));
 
/* MDA writes ============================================================== */
        switch (addr)
        {
		case 0x3b1:
		case 0x3b3:
		case 0x3b5:
                case 0x3b7:

		/* Writes banned to CRTC registers 0-11? */
                if (!(pc200->operation_ctrl & 0x40) && mda->crtcreg <= 11)
                {
			pc200->crtc_index = 0x20 | (mda->crtcreg & 0x1f);
                        if (pc200->operation_ctrl & 0x80)
			{ 
				PC200LOG(("NMI raised from CRTC register access: reg=0x%02x val=0x%02x nmi_mask=%d\n", mda->crtcreg & 0x1F, val, nmi_mask));
                                nmi = 1;
                        }        
                        pc200->reg_3df = val;
                        return;
                }
		PC200LOG(("CRTM[%02x]:=%02x\n", mda->crtcreg, val));
                old = mda->crtc[mda->crtcreg];
                mda->crtc[mda->crtcreg] = val & crtcmask[mda->crtcreg];
                if (old != val)
                {
                        if (mda->crtcreg < 0xe || mda->crtcreg > 0x10)
                        {
                                fullchange = changeframecount;
                                mda_recalctimings(mda);
                        }
                }
                return;

                case 0x3b8:
                old = mda->ctrl;
                mda->ctrl = val;
                if ((mda->ctrl ^ old) & 3)
		{
                	mda_recalctimings(mda);
		}
		pc200->crtc_index &= 0x1F;
               	pc200->crtc_index |= 0x80;
                if (pc200->operation_ctrl & 0x80)
		{
			PC200LOG(("NMI raised from mode control access: val=0x%02x nmi_mask=%d\n", val, nmi_mask));
			nmi = 1;
		}
                return;


/* CGA writes ============================================================== */
		case 0x3d1:
		case 0x3d3:
		case 0x3d5:
                case 0x3d7:

		/* Writes banned to CRTC registers 0-11? */
                if (!(pc200->operation_ctrl & 0x40) && cga->crtcreg <= 11)
                {
			pc200->crtc_index = 0x20 | (cga->crtcreg & 0x1f);
                        if (pc200->operation_ctrl & 0x80)
			{ 
				PC200LOG(("NMI raised from CRTC register access: reg=0x%02x val=0x%02x nmi_mask=%d\n", cga->crtcreg & 0x1F, val, nmi_mask));
                                nmi = 1;
                        }        
                        pc200->reg_3df = val;
                        return;
                }
		PC200LOG(("CRTC[%02x]:=%02x\n", cga->crtcreg, val));
                old = cga->crtc[cga->crtcreg];
                cga->crtc[cga->crtcreg] = val & crtcmask[cga->crtcreg];
                if (old != val)
                {
                        if (cga->crtcreg < 0xe || cga->crtcreg > 0x10)
                        {
                                fullchange = changeframecount;
                                cga_recalctimings(cga);
                        }
                }
                return;
                
                case 0x3d8:
                old = cga->cgamode;
                cga->cgamode = val;
                if ((cga->cgamode ^ old) & 3)
		{
                	cga_recalctimings(cga);
		}
		pc200->crtc_index &= 0x1F;
               	pc200->crtc_index |= 0x80;
                if (pc200->operation_ctrl & 0x80)
		{
			PC200LOG(("NMI raised from mode control access: val=0x%02x nmi_mask=%d\n", val, nmi_mask));
			nmi = 1;
		}
		else set_lcd_cols(val);
                return;
                
/* PC200 control port writes ============================================== */
                case 0x3de:
                pc200->crtc_index = 0x1f;
/* NMI only seems to be triggered if the value being written has the high
 * bit set (enable NMI). So it only protects writes to this port if you 
 * let it? */
                if (val & 0x80)
		{
		   	PC200LOG(("NMI raised from operation control access: val=0x%02x nmi_mask=%d\n", val, nmi_mask));
                	pc200->operation_ctrl = val;
                        pc200->crtc_index |= 0x40;
			nmi = 1;
			return;
		}
                timer_disable(&pc200->cga.timer);
                timer_disable(&pc200->mda.timer);
                timer_disable(&pc200->timer);
                pc200->operation_ctrl = val;
		/* Bits 0 and 1 control emulation and output mode */
		if (val & 1)	/* Monitor */
		{
			pc200->emulation = (val & 2) ? PC200_MDA : PC200_CGA;
		}
		else if (romset == ROM_PPC512)
		{
			pc200->emulation = (val & 2) ? PC200_LCDM : PC200_LCDC;
		}
		else
		{
			pc200->emulation = PC200_TV;

		}
		PC200LOG(("Video emulation set to %d\n", pc200->emulation));
		if (pc200->emulation == PC200_CGA || pc200->emulation == PC200_TV)
                        timer_advance_u64(&pc200->cga.timer, 1);
		else if (pc200->emulation == PC200_MDA)
                        timer_advance_u64(&pc200->mda.timer, 1);
                else
                        timer_advance_u64(&pc200->timer, 1);
                        
		/* Bit 2 disables the IDA. We don't support dynamic enabling
		 * and disabling of the IDA (instead, PCEM disconnects the 
		 * IDA from the bus altogether) so don't implement this */	

		/* Enable the appropriate memory ranges depending whether 
		 * the IDA is configured as MDA or CGA */
		if (pc200->emulation == PC200_MDA || 
		    pc200->emulation == PC200_LCDM)
		{
			mem_mapping_disable(&pc200->cga_mapping);
			mem_mapping_enable(&pc200->mda_mapping);
		}
		else
		{
			mem_mapping_disable(&pc200->mda_mapping);
			mem_mapping_enable(&pc200->cga_mapping);
		}
                return;
        }
	if (addr >= 0x3D0 && addr <= 0x3DF)
	{
        	cga_out(addr, val, cga);
	}
	if (addr >= 0x3B0 && addr <= 0x3BB)
	{
		mda_out(addr, val, mda);
	}
}

uint8_t pc200_in(uint16_t addr, void *p)
{
        pc200_t *pc200 = (pc200_t *)p;
        cga_t *cga = &pc200->cga;
        mda_t *mda = &pc200->mda;
        uint8_t temp;

	PC200LOG(("pc200_in(0x%04x)\n", addr));
        switch (addr)
        {
                case 0x3B8:
		PC200LOG(("  = %02x\n", mda->ctrl));
                return mda->ctrl;

                case 0x3D8:
		PC200LOG(("  = %02x\n", cga->cgamode));
                return cga->cgamode;
                
                case 0x3DD:
                temp = pc200->crtc_index;	/* Read NMI reason */
                pc200->crtc_index &= 0x1f;	/* Reset NMI reason */
		nmi = 0;			/* And reset NMI flag */
		PC200LOG(("  = %02x\n", temp));
                return temp;
                
                case 0x3DE:
		PC200LOG(("  = %02x\n", ((pc200->operation_ctrl & 0xc7) | pc200->dipswitches)));
		return ((pc200->operation_ctrl & 0xc7) | pc200->dipswitches);

                case 0x3DF:
                return pc200->reg_3df;
        }
	if (addr >= 0x3D0 && addr <= 0x3DF)
	{
        	return cga_in(addr, cga);
	}
	if (addr >= 0x3B0 && addr <= 0x3BB)
	{
		return mda_in(addr, mda);
	}
        return 0xFF;
}

void lcd_draw_char_80(pc200_t *pc200, uint32_t *buffer, uint8_t chr, 
			uint8_t attr, int drawcursor, int blink, int sc,
			int mode160, uint8_t control)
{
	int c;
	uint8_t bits = fontdat[chr + pc200->cga.fontbase][sc];
	uint8_t bright = 0;
	uint16_t mask;

	if (attr & 8) /* bright */
	{
/* The brightness algorithm appears to be: replace any bit sequence 011 
 * with 001 (assuming an extra 0 to the left of the byte). 
 */
		bright = bits;
		for (c = 0, mask = 0x100; c < 7; c++, mask >>= 1)
		{
			if (((bits & mask) == 0) &&
			    ((bits & (mask >> 1)) != 0) &&
			    ((bits & (mask >> 2)) != 0))
			{
				bright &= ~(mask >> 1);
			}
		}
		bits = bright;
	}

	if (drawcursor) bits ^= 0xFF;

	for (c = 0, mask = 0x80; c < 8; c++, mask >>= 1)
	{
		if (mode160)
                        buffer[c] = (attr & mask) ? blue : green;
		else if (control & 0x20) /* blinking */
			buffer[c] = lcdcols[attr & 0x7F][blink][(bits & mask) ? 1 : 0];
		else
                        buffer[c] = lcdcols[attr][blink][(bits & mask) ? 1 : 0];
	}
}


void lcd_draw_char_40(pc200_t *pc200, uint32_t *buffer, uint8_t chr, 
			uint8_t attr, int drawcursor, int blink, int sc,
			uint8_t control)
{
	int c;
	uint8_t bits = fontdat[chr + pc200->cga.fontbase][sc];
	uint8_t mask = 0x80;

	if (attr & 8) /* bright */
	{
		bits = bits & (bits >> 1);
	}
	if (drawcursor) bits ^= 0xFF;

	for (c = 0; c < 8; c++, mask >>= 1)
	{
		if (control & 0x20)
		{
			buffer[c*2] = buffer[c*2+1] = 
				lcdcols[attr & 0x7F][blink][(bits & mask) ? 1 : 0];
		}
		else
		{
			buffer[c*2] = buffer[c*2+1] = 
				lcdcols[attr][blink][(bits & mask) ? 1 : 0];
		}
	}
}

void lcdm_poll(pc200_t *pc200)
{
        mda_t *mda = &pc200->mda;
        uint16_t ca = (mda->crtc[15] | (mda->crtc[14] << 8)) & 0x3fff;
        int drawcursor;
        int x;
        int oldvc;
        uint8_t chr, attr;
        int oldsc;
        int blink;
        if (!mda->linepos)
        {
                timer_advance_u64(&pc200->timer, mda->dispofftime);
                mda->stat |= 1;
                mda->linepos = 1;
                oldsc = mda->sc;
                if ((mda->crtc[8] & 3) == 3) 
                        mda->sc = (mda->sc << 1) & 7;
                if (mda->dispon)
                {
                        if (mda->displine < mda->firstline)
                        {
                                mda->firstline = mda->displine;                                
                                video_wait_for_buffer();
                        }
                        mda->lastline = mda->displine;
                        for (x = 0; x < mda->crtc[1]; x++)
                        {
                                chr  = mda->vram[(mda->ma << 1) & 0xfff];
                                attr = mda->vram[((mda->ma << 1) + 1) & 0xfff];
                                drawcursor = ((mda->ma == ca) && mda->con && mda->cursoron);
                                blink = ((mda->blink & 16) && (mda->ctrl & 0x20) && (attr & 0x80) && !drawcursor);

				lcd_draw_char_80(pc200, &((uint32_t *)(buffer32->line[mda->displine]))[x * 8], chr, attr, drawcursor, blink, mda->sc, 0, mda->ctrl);
				mda->ma++;
                        }
                }
                mda->sc = oldsc;
                if (mda->vc == mda->crtc[7] && !mda->sc)
                {
                        mda->stat |= 8;
//                        printf("VSYNC on %i %i\n",vc,sc);
                }
                mda->displine++;
                if (mda->displine >= 500) 
                        mda->displine=0;
        }
        else
        {
                timer_advance_u64(&pc200->timer, mda->dispontime);
                if (mda->dispon) mda->stat&=~1;
                mda->linepos=0;
                if (mda->vsynctime)
                {
                        mda->vsynctime--;
                        if (!mda->vsynctime)
                        {
                                mda->stat&=~8;
//                                printf("VSYNC off %i %i\n",vc,sc);
                        }
                }
                if (mda->sc == (mda->crtc[11] & 31) || ((mda->crtc[8] & 3) == 3 && mda->sc == ((mda->crtc[11] & 31) >> 1))) 
                { 
                        mda->con = 0; 
                        mda->coff = 1; 
                }
                if (mda->vadj)
                {
                        mda->sc++;
                        mda->sc &= 31;
                        mda->ma = mda->maback;
                        mda->vadj--;
                        if (!mda->vadj)
                        {
                                mda->dispon = 1;
                                mda->ma = mda->maback = (mda->crtc[13] | (mda->crtc[12] << 8)) & 0x3fff;
                                mda->sc = 0;
                        }
                }
                else if (mda->sc == mda->crtc[9] || ((mda->crtc[8] & 3) == 3 && mda->sc == (mda->crtc[9] >> 1)))
                {
                        mda->maback = mda->ma;
                        mda->sc = 0;
                        oldvc = mda->vc;
                        mda->vc++;
                        mda->vc &= 127;
                        if (mda->vc == mda->crtc[6]) 
                                mda->dispon=0;
                        if (oldvc == mda->crtc[4])
                        {
//                                printf("Display over at %i\n",displine);
                                mda->vc = 0;
                                mda->vadj = mda->crtc[5];
                                if (!mda->vadj) mda->dispon = 1;
                                if (!mda->vadj) mda->ma = mda->maback = (mda->crtc[13] | (mda->crtc[12] << 8)) & 0x3fff;
                                if ((mda->crtc[10] & 0x60) == 0x20) mda->cursoron = 0;
                                else                                mda->cursoron = mda->blink & 16;
                        }
                        if (mda->vc == mda->crtc[7])
                        {
                                mda->dispon = 0;
                                mda->displine = 0;
                                mda->vsynctime = 16;
                                if (mda->crtc[7])
                                {
//                                        printf("Lastline %i Firstline %i  %i\n",lastline,firstline,lastline-firstline);
                                        x = mda->crtc[1] * 8;
                                        mda->lastline++;
                                        if (x != xsize || (mda->lastline - mda->firstline) != ysize)
                                        {
                                                xsize = x;
                                                ysize = mda->lastline - mda->firstline;
//                                                printf("Resize to %i,%i - R1 %i\n",xsize,ysize,crtcm[1]);
                                                if (xsize < 64) xsize = 656;
                                                if (ysize < 32) ysize = 200;
                                                updatewindowsize(xsize, ysize << 1);
                                        }

                                        video_blit_memtoscreen(0, mda->firstline, 0, ysize, xsize, ysize);

                                        frames++;
                                        video_res_x = mda->crtc[1];
                                        video_res_y = mda->crtc[6];
                                        video_bpp = 0;
                                }
                                mda->firstline = 1000;
                                mda->lastline = 0;
                                mda->blink++;
                        }
                }
                else
                {
                        mda->sc++;
                        mda->sc &= 31;
                        mda->ma = mda->maback;
                }
                if ((mda->sc == (mda->crtc[10] & 31) || ((mda->crtc[8] & 3) == 3 && mda->sc == ((mda->crtc[10] & 31) >> 1))))
                {
                        mda->con = 1;
//                        printf("Cursor on - %02X %02X %02X\n",crtcm[8],crtcm[10],crtcm[11]);
                }
        }
}

void lcdc_poll(pc200_t *pc200)
{
        cga_t *cga = &pc200->cga;
        int drawcursor;
        int x, c;
        int oldvc;
        uint8_t chr, attr;
        uint16_t dat;
        int oldsc;
        uint16_t ca;
        int blink;

	ca = (cga->crtc[15] | (cga->crtc[14] << 8)) & 0x3fff;

        if (!cga->linepos)
        {
                timer_advance_u64(&pc200->timer, cga->dispofftime);
                cga->cgastat |= 1;
                cga->linepos = 1;
                oldsc = cga->sc;
                if ((cga->crtc[8] & 3) == 3) 
                   cga->sc = ((cga->sc << 1) + cga->oddeven) & 7;
                if (cga->cgadispon)
                {
                        if (cga->displine < cga->firstline)
                        {
                                cga->firstline = cga->displine;
                                video_wait_for_buffer();
//                                printf("Firstline %i\n",firstline);
                        }
                        cga->lastline = cga->displine;
                        
                        if (cga->cgamode & 1)
                        {
                                for (x = 0; x < cga->crtc[1]; x++)
                                {
                                        chr = cga->charbuffer[x << 1];
                                        attr = cga->charbuffer[(x << 1) + 1];
                                        drawcursor = ((cga->ma == ca) && cga->con && cga->cursoron);
                                	blink = ((cga->cgablink & 16) && (cga->cgamode & 0x20) && (attr & 0x80) && !drawcursor);
					lcd_draw_char_80(pc200, &((uint32_t *)(buffer32->line[cga->displine]))[x * 8], chr, attr, drawcursor, blink, cga->sc, cga->cgamode & 0x40, cga->cgamode);
                                        cga->ma++;
                                }
                        }
                        else if (!(cga->cgamode & 2))
                        {
                                for (x = 0; x < cga->crtc[1]; x++)
                                {
                                        chr  = cga->vram[((cga->ma << 1) & 0x3fff)];
                                        attr = cga->vram[(((cga->ma << 1) + 1) & 0x3fff)];
                                        drawcursor = ((cga->ma == ca) && cga->con && cga->cursoron);
                                	blink = ((cga->cgablink & 16) && (cga->cgamode & 0x20) && (attr & 0x80) && !drawcursor);
					lcd_draw_char_40(pc200, &((uint32_t *)(buffer32->line[cga->displine]))[x * 16], chr, attr, drawcursor, blink, cga->sc, cga->cgamode);
                                        cga->ma++;
                                }
                        }
                        else	/* Graphics mode */
                        {
                                for (x = 0; x < cga->crtc[1]; x++)
                                {
                                        dat = (cga->vram[((cga->ma << 1) & 0x1fff) + ((cga->sc & 1) * 0x2000)] << 8) | cga->vram[((cga->ma << 1) & 0x1fff) + ((cga->sc & 1) * 0x2000) + 1];
                                        cga->ma++;
                                        for (c = 0; c < 16; c++)
                                        {
                                                ((uint32_t *)buffer32->line[cga->displine])[(x << 4) + c] = (dat & 0x8000) ? blue : green;
                                                dat <<= 1;
                                        }
                                }
                        }
                }
                else
                {
                        if (cga->cgamode & 1) hline(buffer32, 0, cga->displine, (cga->crtc[1] << 3), green);
                        else                  hline(buffer32, 0, cga->displine, (cga->crtc[1] << 4), green);
                }

                if (cga->cgamode & 1) x = (cga->crtc[1] << 3);
                else                  x = (cga->crtc[1] << 4);

                cga->sc = oldsc;
                if (cga->vc == cga->crtc[7] && !cga->sc)
                   cga->cgastat |= 8;
                cga->displine++;
                if (cga->displine >= 360) 
                        cga->displine = 0;
        }
        else
        {
                timer_advance_u64(&pc200->timer, cga->dispontime);
                cga->linepos = 0;
                if (cga->vsynctime)
                {
                        cga->vsynctime--;
                        if (!cga->vsynctime)
                           cga->cgastat &= ~8;
                }
                if (cga->sc == (cga->crtc[11] & 31) || ((cga->crtc[8] & 3) == 3 && cga->sc == ((cga->crtc[11] & 31) >> 1))) 
                { 
                        cga->con = 0; 
                        cga->coff = 1; 
                }
                if ((cga->crtc[8] & 3) == 3 && cga->sc == (cga->crtc[9] >> 1))
                   cga->maback = cga->ma;
                if (cga->vadj)
                {
                        cga->sc++;
                        cga->sc &= 31;
                        cga->ma = cga->maback;
                        cga->vadj--;
                        if (!cga->vadj)
                        {
                                cga->cgadispon = 1;
                                cga->ma = cga->maback = (cga->crtc[13] | (cga->crtc[12] << 8)) & 0x3fff;
                                cga->sc = 0;
                        }
                }
                else if (cga->sc == cga->crtc[9])
                {
                        cga->maback = cga->ma;
                        cga->sc = 0;
                        oldvc = cga->vc;
                        cga->vc++;
                        cga->vc &= 127;

                        if (cga->vc == cga->crtc[6]) 
                                cga->cgadispon = 0;

                        if (oldvc == cga->crtc[4])
                        {
                                cga->vc = 0;
                                cga->vadj = cga->crtc[5];
                                if (!cga->vadj) cga->cgadispon = 1;
                                if (!cga->vadj) cga->ma = cga->maback = (cga->crtc[13] | (cga->crtc[12] << 8)) & 0x3fff;
                                if ((cga->crtc[10] & 0x60) == 0x20) cga->cursoron = 0;
                                else                                cga->cursoron = cga->cgablink & 8;
                        }

                        if (cga->vc == cga->crtc[7])
                        {
                                cga->cgadispon = 0;
                                cga->displine = 0;
                                cga->vsynctime = 16;
                                if (cga->crtc[7])
                                {
                                        if (cga->cgamode & 1) x = (cga->crtc[1] << 3);
                                        else                  x = (cga->crtc[1] << 4);
                                        cga->lastline++;
                                        if (x != xsize || (cga->lastline - cga->firstline) != ysize)
                                        {
                                                xsize = x;
                                                ysize = cga->lastline - cga->firstline;
                                                if (xsize < 64) xsize = 656;
                                                if (ysize < 32) ysize = 200;
                                                updatewindowsize(xsize, (ysize << 1));
                                        }
                                        
                                        video_blit_memtoscreen(0, cga->firstline, 0, (cga->lastline - cga->firstline), xsize, (cga->lastline - cga->firstline));
                                        frames++;

                                        video_res_x = xsize - 16;
                                        video_res_y = ysize;
                                        if (cga->cgamode & 1)
                                        {
                                                video_res_x /= 8;
                                                video_res_y /= cga->crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(cga->cgamode & 2))
                                        {
                                                video_res_x /= 16;
                                                video_res_y /= cga->crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(cga->cgamode & 16))
                                        {
                                                video_res_x /= 2;
                                                video_bpp = 2;
                                        }
                                        else
                                        {
                                                video_bpp = 1;
                                        }
                                }
                                cga->firstline = 1000;
                                cga->lastline = 0;
                                cga->cgablink++;
                                cga->oddeven ^= 1;
                        }
                }
                else
                {
                        cga->sc++;
                        cga->sc &= 31;
                        cga->ma = cga->maback;
                }
                if (cga->cgadispon)
                        cga->cgastat &= ~1;
                if ((cga->sc == (cga->crtc[10] & 31) || ((cga->crtc[8] & 3) == 3 && cga->sc == ((cga->crtc[10] & 31) >> 1)))) 
                        cga->con = 1;
                if (cga->cgadispon && (cga->cgamode & 1))
                {
                        for (x = 0; x < (cga->crtc[1] << 1); x++)
                            cga->charbuffer[x] = cga->vram[(((cga->ma << 1) + x) & 0x3fff)];
                }
        }

}

void pc200_poll(void *p)
{
        pc200_t *pc200 = (pc200_t *)p;

	switch (pc200->emulation)
	{
//		case PC200_CGA:
//		case PC200_TV:
//		pc200->cga.vidtime = pc200->vidtime;
//		cga_poll(&pc200->cga);
//		pc200->vidtime = pc200->cga.vidtime;
//		return;

//		case PC200_MDA:
//		pc200->mda.vidtime = pc200->vidtime;
//		mda_poll(&pc200->mda);
//		pc200->vidtime = pc200->mda.vidtime;
//		return;

		case PC200_LCDM:
		lcdm_poll(pc200);
		return;
		case PC200_LCDC:	
		lcdc_poll(pc200);
		return;
	}
}

void *pc200_init()
{
	int display_type, contrast;

        pc200_t *pc200 = malloc(sizeof(pc200_t));

        memset(pc200, 0, sizeof(pc200_t));

        pc200->emulation = device_get_config_int("video_emulation");
        display_type = device_get_config_int("display_type");
        contrast = device_get_config_int("contrast");

	/* Default to CGA */
	pc200->dipswitches = 0x10;

	/* DIP switches for PC200. Switches 2,3 give video emulation.
	 * Switch 1 is 'swap floppy drives' (not implemented) */
	if (romset == ROM_PC200) switch (pc200->emulation)
	{
		case PC200_CGA:  pc200->dipswitches = 0x10; break;
		case PC200_MDA:  pc200->dipswitches = 0x30; break;
		case PC200_TV:   pc200->dipswitches = 0x00; break;
		/* The other combination is 'IDA disabled' (0x20) - see
		 * amstrad.c */
       	} 
	/* DIP switches for PPC512. Switch 1 is CRT/LCD. Switch 2
	 * is MDA / CGA. Switch 3 disables IDA, not implemented. */
	else switch (pc200->emulation)
	{
		case PC200_CGA:  pc200->dipswitches = 0x08; break;
		case PC200_MDA:  pc200->dipswitches = 0x18; break;
		case PC200_LCDC: pc200->dipswitches = 0x00; break;
		case PC200_LCDM: pc200->dipswitches = 0x10; break;
       	} 
	/* Flag whether the card is behaving like a CGA or an MDA, so that
	 * DIP switches are reported correctly */
	pc200_is_mda = (pc200->emulation == PC200_MDA || 
			pc200->emulation == PC200_LCDM);
	PC200LOG(("pc200: DIP switches = %02x\n", pc200->dipswitches));

        pc200->cga.vram = pc200->mda.vram = malloc(0x4000);
        cga_init(&pc200->cga);
	mda_init(&pc200->mda);

	/* Attribute 8 is white on black (on a real MDA it's black on black) */
	mda_setcol(0x08, 0, 1, 15);
	mda_setcol(0x88, 0, 1, 15);
	/* Attribute 64 is black on black (on a real MDA it's white on black) */
	mda_setcol(0x40, 0, 1, 0);
	mda_setcol(0xC0, 0, 1, 0);

	pc200->cga.fontbase = (device_get_config_int("codepage") & 3) * 256;

//        pclog("pc200=%p pc200->timer=%p pc200->cga=%p pc200->mda=%p pc200->mda->timer=%p\n", pc200, &pc200->timer, &pc200->cga, &pc200->mda, &pc200->mda->timer);
        timer_add(&pc200->timer, pc200_poll, pc200, 1);
        mem_mapping_add(&pc200->mda_mapping, 0xb0000, 0x08000, mda_read, NULL, NULL, mda_write, NULL, NULL,  NULL, 0, &pc200->mda);
        mem_mapping_add(&pc200->cga_mapping, 0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL,  NULL, 0, &pc200->cga);
        io_sethandler(0x03d0, 0x0010, pc200_in, NULL, NULL, pc200_out, NULL, NULL, pc200);
        io_sethandler(0x03b0, 0x000c, pc200_in, NULL, NULL, pc200_out, NULL, NULL, pc200);

	green = makecol(0x1C, 0x71, 0x31);
	blue = makecol(0x0f, 0x21, 0x3f);
        cgapal_rebuild(display_type, contrast);
	set_lcd_cols(0);

        timer_disable(&pc200->cga.timer);
        timer_disable(&pc200->mda.timer);
        timer_disable(&pc200->timer);
	if (pc200->emulation == PC200_CGA || pc200->emulation == PC200_TV)
                timer_enable(&pc200->cga.timer);
	else if (pc200->emulation == PC200_MDA)
                timer_enable(&pc200->mda.timer);
        else
                timer_enable(&pc200->timer);

        return pc200;
}

void pc200_close(void *p)
{
        pc200_t *pc200 = (pc200_t *)p;

        free(pc200->cga.vram);
        free(pc200);
}

void pc200_speed_changed(void *p)
{
        pc200_t *pc200 = (pc200_t *)p;
        
        cga_recalctimings(&pc200->cga);
	mda_recalctimings(&pc200->mda);
}

device_config_t pc200_config[] =
{
	{
		.name = "video_emulation",
		.description = "Display type",
		.type = CONFIG_SELECTION,
		.selection = 
		{
			{
				.description = "CGA monitor",
				.value = PC200_CGA,
			},
			{
				.description = "MDA monitor",
				.value = PC200_MDA,
			},
			{
				.description = "Television",
				.value = PC200_TV,
			},
                        {
                                .description = ""
                        }
		},
		.default_int = PC200_CGA,
	},
        {
                .name = "display_type",
                .description = "Monitor type",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "RGB",
                                .value = DISPLAY_RGB
                        },
                        {
                                .description = "RGB (no brown)",
                                .value = DISPLAY_RGB_NO_BROWN
                        },
                        {
                                .description = "Green Monochrome",
                                .value = DISPLAY_GREEN
                        },
                        {
                                .description = "Amber Monochrome",
                                .value = DISPLAY_AMBER
                        },
                        {
                                .description = "White Monochrome",
                                .value = DISPLAY_WHITE
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = DISPLAY_RGB
        },
        {
                .name = "codepage",
                .description = "Hardware font",
                .type = CONFIG_SELECTION,
		.selection =
		{
			{
				.description = "US English",
				.value = 3
			},
			{
				.description = "Portugese",
				.value = 2
			},
			{
				.description = "Norwegian",
				.value = 1
			},
			{
				.description = "Greek",
				.value = 0
			},
			{
				.description = ""
			}
		},
                .default_int = 3
        },
        {
                .type = -1
        }
};


device_config_t ppc512_config[] =
{
	{
		.name = "video_emulation",
		.description = "Display type",
		.type = CONFIG_SELECTION,
		.selection = 
		{
			{
				.description = "CGA monitor",
				.value = PC200_CGA,
			},
			{
				.description = "MDA monitor",
				.value = PC200_MDA,
			},
			{
				.description = "LCD (CGA mode)",
				.value = PC200_LCDC,
			},
			{
				.description = "LCD (MDA mode)",
				.value = PC200_LCDM,
			},
                        {
                                .description = ""
                        }
		},
		.default_int = PC200_LCDC,
	},
        {
                .name = "display_type",
                .description = "External monitor",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "RGB",
                                .value = DISPLAY_RGB
                        },
                        {
                                .description = "RGB (no brown)",
                                .value = DISPLAY_RGB_NO_BROWN
                        },
                        {
                                .description = "Green Monochrome",
                                .value = DISPLAY_GREEN
                        },
                        {
                                .description = "Amber Monochrome",
                                .value = DISPLAY_AMBER
                        },
                        {
                                .description = "White Monochrome",
                                .value = DISPLAY_WHITE
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = DISPLAY_RGB
        },
        {
                .name = "codepage",
                .description = "Hardware font",
                .type = CONFIG_SELECTION,
		.selection =
		{
			{
				.description = "US English",
				.value = 3
			},
			{
				.description = "Portugese",
				.value = 2
			},
			{
				.description = "Norwegian",
				.value = 1
			},
			{
				.description = "Greek",
				.value = 0
			},
			{
				.description = ""
			}
		},
                .default_int = 3
        },
        {
                .type = -1
        }
};



device_t pc200_device =
{
        "Amstrad PC200 (video)",
        0,
        pc200_init,
        pc200_close,
        NULL,
        pc200_speed_changed,
        NULL,
        NULL,
	pc200_config
};

device_t ppc512_device =
{
        "Amstrad PPC512 (video)",
        0,
        pc200_init,
        pc200_close,
        NULL,
        pc200_speed_changed,
        NULL,
        NULL,
	ppc512_config
};

