#include "ibm.h"
#include "video.h"

void doblit();

int egareads=0,egawrites=0;
int framecount=0;
int changeframecount=2;

void redotextlookup();
int output;
int svgaon=0;
uint32_t svgarbank,svgawbank;
uint8_t svgaseg,svgaseg2;
uint8_t svga3d8;

uint8_t oldvram=0;
int frames;
int incga=1;
int hsync;
uint8_t cgastat;

uint8_t gdcreg[16];
int gdcaddr;
uint8_t attrregs[32];
int attraddr,attrff=0;
uint8_t ega3c2;

uint8_t rotatevga[8][256];
uint8_t writemask,charset;
int writemode,readmode,readplane,chain4;
uint8_t colourcompare,colournocare;
uint8_t la,lb,lc,ld;

uint8_t *vram;

int egares;

uint8_t seqregs[32];
int seqaddr;

uint8_t oak_regs[32];
int oak_index;


extern int egaswitchread,egaswitches;
/*For VGA non-interlace colour, ID bits should be 1 1 0 or 6*/
int vres=0;

PALETTE vgapal;
uint32_t *pallook,pallook16[256],pallook64[256],pallook256[256];
int dacread,dacwrite,dacpos=0;
int palchange=1;

int fullchange;

float dispontime,dispofftime,disptime;

int ega_vtotal,ega_dispend,ega_vsyncstart,ega_split,ega_hdisp,ega_rowoffset;
int vidclock;
float cpuclock;

int bpp;

uint32_t vrammask;

uint8_t changedvram[(8192*1024)/1024];


int charseta,charsetb;

int egapal[16];

int displine;

int rowdbl=0;
int scrblank=0;




uint8_t edatlookup[4][4];

uint32_t textlookup[256][2][16][16];

/*void redotextlookup()
{
        int c,d,e;
        uint32_t temp;
        int coffset=(VGA)?0:64;
        for (c=0;c<256;c++)
        {
                for (d=0;d<16;d++)
                {
//                        printf("ATTR %i=%02X+%02X\n",d,attrregs[d],coffset);
                        for (e=0;e<16;e++)
                        {
                                temp=0;
                                if (c&0x80) temp|=(attrregs[d]+coffset);
                                else        temp|=(attrregs[e]+coffset);
                                if (c&0x40) temp|=(attrregs[d]+coffset)<<8;
                                else        temp|=(attrregs[e]+coffset)<<8;
                                if (c&0x20) temp|=(attrregs[d]+coffset)<<16;
                                else        temp|=(attrregs[e]+coffset)<<16;
                                if (c&0x10) temp|=(attrregs[d]+coffset)<<24;
                                else        temp|=(attrregs[e]+coffset)<<24;
//                                if (c==0x5) printf("%08X %i %i %02X %02X\n",temp,d,e,attrregs[d],attrregs[e]);
                                textlookup[c][0][d][e]=temp;
                                temp=0;
                                if (c&0x08) temp|=(attrregs[d]+coffset);
                                else        temp|=(attrregs[e]+coffset);
                                if (c&0x04) temp|=(attrregs[d]+coffset)<<8;
                                else        temp|=(attrregs[e]+coffset)<<8;
                                if (c&0x02) temp|=(attrregs[d]+coffset)<<16;
                                else        temp|=(attrregs[e]+coffset)<<16;
                                if (c&0x01) temp|=(attrregs[d]+coffset)<<24;
                                else        temp|=(attrregs[e]+coffset)<<24;
                                textlookup[c][1][d][e]=temp;
                        }
                }
        }
}*/

void initega()
{
        int c,d,e;
        bpp=8;
        for (c=0;c<256;c++)
        {
                e=c;
                for (d=0;d<8;d++)
                {
                        rotatevga[d][c]=e;
                        e=(e>>1)|((e&1)?0x80:0);
                }
        }
        crtc[0xC]=crtc[0xD]=0;
        if (romset==ROM_PC1640 || romset==ROM_PC1512) incga=1;
        else                    incga=0;
        for (c=0;c<4;c++)
        {
                for (d=0;d<4;d++)
                {
                        edatlookup[c][d]=0;
                        if (c&1) edatlookup[c][d]|=1;
                        if (d&1) edatlookup[c][d]|=2;
                        if (c&2) edatlookup[c][d]|=0x10;
                        if (d&2) edatlookup[c][d]|=0x20;
//                        printf("Edat %i,%i now %02X\n",c,d,edatlookup[c][d]);
                }
        }
        /*redotextlookup();*/
        for (c=0;c<256;c++)
        {
                pallook64[c]=makecol32(((c>>2)&1)*0xAA,((c>>1)&1)*0xAA,(c&1)*0xAA);
                pallook64[c]+=makecol32(((c>>5)&1)*0x55,((c>>4)&1)*0x55,((c>>3)&1)*0x55);
                pallook16[c]=makecol32(((c>>2)&1)*0xAA,((c>>1)&1)*0xAA,(c&1)*0xAA);
                pallook16[c]+=makecol32(((c>>4)&1)*0x55,((c>>4)&1)*0x55,((c>>4)&1)*0x55);
                if ((c&0x17)==6) pallook16[c]=makecol32(0xAA,0x55,0);
//                printf("%03i %08X\n",c,pallook[c]);
        }
        pallook=pallook16;
        seqregs[0xC]=0x20;
        vrammask=(ET4000)?0xFFFFF:0x1FFFFF;
//        writeega_func=writeega;
        gdcreg[6]=8;
        gdcreg[8]=0xFF;
        writemode=0;
        chain4=0;
        writemask=3;
        
        et4k_b8000=0;
        
        svgarbank=svgawbank=0;
}
uint32_t egabase,egaoffset;

void cacheega()
{
        egabase=(crtc[0xC]<<8)|crtc[0xD];
        if (ET4000 || ET4000W32 || TRIDENT)
           egabase|=((crtc[0x33]&3)<<18);
//        printf("EGABASE %05X\n",egabase);
//        egaoffset=8-((attrregs[0x13])&7);
//        printf("Cache base!\n");
}
void cacheega2()
{
//        egabase=(crtc[0xC]<<8)|crtc[0xD];
        if (gdcreg[5]&0x40) egaoffset=8-(((attrregs[0x13])&7)>>1);
        else                egaoffset=8-((attrregs[0x13])&7);
//        printf("Cache offset!\n");
}

int olddisplines,oldxsize;

int oldreadflash;
int oldegaaddr,oldegasplit;

int vc,sc;
int egadispon=0;
int linepos;
int displine;
uint32_t ma,maback,ca;
int firstline,lastline;
int xsize,ysize;
int scrollcache;
int con,cursoron,cgablink;

BITMAP *buffer,*vbuf,*buffer32;

int linecountff=0;

void dumpegaregs()
{
        int c;
        printf("CRTC :");
        for (c=0;c<0x20;c++) printf(" %02X",crtc[c]);
        printf("\n");
        printf(" EXT :");
        for (c=0;c<0x20;c++) printf(" %02X",crtc[c+32]);
        printf("\n");
        printf(" EXT2:");
        for (c=0;c<0x20;c++) printf(" %02X",crtc[c+64]);
        printf("\n");
        printf("SEQ  :");
        for (c=0;c<0x10;c++) printf(" %02X",seqregs[c]);
        printf("\n");
        printf(" EXT :");
        for (c=0;c<0x10;c++) printf(" %02X",seqregs[c + 0x10]);
        printf("\n");
        printf("ATTR :");
        for (c=0;c<0x20;c++) printf(" %02X",attrregs[c]);
        printf("\n");
        printf("GDC  :");
        for (c=0;c<0x10;c++) printf(" %02X",gdcreg[c]);
        printf("\n");
//        printf("OLD CTRL2 = %02X  NEW CTRL2 = %02X  DAC = %02X  3C2 = %02X\n",tridentoldctrl2,tridentnewctrl2,tridentdac,ega3c2);
        printf("BPP = %02X\n",bpp);
}

int oddeven;

int wx,wy;
int vslines;

int frames = 0;

void doblit()
{
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
        video_blit_memtoscreen(32, 0, 0, ysize, xsize, ysize);
        if (readflash) rectfill(screen,winsizex-40,8,winsizex-8,14,0xFFFFFFFF);
        endblit();
        frames++;
}

int ega_getdepth()
{
        if (!(gdcreg[6]&1)) return 0;
        switch (gdcreg[5]&0x60)
        {
                case 0x00:
                return 4;
                case 0x20:
                return 2;
                case 0x40: case 0x60:
                if (TRIDENT || ET4000W32) return bpp;
                return 8;
        }
}

int ega_getx()
{
        int x;
        if (!(gdcreg[6]&1)) return (crtc[1]+1);
        if ((attrregs[0x10]&0x40) && !(tridentoldctrl2&0x10)) x=(crtc[1]+1)*4;
        else                                                  x=(crtc[1]+1)*8;
        if (TRIDENT && (bpp==15 || bpp==16)) x>>=1;
        if (TRIDENT && bpp==24) x=(x<<1)/3;
        return x;
}

int ega_gety()
{
        if (crtc[0x1E]&4) return (ega_dispend/((crtc[9]&31)+1))<<1;
        if (crtc[9]&0x80) return (ega_dispend/((crtc[9]&31)+1))>>1;
        return ega_dispend/((crtc[9]&31)+1);
}
