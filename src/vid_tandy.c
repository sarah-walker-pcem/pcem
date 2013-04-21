#include "ibm.h"
#include "video.h"
#include "io.h"
#include "mem.h"

static int      tandy_array_index;
static uint8_t  tandy_array[32];
static int      tandy_memctrl=-1;
static uint32_t tandy_base;
static uint8_t  tandy_mode,tandy_col;

static uint8_t *tandy_vram, *tandy_b8000;

void tandy_recalcaddress();
void tandy_recalctimings();
        
void tandy_out(uint16_t addr, uint8_t val)
{
        uint8_t old;
//        pclog("Tandy OUT %04X %02X\n",addr,val);
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
                                tandy_recalctimings();
                        }
                }
                return;
                case 0x3D8:
                tandy_mode=val;
                return;
                case 0x3D9:
                tandy_col=val;
                return;
                case 0x3DA:
                tandy_array_index = val & 31;
                break;
                case 0x3DE:
                if (tandy_array_index & 16) val &= 0xF;
                tandy_array[tandy_array_index & 31] = val;
                break;
                case 0x3DF:
                tandy_memctrl=val;
                tandy_recalcaddress();
                break;
                case 0xA0:
                tandy_base=((val>>1)&7)*128*1024;
                tandy_recalcaddress();
                break;
        }
}

uint8_t tandy_in(uint16_t addr)
{
//        if (addr!=0x3DA) pclog("Tandy IN %04X\n",addr);
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

void tandy_recalcaddress()
{
        if ((tandy_memctrl&0xC0)==0xC0)
        {
                tandy_vram =&ram[((tandy_memctrl&0x6)<<14) +tandy_base];
                tandy_b8000=&ram[((tandy_memctrl&0x30)<<11)+tandy_base];
//                printf("VRAM at %05X B8000 at %05X\n",((tandy_memctrl&0x6)<<14)+tandy_base,((tandy_memctrl&0x30)<<11)+tandy_base);
        }
        else
        {
                tandy_vram =&ram[((tandy_memctrl&0x7)<<14) +tandy_base];
                tandy_b8000=&ram[((tandy_memctrl&0x38)<<11)+tandy_base];
//                printf("VRAM at %05X B8000 at %05X\n",((tandy_memctrl&0x7)<<14)+tandy_base,((tandy_memctrl&0x38)<<11)+tandy_base);
        }
}

void tandy_write(uint32_t addr, uint8_t val)
{
        if (tandy_memctrl==-1) return;
//        pclog("Tandy VRAM write %05X %02X %04X:%04X  %04X:%04X\n",addr,val,CS,pc,DS,SI);
        tandy_b8000[addr&0x7FFF]=val;
}

uint8_t tandy_read(uint32_t addr)
{
        if (tandy_memctrl==-1) return 0xFF;
//        pclog("Tandy VRAM read  %05X %02X %04X:%04X\n",addr,tandy_b8000[addr&0x7FFF],CS,pc);
        return tandy_b8000[addr&0x7FFF];
}

void tandy_recalctimings()
{
        if (tandy_mode&1)
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
        dispontime*=CGACONST;
        dispofftime*=CGACONST;
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

/*static int cga4pal[8][4]=
{
        {0,2,4,6},{0,3,5,7},{0,3,4,7},{0,3,4,7},
        {0,10,12,14},{0,11,13,15},{0,11,12,15},{0,11,12,15}
};*/

void tandy_poll()
{
//        int *cgapal=cga4pal[((tandy_col&0x10)>>2)|((cgamode&4)>>1)|((cgacol&0x20)>>5)];
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
//                cgapal[0]=tandy_col&15;
//                printf("Firstline %i Lastline %i Displine %i\n",firstline,lastline,displine);
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
                        cols[0]=(tandy_array[2]&0xF)+16;
                        for (c=0;c<8;c++)
                        {
                                if (tandy_array[3]&4)
                                {
                                        buffer->line[displine][c]=cols[0];
                                        if (tandy_mode&1) buffer->line[displine][c+(crtc[1]<<3)+8]=cols[0];
                                        else              buffer->line[displine][c+(crtc[1]<<4)+8]=cols[0];
                                }
                                else if ((tandy_mode&0x12)==0x12)
                                {
                                        buffer->line[displine][c]=0;
                                        if (tandy_mode&1) buffer->line[displine][c+(crtc[1]<<3)+8]=0;
                                        else              buffer->line[displine][c+(crtc[1]<<4)+8]=0;
                                }
                                else
                                {
                                        buffer->line[displine][c]=(tandy_col&15)+16;
                                        if (tandy_mode&1) buffer->line[displine][c+(crtc[1]<<3)+8]=(tandy_col&15)+16;
                                        else              buffer->line[displine][c+(crtc[1]<<4)+8]=(tandy_col&15)+16;
                                }
                        }
//                        printf("X %i %i\n",c+(crtc[1]<<4)+8,c+(crtc[1]<<3)+8);
//                        printf("Drawing %i %i %i\n",displine,vc,sc);
                        if ((tandy_array[3]&0x10) && (tandy_mode&1)) /*320x200x16*/
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(tandy_vram[((ma<<1)&0x1FFF)+((sc&3)*0x2000)]<<8)|tandy_vram[((ma<<1)&0x1FFF)+((sc&3)*0x2000)+1];
                                        ma++;
                                        buffer->line[displine][(x<<3)+8]=buffer->line[displine][(x<<3)+9]  =tandy_array[((dat>>12)&tandy_array[1])+16]+16;
                                        buffer->line[displine][(x<<3)+10]=buffer->line[displine][(x<<3)+11]=tandy_array[((dat>>8)&tandy_array[1])+16]+16;
                                        buffer->line[displine][(x<<3)+12]=buffer->line[displine][(x<<3)+13]=tandy_array[((dat>>4)&tandy_array[1])+16]+16;
                                        buffer->line[displine][(x<<3)+14]=buffer->line[displine][(x<<3)+15]=tandy_array[(dat&tandy_array[1])+16]+16;
                                }
                        }
                        else if (tandy_array[3]&0x10) /*160x200x16*/
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(tandy_vram[((ma<<1)&0x1FFF)+((sc&3)*0x2000)]<<8)|tandy_vram[((ma<<1)&0x1FFF)+((sc&3)*0x2000)+1];
                                        ma++;
                                        buffer->line[displine][(x<<4)+8]= buffer->line[displine][(x<<4)+9]= buffer->line[displine][(x<<4)+10]=buffer->line[displine][(x<<4)+11]=tandy_array[((dat>>12)&tandy_array[1])+16]+16;
                                        buffer->line[displine][(x<<4)+12]=buffer->line[displine][(x<<4)+13]=buffer->line[displine][(x<<4)+14]=buffer->line[displine][(x<<4)+15]=tandy_array[((dat>>8)&tandy_array[1])+16]+16;
                                        buffer->line[displine][(x<<4)+16]=buffer->line[displine][(x<<4)+17]=buffer->line[displine][(x<<4)+18]=buffer->line[displine][(x<<4)+19]=tandy_array[((dat>>4)&tandy_array[1])+16]+16;
                                        buffer->line[displine][(x<<4)+20]=buffer->line[displine][(x<<4)+21]=buffer->line[displine][(x<<4)+22]=buffer->line[displine][(x<<4)+23]=tandy_array[(dat&tandy_array[1])+16]+16;
                                }
                        }
                        else if (tandy_array[3]&0x08) /*640x200x4 - this implementation is a complete guess!*/
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(tandy_vram[((ma<<1)&0x1FFF)+((sc&3)*0x2000)]<<8)|tandy_vram[((ma<<1)&0x1FFF)+((sc&3)*0x2000)+1];
                                        ma++;
                                        for (c=0;c<8;c++)
                                        {
                                                chr=(dat>>7)&1;
                                                chr|=((dat>>14)&2);
                                                buffer->line[displine][(x<<3)+8+c]=tandy_array[(chr&tandy_array[1])+16]+16;
                                                dat<<=1;
                                        }
                                }
                        }
                        else if (tandy_mode&1)
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        chr=tandy_vram[(ma<<1)&0x3FFF];
                                        attr=tandy_vram[((ma<<1)+1)&0x3FFF];
                                        drawcursor=((ma==ca) && con && cursoron);
                                        if (tandy_mode&0x20)
                                        {
                                                cols[1]=tandy_array[((attr&15)&tandy_array[1])+16]+16;
                                                cols[0]=tandy_array[(((attr>>4)&7)&tandy_array[1])+16]+16;
                                                if ((cgablink&16) && (attr&0x80) && !drawcursor) cols[1]=cols[0];
                                        }
                                        else
                                        {
                                                cols[1]=tandy_array[((attr&15)&tandy_array[1])+16]+16;
                                                cols[0]=tandy_array[((attr>>4)&tandy_array[1])+16]+16;
                                        }
                                        if (sc&8)
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<3)+c+8]=cols[0];
                                        }
                                        else
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<3)+c+8]=cols[(fontdat[chr][sc&7]&(1<<(c^7)))?1:0];
                                        }
//                                        if (!((ma^(crtc[15]|(crtc[14]<<8)))&0x3FFF)) printf("Cursor match! %04X\n",ma);
                                        if (drawcursor)
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<3)+c+8]^=15;
                                        }
                                        ma++;
                                }
                        }
                        else if (!(tandy_mode&2))
                        {
                                for (x=0;x<crtc[1];x++)
                                {
                                        chr=tandy_vram[(ma<<1)&0x3FFF];
                                        attr=tandy_vram[((ma<<1)+1)&0x3FFF];
                                        drawcursor=((ma==ca) && con && cursoron);
                                        if (tandy_mode&0x20)
                                        {
                                                cols[1]=tandy_array[((attr&15)&tandy_array[1])+16]+16;
                                                cols[0]=tandy_array[(((attr>>4)&7)&tandy_array[1])+16]+16;
                                                if ((cgablink&16) && (attr&0x80) && !drawcursor) cols[1]=cols[0];
                                        }
                                        else
                                        {
                                                cols[1]=tandy_array[((attr&15)&tandy_array[1])+16]+16;
                                                cols[0]=tandy_array[((attr>>4)&tandy_array[1])+16]+16;
                                        }
                                        ma++;
                                        if (sc&8)
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<4)+(c<<1)+8]=buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[0];
                                        }
                                        else
                                        {
                                                for (c=0;c<8;c++)
                                                    buffer->line[displine][(x<<4)+(c<<1)+8]=buffer->line[displine][(x<<4)+(c<<1)+1+8]=cols[(fontdat[chr][sc&7]&(1<<(c^7)))?1:0];
                                        }
                                        if (drawcursor)
                                        {
                                                for (c=0;c<16;c++)
                                                    buffer->line[displine][(x<<4)+c+8]^=15;
                                        }
                                }
                        }
                        else if (!(tandy_mode&16))
                        {
                                cols[0]=(tandy_col&15)|16;
                                col=(tandy_col&16)?24:16;
                                if (tandy_mode&4)
                                {
                                        cols[1]=col|3;
                                        cols[2]=col|4;
                                        cols[3]=col|7;
                                }
                                else if (tandy_col&32)
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
                                        dat=(tandy_vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)]<<8)|tandy_vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)+1];
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
                                cols[0]=0; cols[1]=tandy_array[(tandy_col&tandy_array[1])+16]+16;
                                for (x=0;x<crtc[1];x++)
                                {
                                        dat=(tandy_vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)]<<8)|tandy_vram[((ma<<1)&0x1FFF)+((sc&1)*0x2000)+1];
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
                        if (tandy_array[3]&4)
                        {
                                if (tandy_mode&1) hline(buffer,0,displine,(crtc[1]<<3)+16,(tandy_array[2]&0xF)+16);
                                else              hline(buffer,0,displine,(crtc[1]<<4)+16,(tandy_array[2]&0xF)+16);
                        }
                        else
                        {
                                cols[0]=((tandy_mode&0x12)==0x12)?0:(tandy_col&15)+16;
                                if (tandy_mode&1) hline(buffer,0,displine,(crtc[1]<<3)+16,cols[0]);
                                else              hline(buffer,0,displine,(crtc[1]<<4)+16,cols[0]);
                        }
                }
                if (tandy_mode&1) x=(crtc[1]<<3)+16;
                else              x=(crtc[1]<<4)+16;
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
//                                if (r>255) r=255;
//                                if (g>255) g=255;
//                              if (b>255) b=255;

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
                {
                        cgastat|=8;
//                        printf("VSYNC on %i %i\n",vc,sc);
                }
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
                        {
                                cgastat&=~8;
//                                printf("VSYNC off %i %i\n",vc,sc);
                        }
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
//                                printf("Display on!\n");
                        }
                }
                else if (sc==crtc[9] || ((crtc[8]&3)==3 && sc==(crtc[9]>>1)))
                {
                        maback=ma;
//                        con=0;
//                        coff=0;
                        sc=0;
                        oldvc=vc;
                        vc++;
                        vc&=127;
//                        printf("VC %i %i %i %i  %i\n",vc,crtc[4],crtc[6],crtc[7],cgadispon);
                        if (vc==crtc[6]) cgadispon=0;
                        if (oldvc==crtc[4])
                        {
//                                printf("Display over at %i\n",displine);
                                vc=0;
                                vadj=crtc[5];
                                if (!vadj) cgadispon=1;
                                if (!vadj) ma=maback=(crtc[13]|(crtc[12]<<8))&0x3FFF;
                                if ((crtc[10]&0x60)==0x20) cursoron=0;
                                else                       cursoron=cgablink&16;
//                                printf("CRTC10 %02X %i\n",crtc[10],cursoron);
                        }
                        if (vc==crtc[7])
                        {
                                cgadispon=0;
                                displine=0;
                                vsynctime=16;//(crtc[3]>>4)+1;
//                                printf("Vsynctime %i %02X\n",vsynctime,crtc[3]);
//                                cgastat|=8;
                                if (crtc[7])
                                {
//                                        printf("Lastline %i Firstline %i  %i   %i %i\n",lastline,firstline,lastline-firstline,crtc[1],xsize);
                                        if (tandy_mode&1) x=(crtc[1]<<3)+16;
                                        else              x=(crtc[1]<<4)+16;
                                        lastline++;
                                        if (x!=xsize || (lastline-firstline)!=ysize)
                                        {
                                                xsize=x;
                                                ysize=lastline-firstline;
//                                                printf("Resize to %i,%i - R1 %i\n",xsize,ysize,crtc[1]);
                                                if (xsize<64) xsize=656;
                                                if (ysize<32) ysize=200;
                                                updatewindowsize(xsize,(ysize<<1)+16);
                                        }
//                                        printf("Blit %i %i\n",firstline,lastline);
//printf("Xsize is %i\n",xsize);
                                startblit();
                                        if (cga_comp) 
                                           video_blit_memtoscreen(0, firstline-4, 0, (lastline-firstline)+8, xsize, (lastline-firstline)+8);
                                        else          
                                           video_blit_memtoscreen_8(0, firstline-4, xsize, (lastline-firstline)+8);
                                endblit();
                                        frames++;
                                        video_res_x = xsize - 16;
                                        video_res_y = ysize;
                                        if ((tandy_array[3]&0x10) && (tandy_mode&1)) /*320x200x16*/
                                        {
                                                video_res_x /= 2;
                                                video_bpp = 4;
                                        }
                                        else if (tandy_array[3]&0x10) /*160x200x16*/
                                        {
                                                video_res_x /= 4;
                                                video_bpp = 4;
                                        }
                                        else if (tandy_array[3]&0x08) /*640x200x4 - this implementation is a complete guess!*/
                                           video_bpp = 2;
                                        else if (tandy_mode&1)
                                        {
                                                video_res_x /= 8;
                                                video_res_y /= crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(tandy_mode&2))
                                        {
                                                video_res_x /= 16;
                                                video_res_y /= crtc[9] + 1;
                                                video_bpp = 0;
                                        }
                                        else if (!(tandy_mode&16))
                                        {
                                                video_res_x /= 2;
                                                video_bpp = 2;
                                        }
                                        else
                                           video_bpp = 1;                                                
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
}

int tandy_init()
{
        mem_sethandler(0xb8000, 0x8000, tandy_read, NULL, NULL, tandy_write, NULL, NULL);
        io_sethandler(0x00a0, 0x0002, tandy_in, NULL, NULL, tandy_out, NULL, NULL);
        tandy_memctrl = -1;
        return 0;
}

GFXCARD vid_tandy =
{
        tandy_init,
        /*IO at 3Cx/3Dx*/
        tandy_out,
        tandy_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        tandy_poll,
        tandy_recalctimings,

        video_write_null,
        video_write_null,
        tandy_write,

        video_read_null,
        video_read_null,
        tandy_read
};
