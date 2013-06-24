#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"

void svga_render_blank(svga_t *svga)
{
        int x, xx;
        
        if (svga->firstline_draw == 2000) 
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;
        
        for (x = 0; x < svga->hdisp; x++)
        {
                switch (svga->seqregs[1] & 9)
                {
                        case 0:
                        for (xx = 0; xx < 9; xx++) ((uint32_t *)buffer32->line[svga->displine])[(x * 9) + xx + 32] = 0;
                        break;
                        case 1:
                        for (xx = 0; xx < 8; xx++) ((uint32_t *)buffer32->line[svga->displine])[(x * 8) + xx + 32] = 0;
                        break;
                        case 8:
                        for (xx = 0; xx < 18; xx++) ((uint32_t *)buffer32->line[svga->displine])[(x * 18) + xx + 32] = 0;
                        break;
                        case 9:
                        for (xx = 0; xx < 16; xx++) ((uint32_t *)buffer32->line[svga->displine])[(x * 16) + xx + 32] = 0;
                        break;
                }
        }
}

void svga_render_text_40(svga_t *svga)
{     
        if (svga->firstline_draw == 2000) 
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;
        
        if (fullchange)
        {
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (svga->seqregs[1] & 1) ? 16 : 18;
                
                for (x = 0; x < svga->hdisp; x += xinc)
                {
                        drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
                        chr  = svga->vram[(svga->ma << 1) & svga->vrammask];
                        attr = svga->vram[((svga->ma << 1) + 4) & svga->vrammask];
                        if (attr & 8) charaddr = svga->charsetb + (chr * 128);
                        else          charaddr = svga->charseta + (chr * 128);

                        if (drawcursor) 
                        { 
                                bg = svga->pallook[svga->egapal[attr & 15]]; 
                                fg = svga->pallook[svga->egapal[attr >> 4]]; 
                        }
                        else
                        {
                                fg = svga->pallook[svga->egapal[attr & 15]];
                                bg = svga->pallook[svga->egapal[attr >> 4]];
                                if (attr & 0x80 && svga->attrregs[0x10] & 8)
                                {
                                        bg = svga->pallook[svga->egapal[(attr >> 4) & 7]];
                                        if (svga->blink & 16) 
                                                fg = bg;
                                }
                        }

                        dat = svga->vram[charaddr + (svga->sc << 2)];
                        if (svga->seqregs[1] & 1) 
                        { 
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + (xx << 1)) & 2047] = ((uint32_t *)buffer32->line[svga->displine])[(x + 33 + (xx << 1)) & 2047] = (dat & (0x80 >> xx)) ? fg : bg;
                        }
                        else
                        {
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + (xx << 1)) & 2047] = ((uint32_t *)buffer32->line[svga->displine])[(x + 33 + (xx << 1)) & 2047] = (dat & (0x80 >> xx)) ? fg : bg;
                                if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4)) 
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + 16) & 2047] = ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + 17) & 2047] = bg;
                                else                  
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + 16) & 2047] = ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + 17) & 2047] = (dat & 1) ? fg : bg;
                        }
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                }
        }
}

void svga_render_text_80(svga_t *svga)
{
        if (svga->firstline_draw == 2000) 
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;
        
        if (fullchange)
        {
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (svga->seqregs[1] & 1) ? 8 : 9;

                for (x = 0; x < svga->hdisp; x += xinc)
                {
                        drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
                        chr  = svga->vram[(svga->ma << 1) & svga->vrammask];
                        attr = svga->vram[((svga->ma << 1) + 4) & svga->vrammask];
                        if (attr & 8) charaddr = svga->charsetb + (chr * 128);
                        else          charaddr = svga->charseta + (chr * 128);

                        if (drawcursor) 
                        { 
                                bg = svga->pallook[svga->egapal[attr & 15]]; 
                                fg = svga->pallook[svga->egapal[attr >> 4]]; 
                        }
                        else
                        {
                                fg = svga->pallook[svga->egapal[attr & 15]];
                                bg = svga->pallook[svga->egapal[attr >> 4]];
                                if (attr & 0x80 && svga->attrregs[0x10] & 8)
                                {
                                        bg = svga->pallook[svga->egapal[(attr >> 4) & 7]];
                                        if (svga->blink & 16) 
                                                fg = bg;
                                }
                        }

                        dat = svga->vram[charaddr + (svga->sc << 2)];
                        if (svga->seqregs[1] & 1) 
                        { 
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + xx) & 2047] = (dat & (0x80 >> xx)) ? fg : bg;
                        }
                        else
                        {
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + xx) & 2047] = (dat & (0x80 >> xx)) ? fg : bg;
                                if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4)) 
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + 8) & 2047] = bg;
                                else                  
                                        ((uint32_t *)buffer32->line[svga->displine])[(x + 32 + 8) & 2047] = (dat & 1) ? fg : bg;
                        }
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                }
        }
}

void svga_render_4bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint8_t edat[4], dat;
        
        if (svga->firstline_draw == 2000) 
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;

        offset = ((8 - svga->scrollcache) << 1) + 16;
        for (x = 0; x <= svga->hdisp; x += 16)
        {
                edat[0] = svga->vram[svga->ma];
                edat[1] = svga->vram[svga->ma | 0x1];
                edat[2] = svga->vram[svga->ma | 0x2];
                edat[3] = svga->vram[svga->ma | 0x3];
                svga->ma += 4; 
                svga->ma &= svga->vrammask;

                dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x + 14 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 15 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x + 12 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 13 + offset] = svga->pallook[svga->egapal[dat >> 4]];
                dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x + 10 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 11 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x +  8 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  9 + offset] = svga->pallook[svga->egapal[dat >> 4]];
                dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x +  6 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  7 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x +  4 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  5 + offset] = svga->pallook[svga->egapal[dat >> 4]];
                dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x +  2 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  3 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x      + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  1 + offset] = svga->pallook[svga->egapal[dat >> 4]];
        }
}

void svga_render_4bpp_highres(svga_t *svga)
{
        int x, offset;
        uint8_t edat[4], dat;
        
        if (svga->firstline_draw == 2000) 
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;

        offset = (8 - svga->scrollcache) + 24;
        for (x = 0; x <= svga->hdisp; x += 8)
        {
                edat[0] = svga->vram[svga->ma];
                edat[1] = svga->vram[svga->ma | 0x1];
                edat[2] = svga->vram[svga->ma | 0x2];
                edat[3] = svga->vram[svga->ma | 0x3];
                svga->ma += 4;
                svga->ma &= svga->vrammask;

                dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x + 7 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x + 6 + offset] = svga->pallook[svga->egapal[dat >> 4]];
                dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x + 5 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x + 4 + offset] = svga->pallook[svga->egapal[dat >> 4]];
                dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x + 3 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x + 2 + offset] = svga->pallook[svga->egapal[dat >> 4]];
                dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
                ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = svga->pallook[svga->egapal[dat & 0xf]];
                ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = svga->pallook[svga->egapal[dat >> 4]];
        }
}

void svga_render_2bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint8_t edat[2], dat;
                
        if (svga->firstline_draw == 2000) 
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;
        offset = ((8 - svga->scrollcache) << 1) + 16;
        
        /*Low res (320) only, though high res (640) should be possible*/
        for (x = 0; x <= svga->hdisp; x += 16)
        {
                if (svga->sc & 1 && !(svga->crtc[0x17] & 1))
                {
                        edat[0] = svga->vram[(svga->ma << 1) + 0x8000];
                        edat[1] = svga->vram[(svga->ma << 1) + 0x8004];
                }
                else
                {
                        edat[0] = svga->vram[(svga->ma << 1)];
                        edat[1] = svga->vram[(svga->ma << 1) + 4];
                }
                svga->ma += 4; svga->ma &= svga->vrammask;

                ((uint32_t *)buffer32->line[svga->displine])[x + 14 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 15 + offset] = svga->pallook[svga->egapal[edat[1] & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x + 12 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 13 + offset] = svga->pallook[svga->egapal[(edat[1] >> 2) & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x + 10 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 11 + offset] = svga->pallook[svga->egapal[(edat[1] >> 4) & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x +  8 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  9 + offset] = svga->pallook[svga->egapal[(edat[1] >> 6) & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x +  6 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  7 + offset] = svga->pallook[svga->egapal[edat[0] & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x +  4 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  5 + offset] = svga->pallook[svga->egapal[(edat[0] >> 2) & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x +  2 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  3 + offset] = svga->pallook[svga->egapal[(edat[0] >> 4) & 3]];
                ((uint32_t *)buffer32->line[svga->displine])[x +      offset] = ((uint32_t *)buffer32->line[svga->displine])[x +  1 + offset] = svga->pallook[svga->egapal[(edat[0] >> 6) & 3]];
        }
}

void svga_render_8bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint8_t edat[4];
                
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - (svga->scrollcache & 6)) + 24;
                                                                
                for (x = 0; x <= svga->hdisp; x += 8)
                {
                        edat[0] = svga->vram[svga->ma];
                        edat[1] = svga->vram[svga->ma | 0x1];
                        edat[2] = svga->vram[svga->ma | 0x2];
                        edat[3] = svga->vram[svga->ma | 0x3];
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;                       
                        ((uint32_t *)buffer32->line[svga->displine])[x + 6 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 7 + offset] = svga->pallook[edat[3]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 4 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 5 + offset] = svga->pallook[edat[2]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 2 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 3 + offset] = svga->pallook[edat[1]];
                        ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = svga->pallook[edat[0]];
                }
        }
}

void svga_render_8bpp_highres(svga_t *svga)
{
        int x, offset;
        uint8_t edat[4];
                
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                                                                
                for (x = 0; x <= svga->hdisp; x += 8)
                {
                        edat[0] = svga->vram[svga->ma];
                        edat[1] = svga->vram[svga->ma | 0x1];
                        edat[2] = svga->vram[svga->ma | 0x2];
                        edat[3] = svga->vram[svga->ma | 0x3];
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + 3 + offset] = svga->pallook[edat[3]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 2 + offset] = svga->pallook[edat[2]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = svga->pallook[edat[1]];
                        ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = svga->pallook[edat[0]];
                        
                        edat[0] = svga->vram[svga->ma];
                        edat[1] = svga->vram[svga->ma | 0x1];
                        edat[2] = svga->vram[svga->ma | 0x2];
                        edat[3] = svga->vram[svga->ma | 0x3];
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + 7 + offset] = svga->pallook[edat[3]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 6 + offset] = svga->pallook[edat[2]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 5 + offset] = svga->pallook[edat[1]];
                        ((uint32_t *)buffer32->line[svga->displine])[x + 4 + offset] = svga->pallook[edat[0]];
                }
        }
}

void svga_render_15bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint16_t fg, bg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - (svga->scrollcache & 6)) + 24;

                for (x = 0; x <= svga->hdisp; x += 4)
                {
                        fg = svga->vram[svga->ma]       | (svga->vram[svga->ma | 0x1] << 8);
                        bg = svga->vram[svga->ma | 0x2] | (svga->vram[svga->ma | 0x3] << 8);
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + 2 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 3 + offset] = ((bg & 31) << 3) | (((bg >> 5) & 31) << 11) | (((bg >> 10) & 31) << 19);
                        ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = ((fg & 31) << 3) | (((fg >> 5) & 31) << 11) | (((fg >> 10) & 31) << 19);
                }
        }
}

void svga_render_15bpp_highres(svga_t *svga)
{
        int x, offset;
        uint16_t fg, bg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;

                for (x = 0; x <= svga->hdisp; x += 2)
                {
                        fg = svga->vram[svga->ma]       | (svga->vram[svga->ma | 0x1] << 8);
                        bg = svga->vram[svga->ma | 0x2] | (svga->vram[svga->ma | 0x3] << 8);
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = ((bg & 31) << 3) | (((bg >> 5) & 31) << 11) | (((bg >> 10) & 31) << 19);
                        ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = ((fg & 31) << 3) | (((fg >> 5) & 31) << 11) | (((fg >> 10) & 31) << 19);
                }
        }
}

void svga_render_16bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint16_t fg, bg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - (svga->scrollcache & 6)) + 24;

                for (x = 0; x <= svga->hdisp; x += 4)
                {
                        fg = svga->vram[svga->ma]       | (svga->vram[svga->ma | 0x1] << 8);
                        bg = svga->vram[svga->ma | 0x2] | (svga->vram[svga->ma | 0x3] << 8);
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + 2 + offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 3 + offset] = ((bg & 31) << 3) | (((bg >> 5) & 63) << 10) | (((bg >> 11) & 31) << 19);
                        ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = ((fg & 31) << 3) | (((fg >> 5) & 63) << 10) | (((fg >> 11) & 31) << 19);
                }
        }
}

void svga_render_16bpp_highres(svga_t *svga)
{
        int x, offset;
        uint16_t fg, bg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                                                                
                for (x = 0; x <= svga->hdisp; x += 2)
                {
                        fg = svga->vram[svga->ma]       | (svga->vram[svga->ma | 0x1] << 8);
                        bg = svga->vram[svga->ma | 0x2] | (svga->vram[svga->ma | 0x3] << 8);
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + 1 + offset] = ((bg & 31) << 3) | (((bg >> 5) & 63) << 10) | (((bg >> 11) & 31) << 19);
                        ((uint32_t *)buffer32->line[svga->displine])[x +     offset] = ((fg & 31) << 3) | (((fg >> 5) & 63) << 10) | (((fg >> 11) & 31) << 19);
                }
        }
}

void svga_render_24bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint32_t fg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - (svga->scrollcache & 6)) + 24;

                for (x = 0; x <= svga->hdisp; x++)
                {
                        fg = svga->vram[svga->ma] | (svga->vram[svga->ma + 1] << 8) | (svga->vram[svga->ma + 2] << 16);
                        svga->ma += 3; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[(x << 1) + offset] = ((uint32_t *)buffer32->line[svga->displine])[(x << 1) + 1 + offset] = fg;
                }
        }
}

void svga_render_24bpp_highres(svga_t *svga)
{
        int x, offset;
        uint32_t fg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;

                for (x = 0; x <= svga->hdisp; x++)
                {
                        fg = svga->vram[svga->ma] | (svga->vram[svga->ma + 1] << 8) | (svga->vram[svga->ma + 2] << 16);
                        svga->ma += 3; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + offset] = fg;
                }
        }
}

void svga_render_32bpp_lowres(svga_t *svga)
{
        int x, offset;
        uint32_t fg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - (svga->scrollcache & 6)) + 24;

                for (x = 0; x <= svga->hdisp; x++)
                {
                        fg = svga->vram[svga->ma] | (svga->vram[svga->ma + 1] << 8) | (svga->vram[svga->ma + 2] << 16);
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[(x << 1) + offset] = ((uint32_t *)buffer32->line[svga->displine])[(x << 1) + 1 + offset] = fg;
                }
        }
}

void svga_render_32bpp_highres(svga_t *svga)
{
        int x, offset;
        uint32_t fg;
        
        if (svga->changedvram[svga->ma >> 10] || svga->changedvram[(svga->ma >> 10) + 1] || svga->changedvram[(svga->ma >> 10) + 2] || fullchange)
        {
                if (svga->firstline_draw == 2000) 
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;

                for (x = 0; x <= svga->hdisp; x++)
                {
                        fg = svga->vram[svga->ma] | (svga->vram[svga->ma + 1] << 8) | (svga->vram[svga->ma + 2] << 16);
                        svga->ma += 4; 
                        svga->ma &= svga->vrammask;
                        ((uint32_t *)buffer32->line[svga->displine])[x + offset] = fg;
                }
        }
}

