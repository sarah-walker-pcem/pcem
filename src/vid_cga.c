/*CGA emulation*/
#include "ibm.h"
#include "video.h"

void cga_out(uint16_t addr, uint8_t val)
{
        uint8_t old;
//        pclog("CGA_OUT %04X %02X\n", addr, val);
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
                                cga_recalctimings();
                        }
                }
                return;
                case 0x3D8:
                cgamode=val;
                return;
                case 0x3D9:
                cgacol=val;
                return;
        }
}

uint8_t cga_in(uint16_t addr)
{
//        pclog("CGA_IN %04X\n", addr);
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

extern uint8_t charbuffer[256];

void cga_write(uint32_t addr, uint8_t val)
{
//        pclog("CGA_WRITE %04X %02X\n", addr, val);
        vram[addr&0x3FFF]=val;
        charbuffer[ ((int)(((dispontime - vidtime) * 2) / CGACONST)) & 0xfc] = val;
        charbuffer[(((int)(((dispontime - vidtime) * 2) / CGACONST)) & 0xfc) | 1] = val;
        cycles -= 4;
}

uint8_t cga_read(uint32_t addr)
{
        cycles -= 4;        
        charbuffer[ ((int)(((dispontime - vidtime) * 2) / CGACONST)) & 0xfc] = vram[addr&0x3FFF];
        charbuffer[(((int)(((dispontime - vidtime) * 2) / CGACONST)) & 0xfc) | 1] = vram[addr&0x3FFF];
//        pclog("CGA_READ %04X\n", addr);
        return vram[addr&0x3FFF];
}

void cga_recalctimings()
{
        pclog("Recalc - %i %i %i\n", crtc[0], crtc[1], cgamode & 1);
        if (cgamode&1)
        {
                disptime=crtc[0]+1;
                dispontime=crtc[1];
        }
        else
        {
                disptime=(crtc[0]+1)<<1;
                dispontime=crtc[1]<<1;
        }
        dispofftime=disptime-dispontime;
//        printf("%i %f %f %f  %i %i\n",cgamode&1,disptime,dispontime,dispofftime,crtc[0],crtc[1]);
        dispontime*=CGACONST;
        dispofftime*=CGACONST;
//        printf("Timings - on %f off %f frame %f second %f\n",dispontime,dispofftime,(dispontime+dispofftime)*262.0,(dispontime+dispofftime)*262.0*59.92);
}

static int linepos,displine;
static int sc,vc;
static int cgadispon;
static int con,coff,cursoron,cgablink;
static int vsynctime,vadj;
static uint16_t ma,maback,ca;
static int oddeven = 0;

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


void cga_poll()
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
        int y_buf[8]={0,0,0,0,0,0,0,0},y_val,y_tot;
        int i_buf[8]={0,0,0,0,0,0,0,0},i_val,i_tot;
        int q_buf[8]={0,0,0,0,0,0,0,0},q_val,q_tot;
        int r,g,b;
        if (!linepos)
        {
                vidtime+=dispofftime;
                cgastat|=1;
                linepos=1;
                oldsc=sc;
                if ((crtc[8] & 3) == 3) 
                   sc = ((sc << 1) + oddeven) & 7;
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
                                        chr=charbuffer[x<<1];
                                        attr=charbuffer[(x<<1)+1];
                                        drawcursor=((ma==ca) && con && cursoron);
                                        if (cgamode&0x20)
                                        {
                                                cols[1]=(attr&15)+16;
                                                cols[0]=((attr>>4)&7)+16;
                                                if ((cgablink&8) && (attr&0x80) && !drawcursor) cols[1]=cols[0];
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
                                for (x=0;x<crtc[1];x++)
                                {
                                        chr=vram[((ma<<1)&0x3FFF)];
                                        attr=vram[(((ma<<1)+1)&0x3FFF)];
                                        drawcursor=((ma==ca) && con && cursoron);
                                        if (cgamode&0x20)
                                        {
                                                cols[1]=(attr&15)+16;
                                                cols[0]=((attr>>4)&7)+16;
                                                if ((cgablink&8) && (attr&0x80)) cols[1]=cols[0];
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
                                for (x=0;x<crtc[1];x++)
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
                                cols[0]=0; cols[1]=(cgacol&15)+16;
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)]<<8)|vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)+1];
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
                if (cga_comp)
                {
                        for (c=0;c<x;c++)
                        {
                                y_buf[(c<<1)&6]=ntsc_col[buffer->line[displine][c]&7][(c<<1)&6]?0x6000:0;
                                y_buf[(c<<1)&6]+=(buffer->line[displine][c]&8)?0x3000:0;
                                i_buf[(c<<1)&6]=y_buf[(c<<1)&6]*i_filt[(c<<1)&6];
                                q_buf[(c<<1)&6]=y_buf[(c<<1)&6]*q_filt[(c<<1)&6];
                                y_tot=y_buf[0]+y_buf[1]+y_buf[2]+y_buf[3]+y_buf[4]+y_buf[5]+y_buf[6]+y_buf[7];
                                i_tot=i_buf[0]+i_buf[1]+i_buf[2]+i_buf[3]+i_buf[4]+i_buf[5]+i_buf[6]+i_buf[7];
                                q_tot=q_buf[0]+q_buf[1]+q_buf[2]+q_buf[3]+q_buf[4]+q_buf[5]+q_buf[6]+q_buf[7];

                                y_val=y_tot>>10;
                                if (y_val>255) y_val=255;
                                y_val<<=16;
                                i_val=i_tot>>12;
                                if (i_val>39041) i_val=39041;
                                if (i_val<-39041) i_val=-39041;
                                q_val=q_tot>>12;
                                if (q_val>34249) q_val=34249;
                                if (q_val<-34249) q_val=-34249;

                                r=(y_val+249*i_val+159*q_val)>>16;
                                g=(y_val-70*i_val-166*q_val)>>16;
                                b=(y_val-283*i_val+436*q_val)>>16;

                                y_buf[((c<<1)&6)+1]=ntsc_col[buffer->line[displine][c]&7][((c<<1)&6)+1]?0x6000:0;
                                y_buf[((c<<1)&6)+1]+=(buffer->line[displine][c]&8)?0x3000:0;
                                i_buf[((c<<1)&6)+1]=y_buf[((c<<1)&6)+1]*i_filt[((c<<1)&6)+1];
                                q_buf[((c<<1)&6)+1]=y_buf[((c<<1)&6)+1]*q_filt[((c<<1)&6)+1];
                                y_tot=y_buf[0]+y_buf[1]+y_buf[2]+y_buf[3]+y_buf[4]+y_buf[5]+y_buf[6]+y_buf[7];
                                i_tot=i_buf[0]+i_buf[1]+i_buf[2]+i_buf[3]+i_buf[4]+i_buf[5]+i_buf[6]+i_buf[7];
                                q_tot=q_buf[0]+q_buf[1]+q_buf[2]+q_buf[3]+q_buf[4]+q_buf[5]+q_buf[6]+q_buf[7];

                                y_val=y_tot>>10;
                                if (y_val>255) y_val=255;
                                y_val<<=16;
                                i_val=i_tot>>12;
                                if (i_val>39041) i_val=39041;
                                if (i_val<-39041) i_val=-39041;
                                q_val=q_tot>>12;
                                if (q_val>34249) q_val=34249;
                                if (q_val<-34249) q_val=-34249;

                                r+=(y_val+249*i_val+159*q_val)>>16;
                                g+=(y_val-70*i_val-166*q_val)>>16;
                                b+=(y_val-283*i_val+436*q_val)>>16;
                                if (r>511) r=511;
                                if (g>511) g=511;
                                if (b>511) b=511;

                                ((uint32_t *)buffer32->line[displine])[c]=makecol32(r/2,g/2,b/2);
                        }
                }

                sc=oldsc;
                if (vc==crtc[7] && !sc)
                   cgastat|=8;
                displine++;
                if (displine>=360) displine=0;
        }
        else
        {
                vidtime+=dispontime;
                if (cgadispon) cgastat&=~1;
                linepos=0;
                if (vsynctime)
                {
                        vsynctime--;
                        if (!vsynctime)
                           cgastat&=~8;
                }
                if (sc==(crtc[11]&31) || ((crtc[8]&3)==3 && sc==((crtc[11]&31)>>1))) { con=0; coff=1; }
                if ((crtc[8] & 3) == 3 && sc == (crtc[9] >> 1))
                   maback = ma;
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

                        if (vc==crtc[6]) cgadispon=0;

                        if (oldvc==crtc[4])
                        {
                                vc=0;
                                vadj=crtc[5];
                                if (!vadj) cgadispon=1;
                                if (!vadj) ma=maback=(crtc[13]|(crtc[12]<<8))&0x3FFF;
                                if ((crtc[10]&0x60)==0x20) cursoron=0;
                                else                       cursoron=cgablink&8;
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
                                                updatewindowsize(xsize,(ysize<<1)+16);
                                        }
                                        
startblit();
                                        if (cga_comp) 
                                           video_blit_memtoscreen(0, firstline-4, 0, (lastline-firstline)+8, xsize, (lastline-firstline)+8);
                                        else          
                                           video_blit_memtoscreen_8(0, firstline-4, xsize, (lastline-firstline)+8);
                                        if (readflash) rectfill(screen,winsizex-40,8,winsizex-8,14,0xFFFFFFFF);
                                        readflash=0;
                                        frames++;
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
                                                video_bpp = 1;
                                        }
                                }
                                firstline=1000;
                                lastline=0;
                                cgablink++;
                                oddeven ^= 1;
                        }
                }
                else
                {
                        sc++;
                        sc&=31;
                        ma=maback;
                }
                if ((sc==(crtc[10]&31) || ((crtc[8]&3)==3 && sc==((crtc[10]&31)>>1)))) con=1;
                if (cgadispon && (cgamode&1))
                {
                        for (x=0;x<(crtc[1]<<1);x++)
                            charbuffer[x]=vram[(((ma<<1)+x)&0x3FFF)];
                }
        }
}


int cga_init()
{
        int c;
        int cga_tint = -2;
        for (c=0;c<8;c++)
        {
                i_filt[c]=512.0*cos((3.14*(cga_tint+c*4)/16.0) - 33.0/180.0);
                q_filt[c]=512.0*sin((3.14*(cga_tint+c*4)/16.0) - 33.0/180.0);
        }
        mem_sethandler(0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL);
        return 0;
}

GFXCARD vid_cga =
{
        cga_init,
        /*IO at 3Cx/3Dx*/
        cga_out,
        cga_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        cga_poll,
        cga_recalctimings,

        video_write_null,
        video_write_null,
        cga_write,

        video_read_null,
        video_read_null,
        cga_read
};
