#include "ibm.h"
#include "cpu.h"
#include "model.h"
#include "io.h"
#include "mouse.h"

#include "acer386sx.h"
#include "ali1429.h"
#include "amstrad.h"
#include "compaq.h"
#include "dells200.h"
#include "device.h"
#include "dma.h"
#include "fdc.h"
#include "fdc37c665.h"
#include "gameport.h"
#include "headland.h"
#include "i430fx.h"
#include "i430lx.h"
#include "i430vx.h"
#include "ide.h"
#include "intel.h"
#include "intel_flash.h"
#include "jim.h"
#include "keyboard_amstrad.h"
#include "keyboard_at.h"
#include "keyboard_olim24.h"
#include "keyboard_pcjr.h"
#include "keyboard_xt.h"
#include "lpt.h"
#include "mem.h"
#include "mouse_ps2.h"
#include "neat.h"
#include "nmi.h"
#include "nvr.h"
#include "olivetti_m24.h"
#include "opti495.h"
#include "pci.h"
#include "pic.h"
#include "piix.h"
#include "pit.h"
#include "ps1.h"
#include "ps2.h"
#include "ps2_mca.h"
#include "scat.h"
#include "serial.h"
#include "sis496.h"
#include "sound_ps1.h"
#include "sound_pssj.h"
#include "sound_sn76489.h"
#include "tandy_eeprom.h"
#include "tandy_rom.h"
#include "um8669f.h"
#include "um8881f.h"
#include "vid_pcjr.h"
#include "vid_tandy.h"
#include "wd76c10.h"

void             xt_init();
void           pcjr_init();
void        tandy1k_init();
void     tandy1ksl2_init();
void            ams_init();
void         europc_init();
void         olim24_init();
void             at_init();
void         ibm_at_init();
void         at_ide_init();
void       dells200_init();
void     deskpro386_init();
void      ps1_m2011_init();
void      ps1_m2121_init();
void    ps2_m30_286_init();
void   ps2_model_50_init();
void ps2_model_55sx_init();
void   ps2_model_80_init();
void        at_neat_init();
void        at_scat_init();
void   at_acer386sx_init();
void     at_wd76c10_init();
void     at_ali1429_init();
void    at_headland_init();
void     at_opti495_init();
void     at_um8881f_init();
void      at_sis496_init();
void      at_i430vx_init();
void      at_batman_init();
void    at_endeavor_init();

int model;

int AMSTRAD, AT, PCI, TANDY;

MODEL models[] =
{
        {"IBM PC",                ROM_IBMPC,            "ibmpc",          { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"IBM XT",                ROM_IBMXT,            "ibmxt",          { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"IBM PCjr",              ROM_IBMPCJR,          "ibmpcjr",        { {"",      cpus_pcjr},        {"",    NULL},         {"",      NULL}},        1, 0,                  128, 640,  64,         pcjr_init, &pcjr_device},
        {"Generic XT clone",      ROM_GENXT,            "genxt",          { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"AMI XT clone",          ROM_AMIXT,            "amixt",          { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"DTK XT clone",          ROM_DTKXT,            "dtk",            { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"VTech Laser Turbo XT",  ROM_LTXT,             "ltxt",           { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"VTech Laser XT3",       ROM_LXT3,             "lxt3",           { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"Phoenix XT clone",      ROM_PXXT,             "pxxt",           { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"Juko XT clone",         ROM_JUKOPC,           "jukopc",         { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        0, 0,                   64, 640,  64,           xt_init, NULL},
        {"Tandy 1000",            ROM_TANDY,            "tandy",          { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        1, 0,                  128, 640, 128,      tandy1k_init, &tandy1000_device},
        {"Tandy 1000 HX",         ROM_TANDY1000HX,      "tandy1000hx",    { {"",      cpus_8088},        {"",    NULL},         {"",      NULL}},        1, 0,                  256, 640, 128,      tandy1k_init, &tandy1000hx_device},
        {"Tandy 1000 SL/2",       ROM_TANDY1000SL2,     "tandy1000sl2",   { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        1, 0,                  512, 768, 128,   tandy1ksl2_init, NULL},
        {"Amstrad PC1512",        ROM_PC1512,           "pc1512",         { {"",      cpus_pc1512},      {"",    NULL},         {"",      NULL}},        1, MODEL_AMSTRAD,      512, 640, 128,          ams_init, NULL},
        {"Sinclair PC200",        ROM_PC200,            "pc200",          { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        1, MODEL_AMSTRAD,      512, 640, 128,          ams_init, NULL},
        {"Euro PC",               ROM_EUROPC,           "europc",         { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        0, 0,                  512, 640, 128,       europc_init, NULL},
        {"Olivetti M24",          ROM_OLIM24,           "olivetti_m24",   { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        1, MODEL_OLIM24,       128, 640, 128,       olim24_init, NULL},
        {"Amstrad PC1640",        ROM_PC1640,           "pc1640",         { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        1, MODEL_AMSTRAD,      640, 640,   0,          ams_init, NULL},
        {"Amstrad PC2086",        ROM_PC2086,           "pc2086",         { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        1, MODEL_AMSTRAD,      640, 640,   0,          ams_init, NULL},
        {"Amstrad PC3086",        ROM_PC3086,           "pc3086",         { {"",      cpus_8086},        {"",    NULL},         {"",      NULL}},        1, MODEL_AMSTRAD,      640, 640,   0,          ams_init, NULL},
        {"IBM AT",                ROM_IBMAT,            "ibmat",          { {"",      cpus_ibmat},       {"",    NULL},         {"",      NULL}},        0, MODEL_AT,                           1,  16,   1,         ibm_at_init, NULL},
        {"Commodore PC 30 III",   ROM_CMDPC30,          "cmdpc30",        { {"",      cpus_286},         {"",    NULL},         {"",      NULL}},        0, MODEL_AT|MODEL_HAS_IDE,             1,  16,   1,         at_ide_init, NULL},
        {"AMI 286 clone",         ROM_AMI286,           "ami286",         { {"",      cpus_286},         {"",    NULL},         {"",      NULL}},        0, MODEL_AT|MODEL_HAS_IDE,             1,  16,   1,        at_neat_init, NULL},
        {"Award 286 clone",       ROM_AWARD286,         "award286",       { {"",      cpus_286},         {"",    NULL},         {"",      NULL}},        0, MODEL_AT|MODEL_HAS_IDE,             1,  16,   1,        at_scat_init, NULL},
        {"DELL System 200",       ROM_DELL200,          "dells200",       { {"",      cpus_286},         {"",    NULL},         {"",      NULL}},        0, MODEL_AT,                           1,  16,   1,       dells200_init, NULL},
        {"IBM PS/1 model 2011",   ROM_IBMPS1_2011,      "ibmps1es",       { {"",      cpus_ps1_m2011},   {"",    NULL},         {"",      NULL}},        1, MODEL_AT|MODEL_PS2,                 1,  16,   1,      ps1_m2011_init, NULL},
        {"IBM PS/2 Model 30-286", ROM_IBMPS2_M30_286,   "ibmps2_m30_286", { {"",      cpus_ps2_m30_286}, {"",    NULL},         {"",      NULL}},        1, MODEL_AT|MODEL_PS2,                 1,  16,   1,    ps2_m30_286_init, NULL},
        {"IBM PS/2 Model 50",     ROM_IBMPS2_M50,       "ibmps2_m50",     { {"",      cpus_ps2_m30_286}, {"",    NULL},         {"",      NULL}},        1, MODEL_AT|MODEL_PS2|MODEL_MCA,       1,  16,   1,   ps2_model_50_init, NULL},
        {"IBM PS/1 model 2121",   ROM_IBMPS1_2121,      "ibmps1_2121",    { {"Intel", cpus_i386SX},      {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, 1, MODEL_AT|MODEL_PS2|MODEL_HAS_IDE,   1,  16,   1,      ps1_m2121_init, NULL},
        {"Compaq Deskpro 386",    ROM_DESKPRO_386,      "deskpro386",     { {"Intel", cpus_i386DX},      {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, 0, MODEL_AT,                           1,  15,   1,     deskpro386_init, NULL},
        {"IBM PS/2 Model 55SX",   ROM_IBMPS2_M55SX,     "ibmps2_m55sx",   { {"Intel", cpus_i386SX},      {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, 1, MODEL_AT|MODEL_PS2|MODEL_MCA,       1,   8,   1, ps2_model_55sx_init, NULL},
        {"Acer 386SX25/N",        ROM_ACER386,          "acer386",        { {"Intel", cpus_acer},        {"",    NULL},         {"",      NULL}},        1, MODEL_AT|MODEL_PS2|MODEL_HAS_IDE,   1,  16,   1,   at_acer386sx_init, NULL},
        {"DTK 386SX clone",       ROM_DTK386,           "dtk386",         { {"Intel", cpus_i386SX},      {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, 0, MODEL_AT|MODEL_HAS_IDE,             1,  16,   1,        at_neat_init, NULL},
/*        {"Phoenix 386 clone",   ROM_PX386,       { "Intel", cpus_i386SX,  "AMD", cpus_Am386SX, "Cyrix", cpus_486SLC},  0, MODEL_AT,   1,  16, 1,             at_init, NULL},*/
        {"Amstrad MegaPC",        ROM_MEGAPC,           "megapc",         { {"Intel", cpus_i386SX},      {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, 1, MODEL_AT|MODEL_PS2|MODEL_HAS_IDE,   1,  16,   1,     at_wd76c10_init, NULL},
        {"AMI 386SX clone",       ROM_AMI386SX,         "ami386",         { {"Intel", cpus_i386SX},      {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, 0, MODEL_AT|MODEL_HAS_IDE,             1, 256,   1,    at_headland_init, NULL},
        {"IBM PS/2 Model 80",     ROM_IBMPS2_M80,       "ibmps2_m80",     { {"Intel", cpus_i386DX},      {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, 1, MODEL_AT|MODEL_PS2|MODEL_MCA,       1,  12,   1,   ps2_model_80_init, NULL},
        {"MR 386DX clone",        ROM_MR386DX_OPTI495,  "mr386dx",        { {"Intel", cpus_i386DX},      {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, 0, MODEL_AT|MODEL_HAS_IDE,             1, 256,   1,     at_opti495_init, NULL},
        {"AMI 386DX clone",       ROM_AMI386DX_OPTI495, "ami386dx",       { {"Intel", cpus_i386DX},      {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, 0, MODEL_AT|MODEL_HAS_IDE,             1, 256,   1,     at_opti495_init, NULL},
        {"AMI 486 clone",         ROM_AMI486,           "ami486",         { {"Intel", cpus_i486},        {"AMD", cpus_Am486},   {"Cyrix", cpus_Cx486}},  0, MODEL_AT|MODEL_HAS_IDE,             1, 256,   1,     at_ali1429_init, NULL},
        {"AMI WinBIOS 486",       ROM_WIN486,           "win486",         { {"Intel", cpus_i486},        {"AMD", cpus_Am486},   {"Cyrix", cpus_Cx486}},  0, MODEL_AT|MODEL_HAS_IDE,             1, 256,   1,     at_ali1429_init, NULL},
/*        {"AMI WinBIOS 486 PCI", ROM_PCI486,    { "Intel", cpus_i486,    "AMD", cpus_Am486, "Cyrix", cpus_Cx486},   0, 1,  1, 256, 1, at_um8881f_init},*/
        {"Award SiS 496/497",     ROM_SIS496,           "sis496",         { {"Intel", cpus_i486},        {"AMD", cpus_Am486},   {"Cyrix", cpus_Cx486}},  0, MODEL_AT|MODEL_PCI|MODEL_HAS_IDE,             1, 256,   1,      at_sis496_init, NULL},
        {"Intel Premiere/PCI",    ROM_REVENGE,          "revenge",        { {"Intel", cpus_Pentium5V},   {"",    NULL},         {"",      NULL}},        0, MODEL_AT|MODEL_PCI|MODEL_PS2|MODEL_HAS_IDE,   1, 128,   1,      at_batman_init, NULL},
        {"Intel Advanced/EV",     ROM_ENDEAVOR,         "endeavor",       { {"Intel", cpus_PentiumS5},   {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}},   0, MODEL_AT|MODEL_PCI|MODEL_PS2|MODEL_HAS_IDE,   1, 128,   1,    at_endeavor_init, NULL},
        {"Award 430VX PCI",       ROM_430VX,            "430vx",          { {"Intel", cpus_Pentium},     {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}},   0, MODEL_AT|MODEL_PCI|MODEL_PS2|MODEL_HAS_IDE,   1, 256,   1,      at_i430vx_init, NULL},
        {"", -1, "", {{"", 0}, {"", 0}, {"", 0}}, 0,0,0, 0}
};

int model_count()
{
        return (sizeof(models) / sizeof(MODEL)) - 1;
}

int model_getromset()
{
        return models[model].id;
}

int model_getmodel(int romset)
{
	int c = 0;
	
	while (models[c].id != -1)
	{
		if (models[c].id == romset)
			return c;
		c++;
	}
	
	return 0;
}

char *model_getname()
{
        return models[model].name;
}

device_t *model_getdevice(int model)
{
        return models[model].device;
}

char *model_get_internal_name()
{
        return models[model].internal_name;
}

int model_get_model_from_internal_name(char *s)
{
	int c = 0;
	
	while (models[c].id != -1)
	{
		if (!strcmp(models[c].internal_name, s))
			return c;
		c++;
	}
	
	return 0;
}

void common_init()
{
        dma_init();
        fdc_add();
        lpt_init();
        pic_init();
        pit_init();
        serial1_init(0x3f8, 4);
        serial2_init(0x2f8, 3);
}

void xt_init()
{
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_xt);
        keyboard_xt_init();
	nmi_init();
        device_add(&gameport_device);
}

void pcjr_init()
{
        mem_add_bios();
        fdc_add_pcjr();
        pic_init();
        pit_init();
        pit_set_out_func(&pit, 0, pit_irq0_timer_pcjr);
        serial1_init(0x2f8, 3);
        keyboard_pcjr_init();
        device_add(&sn76489_device);
	nmi_mask = 0x80;
}

void tandy1k_init()
{
        TANDY = 1;
        common_init();
        mem_add_bios();
        keyboard_tandy_init();
        if (romset == ROM_TANDY)
                device_add(&sn76489_device);
        else
                device_add(&ncr8496_device);
	nmi_init();
	if (romset != ROM_TANDY)
                device_add(&tandy_eeprom_device);
        device_add(&gameport_device);
}
void tandy1ksl2_init()
{
//        TANDY = 1;
        common_init();
        mem_add_bios();
        keyboard_tandy_init();
        device_add(&pssj_device);
	nmi_init();
        device_add(&tandy_rom_device);
        device_add(&tandy_eeprom_device);
        device_add(&gameport_device);
}

void ams_init()
{
        AMSTRAD = 1;
        common_init();
        mem_add_bios();
        amstrad_init();
        keyboard_amstrad_init();
        nvr_init();
	nmi_init();
	fdc_set_dskchg_activelow();
        device_add(&gameport_device);
}

void europc_init()
{
        common_init();
        mem_add_bios();
        jim_init();
        keyboard_xt_init();
	nmi_init();
        device_add(&gameport_device);
}

void olim24_init()
{
        common_init();
        mem_add_bios();
        keyboard_olim24_init();
        nvr_init();
        olivetti_m24_init();
	nmi_init();
        device_add(&gameport_device);
}

void at_init()
{
        AT = 1;
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_at);
        dma16_init();
        keyboard_at_init();
        nvr_init();
        pic2_init();
        device_add(&gameport_device);
}

void ibm_at_init()
{
        at_init();
        mem_remap_top_384k();
}

void at_ide_init()
{
        at_init();
        ide_init();
}

void deskpro386_init()
{
        at_init();
        compaq_init();
}

void dells200_init()
{
        at_init();
        dells200_chipset_init();
}

void ps1_common_init()
{
        AT = 1;
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_at);
        dma16_init();
        ide_init();
        keyboard_at_init();
        nvr_init();
        pic2_init();
        fdc_set_dskchg_activelow();
        device_add(&ps1_audio_device);
        /*PS/1 audio uses ports 200h and 202-207h, so only initialise gameport on 201h*/
        device_add(&gameport_201_device);
}

void ps1_m2011_init()
{
        ps1_common_init();
        ps1mb_init();
}

void ps1_m2121_init()
{
        ps1_common_init();
        ps1mb_m2121_init();
        fdc_set_ps1();
}

void ps2_m30_286_init()
{
        AT = 1;
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_at);
        dma16_init();
        ide_init();
        keyboard_at_init();
//        mouse_ps2_init();
        nvr_init();
        pic2_init();
        ps2board_init();
        fdc_set_dskchg_activelow();
}

static void ps2_common_init()
{
        AT = 1;
        common_init();
        mem_add_bios();
        dma16_init();
        ps2_dma_init();
        ide_init();
        keyboard_at_init();
        keyboard_at_init_ps2();
//        mouse_ps2_init();
        nvr_init();
        pic2_init();

        pit_ps2_init();
}

void ps2_model_50_init()
{
        ps2_common_init();
        ps2_mca_board_model_50_init();
}

void ps2_model_55sx_init()
{
        ps2_common_init();
        ps2_mca_board_model_55sx_init();
}

void ps2_model_80_init()
{
        ps2_common_init();
        ps2_mca_board_model_80_type2_init();
}

void at_neat_init()
{
        at_init();
        ide_init();
        neat_init();
}

void at_scat_init()
{
        at_init();
        ide_init();
        scat_init();
}

void at_acer386sx_init()
{
        at_init();
        ide_init();
        acer386sx_init();
}

void at_wd76c10_init()
{
        at_init();
        ide_init();
        wd76c10_init();
}

void at_headland_init()
{
        at_init();
        ide_init();
        headland_init();
}

void at_opti495_init()
{
        at_init();
        ide_init();
        opti495_init();
}

void at_ali1429_init()
{
        at_init();
        ide_init();
        ali1429_init();
}

void at_um8881f_init()
{
        at_init();
        ide_init();
        pci_init(PCI_CONFIG_TYPE_1, 0, 31);
        um8881f_init();
}

void at_sis496_init()
{
        at_init();
        ide_init();
        pci_init(PCI_CONFIG_TYPE_1, 0, 31);
        device_add(&sis496_device);
}

void at_batman_init()
{
        at_init();
        ide_init();
        pci_init(PCI_CONFIG_TYPE_2, 0xd, 0x10);
        i430lx_init();
        fdc37c665_init();
        intel_batman_init();
        device_add(&intel_flash_bxt_ami_device);
}
void at_endeavor_init()
{
        at_init();
        ide_init();
        pci_init(PCI_CONFIG_TYPE_1, 0xd, 0x10);
        i430fx_init();
        piix_init(7);
        um8669f_init();
        intel_endeavor_init();
        device_add(&intel_flash_bxt_ami_device);
}

void at_i430vx_init()
{
        at_init();
        ide_init();
        pci_init(PCI_CONFIG_TYPE_1, 0, 31);
        i430vx_init();
        piix_init(7);
        um8669f_init();
        device_add(&intel_flash_bxt_device);
}

void model_init()
{
        pclog("Initting as %s\n", model_getname());
        AMSTRAD = AT = PCI = TANDY = 0;
        io_init();
        
        models[model].init();
        if (models[model].device)
                device_add(models[model].device);
}
