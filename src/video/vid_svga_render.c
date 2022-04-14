#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_svga_render_remap.h"

void svga_render_null(svga_t* svga)
{
        if (svga->firstline_draw == 2000)
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;
}

void svga_render_blank(svga_t* svga)
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
                        for (xx = 0; xx < 9; xx++) ((uint32_t*)buffer32->line[svga->displine])[(x * 9) + xx + 32] = 0;
                        break;
                case 1:
                        for (xx = 0; xx < 8; xx++) ((uint32_t*)buffer32->line[svga->displine])[(x * 8) + xx + 32] = 0;
                        break;
                case 8:
                        for (xx = 0; xx < 18; xx++) ((uint32_t*)buffer32->line[svga->displine])[(x * 18) + xx + 32] = 0;
                        break;
                case 9:
                        for (xx = 0; xx < 16; xx++) ((uint32_t*)buffer32->line[svga->displine])[(x * 16) + xx + 32] = 0;
                        break;
                }
        }
}

void svga_render_text_40(svga_t* svga)
{
        if (svga->firstline_draw == 2000)
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;

        if (svga->fullchange)
        {
                int offset = ((8 - svga->scrollcache) << 1) + 16;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (svga->seqregs[1] & 1) ? 16 : 18;

                for (x = 0; x < svga->hdisp; x += xinc)
                {
                        uint32_t addr = svga->remap_func(svga, svga->ma) & svga->vram_display_mask;

                        drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
                        chr = svga->vram[addr];
                        attr = svga->vram[addr + 1];

                        if (attr & 8) charaddr = svga->charsetb + (chr * 128);
                        else charaddr = svga->charseta + (chr * 128);

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
                                for (xx = 0; xx < 16; xx += 2)
                                        p[xx] = p[xx + 1] = (dat & (0x80 >> (xx >> 1))) ? fg : bg;
                        }
                        else
                        {
                                for (xx = 0; xx < 16; xx += 2)
                                        p[xx] = p[xx + 1] = (dat & (0x80 >> (xx >> 1))) ? fg : bg;
                                if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4))
                                        p[16] = p[17] = bg;
                                else
                                        p[16] = p[17] = (dat & 1) ? fg : bg;
                        }
                        svga->ma += 4;
                        p += xinc;
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_text_80(svga_t* svga)
{
        if (svga->firstline_draw == 2000)
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;

        if (svga->fullchange)
        {
                int offset = (8 - svga->scrollcache) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (svga->seqregs[1] & 1) ? 8 : 9;

                for (x = 0; x < svga->hdisp; x += xinc)
                {
                        uint32_t addr = svga->remap_func(svga, svga->ma) & svga->vram_display_mask;

                        drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
                        chr = svga->vram[addr];
                        attr = svga->vram[addr + 1];

                        if (attr & 8) charaddr = svga->charsetb + (chr * 128);
                        else charaddr = svga->charseta + (chr * 128);

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
                                        p[xx] = (dat & (0x80 >> xx)) ? fg : bg;
                        }
                        else
                        {
                                for (xx = 0; xx < 8; xx++)
                                        p[xx] = (dat & (0x80 >> xx)) ? fg : bg;
                                if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4))
                                        p[8] = bg;
                                else
                                        p[8] = (dat & 1) ? fg : bg;
                        }
                        svga->ma += 4;
                        p += xinc;
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_text_80_ksc5601(svga_t* svga)
{
        if (svga->firstline_draw == 2000)
                svga->firstline_draw = svga->displine;
        svga->lastline_draw = svga->displine;

        if (svga->fullchange)
        {
                int offset = (8 - svga->scrollcache) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat, nextchr;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (svga->seqregs[1] & 1) ? 8 : 9;

                for (x = 0; x < svga->hdisp; x += xinc)
                {
                        uint32_t addr = svga->remap_func(svga, svga->ma) & svga->vram_display_mask;

                        drawcursor = ((svga->ma == svga->ca) && svga->con && svga->cursoron);
                        chr = svga->vram[addr];
                        nextchr = svga->vram[addr + 8];
                        attr = svga->vram[addr + 1];

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

                        if (x + xinc < svga->hdisp && (chr & (nextchr | svga->ksc5601_sbyte_mask) & 0x80))
                        {
                                if ((chr == svga->ksc5601_udc_area_msb[0] || chr == svga->ksc5601_udc_area_msb[1]) && (nextchr > 0xa0 && nextchr < 0xff))
                                        dat = fontdatksc5601_user[(chr == svga->ksc5601_udc_area_msb[1] ? 96 : 0) + (nextchr & 0x7F) - 0x20][svga->sc];
                                else if (nextchr & 0x80)
                                {
                                        if (svga->ksc5601_swap_mode == 1 && (nextchr > 0xa0 && nextchr < 0xff))
                                        {
                                                if (chr >= 0x80 && chr < 0x99) chr += 0x30;
                                                else if (chr >= 0xB0 && chr < 0xC9) chr -= 0x30;
                                        }
                                        dat = fontdatksc5601[((chr & 0x7F) << 7) | (nextchr & 0x7F)][svga->sc];
                                }
                                else
                                        dat = 0xFF;
                        }
                        else
                        {
                                if (attr & 8) charaddr = svga->charsetb + (chr * 128);
                                else charaddr = svga->charseta + (chr * 128);

                                if ((svga->ksc5601_english_font_type >> 8) == 1) dat = fontdatksc5601[((svga->ksc5601_english_font_type & 0x7F) << 7) | (chr >> 1)][((chr & 1) << 4) | svga->sc];
                                else dat = svga->vram[charaddr + (svga->sc << 2)];
                        }
                        if (svga->seqregs[1] & 1)
                        {
                                for (xx = 0; xx < 8; xx++)
                                        p[xx] = (dat & (0x80 >> xx)) ? fg : bg;
                        }
                        else
                        {
                                for (xx = 0; xx < 8; xx++)
                                        p[xx] = (dat & (0x80 >> xx)) ? fg : bg;
                                if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4))
                                        p[8] = bg;
                                else
                                        p[8] = (dat & 1) ? fg : bg;
                        }
                        svga->ma += 4;
                        p += xinc;

                        if (x + xinc < svga->hdisp && (chr & (nextchr | svga->ksc5601_sbyte_mask) & 0x80))
                        {
                                attr = svga->vram[((svga->ma << 1) + 1) & svga->vram_display_mask];

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

                                if ((chr == svga->ksc5601_udc_area_msb[0] || chr == svga->ksc5601_udc_area_msb[1]) && (nextchr > 0xa0 && nextchr < 0xff))
                                        dat = fontdatksc5601_user[(chr == svga->ksc5601_udc_area_msb[1] ? 96 : 0) + (nextchr & 0x7F) - 0x20][svga->sc + 16];
                                else if (nextchr & 0x80)
                                {
                                        dat = fontdatksc5601[((chr & 0x7F) << 7) | (nextchr & 0x7F)][svga->sc + 16];
                                }
                                else
                                        dat = 0xFF;
                                if (svga->seqregs[1] & 1)
                                {
                                        for (xx = 0; xx < 8; xx++)
                                                p[xx] = (dat & (0x80 >> xx)) ? fg : bg;
                                }
                                else
                                {
                                        for (xx = 0; xx < 8; xx++)
                                                p[xx] = (dat & (0x80 >> xx)) ? fg : bg;
                                        if ((chr & ~0x1F) != 0xC0 || !(svga->attrregs[0x10] & 4))
                                                p[8] = bg;
                                        else
                                                p[8] = (dat & 1) ? fg : bg;
                                }

                                svga->ma += 4;
                                p += xinc;
                                x += xinc;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_2bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = ((8 - svga->scrollcache) << 1) + 16;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                for (x = 0; x <= svga->hdisp; x += 16)
                {
                        uint32_t addr = svga->remap_func(svga, svga->ma);
                        uint8_t dat[2];

                        dat[0] = svga->vram[addr];
                        dat[1] = svga->vram[addr + 1];
                        svga->ma += 4;
                        svga->ma &= svga->vram_display_mask;

                        p[0] = p[1] = svga->pallook[svga->egapal[(dat[0] >> 6) & 3]];
                        p[2] = p[3] = svga->pallook[svga->egapal[(dat[0] >> 4) & 3]];
                        p[4] = p[5] = svga->pallook[svga->egapal[(dat[0] >> 2) & 3]];
                        p[6] = p[7] = svga->pallook[svga->egapal[dat[0] & 3]];
                        p[8] = p[9] = svga->pallook[svga->egapal[(dat[1] >> 6) & 3]];
                        p[10] = p[11] = svga->pallook[svga->egapal[(dat[1] >> 4) & 3]];
                        p[12] = p[13] = svga->pallook[svga->egapal[(dat[1] >> 2) & 3]];
                        p[14] = p[15] = svga->pallook[svga->egapal[dat[1] & 3]];

                        p += 16;
                }
        }
}

void svga_render_2bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - svga->scrollcache) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                for (x = 0; x <= svga->hdisp; x += 8)
                {
                        uint32_t addr = svga->remap_func(svga, svga->ma);
                        uint8_t dat[2];

                        dat[0] = svga->vram[addr];
                        dat[1] = svga->vram[addr + 1];
                        svga->ma += 4;
                        svga->ma &= svga->vram_display_mask;

                        *p++ = svga->pallook[svga->egapal[(dat[0] >> 6) & 3]];
                        *p++ = svga->pallook[svga->egapal[(dat[0] >> 4) & 3]];
                        *p++ = svga->pallook[svga->egapal[(dat[0] >> 2) & 3]];
                        *p++ = svga->pallook[svga->egapal[dat[0] & 3]];
                        *p++ = svga->pallook[svga->egapal[(dat[1] >> 6) & 3]];
                        *p++ = svga->pallook[svga->egapal[(dat[1] >> 4) & 3]];
                        *p++ = svga->pallook[svga->egapal[(dat[1] >> 2) & 3]];
                        *p++ = svga->pallook[svga->egapal[dat[1] & 3]];
                }
        }
}

void svga_render_4bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = ((8 - svga->scrollcache) << 1) + 16;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                for (x = 0; x <= svga->hdisp; x += 16)
                {
                        uint8_t edat[4];
                        uint8_t dat;
                        uint32_t addr = svga->remap_func(svga, svga->ma);

                        *(uint32_t*)(&edat[0]) = *(uint32_t*)(&svga->vram[addr]);
                        svga->ma += 4;
                        svga->ma &= svga->vram_display_mask;

                        dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
                        p[0] = p[1] = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        p[2] = p[3] = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                        dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
                        p[4] = p[5] = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        p[6] = p[7] = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                        dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
                        p[8] = p[9] = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        p[10] = p[11] = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                        dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
                        p[12] = p[13] = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        p[14] = p[15] = svga->pallook[svga->egapal[dat & svga->plane_mask]];

                        p += 16;
                }
        }
}

void svga_render_4bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - svga->scrollcache) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                for (x = 0; x <= svga->hdisp; x += 8)
                {
                        uint8_t edat[4];
                        uint8_t dat;
                        uint32_t addr = svga->remap_func(svga, svga->ma);

                        *(uint32_t*)(&edat[0]) = *(uint32_t*)(&svga->vram[addr]);
                        svga->ma += 4;
                        svga->ma &= svga->vram_display_mask;

                        dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
                        *p++ = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        *p++ = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                        dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
                        *p++ = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        *p++ = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                        dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
                        *p++ = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        *p++ = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                        dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
                        *p++ = svga->pallook[svga->egapal[(dat >> 4) & svga->plane_mask]];
                        *p++ = svga->pallook[svga->egapal[dat & svga->plane_mask]];
                }
        }
}

void svga_render_8bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - (svga->scrollcache & 6)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 8)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[svga->ma & svga->vram_display_mask]);

                                p[0] = p[1] = svga->pallook[dat & 0xff];
                                p[2] = p[3] = svga->pallook[(dat >> 8) & 0xff];
                                p[4] = p[5] = svga->pallook[(dat >> 16) & 0xff];
                                p[6] = p[7] = svga->pallook[(dat >> 24) & 0xff];

                                svga->ma += 4;
                                p += 8;
                        }
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 8)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                p[0] = p[1] = svga->pallook[dat & 0xff];
                                p[2] = p[3] = svga->pallook[(dat >> 8) & 0xff];
                                p[4] = p[5] = svga->pallook[(dat >> 16) & 0xff];
                                p[6] = p[7] = svga->pallook[(dat >> 24) & 0xff];

                                svga->ma += 4;
                                p += 8;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_8bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 8)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[svga->ma & svga->vram_display_mask]);
                                *p++ = svga->pallook[dat & 0xff];
                                *p++ = svga->pallook[(dat >> 8) & 0xff];
                                *p++ = svga->pallook[(dat >> 16) & 0xff];
                                *p++ = svga->pallook[(dat >> 24) & 0xff];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
                                *p++ = svga->pallook[dat & 0xff];
                                *p++ = svga->pallook[(dat >> 8) & 0xff];
                                *p++ = svga->pallook[(dat >> 16) & 0xff];
                                *p++ = svga->pallook[(dat >> 24) & 0xff];

                                svga->ma += 8;
                        }
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 4)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                svga->ma += 4;

                                *p++ = svga->pallook[dat & 0xff];
                                *p++ = svga->pallook[(dat >> 8) & 0xff];
                                *p++ = svga->pallook[(dat >> 16) & 0xff];
                                *p++ = svga->pallook[(dat >> 24) & 0xff];
                        }
                }

                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_15bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - (svga->scrollcache & 6)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 4)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);

                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);

                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];
                        }
                        svga->ma += x << 1;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 2)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];

                                svga->ma += 4;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_15bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 8)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);
                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);
                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 8) & svga->vram_display_mask]);
                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 12) & svga->vram_display_mask]);
                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];
                        }

                        svga->ma += x << 1;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 2)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                *p++ = video_15to32[dat & 0xffff];
                                *p++ = video_15to32[dat >> 16];

                                svga->ma += 4;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_16bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - (svga->scrollcache & 6)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 4)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);

                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);

                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];
                        }
                        svga->ma += x << 1;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 2)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];

                                svga->ma += 4;
                        }
                }
                svga->ma += x << 1;
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_16bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 8)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1)) & svga->vram_display_mask]);
                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 4) & svga->vram_display_mask]);
                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 8) & svga->vram_display_mask]);
                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];

                                dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 1) + 12) & svga->vram_display_mask]);
                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];
                        }

                        svga->ma += x << 1;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 2)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                *p++ = video_16to32[dat & 0xffff];
                                *p++ = video_16to32[dat >> 16];

                                svga->ma += 4;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_24bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - (svga->scrollcache & 6)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t dat0 = *(uint32_t*)(&svga->vram[svga->ma & svga->vram_display_mask]);
                                uint32_t dat1 = *(uint32_t*)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
                                uint32_t dat2 = *(uint32_t*)(&svga->vram[(svga->ma + 8) & svga->vram_display_mask]);

                                p[0] = p[1] = dat0 & 0xffffff;
                                p[2] = p[3] = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
                                p[4] = p[5] = (dat1 >> 16) | ((dat2 & 0xff) << 16);
                                p[6] = p[7] = dat2 >> 8;

                                svga->ma += 12;
                        }
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 4)
                        {
                                uint32_t dat0, dat1, dat2;
                                uint32_t addr;

                                addr = svga->remap_func(svga, svga->ma);
                                dat0 = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                addr = svga->remap_func(svga, svga->ma + 4);
                                dat1 = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                addr = svga->remap_func(svga, svga->ma + 8);
                                dat2 = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                p[0] = p[1] = dat0 & 0xffffff;
                                p[2] = p[3] = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
                                p[4] = p[5] = (dat1 >> 16) | ((dat2 & 0xff) << 16);
                                p[6] = p[7] = dat2 >> 8;

                                svga->ma += 12;
                        }
                }
        }
}

void svga_render_24bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x += 4)
                        {
                                uint32_t dat0 = *(uint32_t*)(&svga->vram[svga->ma & svga->vram_display_mask]);
                                uint32_t dat1 = *(uint32_t*)(&svga->vram[(svga->ma + 4) & svga->vram_display_mask]);
                                uint32_t dat2 = *(uint32_t*)(&svga->vram[(svga->ma + 8) & svga->vram_display_mask]);

                                *p++ = dat0 & 0xffffff;
                                *p++ = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
                                *p++ = (dat1 >> 16) | ((dat2 & 0xff) << 16);
                                *p++ = dat2 >> 8;

                                svga->ma += 12;
                        }
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x += 4)
                        {
                                uint32_t dat0, dat1, dat2;
                                uint32_t addr;

                                addr = svga->remap_func(svga, svga->ma);
                                dat0 = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                addr = svga->remap_func(svga, svga->ma + 4);
                                dat1 = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                addr = svga->remap_func(svga, svga->ma + 8);
                                dat2 = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);

                                *p++ = dat0 & 0xffffff;
                                *p++ = (dat0 >> 24) | ((dat1 & 0xffff) << 8);
                                *p++ = (dat1 >> 16) | ((dat2 & 0xff) << 16);
                                *p++ = dat2 >> 8;

                                svga->ma += 12;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_32bpp_lowres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - (svga->scrollcache & 6)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
                                *p++ = dat & 0xffffff;
                                *p++ = dat & 0xffffff;
                        }
                        svga->ma += x * 4;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr]);
                                *p++ = dat & 0xffffff;
                                *p++ = dat & 0xffffff;
                                svga->ma += 4;
                        }
                }
                svga->ma &= svga->vram_display_mask;
        }
}

/*72%
  91%*/
void svga_render_32bpp_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
                                *p++ = dat & 0xffffff;
                        }
                        svga->ma += x * 4;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                *p++ = dat & 0xffffff;
                                svga->ma += 4;
                        }
                }

                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_ABGR8888_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
                                *p++ = ((dat & 0xff0000) >> 16) | (dat & 0x00ff00) | ((dat & 0x0000ff) << 16);
                        }
                        svga->ma += x * 4;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                *p++ = ((dat & 0xff0000) >> 16) | (dat & 0x00ff00) | ((dat & 0x0000ff) << 16);
                                svga->ma += 4;
                        }
                }

                svga->ma &= svga->vram_display_mask;
        }
}

void svga_render_RGBA8888_highres(svga_t* svga)
{
        uint32_t changed_addr = svga->remap_func(svga, svga->ma);

        if (svga->changedvram[changed_addr >> 12] || svga->changedvram[(changed_addr >> 12) + 1] || svga->fullchange)
        {
                int x;
                int offset = (8 - ((svga->scrollcache & 6) >> 1)) + 24;
                uint32_t* p = &((uint32_t*)buffer32->line[svga->displine])[offset];

                if (svga->firstline_draw == 2000)
                        svga->firstline_draw = svga->displine;
                svga->lastline_draw = svga->displine;

                if (!svga->remap_required)
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t dat = *(uint32_t*)(&svga->vram[(svga->ma + (x << 2)) & svga->vram_display_mask]);
                                *p++ = dat >> 8;
                        }
                        svga->ma += x * 4;
                }
                else
                {
                        for (x = 0; x <= svga->hdisp; x++)
                        {
                                uint32_t addr = svga->remap_func(svga, svga->ma);
                                uint32_t dat = *(uint32_t*)(&svga->vram[addr & svga->vram_display_mask]);
                                *p++ = dat >> 8;
                                svga->ma += 4;
                        }
                }

                svga->ma &= svga->vram_display_mask;
        }
}
