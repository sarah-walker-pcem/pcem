#include "wx-utils.h"
#include "wx-sdl2.h"
#include "wx-joystickconfig.h"

#include "ibm.h"
#include "ide.h"
#include "cpu.h"
#include "device.h"
#include "fdd.h"
#include "gameport.h"
#include "hdd.h"
#include "lpt.h"
#include "model.h"
#include "mouse.h"
#include "mem.h"
#include "nethandler.h"
#include "nvr.h"
#include "scsi_cd.h"
#include "sound.h"
#include "video.h"
#include "vid_voodoo.h"

#include "minivhd/minivhd.h"
#include "minivhd/minivhd_util.h"

//#define MAX_CYLINDERS ((((1 << 28)-1) / 16) / 63)
#define MAX_CYLINDERS 265264 /*Award 430VX won't POST with a larger drive*/
extern int pause;

extern int is486;
static int romstolist[ROM_MAX], listtomodel[ROM_MAX], romstomodel[ROM_MAX],
                modeltolist[ROM_MAX];
static int settings_sound_to_list[20], settings_list_to_sound[20];
static int settings_mouse_to_list[20], settings_list_to_mouse[20];
#ifdef USE_NETWORKING
static int settings_network_to_list[20], settings_list_to_network[20];
#endif
static char *hdd_names[16];

int has_been_inited = 0;

static int hd_changed = 0;

static char hd_new_name[512];
static int hd_new_spt, hd_new_hpc, hd_new_cyl;
static int hd_new_type;
static int new_cdrom_channel;
static int new_zip_channel;

extern int pause;

static int memspin_old;

extern void deviceconfig_open(void* hwnd, device_t *device);
extern int hdconf_init(void* hdlg);
extern int hdconf_update(void* hdlg);

static int hdd_controller_selected_is_ide(void* hdlg);
static int hdd_controller_selected_is_scsi(void* hdlg);
static int hdd_controller_selected_has_config(void *hdlg);
static device_t *hdd_controller_selected_get_device(void *hdlg);

static int mouse_valid(int type, int model)
{
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_PS2)
                        && !(models[model].flags & MODEL_PS2))
                return 0;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_AMSTRAD)
                        && !(models[model].flags & MODEL_AMSTRAD))
                return 0;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_OLIM24)
                        && !(models[model].flags & MODEL_OLIM24))
                return 0;
        return 1;
}

/*static int mpu401_available(int sound_card)
{
        char* name = sound_card_get_internal_name(sound_card);
        if (name && (!strcmp(name, "sb16") || !strcmp(name, "sbawe32")))
                return TRUE;

        return FALSE;
}*/

static void recalc_fpu_list(void *hdlg, int model, int manu, int cpu, int fpu)
{
        void *h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOFPU"));
        int c = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

        while (1)
        {
                const char *name = fpu_get_name_from_index(model, manu, cpu, c);
                int type = fpu_get_type_from_index(model, manu, cpu, c);
                if (!name)
                        break;

                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)name);
                if (!c || type == fpu)
                        wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);

                c++;
        }
        if (c > 1)
                wx_enablewindow(h, TRUE);
        else
                wx_enablewindow(h, FALSE);
}

static void recalc_vid_list(void* hdlg, int model, int force_builtin_video)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
        int c = 0, d = 0;
        int found_card = 0;
        int cur_gfxcard = gfxcard;
        int romset = model_getromset_from_model(model);

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

        if (model_has_fixed_gfx(model))
        {
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Built-in video");
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                wx_enablewindow(h, FALSE);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVID"));
                wx_enablewindow(h, video_card_has_config(gfxcard, romset));
                return;
        }

        if (model_has_optional_gfx(model))
        {
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Built-in video");
                if (gfxcard == GFX_BUILTIN || force_builtin_video)
                {
                        wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);
                        found_card = 1;
                        cur_gfxcard = GFX_BUILTIN;
                }
                d++;
        }

        if (cur_gfxcard == GFX_BUILTIN && !model_has_fixed_gfx(model) && !model_has_optional_gfx(model))
                cur_gfxcard = 0;

        while (1)
        {
                char *s = video_card_getname(c);

                if (!s[0])
                        break;

                if (video_card_available(c) && gfx_present[video_new_to_old(c)] &&
                    ((models[model].flags & MODEL_PCI) || !(video_card_getdevice(c, romset)->flags & DEVICE_PCI)) &&
                    ((models[model].flags & MODEL_MCA) || !(video_card_getdevice(c, romset)->flags & DEVICE_MCA)) &&
                    (!(models[model].flags & MODEL_MCA) || (video_card_getdevice(c, romset)->flags & DEVICE_MCA))
                    )
                {
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                        if (video_new_to_old(c) == gfxcard && !found_card)
                        {
                                wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);
                                found_card = 1;
                        }

                        d++;
                }

                c++;
        }
        if (!found_card)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
        wx_enablewindow(h, TRUE);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVID"));
        if (video_card_has_config(video_old_to_new(cur_gfxcard), romset))
                wx_enablewindow(h, TRUE);
        else
                wx_enablewindow(h, FALSE);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKVOODOO"));
        wx_enablewindow(h, (models[model].flags & MODEL_PCI) ? TRUE : FALSE);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVOODOO"));
        wx_enablewindow(h, (models[model].flags & MODEL_PCI) ? TRUE : FALSE);
}

static void recalc_snd_list(void* hdlg, int model)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
        int c = 0, d = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

        while (1)
        {
                char *s = sound_card_getname(c);

                if (!s[0])
                        break;

                settings_sound_to_list[c] = d;

                if (sound_card_available(c))
                {
                        device_t *sound_dev = sound_card_getdevice(c);

                        if (!sound_dev || (sound_dev->flags & DEVICE_MCA) == (models[model].flags & MODEL_MCA))
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);
                                settings_list_to_sound[d] = c;
                                d++;
                        }
                }

                c++;
        }
        wx_sendmessage(h, WX_CB_SETCURSEL, settings_sound_to_list[sound_card_current], 0);
}

static void recalc_hdd_list(void* hdlg, int model, int use_selected_hdd, int force_ide)
{
        void* h;
        char *s;
        int valid = 0;
        char old_name[16];
        int c, d;
        int hdd_type;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));

        if (force_ide)
                strcpy(old_name, "ide");
        else if (use_selected_hdd)
        {
                c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                if (c != -1 && hdd_names[c])
                        strncpy(old_name, hdd_names[c], sizeof(old_name)-1);
                else
                {
                        if (models[model].flags & MODEL_HAS_IDE)
                                strcpy(old_name, "none");
                        else
                                strcpy(old_name, "ide");
                }
        }
        else
                strncpy(old_name, hdd_controller_name, sizeof(old_name)-1);

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
        c = d = 0;
        while (1)
        {
                s = hdd_controller_get_name(c);
                if (s[0] == 0)
                        break;
                if ((((hdd_controller_get_flags(c) & DEVICE_AT) && !(models[model].flags & MODEL_AT)) ||
                        (hdd_controller_get_flags(c) & DEVICE_MCA) != (models[model].flags & MODEL_MCA)) && c)
                {
                        c++;
                        continue;
                }
                if ((((hdd_controller_get_flags(c) & DEVICE_PS1) && models[model].id != ROM_IBMPS1_2011) ||
                    (!(hdd_controller_get_flags(c) & DEVICE_PS1) && models[model].id == ROM_IBMPS1_2011)) && c)
                {
                        c++;
                        continue;
                }
                if (!hdd_controller_available(c))
                {
                        c++;
                        continue;
                }
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                hdd_names[d] = hdd_controller_get_internal_name(c);

                if (!strcmp(old_name, hdd_names[d]))
                {
                        wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);
                        valid = 1;
                }
                c++;
                d++;
        }

        if (!valid)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

        wx_enablewindow(h, TRUE);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREHDD"));
        if (hdd_controller_selected_has_config(hdlg))
                wx_enablewindow(h, TRUE);
        else
                wx_enablewindow(h, FALSE);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        hdd_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDCBOOK"));

        while (wx_sendmessage(h, WX_CHB_GETPAGECOUNT, 0, 0))
                wx_sendmessage(h, WX_CHB_REMOVEPAGE, 0, 0);

        for (c = 0; c < 7; c++)
        {
                void *page;
                char s[80];

                sprintf(s, "IDC_HDPANEL[%i]", c);

                page = wx_getdlgitem(hdlg, WX_ID(s));
                wx_sendmessage(page, WX_REPARENT, (LONG_PARAM)h, 0);
                wx_sendmessage(h, WX_CHB_ADDPAGE, (LONG_PARAM)page, (LONG_PARAM)"dummy name");
        }
        if (hdd_controller_is_scsi(hdd_names[hdd_type]))
        {
                for (c = 0; c < 7; c++)
                {
                        void *page;
                        char s[80];

                        sprintf(s, "IDC_HDPANEL[%i]", c);
                        page = wx_getdlgitem(hdlg, WX_ID(s));
                        wx_sendmessage(page, WX_REPARENT, (LONG_PARAM)h, 0);
                        sprintf(s, "SCSI ID %i (%c:)", c, 'C'+c);
                        wx_sendmessage(h, WX_CHB_SETPAGETEXT, c, (LONG_PARAM)s);
                }
        }
        else if (hdd_controller_is_ide(hdd_names[hdd_type]))
        {
                for (c = 0; c < 4; c++)
                {
                        void *page;
                        char s[80];

                        sprintf(s, "IDC_HDPANEL[%i]", c);
                        page = wx_getdlgitem(hdlg, WX_ID(s));
                        wx_sendmessage(page, WX_REPARENT, (LONG_PARAM)h, 0);
                        sprintf(s, "Drive %i %s %s (%c:)", c, (c & 2) ? "Secondary" : "Primary", (c & 1) ? "Slave" : "Master", 'C'+c);
                        wx_sendmessage(h, WX_CHB_SETPAGETEXT, c, (LONG_PARAM)s);
                }
                for (c = 6; c >= 4; c--)
                {
                        wx_sendmessage(h, WX_CHB_REMOVEPAGE, c, 0);
                }
        }
        else
        {
                for (c = 0; c < 2; c++)
                {
                        void *page;
                        char s[80];

                        sprintf(s, "IDC_HDPANEL[%i]", c);
                        page = wx_getdlgitem(hdlg, WX_ID(s));
                        wx_sendmessage(page, WX_REPARENT, (LONG_PARAM)h, 0);
                        sprintf(s, "Drive %i %s (%c:)", c, (c & 1) ? "Secondary" : "Primary", 'C'+c);
                        wx_sendmessage(h, WX_CHB_SETPAGETEXT, c, (LONG_PARAM)s);
                }
                for (c = 6; c >= 2; c--)
                {
                        wx_sendmessage(h, WX_CHB_REMOVEPAGE, c, 0);
                }
        }
}

// TODO: split this into model/speed recalcs?
static void recalc_cd_list(void *hdlg, int cur_speed, char *cur_model)
{
        int temp_model = -1;
        void *h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDMODEL"));
        int c = 0;
        int found = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
        if (hdd_controller_selected_is_ide(hdlg) || hdd_controller_selected_is_scsi(hdlg))
                wx_enablewindow(h, TRUE);
        else
                wx_enablewindow(h, FALSE);
        while (1)
        {
                char s[40];
                char *model;

                if ((cd_get_model_interfaces(c) == CD_MODEL_INTERFACE_ALL) ||
                    (cd_get_model_interfaces(c) == CD_MODEL_INTERFACE_IDE && hdd_controller_selected_is_ide(hdlg)) ||
                    (cd_get_model_interfaces(c) == CD_MODEL_INTERFACE_SCSI && hdd_controller_selected_is_scsi(hdlg)))
                {

                        model = cd_get_model(c);
                        sprintf(s, "%s", model);
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);

                        if (!strncmp(s, cur_model, 40))
                        {
                                wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                temp_model = c;
                                found = 1;
                        }
                }

                c++;
                if (c > MAX_CD_MODEL)
                        break;
        }
        if (!found)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0); // assume that there is always at least one TODO: is this necessary?


        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDSPEED"));
        c = 0;
        found = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
        if (temp_model >= 0 && cd_get_model_speed(temp_model) != -1)
        {
                // this model has a specific speed
                wx_enablewindow(h, FALSE);
                cur_speed = cd_get_model_speed(temp_model);
        }
        else if (hdd_controller_selected_is_ide(hdlg) || hdd_controller_selected_is_scsi(hdlg))
                wx_enablewindow(h, TRUE);
        else
                wx_enablewindow(h, FALSE);
        while (1)
        {
                char s[8];
                int speed;

                speed = cd_get_speed(c);
                sprintf(s, "%iX", speed);
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);

                if (speed == cur_speed)
                {
                        wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                        found = 1;
                }

                c++;
                if (speed >= MAX_CD_SPEED)
                        break;
        }
        if (!found)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0); // fall back TODO: is this necessary?
}

#ifdef USE_NETWORKING
static void recalc_net_list(void *hdlg, int model)
{
        void *h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
        int c = 0, d = 0;
        int found_card = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

        while (1)
        {
                char *s = network_card_getname(c);
                device_t *dev;

                if (!s[0])
                        break;

                settings_network_to_list[c] = d;

                dev = network_card_getdevice(c);

                if (network_card_available(c) &&
                    (!dev || (models[model].flags & MODEL_PCI) || !(dev->flags & DEVICE_PCI)))
                {
                        device_t *network_dev = network_card_getdevice(c);

                        if (!network_dev || (network_dev->flags & DEVICE_MCA) == (models[model].flags & MODEL_MCA))
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                settings_list_to_network[d] = c;
                                if (c == network_card_current)
                                {
                                        wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);
                                        found_card = 1;
                                }
                                d++;
                        }
                }

                c++;
        }
        if (!found_card)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
}
#endif

int config_dlgsave(void* hdlg)
{
        char temp_str[256];
        void* h;
        int c;
        int gfx;//, fpu;
        int temp_cpu, temp_cpu_m, temp_model, temp_fpu;
        int temp_GAMEBLASTER, temp_GUS, temp_SSI2001, temp_voodoo, temp_sound_card_current;
        int temp_dynarec;
        int temp_fda_type, temp_fdb_type;
        int temp_mouse_type;
        int temp_lpt1_device;
        int temp_joystick_type;
#ifdef USE_NETWORKING
        int temp_network_card;
#endif
        int hdd_changed = 0;
        char s[260];
        PcemHDC hd[7];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
        temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
        int mem = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
        mem &= ~(models[temp_model].ram_granularity - 1);
        if (mem < models[temp_model].min_ram)
                mem = models[temp_model].min_ram;
        else if (mem > models[temp_model].max_ram)
                mem = models[temp_model].max_ram;
        if ((models[temp_model].flags & MODEL_AT) && models[temp_model].ram_granularity < 128)
                mem *= 1024;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
        wx_sendmessage(h, WX_CB_GETLBTEXT, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0),
        (LONG_PARAM) temp_str);
        gfx = video_new_to_old(video_card_getid(temp_str));

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
        temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
        temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
//        fpu = (models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type
//        >= CPU_i486DX) ? 1 : 0;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOFPU"));
        temp_fpu = fpu_get_type_from_index(temp_model, temp_cpu_m, temp_cpu, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0));

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECK3"));
        temp_GAMEBLASTER = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKGUS"));
        temp_GUS = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSSI"));
        temp_SSI2001 = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSYNC"));
        enable_sync = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKVOODOO"));
        temp_voodoo = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
        temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
        temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRA"));
        temp_fda_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRB"));
        temp_fdb_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
        temp_joystick_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
        temp_mouse_type = settings_list_to_mouse[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                hdd_changed = strncmp(hdd_names[c], hdd_controller_name, sizeof(hdd_controller_name)-1);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOLPT1"));
        temp_lpt1_device = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

#ifdef USE_NETWORKING
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
        temp_network_card = settings_list_to_network[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
#endif

        if (temp_model != model || gfx != gfxcard || mem != mem_size || temp_fpu != fpu_type ||
            temp_GAMEBLASTER != GAMEBLASTER || temp_GUS != GUS ||
            temp_SSI2001 != SSI2001 || temp_sound_card_current != sound_card_current ||
            temp_voodoo != voodoo_enabled || temp_dynarec != cpu_use_dynarec ||
            temp_fda_type != fdd_get_type(0) || temp_fdb_type != fdd_get_type(1) ||
            temp_mouse_type != mouse_type || hdd_changed || hd_changed || cdrom_channel != new_cdrom_channel ||
            zip_channel != new_zip_channel || strcmp(lpt1_device_name, lpt_device_get_internal_name(temp_lpt1_device))
#ifdef USE_NETWORKING
                            || temp_network_card != network_card_current
#endif
            )
        {
                if (!has_been_inited || confirm())
                {
                        savenvr();
                        model = temp_model;
                        romset = model_getromset();
                        gfxcard = gfx;
                        mem_size = mem;
                        cpu_manufacturer = temp_cpu_m;
                        cpu = temp_cpu;
                        fpu_type = temp_fpu;
                        GAMEBLASTER = temp_GAMEBLASTER;
                        GUS = temp_GUS;
                        SSI2001 = temp_SSI2001;
                        sound_card_current = temp_sound_card_current;
                        voodoo_enabled = temp_voodoo;
                        cpu_use_dynarec = temp_dynarec;
                        mouse_type = temp_mouse_type;
                        strcpy(lpt1_device_name, lpt_device_get_internal_name(temp_lpt1_device));
#ifdef USE_NETWORKING
                        network_card_current = temp_network_card;
#endif

                        fdd_set_type(0, temp_fda_type);
                        fdd_set_type(1, temp_fdb_type);

                        if (hdd_names[c])
                                strncpy(hdd_controller_name, hdd_names[c], sizeof(hdd_controller_name)-1);
                        else
                                strcpy(hdd_controller_name, "none");

                        for (c = 0; c < 7; c++)
                        {
                                sprintf(s, "IDC_EDIT_SPT[%i]", c);
                                h = wx_getdlgitem(hdlg, WX_ID(s));
                                wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                sscanf(s, "%i", &hd[c].spt);
                                sprintf(s, "IDC_EDIT_HPC[%i]", c);
                                h = wx_getdlgitem(hdlg, WX_ID(s));
                                wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                sscanf(s, "%i", &hd[c].hpc);
                                sprintf(s, "IDC_EDIT_CYL[%i]", c);
                                h = wx_getdlgitem(hdlg, WX_ID(s));
                                wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                sscanf(s, "%i", &hd[c].tracks);
                                sprintf(s, "IDC_EDIT_FN[%i]", c);
                                h = wx_getdlgitem(hdlg, WX_ID(s));
                                wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)ide_fn[c]);

                                hdc[c] = hd[c];
                        }

                        cdrom_channel = new_cdrom_channel;
                        zip_channel = new_zip_channel;

                        if (has_been_inited)
                        {
                                mem_alloc();
                                loadbios();
                                resetpchard();
                        }
                }
                else
                        return TRUE;
        }

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSPD"));
        video_speed = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0) - 1;

        cpu_manufacturer = temp_cpu_m;
        cpu = temp_cpu;
        cpu_set();

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
        cpu_waitstates = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        cpu_update_waitstates();

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDSPEED"));
        cd_speed = cd_get_speed(wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0));
        cd_set_speed(cd_speed);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDMODEL"));
        cd_model = cd_get_model(wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0));
        cd_set_model(cd_model);

        if (has_been_inited)
                saveconfig(NULL);

        speedchanged();

        joystick_type = temp_joystick_type;
        gameport_update_joystick_type();


        return TRUE;
}

static int prev_model;
int config_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char temp_str[256];
        void* h;
        int c, d;
        int gfx, mem;
        int temp_cpu, temp_cpu_m, temp_model, temp_fpu;
        int temp_sound_card_current;
        int temp_dynarec;
        int cpu_flags;
        int cpu_type;
        int temp_cd_model, temp_cd_speed;
        int temp_mouse_type;
#ifdef USE_NETWORKING
        int temp_network_card;
#endif

        switch (message)
        {
                case WX_INITDIALOG:
                {
                        pause = 1;
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                        for (c = 0; c < ROM_MAX; c++)
                        romstolist[c] = 0;
                        c = d = 0;
                        while (models[c].id != -1)
                        {
                                pclog("INITDIALOG : %i %i %i\n", c, models[c].id,
                                romspresent[models[c].id]);
                                if (romspresent[models[c].id])
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) models[c].name);
                                        modeltolist[c] = d;
                                        listtomodel[d] = c;
                                        romstolist[models[c].id] = d;
                                        romstomodel[models[c].id] = c;
                                        d++;
                                }
                                c++;
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, modeltolist[model], 0);
                        prev_model = model;

                        recalc_vid_list(hdlg, romstomodel[romset], 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                        c = 0;
                        while (models[romstomodel[romset]].cpu[c].cpus != NULL && c < 4)
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                (LONG_PARAM) models[romstomodel[romset]].cpu[c].name);
                                c++;
                        }
                        wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_CB_SETCURSEL, cpu_manufacturer, 0);
                        if (c == 1)
                                wx_enablewindow(h, FALSE);
                        else
                                wx_enablewindow(h, TRUE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                        c = 0;
                        while (models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].cpu_type != -1)
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                (LONG_PARAM) models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].name);
                                c++;
                        }
                        wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_CB_SETCURSEL, cpu, 0);

                        recalc_fpu_list(hdlg, romstomodel[romset], cpu_manufacturer, cpu, fpu_type);

                        recalc_snd_list(hdlg, romstomodel[romset]);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECK3"));
                        wx_sendmessage(h, WX_BM_SETCHECK, GAMEBLASTER, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKGUS"));
                        wx_sendmessage(h, WX_BM_SETCHECK, GUS, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSSI"));
                        wx_sendmessage(h, WX_BM_SETCHECK, SSI2001, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSYNC"));
                        wx_sendmessage(h, WX_BM_SETCHECK, enable_sync, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKVOODOO"));
                        wx_sendmessage(h, WX_BM_SETCHECK, voodoo_enabled, 0);

                        cpu_flags =
                        models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[cpu].cpu_flags;
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                        if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                wx_enablewindow(h, FALSE);
                        else
                                wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_BM_SETCHECK,
                        ((cpu_flags & CPU_SUPPORTS_DYNAREC) && cpu_use_dynarec)
                        || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSPD"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Default");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "8-bit");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Slow 16-bit");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Fast 16-bit");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Slow VLB/PCI");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Mid  VLB/PCI");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Fast VLB/PCI");
                        wx_sendmessage(h, WX_CB_SETCURSEL, video_speed+1, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
                        printf("%d\n", models[model].ram_granularity);
                        wx_sendmessage(h, WX_UDM_SETRANGE, 0,
                        (models[romstomodel[romset]].min_ram << 16)
                        | models[romstomodel[romset]].max_ram);
                        wx_sendmessage(h, WX_UDM_SETINCR, 0, models[model].ram_granularity);
                        if (!((models[model].flags & MODEL_AT) && models[model].ram_granularity < 128))
                        {
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem_size);
                                memspin_old = mem_size;
                        }
                        else
                        {
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem_size / 1024);
                                memspin_old = mem_size / 1024;
                        }

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREMOD"));
                        if (model_getdevice(model))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGURESND"));
                        if (sound_card_has_config(sound_card_current))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRA"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"None");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 360k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M Dual RPM");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 720k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M 3-Mode");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 2.88M");
                        wx_sendmessage(h, WX_CB_SETCURSEL, fdd_get_type(0), 0);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRB"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"None");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 360k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M Dual RPM");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 720k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M 3-Mode");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 2.88M");
                        wx_sendmessage(h, WX_CB_SETCURSEL, fdd_get_type(1), 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_MB"));
                        if ((models[model].flags & MODEL_AT) && (models[model].ram_granularity < 128))
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "MB");
                        else
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "KB");

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
                        c = 0;
                        while (joystick_get_name(c))
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)joystick_get_name(c));
                                c++;
                        }
                        wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_CB_SETCURSEL, joystick_type, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY1"));
                        wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 1) ? TRUE : FALSE);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY2"));
                        wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 2) ? TRUE : FALSE);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY3"));
                        wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 3) ? TRUE : FALSE);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY4"));
                        wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 4) ? TRUE : FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "System default");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "0 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "1 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "2 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "3 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "4 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "5 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "6 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "7 W/S");
                        wx_sendmessage(h, WX_CB_SETCURSEL, cpu_waitstates, 0);
                        cpu_type =
                        models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[cpu].cpu_type;
                        if ((cpu_type >= CPU_286) && (cpu_type <= CPU_386DX))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
                        c = d = 0;
                        while (1)
                        {
                                char *s = mouse_get_name(c);
                                int type;

                                if (!s)
                                break;

                                type = mouse_get_type(c);

                                settings_mouse_to_list[c] = d;

                                if (mouse_valid(type, model))
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);

                                        settings_list_to_mouse[d] = c;
                                        d++;
                                }
                                c++;
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, settings_mouse_to_list[mouse_type], 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOLPT1"));
                        c = d = 0;
                        while (1)
                        {
                                char *s = lpt_device_get_name(c);

                                if (!s)
                                        break;

                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                if (!strcmp(lpt1_device_name, lpt_device_get_internal_name(c)))
                                        d = c;

                                c++;
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);

                        recalc_hdd_list(hdlg, romstomodel[romset], 0, 0);

                        hd_changed = 0;
                        new_cdrom_channel = cdrom_channel;
                        new_zip_channel = zip_channel;

                        hdconf_init(hdlg);

                        for (c = 0; c < 7; c++)
                        {
                                char s[80];

                                sprintf(s, "IDC_COMBODRIVETYPE[%i]", c);
                                h = wx_getdlgitem(hdlg, WX_ID(s));
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Hard drive");
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"CD-ROM");
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Iomega Zip");
                                if (cdrom_channel == c)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, 1, 0);
                                else if (zip_channel == c)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, 2, 0);
                                else
                                        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                        }

                        recalc_cd_list(hdlg, cd_speed, cd_model);

#ifdef USE_NETWORKING
                        recalc_net_list(hdlg, romstomodel[romset]);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGURE_NETCARD"));
                        if (network_card_has_config(network_card_current))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);
#endif

                        return TRUE;
                }
                break;
                case WX_COMMAND:
                {
                        if (wParam == WX_ID("IDC_COMBO1"))
                        {
                                int force_ide = 0;
                                int force_builtin_video = 0;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                if (temp_model == prev_model)
                                        break;

                                if ((models[temp_model].flags & MODEL_HAS_IDE) && !(models[prev_model].flags & MODEL_HAS_IDE))
                                        force_ide = 1;
                                if (model_has_optional_gfx(temp_model))
                                        force_builtin_video = 1;

                                prev_model = temp_model;

                                /*Rebuild manufacturer list*/
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                                temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[c].cpus != NULL && c < 4)
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                        (LONG_PARAM) models[temp_model].cpu[c].name);
                                        c++;
                                }
                                if (temp_cpu_m >= c)
                                temp_cpu_m = c - 1;
                                wx_sendmessage(h, WX_CB_SETCURSEL, temp_cpu_m, 0);
                                if (c == 1)
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);

                                /*Rebuild CPU list*/
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                                temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[temp_cpu_m].cpus[c].cpu_type != -1)
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                        (LONG_PARAM) models[temp_model].cpu[temp_cpu_m].cpus[c].name);
                                        c++;
                                }
                                if (temp_cpu >= c)
                                        temp_cpu = c - 1;
                                wx_sendmessage(h, WX_CB_SETCURSEL, temp_cpu, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOFPU"));
                                temp_fpu = fpu_get_type_from_index(temp_model, temp_cpu_m, temp_cpu, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0));
                                recalc_fpu_list(hdlg, temp_model, temp_cpu_m, temp_cpu, temp_fpu);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                cpu_flags =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);
                                wx_sendmessage(h, WX_BM_SETCHECK,
                                ((cpu_flags & CPU_SUPPORTS_DYNAREC) && temp_dynarec)
                                || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_MB"));
                                if ((models[temp_model].flags & MODEL_AT) && models[temp_model].ram_granularity < 128)
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "MB");
                                else
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "KB");

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
                                mem = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
                                wx_sendmessage(h, WX_UDM_SETRANGE, 0,
                                (models[temp_model].min_ram << 16)
                                | models[temp_model].max_ram);
                                mem &= ~(models[temp_model].ram_granularity - 1);
                                if (mem < models[temp_model].min_ram)
                                        mem = models[temp_model].min_ram;
                                else if (mem > models[temp_model].max_ram)
                                        mem = models[temp_model].max_ram;
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem);
                                wx_sendmessage(h, WX_UDM_SETINCR, 0, models[temp_model].ram_granularity);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                                cpu_type =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
                                if (cpu_type >= CPU_286 && cpu_type <= CPU_386DX)
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREMOD"));
                                if (model_getdevice(temp_model))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
                                temp_mouse_type = settings_list_to_mouse[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = d = 0;
                                while (1)
                                {
                                        char *s = mouse_get_name(c);
                                        int type;

                                        if (!s)
                                        break;

                                        type = mouse_get_type(c);
                                        settings_mouse_to_list[c] = d;

                                        if (mouse_valid(type, temp_model))
                                        {
                                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);

                                                settings_list_to_mouse[d] = c;
                                                d++;
                                        }

                                        c++;
                                }

                                if (mouse_valid(mouse_get_type(temp_mouse_type), temp_model))
                                        wx_sendmessage(h, WX_CB_SETCURSEL, settings_mouse_to_list[temp_mouse_type], 0);
                                else
                                        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

                                recalc_vid_list(hdlg, temp_model, force_builtin_video);

                                recalc_hdd_list(hdlg, temp_model, 1, force_ide);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDMODEL"));
                                temp_cd_model = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDSPEED"));
                                temp_cd_speed = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                recalc_cd_list(hdlg, cd_get_speed(temp_cd_speed), cd_get_model(temp_cd_model));

                                recalc_snd_list(hdlg, temp_model);
#ifdef USE_NETWORKING
                                recalc_net_list(hdlg, temp_model);
#endif
                        }
                        else if (wParam == WX_ID("IDC_COMBOCPUM"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                                temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                /*Rebuild CPU list*/
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                                temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[temp_cpu_m].cpus[c].cpu_type != -1)
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                        (LONG_PARAM) models[temp_model].cpu[temp_cpu_m].cpus[c].name);
                                        c++;
                                }
                                if (temp_cpu >= c)
                                temp_cpu = c - 1;
                                wx_sendmessage(h, WX_CB_SETCURSEL, temp_cpu, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOFPU"));
                                temp_fpu = fpu_get_type_from_index(temp_model, temp_cpu_m, temp_cpu, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0));
                                recalc_fpu_list(hdlg, temp_model, temp_cpu_m, temp_cpu, temp_fpu);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                cpu_flags =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);
                                wx_sendmessage(h, WX_BM_SETCHECK,
                                ((cpu_flags & CPU_SUPPORTS_DYNAREC) && temp_dynarec)
                                || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                                cpu_type =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
                                if (cpu_type >= CPU_286 && cpu_type <= CPU_386DX)
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);

                        }
                        else if (wParam == WX_ID("IDC_COMBO3"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                                temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                                temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOFPU"));
                                temp_fpu = fpu_get_type_from_index(temp_model, temp_cpu_m, temp_cpu, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0));
                                recalc_fpu_list(hdlg, temp_model, temp_cpu_m, temp_cpu, temp_fpu);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                cpu_flags =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);
                                wx_sendmessage(h, WX_BM_SETCHECK,
                                ((cpu_flags & CPU_SUPPORTS_DYNAREC) && temp_dynarec)
                                || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                                cpu_type =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
                                if (cpu_type >= CPU_286 && cpu_type <= CPU_386DX)
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREMOD"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg, (void *) model_getdevice(temp_model));
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREVID"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
                                wx_sendmessage(h, WX_CB_GETLBTEXT, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0), (LONG_PARAM) temp_str);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg, (void *)video_card_getdevice(video_card_getid(temp_str), model_getromset_from_model(temp_model)));
                        }
                        else if (wParam == WX_ID("IDC_COMBOVID"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
                                wx_sendmessage(h, WX_CB_GETLBTEXT, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0),
                                (LONG_PARAM) temp_str);
                                gfx = video_card_getid(temp_str);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVID"));
                                if (video_card_has_config(gfx, model_getromset_from_model(temp_model)))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGURESND"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg, (void *) sound_card_getdevice(temp_sound_card_current));
                        }
                        else if (wParam == WX_ID("IDC_COMBOSND"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGURESND"));
                                if (sound_card_has_config(temp_sound_card_current))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREVOODOO"))
                        {
                                deviceconfig_open(hdlg, (void *) &voodoo_device);
                        }
                        else if (wParam == WX_ID("IDC_COMBOHDD"))
                        {
                                hdconf_update(hdlg);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                recalc_hdd_list(hdlg, temp_model, 1, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDMODEL"));
                                temp_cd_model = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDSPEED"));
                                temp_cd_speed = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                recalc_cd_list(hdlg, cd_get_speed(temp_cd_speed), cd_get_model(temp_cd_model));
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREHDD"))
                        {
                                deviceconfig_open(hdlg, (void *)hdd_controller_selected_get_device(hdlg));
                        }
                        else if (wParam == WX_ID("IDC_COMBO_CDMODEL"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDMODEL"));
                                temp_cd_model = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_CDSPEED"));
                                temp_cd_speed = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                recalc_cd_list(hdlg, cd_get_speed(temp_cd_speed), cd_get_model(temp_cd_model));
                        }
#ifdef USE_NETWORKING
                        else if (wParam == WX_ID("IDC_COMBO_NETCARD"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
                                temp_network_card = settings_list_to_network[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGURE_NETCARD"));
                                if (network_card_has_config(temp_network_card))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGURE_NETCARD"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
                                temp_network_card = settings_list_to_network[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg, (void *)network_card_getdevice(temp_network_card));
                        }
#endif
                        else if (wParam == WX_ID("IDC_COMBOJOY"))
                        {
                                int temp_joystick_type;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
                                temp_joystick_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY1"));
                                wx_enablewindow(h, (joystick_get_max_joysticks(temp_joystick_type) >= 1) ? TRUE : FALSE);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY2"));
                                wx_enablewindow(h, (joystick_get_max_joysticks(temp_joystick_type) >= 2) ? TRUE : FALSE);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY3"));
                                wx_enablewindow(h, (joystick_get_max_joysticks(temp_joystick_type) >= 3) ? TRUE : FALSE);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_JOY4"));
                                wx_enablewindow(h, (joystick_get_max_joysticks(temp_joystick_type) >= 4) ? TRUE : FALSE);
                        }
                        else if (wParam == WX_ID("IDC_JOY1"))
                        {
                                int temp_joystick_type;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
                                temp_joystick_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                joystickconfig_open(hdlg, 0, temp_joystick_type);
                        }
                        else if (wParam == WX_ID("IDC_JOY2"))
                        {
                                int temp_joystick_type;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
                                temp_joystick_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                joystickconfig_open(hdlg, 1, temp_joystick_type);
                        }
                        else if (wParam == WX_ID("IDC_JOY3"))
                        {
                                int temp_joystick_type;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
                                temp_joystick_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                joystickconfig_open(hdlg, 2, temp_joystick_type);
                        }
                        else if (wParam == WX_ID("IDC_JOY4"))
                        {
                                int temp_joystick_type;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOJOY"));
                                temp_joystick_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                joystickconfig_open(hdlg, 3, temp_joystick_type);
                        }
#ifndef __WXGTK__
                        /*Emulate spinner granularity on systems that don't implement wxSpinCtrl->SetIncrement()*/
                        else if (wParam == WX_ID("IDC_MEMSPIN"))
                        {
                                int mem;
                                int granularity;
                                int granularity_mask;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                granularity = models[temp_model].ram_granularity;
                                granularity_mask = granularity - 1;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
                                mem = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
                                if (mem < memspin_old)
                                {
                                        /*Assume decrease*/
                                        mem &= ~granularity_mask;
                                }
                                else
                                {
                                        /*Assume increase*/
                                        mem = (mem + granularity_mask) & ~granularity_mask;
                                }
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem);
                                memspin_old = mem;
                        }
#endif
                        return 0;
                }
        }
        return 0;
}

static struct
{
        int cylinders;
        int heads;
} hd_types[] =
{
        {306,  4},  {615, 4},   {615,  6},  {940,  8},
        {940,  6},  {615, 4},   {462,  8},  {733,  5},
        {900, 15},  {820, 3},   {855,  5},  {855,  7},
        {306,  8},  {733, 7},     {0,  0},  {612,  4},
        {977,  5},  {977, 7},  {1024,  7},  {733,  5},
        {733,  7},  {733, 5},   {306,  4},  {925,  7},
        {925,  9},  {754, 7},   {754, 11},  {699,  7},
        {823, 10},  {918, 7},  {1024, 11}, {1024, 15},
        {1024, 5},  {612, 2},  {1024,  9}, {1024,  8},
        {615,  8},  {987, 3},   {462,  7},  {820,  6},
        {977,  5},  {981, 5},   {830,  7},  {830, 10},
        {917, 15}, {1224, 15}
};

static int hdd_controller_selected_is_mfm(void* hdlg)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        int c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                return hdd_controller_is_mfm(hdd_names[c]);
        return 0;
}
static int hdd_controller_selected_is_ide(void* hdlg)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        int c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                return hdd_controller_is_ide(hdd_names[c]);
        return 0;
}
static int hdd_controller_selected_is_scsi(void* hdlg)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        int c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                return hdd_controller_is_scsi(hdd_names[c]);
        return 0;
}
static int hdd_controller_selected_has_config(void* hdlg)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        int c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                return hdd_controller_has_config(hdd_names[c]);
        return 0;
}
static device_t *hdd_controller_selected_get_device(void *hdlg)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        int c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                return hdd_controller_get_device(hdd_names[c]);
        return NULL;
}

static void check_hd_type(void* hdlg, off64_t sz)
{
        int c;

        hd_new_type = 0;

        if (hdd_controller_selected_is_mfm(hdlg))
        {
                for (c = 0; c < 46; c++)
                {
                        if ((hd_types[c].cylinders * hd_types[c].heads * 17 * 512) == sz)
                        {
                                hd_new_spt = 17;
                                hd_new_hpc = hd_types[c].heads;
                                hd_new_cyl = hd_types[c].cylinders;
                                hd_new_type = c+1;
                                return;
                        }
                }
                hd_new_spt = 63;
                hd_new_hpc = 16;
                hd_new_cyl = ((sz / 512) / 16) / 63;
        }
        else
        {
                if ((sz % 17) == 0 && sz <= 142606336)
                {
                        hd_new_spt = 17;

                        if (sz <= 26738688)
                                hd_new_hpc = 4;
                        else if ((sz % 3072) == 0 && sz <= 53477376)
                                hd_new_hpc = 6;
                        else
                        {
                                for (c=5;c<16;c++)
                                {
                                        if((sz % (c * 512)) == 0 && sz <= 1024*c*17*512) break;
                                        if (c == 5) c++;
                                }
                                hd_new_hpc = c;
                        }
                        hd_new_cyl = (int)((sz / 512) / hd_new_hpc) / 17;
                }
                else
                {
                        hd_new_spt = 63;
                        hd_new_hpc = 16;
                        hd_new_cyl = ((sz / 512) / 16) / 63;
                }
        }
}

static void update_hdd_cdrom(void* hdlg)
{
        void* h;
        int i, j, b;
        char s[25];

        int is_mfm = hdd_controller_selected_is_mfm(hdlg);

        for (i = 0; i < 7; ++i)
        {
                b = !is_mfm && ((new_cdrom_channel == i) || (new_zip_channel == i));
                pclog("update_hdd_cdrom: i=%i b=%i new_cdrom_channel=%i new_zip_channel=%i\n", i, b, new_cdrom_channel, new_zip_channel);
                sprintf(s, "IDC_COMBODRIVETYPE[%d]", i);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                if (i == new_cdrom_channel)
                        wx_sendmessage(h, WX_CB_SETCURSEL, 1, 0);
                else if (i == new_zip_channel)
                        wx_sendmessage(h, WX_CB_SETCURSEL, 2, 0);
                else
                        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                sprintf(s, "IDC_EDIT_FN[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_FILE[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EJECT[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_NEW[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EDIT_SPT[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EDIT_HPC[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EDIT_CYL[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_TEXT_SIZE[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                for (j = 1; j < 6; ++j)
                {
                        sprintf(s, "IDC_HDD_LABEL[%d]", i*10+j);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                }
        }
}

static volatile int create_drive_pos;
static int create_drive_raw(void* data)
{
        int c;
        uint8_t buf[512];
        FILE* f = (FILE*)data;
        memset(buf, 0, 512);
        for (c = 0; c < (hd_new_cyl * hd_new_hpc * hd_new_spt); c++)
        {
                create_drive_pos = c;
                fwrite(buf, 512, 1, f);
        }

        fclose(f);

        return 1;
}

static void vhd_progress_callback(uint32_t current_sector, uint32_t total_sectors)
{
        create_drive_pos = (int)current_sector;
}

/* If the disk geometry requested in the PCem GUI is not compatible with the internal VHD geometry,
 * we adjust it to the next-largest size that is compatible. On average, this will be a difference
 * of about 21 MB, and should only be necessary for VHDs larger than 31.5 GB, so should never be more
 * than a tenth of a percent change in size.
 */
static void adjust_pcem_geometry_for_vhd(MVHDGeom *vhd_geometry)
{
        if (hd_new_cyl <= 65535)
                return;

        int desired_sectors = hd_new_cyl * hd_new_hpc * hd_new_spt;
        if (desired_sectors > 267321600)
                desired_sectors = 267321600;

        int remainder = desired_sectors % 85680; /* 8560 is the LCM of 1008 (63*16) and 4080 (255*16) */
        if (remainder > 0)
                desired_sectors += (85680 - remainder);

        hd_new_cyl = desired_sectors / (16 * 63);
        hd_new_hpc = 16;
        hd_new_spt = 63;

        vhd_geometry->cyl = desired_sectors / (16 * 255);
        vhd_geometry->heads = 16;
        vhd_geometry->spt = 255;
}

static void adjust_vhd_geometry_for_pcem()
{
        if (hd_new_spt <= 63)
                return;

        int desired_sectors = hd_new_cyl * hd_new_hpc * hd_new_spt;
        if (desired_sectors > 267321600)
                desired_sectors = 267321600;

        int remainder = desired_sectors % 85680; /* 8560 is the LCM of 1008 (63*16) and 4080 (255*16) */
        if (remainder > 0)
                desired_sectors -= remainder;

        hd_new_cyl = desired_sectors / (16 * 63);
        hd_new_hpc = 16;
        hd_new_spt = 63;
}

static int create_drive_vhd_fixed(void *data)
{
        MVHDGeom geometry = { .cyl = hd_new_cyl, .heads = hd_new_hpc, .spt = hd_new_spt };
        adjust_pcem_geometry_for_vhd(&geometry);

        int vhd_error = 0;
        MVHDMeta *vhd = mvhd_create_fixed(hd_new_name, geometry, &vhd_error, vhd_progress_callback);
        if (vhd == NULL)
        {
                return 0;
        }
        else
        {
                mvhd_close(vhd);
                return 1;
        }
}

static int create_drive_vhd_dynamic(int blocksize)
{
        MVHDGeom geometry = { .cyl = hd_new_cyl, .heads = hd_new_hpc, .spt = hd_new_spt };
        adjust_pcem_geometry_for_vhd(&geometry);
        int vhd_error = 0;
        MVHDCreationOptions options;
        options.block_size_in_sectors = blocksize;
        options.path = hd_new_name;
        options.size_in_bytes = 0;
        options.geometry = geometry;
        options.type = MVHD_TYPE_DYNAMIC;

        MVHDMeta *vhd = mvhd_create_ex(options, &vhd_error);
        if (vhd == NULL)
        {
                return 0;
        }
        else
        {
                mvhd_close(vhd);
                return 1;
        }
}

static int create_drive_vhd_diff(char* parent_filename, int blocksize)
{
        int vhd_error = 0;
        MVHDCreationOptions options;
        options.block_size_in_sectors = blocksize;
        options.path = hd_new_name;
        options.parent_path = parent_filename;
        options.type = MVHD_TYPE_DIFF;

        MVHDMeta *vhd = mvhd_create_ex(options, &vhd_error);
        if (vhd == NULL)
        {
                return 0;
        }
        else
        {
                MVHDGeom vhd_geom = mvhd_get_geometry(vhd);

                if (vhd_geom.spt > 63)
                {
                        hd_new_cyl = mvhd_calc_size_sectors(&vhd_geom) / (16 * 63);
                        hd_new_hpc = 16;
                        hd_new_spt = 63;
                }
                else
                {
                        hd_new_cyl = vhd_geom.cyl;
                        hd_new_hpc = vhd_geom.heads;
                        hd_new_spt = vhd_geom.spt;
                }

                mvhd_close(vhd);
                return 1;
        }
}

static int hdnew_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[260];
        void* h;
        int c;
        PcemHDC hd[7];
        FILE *f;
        int hd_type, hd_format, hd_blocksize;
        int size;

        switch (message)
        {
                case WX_INITDIALOG:
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                sprintf(s, "%i", 63);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                sprintf(s, "%i", 16);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                sprintf(s, "%i", 511);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDITC"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"");

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT4"));
                sprintf(s, "%i", (((511*16*63)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

//                hd_type = 0;
                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Custom type");
                for (c = 1; c <= 46; c++)
                {
                        sprintf(s, "Type %02i : cylinders=%i, heads=%i, size=%iMB", c, hd_types[c-1].cylinders, hd_types[c-1].heads, (hd_types[c-1].cylinders * hd_types[c-1].heads * 17 * 512) / (1024 * 1024));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                }
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDFORMAT"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Raw (.img)");
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Fixed-size VHD (.vhd)");
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Dynamic-size VHD (.vhd)");
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Differencing VHD (.vhd)");
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Large blocks (2 MB)");
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Small blocks (512 KB)");
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                wx_sendmessage(h, WX_WM_SHOW, 0, (LONG_PARAM)0);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZELABEL"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)" ");

                return TRUE;
                case WX_COMMAND:
                if (wParam == wxID_OK) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDITC"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)hd_new_name);
                        if (!hd_new_name[0])
                        {
                                wx_messagebox(hdlg,"Please enter a valid filename","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_cyl);

                        if (hd_new_spt > 63)
                        {
                                wx_messagebox(hdlg, "Drive has too many sectors (maximum is 63)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                wx_messagebox(hdlg, "Drive has too many heads (maximum is 16)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > MAX_CYLINDERS)
                        {
                                char s[256];
                                sprintf(s, "Drive has too many cylinders (maximum is %i)", MAX_CYLINDERS);
                                wx_messagebox(hdlg, s, "PCem error", WX_MB_OK);
                                return TRUE;
                        }

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDFORMAT"));
                        hd_format = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZE"));
                        hd_blocksize = (wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0) == 0) ? MVHD_BLOCK_LARGE : MVHD_BLOCK_SMALL;

                        if (hd_format == 0) /* Raw .img */
                        {
                                f = fopen64(hd_new_name, "wb");
                                if (!f)
                                {
                                        wx_messagebox(hdlg, "Can't open file for write", "PCem error", WX_MB_OK);
                                        return TRUE;
                                }
                                wx_progressdialog(hdlg, "PCem", "Creating drive, please wait...", create_drive_raw, f, hd_new_cyl * hd_new_hpc * hd_new_spt, &create_drive_pos);
                        }
                        else if (hd_format == 1) /* Fixed VHD */
                        {
                                if (!wx_progressdialog(hdlg, "PCem", "Creating drive, please wait...", create_drive_vhd_fixed, NULL, hd_new_cyl * hd_new_hpc * hd_new_spt, &create_drive_pos))
                                {
                                       wx_messagebox(hdlg, "Can't create VHD", "PCem error", WX_MB_OK);
                                       return TRUE;
                                }
                        }
                        else if (hd_format == 2) /* Dynamic VHD */
                        {
                                if (!create_drive_vhd_dynamic(hd_blocksize))
                                {
                                       wx_messagebox(hdlg, "Can't create VHD", "PCem error", WX_MB_OK);
                                       return TRUE;
                                }
                        }
                        else if (hd_format == 3) /* Differencing VHD */
                        {
                                if (!getfilewithcaption(hdlg, "VHD file (*.vhd)|*.vhd|All files (*.*)|*.*", "", "Select the parent VHD"))
                                {
                                        if (!create_drive_vhd_diff(openfilestring, hd_blocksize))
                                        {
                                                wx_messagebox(hdlg, "Can't create VHD", "PCem error", WX_MB_OK);
                                                return TRUE;
                                        }
                                }
                        }

                        wx_messagebox(hdlg, "Drive created, remember to partition and format the new drive.", "PCem", WX_MB_OK);

                        wx_enddialog(hdlg, 1);
                        return TRUE;
                } else if (wParam == wxID_CANCEL) {
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                } else if (ID_IS("IDC_CFILE")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDFORMAT"));
                        hd_format = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                        if (!getsfile(hdlg,
                                hd_format == 0 ? "Raw Hard disc image (*.img)|*.img|All files (*.*)|*.*" : "VHD file (*.vhd)|*.vhd|All files (*.*)|*.*",
                                "",
                                NULL,
                                hd_format == 0 ? "img" : "vhd"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDITC"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) openfilestring);
                        }
                        return TRUE;
                } else if (ID_IS("IDC_EDIT1") || ID_IS("IDC_EDIT2") || ID_IS("IDC_EDIT3")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT1: %s\n", s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT2: %s\n", s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT3: %s\n", s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT4"));
                        sprintf(s, "%i", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                        hd_type = 0;
                        for (c = 1; c <= 46; c++)
                        {
                                pclog("Compare %i,%i %i,%i %i,%i\n", hd[0].spt,17, hd[0].hpc,hd_types[c-1].heads, hd[0].tracks,hd_types[c-1].cylinders);
                                if (hd[0].spt == 17 && hd[0].hpc == hd_types[c-1].heads && hd[0].tracks == hd_types[c-1].cylinders)
                                {
                                        hd_type = c;
                                        break;
                                }
                        }
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                        wx_sendmessage(h, WX_CB_SETCURSEL, hd_type, 0);

                        return TRUE;
                }
                else if (ID_IS("IDC_EDIT4"))
                {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT4"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT4: %s\n", s);
                        sscanf(s, "%i", &size);

                        hd[0].spt = 63;
                        hd[0].hpc = 16;
                        hd[0].tracks = (int)(((int64_t)size * 1024 * 1024) / (16 * 63 * 512));

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        sprintf(s, "%i", 63);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        sprintf(s, "%i", 16);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        sprintf(s, "%i", hd[0].tracks);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                }
                else if (ID_IS("IDC_HDTYPE")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                        hd_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                        if (hd_type)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                                sprintf(s, "%i", hd_types[hd_type-1].cylinders);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                                sprintf(s, "%i", hd_types[hd_type-1].heads);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"17");

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT4"));
                                sprintf(s, "%i", (int)(((uint64_t)hd_types[hd_type-1].cylinders*hd_types[hd_type-1].heads*17*512)/1024)/1024);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_HDFORMAT")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDFORMAT"));
                        hd_format = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                        if (hd_format == 3) /* They switched to a dynamic VHD; disable the geometry fields. */
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"(use parent)");
                                wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"(use parent)");
                                wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"(use parent)");
                                wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT4"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"(use parent)");
                                wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)0);
                        }
                        else /* Restore geometry fields if necessary. */
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                                wx_sendmessage(h, WX_WM_GETTEXT, 0, (LONG_PARAM)s);
                                if (!strncmp(s, "(use parent)", 12))
                                {
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"63");
                                        wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)1);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"16");
                                        wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)1);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"511");
                                        wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)1);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT4"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"251");
                                        wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)1);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                                        wx_sendmessage(h, WX_WM_ENABLE, 0, (LONG_PARAM)1);
                                }
                        }

                        if (hd_format == 2 || hd_format == 3) /* For dynamic and diff VHDs, show the block size dropdown. */
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZELABEL"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"Block size:");
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZE"));
                                wx_sendmessage(h, WX_WM_SHOW, 0, (LONG_PARAM)1);
                        }
                        else /* Hide it otherwise. */
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZELABEL"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)" ");
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDBLOCKSIZE"));
                                wx_sendmessage(h, WX_WM_SHOW, 0, (LONG_PARAM)0);
                        }

                        return TRUE;
                }
                break;

        }
        return FALSE;
}

static int hdsize_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[260];
        void* h;
        PcemHDC hd[2];
        int c;

        switch (message)
        {
                case WX_INITDIALOG:
                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Custom type");
                for (c = 1; c <= 46; c++)
                {
                        sprintf(s, "Type %02i : cylinders=%i, heads=%i, size=%iMB", c, hd_types[c-1].cylinders, hd_types[c-1].heads, (hd_types[c-1].cylinders * hd_types[c-1].heads * 17 * 512) / (1024 * 1024));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                }
                wx_sendmessage(h, WX_CB_SETCURSEL, hd_new_type, 0);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                sprintf(s, "%i", hd_new_spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                sprintf(s, "%i", hd_new_hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                sprintf(s, "%i", hd_new_cyl);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                sprintf(s, "%imb", (int)((((uint64_t)hd_new_spt*(uint64_t)hd_new_hpc*(uint64_t)hd_new_cyl)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                return TRUE;
                case WX_COMMAND:
                if (wParam == wxID_OK) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_cyl);

                        if (hd_new_spt > 63)
                        {
                                wx_messagebox(hdlg,"Drive has too many sectors (maximum is 63)","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                wx_messagebox(hdlg,"Drive has too many heads (maximum is 16)","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > MAX_CYLINDERS)
                        {
                                char s[256];
                                sprintf(s, "Drive has too many cylinders (maximum is %i)", MAX_CYLINDERS);
                                wx_messagebox(hdlg,s,"PCem error",WX_MB_OK);
                                return TRUE;
                        }

                        wx_enddialog(hdlg,1);
                        return TRUE;
                } else if (wParam == wxID_CANCEL) {
                        wx_enddialog(hdlg,0);
                        return TRUE;
                } else if (ID_IS("IDC_EDIT1") || ID_IS("IDC_EDIT2") || ID_IS("IDC_EDIT3")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                        sprintf(s, "%imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }

        }
        return FALSE;
}

int hdconf_init(void* hdlg)
{
        int c;

        for (c = 0; c < 7; c++)
        {
                char s[260];
                void *h;

                sprintf(s, "IDC_EDIT_SPT[%i]", c);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                sprintf(s, "%i", hdc[c].spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                sprintf(s, "IDC_EDIT_HPC[%i]", c);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                sprintf(s, "%i", hdc[c].hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                sprintf(s, "IDC_EDIT_CYL[%i]", c);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                sprintf(s, "%i", hdc[c].tracks);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                sprintf(s, "IDC_EDIT_FN[%i]", c);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)ide_fn[c]);

                sprintf(s, "IDC_TEXT_SIZE[%i]", c);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                sprintf(s, "%imb", (int)(((((uint64_t)hdc[c].tracks*(uint64_t)hdc[c].hpc)*(uint64_t)hdc[c].spt)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

        }

        return hdconf_update(hdlg);
}

int hdconf_update(void* hdlg)
{
        char s[260];
        void *h;

        int is_mfm = hdd_controller_selected_is_mfm(hdlg);

        update_hdd_cdrom(hdlg);

        int i;
        for (i = 0; i < 7; ++i)
        {
                sprintf(s, "IDC_HDD_LABEL[%d]", i*10);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                sprintf(s, "IDC_COMBODRIVETYPE[%d]", i);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
        }

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREHDD"));
        if (hdd_controller_selected_has_config(hdlg))
                wx_enablewindow(h, TRUE);
        else
                wx_enablewindow(h, FALSE);

        return TRUE;
}

static int hd_eject(void *hdlg, int drive)
{
        char s[80];

        ide_fn[drive][0] = 0;
        sprintf(s, "IDC_EDIT_SPT[%i]", drive);
        wx_setdlgitemtext(hdlg, WX_ID(s), "0");
        sprintf(s, "IDC_EDIT_HPC[%i]", drive);
        wx_setdlgitemtext(hdlg, WX_ID(s), "0");
        sprintf(s, "IDC_EDIT_CYL[%i]", drive);
        wx_setdlgitemtext(hdlg, WX_ID(s), "0");
        sprintf(s, "IDC_EDIT_FN[%i]", drive);
        wx_setdlgitemtext(hdlg, WX_ID(s), "");
        hd_changed = 1;

        return TRUE;
}

static int hd_new(void *hdlg, int drive)
{
        if (wx_dialogbox(hdlg, "HdNewDlg", hdnew_dlgproc) == 1)
        {
                char s[80];
                char id[80];
                void *h;

                sprintf(id, "IDC_EDIT_SPT[%i]", drive);
                h = wx_getdlgitem(hdlg, WX_ID(id));
                sprintf(s, "%i", hd_new_spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                sprintf(id, "IDC_EDIT_HPC[%i]", drive);
                h = wx_getdlgitem(hdlg, WX_ID(id));
                sprintf(s, "%i", hd_new_hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                sprintf(id, "IDC_EDIT_CYL[%i]", drive);
                h = wx_getdlgitem(hdlg, WX_ID(id));
                sprintf(s, "%i", hd_new_cyl);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                sprintf(id, "IDC_EDIT_FN[%i]", drive);
                h = wx_getdlgitem(hdlg, WX_ID(id));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)hd_new_name);

                sprintf(id, "IDC_TEXT_SIZE[%i]", drive);
                h = wx_getdlgitem(hdlg, WX_ID(id));
                sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                hd_changed = 1;
        }
        return TRUE;
}

static int hd_file(void *hdlg, int drive)
{
        if (!getfile(hdlg, "Hard disc image (*.img;*.vhd)|*.img;*.vhd|All files (*.*)|*.*", ""))
        {
                off64_t sz;
                FILE *f = fopen64(openfilestring, "rb");
                if (!f)
                {
                        wx_messagebox(hdlg,"Can't open file for read","PCem error",WX_MB_OK);
                        return TRUE;
                }

                if (mvhd_file_is_vhd(f))
                {
                        fclose(f);
                        int vhdError = 0;
                        MVHDMeta *vhd = mvhd_open(openfilestring, false, &vhdError);
                        if (vhd == NULL)
                        {
                                wx_messagebox(hdlg,mvhd_strerr(vhdError),"PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        else if (vhdError == MVHD_ERR_TIMESTAMP)
                        {
                                const char ts_warning[] =
                                        "WARNING: VHD PARENT/CHILD TIMESTAMPS DO NOT MATCH!\n\n"
                                        "This could indicate that the parent image was modified after this VHD was created.\n\n"
                                        "This could also happen if the VHD files were moved/copied, or the differencing VHD was created with DiskPart.\n\n"
                                        "Do you wish to fix this error after a file copy or DiskPart creation?";
                                int wx_res = wx_messagebox(hdlg,ts_warning,"PCem error", WX_MB_NODEFAULT);
                                if (wx_res == WX_MB_YES)
                                {
                                        int ts_res = mvhd_diff_update_par_timestamp(vhd, &vhdError);
                                        if (ts_res != 0)
                                        {
                                                wx_messagebox(hdlg,"Can't fix VHD timestamps","PCem error",WX_MB_OK);
                                                mvhd_close(vhd);
                                                return TRUE;
                                        }
                                }
                                else
                                {
                                        mvhd_close(vhd);
                                        return TRUE;
                                }

                        }
                        MVHDGeom geom = mvhd_get_geometry(vhd);
                        hd_new_cyl = geom.cyl;
                        hd_new_hpc = geom.heads;
                        hd_new_spt = geom.spt;
                        hd_new_type = 0;
                        adjust_vhd_geometry_for_pcem();
                        mvhd_close(vhd);
                }
                else
                {
                        fseeko64(f, -1, SEEK_END);
                        sz = ftello64(f) + 1;
                        fclose(f);
                        check_hd_type(hdlg, sz);
                }

                if (wx_dialogbox(hdlg, "HdSizeDlg", hdsize_dlgproc) == 1)
                {
                        char s[80];
                        char id[80];
                        void *h;

                        sprintf(id, "IDC_EDIT_SPT[%i]", drive);
                        h = wx_getdlgitem(hdlg, WX_ID(id));
                        sprintf(s, "%i", hd_new_spt);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        sprintf(id, "IDC_EDIT_HPC[%i]", drive);
                        h = wx_getdlgitem(hdlg, WX_ID(id));
                        sprintf(s, "%i", hd_new_hpc);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        sprintf(id, "IDC_EDIT_CYL[%i]", drive);
                        h = wx_getdlgitem(hdlg, WX_ID(id));
                        sprintf(s, "%i", hd_new_cyl);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        sprintf(id, "IDC_EDIT_FN[%i]", drive);
                        h = wx_getdlgitem(hdlg, WX_ID(id));
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)openfilestring);

                        sprintf(id, "IDC_TEXT_SIZE[%i]", drive);
                        h = wx_getdlgitem(hdlg, WX_ID(id));
                        sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                        hd_changed = 1;
                }
        }
        return TRUE;
}

static int hd_edit(void *hdlg, int drive)
{
        char s[80];
        char id[80];
        void *h;
        int spt, hpc, tracks;

        sprintf(id, "IDC_EDIT_SPT[%i]", drive);
        h = wx_getdlgitem(hdlg, WX_ID(id));
        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
        sscanf(s, "%i", &spt);
        sprintf(id, "IDC_EDIT_HPC[%i]", drive);
        h = wx_getdlgitem(hdlg, WX_ID(id));
        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
        sscanf(s, "%i", &hpc);
        sprintf(id, "IDC_EDIT_CYL[%i]", drive);
        h = wx_getdlgitem(hdlg, WX_ID(id));
        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
        sscanf(s, "%i", &tracks);

        sprintf(id, "IDC_TEXT_SIZE[%i]", drive);
        h = wx_getdlgitem(hdlg, WX_ID(id));
        sprintf(s, "%imb", ((((tracks*hpc)*spt)*512)/1024)/1024);
        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

        return TRUE;
}

static int hd_combodrivetype(void *hdlg, int drive)
{
        int type;
        char id[80];
        void *h;

        sprintf(id, "IDC_COMBODRIVETYPE[%i]", drive);
        h = wx_getdlgitem(hdlg, WX_ID(id));
        type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

        switch (type)
        {
                case 0: /*Hard drive*/
                if (new_cdrom_channel == drive)
                        new_cdrom_channel = -1;
                if (new_zip_channel == drive)
                        new_zip_channel = -1;
                break;
                case 1: /*CD-ROM*/
                new_cdrom_channel = drive;
                if (new_zip_channel == drive)
                        new_zip_channel = -1;
                break;
                case 2: /*Zip*/
                new_zip_channel = drive;
                if (new_cdrom_channel == drive)
                        new_cdrom_channel = -1;
                break;
        }
        update_hdd_cdrom(hdlg);
        return TRUE;
}

int hdconf_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        switch (message)
        {
                case WX_INITDIALOG:
                pause = 1;
                return hdconf_init(hdlg);

                case WX_COMMAND:
                if (ID_IS("IDC_EJECT[0]"))
                        return hd_eject(hdlg, 0);
                else if (ID_IS("IDC_EJECT[1]"))
                        return hd_eject(hdlg, 1);
                else if (ID_IS("IDC_EJECT[2]"))
                        return hd_eject(hdlg, 2);
                else if (ID_IS("IDC_EJECT[3]"))
                        return hd_eject(hdlg, 3);
                else if (ID_IS("IDC_EJECT[4]"))
                        return hd_eject(hdlg, 4);
                else if (ID_IS("IDC_EJECT[5]"))
                        return hd_eject(hdlg, 5);
                else if (ID_IS("IDC_EJECT[6]"))
                        return hd_eject(hdlg, 6);
                else if (ID_IS("IDC_NEW[0]"))
                        return hd_new(hdlg, 0);
                else if (ID_IS("IDC_NEW[1]"))
                        return hd_new(hdlg, 1);
                else if (ID_IS("IDC_NEW[2]"))
                        return hd_new(hdlg, 2);
                else if (ID_IS("IDC_NEW[3]"))
                        return hd_new(hdlg, 3);
                else if (ID_IS("IDC_NEW[4]"))
                        return hd_new(hdlg, 4);
                else if (ID_IS("IDC_NEW[5]"))
                        return hd_new(hdlg, 5);
                else if (ID_IS("IDC_NEW[6]"))
                        return hd_new(hdlg, 6);
                else if (ID_IS("IDC_FILE[0]"))
                        return hd_file(hdlg, 0);
                else if (ID_IS("IDC_FILE[1]"))
                        return hd_file(hdlg, 1);
                else if (ID_IS("IDC_FILE[2]"))
                        return hd_file(hdlg, 2);
                else if (ID_IS("IDC_FILE[3]"))
                        return hd_file(hdlg, 3);
                else if (ID_IS("IDC_FILE[4]"))
                        return hd_file(hdlg, 4);
                else if (ID_IS("IDC_FILE[5]"))
                        return hd_file(hdlg, 5);
                else if (ID_IS("IDC_FILE[6]"))
                        return hd_file(hdlg, 6);
                else if (ID_IS("IDC_EDIT_SPT[0]") || ID_IS("IDC_EDIT_HPC[0]") || ID_IS("IDC_EDIT_CYL[0]"))
                        return hd_edit(hdlg, 0);
                else if (ID_IS("IDC_EDIT_SPT[1]") || ID_IS("IDC_EDIT_HPC[1]") || ID_IS("IDC_EDIT_CYL[1]"))
                        return hd_edit(hdlg, 1);
                else if (ID_IS("IDC_EDIT_SPT[2]") || ID_IS("IDC_EDIT_HPC[2]") || ID_IS("IDC_EDIT_CYL[2]"))
                        return hd_edit(hdlg, 2);
                else if (ID_IS("IDC_EDIT_SPT[3]") || ID_IS("IDC_EDIT_HPC[3]") || ID_IS("IDC_EDIT_CYL[3]"))
                        return hd_edit(hdlg, 3);
                else if (ID_IS("IDC_EDIT_SPT[4]") || ID_IS("IDC_EDIT_HPC[4]") || ID_IS("IDC_EDIT_CYL[4]"))
                        return hd_edit(hdlg, 4);
                else if (ID_IS("IDC_EDIT_SPT[5]") || ID_IS("IDC_EDIT_HPC[5]") || ID_IS("IDC_EDIT_CYL[5]"))
                        return hd_edit(hdlg, 5);
                else if (ID_IS("IDC_EDIT_SPT[6]") || ID_IS("IDC_EDIT_HPC[6]") || ID_IS("IDC_EDIT_CYL[6]"))
                        return hd_edit(hdlg, 6);
                else if (ID_IS("IDC_COMBODRIVETYPE[0]"))
                        return hd_combodrivetype(hdlg, 0);
                else if (ID_IS("IDC_COMBODRIVETYPE[1]"))
                        return hd_combodrivetype(hdlg, 1);
                else if (ID_IS("IDC_COMBODRIVETYPE[2]"))
                        return hd_combodrivetype(hdlg, 2);
                else if (ID_IS("IDC_COMBODRIVETYPE[3]"))
                        return hd_combodrivetype(hdlg, 3);
                else if (ID_IS("IDC_COMBODRIVETYPE[4]"))
                        return hd_combodrivetype(hdlg, 4);
                else if (ID_IS("IDC_COMBODRIVETYPE[5]"))
                        return hd_combodrivetype(hdlg, 5);
                else if (ID_IS("IDC_COMBODRIVETYPE[6]"))
                        return hd_combodrivetype(hdlg, 6);
        }
        return FALSE;
}

int config_dialog_proc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        switch (message)
        {
        case WX_INITDIALOG:
                pause = 1;
                return config_dlgproc(hdlg, message, wParam, lParam);
        case WX_COMMAND:
        {
                if (wParam == wxID_OK)
                {
                        config_dlgsave(hdlg);
                        pause = 0;
                        wx_enddialog(hdlg, 1);
                        return TRUE;
                }
                else if (wParam == wxID_CANCEL)
                {
                        pause = 0;
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                else if (wParam == WX_ID("IDC_NOTEBOOK"))
                {
                        if (lParam == 3)
                                hdconf_update(hdlg);
                }
                break;
        }
        }
        config_dlgproc(hdlg, message, wParam, lParam);
        hdconf_dlgproc(hdlg, message, wParam, lParam);
        return TRUE;
}


int config_open(void* hwnd)
{
        return wx_dialogbox(hwnd, "ConfigureDlg", config_dialog_proc);
}
