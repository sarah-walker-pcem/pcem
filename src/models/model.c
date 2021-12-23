#include "ibm.h"
#include "cpu.h"
#include "model.h"
#include "io.h"
#include "mouse.h"

#include "82091aa.h"
#include "acc2036.h"
#include "acc2168.h"
#include "acc3221.h"
#include "acer386sx.h"
#include "ali1429.h"
#include "amstrad.h"
#include "cbm_io.h"
#include "cmd640.h"
#include "compaq.h"
#include "cs8230.h"
#include "dells200.h"
#include "device.h"
#include "cassette.h"
#include "dma.h"
#include "f82c710_upc.h"
#include "fdc.h"
#include "fdc37c665.h"
#include "fdc37c93x.h"
#include "gameport.h"
#include "headland.h"
#include "i430fx.h"
#include "i430hx.h"
#include "i430lx.h"
#include "i430vx.h"
#include "i440bx.h"
#include "i440fx.h"
#include "ide.h"
#include "intel.h"
#include "intel_flash.h"
#include "jim.h"
#include "keyboard_amstrad.h"
#include "keyboard_at.h"
#include "keyboard_olim24.h"
#include "keyboard_pcjr.h"
#include "keyboard_xt.h"
#include "laserxt.h"
#include "lpt.h"
#include "mem.h"
#include "mouse_ps2.h"
#include "mvp3.h"
#include "neat.h"
#include "nmi.h"
#include "nvr.h"
#include "olivetti_m24.h"
#include "opti495.h"
#include "pc87306.h"
#include "pc87307.h"
#include "pci.h"
#include "pic.h"
#include "piix.h"
#include "pit.h"
#include "ps1.h"
#include "ps2.h"
#include "ps2_mca.h"
#include "scamp.h"
#include "scat.h"
#include "serial.h"
#include "sio.h"
#include "sis496.h"
#include "sl82c460.h"
#include "sound_ps1.h"
#include "sound_pssj.h"
#include "sound_sn76489.h"
#include "superxt.h"
#include "sst39sf010.h"
#include "tandy_eeprom.h"
#include "tandy_rom.h"
#include "um8669f.h"
#include "vid_pcjr.h"
#include "vid_tandy.h"
#include "vid_t1000.h"
#include "vl82c480.h"
#include "vt82c586b.h"
#include "w83877tf.h"
#include "w83977tf.h"
#include "wd76c10.h"
#include "xi8088.h"
#include "zenith.h"

void             xt_init();
void           pcjr_init();
void        tandy1k_init();
void     tandy1ksl2_init();
void            ams_init();
void         pc5086_init();
void         europc_init();
void         olim24_init();
void             at_init();
void         ibm_at_init();
void         at_cbm_init();
void       dells200_init();
void     deskpro386_init();
void      pb_l300sx_init();
void      ps1_m2011_init();
void      ps1_m2121_init();
void      ps1_m2133_init(void);
void    ps2_m30_286_init();
void   ps2_model_50_init();
void ps2_model_55sx_init();
void   ps2_model_70_init();
void   ps2_model_80_init();
void        at_neat_init();
void       at_scamp_init();
void        at_scat_init();
void      at_scatsx_init();
void   at_acer386sx_init();
void     at_wd76c10_init();
void      at_cs8230_init();
void     at_ali1429_init();
void    at_headland_init();
void     at_opti495_init();
void      at_sis496_init();
void       at_p55va_init();
void     at_p55tvp4_init();
void      at_i430vx_init();
void      at_batman_init();
void    at_endeavor_init();
void     at_p55t2p4_init();
void     xt_laserxt_init();
void      at_t3100e_init();
void       xt_t1000_init();
void       xt_t1200_init();
void    at_sl82c460_init();
void       at_zappa_init();
void      at_pb410a_init();
void      at_pb520r_init();
void       at_pb570_init();
void      compaq_xt_init();
void      xt_xi8088_init();
void      xt_zenith_init();
void        at_mvp3_init();
void     at_vs440fx_init();
void     at_ga686bx_init();
int model;

int AMSTRAD, AT, PCI, TANDY, MCA;

MODEL *models[256];

int model_count()
{
        int ret = 0;

        while (models[ret] != NULL && ret < 256)
                ret++;

        return ret;
}

int model_getromset()
{
        return models[model]->id;
}

int model_getromset_from_model(int model)
{
        return models[model]->id;
}

int model_getmodel(int romset)
{
	int c = 0;
	
	while (models[c]->id != -1)
	{
		if (models[c]->id == romset)
			return c;
		c++;
	}
	
	return 0;
}

char *model_getname()
{
        return models[model]->name;
}

device_t *model_getdevice(int model)
{
        return models[model]->device;
}

char *model_get_internal_name()
{
        return models[model]->internal_name;
}

int model_get_model_from_internal_name(char *s)
{
	int c = 0;
	
	while (models[c] != NULL)
	{
		if (!strcmp(models[c]->internal_name, s))
			return c;
		c++;
	}
	
	return 0;
}

int model_has_fixed_gfx(int model)
{
        int gfx_flags = models[model]->flags & MODEL_GFX_MASK;
        
        return (gfx_flags == MODEL_GFX_FIXED);
}

int model_has_optional_gfx(int model)
{
        int gfx_flags = models[model]->flags & MODEL_GFX_MASK;
        
        return (gfx_flags == MODEL_GFX_DISABLE_HW || gfx_flags == MODEL_GFX_DISABLE_SW);
}

void common_init()
{
        dma_init();
        fdc_add();
        lpt_init();
        pic_init();
        pit_init();
        serial1_init(0x3f8, 4, 1);
        serial2_init(0x2f8, 3, 1);
}

void xt_init()
{
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_xt);
        keyboard_xt_init();
	nmi_init();
        device_add(&gameport_device);
	if (romset == ROM_IBMPC)
		device_add(&cassette_device);
}

void compaq_xt_init()
{
        xt_init();
        lpt1_remove();
        lpt2_remove();
        lpt1_init(0x3bc);
}

void pcjr_init()
{
        mem_add_bios();
        fdc_add_pcjr();
        pic_init();
        pit_init();
        pit_set_out_func(&pit, 0, pit_irq0_timer_pcjr);
        serial1_init(0x2f8, 3, 1);
        keyboard_pcjr_init();
        device_add(&sn76489_device);
	nmi_mask = 0x80;
	device_add(&cassette_device);
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
        lpt1_remove();
        amstrad_init();
        keyboard_amstrad_init();
        device_add(&nvr_device);
        nmi_init();
        fdc_set_dskchg_activelow();
        device_add(&gameport_device);
}

void pc5086_init()
{
        xt_init();
        lpt1_remove();      /* remove LPT ports, they will be enabled by 82C710 */
        lpt2_remove();
        serial1_remove();   /* remove COM ports, they will be enabled by 82C710 */
        serial2_remove();
        device_add(&nvr_device);
	    fdc_set_dskchg_activelow();
        superxt_init();
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
        device_add(&nvr_device);
        olivetti_m24_init();
	nmi_init();
        device_add(&gameport_device);
}

void xt_laserxt_init()
{
        xt_init();
        laserxt_init();
}

void xt_zenith_init() /* [8088] Zenith Data Systems SupersPort */
{
        dma_init();
        fdc_add();
        lpt_init();
        lpt2_remove(); /* only one parallel port */
        pic_init();
        pit_init();
        serial1_init(0x3f8, 4, 1); /* only one serial port */
        mem_add_bios();
        device_add(&zenith_scratchpad_device);
        pit_set_out_func(&pit, 1, pit_refresh_timer_xt);
        keyboard_xt_init();
        nmi_init();
}

void xt_xi8088_init()
{
        common_init();
        mem_add_bios();
        keyboard_at_init();
        keyboard_at_init_ps2();
        nmi_init();
        device_add(&nvr_device);
        pic2_init();
        device_add(&gameport_device);
        device_add(&sst_39sf010_device);
}

void at_init()
{
        AT = 1;
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_at);
        dma16_init();
        keyboard_at_init();
        device_add(&nvr_device);
        pic2_init();
        device_add(&gameport_device);
        nmi_mask = 0;
}

void ibm_at_init()
{
        at_init();
        mem_remap_top_384k();
}

void at_cbm_init()
{
        at_init();
        cbm_io_init();
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
        keyboard_at_init();
        device_add(&nvr_device);
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
        mem_remap_top_384k();
}

void ps1_m2121_init()
{
        ps1_common_init();
        ps1mb_m2121_init();
        fdc_set_ps1();
}

void ps1_m2133_init(void)
{
        at_init();
        vl82c480_init();
        ps1mb_m2133_init();
}

void ps2_m30_286_init()
{
        AT = 1;
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_at);
        dma16_init();
        keyboard_at_init();
//        mouse_ps2_init();
        device_add(&nvr_device);
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
        keyboard_at_init();
        keyboard_at_init_ps2();
//        mouse_ps2_init();
        device_add(&nvr_device);
        pic2_init();

        pit_ps2_init();

	nmi_mask = 0x80;
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

void ps2_model_70_init()
{
        ps2_common_init();
        ps2_mca_board_model_70_type34_init(romset == ROM_IBMPS2_M70_TYPE4);
}

void ps2_model_80_init()
{
        ps2_common_init();
        ps2_mca_board_model_80_type2_init();
}

void at_neat_init()
{
        at_init();
        neat_init();
}

void at_scamp_init()
{
        at_init();
        scamp_init();
}

void at_scat_init()
{
        at_init();
        scat_init();
}

void at_scatsx_init()
{
        at_init();
        scatsx_init();
}

void at_acer386sx_init()
{
        at_init();
        acer386sx_init();
}

void at_wd76c10_init()
{
        at_init();
        wd76c10_init();
}

void at_headland_init()
{
        at_init();
        headland_init();
}

void at_opti495_init()
{
        at_init();
        opti495_init();
}

void at_cs8230_init()
{
        at_init();
        cs8230_init();
}

void at_ali1429_init()
{
        at_init();
        ali1429_init();
}

void pb_l300sx_init()
{
        at_init();
        acc2036_init();
}

void at_pb410a_init()
{
        at_init();
        acc2168_init();
        acc3221_init();
}


void at_sis496_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0xb);
        pci_slot(0xd);
        pci_slot(0xf);
        device_add(&sis496_device);
}

void at_sl82c460_init()
{
        at_init();
        sl82c460_init();
}

void at_batman_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_2);
        pci_slot(0xc);
        pci_slot(0xe);
        pci_slot(0x6);
        i430lx_init();
        sio_init(2, 0xc, 0xe, 0x6, 0);
        fdc37c665_init();
        intel_batman_init();
        device_add(&intel_flash_bxt_ami_device);
}
void at_pb520r_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_2);
        pci_slot(0xc);
        pci_slot(0xe);
        pci_slot(0x6);
        i430lx_init();
        sio_init(2, 0xc, 0xe, 0x6, 0);
        cmd640b_init(1);
        aip_82091aa_init(0x22);
        intel_batman_init();
        device_add(&intel_flash_bxt_ami_device);
}
void at_endeavor_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0xd);
        pci_slot(0xe);
        pci_slot(0xf);
        pci_slot(0x10);
        i430fx_init();
        piix_init(7, 0xd, 0xe, 0xf, 0x10, i430fx_reset);
        pc87306_init(0x2e);
        intel_endeavor_init();
        device_add(&intel_flash_bxt_ami_device);
}
void at_pb570_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0x11);
        pci_slot(0x13);
        i430fx_init();
        piix_init(7, 0x11, 0x13, 0xb, 0x8, i430fx_reset);
        pc87306_init(0x2e);
        intel_endeavor_init();
        device_add(&intel_flash_bxt_ami_device);
}
void at_zappa_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0xd);
        pci_slot(0xe);
        pci_slot(0xf);
        pci_slot(0x10);
        i430fx_init();
        piix_init(7, 0xd, 0xf, 0xe, 0x10, i430fx_reset);
        pc87306_init(0x2e);
        intel_zappa_init();
        device_add(&intel_flash_bxt_ami_device);
}

void at_p55va_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0x08);
        pci_slot(0x09);
        pci_slot(0x0A);
        pci_slot(0x0B);
        i430vx_init();
        piix_init(7, 0x08, 0x09, 0x0A, 0x0B, i430vx_reset);
        fdc37c932fr_init();
        device_add(&intel_flash_bxt_device);
}

void at_p55tvp4_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0x0C);
        pci_slot(0x0B);
        pci_slot(0x0A);
        pci_slot(0x09);
        i430vx_init();
        piix_init(7, 0x0C, 0x0B, 0x0A, 0x09, i430vx_reset);
        w83877f_init(0x3f0, 0x87);
        device_add(&intel_flash_bxt_device);
}

void at_i430vx_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0x11);
        pci_slot(0x12);
        pci_slot(0x13);
        pci_slot(0x14);
        i430vx_init();
        piix_init(7, 18, 17, 20, 19, i430vx_reset);
        um8669f_init();
        device_add(&intel_flash_bxt_device);
}

void at_p55t2p4_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0x0C);
        pci_slot(0x0B);
        pci_slot(0x0A);
        pci_slot(0x09);
        i430hx_init();
        piix_init(7, 0x0C, 0x0B, 0x0A, 0x09, i430hx_reset);
        w83877f_init(0x3f0, 0x87);
        device_add(&intel_flash_bxt_device);
}

void at_mvp3_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(8);
        pci_slot(9);
        pci_slot(10);
        mvp3_init();
        vt82c586b_init(7, 8, 9, 10, 0);
        w83877tf_init(0x250, 0x89);
        device_add(&sst_39sf010_device);
}

void at_vs440fx_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0xb);
        pci_slot(0xf);
        pci_slot(0x11);
        pci_slot(0x13);
        i440fx_init();
        piix_init(7, 0xb, 0xf, 0x11, 0x13, i440fx_reset);
        pc87307_init(0x2e);
//        i440fx_init();
        device_add(&intel_flash_28fb200bxt_device);
}

void at_ga686bx_init()
{
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0x8);
        pci_slot(0x9);
        pci_slot(0xa);
        pci_slot(0xb);
        i440bx_init();
        piix4_init(7, 0x8, 0x9, 0xa, 0xb, i440bx_reset);
        w83977tf_init(0x3f0, 0x87);
        device_add(&intel_flash_28f002bc_device);
}

void model_init()
{
        pclog("Initting as %s\n", model_getname());
        AMSTRAD = AT = PCI = TANDY = MCA = 0;
        ide_set_bus_master(NULL, NULL, NULL, NULL);
        
        models[model]->init();
        if (models[model]->device)
                device_add(models[model]->device);
}

MODEL m_1 = {"[8088] AMI XT clone", ROM_AMIXT, "amixt", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64, xt_init, NULL};

MODEL m_2 = {"[8088] Atari PC3", ROM_ATARIPC3, "ataripc3", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64, xt_init, NULL};
MODEL m_3 = {"[8088] Compaq Portable Plus", ROM_COMPAQ_PIP, "compaq_pip", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 128, 640, 64, compaq_xt_init, NULL};
MODEL m_4 = {"[8088] DTK XT clone", ROM_DTKXT, "dtk", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64, xt_init, NULL};
MODEL m_5 = {"[8088] Generic XT clone", ROM_GENXT, "genxt", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 32, 704, 16, xt_init, NULL};
MODEL m_6 = {"[8088] IBM PC", ROM_IBMPC, "ibmpc", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 32, xt_init, NULL};
MODEL m_7 = {"[8088] IBM PCjr", ROM_IBMPCJR, "ibmpcjr", {{"", cpus_pcjr}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 128, 640, 64, pcjr_init, &pcjr_device};
MODEL m_8 = {"[8088] IBM XT", ROM_IBMXT, "ibmxt", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64, xt_init, NULL};
MODEL m_9 = {"[8088] Juko XT clone", ROM_JUKOPC, "jukopc", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64, xt_init, NULL};
MODEL m_10 = {"[8088] Leading Edge Model M", ROM_LEDGE_MODELM, "ledge_modelm", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 128, 704, 64, xt_init, NULL};
MODEL m_11 = {"[8088] NCR PC4i", ROM_NCR_PC4I, "ncr_pc4i", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 256, 640, 64, xt_init, NULL};
MODEL m_12 = {"[8088] Phoenix XT clone", ROM_PXXT, "pxxt", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64, xt_init, NULL};
MODEL m_13 = {"[8088] Schneider EuroPC", ROM_EUROPC, "europc", {{"", cpus_europc}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 512, 640, 128, europc_init, NULL};
MODEL m_14 = {"[8088] Tandy 1000", ROM_TANDY, "tandy", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 128, 640, 128, tandy1k_init, &tandy1000_device};
MODEL m_15 = {"[8088] Tandy 1000 HX", ROM_TANDY1000HX, "tandy1000hx", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 256, 640, 128, tandy1k_init, &tandy1000hx_device};
MODEL m_16 = {"[8088] Thomson TO16 PC", ROM_TO16_PC, "to16_pc", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 512, 640, 128, xt_init, NULL};
MODEL m_17 = {"[8088] Toshiba T1000", ROM_T1000, "t1000", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 512, 1280, 768, xt_t1000_init, NULL};
MODEL m_18 = {"[8088] VTech Laser Turbo XT", ROM_LTXT, "ltxt", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 1152, 64, xt_laserxt_init, NULL};
MODEL m_19 = {"[8088] Xi8088", ROM_XI8088, "xi8088", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PS2, 64, 1024, 128, xt_xi8088_init, &xi8088_device};
MODEL m_20 = {"[8088] Zenith Data SupersPort", ROM_ZD_SUPERS, "zdsupers", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 128, 640, 128, xt_zenith_init, NULL};

MODEL m_21 = {"[8086] Amstrad PC1512", ROM_PC1512, "pc1512", {{"", cpus_pc1512}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED | MODEL_AMSTRAD, 512, 640, 128, ams_init, &ams1512_device};
MODEL m_22 = {"[8086] Amstrad PC1640", ROM_PC1640, "pc1640", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD, 640, 640, 0, ams_init, &ams1512_device};
MODEL m_23 = {"[8086] Amstrad PC2086", ROM_PC2086, "pc2086", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD, 640, 640, 0, ams_init, &ams2086_device};
MODEL m_24 = {"[8086] Amstrad PC3086", ROM_PC3086, "pc3086", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD, 640, 640, 0, ams_init, &ams3086_device};
MODEL m_25 = {"[8086] Amstrad PC5086", ROM_PC5086, "pc5086", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_PS2, 640, 640, 0, pc5086_init, &f82c710_upc_device};
MODEL m_26 = {"[8086] Amstrad PPC512/640", ROM_PPC512, "ppc512", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD, 512, 640, 128, ams_init, &ams1512_device};
MODEL m_27 = {"[8086] Compaq Deskpro", ROM_DESKPRO, "deskpro", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 128, 640, 128, compaq_xt_init, NULL};
MODEL m_28 = {"[8086] Olivetti M24", ROM_OLIM24, "olivetti_m24", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED | MODEL_OLIM24, 128, 640, 128, olim24_init, NULL};
MODEL m_29 = {"[8086] Sinclair PC200", ROM_PC200, "pc200", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD, 512, 640, 128, ams_init, &ams1512_device};
MODEL m_30 = {"[8086] Tandy 1000 SL/2", ROM_TANDY1000SL2, "tandy1000sl2", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 512, 768, 128, tandy1ksl2_init, NULL};
MODEL m_31 = {"[8088] Toshiba T1200", ROM_T1200, "t1200", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 1024, 2048, 1024, xt_t1200_init, NULL};
MODEL m_32 = {"[8086] VTech Laser XT3", ROM_LXT3, "lxt3", {{"", cpus_8086}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 512, 1152, 128, xt_laserxt_init, NULL};

MODEL m_33 = {"[286] AMI 286 clone", ROM_AMI286, "ami286", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 512, 16384, 128, at_neat_init, NULL};
MODEL m_34 = {"[286] Award 286 clone", ROM_AWARD286, "award286", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 512, 16384, 128, at_scat_init, NULL};
MODEL m_35 = {"[286] Bull Micral 45", ROM_BULL_MICRAL_45, "bull_micral_45", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1024, 6144, 128, ibm_at_init, NULL};
MODEL m_36 = {"[286] Commodore PC 30 III", ROM_CMDPC30, "cmdpc30", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 640, 16384, 128, at_cbm_init, NULL};
MODEL m_37 = {"[286] Compaq Portable II", ROM_COMPAQ_PII, "compaq_pii", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 256, 15872, 128, ibm_at_init, NULL};
MODEL m_38 = {"[286] DELL System 200", ROM_DELL200, "dells200", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT, 640, 16384, 128, dells200_init, NULL};
MODEL m_39 = {"[286] Epson PC AX", ROM_EPSON_PCAX, "epson_pcax", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT, 256, 15872, 128, at_init, NULL};
MODEL m_40 = {"[286] Epson PC AX2e", ROM_EPSON_PCAX2E, "epson_pcax2e", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PS2, 256, 15872, 128, at_init, NULL};
MODEL m_41 = {"[286] Goldstar GDC-212M", ROM_GDC212M, "gdc212m", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 512, 4096, 512, at_scat_init, NULL};
MODEL m_42 = {"[286] GW-286CT GEAR", ROM_GW286CT, "gw286ct", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT, 512, 16384, 128, at_scat_init, NULL};
MODEL m_43 = {"[286] Hyundai Super-286TR", ROM_HYUNDAI_SUPER286TR, "super286tr", {{"AMD", cpus_super286tr}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1024, 4096, 128, at_scat_init, &f82c710_upc_device};
MODEL m_44 = {"[286] IBM AT", ROM_IBMAT, "ibmat", {{"", cpus_ibmat}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT, 256, 15872, 128, ibm_at_init, NULL};
MODEL m_45 = {"[286] IBM PS/1 model 2011", ROM_IBMPS1_2011, "ibmps1es", {{"", cpus_ps1_m2011}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2, 512, 16384, 512, ps1_m2011_init, NULL};
MODEL m_46 = {"[286] IBM PS/2 Model 30-286", ROM_IBMPS2_M30_286, "ibmps2_m30_286", {{"", cpus_ps2_m30_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2, 1, 16, 1, ps2_m30_286_init, NULL};
MODEL m_47 = {"[286] IBM PS/2 Model 50", ROM_IBMPS2_M50, "ibmps2_m50", {{"", cpus_ps2_m30_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA, 1, 16, 1, ps2_model_50_init, NULL};
MODEL m_48 = {"[286] IBM XT Model 286", ROM_IBMXT286, "ibmxt286", {{"", cpus_ibmxt286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT, 256, 15872, 128, ibm_at_init, NULL};
MODEL m_49 = {"[286] Samsung SPC-4200P", ROM_SPC4200P, "spc4200p", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 512, 2048, 128, at_scat_init, NULL};
MODEL m_50 = {"[286] Samsung SPC-4216P", ROM_SPC4216P, "spc4216p", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 5, 1, at_scat_init, NULL};
MODEL m_51 = {"[286] Samsung SPC-4620P", ROM_SPC4620P, "spc4620p", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_HW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 5, 1, at_scat_init, NULL};
MODEL m_52 = {"[286] Toshiba T3100e", ROM_T3100E, "t3100e", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_HAS_IDE, 1024, 5120, 256, at_t3100e_init, NULL};
MODEL m_53 = {"[286] Trigem 286M", ROM_TG286M, "tg286m", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 512, 8192, 128, at_headland_init, NULL};
MODEL m_54 = {"[286] Tulip AT Compact", ROM_TULIP_TC7, "tulip_tc7", {{"", cpus_286}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 640, 15872, 128, ibm_at_init, NULL};

MODEL m_55 = {"[386SX] Acer 386SX25/N", ROM_ACER386, "acer386", {{"Intel", cpus_acer}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 16, 1, at_acer386sx_init, NULL};
MODEL m_56 = {"[386SX] AMA-932J", ROM_AMA932J, "ama932j", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_HAS_IDE, 512, 8192, 128, at_headland_init, NULL};
MODEL m_57 = {"[386SX] AMI 386SX clone", ROM_AMI386SX, "ami386", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 512, 16384, 128, at_headland_init, NULL};
MODEL m_58 = {"[386SX] Amstrad MegaPC", ROM_MEGAPC, "megapc", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_DISABLE_HW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 16, 1, at_wd76c10_init, NULL};
MODEL m_59 = {"[386SX] Commodore SL386SX-25", ROM_CBM_SL386SX25, "cbm_sl386sx25", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1024, 16384, 512, at_scamp_init, NULL};
MODEL m_60 = {"[386SX] DTK 386SX clone", ROM_DTK386, "dtk386", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 512, 16384, 128, at_neat_init, NULL};
MODEL m_61 = {"[386SX] Epson PC AX3", ROM_EPSON_PCAX3, "epson_pcax3", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_NONE | MODEL_AT, 256, 15872, 128, at_init, NULL};
MODEL m_62 = {"[386SX] IBM PS/1 model 2121", ROM_IBMPS1_2121, "ibmps1_2121", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 16, 1, ps1_m2121_init, NULL};
MODEL m_63 = {"[386SX] IBM PS/2 Model 55SX", ROM_IBMPS2_M55SX, "ibmps2_m55sx", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA, 1, 8, 1, ps2_model_55sx_init, NULL};
MODEL m_64 = {"[386SX] KMX-C-02", ROM_KMXC02, "kmxc02", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_NONE | MODEL_AT, 512, 16384, 512, at_scatsx_init, NULL};
MODEL m_65 = {"[386SX] Packard Bell Legend 300SX", ROM_PB_L300SX, "pb_l300sx", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 16, 1, pb_l300sx_init, NULL};
MODEL m_66 = {"[386SX] Samsung SPC-6033P", ROM_SPC6033P, "spc6033p", {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}}, MODEL_GFX_DISABLE_HW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 2, 12, 2, at_scamp_init, NULL};

MODEL m_67 = {"[386DX] AMI 386DX clone", ROM_AMI386DX_OPTI495, "ami386dx", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1, 256, 1, at_opti495_init, NULL};
MODEL m_68 = {"[386DX] Compaq Deskpro 386", ROM_DESKPRO_386, "deskpro386", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_NONE | MODEL_AT, 1, 15, 1, deskpro386_init, NULL};
MODEL m_69 = {"[386DX] ECS 386/32", ROM_ECS_386_32, "ecs_386_32", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_NONE | MODEL_AT, 1, 16, 1, at_cs8230_init, NULL};
MODEL m_70 = {"[386DX] IBM PS/2 Model 70 (type 3)", ROM_IBMPS2_M70_TYPE3, "ibmps2_m70_type3", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA, 2, 16, 2, ps2_model_70_init, NULL};
MODEL m_71 = {"[386DX] IBM PS/2 Model 80", ROM_IBMPS2_M80, "ibmps2_m80", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA, 1, 12, 1, ps2_model_80_init, NULL};
MODEL m_72 = {"[386DX] MR 386DX clone", ROM_MR386DX_OPTI495, "mr386dx", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1, 256, 1, at_opti495_init, NULL};
MODEL m_73 = {"[386DX] Samsung SPC-6000A", ROM_SPC6000A, "spc6000a", {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1, 32, 1, at_cs8230_init, NULL};

MODEL m_74 = {"[486] AMI 486 clone", ROM_AMI486, "ami486", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1, 256, 1, at_ali1429_init, NULL};
MODEL m_75 = {"[486] AMI WinBIOS 486", ROM_WIN486, "win486", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE, 1, 256, 1, at_ali1429_init, NULL};
MODEL m_76 = {"[486] Award SiS 496/497", ROM_SIS496, "sis496", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_HAS_IDE, 1, 256, 1, at_sis496_init, NULL};
MODEL m_77 = {"[486] Elonex PC-425X", ROM_ELX_PC425X, "elx_pc425x", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_HAS_IDE, 1, 256, 1, at_sl82c460_init, NULL};
MODEL m_78 = {"[486] IBM PS/1 Model 2133 (EMEA 451)", ROM_IBMPS1_2133_451, "ibmps1_2133", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2, 2, 64, 2, ps1_m2133_init, NULL};
MODEL m_79 = {"[486] IBM PS/2 Model 70 (type 4)", ROM_IBMPS2_M70_TYPE4, "ibmps2_m70_type4", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA, 2, 16, 2, ps2_model_70_init, NULL};
MODEL m_80 = {"[486] Packard Bell PB410A", ROM_PB410A, "pb410a", {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE, 1, 64, 1, at_pb410a_init, NULL};

MODEL m_81 = {"[Socket 4] Intel Premiere/PCI", ROM_REVENGE, "revenge", {{"Intel", cpus_Pentium5V}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 1, 128, 1, at_batman_init, NULL};
MODEL m_82 = {"[Socket 4] Packard Bell PB520R", ROM_PB520R, "pb520r", {{"Intel", cpus_Pentium5V}, {"", NULL}, {"", NULL}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 1, 128, 1, at_pb520r_init, NULL};

MODEL m_83 = {"[Socket 5] Intel Advanced/EV", ROM_ENDEAVOR, "endeavor", {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 1, 128, 1, at_endeavor_init, NULL};
MODEL m_84 = {"[Socket 5] Intel Advanced/ZP", ROM_ZAPPA, "zappa", {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 1, 128, 1, at_zappa_init, NULL};
MODEL m_85 = {"[Socket 5] Itautec Infoway Multimidia", ROM_ITAUTEC_INFOWAYM, "infowaym", {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 128, 1, at_zappa_init, NULL};
MODEL m_86 = {"[Socket 5] Packard Bell PB570", ROM_PB570, "pb570", {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}}, MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 1, 128, 1, at_pb570_init, NULL};

MODEL m_87 = {"[Socket 7] ASUS P/I-P55TVP4", ROM_P55TVP4, "p55tvp4", {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 128, 1, at_p55tvp4_init, NULL};
MODEL m_88 = {"[Socket 7] ASUS P/I-P55T2P4", ROM_P55T2P4, "p55t2p4", {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 256, 1, at_p55t2p4_init, NULL};
MODEL m_89 = {"[Socket 7] Epox P55-VA", ROM_P55VA, "p55va", {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 128, 1, at_p55va_init, NULL};
MODEL m_90 = {"[Socket 7] Shuttle HOT-557", ROM_430VX, "430vx", {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 128, 1, at_i430vx_init, NULL};

MODEL m_91 = {"[Super 7] FIC VA-503+", ROM_FIC_VA503P, "fic_va503p", {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_SS7}, {"IDT", cpus_WinChip_SS7}, {"Cyrix", cpus_6x86_SS7}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 1, 512, 1, at_mvp3_init, NULL};

MODEL m_92 = {"[Socket 8] Intel VS440FX", ROM_VS440FX, "vs440fx", {{"Intel", cpus_PentiumPro}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 256, 8, at_vs440fx_init, NULL};

MODEL m_93 = {"[Slot 1] Gigabyte GA-686BX", ROM_GA686BX, "ga686bx", {{"Intel", cpus_Slot1_100MHz}, {"VIA", cpus_VIA_100MHz}}, MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE, 8, 512, 8, at_ga686bx_init, NULL};

void model_init_builtin()
{
        memset(models, 0, sizeof(models));

        models[0] = &m_1;
models[1] = &m_2;
models[2] = &m_3;
models[3] = &m_4;
models[4] = &m_5;
models[5] = &m_6;
models[6] = &m_7;
models[7] = &m_8;
models[8] = &m_9;
models[9] = &m_10;
models[10] = &m_11;
models[11] = &m_12;
models[12] = &m_13;
models[13] = &m_14;
models[14] = &m_15;
models[15] = &m_16;
models[16] = &m_17;
models[17] = &m_18;
models[18] = &m_19;
models[19] = &m_20;
models[20] = &m_21;
models[21] = &m_22;
models[22] = &m_23;
models[23] = &m_24;
models[24] = &m_25;
models[25] = &m_26;
models[26] = &m_27;
models[27] = &m_28;
models[28] = &m_29;
models[29] = &m_30;
models[30] = &m_31;
models[31] = &m_32;
models[32] = &m_33;
models[33] = &m_34;
models[34] = &m_35;
models[35] = &m_36;
models[36] = &m_37;
models[37] = &m_38;
models[38] = &m_39;
models[39] = &m_40;
models[40] = &m_41;
models[41] = &m_42;
models[42] = &m_43;
models[43] = &m_44;
models[44] = &m_45;
models[45] = &m_46;
models[46] = &m_47;
models[47] = &m_48;
models[48] = &m_49;
models[49] = &m_50;
models[50] = &m_51;
models[51] = &m_52;
models[52] = &m_53;
models[53] = &m_54;
models[54] = &m_55;
models[55] = &m_56;
models[56] = &m_57;
models[57] = &m_58;
models[58] = &m_59;
models[59] = &m_60;
models[60] = &m_61;
models[61] = &m_62;
models[62] = &m_63;
models[63] = &m_64;
models[64] = &m_65;
models[65] = &m_66;
models[66] = &m_67;
models[67] = &m_68;
models[68] = &m_69;
models[69] = &m_70;
models[70] = &m_71;
models[71] = &m_72;
models[72] = &m_73;
models[73] = &m_74;
models[74] = &m_75;
models[75] = &m_76;
models[76] = &m_77;
models[77] = &m_78;
models[78] = &m_79;
models[79] = &m_80;
models[80] = &m_81;
models[81] = &m_82;
models[82] = &m_83;
models[83] = &m_84;
models[84] = &m_85;
models[85] = &m_86;
models[86] = &m_87;
models[87] = &m_88;
models[88] = &m_89;
models[89] = &m_90;
models[90] = &m_91;
models[91] = &m_92;
models[92] = &m_93;
}