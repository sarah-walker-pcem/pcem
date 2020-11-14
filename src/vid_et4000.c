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

        uint8_t kasan_cfg_index;
        uint8_t kasan_cfg_regs[16];
        uint16_t kasan_access_addr;
        uint8_t kasan_font_data[4];
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

void et4000_kasan_out(uint16_t addr, uint8_t val, void *p);
uint8_t et4000_kasan_in(uint16_t addr, void *p);

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
                default:
                et4000_out(addr, val, p);
                break;
        }
}

void et4000_kasan_out(uint16_t addr, uint8_t val, void *p)
{
        et4000_t *et4000 = (et4000_t *)p;
                
//        pclog("ET4000k out %04X %02X\n", addr, val);

        
        if(addr == 0x258)
        {
                //pclog("Write port %04X to %02X at %04X:%04X\n", addr, val, CS, cpu_state.oldpc);
                et4000->kasan_cfg_index = val;
        }
        else if(addr == 0x259)
        {
                //pclog("Write port %04X to %02X at %04X:%04X\n", addr, val, CS, cpu_state.oldpc);
                if(et4000->kasan_cfg_index >= 0xF0)
                {
                        switch(et4000->kasan_cfg_index - 0xF0)
                        {
                                case 0:
                                if(et4000->kasan_cfg_regs[4] & 8)
                                        val = (val & 0xFC) | (et4000->kasan_cfg_regs[0] & 3);
                                et4000->kasan_cfg_regs[0] = val;
                                svga_recalctimings(&et4000->svga);
                                break;
                                case 1:
                                case 2:
                                et4000->kasan_cfg_regs[et4000->kasan_cfg_index - 0xF0] = val;
                                io_removehandler(et4000->kasan_access_addr, 0x0008, et4000_kasan_in, NULL, NULL, et4000_kasan_out, NULL, NULL, et4000);
                                et4000->kasan_access_addr = (et4000->kasan_cfg_regs[2] << 8) | et4000->kasan_cfg_regs[1];
                                io_sethandler(et4000->kasan_access_addr, 0x0008, et4000_kasan_in, NULL, NULL, et4000_kasan_out, NULL, NULL, et4000);
                                //pclog("Set font access address to %04X\n", et4000->kasan_access_addr);
                                break;
                                case 4:
                                if(et4000->kasan_cfg_regs[0] & 0x20)
                                        val |= 0x80;
                                et4000->svga.ksc5601_swap_mode = (val & 4) >> 2;
                                et4000->kasan_cfg_regs[4] = val;
                                svga_recalctimings(&et4000->svga);
                                break;
                                case 5:
                                et4000->kasan_cfg_regs[5] = val;
                                et4000->svga.ksc5601_english_font_type = 0x100 | val;
                                case 6:
                                case 7:
                                et4000->svga.ksc5601_udc_area_msb[et4000->kasan_cfg_index - 0xF6] = val;
                                default:
                                et4000->kasan_cfg_regs[et4000->kasan_cfg_index - 0xF0] = val;
                                svga_recalctimings(&et4000->svga);
                                break;
                        }
                }
        }
        else if(addr >= et4000->kasan_access_addr && addr < et4000->kasan_access_addr + 8)
        {
                switch (addr - ((et4000->kasan_cfg_regs[2] << 8) | (et4000->kasan_cfg_regs[1])))
                {
                        case 0:
                        if(et4000->kasan_cfg_regs[0] & 2)
                        {
                                et4000->get_korean_font_index = ((val & 1) << 4) | ((val & 0x1E) >> 1);
                                et4000->get_korean_font_base = (et4000->get_korean_font_base & ~7) | (val >> 5);
                        }
                        break;
                        case 1:
                        if(et4000->kasan_cfg_regs[0] & 2)
                        {
                                et4000->get_korean_font_base = (et4000->get_korean_font_base & ~0x7F8) | (val << 3);
                        }
                        break;
                        case 2:
                        if(et4000->kasan_cfg_regs[0] & 2)
                        {
                                et4000->get_korean_font_base = (et4000->get_korean_font_base & ~0x7F800) | ((val & 7) << 11);
                        }
                        break;
                        case 3:
                        case 4:
                        case 5:
                        if(et4000->kasan_cfg_regs[0] & 1)
                        {
                                et4000->kasan_font_data[addr - (((et4000->kasan_cfg_regs[2] << 8) | (et4000->kasan_cfg_regs[1])) + 3)] = val;
                        }
                        break;
                        case 6:
                        if((et4000->kasan_cfg_regs[0] & 1) && (et4000->kasan_font_data[3] & !val & 0x80) && (et4000->get_korean_font_base & 0x7F) >= 0x20 && (et4000->get_korean_font_base & 0x7F) < 0x7F)
                        {
                                if(((et4000->get_korean_font_base >> 7) & 0x7F) == (et4000->svga.ksc5601_udc_area_msb[0] & 0x7F) && (et4000->svga.ksc5601_udc_area_msb[0] & 0x80))
                                {
                                        fontdatksc5601_user[(et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index] = et4000->kasan_font_data[2];
                                }
                                else if(((et4000->get_korean_font_base >> 7) & 0x7F) == (et4000->svga.ksc5601_udc_area_msb[1] & 0x7F) && (et4000->svga.ksc5601_udc_area_msb[1] & 0x80))
                                {
                                        fontdatksc5601_user[96 + (et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index] = et4000->kasan_font_data[2];
                                }
                        }
                        et4000->kasan_font_data[3] = val;
                        break;
                        default:
                        break;
                }
                //pclog("Write port %04X at %04X:%04X\n", addr, CS, cpu_state.oldpc);
        }
        else
        {
                et4000_out(addr, val, p);
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
        
        return et4000_in(addr, p);
}

uint8_t et4000_kasan_in(uint16_t addr, void *p)
{
        uint8_t val = 0xFF;
        et4000_t *et4000 = (et4000_t *)p;
        
        if(addr == 0x258)
        {
                //pclog("Read port %04X at %04X:%04X\n", addr, CS, cpu_state.oldpc);
                val = et4000->kasan_cfg_index;
        }
        else if(addr == 0x259)
        {
                //pclog("Read port %04X at %04X:%04X\n", addr, CS, cpu_state.oldpc);
                if (et4000->kasan_cfg_index >= 0xF0)
                {
                        val = et4000->kasan_cfg_regs[et4000->kasan_cfg_index - 0xF0];
                        if(et4000->kasan_cfg_index == 0xF4 && et4000->kasan_cfg_regs[0] & 0x20)
                                val |= 0x80;
                }
        }
        else if(addr >= et4000->kasan_access_addr && addr < et4000->kasan_access_addr + 8)
        {
                switch (addr - ((et4000->kasan_cfg_regs[2] << 8) | (et4000->kasan_cfg_regs[1])))
                {
                        case 2:
                        val = 0;
                        break;
                        case 5:
                        if(((et4000->get_korean_font_base >> 7) & 0x7F) == (et4000->svga.ksc5601_udc_area_msb[0] & 0x7F) && (et4000->svga.ksc5601_udc_area_msb[0] & 0x80))
                        {
                                val = fontdatksc5601_user[(et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index];
                        }
                        else if(((et4000->get_korean_font_base >> 7) & 0x7F) == (et4000->svga.ksc5601_udc_area_msb[1] & 0x7F) && (et4000->svga.ksc5601_udc_area_msb[1] & 0x80))
                        {
                                val = fontdatksc5601_user[96 + (et4000->get_korean_font_base & 0x7F) - 0x20][et4000->get_korean_font_index];
                        }
                        else
                        {
                                val = fontdatksc5601[et4000->get_korean_font_base][et4000->get_korean_font_index];
                        }
                        break;
                        default:
                        break;
                }
                //pclog(stderr, "Read port %04X at %04X:%04X\n", addr, CS, cpu_state.oldpc);
        }
        else
        {
                val = et4000_in(addr, p);
        }
        return val;
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
                if(et4000->port_32cb_val & 0x80)
                {
                        svga->ma_latch -= 2;
                        svga->ca_adj = -2;
                }
                if((et4000->port_32cb_val & 0xB4) == ((svga->crtc[0x37] & 3) == 2 ? 0xB4 : 0xB0))
                {
                        svga->render = svga_render_text_80_ksc5601;
                }
        }
}

void et4000_kasan_recalctimings(svga_t *svga)
{
        et4000_t *et4000 = (et4000_t *)svga->p;

        et4000_recalctimings(svga);

        if (svga->render == svga_render_text_80 && (et4000->kasan_cfg_regs[0] & 8))
        {
                svga->ma_latch -= 3;
                svga->ca_adj = (et4000->kasan_cfg_regs[0] >> 6) - 3;
                svga->ksc5601_sbyte_mask = (et4000->kasan_cfg_regs[0] & 4) << 5;
                if((et4000->kasan_cfg_regs[0] & 0x23) == 0x20 && (et4000->kasan_cfg_regs[4] & 0x80) && ((svga->crtc[0x37] & 0x0B) == 0x0A))
                        svga->render = svga_render_text_80_ksc5601;
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
        et4000->svga.ksc5601_udc_area_msb[0] = 0xC9;
        et4000->svga.ksc5601_udc_area_msb[1] = 0xFE;
        et4000->svga.ksc5601_swap_mode = 0;
        et4000->svga.ksc5601_english_font_type = 0;

        return et4000;
}

void *et4000_kasan_init()
{
        int i;
        et4000_t *et4000 = malloc(sizeof(et4000_t));
        memset(et4000, 0, sizeof(et4000_t));

        rom_init(&et4000->bios_rom, "et4000_kasan16.bin", 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        loadfont("kasan_ksc5601.rom", 6);
                
        io_sethandler(0x03c0, 0x0020, et4000_in, NULL, NULL, et4000_out, NULL, NULL, et4000);

        io_sethandler(0x0258, 0x0002, et4000_kasan_in, NULL, NULL, et4000_kasan_out, NULL, NULL, et4000);

        svga_init(&et4000->svga, et4000, 1 << 20, /*1mb*/
                   et4000_kasan_recalctimings,
                   et4000_in, et4000_out,
                   NULL,
                   NULL);

        et4000->svga.ksc5601_sbyte_mask = 0;
        et4000->kasan_cfg_index = 0;
        for(i=0; i<16; i++)
                et4000->kasan_cfg_regs[i] = 0;
        for(i=0; i<4; i++)
                et4000->kasan_font_data[i] = 0;
        et4000->kasan_cfg_regs[1] = 0x50;
        et4000->kasan_cfg_regs[2] = 2;
        et4000->kasan_cfg_regs[3] = 6;
        et4000->kasan_cfg_regs[4] = 0x78;
        et4000->kasan_cfg_regs[5] = 0xFF;
        et4000->kasan_cfg_regs[6] = 0xC9;
        et4000->kasan_cfg_regs[7] = 0xFE;
        et4000->kasan_access_addr = 0x250;
        io_sethandler(0x250, 0x0008, et4000_kasan_in, NULL, NULL, et4000_kasan_out, NULL, NULL, et4000);

        et4000->svga.ksc5601_sbyte_mask = 0;
        et4000->svga.ksc5601_udc_area_msb[0] = 0xC9;
        et4000->svga.ksc5601_udc_area_msb[1] = 0xFE;
        et4000->svga.ksc5601_swap_mode = 0;
        et4000->svga.ksc5601_english_font_type = 0x1FF;

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

static int et4000_kasan_available()
{
        return rom_present("et4000_kasan16.bin") && rom_present("kasan_ksc5601.rom");;
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

device_t et4000_kasan_device =
{
        "Kasan Hangulmadang-16(Tseng Labs ET4000AX)",
        0,
        et4000_kasan_init,
        et4000_close,
        et4000_kasan_available,
        et4000_speed_changed,
        et4000_force_redraw,
        et4000_add_status_info
};
