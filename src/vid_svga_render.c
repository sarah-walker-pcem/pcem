#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"

void svga_render_blank()
{
        int x, xx;
        
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

void svga_render_text_40()
{     
        if (firstline_draw == 2000) firstline_draw = displine;
        lastline_draw = displine;
        
        if (fullchange)
        {
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (seqregs[1] & 1) ? 16 : 18;
                
                for (x=0;x<svga_hdisp;x+=xinc)
                {
                        drawcursor=((ma==ca) && con && cursoron);
                        chr=vram[(ma<<1)];
                        attr=vram[(ma<<1)+4];
                        if (attr&8) charaddr=charsetb+(chr*128);
                        else        charaddr=charseta+(chr*128);

                        if (drawcursor) 
                        { 
                                bg=pallook[egapal[attr&15]]; 
                                fg=pallook[egapal[attr>>4]]; 
                        }
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
                        if (seqregs[1]&1) 
                        { 
                                for (xx=0;xx<8;xx++) 
                                        ((uint32_t *)buffer32->line[displine])[(x+32+(xx<<1))&2047]=((uint32_t *)buffer32->line[displine])[(x+33+(xx<<1))&2047]=(dat&(0x80>>xx))?fg:bg; 
                        }
                        else
                        {
                                for (xx=0;xx<8;xx++) 
                                        ((uint32_t *)buffer32->line[displine])[(x+32+(xx<<1))&2047]=((uint32_t *)buffer32->line[displine])[(x+33+(xx<<1))&2047]=(dat&(0x80>>xx))?fg:bg;
                                if ((chr&~0x1F)!=0xC0 || !(attrregs[0x10]&4)) 
                                        ((uint32_t *)buffer32->line[displine])[(x+32+16)&2047]=((uint32_t *)buffer32->line[displine])[(x+32+17)&2047]=bg;
                                else                  
                                        ((uint32_t *)buffer32->line[displine])[(x+32+16)&2047]=((uint32_t *)buffer32->line[displine])[(x+32+17)&2047]=(dat&1)?fg:bg;
                        }
                        ma+=4; ma&=vrammask;
                }
        }
}

void svga_render_text_80()
{
        if (firstline_draw == 2000) firstline_draw = displine;
        lastline_draw = displine;
        
        if (fullchange)
        {
                int x, xx;
                int drawcursor;
                uint8_t chr, attr, dat;
                uint32_t charaddr;
                int fg, bg;
                int xinc = (seqregs[1] & 1) ? 8 : 9;

                for (x=0;x<svga_hdisp;x+=xinc)
                {
                        drawcursor=((ma==ca) && con && cursoron);
                        chr=vram[(ma<<1)];
                        attr=vram[(ma<<1)+4];
                        if (attr&8) charaddr=charsetb+(chr*128);
                        else        charaddr=charseta+(chr*128);

                        if (drawcursor) 
                        { 
                                bg=pallook[egapal[attr&15]]; 
                                fg=pallook[egapal[attr>>4]]; 
                        }
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
                        if (seqregs[1]&1) 
                        { 
                                for (xx=0;xx<8;xx++) 
                                        ((uint32_t *)buffer32->line[displine])[(x+32+xx)&2047]=(dat&(0x80>>xx))?fg:bg; 
                        }
                        else
                        {
                                for (xx=0;xx<8;xx++) 
                                        ((uint32_t *)buffer32->line[displine])[(x+32+xx)&2047]=(dat&(0x80>>xx))?fg:bg;
                                if ((chr&~0x1F)!=0xC0 || !(attrregs[0x10]&4)) 
                                        ((uint32_t *)buffer32->line[displine])[(x+32+8)&2047]=bg;
                                else                  
                                        ((uint32_t *)buffer32->line[displine])[(x+32+8)&2047]=(dat&1)?fg:bg;
                        }
                        ma+=4; ma&=vrammask;
                }
        }
}

void svga_render_4bpp_lowres()
{
        int x, offset;
        uint8_t edat[4], dat;
        
        if (firstline_draw == 2000) firstline_draw = displine;
        lastline_draw = displine;

        offset=((8-scrollcache)<<1)+16;
        for (x = 0; x <= svga_hdisp; x += 16)
        {
                edat[0]=vram[ma];
                edat[1]=vram[ma|0x1];
                edat[2]=vram[ma|0x2];
                edat[3]=vram[ma|0x3];
                ma+=4; ma&=vrammask;

                dat=edatlookup[edat[0]&3][edat[1]&3]|(edatlookup[edat[2]&3][edat[3]&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+14+offset]=((uint32_t *)buffer32->line[displine])[x+15+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+12+offset]=((uint32_t *)buffer32->line[displine])[x+13+offset]=pallook[egapal[dat>>4]];
                dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+10+offset]=((uint32_t *)buffer32->line[displine])[x+11+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+8+offset]= ((uint32_t *)buffer32->line[displine])[x+9+offset]=pallook[egapal[dat>>4]];
                dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+6+offset]= ((uint32_t *)buffer32->line[displine])[x+7+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+4+offset]= ((uint32_t *)buffer32->line[displine])[x+5+offset]=pallook[egapal[dat>>4]];
                dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                ((uint32_t *)buffer32->line[displine])[x+2+offset]= ((uint32_t *)buffer32->line[displine])[x+3+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+offset]=   ((uint32_t *)buffer32->line[displine])[x+1+offset]=pallook[egapal[dat>>4]];
        }
}

void svga_render_4bpp_highres()
{
        int x, offset;
        uint8_t edat[4], dat;
        
        if (firstline_draw == 2000) firstline_draw = displine;
        lastline_draw = displine;

        offset=(8-scrollcache)+24;
        for (x = 0; x <= svga_hdisp; x += 8)
        {
                edat[0]=vram[ma];
                edat[1]=vram[ma|0x1];
                edat[2]=vram[ma|0x2];
                edat[3]=vram[ma|0x3];
                ma+=4; ma&=vrammask;

                dat=edatlookup[edat[0]&3][edat[1]&3]|(edatlookup[edat[2]&3][edat[3]&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+7+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+6+offset]=pallook[egapal[dat>>4]];
                dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+5+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+4+offset]=pallook[egapal[dat>>4]];
                dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+3+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+2+offset]=pallook[egapal[dat>>4]];
                dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                ((uint32_t *)buffer32->line[displine])[x+1+offset]=pallook[egapal[dat&0xF]];
                ((uint32_t *)buffer32->line[displine])[x+offset]=  pallook[egapal[dat>>4]];
        }
}

void svga_render_2bpp_lowres()
{
        int x, offset;
        uint8_t edat[4], dat;
                
        if (firstline_draw == 2000) firstline_draw = displine;
        lastline_draw = displine;
        offset=((8-scrollcache)<<1)+16;
        /*Low res (320) only, though high res (640) should be possible*/
        for (x = 0; x <= svga_hdisp; x += 16)
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

                ((uint32_t *)buffer32->line[displine])[x+14+offset]=((uint32_t *)buffer32->line[displine])[x+15+offset]=pallook[egapal[edat[1]&3]];
                ((uint32_t *)buffer32->line[displine])[x+12+offset]=((uint32_t *)buffer32->line[displine])[x+13+offset]=pallook[egapal[(edat[1]>>2)&3]];
                dat=edatlookup[(edat[0]>>2)&3][(edat[1]>>2)&3]|(edatlookup[(edat[2]>>2)&3][(edat[3]>>2)&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+10+offset]=((uint32_t *)buffer32->line[displine])[x+11+offset]=pallook[egapal[(edat[1]>>4)&3]];
                ((uint32_t *)buffer32->line[displine])[x+8+offset]= ((uint32_t *)buffer32->line[displine])[x+9+offset]=pallook[egapal[(edat[1]>>6)&3]];
                dat=edatlookup[(edat[0]>>4)&3][(edat[1]>>4)&3]|(edatlookup[(edat[2]>>4)&3][(edat[3]>>4)&3]<<2);
                ((uint32_t *)buffer32->line[displine])[x+6+offset]= ((uint32_t *)buffer32->line[displine])[x+7+offset]=pallook[egapal[(edat[0]>>0)&3]];
                ((uint32_t *)buffer32->line[displine])[x+4+offset]= ((uint32_t *)buffer32->line[displine])[x+5+offset]=pallook[egapal[(edat[0]>>2)&3]];
                dat=edatlookup[edat[0]>>6][edat[1]>>6]|(edatlookup[edat[2]>>6][edat[3]>>6]<<2);
                ((uint32_t *)buffer32->line[displine])[x+2+offset]= ((uint32_t *)buffer32->line[displine])[x+3+offset]=pallook[egapal[(edat[0]>>4)&3]];
                ((uint32_t *)buffer32->line[displine])[x+offset]=   ((uint32_t *)buffer32->line[displine])[x+1+offset]=pallook[egapal[(edat[0]>>6)&3]];
        }
}

void svga_render_8bpp_lowres()
{
        int x, offset;
        uint8_t edat[4];
                
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset=(8-(scrollcache&6))+24;
                                                                
                for (x = 0; x <= svga_hdisp; x += 8)
                {
                        edat[0]=vram[ma];
                        edat[1]=vram[ma|0x1];
                        edat[2]=vram[ma|0x2];
                        edat[3]=vram[ma|0x3];
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x+6+offset]= ((uint32_t *)buffer32->line[displine])[x+7+offset]=pallook[edat[3]];
                        ((uint32_t *)buffer32->line[displine])[x+4+offset]= ((uint32_t *)buffer32->line[displine])[x+5+offset]=pallook[edat[2]];
                        ((uint32_t *)buffer32->line[displine])[x+2+offset]= ((uint32_t *)buffer32->line[displine])[x+3+offset]=pallook[edat[1]];
                        ((uint32_t *)buffer32->line[displine])[x+offset]=   ((uint32_t *)buffer32->line[displine])[x+1+offset]=pallook[edat[0]];
                }
        }
}

void svga_render_8bpp_highres()
{
        int x, offset;
        uint8_t edat[4];
                
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset = (8 - ((scrollcache & 6) >> 1)) + 24;
                                                                
                for (x = 0; x <= svga_hdisp; x += 8)
                {
                        edat[0]=vram[ma];
                        edat[1]=vram[ma|0x1];
                        edat[2]=vram[ma|0x2];
                        edat[3]=vram[ma|0x3];
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x+3+offset] = pallook[edat[3]];
                        ((uint32_t *)buffer32->line[displine])[x+2+offset] = pallook[edat[2]];
                        ((uint32_t *)buffer32->line[displine])[x+1+offset] = pallook[edat[1]];
                        ((uint32_t *)buffer32->line[displine])[x+offset]   = pallook[edat[0]];
                        edat[0]=vram[ma];
                        edat[1]=vram[ma|0x1];
                        edat[2]=vram[ma|0x2];
                        edat[3]=vram[ma|0x3];
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x+7+offset] = pallook[edat[3]];
                        ((uint32_t *)buffer32->line[displine])[x+6+offset] = pallook[edat[2]];
                        ((uint32_t *)buffer32->line[displine])[x+5+offset] = pallook[edat[1]];
                        ((uint32_t *)buffer32->line[displine])[x+4+offset] = pallook[edat[0]];
                }
        }
}

void svga_render_15bpp_lowres()
{
        int x, offset;
        uint16_t fg, bg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset=(8-(scrollcache&6))+24;

                for (x = 0; x <= svga_hdisp; x += 4)
                {
                        fg=vram[ma]|(vram[ma|0x1]<<8);
                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x+2+offset]=((uint32_t *)buffer32->line[displine])[x+3+offset]=((bg&31)<<3)|(((bg>>5)&31)<<11)|(((bg>>10)&31)<<19);
                        ((uint32_t *)buffer32->line[displine])[x+0+offset]=((uint32_t *)buffer32->line[displine])[x+1+offset]=((fg&31)<<3)|(((fg>>5)&31)<<11)|(((fg>>10)&31)<<19);
                }
        }
}

void svga_render_15bpp_highres()
{
        int x, offset;
        uint16_t fg, bg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset = (8 - ((scrollcache & 6) >> 1)) + 24;
                                                                
                for (x = 0; x <= svga_hdisp; x += 2)
                {
                        fg=vram[ma]|(vram[ma|0x1]<<8);
                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x + 1 + offset] = ((bg&31)<<3)|(((bg>>5)&31)<<11)|(((bg>>10)&31)<<19);
                        ((uint32_t *)buffer32->line[displine])[x + 0 + offset] = ((fg&31)<<3)|(((fg>>5)&31)<<11)|(((fg>>10)&31)<<19);
                }
        }
}

void svga_render_16bpp_lowres()
{
        int x, offset;
        uint16_t fg, bg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset=(8-(scrollcache&6))+24;

                for (x = 0; x <= svga_hdisp; x += 4)
                {
                        fg=vram[ma]|(vram[ma|0x1]<<8);
                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x+2+offset]=((uint32_t *)buffer32->line[displine])[x+3+offset]=((bg&31)<<3)|(((bg>>5)&63)<<10)|(((bg>>11)&31)<<19);
                        ((uint32_t *)buffer32->line[displine])[x+0+offset]=((uint32_t *)buffer32->line[displine])[x+1+offset]=((fg&31)<<3)|(((fg>>5)&63)<<10)|(((fg>>11)&31)<<19);
                }
        }
}

void svga_render_16bpp_highres()
{
        int x, offset;
        uint16_t fg, bg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset = (8 - ((scrollcache & 6) >> 1)) + 24;
                                                                
                for (x = 0; x <= svga_hdisp; x += 2)
                {
                        fg=vram[ma]|(vram[ma|0x1]<<8);
                        bg=vram[ma|0x2]|(vram[ma|0x3]<<8);
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x + 1 + offset] = ((bg&31)<<3)|(((bg>>5)&63)<<10)|(((bg>>11)&31)<<19);
                        ((uint32_t *)buffer32->line[displine])[x + 0 + offset] = ((fg&31)<<3)|(((fg>>5)&63)<<10)|(((fg>>11)&31)<<19);
                }
        }
}

void svga_render_24bpp_lowres()
{
        int x, offset;
        uint32_t fg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset=(8-(scrollcache&6))+24;

                for (x = 0; x <= svga_hdisp; x++)
                {
                        fg=vram[ma]|(vram[ma+1]<<8)|(vram[ma+2]<<16);
                        ma+=3; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[(x<<1)+offset]=((uint32_t *)buffer32->line[displine])[(x<<1)+1+offset]=fg;
                }
        }
}

void svga_render_24bpp_highres()
{
        int x, offset;
        uint32_t fg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset = (8 - ((scrollcache & 6) >> 1)) + 24;

                for (x = 0; x <= svga_hdisp; x++)
                {
                        fg=vram[ma]|(vram[ma+1]<<8)|(vram[ma+2]<<16);
                        ma+=3; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x + offset] = fg;
                }
        }
}

void svga_render_32bpp_lowres()
{
        int x, offset;
        uint32_t fg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset=(8-(scrollcache&6))+24;

                for (x = 0; x <= svga_hdisp; x++)
                {
                        fg = vram[ma] | (vram[ma + 1] << 8) | (vram[ma + 2] << 16);
                        ma += 4; ma &= vrammask;
                        ((uint32_t *)buffer32->line[displine])[(x << 1) + offset] = ((uint32_t *)buffer32->line[displine])[(x << 1) + 1 + offset] = fg;
                }
        }
}

void svga_render_32bpp_highres()
{
        int x, offset;
        uint32_t fg;
        
        if (changedvram[ma>>10] || changedvram[(ma>>10)+1] || changedvram[(ma>>10)+2] || fullchange)
        {
                if (firstline_draw == 2000) firstline_draw = displine;
                lastline_draw = displine;

                offset = (8 - ((scrollcache & 6) >> 1)) + 24;

                for (x = 0; x <= svga_hdisp; x++)
                {
                        fg=vram[ma]|(vram[ma+1]<<8)|(vram[ma+2]<<16);
                        ma+=4; ma&=vrammask;
                        ((uint32_t *)buffer32->line[displine])[x + offset] = fg;
                }
        }
}

