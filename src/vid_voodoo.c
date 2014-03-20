#include <stdlib.h>
#include "ibm.h"
#include "mem.h"
#include "pci.h"
#include "vid_voodoo.h"

static int tris = 0;

static struct voodoo
{
        mem_mapping_t mmio_mapping;
        mem_mapping_t   fb_mapping;
        mem_mapping_t  tex_mapping;
        
        int pci_enable;

        uint32_t color0, color1;
        
        uint8_t dac_data[8];
        int dac_reg;
        uint8_t dac_readdata;
        
        uint32_t dRdX, dGdX, dBdX, dZdX, dAdX, dSdX, dTdX, dWdX;
        
        uint32_t dRdY, dGdY, dBdY, dZdY, dAdY, dSdY, dTdY, dWdY;
        
        uint32_t fbiInit0, fbiInit1, fbiInit2, fbiInit3, fbiInit4;
        
        uint32_t fbzMode;
        
        uint8_t initEnable;
        
        uint32_t lfbMode;
        
        uint32_t memBaseAddr;
        
        uint32_t startR, startG, startB, startZ, startA, startS, startT, startW;
        
        uint32_t vertexAx, vertexAy, vertexBx, vertexBy, vertexCx, vertexCy;
        
        uint32_t front_offset, back_offset, aux_offset;
        
        uint32_t fb_read_offset, fb_write_offset;
        
        uint32_t draw_offset;
        
        int row_width;
        
        uint8_t *fb_mem, *tex_mem;
} voodoo;

enum
{
        SST_status = 0x000,
        
        SST_vertexAx = 0x008,
        SST_vertexAy = 0x00c,
        SST_vertexBx = 0x010,
        SST_vertexBy = 0x014,
        SST_vertexCx = 0x018,
        SST_vertexCy = 0x01c,
        
        SST_startR   = 0x0020,
        SST_startG   = 0x0024,
        SST_startB   = 0x0028,
        SST_startZ   = 0x002c,
        SST_startA   = 0x0030,
        SST_startS   = 0x0034,
        SST_startT   = 0x0038,
        SST_startW   = 0x003c,

        SST_dRdX     = 0x0040,
        SST_dGdX     = 0x0044,
        SST_dBdX     = 0x0048,
        SST_dZdX     = 0x004c,
        SST_dAdX     = 0x0050,
        SST_dSdX     = 0x0054,
        SST_dTdX     = 0x0058,
        SST_dWdX     = 0x005c,
        
        SST_dRdY     = 0x0060,
        SST_dGdY     = 0x0064,
        SST_dBdY     = 0x0068,
        SST_dZdY     = 0x006c,
        SST_dAdY     = 0x0070,
        SST_dSdY     = 0x0074,
        SST_dTdY     = 0x0078,
        SST_dWdY     = 0x007c,
        
        SST_triangleCMD = 0x0080,
        
        SST_fbzMode = 0x110,
        SST_lfbMode = 0x114,
        SST_swapbufferCMD = 0x128,
        
        SST_color0 = 0x144,
        SST_color1 = 0x148,
        
        SST_fbiInit4 = 0x200,
        SST_fbiInit0 = 0x210,
        SST_fbiInit1 = 0x214,
        SST_fbiInit2 = 0x218,
        SST_fbiInit3 = 0x21c,
        SST_dacData = 0x22c
};

enum
{
        LFB_WRITE_FRONT = 0x0000,
        LFB_WRITE_BACK  = 0x0010,
        LFB_WRITE_MASK  = 0x0030
};

enum
{
        LFB_READ_FRONT = 0x0000,
        LFB_READ_BACK  = 0x0040,
        LFB_READ_AUX   = 0x0080,
        LFB_READ_MASK  = 0x00c0
};

enum
{
        LFB_FORMAT_RGB565 = 0,
        LFB_FORMAT_DEPTH = 15,
        LFB_FORMAT_MASK = 15
};

enum
{
        LFB_WRITE_COLOUR = 1,
        LFB_WRITE_DEPTH = 2
};

enum
{
        FBZ_DITHER  = 0x100,
        FBZ_DRAWRGB = 0x200,
        
        FBZ_DRAW_FRONT = 0x0000,
        FBZ_DRAW_BACK  = 0x4000,
        FBZ_DRAW_MASK  = 0xc000
};

static int voodoo_reads = 0;

static void voodoo_recalc()
{
        uint32_t buffer_offset = ((voodoo.fbiInit2 >> 11) & 511) * 4096;
        
        voodoo.front_offset = 0;
        voodoo.back_offset = buffer_offset;
        voodoo.aux_offset = buffer_offset * 2;
        
        switch (voodoo.lfbMode & LFB_WRITE_MASK)
        {
                case LFB_WRITE_FRONT:
                voodoo.fb_write_offset = voodoo.front_offset;
                break;
                case LFB_WRITE_BACK:
                voodoo.fb_write_offset = voodoo.back_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown lfb destination\n");
        }

        switch (voodoo.lfbMode & LFB_READ_MASK)
        {
                case LFB_READ_FRONT:
                voodoo.fb_read_offset = voodoo.front_offset;
                break;
                case LFB_READ_BACK:
                voodoo.fb_read_offset = voodoo.back_offset;
                break;
                case LFB_READ_AUX:
                voodoo.fb_read_offset = voodoo.aux_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown lfb source\n");
        }
        
        switch (voodoo.fbzMode & FBZ_DRAW_MASK)
        {
                case FBZ_DRAW_FRONT:
                voodoo.draw_offset = voodoo.front_offset;
                break;
                case FBZ_DRAW_BACK:
                voodoo.draw_offset = voodoo.back_offset;
                break;

                default:
                fatal("voodoo_recalc : unknown draw buffer\n");
        }
                
        voodoo.row_width = ((voodoo.fbiInit1 >> 4) & 15) * 64 * 2;
        pclog("voodoo_recalc : front_offset %08X  back_offset %08X  aux_offset %08X\n", voodoo.front_offset, voodoo.back_offset, voodoo.aux_offset);
        pclog("                fb_read_offset %08X  fb_write_offset %08X  row_width %i\n", voodoo.fb_read_offset, voodoo.fb_write_offset, voodoo.row_width);
}

static int dither_matrix_4x4[16] =
{
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
};

static int dither_rb[256][4][4];
static int dither_g[256][4][4];

static void voodoo_make_dither()
{
        int c, x, y;
        int seen_rb[1024];
        int seen_g[1024];
        
        for (c = 0; c < 256; c++)
        {
                int rb = (int)(((double)c *  496.0) / 256.0);
                int  g = (int)(((double)c * 1008.0) / 256.0);
                pclog("rb %i %i\n", rb, c);
                for (y = 0; y < 4; y++)
                {
                        for (x = 0; x < 4; x++)
                        {
                                int val;
                                
                                val = rb + dither_matrix_4x4[x + (y << 2)];
                                dither_rb[c][y][x] = val >> 4;
                                pclog("RB %i %i, %i  %i %i %i\n", c, x, y, val, val >> 4, dither_rb[c][y][x]);
                                
                                if (dither_rb[c][y][x] > 31)
                                        fatal("RB overflow %i %i %i  %i\n", c, x, y, dither_rb[c][y][x]);

                                val = g + dither_matrix_4x4[x + (y << 2)];
                                dither_g[c][y][x] = val >> 4;                                

                                if (dither_g[c][y][x] > 63)
                                        fatal("G overflow %i %i %i  %i\n", c, x, y, dither_g[c][y][x]);
                        }
                }
        }
        
        memset(seen_rb, 0, sizeof(seen_rb));
        memset(seen_g,  0, sizeof(seen_g));
        
        for (c = 0; c < 256; c++)
        {
                int total_rb = 0;
                int total_g = 0;
                for (y = 0; y < 4; y++)
                {
                        for (x = 0; x < 4; x++)
                        {
                                total_rb += dither_rb[c][y][x];
                                pclog("total_rb %i %i, %i  %i %i\n", c, x, y, total_rb, dither_rb[c][y][x]);
                                total_g += dither_g[c][y][x];
                        }
                }

                if (seen_rb[total_rb])
                        fatal("Duplicate rb %i %i\n", c, total_rb);
                seen_rb[total_rb] = 1;

                if (seen_g[total_g])
                        fatal("Duplicate g %i %i\n", c, total_g);
                seen_g[total_g] = 1;
        }
}

static void voodoo_triangle(struct voodoo *voodoo)
{
        int y, yend, ydir;
        int xstart, xend, xdir;
        int dx1, dx2;
        
        pclog("voodoo_triangle %i : vA %f, %f  vB %f, %f  vC %f, %f\n", tris, (float)voodoo->vertexAx / 16.0, (float)voodoo->vertexAy / 16.0, 
                                                                     (float)voodoo->vertexBx / 16.0, (float)voodoo->vertexBy / 16.0, 
                                                                     (float)voodoo->vertexCx / 16.0, (float)voodoo->vertexCy / 16.0);
//        output = 3;
        tris++;
//        if (tris == 3)
//                fatal("tris 2\n");

        y = voodoo->vertexAy >> 4;
        ydir = (voodoo->vertexAy >= voodoo->vertexCy) ? -1 : 1;

        if (voodoo->vertexAy == voodoo->vertexBy)
        {
                xstart = voodoo->vertexAx << 8;
                xend = voodoo->vertexBx << 8;
        }
        else
                xstart = xend = voodoo->vertexAx << 8;
        xdir = (xstart >= xend) ? -1 : 1;
        
        if (voodoo->vertexAy != voodoo->vertexBy)
        {
                /*Draw top half*/
                dx1 = (int)(((voodoo->vertexBx << 8) - xstart) << 4) / (int)((voodoo->vertexBy - voodoo->vertexAy) * ydir);
                dx2 = (int)(((voodoo->vertexCx << 8) -   xend) << 4) / (int)((voodoo->vertexCy - voodoo->vertexAy) * ydir);
                yend = voodoo->vertexBy >> 4;
                
                pclog("voodoo_triangle : top-half %X %X %X %X %i  %i %i %i\n", xstart, xend, dx1, dx2, xdir,  y, yend, ydir);
                
                for (; y != yend; y += ydir)
                {
                        pclog("    %i : %i - %i\n", xstart >> 12, xend >> 12);
                        xstart += dx1;
                        xend += dx2;
                }
        }
        
        
        if (voodoo->vertexBy != voodoo->vertexCy)
        {        
                /*Draw bottom half*/
                dx1 = (int)(((voodoo->vertexCx << 8) - xstart) << 4) / (int)((voodoo->vertexCy - voodoo->vertexAy) * ydir);
                dx2 = (int)(((voodoo->vertexCx << 8) -   xend) << 4) / (int)((voodoo->vertexCy - voodoo->vertexAy) * ydir);
                yend = voodoo->vertexCy >> 4;
                                
                pclog("voodoo_triangle : bottom-half %X %X %X %X %X %i  %i %i %i\n", xstart, xend, dx1, dx2, dx2 * 36, xdir,  y, yend, ydir);

                for (; y != yend; y += ydir)
                {
                        int x = xstart >> 12, x2 = xend >> 12;
                        pclog("    %i : %i - %i\n", y, x, x2);
                        
                        for (; x != x2; x += xdir)
                        {
                                if (voodoo->fbzMode & FBZ_DRAWRGB)
                                {
                                        int r = (voodoo->color1 >> 16) & 0xff;
                                        int g = (voodoo->color1 >> 8)  & 0xff;
                                        int b =  voodoo->color1        & 0xff;
                                        if (voodoo->fbzMode & FBZ_DITHER)
                                        {
                                                r = dither_rb[r][y & 3][x & 3];
                                                g =  dither_g[g][y & 3][x & 3];
                                                b = dither_rb[b][y & 3][x & 3];
/*                                                r = ((r << 1) + dither_matrix_4x4[(x & 3) + ((y & 3) << 2)]) >> 4;
                                                g = ((g << 2) + dither_matrix_4x4[(x & 3) + ((y & 3) << 2)]) >> 4;
                                                b = ((b << 1) + dither_matrix_4x4[(x & 3) + ((y & 3) << 2)]) >> 4;
                                                if (r > 31) r = 31;
                                                if (g > 63) g = 63;
                                                if (b > 31) b = 31;*/
/*                                                r = (((r << 1) - (r >> 4) + (r >> 7) + dither_matrix_4x4[(x & 3) + ((y & 3) << 2)]) >> 1) >> 3;
                                                g = (((g << 2) - (g >> 4) + (g >> 6) + dither_matrix_4x4[(x & 3) + ((y & 3) << 2)]) >> 2) >> 2;
                                                b = (((b << 1) - (b >> 4) + (b >> 7) + dither_matrix_4x4[(x & 3) + ((y & 3) << 2)]) >> 1) >> 3;*/
                                        }
                                        else
                                        {
                                                r >>= 3;
                                                g >>= 2;
                                                b >>= 3;
                                        }
                                        *(uint16_t *)(&voodoo->fb_mem[voodoo->draw_offset + (y * voodoo->row_width) + (x << 1)]) = b | (g << 5) | (r << 11);
                                        pclog("       %i = %i %i %i %04X  %08X   %08X\n", x, r, g, b, b | (g << 5) | (r << 11), voodoo->draw_offset + (y * voodoo->row_width) + (x << 1), *(uint32_t *)(&voodoo->fb_mem[4]));
                                }
                        }
                        xstart += dx1;
                        xend += dx2;
                }
        }
                
/*        for (y = ystart, y != yend; y += ydir)
        {
                if (voodoo.fbzMode & FBZ_DITHER
        }*/
}

static uint32_t voodoo_readl(uint32_t addr, void *priv)
{
        uint32_t temp;
        switch (addr & 0x3fc)
        {
                case SST_status:
                temp = 0x0ffff03f; /*FIFOs empty*/
                break;
                
                case SST_lfbMode:
                return voodoo.lfbMode;
                
                case SST_fbiInit4:
                temp = voodoo.fbiInit4;
                break;
                case SST_fbiInit0:
                temp = voodoo.fbiInit0;
                break;
                case SST_fbiInit1:
                temp = voodoo.fbiInit1 & ~5; /*Pass-thru board with one SST-1*/
                break;              
                case SST_fbiInit2:
                if (voodoo.initEnable & 0x04)
                        temp = voodoo.dac_readdata;
                else
                        temp = voodoo.fbiInit2;
                break;
                case SST_fbiInit3:
                temp = voodoo.fbiInit3;
                break;
                
                default:
                fatal("voodoo_readl  : bad addr %08X\n", addr);
                temp = 0xffffffff;
        }
        pclog("voodoo_readl  : addr %08X val %08X  %04X(%08X):%08X  %i\n", addr, temp, CS, cs, pc, voodoo_reads++);
//        if (voodoo_reads == 200494)
//          output = 3;
        return temp;
}

static void voodoo_writel(uint32_t addr, uint32_t val, void *priv)
{
        pclog("voodoo_writel : addr %08X val %08X  %04X(%08X):%08X\n", addr, val, CS, cs, pc);
        switch (addr & 0x3fc)
        {
                case SST_swapbufferCMD:
                pclog("  start swap buffer command\n");
//                output = 3;
                break;
                        
                case SST_vertexAx:
                voodoo.vertexAx = val & 0xffff;
                break;
                case SST_vertexAy:
                voodoo.vertexAy = val & 0xffff;
                break;
                case SST_vertexBx:
                voodoo.vertexBx = val & 0xffff;
                break;
                case SST_vertexBy:
                voodoo.vertexBy = val & 0xffff;
                break;
                case SST_vertexCx:
                voodoo.vertexCx = val & 0xffff;
                break;
                case SST_vertexCy:
                voodoo.vertexCy = val & 0xffff;
                break;
                
                case SST_startR:
                voodoo.startR = val & 0xffffff;
                break;
                case SST_startG:
                voodoo.startG = val & 0xffffff;
                break;
                case SST_startB:
                voodoo.startB = val & 0xffffff;
                break;
                case SST_startZ:
                voodoo.startZ = val;
                break;
                case SST_startA:
                voodoo.startA = val & 0xffffff;
                break;
                case SST_startS:
                voodoo.startS = val;
                break;
                case SST_startT:
                voodoo.startT = val;
                break;
                case SST_startW:
                voodoo.startW = val;
                break;

                case SST_dRdX:
                voodoo.dRdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dGdX:
                voodoo.dGdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dBdX:
                voodoo.dBdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dZdX:
                voodoo.dZdX = val;
                break;
                case SST_dAdX:
                voodoo.dAdX = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dSdX:
                voodoo.dSdX = val;
                break;
                case SST_dTdX:
                voodoo.dTdX = val;
                break;
                case SST_dWdX:
                voodoo.dWdX = val;
                break;

                case SST_dRdY:
                voodoo.dRdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dGdY:
                voodoo.dGdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dBdY:
                voodoo.dBdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dZdY:
                voodoo.dZdY = val;
                break;
                case SST_dAdY:
                voodoo.dAdY = (val & 0xffffff) | ((val & 0x800000) ? 0xff000000 : 0);
                break;
                case SST_dSdY:
                voodoo.dSdY = val;
                break;
                case SST_dTdY:
                voodoo.dTdY = val;
                break;
                case SST_dWdY:
                voodoo.dWdY = val;
                break;

                case SST_triangleCMD:
                voodoo_triangle(&voodoo);
                break;
                
                        
                case SST_fbzMode:
                voodoo.fbzMode = val;
                break;
                case SST_lfbMode:
                voodoo.lfbMode = val;
                voodoo_recalc();
                break;

                case SST_color0:
                voodoo.color0 = val;
                break;
                case SST_color1:
                voodoo.color1 = val;
                break;

                case SST_fbiInit4:
                if (voodoo.initEnable & 0x01)
                        voodoo.fbiInit4 = val;
                break;
                case SST_fbiInit0:
                if (voodoo.initEnable & 0x01)
                        voodoo.fbiInit0 = val;
                break;
                case SST_fbiInit1:
                if (voodoo.initEnable & 0x01)
                        voodoo.fbiInit1 = val;
                break;
                case SST_fbiInit2:
                if (voodoo.initEnable & 0x01)
                {
                        voodoo.fbiInit2 = val;
                        voodoo_recalc();
                }
                break;
                case SST_fbiInit3:
                if (voodoo.initEnable & 0x01)
                        voodoo.fbiInit3 = val;
                break;
                
                case SST_dacData:
                voodoo.dac_reg = (val >> 8) & 7;
                voodoo.dac_readdata = 0xff;
                if (val & 0x800)
                {
                        pclog("  dacData read %i %02X\n", voodoo.dac_reg, voodoo.dac_data[7]);
                        if (voodoo.dac_reg == 5)
                        {
                                switch (voodoo.dac_data[7])
                                {
        				case 0x01: voodoo.dac_readdata = 0x55; break;
        				case 0x07: voodoo.dac_readdata = 0x71; break;
        				case 0x0b: voodoo.dac_readdata = 0x79; break;
                                }
                        }
                        else
                                voodoo.dac_readdata = voodoo.dac_data[voodoo.dac_readdata];
                }
                else
                        voodoo.dac_data[voodoo.dac_reg] = val & 0xff;
                break;
        }
}

static uint16_t voodoo_fb_readw(uint32_t addr, void *priv)
{
        pclog("voodoo_fb_readw : %08X %04X\n", addr, *(uint16_t *)(&voodoo.fb_mem[addr & 0x1fffff]));
        return *(uint16_t *)(&voodoo.fb_mem[addr & 0x1fffff]);
}
static uint32_t voodoo_fb_readl(uint32_t addr, void *priv)
{
        int x, y;
        uint32_t read_addr;
        uint32_t temp;
        
        x = (addr >> 1) & 0x3ff;
        y = (addr >> 11) & 0x3ff;
        read_addr = voodoo.fb_read_offset + (x << 1) + (y * voodoo.row_width);
        
        temp = *(uint32_t *)(&voodoo.fb_mem[read_addr & 0x1fffff]);

        pclog("voodoo_fb_readl : %08X %08X  %i %i  %08X %08X\n", addr, temp, x, y, read_addr, *(uint32_t *)(&voodoo.fb_mem[4]));
        return temp;
}
static void voodoo_fb_writew(uint32_t addr, uint16_t val, void *priv)
{
        pclog("voodoo_fb_writew : %08X %04X\n", addr, val);
        *(uint16_t *)(&voodoo.fb_mem[addr & 0x1fffff]) = val;
}
static void voodoo_fb_writel(uint32_t addr, uint32_t val, void *priv)
{
        int x, y;
        uint32_t write_addr, write_addr_aux;
        uint32_t colour_data, depth_data;
        int write_mask, count = 1;
        
        pclog("voodoo_fb_writel : %08X %08X\n", addr, val);
        
        if (voodoo.lfbMode & 0x100)
                fatal("voodoo_fb_writel : using pixel processing\n");
                
        switch (voodoo.lfbMode & LFB_FORMAT_MASK)
        {
                case LFB_FORMAT_RGB565:
                colour_data = val;
                write_mask = LFB_WRITE_COLOUR;
                break;
                
                case LFB_FORMAT_DEPTH:
                depth_data = val;
                write_mask = LFB_WRITE_DEPTH;
                addr <<= 1;
                count = 2;
                break;
                
                default:
                fatal("voodoo_fb_writel : bad LFB format %08X\n", voodoo.lfbMode);
        }

        x = addr & 0x3fe;
        y = (addr >> 10) & 0x3ff;

        write_addr = voodoo.fb_read_offset + x + (y * voodoo.row_width);      
        write_addr_aux = voodoo.aux_offset + x + (y * voodoo.row_width);
        
        while (count--)
        {               
                if (write_mask & LFB_WRITE_COLOUR)
                        *(uint16_t *)(&voodoo.fb_mem[write_addr & 0x1ffffe]) = colour_data;
                if (write_mask & LFB_WRITE_DEPTH)
                        *(uint16_t *)(&voodoo.fb_mem[write_addr_aux & 0x1ffffe]) = depth_data;
                        
                colour_data >>= 16;
                depth_data >>= 16;
                
                write_addr += 2;
                write_addr_aux += 2;
        }
}

static void voodoo_tex_writew(uint32_t addr, uint16_t val, void *priv)
{
        pclog("voodoo_tex_write : %08X %04X\n", addr, val);
        *(uint16_t *)(&voodoo.tex_mem[addr & 0x1fffff]) = val;
}
static void voodoo_tex_writel(uint32_t addr, uint32_t val, void *priv)
{
        pclog("voodoo_tex_write : %08X %08X\n", addr, val);
        *(uint32_t *)(&voodoo.tex_mem[addr & 0x1fffff]) = val;
}

static void voodoo_recalcmapping()
{
        if (voodoo.pci_enable)
        {
                pclog("voodoo_recalcmapping : memBaseAddr %08X\n", voodoo.memBaseAddr);
                mem_mapping_set_addr(&voodoo.mmio_mapping, voodoo.memBaseAddr,              0x00400000);
                mem_mapping_set_addr(&voodoo.fb_mapping,   voodoo.memBaseAddr + 0x00400000, 0x00400000);
                mem_mapping_set_addr(&voodoo.tex_mapping,  voodoo.memBaseAddr + 0x00800000, 0x00800000);
        }
        else
        {
                pclog("voodoo_recalcmapping : disabled\n");
                mem_mapping_disable(&voodoo.mmio_mapping);
                mem_mapping_disable(&voodoo.fb_mapping);
                mem_mapping_disable(&voodoo.tex_mapping);
        }
}

uint8_t voodoo_pci_read(int func, int addr, void *priv)
{
        pclog("Voodoo PCI read %08X\n", addr);
        switch (addr)
        {
                case 0x00: return 0x1a; /*3dfx*/
                case 0x01: return 0x12;
                
                case 0x02: return 0x01; /*SST-1 (Voodoo Graphics)*/
                case 0x03: return 0x00;
                
                case 0x04: return voodoo.pci_enable ? 0x02 : 0x00; /*Respond to memory accesses*/

                case 0x08: return 0; /*Revision ID*/
                case 0x09: return 0; /*Programming interface*/
                
                case 0x10: return 0x00; /*memBaseAddr*/
                case 0x11: return 0x00;
                case 0x12: return 0x00;
                case 0x13: return voodoo.memBaseAddr >> 24;

                case 0x40: return voodoo.initEnable;
        }
        return 0;
}

void voodoo_pci_write(int func, int addr, uint8_t val, void *priv)
{
        pclog("Voodoo PCI write %04X %02X\n", addr, val);
        switch (addr)
        {
                case 0x04: voodoo.pci_enable = val & 2; voodoo_recalcmapping(); break;
                
                case 0x13: voodoo.memBaseAddr = val << 24; voodoo_recalcmapping(); break;
                
                case 0x40: voodoo.initEnable = val; break;
        }
}

void voodoo_init()
{
        return;
        voodoo_make_dither();
        pci_add(voodoo_pci_read, voodoo_pci_write, NULL);

        mem_mapping_add(&voodoo.mmio_mapping, 0, 0, NULL, NULL,            voodoo_readl,    NULL,       NULL,              voodoo_writel,     NULL, 0, NULL);
        mem_mapping_add(&voodoo.fb_mapping,   0, 0, NULL, voodoo_fb_readw, voodoo_fb_readl, NULL,       voodoo_fb_writew,  voodoo_fb_writel,  NULL, 0, NULL);
        mem_mapping_add(&voodoo.tex_mapping,  0, 0, NULL, NULL,            NULL,            NULL,       voodoo_tex_writew, voodoo_tex_writel, NULL, 0, NULL);

        voodoo.fb_mem = malloc(2 * 1024 * 1024);
        voodoo.tex_mem = malloc(2 * 1024 * 1024);        
}
