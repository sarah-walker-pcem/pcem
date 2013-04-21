#include <stdio.h>
#include <math.h>
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "io.h"
#include "cpu.h"

/*Video timing settings -

8-bit - 1mb/sec
        B = 8 ISA clocks
        W = 16 ISA clocks
        L = 32 ISA clocks
        
Slow 16-bit - 2mb/sec
        B = 6 ISA clocks
        W = 8 ISA clocks
        L = 16 ISA clocks

Fast 16-bit - 4mb/sec
        B = 3 ISA clocks
        W = 3 ISA clocks
        L = 6 ISA clocks
        
Slow VLB/PCI - 8mb/sec (ish)
        B = 4 bus clocks
        W = 8 bus clocks
        L = 16 bus clocks
        
Mid VLB/PCI -
        B = 4 bus clocks
        W = 5 bus clocks
        L = 10 bus clocks
        
Fast VLB/PCI -
        B = 3 bus clocks
        W = 3 bus clocks
        L = 4 bus clocks
*/

enum
{
        VIDEO_ISA = 0,
        VIDEO_BUS
};

int video_speed = 0;
int video_timing[6][4] =
{
        {VIDEO_ISA, 8, 16, 32},
        {VIDEO_ISA, 6,  8, 16},
        {VIDEO_ISA, 3,  3,  6},
        {VIDEO_BUS, 4,  8, 16},
        {VIDEO_BUS, 4,  5, 10},
        {VIDEO_BUS, 3,  3,  4}
};

void video_updatetiming()
{
        if (video_timing[video_speed][0] == VIDEO_ISA)
        {
                video_timing_b = (int)(isa_timing * video_timing[video_speed][1]);
                video_timing_w = (int)(isa_timing * video_timing[video_speed][2]);
                video_timing_l = (int)(isa_timing * video_timing[video_speed][3]);
        }
        else
        {
                video_timing_b = (int)(bus_timing * video_timing[video_speed][1]);
                video_timing_w = (int)(bus_timing * video_timing[video_speed][2]);
                video_timing_l = (int)(bus_timing * video_timing[video_speed][3]);
        }
        if (cpu_16bitbus)
           video_timing_l = video_timing_w * 2;
}

int video_timing_b, video_timing_w, video_timing_l;

int video_res_x, video_res_y, video_bpp;

void (*video_blit_memtoscreen)(int x, int y, int y1, int y2, int w, int h);
void (*video_blit_memtoscreen_8)(int x, int y, int w, int h);

void (*video_out)     (uint16_t addr, uint8_t val);
void (*video_mono_out)(uint16_t addr, uint8_t val);
uint8_t (*video_in)     (uint16_t addr);
uint8_t (*video_mono_in)(uint16_t addr);

void (*video_write_a000)(uint32_t addr, uint8_t val);
void (*video_write_b000)(uint32_t addr, uint8_t val);
void (*video_write_b800)(uint32_t addr, uint8_t val);

void (*video_write_a000_w)(uint32_t addr, uint16_t val);
void (*video_write_a000_l)(uint32_t addr, uint32_t val);

uint8_t (*video_read_a000)(uint32_t addr);
uint8_t (*video_read_b000)(uint32_t addr);
uint8_t (*video_read_b800)(uint32_t addr);

void (*video_recalctimings)();

void video_out_null(uint16_t addr, uint8_t val)
{
}

uint8_t video_in_null(uint16_t addr)
{
        return 0xFF;
}

void video_write_null(uint32_t addr, uint8_t val)
{
}

uint8_t video_read_null(uint32_t addr)
{
        return 0xff;
}

void video_load(GFXCARD g)
{
        io_sethandler(0x03a0, 0x0020, g.mono_in, NULL, NULL, g.mono_out, NULL, NULL);
        io_sethandler(0x03c0, 0x0020, g.in,      NULL, NULL, g.out,      NULL, NULL);        
        
        video_out      = g.out;
        video_in       = g.in;
        video_mono_out = g.mono_out;
        video_mono_in  = g.mono_in;
        
        pollvideo = g.poll;
        video_recalctimings = g.recalctimings;
        
        video_write_a000 = g.write_a000;
        video_write_b000 = g.write_b000;
        video_write_b800 = g.write_b800;

        video_read_a000  = g.read_a000;
        video_read_b000  = g.read_b000;
        video_read_b800  = g.read_b800;
        
        video_write_a000_w = video_write_a000_l = NULL;
        
        g.init();
}

void video_init()
{
        pclog("Video_init %i %i\n",romset,gfxcard);

        switch (romset)
        {
                case ROM_TANDY:
                video_load(vid_tandy);
                return;

                case ROM_PC1512:
                video_load(vid_pc1512);
                return;
                
                case ROM_PC1640:
                video_load(vid_pc1640);
                return;
                
                case ROM_PC200:
                video_load(vid_pc200);
                return;
                
                case ROM_OLIM24:
                video_load(vid_m24);
                return;

                case ROM_PC2086:
                case ROM_PC3086:
                case ROM_MEGAPC:
                video_load(vid_paradise);
                return;
                        
                case ROM_ACER386:
                video_load(vid_oti067);
                return;
        }
        switch (gfxcard)
        {
                case GFX_MDA:
                video_load(vid_mda);
                break;

                case GFX_HERCULES:
                video_load(vid_hercules);
                break;
                
                case GFX_CGA:
                video_load(vid_cga);
                break;
                
                case GFX_EGA:
                video_load(vid_ega);
                break;
                
                case GFX_TVGA:
                video_load(vid_tvga);
                break;
                
                case GFX_ET4000:
                video_load(vid_et4000);
                break;
                
                case GFX_ET4000W32:
                video_load(vid_et4000w32p);
                break;

                case GFX_BAHAMAS64:
                video_load(vid_s3);
                break;

                case GFX_N9_9FX:
                video_load(vid_s3);
                break;
                
                case GFX_STEALTH64:
                video_load(vid_s3);
                break;
                
                default:
                fatal("Bad gfx card %i\n",gfxcard);
        }
}

uint32_t cga32[256];

BITMAP *buffer,*vbuf;//,*vbuf2;
BITMAP *buffer32;
int speshul=0;

uint8_t fontdat[256][8];
uint8_t fontdatm[256][16];

float dispontime,dispofftime,disptime;
int svgaon;
uint8_t cgamode=1,cgastat,cgacol=7,ocgamode;
int linepos=0;
int output;
uint8_t *vram,*ram;

int cgadispon=0;
int cgablink;

uint8_t port3de,port3dd,port3df;

uint8_t crtc[128],crtcreg;
uint8_t crtcmask[128]={0xFF,0xFF,0xFF,0xFF,0x7F,0x1F,0x7F,0x7F,0xF3,0x1F,0x7F,0x1F,0x3F,0xFF,0x3F,0xFF,0xFF,0xFF};
uint8_t charbuffer[256];
int crtcams[64]={0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1};
int nmi=0;


int xsize=1,ysize=1;
int firstline=1000,lastline=0;

int ntsc_col[8][8]=
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


extern int fullchange;

extern uint32_t vrammask;


int mdacols[256][2][2];
uint8_t gdcreg[16];


int cga4pal[8][4]=
{
        {0,2,4,6},{0,3,5,7},{0,3,4,7},{0,3,4,7},
        {0,10,12,14},{0,11,13,15},{0,11,12,15},{0,11,12,15}
};


PALETTE cgapal=
{
        {0,0,0},{0,42,0},{42,0,0},{42,21,0},
        {0,0,0},{0,42,42},{42,0,42},{42,42,42},
        {0,0,0},{21,63,21},{63,21,21},{63,63,21},
        {0,0,0},{21,63,63},{63,21,63},{63,63,63},

        {0,0,0},{0,0,42},{0,42,0},{0,42,42},
        {42,0,0},{42,0,42},{42,21,00},{42,42,42},
        {21,21,21},{21,21,63},{21,63,21},{21,63,63},
        {63,21,21},{63,21,63},{63,63,21},{63,63,63},

        {0,0,0},{0,21,0},{0,0,42},{0,42,42},
        {42,0,21},{21,10,21},{42,0,42},{42,0,63},
        {21,21,21},{21,63,21},{42,21,42},{21,63,63},
        {63,0,0},{42,42,0},{63,21,42},{41,41,41},
        
        {0,0,0},{0,42,42},{42,0,0},{42,42,42},
        {0,0,0},{0,42,42},{42,0,0},{42,42,42},
        {0,0,0},{0,63,63},{63,0,0},{63,63,63},
        {0,0,0},{0,63,63},{63,0,0},{63,63,63},
};

void loadfont(char *s, int format)
{
        FILE *f=romfopen(s,"rb");
        int c,d;
        if (!f)
           return;

        if (!format)
        {
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdatm[c][d]=getc(f);
                        }
                }
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdatm[c][d+8]=getc(f);
                        }
                }
                fseek(f,4096+2048,SEEK_SET);
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdat[c][d]=getc(f);
                        }
                }
        }
        else if (format == 1)
        {
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdatm[c][d]=getc(f);
                        }
                }
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdatm[c][d+8]=getc(f);
                        }
                }
                fseek(f, 4096, SEEK_SET);
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdat[c][d]=getc(f);
                        }
                        for (d=0;d<8;d++) getc(f);                
                }
        }
        else
        {
                for (c=0;c<256;c++)
                {
                        for (d=0;d<8;d++)
                        {
                                fontdat[c][d]=getc(f);
                        }
                }
        }
        fclose(f);
}


void drawscreen()
{
//        printf("Drawscreen %i %i %i %i\n",gfxcard,MDA,EGA,TANDY);
//        if (EGA) drawscreenega(buffer,vbuf);
/*        else if (MDA) {}//drawscreenmda();
        else if (TANDY)
        {
                if ((cgamode&3)==3 || array[3]&0x10) drawscreentandy(tandyvram);
                else                drawscreencga(tandyvram);
        }*/
//        else            drawscreencga(&vram[0x8000]);
}

PALETTE comppal=
{
        {0,0,0},{0,21,0},{0,0,42},{0,42,42},
        {42,0,21},{21,10,21},{42,0,42},{42,0,63},
        {21,21,21},{21,63,21},{42,21,42},{21,63,31},
        {63,0,0},{42,42,0},{63,21,42},{41,41,41}
};

void initvideo()
{
        int c,d;
//        set_color_depth(desktop_color_depth());
//        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED,2048,2048,0,0))
//           set_gfx_mode(GFX_AUTODETECT_WINDOWED,1024,768,0,0);
//        vbuf=create_video_bitmap(1280+32,1024+32);
//        set_color_depth(32);
        buffer32=create_bitmap(2048,2048);
//        set_color_depth(8);
        buffer=create_bitmap(2048,2048);
//        set_color_depth(32);
        for (c=0;c<64;c++)
        {
                cgapal[c+64].r=(((c&4)?2:0)|((c&0x10)?1:0))*21;
                cgapal[c+64].g=(((c&2)?2:0)|((c&0x10)?1:0))*21;
                cgapal[c+64].b=(((c&1)?2:0)|((c&0x10)?1:0))*21;
                if ((c&0x17)==6) cgapal[c+64].g>>=1;
        }
        for (c=0;c<64;c++)
        {
                cgapal[c+128].r=(((c&4)?2:0)|((c&0x20)?1:0))*21;
                cgapal[c+128].g=(((c&2)?2:0)|((c&0x10)?1:0))*21;
                cgapal[c+128].b=(((c&1)?2:0)|((c&0x08)?1:0))*21;
        }
        for (c=0;c<256;c++) cga32[c]=makecol(cgapal[c].r<<2,cgapal[c].g<<2,cgapal[c].b<<2);
//        for (c=0;c<16;c++) cgapal[c+192]=comppal[c];
//        set_palette(cgapal);
        if (MDA && !TANDY && !AMSTRAD && romset!=ROM_PC200) updatewindowsize(720,350);
        else                                                updatewindowsize(656,416);
        for (c=17;c<64;c++) crtcmask[c]=0xFF;
}

void resetvideo()
{
        int c;
        if (MDA && !TANDY && !AMSTRAD && romset!=ROM_PC200) updatewindowsize(720,350);
        else                                                updatewindowsize(656,416);
        cgastat=0;
        set_palette(cgapal);

        for (c=18;c<64;c++) crtcmask[c]=0xFF;
        crtc[0]=0x38;
        crtc[1]=0x28;
//        crtc[3]=0xFA;
        crtc[4]=0x7F;
        crtc[5]=0x06;
        crtc[6]=0x64;
        crtc[7]=0x70;
        crtc[8]=0x02;
        crtc[9]=1;

        cgacol=7;
        for (c=0;c<256;c++)
        {
                mdacols[c][0][0]=mdacols[c][1][0]=mdacols[c][1][1]=16;
                if (c&8) mdacols[c][0][1]=15+16;
                else     mdacols[c][0][1]=7+16;
        }
        mdacols[0x70][0][1]=16;
        mdacols[0x70][0][0]=mdacols[0x70][1][0]=mdacols[0x70][1][1]=16+15;
        mdacols[0xF0][0][1]=16;
        mdacols[0xF0][0][0]=mdacols[0xF0][1][0]=mdacols[0xF0][1][1]=16+15;
        mdacols[0x78][0][1]=16+7;
        mdacols[0x78][0][0]=mdacols[0x78][1][0]=mdacols[0x78][1][1]=16+15;
        mdacols[0xF8][0][1]=16+7;
        mdacols[0xF8][0][0]=mdacols[0xF8][1][0]=mdacols[0xF8][1][1]=16+15;
        mdacols[0x00][0][1] = mdacols[0x00][1][1] = 16;
        mdacols[0x08][0][1] = mdacols[0x08][1][1] = 16;
        mdacols[0x80][0][1] = mdacols[0x80][1][1] = 16;
        mdacols[0x88][0][1] = mdacols[0x88][1][1] = 16;
/*        switch (gfxcard)
        {
                case GFX_CGA: pollvideo=pollcga; break;
                case GFX_MDA:
                case GFX_HERCULES: pollvideo=pollmda; break;
        }
        if (TANDY) pollvideo=polltandy;
        if (EGA) pollvideo=pollega;
        if (romset==ROM_PC1512 || romset==ROM_PC200) pollvideo=pollcga;*/
//        tandyvram=&ram[0x9C000];
//        printf("Tandy VRAM %08X\n",tandyvram);
//        tandyb8000=&ram[0x9C000];
}

void closevideo()
{
        destroy_bitmap(buffer);
        destroy_bitmap(vbuf);
}
