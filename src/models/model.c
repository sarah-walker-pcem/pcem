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
#include "grid.h"
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

#include <pcem/devices.h>

void xt_init();
void pcjr_init();
void tandy1k_init();
void tandy1ksl2_init();
void ams_init();
void pc5086_init();
void europc_init();
void olim24_init();
void at_init();
void ibm_at_init();
void at_cbm_init();
void dells200_init();
void deskpro386_init();
void pb_l300sx_init();
void ps1_m2011_init();
void ps1_m2121_init();
void ps1_m2133_init(void);
void ps2_m30_286_init();
void ps2_model_50_init();
void ps2_model_55sx_init();
void ps2_model_70_init();
void ps2_model_80_init();
void at_neat_init();
void at_scamp_init();
void at_scat_init();
void at_scatsx_init();
void at_acer386sx_init();
void at_wd76c10_init();
void at_cs8230_init();
void at_ali1429_init();
void at_headland_init();
void at_grid1520_init();
void at_opti495_init();
void at_sis496_init();
void at_p55va_init();
void at_p55tvp4_init();
void at_i430vx_init();
void at_batman_init();
void at_endeavor_init();
void at_p55t2p4_init();
void xt_laserxt_init();
void at_t3100e_init();
void xt_t1000_init();
void xt_t1200_init();
void at_sl82c460_init();
void at_zappa_init();
void at_pb410a_init();
void at_pb520r_init();
void at_pb570_init();
void compaq_xt_init();
void xt_xi8088_init();
void xt_zenith_init();
void at_mvp3_init();
void at_vs440fx_init();
void at_ga686bx_init();

int AMSTRAD, AT, PCI, TANDY, MCA;

int model_getromset() { return models[model]->id; }

int model_getromset_from_model(int model) { return models[model]->id; }

int model_getmodel(int romset) {
        int c = 0;

        while (models[c]->id != -1) {
                if (models[c]->id == romset)
                        return c;
                c++;
        }

        return 0;
}

char *model_getname() { return models[model]->name; }

char *model_get_internal_name() { return models[model]->internal_name; }

int model_get_model_from_internal_name(char *s) {
        int c = 0;

        while (models[c] != NULL) {
                if (!strcmp(models[c]->internal_name, s))
                        return c;
                c++;
        }

        return 0;
}

int model_has_fixed_gfx(int model) {
        int gfx_flags = models[model]->flags & MODEL_GFX_MASK;

        return (gfx_flags == MODEL_GFX_FIXED);
}

int model_has_optional_gfx(int model) {
        int gfx_flags = models[model]->flags & MODEL_GFX_MASK;

        return (gfx_flags == MODEL_GFX_DISABLE_HW || gfx_flags == MODEL_GFX_DISABLE_SW);
}

void common_init() {
        dma_init();
        fdc_add();
        lpt_init();
        pic_init();
        pit_init();
        serial1_init(0x3f8, 4, 1);
        serial2_init(0x2f8, 3, 1);
}

void xt_init() {
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_xt);
        keyboard_xt_init();
        nmi_init();
        device_add(&gameport_device);
        if (romset == ROM_IBMPC)
                device_add(&cassette_device);
}

void compaq_xt_init() {
        xt_init();
        lpt1_remove();
        lpt2_remove();
        lpt1_init(0x3bc);
}

void pcjr_init() {
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

void tandy1k_init() {
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
void tandy1ksl2_init() {
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

void ams_init() {
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

void pc5086_init() {
        xt_init();
        lpt1_remove(); /* remove LPT ports, they will be enabled by 82C710 */
        lpt2_remove();
        serial1_remove(); /* remove COM ports, they will be enabled by 82C710 */
        serial2_remove();
        device_add(&nvr_device);
        fdc_set_dskchg_activelow();
        superxt_init();
}

void europc_init() {
        common_init();
        mem_add_bios();
        jim_init();
        keyboard_xt_init();
        nmi_init();
        device_add(&gameport_device);
}

void olim24_init() {
        common_init();
        mem_add_bios();
        keyboard_olim24_init();
        device_add(&nvr_device);
        olivetti_m24_init();
        nmi_init();
        device_add(&gameport_device);
}

void xt_laserxt_init() {
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

void xt_xi8088_init() {
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

void at_init() {
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

void ibm_at_init() {
        at_init();
        mem_remap_top_384k();
}

void at_cbm_init() {
        at_init();
        cbm_io_init();
}

void deskpro386_init() {
        at_init();
        compaq_init();
}

void dells200_init() {
        at_init();
        dells200_chipset_init();
}

void ps1_common_init() {
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

void ps1_m2011_init() {
        ps1_common_init();
        ps1mb_init();
        mem_remap_top_384k();
}

void ps1_m2121_init() {
        ps1_common_init();
        ps1mb_m2121_init();
        fdc_set_ps1();
}

void ps1_m2133_init(void) {
        at_init();
        vl82c480_init();
        ps1mb_m2133_init();
}

void ps2_m30_286_init() {
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

static void ps2_common_init() {
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

void ps2_model_50_init() {
        ps2_common_init();
        ps2_mca_board_model_50_init();
}

void ps2_model_55sx_init() {
        ps2_common_init();
        ps2_mca_board_model_55sx_init();
}

void ps2_model_70_init() {
        ps2_common_init();
        ps2_mca_board_model_70_type34_init(romset == ROM_IBMPS2_M70_TYPE4);
}

void ps2_model_80_init() {
        ps2_common_init();
        ps2_mca_board_model_80_type2_init();
}

void at_neat_init() {
        at_init();
        neat_init();
}

void at_scamp_init() {
        at_init();
        scamp_init();
}

void at_scat_init() {
        at_init();
        scat_init();
}

void at_scatsx_init() {
        at_init();
        scatsx_init();
}

void at_acer386sx_init() {
        at_init();
        acer386sx_init();
}

void at_wd76c10_init() {
        at_init();
        wd76c10_init();
}

void at_headland_init() {
        at_init();
        headland_init();
}

void at_grid1520_init() {
        at_init();
        grid_init();
}

void at_opti495_init() {
        at_init();
        opti495_init();
}

void at_cs8230_init() {
        at_init();
        cs8230_init();
}

void at_ali1429_init() {
        at_init();
        ali1429_init();
}

void pb_l300sx_init() {
        at_init();
        acc2036_init();
}

void at_pb410a_init() {
        at_init();
        acc2168_init();
        acc3221_init();
}

void at_sis496_init() {
        at_init();
        pci_init(PCI_CONFIG_TYPE_1);
        pci_slot(0xb);
        pci_slot(0xd);
        pci_slot(0xf);
        device_add(&sis496_device);
}

void at_sl82c460_init() {
        at_init();
        sl82c460_init();
}

void at_batman_init() {
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
void at_pb520r_init() {
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
void at_endeavor_init() {
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
void at_pb570_init() {
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
void at_zappa_init() {
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

void at_p55va_init() {
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

void at_p55tvp4_init() {
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

void at_i430vx_init() {
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

void at_p55t2p4_init() {
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

void at_mvp3_init() {
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

void at_vs440fx_init() {
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

void at_ga686bx_init() {
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

void model_init() {
        pclog("Initting as %s\n", model_getname());
        AMSTRAD = AT = PCI = TANDY = MCA = 0;
        ide_set_bus_master(NULL, NULL, NULL, NULL);

        models[model]->init();
        if (models[model]->device)
                device_add(models[model]->device);
}

/* 8088 PC's */
MODEL m_amixt = {"[8088] AMI XT clone",
                 ROM_AMIXT,
                 "amixt",
                 {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                 MODEL_GFX_NONE,
                 64,
                 640,
                 64,
                 xt_init,
                 NULL};
MODEL m_ataripc3 = {"[8088] Atari PC3",
                    ROM_ATARIPC3,
                    "ataripc3",
                    {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE,
                    64,
                    640,
                    64,
                    xt_init,
                    NULL};
MODEL m_cbm_pc10 = {"[8088] Commodore PC-10",
                    ROM_CBM_PC10,
                    "cbm_pc10",
                    {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE,
                    640,
                    640,
                    64,
                    xt_init,
                    NULL};
MODEL m_compaq_pip = {"[8088] Compaq Portable Plus",
                      ROM_COMPAQ_PIP,
                      "compaq_pip",
                      {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                      MODEL_GFX_NONE,
                      128,
                      640,
                      64,
                      compaq_xt_init,
                      NULL};
MODEL m_dtk = {"[8088] DTK XT clone",
               ROM_DTKXT,
               "dtk",
               {{"", cpus_8088}, {"", NULL}, {"", NULL}},
               MODEL_GFX_NONE,
               64,
               640,
               64,
               xt_init,
               NULL};
MODEL m_genxt = {"[8088] Generic XT clone",
                 ROM_GENXT,
                 "genxt",
                 {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                 MODEL_GFX_NONE,
                 32,
                 704,
                 16,
                 xt_init,
                 NULL};
MODEL m_super16t = {"[8088] Hyundai Super16T",
                     ROM_HYUNDAI_SUPER16T,
                     "super16t",
                     {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                     MODEL_GFX_NONE,
                     640,
                     640,
                     64,
                     xt_init,
                     NULL};
MODEL m_super16te = {"[8088] Hyundai Super16TE",
                      ROM_HYUNDAI_SUPER16TE,
                      "super16te",
                      {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                      MODEL_GFX_NONE,
                      640,
                      640,
                      64,
                      xt_init,
                      NULL};
MODEL m_ibmpc = {"[8088] IBM PC", ROM_IBMPC, "ibmpc", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 32,
                 xt_init,         NULL};
MODEL m_ibmpcjr = {"[8088] IBM PCjr", ROM_IBMPCJR, "ibmpcjr", {{"", cpus_pcjr}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_FIXED,   128,         640,       64,
                   pcjr_init,         &pcjr_device};
MODEL m_ibmxt = {"[8088] IBM XT", ROM_IBMXT, "ibmxt", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_NONE, 64, 640, 64,
                 xt_init,         NULL};
MODEL m_jukopc = {"[8088] Juko XT clone",
                  ROM_JUKOPC,
                  "jukopc",
                  {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_NONE,
                  64,
                  640,
                  64,
                  xt_init,
                  NULL};
MODEL m_ledge_modelm = {"[8088] Leading Edge Model M",
                        ROM_LEDGE_MODELM,
                        "ledge_modelm",
                        {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                        MODEL_GFX_NONE,
                        128,
                        704,
                        64,
                        xt_init,
                        NULL};
MODEL m_ncr_pc4i = {"[8088] NCR PC4i", ROM_NCR_PC4I, "ncr_pc4i", {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE,    256,          640,        64,
                    xt_init,           NULL};
MODEL m_pxxt = {"[8088] Phoenix XT clone",
                ROM_PXXT,
                "pxxt",
                {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                MODEL_GFX_NONE,
                64,
                640,
                64,
                xt_init,
                NULL};
MODEL m_europc = {"[8088] Schneider EuroPC",
                  ROM_EUROPC,
                  "europc",
                  {{"", cpus_europc}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_NONE,
                  512,
                  640,
                  128,
                  europc_init,
                  NULL};
MODEL m_tandy = {
        "[8088] Tandy 1000", ROM_TANDY,        "tandy", {{"", cpus_8088}, {"", NULL}, {"", NULL}}, MODEL_GFX_FIXED, 128, 640, 128,
        tandy1k_init,        &tandy1000_device};
MODEL m_tandy1000hx = {"[8088] Tandy 1000 HX",
                       ROM_TANDY1000HX,
                       "tandy1000hx",
                       {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                       MODEL_GFX_FIXED,
                       256,
                       640,
                       128,
                       tandy1k_init,
                       &tandy1000hx_device};
MODEL m_to16_pc = {"[8088] Thomson TO16 PC",
                   ROM_TO16_PC,
                   "to16_pc",
                   {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_NONE,
                   512,
                   640,
                   128,
                   xt_init,
                   NULL};
MODEL m_t1000 = {"[8088] Toshiba T1000", ROM_T1000, "t1000", {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                 MODEL_GFX_FIXED,        512,       1280,    768,
                 xt_t1000_init,          NULL};
MODEL m_ltxt = {"[8088] VTech Laser Turbo XT",
                ROM_LTXT,
                "ltxt",
                {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                MODEL_GFX_NONE,
                64,
                1152,
                64,
                xt_laserxt_init,
                NULL};
MODEL m_xi8088 = {"[8088] Xi8088",
                  ROM_XI8088,
                  "xi8088",
                  {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_PS2,
                  64,
                  1024,
                  128,
                  xt_xi8088_init,
                  &xi8088_device};
MODEL m_zdsupers = {"[8088] Zenith Data SupersPort",
                    ROM_ZD_SUPERS,
                    "zdsupers",
                    {{"", cpus_8088}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE,
                    128,
                    640,
                    128,
                    xt_zenith_init,
                    NULL};

/* 8086 PC's */
MODEL m_pc1512 = {"[8086] Amstrad PC1512",
                  ROM_PC1512,
                  "pc1512",
                  {{"", cpus_pc1512}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_FIXED | MODEL_AMSTRAD,
                  512,
                  640,
                  128,
                  ams_init,
                  &ams1512_device};
MODEL m_pc1640 = {"[8086] Amstrad PC1640",
                  ROM_PC1640,
                  "pc1640",
                  {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD,
                  640,
                  640,
                  0,
                  ams_init,
                  &ams1512_device};
MODEL m_pc2086 = {"[8086] Amstrad PC2086",
                  ROM_PC2086,
                  "pc2086",
                  {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD,
                  640,
                  640,
                  0,
                  ams_init,
                  &ams2086_device};
MODEL m_pc3086 = {"[8086] Amstrad PC3086",
                  ROM_PC3086,
                  "pc3086",
                  {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD,
                  640,
                  640,
                  0,
                  ams_init,
                  &ams3086_device};
MODEL m_pc5086 = {"[8086] Amstrad PC5086",
                  ROM_PC5086,
                  "pc5086",
                  {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_NONE | MODEL_PS2,
                  640,
                  640,
                  0,
                  pc5086_init,
                  &f82c710_upc_device};
MODEL m_ppc512 = {"[8086] Amstrad PPC512/640",
                  ROM_PPC512,
                  "ppc512",
                  {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD,
                  512,
                  640,
                  128,
                  ams_init,
                  &ams1512_device};
MODEL m_deskpro = {"[8086] Compaq Deskpro", ROM_DESKPRO, "deskpro", {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_NONE,          128,         640,       128,
                   compaq_xt_init,          NULL};
MODEL m_olivetti_m24 = {"[8086] Olivetti M24",
                        ROM_OLIM24,
                        "olivetti_m24",
                        {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                        MODEL_GFX_FIXED | MODEL_OLIM24,
                        128,
                        640,
                        128,
                        olim24_init,
                        NULL};
MODEL m_pc200 = {"[8086] Sinclair PC200",
                 ROM_PC200,
                 "pc200",
                 {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                 MODEL_GFX_DISABLE_HW | MODEL_AMSTRAD,
                 512,
                 640,
                 128,
                 ams_init,
                 &ams1512_device};
MODEL m_tandy1000sl2 = {"[8086] Tandy 1000 SL/2",
                        ROM_TANDY1000SL2,
                        "tandy1000sl2",
                        {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                        MODEL_GFX_FIXED,
                        512,
                        768,
                        128,
                        tandy1ksl2_init,
                        NULL};
MODEL m_t1200 = {"[8088] Toshiba T1200", ROM_T1200, "t1200", {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                 MODEL_GFX_FIXED,        1024,      2048,    1024,
                 xt_t1200_init,          NULL};
MODEL m_lxt3 = {"[8086] VTech Laser XT3", ROM_LXT3, "lxt3", {{"", cpus_8086}, {"", NULL}, {"", NULL}},
                MODEL_GFX_NONE,           512,      1152,   128,
                xt_laserxt_init,          NULL};

/* 286 PC's */
MODEL m_ami286 = {"[286] AMI 286 clone",
                  ROM_AMI286,
                  "ami286",
                  {{"", cpus_286}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                  512,
                  16384,
                  128,
                  at_neat_init,
                  NULL};
MODEL m_award286 = {"[286] Award 286 clone",
                    ROM_AWARD286,
                    "award286",
                    {{"", cpus_286}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                    512,
                    16384,
                    128,
                    at_scat_init,
                    NULL};
MODEL m_bull_micral_45 = {"[286] Bull Micral 45",
                          ROM_BULL_MICRAL_45,
                          "bull_micral_45",
                          {{"", cpus_286}, {"", NULL}, {"", NULL}},
                          MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                          1024,
                          6144,
                          128,
                          ibm_at_init,
                          NULL};
MODEL m_cmdpc30 = {"[286] Commodore PC 30 III",
                   ROM_CMDPC30,
                   "cmdpc30",
                   {{"", cpus_286}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                   640,
                   16384,
                   128,
                   at_cbm_init,
                   NULL};
MODEL m_compaq_pii = {"[286] Compaq Portable II",
                      ROM_COMPAQ_PII,
                      "compaq_pii",
                      {{"", cpus_286}, {"", NULL}, {"", NULL}},
                      MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                      256,
                      15872,
                      128,
                      ibm_at_init,
                      NULL};
MODEL m_dells200 = {"[286] DELL System 200",
                    ROM_DELL200,
                    "dells200",
                    {{"", cpus_286}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT,
                    640,
                    16384,
                    128,
                    dells200_init,
                    NULL};
MODEL m_epson_pcax = {"[286] Epson PC AX",
                      ROM_EPSON_PCAX,
                      "epson_pcax",
                      {{"", cpus_286}, {"", NULL}, {"", NULL}},
                      MODEL_GFX_NONE | MODEL_AT,
                      256,
                      15872,
                      128,
                      at_init,
                      NULL};
MODEL m_epson_pcax2e = {"[286] Epson PC AX2e",
                        ROM_EPSON_PCAX2E,
                        "epson_pcax2e",
                        {{"", cpus_286}, {"", NULL}, {"", NULL}},
                        MODEL_GFX_NONE | MODEL_AT | MODEL_PS2,
                        256,
                        15872,
                        128,
                        at_init,
                        NULL};
MODEL m_gdc212m = {"[286] Goldstar GDC-212M",
                   ROM_GDC212M,
                   "gdc212m",
                   {{"", cpus_286}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                   512,
                   4096,
                   512,
                   at_scat_init,
                   NULL};
MODEL m_grid1520 = {"[286] GRiD GRiDcase 1520",
                   ROM_GRID1520,
                   "grid1520",
                   {{"", cpus_286}, {"", NULL}, {"", NULL}},
                   MODEL_AT | MODEL_HAS_IDE,
                   1024,
                   8192,
                   1024,
                   at_grid1520_init,
                   NULL};
MODEL m_gw286ct = {"[286] GW-286CT GEAR",
                   ROM_GW286CT,
                   "gw286ct",
                   {{"", cpus_286}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_NONE | MODEL_AT,
                   512,
                   16384,
                   128,
                   at_scat_init,
                   NULL};
MODEL m_super286tr = {"[286] Hyundai Super-286TR",
                      ROM_HYUNDAI_SUPER286TR,
                      "super286tr",
                      {{"AMD", cpus_super286tr}, {"", NULL}, {"", NULL}},
                      MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                      1024,
                      4096,
                      128,
                      at_scat_init,
                      &f82c710_upc_device};
MODEL m_ibmat = {"[286] IBM AT",
                 ROM_IBMAT,
                 "ibmat",
                 {{"", cpus_ibmat}, {"", NULL}, {"", NULL}},
                 MODEL_GFX_NONE | MODEL_AT,
                 256,
                 15872,
                 128,
                 ibm_at_init,
                 NULL};
MODEL m_ibmps1es = {"[286] IBM PS/1 model 2011",
                    ROM_IBMPS1_2011,
                    "ibmps1es",
                    {{"", cpus_ps1_m2011}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2,
                    512,
                    16384,
                    512,
                    ps1_m2011_init,
                    NULL};
MODEL m_ibmps2_m30_286 = {"[286] IBM PS/2 Model 30-286",
                          ROM_IBMPS2_M30_286,
                          "ibmps2_m30_286",
                          {{"", cpus_ps2_m30_286}, {"", NULL}, {"", NULL}},
                          MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2,
                          1,
                          16,
                          1,
                          ps2_m30_286_init,
                          NULL};
MODEL m_ibmps2_m50 = {"[286] IBM PS/2 Model 50",
                      ROM_IBMPS2_M50,
                      "ibmps2_m50",
                      {{"", cpus_ps2_m30_286}, {"", NULL}, {"", NULL}},
                      MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA,
                      1,
                      16,
                      1,
                      ps2_model_50_init,
                      NULL};
MODEL m_ibmxt286 = {"[286] IBM XT Model 286",
                    ROM_IBMXT286,
                    "ibmxt286",
                    {{"", cpus_ibmxt286}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT,
                    256,
                    15872,
                    128,
                    ibm_at_init,
                    NULL};
MODEL m_spc4200p = {"[286] Samsung SPC-4200P",
                    ROM_SPC4200P,
                    "spc4200p",
                    {{"", cpus_286}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                    512,
                    2048,
                    128,
                    at_scat_init,
                    NULL};
MODEL m_spc4216p = {"[286] Samsung SPC-4216P",
                    ROM_SPC4216P,
                    "spc4216p",
                    {{"", cpus_286}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                    1,
                    5,
                    1,
                    at_scat_init,
                    NULL};
MODEL m_spc4620p = {"[286] Samsung SPC-4620P",
                    ROM_SPC4620P,
                    "spc4620p",
                    {{"", cpus_286}, {"", NULL}, {"", NULL}},
                    MODEL_GFX_DISABLE_HW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                    1,
                    5,
                    1,
                    at_scat_init,
                    NULL};
MODEL m_t3100e = {"[286] Toshiba T3100e",
                  ROM_T3100E,
                  "t3100e",
                  {{"", cpus_286}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_FIXED | MODEL_AT | MODEL_HAS_IDE,
                  1024,
                  5120,
                  256,
                  at_t3100e_init,
                  NULL};
MODEL m_tg286m = {"[286] Trigem 286M",
                  ROM_TG286M,
                  "tg286m",
                  {{"", cpus_286}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                  512,
                  8192,
                  128,
                  at_headland_init,
                  NULL};
MODEL m_tulip_tc7 = {"[286] Tulip AT Compact",
                     ROM_TULIP_TC7,
                     "tulip_tc7",
                     {{"", cpus_286}, {"", NULL}, {"", NULL}},
                     MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                     640,
                     15872,
                     128,
                     ibm_at_init,
                     NULL};

/* 386SX PC's */
MODEL m_acer386 = {"[386SX] Acer 386SX25/N",
                   ROM_ACER386,
                   "acer386",
                   {{"Intel", cpus_acer}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                   1,
                   16,
                   1,
                   at_acer386sx_init,
                   NULL};
MODEL m_ama932j = {"[386SX] AMA-932J",
                   ROM_AMA932J,
                   "ama932j",
                   {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                   MODEL_GFX_FIXED | MODEL_AT | MODEL_HAS_IDE,
                   512,
                   8192,
                   128,
                   at_headland_init,
                   NULL};
MODEL m_ami386 = {"[386SX] AMI 386SX clone",
                  ROM_AMI386SX,
                  "ami386",
                  {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                  512,
                  16384,
                  128,
                  at_headland_init,
                  NULL};
MODEL m_megapc = {"[386SX] Amstrad MegaPC",
                  ROM_MEGAPC,
                  "megapc",
                  {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                  MODEL_GFX_DISABLE_HW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                  1,
                  16,
                  1,
                  at_wd76c10_init,
                  NULL};
MODEL m_cbm_sl386sx25 = {"[386SX] Commodore SL386SX-25",
                         ROM_CBM_SL386SX25,
                         "cbm_sl386sx25",
                         {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                         MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                         1024,
                         16384,
                         512,
                         at_scamp_init,
                         NULL};
MODEL m_dtk386 = {"[386SX] DTK 386SX clone",
                  ROM_DTK386,
                  "dtk386",
                  {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                  512,
                  16384,
                  128,
                  at_neat_init,
                  NULL};
MODEL m_epson_pcax3 = {"[386SX] Epson PC AX3",
                       ROM_EPSON_PCAX3,
                       "epson_pcax3",
                       {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                       MODEL_GFX_NONE | MODEL_AT,
                       256,
                       15872,
                       128,
                       at_init,
                       NULL};
MODEL m_ibmps1_2121 = {"[386SX] IBM PS/1 model 2121",
                       ROM_IBMPS1_2121,
                       "ibmps1_2121",
                       {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                       MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                       1,
                       16,
                       1,
                       ps1_m2121_init,
                       NULL};
MODEL m_ibmps2_m55sx = {"[386SX] IBM PS/2 Model 55SX",
                        ROM_IBMPS2_M55SX,
                        "ibmps2_m55sx",
                        {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                        MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA,
                        1,
                        8,
                        1,
                        ps2_model_55sx_init,
                        NULL};
MODEL m_kmxc02 = {"[386SX] KMX-C-02",
                  ROM_KMXC02,
                  "kmxc02",
                  {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                  MODEL_GFX_NONE | MODEL_AT,
                  512,
                  16384,
                  512,
                  at_scatsx_init,
                  NULL};
MODEL m_pb_l300sx = {"[386SX] Packard Bell Legend 300SX",
                     ROM_PB_L300SX,
                     "pb_l300sx",
                     {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                     MODEL_GFX_NONE | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                     1,
                     16,
                     1,
                     pb_l300sx_init,
                     NULL};
MODEL m_spc6033p = {"[386SX] Samsung SPC-6033P",
                    ROM_SPC6033P,
                    "spc6033p",
                    {{"Intel", cpus_i386SX}, {"AMD", cpus_Am386SX}, {"Cyrix", cpus_486SLC}},
                    MODEL_GFX_DISABLE_HW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                    2,
                    12,
                    2,
                    at_scamp_init,
                    NULL};

/* 386DX PC's */
MODEL m_ami386dx = {"[386DX] AMI 386DX clone",
                    ROM_AMI386DX_OPTI495,
                    "ami386dx",
                    {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                    1,
                    256,
                    1,
                    at_opti495_init,
                    NULL};
MODEL m_deskpro386 = {"[386DX] Compaq Deskpro 386",
                      ROM_DESKPRO_386,
                      "deskpro386",
                      {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                      MODEL_GFX_NONE | MODEL_AT,
                      1,
                      15,
                      1,
                      deskpro386_init,
                      NULL};
MODEL m_ecs_386_32 = {"[386DX] ECS 386/32",
                      ROM_ECS_386_32,
                      "ecs_386_32",
                      {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                      MODEL_GFX_NONE | MODEL_AT,
                      1,
                      16,
                      1,
                      at_cs8230_init,
                      NULL};
MODEL m_ibmps2_m70_type3 = {"[386DX] IBM PS/2 Model 70 (type 3)",
                            ROM_IBMPS2_M70_TYPE3,
                            "ibmps2_m70_type3",
                            {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                            MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA,
                            2,
                            16,
                            2,
                            ps2_model_70_init,
                            NULL};
MODEL m_ibmps2_m80 = {"[386DX] IBM PS/2 Model 80",
                      ROM_IBMPS2_M80,
                      "ibmps2_m80",
                      {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                      MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA,
                      1,
                      12,
                      1,
                      ps2_model_80_init,
                      NULL};
MODEL m_mr386dx = {"[386DX] MR 386DX clone",
                   ROM_MR386DX_OPTI495,
                   "mr386dx",
                   {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                   1,
                   256,
                   1,
                   at_opti495_init,
                   NULL};
MODEL m_spc6000a = {"[386DX] Samsung SPC-6000A",
                    ROM_SPC6000A,
                    "spc6000a",
                    {{"Intel", cpus_i386DX}, {"AMD", cpus_Am386DX}, {"Cyrix", cpus_486DLC}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                    1,
                    32,
                    1,
                    at_cs8230_init,
                    NULL};

/* 486 PC's */
MODEL m_ami486 = {"[486] AMI 486 clone",
                  ROM_AMI486,
                  "ami486",
                  {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                  1,
                  256,
                  1,
                  at_ali1429_init,
                  NULL};
MODEL m_win486 = {"[486] AMI WinBIOS 486",
                  ROM_WIN486,
                  "win486",
                  {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_HAS_IDE,
                  1,
                  256,
                  1,
                  at_ali1429_init,
                  NULL};
MODEL m_sis496 = {"[486] Award SiS 496/497",
                  ROM_SIS496,
                  "sis496",
                  {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                  MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_HAS_IDE,
                  1,
                  256,
                  1,
                  at_sis496_init,
                  NULL};
MODEL m_elx_pc425x = {"[486] Elonex PC-425X",
                      ROM_ELX_PC425X,
                      "elx_pc425x",
                      {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                      MODEL_GFX_FIXED | MODEL_AT | MODEL_HAS_IDE,
                      1,
                      256,
                      1,
                      at_sl82c460_init,
                      NULL};
MODEL m_ibmps1_2133 = {"[486] IBM PS/1 Model 2133 (EMEA 451)",
                       ROM_IBMPS1_2133_451,
                       "ibmps1_2133",
                       {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                       MODEL_GFX_FIXED | MODEL_AT | MODEL_PS2,
                       2,
                       64,
                       2,
                       ps1_m2133_init,
                       NULL};
MODEL m_ibmps2_m70_type4 = {"[486] IBM PS/2 Model 70 (type 4)",
                            ROM_IBMPS2_M70_TYPE4,
                            "ibmps2_m70_type4",
                            {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                            MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_MCA,
                            2,
                            16,
                            2,
                            ps2_model_70_init,
                            NULL};
MODEL m_pb410a = {"[486] Packard Bell PB410A",
                  ROM_PB410A,
                  "pb410a",
                  {{"Intel", cpus_i486}, {"AMD", cpus_Am486}, {"Cyrix", cpus_Cx486}},
                  MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PS2 | MODEL_HAS_IDE,
                  1,
                  64,
                  1,
                  at_pb410a_init,
                  NULL};

/* Socket 4 PC's */
MODEL m_revenge = {"[Socket 4] Intel Premiere/PCI",
                   ROM_REVENGE,
                   "revenge",
                   {{"Intel", cpus_Pentium5V}, {"", NULL}, {"", NULL}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                   1,
                   128,
                   1,
                   at_batman_init,
                   NULL};
MODEL m_pb520r = {"[Socket 4] Packard Bell PB520R",
                  ROM_PB520R,
                  "pb520r",
                  {{"Intel", cpus_Pentium5V}, {"", NULL}, {"", NULL}},
                  MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                  1,
                  128,
                  1,
                  at_pb520r_init,
                  NULL};

/* Socket 5 PC's */
MODEL m_endeavor = {"[Socket 5] Intel Advanced/EV",
                    ROM_ENDEAVOR,
                    "endeavor",
                    {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                    1,
                    128,
                    1,
                    at_endeavor_init,
                    NULL};
MODEL m_zappa = {"[Socket 5] Intel Advanced/ZP",
                 ROM_ZAPPA,
                 "zappa",
                 {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}},
                 MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                 1,
                 128,
                 1,
                 at_zappa_init,
                 NULL};
MODEL m_infowaym = {"[Socket 5] Itautec Infoway Multimidia",
                    ROM_ITAUTEC_INFOWAYM,
                    "infowaym",
                    {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}},
                    MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                    8,
                    128,
                    1,
                    at_zappa_init,
                    NULL};
MODEL m_pb570 = {"[Socket 5] Packard Bell PB570",
                 ROM_PB570,
                 "pb570",
                 {{"Intel", cpus_PentiumS5}, {"IDT", cpus_WinChip}, {"", NULL}},
                 MODEL_GFX_DISABLE_SW | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                 1,
                 128,
                 1,
                 at_pb570_init,
                 NULL};

/* Socket 7 PC's */
MODEL m_p55tvp4 = {"[Socket 7] ASUS P/I-P55TVP4",
                   ROM_P55TVP4,
                   "p55tvp4",
                   {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                   8,
                   128,
                   1,
                   at_p55tvp4_init,
                   NULL};
MODEL m_p55t2p4 = {"[Socket 7] ASUS P/I-P55T2P4",
                   ROM_P55T2P4,
                   "p55t2p4",
                   {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                   8,
                   256,
                   1,
                   at_p55t2p4_init,
                   NULL};
MODEL m_p55va = {"[Socket 7] Epox P55-VA",
                 ROM_P55VA,
                 "p55va",
                 {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}},
                 MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                 8,
                 128,
                 1,
                 at_p55va_init,
                 NULL};
MODEL m_430vx = {"[Socket 7] Shuttle HOT-557",
                 ROM_430VX,
                 "430vx",
                 {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_S7}, {"IDT", cpus_WinChip}, {"Cyrix", cpus_6x86}},
                 MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                 8,
                 128,
                 1,
                 at_i430vx_init,
                 NULL};

/* Super 7 PC's */
MODEL m_fic_va503p = {"[Super 7] FIC VA-503+",
                      ROM_FIC_VA503P,
                      "fic_va503p",
                      {{"Intel", cpus_Pentium}, {"AMD", cpus_K6_SS7}, {"IDT", cpus_WinChip_SS7}, {"Cyrix", cpus_6x86_SS7}},
                      MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                      1,
                      512,
                      1,
                      at_mvp3_init,
                      NULL};

/* Socket 8 PC's */
MODEL m_vs440fx = {"[Socket 8] Intel VS440FX",
                   ROM_VS440FX,
                   "vs440fx",
                   {{"Intel", cpus_PentiumPro}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                   8,
                   256,
                   8,
                   at_vs440fx_init,
                   NULL};

/* Slot 1 PC's */
MODEL m_ga686bx = {"[Slot 1] Gigabyte GA-686BX",
                   ROM_GA686BX,
                   "ga686bx",
                   {{"Intel", cpus_Slot1_100MHz}, {"VIA", cpus_VIA_100MHz}},
                   MODEL_GFX_NONE | MODEL_AT | MODEL_PCI | MODEL_PS2 | MODEL_HAS_IDE,
                   8,
                   512,
                   8,
                   at_ga686bx_init,
                   NULL};

void model_init_builtin() {
        /* 8088 PC's */
        pcem_add_model(&m_amixt);
        pcem_add_model(&m_ataripc3);
        pcem_add_model(&m_cbm_pc10);
        pcem_add_model(&m_compaq_pip);
        pcem_add_model(&m_dtk);
        pcem_add_model(&m_genxt);
        pcem_add_model(&m_super16t);
        pcem_add_model(&m_super16te);
        pcem_add_model(&m_ibmpc);
        pcem_add_model(&m_ibmpcjr);
        pcem_add_model(&m_ibmxt);
        pcem_add_model(&m_jukopc);
        pcem_add_model(&m_ledge_modelm);
        pcem_add_model(&m_ncr_pc4i);
        pcem_add_model(&m_pxxt);
        pcem_add_model(&m_europc);
        pcem_add_model(&m_tandy);
        pcem_add_model(&m_tandy1000hx);
        pcem_add_model(&m_to16_pc);
        pcem_add_model(&m_t1000);
        pcem_add_model(&m_ltxt);
        pcem_add_model(&m_xi8088);
        pcem_add_model(&m_zdsupers);

        /* 8086 PC's */
        pcem_add_model(&m_pc1512);
        pcem_add_model(&m_pc1640);
        pcem_add_model(&m_pc2086);
        pcem_add_model(&m_pc3086);
        pcem_add_model(&m_pc5086);
        pcem_add_model(&m_ppc512);
        pcem_add_model(&m_deskpro);
        pcem_add_model(&m_olivetti_m24);
        pcem_add_model(&m_pc200);
        pcem_add_model(&m_tandy1000sl2);
        pcem_add_model(&m_t1200);
        pcem_add_model(&m_lxt3);

        /* 286 PC's */
        pcem_add_model(&m_ami286);
        pcem_add_model(&m_award286);
        pcem_add_model(&m_bull_micral_45);
        pcem_add_model(&m_cmdpc30);
        pcem_add_model(&m_compaq_pii);
        pcem_add_model(&m_dells200);
        pcem_add_model(&m_epson_pcax);
        pcem_add_model(&m_epson_pcax2e);
        pcem_add_model(&m_gdc212m);
        pcem_add_model(&m_grid1520);
        pcem_add_model(&m_gw286ct);
        pcem_add_model(&m_super286tr);
        pcem_add_model(&m_ibmat);
        pcem_add_model(&m_ibmps1es);
        pcem_add_model(&m_ibmps2_m30_286);
        pcem_add_model(&m_ibmps2_m50);
        pcem_add_model(&m_ibmxt286);
        pcem_add_model(&m_spc4200p);
        pcem_add_model(&m_spc4216p);
        pcem_add_model(&m_spc4620p);
        pcem_add_model(&m_t3100e);
        pcem_add_model(&m_tg286m);
        pcem_add_model(&m_tulip_tc7);

        /* 386SX PC's */
        pcem_add_model(&m_acer386);
        pcem_add_model(&m_ama932j);
        pcem_add_model(&m_ami386);
        pcem_add_model(&m_megapc);
        pcem_add_model(&m_cbm_sl386sx25);
        pcem_add_model(&m_dtk386);
        pcem_add_model(&m_epson_pcax3);
        pcem_add_model(&m_ibmps1_2121);
        pcem_add_model(&m_ibmps2_m55sx);
        pcem_add_model(&m_kmxc02);
        pcem_add_model(&m_pb_l300sx);
        pcem_add_model(&m_spc6033p);

        /* 386DX PC's */
        pcem_add_model(&m_ami386dx);
        pcem_add_model(&m_deskpro386);
        pcem_add_model(&m_ecs_386_32);
        pcem_add_model(&m_ibmps2_m70_type3);
        pcem_add_model(&m_ibmps2_m80);
        pcem_add_model(&m_mr386dx);
        pcem_add_model(&m_spc6000a);

        /* 486 PC's */
        pcem_add_model(&m_ami486);
        pcem_add_model(&m_win486);
        pcem_add_model(&m_sis496);
        pcem_add_model(&m_elx_pc425x);
        pcem_add_model(&m_ibmps1_2133);
        pcem_add_model(&m_ibmps2_m70_type4);
        pcem_add_model(&m_pb410a);

        /* Socket 4 PC's */
        pcem_add_model(&m_revenge);
        pcem_add_model(&m_pb520r);

        /* Socket 5 PC's */
        pcem_add_model(&m_endeavor);
        pcem_add_model(&m_zappa);
        pcem_add_model(&m_infowaym);
        pcem_add_model(&m_pb570);

        /* Socket 7 PC's */
        pcem_add_model(&m_p55tvp4);
        pcem_add_model(&m_p55t2p4);
        pcem_add_model(&m_p55va);
        pcem_add_model(&m_430vx);

        /* Super 7 PC's */
        pcem_add_model(&m_fic_va503p);

        /* Socket 8 PC's */
        pcem_add_model(&m_vs440fx);

        /* Slot 1 PC's */
        pcem_add_model(&m_ga686bx);
}
