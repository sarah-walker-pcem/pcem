#include <stdio.h>
#include <math.h>
#include "ibm.h"
#include "device.h"
#include "video.h"
#include "vid_svga.h"
#include "io.h"
#include "cpu.h"
#include "timer.h"

#include "vid_ati18800.h"
#include "vid_ati28800.h"
#include "vid_ati_mach64.h"
#include "vid_cga.h"
#include "vid_cl5429.h"
#include "vid_ega.h"
#include "vid_et4000.h"
#include "vid_et4000w32.h"
#include "vid_hercules.h"
#include "vid_mda.h"
#include "vid_olivetti_m24.h"
#include "vid_oti067.h"
#include "vid_paradise.h"
#include "vid_pc1512.h"
#include "vid_pc1640.h"
#include "vid_pc200.h"
#include "vid_s3.h"
#include "vid_s3_virge.h"
#include "vid_tandy.h"
#include "vid_tvga.h"
#include "vid_vga.h"

int egareads=0,egawrites=0;
int changeframecount=2;

uint8_t rotatevga[8][256];

int frames = 0;

int fullchange;

uint8_t edatlookup[4][4];

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

void video_init()
{
        pclog("Video_init %i %i\n",romset,gfxcard);

        switch (romset)
        {
                case ROM_TANDY:
                device_add(&tandy_device);
                return;

                case ROM_PC1512:
                device_add(&pc1512_device);
                return;
                
                case ROM_PC1640:
                device_add(&pc1640_device);
                return;
                
                case ROM_PC200:
                device_add(&pc200_device);
                return;
                
                case ROM_OLIM24:
                device_add(&m24_device);
                return;

                case ROM_PC2086:
                case ROM_PC3086:
                device_add(&paradise_pvga1a_device);
                return;
                case ROM_MEGAPC:
                device_add(&paradise_wd90c11_device);
                return;
                        
                case ROM_ACER386:
                device_add(&oti067_device);
                return;
        }
        switch (gfxcard)
        {
                case GFX_MDA:
                device_add(&mda_device);
                break;

                case GFX_HERCULES:
                device_add(&hercules_device);
                break;
                
                case GFX_CGA:
                device_add(&cga_device);
                break;
                
                case GFX_EGA:
                device_add(&ega_device);
                break;
                
                case GFX_TVGA:
                device_add(&tvga8900d_device);
                break;
                
                case GFX_ET4000:
                device_add(&et4000_device);
                break;
                
                case GFX_ET4000W32:
                device_add(&et4000w32p_device);
                break;

                case GFX_BAHAMAS64:
                device_add(&s3_bahamas64_device);
                break;

                case GFX_N9_9FX:
                device_add(&s3_9fx_device);
                break;
                
                case GFX_STEALTH64:
                break;
                
                case GFX_VIRGE:
                device_add(&s3_virge_device);
                break;
                
                case GFX_TGUI9440:
                device_add(&tgui9440_device);
                break;
                
                case GFX_VGA:
                device_add(&vga_device);
                break;

                case GFX_VGAEDGE16:
                device_add(&ati18800_device);
                break;

                case GFX_VGACHARGER:
                device_add(&ati18800_device);
                break;
                                
                case GFX_OTI067:
                device_add(&oti067_device);
                return;

                case GFX_MACH64GX:
                device_add(&mach64gx_device);
                return;

                case GFX_CL_GD5429:
                device_add(&gd5429_device);
                return;

                default:
                fatal("Bad gfx card %i\n",gfxcard);
        }
}


BITMAP *buffer, *buffer32;

uint8_t fontdat[256][8];
uint8_t fontdatm[256][16];

int xsize=1,ysize=1;


PALETTE cgapal;/* =
{
        { 0,  0,  0},{0,42,0},{42,0,0},{42,21,0},
        { 0,  0,  0},{0,42,42},{42,0,42},{42,42,42},
        { 0,  0,  0},{21,63,21},{63,21,21},{63,63,21},
        { 0,  0,  0},{21,63,63},{63,21,63},{63,63,63},

        {0,   0,  0}, { 0,  0, 42}, { 0, 42,  0}, { 0, 42, 42},
        {42,  0,  0}, {42,  0, 42}, {42, 21, 00}, {42, 42, 42},
        {21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
        {63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63},

        {0,0,0},{0,21,0},{0,0,42},{0,42,42},
        {42,0,21},{21,10,21},{42,0,42},{42,0,63},
        {21,21,21},{21,63,21},{42,21,42},{21,63,63},
        {63,0,0},{42,42,0},{63,21,42},{41,41,41},
        
        {0,0,0},{0,42,42},{42,0,0},{42,42,42},
        {0,0,0},{0,42,42},{42,0,0},{42,42,42},
        {0,0,0},{0,63,63},{63,0,0},{63,63,63},
        {0,0,0},{0,63,63},{63,0,0},{63,63,63},
};*/

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


void initvideo()
{
        int c, d, e;

        buffer32 = create_bitmap(2048, 2048);

        buffer = create_bitmap(2048, 2048);

        for (c = 0; c < 64; c++)
        {
                cgapal[c + 64].r = (((c & 4) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
                cgapal[c + 64].g = (((c & 2) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
                cgapal[c + 64].b = (((c & 1) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
                if ((c & 0x17) == 6) 
                        cgapal[c + 64].g >>= 1;
        }
        for (c = 0; c < 64; c++)
        {
                cgapal[c + 128].r = (((c & 4) ? 2 : 0) | ((c & 0x20) ? 1 : 0)) * 21;
                cgapal[c + 128].g = (((c & 2) ? 2 : 0) | ((c & 0x10) ? 1 : 0)) * 21;
                cgapal[c + 128].b = (((c & 1) ? 2 : 0) | ((c & 0x08) ? 1 : 0)) * 21;
        }

        for (c = 0; c < 256; c++)
        {
                e = c;
                for (d = 0; d < 8; d++)
                {
                        rotatevga[d][c] = e;
                        e = (e >> 1) | ((e & 1) ? 0x80 : 0);
                }
        }
        for (c = 0; c < 4; c++)
        {
                for (d = 0; d < 4; d++)
                {
                        edatlookup[c][d] = 0;
                        if (c & 1) edatlookup[c][d] |= 1;
                        if (d & 1) edatlookup[c][d] |= 2;
                        if (c & 2) edatlookup[c][d] |= 0x10;
                        if (d & 2) edatlookup[c][d] |= 0x20;
//                        printf("Edat %i,%i now %02X\n",c,d,edatlookup[c][d]);
                }
        }
}

void closevideo()
{
        destroy_bitmap(buffer);
}
