#include "ibm.h"
#include "cpu.h"
#include "model.h"
#include "io.h"

#include "acer386sx.h"
#include "ali1429.h"
#include "amstrad.h"
#include "device.h"
#include "dma.h"
#include "fdc.h"
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
#include "mouse_ps2.h"
#include "mouse_serial.h"
#include "neat.h"
#include "nmi.h"
#include "nvr.h"
#include "olivetti_m24.h"
#include "pci.h"
#include "pic.h"
#include "piix.h"
#include "pit.h"
#include "serial.h"
#include "sis496.h"
#include "sound_sn76489.h"
#include "um8669f.h"
#include "um8881f.h"
#include "wd76c10.h"
#include "xtide.h"

void            xt_init();
void          pcjr_init();
void       tandy1k_init();
void           ams_init();
void        europc_init();
void        olim24_init();
void            at_init();
void       at_neat_init();
void  at_acer386sx_init();
void    at_wd76c10_init();
void    at_ali1429_init();
void   at_headland_init();
void    at_um8881f_init();
void     at_sis496_init();
void     at_i430vx_init();
void     at_batman_init();
void   at_endeavor_init();

int model;

MODEL models[] =
{
        {"IBM PC",              ROM_IBMPC,     { "",      cpus_8088,    "",    NULL,         "",      NULL},         0,      xt_init},
        {"IBM XT",              ROM_IBMXT,     { "",      cpus_8088,    "",    NULL,         "",      NULL},         0,      xt_init},
        {"IBM PCjr",            ROM_IBMPCJR,   { "",      cpus_pcjr,    "",    NULL,         "",      NULL},         1,    pcjr_init},
        {"Generic XT clone",    ROM_GENXT,     { "",      cpus_8088,    "",    NULL,         "",      NULL},         0,      xt_init},
        {"DTK XT clone",        ROM_DTKXT,     { "",      cpus_8088,    "",    NULL,         "",      NULL},         0,      xt_init},        
        {"Tandy 1000",          ROM_TANDY,     { "",      cpus_8088,    "",    NULL,         "",      NULL},         1, tandy1k_init},
        {"Amstrad PC1512",      ROM_PC1512,    { "",      cpus_pc1512,  "",    NULL,         "",      NULL},         1,     ams_init},
        {"Sinclair PC200",      ROM_PC200,     { "",      cpus_8086,    "",    NULL,         "",      NULL},         1,     ams_init},
        {"Euro PC",             ROM_EUROPC,    { "",      cpus_8086,    "",    NULL,         "",      NULL},         0,  europc_init},
        {"Olivetti M24",        ROM_OLIM24,    { "",      cpus_8086,    "",    NULL,         "",      NULL},         1,  olim24_init},        
        {"Amstrad PC1640",      ROM_PC1640,    { "",      cpus_8086,    "",    NULL,         "",      NULL},         1,     ams_init},
        {"Amstrad PC2086",      ROM_PC2086,    { "",      cpus_8086,    "",    NULL,         "",      NULL},         1,     ams_init},        
        {"Amstrad PC3086",      ROM_PC3086,    { "",      cpus_8086,    "",    NULL,         "",      NULL},         1,     ams_init},
        {"IBM AT",              ROM_IBMAT,     { "",      cpus_ibmat,   "",    NULL,         "",      NULL},         0,      at_init},
        {"Commodore PC 30 III", ROM_CMDPC30,   { "",      cpus_286,     "",    NULL,         "",      NULL},         0,      at_init},        
        {"AMI 286 clone",       ROM_AMI286,    { "",      cpus_286,     "",    NULL,         "",      NULL},         0,      at_neat_init},        
        {"DELL System 200",     ROM_DELL200,   { "",      cpus_286,     "",    NULL,         "",      NULL},         0,           at_init},
        {"Acer 386SX25/N",      ROM_ACER386,   { "Intel", cpus_acer,    "",    NULL,         "",      NULL},         1, at_acer386sx_init},
        {"Amstrad MegaPC",      ROM_MEGAPC,    { "Intel", cpus_i386,    "AMD", cpus_Am386,   "Cyrix", cpus_486SDLC}, 1,   at_wd76c10_init},
        {"AMI 386 clone",       ROM_AMI386,    { "Intel", cpus_i386,    "AMD", cpus_Am386,   "Cyrix", cpus_486SDLC}, 0,  at_headland_init},
        {"AMI 486 clone",       ROM_AMI486,    { "Intel", cpus_i486,    "AMD", cpus_Am486,   "Cyrix", cpus_Cx486},   0,   at_ali1429_init},
        {"AMI WinBIOS 486",     ROM_WIN486,    { "Intel", cpus_i486,    "AMD", cpus_Am486,   "Cyrix", cpus_Cx486},   0,   at_ali1429_init},
/*        {"AMI WinBIOS 486 PCI", ROM_PCI486,    { "Intel", cpus_i486,    "AMD", cpus_Am486, "Cyrix", cpus_Cx486},   0,   at_um8881f_init},*/
        {"Award SiS 496/497",   ROM_SIS496,    { "Intel", cpus_i486,    "AMD", cpus_Am486,   "Cyrix", cpus_Cx486},   0,    at_sis496_init},
#ifdef DYNAREC
        {"Intel Premiere/PCI",  ROM_REVENGE,   { "Intel", cpus_Pentium5V, "",  NULL,         "",      NULL},         0,    at_batman_init},
        {"Intel Advanced/EV",   ROM_ENDEAVOR,  { "Intel", cpus_Pentium, "IDT", cpus_WinChip, "",      NULL},         0,  at_endeavor_init},
        {"Award 430VX PCI",     ROM_430VX,     { "Intel", cpus_Pentium, "IDT", cpus_WinChip, "",      NULL},         0,    at_i430vx_init},
#else
        {"Intel Advanced/EV",   ROM_ENDEAVOR,  { "IDT", cpus_WinChip,   "",    NULL,         "",      NULL},         0,  at_endeavor_init},
        {"Award 430VX PCI",     ROM_430VX,     { "IDT", cpus_WinChip,   "",    NULL,         "",      NULL},         0,    at_i430vx_init},
#endif
        {"", -1, {"", 0, "", 0, "", 0}, 0}
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

void common_init()
{
        dma_init();
        fdc_add();
        lpt_init();
        pic_init();
        pit_init();
        serial1_init(0x3f8, 4);
        serial2_init(0x2f8, 3);
        device_add(&gameport_device);
}

void xt_init()
{
        common_init();
        pit_set_out_func(1, pit_refresh_timer_xt);
        keyboard_xt_init();
        mouse_serial_init();
        xtide_init();
	nmi_init();
}

void pcjr_init()
{
        fdc_add_pcjr();
        pic_init();
        pit_init();
        pit_set_out_func(0, pit_irq0_timer_pcjr);
        serial1_init(0x2f8, 3);
        keyboard_pcjr_init();
        device_add(&sn76489_device);
	nmi_mask = 0x80;
}

void tandy1k_init()
{
        common_init();
        keyboard_xt_init();
        mouse_serial_init();
        device_add(&sn76489_device);
        xtide_init();
	nmi_init();
}

void ams_init()
{
        common_init();
        amstrad_init();
        keyboard_amstrad_init();
        nvr_init();
        xtide_init();
	nmi_init();
}

void europc_init()
{
        common_init();
        jim_init();
        keyboard_xt_init();
        mouse_serial_init();
        xtide_init();
	nmi_init();
}

void olim24_init()
{
        common_init();
        keyboard_olim24_init();
        nvr_init();
        olivetti_m24_init();
        xtide_init();
	nmi_init();
}

void at_init()
{
        common_init();
        pit_set_out_func(1, pit_refresh_timer_at);
        dma16_init();
        ide_init();
        keyboard_at_init();
        if (models[model].init == at_init)
           mouse_serial_init();
        nvr_init();
        pic2_init();
}

void at_neat_init()
{
        at_init();
        mouse_serial_init();
        neat_init();
}

void at_acer386sx_init()
{
        at_init();
        mouse_ps2_init();
        acer386sx_init();
}

void at_wd76c10_init()
{
        at_init();
        mouse_ps2_init();
        wd76c10_init();
}

void at_headland_init()
{
        at_init();
        headland_init();
        mouse_serial_init();
}

void at_ali1429_init()
{
        at_init();
        ali1429_init();
        mouse_serial_init();
}

void at_um8881f_init()
{
        at_init();
        mouse_serial_init();
        pci_init(PCI_CONFIG_TYPE_1, 0, 31);
        um8881f_init();
}

void at_sis496_init()
{
        at_init();
        mouse_serial_init();
        pci_init(PCI_CONFIG_TYPE_1, 0, 31);
        device_add(&sis496_device);
}

void at_batman_init()
{
        at_init();
        mouse_serial_init();
        pci_init(PCI_CONFIG_TYPE_2, 0xd, 0x10);
        i430lx_init();
        um8669f_init();
        intel_batman_init();
}
void at_endeavor_init()
{
        at_init();
        mouse_serial_init();
        pci_init(PCI_CONFIG_TYPE_1, 0xd, 0x10);
        i430fx_init();
        piix_init(7);
        um8669f_init();
        intel_endeavor_init();
        device_add(&intel_flash_device);
}

void at_i430vx_init()
{
        at_init();
        mouse_serial_init();
        pci_init(PCI_CONFIG_TYPE_1, 0, 31);
        i430vx_init();
        piix_init(7);
        um8669f_init();
}

void model_init()
{
        pclog("Initting as %s\n", model_getname());
        io_init();
        
        models[model].init();
}
