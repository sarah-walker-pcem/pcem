/*ET4000 emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_svga_render.h"
#include "vid_unk_ramdac.h"

#include "vid_et4000.h"

typedef struct et4000_t
{
        svga_t svga;
        unk_ramdac_t ramdac;
        
        rom_t bios_rom;
        
        uint8_t banking;
        uint8_t port_22cb_val;
        uint8_t port_32cb_val;
        int get_korean_font_enabled;
        int get_korean_font_index;
        uint16_t get_korean_font_base;
} et4000_t;

static uint8_t crtc_mask[0x40] =
{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void et4000_out(uint16_t addr, uint8_t val, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        svga_t *svga = &et4000->svga;
                
        uint8_t old;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) 
                addr ^= 0x60;

//        pclog("ET4000 out %04X %02X\n", addr, val);

        switch (addr)
        {
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                unk_ramdac_out(addr, val, &et4000->ramdac, svga);
                return;
                
                case 0x3CD: /*Banking*/
                svga->write_bank = (val & 0xf) * 0x10000;
                svga->read_bank = ((val >> 4) & 0xf) * 0x10000;
                et4000->banking = val;
//                pclog("Banking write %08X %08X %02X\n", svga->write_bank, svga->read_bank, val);
                return;
                case 0x3D4:
                svga->crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                        return;
                if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                old = svga->crtc[svga->crtcreg];
                val &= crtc_mask[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                if (old != val)
                {
                        if (svga->crtcreg < 0xE || svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;
        }
        svga_out(addr, val, svga);
}

void et4000k_out(uint16_t addr, uint8_t val, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
                
//        pclog("ET4000k out %04X %02X\n", addr, val);

        switch (addr)
        {
                case 0x22CB:
                et4000->port_22cb_val = (et4000->port_22cb_val & 0xF0) | (val & 0x0F);
                et4000->get_korean_font_enabled = val & 7;
                if (et4000->get_korean_font_enabled == 3)
                        et4000->get_korean_font_index = 0;
                break;
                case 0x22CF:
                switch (et4000->get_korean_font_enabled)
                {
                        case 1:
                        et4000->get_korean_font_base = ((val & 0x7F) << 7) | (et4000->get_korean_font_base & 0x7F);
                        break;
                        case 2:
                        et4000->get_korean_font_base = (et4000->get_korean_font_base & 0x3F80) | (val & 0x7F) | (((val ^ 0x80) & 0x80) << 8);
                        break;
                        case 3:
                        if ((et4000->port_32cb_val & 0x30) == 0x20 && (et4000->get_korean_font_base & 0x7F) > 0x20 && (et4000->get_korean_font_base & 0x7F) < 0x7F)
                        {
                                switch (et4000->get_korean_font_base & 0x3F80)
                                {
                                        case 0x2480:
                                        if (et4000->get_korean_font_index < 16)
                                                fontdatksc5601_user[(et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index] = val;
                                        else if (et4000->get_korean_font_index >= 24 && et4000->get_korean_font_index < 40)
                                                fontdatksc5601_user[(et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index - 8] = val;
                                        break;
                                        case 0x3F00:
                                        if (et4000->get_korean_font_index < 16)
                                                fontdatksc5601_user[96 + (et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index] = val;
                                        else if (et4000->get_korean_font_index >= 24 && et4000->get_korean_font_index < 40)
                                                fontdatksc5601_user[96 + (et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index - 8] = val;
                                        break;
                                }
                                et4000->get_korean_font_index++;
                        }
                        break;
                }
                break;
                case 0x32CB:
                et4000->port_32cb_val = val;
                svga_recalctimings(&et4000->svga);
                break;
        }
}

uint8_t et4000_in(uint16_t addr, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        svga_t *svga = &et4000->svga;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout & 1)) 
                addr ^= 0x60;
        
//        if (addr != 0x3da) pclog("IN ET4000 %04X\n", addr);
        
        switch (addr)
        {
                case 0x3C5:
                if ((svga->seqaddr & 0xf) == 7) return svga->seqregs[svga->seqaddr & 0xf] | 4;
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                return unk_ramdac_in(addr, &et4000->ramdac, svga);
                
                case 0x3CD: /*Banking*/
                return et4000->banking;
                case 0x3D4:
                return svga->crtcreg;
                case 0x3D5:
                return svga->crtc[svga->crtcreg];
        }
        return svga_in(addr, svga);
}

uint8_t et4000k_in(uint16_t addr, void *p)
{
        uint8_t val = 0xFF;
        et4000_t *et4000 = (et4000_t *)p;
        
//        if (addr != 0x3da) pclog("IN ET4000 %04X\n", addr);
        
        switch (addr)
        {
                case 0x22CB:
                return et4000->port_22cb_val;
                case 0x22CF:
                val = 0;
                switch (et4000->get_korean_font_enabled)
                {
                        case 3:
                        if ((et4000->port_32cb_val & 0x30) == 0x30)
                        {
                                val = fontdatksc5601[et4000->get_korean_font_base][et4000->get_korean_font_index++];
                                et4000->get_korean_font_index &= 0x1F;
                        }
                        else if ((et4000->port_32cb_val & 0x30) == 0x20 && (et4000->get_korean_font_base & 0x7F) > 0x20 && (et4000->get_korean_font_base & 0x7F) < 0x7F)
                        {
                                switch (et4000->get_korean_font_base & 0x3F80)
                                {
                                        case 0x2480:
                                        if (et4000->get_korean_font_index < 16)
                                                val = fontdatksc5601_user[(et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index];
                                        else if (et4000->get_korean_font_index >= 24 && et4000->get_korean_font_index < 40)
                                                val = fontdatksc5601_user[(et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index - 8];
                                        break;
                                        case 0x3F00:
                                        if (et4000->get_korean_font_index < 16)
                                                val = fontdatksc5601_user[96 + (et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index];
                                        else if (et4000->get_korean_font_index >= 24 && et4000->get_korean_font_index < 40)
                                                val = fontdatksc5601_user[96 + (et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index - 8];
                                        break;
                                }
                                et4000->get_korean_font_index++;
                                et4000->get_korean_font_index %= 72;
                        }
                        break;
                        case 4:
                        val = 0x0F;
                        break;
                }
                return val;
                case 0x32CB:
                return et4000->port_32cb_val;
        }
        
        return 0xff;
}

void et4000_recalctimings(svga_t *svga)
{
        svga->ma_latch |= (svga->crtc[0x33]&3)<<16;
        if (svga->crtc[0x35] & 1)    svga->vblankstart += 0x400;
        if (svga->crtc[0x35] & 2)    svga->vtotal += 0x400;
        if (svga->crtc[0x35] & 4)    svga->dispend += 0x400;
        if (svga->crtc[0x35] & 8)    svga->vsyncstart += 0x400;
        if (svga->crtc[0x35] & 0x10) svga->split += 0x400;
        if (!svga->rowoffset)        svga->rowoffset = 0x100;
        if (svga->crtc[0x3f] & 1)    svga->htotal += 256;
        if (svga->attrregs[0x16] & 0x20) svga->hdisp <<= 1;

//        pclog("Rowoffset %i\n",svga_rowoffset);

        switch (((svga->miscout >> 2) & 3) | ((svga->crtc[0x34] << 1) & 4))
        {
                case 0: case 1: break;
                case 3: svga->clock = (cpuclock * (double)(1ull << 32)) / 40000000.0; break;
                case 5: svga->clock = (cpuclock * (double)(1ull << 32)) / 65000000.0; break;
                default: svga->clock = (cpuclock * (double)(1ull << 32)) / 36000000.0; break;
        }
        
        switch (svga->bpp)
        {
                case 15: case 16:
                svga->hdisp /= 2;
                break;
                case 24:
                svga->hdisp /= 3;
                break;
        }
}

void et4000k_recalctimings(svga_t *svga)
{
        et4000_t *et4000 = (et4000_t *)svga->p;

        et4000_recalctimings(svga);

        if (svga->render == svga_render_text_80 && ((svga->crtc[0x37] & 0x0A) == 0x0A))
        {
                if((et4000->port_32cb_val & 0xB4) == ((svga->crtc[0x37] & 3) == 2 ? 0xB4 : 0xB0))
                {
                        svga->render = svga_render_text_80_ksc5601;
                }
        }
}

void *et4000_init()
{
        et4000_t *et4000 = malloc(sizeof(et4000_t));
        memset(et4000, 0, sizeof(et4000_t));

        rom_init(&et4000->bios_rom, "et4000.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
                
        io_sethandler(0x03c0, 0x0020, et4000_in, NULL, NULL, et4000_out, NULL, NULL, et4000);

        svga_init(&et4000->svga, et4000, 1 << 20, /*1mb*/
                   et4000_recalctimings,
                   et4000_in, et4000_out,
                   NULL,
                   NULL);
        
        return et4000;
}

void *et4000k_init()
{
        et4000_t *et4000 = malloc(sizeof(et4000_t));
        memset(et4000, 0, sizeof(et4000_t));

        rom_init(&et4000->bios_rom, "tgkorvga.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        loadfont("tg_ksc5601.rom", 6);
                
        io_sethandler(0x03c0, 0x0020, et4000_in, NULL, NULL, et4000_out, NULL, NULL, et4000);

        io_sethandler(0x22cb, 0x0001, et4000k_in, NULL, NULL, et4000k_out, NULL, NULL, et4000);
        io_sethandler(0x22cf, 0x0001, et4000k_in, NULL, NULL, et4000k_out, NULL, NULL, et4000);
        io_sethandler(0x32cb, 0x0001, et4000k_in, NULL, NULL, et4000k_out, NULL, NULL, et4000);
        et4000->port_22cb_val = 0x60;
        et4000->port_32cb_val = 0;

        svga_init(&et4000->svga, et4000, 1 << 20, /*1mb*/
                   et4000k_recalctimings,
                   et4000k_in, et4000k_out,
                   NULL,
                   NULL);

        et4000->svga.ksc5601_sbyte_mask = 0x80;

        return et4000;
}

static int et4000_available()
{
        return rom_present("et4000.bin");
}

static int et4000k_available()
{
        return rom_present("tgkorvga.bin") && rom_present("tg_ksc5601.rom");;
}

void et4000_close(void *p)
{
        et4000_t *et4000 = (et4000_t *)p;

        svga_close(&et4000->svga);
        
        free(et4000);
}

void et4000_speed_changed(void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        
        svga_recalctimings(&et4000->svga);
}

void et4000_force_redraw(void *p)
{
        et4000_t *et4000 = (et4000_t *)p;

        et4000->svga.fullchange = changeframecount;
}

void et4000_add_status_info(char *s, int max_len, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
        
        svga_add_status_info(s, max_len, &et4000->svga);
}

device_t et4000_device =
{
        "Tseng Labs ET4000AX",
        0,
        et4000_init,
        et4000_close,
        et4000_available,
        et4000_speed_changed,
        et4000_force_redraw,
        et4000_add_status_info
};

device_t et4000k_device =
{
        "Trigem Korean VGA(Tseng Labs ET4000AX)",
        0,
        et4000k_init,
        et4000_close,
        et4000k_available,
        et4000_speed_changed,
        et4000_force_redraw,
        et4000_add_status_info
};
