/*ATI 28800 emulation (VGA Charger & Korean VGA)*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "rom.h"
#include "video.h"
#include "vid_ati28800.h"
#include "vid_ati_eeprom.h"
#include "vid_svga.h"
#include "vid_svga_render.h"

typedef struct ati28800_t
{
        svga_t svga;
        ati_eeprom_t eeprom;
        
        rom_t bios_rom;
        
        uint8_t regs[256];
        int index;

        uint8_t port_03dd_val;
        uint16_t get_korean_font_kind;
        int in_get_korean_font_kind_set;
        int get_korean_font_enabled;
        int get_korean_font_index;
        uint16_t get_korean_font_base;
        int ksc5601_mode_enabled;
} ati28800_t;

void ati28800_out(uint16_t addr, uint8_t val, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        svga_t *svga = &ati28800->svga;
        uint8_t old;
        
//        pclog("ati28800_out : %04X %02X  %04X:%04X\n", addr, val, CS,pc);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) 
                addr ^= 0x60;

        switch (addr)
        {
                case 0x1ce:
                ati28800->index = val;
                break;
                case 0x1cf:
                old = ati28800->regs[ati28800->index];
                ati28800->regs[ati28800->index] = val;
                switch (ati28800->index)
                {
                        case 0xb2:
                        case 0xbe:
                        if (ati28800->regs[0xbe] & 8) /*Read/write bank mode*/
                        {
                                svga->read_bank  = ((ati28800->regs[0xb2] >> 5) & 7) * 0x10000;
                                svga->write_bank = ((ati28800->regs[0xb2] >> 1) & 7) * 0x10000;
                        }
                        else                    /*Single bank mode*/
                                svga->read_bank = svga->write_bank = ((ati28800->regs[0xb2] >> 1) & 7) * 0x10000;
                        if (ati28800->index == 0xbe && ((old ^ val) & 0x10))
                                svga_recalctimings(svga);
                        break;
                        case 0xb3:
                        ati_eeprom_write(&ati28800->eeprom, val & 8, val & 2, val & 1);
                        break;
                        case 0xb6:
                        if ((old ^ val) & 0x10)
                                svga_recalctimings(svga);
                        break;
                        case 0xb8:
                        if ((old ^ val) & 0x40)
                                svga_recalctimings(svga);
                        break;
                        case 0xb9:
                        if ((old ^ val) & 2)
                                svga_recalctimings(svga);
                        break;
                }
                break;
                
                case 0x3D4:
                svga->crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                        return;
                if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                        val = (svga->crtc[7] & ~0x10) | (val & 0x10);
                old = svga->crtc[svga->crtcreg];
                svga->crtc[svga->crtcreg] = val;
                if (old != val)
                {
                        if (svga->crtcreg < 0xe || svga->crtcreg > 0x10)
                        {
                                svga->fullchange = changeframecount;
                                svga_recalctimings(svga);
                        }
                }
                break;
        }
        svga_out(addr, val, svga);
}

void ati28800k_out(uint16_t addr, uint8_t val, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        svga_t *svga = &ati28800->svga;
        uint16_t oldaddr = addr;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) 
                addr ^= 0x60;

        switch (addr)
        {
                case 0x1CF:
                if (ati28800->index == 0xBF && ((ati28800->regs[0xBF] ^ val) & 0x20))
                {
                        ati28800->ksc5601_mode_enabled = val & 0x20;
//                        pclog("Switching mode to %s at %04X:%04X\n", ksc5601_mode_enabled ? "DBCS" : "SBCS", CS, cpu_state.oldpc);
                        svga_recalctimings(svga);
                }
                ati28800_out(oldaddr, val, p);
                break;
                case 0x3DD:
//                pclog("ati28800k_out : set port 03DD to %02X at %04X:%04X\n", val, CS, cpu_state.oldpc);
                ati28800->port_03dd_val = val;
                if (val == 1)
                        ati28800->get_korean_font_enabled = 0;
                if (ati28800->in_get_korean_font_kind_set)
                {
                        ati28800->get_korean_font_kind = (val << 8) | (ati28800->get_korean_font_kind & 0xFF);
                        ati28800->get_korean_font_enabled = 1;
                        ati28800->get_korean_font_index = 0;
                        ati28800->in_get_korean_font_kind_set = 0;
                }
                break;
                case 0x3DE:
//                pclog("ati28800k_out : set port 03DE to %02X at %04X:%04X\n", val, CS, cpu_state.oldpc);
                ati28800->in_get_korean_font_kind_set = 0;
                if(ati28800->get_korean_font_enabled)
                {
                        if((ati28800->get_korean_font_base & 0x7F) > 0x20 && (ati28800->get_korean_font_base & 0x7F) < 0x7F)
                                fontdatksc5601_user[(ati28800->get_korean_font_kind & 4) * 24 + (ati28800->get_korean_font_base & 0x7F) - 0x20][ati28800->get_korean_font_index] = val;
                        ati28800->get_korean_font_index++;
                        ati28800->get_korean_font_index &= 0x1F;
                }
                else
                {
                        switch (ati28800->port_03dd_val)
                        {
                                case 0x10:
                                ati28800->get_korean_font_base = ((val & 0x7F) << 7) | (ati28800->get_korean_font_base & 0x7F);
                                break;
                                case 8:
                                ati28800->get_korean_font_base = (ati28800->get_korean_font_base & 0x3F80) | (val & 0x7F);
                                break;
                                case 1:
                                ati28800->get_korean_font_kind = (ati28800->get_korean_font_kind & 0xFF00) | val;
                                if (val & 2)
                                        ati28800->in_get_korean_font_kind_set = 1;
                                break;
                                default:
                                break;
                        }
                }
                break;
                default:
                ati28800_out(oldaddr, val, p);
                break;
        }
}

uint8_t ati28800_in(uint16_t addr, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        svga_t *svga = &ati28800->svga;
        uint8_t temp;

//        if (addr != 0x3da) pclog("ati28800_in : %04X ", addr);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) addr ^= 0x60;
             
        switch (addr)
        {
                case 0x1ce:
                temp = ati28800->index;
                break;
                case 0x1cf:
                switch (ati28800->index)
                {
                        case 0xb7:
                        temp = ati28800->regs[ati28800->index] & ~8;
                        if (ati_eeprom_read(&ati28800->eeprom))
                                temp |= 8;
                        break;
                        
                        default:
                        temp = ati28800->regs[ati28800->index];
                        break;
                }
                break;

                case 0x3c2:
                if ((svga->vgapal[0].r + svga->vgapal[0].g + svga->vgapal[0].b) >= 0x50)
                        temp = 0;
                else
                        temp = 0x10;
                break;
                case 0x3D4:
                temp = svga->crtcreg;
                break;
                case 0x3D5:
                temp = svga->crtc[svga->crtcreg];
                break;
                default:
                temp = svga_in(addr, svga);
                break;
        }
//        if (addr != 0x3da) pclog("%02X  %04X:%04X\n", temp, CS,cpu_state.pc);
        return temp;
}

uint8_t ati28800k_in(uint16_t addr, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        svga_t *svga = &ati28800->svga;
        uint16_t oldaddr = addr;
        uint8_t temp = 0xFF;

//        if (addr != 0x3da) pclog("ati28800_in : %04X ", addr);
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga->miscout&1)) addr ^= 0x60;
             
        switch (addr)
        {
                case 0x3DE:
                if (ati28800->get_korean_font_enabled)
                {
                        switch (ati28800->get_korean_font_kind >> 8)
                        {
                                case 4: /* ROM font */
                                temp = fontdatksc5601[ati28800->get_korean_font_base][ati28800->get_korean_font_index++];
                                break;
                                case 2: /* User defined font */
                                if((ati28800->get_korean_font_base & 0x7F) > 0x20 && (ati28800->get_korean_font_base & 0x7F) < 0x7F)
                                        temp = fontdatksc5601_user[(ati28800->get_korean_font_kind & 4) * 24 + (ati28800->get_korean_font_base & 0x7F) - 0x20][ati28800->get_korean_font_index];
                                else
                                        temp = 0xFF;
                                ati28800->get_korean_font_index++;
                                break;
                                default:
                                break;
                        }
                        ati28800->get_korean_font_index &= 0x1F;
                }
                break;
                default:
                temp = ati28800_in(oldaddr, p);
                break;
        }
//        if (addr != 0x3da) pclog("%02X  %04X:%04X\n", temp, CS,cpu_state.pc);
        return temp;
}

void ati28800_recalctimings(svga_t *svga)
{
        ati28800_t *ati28800 = (ati28800_t *)svga->p;
        pclog("ati28800_recalctimings\n");

        switch (((ati28800->regs[0xbe] & 0x10) >> 1) | ((ati28800->regs[0xb9] & 2) << 1) | ((svga->miscout & 0x0C) >> 2))
        {
                case 0x00: svga->clock = (cpuclock * (double)(1ull << 32)) / 42954000.0; break;
                case 0x01: svga->clock = (cpuclock * (double)(1ull << 32)) / 48771000.0; break;
                case 0x03: svga->clock = (cpuclock * (double)(1ull << 32)) / 36000000.0; break;
                case 0x04: svga->clock = (cpuclock * (double)(1ull << 32)) / 50350000.0; break;
                case 0x05: svga->clock = (cpuclock * (double)(1ull << 32)) / 56640000.0; break;
                case 0x07: svga->clock = (cpuclock * (double)(1ull << 32)) / 44900000.0; break;
                case 0x08: svga->clock = (cpuclock * (double)(1ull << 32)) / 30240000.0; break;
                case 0x09: svga->clock = (cpuclock * (double)(1ull << 32)) / 32000000.0; break;
                case 0x0A: svga->clock = (cpuclock * (double)(1ull << 32)) / 37500000.0; break;
                case 0x0B: svga->clock = (cpuclock * (double)(1ull << 32)) / 39000000.0; break;
                case 0x0C: svga->clock = (cpuclock * (double)(1ull << 32)) / 40000000.0; break;
                case 0x0D: svga->clock = (cpuclock * (double)(1ull << 32)) / 56644000.0; break;
                case 0x0E: svga->clock = (cpuclock * (double)(1ull << 32)) / 75000000.0; break;
                case 0x0F: svga->clock = (cpuclock * (double)(1ull << 32)) / 65000000.0; break;
                default: break;
        }
        if (ati28800->regs[0xb8] & 0x40)
                svga->clock *= 2;
        if (ati28800->regs[0xb6] & 0x10)
        {
                svga->hdisp <<= 1;
                svga->htotal <<= 1;
                svga->rowoffset <<= 1;
        }

        if (!svga->scrblank && (ati28800->regs[0xb0] & 0x20)) /*Extended 256 colour modes*/
        {
                pclog("8bpp_highres\n");
                svga->render = svga_render_8bpp_highres;
                svga->rowoffset <<= 1;
                svga->ma <<= 1;
        }
}               

void ati28800k_recalctimings(svga_t *svga)
{
        ati28800_t *ati28800 = (ati28800_t *)svga->p;
        
        ati28800_recalctimings(svga);

        if (svga->render == svga_render_text_80 && ati28800->ksc5601_mode_enabled)
        {
                svga->render = svga_render_text_80_ksc5601;
        }
}               

void *ati28800_init()
{
        ati28800_t *ati28800 = malloc(sizeof(ati28800_t));
        memset(ati28800, 0, sizeof(ati28800_t));
        
        rom_init(&ati28800->bios_rom, "bios.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        
        svga_init(&ati28800->svga, ati28800, 1 << 19, /*512kb*/
                   ati28800_recalctimings,
                   ati28800_in, ati28800_out,
                   NULL,
                   NULL);

        io_sethandler(0x01ce, 0x0002, ati28800_in, NULL, NULL, ati28800_out, NULL, NULL, ati28800);
        io_sethandler(0x03c0, 0x0020, ati28800_in, NULL, NULL, ati28800_out, NULL, NULL, ati28800);

        ati28800->svga.miscout = 1;

        ati_eeprom_load(&ati28800->eeprom, "ati28800.nvr", 0);

        return ati28800;
}

void *ati28800k_init()
{
        ati28800_t *ati28800 = malloc(sizeof(ati28800_t));
        memset(ati28800, 0, sizeof(ati28800_t));

        ati28800->port_03dd_val = 0;
        ati28800->get_korean_font_base = 0;
        ati28800->get_korean_font_index = 0;
        ati28800->get_korean_font_enabled = 0;
        ati28800->get_korean_font_kind = 0;
        ati28800->in_get_korean_font_kind_set = 0;
        ati28800->ksc5601_mode_enabled = 0;
        
        rom_init(&ati28800->bios_rom, "atikorvga.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        loadfont("ati_ksc5601.rom", FONT_KSC5601);
        
        svga_init(&ati28800->svga, ati28800, 1 << 19, /*512kb*/
                   ati28800k_recalctimings,
                   ati28800k_in, ati28800k_out,
                   NULL,
                   NULL);

        io_sethandler(0x01ce, 0x0002, ati28800k_in, NULL, NULL, ati28800k_out, NULL, NULL, ati28800);
        io_sethandler(0x03c0, 0x0020, ati28800k_in, NULL, NULL, ati28800k_out, NULL, NULL, ati28800);

        ati28800->svga.miscout = 1;
        ati28800->svga.ksc5601_sbyte_mask = 0;

        ati_eeprom_load(&ati28800->eeprom, "atikorvga.nvr", 0);

        return ati28800;
}

void *ati28800k_spc4620p_init()
{
        ati28800_t *ati28800 = malloc(sizeof(ati28800_t));
        memset(ati28800, 0, sizeof(ati28800_t));

        ati28800->port_03dd_val = 0;
        ati28800->get_korean_font_base = 0;
        ati28800->get_korean_font_index = 0;
        ati28800->get_korean_font_enabled = 0;
        ati28800->get_korean_font_kind = 0;
        ati28800->in_get_korean_font_kind_set = 0;
        ati28800->ksc5601_mode_enabled = 0;

        rom_init_interleaved(&ati28800->bios_rom, "spc4620p/31005h.u8", "spc4620p/31005h.u10", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        loadfont("spc4620p/svb6120a_font.rom", 6);
        
        svga_init(&ati28800->svga, ati28800, 1 << 19, /*512kb*/
                   ati28800k_recalctimings,
                   ati28800k_in, ati28800k_out,
                   NULL,
                   NULL);

        io_sethandler(0x01ce, 0x0002, ati28800k_in, NULL, NULL, ati28800k_out, NULL, NULL, ati28800);
        io_sethandler(0x03c0, 0x0020, ati28800k_in, NULL, NULL, ati28800k_out, NULL, NULL, ati28800);

        ati28800->svga.miscout = 1;
        ati28800->svga.ksc5601_sbyte_mask = 0;

        ati_eeprom_load(&ati28800->eeprom, "svb6120a.nvr", 0);

        return ati28800;
}

static int ati28800_available()
{
        return rom_present("bios.bin");
}

static int ati28800k_available()
{
        return rom_present("atikorvga.bin") && rom_present("ati_ksc5601.rom");
}

void ati28800_close(void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;

        svga_close(&ati28800->svga);
        
        free(ati28800);
}

void ati28800_speed_changed(void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        
        svga_recalctimings(&ati28800->svga);
}

void ati28800_force_redraw(void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;

        ati28800->svga.fullchange = changeframecount;
}

void ati28800_add_status_info(char *s, int max_len, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        
        svga_add_status_info(s, max_len, &ati28800->svga);
}

void ati28800k_add_status_info(char *s, int max_len, void *p)
{
        ati28800_t *ati28800 = (ati28800_t *)p;
        char temps[128];
        
        ati28800_add_status_info(s, max_len, p);

        sprintf(temps, "Korean SVGA mode enabled : %s\n\n", ati28800->ksc5601_mode_enabled ? "Yes" : "No");
        strncat(s, temps, max_len);
}

device_t ati28800_device =
{
        "ATI-28800",
        0,
        ati28800_init,
        ati28800_close,
        ati28800_available,
        ati28800_speed_changed,
        ati28800_force_redraw,
        ati28800_add_status_info
};

device_t ati28800k_device =
{
        "ATI Korean VGA",
        0,
        ati28800k_init,
        ati28800_close,
        ati28800k_available,
        ati28800_speed_changed,
        ati28800_force_redraw,
        ati28800k_add_status_info
};

device_t ati28800k_spc4620p_device =
{
        "SVB-6120A",
        0,
        ati28800k_spc4620p_init,
        ati28800_close,
        NULL,
        ati28800_speed_changed,
        ati28800_force_redraw,
        ati28800k_add_status_info
};
