/*Generic SVGA handling*/
/*This is intended to be used by another SVGA driver, and not as a card in it's own right*/
#include <stdlib.h>
#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "io.h"
#include "timer.h"

#define svga_output 0

void svga_doblit(int y1, int y2, int wx, int wy, svga_t *svga);

extern uint8_t edatlookup[4][4];

uint8_t svga_rotate[8][256];

/*Primary SVGA device. As multiple video cards are not yet supported this is the
  only SVGA device.*/
static svga_t *svga_pri;

svga_t *svga_get_pri() { return svga_pri; }
void svga_set_override(svga_t *svga, int val) {
        if (svga->override && !val)
                svga->fullchange = changeframecount;
        svga->override = val;
}

void svga_out(uint16_t addr, uint8_t val, void *p) {
        svga_t *svga = (svga_t *)p;
        int c;
        uint8_t o;
        //        printf("OUT SVGA %03X %02X %04X:%04X\n",addr,val,CS,pc);
        switch (addr) {
        case 0x3C0:
                if (!svga->attrff) {
                        svga->attraddr = val & 31;
                        if ((val & 0x20) != svga->attr_palette_enable) {
                                svga->fullchange = 3;
                                svga->attr_palette_enable = val & 0x20;
                                svga_recalctimings(svga);
                        }
                } else {
                        if ((svga->attraddr == 0x13) && (svga->attrregs[0x13] != val))
                                svga->fullchange = changeframecount;
                        svga->attrregs[svga->attraddr & 31] = val;
                        if (svga->attraddr < 16)
                                svga->fullchange = changeframecount;
                        if (svga->attraddr == 0x10 || svga->attraddr == 0x14 || svga->attraddr < 0x10) {
                                for (c = 0; c < 16; c++) {
                                        if (svga->attrregs[0x10] & 0x80)
                                                svga->egapal[c] = (svga->attrregs[c] & 0xf) | ((svga->attrregs[0x14] & 0xf) << 4);
                                        else
                                                svga->egapal[c] =
                                                        (svga->attrregs[c] & 0x3f) | ((svga->attrregs[0x14] & 0xc) << 4);
                                }
                        }
                        if (svga->attraddr == 0x10)
                                svga_recalctimings(svga);
                        if (svga->attraddr == 0x12) {
                                if ((val & 0xf) != svga->plane_mask)
                                        svga->fullchange = changeframecount;
                                svga->plane_mask = val & 0xf;
                        }
                }
                svga->attrff ^= 1;
                break;
        case 0x3C2:
                svga->miscout = val;
                svga->vidclock = val & 4; // printf("3C2 write %02X\n",val);
                io_removehandler(0x03a0, 0x0020, svga->video_in, NULL, NULL, svga->video_out, NULL, NULL, svga->p);
                if (!(val & 1))
                        io_sethandler(0x03a0, 0x0020, svga->video_in, NULL, NULL, svga->video_out, NULL, NULL, svga->p);
                svga_recalctimings(svga);
                break;
        case 0x3C4:
                svga->seqaddr = val;
                break;
        case 0x3C5:
                if (svga->seqaddr > 0xf)
                        return;
                o = svga->seqregs[svga->seqaddr & 0xf];
                svga->seqregs[svga->seqaddr & 0xf] = val;
                if (o != val && (svga->seqaddr & 0xf) == 1)
                        svga_recalctimings(svga);
                switch (svga->seqaddr & 0xf) {
                case 1:
                        if (svga->scrblank && !(val & 0x20))
                                svga->fullchange = 3;
                        svga->scrblank = (svga->scrblank & ~0x20) | (val & 0x20);
                        svga_recalctimings(svga);
                        break;
                case 2:
                        svga->writemask = val & 0xf;
                        break;
                case 3:
                        svga->charsetb = (((val >> 2) & 3) * 0x10000) + 2;
                        svga->charseta = ((val & 3) * 0x10000) + 2;
                        if (val & 0x10)
                                svga->charseta += 0x8000;
                        if (val & 0x20)
                                svga->charsetb += 0x8000;
                        break;
                case 4:
                        svga->chain2_write = !(val & 4);
                        svga->chain4 = val & 8;
                        svga->fast = (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) && !svga->gdcreg[1]) &&
                                     ((svga->chain4 && svga->packed_chain4) || svga->fb_only);
                        break;
                }
                break;
        case 0x3c6:
                svga->dac_mask = val;
                break;
        case 0x3C7:
                svga->dac_read = val;
                svga->dac_pos = 0;
                break;
        case 0x3C8:
                svga->dac_write = val;
                svga->dac_read = val - 1;
                svga->dac_pos = 0;
                break;
        case 0x3C9:
                svga->dac_status = 0;
                svga->fullchange = changeframecount;
                switch (svga->dac_pos) {
                case 0:
                        svga->dac_r = val;
                        svga->dac_pos++;
                        break;
                case 1:
                        svga->dac_g = val;
                        svga->dac_pos++;
                        break;
                case 2:
                        svga->vgapal[svga->dac_write].r = svga->dac_r;
                        svga->vgapal[svga->dac_write].g = svga->dac_g;
                        svga->vgapal[svga->dac_write].b = val;
                        if (svga->ramdac_type == RAMDAC_8BIT)
                                svga->pallook[svga->dac_write] =
                                        makecol32(svga->vgapal[svga->dac_write].r, svga->vgapal[svga->dac_write].g,
                                                  svga->vgapal[svga->dac_write].b);
                        else
                                svga->pallook[svga->dac_write] = makecol32((svga->vgapal[svga->dac_write].r & 0x3f) * 4,
                                                                           (svga->vgapal[svga->dac_write].g & 0x3f) * 4,
                                                                           (svga->vgapal[svga->dac_write].b & 0x3f) * 4);
                        svga->dac_pos = 0;
                        svga->dac_write = (svga->dac_write + 1) & 255;
                        break;
                }
                break;
        case 0x3CE:
                svga->gdcaddr = val;
                break;
        case 0x3CF:
                o = svga->gdcreg[svga->gdcaddr & 15];
                switch (svga->gdcaddr & 15) {
                case 2:
                        svga->colourcompare = val;
                        break;
                case 4:
                        svga->readplane = val & 3;
                        break;
                case 5:
                        svga->writemode = val & 3;
                        svga->readmode = val & 8;
                        svga->chain2_read = val & 0x10;
                        break;
                case 6:
                        //                                pclog("svga_out recalcmapping %p\n", svga);
                        if ((svga->gdcreg[6] & 0xc) != (val & 0xc)) {
                                //                                pclog("Write mapping %02X\n", val);
                                switch (val & 0xC) {
                                case 0x0: /*128k at A0000*/
                                        mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x20000);
                                        svga->banked_mask = 0xffff;
                                        break;
                                case 0x4: /*64k at A0000*/
                                        mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
                                        svga->banked_mask = 0xffff;
                                        break;
                                case 0x8: /*32k at B0000*/
                                        mem_mapping_set_addr(&svga->mapping, 0xb0000, 0x08000);
                                        svga->banked_mask = 0x7fff;
                                        break;
                                case 0xC: /*32k at B8000*/
                                        mem_mapping_set_addr(&svga->mapping, 0xb8000, 0x08000);
                                        svga->banked_mask = 0x7fff;
                                        break;
                                }
                        }
                        break;
                case 7:
                        svga->colournocare = val;
                        break;
                }
                svga->gdcreg[svga->gdcaddr & 15] = val;
                svga->fast = (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) && !svga->gdcreg[1]) &&
                             ((svga->chain4 && svga->packed_chain4) || svga->fb_only);
                if (((svga->gdcaddr & 15) == 5 && (val ^ o) & 0x70) || ((svga->gdcaddr & 15) == 6 && (val ^ o) & 1))
                        svga_recalctimings(svga);
                break;
        }
}

uint8_t svga_in(uint16_t addr, void *p) {
        svga_t *svga = (svga_t *)p;
        uint8_t temp;
        //        if (addr!=0x3da) pclog("Read port %04X\n",addr);
        switch (addr) {
        case 0x3C0:
                return svga->attraddr | svga->attr_palette_enable;
        case 0x3C1:
                return svga->attrregs[svga->attraddr];
        case 0x3c2:
                if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x50)
                        temp = 0;
                else
                        temp = 0x10;
                return temp;
        case 0x3C4:
                return svga->seqaddr;
        case 0x3C5:
                return svga->seqregs[svga->seqaddr & 0xF];
        case 0x3c6:
                return svga->dac_mask;
        case 0x3c7:
                return svga->dac_status;
        case 0x3c8:
                return svga->dac_write;
        case 0x3c9:
                svga->dac_status = 3;
                switch (svga->dac_pos) {
                case 0:
                        svga->dac_pos++;
                        if (svga->ramdac_type == RAMDAC_8BIT)
                                return svga->vgapal[svga->dac_read].r;
                        return svga->vgapal[svga->dac_read].r & 0x3f;
                case 1:
                        svga->dac_pos++;
                        if (svga->ramdac_type == RAMDAC_8BIT)
                                return svga->vgapal[svga->dac_read].g;
                        return svga->vgapal[svga->dac_read].g & 0x3f;
                case 2:
                        svga->dac_pos = 0;
                        svga->dac_read = (svga->dac_read + 1) & 255;
                        if (svga->ramdac_type == RAMDAC_8BIT)
                                return svga->vgapal[(svga->dac_read - 1) & 255].b;
                        return svga->vgapal[(svga->dac_read - 1) & 255].b & 0x3f;
                }
                break;
        case 0x3CC:
                return svga->miscout;
        case 0x3CE:
                return svga->gdcaddr;
        case 0x3CF:
                return svga->gdcreg[svga->gdcaddr & 0xf];
        case 0x3DA:
                svga->attrff = 0;

                if (svga->cgastat & 0x01)
                        svga->cgastat &= ~0x30;
                else
                        svga->cgastat ^= 0x30;
                return svga->cgastat;
        }
        //        printf("Bad EGA read %04X %04X:%04X\n",addr,cs>>4,pc);
        return 0xFF;
}

void svga_set_ramdac_type(svga_t *svga, int type) {
        int c;

        if (svga->ramdac_type != type) {
                svga->ramdac_type = type;

                for (c = 0; c < 256; c++) {
                        if (svga->ramdac_type == RAMDAC_8BIT)
                                svga->pallook[c] = makecol32(svga->vgapal[c].r, svga->vgapal[c].g, svga->vgapal[c].b);
                        else
                                svga->pallook[c] = makecol32((svga->vgapal[c].r & 0x3f) * 4, (svga->vgapal[c].g & 0x3f) * 4,
                                                             (svga->vgapal[c].b & 0x3f) * 4);
                }
        }
}

void svga_recalctimings(svga_t *svga) {
        double crtcconst;
        double _dispontime, _dispofftime, disptime;

        svga->vtotal = svga->crtc[6];
        svga->dispend = svga->crtc[0x12];
        svga->vsyncstart = svga->crtc[0x10];
        svga->split = svga->crtc[0x18];
        svga->vblankstart = svga->crtc[0x15];

        if (svga->crtc[7] & 1)
                svga->vtotal |= 0x100;
        if (svga->crtc[7] & 32)
                svga->vtotal |= 0x200;
        svga->vtotal += 2;

        if (svga->crtc[7] & 2)
                svga->dispend |= 0x100;
        if (svga->crtc[7] & 64)
                svga->dispend |= 0x200;
        svga->dispend++;

        if (svga->crtc[7] & 4)
                svga->vsyncstart |= 0x100;
        if (svga->crtc[7] & 128)
                svga->vsyncstart |= 0x200;
        svga->vsyncstart++;

        if (svga->crtc[7] & 0x10)
                svga->split |= 0x100;
        if (svga->crtc[9] & 0x40)
                svga->split |= 0x200;
        svga->split++;

        if (svga->crtc[7] & 0x08)
                svga->vblankstart |= 0x100;
        if (svga->crtc[9] & 0x20)
                svga->vblankstart |= 0x200;
        svga->vblankstart++;

        svga->hdisp = svga->crtc[1];
        svga->hdisp++;

        svga->htotal = svga->crtc[0];
        svga->htotal += 6; /*+6 is required for Tyrian*/

        svga->rowoffset = svga->crtc[0x13];

        svga->clock = (svga->vidclock) ? VGACONST2 : VGACONST1;

        svga->lowres = svga->attrregs[0x10] & 0x40;

        svga->interlace = 0;

        svga->ma_latch = ((svga->crtc[0xc] << 8) | svga->crtc[0xd]) + ((svga->crtc[8] & 0x60) >> 5);
        svga->ca_adj = 0;

        svga->rowcount = svga->crtc[9] & 31;
        svga->linedbl = svga->crtc[9] & 0x80;

        svga->hdisp_time = svga->hdisp;
        svga->render = svga_render_blank;
        if (!svga->scrblank && svga->attr_palette_enable) {
                if (!(svga->gdcreg[6] & 1) && !(svga->attrregs[0x10] & 1)) /*Text mode*/
                {
                        if (svga->seqregs[1] & 8) /*40 column*/
                        {
                                svga->render = svga_render_text_40;
                                svga->hdisp *= (svga->seqregs[1] & 1) ? 16 : 18;
                        } else {
                                svga->render = svga_render_text_80;
                                svga->hdisp *= (svga->seqregs[1] & 1) ? 8 : 9;
                        }
                        svga->hdisp_old = svga->hdisp;
                } else {
                        svga->hdisp *= (svga->seqregs[1] & 8) ? 16 : 8;
                        svga->hdisp_old = svga->hdisp;

                        switch (svga->gdcreg[5] & 0x60) {
                        case 0x00:                        /*16 colours*/
                                if (svga->seqregs[1] & 8) /*Low res (320)*/
                                        svga->render = svga_render_4bpp_lowres;
                                else
                                        svga->render = svga_render_4bpp_highres;
                                break;
                        case 0x20:                        /*4 colours*/
                                if (svga->seqregs[1] & 8) /*Low res (320)*/
                                        svga->render = svga_render_2bpp_lowres;
                                else
                                        svga->render = svga_render_2bpp_highres;
                                break;
                        case 0x40:
                        case 0x60: /*256+ colours*/
                                switch (svga->bpp) {
                                case 8:
                                        if (svga->lowres)
                                                svga->render = svga_render_8bpp_lowres;
                                        else
                                                svga->render = svga_render_8bpp_highres;
                                        break;
                                case 15:
                                        if (svga->lowres)
                                                svga->render = svga_render_15bpp_lowres;
                                        else
                                                svga->render = svga_render_15bpp_highres;
                                        break;
                                case 16:
                                        if (svga->lowres)
                                                svga->render = svga_render_16bpp_lowres;
                                        else
                                                svga->render = svga_render_16bpp_highres;
                                        break;
                                case 24:
                                        if (svga->lowres)
                                                svga->render = svga_render_24bpp_lowres;
                                        else
                                                svga->render = svga_render_24bpp_highres;
                                        break;
                                case 32:
                                        if (svga->lowres)
                                                svga->render = svga_render_32bpp_lowres;
                                        else
                                                svga->render = svga_render_32bpp_highres;
                                        break;
                                }
                                break;
                        }
                }
        }

        //        pclog("svga_render %08X : %08X %08X %08X %08X %08X  %i %i %02X %i %i\n", svga_render, svga_render_text_40,
        //        svga_render_text_80, svga_render_8bpp_lowres, svga_render_8bpp_highres, svga_render_blank,
        //        scrblank,gdcreg[6]&1,gdcreg[5]&0x60,bpp,seqregs[1]&8);

        svga->char_width = (svga->seqregs[1] & 1) ? 8 : 9;
        if (svga->recalctimings_ex)
                svga->recalctimings_ex(svga);

        if (svga->vblankstart < svga->dispend)
                svga->dispend = svga->vblankstart;

        crtcconst = svga->clock * svga->char_width;

        disptime = svga->htotal;
        _dispontime = svga->hdisp_time;

        //        printf("Disptime %f dispontime %f hdisp %i\n",disptime,dispontime,crtc[1]*8);
        if (svga->seqregs[1] & 8) {
                disptime *= 2;
                _dispontime *= 2;
        }
        _dispofftime = disptime - _dispontime;
        _dispontime *= crtcconst;
        _dispofftime *= crtcconst;

        svga->dispontime = (uint64_t)_dispontime;
        svga->dispofftime = (uint64_t)_dispofftime;
        if (svga->dispontime < TIMER_USEC)
                svga->dispontime = TIMER_USEC;
        if (svga->dispofftime < TIMER_USEC)
                svga->dispofftime = TIMER_USEC;

        svga_recalc_remap_func(svga);
        /*        printf("SVGA horiz total %i display end %i vidclock %f\n",svga->crtc[0],svga->crtc[1],svga->clock);
                printf("SVGA vert total %i display end %i max row %i vsync
           %i\n",svga->vtotal,svga->dispend,(svga->crtc[9]&31)+1,svga->vsyncstart); printf("total %f on %i cycles off %i cycles
           frame %i sec %i
           %02X\n",disptime*crtcconst,svga->dispontime,svga->dispofftime,(svga->dispontime+svga->dispofftime)*svga->vtotal,(svga->dispontime+svga->dispofftime)*svga->vtotal*70,svga->seqregs[1]);

                pclog("svga->render %08X\n", svga->render);*/
}

extern int cyc_total;
void svga_poll(void *p) {
        svga_t *svga = (svga_t *)p;
        int x;

        if (!svga->linepos) {
                //                if (!(vc & 15)) pclog("VC %i %i\n", vc, GetTickCount());
                if (svga->displine == svga->hwcursor_latch.y && svga->hwcursor_latch.ena) {
                        svga->hwcursor_on = svga->hwcursor.ysize - svga->hwcursor_latch.yoff;
                        if (svga->hwcursor_on < 0)
                                svga->hwcursor_on = 0;
                        svga->hwcursor_oddeven = 0;
                }

                if (svga->displine == svga->hwcursor_latch.y + 1 && svga->hwcursor_latch.ena && svga->interlace) {
                        svga->hwcursor_on = svga->hwcursor.ysize - svga->hwcursor_latch.yoff;
                        if (svga->hwcursor_on < 0)
                                svga->hwcursor_on = 0;
                        svga->hwcursor_oddeven = 1;
                }

                if (svga->displine == svga->overlay_latch.y && svga->overlay_latch.ena) {
                        svga->overlay_on = svga->overlay_latch.ysize - svga->overlay_latch.yoff;
                        svga->overlay_oddeven = 0;
                }
                if (svga->displine == svga->overlay_latch.y + 1 && svga->overlay_latch.ena && svga->interlace) {
                        svga->overlay_on = svga->overlay_latch.ysize - svga->overlay_latch.yoff;
                        svga->overlay_oddeven = 1;
                }

                timer_advance_u64(&svga->timer, svga->dispofftime);
                //                if (output) printf("Display off %f\n",vidtime);
                svga->cgastat |= 1;
                svga->linepos = 1;

                if (svga->dispon) {
                        svga->hdisp_on = 1;

                        svga->ma &= svga->vram_display_mask;
                        if (svga->firstline == 2000) {
                                svga->firstline = svga->displine;
                                video_wait_for_buffer();
                        }

                        if (svga->hwcursor_on || svga->overlay_on)
                                svga->changedvram[svga->ma >> 12] = svga->changedvram[(svga->ma >> 12) + 1] =
                                        svga->interlace ? 3 : 2;

                        if (!svga->override)
                                svga->render(svga);

                        if (svga->overlay_on) {
                                if (!svga->override)
                                        svga->overlay_draw(svga, svga->displine);
                                svga->overlay_on--;
                                if (svga->overlay_on && svga->interlace)
                                        svga->overlay_on--;
                        }

                        if (svga->hwcursor_on) {
                                if (!svga->override)
                                        svga->hwcursor_draw(svga, svga->displine);
                                svga->hwcursor_on--;
                                if (svga->hwcursor_on && svga->interlace)
                                        svga->hwcursor_on--;
                        }

                        if (svga->lastline < svga->displine)
                                svga->lastline = svga->displine;
                }

                //                pclog("%03i %06X %06X\n",displine,ma,vrammask);
                svga->displine++;
                if (svga->interlace)
                        svga->displine++;
                if ((svga->cgastat & 8) && ((svga->displine & 15) == (svga->crtc[0x11] & 15)) && svga->vslines) {
                        //                        printf("Vsync off at line %i\n",displine);
                        svga->cgastat &= ~8;
                }
                svga->vslines++;
                if (svga->displine > 1500)
                        svga->displine = 0;
                //                pclog("Col is %08X %08X %08X   %i %i  %08X\n",((uint32_t
                //                *)buffer32->line[displine])[320],((uint32_t *)buffer32->line[displine])[321],((uint32_t
                //                *)buffer32->line[displine])[322],
                //                                                                displine, vc, ma);
        } else {
                //                pclog("VC %i ma %05X\n", svga->vc, svga->ma);
                timer_advance_u64(&svga->timer, svga->dispontime);

                //                if (output) printf("Display on %f\n",vidtime);
                if (svga->dispon)
                        svga->cgastat &= ~1;
                svga->hdisp_on = 0;

                svga->linepos = 0;
                if (svga->sc == (svga->crtc[11] & 31))
                        svga->con = 0;
                if (svga->dispon) {
                        if (svga->linedbl && !svga->linecountff) {
                                svga->linecountff = 1;
                                svga->ma = svga->maback;
                        } else if (svga->sc == svga->rowcount) {
                                svga->linecountff = 0;
                                svga->sc = 0;

                                svga->maback += (svga->rowoffset << 3);
                                if (svga->interlace)
                                        svga->maback += (svga->rowoffset << 3);
                                svga->maback &= svga->vram_display_mask;
                                svga->ma = svga->maback;
                        } else {
                                svga->linecountff = 0;
                                svga->sc++;
                                svga->sc &= 31;
                                svga->ma = svga->maback;
                        }
                }
                svga->hsync_divisor = !svga->hsync_divisor;

                if (svga->hsync_divisor && (svga->crtc[0x17] & 4))
                        return;

                svga->vc++;
                svga->vc &= 2047;

                if (svga->vc == svga->split) {
                        int ret = 1;

                        if (svga->line_compare)
                                ret = svga->line_compare(svga);

                        if (ret) {
                                //                               pclog("VC split\n");
                                svga->ma = svga->maback = 0;
                                svga->sc = 0;
                                if (svga->attrregs[0x10] & 0x20)
                                        svga->scrollcache = 0;
                        }
                }
                if (svga->vc == svga->dispend) {
                        if (svga->vblank_start)
                                svga->vblank_start(svga);
                        //                        pclog("VC dispend\n");
                        svga->dispon = 0;
                        if (svga->crtc[10] & 0x20)
                                svga->cursoron = 0;
                        else
                                svga->cursoron = svga->blink & 16;
                        if (!(svga->gdcreg[6] & 1) && !(svga->blink & 15))
                                svga->fullchange = 2;
                        svga->blink++;

                        for (x = 0; x < ((svga->vram_mask + 1) >> 12); x++) {
                                if (svga->changedvram[x])
                                        svga->changedvram[x]--;
                        }
                        //                        memset(changedvram,0,2048);
                        if (svga->fullchange)
                                svga->fullchange--;
                }
                if (svga->vc == svga->vsyncstart) {
                        int wx, wy;
                        //                        pclog("VC vsync  %i %i\n", svga->firstline_draw, svga->lastline_draw);
                        svga->dispon = 0;
                        svga->cgastat |= 8;
                        x = svga->hdisp;

                        if (svga->interlace && !svga->oddeven)
                                svga->lastline++;
                        if (svga->interlace && svga->oddeven)
                                svga->firstline--;

                        wx = x;
                        wy = svga->lastline - svga->firstline;

                        if (!svga->override)
                                svga_doblit(svga->firstline_draw, svga->lastline_draw + 1, wx, wy, svga);

                        readflash = 0;

                        svga->firstline = 2000;
                        svga->lastline = 0;

                        svga->firstline_draw = 2000;
                        svga->lastline_draw = 0;

                        svga->oddeven ^= 1;

                        changeframecount = svga->interlace ? 3 : 2;
                        svga->vslines = 0;

                        if (svga->interlace && svga->oddeven)
                                svga->ma = svga->maback = svga->ma_latch + (svga->rowoffset << 1);
                        else
                                svga->ma = svga->maback = svga->ma_latch;
                        svga->ca = ((svga->crtc[0xe] << 8) | svga->crtc[0xf]) + svga->ca_adj;

                        svga->ma <<= 2;
                        svga->maback <<= 2;
                        svga->ca <<= 2;

                        if (!svga->video_res_override) {
                                svga->video_res_x = wx;
                                svga->video_res_y = wy + 1;

                                if (!(svga->gdcreg[6] & 1) && !(svga->attrregs[0x10] & 1)) /*Text mode*/
                                {
                                        svga->video_res_x /= svga->char_width;
                                        svga->video_res_y /= (svga->crtc[9] & 31) + 1;
                                        svga->video_bpp = 0;
                                } else {
                                        if (svga->crtc[9] & 0x80)
                                                svga->video_res_y /= 2;
                                        if (!(svga->crtc[0x17] & 2))
                                                svga->video_res_y *= 4;
                                        else if (!(svga->crtc[0x17] & 1))
                                                svga->video_res_y *= 2;
                                        svga->video_res_y /= (svga->crtc[9] & 31) + 1;
                                        if (svga->render == svga_render_8bpp_lowres || svga->render == svga_render_15bpp_lowres ||
                                            svga->render == svga_render_16bpp_lowres ||
                                            svga->render == svga_render_24bpp_lowres || svga->render == svga_render_32bpp_lowres)
                                                svga->video_res_x /= 2;

                                        switch (svga->gdcreg[5] & 0x60) {
                                        case 0x00:
                                                svga->video_bpp = 4;
                                                break;
                                        case 0x20:
                                                svga->video_bpp = 2;
                                                break;
                                        case 0x40:
                                        case 0x60:
                                                svga->video_bpp = svga->bpp;
                                                break;
                                        }
                                }
                        }
                        //                        if (svga_interlace && oddeven) ma=maback=ma+(svga_rowoffset<<2);

                        //                        pclog("Addr %08X vson %03X vsoff %01X  %02X %02X %02X %i
                        //                        %i\n",ma,svga_vsyncstart,crtc[0x11]&0xF,crtc[0xD],crtc[0xC],crtc[0x33],
                        //                        svga_interlace, oddeven);

                        if (svga->vsync_callback)
                                svga->vsync_callback(svga);
                }
                if (svga->vc == svga->vtotal) {
                        //                        pclog("VC vtotal\n");

                        //                        printf("Frame over at line %i %i  %i
                        //                        %i\n",displine,vc,svga_vsyncstart,svga_dispend);
                        svga->vc = 0;
                        svga->sc = svga->crtc[8] & 0x1f;
                        svga->dispon = 1;
                        svga->displine = (svga->interlace && svga->oddeven) ? 1 : 0;
                        svga->scrollcache = svga->attrregs[0x13] & 7;
                        svga->linecountff = 0;

                        svga->hwcursor_on = 0;
                        svga->hwcursor_latch = svga->hwcursor;

                        svga->overlay_on = 0;
                        svga->overlay_latch = svga->overlay;
                        //                        pclog("Latch HWcursor addr %08X\n", svga_hwcursor_latch.addr);

                        //                        pclog("ADDR %08X\n",hwcursor_addr);
                }
                if (svga->sc == (svga->crtc[10] & 31))
                        svga->con = 1;
        }
        //        printf("2 %i\n",svga_vsyncstart);
        // pclog("svga_poll %i %i %i %i %i  %i %i\n", ins, svga->dispofftime, svga->dispontime, svga->vidtime, cyc_total,
        // svga->linepos, svga->vc);
}

int svga_init(svga_t *svga, void *p, int memsize, void (*recalctimings_ex)(struct svga_t *svga),
              uint8_t (*video_in)(uint16_t addr, void *p), void (*video_out)(uint16_t addr, uint8_t val, void *p),
              void (*hwcursor_draw)(struct svga_t *svga, int displine), void (*overlay_draw)(struct svga_t *svga, int displine)) {
        int c, d, e;

        svga->p = p;

        for (c = 0; c < 256; c++) {
                e = c;
                for (d = 0; d < 8; d++) {
                        svga_rotate[d][c] = e;
                        e = (e >> 1) | ((e & 1) ? 0x80 : 0);
                }
        }
        svga->readmode = 0;

        svga->crtc[0] = 63;
        svga->crtc[6] = 255;
        svga->dispontime = 1000ull << 32;
        svga->dispofftime = 1000ull << 32;
        svga->bpp = 8;
        svga->vram = malloc(memsize);
        svga->vram_max = memsize;
        svga->vram_display_mask = memsize - 1;
        svga->vram_mask = memsize - 1;
        svga->decode_mask = 0x7fffff;
        svga->changedvram = malloc(/*(memsize >> 12) << 1*/ 0x1000000 >> 12);
        svga->recalctimings_ex = recalctimings_ex;
        svga->video_in = video_in;
        svga->video_out = video_out;
        svga->hwcursor_draw = hwcursor_draw;
        svga->overlay_draw = overlay_draw;
        svga->hwcursor.ysize = 64;
        svga->ksc5601_english_font_type = 0;
        //        _svga_recalctimings(svga);

        mem_mapping_add(&svga->mapping, 0xa0000, 0x20000, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel,
                        NULL, MEM_MAPPING_EXTERNAL, svga);

        timer_add(&svga->timer, svga_poll, svga, 1);

        svga_pri = svga;

        svga->ramdac_type = RAMDAC_6BIT;

        return 0;
}

void svga_close(svga_t *svga) {
        free(svga->changedvram);
        free(svga->vram);

        svga_pri = NULL;
}

void svga_write(uint32_t addr, uint8_t val, void *p) {
        svga_t *svga = (svga_t *)p;
        uint8_t vala, valb, valc, vald, wm = svga->writemask;
        int writemask2 = svga->writemask;

        egawrites++;

        cycles -= video_timing_write_b;
        cycles_lost += video_timing_write_b;

        if (svga_output)
                pclog("Writeega %06X   ", addr);
        addr &= svga->banked_mask;
        addr += svga->write_bank;

        if (!(svga->gdcreg[6] & 1))
                svga->fullchange = 2;
        if ((svga->chain4 && svga->packed_chain4) || svga->fb_only) {
                writemask2 = 1 << (addr & 3);
                addr &= ~3;
        } else if (svga->chain4) {
                writemask2 = 1 << (addr & 3);
                addr &= ~3;
                addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
        } else if (svga->chain2_write) {
                writemask2 &= ~0xa;
                if (addr & 1)
                        writemask2 <<= 1;
                addr &= ~1;
                addr <<= 2;
        } else {
                addr <<= 2;
        }
        addr &= svga->decode_mask;

        if (addr >= svga->vram_max)
                return;

        addr &= svga->vram_mask;

        if (svga_output)
                pclog("%08X (%i, %i) %02X %i %i %i %02X\n", addr, addr & 1023, addr >> 10, val, writemask2, svga->writemode,
                      svga->chain4, svga->gdcreg[8]);
        svga->changedvram[addr >> 12] = changeframecount;

        switch (svga->writemode) {
        case 1:
                if (writemask2 & 1)
                        svga->vram[addr] = svga->la;
                if (writemask2 & 2)
                        svga->vram[addr | 0x1] = svga->lb;
                if (writemask2 & 4)
                        svga->vram[addr | 0x2] = svga->lc;
                if (writemask2 & 8)
                        svga->vram[addr | 0x3] = svga->ld;
                break;
        case 0:
                if (svga->gdcreg[3] & 7)
                        val = svga_rotate[svga->gdcreg[3] & 7][val];
                if (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
                        if (writemask2 & 1)
                                svga->vram[addr] = val;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = val;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = val;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = val;
                } else {
                        if (svga->gdcreg[1] & 1)
                                vala = (svga->gdcreg[0] & 1) ? 0xff : 0;
                        else
                                vala = val;
                        if (svga->gdcreg[1] & 2)
                                valb = (svga->gdcreg[0] & 2) ? 0xff : 0;
                        else
                                valb = val;
                        if (svga->gdcreg[1] & 4)
                                valc = (svga->gdcreg[0] & 4) ? 0xff : 0;
                        else
                                valc = val;
                        if (svga->gdcreg[1] & 8)
                                vald = (svga->gdcreg[0] & 8) ? 0xff : 0;
                        else
                                vald = val;

                        switch (svga->gdcreg[3] & 0x18) {
                        case 0: /*Set*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                                break;
                        case 8: /*AND*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala | ~svga->gdcreg[8]) & svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->ld;
                                break;
                        case 0x10: /*OR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->ld;
                                break;
                        case 0x18: /*XOR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) ^ svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->ld;
                                break;
                        }
                        //                        pclog("- %02X %02X %02X %02X
                        //                        %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                }
                break;
        case 2:
                if (!(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
                        if (writemask2 & 1)
                                svga->vram[addr] = (((val & 1) ? 0xff : 0) & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] =
                                        (((val & 2) ? 0xff : 0) & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] =
                                        (((val & 4) ? 0xff : 0) & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] =
                                        (((val & 8) ? 0xff : 0) & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                } else {
                        vala = ((val & 1) ? 0xff : 0);
                        valb = ((val & 2) ? 0xff : 0);
                        valc = ((val & 4) ? 0xff : 0);
                        vald = ((val & 8) ? 0xff : 0);
                        switch (svga->gdcreg[3] & 0x18) {
                        case 0: /*Set*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                                break;
                        case 8: /*AND*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala | ~svga->gdcreg[8]) & svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->ld;
                                break;
                        case 0x10: /*OR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->ld;
                                break;
                        case 0x18: /*XOR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) ^ svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->ld;
                                break;
                        }
                }
                break;
        case 3:
                if (svga->gdcreg[3] & 7)
                        val = svga_rotate[svga->gdcreg[3] & 7][val];
                wm = svga->gdcreg[8];
                svga->gdcreg[8] &= val;

                vala = (svga->gdcreg[0] & 1) ? 0xff : 0;
                valb = (svga->gdcreg[0] & 2) ? 0xff : 0;
                valc = (svga->gdcreg[0] & 4) ? 0xff : 0;
                vald = (svga->gdcreg[0] & 8) ? 0xff : 0;
                switch (svga->gdcreg[3] & 0x18) {
                case 0: /*Set*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                        break;
                case 8: /*AND*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala | ~svga->gdcreg[8]) & svga->la;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->lb;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->lc;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->ld;
                        break;
                case 0x10: /*OR*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala & svga->gdcreg[8]) | svga->la;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->lb;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->lc;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->ld;
                        break;
                case 0x18: /*XOR*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala & svga->gdcreg[8]) ^ svga->la;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->lb;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->lc;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->ld;
                        break;
                }
                svga->gdcreg[8] = wm;
                break;
        }
}

uint8_t svga_read(uint32_t addr, void *p) {
        svga_t *svga = (svga_t *)p;
        uint8_t temp, temp2, temp3, temp4;
        uint32_t latch_addr;
        int readplane = svga->readplane;

        cycles -= video_timing_read_b;
        cycles_lost += video_timing_read_b;

        egareads++;
        //        pclog("Readega %06X   ",addr);

        addr &= svga->banked_mask;
        addr += svga->read_bank;

        latch_addr = (addr << 2) & svga->decode_mask;

        //        pclog("%05X %i %04X:%04X %02X %02X %i\n",addr,svga->chain4,CS,pc, vram[addr & 0x7fffff], vram[(addr << 2) &
        //        0x7fffff], svga->readmode); pclog("%i\n", svga->readmode);
        if ((svga->chain4 && svga->packed_chain4) || svga->fb_only) {
                addr &= svga->decode_mask;
                if (addr >= svga->vram_max)
                        return 0xff;
                latch_addr = addr & svga->vram_mask & ~3;
                svga->la = svga->vram[latch_addr];
                svga->lb = svga->vram[latch_addr | 0x1];
                svga->lc = svga->vram[latch_addr | 0x2];
                svga->ld = svga->vram[latch_addr | 0x3];
                return svga->vram[addr & svga->vram_mask];
        } else if (svga->chain4) {
                readplane = addr & 3;
                addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
        } else if (svga->chain2_read) {
                readplane = (readplane & 2) | (addr & 1);
                addr &= ~1;
                addr <<= 2;
        } else
                addr <<= 2;

        addr &= svga->decode_mask;

        if (latch_addr >= svga->vram_max) {
                svga->la = svga->lb = svga->lc = svga->ld = 0xff;
        } else {
                latch_addr &= svga->vram_mask;
                svga->la = svga->vram[latch_addr];
                svga->lb = svga->vram[latch_addr | 0x1];
                svga->lc = svga->vram[latch_addr | 0x2];
                svga->ld = svga->vram[latch_addr | 0x3];
        }

        if (addr >= svga->vram_max)
                return 0xff;

        addr &= svga->vram_mask;

        if (svga->readmode) {
                temp = svga->la;
                temp ^= (svga->colourcompare & 1) ? 0xff : 0;
                temp &= (svga->colournocare & 1) ? 0xff : 0;
                temp2 = svga->lb;
                temp2 ^= (svga->colourcompare & 2) ? 0xff : 0;
                temp2 &= (svga->colournocare & 2) ? 0xff : 0;
                temp3 = svga->lc;
                temp3 ^= (svga->colourcompare & 4) ? 0xff : 0;
                temp3 &= (svga->colournocare & 4) ? 0xff : 0;
                temp4 = svga->ld;
                temp4 ^= (svga->colourcompare & 8) ? 0xff : 0;
                temp4 &= (svga->colournocare & 8) ? 0xff : 0;
                return ~(temp | temp2 | temp3 | temp4);
        }
        // pclog("Read %02X %04X %04X\n",vram[addr|svga->readplane],addr,svga->readplane);
        return svga->vram[addr | readplane];
}

void svga_write_linear(uint32_t addr, uint8_t val, void *p) {
        svga_t *svga = (svga_t *)p;
        uint8_t vala, valb, valc, vald, wm = svga->writemask;
        int writemask2 = svga->writemask;

        cycles -= video_timing_write_b;
        cycles_lost += video_timing_write_b;

        egawrites++;

        if (svga_output)
                pclog("Write LFB %08X %02X ", addr, val);
        if (!(svga->gdcreg[6] & 1))
                svga->fullchange = 2;
        if ((svga->chain4 && svga->packed_chain4) || svga->fb_only) {
                writemask2 = 1 << (addr & 3);
                addr &= ~3;
        } else if (svga->chain4) {
                writemask2 = 1 << (addr & 3);
                addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
        } else if (svga->chain2_write) {
                writemask2 &= ~0xa;
                if (addr & 1)
                        writemask2 <<= 1;
                addr &= ~1;
                addr <<= 2;
        } else {
                addr <<= 2;
        }
        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        if (svga_output)
                pclog("%08X\n", addr);
        svga->changedvram[addr >> 12] = changeframecount;

        switch (svga->writemode) {
        case 1:
                if (writemask2 & 1)
                        svga->vram[addr] = svga->la;
                if (writemask2 & 2)
                        svga->vram[addr | 0x1] = svga->lb;
                if (writemask2 & 4)
                        svga->vram[addr | 0x2] = svga->lc;
                if (writemask2 & 8)
                        svga->vram[addr | 0x3] = svga->ld;
                break;
        case 0:
                if (svga->gdcreg[3] & 7)
                        val = svga_rotate[svga->gdcreg[3] & 7][val];
                if (svga->gdcreg[8] == 0xff && !(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
                        if (writemask2 & 1)
                                svga->vram[addr] = val;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = val;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = val;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = val;
                } else {
                        if (svga->gdcreg[1] & 1)
                                vala = (svga->gdcreg[0] & 1) ? 0xff : 0;
                        else
                                vala = val;
                        if (svga->gdcreg[1] & 2)
                                valb = (svga->gdcreg[0] & 2) ? 0xff : 0;
                        else
                                valb = val;
                        if (svga->gdcreg[1] & 4)
                                valc = (svga->gdcreg[0] & 4) ? 0xff : 0;
                        else
                                valc = val;
                        if (svga->gdcreg[1] & 8)
                                vald = (svga->gdcreg[0] & 8) ? 0xff : 0;
                        else
                                vald = val;

                        switch (svga->gdcreg[3] & 0x18) {
                        case 0: /*Set*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                                break;
                        case 8: /*AND*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala | ~svga->gdcreg[8]) & svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->ld;
                                break;
                        case 0x10: /*OR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->ld;
                                break;
                        case 0x18: /*XOR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) ^ svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->ld;
                                break;
                        }
                        //                        pclog("- %02X %02X %02X %02X
                        //                        %08X\n",vram[addr],vram[addr|0x1],vram[addr|0x2],vram[addr|0x3],addr);
                }
                break;
        case 2:
                if (!(svga->gdcreg[3] & 0x18) && (!svga->gdcreg[1] || svga->set_reset_disabled)) {
                        if (writemask2 & 1)
                                svga->vram[addr] = (((val & 1) ? 0xff : 0) & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] =
                                        (((val & 2) ? 0xff : 0) & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] =
                                        (((val & 4) ? 0xff : 0) & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] =
                                        (((val & 8) ? 0xff : 0) & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                } else {
                        vala = ((val & 1) ? 0xff : 0);
                        valb = ((val & 2) ? 0xff : 0);
                        valc = ((val & 4) ? 0xff : 0);
                        vald = ((val & 8) ? 0xff : 0);
                        switch (svga->gdcreg[3] & 0x18) {
                        case 0: /*Set*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                                break;
                        case 8: /*AND*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala | ~svga->gdcreg[8]) & svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->ld;
                                break;
                        case 0x10: /*OR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) | svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->ld;
                                break;
                        case 0x18: /*XOR*/
                                if (writemask2 & 1)
                                        svga->vram[addr] = (vala & svga->gdcreg[8]) ^ svga->la;
                                if (writemask2 & 2)
                                        svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->lb;
                                if (writemask2 & 4)
                                        svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->lc;
                                if (writemask2 & 8)
                                        svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->ld;
                                break;
                        }
                }
                break;
        case 3:
                if (svga->gdcreg[3] & 7)
                        val = svga_rotate[svga->gdcreg[3] & 7][val];
                wm = svga->gdcreg[8];
                svga->gdcreg[8] &= val;

                vala = (svga->gdcreg[0] & 1) ? 0xff : 0;
                valb = (svga->gdcreg[0] & 2) ? 0xff : 0;
                valc = (svga->gdcreg[0] & 4) ? 0xff : 0;
                vald = (svga->gdcreg[0] & 8) ? 0xff : 0;
                switch (svga->gdcreg[3] & 0x18) {
                case 0: /*Set*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala & svga->gdcreg[8]) | (svga->la & ~svga->gdcreg[8]);
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | (svga->lb & ~svga->gdcreg[8]);
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | (svga->lc & ~svga->gdcreg[8]);
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | (svga->ld & ~svga->gdcreg[8]);
                        break;
                case 8: /*AND*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala | ~svga->gdcreg[8]) & svga->la;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb | ~svga->gdcreg[8]) & svga->lb;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc | ~svga->gdcreg[8]) & svga->lc;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald | ~svga->gdcreg[8]) & svga->ld;
                        break;
                case 0x10: /*OR*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala & svga->gdcreg[8]) | svga->la;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) | svga->lb;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) | svga->lc;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) | svga->ld;
                        break;
                case 0x18: /*XOR*/
                        if (writemask2 & 1)
                                svga->vram[addr] = (vala & svga->gdcreg[8]) ^ svga->la;
                        if (writemask2 & 2)
                                svga->vram[addr | 0x1] = (valb & svga->gdcreg[8]) ^ svga->lb;
                        if (writemask2 & 4)
                                svga->vram[addr | 0x2] = (valc & svga->gdcreg[8]) ^ svga->lc;
                        if (writemask2 & 8)
                                svga->vram[addr | 0x3] = (vald & svga->gdcreg[8]) ^ svga->ld;
                        break;
                }
                svga->gdcreg[8] = wm;
                break;
        }
}

uint8_t svga_read_linear(uint32_t addr, void *p) {
        svga_t *svga = (svga_t *)p;
        uint8_t temp, temp2, temp3, temp4;
        int readplane = svga->readplane;
        uint32_t latch_addr = (addr << 2) & svga->decode_mask;

        cycles -= video_timing_read_b;
        cycles_lost += video_timing_read_b;

        egareads++;

        if ((svga->chain4 && svga->packed_chain4) || svga->fb_only) {
                addr &= svga->decode_mask;
                if (addr >= svga->vram_max)
                        return 0xff;
                return svga->vram[addr & svga->vram_mask];
        } else if (svga->chain4) {
                readplane = addr & 3;
                addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
        } else if (svga->chain2_read) {
                readplane = (readplane & 2) | (addr & 1);
                addr &= ~1;
                addr <<= 2;
        } else {
                addr <<= 2;
        }

        addr &= svga->decode_mask;

        if (latch_addr >= svga->vram_max) {
                svga->la = svga->lb = svga->lc = svga->ld = 0xff;
        } else {
                latch_addr &= svga->vram_mask;
                svga->la = svga->vram[latch_addr];
                svga->lb = svga->vram[latch_addr | 0x1];
                svga->lc = svga->vram[latch_addr | 0x2];
                svga->ld = svga->vram[latch_addr | 0x3];
        }

        if (addr >= svga->vram_max)
                return 0xff;

        addr &= svga->vram_mask;

        if (svga->readmode) {
                temp = svga->la;
                temp ^= (svga->colourcompare & 1) ? 0xff : 0;
                temp &= (svga->colournocare & 1) ? 0xff : 0;
                temp2 = svga->lb;
                temp2 ^= (svga->colourcompare & 2) ? 0xff : 0;
                temp2 &= (svga->colournocare & 2) ? 0xff : 0;
                temp3 = svga->lc;
                temp3 ^= (svga->colourcompare & 4) ? 0xff : 0;
                temp3 &= (svga->colournocare & 4) ? 0xff : 0;
                temp4 = svga->ld;
                temp4 ^= (svga->colourcompare & 8) ? 0xff : 0;
                temp4 &= (svga->colournocare & 8) ? 0xff : 0;
                return ~(temp | temp2 | temp3 | temp4);
        }
        // printf("Read %02X %04X %04X\n",vram[addr|svga->readplane],addr,svga->readplane);
        return svga->vram[addr | readplane];
}

void svga_doblit(int y1, int y2, int wx, int wy, svga_t *svga) {
        //        pclog("svga_doblit start\n");
        svga->frames++;
        //        pclog("doblit %i %i\n", y1, y2);
        //        pclog("svga_doblit %i %i\n", wx, svga->hdisp);
        if (y1 > y2) {
                video_blit_memtoscreen(32, 0, 0, 0, xsize, ysize);
                return;
        }

        if ((wx != xsize || wy != ysize) && !vid_resize) {
                xsize = wx;
                ysize = wy + 1;
                if (xsize < 64)
                        xsize = 656;
                if (ysize < 32)
                        ysize = 200;

                if (svga->vertical_linedbl)
                        updatewindowsize(xsize, ysize * 2);
                else
                        updatewindowsize(xsize, ysize);
        }
        if (vid_resize) {
                xsize = wx;
                ysize = wy + 1;
        }
        video_blit_memtoscreen(32, 0, y1, y2, xsize, ysize);
        //        pclog("svga_doblit end\n");
}

void svga_writew(uint32_t addr, uint16_t val, void *p) {
        svga_t *svga = (svga_t *)p;
        if (!svga->fast) {
                svga_write(addr, val, p);
                svga_write(addr + 1, val >> 8, p);
                return;
        }

        egawrites += 2;

        cycles -= video_timing_write_w;
        cycles_lost += video_timing_write_w;

        if (svga_output)
                pclog("svga_writew: %05X ", addr);
        addr = (addr & svga->banked_mask) + svga->write_bank;
        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        if (svga_output)
                pclog("%08X (%i, %i) %04X\n", addr, addr & 1023, addr >> 10, val);
        svga->changedvram[addr >> 12] = changeframecount;
        *(uint16_t *)&svga->vram[addr] = val;
}

void svga_writel(uint32_t addr, uint32_t val, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast) {
                svga_write(addr, val, p);
                svga_write(addr + 1, val >> 8, p);
                svga_write(addr + 2, val >> 16, p);
                svga_write(addr + 3, val >> 24, p);
                return;
        }

        egawrites += 4;

        cycles -= video_timing_write_l;
        cycles_lost += video_timing_write_l;

        if (svga_output)
                pclog("svga_writel: %05X ", addr);
        addr = (addr & svga->banked_mask) + svga->write_bank;
        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        if (svga_output)
                pclog("%08X (%i, %i) %08X\n", addr, addr & 1023, addr >> 10, val);

        svga->changedvram[addr >> 12] = changeframecount;
        *(uint32_t *)&svga->vram[addr] = val;
}

uint16_t svga_readw(uint32_t addr, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast)
                return svga_read(addr, p) | (svga_read(addr + 1, p) << 8);

        egareads += 2;

        cycles -= video_timing_read_w;
        cycles_lost += video_timing_read_w;

        //        pclog("Readw %05X ", addr);
        addr = (addr & svga->banked_mask) + svga->read_bank;
        addr &= svga->decode_mask;
        //        pclog("%08X %04X\n", addr, *(uint16_t *)&vram[addr]);
        if (addr >= svga->vram_max)
                return 0xffff;

        return *(uint16_t *)&svga->vram[addr & svga->vram_mask];
}

uint32_t svga_readl(uint32_t addr, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast)
                return svga_read(addr, p) | (svga_read(addr + 1, p) << 8) | (svga_read(addr + 2, p) << 16) |
                       (svga_read(addr + 3, p) << 24);

        egareads += 4;

        cycles -= video_timing_read_l;
        cycles_lost += video_timing_read_l;

        //        pclog("Readl %05X ", addr);
        addr = (addr & svga->banked_mask) + svga->read_bank;
        addr &= svga->decode_mask;
        //        pclog("%08X %08X\n", addr, *(uint32_t *)&vram[addr]);
        if (addr >= svga->vram_max)
                return 0xffffffff;

        return *(uint32_t *)&svga->vram[addr & svga->vram_mask];
}

void svga_writew_linear(uint32_t addr, uint16_t val, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast) {
                svga_write_linear(addr, val, p);
                svga_write_linear(addr + 1, val >> 8, p);
                return;
        }

        egawrites += 2;

        cycles -= video_timing_write_w;
        cycles_lost += video_timing_write_w;

        if (svga_output)
                pclog("Write LFBw %08X %04X\n", addr, val);
        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        svga->changedvram[addr >> 12] = changeframecount;
        *(uint16_t *)&svga->vram[addr] = val;
}

void svga_writel_linear(uint32_t addr, uint32_t val, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast) {
                svga_write_linear(addr, val, p);
                svga_write_linear(addr + 1, val >> 8, p);
                svga_write_linear(addr + 2, val >> 16, p);
                svga_write_linear(addr + 3, val >> 24, p);
                return;
        }

        egawrites += 4;

        cycles -= video_timing_write_l;
        cycles_lost += video_timing_write_l;

        if (svga_output)
                pclog("Write LFBl %08X %08X\n", addr, val);
        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return;
        addr &= svga->vram_mask;
        svga->changedvram[addr >> 12] = changeframecount;
        *(uint32_t *)&svga->vram[addr] = val;
}

uint16_t svga_readw_linear(uint32_t addr, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast)
                return svga_read_linear(addr, p) | (svga_read_linear(addr + 1, p) << 8);

        egareads += 2;

        cycles -= video_timing_read_w;
        cycles_lost += video_timing_read_w;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return 0xffff;

        return *(uint16_t *)&svga->vram[addr & svga->vram_mask];
}

uint32_t svga_readl_linear(uint32_t addr, void *p) {
        svga_t *svga = (svga_t *)p;

        if (!svga->fast)
                return svga_read_linear(addr, p) | (svga_read_linear(addr + 1, p) << 8) | (svga_read_linear(addr + 2, p) << 16) |
                       (svga_read_linear(addr + 3, p) << 24);

        egareads += 4;

        cycles -= video_timing_read_l;
        cycles_lost += video_timing_read_l;

        addr &= svga->decode_mask;
        if (addr >= svga->vram_max)
                return 0xffffffff;

        return *(uint32_t *)&svga->vram[addr & svga->vram_mask];
}

void svga_add_status_info(char *s, int max_len, void *p) {
        svga_t *svga = (svga_t *)p;
        char temps[128];

        if (svga->chain4)
                strcpy(temps, "SVGA chained (possibly mode 13h)\n");
        else
                strcpy(temps, "SVGA unchained (possibly mode-X)\n");
        strncat(s, temps, max_len);

        if (!svga->video_bpp)
                strcpy(temps, "SVGA in text mode\n");
        else
                sprintf(temps, "SVGA colour depth : %i bpp\n", svga->video_bpp);
        strncat(s, temps, max_len);

        sprintf(temps, "SVGA resolution : %i x %i\n", svga->video_res_x, svga->video_res_y);
        strncat(s, temps, max_len);

        sprintf(temps, "SVGA refresh rate : %i Hz\n\n", svga->frames);
        svga->frames = 0;
        strncat(s, temps, max_len);
}
