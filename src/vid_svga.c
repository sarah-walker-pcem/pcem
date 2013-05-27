/*Generic SVGA handling*/
/*This is intended to be used by another SVGA driver, and not as a card in it's own right*/
#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "io.h"
#include "timer.h"

#define svga_output 0

uint32_t svga_vram_limit;

extern uint8_t edatlookup[4][4];
uint8_t svga_miscout;

static uint8_t la, lb, lc, ld;
uint8_t svga_rotate[8][256];

static int svga_fast;

void (*svga_render)();

static uint8_t dacmask, dacstatus;

void svga_out(uint16_t addr, uint8_t val, void *priv)
{
        int c;
        uint8_t o;
//        printf("OUT SVGA %03X %02X %04X:%04X %i %08X\n",addr,val,CS,pc,TRIDENT,svgawbank);
        switch (addr)
        {
                case 0x3C0:
                if (!attrff)
                   attraddr=val&31;
                else
                {
                        attrregs[attraddr&31]=val;
                        if (attraddr<16) fullchange=changeframecount;
                        if (attraddr==0x10 || attraddr==0x14 || attraddr<0x10)
                        {
                                for (c=0;c<16;c++)
                                {
                                        if (attrregs[0x10]&0x80) egapal[c]=(attrregs[c]&0xF)|((attrregs[0x14]&0xF)<<4);
                                        else                     egapal[c]=(attrregs[c]&0x3F)|((attrregs[0x14]&0xC)<<4);
                                }
                        }
                        if (attraddr == 0x10) svga_recalctimings();
                }
                attrff^=1;
                break;
                case 0x3C2:
                svga_miscout = val;
                vres = 0; pallook = pallook256;
                vidclock = val & 4; printf("3C2 write %02X\n",val);
                if (val & 1)
                {
                        pclog("Remove mono handler\n");
                        io_removehandler(0x03a0, 0x0020, video_in,      NULL, NULL, video_out,      NULL, NULL, NULL);
                }
                else
                {
                        pclog("Set mono handler\n");
                        io_sethandler(0x03a0, 0x0020, video_in, NULL, NULL, video_out, NULL, NULL, NULL);
                }
                break;
                case 0x3C4: seqaddr=val; break;
                case 0x3C5:
                if (seqaddr > 0xf) return;
                o=seqregs[seqaddr&0xF];
                seqregs[seqaddr&0xF]=val;
                if (o!=val && (seqaddr&0xF)==1) svga_recalctimings();
                switch (seqaddr&0xF)
                {
                        case 1: 
                        if (scrblank && !(val&0x20)) 
                                fullchange=3; 
                        scrblank=(scrblank&~0x20)|(val&0x20); 
                        svga_recalctimings();
                        break;
                        case 2: writemask=val&0xF; break;
                        case 3:
                        charset=val&0xF;
                        charseta=((charset>>2)*0x10000)+2;
                        charsetb=((charset&3) *0x10000)+2;
                        break;
                        case 4: 
                        chain4=val&8; /*printf("Chain4 %i %02X\n",val&8,gdcreg[5]&0x60); */
                        svga_fast = (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1]) && chain4;
                        break;
                }
                break;
                case 0x3c6: dacmask=val; break;
                case 0x3C7: dacread=val; dacpos=0; break;
                case 0x3C8: dacwrite=val; dacpos=0; break;
                case 0x3C9:
                dacstatus = 0;
/*                if (pc == 0x68AD)
                {
                        dumpregs();
                        exit(-1);
                }*/
                palchange=1;
                fullchange=changeframecount;
                switch (dacpos)
                {
                        case 0: vgapal[dacwrite].r=val&63; pallook256[dacwrite]=makecol32(vgapal[dacwrite].r*4,vgapal[dacwrite].g*4,vgapal[dacwrite].b*4); dacpos++; break;
                        case 1: vgapal[dacwrite].g=val&63; pallook256[dacwrite]=makecol32(vgapal[dacwrite].r*4,vgapal[dacwrite].g*4,vgapal[dacwrite].b*4); dacpos++; break;
                        case 2: vgapal[dacwrite].b=val&63; pallook256[dacwrite]=makecol32(vgapal[dacwrite].r*4,vgapal[dacwrite].g*4,vgapal[dacwrite].b*4); dacpos=0; dacwrite=(dacwrite+1)&255; break;
                }
                break;
                case 0x3CE: gdcaddr=val; break;
                case 0x3CF:
                o = gdcreg[gdcaddr & 15];
                switch (gdcaddr&15)
                {
                        case 2: colourcompare=val; break;
                        case 4: readplane=val&3; break;
                        case 5: writemode=val&3; readmode=val&8; break;
                        case 6:
                        if ((gdcreg[6] & 0xc) != (val & 0xc))
                        {
                                mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                                pclog("Write mapping %02X\n", val);
                                switch (val&0xC)
                                {
                                        case 0x0: /*128k at A0000*/
                                        mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                                        break;
                                        case 0x4: /*64k at A0000*/
                                        mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                                        break;
                                        case 0x8: /*32k at B0000*/
                                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                                        break;
                                        case 0xC: /*32k at B8000*/
                                        mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel, NULL);
                                        break;
                                }
                        }
                        break;
                        case 7: colournocare=val; break;
                }
                gdcreg[gdcaddr&15]=val;                
                svga_fast = (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1]) && chain4;
                if (((gdcaddr & 15) == 5  && (val ^ o) & 0x70) || ((gdcaddr & 15) == 6 && (val ^ o) & 1))
                        svga_recalctimings();
                break;
        }
}

uint8_t svga_in(uint16_t addr, void *priv)
{
        uint8_t temp;
//        if (addr!=0x3da) pclog("Read port %04X\n",addr);
        switch (addr)
        {
                case 0x3C0: return attraddr;
                case 0x3C1: return attrregs[attraddr];
                case 0x3c2:
                if ((vgapal[0].r + vgapal[0].g + vgapal[0].b) >= 0x50)
                        temp = 0;
                else
                        temp = 0x10;
                return temp;
                case 0x3C4: return seqaddr;
                case 0x3C5:
                return seqregs[seqaddr&0xF];
                case 0x3c6: return dacmask;
                case 0x3c7: return dacstatus;
                case 0x3C8: return dacwrite;
                case 0x3C9:
                dacstatus=3;
                palchange=1;
                switch (dacpos)
                {
                        case 0: dacpos++; return vgapal[dacread].r;
                        case 1: dacpos++; return vgapal[dacread].g;
                        case 2: dacpos=0; dacread=(dacread+1)&255; return vgapal[(dacread-1)&255].b;
                }
                break;
                case 0x3CC: return svga_miscout;
                case 0x3CE: return gdcaddr;
                case 0x3CF:
                return gdcreg[gdcaddr&0xF];
                case 0x3DA:
                attrff=0;
                cgastat^=0x30;
                return cgastat;
        }
//        printf("Bad EGA read %04X %04X:%04X\n",addr,cs>>4,pc);
        return 0xFF;
}

int svga_vtotal, svga_dispend, svga_vsyncstart, svga_split;
int svga_hdisp,  svga_htotal,  svga_hdisp_time, svga_rowoffset;
int svga_lowres, svga_interlace;
int svga_linedbl, svga_rowcount;
double svga_clock;
uint32_t svga_ma;
void (*svga_recalctimings_ex)() = NULL;
void (*svga_hwcursor_draw)(int displine) = NULL;

void svga_recalctimings()
{
        double crtcconst;
        double _dispontime, _dispofftime;

        svga_vtotal=crtc[6];
        svga_dispend=crtc[0x12];
        svga_vsyncstart=crtc[0x10];
        svga_split=crtc[0x18];

        if (crtc[7]&1)    svga_vtotal|=0x100;
        if (crtc[7]&32)   svga_vtotal|=0x200;
        svga_vtotal+=2;


        if (crtc[7]&2)    svga_dispend|=0x100;
        if (crtc[7]&64)   svga_dispend|=0x200;
        svga_dispend++;

        if (crtc[7]&4)    svga_vsyncstart|=0x100;
        if (crtc[7]&128)  svga_vsyncstart|=0x200;
        svga_vsyncstart++;

        if (crtc[7]&0x10) svga_split|=0x100;
        if (crtc[9]&0x40) svga_split|=0x200;
        svga_split++;

        svga_hdisp=crtc[1];
        svga_hdisp++;

        svga_htotal=crtc[0];
        svga_htotal+=6; /*+6 is required for Tyrian*/

        svga_rowoffset=crtc[0x13];

        svga_clock = (vidclock) ? VGACONST2 : VGACONST1;
        
        svga_lowres = attrregs[0x10]&0x40;
        
        svga_interlace = 0;
        
        svga_ma = (crtc[0xC]<<8)|crtc[0xD];

        svga_hdisp_time = svga_hdisp;
        svga_render = svga_render_blank;
        if (!scrblank)
        {
                if (!(gdcreg[6]&1)) /*Text mode*/
                {
                        if (seqregs[1]&8) /*40 column*/
                        {
                                svga_render = svga_render_text_40;
                                svga_hdisp *= (seqregs[1] & 1) ? 16 : 18;
                        }
                        else
                        {
                                svga_render = svga_render_text_80;
                                svga_hdisp *= (seqregs[1] & 1) ? 8 : 9;
                        }
                }
                else 
                {
                        svga_hdisp *= (seqregs[1] & 8) ? 16 : 8;
                        
                        switch (gdcreg[5]&0x60)
                        {
                                case 0x00: /*16 colours*/
                                if (seqregs[1]&8) /*Low res (320)*/
                                        svga_render = svga_render_4bpp_lowres;
                                else
                                        svga_render = svga_render_4bpp_highres;
                                break;
                                case 0x20: /*4 colours*/
                                svga_render = svga_render_2bpp_lowres;
                                break;
                                case 0x40: case 0x60: /*256+ colours*/
                                switch (bpp)
                                {
                                        case 8:
                                        if (svga_lowres)
                                                svga_render = svga_render_8bpp_lowres;
                                        else
                                        {
                                                svga_render = svga_render_8bpp_highres;
//                                                svga_hdisp <<= 1;
                                        }
                                        break;
                                        case 15:
                                        if (svga_lowres)
                                                svga_render = svga_render_15bpp_lowres;
                                        else
                                        {
                                                svga_render = svga_render_15bpp_highres;
                                                svga_hdisp <<= 1;
                                        }
                                        break;
                                        case 16:
                                        if (svga_lowres)
                                                svga_render = svga_render_16bpp_lowres;
                                        else
                                        {
                                                svga_render = svga_render_16bpp_highres;
                                                svga_hdisp <<= 1;
                                        }
                                        break;
                                        case 24:
                                        if (svga_lowres)
                                        {
                                                svga_render = svga_render_24bpp_lowres;
                                                svga_hdisp = ((svga_hdisp<<3)/3);
                                        }
                                        else
                                        {
                                                svga_render = svga_render_24bpp_highres;
                                                svga_hdisp = ((svga_hdisp<<3)/3);
                                        }
                                        break;
                                        case 32:
                                        if (svga_lowres)
                                                svga_render = svga_render_32bpp_lowres;
                                        else
                                        {
                                                svga_render = svga_render_32bpp_highres;
                                                svga_hdisp <<= 1;
                                        }
                                        break;
                                }
                                break;
                        }
                }
        }        

//        pclog("svga_render %08X : %08X %08X %08X %08X %08X  %i %i %02X %i %i\n", svga_render, svga_render_text_40, svga_render_text_80, svga_render_8bpp_lowres, svga_render_8bpp_highres, svga_render_blank, scrblank,gdcreg[6]&1,gdcreg[5]&0x60,bpp,seqregs[1]&8);
        
        svga_linedbl = crtc[9] & 0x80;
        svga_rowcount = crtc[9] & 31;
        if (svga_recalctimings_ex) svga_recalctimings_ex();
        
        crtcconst=(seqregs[1]&1)?(svga_clock*8.0):(svga_clock*9.0);

        disptime  =svga_htotal;
        _dispontime = svga_hdisp_time;
        
//        printf("Disptime %f dispontime %f hdisp %i\n",disptime,dispontime,crtc[1]*8);
        if (seqregs[1]&8) { disptime*=2; _dispontime*=2; }
        _dispofftime=disptime-_dispontime;
        _dispontime*=crtcconst;
        _dispofftime*=crtcconst;

	dispontime = (int)(_dispontime * (1 << TIMER_SHIFT));
	dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));
//        printf("SVGA horiz total %i display end %i clock rate %i vidclock %i %i\n",crtc[0],crtc[1],egaswitchread,vidclock,((ega3c2>>2)&3) | ((tridentnewctrl2<<2)&4));
//        printf("SVGA vert total %i display end %i max row %i vsync %i\n",svga_vtotal,svga_dispend,(crtc[9]&31)+1,svga_vsyncstart);
//        printf("total %f on %f cycles off %f cycles frame %f sec %f %02X\n",disptime*crtcconst,dispontime,dispofftime,(dispontime+dispofftime)*svga_vtotal,(dispontime+dispofftime)*svga_vtotal*70,seqregs[1]);

//        pclog("svga_render %08X\n", svga_render);
}

void (*svga_render)();

uint32_t ma, maback, ca;
static int vc;
int sc;
static int linepos, vslines, linecountff, oddeven;
static int svga_dispon;
int con, coff, cursoron, cgablink;
int scrollcache;

int svga_hwcursor_on;

SVGA_HWCURSOR svga_hwcursor, svga_hwcursor_latch;

int svga_hdisp_on;

int firstline_draw = 2000, lastline_draw = 0;
int displine;

void svga_poll()
{
        int x;

        if (!linepos)
        {
//                if (!(vc & 15)) pclog("VC %i %i\n", vc, GetTickCount());
                if (displine == svga_hwcursor_latch.y && svga_hwcursor_latch.ena)
                   svga_hwcursor_on = 64 - svga_hwcursor_latch.yoff;
                vidtime+=dispofftime;
//                if (output) printf("Display off %f\n",vidtime);
                cgastat|=1;
                linepos=1;

                if (svga_dispon)
                {
                        svga_hdisp_on=1;
                        
                        ma&=vrammask;
                        if (firstline==2000) firstline=displine;
                        
                        if (svga_hwcursor_on) changedvram[ma >> 10] = changedvram[(ma >> 10) + 1] = 2;
                        
                        svga_render();
                        
                                if (svga_hwcursor_on)
                                {
                                        svga_hwcursor_draw(displine);
                                        svga_hwcursor_on--;
                                }

                        if (lastline<displine) lastline=displine;
                }

//                pclog("%03i %06X %06X\n",displine,ma,vrammask);
                displine++;
                if (svga_interlace) displine++;
                if ((cgastat&8) && ((displine&15)==(crtc[0x11]&15)) && vslines)
                {
//                        printf("Vsync off at line %i\n",displine);
                        cgastat&=~8;
                }
                vslines++;
                if (displine>1500) displine=0;
//                pclog("Col is %08X %08X %08X   %i %i  %08X\n",((uint32_t *)buffer32->line[displine])[320],((uint32_t *)buffer32->line[displine])[321],((uint32_t *)buffer32->line[displine])[322],
//                                                                displine, vc, ma);
        }
        else
        {
//                pclog("VC %i ma %05X\n", vc, ma);
                vidtime+=dispontime;
                
//                if (output) printf("Display on %f\n",vidtime);
                if (svga_dispon) cgastat&=~1;
                svga_hdisp_on=0;
                
                linepos=0;
                if (sc==(crtc[11]&31)) con = 0;
                if (svga_dispon)
                {
                        if (svga_linedbl && !linecountff)
                        {
                                linecountff=1;
                                ma=maback;
                        }
                        else if (sc == svga_rowcount)
                        {
                                linecountff=0;
                                sc=0;

                                maback += (svga_rowoffset<<3);
                                if (svga_interlace) maback += (svga_rowoffset<<3);
                                maback&=vrammask;
                                ma=maback;
                        }
                        else
                        {
                                linecountff=0;
                                sc++;
                                sc&=31;
                                ma=maback;
                        }
                }
                vc++;
                vc&=2047;

                if (vc==svga_split)
                {
//                        pclog("VC split\n");
                        ma=maback=0;
                        if (attrregs[0x10]&0x20) scrollcache=0;
                }
                if (vc==svga_dispend)
                {
//                        pclog("VC dispend\n");
                        svga_dispon=0;
                        if (crtc[10] & 0x20) cursoron=0;
                        else                 cursoron=cgablink&16;
                        if (!(gdcreg[6]&1) && !(cgablink&15)) fullchange=2;
                        cgablink++;

                        for (x=0;x<2048;x++) if (changedvram[x]) changedvram[x]--;
//                        memset(changedvram,0,2048);
                        if (fullchange) fullchange--;
                }
                if (vc==svga_vsyncstart)
                {
//                        pclog("VC vsync  %i %i\n", firstline_draw, lastline_draw);
                        svga_dispon=0;
                        cgastat|=8;
                        /*if (seqregs[1]&8) x=(svga_hdisp*((seqregs[1]&1)?8:9))*2;
                        else              */x = svga_hdisp;//*((seqregs[1]&1)?8:9);

                        if (bpp == 16 || bpp == 15) x /= 2;
//                        if (bpp == 24)              x /= 3;
                        if (bpp == 32)              x /= 4;
                        if (svga_interlace && !oddeven) lastline++;
                        if (svga_interlace &&  oddeven) firstline--;

                        wx=x;
                        wy=lastline-firstline;


                        svga_doblit(firstline_draw, lastline_draw + 1);

                        readflash=0;

                        firstline=2000;
                        lastline=0;
                        
                        firstline_draw = 2000;
                        lastline_draw = 0;
                        
                        oddeven^=1;

                        changeframecount=(svga_interlace)?3:2;
                        vslines=0;
                        
                        if (svga_interlace && oddeven) ma = maback = svga_ma + (svga_rowoffset << 1);
                        else                           ma = maback = svga_ma;
                        ca=(crtc[0xE]<<8)|crtc[0xF];

                        ma <<= 2;
                        maback <<= 2;
                        ca <<= 2;


                        video_res_x = wx;
                        video_res_y = wy + 1;
                        
                        if (!(gdcreg[6]&1)) /*Text mode*/
                        {
                                video_res_x /= (seqregs[1] & 1) ? 8 : 9;
                                video_res_y /= (crtc[9] & 31) + 1;
                                video_bpp = 0;
                        }
                        else
                        {
                                if (crtc[9] & 0x80)
                                   video_res_y /= 2;
                                if (!(crtc[0x17] & 1))
                                   video_res_y *= 2;
                                video_res_y /= (crtc[9] & 31) + 1;                                   
                                if ((seqregs[1] & 8) || svga_lowres)
                                   video_res_x /= 2;
                                switch (gdcreg[5]&0x60)
                                {
                                        case 0x00:            video_bpp = 4;   break;
                                        case 0x20:            video_bpp = 2;   break;
                                        case 0x40: case 0x60: video_bpp = bpp; break;
                                }
                        }
                        video_bpp = bpp;
//                        if (svga_interlace && oddeven) ma=maback=ma+(svga_rowoffset<<2);
                        
//                        pclog("Addr %08X vson %03X vsoff %01X  %02X %02X %02X %i %i\n",ma,svga_vsyncstart,crtc[0x11]&0xF,crtc[0xD],crtc[0xC],crtc[0x33], svga_interlace, oddeven);
                }
                if (vc==svga_vtotal)
                {
//                        pclog("VC vtotal\n");


//                        printf("Frame over at line %i %i  %i %i\n",displine,vc,svga_vsyncstart,svga_dispend);
                        vc=0;
                        sc=0;
                        svga_dispon=1;
                        displine=(svga_interlace && oddeven) ? 1 : 0; //(TRIDENT && (crtc[0x1E]&4) && oddeven)?1:0;
                        scrollcache=attrregs[0x13]&7;
                        linecountff=0;
                        
                        svga_hwcursor_on=0;
                        svga_hwcursor_latch = svga_hwcursor;
//                        pclog("Latch HWcursor addr %08X\n", svga_hwcursor_latch.addr);
                        
//                        pclog("ADDR %08X\n",hwcursor_addr);
                }
                if (sc==(crtc[10]&31)) con=1;
        }
//        printf("2 %i\n",svga_vsyncstart);
}

int svga_init()
{
        int c, d, e;
        for (c = 0; c < 256; c++)
        {
                e = c;
                for (d = 0; d < 8; d++)
                {
                        svga_rotate[d][c] = e;
                        e = (e >> 1) | ((e & 1) ? 0x80 : 0);
                }
        }
        readmode = 0;
        return 0;
}

#define egacycles 1
#define egacycles2 1
void svga_write(uint32_t addr, uint8_t val, void *priv)
{
        uint8_t vala,valb,valc,vald,wm=writemask;
        int writemask2 = writemask;

        egawrites++;

        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

        if (svga_output) pclog("Writeega %06X   ",addr);
        if (addr>=0xB0000) addr&=0x7FFF;
        else
        {
                //if (gdcreg[6]&8) return;
                addr &= 0xFFFF;
                addr += svgawbank;
        }

        if (!(gdcreg[6]&1)) fullchange=2;
        if (chain4)
        {
                writemask2=1<<(addr&3);
                addr&=~3;
        }
        else
        {
                addr<<=2;
        }
        addr&=0x7FFFFF;
        if (svga_output) pclog("%08X (%i, %i) %02X %i %i %i\n", addr, addr & 1023, addr >> 10, val, writemask2, writemode, chain4);
        changedvram[addr>>10]=changeframecount;

        switch (writemode)
        {
                case 1:
                if (writemask2&1) vram[addr]=la;
                if (writemask2&2) vram[addr|0x1]=lb;
                if (writemask2&4) vram[addr|0x2]=lc;
                if (writemask2&8) vram[addr|0x3]=ld;
                break;
                case 0:
                if (gdcreg[3]&7) val=svga_rotate[gdcreg[3]&7][val];
                if (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1])
                {
                        if (writemask2&1) vram[addr]=val;
                        if (writemask2&2) vram[addr|0x1]=val;
                        if (writemask2&4) vram[addr|0x2]=val;
                        if (writemask2&8) vram[addr|0x3]=val;
                }
                else
                {
                        if (gdcreg[1]&1) vala=(gdcreg[0]&1)?0xFF:0;
                        else             vala=val;
                        if (gdcreg[1]&2) valb=(gdcreg[0]&2)?0xFF:0;
                        else             valb=val;
                        if (gdcreg[1]&4) valc=(gdcreg[0]&4)?0xFF:0;
                        else             valc=val;
                        if (gdcreg[1]&8) vald=(gdcreg[0]&8)?0xFF:0;
                        else             vald=val;

                        switch (gdcreg[3]&0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                                if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                break;
                        }
//                        pclog("- %02X %02X %02X %02X   %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                }
                break;
                case 2:
                if (!(gdcreg[3]&0x18) && !gdcreg[1])
                {
                        if (writemask2&1) vram[addr]=(((val&1)?0xFF:0)&gdcreg[8])|(la&~gdcreg[8]);
                        if (writemask2&2) vram[addr|0x1]=(((val&2)?0xFF:0)&gdcreg[8])|(lb&~gdcreg[8]);
                        if (writemask2&4) vram[addr|0x2]=(((val&4)?0xFF:0)&gdcreg[8])|(lc&~gdcreg[8]);
                        if (writemask2&8) vram[addr|0x3]=(((val&8)?0xFF:0)&gdcreg[8])|(ld&~gdcreg[8]);
                }
                else
                {
                        vala=((val&1)?0xFF:0);
                        valb=((val&2)?0xFF:0);
                        valc=((val&4)?0xFF:0);
                        vald=((val&8)?0xFF:0);
                        switch (gdcreg[3]&0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                                if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                break;
                        }
                }
                break;
                case 3:
                if (gdcreg[3]&7) val=svga_rotate[gdcreg[3]&7][val];
                wm=gdcreg[8];
                gdcreg[8]&=val;

                vala=(gdcreg[0]&1)?0xFF:0;
                valb=(gdcreg[0]&2)?0xFF:0;
                valc=(gdcreg[0]&4)?0xFF:0;
                vald=(gdcreg[0]&8)?0xFF:0;
                switch (gdcreg[3]&0x18)
                {
                        case 0: /*Set*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                        break;
                        case 8: /*AND*/
                        if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                        if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                        if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                        if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                        break;
                        case 0x10: /*OR*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                        break;
                        case 0x18: /*XOR*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                        break;
                }
                gdcreg[8]=wm;
                break;
        }
}

uint8_t svga_read(uint32_t addr, void *priv)
{
        uint8_t temp,temp2,temp3,temp4;
        
        cycles -= video_timing_b;
        cycles_lost += video_timing_b;
        
        egareads++;
//        pclog("Readega %06X   ",addr);
        if (addr>=0xB0000) addr&=0x7FFF;
        else
        {
                addr &= 0xFFFF;
                addr += svgarbank;
        }
//        pclog("%05X %i %04X:%04X %02X %02X %i\n",addr,chain4,CS,pc, vram[addr & 0x7fffff], vram[(addr << 2) & 0x7fffff], readmode);
//        pclog("%i\n", readmode);
        if (chain4) 
        { 
                addr &= 0x7fffff;
                if (addr >= svga_vram_limit)
                   return 0xff;
                return vram[addr];
        }
        else        addr<<=2;
        
        addr &= 0x7fffff;
        
        if (addr >= svga_vram_limit)
           return 0xff;
           
        la=vram[addr];
        lb=vram[addr|0x1];
        lc=vram[addr|0x2];
        ld=vram[addr|0x3];
        if (readmode)
        {
                temp= (colournocare&1) ?0xFF:0;
                temp&=la;
                temp^=(colourcompare&1)?0xFF:0;
                temp2= (colournocare&2) ?0xFF:0;
                temp2&=lb;
                temp2^=(colourcompare&2)?0xFF:0;
                temp3= (colournocare&4) ?0xFF:0;
                temp3&=lc;
                temp3^=(colourcompare&4)?0xFF:0;
                temp4= (colournocare&8) ?0xFF:0;
                temp4&=ld;
                temp4^=(colourcompare&8)?0xFF:0;
                return ~(temp|temp2|temp3|temp4);
        }
//pclog("Read %02X %04X %04X\n",vram[addr|readplane],addr,readplane);
        return vram[addr|readplane];
}

void svga_write_linear(uint32_t addr, uint8_t val, void *priv)
{
        uint8_t vala,valb,valc,vald,wm=writemask;
        int writemask2 = writemask;

        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

        egawrites++;
        
        if (svga_output) pclog("Write LFB %08X %02X ", addr, val);
        if (!(gdcreg[6]&1)) fullchange=2;
        if (chain4)
        {
                writemask2=1<<(addr&3);
                addr&=~3;
        }
        else
        {
                addr<<=2;
        }
        addr &= 0x7fffff;
        if (svga_output) pclog("%08X\n", addr);
        changedvram[addr>>10]=changeframecount;
        
        switch (writemode)
        {
                case 1:
                if (writemask2&1) vram[addr]=la;
                if (writemask2&2) vram[addr|0x1]=lb;
                if (writemask2&4) vram[addr|0x2]=lc;
                if (writemask2&8) vram[addr|0x3]=ld;
                break;
                case 0:
                if (gdcreg[3]&7) val=svga_rotate[gdcreg[3]&7][val];
                if (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1])
                {
                        if (writemask2&1) vram[addr]=val;
                        if (writemask2&2) vram[addr|0x1]=val;
                        if (writemask2&4) vram[addr|0x2]=val;
                        if (writemask2&8) vram[addr|0x3]=val;
                }
                else
                {
                        if (gdcreg[1]&1) vala=(gdcreg[0]&1)?0xFF:0;
                        else             vala=val;
                        if (gdcreg[1]&2) valb=(gdcreg[0]&2)?0xFF:0;
                        else             valb=val;
                        if (gdcreg[1]&4) valc=(gdcreg[0]&4)?0xFF:0;
                        else             valc=val;
                        if (gdcreg[1]&8) vald=(gdcreg[0]&8)?0xFF:0;
                        else             vald=val;

                        switch (gdcreg[3]&0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                                if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                break;
                        }
//                        pclog("- %02X %02X %02X %02X   %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                }
                break;
                case 2:
                if (!(gdcreg[3]&0x18) && !gdcreg[1])
                {
                        if (writemask2&1) vram[addr]=(((val&1)?0xFF:0)&gdcreg[8])|(la&~gdcreg[8]);
                        if (writemask2&2) vram[addr|0x1]=(((val&2)?0xFF:0)&gdcreg[8])|(lb&~gdcreg[8]);
                        if (writemask2&4) vram[addr|0x2]=(((val&4)?0xFF:0)&gdcreg[8])|(lc&~gdcreg[8]);
                        if (writemask2&8) vram[addr|0x3]=(((val&8)?0xFF:0)&gdcreg[8])|(ld&~gdcreg[8]);
                }
                else
                {
                        vala=((val&1)?0xFF:0);
                        valb=((val&2)?0xFF:0);
                        valc=((val&4)?0xFF:0);
                        vald=((val&8)?0xFF:0);
                        switch (gdcreg[3]&0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                                if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                                if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                break;
                        }
                }
                break;
                case 3:
                if (gdcreg[3]&7) val=svga_rotate[gdcreg[3]&7][val];
                wm=gdcreg[8];
                gdcreg[8]&=val;

                vala=(gdcreg[0]&1)?0xFF:0;
                valb=(gdcreg[0]&2)?0xFF:0;
                valc=(gdcreg[0]&4)?0xFF:0;
                vald=(gdcreg[0]&8)?0xFF:0;
                switch (gdcreg[3]&0x18)
                {
                        case 0: /*Set*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                        break;
                        case 8: /*AND*/
                        if (writemask2&1) vram[addr]=(vala|~gdcreg[8])&la;
                        if (writemask2&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                        if (writemask2&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                        if (writemask2&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                        break;
                        case 0x10: /*OR*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])|la;
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                        break;
                        case 0x18: /*XOR*/
                        if (writemask2&1) vram[addr]=(vala&gdcreg[8])^la;
                        if (writemask2&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                        if (writemask2&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                        if (writemask2&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                        break;
                }
                gdcreg[8]=wm;
                break;
        }
}

uint8_t svga_read_linear(uint32_t addr, void *priv)
{
        uint8_t temp,temp2,temp3,temp4;
  
        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

        egareads++;
        
        if (chain4) 
        { 
                addr &= 0x7fffff;
                if (addr >= svga_vram_limit)
                   return 0xff;
                return vram[addr & 0x7fffff]; 
        }
        else        addr<<=2;

        addr &= 0x7fffff;
        
        if (addr >= svga_vram_limit)
           return 0xff;

        la=vram[addr];
        lb=vram[addr|0x1];
        lc=vram[addr|0x2];
        ld=vram[addr|0x3];
        if (readmode)
        {
                temp= (colournocare&1) ?0xFF:0;
                temp&=la;
                temp^=(colourcompare&1)?0xFF:0;
                temp2= (colournocare&2) ?0xFF:0;
                temp2&=lb;
                temp2^=(colourcompare&2)?0xFF:0;
                temp3= (colournocare&4) ?0xFF:0;
                temp3&=lc;
                temp3^=(colourcompare&4)?0xFF:0;
                temp4= (colournocare&8) ?0xFF:0;
                temp4&=ld;
                temp4^=(colourcompare&8)?0xFF:0;
                return ~(temp|temp2|temp3|temp4);
        }
//printf("Read %02X %04X %04X\n",vram[addr|readplane],addr,readplane);
        return vram[addr|readplane];
}

void svga_doblit(int y1, int y2)
{
        frames++;
//        pclog("doblit %i %i\n", y1, y2);
        if (y1 > y2) 
        {
                startblit();
                video_blit_memtoscreen(32, 0, 0, 0, xsize, ysize);
                endblit();
                return;   
        }     
        startblit();
        if ((wx!=xsize || wy!=ysize) && !vid_resize)
        {
                xsize=wx;
                ysize=wy+1;
                if (xsize<64) xsize=656;
                if (ysize<32) ysize=200;
/*                if (bpp > 8)
                {
                        if (vres) updatewindowsize(xsize<<1,ysize<<1);
                        else      updatewindowsize(xsize<<1,ysize);
                }
                else
                {*/
                        if (vres) updatewindowsize(xsize,ysize<<1);
                        else      updatewindowsize(xsize,ysize);
//                }
        }
        if (vid_resize)
        {
                xsize = wx;
                ysize = wy + 1;
        }
        video_blit_memtoscreen(32, 0, y1, y2, xsize, ysize);
        if (readflash) rectfill(screen,winsizex-40,8,winsizex-8,14,0xFFFFFFFF);
        endblit();
        
}

void svga_writew(uint32_t addr, uint16_t val, void *priv)
{
        if (!svga_fast)
        {
                svga_write(addr, val, priv);
                svga_write(addr + 1, val >> 8, priv);
                return;
        }
        
        egawrites += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

        if (svga_output) pclog("Writew %05X ", addr);
        addr = (addr & 0xffff) + svgawbank;
        addr &= 0x7FFFFF;        
        if (svga_output) pclog("%08X (%i, %i) %04X\n", addr, addr & 1023, addr >> 10, val);
        changedvram[addr >> 10] = changeframecount;
        *(uint16_t *)&vram[addr] = val;
}

void svga_writel(uint32_t addr, uint32_t val, void *priv)
{
        if (!svga_fast)
        {
                svga_write(addr, val, priv);
                svga_write(addr + 1, val >> 8, priv);
                svga_write(addr + 2, val >> 16, priv);
                svga_write(addr + 3, val >> 24, priv);
                return;
        }
        
        egawrites += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

        if (svga_output) pclog("Writel %05X ", addr);
        addr = (addr & 0xffff) + svgawbank;
        addr &= 0x7FFFFF;
        if (svga_output) pclog("%08X (%i, %i) %08X\n", addr, addr & 1023, addr >> 10, val);
        
        changedvram[addr >> 10] = changeframecount;
        *(uint32_t *)&vram[addr] = val;
}

uint16_t svga_readw(uint32_t addr, void *priv)
{
        if (!svga_fast)
           return svga_read(addr, priv) | (svga_read(addr + 1, priv) << 8);
        
        egareads += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

//        pclog("Readw %05X ", addr);
        addr = (addr & 0xffff) + svgarbank;
        addr &= 0x7FFFFF;
//        pclog("%08X %04X\n", addr, *(uint16_t *)&vram[addr]);
        if (addr >= svga_vram_limit) return 0xffff;
        
        return *(uint16_t *)&vram[addr];
}

uint32_t svga_readl(uint32_t addr, void *priv)
{
        if (!svga_fast)
           return svga_read(addr, priv) | (svga_read(addr + 1, priv) << 8) | (svga_read(addr + 2, priv) << 16) | (svga_read(addr + 3, priv) << 24);
        
        egareads += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

//        pclog("Readl %05X ", addr);
        addr = (addr & 0xffff) + svgarbank;
        addr &= 0x7FFFFF;
//        pclog("%08X %08X\n", addr, *(uint32_t *)&vram[addr]);
        if (addr >= svga_vram_limit) return 0xffffffff;
        
        return *(uint32_t *)&vram[addr];
}

void svga_writew_linear(uint32_t addr, uint16_t val, void *priv)
{
        if (!svga_fast)
        {
                svga_write_linear(addr, val, priv);
                svga_write_linear(addr + 1, val >> 8, priv);
                return;
        }
        
        egawrites += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

	if (svga_output) pclog("Write LFBw %08X %04X\n", addr, val);
        addr &= 0x7FFFFF;
        changedvram[addr >> 10] = changeframecount;
        *(uint16_t *)&vram[addr] = val;
}

void svga_writel_linear(uint32_t addr, uint32_t val, void *priv)
{
        if (!svga_fast)
        {
                svga_write_linear(addr, val, priv);
                svga_write_linear(addr + 1, val >> 8, priv);
                svga_write_linear(addr + 2, val >> 16, priv);
                svga_write_linear(addr + 3, val >> 24, priv);
                return;
        }
        
        egawrites += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

	if (svga_output) pclog("Write LFBl %08X %08X\n", addr, val);
        addr &= 0x7fffff;
        changedvram[addr >> 10] = changeframecount;
        *(uint32_t *)&vram[addr] = val;
}

uint16_t svga_readw_linear(uint32_t addr, void *priv)
{
        if (!svga_fast)
           return svga_read_linear(addr, priv) | (svga_read_linear(addr + 1, priv) << 8);
        
        egareads += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

        addr &= 0x7FFFFF;
        if (addr >= svga_vram_limit) return 0xffff;
        
        return *(uint16_t *)&vram[addr];
}

uint32_t svga_readl_linear(uint32_t addr, void *priv)
{
        if (!svga_fast)
           return svga_read_linear(addr, priv) | (svga_read_linear(addr + 1, priv) << 8) | (svga_read_linear(addr + 2, priv) << 16) | (svga_read_linear(addr + 3, priv) << 24);
        
        egareads += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

        addr &= 0x7FFFFF;
        if (addr >= svga_vram_limit) return 0xffffffff;

        return *(uint32_t *)&vram[addr];
}
