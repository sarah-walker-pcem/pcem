/*EGA emulation*/
#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "timer.h"
#include "video.h"
#include "vid_ega.h"
#include "vid_svga.h"

extern uint8_t edatlookup[4][4];

uint8_t ega_3c2;

static uint8_t la, lb, lc, ld;
static uint8_t ega_rotate[8][256];

/*3C2 controls default mode on EGA. On VGA, it determines monitor type (mono or colour)*/
int egaswitchread,egaswitches=9; /*7=CGA mode (200 lines), 9=EGA mode (350 lines), 8=EGA mode (200 lines)*/

void ega_out(uint16_t addr, uint8_t val, void *priv)
{
        int c;
        uint8_t o,old;
        if ((addr&0xFFF0) == 0x3B0) addr |= 0x20;
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
                }
                attrff^=1;
                break;
                case 0x3C2:
                egaswitchread=val&0xC;
                vres=!(val&0x80);
                pallook=(vres)?pallook16:pallook64;
                vidclock=val&4; /*printf("3C2 write %02X\n",val);*/
                ega_3c2=val;
                break;
                case 0x3C4: seqaddr=val; break;
                case 0x3C5:
                o=seqregs[seqaddr&0xF];
                seqregs[seqaddr&0xF]=val;
                if (o!=val && (seqaddr&0xF)==1) ega_recalctimings();
                switch (seqaddr&0xF)
                {
                        case 1: if (scrblank && !(val&0x20)) fullchange=3; scrblank=(scrblank&~0x20)|(val&0x20); break;
                        case 2: writemask=val&0xF; break;
                        case 3:
                        charset=val&0xF;
                        charseta=((charset>>2)*0x10000)+2;
                        charsetb=((charset&3) *0x10000)+2;
                        break;
                }
                break;
                case 0x3CE: gdcaddr=val; break;
                case 0x3CF:
                gdcreg[gdcaddr&15]=val;
                switch (gdcaddr&15)
                {
                        case 2: colourcompare=val; break;
                        case 4: readplane=val&3; break;
                        case 5: writemode=val&3; readmode=val&8; break;
                        case 6:
                        mem_removehandler(0xa0000, 0x20000, ega_read, NULL, NULL, ega_write, NULL, NULL,        NULL);
//                                pclog("Write mapping %02X\n", val);
                        switch (val&0xC)
                        {
                                case 0x0: /*128k at A0000*/
                                mem_sethandler(0xa0000, 0x20000, ega_read, NULL, NULL, ega_write, NULL, NULL,   NULL);
                                break;
                                case 0x4: /*64k at A0000*/
                                mem_sethandler(0xa0000, 0x10000, ega_read, NULL, NULL, ega_write, NULL, NULL,   NULL);
                                break;
                                case 0x8: /*32k at B0000*/
                                mem_sethandler(0xb0000, 0x08000, ega_read, NULL, NULL, ega_write, NULL, NULL,   NULL);
                                break;
                                case 0xC: /*32k at B8000*/
                                mem_sethandler(0xb8000, 0x08000, ega_read, NULL, NULL, ega_write, NULL, NULL,   NULL);
                                break;
                        }

                        break;
                        case 7: colournocare=val; break;
                }
                break;
                case 0x3D4:
                crtcreg=val;
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
                                ega_recalctimings();
                        }
                }
                break;
        }
}

uint8_t ega_in(uint16_t addr, void *priv)
{
        if ((addr&0xFFF0) == 0x3B0) addr |= 0x20;
        switch (addr)
        {
                case 0x3C0: return attraddr;
                case 0x3C1: return attrregs[attraddr];
                case 0x3C2:
//                printf("Read egaswitch %02X %02X %i\n",egaswitchread,egaswitches,VGA);
                switch (egaswitchread)
                {
                        case 0xC: return (egaswitches&1)?0x10:0;
                        case 0x8: return (egaswitches&2)?0x10:0;
                        case 0x4: return (egaswitches&4)?0x10:0;
                        case 0x0: return (egaswitches&8)?0x10:0;
                }
                break;
                case 0x3C4: return seqaddr;
                case 0x3C5:
                return seqregs[seqaddr&0xF];
                case 0x3CE: return gdcaddr;
                case 0x3CF:
                return gdcreg[gdcaddr&0xF];
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
                case 0x3DA:
                attrff=0;
                cgastat^=0x30; /*Fools IBM EGA video BIOS self-test*/
                return cgastat;
        }
//        printf("Bad EGA read %04X %04X:%04X\n",addr,cs>>4,pc);
        return 0xFF;
}

static int linepos,displine,vslines;
static int egadispon;
static uint32_t ma,ca,maback;
static int vc,sc;
static int con,cursoron,cgablink;
static int scrollcache;
#define vrammask 0x3FFFF

void ega_recalctimings()
{
	double _dispontime, _dispofftime;
        double crtcconst;

        ega_vtotal=crtc[6];
        ega_dispend=crtc[0x12];
        ega_vsyncstart=crtc[0x10];
        ega_split=crtc[0x18];

        if (crtc[7]&1) ega_vtotal|=0x100;
        if (crtc[7]&32) ega_vtotal|=0x200;
        ega_vtotal++;

        if (crtc[7]&2) ega_dispend|=0x100;
        if (crtc[7]&64) ega_dispend|=0x200;
        ega_dispend++;

        if (crtc[7]&4) ega_vsyncstart|=0x100;
        if (crtc[7]&128) ega_vsyncstart|=0x200;
        ega_vsyncstart++;

        if (crtc[7]&0x10)    ega_split|=0x100;
        if (crtc[9]&0x40)    ega_split|=0x200;
        ega_split+=2;

        ega_hdisp=crtc[1];
        ega_hdisp++;

        ega_rowoffset=crtc[0x13];

        printf("Recalc! %i %i %i %i   %i %02X\n",ega_vtotal,ega_dispend,ega_vsyncstart,ega_split,ega_hdisp,attrregs[0x16]);

        if (vidclock) crtcconst=(seqregs[1]&1)?(MDACONST*(8.0/9.0)):MDACONST;
        else          crtcconst=(seqregs[1]&1)?(CGACONST*(8.0/9.0)):CGACONST;

        disptime=crtc[0]+2;
        _dispontime=crtc[1]+1;

        printf("Disptime %f dispontime %f hdisp %i\n",disptime,_dispontime,crtc[1]*8);
        if (seqregs[1]&8) { disptime*=2; _dispontime*=2; }
        _dispofftime=disptime-_dispontime;
        _dispontime*=crtcconst;
        _dispofftime*=crtcconst;

	dispontime = (int)(_dispontime * (1 << TIMER_SHIFT));
	dispofftime = (int)(_dispofftime * (1 << TIMER_SHIFT));

//        printf("EGA horiz total %i display end %i clock rate %i vidclock %i %i\n",crtc[0],crtc[1],egaswitchread,vidclock,((ega3c2>>2)&3) | ((tridentnewctrl2<<2)&4));
//        printf("EGA vert total %i display end %i max row %i vsync %i\n",ega_vtotal,ega_dispend,(crtc[9]&31)+1,ega_vsyncstart);
//        printf("total %f on %f cycles off %f cycles frame %f sec %f %02X\n",disptime*crtcconst,dispontime,dispofftime,(dispontime+dispofftime)*ega_vtotal,(dispontime+dispofftime)*ega_vtotal*70,seqregs[1]);
}

void ega_poll()
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
                vidtime+=dispofftime;

                cgastat|=1;
                linepos=1;

                if (egadispon)
                {
                        if (firstline==2000) firstline=displine;

                        if (scrblank)
                        {
                                for (x=0;x<ega_hdisp;x++)
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
                        else if (!(gdcreg[6]&1))
                        {
                                if (fullchange)
                                {
                                        for (x=0;x<ega_hdisp;x++)
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
                                                if (seqregs[1]&8)
                                                {
                                                        if (seqregs[1]&1) { for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[((x<<4)+32+(xx<<1))&2047]=((uint32_t *)buffer32->line[displine])[((x<<4)+33+(xx<<1))&2047]=(dat&(0x80>>xx))?fg:bg; }
                                                        else
                                                        {
                                                                for (xx=0;xx<8;xx++) ((uint32_t *)buffer32->line[displine])[((x*18)+32+(xx<<1))&2047]=((uint32_t *)buffer32->line[displine])[((x*18)+33+(xx<<1))&2047]=(dat&(0x80>>xx))?fg:bg;
                                                                if ((chr&~0x1F)!=0xC0 || !(attrregs[0x10]&4)) ((uint32_t *)buffer32->line[displine])[((x*18)+32+16)&2047]=((uint32_t *)buffer32->line[displine])[((x*18)+32+17)&2047]=bg;
                                                                else                  ((uint32_t *)buffer32->line[displine])[((x*18)+32+16)&2047]=((uint32_t *)buffer32->line[displine])[((x*18)+32+17)&2047]=(dat&1)?fg:bg;
                                                        }
                                                }
                                                else
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
                                switch (gdcreg[5]&0x20)
                                {
                                        case 0x00:
                                        if (seqregs[1]&8)
                                        {
                                                offset=((8-scrollcache)<<1)+16;
                                                for (x=0;x<=ega_hdisp;x++)
                                                {
                                                        if (sc&1 && !(crtc[0x17]&1))
                                                        {
                                                                edat[0] = vram[ma | 0x8000];
                                                                edat[1] = vram[ma | 0x8001];
                                                                edat[2] = vram[ma | 0x8002];
                                                                edat[3] = vram[ma | 0x8003];
                                                        }
                                                        else
                                                        {
                                                                edat[0] = vram[ma];
                                                                edat[1] = vram[ma | 0x1];
                                                                edat[2] = vram[ma | 0x2];
                                                                edat[3] = vram[ma | 0x3];
                                                        }
                                                        ma+=4; ma&=vrammask;

                                                        dat=edatlookup[edat[0]&3][edat[1]&3]|(edatlookup[edat[2]&3][edat[3]&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+14+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+15+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+12+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+13+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                        dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+10+offset]=((uint32_t *)buffer32->line[displine])[(x<<4)+11+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+8+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+9+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                        dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+6+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+7+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+4+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+5+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                        dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+2+offset]= ((uint32_t *)buffer32->line[displine])[(x<<4)+3+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<4)+offset]=   ((uint32_t *)buffer32->line[displine])[(x<<4)+1+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                }
                                        }
                                        else
                                        {
                                                offset=(8-scrollcache)+24;
                                                for (x=0;x<=ega_hdisp;x++)
                                                {
                                                        if (sc&1 && !(crtc[0x17]&1))
                                                        {
                                                                edat[0] = vram[ma | 0x8000];
                                                                edat[1] = vram[ma | 0x8001];
                                                                edat[2] = vram[ma | 0x8002];
                                                                edat[3] = vram[ma | 0x8003];
                                                        }
                                                        else
                                                        {
                                                                edat[0] = vram[ma];
                                                                edat[1] = vram[ma | 0x1];
                                                                edat[2] = vram[ma | 0x2];
                                                                edat[3] = vram[ma | 0x3];
                                                        }
                                                        ma+=4; ma&=vrammask;

                                                        dat=edatlookup[edat[0]&3][edat[1]&3]|(edatlookup[edat[2]&3][edat[3]&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+7+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+6+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                        dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+5+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+4+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                        dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+3+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+2+offset]=pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                        dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+1+offset]=pallook[egapal[(dat & 0xF) & attrregs[0x12]]];
                                                        ((uint32_t *)buffer32->line[displine])[(x<<3)+offset]=  pallook[egapal[(dat >> 4)  & attrregs[0x12]]];
                                                }
                                        }
                                        break;
                                        case 0x20:
                                        offset=((8-scrollcache)<<1)+16;
                                        for (x=0;x<=ega_hdisp;x++)
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
                                }
                        }
                        if (lastline<displine) lastline=displine;
                }

                displine++;
                if ((cgastat&8) && ((displine&15)==(crtc[0x11]&15)) && vslines)
                   cgastat&=~8;
                vslines++;
                if (displine>500) displine=0;
        }
        else
        {
                vidtime+=dispontime;
//                if (output) printf("Display on %f\n",vidtime);
                if (egadispon) cgastat&=~1;
                linepos=0;
                if (sc==(crtc[11]&31)) 
                   con=0; 
                if (egadispon)
                {
                        if (sc==(crtc[9]&31))
                        {
                                sc=0;

                                maback+=(ega_rowoffset<<3);
                                maback&=vrammask;
                                ma=maback;
                        }
                        else
                        {
                                sc++;
                                sc&=31;
                                ma=maback;
                        }
                }
                vc++;
                vc&=1023;
//                printf("Line now %i %i ma %05X\n",vc,displine,ma);
                if (vc==ega_split)
                {
//                        printf("Split at line %i %i\n",displine,vc);
                        ma=maback=0;
                        if (attrregs[0x10]&0x20) scrollcache=0;
                }
                if (vc==ega_dispend)
                {
//                        printf("Display over at line %i %i\n",displine,vc);
                        egadispon=0;
                        if (crtc[10] & 0x20) cursoron=0;
                        else                 cursoron=cgablink&16;
                        if (!(gdcreg[6]&1) && !(cgablink&15)) fullchange=2;
                        cgablink++;

                        for (x=0;x<2048;x++) if (changedvram[x]) changedvram[x]--;
//                        memset(changedvram,0,2048);
                        if (fullchange) fullchange--;
                }
                if (vc==ega_vsyncstart)
                {
                        egadispon=0;
//                        printf("Vsync on at line %i %i\n",displine,vc);
                        cgastat|=8;
                        if (seqregs[1]&8) x=ega_hdisp*((seqregs[1]&1)?8:9)*2;
                        else              x=ega_hdisp*((seqregs[1]&1)?8:9);
                        wx=x;
                        wy=lastline-firstline;
//                        pclog("Cursor %02X %02X\n",crtc[10],crtc[11]);
//                        pclog("Firstline %i Lastline %i wx %i %i\n",firstline,lastline,wx,oddeven);
//                        doblit();
                        svga_doblit(firstline, lastline + 1);

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
                                if (seqregs[1] & 8)
                                   video_res_x /= 2;
                                video_bpp = (gdcreg[5] & 0x20) ? 2 : 4;
                        }

//                        wakeupblit();
                        readflash=0;
                        //framecount++;
                        firstline=2000;
                        lastline=0;

                        maback=ma=(crtc[0xC]<<8)|crtc[0xD];
                        ca=(crtc[0xE]<<8)|crtc[0xF];
                        ma<<=2;
                        maback<<=2;
                        ca<<=2;
                        changeframecount=2;
                        vslines=0;
                }
                if (vc==ega_vtotal)
                {
                        vc=0;
                        sc=0;
                        egadispon=1;
                        displine=0;
                        scrollcache=attrregs[0x13]&7;
                }
                if (sc == (crtc[10] & 31)) con=1;
        }
}


void ega_write(uint32_t addr, uint8_t val, void *priv)
{
        uint8_t vala,valb,valc,vald;

        egawrites++;
        cycles -= video_timing_b;
        cycles_lost += video_timing_b;
        
        if (addr>=0xB0000) addr &= 0x7fff;
        else               addr &= 0xffff;

        if (!(gdcreg[6]&1)) fullchange=2;
        addr <<= 2;

//        pclog("%i %08X %i %i %02X   %02X %02X %02X %02X\n",chain4,addr,writemode,writemask,gdcreg[8],vram[0],vram[1],vram[2],vram[3]);
        switch (writemode)
                {
                        case 1:
                        if (writemask&1) vram[addr]=la;
                        if (writemask&2) vram[addr|0x1]=lb;
                        if (writemask&4) vram[addr|0x2]=lc;
                        if (writemask&8) vram[addr|0x3]=ld;
                        break;
                        case 0:
                        if (gdcreg[3]&7) val=ega_rotate[gdcreg[3]&7][val];
                        if (gdcreg[8]==0xFF && !(gdcreg[3]&0x18) && !gdcreg[1])
                        {
//                                pclog("Easy write %05X %02X\n",addr,val);
                                if (writemask&1) vram[addr]=val;
                                if (writemask&2) vram[addr|0x1]=val;
                                if (writemask&4) vram[addr|0x2]=val;
                                if (writemask&8) vram[addr|0x3]=val;
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
//                                pclog("Write %02X %01X %02X %02X %02X %02X  %02X\n",gdcreg[3]&0x18,writemask,vala,valb,valc,vald,gdcreg[8]);
                                switch (gdcreg[3]&0x18)
                                {
                                        case 0: /*Set*/
                                        if (writemask&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                        if (writemask&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                        if (writemask&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                        if (writemask&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                        break;
                                        case 8: /*AND*/
                                        if (writemask&1) vram[addr]=(vala|~gdcreg[8])&la;
                                        if (writemask&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                        if (writemask&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                        if (writemask&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                        break;
                                        case 0x10: /*OR*/
                                        if (writemask&1) vram[addr]=(vala&gdcreg[8])|la;
                                        if (writemask&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                        if (writemask&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                        if (writemask&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                        break;
                                        case 0x18: /*XOR*/
                                        if (writemask&1) vram[addr]=(vala&gdcreg[8])^la;
                                        if (writemask&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                        if (writemask&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                        if (writemask&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                        break;
                                }
//                                pclog("- %02X %02X %02X %02X   %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                        }
                        break;
                        case 2:
                        if (!(gdcreg[3]&0x18) && !gdcreg[1])
                        {
                                if (writemask&1) vram[addr]=(((val&1)?0xFF:0)&gdcreg[8])|(la&~gdcreg[8]);
                                if (writemask&2) vram[addr|0x1]=(((val&2)?0xFF:0)&gdcreg[8])|(lb&~gdcreg[8]);
                                if (writemask&4) vram[addr|0x2]=(((val&4)?0xFF:0)&gdcreg[8])|(lc&~gdcreg[8]);
                                if (writemask&8) vram[addr|0x3]=(((val&8)?0xFF:0)&gdcreg[8])|(ld&~gdcreg[8]);
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
                                        if (writemask&1) vram[addr]=(vala&gdcreg[8])|(la&~gdcreg[8]);
                                        if (writemask&2) vram[addr|0x1]=(valb&gdcreg[8])|(lb&~gdcreg[8]);
                                        if (writemask&4) vram[addr|0x2]=(valc&gdcreg[8])|(lc&~gdcreg[8]);
                                        if (writemask&8) vram[addr|0x3]=(vald&gdcreg[8])|(ld&~gdcreg[8]);
                                        break;
                                        case 8: /*AND*/
                                        if (writemask&1) vram[addr]=(vala|~gdcreg[8])&la;
                                        if (writemask&2) vram[addr|0x1]=(valb|~gdcreg[8])&lb;
                                        if (writemask&4) vram[addr|0x2]=(valc|~gdcreg[8])&lc;
                                        if (writemask&8) vram[addr|0x3]=(vald|~gdcreg[8])&ld;
                                        break;
                                        case 0x10: /*OR*/
                                        if (writemask&1) vram[addr]=(vala&gdcreg[8])|la;
                                        if (writemask&2) vram[addr|0x1]=(valb&gdcreg[8])|lb;
                                        if (writemask&4) vram[addr|0x2]=(valc&gdcreg[8])|lc;
                                        if (writemask&8) vram[addr|0x3]=(vald&gdcreg[8])|ld;
                                        break;
                                        case 0x18: /*XOR*/
                                        if (writemask&1) vram[addr]=(vala&gdcreg[8])^la;
                                        if (writemask&2) vram[addr|0x1]=(valb&gdcreg[8])^lb;
                                        if (writemask&4) vram[addr|0x2]=(valc&gdcreg[8])^lc;
                                        if (writemask&8) vram[addr|0x3]=(vald&gdcreg[8])^ld;
                                        break;
                                }
                        }
                        break;
                }
}

uint8_t ega_read(uint32_t addr, void *priv)
{
        uint8_t temp,temp2,temp3,temp4;
        egareads++;
        cycles -= video_timing_b;
        cycles_lost += video_timing_b;
//        pclog("Readega %06X   ",addr);
        if (addr>=0xB0000) addr &= 0x7fff;
        else               addr &= 0xffff;
        addr<<=2;
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
        return vram[addr|readplane];
}

int ega_init()
{
        int c, d, e;
        for (c = 0; c < 256; c++)
        {
                e = c;
                for (d = 0; d < 8; d++)
                {
                        ega_rotate[d][c] = e;
                        e = (e >> 1) | ((e & 1) ? 0x80 : 0);
                }
        }
        return 0;
}

GFXCARD vid_ega =
{
        ega_init,
        /*IO at 3Cx/3Dx*/
        ega_out,
        ega_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,

        ega_poll,
        ega_recalctimings,

        ega_write,
        video_write_null,
        video_write_null,

        ega_read,
        video_read_null,
        video_read_null
};

