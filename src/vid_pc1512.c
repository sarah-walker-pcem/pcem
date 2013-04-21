/*PC1512 CGA emulation

  The PC1512 extends CGA with a bit-planar 640x200x16 mode.
  
  Most CRTC registers are fixed.
  
  The Technical Reference Manual lists the video waitstate time as between 12 
  and 46 cycles. PCem currently always uses the lower number.*/

#include "ibm.h"
#include "video.h"
#include "vid_cga.h"

static uint8_t pc1512_plane_write,pc1512_plane_read,pc1512_border;

void pc1512_recalctimings();

void pc1512_out(uint16_t addr, uint8_t val)
{
        uint8_t old;
//        pclog("PC1512 out %04X %02X %04X:%04X\n",addr,val,CS,pc);
        switch (addr)
        {
                case 0x3D4:
                crtcreg=val&31;
                return;
                case 0x3D5:
                old=crtc[crtcreg];
                crtc[crtcreg]=val&crtcmask[crtcreg];
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                pc1512_recalctimings();
                        }
                }
                return;
                case 0x3D8:
                if ((val&0x12)==0x12 && (cgamode&0x12)!=0x12)
                {
                        pc1512_plane_write=0xF;
                        pc1512_plane_read =0;
                }
                cgamode=val;
                return;
                case 0x3D9:
                cgacol=val;
                return;
                case 0x3DD:
                pc1512_plane_write=val;
                return;
                case 0x3DE:
                pc1512_plane_read=val&3;
                return;
                case 0x3DF:
                pc1512_border=val;
                return;
        }
}

uint8_t pc1512_in(uint16_t addr)
{
//        pclog("PC1512 in %04X %02X %04X:%04X\n",addr,CS,pc);
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

void pc1512_write(uint32_t addr, uint8_t val)
{
/*        if (CS==0x023E && pc==0x524E)
        {
                dumpregs();
                exit(-1);
        }
       pclog("PC1512 write %08X %02X %02X %01X  %04X:%04X\n",addr,val,cgamode&0x12,pc1512_plane_write,CS,pc);*/
        cycles-=12;
        addr&=0x7FFF;
        if ((cgamode&0x12)==0x12)
        {
                if (pc1512_plane_write&1) vram[addr]=val;
                if (pc1512_plane_write&2) vram[addr|0x10000]=val;
                if (pc1512_plane_write&4) vram[addr|0x20000]=val;
                if (pc1512_plane_write&8) vram[addr|0x30000]=val;
        }
        else
           vram[addr]=val;
}

uint8_t pc1512_read(uint32_t addr)
{
//        pclog("PC1512 read %08X %02X %01X\n",addr,cgamode&0x12,pc1512_plane_read);
        cycles-=12;
        addr&=0x7FFF;
        if ((cgamode&0x12)==0x12)
           return vram[addr|(pc1512_plane_read<<16)];
        return vram[addr];
}


static int linepos,displine;
static int sc,vc;
static int cgadispon;
static int con,coff,cursoron,cgablink;
static int vsynctime,vadj;
static uint16_t ma,maback,ca;

static int ntsc_col[8][8]=
{
        {0,0,0,0,0,0,0,0}, /*Black*/
        {0,0,1,1,1,1,0,0}, /*Blue*/
        {1,0,0,0,0,1,1,1}, /*Green*/
        {0,0,0,0,1,1,1,1}, /*Cyan*/
        {1,1,1,1,0,0,0,0}, /*Red*/
        {0,1,1,1,1,0,0,0}, /*Magenta*/
        {1,1,0,0,0,0,1,1}, /*Yellow*/
        {1,1,1,1,1,1,1,1}  /*White*/
};

int i_filt[8],q_filt[8];


void pc1512_recalctimings()
{
        disptime = 128; /*Fixed on PC1512*/
        dispontime = 80;
        dispofftime=disptime-dispontime;
//        printf("%i %f %f %f  %i %i\n",cgamode&1,disptime,dispontime,dispofftime,crtc[0],crtc[1]);
        dispontime*=CGACONST;
        dispofftime*=CGACONST;
//        printf("Timings - on %f off %f frame %f second %f\n",dispontime,dispofftime,(dispontime+dispofftime)*262.0,(dispontime+dispofftime)*262.0*59.92);
}

void pc1512_poll()
{
        uint16_t ca=(crtc[15]|(crtc[14]<<8))&0x3FFF;
        int drawcursor;
        int x,c;
        int oldvc;
        uint8_t chr,attr;
        uint16_t dat,dat2,dat3,dat4;
        int cols[4];
        int col;
        int oldsc;

        if (!linepos)
        {
                vidtime+=dispofftime;
                cgastat|=1;
                linepos=1;
                oldsc=sc;
                if (cgadispon)
                {
                        if (displine<firstline) firstline=displine;
                        lastline=displine;
                        for (c=0;c<8;c++)
                        {
                                if ((cgamode&0x12)==0x12)
                                {
                                        buffer->line[displine][c]=(pc1512_border&15)+16;
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
                                for (x = 0; x < 80; x++)
                                {
                                        chr=vram[((ma<<1)&0x3FFF)];
                                        attr=vram[(((ma<<1)+1)&0x3FFF)];
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
                                                    buffer->line[displine][(x<<3)+c+8]=cols[(fontdat[chr][sc&7]&(1<<(c^7)))?1:0]^15;
                                        }
                                        else
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<3)+c+8]=cols[(fontdat[chr][sc&7]&(1<<(c^7)))?1:0];
                                        }
                                        ma++;
                                }
                        }
                        else if (!(cgamode&2))
                        {
                                for (x = 0; x < 40; x++)
                                {
                                        chr=vram[((ma<<1)&0x3FFF)];
                                        attr=vram[(((ma<<1)+1)&0x3FFF)];
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
                                                    buffer->line[displine][(x<<4)+(c<<1)+8]=buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[(fontdat[chr][sc&7]&(1<<(c^7)))?1:0]^15;
                                        }
                                        else
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<4)+(c<<1)+8]=buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[(fontdat[chr][sc&7]&(1<<(c^7)))?1:0];
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
                                for (x = 0; x < 40; x++)
                                {
                                        dat=(vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)]<<8)|vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)+1];
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
                                for (x = 0; x < 40; x++)
                                {
                                        ca=((ma<<1)&0x1FFF)+((sc&1)*0x2000);
                                        dat=(vram[ca]<<8)|vram[ca+1];
                                        dat2=(vram[ca+0x10000]<<8)|vram[ca+0x10001];
                                        dat3=(vram[ca+0x20000]<<8)|vram[ca+0x20001];
                                        dat4=(vram[ca+0x30000]<<8)|vram[ca+0x30001];

                                        ma++;
                                        for (c=0;c<16;c++)
                                        {
                                                buffer->line[displine][(x<<4)+c+8]=(((dat>>15)|((dat2>>15)<<1)|((dat3>>15)<<2)|((dat4>>15)<<3))&(cgacol&15))+16;
                                                dat<<=1;
                                                dat2<<=1;
                                                dat3<<=1;
                                                dat4<<=1;
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

                sc=oldsc;
                if (vsynctime)
                   cgastat|=8;
                displine++;
                if (displine>=360) displine=0;
//                pclog("Line %i %i %i %i  %i %i\n",displine,cgadispon,firstline,lastline,vc,sc);
        }
        else
        {
                vidtime+=dispontime;
                if ((lastline-firstline)==199) cgadispon=0; /*Amstrad PC1512 always displays 200 lines, regardless of CRTC settings*/
                if (cgadispon) cgastat&=~1;
                linepos=0;
                if (vsynctime)
                {
                        vsynctime--;
                        if (!vsynctime)
                           cgastat&=~8;
                }
                if (sc==(crtc[11]&31)) { con=0; coff=1; }
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
                else if (sc==crtc[9])
                {
                        maback=ma;
                        sc=0;
                        oldvc=vc;
                        vc++;
                        vc&=127;

                        if (displine == 32)//oldvc == (cgamode & 2) ? 127 : 31)
                        {
                                vc=0;
                                vadj=6;
                                if ((crtc[10]&0x60)==0x20) cursoron=0;
                                else                       cursoron=cgablink&16;
                        }

                        if (displine >= 262)//vc == (cgamode & 2) ? 111 : 27)
                        {
                                cgadispon=0;
                                displine=0;
                                vsynctime=46;

                                        if (cgamode&1) x=(crtc[1]<<3)+16;
                                        else           x=(crtc[1]<<4)+16;
                                        x = 640 + 16;
                                        lastline++;
                                        if (x!=xsize || (lastline-firstline)!=ysize)
                                        {
                                                xsize=x;
                                                ysize=lastline-firstline;
                                                if (xsize<64) xsize=656;
                                                if (ysize<32) ysize=200;
                                                updatewindowsize(xsize,(ysize<<1)+16);
                                        }
startblit();
                                        video_blit_memtoscreen_8(0, firstline - 4, xsize, (lastline - firstline) + 8);
//                                        blit(buffer,vbuf,0,firstline-4,0,0,xsize,(lastline-firstline)+8+1);
//                                        if (vid_resize) stretch_blit(vbuf,screen,0,0,xsize,(lastline-firstline)+8+1,0,0,winsizex,winsizey);
//                                        else            stretch_blit(vbuf,screen,0,0,xsize,(lastline-firstline)+8+1,0,0,xsize,((lastline-firstline)<<1)+16+2);
//                                        if (readflash) rectfill(screen,winsizex-40,8,winsizex-8,14,0xFFFFFFFF);
//                                        readflash=0;
endblit();
                                        video_res_x = xsize - 16;
                                        video_res_y = ysize;
                                        if (cgamode & 1)
                                        {
                                                video_res_x /= 8;
                                                video_res_y /= crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(cgamode & 2))
                                        {
                                                video_res_x /= 16;
                                                video_res_y /= crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(cgamode&16))
                                        {
                                                video_res_x /= 2;
                                                video_bpp = 2;
                                        }
                                        else
                                        {
                                                video_bpp = 4;
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
                if (sc==(crtc[10]&31)) con=1;
        }
}

int pc1512_init()
{
        mem_sethandler(0xb8000, 0x08000, pc1512_read, NULL, NULL, pc1512_write, NULL, NULL);
        return 0;
}

GFXCARD vid_pc1512 =
{
        pc1512_init,
        /*IO at 3Cx/3Dx*/
        pc1512_out,
        pc1512_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        pc1512_poll,
        pc1512_recalctimings,

        video_write_null,
        video_write_null,
        pc1512_write,

        video_read_null,
        video_read_null,
        pc1512_read
};
