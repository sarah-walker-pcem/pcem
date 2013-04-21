/*Generic SVGA handling*/
/*This is intended to be used by another SVGA driver, and not as a card in it's own right*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "io.h"

uint32_t svga_vram_limit;

extern uint8_t edatlookup[4][4];
uint8_t svga_miscout;

static uint8_t la, lb, lc, ld;
static uint8_t svga_rotate[8][256];

static int svga_fast;

void svga_out(uint16_t addr, uint8_t val)
{
        int c;
        uint8_t o;
        //printf("OUT SVGA %03X %02X %04X:%04X %i %08X\n",addr,val,CS,pc,TRIDENT,svgawbank);
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
                if (val&1)
                   io_sethandler(0x03a0, 0x0020, video_in,      NULL, NULL, video_out,      NULL, NULL);
                else
                   io_sethandler(0x03a0, 0x0020, video_in_null, NULL, NULL, video_out_null, NULL, NULL);
                break;
                case 0x3C4: seqaddr=val; break;
                case 0x3C5:
                if (seqaddr > 0xf) return;
                o=seqregs[seqaddr&0xF];
                seqregs[seqaddr&0xF]=val;
                if (o!=val && (seqaddr&0xF)==1) svga_recalctimings();
                switch (seqaddr&0xF)
                {
                        case 1: if (scrblank && !(val&0x20)) fullchange=3; scrblank=(scrblank&~0x20)|(val&0x20); break;
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
                case 0x3C7: dacread=val; dacpos=0; break;
                case 0x3C8: dacwrite=val; dacpos=0; break;
                case 0x3C9:
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
                switch (gdcaddr&15)
                {
                        case 2: colourcompare=val; break;
                        case 4: readplane=val&3; break;
                        case 5: writemode=val&3; readmode=val&8; break;
                        case 6:
                        if ((gdcreg[6] & 0xc) != (val & 0xc))
                        {
                                mem_removehandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
//                                pclog("Write mapping %02X\n", val);
                                switch (val&0xC)
                                {
                                        case 0x0: /*128k at A0000*/
                                        mem_sethandler(0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                                        break;
                                        case 0x4: /*64k at A0000*/
                                        mem_sethandler(0xa0000, 0x10000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                                        break;
                                        case 0x8: /*32k at B0000*/
                                        mem_sethandler(0xb0000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                                        break;
                                        case 0xC: /*32k at B8000*/
                                        mem_sethandler(0xb8000, 0x08000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
                                        break;
                                }
                        }
                        break;
                        case 7: colournocare=val; break;
                }
                gdcreg[gdcaddr&15]=val;                
                svga_fast = (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1]) && chain4;
                break;
        }
}

uint8_t svga_in(uint16_t addr)
{
        uint8_t temp;
//        if (addr!=0x3da) pclog("Read port %04X\n",addr);
        switch (addr)
        {
                case 0x3C0: return attraddr;
                case 0x3C1: return attrregs[attraddr];
                case 0x3C2: return 0x10;
                case 0x3C4: return seqaddr;
                case 0x3C5:
                return seqregs[seqaddr&0xF];
                case 0x3C8: return dacwrite;
                case 0x3C9:
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
int svga_hdisp,  svga_htotal,  svga_rowoffset;
int svga_lowres, svga_interlace;
double svga_clock;
uint32_t svga_ma;
void (*svga_recalctimings_ex)() = NULL;
void (*svga_hwcursor_draw)(int displine) = NULL;

void svga_recalctimings()
{
        double crtcconst;
        int temp;
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
        
        if (svga_recalctimings_ex) svga_recalctimings_ex();
        
        crtcconst=(seqregs[1]&1)?(svga_clock*8.0):(svga_clock*9.0);

        disptime  =svga_htotal;
        dispontime=svga_hdisp;

//        printf("Disptime %f dispontime %f hdisp %i\n",disptime,dispontime,crtc[1]*8);
        if (seqregs[1]&8) { disptime*=2; dispontime*=2; }
        dispofftime=disptime-dispontime;
        dispontime*=crtcconst;
        dispofftime*=crtcconst;

//        printf("SVGA horiz total %i display end %i clock rate %i vidclock %i %i\n",crtc[0],crtc[1],egaswitchread,vidclock,((ega3c2>>2)&3) | ((tridentnewctrl2<<2)&4));
//        printf("SVGA vert total %i display end %i max row %i vsync %i\n",svga_vtotal,svga_dispend,(crtc[9]&31)+1,svga_vsyncstart);
//        printf("total %f on %f cycles off %f cycles frame %f sec %f %02X\n",disptime*crtcconst,dispontime,dispofftime,(dispontime+dispofftime)*svga_vtotal,(dispontime+dispofftime)*svga_vtotal*70,seqregs[1]);
}


static uint32_t ma,maback,ca;
static int vc,sc;
static int linepos, displine, vslines, linecountff, oddeven;
static int svga_dispon;
static int con, coff, cursoron, cgablink;
static int scrollcache;

int svga_hwcursor_on;

SVGA_HWCURSOR svga_hwcursor, svga_hwcursor_latch;

int svga_hdisp_on;

static int firstline_draw = 2000, lastline_draw = 0;

void svga_poll()
{
        uint8_t chr,dat,attr;
        uint32_t charaddr;
        int x,xx;
        uint32_t fg,bg;
        int offset;
        uint8_t edat[4];
        int drawcursor=0;
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
                        if (scrblank)
                        {
                                if (firstline_draw == 2000) firstline_draw = displine;
                                lastline_draw = displine;
                                for (x=0;x<svga_hdisp;x++)
                                {
                                        switch (seqregs[1]&9)
                                        {
                                                case 0:
                                                for (xx=0;xx<9;xx++) ((uint32_t *)buffer32->line[displine])[(x*9)+xx+32]=0;
                                                break;
                                                case 1:
                                                for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[(x*8)+xx+32]=0;
                                                break;
                                                case 8:
                                                for (xx=0;xx<18;xx++) ((uint32_t *)buffer32->line[displine])[(x*18)+xx+32]=0;
                                                break;
                                                case 9:
                                                for (xx=0;xx<16;xx++) ((uint32_t *)buffer32->line[displine])[(x*16)+xx+32]=0;
                                                break;
                                        }
                                }
                        }
                        else if (!(gdcreg[6]&1)) /*Text mode*/
                        {
                                if (firstline_draw == 2000) firstline_draw = displine;
                                lastline_draw = displine;
                                if (fullchange)
                                {
                                        for (x=0;x<svga_hdisp;x++)
                                        {
                                                drawcursor=((ma==ca) && con && cursoron);
                                                chr=vram[(ma<<1)];
                                                attr=vram[(ma<<1)+4];
                                                if (attr&8) charaddr=charsetb+(chr*128);
                                                else        charaddr=charseta+(chr*128);

                                                if (drawcursor) { bg=pallook[egapal[attr&15]]; fg=pallook[egapal[attr>>4]]; }
                                                else
                                                {
                                                        fg=pallook[egapal[attr&15]];
                                                        bg=pallook[egapal[attr>>4]];
                                                        if (attr&0x80 && attrregs[0x10]&8)
                                                        {
                                                                bg=pallook[egapal[(attr>>4)&7]];
                                                                if (cgablink&16) fg=bg;
                                                        }
                                                }

                                                dat=vram[charaddr+(sc<<2)];
                                                if (seqregs[1]&8) /*40 column*/
                                                {
                                                        if (seqregs[1]&1) { for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[((x<<4)+32+(xx<<1))&2047]=((uint32_t *)buffer32->line[displine])[((x<<4)+33+(xx<<1))&2047]=(dat&(0x80>>xx))?fg:bg; }
                                                        else
                                                        {
                                                                for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[((x*18)+32+(xx<<1))&2047]=((uint32_t *)buffer32->line[displine])[((x*18)+33+(xx<<1))&2047]=(dat&(0x80>>xx))?fg:bg;
                                                                if ((chr&~0x1F)!=0xC0 || !(attrregs[0x10]&4)) ((uint32_t *)buffer32->line[displine])[((x*18)+32+16)&2047]=((uint32_t *)buffer32->line[displine])[((x*18)+32+17)&2047]=bg;
                                                                else                  ((uint32_t *)buffer32->line[displine])[((x*18)+32+16)&2047]=((uint32_t *)buffer32->line[displine])[((x*18)+32+17)&2047]=(dat&1)?fg:bg;
                                                        }
                                                }
                                                else             /*80 column*/
                                                {
                                                        if (seqregs[1]&1) { for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[((x<<3)+32+xx)&2047]=(dat&(0x80>>xx))?fg:bg; }
                                                        else
                                                        {
                                                                for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[((x*9)+32+xx)&2047]=(dat&(0x80>>xx))?fg:bg;
                                                                if ((chr&~0x1F)!=0xC0 || !(attrregs[0x10]&4)) ((uint32_t *)buffer32->line[displine])[((x*9)+32+8)&2047]=bg;
                                                                else                  ((uint32_t *)buffer32->line[displine])[((x*9)+32+8)&2047]=(dat&1)?fg:bg;
                                                        }
                                                }
                                                ma+=4; ma&=vrammask;
                                        }
                                }
                        }
                        else
                        {
//                                if (hwcursor_on) changedvram[ma>>10]=changedvram[(ma>>10)+1]=2;
                                switch (gdcreg[5]&0x60)
                                {
                                        case 0x00: /*16 colours*/
                                        if (firstline_draw == 2000) firstline_draw = displine;
                                        lastline_draw = displine;
                                        if (seqregs[1]&8) /*Low res (320)*/
                                        {
                                                offset=((8-scrollcache)<<1)+16;
                                                for (x=0;x<=svga_hdisp;x++)
                                                {
                                                        edat[0]=vram[ma];
                                                        edat[1]=vram[ma|0x1];
                                                        edat[2]=vram[ma|0x2];
                                                        edat[3]=vram[ma|0x3];
                                                        ma+=4; ma&=vrammask;

                                                        dat=edatlookup[edat[0]&3][edat[1]&3]|(edatlookup[edat[2]&3][edat[3]&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+14+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+15+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+12+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+13+offset]=pallook[egapal[dat>>4]];
                                                        dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+10+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+11+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+8+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+9+offset]=pallook[egapal[dat>>4]];
                                                        dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+6+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+7+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+4+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+5+offset]=pallook[egapal[dat>>4]];
                                                        dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+2+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+3+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+offset]=   ((uint32_t *)buffer32->line[displine])[(x<<4)+1+offset]=pallook[egapal[dat>>4]];
                                                }
                                        }
                                        else              /*High res (640)*/
                                        {
                                                offset=(8-scrollcache)+24;
                                                for (x=0;x<=svga_hdisp;x++)
                                                {
                                                        edat[0]=vram[ma];
                                                        edat[1]=vram[ma|0x1];
                                                        edat[2]=vram[ma|0x2];
                                                        edat[3]=vram[ma|0x3];
                                                        ma+=4; ma&=vrammask;

                                                        dat=edatlookup[edat[0]&3][edat[1]&3]|(edatlookup[edat[2]&3][edat[3]&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+7+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+6+offset]=pallook[egapal[dat>>4]];
                                                        dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+5+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+4+offset]=pallook[egapal[dat>>4]];
                                                        dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+3+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+2+offset]=pallook[egapal[dat>>4]];
                                                        dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+1+offset]=pallook[egapal[dat&0xF]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+offset]=  pallook[egapal[dat>>4]];
                                                }
                                        }
                                        break;
                                        case 0x20: /*4 colours*/
                                        if (firstline_draw == 2000) firstline_draw = displine;
                                        lastline_draw = displine;
                                        offset=((8-scrollcache)<<1)+16;
                                        /*Low res (320) only, though high res (640) should be possible*/
                                        for (x=0;x<=svga_hdisp;x++)
                                        {
                                                if (sc&1 && !(crtc[0x17]&1))
                                                {
                                                        edat[0]=vram[(ma<<1)+0x8000];
                                                        edat[1]=vram[(ma<<1)+0x8004];
                                                }
                                                else
                                                {
                                                        edat[0]=vram[(ma<<1)];
                                                        edat[1]=vram[(ma<<1)+4];
                                                }
                                                ma+=4; ma&=vrammask;

                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+14+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+15+offset]=pallook[egapal[edat[1]&3]];
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+12+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+13+offset]=pallook[egapal[(edat[1]>>2)&3]];
                                                dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+10+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+11+offset]=pallook[egapal[(edat[1]>>4)&3]];
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+8+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+9+offset]=pallook[egapal[(edat[1]>>6)&3]];
                                                dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+6+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+7+offset]=pallook[egapal[(edat[0]>>0)&3]];
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+4+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+5+offset]=pallook[egapal[(edat[0]>>2)&3]];
                                                dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+2+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+3+offset]=pallook[egapal[(edat[0]>>4)&3]];
                                                ((uint32_t *)buffer32->line[displine])[(x<<4)+offset]=   ((uint32_t *)buffer32->line[displine])[(x<<4)+1+offset]=pallook[egapal[(edat[0]>>6)&3]];
                                        }
                                        break;
                                        case 0x40: case 0x60: /*256 colours (and more, with high/true colour RAMDAC)*/
                                        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
                                        {
                                                if (firstline_draw == 2000) firstline_draw = displine;
                                                lastline_draw = displine;
                                                if (svga_lowres) /*Low res (320)*/
                                                {
//                                                        if (!displine) pclog("Lo res %i %i %i\n",bpp,svga_hdisp,(svga_hdisp<<3)/3);
                                                        offset=(8-(scrollcache&6))+24;
                                                        if (bpp==8)   /*Regular 8 bpp palette mode*/
                                                        {
                                                                for (x=0;x<=svga_hdisp;x++)
                                                                {
                                                                        edat[0]=vram[ma];
                                                                        edat[1]=vram[ma|0x1];
                                                                        edat[2]=vram[ma|0x2];
                                                                        edat[3]=vram[ma|0x3];
                                                                        ma+=4; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+6+offset]= ((uint32_t *)buffer32->line[displine])[(x<<3)+7+offset]=pallook[edat[3]];
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+4+offset]= ((uint32_t *)buffer32->line[displine])[(x<<3)+5+offset]=pallook[edat[2]];
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+2+offset]= ((uint32_t *)buffer32->line[displine])[(x<<3)+3+offset]=pallook[edat[1]];
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+offset]=   ((uint32_t *)buffer32->line[displine])[(x<<3)+1+offset]=pallook[edat[0]];
                                                                }
                                                        }
                                                        else if (bpp==16) /*16 bpp 565 mode*/
                                                        {
                                                                for (x=0;x<=svga_hdisp;x++)
                                                                {
                                                                        fg=vram[ma]|(vram[ma|0x1]<<8);
                                                                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                                                                        ma+=4; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<2)+2+offset]=((uint32_t *)buffer32->line[displine])[(x<<2)+3+offset]=((bg&31)<<3)|(((bg>>5)&63)<<10)|(((bg>>11)&31)<<19);
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<2)+0+offset]=((uint32_t *)buffer32->line[displine])[(x<<2)+1+offset]=((fg&31)<<3)|(((fg>>5)&63)<<10)|(((fg>>11)&31)<<19);
                                                                }
                                                        }
                                                        else if (bpp==15) /*15 bpp 555 mode*/
                                                        {
                                                                for (x=0;x<=svga_hdisp;x++)
                                                                {
                                                                        fg=vram[ma]|(vram[ma|0x1]<<8);
                                                                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                                                                        ma+=4; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+2+offset]=((uint32_t *)buffer32->line[displine])[(x<<2)+3+offset]=((bg&31)<<3)|(((bg>>5)&31)<<11)|(((bg>>10)&31)<<19);
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+0+offset]=((uint32_t *)buffer32->line[displine])[(x<<2)+1+offset]=((fg&31)<<3)|(((fg>>5)&31)<<11)|(((fg>>10)&31)<<19);
                                                                }
                                                        }
                                                        else if (bpp==24) /*24 bpp 888 mode*/
                                                        {
                                                                for (x=0;x<=((svga_hdisp<<3)/3);x++)
                                                                {
                                                                        fg=vram[ma]|(vram[ma+1]<<8)|(vram[ma+2]<<16);
                                                                        ma+=3; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+offset]=((uint32_t *)buffer32->line[displine])[(x<<1)+1+offset]=fg;
                                                                }
                                                        }
                                                        else if (bpp==32) /*32 bpp 8888 mode*/
                                                        {
                                                                for (x = 0; x <= svga_hdisp; x++)
                                                                {
                                                                        fg = vram[ma] | (vram[ma + 1] << 8) | (vram[ma + 2] << 16);
                                                                        ma += 4; ma &= vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x << 1) + offset] = ((uint32_t *)buffer32->line[displine])[(x << 1) + 1 + offset] = fg;
                                                                }
                                                        }
                                                }
                                                else    /*High res (SVGA only)*/
                                                {
//                                                        if (!displine) pclog("Hi res %i %i %i\n",bpp,svga_hdisp,(svga_hdisp<<3)/3);
                                                        offset=(8-((scrollcache&6)>>1))+24;
                                                        if (bpp==8)
                                                        {
                                                                for (x=0;x<=(svga_hdisp<<1);x++)
                                                                {
                                                                        edat[0]=vram[ma];
                                                                        edat[1]=vram[ma|0x1];
                                                                        edat[2]=vram[ma|0x2];
                                                                        edat[3]=vram[ma|0x3];
                                                                        ma+=4; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<2)+3+offset]=pallook[edat[3]];
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<2)+2+offset]=pallook[edat[2]];
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<2)+1+offset]=pallook[edat[1]];
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<2)+offset]=  pallook[edat[0]];
                                                                }
                                                        }
                                                        else if (bpp==16)
                                                        {
                                                                for (x=0;x<=(svga_hdisp<<1);x++)
                                                                {
                                                                        fg=vram[ma]|(vram[ma|0x1]<<8);
                                                                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                                                                        ma+=4; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+1+offset]=((bg&31)<<3)|(((bg>>5)&63)<<10)|(((bg>>11)&31)<<19);
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+0+offset]=((fg&31)<<3)|(((fg>>5)&63)<<10)|(((fg>>11)&31)<<19);
                                                                }
                                                        }
                                                        else if (bpp==15)
                                                        {
                                                                for (x=0;x<=(svga_hdisp<<1);x++)
                                                                {
                                                                        fg=vram[ma]|(vram[ma|0x1]<<8);
                                                                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                                                                        ma+=4; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+1+offset]=((bg&31)<<3)|(((bg>>5)&31)<<11)|(((bg>>10)&31)<<19);
                                                                        ((uint32_t *)buffer32->line[displine])[(x<<1)+0+offset]=((fg&31)<<3)|(((fg>>5)&31)<<11)|(((fg>>10)&31)<<19);
                                                                }
                                                        }
                                                        else if (bpp==24)
                                                        {
                                                                for (x=0;x<=((svga_hdisp<<3)/3);x++)
                                                                {
                                                                        fg=vram[ma]|(vram[ma+1]<<8)|(vram[ma+2]<<16);
                                                                        ma+=3; ma&=vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[x+offset]=fg;
                                                                }
                                                        }
                                                        else if (bpp==32)
                                                        {
                                                                for (x = 0; x <= (svga_hdisp<<2); x++)
                                                                {
                                                                        fg = vram[ma] | (vram[ma + 1] << 8) | (vram[ma + 2] << 16);
                                                                        ma += 4; ma &= vrammask;
                                                                        ((uint32_t *)buffer32->line[displine])[x + offset] = fg;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                }
                                if (svga_hwcursor_on)
                                {
                                        svga_hwcursor_draw(displine);
                                        svga_hwcursor_on--;
                                }
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
                        if ((crtc[9]&0x80) && !linecountff)
                        {
                                linecountff=1;
                                ma=maback;
                        }
                        else if (sc==(crtc[9]&31))
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
                        if (seqregs[1]&8) x=svga_hdisp*((seqregs[1]&1)?8:9)*2;
                        else              x=svga_hdisp*((seqregs[1]&1)?8:9);

                        if (bpp == 16 || bpp == 15) x /= 2;
                        if (bpp == 24)              x /= 3;
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
void svga_write(uint32_t addr, uint8_t val)
{
        int x,y;
        char s[2]={0,0};
        uint8_t vala,valb,valc,vald,wm=writemask;
        int writemask2 = writemask;
        int bankaddr;

        egawrites++;

        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

//        pclog("Writeega %06X   ",addr);
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
//        pclog("%08X %02X %i %i %i\n", addr, val, writemask2, writemode, chain4);
        addr&=0x7FFFFF;
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

uint8_t svga_read(uint32_t addr)
{
        uint8_t temp,temp2,temp3,temp4;
        uint32_t addr2;
        
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

void svga_write_linear(uint32_t addr, uint8_t val)
{
        int x,y;
        char s[2]={0,0};
        uint8_t vala,valb,valc,vald,wm=writemask;
        int writemask2 = writemask;
        int bankaddr;

        cycles -= video_timing_b;
        cycles_lost += video_timing_b;

        egawrites++;
        
//        pclog("Write LFB %08X %02X ", addr, val);
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
//        pclog("%08X\n", addr);
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

uint8_t svga_read_linear(uint32_t addr)
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
                if (vres) updatewindowsize(xsize,ysize<<1);
                else      updatewindowsize(xsize,ysize);
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

void svga_writew(uint32_t addr, uint16_t val)
{
        if (!svga_fast)
        {
                svga_write(addr, val);
                svga_write(addr + 1, val >> 8);
                return;
        }
        
        egawrites += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

//        pclog("Writew %05X ", addr);
        addr = (addr & 0xffff) + svgawbank;
        addr &= 0x7FFFFF;        
//        pclog("%08X %04X\n", addr, val);
        changedvram[addr >> 10] = changeframecount;
        *(uint16_t *)&vram[addr] = val;
}

void svga_writel(uint32_t addr, uint32_t val)
{
        if (!svga_fast)
        {
                svga_write(addr, val);
                svga_write(addr + 1, val >> 8);
                svga_write(addr + 2, val >> 16);
                svga_write(addr + 3, val >> 24);
                return;
        }
        
        egawrites += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

//        pclog("Writel %05X ", addr);
        addr = (addr & 0xffff) + svgawbank;
        addr &= 0x7FFFFF;
//        pclog("%08X %08X\n", addr, val);
        
        changedvram[addr >> 10] = changeframecount;
        *(uint32_t *)&vram[addr] = val;
}

uint16_t svga_readw(uint32_t addr)
{
        if (!svga_fast)
           return svga_read(addr) | (svga_read(addr + 1) << 8);
        
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

uint32_t svga_readl(uint32_t addr)
{
        if (!svga_fast)
           return svga_read(addr) | (svga_read(addr + 1) << 8) | (svga_read(addr + 2) << 16) | (svga_read(addr + 3) << 24);
        
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

void svga_writew_linear(uint32_t addr, uint16_t val)
{
        if (!svga_fast)
        {
                svga_write_linear(addr, val);
                svga_write_linear(addr + 1, val >> 8);
                return;
        }
        
        egawrites += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

        addr &= 0x7FFFFF;
        changedvram[addr >> 10] = changeframecount;
        *(uint16_t *)&vram[addr] = val;
}

void svga_writel_linear(uint32_t addr, uint32_t val)
{
        if (!svga_fast)
        {
                svga_write_linear(addr, val);
                svga_write_linear(addr + 1, val >> 8);
                svga_write_linear(addr + 2, val >> 16);
                svga_write_linear(addr + 3, val >> 24);
                return;
        }
        
        egawrites += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

        addr &= 0x7fffff;
        changedvram[addr >> 10] = changeframecount;
        *(uint32_t *)&vram[addr] = val;
}

uint16_t svga_readw_linear(uint32_t addr)
{
        if (!svga_fast)
           return svga_read_linear(addr) | (svga_read_linear(addr + 1) << 8);
        
        egareads += 2;

        cycles -= video_timing_w;
        cycles_lost += video_timing_w;

        addr &= 0x7FFFFF;
        if (addr >= svga_vram_limit) return 0xffff;
        
        return *(uint16_t *)&vram[addr];
}

uint32_t svga_readl_linear(uint32_t addr)
{
        if (!svga_fast)
           return svga_read_linear(addr) | (svga_read_linear(addr + 1) << 8) | (svga_read_linear(addr + 2) << 16) | (svga_read_linear(addr + 3) << 24);
        
        egareads += 4;

        cycles -= video_timing_l;
        cycles_lost += video_timing_l;

        addr &= 0x7FFFFF;
        if (addr >= svga_vram_limit) return 0xffffffff;

        return *(uint32_t *)&vram[addr];
}
