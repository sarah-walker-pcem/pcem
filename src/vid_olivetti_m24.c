/*Olivetti M24 video emulation
  Essentially double-res CGA*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "timer.h"
#include "video.h"

void m24_recalctimings();

static uint8_t  m24_ctrl;
static uint32_t m24_base;

void m24_out(uint16_t addr, uint8_t val, void *priv)
{
        uint8_t old;
        switch (addr)
        {
                case 0x3D4:
                crtcreg = val & 31;
                return;
                case 0x3D5:
                old = crtc[crtcreg];
                crtc[crtcreg] = val & crtcmask[crtcreg];
                if (old != val)
                {
                        if (crtcreg < 0xE || crtcreg > 0x10)
                        {
                                fullchange = changeframecount;
                                m24_recalctimings();
                        }
                }
                return;
                case 0x3D8:
                cgamode = val;
                return;
                case 0x3D9:
                cgacol = val;
                return;
                case 0x3de:
                m24_ctrl = val;
                m24_base = (val & 0x08) ? 0x4000 : 0;
                return;
        }
}

uint8_t m24_in(uint16_t addr, void *priv)
{
        switch (addr)
        {
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
                case 0x3DA:
                return cgastat;
        }
        return 0xFF;
}

static uint8_t charbuffer[256];

void m24_write(uint32_t addr, uint8_t val, void *priv)
{
        vram[addr & 0x7FFF]=val;
        charbuffer[ ((int)(((dispontime - vidtime) * 2) / (CGACONST / 2))) & 0xfc] = val;
        charbuffer[(((int)(((dispontime - vidtime) * 2) / (CGACONST / 2))) & 0xfc) | 1] = val;        
}

uint8_t m24_read(uint32_t addr, void *priv)
{
        return vram[addr & 0x7FFF];
}

void m24_recalctimings()
{
	double _dispontime, _dispofftime;
        if (cgamode & 1)
        {
                disptime   = crtc[0] + 1;
                _dispontime = crtc[1];
        }
        else
        {
                disptime   = (crtc[0] + 1) << 1;
                _dispontime = crtc[1] << 1;
        }
        _dispofftime = disptime - _dispontime;
//        printf("%i %f %f %f  %i %i\n",cgamode&1,disptime,dispontime,dispofftime,crtc[0],crtc[1]);
        _dispontime  *= CGACONST / 2;
        _dispofftime *= CGACONST / 2;
//        printf("Timings - on %f off %f frame %f second %f\n",dispontime,dispofftime,(dispontime+dispofftime)*262.0,(dispontime+dispofftime)*262.0*59.92);
	dispontime = (int)(_dispontime * (1 << TIMER_SHIFT));
	dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));
}

static int linepos,displine;
static int sc,vc;
static int cgadispon;
static int con,coff,cursoron,cgablink;
static int vsynctime,vadj;
static int m24_lineff = 0;
static uint16_t ma,maback;

void m24_poll()
{
        uint16_t ca=(crtc[15]|(crtc[14]<<8))&0x3FFF;
        int drawcursor;
        int x,c;
        int oldvc;
        uint8_t chr,attr;
        uint16_t dat,dat2;
        int cols[4];
        int col;
        int oldsc;
        if (!linepos)
        {
//                pclog("Line poll  %i %i %i %i - %04X %i %i %i\n", m24_lineff, vc, sc, vadj, ma, firstline, lastline, displine);
                vidtime+=dispofftime;
                cgastat|=1;
                linepos=1;
                oldsc=sc;
                if ((crtc[8]&3)==3) sc=(sc<<1)&7;
                if (cgadispon)
                {
                        if (displine<firstline)
                        {
                                firstline=displine;
//                                printf("Firstline %i\n",firstline);
                        }
                        lastline=displine;
                        for (c=0;c<8;c++)
                        {
                                if ((cgamode&0x12)==0x12)
                                {
                                        buffer->line[displine][c]=0;
                                        if (cgamode&1) buffer->line[displine][c+(crtc[1]<<3)+8]=0;
                                        else           buffer->line[displine][c+(crtc[1]<<4)+8]=0;
                                }
                                else
                                {
                                        buffer->line[displine][c]=(cgacol&15)+16;
                                        if (cgamode&1) buffer->line[displine][c+(crtc[1]<<3)+8]=(cgacol&15)+16;
                                        else           buffer->line[displine][c+(crtc[1]<<4)+8]=(cgacol&15)+16;
                                }
                        }
                        if (cgamode&1)
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        chr  = charbuffer[ x << 1];
                                        attr = charbuffer[(x << 1) + 1];
                                        drawcursor=((ma==ca) && con && cursoron);
                                        if (cgamode&0x20)
                                        {
                                                cols[1]=(attr&15)+16;
                                                cols[0]=((attr>>4)&7)+16;
                                                if ((cgablink&16) && (attr&0x80) && !drawcursor) cols[1]=cols[0];
                                        }
                                        else
                                        {
                                                cols[1]=(attr&15)+16;
                                                cols[0]=(attr>>4)+16;
                                        }
                                        if (drawcursor)
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<3)+c+8]=cols[(fontdatm[chr][((sc & 7) << 1) | m24_lineff]&(1<<(c^7)))?1:0]^15;
                                        }
                                        else
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<3)+c+8]=cols[(fontdatm[chr][((sc & 7) << 1) | m24_lineff]&(1<<(c^7)))?1:0];
                                        }
                                        ma++;
                                }
                        }
                        else if (!(cgamode&2))
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        chr=vram[((ma<<1)&0x3FFF) + m24_base];
                                        attr=vram[(((ma<<1)+1)&0x3FFF) + m24_base];
                                        drawcursor=((ma==ca) && con && cursoron);
                                        if (cgamode&0x20)
                                        {
                                                cols[1]=(attr&15)+16;
                                                cols[0]=((attr>>4)&7)+16;
                                                if ((cgablink&16) && (attr&0x80)) cols[1]=cols[0];
                                        }
                                        else
                                        {
                                                cols[1]=(attr&15)+16;
                                                cols[0]=(attr>>4)+16;
                                        }
                                        ma++;
                                        if (drawcursor)
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<4)+(c<<1)+8]=buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[(fontdatm[chr][((sc & 7) << 1) | m24_lineff]&(1<<(c^7)))?1:0]^15;
                                        }
                                        else
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<4)+(c<<1)+8]=buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[(fontdatm[chr][((sc & 7) << 1) | m24_lineff]&(1<<(c^7)))?1:0];
                                        }
                                }
                        }
                        else if (!(cgamode&16))
                        {
                                cols[0]=(cgacol&15)|16;
                                col=(cgacol&16)?24:16;
                                if (cgamode&4)
                                {
                                        cols[1]=col|3;
                                        cols[2]=col|4;
                                        cols[3]=col|7;
                                }
                                else if (cgacol&32)
                                {
                                        cols[1]=col|3;
                                        cols[2]=col|5;
                                        cols[3]=col|7;
                                }
                                else
                                {
                                        cols[1]=col|2;
                                        cols[2]=col|4;
                                        cols[3]=col|6;
                                }
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000) + m24_base]<<8)|vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)+1 + m24_base];
                                        ma++;
                                        for (c=0;c<8;c++)
                                        {
                                                buffer->line[displine][(x<<4)+(c<<1)+8]=
                                                  buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[dat>>14];
                                                dat<<=2;
                                        }
                                }
                        }
                        else
                        {
                                
                                if (m24_ctrl & 1)
                                {                                        
                                        dat2 = ((sc & 1) * 0x4000) | (m24_lineff * 0x2000);
                                        cols[0]=0; cols[1]=/*(cgacol&15)*/15+16;
                                }
                                else
                                {
                                        dat2 =  (sc & 1) * 0x2000;
                                        cols[0]=0; cols[1]=(cgacol&15)+16;
                                }
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(vram[((ma<<1)&0x1FFF) + dat2]<<8) | vram[((ma<<1)&0x1FFF) + dat2 + 1];
                                        ma++;
                                        for (c=0;c<16;c++)
                                        {
                                                buffer->line[displine][(x<<4)+c+8]=cols[dat>>15];
                                                dat<<=1;
                                        }
                                }
                        }
                }
                else
                {
                        cols[0]=((cgamode&0x12)==0x12)?0:(cgacol&15)+16;
                        if (cgamode&1) hline(buffer,0,displine,(crtc[1]<<3)+16,cols[0]);
                        else           hline(buffer,0,displine,(crtc[1]<<4)+16,cols[0]);
                }

                if (cgamode&1) x=(crtc[1]<<3)+16;
                else           x=(crtc[1]<<4)+16;

                sc=oldsc;
                if (vc==crtc[7] && !sc)
                   cgastat|=8;
                displine++;
                if (displine>=720) displine=0;
        }
        else
        {
//                pclog("Line poll  %i %i %i %i\n", m24_lineff, vc, sc, vadj);
                vidtime+=dispontime;
                if (cgadispon) cgastat&=~1;
                linepos=0;
                m24_lineff ^= 1;
                if (m24_lineff)
                {
                        ma = maback;
                }                
                else
                {
                        if (vsynctime)
                        {
                                vsynctime--;
                                if (!vsynctime)
                                   cgastat&=~8;
                        }
                        if (sc==(crtc[11]&31) || ((crtc[8]&3)==3 && sc==((crtc[11]&31)>>1))) { con=0; coff=1; }
                        if (vadj)
                        {
                                sc++;
                                sc&=31;
                                ma=maback;
                                vadj--;
                                if (!vadj)
                                {
                                        cgadispon=1;
                                        ma=maback=(crtc[13]|(crtc[12]<<8))&0x3FFF;
                                        sc=0;
                                }
                        }
                        else if (sc==crtc[9] || ((crtc[8]&3)==3 && sc==(crtc[9]>>1)))
                        {
                                maback=ma;
                                sc=0;
                                oldvc=vc;
                                vc++;
                                vc&=127;

                                if (vc==crtc[6]) 
                                   cgadispon=0;
                                   
                                if (oldvc==crtc[4])
                                {
                                        vc=0;
                                        vadj=crtc[5];
                                        if (!vadj) cgadispon=1;
                                        if (!vadj) ma=maback=(crtc[13]|(crtc[12]<<8))&0x3FFF;
                                        if ((crtc[10]&0x60)==0x20) cursoron=0;
                                        else                       cursoron=cgablink&16;
                                }

                                if (vc==crtc[7])
                                {
                                        cgadispon=0;
                                        displine=0;
                                        vsynctime=(crtc[3]>>4)+1;
                                        if (crtc[7])
                                        {
                                                if (cgamode&1) x=(crtc[1]<<3)+16;
                                                else           x=(crtc[1]<<4)+16;
                                                lastline++;
                                                if (x!=xsize || (lastline-firstline)!=ysize)
                                                {
                                                        xsize=x;
                                                        ysize=lastline-firstline;
                                                        if (xsize<64) xsize=656;
                                                        if (ysize<32) ysize=200;
                                                        updatewindowsize(xsize, ysize + 16);
                                                }
startblit();
                                                video_blit_memtoscreen_8(0, firstline - 8, xsize, (lastline - firstline) + 16);
                                                if (readflash) rectfill(screen,winsizex-40,8,winsizex-8,14,0xFFFFFFFF);
                                                readflash=0;
                                                frames++;
endblit();
                                                video_res_x = xsize - 16;
                                                video_res_y = ysize;
                                                if (cgamode & 1)
                                                {
                                                        video_res_x /= 8;
                                                        video_res_y /= (crtc[9] + 1) * 2;
                                                        video_bpp = 0;
                                                }
                                                else if (!(cgamode & 2))
                                                {
                                                        video_res_x /= 16;
                                                        video_res_y /= (crtc[9] + 1) * 2;
                                                        video_bpp = 0;
                                                }
                                                else if (!(cgamode&16))
                                                {
                                                        video_res_x /= 2;
                                                        video_res_y /= 2;
                                                        video_bpp = 2;
                                                }
                                                else if (!(m24_ctrl & 1))
                                                {
                                                        video_res_y /= 2;
                                                        video_bpp = 1;
                                                }
                                        }
                                        firstline=1000;
                                        lastline=0;
                                        cgablink++;
                                }
                        }
                        else
                        {
                                sc++;
                                sc&=31;
                                ma=maback;
                        }
                        if ((sc==(crtc[10]&31) || ((crtc[8]&3)==3 && sc==((crtc[10]&31)>>1)))) con=1;
                }
                if (cgadispon && (cgamode&1))
                {
                        for (x=0;x<(crtc[1]<<1);x++)
                            charbuffer[x]=vram[(((ma<<1)+x)&0x3FFF) + m24_base];
                }
        }
}


int m24_init()
{
        mem_sethandler(0xb8000, 0x08000, m24_read, NULL, NULL, m24_write, NULL, NULL,  NULL);
        return 0;
}

GFXCARD vid_m24 =
{
        m24_init,
        /*IO at 3Cx/3Dx*/
        m24_out,
        m24_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        m24_poll,
        m24_recalctimings,

        video_write_null,
        video_write_null,
        m24_write,

        video_read_null,
        video_read_null,
        m24_read
};
