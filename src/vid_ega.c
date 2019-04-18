/*EGA emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "timer.h"
#include "video.h"
#include "vid_ega.h"

extern uint8_t edatlookup[4][4];

static uint8_t ega_rotate[8][256];

static uint32_t pallook16[256], pallook64[256];

/*3C2 controls default mode on EGA. On VGA, it determines monitor type (mono or colour)*/
int egaswitchread,egaswitches=9; /*7=CGA mode (200 lines), 9=EGA mode (350 lines), 8=EGA mode (200 lines)*/

void ega_out(uint16_t addr, uint8_t val, void *p)
{
        ega_t *ega = (ega_t *)p;
        int c;
        uint8_t o, old;
        
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(ega->miscout & 1)) 
                addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3c0:
                if (!ega->attrff)
                   ega->attraddr = val & 31;
                else
                {
                        ega->attrregs[ega->attraddr & 31] = val;
                        if (ega->attraddr < 16) 
                                fullchange = changeframecount;
                        if (ega->attraddr == 0x10 || ega->attraddr == 0x14 || ega->attraddr < 0x10)
                        {
                                for (c = 0; c < 16; c++)
                                {
                                        if (ega->attrregs[0x10] & 0x80) ega->egapal[c] = (ega->attrregs[c] &  0xf) | ((ega->attrregs[0x14] & 0xf) << 4);
                                        else                            ega->egapal[c] = (ega->attrregs[c] & 0x3f) | ((ega->attrregs[0x14] & 0xc) << 4);
                                }
                        }
                }
                ega->attrff ^= 1;
                break;
                case 0x3c2:
                egaswitchread = val & 0xc;
                ega->vres = !(val & 0x80);
                ega->pallook = ega->vres ? pallook16 : pallook64;
                ega->vidclock = val & 4; /*printf("3C2 write %02X\n",val);*/
                ega->miscout=val;
                break;
                case 0x3c4: 
                ega->seqaddr = val; 
                break;
                case 0x3c5:
                o = ega->seqregs[ega->seqaddr & 0xf];
                ega->seqregs[ega->seqaddr & 0xf] = val;
                if (o != val && (ega->seqaddr & 0xf) == 1) 
                        ega_recalctimings(ega);
                switch (ega->seqaddr & 0xf)
                {
                        case 1: 
                        if (ega->scrblank && !(val & 0x20)) 
                                fullchange = 3; 
                        ega->scrblank = (ega->scrblank & ~0x20) | (val & 0x20); 
                        break;
                        case 2: 
                        ega->writemask = val & 0xf; 
                        break;
                        case 3:
                        ega->charsetb = (((val >> 2) & 3) * 0x10000) + 2;
                        ega->charseta = ((val & 3)        * 0x10000) + 2;
                        break;
                        case 4:
                        ega->chain2_write = !(val & 4);
                        break;
                }
                break;
                case 0x3ce: 
                ega->gdcaddr = val; 
                break;
                case 0x3cf:
                ega->gdcreg[ega->gdcaddr & 15] = val;
                switch (ega->gdcaddr & 15)
                {
                        case 2: 
                        ega->colourcompare = val; 
                        break;
                        case 4: 
                        ega->readplane = val & 3; 
                        break;
                        case 5: 
                        ega->writemode = val & 3;
                        ega->readmode = val & 8; 
                        ega->chain2_read = val & 0x10;
                        break;
                        case 6:
//                                pclog("Write mapping %02X\n", val);
                        switch (val & 0xc)
                        {
                                case 0x0: /*128k at A0000*/
                                mem_mapping_set_addr(&ega->mapping, 0xa0000, 0x20000);
                                break;
                                case 0x4: /*64k at A0000*/
                                mem_mapping_set_addr(&ega->mapping, 0xa0000, 0x10000);
                                break;
                                case 0x8: /*32k at B0000*/
                                mem_mapping_set_addr(&ega->mapping, 0xb0000, 0x08000);
                                break;
                                case 0xC: /*32k at B8000*/
                                mem_mapping_set_addr(&ega->mapping, 0xb8000, 0x08000);
                                break;
                        }
                        break;
                        case 7: 
                        ega->colournocare = val; 
                        break;
                }
                break;
                case 0x3d4:
                        pclog("Write 3d4 %02X  %04X:%04X\n", val, CS, cpu_state.pc);
                ega->crtcreg = val & 31;
                return;
                case 0x3d5:
                        pclog("Write 3d5 %02X %02X %02X\n", ega->crtcreg, val, ega->crtc[0x11]);
//                if (ega->crtcreg == 1 && val == 0x14)
//                        fatal("Here\n");
                if (ega->crtcreg <= 7 && ega->crtc[0x11] & 0x80) return;
                old = ega->crtc[ega->crtcreg];
                ega->crtc[ega->crtcreg] = val;
                if (old != val)
                {
                        if (ega->crtcreg < 0xe || ega->crtcreg > 0x10)
                        {
                                fullchange = changeframecount;
                                ega_recalctimings(ega);
                        }
                }
                break;
        }
}

uint8_t ega_in(uint16_t addr, void *p)
{
        ega_t *ega = (ega_t *)p;

        if (addr != 0x3da && addr != 0x3ba)
                pclog("ega_in %04X\n", addr);
        if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(ega->miscout & 1)) 
                addr ^= 0x60;

        switch (addr)
        {
                case 0x3c0: 
                return ega->attraddr;
                case 0x3c1: 
                return ega->attrregs[ega->attraddr];
                case 0x3c2:
//                printf("Read egaswitch %02X %02X %i\n",egaswitchread,egaswitches,VGA);
                switch (egaswitchread)
                {
                        case 0xc: return (egaswitches & 1) ? 0x10 : 0;
                        case 0x8: return (egaswitches & 2) ? 0x10 : 0;
                        case 0x4: return (egaswitches & 4) ? 0x10 : 0;
                        case 0x0: return (egaswitches & 8) ? 0x10 : 0;
                }
                break;
                case 0x3c4: 
                return ega->seqaddr;
                case 0x3c5:
                return ega->seqregs[ega->seqaddr & 0xf];
                case 0x3ce: 
                return ega->gdcaddr;
                case 0x3cf:
                return ega->gdcreg[ega->gdcaddr & 0xf];
                case 0x3d4:
                return ega->crtcreg;
                case 0x3d5:
                return ega->crtc[ega->crtcreg];
                case 0x3da:
                ega->attrff = 0;
                ega->stat ^= 0x30; /*Fools IBM EGA video BIOS self-test*/
                return ega->stat;
        }
//        printf("Bad EGA read %04X %04X:%04X\n",addr,cs>>4,pc);
        return 0xff;
}

void ega_recalctimings(ega_t *ega)
{
	double _dispontime, _dispofftime, disptime;
        double crtcconst;

        ega->vtotal = ega->crtc[6];
        ega->dispend = ega->crtc[0x12];
        ega->vsyncstart = ega->crtc[0x10];
        ega->split = ega->crtc[0x18];

        if (ega->crtc[7] & 1)  ega->vtotal |= 0x100;
        if (ega->crtc[7] & 32) ega->vtotal |= 0x200;
        ega->vtotal++;

        if (ega->crtc[7] & 2)  ega->dispend |= 0x100;
        if (ega->crtc[7] & 64) ega->dispend |= 0x200;
        ega->dispend++;

        if (ega->crtc[7] & 4)   ega->vsyncstart |= 0x100;
        if (ega->crtc[7] & 128) ega->vsyncstart |= 0x200;
        ega->vsyncstart++;

        if (ega->crtc[7] & 0x10) ega->split |= 0x100;
        if (ega->crtc[9] & 0x40) ega->split |= 0x200;
        ega->split+=2;

        ega->hdisp = ega->crtc[1];
        ega->hdisp++;

        ega->rowoffset = ega->crtc[0x13];

        printf("Recalc! %i %i %i %i   %i %02X\n", ega->vtotal, ega->dispend, ega->vsyncstart, ega->split, ega->hdisp, ega->attrregs[0x16]);

        if (ega->vidclock) crtcconst = (ega->seqregs[1] & 1) ? MDACONST : (MDACONST * (9.0 / 8.0));
        else               crtcconst = (ega->seqregs[1] & 1) ? CGACONST : (CGACONST * (9.0 / 8.0));

        disptime = ega->crtc[0] + 2;
        _dispontime = ega->crtc[1] + 1;

        printf("Disptime %f dispontime %f hdisp %i\n", disptime, _dispontime, ega->crtc[1] * 8);
        if (ega->seqregs[1] & 8) 
        { 
                disptime*=2; 
                _dispontime*=2; 
        }
        _dispofftime = disptime - _dispontime;
        _dispontime  *= crtcconst;
        _dispofftime *= crtcconst;

	ega->dispontime  = (uint64_t)_dispontime;
	ega->dispofftime = (uint64_t)_dispofftime;
        pclog("dispontime %i (%f)  dispofftime %i (%f)\n", ega->dispontime, (float)ega->dispontime,
                                                           ega->dispofftime, (float)ega->dispofftime);
//        printf("EGA horiz total %i display end %i clock rate %i vidclock %i %i\n",crtc[0],crtc[1],egaswitchread,vidclock,((ega3c2>>2)&3) | ((tridentnewctrl2<<2)&4));
//        printf("EGA vert total %i display end %i max row %i vsync %i\n",ega_vtotal,ega_dispend,(crtc[9]&31)+1,ega_vsyncstart);
//        printf("total %f on %f cycles off %f cycles frame %f sec %f %02X\n",disptime*crtcconst,dispontime,dispofftime,(dispontime+dispofftime)*ega_vtotal,(dispontime+dispofftime)*ega_vtotal*70,seqregs[1]);
}

static void ega_draw_text(ega_t *ega)
{
        int x, xx;
        
        for (x = 0; x < ega->hdisp; x++)
        {
                int drawcursor = ((ega->ma == ega->ca) && ega->con && ega->cursoron);
                uint8_t chr  = ega->vram[(ega->ma << 1) & ega->vrammask];
                uint8_t attr = ega->vram[((ega->ma << 1) + 1) & ega->vrammask];
                uint8_t dat;
                uint32_t fg, bg;
                uint32_t charaddr;
        
                if (attr & 8)
                        charaddr = ega->charsetb + (chr * 128);
                else
                        charaddr = ega->charseta + (chr * 128);

                if (drawcursor) 
                { 
                        bg = ega->pallook[ega->egapal[attr & 15]]; 
                        fg = ega->pallook[ega->egapal[attr >> 4]]; 
                }
                else
                {
                        fg = ega->pallook[ega->egapal[attr & 15]];
                        bg = ega->pallook[ega->egapal[attr >> 4]];
                        if (attr & 0x80 && ega->attrregs[0x10] & 8)
                        {
                                bg = ega->pallook[ega->egapal[(attr >> 4) & 7]];
                                if (ega->blink & 16) 
                                        fg = bg;
                        }
                }

                dat = ega->vram[charaddr + (ega->sc << 2)];
                if (ega->seqregs[1] & 8)
                {
                        if (ega->seqregs[1] & 1) 
                        { 
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x << 4) + 32 + (xx << 1)) & 2047] =
                                        ((uint32_t *)buffer32->line[ega->displine])[((x << 4) + 33 + (xx << 1)) & 2047] = (dat & (0x80 >> xx)) ? fg : bg; 
                        }
                        else
                        {
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 18) + 32 + (xx << 1)) & 2047] = 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 18) + 33 + (xx << 1)) & 2047] = (dat & (0x80 >> xx)) ? fg : bg;
                                if ((chr & ~0x1f) != 0xc0 || !(ega->attrregs[0x10] & 4)) 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 18) + 32 + 16) & 2047] = 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 18) + 32 + 17) & 2047] = bg;
                                else
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 18) + 32 + 16) & 2047] = 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 18) + 32 + 17) & 2047] = (dat & 1) ? fg : bg;
                        }
                }
                else
                {
                        if (ega->seqregs[1] & 1) 
                        { 
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x << 3) + 32 + xx) & 2047] = (dat & (0x80 >> xx)) ? fg : bg; 
                        }
                        else
                        {
                                for (xx = 0; xx < 8; xx++) 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 9) + 32 + xx) & 2047] = (dat & (0x80 >> xx)) ? fg : bg;
                                if ((chr & ~0x1f) != 0xc0 || !(ega->attrregs[0x10] & 4)) 
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 9) + 32 + 8) & 2047] = bg;
                                else                  
                                        ((uint32_t *)buffer32->line[ega->displine])[((x * 9) + 32 + 8) & 2047] = (dat & 1) ? fg : bg;
                        }
                }
                ega->ma += 4; 
                ega->ma &= ega->vrammask;
        }
}

static void ega_draw_2bpp(ega_t *ega)
{
        int x;
        int offset = ((8 - ega->scrollcache) << 1) + 16;
        
        for (x = 0; x <= ega->hdisp; x++)
        {
                uint8_t edat[2];
                uint32_t addr = ega->ma;
                                
                if (!(ega->crtc[0x17] & 0x40))
                {
                        addr = (addr << 1) & ega->vrammask;
                        addr &= ~7;
                        if ((ega->crtc[0x17] & 0x20) && (ega->ma & 0x20000))
                                addr |= 4;
                        if (!(ega->crtc[0x17] & 0x20) && (ega->ma & 0x8000))
                                addr |= 4;
                }
                if (!(ega->crtc[0x17] & 0x01))
                        addr = (addr & ~0x8000) | ((ega->sc & 1) ? 0x8000 : 0);
                if (!(ega->crtc[0x17] & 0x02))
                        addr = (addr & ~0x10000) | ((ega->sc & 2) ? 0x10000 : 0);

                edat[0] = ega->vram[addr];
                edat[1] = ega->vram[addr | 0x1];
                if (ega->seqregs[1] & 4)
                        ega->ma += 2;
                else
                        ega->ma += 4;

                ega->ma &= ega->vrammask;

                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 14 + offset]=  ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 15 + offset] = ega->pallook[ega->egapal[edat[1] & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 12 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 13 + offset] = ega->pallook[ega->egapal[(edat[1] >> 2) & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 10 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 11 + offset] = ega->pallook[ega->egapal[(edat[1] >> 4) & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  8 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  9 + offset] = ega->pallook[ega->egapal[(edat[1] >> 6) & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  6 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  7 + offset] = ega->pallook[ega->egapal[(edat[0] >> 0) & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  4 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  5 + offset] = ega->pallook[ega->egapal[(edat[0] >> 2) & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  2 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  3 + offset] = ega->pallook[ega->egapal[(edat[0] >> 4) & 3]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +      offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  1 + offset] = ega->pallook[ega->egapal[(edat[0] >> 6) & 3]];
        }
}

static void ega_draw_4bpp_lowres(ega_t *ega)
{
        int x;
        int offset = ((8 - ega->scrollcache) << 1) + 16;
        
        for (x = 0; x <= ega->hdisp; x++)
        {
                uint8_t edat[4];
                uint8_t dat;
                uint32_t addr = ega->ma;
                int oddeven = 0;
                                
                if (!(ega->crtc[0x17] & 0x40))
                {
                        addr = (addr << 1) & ega->vrammask;
                        if (ega->seqregs[1] & 4)
                                oddeven = (addr & 4) ? 1 : 0;
                        addr &= ~7;
                        if ((ega->crtc[0x17] & 0x20) && (ega->ma & 0x20000))
                                addr |= 4;
                        if (!(ega->crtc[0x17] & 0x20) && (ega->ma & 0x8000))
                                addr |= 4;
                }
                if (!(ega->crtc[0x17] & 0x01))
                        addr = (addr & ~0x8000) | ((ega->sc & 1) ? 0x8000 : 0);
                if (!(ega->crtc[0x17] & 0x02))
                        addr = (addr & ~0x10000) | ((ega->sc & 2) ? 0x10000 : 0);

                if (ega->seqregs[1] & 4)
                {
                        edat[0] = ega->vram[addr | oddeven];
                        edat[2] = ega->vram[addr | oddeven | 0x2];
                        edat[1] = edat[3] = 0;
                        ega->ma += 2;
                }
                else
                {
                        edat[0] = ega->vram[addr];
                        edat[1] = ega->vram[addr | 0x1];
                        edat[2] = ega->vram[addr | 0x2];
                        edat[3] = ega->vram[addr | 0x3];
                        ega->ma += 4;
                }
                ega->ma &= ega->vrammask;

                dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 14 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 15 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 12 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 13 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
                dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 10 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) + 11 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  8 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  9 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
                dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  6 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  7 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  4 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  5 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
                dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  2 + offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  3 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +      offset] = ((uint32_t *)buffer32->line[ega->displine])[(x << 4) +  1 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
        }
}

static void ega_draw_4bpp_highres(ega_t *ega)
{
        int x;
        int offset = (8 - ega->scrollcache) + 24;
        
        for (x = 0; x <= ega->hdisp; x++)
        {
                uint8_t edat[4];
                uint8_t dat;
                uint32_t addr = ega->ma;
                int oddeven = 0;

                if (!(ega->crtc[0x17] & 0x40))
                {
                        addr = (addr << 1) & ega->vrammask;
                        if (ega->seqregs[1] & 4)
                                oddeven = (addr & 4) ? 1 : 0;
                        addr &= ~7;
                        if ((ega->crtc[0x17] & 0x20) && (ega->ma & 0x20000))
                                addr |= 4;
                        if (!(ega->crtc[0x17] & 0x20) && (ega->ma & 0x8000))
                                addr |= 4;
                }
                if (!(ega->crtc[0x17] & 0x01))
                        addr = (addr & ~0x8000) | ((ega->sc & 1) ? 0x8000 : 0);
                if (!(ega->crtc[0x17] & 0x02))
                        addr = (addr & ~0x10000) | ((ega->sc & 2) ? 0x10000 : 0);

                if (ega->seqregs[1] & 4)
                {
                        edat[0] = ega->vram[addr | oddeven];
                        edat[2] = ega->vram[addr | oddeven | 0x2];
                        edat[1] = edat[3] = 0;
                        ega->ma += 2;
                }
                else
                {
                        edat[0] = ega->vram[addr];
                        edat[1] = ega->vram[addr | 0x1];
                        edat[2] = ega->vram[addr | 0x2];
                        edat[3] = ega->vram[addr | 0x3];
                        ega->ma += 4;
                }
                ega->ma &= ega->vrammask;

                dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 7 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 6 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
                dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 5 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 4 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
                dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 3 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 2 + offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
                dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) + 1 + offset] = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
                ((uint32_t *)buffer32->line[ega->displine])[(x << 3) +     offset] = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
        }
}

void ega_poll(void *p)
{
        ega_t *ega = (ega_t *)p;
        int x, xx;

        if (!ega->linepos)
        {
		timer_advance_u64(&ega->timer, ega->dispofftime);

                ega->stat |= 1;
                ega->linepos = 1;

                if (ega->dispon)
                {
                        if (ega->firstline == 2000) 
                        {
                                ega->firstline = ega->displine;
                                video_wait_for_buffer();
                        }

                        if (ega->scrblank)
                        {
                                for (x = 0; x < ega->hdisp; x++)
                                {
                                        switch (ega->seqregs[1] & 9)
                                        {
                                                case 0:
                                                for (xx = 0; xx < 9; xx++)  ((uint32_t *)buffer32->line[ega->displine])[(x * 9) + xx + 32] = 0;
                                                break;
                                                case 1:
                                                for (xx = 0; xx < 8; xx++)  ((uint32_t *)buffer32->line[ega->displine])[(x * 8) + xx + 32] = 0;
                                                break;
                                                case 8:
                                                for (xx = 0; xx < 18; xx++) ((uint32_t *)buffer32->line[ega->displine])[(x * 18) + xx + 32] = 0;
                                                break;
                                                case 9:
                                                for (xx = 0; xx < 16; xx++) ((uint32_t *)buffer32->line[ega->displine])[(x * 16) + xx + 32] = 0;
                                                break;
                                        }
                                }
                        }
                        else if (!(ega->gdcreg[6] & 1))
                        {
                                if (fullchange)
                                        ega_draw_text(ega);
                        }
                        else
                        {
                                switch (ega->gdcreg[5] & 0x20)
                                {
                                        case 0x00:
                                        if (ega->seqregs[1] & 8)
                                                ega_draw_4bpp_lowres(ega);
                                        else
                                                ega_draw_4bpp_highres(ega);
                                        break;
                                        case 0x20:
                                        ega_draw_2bpp(ega);
                                        break;
                                }
                        }
                        if (ega->lastline < ega->displine) 
                                ega->lastline = ega->displine;
                }

                ega->displine++;
                if ((ega->stat & 8) && ((ega->displine & 15) == (ega->crtc[0x11] & 15)) && ega->vslines)
                        ega->stat &= ~8;
                ega->vslines++;
                if (ega->displine > 500) 
                        ega->displine = 0;
        }
        else
        {
		timer_advance_u64(&ega->timer, ega->dispontime);
//                if (output) printf("Display on %f\n",vidtime);
                if (ega->dispon) 
                        ega->stat &= ~1;
                ega->linepos = 0;
                if (ega->sc == (ega->crtc[11] & 31)) 
                   ega->con = 0; 
                if (ega->dispon)
                {
                        if (ega->sc == (ega->crtc[9] & 31))
                        {
                                ega->sc = 0;
                                if (ega->sc == (ega->crtc[11] & 31))
                                        ega->con = 0;

                                ega->maback += (ega->rowoffset << 3);
                                ega->maback &= ega->vrammask;
                                ega->ma = ega->maback;
                        }
                        else
                        {
                                ega->sc++;
                                ega->sc &= 31;
                                ega->ma = ega->maback;
                        }
                }
                ega->vc++;
                ega->vc &= 1023;
//                printf("Line now %i %i ma %05X\n",vc,displine,ma);
                if (ega->vc == ega->split)
                {
//                        printf("Split at line %i %i\n",displine,vc);
                        ega->ma = ega->maback = 0;
                        if (ega->attrregs[0x10] & 0x20)
                                ega->scrollcache = 0;
                }
                if (ega->vc == ega->dispend)
                {
//                        printf("Display over at line %i %i\n",displine,vc);
                        ega->dispon=0;
                        if (ega->crtc[10] & 0x20) ega->cursoron = 0;
                        else                      ega->cursoron = ega->blink & 16;
                        if (!(ega->gdcreg[6] & 1) && !(ega->blink & 15)) 
                                fullchange = 2;
                        ega->blink++;

                        if (fullchange) 
                                fullchange--;
                }
                if (ega->vc == ega->vsyncstart)
                {
                        ega->dispon = 0;
//                        printf("Vsync on at line %i %i\n",displine,vc);
                        ega->stat |= 8;
                        if (ega->seqregs[1] & 8) x = ega->hdisp * ((ega->seqregs[1] & 1) ? 8 : 9) * 2;
                        else                     x = ega->hdisp * ((ega->seqregs[1] & 1) ? 8 : 9);
//                        pclog("Cursor %02X %02X\n",crtc[10],crtc[11]);
//                        pclog("Firstline %i Lastline %i wx %i %i\n",firstline,lastline,wx,oddeven);
//                        doblit();
                        if (x != xsize || (ega->lastline - ega->firstline) != ysize)
                        {
                                xsize = x;
                                ysize = ega->lastline - ega->firstline;
                                if (xsize < 64) xsize = 656;
                                if (ysize < 32) ysize = 200;
                                if (ega->vres || ysize <= 200)
                                        updatewindowsize(xsize, ysize << 1);
                                else
                                        updatewindowsize(xsize, ysize);
                        }
                                        
                        video_blit_memtoscreen(32, 0, ega->firstline, ega->lastline, xsize, ega->lastline - ega->firstline);

                        ega->frames++;
                        ega->video_res_x = xsize;
                        ega->video_res_y = ysize+1;
                        if (!(ega->gdcreg[6] & 1)) /*Text mode*/
                        {
                                ega->video_res_x /= (ega->seqregs[1] & 1) ? 8 : 9;
                                ega->video_res_y /= (ega->crtc[9] & 31) + 1;
                                ega->video_bpp = 0;
                        }
                        else
                        {
                                if (ega->crtc[9] & 0x80)
                                   ega->video_res_y /= 2;
                                if (!(ega->crtc[0x17] & 1))
                                   ega->video_res_y *= 2;
                                ega->video_res_y /= (ega->crtc[9] & 31) + 1;                                   
                                if (ega->seqregs[1] & 8)
                                   ega->video_res_x /= 2;
                                ega->video_bpp = (ega->gdcreg[5] & 0x20) ? 2 : 4;
                        }

//                        wakeupblit();
                        readflash=0;
                        //framecount++;
                        ega->firstline = 2000;
                        ega->lastline = 0;

                        ega->maback = ega->ma = (ega->crtc[0xc] << 8)| ega->crtc[0xd];
                        ega->ca = (ega->crtc[0xe] << 8) | ega->crtc[0xf];
                        ega->ma <<= 2;
                        ega->maback <<= 2;
                        ega->ca <<= 2;
                        changeframecount = 2;
                        ega->vslines = 0;
                }
                if (ega->vc == ega->vtotal)
                {
                        ega->vc = 0;
                        ega->sc = ega->crtc[8] & 0x1f;
                        ega->dispon = 1;
                        ega->displine = 0;
                        ega->scrollcache = ega->attrregs[0x13] & 7;
                }
                if (ega->sc == (ega->crtc[10] & 31)) 
                        ega->con = 1;
        }
}


void ega_write(uint32_t addr, uint8_t val, void *p)
{
        ega_t *ega = (ega_t *)p;
        uint8_t vala, valb, valc, vald;
        int writemask2 = ega->writemask;

        egawrites++;
        cycles -= video_timing_write_b;
        cycles_lost += video_timing_write_b;
        
        if (addr >= 0xB0000) addr &= 0x7fff;
        else                 addr &= 0xffff;

        if (ega->chain2_write)
        {
                writemask2 &= ~0xa;
                if (addr & 1)
                        writemask2 <<= 1;
                addr &= ~1;
                if (addr & 0x4000)
                        addr |= 1;
                addr &= ~0x4000;
        }

        addr <<= 2;

        if (addr >= ega->vram_limit)
                return;

        if (!(ega->gdcreg[6] & 1)) 
                fullchange = 2;

//        pclog("%i %08X %i %i %02X   %02X %02X %02X %02X\n",chain4,addr,writemode,writemask,gdcreg[8],vram[0],vram[1],vram[2],vram[3]);
        switch (ega->writemode)
        {
                case 1:
                if (writemask2 & 1) ega->vram[addr]       = ega->la;
                if (writemask2 & 2) ega->vram[addr | 0x1] = ega->lb;
                if (writemask2 & 4) ega->vram[addr | 0x2] = ega->lc;
                if (writemask2 & 8) ega->vram[addr | 0x3] = ega->ld;
                break;
                case 0:
                if (ega->gdcreg[3] & 7) 
                        val = ega_rotate[ega->gdcreg[3] & 7][val];
                        
                if (ega->gdcreg[8] == 0xff && !(ega->gdcreg[3] & 0x18) && !ega->gdcreg[1])
                {
                        if (writemask2 & 1) ega->vram[addr]       = val;
                        if (writemask2 & 2) ega->vram[addr | 0x1] = val;
                        if (writemask2 & 4) ega->vram[addr | 0x2] = val;
                        if (writemask2 & 8) ega->vram[addr | 0x3] = val;
                }
                else
                {
                        if (ega->gdcreg[1] & 1) vala = (ega->gdcreg[0] & 1) ? 0xff : 0;
                        else                    vala = val;
                        if (ega->gdcreg[1] & 2) valb = (ega->gdcreg[0] & 2) ? 0xff : 0;
                        else                    valb = val;
                        if (ega->gdcreg[1] & 4) valc = (ega->gdcreg[0] & 4) ? 0xff : 0;
                        else                    valc = val;
                        if (ega->gdcreg[1] & 8) vald = (ega->gdcreg[0] & 8) ? 0xff : 0;
                        else                    vald = val;
//                                pclog("Write %02X %01X %02X %02X %02X %02X  %02X\n",gdcreg[3]&0x18,writemask,vala,valb,valc,vald,gdcreg[8]);
                        switch (ega->gdcreg[3] & 0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala & ega->gdcreg[8]) | (ega->la & ~ega->gdcreg[8]);
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb & ega->gdcreg[8]) | (ega->lb & ~ega->gdcreg[8]);
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc & ega->gdcreg[8]) | (ega->lc & ~ega->gdcreg[8]);
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald & ega->gdcreg[8]) | (ega->ld & ~ega->gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala | ~ega->gdcreg[8]) & ega->la;
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb | ~ega->gdcreg[8]) & ega->lb;
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc | ~ega->gdcreg[8]) & ega->lc;
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald | ~ega->gdcreg[8]) & ega->ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala & ega->gdcreg[8]) | ega->la;
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb & ega->gdcreg[8]) | ega->lb;
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc & ega->gdcreg[8]) | ega->lc;
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald & ega->gdcreg[8]) | ega->ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala & ega->gdcreg[8]) ^ ega->la;
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb & ega->gdcreg[8]) ^ ega->lb;
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc & ega->gdcreg[8]) ^ ega->lc;
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald & ega->gdcreg[8]) ^ ega->ld;
                                break;
                        }
//                                pclog("- %02X %02X %02X %02X   %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                }
                break;
                case 2:
                if (!(ega->gdcreg[3] & 0x18) && !ega->gdcreg[1])
                {
                        if (writemask2 & 1) ega->vram[addr]       = (((val & 1) ? 0xff : 0) & ega->gdcreg[8]) | (ega->la & ~ega->gdcreg[8]);
                        if (writemask2 & 2) ega->vram[addr | 0x1] = (((val & 2) ? 0xff : 0) & ega->gdcreg[8]) | (ega->lb & ~ega->gdcreg[8]);
                        if (writemask2 & 4) ega->vram[addr | 0x2] = (((val & 4) ? 0xff : 0) & ega->gdcreg[8]) | (ega->lc & ~ega->gdcreg[8]);
                        if (writemask2 & 8) ega->vram[addr | 0x3] = (((val & 8) ? 0xff : 0) & ega->gdcreg[8]) | (ega->ld & ~ega->gdcreg[8]);
                }
                else
                {
                        vala = ((val & 1) ? 0xff : 0);
                        valb = ((val & 2) ? 0xff : 0);
                        valc = ((val & 4) ? 0xff : 0);
                        vald = ((val & 8) ? 0xff : 0);
                        switch (ega->gdcreg[3] & 0x18)
                        {
                                case 0: /*Set*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala & ega->gdcreg[8]) | (ega->la & ~ega->gdcreg[8]);
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb & ega->gdcreg[8]) | (ega->lb & ~ega->gdcreg[8]);
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc & ega->gdcreg[8]) | (ega->lc & ~ega->gdcreg[8]);
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald & ega->gdcreg[8]) | (ega->ld & ~ega->gdcreg[8]);
                                break;
                                case 8: /*AND*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala | ~ega->gdcreg[8]) & ega->la;
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb | ~ega->gdcreg[8]) & ega->lb;
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc | ~ega->gdcreg[8]) & ega->lc;
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald | ~ega->gdcreg[8]) & ega->ld;
                                break;
                                case 0x10: /*OR*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala & ega->gdcreg[8]) | ega->la;
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb & ega->gdcreg[8]) | ega->lb;
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc & ega->gdcreg[8]) | ega->lc;
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald & ega->gdcreg[8]) | ega->ld;
                                break;
                                case 0x18: /*XOR*/
                                if (writemask2 & 1) ega->vram[addr]       = (vala & ega->gdcreg[8]) ^ ega->la;
                                if (writemask2 & 2) ega->vram[addr | 0x1] = (valb & ega->gdcreg[8]) ^ ega->lb;
                                if (writemask2 & 4) ega->vram[addr | 0x2] = (valc & ega->gdcreg[8]) ^ ega->lc;
                                if (writemask2 & 8) ega->vram[addr | 0x3] = (vald & ega->gdcreg[8]) ^ ega->ld;
                                break;
                        }
                }
                break;
        }
}

uint8_t ega_read(uint32_t addr, void *p)
{
        ega_t *ega = (ega_t *)p;
        uint8_t temp, temp2, temp3, temp4;
        int readplane = ega->readplane;
        
        egareads++;
        cycles -= video_timing_read_b;
        cycles_lost += video_timing_read_b;
//        pclog("Readega %06X   ",addr);
        if (addr >= 0xb0000) addr &= 0x7fff;
        else                 addr &= 0xffff;

        if (ega->chain2_read)
        {
                readplane = (readplane & 2) | (addr & 1);
                addr &= ~1;
                if (addr & 0x4000)
                        addr |= 1;
                addr &= ~0x4000;
        }

        addr <<= 2;
        if (addr >= ega->vram_limit)
                return 0xff;
                
        ega->la = ega->vram[addr];
        ega->lb = ega->vram[addr | 0x1];
        ega->lc = ega->vram[addr | 0x2];
        ega->ld = ega->vram[addr | 0x3];
        if (ega->readmode)
        {
                temp   = ega->la;
                temp  ^= (ega->colourcompare & 1) ? 0xff : 0;
                temp  &= (ega->colournocare & 1)  ? 0xff : 0;
                temp2  = ega->lb;
                temp2 ^= (ega->colourcompare & 2) ? 0xff : 0;
                temp2 &= (ega->colournocare & 2)  ? 0xff : 0;
                temp3  = ega->lc;
                temp3 ^= (ega->colourcompare & 4) ? 0xff : 0;
                temp3 &= (ega->colournocare & 4)  ? 0xff : 0;
                temp4  = ega->ld;
                temp4 ^= (ega->colourcompare & 8) ? 0xff : 0;
                temp4 &= (ega->colournocare & 8)  ? 0xff : 0;
                return ~(temp | temp2 | temp3 | temp4);
        }
        return ega->vram[addr | readplane];
}

void ega_init(ega_t *ega, int monitor_type, int is_mono)
{
        int c, d, e;
        
        ega->vram = malloc(0x40000);
        ega->vrammask = 0x3ffff;
        
        for (c = 0; c < 256; c++)
        {
                e = c;
                for (d = 0; d < 8; d++)
                {
                        ega_rotate[d][c] = e;
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
                }
        }

        if (is_mono)
        {
                for (c = 0; c < 256; c++)
                {
                        switch (monitor_type >> 4)
                        {
                                case DISPLAY_GREEN:
                                switch ((c >> 3) & 3)
                                {
                                        case 0:
                                        pallook64[c] = pallook16[c] = makecol32(0, 0, 0);
                                        break;
                                        case 2:
                                        pallook64[c] = pallook16[c] = makecol32(0x04, 0x8a, 0x20);
                                        break;
                                        case 1:
                                        pallook64[c] = pallook16[c] = makecol32(0x08, 0xc7, 0x2c);
                                        break;
                                        case 3:
                                        pallook64[c] = pallook16[c] = makecol32(0x34, 0xff, 0x5d);
                                        break;
                                }
                                break;
                                case DISPLAY_AMBER:
                                switch ((c >> 3) & 3)
                                {
                                        case 0:
                                        pallook64[c] = pallook16[c] = makecol32(0, 0, 0);
                                        break;
                                        case 2:
                                        pallook64[c] = pallook16[c] = makecol32(0xb2, 0x4d, 0x00);
                                        break;
                                        case 1:
                                        pallook64[c] = pallook16[c] = makecol32(0xef, 0x79, 0x00);
                                        break;
                                        case 3:
                                        pallook64[c] = pallook16[c] = makecol32(0xff, 0xe3, 0x34);
                                        break;
                                }
                                break;
                                case DISPLAY_WHITE: default:
                                switch ((c >> 3) & 3)
                                {
                                        case 0:
                                        pallook64[c] = pallook16[c] = makecol32(0, 0, 0);
                                        break;
                                        case 2:
                                        pallook64[c] = pallook16[c] = makecol32(0x7a, 0x81, 0x83);
                                        break;
                                        case 1:
                                        pallook64[c] = pallook16[c] = makecol32(0xaf, 0xb3, 0xb0);
                                        break;
                                        case 3:
                                        pallook64[c] = pallook16[c] = makecol32(0xff, 0xfd, 0xed);
                                        break;
                                }
                                break;
                        }
                }
        }
        else
        {
                for (c = 0; c < 256; c++)
                {
                        pallook64[c]  = makecol32(((c >> 2) & 1) * 0xaa, ((c >> 1) & 1) * 0xaa, (c & 1) * 0xaa);
                        pallook64[c] += makecol32(((c >> 5) & 1) * 0x55, ((c >> 4) & 1) * 0x55, ((c >> 3) & 1) * 0x55);
                        pallook16[c]  = makecol32(((c >> 2) & 1) * 0xaa, ((c >> 1) & 1) * 0xaa, (c & 1) * 0xaa);
                        pallook16[c] += makecol32(((c >> 4) & 1) * 0x55, ((c >> 4) & 1) * 0x55, ((c >> 4) & 1) * 0x55);
                        if ((c & 0x17) == 6) 
                                pallook16[c] = makecol32(0xaa, 0x55, 0);
                }
        }
        ega->pallook = pallook16;

        egaswitches = monitor_type & 0xf;
        ega->vram_limit = 256 * 1024;
        ega->vrammask = ega->vram_limit-1;

        timer_add(&ega->timer, ega_poll, ega, 1);
}

void *ega_standalone_init()
{
        ega_t *ega = malloc(sizeof(ega_t));
        memset(ega, 0, sizeof(ega_t));
        int monitor_type;
        
        rom_init(&ega->bios_rom, "ibm_6277356_ega_card_u44_27128.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

        if (ega->bios_rom.rom[0x3ffe] == 0xaa && ega->bios_rom.rom[0x3fff] == 0x55)
        {
                int c;
                pclog("Read EGA ROM in reverse\n");

                for (c = 0; c < 0x2000; c++)
                {
                        uint8_t temp = ega->bios_rom.rom[c];
                        ega->bios_rom.rom[c] = ega->bios_rom.rom[0x3fff - c];
                        ega->bios_rom.rom[0x3fff - c] = temp;
                }
        }

        monitor_type = device_get_config_int("monitor_type");
        ega_init(ega, monitor_type, (monitor_type & 0xf) == 10);

        ega->vram_limit = device_get_config_int("memory") * 1024;
        ega->vrammask = ega->vram_limit-1;
        
        mem_mapping_add(&ega->mapping, 0xa0000, 0x20000, ega_read, NULL, NULL, ega_write, NULL, NULL, NULL, MEM_MAPPING_EXTERNAL, ega);
        io_sethandler(0x03a0, 0x0040, ega_in, NULL, NULL, ega_out, NULL, NULL, ega);

        return ega;
}

static int ega_standalone_available()
{
        return rom_present("ibm_6277356_ega_card_u44_27128.bin");
}

void ega_close(void *p)
{
        ega_t *ega = (ega_t *)p;

        free(ega->vram);
        free(ega);
}

void ega_speed_changed(void *p)
{
        ega_t *ega = (ega_t *)p;
        
        ega_recalctimings(ega);
}

void ega_add_status_info(char *s, int max_len, void *p)
{
        ega_t *ega = (ega_t *)p;
        char temps[128];
        
        if (!ega->video_bpp)      strcpy(temps, "EGA in text mode\n");
        else                      sprintf(temps, "EGA colour depth : %i bpp\n", ega->video_bpp);
        strncat(s, temps, max_len);
        
        sprintf(temps, "EGA resolution : %i x %i\n", ega->video_res_x, ega->video_res_y);
        strncat(s, temps, max_len);
        
        sprintf(temps, "EGA refresh rate : %i Hz\n\n", ega->frames);
        ega->frames = 0;
        strncat(s, temps, max_len);
}

static device_config_t ega_config[] =
{
        {
                .name = "memory",
                .description = "Memory size",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "64 kB",
                                .value = 64
                        },
                        {
                                .description = "128 kB",
                                .value = 128
                        },
                        {
                                .description = "256 kB",
                                .value = 256
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 256
        },
        {
                .name = "monitor_type",
                .description = "Monitor type",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "EGA Colour, 40x25",
                                .value = 6
                        },
                        {
                                .description = "EGA Colour, 80x25",
                                .value = 7
                        },
                        {
                                .description = "EGA Colour, ECD",
                                .value = 9
                        },
                        {
                                .description = "EGA Monochrome (white)",
                                .value = 10 | (DISPLAY_WHITE << 4)
                        },
                        {
                                .description = "EGA Monochrome (green)",
                                .value = 10 | (DISPLAY_GREEN << 4)
                        },
                        {
                                .description = "EGA Monochrome (amber)",
                                .value = 10 | (DISPLAY_AMBER << 4)
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 9
        },
        {
                .type = -1
        }
};

device_t ega_device =
{
        "EGA",
        0,
        ega_standalone_init,
        ega_close,
        ega_standalone_available,
        ega_speed_changed,
        NULL,
        ega_add_status_info,
        ega_config
};
